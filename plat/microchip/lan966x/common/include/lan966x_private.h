/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_PRIVATE_H
#define LAN966X_PRIVATE_H

#include <stdint.h>
#include <plat_crypto.h>

typedef struct {
	void *fw_config;
	void *mbedtls_heap_addr;
	size_t mbedtls_heap_size;
} shared_memory_desc_t;

extern shared_memory_desc_t shared_memory_desc;

#define FW_PARTITION_NAME		"fip"
#define FW_BACKUP_PARTITION_NAME	"fip.bak"

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

void lan966x_init_strapping(void);
uint8_t lan966x_get_strapping(void);
bool lan966x_monitor_enabled(void);
void lan966x_set_max_trace_level(void);
void lan966x_reset_max_trace_level(void);

void lan966x_console_init(void);
void lan966x_timer_init(void);
void lan966x_io_bootsource_init(void);
void lan966x_io_setup(void);
void lan966x_ddr_init(void);
void lan966x_tz_init(void);
void lan966x_pcie_init(void);
void lan966x_usb_init(uint32_t bias, uint32_t rbias);
void lan966x_usb_register_console(void);
void lan966x_validate_strapping(void);
void lan966x_crash_console(console_t *console);

void lan966x_mmc_plat_config(boot_source_type boot_source);

boot_source_type lan966x_get_boot_source(void);
void lan966x_fwconfig_apply(void);
int lan966x_set_fip_addr(unsigned int image_id, const char *name);

#if defined(LAN966X_AES_TESTS)
void lan966x_crypto_tests(void);
#endif

void lan966x_crypto_ecdsa_tests(void);

void lan966x_mbed_heap_set(shared_memory_desc_t *d);

#if defined(LAN966X_EMMC_TESTS)
void lan966x_emmc_tests(void);
#endif

#endif /* LAN966X_PRIVATE_H */
