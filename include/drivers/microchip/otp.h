/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TFA_OTP_H
#define TFA_OTP_H

#include <stdint.h>

#define OTP_MEM_SIZE	1024

int otp_read_bits(uint8_t *dst, unsigned int offset, unsigned int nbits);
int otp_read_uint32(uint32_t *dst, unsigned int offset);

#endif  /* TFA_OTP_H */
