/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/console.h>
#include <drivers/microchip/flexcom_uart.h>
#include <drivers/microchip/lan966x_clock.h>
#include <drivers/microchip/qspi.h>
#include <drivers/microchip/tz_matrix.h>
#include <drivers/microchip/vcore_gpio.h>
#include <lib/mmio.h>
#include <platform_def.h>

#include <plat/arm/common/arm_config.h>
#include <plat/arm/common/plat_arm.h>

#include "lan966x_regs.h"
#include "lan966x_private.h"
#include "plat_otp.h"

/* Boot media config blob, little endian */
#define CONFIG_HEADER_SIGNATURE1	0x5048434d	// MCHP
#define CONFIG_HEADER_SIGNATURE2	0x58363639	// 966X
#define CONFIG_SPACE_SPI_OFFSET		0

static console_t lan966x_console;

static lan966x_boot_media_config_t lan966x_conf;
bool conf_read, conf_valid;

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

#define LAN996X_MAP_ORG							\
	MAP_REGION_FLAT(						\
		LAN996X_ORG_BASE,					\
		LAN996X_ORG_SIZE,					\
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
	LAN996X_MAP_ORG,
	LAN966X_MAP_DDR,
	{0}
};
#endif
#ifdef IMAGE_BL32
const mmap_region_t plat_arm_mmap[] = {
	LAN996X_MAP_QSPI0,
	LAN996X_MAP_AXI,
	LAN996X_MAP_CPU,
	LAN996X_MAP_ORG,
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

static bool lan966x_boot_media_cfg_parse(uint8_t *src, lan966x_boot_media_config_t *out)
{
	lan966x_boot_media_config_t *in = (lan966x_boot_media_config_t *)src;

	if (in->signature1 == CONFIG_HEADER_SIGNATURE1 &&
	    in->signature2 == CONFIG_HEADER_SIGNATURE2) {
		uint32_t crc = Crc32c(0, src, sizeof(lan966x_boot_media_config_t) - 4);
		if (crc == in->crc) {
			memcpy(out, in, sizeof(lan966x_boot_media_config_t));
			return true;
		}
	}

	return false;
}

static bool lan966x_boot_media_cfg(lan966x_boot_media_config_t *out)
{
	bool ret = false;

	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC: /* XXX */
	case LAN966X_STRAP_BOOT_EMMC_CONT: /* XXX */
	case LAN966X_STRAP_BOOT_SD:  /* XXX */
		INFO("Fix me, emulating reading configuration\n");
		memset(out, 0xff, sizeof(*out));
		out->signature1 = CONFIG_HEADER_SIGNATURE1;
		out->signature2 = CONFIG_HEADER_SIGNATURE2;
		out->console = 0;
		out->max_log_level = LOG_LEVEL_VERBOSE;
		ret = true;
		break;

	case LAN966X_STRAP_BOOT_QSPI_CONT:
	case LAN966X_STRAP_BOOT_QSPI:
		ret = lan966x_boot_media_cfg_parse((void*)LAN996X_QSPI0_MMAP +
						   CONFIG_SPACE_SPI_OFFSET, out);
		break;

	default:
		break;
	}

	return ret;
}

lan966x_boot_media_config_t *lan966x_boot_media_cfg_get(void)
{
	if (!conf_read) {
		conf_read = true;
		conf_valid = lan966x_boot_media_cfg(&lan966x_conf);
	}

	return conf_valid ? &lan966x_conf : NULL;
}

#if defined(LAN966X_ASIC)
static uintptr_t lan966x_get_conf_console(void)
{
	uintptr_t base = 0;
	lan966x_boot_media_config_t *cfg;

	cfg = lan966x_boot_media_cfg_get();
	if (cfg) {
		switch (cfg->console) {
		case 0:
			base = LAN966X_FLEXCOM_0_BASE;
			break;
		case 1:
			base = LAN966X_FLEXCOM_1_BASE;
			break;
		case 2:
			base = LAN966X_FLEXCOM_2_BASE;
			break;
		case 3:
			base = LAN966X_FLEXCOM_3_BASE;
			break;
		case 4:
			base = LAN966X_FLEXCOM_4_BASE;
			break;
		default:
			/* Unsupported value, ignore */
			break;
		}

		if ((cfg->max_log_level % 10U) == 0U &&
		    cfg->max_log_level <= LOG_LEVEL_VERBOSE)
			tf_log_set_max_level(cfg->max_log_level);

	}

	return base;
}
#endif

void lan966x_console_init(void)
{
	uintptr_t base;

	/* NB: Needed as OTP emulation need qspi */
	lan966x_io_init();

	vcore_gpio_init(GCB_GPIO_OUT_SET(LAN966X_GCB_BASE));

	/* See if boot media config defines a console */
#if defined(LAN966X_ASIC)
	base = lan966x_get_conf_console();
	if (!base)
		base = LAN966X_FLEXCOM_3_BASE;
#else
	base = LAN966X_FLEXCOM_0_BASE;
#endif

#if defined(IMAGE_BL1)
	/* Override if strappings say so */
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
	/* If no console, use compile-time value (for BL2/BL3x) */
	if (!base)
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

#if defined(LAN966X_ASIC)
	lan966x_clk_disable(LAN966X_CLK_FC3);
	lan966x_clk_set_rate(LAN966X_CLK_FC3, 30000000); /* 30MHz */
	lan966x_clk_enable(LAN966X_CLK_FC3);
#endif

	if (base) {
		/* Initialize the console to provide early debug support */
		console_flexcom_register(&lan966x_console,
					 base + FLEXCOM_UART_OFFSET,
					 FLEXCOM_DIVISOR(FACTORY_CLK, FLEXCOM_BAUDRATE));
		console_set_scope(&lan966x_console,
				  CONSOLE_FLAG_BOOT | CONSOLE_FLAG_RUNTIME);
	}

	lan966x_io_init();
}

void lan966x_io_init(void)
{
	/* We own SPI */
	mmio_setbits_32(CPU_GENERAL_CTRL(LAN966X_CPU_BASE),
			CPU_GENERAL_CTRL_IF_SI_OWNER_M);

	/* Enable memmap access */
	qspi_init(LAN966X_QSPI_0_BASE, QSPI_SIZE);

	/* Register TZ MATRIX driver */
	matrix_init(LAN966X_HMATRIX2_BASE);

	/* Ensure we have ample reach on QSPI mmap area */
	/* 16M should be more than adequate - EVB/SVB have 2M */
	matrix_configure_srtop(MATRIX_SLAVE_QSPI0,
			       MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_16M) |
			       MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_16M));

#if defined(LAN966X_TZ)
	/* Enable QSPI0 for NS access */
	matrix_configure_slave_security(MATRIX_SLAVE_QSPI0,
					MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_16M) |
					MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_16M),
					MATRIX_SASPLIT(0, MATRIX_SRTOP_VALUE_16M),
					MATRIX_LANSECH_NS(0));
#endif
}

void lan966x_timer_init(void)
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

	return strapping;
}

void lan966x_set_strapping(uint8_t value)
{
#if defined(DEBUG)
	mmio_write_32(CPU_GPR(LAN966X_CPU_BASE, 0), 0x10000 | value);
#endif
}

void qspi_plat_configure(void)
{
	lan966x_boot_media_config_t *cfg;

	cfg = lan966x_boot_media_cfg_get();
	if (cfg) {
		INFO("Speed up QSPI, dlybs = %d\n", cfg->qspi0_dlybs);
		qspi_set_dly(cfg->qspi0_dlybs);
	}
}

uint32_t lan966x_get_boot_source(void)
{
	boot_source_type boot_source;

	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC:
		boot_source = BOOT_SOURCE_EMMC;
		break;
	case LAN966X_STRAP_BOOT_QSPI :
		boot_source = BOOT_SOURCE_QSPI;
		break;
	case LAN966X_STRAP_BOOT_SD:
		boot_source = BOOT_SOURCE_SDMMC;
		break;
	default:
		boot_source = BOOT_SOURCE_NONE;
		break;
	}

	return boot_source;
}
