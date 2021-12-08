/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/io/io_storage.h>
#include <fw_config.h>
#include <plat/common/platform.h>
#include <stddef.h>

int lan966x_load_fw_config_raw(unsigned int image_id)
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

	result = io_read(image_handle, (uintptr_t)&lan966x_fw_config,
			 sizeof(lan966x_fw_config), &bytes_read);
	if (result != 0)
		WARN("Failed to read data (%i)\n", result);

	io_close(image_handle);

#ifdef IMAGE_BL1
	/* This is fwd to BL2 */
	flush_dcache_range((uintptr_t)&lan966x_fw_config, sizeof(lan966x_fw_config));
#endif

	return result;
}

int lan966x_load_fw_config(unsigned int image_id)
{
	uint8_t config[FW_CONFIG_MAX_DATA];
	int result = 0;

	/* Save the current config */
	memcpy(config, lan966x_fw_config.config, sizeof(config));

	/* Make sure OTP emu is initialized */
	otp_emu_init();

	/* If OTP emu active, read fw_config raw/unauthenticated */
	if (otp_in_emulation()) {
		result = lan966x_load_fw_config_raw(image_id);
	}

#ifdef IMAGE_BL1
	/* Only authenticate at BL1 */
	{
		image_desc_t *desc = bl1_plat_get_image_desc(image_id);

		if (desc == NULL) {
			result = -ENOENT;
		} else {
			result = load_auth_image(image_id, &desc->image_info);
			if (result != 0)
				ERROR("FW_CONFIG did not authenticate: rc %d\n", result);
		}
	}
#endif

	/* If something went wrong, restore fw_config.config, zero OTP emu */
	if (result != 0) {
		memset(&lan966x_fw_config, 0, sizeof(lan966x_fw_config));
		memcpy(lan966x_fw_config.config, config, sizeof(config));
	}

	return result;
}

static int fw_config_read_bytes(unsigned int offset,
				unsigned int num_bytes,
				uint8_t *dst)
{
	int ret_val = -1;
	int cnt;
	uint8_t data;

	assert(num_bytes > 0);
	assert((offset + num_bytes) < (sizeof(lan966x_fw_config.config)));

	if (offset < LAN966X_FW_CONF_NUM_OF_ITEMS) {
		for (cnt = 0; cnt < num_bytes; cnt++) {
			data = lan966x_fw_config.config[offset + cnt];
			*dst++ = data;
		}

		ret_val = 0;
	} else {
		ERROR("Illegal offset access to fw_config structure\n");
	}

	return ret_val;
}

int lan966x_fw_config_read_uint8(unsigned int offset, uint8_t *dst)
{
	return fw_config_read_bytes(offset, 1, (uint8_t *)dst);
}

int lan966x_fw_config_read_uint16(unsigned int offset, uint16_t *dst)
{
	return fw_config_read_bytes(offset, 2, (uint8_t *)dst);
}

int lan966x_fw_config_read_uint32(unsigned int offset, uint32_t *dst)
{
	return fw_config_read_bytes(offset, 4, (uint8_t *)dst);
}
