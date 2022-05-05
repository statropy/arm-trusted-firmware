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

#include "lan966x_regs.h"
#include "lan966x_private.h"

void plat_lan966x_pinConfig(boot_source_type mode)
{
	vcore_gpio_init(GCB_GPIO_OUT_SET(LAN966X_GCB_BASE));

	switch (mode) {
	case BOOT_SOURCE_EMMC:
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
		break;
	case BOOT_SOURCE_SDMMC:
		vcore_gpio_set_alt(67, 1);	//PIO_67_SDMMC0_CMD
		vcore_gpio_set_alt(68, 1);	//PIO_68_SDMMC0_CK
		vcore_gpio_set_alt(69, 1);	//PIO_69_SDMMC0_DAT0
		vcore_gpio_set_alt(70, 1);	//PIO_70_SDMMC0_DAT1
		vcore_gpio_set_alt(71, 1);	//PIO_71_SDMMC0_DAT2
		vcore_gpio_set_alt(72, 1);	//PIO_72_SDMMC0_DAT3
		vcore_gpio_set_alt(73, 4);	//PIO_73_SDMMC0_1V8SEL
		vcore_gpio_set_alt(74, 4);	//PIO_74_SDMMC0_WP
		vcore_gpio_set_alt(75, 4);	//PIO_75_SDMMC0_CD
		vcore_gpio_set_alt(76, 4);	//PIO_76_SDMMC0_LED (unused)
		break;
	default:
		ERROR("BL1: Not supported pin config mode: %d\n", mode);
		break;
	}
}
