/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/console.h>
#include <drivers/io/io_storage.h>
#include <drivers/microchip/emmc.h>
#include <drivers/microchip/flexcom_uart.h>
#include <drivers/microchip/lan966x_clock.h>
#include <drivers/microchip/qspi.h>
#include <drivers/microchip/tz_matrix.h>
#include <drivers/microchip/vcore_gpio.h>
#include <drivers/microchip/sha.h>
#include <lib/mmio.h>
#include <platform_def.h>

#include <plat/common/platform.h>
#include <plat/arm/common/arm_config.h>
#include <plat/arm/common/plat_arm.h>

#include "lan966x_regs.h"
#include "lan966x_private.h"
#include "plat_otp.h"

CASSERT((BL1_RW_SIZE + BL2_SIZE + MMC_BUF_SIZE) <= LAN996X_SRAM_SIZE, assert_sram_depletion);

static console_t lan966x_console;

/* Define global fw_config and set default startup values */
lan966x_fw_config_t lan966x_fw_config = {
	 .config[LAN966X_CONF_CLK_RATE] = SDCLOCK_400KHZ,
	 .config[LAN966X_CONF_BUS_WIDTH] = MMC_BUS_WIDTH_1,
};

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

#define MAX_BARS	5		/* Maximum number of configurable bars */
#define OTP_BAR_SIZE	(MAX_BARS*2)	/* Address and size information */

typedef enum {
	LAN966X_DEVICE_ID,
	LAN966X_VENDOR_ID,
	LAN966X_SUBSYS_DEVICE_ID,
	LAN966X_SUBSYS_VENDOR_ID
} lan966x_pcie_id;

#define PCIE_BAR_REG(B)		PCIE_DBI_IATU_REGION_CTRL_2_OFF_INBOUND_## B(LAN966X_PCIE_DBI_BASE)
#define PCIE_BAR_MASK(B)	PCIE_DBI_IATU_REGION_CTRL_2_OFF_INBOUND_## B ##_IATU_REGION_CTRL_2_OFF_INBOUND_## B ##_REGION_EN_M |	\
				PCIE_DBI_IATU_REGION_CTRL_2_OFF_INBOUND_## B ##_IATU_REGION_CTRL_2_OFF_INBOUND_## B ##_MATCH_MODE_M |	\
				PCIE_DBI_IATU_REGION_CTRL_2_OFF_INBOUND_## B ##_IATU_REGION_CTRL_2_OFF_INBOUND_## B ##_BAR_NUM_M
#define PCIE_BAR_VALUE(B)	PCIE_DBI_IATU_REGION_CTRL_2_OFF_INBOUND_## B ##_IATU_REGION_CTRL_2_OFF_INBOUND_## B ##_REGION_EN_M |	\
				PCIE_DBI_IATU_REGION_CTRL_2_OFF_INBOUND_## B ##_IATU_REGION_CTRL_2_OFF_INBOUND_## B ##_MATCH_MODE_M |	\
				PCIE_DBI_IATU_REGION_CTRL_2_OFF_INBOUND_## B ##_IATU_REGION_CTRL_2_OFF_INBOUND_## B ##_BAR_NUM(B)
#define PCIE_BAR_TARGET_REG(B)	PCIE_DBI_IATU_LWR_TARGET_ADDR_OFF_INBOUND_ ## B(LAN966X_PCIE_DBI_BASE)
#define PCIE_BAR_MASK_REG(B)	PCIE_DBI_BAR ## B ##_MASK_REG(LAN966X_PCIE_DBI_BASE)

static const struct {
	char *name;
	uint32_t bar_start;
	uint32_t bar_size;
	uintptr_t bar_reg;
	uint32_t bar_mask;
	uint32_t bar_value;
	uintptr_t bar_target_reg;
	uintptr_t bar_mask_reg;
} lan966x_pcie_bar_config[] = {
	{
		"CSR",
		0xe2000000,
		 0x2000000,
		PCIE_BAR_REG(0),
		PCIE_BAR_MASK(0),
		PCIE_BAR_VALUE(0),
		PCIE_BAR_TARGET_REG(0),
		PCIE_BAR_MASK_REG(0)
	},
	{
		"CPU",
		0xe0000000,
		 0x1000000,
		PCIE_BAR_REG(1),
		PCIE_BAR_MASK(1),
		PCIE_BAR_VALUE(1),
		PCIE_BAR_TARGET_REG(1),
		PCIE_BAR_MASK_REG(1)
	},
	{
		"QSPI1",
		0x40000000,
		  0x800000,
		PCIE_BAR_REG(2),
		PCIE_BAR_MASK(2),
		PCIE_BAR_VALUE(2),
		PCIE_BAR_TARGET_REG(2),
		PCIE_BAR_MASK_REG(2)
	},
	{
		"PI",
		0x48000000,
		  0x800000,
		PCIE_BAR_REG(3),
		PCIE_BAR_MASK(3),
		PCIE_BAR_VALUE(3),
		PCIE_BAR_TARGET_REG(3),
		PCIE_BAR_MASK_REG(3)
	},
	{
		"SRAM",
		0x01000000,
		   0x20000,
		PCIE_BAR_REG(4),
		PCIE_BAR_MASK(4),
		PCIE_BAR_VALUE(4),
		PCIE_BAR_TARGET_REG(4),
		PCIE_BAR_MASK_REG(4)
	},
};


/*******************************************************************************
 * Returns ARM platform specific memory map regions.
 ******************************************************************************/
const mmap_region_t *plat_arm_get_mmap(void)
{
	return plat_arm_mmap;
}

void lan966x_console_init(void)
{
	uintptr_t base;

	/* NB: Needed as OTP emulation need qspi */
	lan966x_io_init();

	vcore_gpio_init(GCB_GPIO_OUT_SET(LAN966X_GCB_BASE));

	/* See if boot media config defines a console */
#if defined(LAN966X_ASIC)
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
	lan966x_clk_disable(LAN966X_CLK_ID_FC3);
	lan966x_clk_set_rate(LAN966X_CLK_ID_FC3, LAN966X_CLK_FREQ_FLEXCOM); /* 30MHz */
	lan966x_clk_enable(LAN966X_CLK_ID_FC3);
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

	mmio_write_32(CPU_SYSCNT_CNTCVL(syscnt), 0);	/* Low */
	mmio_write_32(CPU_SYSCNT_CNTCVU(syscnt), 0);	/* High */
	mmio_write_32(CPU_SYSCNT_CNTCR(syscnt),
		      CPU_SYSCNT_CNTCR_CNTCR_EN(1));	/*Enable */
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

uint32_t lan966x_get_boot_source(void)
{
	boot_source_type boot_source;

	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC:
		boot_source = BOOT_SOURCE_EMMC;
		break;
	case LAN966X_STRAP_BOOT_QSPI:
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

int lan966x_get_fw_config_data(lan966x_fw_cfg_data id)
{
	int value = -1;

	if (id >= 0 && id < LAN966X_CONF_NUM_OF_ITEMS) {
		value = lan966x_fw_config.config[id];
	} else {
		ERROR("Illegal access id to fw_config structure\n");
	}

	return value;
}

/*
 * Derive a 32 byte key with a 32 byte salt, output a 32 byte key
 */
int lan966x_derive_key(const lan966x_key32_t *in,
		       const lan966x_key32_t *salt,
		       lan966x_key32_t *out)
{
	uint8_t buf[LAN966X_KEY32_LEN * 2];
	int ret;

	/* Use one contiguous buffer for now */
	memcpy(buf, in->b, LAN966X_KEY32_LEN);
	memcpy(buf + LAN966X_KEY32_LEN, salt->b, LAN966X_KEY32_LEN);

	ret = sha_calc(SHA_MR_ALGO_SHA256, buf, sizeof(buf), out->b);

	/* Don't leak */
	memset(buf, 0, sizeof(buf));

	return ret;
}

static void lan966x_config_pcie_id(lan966x_pcie_id id, uint32_t defvalue, const char *name)
{
	uint32_t value, mask, ival;
	uintptr_t addr;
	int ret;

	switch (id) {
	case LAN966X_DEVICE_ID:
		ret = otp_read_otp_pcie_device_id((uint8_t *)&value, OTP_OTP_PCIE_DEVICE_ID_BITS/8);
		break;
	case LAN966X_VENDOR_ID:
		ret = otp_read_otp_pcie_vendor_id((uint8_t *)&value, OTP_OTP_PCIE_VENDOR_ID_BITS/8);
		break;
	case LAN966X_SUBSYS_DEVICE_ID:
		ret = otp_read_otp_pcie_subsys_device_id((uint8_t *)&value, OTP_OTP_PCIE_SUBSYS_DEVICE_ID_BITS/8);
		break;
	case LAN966X_SUBSYS_VENDOR_ID:
		ret = otp_read_otp_pcie_subsys_vendor_id((uint8_t *)&value, OTP_OTP_PCIE_SUBSYS_VENDOR_ID_BITS/8);
		break;
	}
	if (ret == 0) {
		if (value != 0) {
		} else {
			INFO("Empty OTP PCIe %s id\n", name);
			value = defvalue;
		}
	} else {
		ERROR("Could not read OTP PCIe %s id\n", name);
		value = defvalue;
	}
	switch (id) {
	case LAN966X_DEVICE_ID:
		addr = PCIE_DBI_DEVICE_ID_VENDOR_ID_REG(LAN966X_PCIE_DBI_BASE);
		mask = PCIE_DBI_DEVICE_ID_VENDOR_ID_REG_PCI_TYPE0_DEVICE_ID_M;
		ival = PCIE_DBI_DEVICE_ID_VENDOR_ID_REG_PCI_TYPE0_DEVICE_ID(value);
		break;
	case LAN966X_VENDOR_ID:
		addr = PCIE_DBI_DEVICE_ID_VENDOR_ID_REG(LAN966X_PCIE_DBI_BASE);
		mask = PCIE_DBI_DEVICE_ID_VENDOR_ID_REG_PCI_TYPE0_VENDOR_ID_M;
		ival = PCIE_DBI_DEVICE_ID_VENDOR_ID_REG_PCI_TYPE0_VENDOR_ID(value);
		break;
	case LAN966X_SUBSYS_DEVICE_ID:
		addr = PCIE_DBI_SUBSYSTEM_ID_SUBSYSTEM_VENDOR_ID_REG(LAN966X_PCIE_DBI_BASE);
		mask = PCIE_DBI_SUBSYSTEM_ID_SUBSYSTEM_VENDOR_ID_REG_SUBSYS_DEV_ID_M;
		ival = PCIE_DBI_SUBSYSTEM_ID_SUBSYSTEM_VENDOR_ID_REG_SUBSYS_DEV_ID(value);
		break;
	case LAN966X_SUBSYS_VENDOR_ID:
		addr = PCIE_DBI_SUBSYSTEM_ID_SUBSYSTEM_VENDOR_ID_REG(LAN966X_PCIE_DBI_BASE);
		mask = PCIE_DBI_SUBSYSTEM_ID_SUBSYSTEM_VENDOR_ID_REG_SUBSYS_VENDOR_ID_M;
		ival = PCIE_DBI_SUBSYSTEM_ID_SUBSYSTEM_VENDOR_ID_REG_SUBSYS_VENDOR_ID(value);
		break;
	}
	INFO("Set PCIe %s id: 0x%04x\n", name, ival);
	mmio_clrsetbits_32(addr, mask, ival);
}

static void lan966x_config_pcie_bar(int bar, uint32_t otp_start, uint32_t otp_size, int status)
{
	bool enable = true;
	uint32_t start = lan966x_pcie_bar_config[bar].bar_start;
	uint32_t size = lan966x_pcie_bar_config[bar].bar_size;

	if (status == 0) {
		INFO("OTP BAR[%d]: offset: 0x%08x, size: %u\n", bar, otp_start, otp_size);
		if (otp_start != 0 && otp_size != 0) {
			start = otp_start;
			size = otp_size;
		} else if (otp_start != 0 && otp_size == 0) {
			enable = false;
		}
	}
	if (enable) {
		INFO("Enable PCIe BAR[%d]: offset: 0x%08x, mask: 0x%x\n", bar, start, size - 1);
		mmio_clrsetbits_32(lan966x_pcie_bar_config[bar].bar_reg,
				   lan966x_pcie_bar_config[bar].bar_mask,
				   lan966x_pcie_bar_config[bar].bar_value);
		mmio_write_32(lan966x_pcie_bar_config[bar].bar_target_reg, start);
		mmio_write_32(lan966x_pcie_bar_config[bar].bar_mask_reg, size - 1);
	} else {
		INFO("Disable PCIe BAR[%d]\n", bar);
		mmio_write_32(lan966x_pcie_bar_config[bar].bar_mask_reg, 0);
	}
}

void lan966x_pcie_init(void)
{
	uint32_t bar[OTP_BAR_SIZE];
	uint32_t cls;
	int ret;

	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_PCIE_ENDPOINT:
#ifdef _LAN966X_REGS_A0_H_
	case LAN966X_STRAP_BOOT_QSPI:
#endif
		/* The NIC card is strapped to QSPI boot because the A0
		 * bootrom does not handle the PCIe boot case correctly
		 */
		INFO("PCIe endpoint mode\n");
		break;
	default:
		return;
	}
	/* Stop PCIe Link Training and enable writing */
	mmio_clrsetbits_32(PCIE_CFG_PCIE_CFG(LAN966X_PCIE_CFG_BASE),
			   PCIE_CFG_PCIE_CFG_LTSSM_ENA_M |
			   PCIE_CFG_PCIE_CFG_DBI_RO_WR_DIS_M,
			   PCIE_CFG_PCIE_CFG_LTSSM_ENA(0) |
			   PCIE_CFG_PCIE_CFG_DBI_RO_WR_DIS(0));
	mmio_clrsetbits_32(PCIE_DBI_MISC_CONTROL_1_OFF(LAN966X_PCIE_DBI_BASE),
			   PCIE_DBI_MISC_CONTROL_1_OFF_DBI_RO_WR_EN_M,
			   PCIE_DBI_MISC_CONTROL_1_OFF_DBI_RO_WR_EN(1));
	/* Set class and revision id */
	ret = otp_read_otp_pcie_dev((uint8_t *)&cls, OTP_PCIE_DEV_SIZE);
	if (ret == 0) {
		if (cls == 0) {
			INFO("Set default PCIe device class info\n");
			cls = 0x02000000;
		}
	} else {
		ERROR("Could not read OTP PCIe device class info\n");
		cls = 0x02000000;
	}
	INFO("Set PCIe device class/subclass/progif/revid: 0x%08x\n", cls);
	mmio_write_32(PCIE_DBI_CLASS_CODE_REVISION_ID(LAN966X_PCIE_DBI_BASE), cls);

	/* Set Device and Vendor IDs */
	lan966x_config_pcie_id(LAN966X_DEVICE_ID, 0x9660, "device");
	lan966x_config_pcie_id(LAN966X_VENDOR_ID, 0x1055, "vendor");
	lan966x_config_pcie_id(LAN966X_SUBSYS_DEVICE_ID, 0x9660, "subsys_device");
	lan966x_config_pcie_id(LAN966X_SUBSYS_VENDOR_ID, 0x1055, "subsys_vendor");

	/* Configure Base Address Registers to map to local address space */
	ret = otp_read_otp_pcie_bars((uint8_t *)bar, OTP_PCIE_BARS_SIZE);
	lan966x_config_pcie_bar(0, bar[0], bar[0+MAX_BARS], ret);
	lan966x_config_pcie_bar(1, bar[1], bar[1+MAX_BARS], ret);
	lan966x_config_pcie_bar(2, bar[2], bar[2+MAX_BARS], ret);
	lan966x_config_pcie_bar(3, bar[3], bar[3+MAX_BARS], ret);
	lan966x_config_pcie_bar(4, bar[4], bar[4+MAX_BARS], ret);

	/* Configure Maximum Link Speed */
	ret = otp_read_otp_pcie_flags_max_link_speed();
	if (ret == 0) {
		ret = 2;
	}
	mmio_clrsetbits_32(PCIE_DBI_LINK_CAPABILITIES_REG(LAN966X_PCIE_DBI_BASE),
			   PCIE_DBI_LINK_CAPABILITIES_REG_PCIE_CAP_MAX_LINK_SPEED_M,
			   PCIE_DBI_LINK_CAPABILITIES_REG_PCIE_CAP_MAX_LINK_SPEED(ret));
	INFO("Set PCIe max link speed: %d\n", ret);

	/* Start PCIe Link Training and disable writing */
	mmio_clrsetbits_32(PCIE_DBI_MISC_CONTROL_1_OFF(LAN966X_PCIE_DBI_BASE),
			   PCIE_DBI_MISC_CONTROL_1_OFF_DBI_RO_WR_EN_M,
			   PCIE_DBI_MISC_CONTROL_1_OFF_DBI_RO_WR_EN(0));
	mmio_clrsetbits_32(PCIE_CFG_PCIE_CFG(LAN966X_PCIE_CFG_BASE),
			   PCIE_CFG_PCIE_CFG_LTSSM_ENA_M |
			   PCIE_CFG_PCIE_CFG_DBI_RO_WR_DIS_M,
			   PCIE_CFG_PCIE_CFG_LTSSM_ENA(1) |
			   PCIE_CFG_PCIE_CFG_DBI_RO_WR_DIS(1));

	/* Read protect region 4 of the OTP (keys) */
	mmio_clrsetbits_32(OTP_OTP_READ_PROTECT(LAN966X_OTP_BASE), BIT(4), BIT(4));
	ret = mmio_read_32(OTP_OTP_READ_PROTECT(LAN966X_OTP_BASE));
	INFO("OTP Read Protected regions: 0x%x\n", ret);

	/* Go to sleep */
	asm volatile (
		"sleep_loop:"
		"    wfi ;"
		"    b sleep_loop;"
	);
}
