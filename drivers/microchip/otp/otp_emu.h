
/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef OTP_EMU_H
#define OTP_EMU_H

bool otp_emu_init(void);
void otp_emu_add_bits(uint8_t *dst, unsigned int offset, unsigned int nbits);

#endif	/* OTP_EMU_H */
