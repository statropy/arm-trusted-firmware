/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_MEMMAP_H
#define LAN966X_MEMMAP_H

#include <lib/xlat_tables/xlat_tables_compat.h>

#define MAP_PKCL_CODE		MAP_REGION_FLAT(			\
					LAN966X_PKCL_ROM_BASE,		\
					LAN966X_PKCL_ROM_SIZE,		\
					MT_CODE | MT_SECURE)

#define MAP_PKCL_DATA		MAP_REGION_FLAT(			\
					LAN966X_PKCL_RAM_BASE,		\
					LAN966X_PKCL_RAM_SIZE,		\
					MT_DEVICE | MT_RW | MT_SECURE)

#endif /* LAN966X_MEMMAP_H */
