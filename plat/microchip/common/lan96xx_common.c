/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/microchip/qspi.h>
#include <drivers/microchip/tz_matrix.h>
#include <errno.h>
#include <lan96xx_common.h>
#include <lan96xx_mmc.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>
#include <plat_otp.h>

#include <lan966x_regs.h>

#define GPR0_STRAPPING_SET	BIT(20) /* 0x100000 */

/*
 * Read strapping into GPR(0) to allow override
 */
void lan966x_init_strapping(void)
{
	uint32_t status;
	uint8_t strapping;

	status = mmio_read_32(CPU_GENERAL_STAT(LAN966X_CPU_BASE));
	strapping = CPU_GENERAL_STAT_VCORE_CFG_X(status);
	mmio_write_32(CPU_GPR(LAN966X_CPU_BASE, 0), GPR0_STRAPPING_SET | strapping);
}

soc_strapping lan966x_get_strapping(void)
{
	uint32_t status;
	soc_strapping strapping;

	status = mmio_read_32(CPU_GPR(LAN966X_CPU_BASE, 0));
	assert(status & GPR0_STRAPPING_SET);
	strapping = CPU_GENERAL_STAT_VCORE_CFG_X(status);

	VERBOSE("VCORE_CFG = %d\n", strapping);

	return strapping;
}

void lan966x_io_bootsource_init(void)
{
	boot_source_type boot_source = lan966x_get_boot_source();

	switch (boot_source) {
	case BOOT_SOURCE_EMMC:
	case BOOT_SOURCE_SDMMC:
		/* Setup MMC */
		lan966x_mmc_plat_config(boot_source);
		break;

	case BOOT_SOURCE_QSPI:
		/* Init QSPI */
		qspi_init();
		break;

	default:
		break;
	}

	/*
	 * Note: QSPI access is granted even though we don't boot from it
	 */

	/* Ensure we have ample reach on QSPI mmap area */
	/* 16M should be more than adequate - EVB/SVB have 2M */
	matrix_configure_srtop(MATRIX_SLAVE_QSPI0,
			       MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_16M) |
			       MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_16M));
}

void plat_bootstrap_set_strapping(soc_strapping value)
{

	/* To override strapping previous boot src must be 'none' */
	if (lan966x_get_boot_source() == BOOT_SOURCE_NONE) {
		/* And new strapping should be limited as below */
		if (value == LAN966X_STRAP_BOOT_MMC ||
		    value == LAN966X_STRAP_BOOT_QSPI ||
		    value == LAN966X_STRAP_BOOT_SD ||
		    value == LAN966X_STRAP_PCIE_ENDPOINT) {
			NOTICE("OVERRIDE strapping = 0x%08x\n", value);
			mmio_write_32(CPU_GPR(LAN966X_CPU_BASE, 0), GPR0_STRAPPING_SET | value);
			/* Do initialization according to new source */
			lan966x_io_bootsource_init();
		} else {
			ERROR("Strap override %d illegal\n", value);
		}
	} else {
		ERROR("Strap override is illegal if boot source is already valid\n");
	}
}

/*
 * Check if the current boot strapping mode has been masked out by the
 * OTP strapping mask and abort if this is the case.
 */
void lan966x_validate_strapping(void)
{
	union {
		uint8_t b[2];
		uint16_t w;
	} mask;
	uint16_t strapmask = BIT(lan966x_get_strapping());

	if (otp_read_otp_strap_disable_mask(mask.b, OTP_STRAP_DISABLE_MASK_SIZE)) {
		return;
	}
	if (strapmask & mask.w) {
		ERROR("Bootstrapping masked: %u\n", lan966x_get_strapping());
		plat_error_handler(-EINVAL);
	}
}
