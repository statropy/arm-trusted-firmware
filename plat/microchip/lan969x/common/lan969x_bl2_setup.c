/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <common/bl_common.h>
#include <common/desc_image_load.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/microchip/lan969x_pcie_ep.h>
#include <drivers/microchip/otp.h>
#include <lib/mmio.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <plat/arm/common/plat_arm.h>
#include <plat/common/platform.h>
#include <plat/microchip/common/fw_config.h>
#include <plat/microchip/common/lan966x_sjtag.h>

#include "lan969x_private.h"
#include "lan969x_regs.h"
#include "lan969x_memmap.h"
#include "plat_otp.h"

/* Data structure which holds the extents of the trusted SRAM for BL2 */
static meminfo_t bl2_tzram_layout __aligned(CACHE_WRITEBACK_GRANULE);

#define MAP_SHARED_HEAP		MAP_REGION_FLAT(			\
					LAN969X_SRAM_BASE,		\
					LAN969X_SRAM_SIZE, 		\
					MT_MEMORY | MT_RW | MT_SECURE)

#define MAP_BL2_TOTAL		MAP_REGION_FLAT(			\
					bl2_tzram_layout.total_base,	\
					bl2_tzram_layout.total_size,	\
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
 * Perform the very early platform specific architectural setup here. At the
 * moment this is only initializes the mmu in a quick and dirty way.
 ******************************************************************************/
void bl2_plat_arch_setup(void)
{
	const mmap_region_t bl_regions[] = {
		MAP_BL2_TOTAL,
		MAP_SILEX_REGS,
		MAP_SILEX_RAM,
		ARM_MAP_BL_RO,
#if USE_COHERENT_MEM
		ARM_MAP_BL_COHERENT_RAM,
#endif
		{0}
	};

	setup_page_tables(bl_regions, plat_arm_get_mmap());

#ifdef __aarch64__
	enable_mmu_el1(0);
#else
	enable_mmu_svc_mon(0);
#endif
}

static void bl2_early_platform_setup(void)
{
	/* Enable arch timer */
	generic_delay_timer_init();

	/* Set logging level */
	lan969x_set_max_trace_level();

	/* Console */
	lan969x_console_init();

	/* Announce HW */
	INFO("Running on platform build: 0x%08x\n",
	     mmio_read_32(CPU_BUILDID(LAN969X_CPU_BASE)));
}

void bl2_early_platform_setup2(u_register_t arg0, u_register_t arg1, u_register_t arg2, u_register_t arg3)
{
	/* Save memory layout */
	bl2_tzram_layout = *(struct meminfo *) arg1;

	/* Shared data */
	memcpy(&lan966x_fw_config, (const void *) arg2, sizeof(lan966x_fw_config));

	/* Common setup */
	bl2_early_platform_setup();
}

void lan969x_uvov_configure(void)
{
	uint32_t v;

	/* 24:19 UVOV_MAG_TRIM => CFG (using legacy GPIOA field) */
	v = otp_read_uvov_gpioa_trim();
	VERBOSE("UVOV: mag_trim = %d\n", v);
	mmio_clrsetbits_32(UVOV_TUNE(LAN969X_UVOV_BASE),
			   UVOV_TUNE_TUNE_M_M,
			   UVOV_TUNE_TUNE_M(v));
}

void bl2_platform_setup(void)
{
	/* IO */
	lan966x_io_setup();

	/* SJTAG: Freeze mode and configuration */
	lan966x_sjtag_configure();

	/* UVOV OTP */
	lan969x_uvov_configure();

	/* Init tzpm */
	lan969x_tz_init();

#if defined(LAN969X_PCIE)
	/* Init PCIe Endpoint - never returns */
	lan969x_pcie_ep_init(lan966x_get_dt());
#endif

#if !defined(LAN969X_LMSTAX)
	/* Init DDR */
	lan966x_ddr_init(lan966x_get_dt());

	/* Init PCIe Endpoint */
	lan969x_pcie_ep_init(lan966x_get_dt());
#endif
}

/*******************************************************************************
 * This function flushes the data structures so that they are visible
 * in memory for the next BL image.
 ******************************************************************************/
void plat_flush_next_bl_params(void)
{
	flush_bl_params_desc();

	/* Last TZPM settings */
	VERBOSE("Enable last NS devices\n");
	lan969x_tz_finish();
}
