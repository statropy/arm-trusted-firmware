/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TFA_OTP_H
#define TFA_OTP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "platform_def.h"

#if defined(MCHP_SOC_LAN966X)
#define OTP_MEM_SIZE	SIZE_K(8)
#elif defined(MCHP_SOC_LAN969X)
#define OTP_MEM_SIZE	SIZE_K(16)
#define OTP_TRUSTZONE_AWARE
#endif

#if defined(MCHP_OTP_EMULATION)
void otp_emu_init(void);
bool otp_in_emulation(void);
int otp_commit_emulation(void);
#endif
int otp_read_bytes(unsigned int offset, unsigned int nbytes, uint8_t *dst);
int otp_write_bytes(unsigned int offset, unsigned int nbytes, const uint8_t *src);
int otp_read_uint32(unsigned int offset, uint32_t *dst);
int otp_write_uint32(unsigned int offset, uint32_t w);
int otp_read_bytes_raw(unsigned int offset, unsigned int nbytes, uint8_t *dst);

int otp_write_regions(void);

bool otp_all_zero(const uint8_t *p, size_t nbytes);

#endif  /* TFA_OTP_H */
