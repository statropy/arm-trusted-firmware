/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/delay_timer.h>
#include <lib/mmio.h>

#include "lan966x_regs.h"
#include "lan966x_private.h"
#include "plat_otp.h"

static void setup_ns_access(uintptr_t gpv, uintptr_t tzpm)
{
	mmio_write_32(GPV_SECURITY_CPU_REGS(gpv), true);
	mmio_write_32(GPV_SECURITY_CSR_REGS(gpv), true);

	/* Magic key to unlock protection */
	mmio_write_32(TZPM_TZPM_KEY(tzpm), 0x12AC4B5D);
	mmio_setbits_32(TZPM_TZPCTL0(tzpm),
			TZPM_TZPCTL0_QSPI0(1) |
			TZPM_TZPCTL0_QSPI2(1) |
			TZPM_TZPCTL0_MCAN0(1) |
			TZPM_TZPCTL0_MCAN1(1) |
			TZPM_TZPCTL0_UDPHS0(1) |
			TZPM_TZPCTL0_SDMMC(1));
	mmio_setbits_32(TZPM_TZPCTL1(tzpm),
			TZPM_TZPCTL1_FLEXCOM0(1) |
			TZPM_TZPCTL1_FLEXCOM1(1) |
			TZPM_TZPCTL1_FLEXCOM2(1) |
			TZPM_TZPCTL1_FLEXCOM3(1) |
			TZPM_TZPCTL1_FLEXCOM4(1) |
			TZPM_TZPCTL1_SHA(1) |
			TZPM_TZPCTL1_XDMA(1) |
			TZPM_TZPCTL1_QSPI1(1) |
			TZPM_TZPCTL1_TRNG(1));
	mmio_setbits_32(TZPM_TZPCTL2(tzpm),
			TZPM_TZPCTL2_PKCC(1));
	mmio_setbits_32(TZPM_TZPCTL3(tzpm),
			TZPM_TZPCTL3_RTE(1) |
			TZPM_TZPCTL3_FDMA(1));
	/* Reset key to reestablish protection */
	mmio_write_32(TZPM_TZPM_KEY(tzpm), 0);
}

static void setup_tzaesb(uintptr_t tzaesb)
{
	uint32_t keys[4];
	int i;

	for (i = 0; i < ARRAY_SIZE(keys); i++)
		keys[i] = lan966x_trng_read();

	/* KEY REGS */
	mmio_write_32(TZAESBNS_TZAESB_MR(tzaesb),
		      TZAESBNS_TZAESB_MR_OPMOD(4) | /* CTR mode */
		      TZAESBNS_TZAESB_MR_DUALBUFF(1) |
		      TZAESBNS_TZAESB_MR_AAHB(1));
	mmio_write_32(TZAESBNS_TZAESB_KEYWR0(tzaesb), keys[0]);
	mmio_write_32(TZAESBNS_TZAESB_KEYWR1(tzaesb), keys[1]);
	mmio_write_32(TZAESBNS_TZAESB_KEYWR2(tzaesb), keys[2]);
	mmio_write_32(TZAESBNS_TZAESB_KEYWR3(tzaesb), keys[3]);

	/* IV REGS */
	mmio_write_32(TZAESBNS_TZAESB_IVR0(tzaesb), 0xfcfdfeff);
	mmio_write_32(TZAESBNS_TZAESB_IVR1(tzaesb), 0xf8f9fafb);
	mmio_write_32(TZAESBNS_TZAESB_IVR2(tzaesb), 0xf4f5f6f7);
	mmio_write_32(TZAESBNS_TZAESB_IVR3(tzaesb), 0xf0f1f2f3);

	/* Don't leak keys */
	memset(keys, 0, sizeof(keys));
}

static void tzaes_asc_region_enable(uintptr_t tzaes_asc,
				    int region, bool ns,
				    uintptr_t start, uintptr_t end)
{
	uint32_t val, mask = BIT(region);
	int i;

	/* Region - start/end */
	mmio_write_32(AESB_ASC_TZAESBASC_RBAR0(tzaes_asc) + (region * 8), start);
	mmio_write_32(AESB_ASC_TZAESBASC_RTAR0(tzaes_asc) + (region * 8), end - 1);

	/* One non-secure region */
	if (ns) {
		mmio_setbits_32(AESB_ASC_TZAESBASC_RSECR(tzaes_asc),
				AESB_ASC_TZAESBASC_RSECR_SECX(mask));
	}

	/* Enable the regions */
	mmio_setbits_32(AESB_ASC_TZAESBASC_RER(tzaes_asc),
			AESB_ASC_TZAESBASC_RER_ENX(mask));

	/* Check - Read Error Status of regions */
	val = mmio_read_32(AESB_ASC_TZAESBASC_RESR(tzaes_asc));
	if (val & mask)
		NOTICE("RESR error %04x\n", val);
	assert((val & mask) == 0);

	/* Wait for Regions Enabled */
	for (i = 0; i < 10 ; i++) {
		val = mmio_read_32(AESB_ASC_TZAESBASC_RSR(tzaes_asc));
		if ((AESB_ASC_TZAESBASC_RSR_ESX_X(val)) & mask)
			break;
		udelay(1);
	}
	if (!((AESB_ASC_TZAESBASC_RSR_ESX_X(val)) & mask))
		WARN("RSR_ESx is %04x, expected %04x\n", val, mask);
	assert(val & mask);

	INFO("TrustZone region %d set up - %s\n", region, ns ? "NS" : "S");
}

static void setup_tzaes_asc(void)
{
	uintptr_t tzaes_asc = LAN966X_AESB_ASC_BASE;

	/*
	 * Configure ONE region of TZAESBASC with BL32 (S) address ranges
	 */
	tzaes_asc_region_enable(tzaes_asc, 0, false,
				BL32_BASE,
				BL32_LIMIT);
}

void lan966x_tz_init(void)
{
	INFO("Configuring TrustZone (with encrypted RAM)\n");

	/* Enable TZAESB_LITE_EN - UNG_MASERATI-759 and better performance */
	mmio_write_32(CPU_TZAESB(LAN966X_CPU_BASE),
		      CPU_TZAESB_TZAESB_LITE_EN(1));

	/*
	 * Enable TZPM for NS transactions, Otherwise all are treated
	 * as Secure transactions in CPU subsystem
	 */
	mmio_write_32(TZPM_TZPM_EN(LAN966X_TZPM_BASE),
		      TZPM_TZPM_EN_TZPM_EN(1));

	/* TZASC controller */
	setup_tzaes_asc();

	/* S setup */
	setup_tzaesb(LAN966X_TZAESBS_BASE);

	/* Configure SECURITY_DDR_CSS for Non-Secure */
	mmio_write_32(GPV_SECURITY_DDR_CSS(LAN966X_GPV_BASE),
		      GPV_SECURITY_DDR_CSS_SECURITY_DDR_CSS(1));

	/* Configure SECURITY_DDR_MAIN for Non-Secure */
	mmio_write_32(GPV_SECURITY_DDR_MAIN(LAN966X_GPV_BASE),
		      GPV_SECURITY_DDR_MAIN_SECURITY_DDR_MAIN(1));

	/* Configure SECURITY_CuPHY for Non-Secure */
	mmio_write_32(GPV_SECURITY_CuPHY(LAN966X_GPV_BASE),
		      GPV_SECURITY_CuPHY_SECURITY_CuPHY(1));

	/* Configure SECURITY_PCIE_OB for Non-secure */
	mmio_write_32(GPV_SECURITY_PCIE_OB(LAN966X_GPV_BASE),
		      GPV_SECURITY_PCIE_OB_SECURITY_PCIE_OB(1));

	/* Configure SECURITY_PCIE_DBI for Non-secure */
	mmio_write_32(GPV_SECURITY_PCIE_DBI(LAN966X_GPV_BASE),
		      GPV_SECURITY_PCIE_DBI_SECURITY_PCIE_DBI(1));

	/* Configure SECURITY_PCIE_CFG for Non-secure */
	mmio_write_32(GPV_SECURITY_PCIE_CFG(LAN966X_GPV_BASE),
		      GPV_SECURITY_PCIE_CFG_SECURITY_PCIE_CFG(1));

	/* Configure Misc AHB for Non-Secure */
	mmio_write_32(GPV_SECURITY_APB_MAIN3(LAN966X_GPV_BASE),
		      BIT(4) |	/* Timers */
		      BIT(5));	/* WDT */


	/* Configure OTP for Non-Secure */
	mmio_write_32(GPV_SECURITY_APB_CSS2(LAN966X_GPV_BASE),
		      BIT(0));

	/* Configure SECURITY_QSPI1 for Non-secure */
	mmio_write_32(GPV_SECURITY_QSPI1(LAN966X_GPV_BASE),
		      GPV_SECURITY_QSPI1_SECURITY_QSPI1(1));

	/* NS periph access */
	setup_ns_access(LAN966X_GPV_BASE, LAN966X_TZPM_BASE);
}
