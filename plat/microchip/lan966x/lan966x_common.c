/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <lib/mmio.h>

#include <drivers/console.h>
#include <drivers/microchip/flexcom_uart.h>
#include <drivers/microchip/vcore_gpio.h>

#include <plat/arm/common/arm_config.h>
#include <plat/arm/common/plat_arm.h>
#include <platform_def.h>

#include "lan966x_regs.h"
#include "lan966x_private.h"

static console_t lan966x_console;

#define LAN996X_MAP_QSPI0						\
	MAP_REGION_FLAT(						\
		LAN996X_QSPI0_MMAP,					\
		LAN996X_QSPI0_RANGE,					\
		MT_MEMORY | MT_RW | MT_SECURE)

#define LAN996X_MAP_AXI							\
	MAP_REGION_FLAT(						\
		LAN996X_AXI_BASE,					\
		LAN996X_AXI_SIZE,					\
		MT_DEVICE | MT_RW | MT_SECURE)

#define LAN996X_MAP_CPU							\
	MAP_REGION_FLAT(						\
		LAN996X_CPU_BASE,					\
		LAN996X_CPU_SIZE,					\
		MT_DEVICE | MT_RW | MT_SECURE)

#define LAN966X_MAP_DDR						\
	MAP_REGION_FLAT(					\
		LAN996X_DDR_BASE,				\
		LAN996X_DDR_SIZE,				\
		MT_MEMORY | MT_RW | MT_NS)

#ifdef IMAGE_BL1
const mmap_region_t plat_arm_mmap[] = {
	LAN996X_MAP_QSPI0,
	LAN996X_MAP_AXI,
	LAN996X_MAP_CPU,
	{0}
};
#endif
#ifdef IMAGE_BL2
const mmap_region_t plat_arm_mmap[] = {
	LAN996X_MAP_QSPI0,
	LAN996X_MAP_AXI,
	LAN996X_MAP_CPU,
	LAN966X_MAP_DDR,
	{0}
};
#endif
#ifdef IMAGE_BL32
const mmap_region_t plat_arm_mmap[] = {
	LAN996X_MAP_QSPI0,
	LAN996X_MAP_AXI,
	LAN996X_MAP_CPU,
	LAN966X_MAP_DDR,
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

	vcore_gpio_init(LAN966X_GCB_BASE + GCB_GPIO_OUT_SET);

	vcore_gpio_set_alt(25, 1);
	vcore_gpio_set_alt(26, 1);

	/* Initialize the console to provide early debug support */
	console_flexcom_register(&lan966x_console,
				 LAN966X_FLEXCOM_0_BASE + FLEXCOM_UART_OFFSET,
				 FLEXCOM_DIVISOR(FACTORY_CLK, FLEXCOM_BAUDRATE));

	console_set_scope(&lan966x_console, console_scope);
}

void lan966x_init_timer(void)
{
	uintptr_t syscnt = LAN966X_CPU_SYSCNT_BASE;

	mmio_write_32(syscnt + CPU_SYSCNT_CNTCVL, 0); /* Low */
	mmio_write_32(syscnt + CPU_SYSCNT_CNTCVU, 0); /* High */
	mmio_write_32(syscnt + CPU_SYSCNT_CNTCR,
		      CPU_SYSCNT_CNTCR_CNTCR_EN(1)); /*Enable */
}

unsigned int plat_get_syscnt_freq2(void)
{
        return SYS_COUNTER_FREQ_IN_TICKS;
}
