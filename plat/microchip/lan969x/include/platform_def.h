/*
 * Copyright (c) 2014-2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLATFORM_DEF_H
#define PLATFORM_DEF_H

#include <drivers/arm/tzc400.h>
#include <lib/utils_def.h>
#include <plat/common/common_def.h>

#include "lan969x_def.h"

/*
 * Generic platform constants
 */

#define PLAT_MAX_RET_STATE		U(1)

#define PLATFORM_CACHE_LINE_SIZE	64
#define PLATFORM_CLUSTER_COUNT		U(1)
#define PLATFORM_CORE_COUNT_PER_CLUSTER	U(1)
#define PLATFORM_CORE_COUNT		U(1)
#define PLAT_MAX_PWR_LVL		(MPIDR_AFFLVL2)
#define PLAT_NUM_PWR_DOMAINS		(PLATFORM_CORE_COUNT + \
					 PLATFORM_CLUSTER_COUNT + U(1))

#define PLAT_MAX_OFF_STATE		U(2)

#define MAX_IO_DEVICES			4
#define MAX_IO_HANDLES			4
/* eMMC RPMB and eMMC User Data */
#define MAX_IO_BLOCK_DEVICES		U(2)

#define LAN969x_PRIMARY_CPU		0x0

/*
 * BL1 specific defines.
 */
#define BL1_RO_BASE		LAN969X_BOOTROM_BASE
#define BL1_RO_SIZE		LAN969X_BOOTROM_SIZE
#define BL1_RO_LIMIT		(BL1_RO_BASE + BL1_RO_SIZE)

/*
 * Put BL1 RW at the top of the Secure SRAM. BL1_RW_BASE is calculated using
 * the current BL1 RW debug size plus a little space for growth.
 */
#define BL1_RW_BASE		(BL1_RW_LIMIT - BL1_RW_SIZE)
#define BL1_RW_SIZE		SIZE_K(64)
#define BL1_RW_LIMIT		(LAN969X_SRAM_BASE + LAN969X_SRAM_SIZE)

/*
 * BL2 - Entire SRAM excl. BL1_RW
 */
#define BL2_BASE		LAN969X_SRAM_BASE
#define BL2_SIZE       		(LAN969X_SRAM_SIZE - BL1_RW_SIZE)
#define BL2_LIMIT		(BL2_BASE + BL2_SIZE)

/*
 * BL2U - As BL2
 */
#define BL2U_BASE		BL2_BASE
#define BL2U_SIZE		BL2_SIZE
#define BL2U_LIMIT		BL2_LIMIT

/*
 * BL1 bootstrap download area
 */
#define BL1_MON_MIN_OFFSET	SIZE_K(4)
#define BL1_MON_MAX_SIZE	(BL2_SIZE - BL1_MON_MIN_OFFSET)
#define BL1_MON_MIN_BASE	(BL2_BASE + BL1_MON_MIN_OFFSET)
#define BL1_MON_LIMIT		BL2_LIMIT

/*
 * BL32 - start of DDR
 */
#define BL32_BASE		LAN969X_DDR_BASE
#define BL32_SIZE		SIZE_M(2)
#define BL32_LIMIT		(BL32_BASE + BL32_SIZE)

/*
 * BL33 - top of DDR
 */

#define PLAT_LAN969X_NS_IMAGE_BASE	BL32_LIMIT
#define PLAT_LAN969X_NS_IMAGE_SIZE	(LAN969X_DDR_SIZE - BL32_SIZE)
#define PLAT_LAN969X_NS_IMAGE_LIMIT	(PLAT_LAN969X_NS_IMAGE_BASE + PLAT_LAN969X_NS_IMAGE_SIZE)

#if LAN969X_DIRECT_LINUX_BOOT
#define LAN969X_LINUX_DTB_BASE		(PLAT_LAN969X_NS_IMAGE_BASE + SIZE_M(16))
#endif

/*
 * Default FlexCom console
 */
#ifndef FC_DEFAULT
#define FC_DEFAULT		FLEXCOM0
#endif

/*
 * Size of cacheable stacks
 */
#if defined(IMAGE_BL1)
# define PLATFORM_STACK_SIZE		UL(0x1000)
#elif defined(IMAGE_BL2)
# define PLATFORM_STACK_SIZE		UL(0x1000)
#elif defined(IMAGE_BL2U)
# define PLATFORM_STACK_SIZE		UL(0x400)
#elif defined(IMAGE_BL31)
#  define PLATFORM_STACK_SIZE		UL(0x800)
#elif defined(IMAGE_BL32)
# define PLATFORM_STACK_SIZE		UL(0x440)
#endif

#if defined(IMAGE_BL1)
#define MAX_XLAT_TABLES			3
#elif defined(IMAGE_BL2) || defined(IMAGE_BL2U)
#define MAX_XLAT_TABLES			3
#elif defined(IMAGE_BL32)
#define MAX_XLAT_TABLES			8
#endif

#define MAX_MMAP_REGIONS		16

#define CACHE_WRITEBACK_SHIFT		U(6)
#define CACHE_WRITEBACK_GRANULE		(U(1) << CACHE_WRITEBACK_SHIFT)

/*
 * Physical and virtual address space limits for MMU in AARCH64 & AARCH32 modes
 */
#ifdef __aarch64__
#define PLAT_PHY_ADDR_SPACE_SIZE	(1ULL << 36)
#define PLAT_VIRT_ADDR_SPACE_SIZE	(1ULL << 36)
#else
#define PLAT_PHY_ADDR_SPACE_SIZE	(1ULL << 32)
#define PLAT_VIRT_ADDR_SPACE_SIZE	(1ULL << 32)
#endif

#endif /* PLATFORM_DEF_H */
