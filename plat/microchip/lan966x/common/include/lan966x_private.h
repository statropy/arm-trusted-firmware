/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_PRIVATE_H
#define LAN966X_PRIVATE_H

#include <stdint.h>

#define LAN966X_KEY32_LEN	32
typedef struct {
	union {
		uint8_t b[LAN966X_KEY32_LEN];
		uint8_t w[LAN966X_KEY32_LEN / 4];
	};
} lan966x_key32_t;

typedef enum {
	LAN966X_FW_CONF_MMC_CLK_RATE	= 0,	// mmc clock frequency	- word access
	LAN966X_FW_CONF_MMC_BUS_WIDTH	= 4,	// mmc bus width	- byte access
	LAN966X_FW_CONF_QSPI_CLK 	= 5,	// qspi clock frequency	- byte access
	LAN966X_FW_CONF_NUM_OF_ITEMS
} lan966x_fw_cfg_data;

#define OTP_EMU_MAX_DATA	384
#define FW_CONFIG_MAX_DATA	128

#define FW_PARTITION_NAME		"fip"
#define FW_BACKUP_PARTITION_NAME	"fip.bak"

typedef struct {
	uint8_t otp_emu_data[OTP_EMU_MAX_DATA];
	uint8_t config[FW_CONFIG_MAX_DATA];
} lan966x_fw_config_t;

extern lan966x_fw_config_t lan966x_fw_config;

enum {
	LAN966X_STRAP_BOOT_MMC = 0,
	LAN966X_STRAP_BOOT_QSPI,
	LAN966X_STRAP_BOOT_SD,
	LAN966X_STRAP_RESERVED_3,
	LAN966X_STRAP_RESERVED_4,
	LAN966X_STRAP_PCIE_ENDPOINT,
	LAN966X_STRAP_RESERVED_5,
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
	BOOT_SOURCE_QSPI,
	BOOT_SOURCE_SDMMC,
	BOOT_SOURCE_NONE
} boot_source_type;

uint8_t lan966x_get_strapping(void);
void lan966x_set_strapping(uint8_t value);

void lan966x_bootstrap_monitor(void);
void lan966x_console_init(void);
void lan966x_timer_init(void);
void lan966x_io_bootsource_init(void);
void lan966x_io_setup(void);
void lan966x_ddr_init(void);
void lan966x_tz_init(void);
void lan966x_sdmmc_init(boot_source_type boot_source);
void lan966x_pcie_init(void);

uint32_t lan966x_trng_read(void);

void plat_lan966x_gic_driver_init(void);
void plat_lan966x_gic_init(void);
void lan966x_mmc_plat_config(void);

uint32_t Crc32c(uint32_t crc, const void *data, size_t size);
uint32_t lan966x_get_boot_source(void);
int lan966x_load_fw_config(unsigned int image_id);
void lan966x_fwconfig_apply(void);
int lan966x_get_fw_config_data(lan966x_fw_cfg_data id);
int lan966x_fw_config_read_uint8(unsigned int offset, uint8_t *dst);
int lan966x_fw_config_read_uint16(unsigned int offset, uint16_t *dst);
int lan966x_fw_config_read_uint32(unsigned int offset, uint32_t *dst);
int lan966x_set_fip_addr(unsigned int image_id, const char *name);

int lan966x_derive_key(const lan966x_key32_t *in, const lan966x_key32_t *salt, lan966x_key32_t *out);

void lan966x_sjtag_configure(void);
int  lan966x_sjtag_read_challenge(lan966x_key32_t *k);
int  lan966x_sjtag_write_response(const lan966x_key32_t *k);

#endif	/* LAN966X_PRIVATE_H */
