/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLAT_CRYPTO_H
#define PLAT_CRYPTO_H

#include <stdint.h>

#define LAN966X_KEY32_LEN	32
typedef struct {
	union {
		uint8_t  b[LAN966X_KEY32_LEN];
		uint32_t w[LAN966X_KEY32_LEN / 4];
	};
} lan966x_key32_t;

int lan966x_derive_key(const lan966x_key32_t *in, const lan966x_key32_t *salt, lan966x_key32_t *out);

#endif /* PLAT_CRYPTO_H */
