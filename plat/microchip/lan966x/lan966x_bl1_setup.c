/*
 * Copyright (c) 2015-2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <arch.h>
#include <bl1/bl1.h>
#include <common/bl_common.h>
#include <lib/fconf/fconf.h>
#include <lib/utils.h>
#include <plat/common/platform.h>

#include "lan966x_private.h"
#include "lan966x_regs_common.h"
#include "lan966x_baremetal_cpu_regs.h"
#include "mchp,lan966x_icpu.h"
#include "usart.h"

#define MAP_BL1_TOTAL   MAP_REGION_FLAT(                \
					    bl1_tzram_layout.total_base,	\
					    bl1_tzram_layout.total_size,	\
					    MT_MEMORY | MT_RW | MT_SECURE)
/*
 * If SEPARATE_CODE_AND_RODATA=1 we define a region for each section
 * otherwise one region is defined containing both
 */
#if SEPARATE_CODE_AND_RODATA
#define MAP_BL1_RO		MAP_REGION_FLAT(	\
					BL_CODE_BASE,			\
					BL1_CODE_END - BL_CODE_BASE,	\
					MT_CODE | MT_SECURE),		    \
				MAP_REGION_FLAT(			\
					BL1_RO_DATA_BASE,		\
					BL1_RO_DATA_END			\
						- BL_RO_DATA_BASE,	\
					MT_RO_DATA | MT_SECURE)
#else
#define MAP_BL1_RO	MAP_REGION_FLAT(		\
					BL_CODE_BASE,			\
					BL1_CODE_END - BL_CODE_BASE,	\
					MT_CODE | MT_SECURE)
#endif

/* Data structure which holds the extents of the trusted SRAM for BL1*/
static meminfo_t bl1_tzram_layout;

/* Boolean variable to hold condition whether firmware update needed or not */
static bool is_fwu_needed;


/* will hold the maserati register structure */
uint32_t maserati_regs[NUM_TARGETS];
#define update_regs(T) maserati_regs[TARGET_ ## T] = T ## _ADDR


struct meminfo *bl1_plat_sec_mem_layout(void)
{
	return &bl1_tzram_layout;
}


void bl1_early_platform_setup(void) {

    /* Initialise  maserati/sunrise specific UART interface */
    maserati_regs[TARGET_FLEXCOM] = FLEXCOM_0_ADDR;
    usart_init( BAUDRATE(FACTORY_CLK, UART_BAUDRATE) );

    /* Allow BL1 to see the whole Trusted RAM */
    bl1_tzram_layout.total_base = BL1_RW_BASE;
    bl1_tzram_layout.total_size = BL1_RW_SIZE + BL2_SIZE; /* XXX - revisit */
}


void bl1_plat_arch_setup(void)
{
}

void bl1_platform_setup(void)
{
	/* redirect test output to specific usart function */
	usart_puts(">>>>>> Running Arm Trusted Firmware BL1 stage on LAN966x <<<<<< \n");
	usart_puts("Enter main() loop \n");

	lan966x_io_setup();
}

void bl1_plat_prepare_exit(entry_point_info_t *ep_info)
{
}


/*
 * On Arm platforms, the FWU process is triggered when the FIP image has been tampered with.
 */
bool plat_arm_bl1_fwu_needed(void)
{
	return false;
}


/*******************************************************************************
 * The following function checks if Firmware update is needed,
 * by checking if TOC in FIP image is valid or not.
 ******************************************************************************/
unsigned int bl1_plat_get_next_image_id(void)
{
	return  is_fwu_needed ? NS_BL1U_IMAGE_ID : BL2_IMAGE_ID;
}
