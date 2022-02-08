/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/arm/tzc400.h>
#include <drivers/microchip/lan966x_trng.h>
#include <lib/mmio.h>

#include "lan966x_regs.h"
#include "lan969x_private.h"
#include "plat_otp.h"

#define TZC_NSAID_DEFAULT		0
#define PLAT_ARM_TZC_NS_DEV_ACCESS	TZC_REGION_ACCESS_RDWR(TZC_NSAID_DEFAULT)
#define PLAT_ARM_TZC_FILTERS		TZC_400_REGION_ATTR_FILTER_BIT(0)

static bool use_css = true;

static void setup_ns_access(uintptr_t gpv, uintptr_t tzpm)
{
	mmio_write_32(GPV_SECURITY_CPU_REGS(gpv), true);
	mmio_write_32(GPV_SECURITY_CSR_REGS(gpv), true);

	/* Magic key to unlock protection */
	mmio_write_32(TZPM_TZPM_KEY(tzpm), 0x12AC4B5D);
	mmio_setbits_32(TZPM_TZPCTL0(tzpm),
			TZPM_TZPCTL0_QSPI0(1) |
			TZPM_TZPCTL0_SDMMC(1));
	mmio_setbits_32(TZPM_TZPCTL1(tzpm),
			TZPM_TZPCTL1_FLEXCOM0(1) |
			TZPM_TZPCTL1_FLEXCOM1(1) |
			TZPM_TZPCTL1_FLEXCOM2(1) |
			TZPM_TZPCTL1_FLEXCOM3(1));
	mmio_setbits_32(TZPM_TZPCTL3(tzpm),
			TZPM_TZPCTL3_RTE(1) |
			TZPM_TZPCTL3_FDMA(1));
	/* Reset key to reestablish protection */
	mmio_write_32(TZPM_TZPM_KEY(tzpm), 0);
}

void lan969x_tzc_configure(uintptr_t tzc_base)
{
	INFO("Configuring TZC@%08lx\n", tzc_base);
	tzc400_init(tzc_base);
	tzc400_disable_filters();

#if 0
	/* Region 0 set to no access by default */
	tzc400_configure_region0(TZC_REGION_S_NONE, 0);

	/* "Rest" Regions set according to tzc_regions array */
	tzc400_configure_region(
		PLAT_ARM_TZC_FILTERS,
		1,
		PLAT_LAN969X_NS_IMAGE_BASE,
		PLAT_LAN969X_NS_IMAGE_BASE + PLAT_LAN969X_NS_IMAGE_SIZE - 1,
		TZC_REGION_S_RDWR,	     /* Secure = RDWR */
		PLAT_ARM_TZC_NS_DEV_ACCESS); /* NS = RDWR */
#else

	/* Allow Secure and Non-secure access to DRAM for EL3 payloads */
	tzc400_configure_region0(TZC_REGION_S_RDWR, PLAT_ARM_TZC_NS_DEV_ACCESS);

#endif

	/* Raise an exception+irq if a NS device tries to access secure memory */
	tzc400_set_action(TZC_ACTION_ERR_INT);

	/* Shields UP */
	tzc400_enable_filters();

	/* Done */
	INFO("TZC Active\n");
}

void lan969x_tz_init(void)
{
	INFO("Configuring TrustZone\n");

	/*
	 * Enable TZPM for NS transactions, Otherwise all are treated
	 * as Secure transactions in CPU subsystem
	 */
	mmio_write_32(TZPM_TZPM_EN(LAN969X_TZPM_BASE),
		      TZPM_TZPM_EN_TZPM_EN(1));

	/* NS periph access */
	setup_ns_access(LAN969X_GPV_BASE, LAN969X_TZPM_BASE);

	/* TZC - 2 of them */
	if (use_css)
		/* DDR accesess through CSS */
		lan969x_tzc_configure(LAN969X_TZC_CSS_BASE);
	else
		/* DDR access through TZAESB */
		lan969x_tzc_configure(LAN969X_TZC_MAIN_HSS_BASE);
}
