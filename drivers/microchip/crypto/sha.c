/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/auth/crypto_mod.h>
#include <endian.h>
#include <lib/mmio.h>
#include <mbedtls/md.h>
#include <sha.h>

#include "lan966x_regs.h"

#define SHA_MR_ALGO_SHA1	0
#define SHA_MR_ALGO_SHA256	1
#define SHA_MR_ALGO_SHA384	2
#define SHA_MR_ALGO_SHA512	3
#define SHA_MR_ALGO_SHA224	4

#define SHA_SHA_IDATAR(b, n)	(SHA_SHA_IDATAR0(b) + (n * 4))
#define SHA_SHA_IODATAR(b, n)	(SHA_SHA_IODATAR0(b) + (n * 4))

#define CHKST_OK		5 /* Code for hash verified */

static const uint32_t base = LAN966X_SHA_BASE;

#define SHA_TESTS
#if defined(SHA_TESTS)
#define FOX "The quick brown fox jumps over the lazy dog"
static const char inp_1[] = FOX;
static const char inp_2[] = "abc";
static const char inp_3[] = FOX FOX FOX;
static const uint32_t hash_sha1_1[] = {
	0x2fd4e1c6, 0x7a2d28fc, 0xed849ee1, 0xbb76e739, 0x1b93eb12
};
static const uint32_t hash_sha1_2[] = {
	0xa9993e36, 0x4706816a, 0xba3e2571, 0x7850c26c, 0x9cd0d89d
};
static const uint32_t hash_sha224_1[] = {
	0x730e109b, 0xd7a8a32b, 0x1cb9d9a0, 0x9aa2325d, 0x2430587d, 0xdbc0c38b, 0xad911525
};
static const uint32_t hash_sha224_2[] = {
	0x23097d22, 0x3405d822, 0x8642a477, 0xbda255b3, 0x2aadbce4, 0xbda0b3f7, 0xe36c9da7
};
static const uint32_t hash_sha256_1[] = {
	0xd7a8fbb3, 0x07d78094, 0x69ca9abc, 0xb0082e4f, 0x8d5651e4, 0x6d3cdb76, 0x2d02d0bf, 0x37c9e592
};
static const uint32_t hash_sha256_2[] = {
	0xba7816bf, 0x8f01cfea, 0x414140de, 0x5dae2223, 0xb00361a3, 0x96177a9c, 0xb410ff61, 0xf20015ad
};
static const uint32_t hash_sha384_1[] = {
	0xca737f10, 0x14a48f4c, 0x0b6dd43c, 0xb177b0af, 0xd9e51693, 0x67544c49, 0x4011e331,
	0x7dbf9a50, 0x9cb1e5dc, 0x1e85a941, 0xbbee3d7f, 0x2afbc9b1,
};
static const uint32_t hash_sha384_2[] = {
	0xcb00753f, 0x45a35e8b, 0xb5a03d69, 0x9ac65007, 0x272c32ab, 0x0eded163, 0x1a8b605a,
	0x43ff5bed, 0x8086072b, 0xa1e7cc23, 0x58baeca1, 0x34c825a7,
};
static const uint32_t hash_sha512_1[] = {
	0x07e547d9, 0x586f6a73, 0xf73fbac0, 0x435ed769, 0x51218fb7, 0xd0c8d788, 0xa309d785,
	0x436bbb64, 0x2e93a252, 0xa954f239, 0x12547d1e, 0x8a3b5ed6, 0xe1bfd709, 0x7821233f,
	0xa0538f3d, 0xb854fee6,
};
static const uint32_t hash_sha512_2[] = {
	0xddaf35a1, 0x93617aba, 0xcc417349, 0xae204131, 0x12e6fa4e, 0x89a97ea2, 0x0a9eeee6,
	0x4b55d39a, 0x2192992a, 0x274fc1a8, 0x36ba3c23, 0xa3feebbd, 0x454d4423, 0x643ce80e,
	0x2a9ac94f, 0xa54ca49f,
};
static const uint32_t hash_sha512_3[] = {
	0x8bbc0670, 0xdc3e29c7, 0x341035e6, 0x110968c8, 0x78dca505, 0x248f09b3, 0x380899ed,
	0x9b3a1aec, 0x19282f1d, 0x2de75d6c, 0x6acc1d3e, 0x0b63be33, 0xc0c5a731, 0xac00f7d2,
	0x9c02e31c, 0x2846cfde,
};

static struct {
	int hash_type;
	const void *data;
	size_t len;
	const void *hash;
	size_t hash_len;
} testdata[] = {
	{ MBEDTLS_MD_SHA1, inp_1, sizeof(inp_1)-1, hash_sha1_1, sizeof(hash_sha1_1) },
	{ MBEDTLS_MD_SHA1, inp_2, sizeof(inp_2)-1, hash_sha1_2, sizeof(hash_sha1_2) },
	{ MBEDTLS_MD_SHA224, inp_1, sizeof(inp_1)-1, hash_sha224_1, sizeof(hash_sha224_1) },
	{ MBEDTLS_MD_SHA224, inp_2, sizeof(inp_2)-1, hash_sha224_2, sizeof(hash_sha224_2) },
	{ MBEDTLS_MD_SHA256, inp_1, sizeof(inp_1)-1, hash_sha256_1, sizeof(hash_sha256_1) },
	{ MBEDTLS_MD_SHA256, inp_2, sizeof(inp_2)-1, hash_sha256_2, sizeof(hash_sha256_2) },
	{ MBEDTLS_MD_SHA384, inp_1, sizeof(inp_1)-1, hash_sha384_1, sizeof(hash_sha384_1) },
	{ MBEDTLS_MD_SHA384, inp_2, sizeof(inp_2)-1, hash_sha384_2, sizeof(hash_sha384_2) },
	{ MBEDTLS_MD_SHA512, inp_1, sizeof(inp_1)-1, hash_sha512_1, sizeof(hash_sha512_1) },
	{ MBEDTLS_MD_SHA512, inp_2, sizeof(inp_2)-1, hash_sha512_2, sizeof(hash_sha512_2) },
	{ MBEDTLS_MD_SHA512, inp_3, sizeof(inp_3)-1, hash_sha512_3, sizeof(hash_sha512_3) },
};
#endif

typedef struct {
	uint8_t mbed_algo;
	uint8_t algo;
	uint8_t hash_len;
} hash_info_t;

static const hash_info_t hashes[] = {
	[SHA_MR_ALGO_SHA1]   = { MBEDTLS_MD_SHA1,   SHA_MR_ALGO_SHA1,   20 },
	[SHA_MR_ALGO_SHA256] = { MBEDTLS_MD_SHA256, SHA_MR_ALGO_SHA256, 32 },
	[SHA_MR_ALGO_SHA384] = { MBEDTLS_MD_SHA384, SHA_MR_ALGO_SHA384, 48 },
	[SHA_MR_ALGO_SHA512] = { MBEDTLS_MD_SHA512, SHA_MR_ALGO_SHA512, 64 },
	[SHA_MR_ALGO_SHA224] = { MBEDTLS_MD_SHA224, SHA_MR_ALGO_SHA224, 28 },
};

static const hash_info_t *sha_get_info(int algo)
{
	switch (algo) {
	case MBEDTLS_MD_SHA1:
		return &hashes[SHA_MR_ALGO_SHA1];
	case MBEDTLS_MD_SHA224:
		return &hashes[SHA_MR_ALGO_SHA224];
	case MBEDTLS_MD_SHA256:
		return &hashes[SHA_MR_ALGO_SHA256];
	case MBEDTLS_MD_SHA384:
		return &hashes[SHA_MR_ALGO_SHA384];
	case MBEDTLS_MD_SHA512:
		return &hashes[SHA_MR_ALGO_SHA512];
	default:
		break;
	}
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

#if defined(SHA_TESTS)
void sha_test(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(testdata); i++) {
		int res = sha_verify(testdata[i].hash_type,
				     testdata[i].data,
				     testdata[i].len,
				     testdata[i].hash,
				     testdata[i].hash_len);
		assert(res == CRYPTO_SUCCESS);
	}
}
#endif

void sha_init(void)
{
	mmio_write_32(SHA_SHA_CR(base), SHA_SHA_CR_SWRST(1));
#if defined(SHA_TESTS)
	sha_test();
#endif
}

#define MAX_WAIT_LOOP 10000
static uint32_t sha_wait_flag(uint32_t mask)
{
	int tmo;
	uint32_t s;

	for (tmo = MAX_WAIT_LOOP; tmo > 0; tmo--) {
		s = mmio_read_32(SHA_SHA_ISR(base));
		if (mask & s)
			break;
		/* Hummmm */
		//udelay(1);
	}

	assert(tmo > 0);

	INFO("Wait(%d): %08x\n", tmo, s);
	return s;
}

static void sha_start_process(void)
{
	/* Start processing */
	mmio_setbits_32(SHA_SHA_CR(base), SHA_SHA_CR_START(1));
	/* Wait until processed */
	(void) sha_wait_flag(SHA_SHA_ISR_DATRDY_ISR_M);
}

static int sha_process(int hash_type, const void *input, size_t len,
		       const void *in_hash, const void *out_hash, size_t hash_len)
{
	int i, j, nwords, fifo;
	uint32_t w;

	assert((hash_len % 4) == 0);
	hash_len /= 4;

	INFO("SHA algo %d, %d words hash, input %d bytes\n", hash_type, hash_len, len);

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
			w = __ntohl(w); 	/* Always in BE */
			INFO("Exp(%d): %08x\n", i, w);
			mmio_write_32(SHA_SHA_IDATAR(base, i), w);
		}
		mmio_clrbits_32(SHA_SHA_CR(base), SHA_SHA_CR_WUIEHV(1));
	}

	/* Write data */
	nwords = div_round_up(len, 4);
	mmio_setbits_32(SHA_SHA_CR(base), SHA_SHA_CR_FIRST(1));
	INFO("Len(%d) -> %d words\n", len, nwords);

	/* Fox sha384/512, also use IODATARx */
	fifo = ((hash_type == SHA_MR_ALGO_SHA384) |
		(hash_type == SHA_MR_ALGO_SHA512)) ? 32 : 16;

	if (nwords == 0)
		sha_start_process();
	else
		for (i = 0; i < nwords; ) {
			/* Up to 16/32 words at a time */
			for (j = 0; j < fifo && i < nwords; j++, i++) {
				w = unaligned_get32(input + (i * 4));
				INFO("Inp(%d -> %d): %08x\n", i, j, w);
				mmio_write_32(SHA_SHA_IDATAR(base, j), w);
			}
			sha_start_process();
		}

	/* Wait until checked */
	if (in_hash) {
		w = sha_wait_flag(SHA_SHA_ISR_CHECKF_ISR_M);
		INFO("Check(%08x) -> %d\n", w, SHA_SHA_ISR_CHKST_ISR_X(w));
		return SHA_SHA_ISR_CHKST_ISR_X(w) == CHKST_OK ?
			CRYPTO_SUCCESS : CRYPTO_ERR_SIGNATURE;
	}

	/* Read hash */
	assert(out_hash != NULL);
	for (i = 0; i < hash_len; i++) {
		w = mmio_read_32(SHA_SHA_IODATAR(base, i));
		w = __ntohl(w);	/* Always in BE */
		*(uint32_t *)(out_hash + (i * 4)) = __ntohl(w);
		INFO("HASH(%d): %08x vs %08x\n", i, w,
		     unaligned_get32(in_hash + (i * 4)));
	}

	return 0;
}

int sha_verify(int hash_type, const void *input, size_t len, const void *hash, size_t hash_len)
{
	const hash_info_t *hinfo = sha_get_info(hash_type);

	if (hinfo)
		return sha_process(hinfo->algo, input, len, hash, NULL, hash_len);

	return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
}

int sha_calc(int hash_type, const void *input, size_t len, void *hash)
{
	const hash_info_t *hinfo = sha_get_info(hash_type);

	if (hinfo)
		return sha_process(hinfo->algo, input, len, NULL, hash, hinfo->hash_len);

	return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
}
