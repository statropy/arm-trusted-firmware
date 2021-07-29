/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/auth/crypto_mod.h>
#include <mbedtls/ecdsa.h>

#include "inc/CryptoLib_Headers_pb.h"
#include "inc/CryptoLib_JumpTable_pb.h"

#include <pkcl.h>

#define NEARTOFAR(x) ((void*)(LAN996X_PKCL_RAM_BASE + (x)))

//******************************************************************************
// Memory mapping for ECDSA signature verification
//******************************************************************************
#define BASE_ECDSAV_MODULO(a,b)         (nu1) 0x0
#define BASE_ECDSAV_CNS(a,b)            (nu1) (BASE_ECDSAV_MODULO(a,b) + a + 4)
#define BASE_ECDSAV_ORDER(a,b)          (nu1) (BASE_ECDSAV_CNS(a,b) + b + 12)
#define BASE_ECDSAV_SIGNATURE(a,b)      (nu1) (BASE_ECDSAV_ORDER(a,b) + b + 8)
#define BASE_ECDSAV_HASH(a,b)           (nu1) (BASE_ECDSAV_SIGNATURE(a,b) + 2*b + 8)
#define BASE_ECDSAV_POINT_A(a,b)        (nu1) (BASE_ECDSAV_HASH(a,b) + b + 4)
#define BASE_ECDSAV_POINT_A_X(a,b)      (nu1) (BASE_ECDSAV_POINT_A(a,b))
#define BASE_ECDSAV_POINT_A_Y(a,b)      (nu1) (BASE_ECDSAV_POINT_A_X(a,b) + a + 4)
#define BASE_ECDSAV_POINT_A_Z(a,b)      (nu1) (BASE_ECDSAV_POINT_A_Y(a,b) + a + 4)
#define BASE_ECDSAV_PUBLIC_KEY(a,b)     (nu1) (BASE_ECDSAV_POINT_A_Z(a,b) + a + 4)
#define BASE_ECDSAV_PUBLIC_KEY_X(a,b)   (nu1) (BASE_ECDSAV_PUBLIC_KEY(a,b))
#define BASE_ECDSAV_PUBLIC_KEY_Y(a,b)   (nu1) (BASE_ECDSAV_PUBLIC_KEY_X(a,b) + a + 4)
#define BASE_ECDSAV_PUBLIC_KEY_Z(a,b)   (nu1) (BASE_ECDSAV_PUBLIC_KEY_Y(a,b) + a + 4)
#define BASE_ECDSAV_A(a,b)              (nu1) (BASE_ECDSAV_PUBLIC_KEY_Z(a,b) + a + 4)
#define BASE_ECDSAV_WORKSPACE(a,b)      (nu1) (BASE_ECDSAV_A(a,b) + a + 4)

void pkcl_init(void)
{
	CPKCL_PARAM CPKCLParam;
	PCPKCL_PARAM pvCPKCLParam = &CPKCLParam;
	// vCPKCL_Process() is a macro command which fills-in the Command field
	// and then calls the library
	vCPKCL_Process(SelfTest, pvCPKCLParam);
	if (CPKCL(u2Status) == CPKCL_OK) {
		INFO("CPKCL V: 0x%08lx\n", CPKCL_SelfTest(u4Version));
	}
}

static void cpy_mpi(u2 offset, const mbedtls_mpi *mpi)
{
	memcpy(NEARTOFAR(offset), mpi->p, mbedtls_mpi_size(mpi));
}

/*static*/ void pkcl_ecdsa_verify_setup(PCPKCL_PARAM pvCPKCLParam,
					mbedtls_ecp_keypair *pubkey,
					const unsigned char *hash, size_t hash_len,
					const unsigned char *sig, size_t sig_len)
{
	u2 u2ModuloPSize = pubkey->grp.pbits / 8;
	u2 u2OrderSize   = pubkey->grp.nbits / 8;
	void *pStart, *pEnd;

	CPKCL(u2Option) = 0;

	/* Zero out parameter memory */
	pStart = NEARTOFAR(BASE_ECDSAV_MODULO(u2ModuloPSize,u2OrderSize));
	pEnd   = NEARTOFAR(BASE_ECDSAV_WORKSPACE(u2ModuloPSize,u2OrderSize));
	memset(pStart, 0, pEnd - pStart);

	/* Copies the signature into appropriate memory area */
	// Take care of the input signature format (???)
	memcpy(NEARTOFAR(BASE_ECDSAV_SIGNATURE(u2ModuloPSize,u2OrderSize)),
	       sig, sig_len);
	/* Hash (byteorder?) */
	memcpy(NEARTOFAR(BASE_ECDSAV_HASH(u2ModuloPSize,u2OrderSize)),
	       hash, hash_len);

	/* Mapping of mbedtls to PKCL:
	  +--------+-----+------+---------------+
	  |Element | Set | Used | Meaming/name  |
	  +--------+-----+------+---------------+
	  | grp.P  | Yes | Yes  | ECDSAV_MODULO |
	  +--------+-----+------+---------------+
	  | grp.A  | Nil | Yes  | ECDSAV_A (?)  |
	  +--------+-----+------+---------------+
	  | grp.B  | Yes | No   |               |
	  +--------+-----+------+---------------+
	  | grp.N  | Yes | Yes  | ECDSAV_ORDER  |
	  +--------+-----+------+---------------+
	  | grp.G  | Yes | Yes  | Generator Pt  |
	  +--------+-----+------+---------------+
	*/

	/* Copy in parameters */
	cpy_mpi(BASE_ECDSAV_MODULO(u2ModuloPSize,u2OrderSize), &pubkey->grp.P);
	cpy_mpi(BASE_ECDSAV_POINT_A_X(u2ModuloPSize,u2OrderSize), &pubkey->grp.G.X);
	cpy_mpi(BASE_ECDSAV_POINT_A_Y(u2ModuloPSize,u2OrderSize), &pubkey->grp.G.Y);
	cpy_mpi(BASE_ECDSAV_POINT_A_Z(u2ModuloPSize,u2OrderSize), &pubkey->grp.G.Z);
	cpy_mpi(BASE_ECDSAV_A(u2ModuloPSize,u2OrderSize), &pubkey->grp.A);
	cpy_mpi(BASE_ECDSAV_ORDER(u2ModuloPSize,u2OrderSize), &pubkey->grp.N);
	cpy_mpi(BASE_ECDSAV_PUBLIC_KEY_X(u2ModuloPSize,u2OrderSize), &pubkey->Q.X);
	cpy_mpi(BASE_ECDSAV_PUBLIC_KEY_Y(u2ModuloPSize,u2OrderSize), &pubkey->Q.Y);
	cpy_mpi(BASE_ECDSAV_PUBLIC_KEY_Z(u2ModuloPSize,u2OrderSize), &pubkey->Q.Z);

	// Ask for a Verify generation
	CPKCL_ZpEcDsaVerify(u2ModLength)          = u2ModuloPSize;
	CPKCL_ZpEcDsaVerify(u2ScalarLength)       = u2OrderSize;
	CPKCL_ZpEcDsaVerify(nu1ModBase)           = BASE_ECDSAV_MODULO(u2ModuloPSize,u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1CnsBase)           = BASE_ECDSAV_CNS(u2ModuloPSize,u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1PointABase)        = BASE_ECDSAV_POINT_A(u2ModuloPSize,u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1PointPublicKeyGen) = BASE_ECDSAV_PUBLIC_KEY(u2ModuloPSize,u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1PointSignature)    = BASE_ECDSAV_SIGNATURE(u2ModuloPSize,u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1OrderPointBase)    = BASE_ECDSAV_ORDER(u2ModuloPSize,u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1ABase)             = BASE_ECDSAV_A(u2ModuloPSize,u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1Workspace)         = BASE_ECDSAV_WORKSPACE(u2ModuloPSize,u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1HashBase)          = BASE_ECDSAV_HASH(u2ModuloPSize,u2OrderSize);
}

int pkcl_ecdsa_verify_signature(mbedtls_pk_type_t type,
				mbedtls_ecp_keypair *pubkey,
				mbedtls_md_type_t md_alg,
				const unsigned char *hash, size_t hash_len,
				const unsigned char *sig, size_t sig_len)
{
	CPKCL_PARAM CPKCLParam;
	PCPKCL_PARAM pvCPKCLParam = &CPKCLParam;
	int ret;

	/* Only ECDSA signature */
	if (type != MBEDTLS_PK_ECDSA) {
		INFO("verify: Want ECDSA only\n");
		return CRYPTO_ERR_SIGNATURE;
	}

	if (pubkey->grp.id != MBEDTLS_ECP_DP_SECP256R1) {
		NOTICE("verify: Want ECDSA group secp256r1\n");
		return CRYPTO_ERR_SIGNATURE;
	}

	pkcl_ecdsa_verify_setup(pvCPKCLParam, pubkey,
				hash, hash_len, sig, sig_len);

	vCPKCL_Process(ZpEcDsaVerifyFast, pvCPKCLParam);

	switch (CPKCL(u2Status)) {
	case CPKCL_OK:
		INFO("verify: All OK\n");
		ret = CRYPTO_SUCCESS;
		break;

	default:
		NOTICE("verify: Unexpected rc: %0x\n", CPKCL(u2Status));
	case CPKCL_WRONG_SIGNATURE:
		INFO("verify: Not OK\n");
		ret = CRYPTO_ERR_SIGNATURE;
		break;
	}

	return ret;
}
