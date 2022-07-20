/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN96XX_COMMON_H
#define LAN96XX_COMMON_H

#include <stdbool.h>

#define FW_PARTITION_NAME		"fip"
#define FW_BACKUP_PARTITION_NAME	"fip.bak"

typedef enum {
	LAN966X_STRAP_BOOT_MMC_FC = 0,
	LAN966X_STRAP_BOOT_QSPI_FC = 1,
	LAN966X_STRAP_BOOT_SD_FC = 2,
	LAN966X_STRAP_BOOT_MMC = 3,
	LAN966X_STRAP_BOOT_QSPI = 4,
	LAN966X_STRAP_BOOT_SD = 5,
	LAN966X_STRAP_PCIE_ENDPOINT = 6,
#if defined(MCHP_SOC_LAN966X)
	LAN966X_STRAP_BOOT_MMC_TFAMON_FC = 7,
	LAN966X_STRAP_BOOT_QSPI_TFAMON_FC = 8,
	LAN966X_STRAP_BOOT_SD_TFAMON_FC = 9,
#else
	_LAN966X_STRAP_BOOT_MMC_FC_ALIAS = 7,
	_LAN966X_STRAP_BOOT_QSPI_FC_ALIAS = 8,
	_LAN966X_STRAP_BOOT_SD_FC_ALIAS = 9,
#endif
	LAN966X_STRAP_TFAMON_FC0 = 10,
#if defined(MCHP_SOC_LAN966X)
	LAN966X_STRAP_TFAMON_FC2 = 11,
	LAN966X_STRAP_TFAMON_FC3 = 12,
	LAN966X_STRAP_TFAMON_FC4 = 13,
	LAN966X_STRAP_TFAMON_USB = 14,
#else
	LAN966X_STRAP_TFAMON_FC0_HS = 11,
	_LAN966X_STRAP_BOOT_MMC_ALIAS = 12,
	_LAN966X_STRAP_BOOT_QSPI_ALIAS = 13,
	_LAN966X_STRAP_BOOT_SD_ALIAS = 14,
#endif
	LAN966X_STRAP_SPI_SLAVE = 15,
} soc_strapping;

typedef enum {
	BOOT_SOURCE_EMMC = 0,
	BOOT_SOURCE_QSPI,
	BOOT_SOURCE_SDMMC,
	BOOT_SOURCE_NONE
} boot_source_type;

void lan966x_init_strapping(void);
void lan966x_set_strapping(soc_strapping value);
soc_strapping lan966x_get_strapping(void);
bool lan966x_monitor_enabled(void);

boot_source_type lan966x_get_boot_source(void);
bool lan966x_bootable_source(void);

void lan966x_bl2u_io_init_dev(boot_source_type boot_source);

void lan966x_io_setup(void);
void lan966x_io_bootsource_init(void);

#endif	/* LAN96XX_COMMON_H */
