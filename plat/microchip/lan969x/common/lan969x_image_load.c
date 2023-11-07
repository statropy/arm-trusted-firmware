/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/bl_common.h>
#include <common/debug.h>
#include <common/desc_image_load.h>
#include <common/fdt_wrappers.h>
#include <lib/mmio.h>
#include <libfdt.h>
#include <plat/common/platform.h>

#include "lan969x_regs.h"
#include "lan969x_private.h"

static bl31_params_t bl31_params;

void *lan966x_get_dt(void);

static int plat_get_board(void *fdt)
{
	int offs;

	if(fdt == NULL || fdt_check_header(fdt) != 0) {
		return 0;
	}

	offs = fdt_path_offset(fdt, "/board");
	if (offs < 0) {
		NOTICE("No /board\n");
		return 0;
	}

	return fdt_read_uint32_default(fdt, offs, "board-number", 0);
}

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

	/* Setup arg1 beeing bl31 params struct */
	bl31_params.magic = BL31_MAGIC_TAG;
	bl31_params.size = sizeof(bl31_params_t);
	bl31_params.ddr_size = lan966x_ddr_size();
	bl31_params.board_number = plat_get_board(lan966x_get_dt());
	bl31_params.boot_offset = lan966x_get_boot_offset();
	bl31_params.bl2_version = PLAT_BL2_VERSION;
	ep_info->args.arg1 = (uintptr_t) &bl31_params;
	/* Passed between contexts */
	flush_dcache_range(ep_info->args.arg1, sizeof(bl31_params));

	return get_next_bl_params_from_mem_params_desc();
}
