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
#include <mbedtls/platform.h>
#include <mbedtls/x509.h>

#include "pkcl.h"
#include "sha.h"
#include "aes.h"

#define LIB_NAME		"SAMA5 crypto core"

static void init(void)
{

	sha_init();
	aes_init();
	pkcl_init();
}

static int lan966x_get_pk_alg(unsigned char **p,
			 const unsigned char *end,
			 mbedtls_pk_type_t *pk_alg,
			 mbedtls_asn1_buf *params)
{
	int ret;
	mbedtls_asn1_buf alg_oid;

	memset(params, 0, sizeof(mbedtls_asn1_buf));

	if ((ret = mbedtls_asn1_get_alg(p, end, &alg_oid, params)) != 0)
		return ret;

	if (mbedtls_oid_get_pk_alg(&alg_oid, pk_alg) != 0)
		return MBEDTLS_ERR_PK_UNKNOWN_PK_ALG;

	/*
	 * No parameters with RSA (only for EC)
	 */
	if (*pk_alg == MBEDTLS_PK_RSA &&
            ((params->tag != MBEDTLS_ASN1_NULL && params->tag != 0) ||
	     params->len != 0))
		return ret;

	return 0;
}

static int lan966x_use_ecparams(const mbedtls_asn1_buf *params, mbedtls_ecp_group *grp)
{
	int ret;
	mbedtls_ecp_group_id grp_id;

	if (params->tag == MBEDTLS_ASN1_OID) {
		if (mbedtls_oid_get_ec_grp(params, &grp_id) != 0)
			return MBEDTLS_ERR_PK_UNKNOWN_NAMED_CURVE;
	} else {
		return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;
	}

	/*
	 * grp may already be initilialized; if so, make sure IDs match
	 */
	if (grp->id != MBEDTLS_ECP_DP_NONE && grp->id != grp_id)
		return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;

	if ((ret = mbedtls_ecp_group_load(grp, grp_id)) != 0)
		return ret;

	return 0;
}

static int lan966x_get_ecpubkey(unsigned char **p,
				 const unsigned char *end,
				 mbedtls_ecp_keypair *key)
{
	int ret;

	if ((ret = mbedtls_ecp_point_read_binary(&key->grp, &key->Q,
						   (const unsigned char *) *p, end - *p)) == 0)
		ret = mbedtls_ecp_check_pubkey(&key->grp, &key->Q);

	/*
	 * We know mbedtls_ecp_point_read_binary consumed all bytes or failed
	 */
	*p = (unsigned char *) end;

	return ret;
}

static int lan966x_pk_parse_subpubkey(unsigned char **p,
				      const unsigned char *end,
				      mbedtls_ecp_keypair *kp)
{
	int ret;
	size_t len;
	mbedtls_asn1_buf alg_params;
	mbedtls_pk_type_t pk_alg = MBEDTLS_PK_NONE;

	if ((ret = mbedtls_asn1_get_tag(p, end, &len,
					MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
		return ret;
	}

	end = *p + len;

	if ((ret = lan966x_get_pk_alg(p, end, &pk_alg, &alg_params)) != 0)
		return ret;

	if ((ret = mbedtls_asn1_get_bitstring_null(p, end, &len)) != 0)
		return ret;

	if (*p + len != end)
		return MBEDTLS_ERR_PK_INVALID_PUBKEY + MBEDTLS_ERR_ASN1_LENGTH_MISMATCH;

	if (pk_alg == MBEDTLS_PK_ECKEY_DH || pk_alg == MBEDTLS_PK_ECKEY) {
		ret = lan966x_use_ecparams(&alg_params, &kp->grp);
		if (ret == 0)
			ret = lan966x_get_ecpubkey(p, end, kp);
	} else
		ret = MBEDTLS_ERR_PK_UNKNOWN_PK_ALG;

	if (ret == 0 && *p != end)
		ret = MBEDTLS_ERR_PK_INVALID_PUBKEY +
			MBEDTLS_ERR_ASN1_LENGTH_MISMATCH;

	return ret;
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
	int rc;
	mbedtls_asn1_buf sig_oid, sig_params;
	mbedtls_asn1_buf signature;
	mbedtls_md_type_t md_alg;
	mbedtls_pk_type_t pk_alg;
	uint8_t *p, *end;
	unsigned char hash[MBEDTLS_MD_MAX_SIZE];
	void *sig_opts = NULL;
	const mbedtls_md_info_t *md_info;
	//mbedtls_pk_context _pk = { 0 };
	mbedtls_ecp_keypair kp;

	/* Signature verification success */
	INFO("%s: Called dlen = %d, siglen = %d, sig_alg_len = %d, pk_len = %d\n",
	     __FUNCTION__,
	     data_len, sig_len, sig_alg_len, pk_len);

	/* Verify the signature algorithm */
	/* Get pointers to signature OID and parameters */
	p = sig_alg;
	end = p + sig_alg_len;
	rc = mbedtls_asn1_get_alg(&p, end, &sig_oid, &sig_params);
	if (rc != 0)
		return CRYPTO_ERR_SIGNATURE;

	/* Get the actual signature algorithm (MD + PK) */
	rc = mbedtls_x509_get_sig_alg(&sig_oid, &sig_params, &md_alg, &pk_alg, &sig_opts);
	if (rc != 0) {
		return CRYPTO_ERR_SIGNATURE;
	}

	INFO("verify: pk_alg %d, md_alg %d\n", pk_alg, md_alg);

	p = (unsigned char *)pk_ptr;
	end = (unsigned char *)(p + pk_len);
	mbedtls_ecp_keypair_init(&kp);
	rc = lan966x_pk_parse_subpubkey(&p, end, &kp);
	//rc = mbedtls_pk_parse_subpubkey(&p, end, &_pk);
	if (rc != 0) {
		rc = CRYPTO_ERR_SIGNATURE;
		goto end2;
	}

	/* Get the signature (bitstring) */
	p = (unsigned char *)sig_ptr;
	end = (unsigned char *)(p + sig_len);
	signature.tag = *p;
	rc = mbedtls_asn1_get_bitstring_null(&p, end, &signature.len);
	if (rc != 0) {
		rc = CRYPTO_ERR_SIGNATURE;
		goto end1;
	}
	signature.p = p;

	/* Calculate the hash of the data */
	md_info = mbedtls_md_info_from_type(md_alg);
	if (md_info == NULL) {
		rc = CRYPTO_ERR_SIGNATURE;
		goto end1;
	}
	rc = sha_calc(md_info->type, data_ptr, data_len, hash);
	if (rc != 0) {
		rc = CRYPTO_ERR_SIGNATURE;
		goto end1;
	}

	/* General case: no options */
	if (sig_opts != NULL) {
		rc = CRYPTO_ERR_SIGNATURE;
		goto end1;
	}

	rc = pkcl_ecdsa_verify_signature(pk_alg, &kp, md_alg, hash,
					 mbedtls_md_get_size(md_info),
					 signature.p, signature.len);

end1:
	mbedtls_ecp_keypair_free(&kp);
end2:
	mbedtls_free(sig_opts);

	return rc;
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
		INFO("%s: Called len = %d, key_len = %d, flags 0x%x - ret = %d\n", __FUNCTION__,
		     len, key_len, key_flags, rc);
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
