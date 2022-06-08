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

#define SJTAG_NREGS_KEY		(LAN966X_KEY32_LEN / 4)

#if defined(IMAGE_BL1) || defined(IMAGE_BL2)
void lan966x_sjtag_configure(void)
{
	uint32_t w;
	int mode;

	w = mmio_read_32(SJTAG_CTL(LAN966X_SJTAG_BASE));
	mode = SJTAG_CTL_SJTAG_MODE_X(w);

#if defined(IMAGE_BL1) || BL2_AT_EL3
	/* Initial configuration */
	if (mode == LAN966X_SJTAG_MODE1 || mode == LAN966X_SJTAG_MODE2) {
		lan966x_key32_t sjtag_nonce, sjtag_ssk;
		int i;

		/* Secure mode, initialize challenge registers */
		INFO("SJTAG: Secure mode %d\n", mode);

		/* Generate nonce */
		for (i = 0; i < SJTAG_NREGS_KEY; i++) {
			sjtag_nonce.w[i] = lan966x_trng_read();
			mmio_write_32(SJTAG_NONCE(LAN966X_SJTAG_BASE, i), sjtag_nonce.w[i]);
		}

		/* Read SSK */
		otp_read_otp_sjtag_ssk(sjtag_ssk.b, sizeof(sjtag_ssk.b));

		/* Generate digest with nonce and ssk, write it */
		i = lan966x_derive_key(&sjtag_nonce, &sjtag_ssk, &sjtag_ssk);
		assert(i == 0);
		for (i = 0; i < SJTAG_NREGS_KEY; i++)
			mmio_write_32(SJTAG_DEVICE_DIGEST(LAN966X_SJTAG_BASE, i), sjtag_ssk.w[i]);

		/* Now enable */
		mmio_setbits_32(SJTAG_CTL(LAN966X_SJTAG_BASE),
				SJTAG_CTL_SJTAG_DIGEST_CMP(1) | SJTAG_CTL_SJTAG_EN(1));

		/* Don't leak data */
		memset(&sjtag_nonce, 0, sizeof(sjtag_nonce));
		memset(&sjtag_ssk,   0, sizeof(sjtag_ssk));
	}
#endif

#if defined(IMAGE_BL2)
	if (mode == LAN966X_SJTAG_MODE1 || mode == LAN966X_SJTAG_MODE2) {
		INFO("SJTAG: Freeze mode NOT enabled (Linux unlock can be used)\n");
		//mmio_setbits_32(SJTAG_CTL(LAN966X_SJTAG_BASE), SJTAG_CTL_SJTAG_FREEZE(1));
	}
#endif
}
#endif /* defined(IMAGE_BL1) || defined(IMAGE_BL2) */

int lan966x_sjtag_read_challenge(lan966x_key32_t *k)
{
	int i;

	if (!(mmio_read_32(SJTAG_CTL(LAN966X_SJTAG_BASE)) & SJTAG_CTL_SJTAG_EN(1)))
		return -1;

	for (i = 0; i < SJTAG_NREGS_KEY; i++) {
		uint32_t w = mmio_read_32(SJTAG_NONCE(LAN966X_SJTAG_BASE, i));
		k->w[i] = w;
	}

	return 0;
}

int lan966x_sjtag_write_response(const lan966x_key32_t *k)
{
	uint32_t diff = 0;
	int i;

	if (!(mmio_read_32(SJTAG_CTL(LAN966X_SJTAG_BASE)) & SJTAG_CTL_SJTAG_EN(1)))
		return -1;

	for (i = 0; i < SJTAG_NREGS_KEY; i++) {
		uint32_t w = mmio_read_32(SJTAG_DEVICE_DIGEST(LAN966X_SJTAG_BASE, i));
		diff |= (k->w[i] ^ w);
	}

	if (diff == 0) {
		mmio_clrsetbits_32(SJTAG_CTL(LAN966X_SJTAG_BASE),
				   SJTAG_CTL_SJTAG_MODE_M,
				   SJTAG_CTL_SJTAG_MODE(LAN966X_SJTAG_OPEN));
		mmio_setbits_32(SJTAG_CTL(LAN966X_SJTAG_BASE),
				SJTAG_CTL_SJTAG_CPU_EN(1) | SJTAG_CTL_SJTAG_TST_EN(1));
	}

	return diff;
}
