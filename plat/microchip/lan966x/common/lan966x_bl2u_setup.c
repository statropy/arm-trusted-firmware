/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <common/bl_common.h>
#include <drivers/generic_delay_timer.h>
#include <lib/mmio.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <plat/arm/common/plat_arm.h>
#include <plat/common/platform.h>

#include "lan966x_private.h"
#include "lan966x_regs.h"
#include "lan966x_memmap.h"
#include "lan966x_bl2u_bootstrap.h"

#define MAP_BL2U_TOTAL		MAP_REGION_FLAT(			\
					BL2U_BASE,			\
					BL2U_LIMIT - BL2U_BASE,		\
					MT_MEMORY | MT_RW | MT_SECURE)

#define ARM_MAP_BL_RO			MAP_REGION_FLAT(			\
						BL_CODE_BASE,			\
						BL_CODE_END - BL_CODE_BASE,	\
						MT_CODE | MT_SECURE)

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

void bl2u_platform_setup(void)
{
	/* IO */
	lan966x_io_setup();

	/* Initialize DDR */
	lan966x_ddr_init();

	/* Prepare fw_config from applicable boot source */
	if (lan966x_bootable_source()) {
		lan966x_load_fw_config(FW_CONFIG_ID);
		lan966x_fwconfig_apply();
	}

	/* Call BL2U UART monitor */
	lan966x_bl2u_bootstrap_monitor();

	/* Unprotect VCORE */
	mmio_clrbits_32(CPU_RESET_PROT_STAT(LAN966X_CPU_BASE),
			CPU_RESET_PROT_STAT_SYS_RST_PROT_VCORE(1));

	/* Issue GCB reset */
	mmio_write_32(GCB_SOFT_RST(LAN966X_GCB_BASE),
		      GCB_SOFT_RST_SOFT_SWC_RST(1));
}

void bl2u_early_platform_setup(struct meminfo *mem_layout, void *plat_info)
{
	/* Strapping */
	lan966x_init_strapping();

	/* Timer */
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

void bl2u_plat_arch_setup(void)
{

#if USE_COHERENT_MEM
	/* Ensure ARM platforms dont use coherent memory in BL2U */
	assert((BL_COHERENT_RAM_END - BL_COHERENT_RAM_BASE) == 0U);
#endif

	const mmap_region_t bl_regions[] = {
		MAP_BL2U_TOTAL,
		ARM_MAP_BL_RO,
		{0}
	};

	setup_page_tables(bl_regions, plat_arm_get_mmap());

#ifdef __aarch64__
	enable_mmu_el1(0);
#else
	enable_mmu_svc_mon(0);
#endif
}
