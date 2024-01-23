/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <drivers/microchip/sha.h>
#include <plat_crypto.h>
#include <stddef.h>
#include <string.h>

/*
 * Derive a 32 byte key with a 32 byte salt, output a 32 byte key
 */
int lan966x_derive_key(const lan966x_key32_t *in,
		       const lan966x_key32_t *salt,
		       lan966x_key32_t *out)
{
	uint8_t buf[LAN966X_KEY32_LEN * 2];
	int ret;

	/* Use one contiguous buffer for now */
	memcpy(buf, in->b, LAN966X_KEY32_LEN);
	memcpy(buf + LAN966X_KEY32_LEN, salt->b, LAN966X_KEY32_LEN);

	ret = sha_calc(SHA_MR_ALGO_SHA256, buf, sizeof(buf), out->b, sizeof(out->b));

	/* Don't leak */
	memset(buf, 0, sizeof(buf));

	return ret;
}
