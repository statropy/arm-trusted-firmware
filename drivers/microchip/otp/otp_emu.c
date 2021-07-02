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

#define OTP_SPI_OFFSET	0x1C0000 /* 1536k + 256k */

bool otp_emu_init(void)
{
	uint8_t *spi_src;
	uint32_t crc1, crc2;
	bool active = false;

	/* CRC check of flash emulation data */
	spi_src = (uint8_t*) (LAN996X_QSPI0_MMAP + OTP_SPI_OFFSET);
	crc1 = Crc32c(0, spi_src, OTP_MEM_SIZE);
	crc2 = *(uint32_t*)(spi_src + OTP_MEM_SIZE);
	if (crc1 == crc2) {
		INFO("OTP flash emulation active\n");
		active = true;
	} else {
		INFO("OTP flash emulation *disabled*\n");
	}

	return active;
}

void otp_emu_add_bits(uint8_t *dst, unsigned int offset, unsigned int nbits)
{
	uint8_t *spi_src;
	int i, len;

	offset /= 8;
	len = nbits / 8;

	spi_src = (uint8_t*) (LAN996X_QSPI0_MMAP + OTP_SPI_OFFSET + offset);

	/* Or in data from SPI */
	for (i = 0; i < len; i++)
		dst[i] |= spi_src[i];
}
