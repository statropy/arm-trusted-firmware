// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (C) 2023 Microchip Technology Inc. and its subsidiaries.
 *
 */

#include <assert.h>
#include <lan969x_ddr_clock.h>
#include <lib/mmio.h>
#include <drivers/delay_timer.h>

#include "platform_def.h"
#include "lan969x_regs.h"

#define DDR_CLK_OPTIONS	7

static const pll_cfg_t pll_settings[2][DDR_CLK_OPTIONS] = {
	{
		/* 25 MHz reference clock */
		{1333, 6, 3, 38, 16609444},  /* Actual 333,25 => ddr_speed/4 */
		{1600, 6, 3, 47, 0},         /* Actual 400 => ddr_speed/4 */
		{1866, 6, 3, 54, 16441672},  /* Actual 466,5 => ddr_speed/4 */
		{2133, 6, 3, 62, 16609444},  /* Actual 533,25 => ddr_speed/4 */
		{2400, 6, 3, 71, 0},	     /* Actual 600 => ddr_speed/4 */
		{2666, 6, 3, 78, 16441672},  /* Actual 666,50 => ddr_speed/4 */
	}, {
		/* 39 MHz reference clock */
		{1333, 6, 6, 42, 13233868},  /* Actual 333,25 => ddr_speed/4 */
		{1600, 6, 6, 52, 12750684},  /* Actual 400 => ddr_speed/4 */
		{1866, 6, 6, 61, 11703786},  /* Actual 466,5 => ddr_speed/4 */
		{2133, 6, 6, 70, 11220602},  /* Actual 533,25 => ddr_speed/4 */
		{2400, 6, 6, 79, 10737418},  /* Actual 600 => ddr_speed/4 */
		{2666, 6, 6, 88, 9690520},   /* Actual 666,50 => ddr_speed/4 */
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
	uint32_t w, max;

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

	/* Enable glitch free operation */
	mmio_setbits_32(CHIP_TOP_DDR_PLL_CFG(LAN969X_CHIP_TOP_BASE),
			CHIP_TOP_DDR_PLL_CFG_NEWDIV(1));

	/* Wait for DIVACK */
	for (max = 20; max > 0; udelay(1)) {
		w = mmio_read_32(CHIP_TOP_DDR_PLL_CFG(LAN969X_CHIP_TOP_BASE));
		if (CHIP_TOP_DDR_PLL_CFG_DIVACK_X(w) == 1)
			break;
		if (--max == 0)
			ERROR("clock: Timeout waiting for DIVACK\n");
	}

	/* Clear NEWDIV + ENA_CFG */
	mmio_clrbits_32(CHIP_TOP_DDR_PLL_CFG(LAN969X_CHIP_TOP_BASE),
			CHIP_TOP_DDR_PLL_CFG_ENA_CFG(1) |
			CHIP_TOP_DDR_PLL_CFG_NEWDIV(1));

	/* Wait for LOCK */
	for (max = 20; max > 0; udelay(1)) {
		w = mmio_read_32(CHIP_TOP_DDR_PLL_CFG(LAN969X_CHIP_TOP_BASE));
		if (CHIP_TOP_DDR_PLL_CFG_LOCK_STAT_X(w) == 1)
			break;
		if (--max == 0)
			ERROR("clock: Timeout waiting for LOCK\n");
	}

	INFO("Configured clock to %d MHz\n", pll->clock);
}
