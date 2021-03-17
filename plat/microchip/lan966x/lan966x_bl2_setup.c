/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <drivers/generic_delay_timer.h>
#include <lib/mmio.h>
#include <common/bl_common.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <plat/arm/common/plat_arm.h>
#include <plat/common/platform.h>

#include "lan966x_private.h"

/* Data structure which holds the extents of the trusted SRAM for BL2 */
static meminfo_t bl2_tzram_layout __aligned(CACHE_WRITEBACK_GRANULE);

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

void bl2_early_platform_setup2(u_register_t arg0, u_register_t arg1, u_register_t arg2, u_register_t arg3)
{
	struct meminfo *mem_layout = (struct meminfo *)arg1;

	/* Console */
	lan966x_console_init();

	/* Enable arch timer */
	generic_delay_timer_init();

	bl2_tzram_layout = *mem_layout;
}

int bl2_aes_ddr_test_block(int block, uintptr_t addr, uint32_t *data, size_t len)
{
	int fail = 0;

	memcpy((void*)addr, data, len);
	flush_dcache_range(addr, len);
	inv_dcache_range(addr, len);
	if (memcmp((void*)addr, data, len)) {
		int i, cnt;
		uint32_t *dest = (void*)addr;

		for (i = cnt = 0; i < (len / 4); i++) {
			if (dest[i] != data[i]) {
				ERROR("Mismatch at %p: 0x%08x vs 0x%08x\n",
				      &dest[i], dest[i], data[i]);
				cnt++;
			}
		}
		ERROR("FAIL block %02d @ 0x%08lx: %d words failure\n", block, addr, cnt);
		fail++;
	}
	memset((void*)addr, lan966x_trng_read(), len);
	return fail;
}

void bl2_aes_ddr_test(uintptr_t ddr)
{
	static uint32_t membuf[256];
	int i, runs, failures;

	/* Fill test pattern */
	for (i = 0; i < ARRAY_SIZE(membuf); i++)
		membuf[i] = lan966x_trng_read();

	/* AESB test sweep */
	runs = (256 * 1024) / sizeof(membuf);
	NOTICE("AESB DDR Memory Test @ %p, start %d runs of %d bytes\n",
	       (void*)ddr, runs, sizeof(membuf));
	for (i = failures = 0; i < runs; i++)
		if (bl2_aes_ddr_test_block(i, ddr + i * sizeof(membuf),
					   membuf, sizeof(membuf)))
			failures++;
	NOTICE("AESB DDR Memory Test @ %p, completed %d runs, %d failures\n",
	       (void*)ddr, runs, failures);
}

void bl2_platform_setup(void)
{
	/* Placed crudely */
	lan966x_ddr_init();

	/* IO */
	lan966x_io_setup();

	/* IO */
	lan966x_trng_init();

	/* Initialize the secure environment */
	//lan966x_tz_init();

	/* Temporary */
	//bl2_aes_ddr_test(PLAT_LAN966X_NS_IMAGE_BASE);
	//bl2_aes_ddr_test(BL32_BASE);
}
