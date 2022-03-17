/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include "lan966x_bootstrap.h"
#include "lan966x_private.h"

static uint8_t bootstrap_req_flags;

static int hex2nibble(int ch)
{
	if ( ch >= '0' && ch <= '9' )
		return ch - '0';
	if ( ch >= 'a' && ch <= 'f' )
		return 10 + (ch - 'a');
	if ( ch >= 'A' && ch <= 'F' )
		return 10 + (ch - 'A');
	return -1;
}

// Converts input string into hex value
static uint32_t atohex(char *buf, int n)
{
	uint32_t data = 0;
	char *ptr;

	for (ptr = buf; n && *ptr; ptr++, n--)
		data = (data << 4) | hex2nibble(*ptr);

	return data;
}

static void hex2str_byte(char *buf, uint8_t val)
{
	static char *cvt = "0123456789ABCDEF";

	*buf++ = cvt[(val >> 4) & 0xF];
	*buf++ = cvt[(val >> 0) & 0xF];
}

static void hex2str(char *buf, uint32_t val)
{
	hex2str_byte(buf + 0, (val >> 24) & 0xFF);
	hex2str_byte(buf + 2, (val >> 16) & 0xFF);
	hex2str_byte(buf + 4, (val >>  8) & 0xFF);
	hex2str_byte(buf + 6, (val >>  0) & 0xFF);
}

static int MON_GET(void)
{
	return console_getc();
}

static int MON_GET_Data(char *data, uint32_t length)
{
	int i, c;

	for (i = 0; i < length; i++)
		if ((c = MON_GET()) < 0)
			return -1;
		else {
			*data++ = c;
			if (c == '#' || c == '%')
				break;
		}

	return length;
}

static void MON_PUT(char c)
{
	console_putc(c);
}

static uint32_t MON_PUT_Data(const void *buffer, uint32_t length, uint32_t crc)
{
	const unsigned char *data = buffer;
	int i;

	for (i = 0; i < length; i++)
		MON_PUT(*data++);

	return Crc32c(crc, buffer, length);
}

static void bootstrap_RxPayload(uint8_t *data, bootstrap_req_t *req)
{
	int datasize = req->len;

	if (req->flags & BSTRAP_REQ_FLAG_BINARY) {
		uint8_t *ptr = data;

		while (datasize--)
			*ptr++ = (uint8_t) MON_GET();
		req->crc = Crc32c(req->crc, data, req->len);
	} else {
		char in[2];

		while (datasize--) {
			in[0] = MON_GET();
			in[1] = MON_GET();
			*data++ = hex2nibble(in[1]) + (hex2nibble(in[0]) << 4);
			req->crc = Crc32c(req->crc, in, sizeof(in));
		}
	}
}

static bool bootstrap_RxCrcCheck(bootstrap_req_t *req)
{
	char hexdigest[8];
	uint32_t crc;

	MON_GET_Data(hexdigest, sizeof(hexdigest));
	crc = atohex(hexdigest, sizeof(hexdigest));

	return crc == req->crc;
}

bool bootstrap_RxReq(bootstrap_req_t *req)
{
	bstrap_char_req_t rxdata;
	int rx;

	bootstrap_req_flags = 0; /* Reset flags */

	/* Syncronize SOF */
	while(MON_GET() != BOOTSTRAP_SOF)
		;

	/* Read fixed parts */
	rx = MON_GET_Data((char*)&rxdata, sizeof(rxdata));

	/* Check format */
	if (rx == sizeof(rxdata) &&
	    rxdata.delim1 == ',' &&
	    rxdata.delim2 == ',' &&
	    (rxdata.pay_delim == '#' || rxdata.pay_delim == '%')) {
		req->cmd = rxdata.cmd;
		req->flags = 0;
		req->arg0 = atohex(rxdata.arg0, BSTRAP_HEXFLD_LEN);
		req->len = atohex(rxdata.len, BSTRAP_HEXFLD_LEN);
		/* Req flags */
		if (rxdata.pay_delim == '%') {
			bootstrap_req_flags |= BSTRAP_REQ_FLAG_BINARY;
			req->flags |= BSTRAP_REQ_FLAG_BINARY;
		}
		/* Fixed part of CRC */
		req->crc = Crc32c(0, &rxdata, sizeof(rxdata));
		/* Commands with payloads are checked later */
		if (req->len == 0)
			return bootstrap_RxCrcCheck(req);
		return true;
	}

	return false;
}

static uint32_t bootstrap_TxPayload(const uint8_t *data,
				    uint32_t length, uint32_t crc)
{
	char out[2];

	while (length--) {
		hex2str_byte(out, *data++);
		crc = MON_PUT_Data(out, sizeof(out), crc);
	}

	return crc;
}

void bootstrap_Tx(char cmd, int32_t status,
		  uint32_t length, const uint8_t *payload)
{
	bstrap_char_req_t bootstrapTx;
	uint32_t crc = 0;
	char hexdigest[8];

	bootstrapTx.cmd = cmd;

	hex2str(bootstrapTx.arg0, status);
	hex2str(bootstrapTx.len, length);

	bootstrapTx.delim1 =
		bootstrapTx.delim2 = ',';
	bootstrapTx.pay_delim = (bootstrap_req_flags & BSTRAP_REQ_FLAG_BINARY) ? '%' : '#';

	MON_PUT(BOOTSTRAP_SOF);
	crc = MON_PUT_Data((char*)&bootstrapTx, sizeof(bootstrapTx), crc);

	// payload if provided
	if (payload) {
		assert(length != 0);
		if (bootstrap_req_flags & BSTRAP_REQ_FLAG_BINARY)
			crc = MON_PUT_Data(payload, length, crc);
		else
			crc = bootstrap_TxPayload(payload, length, crc);
	}

	/* Send CRC */
	hex2str(hexdigest, crc);
	MON_PUT_Data(hexdigest, sizeof(hexdigest), 0);
	console_flush();
}

int bootstrap_RxData(uint8_t *data,
		     int offset,
		     int datasize)
{
	bootstrap_req_t req;
	const char *errtxt = "Expected DATA";

	req.arg0 = 0;
	if (bootstrap_RxReq(&req) &&
	    is_cmd(&req, BOOTSTRAP_DATA)) {
		if (req.len > datasize) {
			errtxt = "Too much data";
			goto send_err;
		}
		if (req.arg0 != offset) {
			errtxt = "Data misordering";
			goto send_err;
		}
		bootstrap_RxPayload(data, &req);
		if (bootstrap_RxCrcCheck(&req)) {
			bootstrap_Tx(BOOTSTRAP_ACK, req.arg0, 0, NULL);
			return req.len;
		}
		errtxt = "CRC failure";
	}

send_err:
	bootstrap_Tx(BOOTSTRAP_NACK, req.arg0, strlen(errtxt), (const uint8_t*)errtxt);
	return -1;
}

bool bootstrap_RxDataCrc(bootstrap_req_t *req, uint8_t *data)
{
	bootstrap_RxPayload(data, req);
	return bootstrap_RxCrcCheck(req);
}
