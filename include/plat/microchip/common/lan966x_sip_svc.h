/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_SIP_SVC_H
#define LAN966X_SIP_SVC_H

#include <stdint.h>
#include <drivers/auth/crypto_mod.h>
#include "lan966x_ns_enc.h"

/* SMC function IDs for SiP Service queries */
#define SIP_SVC_UID		0x8200ff01
#define SIP_SVC_VERSION		0x8200ff02
#define SIP_SVC_SJTAG_STATUS	0x8200ff03
#define SIP_SVC_SJTAG_CHALLENGE	0x8200ff04
#define SIP_SVC_SJTAG_UNLOCK	0x8200ff05
#define SIP_SVC_FW_BIND		0x8200ff06
#define SIP_SVC_NS_ENCRYPT	0x8200ff07
#define SIP_SVC_NS_DECRYPT	0x8200ff08
#define SIP_SVC_GET_BOOTSRC	0x8200ff09
#define SIP_SVC_GET_DDR_SIZE	0x8200ff0a

/* SiP Service Calls version numbers */
#define SIP_SVC_VERSION_MAJOR	0
#define SIP_SVC_VERSION_MINOR	1

/* This is used as a signature to validate the encryption header */
#define NS_ENC_HEADER_MAGIC		0xAA64BE05U

enum ns_enc_algo {
	NS_ENC_ALG_GCM		/* AES-GCM (default) */
};

struct ns_enc_hdr {
	uint32_t magic;
	uint32_t data_length;
	uint16_t algo;		/* ns_enc_algo */
	uint16_t flags;		/* ns_enc_flags */
	uint16_t iv_len;
	uint16_t tag_len;
	uint8_t iv[CRYPTO_MAX_IV_SIZE];
	uint8_t tag[CRYPTO_MAX_TAG_SIZE];
};

static inline bool is_valid_enc_hdr(const struct ns_enc_hdr *encp)
{
	return encp->magic == NS_ENC_HEADER_MAGIC &&
		encp->data_length > 0 &&
		encp->algo == NS_ENC_ALG_GCM &&
		(encp->flags == NS_ENC_WITH_SSK || encp->flags == NS_ENC_WITH_BSSK);
}

static inline void init_enc_hdr(struct ns_enc_hdr *encp)
{
	memset(encp, 0, sizeof(*encp));
	encp->magic = NS_ENC_HEADER_MAGIC;
	encp->algo = NS_ENC_ALG_GCM;
	encp->flags = NS_ENC_WITH_BSSK;
}

uintptr_t microchip_plat_ns_ddr_base(void);
size_t microchip_plat_ns_ddr_size(void);

uintptr_t microchip_plat_sip_handler(uint32_t smc_fid,
				     u_register_t x1,
				     u_register_t x2,
				     u_register_t x3,
				     u_register_t x4,
				     void *cookie,
				     void *handle,
				     u_register_t flags);

#endif
