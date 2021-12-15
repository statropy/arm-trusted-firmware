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
#include <drivers/microchip/vcore_gpio.h>
#include <fw_config.h>
#include <lib/mmio.h>
#include <plat/arm/common/plat_arm.h>

#include "lan969x_regs.h"
#include "lan969x_private.h"

/* Define global fw_config, set default MMC settings */
lan966x_fw_config_t lan966x_fw_config = {
	FW_CONFIG_INIT_32(LAN966X_FW_CONF_MMC_CLK_RATE, MMC_DEFAULT_SPEED),
	FW_CONFIG_INIT_8(LAN966X_FW_CONF_MMC_BUS_WIDTH, MMC_BUS_WIDTH_1),
	FW_CONFIG_INIT_8(LAN966X_FW_CONF_QSPI_CLK, 25), /* 25Mhz */
};

#define LAN969X_MAP_QSPI0						\
	MAP_REGION_FLAT(						\
		LAN969X_QSPI0_MMAP,					\
		LAN969X_QSPI0_RANGE,					\
		MT_MEMORY | MT_RO | MT_SECURE)

#define LAN969X_MAP_AXI							\
	MAP_REGION_FLAT(						\
		LAN969X_DEV_BASE,					\
		LAN969X_DEV_SIZE,					\
		MT_DEVICE | MT_RW | MT_SECURE)

#define LAN969X_MAP_BL31					\
	MAP_REGION_FLAT(					\
		BL31_BASE,					\
		BL31_SIZE,					\
		MT_MEMORY | MT_RW | MT_SECURE)

#define LAN969X_MAP_USB						\
	MAP_REGION_FLAT(					\
		LAN969X_USB_BASE,				\
		LAN969X_USB_SIZE,				\
		MT_DEVICE | MT_RW | MT_SECURE)

#ifdef IMAGE_BL1
const mmap_region_t plat_arm_mmap[] = {
	LAN969X_MAP_QSPI0,
	LAN969X_MAP_AXI,
	//LAN969X_MAP_USB,
	{0}
};
#endif
#if defined(IMAGE_BL2) || defined(IMAGE_BL2U) || defined(IMAGE_BL31)
const mmap_region_t plat_arm_mmap[] = {
	LAN969X_MAP_QSPI0,
	LAN969X_MAP_AXI,
	LAN969X_MAP_BL31,
	//LAN969X_MAP_USB,
	{0}
};
#endif

static console_t lan969x_console;

enum lan969x_flexcom_id {
	FLEXCOM0 = 0,
	FLEXCOM1,
	FLEXCOM2,
	FLEXCOM3,
	FLEXCOM4,
};

static struct lan969x_flexcom_args {
	uintptr_t base;
	unsigned int clk_id;
	int rx_gpio;
	int tx_gpio;
} lan969x_flexcom_map[] = {
	[FLEXCOM0] = {
		LAN969X_FLEXCOM_0_BASE, LAN966X_CLK_ID_FLEXCOM0, 25, 26
	},
	[FLEXCOM1] = { 0 },
	[FLEXCOM2] = {
		LAN969X_FLEXCOM_2_BASE, LAN966X_CLK_ID_FLEXCOM2, 44, 45
	},
	[FLEXCOM3] = {
		LAN969X_FLEXCOM_3_BASE, LAN966X_CLK_ID_FLEXCOM3, 52, 53
	},
	[FLEXCOM4] = {
		LAN969X_FLEXCOM_4_BASE, LAN966X_CLK_ID_FLEXCOM4, 57, 58
	},
};

static void lan969x_flexcom_init(int idx)
{
	struct lan969x_flexcom_args *fc;

	if (idx < 0)
		return;

	fc = &lan969x_flexcom_map[idx];

	if (fc->base == 0)
		return;

	/* GPIOs for RX and TX */
	//vcore_gpio_set_alt(fc->rx_gpio, 1);
	//vcore_gpio_set_alt(fc->tx_gpio, 1);

	/* Initialize the console to provide early debug support */
	console_flexcom_register(&lan969x_console,
				 fc->base + FLEXCOM_UART_OFFSET,
				 FLEXCOM_DIVISOR(PERIPHERAL_CLK, FLEXCOM_BAUDRATE));
	console_set_scope(&lan969x_console,
			  CONSOLE_FLAG_BOOT | CONSOLE_FLAG_RUNTIME);
}

void lan969x_console_init(void)
{
	//vcore_gpio_init(GCB_GPIO_OUT_SET(LAN969X_GCB_BASE));

	lan969x_flexcom_init(FLEXCOM0);
}

uintptr_t plat_get_ns_image_entrypoint(void)
{
	return PLAT_LAN969X_NS_IMAGE_BASE;
}

#define GPR0_STRAPPING_SET	BIT(20) /* 0x100000 */

/*
 * Read strapping into GPR(0) to allow override
 */
void lan969x_init_strapping(void)
{
	uint32_t status;
	uint8_t strapping;

	status = mmio_read_32(CPU_GENERAL_STAT(LAN969X_CPU_BASE));
	strapping = CPU_GENERAL_STAT_VCORE_CFG_X(status);
	mmio_write_32(CPU_GPR(LAN969X_CPU_BASE, 0), GPR0_STRAPPING_SET | strapping);
}

soc_strapping lan969x_get_strapping(void)
{
	uint32_t status;
	uint8_t strapping;

	status = mmio_read_32(CPU_GPR(LAN969X_CPU_BASE, 0));
	assert(status & GPR0_STRAPPING_SET);
	strapping = CPU_GENERAL_STAT_VCORE_CFG_X(status);

	VERBOSE("VCORE_CFG = %d\n", strapping);

	return strapping;
}

void lan969x_set_strapping(soc_strapping value)
{

	/* To override strapping previous boot src must be 'none' */
	if (lan969x_get_boot_source() == BOOT_SOURCE_NONE) {
		/* And new strapping should be limited as below */
		if (value == LAN969X_STRAP_BOOT_MMC ||
		    value == LAN969X_STRAP_BOOT_QSPI ||
		    value == LAN969X_STRAP_BOOT_SD) {
			NOTICE("OVERRIDE strapping = 0x%08x\n", value);
			mmio_write_32(CPU_GPR(LAN969X_CPU_BASE, 0), GPR0_STRAPPING_SET | value);
		} else {
			ERROR("Strap override %d illegal\n", value);
		}
	} else {
		ERROR("Strap override is illegal if boot source is already valid\n");
	}
}

boot_source_type lan969x_get_boot_source(void)
{
        boot_source_type boot_source;

        switch (lan969x_get_strapping()) {
        case LAN969X_STRAP_BOOT_MMC:
                boot_source = BOOT_SOURCE_EMMC;
                break;
        case LAN969X_STRAP_BOOT_QSPI:
                boot_source = BOOT_SOURCE_QSPI;
                break;
        case LAN969X_STRAP_BOOT_SD:
                boot_source = BOOT_SOURCE_SDMMC;
                break;
        default:
                boot_source = BOOT_SOURCE_NONE;
                break;
        }

        return boot_source;
}

unsigned int plat_get_syscnt_freq2(void)
{
	return SYS_COUNTER_FREQ_IN_TICKS;
	//return read_cntfrq_el0();
}

void lan969x_timer_init(void)
{
	uintptr_t syscnt = LAN969X_CPU_SYSCNT_BASE;

	mmio_write_32(CPU_SYSCNT_CNTCVL(syscnt), 0);	/* Low */
	mmio_write_32(CPU_SYSCNT_CNTCVU(syscnt), 0);	/* High */
	mmio_write_32(CPU_SYSCNT_CNTCR(syscnt),
		      CPU_SYSCNT_CNTCR_CNTCR_EN(1));	/*Enable */
}

/*******************************************************************************
 * Returns ARM platform specific memory map regions.
 ******************************************************************************/
const mmap_region_t *plat_arm_get_mmap(void)
{
	return plat_arm_mmap;
}

/* Platform QSPI clock init - currenly empty */
void plat_qspi_init_clock(void)
{
	INFO("QSPI: Platform clock init (TBD)\n");
}

void lan969x_fwconfig_apply(void)
{
	boot_source_type boot_source = lan969x_get_boot_source();

	/* Update storage drivers with new values from fw_config */
	switch (boot_source) {
	case BOOT_SOURCE_QSPI:
		qspi_reinit();
		break;
	case BOOT_SOURCE_SDMMC:
	case BOOT_SOURCE_EMMC:
		lan969x_mmc_plat_config(boot_source);
		break;
	default:
		break;
	}
}
