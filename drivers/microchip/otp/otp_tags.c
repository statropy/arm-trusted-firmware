/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <string.h>
#include <lib/utils_def.h>
#include <common/debug.h>

#include <otp.h>
#include <otp_tags.h>

#define OTP_MAX_SIZE 8192
#define OTP_TAG_OFFSET 4096
#define OTP_TAG_ENTRY_LENGTH 8
#define OTP_TAG_VAL_LENGTH 6

#define OTP_TAG_GET_SIZE(tag) ((0xff & (uint16_t)tag[7]) / 32)
#define OTP_TAG_GET_CONT(tag) (((0xff & (uint16_t)tag[7]) / 16) & 0x1)
#define OTP_TAG_GET_TAG(tag)  ((((uint16_t)tag[7]) & 0x4) | tag[6])

static bool otp_tag_valid(uint8_t *tag_raw)
{
	uint8_t size;

	size = OTP_TAG_GET_SIZE(tag_raw);
	if (size != 0 && size != 7)
		return true;

	return false;
}

static size_t otp_tag_copy_chunk(uint8_t *tag, void *buf, size_t buf_size)
{
	size_t tag_data_size;

	/* First take this tag's worth of data */
	tag_data_size = OTP_TAG_GET_SIZE(tag);

	/* Its valid to read truncated len */
	tag_data_size = MIN(tag_data_size, buf_size);

	/* Copy data */
	memcpy(buf, tag, tag_data_size);

	return tag_data_size;
}

int otp_tag_get_value(uint8_t *tag, int offset, void *buf, size_t buf_size)
{
	size_t tag_data_size, total = 0;
	int ret;

	/* Zero out destination */
	memset(buf, 0, buf_size);

	/* Get data */
	tag_data_size = otp_tag_copy_chunk(tag, buf, buf_size);

	/* Advance */
	total += tag_data_size;
	buf += tag_data_size;
	buf_size -= tag_data_size;

	/* Repeat as necessary */
	while (buf_size > 0 && OTP_TAG_GET_CONT(tag)) {
		/* Next tag */
		offset += OTP_TAG_ENTRY_LENGTH;

		/* Incomplete? */
		if (offset >= OTP_MAX_SIZE)
			return -EBADF;

		/* Re-use 'tag' buffer */
		ret = otp_read_bytes_raw(offset, OTP_TAG_ENTRY_LENGTH, tag);

		/* We should get consecutive tags */
		if (ret || !otp_tag_valid(tag))
			return -EIO;

		/* Get data */
		tag_data_size = otp_tag_copy_chunk(tag, buf, buf_size);

		/* Advance */
		total += tag_data_size;
		buf += tag_data_size;
		buf_size -= tag_data_size;
	}

	return total;
}

int otp_tag_skip_cont(uint8_t *tag, int offset)
{
	while (OTP_TAG_GET_CONT(tag)) {
		int ret;

		/* Advance to next tag */
		offset += OTP_TAG_ENTRY_LENGTH;

		/* Check offset */
		if (offset >= OTP_MAX_SIZE)
			return offset;

		/* Read, Re-use 'tag' buffer */
		ret = otp_read_bytes_raw(offset, OTP_TAG_ENTRY_LENGTH, tag);
		if (ret)
			break;
	}

	return offset;
}

int otp_tag_get(enum otp_tag_type tag, void *buf, size_t buf_size)
{
	uint8_t tag_raw[OTP_TAG_ENTRY_LENGTH];
	int i, ret;

	if (tag == otp_tag_type_invalid)
		return -EINVAL;

	for (i = OTP_TAG_OFFSET; i < OTP_MAX_SIZE; i += OTP_TAG_ENTRY_LENGTH) {
		ret = otp_read_bytes_raw(i, sizeof(tag_raw), tag_raw);

		if (ret || !otp_tag_valid(tag_raw))
			continue;

		if (tag == OTP_TAG_GET_TAG(tag_raw))
			return otp_tag_get_value(tag_raw, i, buf, buf_size);

		/* Skip tag 'cont' records */
		i = otp_tag_skip_cont(tag_raw, i);
	}

	return -ENOENT;
}
