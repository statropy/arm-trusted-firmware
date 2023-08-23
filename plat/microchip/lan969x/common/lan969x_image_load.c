/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/bl_common.h>
#include <common/debug.h>
#include <common/desc_image_load.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>

#include "lan969x_regs.h"
#include "lan969x_private.h"

/*******************************************************************************
 * This function returns the list of loadable images.
 ******************************************************************************/
bl_load_info_t *plat_get_bl_image_load_info(void)
{
	return get_bl_load_info_from_mem_params_desc();
}

/*******************************************************************************
 * This function returns the list of executable images.
 ******************************************************************************/
bl_params_t *plat_get_next_bl_params(void)
{
	bl_mem_params_node_t *param_node;
	entry_point_info_t *ep_info;

	/* Get BL31 image node */
	param_node = get_bl_mem_params_node(BL31_IMAGE_ID);

	assert(param_node != NULL);
	ep_info = &param_node->ep_info;

	/* Setup arg1 beeing actual DDR size */
	ep_info->args.arg1 = lan966x_ddr_size();

	return get_next_bl_params_from_mem_params_desc();
}
