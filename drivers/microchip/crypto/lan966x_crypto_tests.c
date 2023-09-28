/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <drivers/auth/crypto_mod.h>
#include <endian.h>
#include <lib/mmio.h>
#include <mbedtls/md.h>
#include <plat/common/platform.h>
#include <platform_def.h>
#include <sha.h>

#include "aes.h"

#if defined(LAN966X_AES_TESTS)
#define AES_TEST_MAXBUF	64
static const struct {
	int ct_len;
	const uint8_t key[32];
	const uint8_t iv[12];
	const uint8_t ct[AES_TEST_MAXBUF];
	const uint8_t pt[AES_TEST_MAXBUF];
	const uint8_t tag[16];
} test_data[] = {
	{
		.ct_len = 16,
		/* key */
		{
			0x4c, 0x8e, 0xbf, 0xe1, 0x44, 0x4e, 0xc1, 0xb2, 0xd5, 0x03, 0xc6, 0x98, 0x66, 0x59, 0xaf, 0x2c, 0x94, 0xfa, 0xfe, 0x94, 0x5f, 0x72, 0xc1, 0xe8, 0x48, 0x6a, 0x5a, 0xcf, 0xed, 0xb8, 0xa0, 0xf8,
		},
		/* iv */
		{
			0x47, 0x33, 0x60, 0xe0, 0xad, 0x24, 0x88, 0x99, 0x59, 0x85, 0x89, 0x95,
		},
		/* ct */
		{
			0xd2, 0xc7, 0x81, 0x10, 0xac, 0x7e, 0x8f, 0x10, 0x7c, 0x0d, 0xf0, 0x57, 0x0b, 0xd7, 0xc9, 0x0c,
		},
		/* pt */
		{
			0x77, 0x89, 0xb4, 0x1c, 0xb3, 0xee, 0x54, 0x88, 0x14, 0xca, 0x0b, 0x38, 0x8c, 0x10, 0xb3, 0x43,
		},
		/* tag */
		{
			0xc2, 0x6a, 0x37, 0x9b, 0x6d, 0x98, 0xef, 0x28, 0x52, 0xea, 0xd8, 0xce, 0x83, 0xa8, 0x33, 0xa7,
		},
	}, {
		.ct_len = 51,
		/* key */
		{
			0xf9, 0xb7, 0x0f, 0xd0, 0x65, 0x66, 0x8b, 0x9f, 0xc4, 0xee, 0x7e, 0x22, 0x2f, 0x1c, 0x4a, 0xe2, 0x7e, 0x0a, 0x6e, 0x37, 0xb5, 0x51, 0xe7, 0xd5, 0xfb, 0x58, 0xee, 0xa4, 0x0a, 0x59, 0xfb, 0xa3,
		},
		/* iv */
		{
			0xa7, 0xf5, 0xdd, 0xb3, 0x9b, 0x8c, 0x62, 0xb5, 0x0b, 0x5a, 0x8c, 0x0c,
		},
		/* ct */
		{
			0x0d, 0x6d, 0xcd, 0xf0, 0x82, 0x0f, 0x54, 0x6d, 0x54, 0xf5, 0x47, 0x6f, 0x49, 0xbb, 0xf1, 0xcf, 0xaf, 0xae, 0x3b, 0x5c, 0x7c, 0xb0, 0x87, 0x5c, 0x82, 0x67, 0x57, 0x65, 0x08, 0x64, 0xf9, 0x9d, 0x74, 0xee, 0x40, 0x73, 0x65, 0x1e, 0xed, 0x0d, 0xba, 0xf5, 0x78, 0x9d, 0x21, 0x1c, 0x1b, 0xe5, 0x57, 0x98, 0x43,
		},
		/* pt */
		{
			0x6e, 0x9c, 0x24, 0xc1, 0x72, 0xae, 0x8e, 0x81, 0xe6, 0x9e, 0x79, 0x7a, 0x8b, 0xd9, 0xf8, 0xde, 0x4e, 0x5e, 0x43, 0xcc, 0xbd, 0xee, 0xc5, 0xa0, 0xd0, 0xec, 0x1a, 0x7b, 0x35, 0x27, 0x38, 0x4e, 0x06, 0x12, 0x92, 0x90, 0xc5, 0xf6, 0x1f, 0xa2, 0xf9, 0x0a, 0xe8, 0xb0, 0x3a, 0x94, 0x02, 0xae, 0xb0, 0xb6, 0xce,
		},
		/* tag */
		{
			0x31, 0xef, 0xc6, 0x9d, 0xaa, 0xe6, 0xf7, 0xf0, 0x06, 0x7f, 0xd6, 0xe9, 0x69, 0xbd, 0x92, 0x40,
		},
	},
};

static void aes_run_tests(void)
{
	int i, rc;
	uint8_t buf[AES_TEST_MAXBUF];
	uint8_t tag[16];

	for (i = 0; i < ARRAY_SIZE(test_data); i++) {
		assert(test_data[i].ct_len <= AES_TEST_MAXBUF);
		memcpy(buf, test_data[i].ct, test_data[i].ct_len);
		rc = crypto_mod_auth_decrypt(CRYPTO_GCM_DECRYPT,
					     buf,
					     test_data[i].ct_len,
					     test_data[i].key,
					     32, 0,
					     test_data[i].iv,
					     12,
					     test_data[i].tag,
					     16);
		if (rc) {
			WARN("test(%d): decrypt failed: %d\n", i, rc);
			assert(0);
		} else {
			if (memcmp(buf, test_data[i].pt, test_data[i].ct_len)) {
				WARN("test(%d): decrypt data compare fails\n", i);
				assert(0);
			}
			WARN("test(%d): decrypt succeeded: %d\n", i, rc);
		}

		/* Now encrypt */
		memcpy(buf, test_data[i].pt, test_data[i].ct_len);
		rc = aes_gcm_encrypt(buf, test_data[i].ct_len,
				     test_data[i].key, sizeof(test_data[i].key),
				     test_data[i].iv, sizeof(test_data[i].iv),
				     tag, sizeof(tag));
		if (rc) {
			WARN("test(%d): encrypt failed: %d\n", i, rc);
			assert(0);
		} else {
			if (memcmp(buf, test_data[i].ct, test_data[i].ct_len)) {
				WARN("test(%d): encrypt data compare fails\n", i);
				assert(0);
			}
			if (memcmp(tag, test_data[i].tag, sizeof(tag))) {
				WARN("test(%d): encrypt tag compare fails\n", i);
				assert(0);
			}
			WARN("test(%d): encrypt succeeded: %d\n", i, rc);
		}
	}
}
#endif

#if defined(LAN966X_SHA_TESTS)
#define FOX "The quick brown fox jumps over the lazy dog"
static const char inp_1[] = FOX;
static const char inp_2[] = "abc";
static const char inp_3[] = FOX FOX FOX;
static const uint32_t hash_sha1_1[] = {
	__ntohl(0x2fd4e1c6), __ntohl(0x7a2d28fc), __ntohl(0xed849ee1), __ntohl(0xbb76e739),
	__ntohl(0x1b93eb12)
};
static const uint32_t hash_sha1_2[] = {
	__ntohl(0xa9993e36), __ntohl(0x4706816a), __ntohl(0xba3e2571), __ntohl(0x7850c26c),
	__ntohl(0x9cd0d89d)
};
static const uint32_t hash_sha224_1[] = {
	__ntohl(0x730e109b), __ntohl(0xd7a8a32b), __ntohl(0x1cb9d9a0), __ntohl(0x9aa2325d),
	__ntohl(0x2430587d), __ntohl(0xdbc0c38b), __ntohl(0xad911525)
};
static const uint32_t hash_sha224_2[] = {
	__ntohl(0x23097d22), __ntohl(0x3405d822), __ntohl(0x8642a477), __ntohl(0xbda255b3),
	__ntohl(0x2aadbce4), __ntohl(0xbda0b3f7), __ntohl(0xe36c9da7)
};
static const uint32_t hash_sha256_1[] = {
	__ntohl(0xd7a8fbb3), __ntohl(0x07d78094), __ntohl(0x69ca9abc), __ntohl(0xb0082e4f),
	__ntohl(0x8d5651e4), __ntohl(0x6d3cdb76), __ntohl(0x2d02d0bf), __ntohl(0x37c9e592)
};
static const uint32_t hash_sha256_2[] = {
	__ntohl(0xba7816bf), __ntohl(0x8f01cfea), __ntohl(0x414140de), __ntohl(0x5dae2223),
	__ntohl(0xb00361a3), __ntohl(0x96177a9c), __ntohl(0xb410ff61), __ntohl(0xf20015ad)
};
static const uint32_t hash_sha384_1[] = {
	__ntohl(0xca737f10), __ntohl(0x14a48f4c), __ntohl(0x0b6dd43c), __ntohl(0xb177b0af),
	__ntohl(0xd9e51693), __ntohl(0x67544c49), __ntohl(0x4011e331), __ntohl(0x7dbf9a50),
	__ntohl(0x9cb1e5dc), __ntohl(0x1e85a941), __ntohl(0xbbee3d7f), __ntohl(0x2afbc9b1),
};
static const uint32_t hash_sha384_2[] = {
	__ntohl(0xcb00753f), __ntohl(0x45a35e8b), __ntohl(0xb5a03d69), __ntohl(0x9ac65007),
	__ntohl(0x272c32ab), __ntohl(0x0eded163), __ntohl(0x1a8b605a), __ntohl(0x43ff5bed),
	__ntohl(0x8086072b), __ntohl(0xa1e7cc23), __ntohl(0x58baeca1), __ntohl(0x34c825a7),
};
static const uint32_t hash_sha512_1[] = {
	__ntohl(0x07e547d9), __ntohl(0x586f6a73), __ntohl(0xf73fbac0), __ntohl(0x435ed769),
	__ntohl(0x51218fb7), __ntohl(0xd0c8d788), __ntohl(0xa309d785), __ntohl(0x436bbb64),
	__ntohl(0x2e93a252), __ntohl(0xa954f239), __ntohl(0x12547d1e), __ntohl(0x8a3b5ed6),
	__ntohl(0xe1bfd709), __ntohl(0x7821233f), __ntohl(0xa0538f3d), __ntohl(0xb854fee6),
};
static const uint32_t hash_sha512_2[] = {
	__ntohl(0xddaf35a1), __ntohl(0x93617aba), __ntohl(0xcc417349), __ntohl(0xae204131),
	__ntohl(0x12e6fa4e), __ntohl(0x89a97ea2), __ntohl(0x0a9eeee6), __ntohl(0x4b55d39a),
	__ntohl(0x2192992a), __ntohl(0x274fc1a8), __ntohl(0x36ba3c23), __ntohl(0xa3feebbd),
	__ntohl(0x454d4423), __ntohl(0x643ce80e), __ntohl(0x2a9ac94f), __ntohl(0xa54ca49f),
};
static const uint32_t hash_sha512_3[] = {
	__ntohl(0x8bbc0670), __ntohl(0xdc3e29c7), __ntohl(0x341035e6), __ntohl(0x110968c8),
	__ntohl(0x78dca505), __ntohl(0x248f09b3), __ntohl(0x380899ed), __ntohl(0x9b3a1aec),
	__ntohl(0x19282f1d), __ntohl(0x2de75d6c), __ntohl(0x6acc1d3e), __ntohl(0x0b63be33),
	__ntohl(0xc0c5a731), __ntohl(0xac00f7d2), __ntohl(0x9c02e31c), __ntohl(0x2846cfde),
};

static struct {
	int hash_type;
	const void *data;
	size_t len;
	const void *hash;
	size_t hash_len;
} testdata[] = {
	{ SHA_MR_ALGO_SHA1, inp_1, sizeof(inp_1)-1, hash_sha1_1, sizeof(hash_sha1_1) },
	{ SHA_MR_ALGO_SHA1, inp_2, sizeof(inp_2)-1, hash_sha1_2, sizeof(hash_sha1_2) },
	{ SHA_MR_ALGO_SHA224, inp_1, sizeof(inp_1)-1, hash_sha224_1, sizeof(hash_sha224_1) },
	{ SHA_MR_ALGO_SHA224, inp_2, sizeof(inp_2)-1, hash_sha224_2, sizeof(hash_sha224_2) },
	{ SHA_MR_ALGO_SHA256, inp_1, sizeof(inp_1)-1, hash_sha256_1, sizeof(hash_sha256_1) },
	{ SHA_MR_ALGO_SHA256, inp_2, sizeof(inp_2)-1, hash_sha256_2, sizeof(hash_sha256_2) },
	{ SHA_MR_ALGO_SHA384, inp_1, sizeof(inp_1)-1, hash_sha384_1, sizeof(hash_sha384_1) },
	{ SHA_MR_ALGO_SHA384, inp_2, sizeof(inp_2)-1, hash_sha384_2, sizeof(hash_sha384_2) },
	{ SHA_MR_ALGO_SHA512, inp_1, sizeof(inp_1)-1, hash_sha512_1, sizeof(hash_sha512_1) },
	{ SHA_MR_ALGO_SHA512, inp_2, sizeof(inp_2)-1, hash_sha512_2, sizeof(hash_sha512_2) },
	{ SHA_MR_ALGO_SHA512, inp_3, sizeof(inp_3)-1, hash_sha512_3, sizeof(hash_sha512_3) },
};

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

void lan966x_crypto_tests(void)
{
#if defined(LAN966X_SHA_TESTS)
	sha_test();
#endif
#if defined(LAN966X_AES_TESTS)
	aes_run_tests();
#endif
}
