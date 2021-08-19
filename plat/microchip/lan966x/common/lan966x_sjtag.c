/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>

#include "lan966x_regs.h"
#include "lan966x_private.h"
#include "plat_otp.h"

#define LAN966X_SJTAG_OPEN	0
#define LAN966X_SJTAG_MODE1	1
#define LAN966X_SJTAG_MODE2	2
#define LAN966X_SJTAG_CLOSED	3

#define SJTAG_NREGS_KEY		8
#define SJTAG_NREGS_UUID	3

void lan966x_sjtag_configure(void)
{
	uint32_t w;
	int mode;

	w = mmio_read_32(SJTAG_CTL(LAN966X_SJTAG_BASE));
	mode = SJTAG_CTL_SJTAG_MODE_X(w);

#if defined(IMAGE_BL1) || defined(BL2_AT_EL3)
	/* Initial configuration */
	if (mode == LAN966X_SJTAG_MODE1 || mode == LAN966X_SJTAG_MODE2) {
		uint32_t sjtag_nonce[SJTAG_NREGS_KEY];
		uint32_t sjtag_ssk[SJTAG_NREGS_KEY];
		uint32_t sjtag_uuid[SJTAG_NREGS_UUID];
		int i;

		/* Secure mode, initialize challenge registers */
		INFO("SJTAG: Secure mode %d\n", mode);

		/* Generate nonce */
		for (i = 0; i < SJTAG_NREGS_KEY; i++) {
			sjtag_nonce[i] = lan966x_trng_read();
			mmio_write_32(SJTAG_NONCE(LAN966X_SJTAG_BASE, i), sjtag_nonce[i]);
		}

		/* Read SSK */
		otp_read_otp_sjtag_ssk((uint8_t *) sjtag_ssk, sizeof(sjtag_ssk));

		/* Generate digest with nonce and ssk, write it */
		i = lan966x_derive_key(sjtag_nonce, sjtag_ssk, sjtag_ssk);
		assert(i == 0);
		for (i = 0; i < SJTAG_NREGS_KEY; i++)
			mmio_write_32(SJTAG_DEVICE_DIGEST(LAN966X_SJTAG_BASE, i), sjtag_ssk[i]);

		/* Write UUID */
		otp_read_jtag_uuid((uint8_t *) sjtag_uuid, sizeof(sjtag_uuid));
		for (i = 0; i < SJTAG_NREGS_UUID; i++)
			mmio_write_32(SJTAG_UUID(LAN966X_SJTAG_BASE, i), sjtag_uuid[i]);

		/* Don't leak data */
		memset(sjtag_nonce, 0, sizeof(sjtag_nonce));
		memset(sjtag_ssk,   0, sizeof(sjtag_ssk));
	}
#endif

#if defined(IMAGE_BL2)
	if (mode == LAN966X_SJTAG_MODE1 || mode == LAN966X_SJTAG_MODE2) {
		INFO("SJTAG: Freeze mode enabled\n");
		mmio_setbits_32(SJTAG_CTL(LAN966X_SJTAG_BASE), SJTAG_CTL_SJTAG_FREEZE(1));
	}
#endif
}
