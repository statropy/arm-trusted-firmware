/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/auth/crypto_mod.h>
#include <drivers/microchip/sha.h>
#include <drivers/microchip/xdmac.h>
#include <lib/mmio.h>

#include <platform_def.h>
#include "lan966x_regs.h"

#define SHA_SHA_IDATAR(b, n)	(SHA_SHA_IDATAR0(b) + (n * 4))
#define SHA_SHA_IODATAR(b, n)	(SHA_SHA_IODATAR0(b) + (n * 4))

#define CHKST_OK		5 /* Code for hash verified */

static const uint32_t base = LAN966X_SHA_BASE;

typedef struct {
	uint8_t hash_len;
} hash_info_t;

#define SHA_SMOD_DMA	2

#define MAX_HASH_LEN	64

/* Supported hashes and their length */
static const hash_info_t hashes[] = {
	[SHA_MR_ALGO_SHA1]   = { 20 },
	[SHA_MR_ALGO_SHA256] = { 32 },
	[SHA_MR_ALGO_SHA384] = { 48 },
	[SHA_MR_ALGO_SHA512] = { 64 },
	[SHA_MR_ALGO_SHA224] = { 28 },
};

static struct hash_state {
	lan966x_sha_type_t algo;
	bool		   verify;
	bool		   dma;
	size_t		   hash_nwords;
	size_t		   fifo_len;
} sha_hash_state;

static const hash_info_t *sha_get_info(lan966x_sha_type_t algo)
{
	if (algo < ARRAY_SIZE(hashes))
               return &hashes[algo];

	return NULL;
}

static uint32_t unaligned_get32(const void *p)
{
        const uint8_t *bp = (const uint8_t *)p;

        return ((uint32_t)bp[3] << 24)
                | ((uint32_t)bp[2] << 16)
                | ((uint32_t)bp[1] << 8)
                | bp[0];
}

void sha_init(void)
{
	mmio_write_32(SHA_SHA_CR(base), SHA_SHA_CR_SWRST(1));
}

static uint32_t sha_wait_flag(uint32_t mask)
{
	const int MAX_WAIT_LOOP = 10000;
	int tmo;
	uint32_t s;

	for (tmo = MAX_WAIT_LOOP; tmo > 0; tmo--) {
		s = mmio_read_32(SHA_SHA_ISR(base));
		if (mask & s)
			break;
	}

	assert(tmo > 0);

	VERBOSE("Wait(%d): %08x\n", tmo, s);
	return s;
}

static void sha_start_process(void)
{
	/* Start processing */
	mmio_setbits_32(SHA_SHA_CR(base), SHA_SHA_CR_START(1));
	/* Wait until processed */
	(void) sha_wait_flag(SHA_SHA_ISR_DATRDY_ISR_M);
}

static struct hash_state *
_sha_init(lan966x_sha_type_t hash_type, size_t data_len, const void *in_hash, size_t hash_len)
{
	struct hash_state *st = &sha_hash_state;
	uint32_t w;
	size_t i;

	assert((hash_len % 4) == 0);

	st->algo = hash_type;
	st->hash_nwords = hash_len / 4;
	st->fifo_len = ((hash_type == SHA_MR_ALGO_SHA384) ||
			(hash_type == SHA_MR_ALGO_SHA512)) ? 32 : 16;
	st->verify = (in_hash != NULL);
	st->dma = (data_len > 512); /* Use DMA for 'large' blocks */

	VERBOSE("SHA %s algo %d, %zd words hash, input %zd bytes\n",
		st->verify ? "verify" : "calc",
		hash_type, st->hash_nwords, data_len);

	/* Set algo and mode */
	w = SHA_SHA_MR_ALGO(hash_type) | SHA_SHA_MR_CHKCNT(st->hash_nwords);
	if (in_hash)
		w |= SHA_SHA_MR_CHECK(1);
	if (st->dma)
		w |= SHA_SHA_MR_SMOD(SHA_SMOD_DMA);
	mmio_write_32(SHA_SHA_MR(base), w);

	/* Set length of data (automatic padding) */
	mmio_write_32(SHA_SHA_MSR(base), data_len);
	mmio_write_32(SHA_SHA_BCR(base), data_len);

	if (st->verify) {
		/* Set expected hash */
		mmio_setbits_32(SHA_SHA_CR(base), SHA_SHA_CR_WUIEHV(1));
		for (i = 0; i < st->hash_nwords; i++) {
			w = unaligned_get32(in_hash + (i * 4));
			VERBOSE("Exp(%zd): %08x\n", i, w);
			mmio_write_32(SHA_SHA_IDATAR(base, i), w);
		}
		mmio_clrbits_32(SHA_SHA_CR(base), SHA_SHA_CR_WUIEHV(1));
	}

	/* Restart HASH */
	mmio_setbits_32(SHA_SHA_CR(base), SHA_SHA_CR_FIRST(1));

	return st;
}

static void _sha_update_mmio(const struct hash_state *st, const void *input, size_t len)
{
	size_t i, j, nwords;
	uint32_t w;

	/* Write data */
	nwords = div_round_up(len, 4);
	VERBOSE("Len(%zd) -> %zd words\n", len, nwords);

	if (nwords == 0) {
		sha_start_process();
	} else {
		for (i = 0; i < nwords; ) {
			/* Up to 16/32 words at a time */
			for (j = 0; j < st->fifo_len && i < nwords; j++, i++) {
				w = unaligned_get32(input + (i * 4));
				VERBOSE("Inp(%zd -> %zd): %08x\n", i, j, w);
				mmio_write_32(SHA_SHA_IDATAR(base, j), w);
			}
			sha_start_process();
		}
	}
}

static void _sha_update_dma(const struct hash_state *st, const void *input, size_t len)
{
	size_t dma_len = round_up(len, 4U); /* Transfer whole words */
	xdmac_memcpy((void*) (uintptr_t) SHA_SHA_IDATAR(base, 0), input, dma_len,
		     XDMA_DIR_MEM_TO_DEV, XDMA_SHA_TX);
}

static void _sha_update(const struct hash_state *st, const void *input, size_t len)
{
	if (st->dma)
		_sha_update_dma(st, input, len);
	else
		_sha_update_mmio(st, input, len);
}

static int _sha_finish(const struct hash_state *st, const void *out_hash)
{
	size_t i;
	uint32_t w;

	/* Wait for DMA processing to end */
	w = mmio_read_32(SHA_SHA_MR(base));
	if (SHA_SHA_MR_SMOD_X(w) == SHA_SMOD_DMA) {
		/* Must see DATRDY before proceeding */
		(void) sha_wait_flag(SHA_SHA_ISR_DATRDY_ISR_M);
	}

	/* Wait until checked */
	if (st->verify) {
		w = sha_wait_flag(SHA_SHA_ISR_CHECKF_ISR_M);
		VERBOSE("Check(%08x) -> %d\n", w, (unsigned int) SHA_SHA_ISR_CHKST_ISR_X(w));
		return SHA_SHA_ISR_CHKST_ISR_X(w) == CHKST_OK ?
			CRYPTO_SUCCESS : CRYPTO_ERR_SIGNATURE;
	}

	/* Read hash */
	assert(out_hash != NULL);
	for (i = 0; i < st->hash_nwords; i++) {
		w = mmio_read_32(SHA_SHA_IODATAR(base, i));
		*(uint32_t *)(out_hash + (i * 4)) = w;
		VERBOSE("HASH(%zd): %08x\n", i, w);
	}

	return 0;
}

int sha_calc(lan966x_sha_type_t hash_type, const void *input, size_t len, void *hash, size_t hash_len)
{
	const hash_info_t *hinfo = sha_get_info(hash_type);

	if (hinfo == NULL || hash_len < hinfo->hash_len)
		return -1;

	struct hash_state *st = _sha_init(hash_type, len, NULL, hinfo->hash_len);

	_sha_update(st, input, len);

	return _sha_finish(st, hash);
}

int sha_verify(lan966x_sha_type_t hash_type, const void *input, size_t len, const void *hash, size_t hash_len)
{
	const hash_info_t *hinfo = sha_get_info(hash_type);

	if (hinfo == NULL || hash_len < hinfo->hash_len)
		return -1;

	struct hash_state *st = _sha_init(hash_type, len, hash, hash_len);

	_sha_update(st, input, len);

	return _sha_finish(st, NULL);
}

void *sha_calc_init(lan966x_sha_type_t hash_type, size_t data_len, size_t hash_len)
{
	const hash_info_t *hinfo = sha_get_info(hash_type);

	if (hinfo == NULL || hash_len < hinfo->hash_len)
		return NULL;

	return _sha_init(hash_type, data_len, NULL, hash_len);
}

void sha_update(void *state, const void *input, size_t len)
{
	_sha_update(state, input, len);
}

int sha_calc_finish(void *state, void *hash)
{
	return _sha_finish(state, hash);
}
