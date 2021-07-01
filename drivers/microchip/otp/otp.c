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
	OTP_FLAG_HW_BLANK    = BIT(2),
};

static uint32_t otp_flags;
static uintptr_t reg_base = LAN966X_OTP_BASE;

static bool otp_hw_wait_flag_clear(uintptr_t reg, uint32_t flag)
{
	int i = 100;		/* Wait at most 100 ms */
	while (--i) {
		uint32_t val = mmio_read_32(reg);
		VERBOSE("Wait reg 0x%lx for clr %08x: Have %08x iter %d\n",
			(reg - reg_base) / 4, flag, val, i);
		if (!(val & flag))
			return true;
		mdelay(1);
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
	if (!rc) {
		uint32_t pass = mmio_read_32(OTP_OTP_PASS_FAIL(reg_base));
		VERBOSE("OTP exec Status: %0x\n", pass);
		assert(pass & (OTP_OTP_PASS_FAIL_OTP_PASS_M | OTP_OTP_PASS_FAIL_OTP_FAIL_M));
		rc = !!(pass & OTP_OTP_PASS_FAIL_OTP_PASS(1));
	}
	return rc;
}

static int otp_hw_blankcheck(void)
{
	int rc;

	otp_hw_power(true);
	mmio_write_32(OTP_OTP_TEST_CMD(reg_base), OTP_OTP_TEST_CMD_OTP_BLANKCHECK(1));
	mmio_write_32(OTP_OTP_CMD_GO(reg_base), OTP_OTP_CMD_GO_OTP_GO(1));
	rc = otp_hw_execute_status();
	if (rc >= 0) {
		INFO("OTP blank check: %s\n", rc ? "blank" : "non-blank");
		if (rc == 0)
			otp_flags |= OTP_FLAG_HW_BLANK;
	}
	otp_hw_power(false);

	return rc;
}

static int otp_hw_reset(void)
{
	int rc;

	otp_hw_power(true);
	mmio_write_32(OTP_OTP_FUNC_CMD(reg_base), OTP_OTP_FUNC_CMD_OTP_RESET(1));
	mmio_write_32(OTP_OTP_CMD_GO(reg_base), OTP_OTP_CMD_GO_OTP_GO(1));
	rc = otp_hw_execute();
	if (rc >= 0) {
		INFO("OTP reset: done\n");
	}
	otp_hw_power(false);

	return rc;
}

static void otp_hw_set_address(unsigned int offset)
{
	mmio_write_32(OTP_OTP_ADDR_HI(reg_base), 0xff & (offset >> 8));
	mmio_write_32(OTP_OTP_ADDR_LO(reg_base), 0xff & offset);
}

static int otp_hw_read_byte(uint8_t *dst, unsigned int offset)
{
	int rc;

	otp_hw_set_address(offset);
	mmio_write_32(OTP_OTP_FUNC_CMD(reg_base), OTP_OTP_FUNC_CMD_OTP_READ(1));
	mmio_write_32(OTP_OTP_CMD_GO(reg_base), OTP_OTP_CMD_GO_OTP_GO(1));
	rc = otp_hw_execute();
	if (rc >= 0) {
		uint32_t pass = mmio_read_32(OTP_OTP_PASS_FAIL(reg_base));
		if (pass & OTP_OTP_PASS_FAIL_OTP_READ_PROHIBITED(1))
			return -EACCES;
		*dst = mmio_read_32(OTP_OTP_RD_DATA(reg_base));
		VERBOSE("OTP read %02x @ %d\n", *dst, offset);
	}
	return rc;
}

static int otp_hw_read_bits(uint8_t *dst, unsigned int offset, unsigned int nbits)
{
	uint8_t data, bits;
	int i, rc = 0, len = nbits / 8;

	if (otp_flags & OTP_FLAG_HW_BLANK)
		return -EIO;	/* Bail out early if not programmed */

	otp_hw_power(true);
	for (bits = 0, i = 0; i < len; i++, offset += 8) {
		rc = otp_hw_read_byte(&data, offset);
		if (rc < 0)
			break;
		bits |= data;
		*dst++ = data;
	}
	otp_hw_power(false);

	if (rc < 0)
		return rc;

	return bits ? 0 : -EIO;	/* Signal if "blank" read */
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
	otp_hw_read_bits((uint8_t*)&flags1, OTP_FLAGS1_ADDR * 8, 32);
	if (flags1 & BIT(OTP_OTP_FLAGS1_DISABLE_OTP_EMU_OFF)) {
		NOTICE("OTP emulation disabled (by OTP)\n");
	} else {
		if (otp_emu_init())
			otp_flags |= OTP_FLAG_EMULATION;
	}
}

int otp_read_bits(uint8_t *dst, unsigned int offset, unsigned int nbits)
{
	assert(nbits > 0);
	assert((nbits % 8) == 0);
	assert((offset % 8) == 0);
	assert((offset + nbits) < (OTP_MEM_SIZE*8));

	/* Make sure we're initialized */
	otp_init();

	return (otp_flags & OTP_FLAG_EMULATION) ?
		otp_emu_read_bits(dst, offset, nbits) :
		otp_hw_read_bits(dst, offset, nbits);
}

int otp_read_uint32(uint32_t *dst, unsigned int offset)
{
	return otp_read_bits((uint8_t *)dst, offset, 32);
}
