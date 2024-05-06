/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

// BOOTSTRAP Start-Of-Frame
#define BOOTSTRAP_SOF          '>'

// Get version
#define BOOTSTRAP_VERS         'V'
// Send Code
#define BOOTSTRAP_SEND         'S'
// Data transmitted
#define BOOTSTRAP_DATA         'D'
// Gunzip (BL2U)
#define BOOTSTRAP_UNZIP        'Z'
// Authenticate & load BL2U
#define BOOTSTRAP_AUTH         'U'
// Program OTP data
#define BOOTSTRAP_OTPD         'P'
// Program OTP random data
#define BOOTSTRAP_OTPR         'R'
// Program OTP regions
#define BOOTSTRAP_OTP_REGIONS  'G'
// Read OTP data
#define BOOTSTRAP_OTP_READ     'L' /* BL2U */
// SJTAG Read Challenge
#define BOOTSTRAP_SJTAG_RD     'Q'
// SJTAG Write Resonse
#define BOOTSTRAP_SJTAG_WR     'A'
// Write FIP data to eMMC/SD/NOR (BL2U)
#define BOOTSTRAP_WRITE        'W'
// Write raw image to eMMC/SD/NOR (BL2U)
#define BOOTSTRAP_IMAGE        'I'
// Binding operation for decrypt/encrypt (BL2U)
#define BOOTSTRAP_BIND         'B'
// Binding operation for decrypt/encrypt (BL2U)
#define BOOTSTRAP_BIND_FLASH   'b'
// Reset (BL2U)
#define BOOTSTRAP_RESET        'e'
// Send DDR config (BL2U)
#define BOOTSTRAP_DDR_CFG_SET  'C'
// DDR init before data (BL2U)
#define BOOTSTRAP_DDR_INIT     'd'
// Read DDR config (BL2U)
#define BOOTSTRAP_DDR_CFG_GET  'c'
// Perform DDR test (BL2U)
#define BOOTSTRAP_DDR_TEST     'T'
// Get download data info (BL2U)
#define BOOTSTRAP_DATA_HASH    'H'
// Read register(s) (BL2U)
#define BOOTSTRAP_READ_REG     'x'
// Write data to SRAM (BL2U)
#define BOOTSTRAP_SEND_SRAM    'J'
// Write SRAM data to device, with readback (BL2U)
#define BOOTSTRAP_WRITE_READBACK    'j'
// Get SRAM block size (for BOOTSTRAP_SEND_SRAM cmd) (BL2U)
#define BOOTSTRAP_SRAM_INFO    's'
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

static inline void bootstrap_TxAckData_arg(const void *data, uint32_t len, uint32_t arg)
{
	bootstrap_Tx(BOOTSTRAP_ACK, arg, len, data);
}

static inline void bootstrap_TxAckData(const void *data, uint32_t len)
{
	bootstrap_Tx(BOOTSTRAP_ACK, 0, len, data);
}

static inline void bootstrap_TxAckStr(const char *str)
{
	bootstrap_Tx(BOOTSTRAP_ACK, 0, strlen(str), (const uint8_t *)str);
}

static inline void bootstrap_TxNack_rc(const char *str, uint32_t rc)
{
	bootstrap_Tx(BOOTSTRAP_NACK, rc, strlen(str), (const uint8_t *)str);
}

static inline void bootstrap_TxNack(const char *str)
{
	bootstrap_Tx(BOOTSTRAP_NACK, 0, strlen(str), (const uint8_t *)str);
}

bool bootstrap_RxDataCrc(bootstrap_req_t *req, uint8_t *data);
