/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <common/desc_image_load.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/microchip/tz_matrix.h>
#include <lib/mmio.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <plat/arm/common/plat_arm.h>
#include <plat/microchip/common/lan966x_gic.h>

#include "lan966x_private.h"

static entry_point_info_t bl33_ep_info;

#define MAP_SRAM_TOTAL   MAP_REGION_FLAT(				\
		LAN966X_SRAM_BASE,					\
		LAN966X_SRAM_SIZE,					\
		MT_MEMORY | MT_RW | MT_SECURE)

#define MAP_BL_SP_MIN_TOTAL	MAP_REGION_FLAT(			\
					BL32_BASE,			\
					BL32_END - BL32_BASE,		\
					MT_MEMORY | MT_RW | MT_SECURE)

#if SEPARATE_CODE_AND_RODATA
#define ARM_MAP_BL_RO			MAP_REGION_FLAT(			\
						BL_CODE_BASE,			\
						BL_CODE_END - BL_CODE_BASE,	\
						MT_CODE | MT_SECURE),		\
					MAP_REGION_FLAT(			\
						BL_RO_DATA_BASE,		\
						BL_RO_DATA_END			\
							- BL_RO_DATA_BASE,	\
						MT_RO_DATA | MT_SECURE)
#else
#define ARM_MAP_BL_RO			MAP_REGION_FLAT(			\
						BL_CODE_BASE,			\
						BL_CODE_END - BL_CODE_BASE,	\
						MT_CODE | MT_SECURE)
#endif
#if USE_COHERENT_MEM
#define ARM_MAP_BL_COHERENT_RAM		MAP_REGION_FLAT(			\
						BL_COHERENT_RAM_BASE,		\
						BL_COHERENT_RAM_END		\
							- BL_COHERENT_RAM_BASE, \
						MT_DEVICE | MT_RW | MT_SECURE)
#endif

/*******************************************************************************
 * Return a pointer to the 'entry_point_info' structure of the next image for
 * the security state specified. BL33 corresponds to the non-secure image type.
 * A NULL pointer is returned if the image does not exist.
 ******************************************************************************/
entry_point_info_t *sp_min_plat_get_bl33_ep_info(void)
{
	entry_point_info_t *next_image_info;

	next_image_info = &bl33_ep_info;

	if (next_image_info->pc == 0U) {
		return NULL;
	}

#if LAN966X_DIRECT_LINUX_BOOT
	/*
	 * According to the file ``Documentation/arm/Booting`` of the Linux
	 * kernel tree, Linux expects:
	 * r0 = 0
	 * r1 = machine type number, optional in DT-only platforms (~0 if so)
	 * r2 = Physical address of the device tree blob
	 */
	INFO("lan966x: Preparing to boot 32-bit Linux kernel\n");
	next_image_info->args.arg0 = 0U;
	next_image_info->args.arg1 = ~0U;
	next_image_info->args.arg2 = (u_register_t) LAN966X_LINUX_DTB_BASE;
#endif

	return next_image_info;
}

#pragma weak params_early_setup
void params_early_setup(u_register_t plat_param_from_bl2)
{
}

/*******************************************************************************
 * Perform any BL32 specific platform actions.
 ******************************************************************************/
void sp_min_early_platform_setup2(u_register_t arg0, u_register_t arg1,
				  u_register_t arg2, u_register_t arg3)
{
	params_early_setup(arg1);

	generic_delay_timer_init();

	/* Console */
	lan966x_console_init();

	bl31_params_parse_helper(arg0, NULL, &bl33_ep_info);
}

/*******************************************************************************
 * Perform any sp_min platform setup code
 ******************************************************************************/
void sp_min_platform_setup(void)
{
	/* Initialize the gic cpu and distributor interfaces */
	plat_lan966x_gic_driver_init();
	plat_lan966x_gic_init();
}

/*******************************************************************************
 * Perform the very early platform specific architectural setup here. At the
 * moment this is only intializes the mmu in a quick and dirty way.
 ******************************************************************************/
void sp_min_plat_arch_setup(void)
{
	const mmap_region_t bl_regions[] = {
		MAP_SRAM_TOTAL,
		MAP_BL_SP_MIN_TOTAL,
		ARM_MAP_BL_RO,
#if USE_COHERENT_MEM
		ARM_MAP_BL_COHERENT_RAM,
#endif
		{0}
	};

	setup_page_tables(bl_regions, plat_arm_get_mmap());

	enable_mmu_svc_mon(0);
}

void sp_min_plat_fiq_handler(uint32_t id)
{
	VERBOSE("[sp_min] interrupt #%d\n", id);
}

static void configure_sram(void)
{
	INFO("Enabling SRAM NS access\n");
	/* Reset SRAM content */
	memset((void *)LAN966X_SRAM_BASE, 0, LAN966X_SRAM_SIZE);
	flush_dcache_range((uintptr_t)LAN966X_SRAM_BASE, LAN966X_SRAM_SIZE);
	/* Enable SRAM for NS access */
	matrix_configure_slave_security(MATRIX_SLAVE_FLEXRAM0,
					MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_128K),
					MATRIX_SASPLIT(0, MATRIX_SRTOP_VALUE_128K),
					MATRIX_LANSECH_NS(0));
	/* - and DMAC SRAM access */
	matrix_configure_slave_security(MATRIX_SLAVE_FLEXRAM1,
					MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_128K),
					MATRIX_SASPLIT(0, MATRIX_SRTOP_VALUE_128K),
					MATRIX_LANSECH_NS(0));

	/* Enable QSPI0 for NS access */
	matrix_configure_slave_security(MATRIX_SLAVE_QSPI0,
					MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_16M) |
					MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_16M),
					MATRIX_SASPLIT(0, MATRIX_SRTOP_VALUE_16M),
					MATRIX_LANSECH_NS(0));
	/* Enable USB for NS access.
	 * Has two RAMs, MAP:128K + xHCI:512K
	 */
	matrix_configure_slave_security(MATRIX_SLAVE_USB,
					MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_512K) |
					MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_128K),
					MATRIX_SASPLIT(1, MATRIX_SRTOP_VALUE_512K) |
					MATRIX_SASPLIT(0, MATRIX_SRTOP_VALUE_128K),
					MATRIX_RDNSECH(0, 1) | MATRIX_RDNSECH(1, 1) |
					MATRIX_WRNSECH(0, 1) | MATRIX_WRNSECH(1, 1));

}

void sp_min_plat_runtime_setup(void)
{
	/* Reset SRAM and allow NS access */
	configure_sram();
}
