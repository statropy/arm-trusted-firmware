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

uint32_t lan966x_trng_read(void)
{
	/* Wait for data rdy */
	while ((mmio_read_32(LAN966X_TRNG_BASE + TRNG_TRNG_ISR) &
		TRNG_TRNG_ISR_DATRDY_ISR_M) == 0)
		;
	/* then, read the data and return it */
	return mmio_read_32(LAN966X_TRNG_BASE + TRNG_TRNG_ODATA);
}

void lan966x_trng_init(void)
{
	mmio_write_32(LAN966X_TRNG_BASE + TRNG_TRNG_CR,
		      TRNG_TRNG_CR_WAKEY(0x524E47) |
		      TRNG_TRNG_CR_ENABLE(true));
}
