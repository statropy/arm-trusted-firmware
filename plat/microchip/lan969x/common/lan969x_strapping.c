/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <drivers/console.h>
#include <lan96xx_common.h>

boot_source_type lan966x_get_boot_source(void)
{
	boot_source_type boot_source;

	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC:
	case LAN966X_STRAP_BOOT_MMC_FC:
		boot_source = BOOT_SOURCE_EMMC;
		break;
	case LAN966X_STRAP_BOOT_QSPI:
	case LAN966X_STRAP_BOOT_QSPI_FC:
	case LAN966X_STRAP_BOOT_QSPI_HS_FC:
	case LAN966X_STRAP_BOOT_QSPI_HS:
		boot_source = BOOT_SOURCE_QSPI;
		break;
	case LAN966X_STRAP_BOOT_SD:
	case LAN966X_STRAP_BOOT_SD_FC:
		boot_source = BOOT_SOURCE_SDMMC;
		break;
	default:
		boot_source = BOOT_SOURCE_NONE;
		break;
	}

	return boot_source;
}

bool lan966x_bootable_source(void)
{
	boot_source_type boot_source = lan966x_get_boot_source();

	switch (boot_source) {
	case BOOT_SOURCE_QSPI:
	case BOOT_SOURCE_EMMC:
	case BOOT_SOURCE_SDMMC:
		return true;
	default:
		break;
	}

	return false;
}

bool lan966x_monitor_enabled(void)
{
	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_TFAMON_FC0:
	case LAN966X_STRAP_TFAMON_FC0_HS:
		return true;
	default:
		break;
	}

	return false;
}
