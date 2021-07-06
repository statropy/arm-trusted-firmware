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
#include "otp_emu.h"
#include "plat_otp.h"

enum {
	OTP_FLAG_INITIALIZED = BIT(0),
	OTP_FLAG_EMULATION   = BIT(1),
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

static int otp_hw_execute_status(void)
{
	int rc = otp_hw_execute();
	if (rc == 0) {
		uint32_t pass = mmio_read_32(OTP_OTP_PASS_FAIL(reg_base));
		VERBOSE("OTP exec Status: %0x\n", pass);
		assert(pass & (OTP_OTP_PASS_FAIL_OTP_PASS_M | OTP_OTP_PASS_FAIL_OTP_FAIL_M));
		rc = (pass & OTP_OTP_PASS_FAIL_OTP_PASS(1)) ? 0 : -EFAULT;
	}
	return rc;
}

static void otp_hw_blankcheck(void)
{
	int rc;

	otp_hw_power(true);
	mmio_write_32(OTP_OTP_TEST_CMD(reg_base), OTP_OTP_TEST_CMD_OTP_BLANKCHECK(1));
	mmio_write_32(OTP_OTP_CMD_GO(reg_base), OTP_OTP_CMD_GO_OTP_GO(1));
	rc = otp_hw_execute_status();
	if (rc == 0) {
		INFO("OTP blank check: %s\n", rc ? "blank" : "non-blank");
	}
	otp_hw_power(false);
}

static void otp_hw_reset(void)
{
	int rc;

	otp_hw_power(true);
	mmio_write_32(OTP_OTP_FUNC_CMD(reg_base), OTP_OTP_FUNC_CMD_OTP_RESET(1));
	mmio_write_32(OTP_OTP_CMD_GO(reg_base), OTP_OTP_CMD_GO_OTP_GO(1));
	rc = otp_hw_execute();
	if (rc == 0) {
		INFO("OTP reset: done\n");
	}
	otp_hw_power(false);
}

static void otp_hw_set_address(unsigned int offset)
{
	assert(offset < OTP_MEM_SIZE);
	offset *= 8;		/* HW Address is in bits */
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
		*dst = mmio_read_32(OTP_OTP_RD_DATA(reg_base));
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

static void otp_hw_init(void)
{
	otp_hw_reset();
	otp_hw_blankcheck();
}

static void otp_init(void)
{
	uint32_t flags1 = 0;	/* OTP may be empty/flags not set */

	if (otp_flags & OTP_FLAG_INITIALIZED)
		return;

	otp_hw_init();

	/* HW now initialized */
	otp_flags |= OTP_FLAG_INITIALIZED;

	/* Note: Now read the "flags1" element, to decide whether OTP
	 * emulation has been disabled.
	 */
	otp_hw_read_bytes(OTP_FLAGS1_ADDR, 4, (uint8_t*)&flags1);
	if (flags1 & BIT(OTP_OTP_FLAGS1_DISABLE_OTP_EMU_OFF)) {
		NOTICE("OTP emulation disabled (by OTP)\n");
		flags1 = 0;	/* Don't leak data */
	} else {
		if (otp_emu_init())
			otp_flags |= OTP_FLAG_EMULATION;
	}
}

int otp_read_bytes(unsigned int offset, unsigned int nbytes, uint8_t *dst)
{
	int rc;

	assert(nbytes > 0);
	assert((offset + nbytes) < OTP_MEM_SIZE);

	/* Make sure we're initialized */
	otp_init();

	/* Read bitstream */
	rc = otp_hw_read_bytes(offset, nbytes, dst);

	/* If we read data, possibly or in emulation data */
	if (rc >= 0 && otp_flags & OTP_FLAG_EMULATION)
		otp_emu_add_bytes(offset, nbytes, dst);

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

	/* Make sure we're initialized */
	otp_init();

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

int otp_commit_emulation(void)
{
	int rc = -ENODEV, i;

	if (otp_flags & OTP_FLAG_EMULATION) {
		otp_hw_power(true);
		for (i = 0; i < OTP_MEM_SIZE; i++) {
			uint8_t eb = 0, ob, nb;
			otp_emu_add_bytes(i, 1, &eb);
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
