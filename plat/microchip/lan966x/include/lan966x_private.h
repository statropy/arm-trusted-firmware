/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_PRIVATE_H
#define LAN966X_PRIVATE_H

void lan966x_console_init(void);
void lan966x_init_timer(void);
void lan966x_io_setup(void);
void lan966x_ddr_init(void);
void lan966x_tz_init(void);

void plat_lan966x_gic_driver_init(void);
void plat_lan966x_gic_init(void);

#endif /* LAN966X_PRIVATE_H */
