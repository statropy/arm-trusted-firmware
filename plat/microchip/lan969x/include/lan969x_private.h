/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN969X_PRIVATE_H
#define LAN969X_PRIVATE_H

#include <stdint.h>

typedef enum {
	LAN969X_STRAP_BOOT_MMC   = 0,
	LAN969X_STRAP_BOOT_QSPI  = 1,
	LAN969X_STRAP_BOOT_SD    = 2,
	LAN969X_STRAP_TFAMON_FC  = 8,
	LAN969X_STRAP_TFAMON_USB = 9,
	LAN969X_STRAP_SPI_SLAVE  = 10,
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

void lan969x_console_init(void);
void lan969x_timer_init(void);
void lan969x_io_setup(void);

void lan969x_mmc_plat_config(boot_source_type boot_source);

void lan966x_bootstrap_monitor(void);

void lan966x_ddr_init(void);

void lan969x_tz_init(void);

#endif /* LAN969X_PRIVATE_H */
