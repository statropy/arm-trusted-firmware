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

#define LIB_NAME		"SAMA5 crypto core"

static const uint32_t hash_sha1_1[] = { 0x2fd4e1c6, 0x7a2d28fc, 0xed849ee1, 0xbb76e739, 0x1b93eb12 };
static const uint32_t hash_sha1_2[] = { 0xa9993e36, 0x4706816a, 0xba3e2571, 0x7850c26c, 0x9cd0d89d };
static const char inp_sha1_1[] = "The quick brown fox jumps over the lazy dog";
static const char inp_sha1_2[] = "abc";

static struct {
	int hash_type;
	const void *data;
	size_t len;
	const void *hash;
	size_t hash_len;
} testdata[] = {
	{ MBEDTLS_MD_SHA1, inp_sha1_1, sizeof(inp_sha1_1)-1, hash_sha1_1, sizeof(hash_sha1_1) },
	{ MBEDTLS_MD_SHA1, inp_sha1_2, sizeof(inp_sha1_2)-1, hash_sha1_2, sizeof(hash_sha1_2) },
};

static void init(void)
{
	int i;

	sha_init();
	for (i = 0; i < ARRAY_SIZE(testdata); i++) {
		sha_verify(testdata[i].hash_type,
			   testdata[i].data,
			   testdata[i].len,
			   testdata[i].hash,
			   testdata[i].hash_len);
	}
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

static int aes_gcm_decrypt(void *data_ptr, size_t len, const void *key,
			   unsigned int key_len, const void *iv,
			   unsigned int iv_len, const void *tag,
			   unsigned int tag_len)
{
	return CRYPTO_ERR_DECRYPTION;
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
