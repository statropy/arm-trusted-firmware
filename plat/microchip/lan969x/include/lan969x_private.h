/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN969X_PRIVATE_H
#define LAN969X_PRIVATE_H

#include <stdint.h>

typedef enum {
	LAN969X_STRAP_BOOT_MMC_FC = 0,
	LAN969X_STRAP_BOOT_QSPI_FC = 1,
	LAN969X_STRAP_BOOT_SD_FC = 2,
	LAN969X_STRAP_BOOT_MMC = 3,
	LAN969X_STRAP_BOOT_QSPI = 4,
	LAN969X_STRAP_BOOT_SD = 5,
	LAN969X_STRAP_PCIE_ENDPOINT = 6,
	LAN969X_STRAP_BOOT_MMC_TFAMON_FC = 7,
	LAN969X_STRAP_BOOT_QSPI_TFAMON_FC = 8,
	LAN969X_STRAP_BOOT_SD_TFAMON_FC = 9,
	LAN969X_STRAP_TFAMON_FC0 = 10,
	LAN969X_STRAP_TFAMON_FC2 = 11,
	LAN969X_STRAP_TFAMON_FC3 = 12,
	LAN969X_STRAP_TFAMON_FC4 = 13,
	LAN969X_STRAP_TFAMON_USB = 14,
	LAN969X_STRAP_SPI_SLAVE = 15,
} soc_strapping;

typedef enum {
	BOOT_SOURCE_EMMC = 0,
	BOOT_SOURCE_QSPI,
	BOOT_SOURCE_SDMMC,
	BOOT_SOURCE_NONE
} boot_source_type;

void lan969x_init_strapping(void);
soc_strapping lan969x_get_strapping(void);
void lan969x_set_strapping(soc_strapping value);
boot_source_type lan969x_get_boot_source(void);
bool lan969x_monitor_enabled(void);
void lan969x_fwconfig_apply(void);
void lan969x_set_max_trace_level(void);

void lan969x_console_init(void);
void lan969x_timer_init(void);
void lan969x_io_setup(void);
void lan969x_io_bootsource_init(void);

void lan969x_mmc_plat_config(boot_source_type boot_source);

void lan966x_bootstrap_monitor(void);

void lan966x_ddr_init(void);

void lan969x_tz_init(void);

#endif /* LAN969X_PRIVATE_H */
