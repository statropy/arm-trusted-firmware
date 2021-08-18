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
#include <drivers/io/io_storage.h>
#include <drivers/microchip/otp.h>
#include <lib/fconf/fconf.h>
#include <lib/utils.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <plat/arm/common/plat_arm.h>
#include <plat/common/platform.h>

#include "lan966x_private.h"

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
	/* Console */
	lan966x_console_init();

	/* Timer */
	lan966x_timer_init();

	/* Enable arch timer */
	generic_delay_timer_init();

	/* Setup MMC */
	lan966x_sdmmc_init();

	/* Allow BL1 to see the whole Trusted RAM */
	bl1_tzram_layout.total_base = LAN996X_SRAM_BASE;
	bl1_tzram_layout.total_size = LAN996X_SRAM_SIZE;
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

/*
 * Note: The FW_CONFIG is read *wo* authentication, as the OTP
 * emulation data may contain the actual (emulated) rotpk. This is
 * only called when a *hw* ROTPK has *not* been deployed.
 */
static int bl1_load_fw_config(unsigned int image_id)
{
	uintptr_t dev_handle, image_handle, image_spec = 0;
	size_t bytes_read;
	int result;

	result = plat_get_image_source(image_id, &dev_handle, &image_spec);
	if (result != 0) {
		WARN("Failed to obtain reference to image id=%u (%i)\n",
			image_id, result);
		return result;
	}

	result = io_open(dev_handle, image_spec, &image_handle);
	if (result != 0) {
		WARN("Failed to access image id=%u (%i)\n", image_id, result);
		return result;
	}

	result = io_read(image_handle, (uintptr_t)&lan966x_fw_config,
			 sizeof(lan966x_fw_config), &bytes_read);
	if (result != 0)
		WARN("Failed to read data (%i)\n", result);

	io_close(image_handle);

	/* This is fwd to BL2 */
	flush_dcache_range((uintptr_t)&lan966x_fw_config, sizeof(lan966x_fw_config));

	return result;
}

void bl1_platform_setup(void)
{
	INFO("BL1 platform setup start\n");

	/* IO */
	lan966x_io_setup();

	/* OTP */
	otp_init();

	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_SAMBA_FC0:
	case LAN966X_STRAP_SAMBA_FC2:
	case LAN966X_STRAP_SAMBA_FC3:
	case LAN966X_STRAP_SAMBA_FC4:
	case LAN966X_STRAP_SAMBA_USB:
		lan966x_bootstrap_monitor();
		break;
	case LAN966X_STRAP_BOOT_MMC:
	case LAN966X_STRAP_BOOT_SD:
		bl1_load_fw_config(FW_CONFIG_ID);
		break;
	}

	INFO("BL1 platform setup done\n");
}

void bl1_plat_prepare_exit(entry_point_info_t *ep_info)
{
}

/*******************************************************************************
 * The following function checks if Firmware update is needed,
 * by checking if TOC in FIP image is valid or not.
 ******************************************************************************/
unsigned int bl1_plat_get_next_image_id(void)
{
	return  is_fwu_needed ? NS_BL1U_IMAGE_ID : BL2_IMAGE_ID;
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
	ep_info->args.arg2 = (uintptr_t) &lan966x_fw_config;

	return 0;
}
