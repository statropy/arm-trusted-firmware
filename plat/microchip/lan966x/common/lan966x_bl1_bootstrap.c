/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <errno.h>
#include <common/debug.h>
#include <plat/common/platform.h>
#include <endian.h>

#include "lan966x_private.h"
#include "lan966x_bootstrap.h"
#include "otp.h"

/* Max OTP data to write in one req */
#define MAX_OTP_DATA	1024

static struct {
	uint32_t length;
} received_code_status;

static void handle_read_rom_version(const bootstrap_req_t *req)
{
	// Send Version
	bootstrap_TxAckData(version_string, strlen(version_string));
}

static void handle_strap(const bootstrap_req_t *req)
{
	bootstrap_TxAck();
	lan966x_set_strapping(req->arg0);
}

static void handle_otp_data(bootstrap_req_t *req)
{
	uint8_t *ptr = (uint8_t *) BL2_BASE;

	if (req->len > 0 && req->len < MAX_OTP_DATA &&
	    bootstrap_RxDataCrc(req, ptr)) {
		if (otp_write_bytes(req->arg0, req->len, ptr) == 0)
			bootstrap_Tx(BOOTSTRAP_ACK, req->arg0, 0, NULL);
		else
			bootstrap_TxNack("OTP program failed");
		/* Wipe data */
		memset(ptr, 0, req->len);
	} else
		bootstrap_TxNack("OTP rx data failed or illegal data size");
}

static void handle_otp_random(bootstrap_req_t *req)
{
	uint32_t datalen, *data = (uint32_t *) BL2_BASE;
	int i;

	if (req->len == sizeof(uint32_t) && bootstrap_RxDataCrc(req, (uint8_t *)&datalen)) {
		datalen = __ntohl(datalen);
		if (datalen > 0 &&
		    datalen < MAX_OTP_DATA) {
			/* Read TRNG data */
			for (i = 0; i < div_round_up(datalen, sizeof(uint32_t)); i++)
				data[i] = lan966x_trng_read();
			/* Write to OTP */
			if (otp_write_bytes(req->arg0, datalen, (uint8_t *)data) == 0)
				bootstrap_Tx(BOOTSTRAP_ACK, req->arg0, 0, NULL);
			else
				bootstrap_TxNack("OTP program random failed");
			/* Wipe data */
			memset(data, 0, datalen);
		} else
			bootstrap_TxNack("OTP random data illegal length");
	} else
		bootstrap_TxNack("OTP random data illegal req length length");
}

static void handle_otp_commit(const bootstrap_req_t *req)
{
	if (otp_commit_emulation() == 0)
		bootstrap_TxAck();
	else
		bootstrap_TxNack("OTP commit failed");
}

static void handle_otp_regions(const bootstrap_req_t *req)
{
	if (otp_write_regions() == 0)
		bootstrap_TxAck();
	else
		bootstrap_TxNack("OTP write regions failed");
}

static int bl1_load_image(unsigned int image_id)
{
	image_desc_t *desc;
	image_info_t *info;
	int err;

	/* Get the image descriptor */
	desc = bl1_plat_get_image_desc(image_id);
	if (desc == NULL)
		return -ENOENT;

	/* Get the image info */
	info = &desc->image_info;

	err = bl1_plat_handle_pre_image_load(image_id);
	if (err != 0) {
		ERROR("Failure in pre image load handling of BL2U (%d)\n", err);
		return err;
	}

	err = load_auth_image(image_id, info);
	if (err != 0) {
		ERROR("Failed to load BL2U firmware.\n");
		return err;
	}

	/* Allow platform to handle image information. */
	err = bl1_plat_handle_post_image_load(image_id);
	if (err != 0) {
		ERROR("Failure in post image load handling of BL2U (%d)\n", err);
		return err;
	}

	return 0;
}

static int handle_auth(const bootstrap_req_t *req)
{
	int rc = bl1_load_image(BL2U_IMAGE_ID);

	if (rc == 0) {
		bootstrap_TxAck();
		lan966x_bl1_trigger_fwu();
	} else {
		bootstrap_TxNack_rc("Authenticate fails", rc);
	}

	return rc;
}

static void handle_sjtag_rd(bootstrap_req_t *req)
{
	lan966x_key32_t k;

	if (lan966x_sjtag_read_challenge(&k) == 0)
		bootstrap_TxAckData(k.b, sizeof(k.b));
	else
		bootstrap_TxNack("SJTAG read challenge failed");
}

static void handle_sjtag_wr(bootstrap_req_t *req)
{
	lan966x_key32_t k;

	if (req->len == sizeof(k.b) && bootstrap_RxDataCrc(req, k.b)) {
		if (lan966x_sjtag_write_response(&k) == 0)
			bootstrap_Tx(BOOTSTRAP_ACK, req->arg0, 0, NULL);
		else
			bootstrap_TxNack("SJTAG unlock failed");
	} else
		bootstrap_TxNack("SJTAG rx data failed or illegal data size");
}

static void handle_send_data(const bootstrap_req_t *req)
{
	uint32_t length = req->arg0;
	uintptr_t start;
	uint8_t *ptr;
	int nBytes, offset;

	if (length == 0 || length > BL1_MON_MAX_SIZE) {
		bootstrap_TxNack("Length Error");
		return;
	}

	/* Put dld as high in BL2 area as possible */
	start = ((BL1_MON_LIMIT - length) & ~0xFF);
	assert(start >= BL1_MON_MIN_BASE && start < BL1_MON_LIMIT);
	/* Download to this (aligned) address */
	ptr = (uint8_t *) start;

	// Go ahead, receive data
	bootstrap_TxAck();

	/* Gobble up the data chunks */
	nBytes = offset = 0;
	while (offset < length &&
	       (nBytes = bootstrap_RxData(ptr, offset,
					  length - offset)) > 0) {
		ptr += nBytes;
		offset += nBytes;
	}

	if (offset != length) {
		ERROR("RxData Error: n = %d, l = %d, o = %d\n", nBytes, length, offset);
		return;
	}

	/* We have data */
	received_code_status.length = length;

	/* Inform IO layer of the FIP */
	lan966x_bl1_io_enable_ram_fip(start, length);

	INFO("Received %d bytes\n", length);
}

static void handle_trace_lvl(const bootstrap_req_t *req)
{
	bootstrap_TxAck();
	tf_log_set_max_level(req->arg0);
}

void lan966x_bootstrap_monitor(void)
{
	bootstrap_req_t req;
	bool exit_monitor = false;

	lan966x_reset_max_trace_level();
	INFO("*** ENTERING BOOTSTRAP MONITOR ***\n");

	while (!exit_monitor) {

		if (!bootstrap_RxReq(&req)) {
			bootstrap_TxNack("Garbled command");
			continue;
		}

		if (is_cmd(&req, BOOTSTRAP_CONT)) {
			bootstrap_TxAck();
			exit_monitor = true;
		} else if (is_cmd(&req, BOOTSTRAP_VERS))
			handle_read_rom_version(&req);
		else if (is_cmd(&req, BOOTSTRAP_SEND))
			handle_send_data(&req);
		else if (is_cmd(&req, BOOTSTRAP_STRAP))
			handle_strap(&req);
		else if (is_cmd(&req, BOOTSTRAP_TRACE_LVL))
			handle_trace_lvl(&req);
		else if (is_cmd(&req, BOOTSTRAP_OTPD))
			handle_otp_data(&req);
		else if (is_cmd(&req, BOOTSTRAP_OTPR))
			handle_otp_random(&req);
		else if (is_cmd(&req, BOOTSTRAP_OTPC))
			handle_otp_commit(&req);
		else if (is_cmd(&req, BOOTSTRAP_OTP_REGIONS))
			handle_otp_regions(&req);
		else if (is_cmd(&req, BOOTSTRAP_AUTH)) {
			if (handle_auth(&req) == 0)
				exit_monitor = true;
		} else if (is_cmd(&req, BOOTSTRAP_SJTAG_RD))
			handle_sjtag_rd(&req);
		else if (is_cmd(&req, BOOTSTRAP_SJTAG_WR))
			handle_sjtag_wr(&req);
		else
			bootstrap_TxNack("Unknown command");
	}

	INFO("*** EXITING BOOTSTRAP MONITOR ***\n");
	lan966x_set_max_trace_level();
}
