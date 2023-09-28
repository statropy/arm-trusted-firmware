/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_FW_BIND_H
#define LAN966X_FW_BIND_H

#include <tools_share/firmware_image_package.h>

/* Size of AES attributes in bytes */
#define IV_SIZE 		12
#define TAG_SIZE		16
#define KEY_SIZE		32

typedef enum {
	FW_BIND_OK	= 0,
	FW_FIP_HDR	= 16,
	FW_FIP_ALIGN,
	FW_FIP_INCOMPLETE,
	FW_TOC_TERM_MISSING,
	FW_NOT_SSK_ENCRYPTED,
	FW_SSK_FAILURE,
	FW_DECRYPT,
	FW_BSSK_FAILURE,
	FW_ENCRYPT,
} fw_bind_res_t;

static inline const char *lan966x_bind_err2str(fw_bind_res_t err)
{
	switch (err) {
	case FW_BIND_OK:
		return "(OK)";
	case FW_FIP_HDR:
		return "Header check of FIP failed";
	case FW_FIP_ALIGN:
		return "FIP needs to be produced with FIP_ALIGN";
	case FW_FIP_INCOMPLETE:
		return "FIP is incomplete (truncated, garbled)";
	case FW_TOC_TERM_MISSING:
		return "FIP does not have a ToC terminator entry";
	case FW_NOT_SSK_ENCRYPTED:
		return "FIP must be encrypted with SSK";
	case FW_SSK_FAILURE:
		return "Failed to obtain SSK key";
	case FW_DECRYPT:
		return "Failed to decrypt FIP image";
	case FW_BSSK_FAILURE:
		return "Failed to obtain BSSK key";
	case FW_ENCRYPT:
		return "Failed to encrypt FIP image";
	default:;
	}
	return "Unknown error";
}

static inline int is_valid_fip_hdr(const fip_toc_header_t *header)
{
	if ((header->name == TOC_HEADER_NAME) && (header->serial_number != 0)) {
		return 1;
	} else {
		return 0;
	}
}

fw_bind_res_t lan966x_bind_fip(const uintptr_t fip_base_addr, size_t length);

#endif	/* LAN966X_FW_BIND_H */
