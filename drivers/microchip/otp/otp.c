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

/*
 * NOTE: This is really a skeleton right now, using the QSPI as a
 * back-end. Eventually it will be turned into a real OTP driver,
 * possibly using QSPI as a possible (and optional) emulation device.
 * TODO: This needs to be expanded with
 *	 - SD
 *	 - eMMC
 *	 - Real OTP support
 */

#define OTP_MEM_SIZE	1024
#define OTP_SPI_OFFSET	0x1C0000 /* 1536k + 256k */

static bool otp_initialized;
static bool otp_emulation;

static void otp_init()
{
	uint8_t *spi_src;
	uint32_t crc1, crc2;

	if (otp_initialized)
		return;

	/* XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX
	 * TODO: This will need to be guarded by the real OTP:
	 * OTP_FLAGS1_ENABLE_OTP_EMU bit must be *zero* to disable.
	 * XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX
	 */

	/* CRC check of flash emulation data */
	spi_src = (uint8_t*) (LAN996X_QSPI0_MMAP + OTP_SPI_OFFSET);
	crc1 = Crc32c(0, spi_src, OTP_MEM_SIZE);
	crc2 = *(uint32_t*)(spi_src + OTP_MEM_SIZE);
	if (crc1 == crc2) {
		INFO("OTP flash emulation active\n");
		otp_emulation = true;
	} else {
		INFO("OTP flash emulation *disabled*\n");
	}
}

int otp_read_bits(uint8_t *dst, unsigned int offset, unsigned int nbits)
{
	uint8_t *spi_src, bits;
	int i, len;

	assert(nbits > 0);
	assert((nbits % 8) == 0);
	assert((offset % 8) == 0);
	assert((offset + nbits) < (OTP_MEM_SIZE*8));

	otp_init();

	if (!otp_emulation)
		return -EIO;

	offset /= 8;
	len = nbits / 8;

	spi_src = (uint8_t*) (LAN996X_QSPI0_MMAP + OTP_SPI_OFFSET + offset);

	/* Copy data from SPI */
	memcpy(dst, spi_src, len);

	/* Check if any bits cleared */
	for (bits = 0, i = 0; i < len; i++)
		bits |= ~dst[i];

	/* Was a bit set? */
	return bits ? nbits : -EIO;
}

int otp_read_uint32(uint32_t *dst, unsigned int offset)
{
	return otp_read_bits((uint8_t *)dst, offset, 32);
}
