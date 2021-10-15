/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/auth/crypto_mod.h>
#include <lib/mmio.h>
#include <mbedtls/bignum.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/error.h>

#include "inc/CryptoLib_Headers_pb.h"
#include "inc/CryptoLib_JumpTable_pb.h"

#include <pkcl.h>

#define PKCL_RAM_OFFSET 0x1000

#define NEARTOFAR(x) ((void *)(LAN966X_PKCL_RAM_BASE - PKCL_RAM_OFFSET + (x)))

//******************************************************************************
// Memory mapping for ECDSA signature
//******************************************************************************
#define BASE_ECDSA_MODULO(a, b)          (nu1) PKCL_RAM_OFFSET
#define BASE_ECDSA_CNS(a, b)             (nu1) (BASE_ECDSA_MODULO(a, b) + a + 4)
#define BASE_ECDSA_POINT_A(a, b)         (nu1) (BASE_ECDSA_CNS(a, b) + b + 12)
#define BASE_ECDSA_POINT_A_X(a, b)       (nu1) (BASE_ECDSA_POINT_A(a, b))
#define BASE_ECDSA_POINT_A_Y(a, b)       (nu1) (BASE_ECDSA_POINT_A_X(a, b) + a + 4)
#define BASE_ECDSA_POINT_A_Z(a, b)       (nu1) (BASE_ECDSA_POINT_A_Y(a, b) + a + 4)
#define BASE_ECDSA_A(a, b)               (nu1) (BASE_ECDSA_POINT_A_Z(a, b) + a + 4)
#define BASE_PRIVATE_KEY(a, b)           (nu1) (BASE_ECDSA_A(a, b) + a + 4)
#define BASE_ECDSA_SCALAR(a, b)          (nu1) (BASE_PRIVATE_KEY(a, b) + b + 4)
#define BASE_ECDSA_ORDER(a, b)           (nu1) (BASE_ECDSA_SCALAR(a, b) + b + 4)
#define BASE_ECDSA_HASH(a, b)            (nu1) (BASE_ECDSA_ORDER(a, b) + b + 4)
#define BASE_ECDSA_WORKSPACE(a, b)       (nu1) (BASE_ECDSA_HASH(a, b) + b + 4)

//******************************************************************************
// Memory mapping for ECDSA signature verification
//******************************************************************************
#define BASE_ECDSAV_MODULO(a, b)         (nu1) PKCL_RAM_OFFSET
#define BASE_ECDSAV_CNS(a, b)            (nu1) (BASE_ECDSAV_MODULO(a, b) + a + 4)
#define BASE_ECDSAV_ORDER(a, b)          (nu1) (BASE_ECDSAV_CNS(a, b) + b + 12)
#define BASE_ECDSAV_SIGNATURE(a, b)      (nu1) (BASE_ECDSAV_ORDER(a, b) + b + 8)
#define BASE_ECDSAV_HASH(a, b)           (nu1) (BASE_ECDSAV_SIGNATURE(a, b) + 2*b + 8)
#define BASE_ECDSAV_POINT_A(a, b)        (nu1) (BASE_ECDSAV_HASH(a, b) + b + 4)
#define BASE_ECDSAV_POINT_A_X(a, b)      (nu1) (BASE_ECDSAV_POINT_A(a, b))
#define BASE_ECDSAV_POINT_A_Y(a, b)      (nu1) (BASE_ECDSAV_POINT_A_X(a, b) + a + 4)
#define BASE_ECDSAV_POINT_A_Z(a, b)      (nu1) (BASE_ECDSAV_POINT_A_Y(a, b) + a + 4)
#define BASE_ECDSAV_PUBLIC_KEY(a, b)     (nu1) (BASE_ECDSAV_POINT_A_Z(a, b) + a + 4)
#define BASE_ECDSAV_PUBLIC_KEY_X(a, b)   (nu1) (BASE_ECDSAV_PUBLIC_KEY(a, b))
#define BASE_ECDSAV_PUBLIC_KEY_Y(a, b)   (nu1) (BASE_ECDSAV_PUBLIC_KEY_X(a, b) + a + 4)
#define BASE_ECDSAV_PUBLIC_KEY_Z(a, b)   (nu1) (BASE_ECDSAV_PUBLIC_KEY_Y(a, b) + a + 4)
#define BASE_ECDSAV_A(a, b)              (nu1) (BASE_ECDSAV_PUBLIC_KEY_Z(a, b) + a + 4)
#define BASE_ECDSAV_WORKSPACE(a, b)      (nu1) (BASE_ECDSAV_A(a, b) + a + 4)

// CPKCCSR
#define OffCPKCCSR		(0x00000040)
#define PKCC_SR			(0xE0000000 + OffCPKCCSR)

#define BIT_CPKCCSR_BUSY         0x00000001
#define BIT_CPKCCSR_CACHE        0x00000002
#define BIT_CPKCCSR_CARRY        0x00000004
#define BIT_CPKCCSR_ZERO         0x00000008
#define BIT_CPKCCSR_RAMV         0x00000010
#define BIT_CPKCCSR_SHAREV       0x00000020
#define BIT_CPKCCSR_CLRRAM_BUSY  0x00000040

void pkcl_init(void)
{
	CPKCL_PARAM CPKCLParam;
	PCPKCL_PARAM pvCPKCLParam = &CPKCLParam;

	/* Step 1: Wait for CPKCC RAM clear */
	while (mmio_read_32(PKCC_SR) & BIT_CPKCCSR_CLRRAM_BUSY)
		;

	// vCPKCL_Process() is a macro command which fills-in the Command field
	// and then calls the library
	vCPKCL_Process(SelfTest, pvCPKCLParam);
	if (CPKCL(u2Status) == CPKCL_OK) {
		INFO("CPKCL V: 0x%08lx\n", CPKCL_SelfTest(u4Version));
	} else {
		ERROR("CPKCL error: 0x%08x\n", CPKCL(u2Status));
		panic();
	}
}

static void cpy_mpi(uint16_t offset, const mbedtls_mpi *mpi)
{
	memcpy(NEARTOFAR(offset), &mpi->p[0], mbedtls_mpi_size(mpi));
}

/*
 * Derive a suitable integer for group grp from a buffer of length len
 * SEC1 4.1.3 step 5 aka SEC1 4.1.4 step 3
 */
static int derive_mpi(const mbedtls_ecp_group *grp, mbedtls_mpi *x,
		      const unsigned char *buf, size_t blen)
{
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    size_t n_size = (grp->nbits + 7) / 8;
    size_t use_size = blen > n_size ? n_size : blen;

    mbedtls_mpi_init(x);

    MBEDTLS_MPI_CHK(mbedtls_mpi_read_binary(x, buf, use_size));
    if (use_size * 8 > grp->nbits)
	    MBEDTLS_MPI_CHK(mbedtls_mpi_shift_r(x, use_size * 8 - grp->nbits));

    /* While at it, reduce modulo N */
    if (mbedtls_mpi_cmp_mpi(x, &grp->N) >= 0)
	    MBEDTLS_MPI_CHK(mbedtls_mpi_sub_mpi(x, x, &grp->N));

cleanup:
    return (ret);
}

void pkcl_ecdsa_verify_setup(PCPKCL_PARAM pvCPKCLParam,
			     mbedtls_ecp_keypair *pubkey,
			     const mbedtls_mpi *r, const mbedtls_mpi *s,
			     const mbedtls_mpi *h)
{
	u2 u2ModuloPSize = pubkey->grp.pbits / 8;
	u2 u2OrderSize   = pubkey->grp.nbits / 8;
	uint16_t beg, end;

	CPKCL(u2Option) = 0;

	/* Zero out parameter memory */
	beg = BASE_ECDSAV_MODULO(u2ModuloPSize, u2OrderSize);
	end = BASE_ECDSAV_WORKSPACE(u2ModuloPSize, u2OrderSize);
	memset(NEARTOFAR(beg), 0, end - beg);

	/* Copies the signature into appropriate memory area */
	// Take care of the input signature format (???)
	/* Copy R, S with 4 bytes zero spacing */
	beg = BASE_ECDSAV_SIGNATURE(u2ModuloPSize, u2OrderSize);
	cpy_mpi(beg, r);
	beg += u2OrderSize + 4;
	cpy_mpi(beg, s);

	/* Hash (byteorder?) */
	beg = BASE_ECDSAV_HASH(u2ModuloPSize, u2OrderSize);
	cpy_mpi(beg, h);

	/* Mapping of mbedtls to PKCL:
	  +--------+-----+------+---------------+---------------+
	  |Element | Set | Used | Meaning/name  | Demo name     |
	  +--------+-----+------+---------------+---------------+
	  | grp.P  | Yes | Yes  | ECDSAV_MODULO | au1ModuloP    |
	  +--------+-----+------+---------------+---------------+
	  | grp.A  | Nil | Yes  | ECDSAV_A      | au1ACurve     |
	  +--------+-----+------+---------------+---------------+
	  | grp.B  | Yes | No   |               | au1BCurve     |
	  +--------+-----+------+---------------+---------------+
	  | grp.N  | Yes | Yes  | ECDSAV_ORDER  | au1OrderPoint |
	  +--------+-----+------+---------------+---------------+
	  | grp.G  | Yes | Yes  | Generator Pt  | au1PtA_X/Y/Z  |
	  +--------+-----+------+---------------+---------------+
	*/

	/* Copy in parameters */
	cpy_mpi(BASE_ECDSAV_MODULO(u2ModuloPSize, u2OrderSize), &pubkey->grp.P);
	cpy_mpi(BASE_ECDSAV_POINT_A_X(u2ModuloPSize, u2OrderSize), &pubkey->grp.G.X);
	cpy_mpi(BASE_ECDSAV_POINT_A_Y(u2ModuloPSize, u2OrderSize), &pubkey->grp.G.Y);
	cpy_mpi(BASE_ECDSAV_POINT_A_Z(u2ModuloPSize, u2OrderSize), &pubkey->grp.G.Z);
	cpy_mpi(BASE_ECDSAV_A(u2ModuloPSize, u2OrderSize), &pubkey->grp.A);
	cpy_mpi(BASE_ECDSAV_ORDER(u2ModuloPSize, u2OrderSize), &pubkey->grp.N);
	cpy_mpi(BASE_ECDSAV_PUBLIC_KEY_X(u2ModuloPSize, u2OrderSize), &pubkey->Q.X);
	cpy_mpi(BASE_ECDSAV_PUBLIC_KEY_Y(u2ModuloPSize, u2OrderSize), &pubkey->Q.Y);
	cpy_mpi(BASE_ECDSAV_PUBLIC_KEY_Z(u2ModuloPSize, u2OrderSize), &pubkey->Q.Z);

	// Ask for a Verify generation
	CPKCL_ZpEcDsaVerify(u2ModLength)          = u2ModuloPSize;
	CPKCL_ZpEcDsaVerify(u2ScalarLength)       = u2OrderSize;
	CPKCL_ZpEcDsaVerify(nu1ModBase)           = BASE_ECDSAV_MODULO(u2ModuloPSize, u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1CnsBase)           = BASE_ECDSAV_CNS(u2ModuloPSize, u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1PointABase)        = BASE_ECDSAV_POINT_A(u2ModuloPSize, u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1PointPublicKeyGen) = BASE_ECDSAV_PUBLIC_KEY(u2ModuloPSize, u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1PointSignature)    = BASE_ECDSAV_SIGNATURE(u2ModuloPSize, u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1OrderPointBase)    = BASE_ECDSAV_ORDER(u2ModuloPSize, u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1ABase)             = BASE_ECDSAV_A(u2ModuloPSize, u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1Workspace)         = BASE_ECDSAV_WORKSPACE(u2ModuloPSize, u2OrderSize);
	CPKCL_ZpEcDsaVerify(nu1HashBase)          = BASE_ECDSAV_HASH(u2ModuloPSize, u2OrderSize);
}

int pkcl_ecdsa_verify_signature(mbedtls_pk_type_t type,
				mbedtls_ecp_keypair *pubkey,
				const mbedtls_mpi *r, const mbedtls_mpi *s,
				const unsigned char *hash, size_t hash_len)
{
	int ret;
	CPKCL_PARAM CPKCLParam;
	PCPKCL_PARAM pvCPKCLParam = &CPKCLParam;
	mbedtls_mpi h;

	/* Only ECDSA signature */
	if (type != MBEDTLS_PK_ECDSA) {
		ERROR("verify: Want ECDSA only\n");
		return CRYPTO_ERR_SIGNATURE;
	}

	if (pubkey->grp.id != MBEDTLS_ECP_DP_SECP256R1) {
		ERROR("verify: Want ECDSA group secp256r1\n");
		return CRYPTO_ERR_SIGNATURE;
	}

	if (derive_mpi(&pubkey->grp, &h, hash, hash_len)) {
		ERROR("verify: Unable to derive hash MPI\n");
		return CRYPTO_ERR_SIGNATURE;
	}

	pkcl_ecdsa_verify_setup(pvCPKCLParam, pubkey, r, s, &h);

	vCPKCL_Process(ZpEcDsaVerifyFast, pvCPKCLParam);

	switch (CPKCL(u2Status)) {
	case CPKCL_OK:
		ret = CRYPTO_SUCCESS;
		break;

	default:
		ERROR("verify: Unexpected rc: %0x\n", CPKCL(u2Status));
	case CPKCL_WRONG_SIGNATURE:
		ERROR("verify: Not OK\n");
		ret = CRYPTO_ERR_SIGNATURE;
		break;
	}

	return ret;
}
