/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <drivers/delay_timer.h>
#include <drivers/microchip/otp.h>
#include <errno.h>
#include <lib/mmio.h>
#include <string.h>

#include "lan966x_regs.h"
#include "lan966x_private.h"
#include "plat_otp.h"

/* Restrict OTP emulation */
#define OTP_EMU_START_OFF	256

enum {
	OTP_FLAG_EMULATION   = BIT(0),
};

static uint32_t otp_flags;
static uintptr_t reg_base = LAN966X_OTP_BASE;

static bool otp_hw_wait_flag_clear(uintptr_t reg, uint32_t flag)
{
	int i = 500 * 1000;		/* Wait at most 500 ms*/
	while (i--) {
		uint32_t val = mmio_read_32(reg);
		VERBOSE("Wait reg 0x%lx for clr %08x: Have %08x iter %d\n",
			(reg - reg_base) / 4, flag, val, i);
		if (!(val & flag))
			return true;
		udelay(1);
	}
	return false;
}

static void otp_hw_power(bool up)
{
	if (up) {
		mmio_clrbits_32(OTP_OTP_PWR_DN(reg_base), OTP_OTP_PWR_DN_OTP_PWRDN_N(1));
		if (!otp_hw_wait_flag_clear(OTP_OTP_STATUS(reg_base), OTP_OTP_STATUS_OTP_CPUMPEN(1))) {
			ERROR("OTP: CPUMPEN did not clear\n");
		}
	} else {
		mmio_setbits_32(OTP_OTP_PWR_DN(reg_base), OTP_OTP_PWR_DN_OTP_PWRDN_N(1));
	}
}

static int otp_hw_execute(void)
{
	if (!otp_hw_wait_flag_clear(OTP_OTP_CMD_GO(reg_base), OTP_OTP_CMD_GO_OTP_GO(1))) {
		ERROR("OTP: OTP_GO did not clear\n");
		return -ETIMEDOUT;
	}
	if (!otp_hw_wait_flag_clear(OTP_OTP_STATUS(reg_base), OTP_OTP_STATUS_OTP_BUSY(1))) {
		ERROR("OTP: OTP_BUSY did not clear\n");
		return -ETIMEDOUT;
	}
	return 0;
}

static void otp_hw_set_address(unsigned int offset)
{
	assert(offset < OTP_MEM_SIZE);
	mmio_write_32(OTP_OTP_ADDR_HI(reg_base), 0xff & (offset >> 8));
	mmio_write_32(OTP_OTP_ADDR_LO(reg_base), 0xff & offset);
}

static int otp_hw_read_byte(unsigned int offset, uint8_t *dst)
{
	int rc;

	otp_hw_set_address(offset);
	mmio_write_32(OTP_OTP_FUNC_CMD(reg_base), OTP_OTP_FUNC_CMD_OTP_READ(1));
	mmio_write_32(OTP_OTP_CMD_GO(reg_base), OTP_OTP_CMD_GO_OTP_GO(1));
	rc = otp_hw_execute();
	if (rc == 0) {
		uint32_t pass = mmio_read_32(OTP_OTP_PASS_FAIL(reg_base));
		if (pass & OTP_OTP_PASS_FAIL_OTP_READ_PROHIBITED(1))
			return -EACCES;
		*dst = (uint8_t) mmio_read_32(OTP_OTP_RD_DATA(reg_base));
		if (*dst) {
			VERBOSE("OTP read %02x @ %04x\n", *dst, offset);
		}
	}
	return rc;
}

static int otp_hw_read_bytes(unsigned int offset, unsigned int nbytes, uint8_t *dst)
{
	uint8_t data;
	int i, rc = 0;

	otp_hw_power(true);
	for (i = 0; i < nbytes; i++) {
		rc = otp_hw_read_byte(offset + i, &data);
		if (rc < 0)
			break;
		*dst++ = data;
	}
	otp_hw_power(false);

	return rc;
}

static int otp_hw_write_byte(unsigned int offset, uint8_t data)
{
	int rc;

	otp_hw_set_address(offset);
	mmio_write_32(OTP_OTP_PRGM_MODE(reg_base), OTP_OTP_PRGM_MODE_OTP_PGM_MODE_BYTE(1));
	mmio_write_32(OTP_OTP_PRGM_DATA(reg_base), data);
	mmio_write_32(OTP_OTP_FUNC_CMD(reg_base), OTP_OTP_FUNC_CMD_OTP_PROGRAM(1));
	mmio_write_32(OTP_OTP_CMD_GO(reg_base), OTP_OTP_CMD_GO_OTP_GO(1));
	rc = otp_hw_execute();
	if (rc == 0) {
		uint32_t pass = mmio_read_32(OTP_OTP_PASS_FAIL(reg_base));
		if (pass & OTP_OTP_PASS_FAIL_OTP_WRITE_PROHIBITED(1))
			return -EACCES;
		if (pass & OTP_OTP_PASS_FAIL_OTP_FAIL(1))
			return -EIO;
		VERBOSE("OTP wrote %02x @ %04x\n", data, offset);
	}
	return rc;
}

static int otp_hw_write_bytes(unsigned int offset, unsigned int nbytes, const uint8_t *src)
{
	uint8_t data, newdata;
	int i, rc = 0;

	otp_hw_power(true);
	for (i = 0; i < nbytes; i++) {
		/* Skip zero bytes */
		if (src[i]) {
			rc = otp_hw_read_byte(offset + i, &data);
			if (rc < 0)
				break;
			/* Will setting input data change cell? */
			newdata = data | src[i];
			if (newdata != data) {
				rc = otp_hw_write_byte(offset + i, newdata);
				if (rc < 0)
					break;
			}
		}
	}
	otp_hw_power(false);

	return rc;
}

void otp_emu_init(void)
{
	uint8_t rotpk[OTP_TBBR_ROTPK_SIZE];

	/* Note: Now read the "ROTPK" element, to decide whether OTP
	 * emulation has been disabled.
	 */
	otp_hw_read_bytes(OTP_TBBR_ROTPK_ADDR, sizeof(rotpk), rotpk);
	if (otp_all_zero(rotpk, sizeof(rotpk))) {
		otp_flags |= OTP_FLAG_EMULATION;
	} else {
		NOTICE("OTP emulation disabled (by OTP)\n");
		memset(rotpk, 0, sizeof(rotpk)); /* Don't leak data */
	}
}

static uint8_t otp_emu_get_byte(unsigned int offset)
{
	/* Only have data for so much */
	if (offset >= OTP_EMU_START_OFF) {
		offset -= OTP_EMU_START_OFF;
		if (offset < OTP_EMU_MAX_DATA)
			return lan966x_fw_config.otp_emu_data[offset];
	}

	/* Otherwise zero contribution */
	return 0;
}

/*
 * Marking this interface as weak enables BL32 to replace it with an
 * interface that passively serves data from a cached memory buffer.
 */
#pragma weak otp_read_bytes
int otp_read_bytes(unsigned int offset, unsigned int nbytes, uint8_t *dst)
{
	int rc;

	assert(nbytes > 0);
	assert((offset + nbytes) < OTP_MEM_SIZE);

	/* Read bitstream */
	rc = otp_hw_read_bytes(offset, nbytes, dst);

	/* If we read data, possibly or in emulation data */
	if (rc >= 0 && otp_flags & OTP_FLAG_EMULATION) {
		int i;

		for (i = 0; i < nbytes; i++)
			dst[i] |= otp_emu_get_byte(offset + i);
	}

	return rc;
}

int otp_read_uint32(unsigned int offset, uint32_t *dst)
{
	return otp_read_bytes(offset, 4, (uint8_t *)dst);
}

int otp_write_bytes(unsigned int offset, unsigned int nbytes, const uint8_t *dst)
{
	assert(nbytes > 0);
	assert((offset + nbytes) < OTP_MEM_SIZE);

	return otp_hw_write_bytes(offset, nbytes, dst);
}

int otp_write_uint32(unsigned int offset, uint32_t w)
{
	return otp_write_bytes(offset, 4, (const uint8_t *)&w);
}

/* Check to see if all bits are clear */
bool otp_all_zero(const uint8_t *p, size_t nbytes)
{
	int i;

	for (i = 0; i < nbytes; i++)
		if (p[i] != 0)
			return false;

	return true;
}

bool otp_in_emulation(void)
{
	return !!(otp_flags & OTP_FLAG_EMULATION);
}

int otp_commit_emulation(void)
{
	int rc = -ENODEV, i;

	if (otp_flags & OTP_FLAG_EMULATION) {
		otp_hw_power(true);
		for (i = 0; i < OTP_MEM_SIZE; i++) {
			uint8_t eb = 0, ob, nb;
			eb = otp_emu_get_byte(i);
			if (eb) {
				rc = otp_hw_read_byte(i, &ob);
				if (rc != 0)
					break;
				/* Add emulation data */
				nb = (eb | ob);
				/* Update iff new data differs */
				if (nb != ob) {
					rc = otp_hw_write_byte(i, nb);
					if (rc != 0)
						break;
				}
			}
			eb = ob = nb = 0; /* Don't leak */
		}
		otp_hw_power(false);
	}

	return rc;
}

/* Bl2U only - for diagnostics */
int otp_read_bytes_raw(unsigned int offset, unsigned int nbytes, uint8_t *dst)
{
	return otp_hw_read_bytes(offset, nbytes, dst);
}

int otp_write_regions(void)
{
	struct {
		uint16_t begin;
		uint16_t end;
	} regions[] = {
		{ 0x0000, 0x003F },  // Manufacturing data
		{ 0x0040, 0x0043 },  // OTP Write protect
		{ 0x0044, 0x0063 },  // OTP Region Definitions
		{ 0x0064, 0x00FF },  // Insecure configuration without emulation
		{ 0x0100, 0x01FF },  // Keys and other security related
		{ 0x0200, 0x023F },  // Non-volatile secure counters
		{ 0x0240, 0x027F },  // Insecure configuration with emulation
		{ 0x0280, 0x1FFF },  // User-space / Customer usage
	};
	uint32_t ck;

	/* Check if programmed already */
	if (otp_read_uint32(PROTECT_REGION_ADDR_ADDR, &ck) == 0 && ck == 0)
	        return otp_write_bytes(PROTECT_REGION_ADDR_ADDR,
				       sizeof(regions), (uint8_t*) regions);

	/* Writing OTP regions not possible, not blank or protected */
	return -1;
}
