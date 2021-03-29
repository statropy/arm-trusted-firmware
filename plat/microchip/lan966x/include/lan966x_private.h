/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_PRIVATE_H
#define LAN966X_PRIVATE_H

enum {
	LAN966X_STRAP_BOOT_MMC = 0,
	LAN966X_STRAP_BOOT_QSPI,
	LAN966X_STRAP_BOOT_SD,
	LAN966X_STRAP_BOOT_QSPI_CONT,
	LAN966X_STRAP_BOOT_EMMC_CONT,
	LAN966X_STRAP_PCIE_ENDPOINT,
	LAN966X_STRAP_BOOT_QSPI_XIP,
	LAN966X_STRAP_RESERVED_0,
	LAN966X_STRAP_RESERVED_1,
	LAN966X_STRAP_RESERVED_2,
	LAN966X_STRAP_SAMBA_FC0,
	LAN966X_STRAP_SAMBA_FC2,
	LAN966X_STRAP_SAMBA_FC3,
	LAN966X_STRAP_SAMBA_FC4,
	LAN966X_STRAP_SAMBA_USB,
	LAN966X_STRAP_SPI_SLAVE,
};

uint8_t lan966x_get_strapping(void);
void    lan966x_set_strapping(uint8_t value);

void lan966x_bootstrap_monitor(void);
void lan966x_console_init(void);
void lan966x_init_timer(void);
void lan966x_io_setup(void);
void lan966x_ddr_init(void);
void lan966x_tz_init(void);
void lan966x_trng_init(void);

uint32_t lan966x_trng_read(void);

void plat_lan966x_gic_driver_init(void);
void plat_lan966x_gic_init(void);

#endif /* LAN966X_PRIVATE_H */
