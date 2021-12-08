/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FW_CONFIG_H
#define FW_CONFIG_H

#include <stdint.h>
#include <drivers/microchip/otp.h>

typedef enum {
	LAN966X_FW_CONF_MMC_CLK_RATE	= 0,	// mmc clock frequency	- word access
	LAN966X_FW_CONF_MMC_BUS_WIDTH	= 4,	// mmc bus width	- byte access
	LAN966X_FW_CONF_QSPI_CLK 	= 5,	// qspi clock frequency	- byte access
	LAN966X_FW_CONF_NUM_OF_ITEMS
} lan966x_fw_cfg_data;

#define FW_CONFIG_INIT_8(offset, value)		\
	.config[offset] = (uint8_t) (value)

#define FW_CONFIG_INIT_32(offset, value)				\
	.config[offset + 0] = (uint8_t) (value),			\
	.config[offset + 1] = (uint8_t) ((value) >> 8),			\
	.config[offset + 2] = (uint8_t) ((value) >> 16),		\
	.config[offset + 3] = (uint8_t) ((value) >> 24)

#define OTP_EMU_MAX_DATA	384
#define FW_CONFIG_MAX_DATA	128

typedef struct {
	uint8_t otp_emu_data[OTP_EMU_MAX_DATA];
	uint8_t config[FW_CONFIG_MAX_DATA];
} lan966x_fw_config_t;

extern lan966x_fw_config_t lan966x_fw_config;

int lan966x_load_fw_config(unsigned int image_id);
int lan966x_get_fw_config_data(lan966x_fw_cfg_data id);
int lan966x_fw_config_read_uint8(unsigned int offset, uint8_t *dst);
int lan966x_fw_config_read_uint16(unsigned int offset, uint16_t *dst);
int lan966x_fw_config_read_uint32(unsigned int offset, uint32_t *dst);

#endif /* FW_CONFIG_H */
