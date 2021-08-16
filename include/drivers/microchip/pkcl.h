/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MICROCHIP_PKCL
#define MICROCHIP_PKCL

#include <mbedtls/pk.h>
#include <CryptoLib_typedef_pb.h>

void pkcl_init(void);

// ECC struture
typedef struct
{
// Prime P
pfu1 pfu1ModuloP;

// Size of modulo P
u2 u2ModuloPSize;

// Size of the order n
u2 u2OrderSize;

// "a" parameter in curve equation
// x^3 = x^2 + a*x + b
pfu1 pfu1ACurve;

// "b" parameter in curve equation
// x^3 = x^2 + a*x + b
pfu1 pfu1BCurve;

// Abscissa of the base point of the curve
pfu1 pfu1APointX;

// Ordinate of the base point of the curve
pfu1 pfu1APointY;

// Height of the base point of the curve
pfu1 pfu1APointZ;

// Abscissa of second point for addition
pfu1 pfu1BPointX;

// Ordinate of second point for addition
pfu1 pfu1BPointY;

// Height of second point for addition
pfu1 pfu1BPointZ;

// Order of the curve
pfu1 pfu1APointOrder;

// Private key
pfu1 pfu1PrivateKey;

// Abscissa of public key
pfu1 pfu1PublicKeyX;

// Ordinate of public key
pfu1 pfu1PublicKeyY;

// Height of public key
pfu1 pfu1PublicKeyZ;

// Reduction constant
pfu1 pfu1Cns;

// Random number
pfu1 pfu1RandomNumber;

// Hash Value
pfu1 pfu1HashValue;

}ECC_Struct;

int pkcl_ecdsa_verify_signature(mbedtls_pk_type_t type,
				mbedtls_ecp_keypair *pubkey,
				const mbedtls_mpi *r,  const mbedtls_mpi *s,
				const unsigned char *hash, size_t hash_len);

int DemoGenerate(ECC_Struct *pECC_Structure,
		 const uint8_t *pfu1ScalarNumber,
		 uint8_t *sig_out);

int DemoVerify(ECC_Struct *pECC_Structure,
	       const uint8_t *sig_r,
	       const uint8_t *sig_s);

#endif  /* MICROCHIP_PKCL */
