/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef EMMC_H
#define EMMC_H

#include <drivers/mmc.h>

#define MMC_INIT_SPEED		375000u
#define MMC_DEFAULT_SPEED	10000000u
#define SD_NORM_SPEED		25000000u
#define SD_HIGH_SPEED		50000000u
#define EMMC_NORM_SPEED		26000000u
#define EMMC_HIGH_SPEED		52000000u

#define SD_CARD_SUPP_VOLT		0x00FF8000

typedef struct lan966x_mmc_params {
	uintptr_t reg_base;
	uintptr_t desc_base;
	size_t desc_size;
	int clk_rate;
	int bus_width;
	unsigned int flags;
	enum mmc_device_type mmc_dev_type;
} lan966x_mmc_params_t;

void lan966x_mmc_init(lan966x_mmc_params_t * params,
		      struct mmc_device_info *info);

#endif	/* EMMC_H */
