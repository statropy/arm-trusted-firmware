/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MICROCHIP_SILEX_CRYPTO
#define MICROCHIP_SILEX_CRYPTO

#include <mbedtls/pk.h>

void silex_init(void);

int silex_crypto_ecdsa_verify_signature(mbedtls_pk_type_t type,
					const mbedtls_ecp_keypair *pubkey,
					const mbedtls_mpi *r,  const mbedtls_mpi *s,
					const unsigned char *hash, size_t hash_len);

#endif  /* MICROCHIP_SILEX_CRYPTO */
