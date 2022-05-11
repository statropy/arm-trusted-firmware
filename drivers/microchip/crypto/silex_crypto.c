/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <drivers/auth/crypto_mod.h>
#include <drivers/microchip/silex_crypto.h>
#include <silexpk/sxbuf/sxbufop.h>
#include <silexpk/sxops/eccweierstrass.h>

static struct sx_pk_cnx *gbl_cnx;
static struct sx_pk_ecurve nistp256_curve;

static void ntohl_sx(const sx_ecop *p)
{
	uint8_t *front = (uint8_t *) p->bytes,
		*back = (uint8_t *) p->bytes + p->sz - 1;

	for (; front < back; front++, back--) {
		uint8_t b = *front;

		*front = *back;
		*back = b;
	}
}

/** Asynchronous (non-blocking) ECDSA verification.
 *
 * Start an ECDSA signature verification on the accelerator
 * and return immediately.
 *
 * @remark When the operation finishes on the accelerator,
 * call sx_pk_release_req()
 *
 *
 * @param[in] curve Elliptic curve on which to perform ECDSA verification
 * @param[in] q The public key Q
 * @param[in] r First part of signature to verify
 * @param[in] s Second part of signature to verify
 * @param[in] h Digest of message to be signed
 *
 * @return Acquired acceleration request for this operation
 */
static
struct sx_pk_acq_req mbed_sx_async_ecdsa_verify_go(
	const struct sx_pk_ecurve *curve,
	const mbedtls_ecp_keypair *kp,
	const mbedtls_mpi *r,  const mbedtls_mpi *s,
	const unsigned char *hash, size_t hash_len)
{
	struct sx_pk_acq_req pkreq;
	struct sx_pk_inops_ecdsa_verify inputs;

	sx_ecop sx_q_x = {
		mbedtls_mpi_size(&kp->Q.X), (char*) kp->Q.X.p
	}, sx_q_y = {
		mbedtls_mpi_size(&kp->Q.Y), (char*) kp->Q.Y.p
	}, sx_r = {
		mbedtls_mpi_size(r), (char *) r->p
	}, sx_s = {
		mbedtls_mpi_size(s), (char *) s->p
	}, sx_h = {
		hash_len, (char*) hash
	};
	sx_pk_affine_point sx_q = { .x = &sx_q_x, .y = &sx_q_y };
	/* The following data come from mbedTLS which use LE */
	ntohl_sx(&sx_q_x);
	ntohl_sx(&sx_q_y);
	ntohl_sx(&sx_r);
	ntohl_sx(&sx_s);
	/* Hash already in correct order */
	//ntohl_sx(&sx_h);

	pkreq = sx_pk_acquire_req(curve->cnx, SX_PK_CMD_ECDSA_VER);
	if (pkreq.status)
		return pkreq;

	// convert and transfer operands
	pkreq.status = sx_pk_list_ecc_inslots(pkreq.req, curve, 0,
					      (struct sx_pk_slot*)&inputs);
	if (pkreq.status)
		return pkreq;
	int opsz = sx_pk_get_opsize(pkreq.req);
	sx_pk_affpt2mem(&sx_q, inputs.qx.addr, inputs.qy.addr, opsz);
	sx_pk_ecop2mem(&sx_r, inputs.r.addr, opsz);
	sx_pk_ecop2mem(&sx_s, inputs.s.addr, opsz);
	sx_pk_ecop2mem(&sx_h, inputs.h.addr, opsz);

	sx_pk_run(pkreq.req);

	return pkreq;
}

/** Verify ECDSA signature on an elliptic curve
 *
 *  The verification has the following steps:
 *   1. check qx and qy are smaller than q from the domain
 *   2. Check that Q lies on the elliptic curve from the domain
 *   3. Check that r and s are smaller than n
 *   4.  w = s ^ -1 mod n
 *   5.  u1 = h * w mod n
 *   6.  u2 = r * w mod n
 *   7.  X(x1, y1) = u1 * G + u2 * Q
 *   8. If X is invalid, then the signature is invalid
 *   9.  v = x1 mod n
 *   10. Accept signature if and only if v == r
 *
 *
 * @param[in] curve Elliptic curve on which to perform ECDSA verification
 * @param[in] q The public key Q
 * @param[in] r First part of signature to verify
 * @param[in] s Second part of signature to verify
 * @param[in] h Digest of message to be signed
 *
 * @return ::SX_OK
 * @return ::SX_ERR_INVALID_PARAM
 * @return ::SX_ERR_NOT_INVERTIBLE
 * @return ::SX_ERR_INVALID_SIGNATURE
 * @return ::SX_ERR_OUT_OF_RANGE
 * @return ::SX_ERR_UNKNOWN_ERROR
 * @return ::SX_ERR_BUSY
 * @return ::SX_ERR_NOT_IMPLEMENTED
 * @return ::SX_ERR_OPERAND_TOO_LARGE
 * @return ::SX_ERR_PLATFORM_ERROR
 * @return ::SX_ERR_EXPIRED
 * @return ::SX_ERR_PK_RETRY
 *
 */
static int mbed_sx_ecdsa_verify(
        const struct sx_pk_ecurve *curve,
	const mbedtls_ecp_keypair *kp,
	const mbedtls_mpi *r,  const mbedtls_mpi *s,
	const unsigned char *hash, size_t hash_len)
{
	uint32_t status;
	struct sx_pk_acq_req pkreq;

	pkreq = mbed_sx_async_ecdsa_verify_go(curve, kp, r, s, hash, hash_len);
	if (pkreq.status)
		return pkreq.status;

	status = sx_pk_wait(pkreq.req);
	sx_pk_release_req(pkreq.req);

	return status;
}

int silex_crypto_ecdsa_verify_signature(mbedtls_pk_type_t type,
					const mbedtls_ecp_keypair *kp,
					const mbedtls_mpi *r,  const mbedtls_mpi *s,
					const unsigned char *hash, size_t hash_len)
{
	/* Only ECDSA signature */
	if (type != MBEDTLS_PK_ECDSA) {
		ERROR("verify: Want ECDSA only\n");
		return CRYPTO_ERR_SIGNATURE;
	}

	if (kp->grp.id != MBEDTLS_ECP_DP_SECP256R1) {
		ERROR("verify: Want ECDSA group secp256r1\n");
		return CRYPTO_ERR_SIGNATURE;
	}

	return mbed_sx_ecdsa_verify(&nistp256_curve, kp, r, s, hash, hash_len);
}

void silex_init(void)
{
	struct sx_pk_config cfg = { .maxpending = 1 };

	/* Init Silex */
	gbl_cnx = sx_pk_open(&cfg);

	/* Get curve data once for all */
	nistp256_curve = sx_pk_get_curve_nistp256(gbl_cnx);
}
