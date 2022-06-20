/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <common/desc_image_load.h>
#include <drivers/generic_delay_timer.h>
#include <drivers/microchip/tz_matrix.h>
#include <errno.h>
#include <lib/mmio.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <plat/arm/common/plat_arm.h>
#include <libfit.h>

#include <lan966x_private.h>
#include <plat_otp.h>
#include <otp_tags.h>

static image_info_t bl33_image_info;
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

static char fit_config[128], *fit_config_ptr;

/*******************************************************************************
 * Return a pointer to the 'entry_point_info' structure of the next image for
 * the security state specified. BL33 corresponds to the non-secure image type.
 * A NULL pointer is returned if the image does not exist.
 ******************************************************************************/
entry_point_info_t *sp_min_plat_get_bl33_ep_info(void)
{
	const char *bootargs = "console=ttyS0,115200 root=/dev/mmcblk0p4 rw rootwait loglevel=8";
	struct fit_context fit;
	entry_point_info_t *next_image_info = &bl33_ep_info;
	image_info_t *image = &bl33_image_info;

	if (next_image_info->pc == 0U)
		return NULL;

	if (fit_init_context(&fit, image->image_base) == EXIT_SUCCESS) {
		INFO("Unpacking FIT image @ %p\n", fit.fit);
		if (fit_select(&fit, fit_config_ptr) == EXIT_SUCCESS &&
		    fit_load(&fit, FITIMG_PROP_DT_TYPE) == EXIT_SUCCESS &&
		    fit_load(&fit, FITIMG_PROP_KERNEL_TYPE) == EXIT_SUCCESS) {
			/* Fixup DT, but allow to fail */
			fit_fdt_update(&fit, PLAT_LAN966X_NS_IMAGE_BASE,
				       PLAT_LAN966X_NS_IMAGE_SIZE,
				       bootargs);
			INFO("Preparing to boot 32-bit Linux kernel\n");
			/*
			 * According to the file ``Documentation/arm/Booting`` of the Linux
			 * kernel tree, Linux expects:
			 * r0 = 0
			 * r1 = machine type number, optional in DT-only platforms (~0 if so)
			 * r2 = Physical address of the device tree blob
			 */
			next_image_info->pc = fit.entry;
			next_image_info->args.arg0 = 0U;
			next_image_info->args.arg1 = ~0U;
			next_image_info->args.arg2 = fit.dtb;
		} else {
			NOTICE("Unpacking FIT image for Linux failed\n");
		}
	} else {
		INFO("Direct boot of BL33 binary image\n");
	}

	return next_image_info;
}

/*
 * Override to ensure we're only accessing cached info. This function
 * ensures we can hand off the OTP device itself to NS when
 * dispatching BL33.
 */
int otp_read_bytes(unsigned int offset, unsigned int nbytes, uint8_t *dst)
{
	int ret = -EIO;

	if (offset < OTP_TBBR_ROTPK_ADDR ||
	    offset > OTP_TBBR_TNVCT_ADDR) {
		//INFO("OTP invalid read offset %d, %d bytes\n", offset, nbytes);
		memset(dst, 0, nbytes);
	} else {
		offset -= OTP_TBBR_ROTPK_ADDR;
		memcpy(dst, lan966x_fw_config.otp_emu_data + offset, nbytes);
		ret = 0;
	}

	return ret;
}

static void otp_cache_data(unsigned int offset, unsigned int size, uint8_t *data)
{
	int i, emu_off = offset - OTP_TBBR_ROTPK_ADDR;

	/* Read *raw* OTP bytes */
	otp_read_bytes_raw(offset, size, data);
	/* OR the data info the emulation data buffer */
	for (i = 0; i < size; i++)
		lan966x_fw_config.otp_emu_data[emu_off	+ i] |= data[i];
}

/*
 * This function will cache the required OTP data in order to
 * implement the SiP PSCI calls. This calls for the SSK and BSSK (HUK)
 * key.
 */
static void otp_cache_init(void)
{
	lan966x_key32_t key;

	if (!otp_in_emulation()) {
		memset(lan966x_fw_config.otp_emu_data, 0, OTP_EMU_MAX_DATA);
	}

	/* Read out to cache these entities */
	otp_cache_data(OTP_TBBR_HUK_ADDR, sizeof(key), key.b);
	otp_cache_data(OTP_TBBR_SSK_ADDR, sizeof(key), key.b);

	/* Read this up front to cache */
	if (otp_tag_get_string(otp_tag_type_fit_config, fit_config, sizeof(fit_config)) > 0)
		fit_config_ptr = fit_config;
	else
		fit_config_ptr = NULL;
}

#pragma weak params_early_setup
void params_early_setup(u_register_t plat_param_from_bl2)
{
	void *src_config = (void *) plat_param_from_bl2;

	/* Get bl2 fw_config (OTP EMU) */
	memcpy(&lan966x_fw_config, src_config, sizeof(lan966x_fw_config));
}

static void lan966x_params_parse_helper(u_register_t param,
					image_info_t *bl33_image_info_out,
					entry_point_info_t *bl33_ep_info_out)
{
	bl_params_node_t *node;
	bl_params_t *v2 = (void *)(uintptr_t)param;

	assert(v2->h.version == PARAM_VERSION_2);
	assert(v2->h.type == PARAM_BL_PARAMS);
	for (node = v2->head; node != NULL; node = node->next_params_info) {
		if (node->image_id == BL33_IMAGE_ID) {
			if (bl33_image_info_out != NULL)
				*bl33_image_info_out = *node->image_info;
			if (bl33_ep_info_out != NULL)
				*bl33_ep_info_out = *node->ep_info;
		}
	}
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

	/* Get BL33 info for Linux booting */
	lan966x_params_parse_helper(arg0, &bl33_image_info, &bl33_ep_info);
}

/*******************************************************************************
 * Perform any sp_min platform setup code
 ******************************************************************************/
void sp_min_platform_setup(void)
{
	/* otp emu init */
	otp_emu_init();

	/* BL32 cached otp interface */
	otp_cache_init();

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
