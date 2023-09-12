/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN969X_PRIVATE_H
#define LAN969X_PRIVATE_H

#include <stdint.h>
#include <lan96xx_common.h>

/* BL2 -> BL31 */
typedef struct {
	uint32_t ddr_size;
	uint32_t board_number;
	size_t boot_offset;
} bl31_params_t;

void lan969x_set_max_trace_level(void);

void lan969x_console_init(void);
void lan969x_timer_init(void);

void lan969x_mmc_plat_config(boot_source_type boot_source);

void lan966x_bootstrap_monitor(void);

void lan966x_ddr_init(void *fdt);

uint32_t lan966x_ddr_size(void);

size_t lan966x_get_boot_offset(void);

void lan969x_tz_init(void);

void lan969x_tz_finish(void);

#endif /* LAN969X_PRIVATE_H */
