/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>
#include <drivers/console.h>
#include <drivers/microchip/vcore_gpio.h>
#include <drivers/microchip/flexcom_uart.h>
#include <plat/arm/common/arm_config.h>
#include <plat/arm/common/plat_arm.h>

#include "lan966x_private.h"

static console_t lan966x_console;

#define LAN996X_MAP_SHARED_RAM						\
	MAP_REGION_FLAT(						\
		LAN996X_SHARED_RAM_BASE,				\
		LAN996X_SHARED_RAM_SIZE,				\
		MT_MEMORY | MT_RW | MT_SECURE)

#define LAN996X_MAP_QSPI0						\
	MAP_REGION_FLAT(						\
		LAN996X_QSPI0_BASE,					\
		LAN996X_QSPI0_RANGE,					\
		MT_MEMORY | MT_RW | MT_SECURE)

#define LAN996X_MAP_APB							\
	MAP_REGION_FLAT(						\
		LAN996X_APB_BASE,					\
		LAN996X_APB_SIZE,					\
		MT_DEVICE | MT_RW | MT_SECURE)

#ifdef IMAGE_BL1
const mmap_region_t plat_arm_mmap[] = {
	LAN996X_MAP_SHARED_RAM,
	LAN996X_MAP_QSPI0,
	LAN996X_MAP_APB,
	{0}
};
#endif
#ifdef IMAGE_BL2
const mmap_region_t plat_arm_mmap[] = {
	LAN996X_MAP_SHARED_RAM,
	LAN996X_MAP_QSPI0,
	LAN996X_MAP_APB,
	{0}
};
#endif
#ifdef IMAGE_BL32
const mmap_region_t plat_arm_mmap[] = {
	LAN996X_MAP_SHARED_RAM,
	MAP_PERIPHBASE,
	MAP_A5_PERIPHERALS,
	{0}
};
#endif

/*******************************************************************************
 * Returns ARM platform specific memory map regions.
 ******************************************************************************/
const mmap_region_t *plat_arm_get_mmap(void)
{
	return plat_arm_mmap;
}

void lan966x_console_init(void)
{
	int console_scope = CONSOLE_FLAG_BOOT | CONSOLE_FLAG_RUNTIME;

	vcore_gpio_init(GCB_GPIO_ADDR);

	vcore_gpio_set_alt(25, 1);
	vcore_gpio_set_alt(26, 1);

	/* Initialize the console to provide early debug support */
	console_flexcom_register(&lan966x_console,
				 FLEXCOM0_BASE + FLEXCOM_UART_OFFSET,
				 FLEXCOM_DIVISOR(FACTORY_CLK, FLEXCOM_BAUDRATE));

	console_set_scope(&lan966x_console, console_scope);
}

unsigned int plat_get_syscnt_freq2(void)
{
        return SYS_COUNTER_FREQ_IN_TICKS;
}
