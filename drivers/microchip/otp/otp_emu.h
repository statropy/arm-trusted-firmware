
/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef OTP_EMU_H
#define OTP_EMU_H

bool otp_emu_init(void);
uint8_t otp_emu_get_byte(unsigned int offset);

#endif	/* OTP_EMU_H */
