/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN969X_DDR_CLOCK_H
#define LAN969X_DDR_CLOCK_H

#include <stdint.h>

typedef struct {
	uint32_t clock;
	uint8_t divfi;
	uint32_t divff;
	uint8_t divq;
	uint8_t divr;
} pll_cfg_t;

const pll_cfg_t *lan969x_ddr_get_clock_cfg(int freq_mhz);
void lan969x_ddr_set_clock_cfg(const pll_cfg_t *cfg);

#endif /* LAN969X_DDR_CLOCK_H */
