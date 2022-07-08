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

#if !defined(FW_CONFIG_DT)
#define FW_CONFIG_INIT_8(offset, value)		\
	.config[offset] = (uint8_t) (value)

#define FW_CONFIG_INIT_32(offset, value)				\
	.config[offset + 0] = (uint8_t) (value),			\
	.config[offset + 1] = (uint8_t) ((value) >> 8),			\
	.config[offset + 2] = (uint8_t) ((value) >> 16),		\
	.config[offset + 3] = (uint8_t) ((value) >> 24)

#define FW_CONFIG_MAX_DATA	128
#endif	/* !defined(FW_CONFIG_DT) */

typedef struct {
#if defined(MCHP_OTP_EMULATION)
#define OTP_EMU_MAX_DATA	384
	uint8_t otp_emu_data[OTP_EMU_MAX_DATA];
#endif
#if defined(FW_CONFIG_DT)
#define MAX_FDT SIZE_K(4)
	/* DT based fw_config use actual DT instead */
	uint8_t fdt_buf[MAX_FDT];
#else
	/* simple byte array with offsets */
	uint8_t config[FW_CONFIG_MAX_DATA];
#endif
} lan966x_fw_config_t;

extern lan966x_fw_config_t lan966x_fw_config;

int lan966x_load_fw_config(unsigned int image_id);
int lan966x_get_fw_config_data(lan966x_fw_cfg_data id);
void lan966x_fwconfig_apply(void);
void lan966x_fw_config_read_uint8(unsigned int offset, uint8_t *dst, uint8_t defval);
void lan966x_fw_config_read_uint16(unsigned int offset, uint16_t *dst, uint16_t defval);
void lan966x_fw_config_read_uint32(unsigned int offset, uint32_t *dst, uint32_t defval);

#if defined(FW_CONFIG_DT)
void *lan966x_get_dt(void);
#endif

#endif /* FW_CONFIG_H */
