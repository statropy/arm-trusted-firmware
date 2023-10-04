/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <lib/smccc.h>

#include <common/bl_common.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/microchip/tz_matrix.h>
#include <lib/mmio.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <libfit.h>
#include <plat/arm/common/plat_arm.h>
#include <plat/common/platform.h>
#include <plat/microchip/common/lan966x_gic.h>
#include <tf_gunzip.h>

#include "lan969x_private.h"
#include <otp_tags.h>

#define PAGE_ALIGN(x, a)	(((x) + (a) - 1) & ~((a) - 1))

static entry_point_info_t bl32_image_ep_info;
static entry_point_info_t bl33_image_ep_info;
static uintptr_t bl33_image_base;
static bl31_params_t bl31_params;

static char fit_config[128], *fit_config_ptr;

#define MAP_SRAM_TOTAL   MAP_REGION_FLAT(				\
		LAN969X_SRAM_BASE,					\
		LAN969X_SRAM_SIZE,					\
		MT_MEMORY | MT_RW | MT_SECURE)

#define MAP_BL31_TOTAL	MAP_REGION_FLAT(				\
					BL31_BASE,			\
					BL31_END - BL31_BASE,		\
					MT_MEMORY | MT_RW | MT_SECURE)

#define ARM_MAP_BL_RO			MAP_REGION_FLAT(			\
						BL_CODE_BASE,			\
						BL_CODE_END - BL_CODE_BASE,	\
						MT_CODE | MT_SECURE)

size_t microchip_plat_ns_ddr_size(void)
{
	return bl31_params.ddr_size;
}

u_register_t microchip_plat_board_number(void)
{
	return bl31_params.board_number;
}

u_register_t microchip_plat_boot_offset(void)
{
	return bl31_params.boot_offset;
}

/* FIT platform check of address */
bool fit_plat_is_ns_addr(uintptr_t addr)
{
	if (addr < PLAT_LAN969X_NS_IMAGE_BASE || addr >= PLAT_LAN969X_NS_IMAGE_LIMIT)
		return false;

	return true;
}

int fit_plat_default_address(const struct fit_context *fit, fit_prop_t prop, uintptr_t *addr)
{
	int ret = 0;

	switch (prop) {
	case FITIMG_PROP_KERNEL_TYPE:
		*addr = PLAT_LAN969X_NS_IMAGE_BASE;
		break;

	case FITIMG_PROP_DT_TYPE:
		*addr = PLAT_LAN969X_NS_IMAGE_LIMIT - SIZE_M(1);
		break;

	case FITIMG_PROP_RAMDISK_TYPE:
		*addr = PLAT_LAN969X_NS_IMAGE_BASE + SIZE_M(32);
		break;

	default:
		ret = -ENOENT;
		break;
	}

	if (ret)
		ERROR("fit: Default address for prop %d is UNDEFINED\n", prop);
	else
		INFO("fit: Default address for prop %d is: %p\n", prop, (void*) *addr);

	return ret;
}

int fit_plat_uncompress(const struct fit_context *fit, uintptr_t dst,
			uintptr_t src, size_t src_len, size_t *output_len)
{
	uintptr_t work_buf, out_buf, out_start, out_limit;
	size_t work_len;
	int ret;

	/* Set up decompress params */
	work_buf = (uintptr_t) fit->fit + fit_size(fit);
	work_buf = PAGE_ALIGN(work_buf, SIZE_M(1));
	work_len = SIZE_M(16);
	out_start = out_buf = work_buf + work_len;
	ret = fit_plat_default_address(fit, FITIMG_PROP_DT_TYPE, &out_limit);
	if (ret)
		return ret;
	INFO("Unzip(src %08lx, src_len %zd, out %08lx, out_len %zd, work %08lx, work_len %zd)\n",
	     src, src_len, out_buf, out_limit - out_start, work_buf, work_len);
	ret = gunzip(&src, src_len, &out_buf, out_limit - out_start, work_buf, work_len);
	if (ret == 0) {
		size_t out_len = out_buf - out_start;
		/* Move into place */
		memcpy((void*) dst, (void*) out_start, out_len);
		INFO("Uncompressed data, length now %zd bytes\n", out_len);
		*output_len = out_len;
	}

	return ret;
}

void bl31_fit_unpack(void)
{
	const char *bootargs = "console=ttyAT0,115200 root=/dev/mmcblk0p5 rw rootwait loglevel=8";
	struct fit_context fit;

	if (fit_init_context(&fit, bl33_image_base) == EXIT_SUCCESS) {
		INFO("Unpacking FIT image @ %p\n", fit.fit);
		if (fit_select(&fit, fit_config_ptr) == EXIT_SUCCESS &&
		    fit_load(&fit, FITIMG_PROP_DT_TYPE) == EXIT_SUCCESS &&
		    fit_load(&fit, FITIMG_PROP_KERNEL_TYPE) == EXIT_SUCCESS) {
			/* Fixup DT, but allow to fail */
			fit_fdt_update(&fit, PLAT_LAN969X_NS_IMAGE_BASE,
				       bl31_params.ddr_size,
				       bootargs);
			NOTICE("Preparing to boot 64-bit Linux kernel\n");
			/*
			 * According to the file ``Documentation/arm64/booting.txt`` of the
			 * Linux kernel tree, Linux expects the physical address of the device
			 * tree blob (DTB) in x0, while x1-x3 are reserved for future use and
			 * must be 0.
			 */
			bl33_image_ep_info.args.arg0 = (u_register_t) fit.dtb;
			bl33_image_ep_info.args.arg1 = 0ULL;
			bl33_image_ep_info.args.arg2 = 0ULL;
			bl33_image_ep_info.args.arg3 = 0ULL;
		} else {
			ERROR("Unpacking FIT image for Linux failed\n");
		}
	} else {
		NOTICE("Direct boot of BL33 binary image\n");
	}
}

void bl31_early_platform_setup2(u_register_t arg0, u_register_t arg1,
				u_register_t arg2, u_register_t arg3)
{
	void *from_bl2 = (void *) arg0;
	bl31_params_t *params_bl2 = (bl31_params_t *) arg1;

	/* Save bl31 params */
	if (params_bl2 &&
	    params_bl2->magic == BL31_MAGIC_TAG &&
	    params_bl2->size == sizeof(bl31_params_t))
		bl31_params = *params_bl2;
	else {
		bl31_params.ddr_size = PLAT_LAN969X_NS_IMAGE_SIZE;
	}

	/* Enable arch timer */
	generic_delay_timer_init();

	/* Set logging level */
	lan969x_set_max_trace_level();

	/* Console */
	lan969x_console_init();

	/* Read this up front to cache */
	if (otp_tag_get_string(otp_tag_type_fit_config, fit_config, sizeof(fit_config)) > 0)
		fit_config_ptr = fit_config;
	else
		fit_config_ptr = "lan9698_ev23x71a_0_at_lan969x";

	/*
	 * Check params passed from BL2 should not be NULL,
	 */
	bl_params_t *params_from_bl2 = (bl_params_t *)from_bl2;

	assert(params_from_bl2 != NULL);
	assert(params_from_bl2->h.type == PARAM_BL_PARAMS);
	assert(params_from_bl2->h.version >= VERSION_2);

	bl_params_node_t *bl_params = params_from_bl2->head;

	/*
	 * Copy BL33 and BL32 (if present), entry point information.
	 * They are stored in Secure RAM, in BL2's address space.
	 */
	while (bl_params) {
		if (bl_params->image_id == BL32_IMAGE_ID)
			bl32_image_ep_info = *bl_params->ep_info;

		if (bl_params->image_id == BL33_IMAGE_ID) {
			bl33_image_ep_info = *bl_params->ep_info;
			bl33_image_base = bl_params->image_info->image_base;
		}

		bl_params = bl_params->next_params_info;
	}

	if (bl33_image_ep_info.pc == 0 || bl33_image_base == 0)
		panic();
}

void bl31_plat_arch_setup(void)
{
	const mmap_region_t bl_regions[] = {
		MAP_SRAM_TOTAL,
		MAP_BL31_TOTAL,
		ARM_MAP_BL_RO,
		{0}
	};

	setup_page_tables(bl_regions, plat_arm_get_mmap());

	enable_mmu_el3(0);
}

void bl31_platform_setup(void)
{
	/* Initialize the gic cpu and distributor interfaces */
	plat_lan966x_gic_driver_init();
	plat_lan966x_gic_init();

	/* See if FIT needs to be moved around */
	bl31_fit_unpack();
}

entry_point_info_t *bl31_plat_get_next_image_ep_info(uint32_t type)
{
	assert(sec_state_is_valid(type) != 0);

	if (type == NON_SECURE)
		return &bl33_image_ep_info;

	if ((type == SECURE) && bl32_image_ep_info.pc)
		return &bl32_image_ep_info;

	return NULL;
}

void bl31_plat_runtime_setup(void)
{
	uint32_t srtop, sasplit, ssr;

	NOTICE("Runtime initialization NOW\n");
	/* Wipe re-purposed SRAM */
	memset((void *)BL2_BASE, 0, BL2_SIZE);
	flush_dcache_range((uintptr_t)BL2_BASE, BL2_SIZE);
	memset((void *)BL1_RW_BASE, 0, BL1_RW_SIZE);
	flush_dcache_range((uintptr_t)BL1_RW_BASE, BL1_RW_SIZE);


	/* Enable SRAM for NS access */
	/* HSEL0: NS (1MB) */
	srtop = MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_1M);
	sasplit = MATRIX_SASPLIT(0, MATRIX_SRTOP_VALUE_1M);
	ssr = MATRIX_LANSECH_NS(0);
	/* HSEL1: S (128K BL31) */
	/* HSEL1: NS (896K) */
	srtop |= MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_1M);
	sasplit |= MATRIX_SASPLIT(1, MATRIX_SRTOP_VALUE_128K);
	/* ssr(1) is zero = S for LA(HSEL1) */
	matrix_configure_slave_security(MATRIX_SLAVE_FLEXRAM0,
					srtop, sasplit, ssr);
}
