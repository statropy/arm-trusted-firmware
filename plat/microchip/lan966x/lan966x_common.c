/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/console.h>
#include <drivers/microchip/flexcom_uart.h>
#include <drivers/microchip/vcore_gpio.h>
#include <lib/mmio.h>
#include <platform_def.h>

#include <plat/arm/common/arm_config.h>
#include <plat/arm/common/plat_arm.h>

#include "lan966x_regs.h"
#include "lan966x_private.h"

static console_t lan966x_console;

static struct {
	bool    read;
	uint8_t value;
} strap_data;

#define LAN996X_MAP_QSPI0						\
	MAP_REGION_FLAT(						\
		LAN996X_QSPI0_MMAP,					\
		LAN996X_QSPI0_RANGE,					\
		MT_MEMORY | MT_RO | MT_SECURE)

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
	uintptr_t base = 0;

	vcore_gpio_init(GCB_GPIO_OUT_SET(LAN966X_GCB_BASE));

#if defined(IMAGE_BL1)
	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_SAMBA_FC0:
		base = LAN966X_FLEXCOM_0_BASE;
		break;
	case LAN966X_STRAP_SAMBA_FC2:
		base = LAN966X_FLEXCOM_2_BASE;
		break;
	case LAN966X_STRAP_SAMBA_FC3:
		base = LAN966X_FLEXCOM_3_BASE;
		break;
	case LAN966X_STRAP_SAMBA_FC4:
		base = LAN966X_FLEXCOM_4_BASE;
		break;
	default:
		break;
	}
#else
	/* Except for BL1 bootrom */
	base = CONSOLE_BASE;
#endif

	switch (base) {
	case LAN966X_FLEXCOM_0_BASE:
		vcore_gpio_set_alt(25, 1);
		vcore_gpio_set_alt(26, 1);
		break;
	case LAN966X_FLEXCOM_1_BASE:
		vcore_gpio_set_alt(47, 1);
		vcore_gpio_set_alt(48, 1);
		break;
	case LAN966X_FLEXCOM_2_BASE:
		vcore_gpio_set_alt(44, 1);
		vcore_gpio_set_alt(45, 1);
		break;
	case LAN966X_FLEXCOM_3_BASE:
		vcore_gpio_set_alt(52, 1);
		vcore_gpio_set_alt(53, 1);
		break;
	case LAN966X_FLEXCOM_4_BASE:
		vcore_gpio_set_alt(57, 1);
		vcore_gpio_set_alt(58, 1);
		break;
	default:
		assert(base == 0);
		break;
	}

	if (base) {
		/* Initialize the console to provide early debug support */
		console_flexcom_register(&lan966x_console,
					 base + FLEXCOM_UART_OFFSET,
					 FLEXCOM_DIVISOR(FACTORY_CLK, FLEXCOM_BAUDRATE));
		console_set_scope(&lan966x_console,
				  CONSOLE_FLAG_BOOT | CONSOLE_FLAG_RUNTIME);
	}
}

void lan966x_init_timer(void)
{
	uintptr_t syscnt = LAN966X_CPU_SYSCNT_BASE;

	mmio_write_32(CPU_SYSCNT_CNTCVL(syscnt), 0); /* Low */
	mmio_write_32(CPU_SYSCNT_CNTCVU(syscnt), 0); /* High */
	mmio_write_32(CPU_SYSCNT_CNTCR(syscnt),
		      CPU_SYSCNT_CNTCR_CNTCR_EN(1)); /*Enable */
}

unsigned int plat_get_syscnt_freq2(void)
{
        return SYS_COUNTER_FREQ_IN_TICKS;
}

uint8_t lan966x_get_strapping(void)
{
	uint32_t status;
	uint8_t strapping;

	if (strap_data.read) {
		INFO("VCORE_CFG = %d (cached)\n", strap_data.value);
		return strap_data.value;
	}

	status = mmio_read_32(CPU_GENERAL_STAT(LAN966X_CPU_BASE));
	strapping = CPU_GENERAL_STAT_VCORE_CFG_X(status);

#if defined(DEBUG)
	/*
	 * NOTE: This allows overriding the strapping switches through
	 * the GPR registers.
	 *
	 * In the DEBUG build, GPR(0) can be used to override the
	 * actual strapping. If any of the non-cfg (lower 4) bits are
	 * set, the the low 4 bits will override the actual
	 * strapping.
	 *
	 * You can set the GPR0 in the DSTREAM init like
	 * this:
	 *
	 * > memory set_typed S:0xE00C0000 (unsigned int) (0x10000a)
	 *
	 * This would override the strapping with the value: 0xa
	 */
	status = mmio_read_32(CPU_GPR(LAN966X_CPU_BASE, 0));
	if (status & ~CPU_GENERAL_STAT_VCORE_CFG_M) {
		NOTICE("OVERRIDE CPU_GENERAL_STAT = 0x%08x\n", status);
		strapping = CPU_GENERAL_STAT_VCORE_CFG_X(status);
	}
#endif

	INFO("VCORE_CFG = %d\n", strapping);

	/* Cache value */
	strap_data.read = true;
	strap_data.value = strapping;

	return strapping;
}

void lan966x_set_strapping(uint8_t value)
{
	strap_data.read = true;
	strap_data.value = value;
}
