/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_CLOCK_H
#define LAN966X_CLOCK_H

enum {
	LAN966X_CLK_ID_QSPI0 = 0,
	LAN966X_CLK_ID_QSPI1,
	LAN966X_CLK_ID_QSPI2,
	LAN966X_CLK_ID_SDMMC0,
	LAN966X_CLK_ID_PI,
	LAN966X_CLK_ID_MCAN0,
	LAN966X_CLK_ID_MCAN1,
	LAN966X_CLK_ID_FLEXCOM0,
	LAN966X_CLK_ID_FLEXCOM1,
	LAN966X_CLK_ID_FLEXCOM2,
	LAN966X_CLK_ID_FLEXCOM3,
	LAN966X_CLK_ID_FLEXCOM4,
	LAN966X_CLK_ID_TIMER,
	LAN966X_CLK_ID_USB_REFCLK,
	LAN966X_MAX_CLOCK
};

#define	LAN966X_CLK_FREQ_QPIO0		25000000	// 25MHz
#define	LAN966X_CLK_FREQ_SDMMC		100000000	// 100MHz

int lan966x_clk_disable(unsigned int clock);
int lan966x_clk_enable(unsigned int clock);
int lan966x_clk_set_rate(unsigned int clock, unsigned long rate);
unsigned long lan966x_clk_get_rate(unsigned int clock);
unsigned int lan966x_clk_get_baseclk_freq(void);
unsigned int lan966x_clk_get_multclk_freq(void);

#endif  /* LAN966X_CLOCK_H */
