/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN96XX_MMC_H
#define LAN96XX_MMC_H

#include <lan96xx_common.h>

void plat_lan966x_pinConfig(boot_source_type mode);
void lan966x_mmc_plat_config(boot_source_type boot_source);

#endif	/* LAN96XX_MMC_H */
