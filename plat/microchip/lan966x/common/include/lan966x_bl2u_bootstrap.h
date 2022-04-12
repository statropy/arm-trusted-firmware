/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_BL2U_BOOTSTRAP_H
#define LAN966X_BL2U_BOOTSTRAP_H

/* Size of AES attributes in bytes */
#define IV_SIZE 		12
#define TAG_SIZE		16
#define KEY_SIZE		32

void lan966x_bl2u_bootstrap_monitor(void);

#endif
