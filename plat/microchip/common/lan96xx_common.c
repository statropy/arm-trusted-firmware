/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <lan96xx_common.h>
#include <lan96xx_mmc.h>
#include <lib/mmio.h>
#include <drivers/microchip/qspi.h>
#include <drivers/microchip/tz_matrix.h>

#include <lan966x_regs.h>

#define GPR0_STRAPPING_SET	BIT(20) /* 0x100000 */

void lan966x_io_init_dev(boot_source_type boot_source)
{
	switch (boot_source) {
	case BOOT_SOURCE_EMMC:
	case BOOT_SOURCE_SDMMC:
		/* Setup MMC */
		lan966x_mmc_plat_config(boot_source);
		break;

	case BOOT_SOURCE_QSPI:
		/* We own SPI */
		mmio_setbits_32(CPU_GENERAL_CTRL(LAN966X_CPU_BASE),
				CPU_GENERAL_CTRL_IF_SI_OWNER_M);

		/* Enable memmap access */
		qspi_init();

		/* Ensure we have ample reach on QSPI mmap area */
		/* 16M should be more than adequate - EVB/SVB have 2M */
		matrix_configure_srtop(MATRIX_SLAVE_QSPI0,
				       MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_16M) |
				       MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_16M));

		/* Enable QSPI0 for NS access */
		matrix_configure_slave_security(MATRIX_SLAVE_QSPI0,
						MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_16M) |
						MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_16M),
						MATRIX_SASPLIT(0, MATRIX_SRTOP_VALUE_16M),
						MATRIX_LANSECH_NS(0));
	default:
		break;
	}
}

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

boot_source_type lan966x_get_boot_source(void)
{
	boot_source_type boot_source;

	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC:
	case LAN966X_STRAP_BOOT_MMC_FC:
	case LAN966X_STRAP_BOOT_MMC_TFAMON_FC:
		boot_source = BOOT_SOURCE_EMMC;
		break;
	case LAN966X_STRAP_BOOT_QSPI:
	case LAN966X_STRAP_BOOT_QSPI_FC:
	case LAN966X_STRAP_BOOT_QSPI_TFAMON_FC:
		boot_source = BOOT_SOURCE_QSPI;
		break;
	case LAN966X_STRAP_BOOT_SD:
	case LAN966X_STRAP_BOOT_SD_FC:
	case LAN966X_STRAP_BOOT_SD_TFAMON_FC:
		boot_source = BOOT_SOURCE_SDMMC;
		break;
	default:
		boot_source = BOOT_SOURCE_NONE;
		break;
	}

	return boot_source;
}

bool lan966x_bootable_source(void)
{
	boot_source_type boot_source = lan966x_get_boot_source();

	switch (boot_source) {
	case BOOT_SOURCE_QSPI:
	case BOOT_SOURCE_EMMC:
	case BOOT_SOURCE_SDMMC:
		return true;
	default:
		break;
	}

	return false;
}

bool lan966x_monitor_enabled(void)
{
	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC_TFAMON_FC:
	case LAN966X_STRAP_BOOT_QSPI_TFAMON_FC:
	case LAN966X_STRAP_BOOT_SD_TFAMON_FC:
	case LAN966X_STRAP_TFAMON_FC0:
	case LAN966X_STRAP_TFAMON_FC2:
	case LAN966X_STRAP_TFAMON_FC3:
	case LAN966X_STRAP_TFAMON_FC4:
	case LAN966X_STRAP_TFAMON_USB:
		return true;
	default:
		break;
	}

	return false;
}
