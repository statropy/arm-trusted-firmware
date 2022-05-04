
/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLAT_COMMON_H
#define PLAT_COMMON_H

#define FW_PARTITION_NAME		"fip"
#define FW_BACKUP_PARTITION_NAME	"fip.bak"

typedef enum {
	BOOT_SOURCE_EMMC = 0,
	BOOT_SOURCE_QSPI,
	BOOT_SOURCE_SDMMC,
	BOOT_SOURCE_NONE
} boot_source_type;

boot_source_type lan966x_get_boot_source(void);

void lan966x_io_init_dev(boot_source_type boot_source);
void lan966x_io_setup(void);

#endif	/* PLAT_COMMON_H */
