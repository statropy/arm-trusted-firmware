/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>

#include <common/bl_common.h>
#include <common/desc_image_load.h>
#include <plat/common/platform.h>

/*******************************************************************************
 * Following descriptor provides BL image/ep information that gets used
 * by BL2 to load the images and also subset of this information is
 * passed to next BL image. The image loading sequence is managed by
 * populating the images in required loading order. The image execution
 * sequence is managed by populating the `next_handoff_image_id` with
 * the next executable image id.
 ******************************************************************************/
static bl_mem_params_node_t bl2_mem_params_descs[] = {
	/* Fill BL31 related information */
	{
		.image_id = BL31_IMAGE_ID,

		SET_STATIC_PARAM_HEAD(ep_info, PARAM_EP,
				      VERSION_2, entry_point_info_t,
				      SECURE | EXECUTABLE | EP_FIRST_EXE),
		.ep_info.pc = BL31_BASE,
		.ep_info.spsr = SPSR_64(MODE_EL3, MODE_SP_ELX,
					DISABLE_ALL_EXCEPTIONS),
		SET_STATIC_PARAM_HEAD(image_info, PARAM_EP,
				      VERSION_2, image_info_t, IMAGE_ATTRIB_PLAT_SETUP),
		.image_info.image_base = BL31_BASE,
		.image_info.image_max_size = BL31_LIMIT - BL31_BASE,
# if defined(BL32_BASE)
		.next_handoff_image_id = BL32_IMAGE_ID,
# else
		.next_handoff_image_id = BL33_IMAGE_ID,
# endif
	},
	/* Fill BL33 related information */
	{
		.image_id = BL33_IMAGE_ID,

		SET_STATIC_PARAM_HEAD(ep_info, PARAM_EP,
				      VERSION_2, entry_point_info_t, NON_SECURE | EXECUTABLE),
#ifdef PRELOADED_BL33_BASE
		.ep_info.pc = PRELOADED_BL33_BASE,

		SET_STATIC_PARAM_HEAD(image_info, PARAM_EP,
				      VERSION_2, image_info_t, IMAGE_ATTRIB_SKIP_LOADING),
#else
		.ep_info.pc = PLAT_LAN969X_NS_IMAGE_BASE,

		SET_STATIC_PARAM_HEAD(image_info, PARAM_EP,
				      VERSION_2, image_info_t, 0),
		.image_info.image_base = PLAT_LAN969X_NS_IMAGE_BASE,
		.image_info.image_max_size = PLAT_LAN969X_NS_IMAGE_SIZE,
#endif /* PRELOADED_BL33_BASE */
                .ep_info.spsr = SPSR_64(MODE_EL1, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS),
		.next_handoff_image_id = INVALID_IMAGE_ID,
	},
#if LAN969X_DIRECT_LINUX_BOOT
	/* Fill BL33 NT_FW_CONFIG DT info */
	{
		.image_id = NT_FW_CONFIG_ID,

		SET_STATIC_PARAM_HEAD(ep_info, PARAM_IMAGE_BINARY, VERSION_2,
				      entry_point_info_t, NON_SECURE | NON_EXECUTABLE),
		SET_STATIC_PARAM_HEAD(image_info, PARAM_IMAGE_BINARY, VERSION_2,
				      image_info_t, 0),

		.image_info.image_base = LAN969X_LINUX_DTB_BASE,
		.image_info.image_max_size = SIZE_M(4),

		.next_handoff_image_id = INVALID_IMAGE_ID,
	},
#endif
};

REGISTER_BL_IMAGE_DESCS(bl2_mem_params_descs)
