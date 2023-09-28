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

#define LIB_NAME		"LAN966X crypto core"

#define BYTES_TO_T_UINT_4(a, b, c, d)		\
	((mbedtls_mpi_uint) (a) <<  0) |	\
	((mbedtls_mpi_uint) (b) <<  8) |	\
	((mbedtls_mpi_uint) (c) << 16) |	\
	((mbedtls_mpi_uint) (d) << 24)

#define BYTES_TO_T_UINT_8(a, b, c, d, e, f, g, h)	\
	BYTES_TO_T_UINT_4(a, b, c, d),			\
	BYTES_TO_T_UINT_4(e, f, g, h)

static const mbedtls_mpi_uint default_a[] = {
	BYTES_TO_T_UINT_8(0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff),
	BYTES_TO_T_UINT_8(0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00),
	BYTES_TO_T_UINT_8(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
	BYTES_TO_T_UINT_8(0x01, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff),
};

static inline void ecp_mpi_load(mbedtls_mpi *X, const mbedtls_mpi_uint *p, size_t len)
{
	X->s = 1;
	X->n = len / sizeof(mbedtls_mpi_uint);
	X->p = (mbedtls_mpi_uint *) p;
}

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

	ret = mbedtls_asn1_get_alg(p, end, &alg_oid, params);
	if (ret != 0)
		return ret;

	if (mbedtls_oid_get_pk_alg(&alg_oid, pk_alg) != 0)
		return MBEDTLS_ERR_PK_UNKNOWN_PK_ALG;

	/*
	 * No parameters with RSA (only for EC)
	 */
	if (*pk_alg == MBEDTLS_PK_RSA &&
	    ((params->tag != MBEDTLS_ASN1_NULL && params->tag != 0) ||
	     params->len != 0))
		return MBEDTLS_ERR_PK_INVALID_ALG;

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
	 * grp may already be initialized; if so, make sure IDs match
	 */
	if (grp->id != MBEDTLS_ECP_DP_NONE && grp->id != grp_id)
		return MBEDTLS_ERR_PK_KEY_INVALID_FORMAT;

	ret = mbedtls_ecp_group_load(grp, grp_id);
	if (ret != 0)
		return ret;

	if (grp->A.p == NULL)
		ecp_mpi_load(&grp->A, default_a, sizeof(default_a));

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

/*
 * Note: This is partially copied from mbedtls_pk_parse_subpubkey() in
 * order to just extract the curve group and key material, not
 * initialize a full context - which pulls is *all* of the mbedtls
 * software ECDSA implementation (which we are not going to use). The
 * previous functions are needed since they are static members in
 * mbedtls.
 */
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

	ret = lan966x_get_pk_alg(p, end, &pk_alg, &alg_params);
	if (ret != 0)
		return ret;

	ret = mbedtls_asn1_get_bitstring_null(p, end, &len);
	if (ret != 0)
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

static int lan966x_ecdsa_read_signature(const unsigned char *sig,
					size_t slen,
					mbedtls_mpi *r, mbedtls_mpi *s)
{
	int ret;
	unsigned char *p = (unsigned char *) sig;
	const unsigned char *end = sig + slen;
	size_t len;

	mbedtls_mpi_init(r);
	mbedtls_mpi_init(s);

	if ((ret = mbedtls_asn1_get_tag(&p, end, &len,
					MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0)
		return ret += MBEDTLS_ERR_ECP_BAD_INPUT_DATA;

	if (p + len != end)
		return ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA +
			MBEDTLS_ERR_ASN1_LENGTH_MISMATCH;

	if ((ret = mbedtls_asn1_get_mpi(&p, end, r)) != 0 ||
	    (ret = mbedtls_asn1_get_mpi(&p, end, s)) != 0)
		ret += MBEDTLS_ERR_ECP_BAD_INPUT_DATA;

	return ret;
}

static inline lan966x_sha_type_t lan966x_shatype(const mbedtls_md_info_t *md_info)
{
	switch (md_info->type) {
	case MBEDTLS_MD_SHA1:
		return SHA_MR_ALGO_SHA1;
	case MBEDTLS_MD_SHA224:
		return SHA_MR_ALGO_SHA224;
	case MBEDTLS_MD_SHA256:
		return SHA_MR_ALGO_SHA256;
	case MBEDTLS_MD_SHA384:
		return SHA_MR_ALGO_SHA384;
	case MBEDTLS_MD_SHA512:
		return SHA_MR_ALGO_SHA512;
	default:
		break;
	}
	return -1;
}

/*
 * Verify a signature.
 *
 * Parameters are passed using the DER encoding format following the ASN.1
 * structures of the associated certificates.
 */
static int verify_signature(void *data_ptr, unsigned int data_len,
			    void *sig_ptr, unsigned int sig_len,
			    void *sig_alg, unsigned int sig_alg_len,
			    void *pk_ptr, unsigned int pk_len)
{
	int ret;
	mbedtls_asn1_buf sig_oid, sig_params;
	mbedtls_asn1_buf signature;
	mbedtls_md_type_t md_alg;
	mbedtls_pk_type_t pk_alg;
	uint8_t *p, *end;
	unsigned char hash[MBEDTLS_MD_MAX_SIZE];
	void *sig_opts = NULL;
	const mbedtls_md_info_t *md_info;
	mbedtls_ecp_keypair kp;
	mbedtls_mpi r, s;

	/* Verify the signature algorithm */
	/* Get pointers to signature OID and parameters */
	p = sig_alg;
	end = p + sig_alg_len;
	ret = mbedtls_asn1_get_alg(&p, end, &sig_oid, &sig_params);
	if (ret != 0)
		return CRYPTO_ERR_SIGNATURE;

	/* Get the actual signature algorithm (MD + PK) */
	ret = mbedtls_x509_get_sig_alg(&sig_oid, &sig_params, &md_alg, &pk_alg, &sig_opts);
	if (ret != 0) {
		return CRYPTO_ERR_SIGNATURE;
	}

	/* General case: no options */
	if (sig_opts != NULL) {
		ret = CRYPTO_ERR_SIGNATURE;
		goto end2;
	}

	p = (unsigned char *)pk_ptr;
	end = (unsigned char *)(p + pk_len);
	mbedtls_ecp_keypair_init(&kp);
	ret = lan966x_pk_parse_subpubkey(&p, end, &kp);
	if (ret != 0) {
		ret = CRYPTO_ERR_SIGNATURE;
		goto end2;
	}

	/* Get the signature (bitstring) */
	p = (unsigned char *)sig_ptr;
	end = (unsigned char *)(p + sig_len);
	signature.tag = *p;
	ret = mbedtls_asn1_get_bitstring_null(&p, end, &signature.len);
	if (ret != 0) {
		ret = CRYPTO_ERR_SIGNATURE;
		goto end1;
	}
	signature.p = p;

	/* Calculate the hash of the data */
	md_info = mbedtls_md_info_from_type(md_alg);
	if (md_info == NULL) {
		ret = CRYPTO_ERR_SIGNATURE;
		goto end1;
	}
	ret = sha_calc(lan966x_shatype(md_info), data_ptr, data_len, hash);
	if (ret != 0) {
		ret = CRYPTO_ERR_SIGNATURE;
		goto end1;
	}

	if (lan966x_ecdsa_read_signature(signature.p, signature.len, &r, &s) == 0)
		ret = pkcl_ecdsa_verify_signature(pk_alg, &kp, &r, &s,
						  hash, mbedtls_md_get_size(md_info));
	else
		ret = CRYPTO_ERR_SIGNATURE;

	mbedtls_mpi_free(&r);
	mbedtls_mpi_free(&s);
end1:
	mbedtls_ecp_keypair_free(&kp);

end2:
	mbedtls_free(sig_opts);

	return ret;
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
	int ret;

	/* Digest info should be an MBEDTLS_ASN1_SEQUENCE */
	p = (unsigned char *)digest_info_ptr;
	end = p + digest_info_len;
	ret = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_CONSTRUCTED |
				   MBEDTLS_ASN1_SEQUENCE);
	if (ret != 0) {
		return CRYPTO_ERR_HASH;
	}

	/* Get the hash algorithm */
	ret = mbedtls_asn1_get_alg(&p, end, &hash_oid, &params);
	if (ret != 0) {
		return CRYPTO_ERR_HASH;
	}

	ret = mbedtls_oid_get_md_alg(&hash_oid, &md_alg);
	if (ret != 0) {
		return CRYPTO_ERR_HASH;
	}

	md_info = mbedtls_md_info_from_type(md_alg);
	if (md_info == NULL) {
		return CRYPTO_ERR_HASH;
	}

	/* Hash should be octet string type */
	ret = mbedtls_asn1_get_tag(&p, end, &len, MBEDTLS_ASN1_OCTET_STRING);
	if (ret != 0) {
		return CRYPTO_ERR_HASH;
	}

	/* Length of hash must match the algorithm's size */
	if (len != mbedtls_md_get_size(md_info)) {
		return CRYPTO_ERR_HASH;
	}
	hash = p;

	/* Calculate & check the hash of the data */
	ret = sha_verify(lan966x_shatype(md_info), data_ptr, data_len,
			 hash, len);

	return ret;
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
	int ret;

	assert((key_flags & ENC_KEY_IS_IDENTIFIER) == 0);

	switch (dec_algo) {
	case CRYPTO_GCM_DECRYPT:
		ret = aes_gcm_decrypt(data_ptr, len, key, key_len, iv, iv_len,
				      tag, tag_len);
		if (ret != 0)
			return ret;
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

	return sha_calc(lan966x_shatype(md_info), data_ptr, data_len, output);
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
