/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <drivers/microchip/vcore_gpio.h>
#include <drivers/microchip/emmc.h>
#include "lan966x_regs.h"
#include "lan966x_private.h"

static void plat_lan966x_pinConfig(void)
{
	/* Configure GPIO pins for eMMC mode */
	vcore_gpio_init(GCB_GPIO_OUT_SET(LAN966X_GCB_BASE));

	vcore_gpio_set_alt(67, 1);	//PIO_67_SDMMC0_CMD
	vcore_gpio_set_alt(68, 1);	//PIO_68_SDMMC0_CK
	vcore_gpio_set_alt(69, 1);	//PIO_69_SDMMC0_DAT0
	vcore_gpio_set_alt(70, 1);	//PIO_70_SDMMC0_DAT1
	vcore_gpio_set_alt(71, 1);	//PIO_71_SDMMC0_DAT2
	vcore_gpio_set_alt(72, 1);	//PIO_72_SDMMC0_DAT3
	vcore_gpio_set_alt(73, 1);	//PIO_73_SDMMC0_DAT4
	vcore_gpio_set_alt(74, 1);	//PIO_74_SDMMC0_DAT5
	vcore_gpio_set_alt(75, 1);	//PIO_75_SDMMC0_DAT6
	vcore_gpio_set_alt(76, 1);	//PIO_76_SDMMC0_DAT7
	vcore_gpio_set_alt(77, 1);	//PIO_77_SDMMC0_RSTN

#if 0
	/* Configure GPIO pins for SDCARD mode */
	vcore_gpio_set_alt(73, 4);	//PIO_73_SDMMC0_1V8SEL
	vcore_gpio_set_alt(74, 4);	//PIO_74_SDMMC0_WP
	vcore_gpio_set_alt(75, 4);	//PIO_75_SDMMC0_DAT6
#endif
}

void lan966x_mmc_plat_config(void)
{
	struct mmc_device_info info;
	lan966x_mmc_params_t params;
	uint32_t clk_rate;
	uint8_t bus_width;

	memset(&params, 0, sizeof(lan966x_mmc_params_t));
	memset(&info, 0, sizeof(struct mmc_device_info));

	params.reg_base = LAN966X_SDMMC_BASE;
	params.desc_base = LAN996X_SRAM_BASE;
	params.desc_size = LAN996X_SRAM_SIZE;

	lan966x_fw_config_read_uint32(LAN966X_FW_CONF_MMC_CLK_RATE, &clk_rate);
	params.clk_rate = clk_rate;
	lan966x_fw_config_read_uint8(LAN966X_FW_CONF_MMC_BUS_WIDTH, &bus_width);
	params.bus_width = bus_width;

	info.mmc_dev_type = MMC_IS_EMMC;
	info.max_bus_freq = 48 * 1000 * 1000;
	info.block_size = MMC_BLOCK_SIZE;

	lan966x_mmc_init(&params, &info);
}

/* This is only called for
 *  - BOOT_SOURCE_EMMC
 *  - BOOT_SOURCE_SDMMC
 */
void lan966x_sdmmc_init(boot_source_type boot_source)
{
	switch (boot_source) {
	case BOOT_SOURCE_EMMC:
		VERBOSE("Initializing eMMC\n");

		/* Configure pins for eMMC device */
		plat_lan966x_pinConfig();

		/* Initialize ATF MMC framework */
		lan966x_mmc_plat_config();
		break;
	case BOOT_SOURCE_SDMMC:
		VERBOSE("Initializing SDMMC\n");
		/* Not supported yet */
		assert(false);
		break;
	default:
		break;
	}
}
