/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/auth/crypto_mod.h>
#include <lib/mmio.h>
#include <sha.h>

#include "lan966x_regs.h"

#define SHA_SHA_IDATAR(b, n)	(SHA_SHA_IDATAR0(b) + (n * 4))
#define SHA_SHA_IODATAR(b, n)	(SHA_SHA_IODATAR0(b) + (n * 4))

#define CHKST_OK		5 /* Code for hash verified */

static const uint32_t base = LAN966X_SHA_BASE;

typedef struct {
	uint8_t hash_len;
} hash_info_t;

static const hash_info_t hashes[] = {
	[SHA_MR_ALGO_SHA1]   = { 20 },
	[SHA_MR_ALGO_SHA256] = { 32 },
	[SHA_MR_ALGO_SHA384] = { 48 },
	[SHA_MR_ALGO_SHA512] = { 64 },
	[SHA_MR_ALGO_SHA224] = { 28 },
};

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

static int sha_process(lan966x_sha_type_t hash_type, const void *input, size_t len,
		       const void *in_hash, const void *out_hash, size_t hash_len)
{
	int i, j, nwords, fifo;
	uint32_t w;

	assert((hash_len % 4) == 0);
	hash_len /= 4;

	VERBOSE("SHA algo %d, %d words hash, input %d bytes\n", hash_type, hash_len, len);

	/* Set algo and mode */
	w = SHA_SHA_MR_ALGO(hash_type) | SHA_SHA_MR_CHKCNT(hash_len);
	if (in_hash)
		w |= SHA_SHA_MR_CHECK(1);
	mmio_write_32(SHA_SHA_MR(base), w);

	/* Set length of data (automatic padding) */
	mmio_write_32(SHA_SHA_MSR(base), len);
	mmio_write_32(SHA_SHA_BCR(base), len);

	if (in_hash) {
		/* Set expected hash */
		mmio_setbits_32(SHA_SHA_CR(base), SHA_SHA_CR_WUIEHV(1));
		for (i = 0; i < hash_len; i++) {
			w = unaligned_get32(in_hash + (i * 4));
			VERBOSE("Exp(%d): %08x\n", i, w);
			mmio_write_32(SHA_SHA_IDATAR(base, i), w);
		}
		mmio_clrbits_32(SHA_SHA_CR(base), SHA_SHA_CR_WUIEHV(1));
	}

	/* Write data */
	nwords = div_round_up(len, 4);
	mmio_setbits_32(SHA_SHA_CR(base), SHA_SHA_CR_FIRST(1));
	VERBOSE("Len(%d) -> %d words\n", len, nwords);

	/* For sha384/512, also use IODATARx */
	fifo = ((hash_type == SHA_MR_ALGO_SHA384) |
		(hash_type == SHA_MR_ALGO_SHA512)) ? 32 : 16;

	if (nwords == 0) {
		sha_start_process();
	} else {
		for (i = 0; i < nwords; ) {
			/* Up to 16/32 words at a time */
			for (j = 0; j < fifo && i < nwords; j++, i++) {
				w = unaligned_get32(input + (i * 4));
				VERBOSE("Inp(%d -> %d): %08x\n", i, j, w);
				mmio_write_32(SHA_SHA_IDATAR(base, j), w);
			}
			sha_start_process();
		}
	}

	/* Wait until checked */
	if (in_hash) {
		w = sha_wait_flag(SHA_SHA_ISR_CHECKF_ISR_M);
		VERBOSE("Check(%08x) -> %d\n", w, SHA_SHA_ISR_CHKST_ISR_X(w));
		return SHA_SHA_ISR_CHKST_ISR_X(w) == CHKST_OK ?
			CRYPTO_SUCCESS : CRYPTO_ERR_SIGNATURE;
	}

	/* Read hash */
	assert(out_hash != NULL);
	for (i = 0; i < hash_len; i++) {
		w = mmio_read_32(SHA_SHA_IODATAR(base, i));
		*(uint32_t *)(out_hash + (i * 4)) = w;
		VERBOSE("HASH(%d): %08x\n", i, w);
	}

	return 0;
}

int sha_verify(lan966x_sha_type_t hash_type, const void *input, size_t len, const void *hash, size_t hash_len)
{
	const hash_info_t *hinfo = sha_get_info(hash_type);

	if (hinfo)
		return sha_process(hash_type, input, len, hash, NULL, hash_len);

	return -1;
}

int sha_calc(lan966x_sha_type_t hash_type, const void *input, size_t len, void *hash)
{
	const hash_info_t *hinfo = sha_get_info(hash_type);

	if (hinfo)
		return sha_process(hash_type, input, len, NULL, hash, hinfo->hash_len);

	return -1;
}
