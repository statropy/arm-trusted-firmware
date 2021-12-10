/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_SJTAG_H
#define LAN966X_SJTAG_H

#include <plat_crypto.h>

void lan966x_sjtag_configure(void);
int  lan966x_sjtag_read_challenge(lan966x_key32_t *k);
int  lan966x_sjtag_write_response(const lan966x_key32_t *k);

#endif /* LAN966X_SJTAG_H */
