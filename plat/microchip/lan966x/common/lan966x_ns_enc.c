/*
 * Copyright (C) 2023 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <drivers/auth/mbedtls/mbedtls_config.h>
#include <errno.h>
#include <plat/common/platform.h>
#include <platform_def.h>
#include <errno.h>
#include <tools_share/firmware_encrypted.h>

#include "plat_otp.h"
#include "lan966x_private.h"
#include "lan966x_ns_enc.h"

static const lan966x_key32_t lan966x_ns_derive = {
	.b = {
		0xaf, 0x10, 0xd7, 0xc3, 0x2e, 0x5c, 0xa3, 0xa2,
		0xdd, 0xe2, 0x9b, 0x12, 0x93, 0x77, 0x0f, 0x14,
		0x64, 0x43, 0x3f, 0xe6, 0xb8, 0x7e, 0x6c, 0xaf,
		0x08, 0xf5, 0xe8, 0xc2, 0x13, 0xb7, 0x80, 0xdf,
	},
};

/*
 * Get the NS data encryption symmetric key, SSK or BSSK.
 * Both keys are derived with a fixed salt.
 */
int lan96xx_get_ns_enc_key(enum ns_enc_type_e ns_enc_type, uint8_t *key, size_t *key_len)
{
	int ret;
	lan966x_key32_t otp_key;
	size_t otp_keylen = LAN966X_KEY32_LEN;

	switch (ns_enc_type) {
	case NS_ENC_WITH_SSK:
		ret = otp_read_otp_tbbr_ssk(otp_key.b, otp_keylen);
		break;
	case NS_ENC_WITH_BSSK:
		ret = otp_read_otp_tbbr_huk(otp_key.b, otp_keylen);
		break;
	default:
		ret = -ENOTSUP;
	}

	if (ret == 0)
		ret = lan966x_derive_key(&otp_key, &lan966x_ns_derive, &otp_key);

	if (ret == 0) {
		/* HUK not used directly */
		otp_keylen = MIN(*key_len, otp_keylen);
		*key_len = otp_keylen;
		memcpy(key, otp_key.b, otp_keylen);
		/* Don't leak */
		memset(&otp_key, 0, sizeof(otp_key));
	}

	return ret;
}
