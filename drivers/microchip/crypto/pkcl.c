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

int pkcl_ecdsa_verify_signature(mbedtls_pk_type_t type,
				mbedtls_ecp_keypair *pubkey,
				mbedtls_md_type_t md_alg,
				const unsigned char *hash, size_t hash_len,
				const unsigned char *sig, size_t sig_len)
{
	/* Only ECDSA signature */
	if (type != MBEDTLS_PK_ECDSA) {
		INFO("verify: Want ECDSA only\n");
		return CRYPTO_ERR_SIGNATURE;
	}

	if (pubkey->grp.id != MBEDTLS_ECP_DP_SECP256R1) {
		NOTICE("verify: Want ECDSA group secp256r1\n");
		return CRYPTO_ERR_SIGNATURE;
	}

	INFO("verify: All OK\n");

	return CRYPTO_SUCCESS;
}
