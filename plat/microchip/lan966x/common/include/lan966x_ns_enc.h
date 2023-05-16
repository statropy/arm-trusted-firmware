/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_NS_ENC_H
#define LAN966X_NS_ENC_H

#include <stdint.h>
#include <tools_share/firmware_encrypted.h>

/*
 * SSK: Secret Symmetric Key
 * BSSK: Binding Secret Symmetric Key
 */
enum ns_enc_type_e {
	NS_ENC_WITH_SSK = 0,
	NS_ENC_WITH_BSSK,
};

int lan96xx_get_ns_enc_key(enum ns_enc_type_e ns_enc_type, uint8_t *key, size_t *key_len);

#endif	/* LAN966X_NS_ENC_H */
