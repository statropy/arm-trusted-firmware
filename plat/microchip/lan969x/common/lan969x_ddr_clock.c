// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2023 Microchip Technology Inc. and its subsidiaries.
 *
 */

#include <assert.h>
#include <lan969x_ddr_clock.h>
#include <lib/mmio.h>

#include "lan969x_regs.h"

#define DDR_CLK_OPTIONS	5

static const pll_cfg_t pll_settings[2][DDR_CLK_OPTIONS] = {
	{
		/* 25 MHz reference clock */
		{1600, 71,        0, 9, 3}, /* Actual 400 = ddr_speed/4 */
		{1866, 64,  5200937, 7, 3}, /* Actual 466,5 = ddr_speed/4 */
		{2133, 62, 16609444, 6, 3}, /* Actual 533,25 = ddr_speed/4 */
		{2400, 71,        0, 6, 3}, /* Actual 600 = ddr_speed/4 */
		{2666, 65, 10905190, 5, 3}, /* Actual 666,50 = ddr_speed/4 */
	}, {
		/* 39 MHz reference clock */
		{1600, 79, 10737418, 9, 6}, /* Actual 400 = ddr_speed/4 */
		{1866, 72,  2469606, 7, 6}, /* Actual 466,5 = ddr_speed/4 */
		{2133, 70, 11220602, 6, 6}, /* Actual 533,25 = ddr_speed/4 */
		{2400, 79, 10737418, 6, 6}, /* Actual 600 = ddr_speed/4 */
		{2666, 73, 10871636, 5, 6}, /* Actual 666,50 = ddr_speed/4 */
	}
};

const pll_cfg_t *lan969x_ddr_get_clock_cfg(int freq_mhz)
{
	int ref, i;

	ref = mmio_read_32(CHIP_TOP_HW_STAT(LAN969X_CHIP_TOP_BASE));
	ref = CHIP_TOP_HW_STAT_REFCLK_SEL_X(ref);
	VERBOSE("Using %d MHz reference clock\n", ref == 0 ? 25 : 39);

	for (i = 0; i < DDR_CLK_OPTIONS; i++) {
		const pll_cfg_t *pll = &pll_settings[ref][i];
		if (freq_mhz == pll->clock)
			return pll;
	}

	return NULL;
}

void lan969x_ddr_set_clock_cfg(const pll_cfg_t *pll)
{
	uint8_t w;

	/* Configure divfi, divff */
	mmio_write_32(CHIP_TOP_DDR_PLL_FREQ_CFG(LAN969X_CHIP_TOP_BASE),
		      CHIP_TOP_DDR_PLL_FREQ_CFG_DIVFI(pll->divfi) |
		      CHIP_TOP_DDR_PLL_FREQ_CFG_DIVFF(pll->divff));

	/* Configure divq, divr */
	mmio_clrsetbits_32(CHIP_TOP_DDR_PLL_CFG(LAN969X_CHIP_TOP_BASE),
			   CHIP_TOP_DDR_PLL_CFG_DIVQ_M |
			   CHIP_TOP_DDR_PLL_CFG_DIVR_M,
			   CHIP_TOP_DDR_PLL_CFG_DIVQ(pll->divq) |
			   CHIP_TOP_DDR_PLL_CFG_DIVR(pll->divr));

	/* Enable custom clock configuration */
	mmio_setbits_32(CHIP_TOP_DDR_PLL_CFG(LAN969X_CHIP_TOP_BASE),
			CHIP_TOP_DDR_PLL_CFG_ENA_CFG(1));

	/* Toggle bypass to update output clock */
	mmio_setbits_32(CHIP_TOP_DDR_PLL_FREQ_CFG(LAN969X_CHIP_TOP_BASE),
			CHIP_TOP_DDR_PLL_FREQ_CFG_BYPASS_ENA(1U));
	mmio_clrbits_32(CHIP_TOP_DDR_PLL_FREQ_CFG(LAN969X_CHIP_TOP_BASE),
			CHIP_TOP_DDR_PLL_FREQ_CFG_BYPASS_ENA(1U));

	/* Wait for lock */
	do {
		w = mmio_read_32(CHIP_TOP_DDR_PLL_CFG(LAN969X_CHIP_TOP_BASE));
	} while (CHIP_TOP_DDR_PLL_CFG_LOCK_STAT_X(w) == 0);

	INFO("Configured clock to %d MHz\n", pll->clock);
}
