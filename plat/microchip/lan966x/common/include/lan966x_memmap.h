/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_MEMMAP_H
#define LAN966X_MEMMAP_H

#include <lib/xlat_tables/xlat_tables_compat.h>

/*
 * Due to chip architecture, PKCL cannot be cached.
 */
#define PKCL_CODE		(MT_NON_CACHEABLE | MT_RO | MT_EXECUTE)

#define MAP_PKCL_CODE		MAP_REGION_FLAT(			\
					LAN996X_PKCL_ROM_BASE,		\
					LAN996X_PKCL_ROM_SIZE,		\
					PKCL_CODE | MT_SECURE)

#define MAP_PKCL_DATA		MAP_REGION_FLAT(			\
					LAN996X_PKCL_RAM_BASE,		\
					LAN996X_PKCL_RAM_SIZE,		\
					MT_DEVICE | MT_RW | MT_SECURE)

#endif /* LAN966X_MEMMAP_H */
