/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <common/fdt_wrappers.h>
#include <drivers/io/io_storage.h>
#include <drivers/microchip/qspi.h>
#include <drivers/mmc.h>
#include <fw_config.h>
#include <lan96xx_common.h>
#include <lan96xx_mmc.h>
#include <libfdt.h>
#include <plat/common/platform.h>
#include <stddef.h>

static bool fdt_valid;

void *lan966x_get_dt(void)
{
	return fdt_valid ? lan966x_fw_config.fdt_buf : NULL;
}

static int lan966x_fw_config_parse_dt(void *fdt)
{
	const fdt32_t *off;
	int node, otp_node;

	node = fdt_path_offset(fdt, "/otp");
	if (node < 0) {
		VERBOSE("No OTP emulation data found\n");
		return -ENOENT;
	}

	fdt_for_each_subnode(otp_node, fdt, node) {
		const char *name = fdt_get_name(fdt, otp_node, NULL);
		int otp_offset, otp_len;
		const void *data;

		off = fdt_getprop(fdt, otp_node, "reg", NULL);
		if (off == NULL) {
			WARN("OTP emu: Skip node %s, no 'reg' property\n", name);
			continue;
		}

		otp_offset = fdt32_to_cpu(*off);

		data = fdt_getprop(fdt, otp_node, "data", &otp_len);
		if (data) {
			if (otp_offset >= 0 &&
			    (otp_len > 0 && otp_len <= OTP_EMU_MAX_DATA) &&
			    (otp_offset + otp_len) <= OTP_EMU_MAX_DATA) {
				/* Copy emulation data */
				VERBOSE("OTP emu: Copy %s: Off %d Len %d\n",
					name, otp_offset, otp_len);
				memcpy(lan966x_fw_config.otp_emu_data + otp_offset,
				       data, otp_len);
			} else {
				WARN("OTP emu: Skip malformed node %s: Off %d Len %d\n",
				     name, otp_offset, otp_len);
			}

		} else {
			WARN("OTP emu: OTP emulation at offset %d has no data\n", otp_offset);
		}
	}

	return 0;
}

int lan966x_load_fw_config(unsigned int image_id)
{
	int result = -ENOTSUP;
#if !defined(IMAGE_BL2U)
	image_info_t config_image_info = {
		.h.type = (uint8_t)PARAM_IMAGE_BINARY,
		.h.version = (uint8_t)VERSION_2,
		.h.size = (uint16_t)sizeof(image_info_t),
		.h.attr = 0
	};

	config_image_info.image_base = (uintptr_t) lan966x_fw_config.fdt_buf;
	config_image_info.image_max_size = sizeof(lan966x_fw_config.fdt_buf);

	result = load_auth_image(image_id, &config_image_info);
	if (result != 0) {
		ERROR("FW_CONFIG did not authenticate: rc %d\n", result);
	} else {
		fdt_valid = true;
	}
#endif

	/* Parse OTP emulation DT info */
	if (result == 0) {
		result = lan966x_fw_config_parse_dt(lan966x_fw_config.fdt_buf);
	}

	return result;
}

void lan966x_fwconfig_apply(void)
{
	boot_source_type boot_source = lan966x_get_boot_source();

	/* Update storage drivers with new values from fw_config */
	switch (boot_source) {
	case BOOT_SOURCE_QSPI:
		qspi_reinit();
		break;
	case BOOT_SOURCE_SDMMC:
	case BOOT_SOURCE_EMMC:
		lan966x_mmc_plat_config(boot_source);
		break;
	default:
		break;
	}
}

static int lan966x_fdt_get_prop(void *fdt,
				const char *compatible,
				const char *property,
				uint32_t *val)
{
	int node, err;

	node = fdt_node_offset_by_compatible(fdt, -1, compatible);
	if (node < 0) {
		WARN("fw_config: No %s device in DT\n", compatible);
		return -ENOENT;
	}

	err = fdt_read_uint32(fdt, node, property, val);
	if (err != 0) {
		WARN("fw_config: No %s property in %s DT node\n", property, compatible);
	}

	return err;
}

int lan966x_fw_config_get_prop(void *fdt, unsigned int offset, uint32_t *dst)
{
	uint32_t tmp;
	int err;

	if (!fdt_valid) {
		/* This occur at initial startup or if no DT is in FIP */
		return -EINVAL;
	}

	switch (offset) {
	case LAN966X_FW_CONF_MMC_CLK_RATE:
		err = lan966x_fdt_get_prop(fdt, "microchip,lan966x-sdhci", "clocks", &tmp);
		if (err == 0) {
			int node = fdt_node_offset_by_phandle(fdt, tmp);

			if (node < 0) {
				WARN("Cannot get SDHCI clock\n");
				return node;
			}
			/* Now get the clock freq */
			err = fdt_read_uint32(fdt, node, "clock-frequency", dst);
		}
		break;
	case LAN966X_FW_CONF_MMC_BUS_WIDTH:
		err = lan966x_fdt_get_prop(fdt, "microchip,lan966x-sdhci", "bus-width", &tmp);
		if (err == 0) {
			switch (tmp) {
			case 8:
				*dst = MMC_BUS_WIDTH_8;
				break;
			case 4:
				*dst = MMC_BUS_WIDTH_4;
				break;
			case 1:
				*dst = MMC_BUS_WIDTH_1;
				break;
			default:
				WARN("fw_config: Illegal bus-width: %d\n", tmp);
				err = -EINVAL;
			}
		}
		break;
	case LAN966X_FW_CONF_QSPI_CLK:
		err = lan966x_fdt_get_prop(fdt, "jedec,spi-nor", "spi-max-frequency", dst);
		if (err == 0)
			*dst /= 1000000; /* To Mhz... */
		break;
	default:
		WARN("fw_config: No config at off %d\n", offset);
		err = -ENOTSUP;
	}

	if (err == 0)
		VERBOSE("fw_config: %d property = %u\n", offset, *dst);
	else
		WARN("fw_config: %d property FAIL: %d\n", offset, err);

	return err;
}

void lan966x_fw_config_read_uint8(unsigned int offset, uint8_t *dst, uint8_t defval)
{
	int ret;
	uint32_t val = 0;

	ret = lan966x_fw_config_get_prop(lan966x_fw_config.fdt_buf, offset, &val);
	if (ret == 0)
		*dst = val;
	else
		*dst = defval;
}

void lan966x_fw_config_read_uint16(unsigned int offset, uint16_t *dst, uint16_t defval)
{
	int ret;
	uint32_t val = 0;

	ret = lan966x_fw_config_get_prop(lan966x_fw_config.fdt_buf, offset, &val);
	if (ret == 0)
		*dst = val;
	else
		*dst = defval;
}

void lan966x_fw_config_read_uint32(unsigned int offset, uint32_t *dst, uint32_t defval)
{
	if (lan966x_fw_config_get_prop(lan966x_fw_config.fdt_buf, offset, dst))
		*dst = defval;
}
