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
				mbedtls_md_type_t md_alg,
				const unsigned char *hash, size_t hash_len,
				const unsigned char *sig, size_t sig_len);

#endif  /* MICROCHIP_PKCL */
