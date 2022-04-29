/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_PRIVATE_H
#define LAN966X_PRIVATE_H

#include <stdint.h>

typedef struct {
	void *fw_config;
	void *mbedtls_heap_addr;
	size_t mbedtls_heap_size;
} shared_memory_desc_t;

extern shared_memory_desc_t shared_memory_desc;

#define LAN966X_KEY32_LEN	32
typedef struct {
	union {
		uint8_t  b[LAN966X_KEY32_LEN];
		uint32_t w[LAN966X_KEY32_LEN / 4];
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
	LAN966X_STRAP_BOOT_MMC_FC = 0,
	LAN966X_STRAP_BOOT_QSPI_FC = 1,
	LAN966X_STRAP_BOOT_SD_FC = 2,
	LAN966X_STRAP_BOOT_MMC = 3,
	LAN966X_STRAP_BOOT_QSPI = 4,
	LAN966X_STRAP_BOOT_SD = 5,
	LAN966X_STRAP_PCIE_ENDPOINT = 6,
	LAN966X_STRAP_BOOT_MMC_TFAMON_FC = 7,
	LAN966X_STRAP_BOOT_QSPI_TFAMON_FC = 8,
	LAN966X_STRAP_BOOT_SD_TFAMON_FC = 9,
	LAN966X_STRAP_TFAMON_FC0 = 10,
	LAN966X_STRAP_TFAMON_FC2 = 11,
	LAN966X_STRAP_TFAMON_FC3 = 12,
	LAN966X_STRAP_TFAMON_FC4 = 13,
	LAN966X_STRAP_TFAMON_USB = 14,
	LAN966X_STRAP_SPI_SLAVE = 15,
};

typedef enum {
	BOOT_SOURCE_EMMC = 0,
	BOOT_SOURCE_QSPI,
	BOOT_SOURCE_SDMMC,
	BOOT_SOURCE_NONE
} boot_source_type;

struct usb_trim {
	bool valid;
	uint32_t bias;
	uint32_t rbias;
};

void lan966x_init_strapping(void);
uint8_t lan966x_get_strapping(void);
void lan966x_set_strapping(uint8_t value);
bool lan966x_monitor_enabled(void);
void lan966x_set_max_trace_level(void);
void lan966x_reset_max_trace_level(void);

void lan966x_bootstrap_monitor(void);
void lan966x_console_init(void);
void lan966x_timer_init(void);
void lan966x_io_bootsource_init(void);
void lan966x_io_init_dev(boot_source_type boot_source);
void lan966x_io_setup(void);
void lan966x_bl1_io_enable_ram_fip(size_t offset, size_t length);
void lan966x_ddr_init(void);
void lan966x_tz_init(void);
void lan966x_pcie_init(void);
void lan966x_usb_init(const struct usb_trim *trim);
void lan966x_usb_register_console(void);
void lan966x_validate_strapping(void);
void lan966x_crash_console(console_t *console);

uint32_t lan966x_trng_read(void);

void plat_lan966x_gic_driver_init(void);
void plat_lan966x_gic_init(void);
void lan966x_mmc_plat_config(boot_source_type boot_source);

uint32_t Crc32c(uint32_t crc, const void *data, size_t size);
boot_source_type lan966x_get_boot_source(void);
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


#if defined(LAN966X_AES_TESTS)
void lan966x_crypto_tests(void);
#endif

void lan966x_crypto_ecdsa_tests(void);

void lan966x_mbed_heap_set(shared_memory_desc_t *d);

void lan966x_bl1_trigger_fwu(void);

#if defined(LAN966X_EMMC_TESTS)
void lan966x_emmc_tests(void);
#endif

#endif /* LAN966X_PRIVATE_H */
