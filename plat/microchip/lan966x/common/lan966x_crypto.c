/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>
#include <drivers/auth/crypto_mod.h>
#include <platform_def.h>

#include <mbedtls/md_internal.h>
#include <mbedtls/md.h>
#include <mbedtls/asn1.h>
#include <mbedtls/oid.h>

#include "pkcl.h"
#include "sha.h"
#include "aes.h"

#define LIB_NAME		"SAMA5 crypto core"

#define AES_TESTS
#ifdef AES_TESTS
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
	},
};

static void aes_run_tests(void)
{
	int i, rc;
	uint8_t buf[AES_TEST_MAXBUF];

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
		}
		if (memcmp(buf, test_data[i].pt, test_data[i].ct_len)) {
			WARN("test(%d): data compare fails\n", i);
			assert(0);
		}
	}

}
#endif

static void init(void)
{

	sha_init();
	aes_init();
#if defined(AES_TESTS)
	aes_run_tests();
#endif
	pkcl_init();
}

/*
 * Verify a signature.
 *
 * Parameters are passed using the DER encoding format following the ASN.1
 * structures detailed above.
 */
static int verify_signature(void *data_ptr, unsigned int data_len,
			    void *sig_ptr, unsigned int sig_len,
			    void *sig_alg, unsigned int sig_alg_len,
			    void *pk_ptr, unsigned int pk_len)
{
	/* Signature verification success */
	INFO("%s: Called dlen = %d, siglen = %d, pk_len = %d\n", __FUNCTION__,
		data_len, sig_len, pk_len);
	return CRYPTO_SUCCESS;
}

/*
 * Match a hash
 *
 * Digest info is passed in DER format following the ASN.1 structure detailed
 * above.
 */
static int verify_hash(void *data_ptr, unsigned int data_len,
		       void *digest_info_ptr, unsigned int digest_info_len)
{
	mbedtls_asn1_buf hash_oid, params;
	mbedtls_md_type_t md_alg;
	const mbedtls_md_info_t *md_info;
	unsigned char *p, *end, *hash;
	size_t len;
	int rc;

	INFO("%s: Called dlen = %d, digest_info_len = %d\n", __FUNCTION__,
	     data_len, digest_info_len);

	/* Digest info should be an MBEDTLS_ASN1_SEQUENCE */
	p = (unsigned char *)digest_info_ptr;
	end = p + digest_info_len;
	rc = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_CONSTRUCTED |
				  MBEDTLS_ASN1_SEQUENCE);
	if (rc != 0) {
		return CRYPTO_ERR_HASH;
	}

	/* Get the hash algorithm */
	rc = mbedtls_asn1_get_alg(&p, end, &hash_oid, &params);
	if (rc != 0) {
		return CRYPTO_ERR_HASH;
	}

	rc = mbedtls_oid_get_md_alg(&hash_oid, &md_alg);
	if (rc != 0) {
		return CRYPTO_ERR_HASH;
	}

	md_info = mbedtls_md_info_from_type(md_alg);
	if (md_info == NULL) {
		return CRYPTO_ERR_HASH;
	}

	/* Hash should be octet string type */
	rc = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_OCTET_STRING);
	if (rc != 0) {
		return CRYPTO_ERR_HASH;
	}

	/* Length of hash must match the algorithm's size */
	if (len != mbedtls_md_get_size(md_info)) {
		return CRYPTO_ERR_HASH;
	}
	hash = p;

	/* Calculate & check the hash of the data */
	rc = sha_verify(md_info->type, data_ptr, data_len,
			hash, len);

	return rc;
}

/*
 * Authenticated decryption of an image
 */
static int auth_decrypt(enum crypto_dec_algo dec_algo, void *data_ptr,
			size_t len, const void *key, unsigned int key_len,
			unsigned int key_flags, const void *iv,
			unsigned int iv_len, const void *tag,
			unsigned int tag_len)
{
	int rc;

	assert((key_flags & ENC_KEY_IS_IDENTIFIER) == 0);

	switch (dec_algo) {
	case CRYPTO_GCM_DECRYPT:
		rc = aes_gcm_decrypt(data_ptr, len, key, key_len, iv, iv_len,
				     tag, tag_len);
		if (rc != 0)
			return rc;
		break;
	default:
		return CRYPTO_ERR_DECRYPTION;
	}

	return CRYPTO_SUCCESS;
}

#if MEASURED_BOOT
/*
 * Calculate a hash
 *
 * output points to the computed hash
 */
static int calc_hash(unsigned int alg, void *data_ptr,
		     unsigned int data_len, unsigned char *output)
{
	const mbedtls_md_info_t *md_info;

	md_info = mbedtls_md_info_from_type((mbedtls_md_type_t)alg);
	if (md_info == NULL) {
		return CRYPTO_ERR_HASH;
	}

	return sha_calc(md_info->type, data_ptr, data_len, output);
}
#endif

/*
 * Register crypto library descriptor
 */
#if MEASURED_BOOT
REGISTER_CRYPTO_LIB(LIB_NAME, init, verify_signature, verify_hash, calc_hash, auth_decrypt);
#else
REGISTER_CRYPTO_LIB(LIB_NAME, init, verify_signature, verify_hash, auth_decrypt);
#endif
