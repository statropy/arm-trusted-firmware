/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_PRIVATE_H
#define LAN966X_PRIVATE_H

#include <stdint.h>

#define OTP_EMU_MAX_DATA	512
#define FW_CONFIG_MAX_PARAM	32

typedef struct {
	uint8_t otp_emu_data[OTP_EMU_MAX_DATA];
	uint32_t config[FW_CONFIG_MAX_PARAM];
} lan966x_fw_config_t;

extern lan966x_fw_config_t lan966x_fw_config;

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

typedef enum {
  BOOT_SOURCE_EMMC = 0,
  BOOT_SOURCE_SDMMC,
  BOOT_SOURCE_QSPI,
  BOOT_SOURCE_NONE
} boot_source_type;


uint8_t lan966x_get_strapping(void);
void	lan966x_set_strapping(uint8_t value);

void lan966x_bootstrap_monitor(void);
void lan966x_console_init(void);
void lan966x_timer_init(void);
void lan966x_io_init(void);
void lan966x_io_setup(void);
void lan966x_ddr_init(void);
void lan966x_tz_init(void);
void lan966x_sdmmc_init(void);

uint32_t lan966x_trng_read(void);

void plat_lan966x_gic_driver_init(void);
void plat_lan966x_gic_init(void);

uint32_t Crc32c(uint32_t crc, const void *data, size_t size);
uint32_t lan966x_get_boot_source(void);
int lan966x_set_fip_addr(unsigned int image_id, const char *name);

#endif /* LAN966X_PRIVATE_H */
