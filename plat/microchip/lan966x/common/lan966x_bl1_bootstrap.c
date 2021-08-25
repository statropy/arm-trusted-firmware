/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <plat/common/platform.h>
#include <endian.h>
#include <../bl1/bl1_private.h>	/* UGLY... */

#include "lan966x_private.h"
#include "lan966x_bootstrap.h"
#include "otp.h"

/* Max OTP data to write in one req */
#define MAX_OTP_DATA	1024

__attribute__((__noreturn__)) void plat_bootstrap_exec(void);

static struct {
	uint32_t length;
	bool     authenticated;
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

static void handle_auth(const bootstrap_req_t *req)
{
	/* TBBR not enabled yet */
	bootstrap_TxAck();
	received_code_status.authenticated = true;
}

static void handle_exec(const bootstrap_req_t *req)
{
	if (received_code_status.length) {

		/* We're going ahead */
		bootstrap_TxAck();

		bl1_plat_handle_post_image_load(BL2_IMAGE_ID);
		bl1_prepare_next_image(BL2_IMAGE_ID);

		plat_bootstrap_exec();
		/* Not reached */
	}
	bootstrap_TxNack("Nothing to execute");
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
	uintptr_t start = BL2_BASE;
	uint8_t *ptr;
	int nBytes, offset;

	if (length == 0 || length > BL2_SIZE) {
		bootstrap_TxNack("Length Error");
		return;
	}

	ptr = (uint8_t*)start;

	// Go ahead, receive data
	bootstrap_TxAck();

	/* Gobble up the data chunks */
	offset = 0;
	while (offset < length &&
	       (nBytes = bootstrap_RxData(ptr, offset,
					  length - offset)) > 0) {
		ptr += nBytes;
		offset += nBytes;
	}

	/*
	 * We need to flush since execution may be using different
	 * context than the current.
	 */
	flush_dcache_range(start, length);

	/* We have data */
	received_code_status.length = length;
	received_code_status.authenticated = false;

	VERBOSE("Received %d out of %d bytes\n", offset, length);
}

void lan966x_bootstrap_monitor(void)
{
	bootstrap_req_t req;

	INFO("*** ENTERING BOOTSTRAP MONITOR ***\n");

	while (1) {

		if (!bootstrap_RxReq(&req)) {
			bootstrap_TxNack("Garbled command");
			continue;
		}

		if (is_cmd(&req, BOOTSTRAP_CONT))
			break;
		else if (is_cmd(&req, BOOTSTRAP_VERS))
			handle_read_rom_version(&req);
		else if (is_cmd(&req, BOOTSTRAP_SEND))
			handle_send_data(&req);
		else if (is_cmd(&req, BOOTSTRAP_STRAP))
			handle_strap(&req);
		else if (is_cmd(&req, BOOTSTRAP_OTPD))
			handle_otp_data(&req);
		else if (is_cmd(&req, BOOTSTRAP_OTPR))
			handle_otp_random(&req);
		else if (is_cmd(&req, BOOTSTRAP_OTPC))
			handle_otp_commit(&req);
		else if (is_cmd(&req, BOOTSTRAP_AUTH))
			handle_auth(&req);
		else if (is_cmd(&req, BOOTSTRAP_EXEC))
			handle_exec(&req);
		else if (is_cmd(&req, BOOTSTRAP_SJTAG_RD))
			handle_sjtag_rd(&req);
		else if (is_cmd(&req, BOOTSTRAP_SJTAG_WR))
			handle_sjtag_wr(&req);
		else
			bootstrap_TxNack("Unknown command");
	}

	INFO("*** EXITING BOOTSTRAP MONITOR ***\n");
}
