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
#include <lib/utils.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <plat/arm/common/plat_arm.h>
#include <plat/common/platform.h>

#include "lan966x_private.h"
#include "lan966x_memmap.h"

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
static meminfo_t bl2_tzram_layout;

/* Boolean variable to hold condition whether firmware update needed or not */
static bool is_fwu_needed;

struct meminfo *bl1_plat_sec_mem_layout(void)
{
	return &bl1_tzram_layout;
}

void bl1_early_platform_setup(void)
{
	/* Timer */
	lan966x_timer_init();

	/* Enable arch timer */
	generic_delay_timer_init();

	/* Strapping */
	lan966x_init_strapping();

	/* Limit trace level if needed */
	lan966x_set_max_trace_level();

	/* Console */
	lan966x_console_init();

	/* Check bootstrap mask: this may abort */
	lan966x_validate_strapping();

	/* PCIe - may never return */
	lan966x_pcie_init();

	/* Allow BL1 to see the whole Trusted RAM */
	bl1_tzram_layout.total_base = LAN966X_SRAM_BASE;
	bl1_tzram_layout.total_size = LAN966X_SRAM_SIZE;
}

void bl1_plat_arch_setup(void)
{
	const mmap_region_t bl_regions[] = {
		MAP_BL1_TOTAL,
		MAP_PKCL_CODE,
		MAP_PKCL_DATA,
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

static bool lan966x_bootable_source(void)
{
	boot_source_type boot_source = lan966x_get_boot_source();

	switch (boot_source) {
	case BOOT_SOURCE_QSPI:
	case BOOT_SOURCE_EMMC:
	case BOOT_SOURCE_SDMMC:
		return true;
	default:
		break;
	}

	return false;
}

void lan966x_bl1_trigger_fwu(void)
{
	is_fwu_needed = true;
}

void bl1_platform_setup(void)
{
	/* IO */
	lan966x_io_setup();

	/* Prepare fw_config from applicable boot source */
	if (lan966x_bootable_source()) {
		lan966x_load_fw_config(FW_CONFIG_ID);
		lan966x_fwconfig_apply();
	}

	/* SJTAG: Configure challenge, no freeze */
	lan966x_sjtag_configure();

	/* Strapped for boot monitor? */
	if (lan966x_monitor_enabled()) {
		lan966x_bootstrap_monitor();
	}
}

void bl1_plat_prepare_exit(entry_point_info_t *ep_info)
{
}

/*******************************************************************************
 * The following function checks if Firmware update is needed,
 * which may be triggered by the monitor mode
 ******************************************************************************/
unsigned int bl1_plat_get_next_image_id(void)
{
	unsigned int img = is_fwu_needed ? BL2U_IMAGE_ID : BL2_IMAGE_ID;
	return  img;
}

/*
 * Implementation for bl1_plat_handle_post_image_load(). This function
 * populates the default arguments to BL2. The BL2 memory layout structure
 * is allocated and the calculated layout is populated in arg1 to BL2.
 */
int bl1_plat_handle_post_image_load(unsigned int image_id)
{
	image_desc_t *image_desc;
	entry_point_info_t *ep_info;

	if (image_id != BL2_IMAGE_ID) {
		return 0;
	}

	/* Get the image descriptor */
	image_desc = bl1_plat_get_image_desc(BL2_IMAGE_ID);
	assert(image_desc != NULL);

	/* Get the entry point info */
	ep_info = &image_desc->ep_info;

	/*
	 * Create a new layout of memory for BL2 as seen by BL1 i.e.
	 * tell it the amount of total and free memory available.
	 */
	bl1_calc_bl2_mem_layout(&bl1_tzram_layout, &bl2_tzram_layout);
	ep_info->args.arg1 = (uintptr_t)&bl2_tzram_layout;

	/* Shared memory info in arg2 */
	shared_memory_desc.fw_config = &lan966x_fw_config;
	flush_dcache_range((uintptr_t) &shared_memory_desc, sizeof(shared_memory_desc));
	ep_info->args.arg2 = (uintptr_t) &shared_memory_desc;

	return 0;
}
