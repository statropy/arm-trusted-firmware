/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <drivers/io/io_storage.h>
#include <drivers/microchip/otp.h>
#include <errno.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>
#include <string.h>

#include "lan966x_regs.h"
#include "lan966x_private.h"

/* Restrict OTP emulation */
#define OTP_EMU_START_OFF	256
#define OTP_EMU_MAX_DATA	512

static uint8_t otp_emu_data[OTP_EMU_MAX_DATA];

/*
 * Note: This is read *wo* authentication, as the OTP emulation data
 * may contain the actual (emulated) rotpk. This is only called when a
 * *hw* ROTPK has *not* been deployed.
 */
int load_otp_data(unsigned int image_id)
{
	uintptr_t dev_handle, image_handle, image_spec = 0;
	size_t bytes_read;
	int result;

	result = plat_get_image_source(image_id, &dev_handle, &image_spec);
	if (result != 0) {
		WARN("Failed to obtain reference to image id=%u (%i)\n",
			image_id, result);
		return result;
	}

	result = io_open(dev_handle, image_spec, &image_handle);
	if (result != 0) {
		WARN("Failed to access image id=%u (%i)\n", image_id, result);
		return result;
	}

	result = io_read(image_handle, (uintptr_t)otp_emu_data,
			 OTP_EMU_MAX_DATA, &bytes_read);
	if (result != 0)
		WARN("Failed to read data (%i)\n", result);
	else {
		if (bytes_read != OTP_EMU_MAX_DATA) {
			WARN("Failed to read data (%zd bytes)\n", bytes_read);
			result = -EIO;
		}
	}

	io_close(image_handle);

	return result;
}

bool otp_emu_init(void)
{
	bool active = false;

	if (load_otp_data(FW_CONFIG_ID) == 0) {
		INFO("OTP flash emulation active\n");
		active = true;
	} else {
		INFO("OTP flash emulation *disabled*\n");
	}

	return active;
}

uint8_t otp_emu_get_byte(unsigned int offset)
{
	/* Only have data for so much */
	if (offset >= OTP_EMU_START_OFF) {
		offset -= OTP_EMU_START_OFF;
		if (offset < OTP_EMU_MAX_DATA)
			return otp_emu_data[offset];
	}

	/* Otherwise zero contribution */
	return 0;
}
