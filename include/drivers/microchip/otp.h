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

#define OTP_MEM_SIZE	8192

void otp_emu_init(void);
int otp_read_bytes(unsigned int offset, unsigned int nbytes, uint8_t *dst);
int otp_write_bytes(unsigned int offset, unsigned int nbytes, const uint8_t *src);
int otp_read_uint32(unsigned int offset, uint32_t *dst);
int otp_write_uint32(unsigned int offset, uint32_t w);
int otp_read_bytes_raw(unsigned int offset, unsigned int nbytes, uint8_t *dst);

bool otp_in_emulation(void);
int otp_commit_emulation(void);
int otp_write_regions(void);

bool otp_all_zero(const uint8_t *p, size_t nbytes);

#endif  /* TFA_OTP_H */
