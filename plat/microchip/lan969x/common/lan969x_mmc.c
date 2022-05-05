/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <drivers/microchip/vcore_gpio.h>
#include <drivers/microchip/emmc.h>
#include <fw_config.h>
#include <lan96xx_mmc.h>

#include "lan969x_regs.h"
#include "lan969x_private.h"

void plat_lan966x_pinConfig(boot_source_type mode)
{
	vcore_gpio_init(GCB_GPIO_OUT_SET(LAN969X_GCB_BASE));

	switch (mode) {
	case BOOT_SOURCE_EMMC:
		vcore_gpio_set_alt(14, 1);	//SDMMC0_CMD
		vcore_gpio_set_alt(15, 1);	//SDMMC0_CK
		vcore_gpio_set_alt(16, 1);	//SDMMC0_DAT0
		vcore_gpio_set_alt(17, 1);	//SDMMC0_DAT1
		vcore_gpio_set_alt(18, 1);	//SDMMC0_DAT2
		vcore_gpio_set_alt(19, 1);	//SDMMC0_DAT3
		vcore_gpio_set_alt(20, 1);	//SDMMC0_DAT4
		vcore_gpio_set_alt(21, 1);	//SDMMC0_DAT5
		vcore_gpio_set_alt(22, 1);	//SDMMC0_DAT6
		vcore_gpio_set_alt(23, 1);	//SDMMC0_DAT7
		vcore_gpio_set_alt(24, 1);	//SDMMC0_RSTN
		break;
	case BOOT_SOURCE_SDMMC:
		vcore_gpio_set_alt(16, 1);	//SDMMC0_CMD
		vcore_gpio_set_alt(17, 1);	//SDMMC0_CK
		vcore_gpio_set_alt(18, 1);	//SDMMC0_DAT0
		vcore_gpio_set_alt(19, 1);	//SDMMC0_DAT1
		vcore_gpio_set_alt(20, 1);	//SDMMC0_DAT2
		vcore_gpio_set_alt(21, 1);	//SDMMC0_DAT3
		vcore_gpio_set_alt(22, 1);	//SDMMC0_1V8SEL
		vcore_gpio_set_alt(23, 1);	//SDMMC0_WP
		vcore_gpio_set_alt(25, 1);	//SDMMC0_CD
		//vcore_gpio_set_alt(xx, 4);	//SDMMC0_LED (unused)
		break;
	default:
		ERROR("BL1: Not supported pin config mode: %d\n", mode);
		break;
	}
}
