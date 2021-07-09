/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <assert.h>
#include <lib/mmio.h>
#include <errno.h>

#include <drivers/microchip/otp.h>
#include <drivers/microchip/qspi.h>

#include "lan966x_regs.h"
#include "lan966x_private.h"

/* Restrict OTP emulation */
#define OTP_EMU_START_OFF	256
#define OTP_EMU_MAX_DATA	512

#define OTP_SPI_OFFSET	0x1C0000 /* 1536k + 256k */

bool otp_emu_init(void)
{
	uint8_t *spi_src;
	uint32_t crc1, crc2;
	bool active = false;

	/* CRC check of flash emulation data */
	spi_src = (uint8_t*) (LAN996X_QSPI0_MMAP + OTP_SPI_OFFSET);
	crc1 = Crc32c(0, spi_src, OTP_EMU_MAX_DATA);
	crc2 = *(uint32_t*)(spi_src + OTP_EMU_MAX_DATA);
	if (crc1 == crc2) {
		INFO("OTP flash emulation active\n");
		active = true;
	} else {
		INFO("OTP flash emulation *disabled*\n");
	}

	return active;
}

uint8_t otp_emu_get_byte(unsigned int offset)
{
	uint8_t *spi_src = (uint8_t*) (LAN996X_QSPI0_MMAP + OTP_SPI_OFFSET);

	/* Only have data for so much */
	if (offset >= OTP_EMU_START_OFF) {
		offset -= OTP_EMU_START_OFF;
		if (offset < OTP_EMU_MAX_DATA)
			return spi_src[offset];
	}

	/* Otherwise zero contribution */
	return 0;
}
