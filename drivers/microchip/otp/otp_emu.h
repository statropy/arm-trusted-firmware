
/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef OTP_EMU_H
#define OTP_EMU_H

bool otp_emu_init(void);
void otp_emu_add_bytes(unsigned int offset, unsigned int nbytes, uint8_t *dst);

#endif	/* OTP_EMU_H */
