/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <arch.h>
#include <bl1/bl1.h>
#include <common/bl_common.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/microchip/otp.h>
#include <fw_config.h>
#include <lib/fconf/fconf.h>
#include <lib/utils.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <plat/arm/common/plat_arm.h>
#include <plat/common/platform.h>
#include <plat/microchip/common/lan966x_sjtag.h>
#include <plat/microchip/common/plat_bootstrap.h>

#include "lan969x_private.h"

#define MAP_BL1_TOTAL   MAP_REGION_FLAT(				\
		bl1_tzram_layout.total_base,				\
		bl1_tzram_layout.total_size,				\
		MT_MEMORY | MT_RW | MT_SECURE)
/*
 * If SEPARATE_CODE_AND_RODATA=1 we define a region for each section
 * otherwise one region is defined containing both
 */
#if SEPARATE_CODE_AND_RODATA
#define MAP_BL1_RO	MAP_REGION_FLAT(				\
		BL_CODE_BASE,						\
		BL1_CODE_END - BL_CODE_BASE,				\
		MT_CODE | MT_SECURE),					\
		MAP_REGION_FLAT(					\
			BL1_RO_DATA_BASE,				\
			BL1_RO_DATA_END					\
			- BL_RO_DATA_BASE,				\
			MT_RO_DATA | MT_SECURE)
#else
#define MAP_BL1_RO	MAP_REGION_FLAT(				\
		BL_CODE_BASE,						\
		BL1_CODE_END - BL_CODE_BASE,				\
		MT_CODE | MT_SECURE)
#endif

/* Data structure which holds the extents of the trusted SRAM for BL1*/
static meminfo_t bl1_tzram_layout;

/* Boolean variable to hold condition whether firmware update needed or not */
static bool is_fwu_needed;

struct meminfo *bl1_plat_sec_mem_layout(void)
{
	return &bl1_tzram_layout;
}

void bl1_early_platform_setup(void)
{
	/* Strapping */
	lan969x_init_strapping();

	/* Console */
	lan969x_console_init();

	/* Timer */
	lan969x_timer_init();

	/* Enable arch timer */
	generic_delay_timer_init();

	/* Allow BL1 to see the whole Trusted RAM */
	bl1_tzram_layout.total_base = LAN969X_SRAM_BASE;
	bl1_tzram_layout.total_size = LAN969X_SRAM_SIZE;
}

void bl1_plat_arch_setup(void)
{
	const mmap_region_t bl_regions[] = {
		MAP_BL1_TOTAL,
		MAP_BL1_RO,
		{0}
	};

	setup_page_tables(bl_regions, plat_arm_get_mmap());
#ifdef __aarch64__
	enable_mmu_el3(0);
#else
	enable_mmu_svc_mon(0);
#endif /* __aarch64__ */
}

static void bl1_io_setup(void)
{
	static bool is_initialized;
	if (!is_initialized) {
		if (lan969x_get_boot_source() != BOOT_SOURCE_NONE) {
			lan969x_io_setup();
			/* Prepare fw_config from applicable boot source */
			lan966x_load_fw_config(FW_CONFIG_ID);
			lan969x_fwconfig_apply();
			is_initialized = true;
		}
	}
}

void bl1_platform_setup(void)
{
	/* IO */
	bl1_io_setup();

	/* SJTAG: Configure challenge, no freeze */
	lan966x_sjtag_configure();

	/* Strapped for boot monitor? */
	if (lan969x_monitor_enabled()) {
		plat_bl1_bootstrap_monitor();
	}
}

void bl1_plat_prepare_exit(entry_point_info_t *ep_info)
{
}

void plat_bootstrap_trigger_fwu(void)
{
	is_fwu_needed = true;
}

/*******************************************************************************
 * The following function checks if Firmware update is needed,
 * by checking if TOC in FIP image is valid or not.
 ******************************************************************************/
unsigned int bl1_plat_get_next_image_id(void)
{
	return is_fwu_needed ? NS_BL1U_IMAGE_ID : BL2_IMAGE_ID;
}
