/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/auth/crypto_mod.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/bignum.h>
#include <mbedtls/error.h>

#include "inc/CryptoLib_Headers_pb.h"
#include "inc/CryptoLib_JumpTable_pb.h"

#include <pkcl.h>

#define PKCL_RAM_OFFSET 0x1000

#define NEARTOFAR(x) ((void*)(LAN996X_PKCL_RAM_BASE - PKCL_RAM_OFFSET + (x)))

//******************************************************************************
// Memory mapping for ECDSA signature
//******************************************************************************
#define BASE_ECDSA_MODULO(a,b)          (nu1) PKCL_RAM_OFFSET
#define BASE_ECDSA_CNS(a,b)             (nu1) (BASE_ECDSA_MODULO(a,b) + a + 4)
#define BASE_ECDSA_POINT_A(a,b)         (nu1) (BASE_ECDSA_CNS(a,b) + b + 12)
#define BASE_ECDSA_POINT_A_X(a,b)       (nu1) (BASE_ECDSA_POINT_A(a,b))
#define BASE_ECDSA_POINT_A_Y(a,b)       (nu1) (BASE_ECDSA_POINT_A_X(a,b) + a + 4)
#define BASE_ECDSA_POINT_A_Z(a,b)       (nu1) (BASE_ECDSA_POINT_A_Y(a,b) + a + 4)
#define BASE_ECDSA_A(a,b)               (nu1) (BASE_ECDSA_POINT_A_Z(a,b) + a + 4)
#define BASE_PRIVATE_KEY(a,b)           (nu1) (BASE_ECDSA_A(a,b) + a + 4)
#define BASE_ECDSA_SCALAR(a,b)          (nu1) (BASE_PRIVATE_KEY(a,b) + b + 4)
#define BASE_ECDSA_ORDER(a,b)           (nu1) (BASE_ECDSA_SCALAR(a,b) + b + 4)
#define BASE_ECDSA_HASH(a,b)            (nu1) (BASE_ECDSA_ORDER(a,b) + b + 4)
#define BASE_ECDSA_WORKSPACE(a,b)       (nu1) (BASE_ECDSA_HASH(a,b) + b + 4)

//******************************************************************************
// Memory mapping for ECDSA signature verification
//******************************************************************************
#define BASE_ECDSAV_MODULO(a,b)         (nu1) PKCL_RAM_OFFSET
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

static void dump_mem(uint8_t *p, size_t len)
{
	while (len) {
		int n = (len < 16 ? len : 16);
		INFO("%p: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		     p,
		     p[0 + 0], p[1 + 0], p[2 + 0], p[3 + 0], p[4 + 0], p[5 + 0], p[6 + 0], p[7 + 0],
		     p[0 + 8], p[1 + 8], p[2 + 8], p[3 + 8], p[4 + 8], p[5 + 8], p[6 + 8], p[7 + 8]);
		p += n;
		len -= n;
	}
}

static void dump_mem16(uint16_t *p, size_t len)
{
	while (len) {
		int n = (len < 8 ? len : 8);
		INFO("%p: %04x %04x %04x %04x %04x %04x %04x %04x\n",
		     p,
		     p[0 + 0], p[1 + 0], p[2 + 0], p[3 + 0], p[4 + 0], p[5 + 0], p[6 + 0], p[7 + 0]);
		p += n;
		len -= n;
	}
}

void dump_pkcl_ram(uint16_t beg, uint16_t end)
{
	dump_mem(NEARTOFAR(beg), end - beg);
}

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
	beg = BASE_ECDSAV_MODULO(u2ModuloPSize,u2OrderSize);
	end = BASE_ECDSAV_WORKSPACE(u2ModuloPSize,u2OrderSize);
	memset(NEARTOFAR(beg), 0, end - beg);

	/* Copies the signature into appropriate memory area */
	// Take care of the input signature format (???)
	/* Copy R, S with 4 bytes zero spacing */
	beg = BASE_ECDSAV_SIGNATURE(u2ModuloPSize,u2OrderSize);
	cpy_mpi(beg, r);
	beg += u2OrderSize + 4;
	cpy_mpi(beg, s);

	/* Hash (byteorder?) */
	beg = BASE_ECDSAV_HASH(u2ModuloPSize,u2OrderSize);
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

	dump_pkcl_ram(BASE_ECDSAV_MODULO(u2ModuloPSize,u2OrderSize),
		      BASE_ECDSAV_WORKSPACE(u2ModuloPSize,u2OrderSize));
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
		INFO("verify: Want ECDSA only\n");
		return CRYPTO_ERR_SIGNATURE;
	}

	if (pubkey->grp.id != MBEDTLS_ECP_DP_SECP256R1) {
		NOTICE("verify: Want ECDSA group secp256r1\n");
		return CRYPTO_ERR_SIGNATURE;
	}

	if (derive_mpi(&pubkey->grp, &h, hash, hash_len)) {
		NOTICE("verify: Unable to derive hash MPI\n");
		return CRYPTO_ERR_SIGNATURE;
	}

	pkcl_ecdsa_verify_setup(pvCPKCLParam, pubkey, r, s, &h);

#define PKCL_HW
#if defined(PKCL_HW)
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
#else
	ret = mbedtls_ecdsa_verify(&pubkey->grp,
				   hash, hash_len,
				   &pubkey->Q,
				   r, s);
	INFO("MBEDTLS verify: %d\n", ret);
	ret = ret ? CRYPTO_ERR_SIGNATURE: CRYPTO_SUCCESS;
#endif

	return ret;
}

//- Function -----------------------------------------
// MyMemCpy()
//
// Copies a buffer from EEPROM to RAM, while changing the endianness
//
// Input parameters :
//   nu1Dest - nu1 - Input
//        Pointer the destination
//   nu1Src - const u1 __farflash * - Input
//        Pointer the source
//   u2Length - u2 - Input
//        Length of the buffer to be copied
//
// Output values :
//   none
//- Remarks ------------------------------------------
// Should also work from RAM to RAM !
//----------------------------------------------------
static void MyMemCpy(uint8_t *pu1Dest,const uint8_t *pu1Src,u2 u2Length)
     {

     u2  u2Cpt;
     pu1 pu1PtrDest;

     u2Cpt      = 0;
     pu1PtrDest = pu1Dest;
     while (u2Cpt < u2Length)
          {
          *(pu1PtrDest++) = pu1Src[u2Length - u2Cpt - 1];
          u2Cpt++;
          }

     }

int DemoGenerate(ECC_Struct *a,
		 const uint8_t *pfu1ScalarNumber,
		 uint8_t *sig_out)
{
     CPKCL_PARAM CPKCLParam;
     PCPKCL_PARAM pvCPKCLParam = &CPKCLParam;
     u2  u2ModuloPSize = a->u2ModuloPSize;
     u2  u2OrderSize   = a->u2OrderSize;
     pu1 pu1Tmp;
     u2  u2NbBytesToPadd;

     INFO("ZpEcDsaGenerateFast: u2ModuloPSize = %d, u2OrderSize = %d\n", u2ModuloPSize, u2OrderSize);
#define ListOffset(n,o) INFO("Offset of %s = 0x%03x\n", n, o)
     ListOffset("BASE_ECDSA_MODULO", BASE_ECDSA_MODULO(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSA_CNS", BASE_ECDSA_CNS(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSA_POINT_A_X", BASE_ECDSA_POINT_A_X(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSA_POINT_A_Y", BASE_ECDSA_POINT_A_Y(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSA_POINT_A_Z", BASE_ECDSA_POINT_A_Z(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSA_A", BASE_ECDSA_A(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_PRIVATE_KEY", BASE_PRIVATE_KEY(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSA_SCALAR", BASE_ECDSA_SCALAR(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSA_ORDER", BASE_ECDSA_ORDER(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSA_HASH", BASE_ECDSA_HASH(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSA_WORKSPACE", BASE_ECDSA_WORKSPACE(u2ModuloPSize, u2OrderSize));

     // Padds the end of SHA1 result with 0 !!!!
     pu1Tmp = (pu1)(NEARTOFAR(BASE_ECDSA_HASH(u2ModuloPSize,u2OrderSize))) + 20;
     for(u2NbBytesToPadd = u2OrderSize - 16; u2NbBytesToPadd > 0; u2NbBytesToPadd--)
          {
          *pu1Tmp++ = 0;
          }

     // Let's copy parameters for ECDSA verification in memory areas
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSA_MODULO(u2ModuloPSize,u2OrderSize)))    ,a->pfu1ModuloP       ,u2ModuloPSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSA_POINT_A_X(u2ModuloPSize,u2OrderSize))) ,a->pfu1APointX       ,u2ModuloPSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSA_POINT_A_Y(u2ModuloPSize,u2OrderSize))) ,a->pfu1APointY       ,u2ModuloPSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSA_POINT_A_Z(u2ModuloPSize,u2OrderSize))) ,a->pfu1APointZ       ,u2ModuloPSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSA_A(u2ModuloPSize,u2OrderSize)))         ,a->pfu1ACurve        ,u2ModuloPSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSA_ORDER(u2ModuloPSize,u2OrderSize)))     ,a->pfu1APointOrder   ,u2OrderSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_PRIVATE_KEY(u2ModuloPSize,u2OrderSize)))     ,a->pfu1PrivateKey    ,u2OrderSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSA_HASH(u2ModuloPSize,u2OrderSize)))      ,a->pfu1HashValue     ,u2ModuloPSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSA_SCALAR(u2ModuloPSize,u2OrderSize)))    ,pfu1ScalarNumber     ,u2OrderSize + 4);

     // ECC signature
     // Asks for a Signature generation
     memset(pvCPKCLParam, 0, sizeof(*pvCPKCLParam));
     CPKCL_ZpEcDsaGenerate(nu1ModBase)        = BASE_ECDSA_MODULO(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaGenerate(nu1CnsBase)        = BASE_ECDSA_CNS(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaGenerate(nu1PointABase)     = BASE_ECDSA_POINT_A(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaGenerate(nu1PrivateKey)     = BASE_PRIVATE_KEY(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaGenerate(nu1ScalarNumber)   = BASE_ECDSA_SCALAR(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaGenerate(nu1OrderPointBase) = BASE_ECDSA_ORDER(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaGenerate(nu1ABase)          = BASE_ECDSA_A(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaGenerate(nu1Workspace)      = BASE_ECDSA_WORKSPACE(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaGenerate(nu1HashBase)       = BASE_ECDSA_HASH(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaGenerate(u2ModLength)       = u2ModuloPSize;
     CPKCL_ZpEcDsaGenerate(u2ScalarLength)    = u2OrderSize;

     INFO("PKCL ZpEcDsaGenerateFast:\n");
     dump_mem16((uint16_t*)&CPKCLParam, sizeof(CPKCLParam) / 2);
     INFO("PKCL RAM:\n");
     dump_pkcl_ram(BASE_ECDSA_MODULO(u2ModuloPSize,u2OrderSize),
		   BASE_ECDSA_WORKSPACE(u2ModuloPSize,u2OrderSize));

     // Launch the signature generation !
     // See PKCL_Rc_pb.h for possible u2Status Values !
     vCPKCL_Process(ZpEcDsaGenerateFast,pvCPKCLParam);
     INFO("Generate: status = %x\n", CPKCL(u2Status));

     if (CPKCL(u2Status) != CPKCL_OK)
	     return CPKCL(u2Status);

     return 0;
}

int DemoVerify(ECC_Struct *a,
	       const uint8_t *sig_r,
	       const uint8_t *sig_s)
{
     CPKCL_PARAM CPKCLParam;
     PCPKCL_PARAM pvCPKCLParam = &CPKCLParam;
     // Init
     pu1 pu1Tmp;
     u2 u2NbBytesToPadd;
     u2  u2ModuloPSize = a->u2ModuloPSize;
     u2  u2OrderSize   = a->u2OrderSize;

     INFO("ZpEcDsaVerifyFast: u2ModuloPSize = %d, u2OrderSize = %d\n", u2ModuloPSize, u2OrderSize);
     ListOffset("BASE_ECDSAV_MODULO", BASE_ECDSAV_MODULO(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSAV_CNS", BASE_ECDSAV_CNS(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSAV_ORDER", BASE_ECDSAV_ORDER(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSAV_SIGNATURE", BASE_ECDSAV_SIGNATURE(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSAV_HASH", BASE_ECDSAV_HASH(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSAV_POINT_A_X", BASE_ECDSAV_POINT_A_X(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSAV_POINT_A_Y", BASE_ECDSAV_POINT_A_Y(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSAV_POINT_A_Z", BASE_ECDSAV_POINT_A_Z(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSAV_PUBLIC_KEY_X", BASE_ECDSAV_PUBLIC_KEY_X(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSAV_PUBLIC_KEY_Y", BASE_ECDSAV_PUBLIC_KEY_Y(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSAV_PUBLIC_KEY_Z", BASE_ECDSAV_PUBLIC_KEY_Z(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSAV_A", BASE_ECDSAV_A(u2ModuloPSize, u2OrderSize));
     ListOffset("BASE_ECDSAV_WORKSPACE", BASE_ECDSAV_WORKSPACE(u2ModuloPSize, u2OrderSize));

     // Padds the end of SHA1 result with 0 !!!
     pu1Tmp = (pu1)(NEARTOFAR(BASE_ECDSAV_HASH(u2ModuloPSize,u2OrderSize))) + 20;
     for(u2NbBytesToPadd = u2OrderSize - 16; u2NbBytesToPadd > 0; u2NbBytesToPadd--)
          {
          *pu1Tmp++ = 0;
          }

     // Copies the signature into appropriate memory area
     // Take care of the input signature format
     pu1Tmp = (pu1)(NEARTOFAR(BASE_ECDSAV_SIGNATURE(u2ModuloPSize,u2OrderSize)));
     MyMemCpy(pu1Tmp,sig_r,u2OrderSize + 4);
     MyMemCpy(pu1Tmp + u2OrderSize + 4,sig_s,u2OrderSize + 4);

     // Let's copy parameters for ECDSA signature verification in memory areas
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSAV_MODULO(u2ModuloPSize,u2OrderSize)))       ,a->pfu1ModuloP       ,u2ModuloPSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSAV_POINT_A_X(u2ModuloPSize,u2OrderSize)))    ,a->pfu1APointX       ,u2ModuloPSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSAV_POINT_A_Y(u2ModuloPSize,u2OrderSize)))    ,a->pfu1APointY       ,u2ModuloPSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSAV_POINT_A_Z(u2ModuloPSize,u2OrderSize)))    ,a->pfu1APointZ       ,u2ModuloPSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSAV_A(u2ModuloPSize,u2OrderSize)))            ,a->pfu1ACurve        ,u2ModuloPSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSAV_ORDER(u2ModuloPSize,u2OrderSize)))        ,a->pfu1APointOrder   ,u2OrderSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSAV_PUBLIC_KEY_X(u2ModuloPSize,u2OrderSize))) ,a->pfu1PublicKeyX    ,u2ModuloPSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSAV_PUBLIC_KEY_Y(u2ModuloPSize,u2OrderSize))) ,a->pfu1PublicKeyY    ,u2ModuloPSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSAV_PUBLIC_KEY_Z(u2ModuloPSize,u2OrderSize))) ,a->pfu1PublicKeyZ    ,u2ModuloPSize + 4);
     MyMemCpy((pu1)(NEARTOFAR(BASE_ECDSAV_HASH(u2ModuloPSize,u2OrderSize))) 	    ,a->pfu1HashValue     ,u2ModuloPSize + 4);

     // Ask for a Verify generation
     memset(pvCPKCLParam, 0, sizeof(*pvCPKCLParam));
     CPKCL_ZpEcDsaVerify(nu1ModBase)               = BASE_ECDSAV_MODULO(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaVerify(nu1CnsBase)               = BASE_ECDSAV_CNS(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaVerify(nu1PointABase)            = BASE_ECDSAV_POINT_A(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaVerify(nu1PointPublicKeyGen)     = BASE_ECDSAV_PUBLIC_KEY(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaVerify(nu1PointSignature)        = BASE_ECDSAV_SIGNATURE(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaVerify(nu1OrderPointBase)        = BASE_ECDSAV_ORDER(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaVerify(nu1ABase)                 = BASE_ECDSAV_A(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaVerify(nu1Workspace)             = BASE_ECDSAV_WORKSPACE(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaVerify(nu1HashBase)              = BASE_ECDSAV_HASH(u2ModuloPSize,u2OrderSize);
     CPKCL_ZpEcDsaVerify(u2ModLength)              = u2ModuloPSize;
     CPKCL_ZpEcDsaVerify(u2ScalarLength)           = u2OrderSize;

     INFO("PKCL ZpEcDsaVerifyFast:\n");
     dump_mem16((uint16_t*)&CPKCLParam, sizeof(CPKCLParam) / 2);
     INFO("PKCL RAM:\n");
     dump_pkcl_ram(BASE_ECDSAV_MODULO(u2ModuloPSize,u2OrderSize),
		   BASE_ECDSAV_WORKSPACE(u2ModuloPSize,u2OrderSize));

     // Verify the signature
     vCPKCL_Process(ZpEcDsaVerifyFast, pvCPKCLParam);

     INFO("ZpEcDsaVerifyFast: ret %0x\n", CPKCL(u2Status));

     return CPKCL(u2Status) == CPKCL_OK ? 0 : -1;
}
