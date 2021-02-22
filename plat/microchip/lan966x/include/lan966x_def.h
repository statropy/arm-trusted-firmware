/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_DEF_H
#define LAN966X_DEF_H

/* LAN966X defines */
#define LAN996X_BOOTROM_BASE	UL(0x00000000)
#define LAN996X_BOOTROM_SIZE	UL(1024 * 64)
#define LAN996X_SRAM_BASE	UL(0x00100000)
#define LAN996X_SRAM_SIZE	UL(1024 * 64)
#define LAN996X_QSPI0_BASE	UL(0x20000000)
#define LAN996X_DDR_BASE	UL(0x60000000)

#define LAN996X_FIP_MAX_SIZE	UL(2048 * 1024) /* 2Mb tentatively */

/*
 * Flexcom offsets
 */
#define FLEXCOM0_BASE			UL(0xE0040000)
#define FLEXCOM1_BASE			UL(0xE0044000)
#define FLEXCOM2_BASE			UL(0xE0060000)
#define FLEXCOM3_BASE			UL(0xE0064000)

/*
 * Flexcom UART related constants
 */
#define FLEXCOM_BAUDRATE		UL(115200)
#define FLEXCOM_UART_CLK_IN_HZ		UL(19200000)

#endif /* LAN966X_DEF_H */
