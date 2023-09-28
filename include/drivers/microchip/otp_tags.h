/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TFA_OTP_TAGS_H
#define TFA_OTP_TAGS_H

enum otp_tag_type {
	otp_tag_type_invalid,
	otp_tag_type_password,
	otp_tag_type_pcb,
	otp_tag_type_revision,
	otp_tag_type_ethaddr,
	otp_tag_type_ethaddr_count,
	otp_tag_type_fit_config,
};

/*
 * Returns number of bytes read from OTP - or < 0 if error occurred.
 */
int otp_tag_get(enum otp_tag_type tag, void *buf, size_t buf_size);

static inline int otp_tag_get_string(enum otp_tag_type tag, char *buf, size_t buf_size)
{
	int ret = otp_tag_get(tag, buf, buf_size - 1);
	if (ret >= 0) {
		/* ASCIIZ */
		buf[buf_size - 1] = '\0';
	}
	return ret;
}

#endif	/* TFA_OTP_TAGS_H */
