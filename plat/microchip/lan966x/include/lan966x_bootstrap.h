/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// BOOTSTRAP Start-Of-Frame
#define BOOTSTRAP_SOF          '>'

// Quit BOOTSTRAP
#define BOOTSTRAP_QUIT         'Q'
// Send Code
#define BOOTSTRAP_SEND         'S'
// Data transmitted
#define BOOTSTRAP_DATA         'D'
// ACK
#define BOOTSTRAP_ACK          'A'
// NACK
#define BOOTSTRAP_NACK         'N'
// Get version
#define BOOTSTRAP_VERS         'V'

#define BSTRAP_ARG0_MAX_LENGTH	8
#define BSTRAP_LEN_MAX_LENGTH	8

typedef struct {
	char cmd;                          /* C        */
	char delim1;		           /* ','      */
	char arg0[BSTRAP_ARG0_MAX_LENGTH]; /* HHHHHHHH */
	char delim2;			   /* ','      */
	char len[BSTRAP_LEN_MAX_LENGTH];   /* HHHHHHHH */
	char pay_delim;			   /* '#'      */
} __packed bstrap_char_req_t;

#define BSTRAP_CMD_FIXED_LEN (sizeof(bstrap_char_req_t))

typedef struct {
	uint8_t  cmd;
	uint32_t arg0;
	uint32_t len;
	uint32_t crc;
} bootstrap_req_t;

static bool is_cmd(const bootstrap_req_t *req, const char cmd)
{
	return req->cmd == cmd;
}
