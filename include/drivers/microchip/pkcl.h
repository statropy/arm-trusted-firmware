/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MICROCHIP_PKCL
#define MICROCHIP_PKCL

#include <mbedtls/pk.h>

void pkcl_init(void);

int pkcl_ecdsa_verify_signature(mbedtls_pk_type_t type,
				mbedtls_ecp_keypair *pubkey,
				const mbedtls_mpi *r,  const mbedtls_mpi *s,
				const unsigned char *hash, size_t hash_len);

int DemoVerify(int modp_size,
	       int order_size,
	       const uint8_t *pfu1ModuloP,
	       const uint8_t *pfu1APointX,
	       const uint8_t *pfu1APointY,
	       const uint8_t *pfu1APointZ,
	       const uint8_t *pfu1ACurve,
	       const uint8_t *pfu1APointOrder,
	       const uint8_t *pfu1PublicKeyX,
	       const uint8_t *pfu1PublicKeyY,
	       const uint8_t *pfu1PublicKeyZ,
	       const uint8_t *pfu1HashValue,
	       const uint8_t *sig_r,
	       const uint8_t *sig_s);

#endif  /* MICROCHIP_PKCL */
