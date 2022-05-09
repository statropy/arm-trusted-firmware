/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN969X_MEMMAP_H
#define LAN969X_MEMMAP_H

#include <lib/xlat_tables/xlat_tables_compat.h>

#define MAP_SILEX_REGS		MAP_REGION_FLAT(			\
					LAN969X_SILEX_REGS_BASE,	\
					LAN969X_SILEX_REGS_SIZE,	\
					MT_DEVICE | MT_RW | MT_SECURE)

#define MAP_SILEX_RAM		MAP_REGION_FLAT(			\
					LAN969X_SILEX_RAM_BASE,		\
					LAN969X_SILEX_RAM_SIZE,		\
					MT_DEVICE | MT_RW | MT_SECURE)

#endif /* LAN969X_MEMMAP_H */
