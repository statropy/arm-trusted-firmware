/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>

#include <drivers/microchip/otp.h>
#include <lib/mmio.h>

#include "lan966x_regs.h"
#include "plat_otp.h"
#include "lan966x_private.h"


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
		  0x100000,
		   0x20000,
		PCIE_BAR_REG(4),
		PCIE_BAR_MASK(4),
		PCIE_BAR_VALUE(4),
		PCIE_BAR_TARGET_REG(4),
		PCIE_BAR_MASK_REG(4)
	},
};

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
		uint32_t mask = size - 1;

		start = start & ~mask;
		INFO("Enable PCIe BAR[%d]: offset: 0x%08x, mask: 0x%x\n", bar, start, mask);
		mmio_clrsetbits_32(lan966x_pcie_bar_config[bar].bar_reg,
				   lan966x_pcie_bar_config[bar].bar_mask,
				   lan966x_pcie_bar_config[bar].bar_value);
		mmio_write_32(lan966x_pcie_bar_config[bar].bar_target_reg, start);
		mmio_write_32(lan966x_pcie_bar_config[bar].bar_mask_reg, mask);
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
