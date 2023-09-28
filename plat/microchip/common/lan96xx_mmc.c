/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <drivers/microchip/vcore_gpio.h>
#include <drivers/microchip/emmc.h>
#include <fw_config.h>
#include <lan96xx_mmc.h>

#include "lan966x_regs.h"

#pragma weak plat_lan966x_pinConfig
void plat_lan966x_pinConfig(boot_source_type mode)
{
	NOTICE("Default (empty) MMC pinconfig\n");
}

void lan966x_mmc_plat_config(boot_source_type boot_source)
{
	struct mmc_device_info info;
	lan966x_mmc_params_t params;
	uint32_t clk_rate;
	uint8_t bus_width;

	memset(&params, 0, sizeof(lan966x_mmc_params_t));
	memset(&info, 0, sizeof(struct mmc_device_info));

	params.reg_base = LAN966X_SDMMC_BASE;
	params.desc_base = LAN966X_SRAM_BASE;
	params.desc_size = LAN966X_SRAM_SIZE;

	plat_lan966x_pinConfig(boot_source);

	if (boot_source == BOOT_SOURCE_EMMC) {
		params.mmc_dev_type = MMC_IS_EMMC;
		params.flags = MMC_FLAG_CMD23;
		info.mmc_dev_type = MMC_IS_EMMC;
		info.max_bus_freq = EMMC_HIGH_SPEED;
	} else if (boot_source == BOOT_SOURCE_SDMMC) {
		params.mmc_dev_type = MMC_IS_SD;
		info.mmc_dev_type = MMC_IS_SD;
		info.ocr_voltage = SD_CARD_SUPP_VOLT;
		info.max_bus_freq = SD_HIGH_SPEED;
	}

	info.block_size = MMC_BLOCK_SIZE;

	/* Update global lan966x_fw_config structure */
	lan966x_fw_config_read_uint32(LAN966X_FW_CONF_MMC_CLK_RATE, &clk_rate, MMC_DEFAULT_SPEED);
	lan966x_fw_config_read_uint8(LAN966X_FW_CONF_MMC_BUS_WIDTH, &bus_width, MMC_BUS_WIDTH_1);

	/* Check if special configuration mode is set */
	if (clk_rate == 0u) {
#if defined(IMAGE_BL1)
		/* In case of BL1, skip lan966x_mmc_init() and return. Proceed
		 * with the default loaded mmc settings.*/
		return;

#elif defined(IMAGE_BL2) || defined(IMAGE_BL2U)
		/* In case of BL2 or BL2U, the default settings are overwritten by the
		 * BL1. Therefore, the mmc configuration settings needs to be
		 * loaded again. Call lan966x_mmc_init() afterwards. */
		params.clk_rate = MMC_DEFAULT_SPEED;
		params.bus_width = MMC_BUS_WIDTH_1;
#endif
	} else {
		params.clk_rate = clk_rate;
		params.bus_width = bus_width;
	}

	lan966x_mmc_init(&params, &info);
}
