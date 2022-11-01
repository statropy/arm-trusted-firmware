/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <common/bl_common.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/microchip/otp.h>
#include <fw_config.h>
#include <lib/mmio.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <plat/arm/common/plat_arm.h>
#include <plat/common/platform.h>
#include <plat/microchip/common/lan966x_sjtag.h>
#include <common/desc_image_load.h>

#include "lan966x_private.h"
#include "lan966x_regs.h"
#include "lan966x_memmap.h"
#include "plat_otp.h"

/* Data structure which holds the extents of the trusted SRAM for BL2 */
static meminfo_t bl2_tzram_layout __aligned(CACHE_WRITEBACK_GRANULE);

#define MAP_SHARED_HEAP		MAP_REGION_FLAT(			\
					LAN966X_SRAM_BASE,		\
					LAN966X_SRAM_SIZE, 		\
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
		MAP_SHARED_HEAP,
		MAP_BL2_TOTAL,
		MAP_PKCL_CODE,
		MAP_PKCL_DATA,
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
	/* Must ensure timer is setup */
	lan966x_timer_init();

	/* Enable arch timer */
	generic_delay_timer_init();

	/* Limit trace level if needed */
	lan966x_set_max_trace_level();

	/* Console */
	lan966x_console_init();

	/* Check bootstrap mask: this may abort */
	lan966x_validate_strapping();

	/* Announce HW */
	INFO("Running on platform build: 0x%08x\n",
	     mmio_read_32(CPU_BUILDID(LAN966X_CPU_BASE)));
}

void bl2_early_platform_setup2(u_register_t arg0, u_register_t arg1, u_register_t arg2, u_register_t arg3)
{
	shared_memory_desc_t *desc = (shared_memory_desc_t *) arg2;

	/* Save memory layout */
	bl2_tzram_layout = *(struct meminfo *) arg1;

	memcpy(&lan966x_fw_config, desc->fw_config, sizeof(lan966x_fw_config));

	/* Forward mbedTLS heap */
	lan966x_mbed_heap_set(desc);

	/* Common setup */
	bl2_early_platform_setup();
}

void bl2_el3_early_platform_setup(u_register_t arg1, u_register_t arg2,
				  u_register_t arg3, u_register_t arg4)
{
	bl2_tzram_layout.total_base = BL2_BASE;
	bl2_tzram_layout.total_size = BL2_SIZE;

	/* Common setup */
	bl2_early_platform_setup();
}

void bl2_el3_plat_arch_setup(void)
{
	bl2_plat_arch_setup();
}

void lan966x_uvov_configure(void)
{
	uint32_t v;

	/* 42:37 UVOV_GPIOB_TRIM => CFG[0] */
	v = otp_read_uvov_gpiob_trim();
	VERBOSE("UVOV: gpiob_trim = %d\n", v);
	mmio_clrsetbits_32(UVOV_UVOV_CFG0(LAN966X_UVOV_BASE, 0),
			   UVOV_UVOV_CFG0_TUNE_M_M,
			   UVOV_UVOV_CFG0_TUNE_M(v));

	/* 36:31 UVOV_BOOT_TRIM => CFG[1] */
	v = otp_read_uvov_boot_trim();
	VERBOSE("UVOV: boot_trim = %d\n", v);
	mmio_clrsetbits_32(UVOV_UVOV_CFG0(LAN966X_UVOV_BASE, 1),
			   UVOV_UVOV_CFG0_TUNE_M_M,
			   UVOV_UVOV_CFG0_TUNE_M(v));

	/* 30:25 UVOV_RGMII_TRIM => CFG[4] */
	v = otp_read_uvov_rgmii_trim();
	VERBOSE("UVOV: rgmii_trim = %d\n", v);
	mmio_clrsetbits_32(UVOV_UVOV_CFG0(LAN966X_UVOV_BASE, 4),
			   UVOV_UVOV_CFG0_TUNE_M_M,
			   UVOV_UVOV_CFG0_TUNE_M(v));

	/* 24:19 UVOV_GPIOA_TRIM => CFG[5] */
	v = otp_read_uvov_gpioa_trim();
	VERBOSE("UVOV: gpioa_trim = %d\n", v);
	mmio_clrsetbits_32(UVOV_UVOV_CFG0(LAN966X_UVOV_BASE, 5),
			   UVOV_UVOV_CFG0_TUNE_M_M,
			   UVOV_UVOV_CFG0_TUNE_M(v));
}

void bl2_platform_setup(void)
{
	/* IO */
	lan966x_io_setup();

	/* OTP */
	otp_emu_init();

	/* SJTAG: Freeze mode and configuration */
	lan966x_sjtag_configure();

	/* UVOV OTP */
	lan966x_uvov_configure();

	/* Initialize DDR for loading BL32/BL33 */
	lan966x_ddr_init();

#if defined(LAN966X_TZ)   /* N/A on LAN966X-A0 due to chip error */
	/* Initialize the secure environment */
	lan966x_tz_init();
#endif
#if defined(LAN966X_AES_TESTS)
	lan966x_crypto_tests();
#endif

#if defined(LAN966X_EMMC_TESTS)
	lan966x_emmc_tests();
#endif
}

int bl2_plat_handle_post_image_load(unsigned int image_id)
{
	int err = 0;
	bl_mem_params_node_t *bl_mem_params = get_bl_mem_params_node(image_id);

	assert(bl_mem_params);

	switch (image_id) {
	case BL32_IMAGE_ID:
		//bl_mem_params->ep_info.args.arg0 = 0xdeadbeef;
		bl_mem_params->ep_info.args.arg1 = (uintptr_t) &lan966x_fw_config;
		break;
	default:
		/* Do nothing in default case */
		break;
	}

	return err;
}

/*******************************************************************************
 * This function flushes the data structures so that they are visible
 * in memory for the next BL image.
 ******************************************************************************/
void plat_flush_next_bl_params(void)
{
	uint32_t regions;

	/* Flush BL params, as this hook normally does */
	flush_bl_params_desc();

	/* Protect OTP section 4 - Keys */
	VERBOSE("Protect OTP, just before BL2 is done\n");
	regions = BIT(4);
	mmio_write_32(OTP_OTP_READ_PROTECT(LAN966X_OTP_BASE), regions);

	/* Zero out PKCL to ensure not leaking data */
	VERBOSE("Zero PKCL RAM, just before BL2 is done\n");
	memset((void*) LAN966X_PKCL_RAM_BASE, 0, LAN966X_PKCL_RAM_SIZE);

	/* Last TZPM settings */
	VERBOSE("Enable last NS devices\n");
	lan966x_tz_finish();
}
