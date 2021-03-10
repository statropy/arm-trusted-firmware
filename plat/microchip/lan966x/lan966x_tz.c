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

static void setup_tzaesb(uintptr_t tzaesb)
{
	/* KEY REGS */
	mmio_write_32(tzaesb + TZAESBNS_TZAESB_MR,
		      TZAESBNS_TZAESB_MR_OPMOD(4) | /* CTR mode */
		      TZAESBNS_TZAESB_MR_DUALBUFF(1) |
		      TZAESBNS_TZAESB_MR_AAHB(1));
	mmio_write_32(tzaesb + TZAESBNS_TZAESB_KEYWR0, 0x09cf4f3c);
	mmio_write_32(tzaesb + TZAESBNS_TZAESB_KEYWR1, 0xabf71588);
	mmio_write_32(tzaesb + TZAESBNS_TZAESB_KEYWR2, 0x28aed2a6);
	mmio_write_32(tzaesb + TZAESBNS_TZAESB_KEYWR3, 0x2b7e1516);

	/* IV REGS */
	mmio_write_32(tzaesb + TZAESBNS_TZAESB_IVR0, 0xfcfdfeff);
	mmio_write_32(tzaesb + TZAESBNS_TZAESB_IVR1, 0xf8f9fafb);
	mmio_write_32(tzaesb + TZAESBNS_TZAESB_IVR2, 0xf4f5f6f7);
	mmio_write_32(tzaesb + TZAESBNS_TZAESB_IVR3, 0xf0f1f2f3);
}

static void tzaes_asc_region_enable(uintptr_t tzaes_asc,
				    int region, bool ns,
				    uintptr_t start, uintptr_t end)
{
	uint32_t val, mask = BIT(region);
	int i;

	/* Region - start/end */
	mmio_write_32(tzaes_asc + AESB_ASC_TZAESBASC_RBAR0 + (region * 8), start);
	mmio_write_32(tzaes_asc + AESB_ASC_TZAESBASC_RTAR0 + (region * 8), end - 1);

	/* One non-secure region */
	if (ns) {
		mmio_setbits_32(tzaes_asc + AESB_ASC_TZAESBASC_RSECR,
				AESB_ASC_TZAESBASC_RSECR_SECx(mask));
	}

	/* Enable the regions */
	mmio_setbits_32(tzaes_asc + AESB_ASC_TZAESBASC_RER,
			AESB_ASC_TZAESBASC_RER_ENx(mask));

	/* Check - Read Error Status of regions */
	val = mmio_read_32(tzaes_asc + AESB_ASC_TZAESBASC_RESR);
	if (val & mask)
		NOTICE("RESR error %04x\n", val);
	assert((val & mask) == 0);

	/* Wait for Regions Enabled */
	for (i = 0; i < 10 ; i++) {
		val = mmio_read_32(tzaes_asc + AESB_ASC_TZAESBASC_RSR);
		if ((AESB_ASC_TZAESBASC_RSR_ESx_X(val)) & mask)
			break;
		udelay(1);
	}
	if (!((AESB_ASC_TZAESBASC_RSR_ESx_X(val)) & mask))
		WARN("RSR_ESx is %04x, expected %04x\n", val, mask);
	assert(val & mask);

	NOTICE("TrustZone region %d set up - %s\n", region, ns ? "NS" : "S");
}

static void setup_tzaes_asc(void)
{
	uintptr_t tzaes_asc = LAN966X_AESB_ASC_BASE;

	/*
	 * Configure two regions of TZAESBASC with DDR address ranges
	 */
	tzaes_asc_region_enable(tzaes_asc, 0, false,
				BL32_BASE,
				BL32_LIMIT);

	tzaes_asc_region_enable(tzaes_asc, 1, true,
				PLAT_LAN966X_NS_IMAGE_BASE,
				PLAT_LAN966X_NS_IMAGE_LIMIT);
}

void lan966x_tz_init(void)
{
	uintptr_t tzbase = LAN966X_TZPM_BASE;

	INFO("Configuring TrustZone\n");

	/*
	 * Enable TZPM for NS transactions, Otherwise all are treated
	 * as Secure transactions in CPU subsystem
	 */
	mmio_write_32(tzbase + TZPM_TZPM_EN,
		      TZPM_TZPM_EN_TZPM_EN(1));

	/* TZASC controller */
	setup_tzaes_asc();

	/* NS setup */
	setup_tzaesb(LAN966X_TZAESBNS_BASE);

	/* S setup */
	setup_tzaesb(LAN966X_TZAESBS_BASE);

	INFO("Initialized TrustZone Controller\n");
}
