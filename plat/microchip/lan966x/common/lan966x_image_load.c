/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/bl_common.h>
#include <common/debug.h>
#include <common/desc_image_load.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>

#include "lan966x_regs.h"

/*******************************************************************************
 * This function flushes the data structures so that they are visible
 * in memory for the next BL image.
 * NB: This code is only part of BL2
 ******************************************************************************/
void plat_flush_next_bl_params(void)
{
	uint32_t regions;

	flush_bl_params_desc();
	VERBOSE("Protect OTP, just before BL2 is done\n");
	/* Protect OTP section 4 - Keys */
	regions = BIT(4);
	mmio_write_32(OTP_OTP_READ_PROTECT(LAN966X_OTP_BASE), regions);
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
	return get_next_bl_params_from_mem_params_desc();
}
