/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_CLOCK_H
#define LAN966X_CLOCK_H

#define LAN966X_MAX_CLOCK	14U

#if defined(LAN966X_ASIC)
#define LAN966X_CLK_ID_QSPI0	0U
#define LAN966X_CLK_ID_EMMC	3U
#define LAN966X_CLK_ID_FC3	10U

#define	LAN966X_CLK_FREQ_QPIO0		25000000	// 25MHz
#define	LAN966X_CLK_FREQ_SDMMC		100000000	// 100MHz
#define	LAN966X_CLK_FREQ_FLEXCOM	30000000	// 30MHz
#endif

int lan966x_clk_disable(unsigned int clock);
int lan966x_clk_enable(unsigned int clock);
int lan966x_clk_set_rate(unsigned int clock, unsigned long rate);
unsigned long lan966x_clk_get_rate(unsigned int clock);
unsigned int lan966x_clk_get_baseclk_freq(void);
unsigned int lan966x_clk_get_multclk_freq(void);

#endif  /* LAN966X_CLOCK_H */
