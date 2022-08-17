/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/console.h>
#include <drivers/microchip/emmc.h>
#include <drivers/microchip/flexcom_uart.h>
#include <drivers/microchip/lan966x_clock.h>
#include <drivers/microchip/qspi.h>
#include <drivers/microchip/tz_matrix.h>
#include <drivers/microchip/usb.h>
#include <drivers/microchip/vcore_gpio.h>
#include <fw_config.h>
#include <lib/mmio.h>
#include <platform_def.h>

#include <plat/common/platform.h>
#include <plat/arm/common/arm_config.h>
#include <plat/arm/common/plat_arm.h>
#include <lan96xx_mmc.h>
#include <lan96xx_common.h>

#include "lan966x_regs.h"
#include "lan966x_private.h"

CASSERT((BL1_RW_SIZE + BL2_SIZE) <= LAN966X_SRAM_SIZE, assert_sram_depletion);

static console_t lan966x_console;
shared_memory_desc_t shared_memory_desc;

/* Define global fw_config, set default MMC settings */
lan966x_fw_config_t lan966x_fw_config = {
	FW_CONFIG_INIT_32(LAN966X_FW_CONF_MMC_CLK_RATE, MMC_DEFAULT_SPEED),
	FW_CONFIG_INIT_8(LAN966X_FW_CONF_MMC_BUS_WIDTH, MMC_BUS_WIDTH_1),
	FW_CONFIG_INIT_8(LAN966X_FW_CONF_QSPI_CLK, QSPI_DEFAULT_SPEED_MHZ),
};

#define LAN966X_MAP_QSPI0						\
	MAP_REGION_FLAT(						\
		LAN966X_QSPI0_MMAP,					\
		LAN966X_QSPI0_RANGE,					\
		MT_MEMORY | MT_RO | MT_SECURE)

#define LAN966X_MAP_QSPI0_RW						\
	MAP_REGION_FLAT(						\
		LAN966X_QSPI0_MMAP,					\
		LAN966X_QSPI0_RANGE,					\
		MT_DEVICE | MT_RW | MT_SECURE)

#define LAN966X_MAP_AXI							\
	MAP_REGION_FLAT(						\
		LAN966X_DEV_BASE,					\
		LAN966X_DEV_SIZE,					\
		MT_DEVICE | MT_RW | MT_SECURE)

#define LAN966X_MAP_BL32					\
	MAP_REGION_FLAT(					\
		BL32_BASE,					\
		BL32_SIZE,					\
		MT_MEMORY | MT_RW | MT_SECURE)

#define LAN966X_MAP_NS_MEM					\
	MAP_REGION_FLAT(					\
		PLAT_LAN966X_NS_IMAGE_BASE,			\
		PLAT_LAN966X_NS_IMAGE_SIZE,			\
		MT_MEMORY | MT_RW | MT_NS)

#ifdef IMAGE_BL1
const mmap_region_t plat_arm_mmap[] = {
	LAN966X_MAP_QSPI0,
	LAN966X_MAP_AXI,
	{0}
};
#endif
#if defined(IMAGE_BL2)
const mmap_region_t plat_arm_mmap[] = {
	LAN966X_MAP_QSPI0,
	LAN966X_MAP_AXI,
	LAN966X_MAP_BL32,
	LAN966X_MAP_NS_MEM,
	{0}
};
#endif
#if defined(IMAGE_BL2U)
const mmap_region_t plat_arm_mmap[] = {
	LAN966X_MAP_QSPI0_RW,
	LAN966X_MAP_AXI,
	LAN966X_MAP_BL32,
	LAN966X_MAP_NS_MEM,
	{0}
};
#endif
#ifdef IMAGE_BL32
const mmap_region_t plat_arm_mmap[] = {
	LAN966X_MAP_QSPI0,
	LAN966X_MAP_AXI,
	LAN966X_MAP_BL32,
	LAN966X_MAP_NS_MEM,
	{0}
};
#endif

enum lan966x_flexcom_id {
	FLEXCOM0 = 0,
	FLEXCOM1,
	FLEXCOM2,
	FLEXCOM3,
	FLEXCOM4,
};

static struct lan966x_flexcom_args {
	uintptr_t base;
	int rx_gpio;
	int tx_gpio;
} lan966x_flexcom_map[] = {
	[FLEXCOM0] = {
		LAN966X_FLEXCOM_0_BASE, 25, 26
	},
	[FLEXCOM1] = { 0 },
	[FLEXCOM2] = {
		LAN966X_FLEXCOM_2_BASE, 44, 45
	},
	[FLEXCOM3] = {
		LAN966X_FLEXCOM_3_BASE, 52, 53
	},
	[FLEXCOM4] = {
		LAN966X_FLEXCOM_4_BASE, 57, 58
	},
};

/*******************************************************************************
 * Returns ARM platform specific memory map regions.
 ******************************************************************************/
const mmap_region_t *plat_arm_get_mmap(void)
{
	return plat_arm_mmap;
}

static void lan966x_flexcom_init(int idx)
{
	struct lan966x_flexcom_args *fc;

	if (idx < 0)
		return;

	fc = &lan966x_flexcom_map[idx];

	if (fc->base == 0)
		return;

	/* GPIOs for RX and TX */
	vcore_gpio_set_alt(fc->rx_gpio, 1);
	vcore_gpio_set_alt(fc->tx_gpio, 1);

	/* Initialize the console to provide early debug support */
	console_flexcom_register(&lan966x_console,
				 fc->base + FLEXCOM_UART_OFFSET,
				 FLEXCOM_DIVISOR(PERIPHERAL_CLK, FLEXCOM_BAUDRATE));
	console_set_scope(&lan966x_console,
			  CONSOLE_FLAG_BOOT | CONSOLE_FLAG_RUNTIME);
	lan966x_crash_console(&lan966x_console);
}

void lan966x_console_init(void)
{
	vcore_gpio_init(GCB_GPIO_OUT_SET(LAN966X_GCB_BASE));

	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC_FC:
	case LAN966X_STRAP_BOOT_QSPI_FC:
	case LAN966X_STRAP_BOOT_SD_FC:
	case LAN966X_STRAP_BOOT_MMC_TFAMON_FC:
	case LAN966X_STRAP_BOOT_QSPI_TFAMON_FC:
	case LAN966X_STRAP_BOOT_SD_TFAMON_FC:
		lan966x_flexcom_init(FC_DEFAULT);
		break;
	case LAN966X_STRAP_TFAMON_FC0:
		lan966x_flexcom_init(FLEXCOM0);
		break;
	case LAN966X_STRAP_TFAMON_FC2:
		lan966x_flexcom_init(FLEXCOM2);
		break;
	case LAN966X_STRAP_TFAMON_FC3:
		lan966x_flexcom_init(FLEXCOM3);
		break;
	case LAN966X_STRAP_TFAMON_FC4:
		lan966x_flexcom_init(FLEXCOM4);
		break;
	default:
		/* No console */
		break;
	}
}

void plat_qspi_init_clock(void)
{
	uint8_t clk = 0;

	lan966x_fw_config_read_uint8(LAN966X_FW_CONF_QSPI_CLK, &clk, QSPI_DEFAULT_SPEED_MHZ);
	/* Clamp to [5MHz ; 100MHz] */
	clk = MAX(clk, (uint8_t) 5);
	clk = MIN(clk, (uint8_t) 100);
	VERBOSE("QSPI: Using clock %u Mhz\n", clk);
	lan966x_clk_disable(LAN966X_CLK_ID_QSPI0);
	lan966x_clk_set_rate(LAN966X_CLK_ID_QSPI0, clk * 1000 * 1000);
	lan966x_clk_enable(LAN966X_CLK_ID_QSPI0);
}

void lan966x_timer_init(void)
{
	uintptr_t syscnt = LAN966X_CPU_SYSCNT_BASE;

	mmio_write_32(CPU_SYSCNT_CNTCVL(syscnt), 0);	/* Low */
	mmio_write_32(CPU_SYSCNT_CNTCVU(syscnt), 0);	/* High */
	mmio_write_32(CPU_SYSCNT_CNTCR(syscnt),
		      CPU_SYSCNT_CNTCR_CNTCR_EN(1));	/*Enable */

        /* Program the counter frequency */
        write_cntfrq_el0(plat_get_syscnt_freq2());
}

unsigned int plat_get_syscnt_freq2(void)
{
	return SYS_COUNTER_FREQ_IN_TICKS;
}

/*
 * Some release build strapping modes will only show error traces by default
 */
void lan966x_set_max_trace_level(void)
{
#if !DEBUG
	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC:
	case LAN966X_STRAP_BOOT_QSPI:
	case LAN966X_STRAP_BOOT_SD:
	case LAN966X_STRAP_PCIE_ENDPOINT:
	case LAN966X_STRAP_TFAMON_FC0:
	case LAN966X_STRAP_TFAMON_FC2:
	case LAN966X_STRAP_TFAMON_FC3:
	case LAN966X_STRAP_TFAMON_FC4:
	case LAN966X_STRAP_SPI_SLAVE:
		tf_log_set_max_level(LOG_LEVEL_ERROR);
		break;
	default:
		/* No change in trace level */
		break;
	}
#endif
}

/*
 * Restore the loglevel for certain strapping modes release builds.
 */
void lan966x_reset_max_trace_level(void)
{
#if !DEBUG
	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC:
	case LAN966X_STRAP_BOOT_QSPI:
	case LAN966X_STRAP_BOOT_SD:
	case LAN966X_STRAP_PCIE_ENDPOINT:
	case LAN966X_STRAP_TFAMON_FC0:
	case LAN966X_STRAP_TFAMON_FC2:
	case LAN966X_STRAP_TFAMON_FC3:
	case LAN966X_STRAP_TFAMON_FC4:
	case LAN966X_STRAP_SPI_SLAVE:
		tf_log_set_max_level(LOG_LEVEL);
		break;
	default:
		/* No change in trace level */
		break;
	}
#endif
}
