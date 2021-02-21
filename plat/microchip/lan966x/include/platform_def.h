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

#define PLAT_MAX_RET_STATE		U(1)

#define PLATFORM_CACHE_LINE_SIZE	64
#define PLATFORM_CLUSTER_COUNT		U(1)
#define PLATFORM_CORE_COUNT_PER_CLUSTER	U(1)
#define PLATFORM_CORE_COUNT		(PLATFORM_CLUSTER_COUNT *	\
					 PLATFORM_CORE_COUNT_PER_CLUSTER)
#define PLAT_MAX_PWR_LVL		(MPIDR_AFFLVL2)
#define PLAT_NUM_PWR_DOMAINS		(PLATFORM_CORE_COUNT + \
					 PLATFORM_CLUSTER_COUNT + U(1))

#define PLAT_MAX_OFF_STATE		U(2)

#define MAX_IO_DEVICES			3
#define MAX_IO_HANDLES			4
/* eMMC RPMB and eMMC User Data */
#define MAX_IO_BLOCK_DEVICES		U(2)

#define LAN966x_PRIMARY_CPU         0x0

/* FVP Power controller base address, ToDo: check address value */
#define PWRC_BASE           UL(0x1c100000)

#define PSYSR_OFF       U(0x10)


/*
 * Other platform porting definitions are provided by included headers
 */

/*
 * Required ARM standard platform porting definitions
 */
#define PLAT_ARM_CLUSTER_COUNT		U(LAN966x_CLUSTER_COUNT)

/*
 * BL1 specific defines.
 */
#define BL1_RO_BASE	UL(0x00000000)
#define BL1_RO_SIZE	UL(1024 * 64)
#define BL1_RO_LIMIT	(BL1_RO_BASE + BL1_RO_SIZE)

#define BL1_RW_BASE	UL(0x00300000)
#define BL1_RW_SIZE	UL(1024 * 64)
#define BL1_RW_LIMIT    (BL1_RW_BASE + BL1_RW_SIZE)

#define BL2_BASE	BL1_RW_LIMIT
#define BL2_SIZE	UL(1024 * 64)
#define BL2_LIMIT	(BL2_BASE + BL2_SIZE)

/*
 * Max size of SPMC is 2MB for fvp. With SPMD enabled this value corresponds to
 * max size of BL32 image.
 */
#if defined(SPD_spmd)
#define PLAT_ARM_SPMC_BASE		PLAT_ARM_TRUSTED_DRAM_BASE
#define PLAT_ARM_SPMC_SIZE		UL(0x200000)  /* 2 MB */
#endif

/* virtual address used by dynamic mem_protect for chunk_base */
#define PLAT_ARM_MEM_PROTEC_VA_FRAME	UL(0xc0000000)

/* No SCP in FVP */
#define PLAT_ARM_SCP_TZC_DRAM1_SIZE	UL(0x0)

#define PLAT_ARM_DRAM2_BASE		ULL(0x880000000)
#define PLAT_ARM_DRAM2_SIZE		UL(0x80000000)

#define PLAT_HW_CONFIG_DTB_BASE		ULL(0x82000000)
#define PLAT_HW_CONFIG_DTB_SIZE		ULL(0x8000)

#define ARM_DTB_DRAM_NS			MAP_REGION_FLAT(		\
					PLAT_HW_CONFIG_DTB_BASE,	\
					PLAT_HW_CONFIG_DTB_SIZE,	\
					MT_MEMORY | MT_RO | MT_NS)
/*
 * Load address of BL33 for this platform port
 */
#define PLAT_ARM_NS_IMAGE_BASE		(ARM_DRAM1_BASE + UL(0x8000000))

/*
 * PLAT_ARM_MMAP_ENTRIES depends on the number of entries in the
 * plat_arm_mmap array defined for each BL stage.
 */
#if defined(IMAGE_BL31)
# if SPM_MM
#  define PLAT_ARM_MMAP_ENTRIES		10
#  define MAX_XLAT_TABLES		9
#  define PLAT_SP_IMAGE_MMAP_REGIONS	30
#  define PLAT_SP_IMAGE_MAX_XLAT_TABLES	10
# else
#  define PLAT_ARM_MMAP_ENTRIES		9
#  if USE_DEBUGFS
#   define MAX_XLAT_TABLES		8
#  else
#   define MAX_XLAT_TABLES		7
#  endif
# endif
#elif defined(IMAGE_BL32)
# define PLAT_ARM_MMAP_ENTRIES		9
# define MAX_XLAT_TABLES		6
#elif !USE_ROMLIB
# define PLAT_ARM_MMAP_ENTRIES		11
# define MAX_XLAT_TABLES		5
#else
# define PLAT_ARM_MMAP_ENTRIES		12
# define MAX_XLAT_TABLES		6
#endif

/*
 * PLAT_ARM_MAX_BL1_RW_SIZE is calculated using the current BL1 RW debug size
 * plus a little space for growth.
 */
#define PLAT_ARM_MAX_BL1_RW_SIZE	UL(0xB000)

/*
 * PLAT_ARM_MAX_ROMLIB_RW_SIZE is define to use a full page
 */

#if USE_ROMLIB
#define PLAT_ARM_MAX_ROMLIB_RW_SIZE	UL(0x1000)
#define PLAT_ARM_MAX_ROMLIB_RO_SIZE	UL(0xe000)
#define LAN966x_BL2_ROMLIB_OPTIMIZATION UL(0x6000)
#else
#define PLAT_ARM_MAX_ROMLIB_RW_SIZE	UL(0)
#define PLAT_ARM_MAX_ROMLIB_RO_SIZE	UL(0)
#define LAN966x_BL2_ROMLIB_OPTIMIZATION UL(0)
#endif

/*
 * PLAT_ARM_MAX_BL2_SIZE is calculated using the current BL2 debug size plus a
 * little space for growth.
 */
#if TRUSTED_BOARD_BOOT
#if COT_DESC_IN_DTB
# define PLAT_ARM_MAX_BL2_SIZE	(UL(0x1E000) - LAN966x_BL2_ROMLIB_OPTIMIZATION)
#else
# define PLAT_ARM_MAX_BL2_SIZE	(UL(0x1D000) - LAN966x_BL2_ROMLIB_OPTIMIZATION)
#endif
#else
# define PLAT_ARM_MAX_BL2_SIZE	(UL(0x13000) - LAN966x_BL2_ROMLIB_OPTIMIZATION)
#endif

#if RESET_TO_BL31
/* Size of Trusted SRAM - the first 4KB of shared memory */
#define PLAT_ARM_MAX_BL31_SIZE		(PLAT_ARM_TRUSTED_SRAM_SIZE - \
					 ARM_SHARED_RAM_SIZE)
#else
/*
 * Since BL31 NOBITS overlays BL2 and BL1-RW, PLAT_ARM_MAX_BL31_SIZE is
 * calculated using the current BL31 PROGBITS debug size plus the sizes of
 * BL2 and BL1-RW
 */
#define PLAT_ARM_MAX_BL31_SIZE		UL(0x3D000)
#endif /* RESET_TO_BL31 */

#ifndef __aarch64__
/*
 * Since BL32 NOBITS overlays BL2 and BL1-RW, PLAT_ARM_MAX_BL32_SIZE is
 * calculated using the current SP_MIN PROGBITS debug size plus the sizes of
 * BL2 and BL1-RW
 */
# define PLAT_ARM_MAX_BL32_SIZE		UL(0x3B000)
#endif

/*
 * Size of cacheable stacks
 */
#if defined(IMAGE_BL1)
# if TRUSTED_BOARD_BOOT
#  define PLATFORM_STACK_SIZE		UL(0x1000)
# else
#  define PLATFORM_STACK_SIZE		UL(0x500)
# endif
#elif defined(IMAGE_BL2)
# if TRUSTED_BOARD_BOOT
#  define PLATFORM_STACK_SIZE		UL(0x1000)
# else
#  define PLATFORM_STACK_SIZE		UL(0x440)
# endif
#elif defined(IMAGE_BL2U)
# define PLATFORM_STACK_SIZE		UL(0x400)
#elif defined(IMAGE_BL31)
#  define PLATFORM_STACK_SIZE		UL(0x800)
#elif defined(IMAGE_BL32)
# define PLATFORM_STACK_SIZE		UL(0x440)
#endif

#define MAX_IO_DEVICES			3
#define MAX_IO_HANDLES			4

/* Reserve the last block of flash for PSCI MEM PROTECT flag */
#define PLAT_ARM_FIP_BASE		V2M_FLASH0_BASE
#define PLAT_ARM_FIP_MAX_SIZE		(V2M_FLASH0_SIZE - V2M_FLASH_BLOCK_SIZE)

#define PLAT_ARM_NVM_BASE		V2M_FLASH0_BASE
#define PLAT_ARM_NVM_SIZE		(V2M_FLASH0_SIZE - V2M_FLASH_BLOCK_SIZE)


#define PLAT_LAN966x_SMMUV3_BASE		UL(0x2b400000)

/* CCI related constants */
#define PLAT_LAN966x_CCI400_BASE		UL(0x2c090000)
#define PLAT_LAN966x_CCI400_CLUS0_SL_PORT	3
#define PLAT_LAN966x_CCI400_CLUS1_SL_PORT	4

/* CCI-500/CCI-550 on Base platform */
#define PLAT_LAN966x_CCI5XX_BASE		UL(0x2a000000)
#define PLAT_LAN966x_CCI5XX_CLUS0_SL_PORT	5
#define PLAT_LAN966x_CCI5XX_CLUS1_SL_PORT	6

/* CCN related constants. Only CCN 502 is currently supported */
#define PLAT_ARM_CCN_BASE		UL(0x2e000000)
#define PLAT_ARM_CLUSTER_TO_CCN_ID_MAP	1, 5, 7, 11

/* System timer related constants */
#define PLAT_ARM_NSTIMER_FRAME_ID		U(1)

/* Mailbox base address */
#define PLAT_ARM_TRUSTED_MAILBOX_BASE	ARM_TRUSTED_SRAM_BASE


/* TrustZone controller related constants
 *
 * Currently only filters 0 and 2 are connected on Base FVP.
 * Filter 0 : CPU clusters (no access to DRAM by default)
 * Filter 1 : not connected
 * Filter 2 : LCDs (access to VRAM allowed by default)
 * Filter 3 : not connected
 * Programming unconnected filters will have no effect at the
 * moment. These filter could, however, be connected in future.
 * So care should be taken not to configure the unused filters.
 *
 * Allow only non-secure access to all DRAM to supported devices.
 * Give access to the CPUs and Virtio. Some devices
 * would normally use the default ID so allow that too.
 */
#define PLAT_ARM_TZC_BASE		UL(0x2a4a0000)
#define PLAT_ARM_TZC_FILTERS		TZC_400_REGION_ATTR_FILTER_BIT(0)

#define PLAT_ARM_TZC_NS_DEV_ACCESS	(				\
		TZC_REGION_ACCESS_RDWR(LAN966x_NSAID_DEFAULT)	|	\
		TZC_REGION_ACCESS_RDWR(LAN966x_NSAID_PCI)		|	\
		TZC_REGION_ACCESS_RDWR(LAN966x_NSAID_AP)		|	\
		TZC_REGION_ACCESS_RDWR(LAN966x_NSAID_VIRTIO)	|	\
		TZC_REGION_ACCESS_RDWR(LAN966x_NSAID_VIRTIO_OLD))

/*
 * GIC related constants to cater for both GICv2 and GICv3 instances of an
 * FVP. They could be overridden at runtime in case the FVP implements the
 * legacy VE memory map.
 */
#define PLAT_ARM_GICD_BASE		BASE_GICD_BASE
#define PLAT_ARM_GICR_BASE		BASE_GICR_BASE
#define PLAT_ARM_GICC_BASE		BASE_GICC_BASE

/*
 * Define a list of Group 1 Secure and Group 0 interrupts as per GICv3
 * terminology. On a GICv2 system or mode, the lists will be merged and treated
 * as Group 0 interrupts.
 */
#define PLAT_ARM_G1S_IRQ_PROPS(grp) \
	ARM_G1S_IRQ_PROPS(grp), \
	INTR_PROP_DESC(LAN966x_IRQ_TZ_WDOG, GIC_HIGHEST_SEC_PRIORITY, (grp), \
			GIC_INTR_CFG_LEVEL), \
	INTR_PROP_DESC(LAN966x_IRQ_SEC_SYS_TIMER, GIC_HIGHEST_SEC_PRIORITY, (grp), \
			GIC_INTR_CFG_LEVEL)

#define PLAT_ARM_G0_IRQ_PROPS(grp)	ARM_G0_IRQ_PROPS(grp)

#if SDEI_IN_FCONF
#define PLAT_SDEI_DP_EVENT_MAX_CNT	ARM_SDEI_DP_EVENT_MAX_CNT
#define PLAT_SDEI_DS_EVENT_MAX_CNT	ARM_SDEI_DS_EVENT_MAX_CNT
#else
#define PLAT_ARM_PRIVATE_SDEI_EVENTS	ARM_SDEI_PRIVATE_EVENTS
#define PLAT_ARM_SHARED_SDEI_EVENTS	ARM_SDEI_SHARED_EVENTS
#endif

#define PLAT_ARM_SP_IMAGE_STACK_BASE	(PLAT_SP_IMAGE_NS_BUF_BASE +	\
					 PLAT_SP_IMAGE_NS_BUF_SIZE)

#define PLAT_SP_PRI			PLAT_RAS_PRI

#define CACHE_WRITEBACK_SHIFT           U(6)
#define CACHE_WRITEBACK_GRANULE         (U(1) << CACHE_WRITEBACK_SHIFT)

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
