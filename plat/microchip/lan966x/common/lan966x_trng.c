/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/delay_timer.h>
#include <plat/common/plat_trng.h>
#include <lib/mmio.h>

#include "lan966x_regs.h"
#include "lan966x_private.h"

static bool lan966x_trng_init_done;

uuid_t plat_trng_uuid = {
	{0xcf, 0x32, 0x0f, 0x5a},
	{0x0b, 0xec},
	{0x4b, 0xa0},
	0x89, 0x2c,
	{0x41, 0xff, 0xb0, 0x86, 0x4e, 0x69}
};

static void lan966x_trng_init(void)
{
	if (!lan966x_trng_init_done) {
		lan966x_trng_init_done = true;
		mmio_write_32(TRNG_TRNG_CR(LAN966X_TRNG_BASE),
			      TRNG_TRNG_CR_WAKEY(0x524E47) |
			      TRNG_TRNG_CR_ENABLE(true));
	}
}

uint32_t lan966x_trng_read(void)
{
	/* Be sure init is called */
	lan966x_trng_init();
	/* Wait for data rdy */
	while ((mmio_read_32(TRNG_TRNG_ISR(LAN966X_TRNG_BASE)) &
		TRNG_TRNG_ISR_DATRDY_ISR_M) == 0)
		;
	/* then, read the data and return it */
	return mmio_read_32(TRNG_TRNG_ODATA(LAN966X_TRNG_BASE));
}

void plat_entropy_setup(void)
{
	lan966x_trng_init();
}

bool plat_get_entropy(uint64_t *out)
{
	uint64_t entropy;
	entropy = lan966x_trng_read();
	entropy <<= 32;
	entropy |= lan966x_trng_read();
	*out = entropy;
	return true;
}
