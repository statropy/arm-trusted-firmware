/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <common/interrupt_props.h>
#include <drivers/arm/gicv2.h>
#include <drivers/arm/gic_common.h>
#include <lib/utils.h>

#include <platform_def.h>

#include "lan966x_private.h"

/*
 * Define a list of Group 0 interrupts.
 */
#define PLAT_LAN966X_GICV2_G0_IRQS					\
	INTR_PROP_DESC(LAN966X_IRQ_SEC_PHY_TIMER, GIC_HIGHEST_SEC_PRIORITY, \
		       GICV2_INTR_GROUP0, GIC_INTR_CFG_LEVEL),		\
	INTR_PROP_DESC(LAN966X_IRQ_SEC_SGI_6, GIC_HIGHEST_SEC_PRIORITY,	\
		       GICV2_INTR_GROUP0, GIC_INTR_CFG_LEVEL)

/******************************************************************************
 * List of interrupts.
 *****************************************************************************/
static const interrupt_prop_t g0_interrupt_props[] = {
	PLAT_LAN966X_GICV2_G0_IRQS
};

static const gicv2_driver_data_t lan966x_gic_data = {
	.gicd_base = PLAT_LAN966X_GICD_BASE,
	.gicc_base = PLAT_LAN966X_GICC_BASE,
	.interrupt_props = g0_interrupt_props,
	.interrupt_props_num = ARRAY_SIZE(g0_interrupt_props),
};

void plat_lan966x_gic_driver_init(void)
{
	gicv2_driver_init(&lan966x_gic_data);
}

void plat_lan966x_gic_init(void)
{
	gicv2_distif_init();
	gicv2_pcpu_distif_init();
	gicv2_cpuif_enable();
}
