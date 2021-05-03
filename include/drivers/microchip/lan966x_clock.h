/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_CLOCK_H
#define LAN966X_CLOCK_H

#define LAN966X_MAX_CLOCK	14U

#if defined(EVB_9662)
#define LAN966X_CLK_QSPI0	0U
#define LAN966X_CLK_EMMC	3U
#define LAN966X_CLK_FC3		10U
#endif

int lan966x_clk_disable(unsigned int clock);
int lan966x_clk_enable(unsigned int clock);
int lan966x_clk_set_rate(unsigned int clock, unsigned long rate);
unsigned long lan966x_clk_get_rate(unsigned int clock);

#endif  /* LAN966X_CLOCK_H */
