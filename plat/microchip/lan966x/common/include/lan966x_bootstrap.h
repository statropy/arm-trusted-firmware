/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// BOOTSTRAP Start-Of-Frame
#define BOOTSTRAP_SOF          '>'

// Get version
#define BOOTSTRAP_VERS         'V'
// Send Code
#define BOOTSTRAP_SEND         'S'
// Data transmitted
#define BOOTSTRAP_DATA         'D'
// Authenticate data
#define BOOTSTRAP_AUTH         'U'
// Exec downloaded data
#define BOOTSTRAP_EXEC         'E'
// Override strapping
#define BOOTSTRAP_STRAP        'O'
// Program OTP data
#define BOOTSTRAP_OTPD         'P'
// Program OTP random data
#define BOOTSTRAP_OTPR         'R'
// Commit OTP emulation data
#define BOOTSTRAP_OTPC         'M'
// Continue boot
#define BOOTSTRAP_CONT         'C'
// ACK
#define BOOTSTRAP_ACK          'a'
// NACK
#define BOOTSTRAP_NACK         'n'

#define BSTRAP_HEXFLD_LEN	8

typedef struct {
	char cmd;                          /* C        */
	char delim1;		           /* ','      */
	char arg0[BSTRAP_HEXFLD_LEN];      /* HHHHHHHH */
	char delim2;			   /* ','      */
	char len[BSTRAP_HEXFLD_LEN];       /* HHHHHHHH */
	char pay_delim;			   /* '(#|%)'  */
} __packed bstrap_char_req_t;

#define BSTRAP_CMD_FIXED_LEN (sizeof(bstrap_char_req_t))

#define BSTRAP_REQ_FLAG_BINARY	BIT(0)

typedef struct {
	uint8_t  cmd;
	uint8_t  flags;
	uint32_t arg0;
	uint32_t len;
	uint32_t crc;
} bootstrap_req_t;

static inline bool is_cmd(const bootstrap_req_t *req, const char cmd)
{
	return req->cmd == cmd;
}

bool bootstrap_RxReq(bootstrap_req_t *req);

int bootstrap_RxData(uint8_t *data,
		     int offset,
		     int datasize);

void bootstrap_Tx(char cmd, int32_t status,
		  uint32_t length, const uint8_t *payload);

static inline void bootstrap_TxAck(void)
{
	bootstrap_Tx(BOOTSTRAP_ACK, 0, 0, NULL);
}

static inline void bootstrap_TxAckData(const void *data, uint32_t len)
{
	bootstrap_Tx(BOOTSTRAP_ACK, 0, len, data);
}

static inline void bootstrap_TxNack(const char *str)
{
	bootstrap_Tx(BOOTSTRAP_NACK, 0, strlen(str), (const uint8_t*)str);
}

bool bootstrap_RxDataCrc(bootstrap_req_t *req, uint8_t *data);
