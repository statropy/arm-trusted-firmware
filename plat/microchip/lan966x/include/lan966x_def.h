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
#define LAN996X_QSPI0_RANGE	UL(16 * 1024 * 1024)
#define LAN996X_DDR_BASE	UL(0x60000000)
#define LAN996X_DDR_SIZE	UL(2 * 1024 * 1024 * 1024)

#define LAN996X_APB_BASE	UL(0xE0000000)
#define LAN996X_APB_SIZE	UL(0x00C00000)

/* NB - temporary use - test */
#define LAN996X_SHARED_RAM_BASE	UL(0x00110000)
#define LAN996X_SHARED_RAM_SIZE	UL(1024 * 64)

/*
 * Flexcom base offsets
 */
#define FLEXCOM0_BASE			UL(0xE0040000)
#define FLEXCOM1_BASE			UL(0xE0044000)
#define FLEXCOM2_BASE			UL(0xE0060000)
#define FLEXCOM3_BASE			UL(0xE0064000)
#define FLEXCOM4_BASE			UL(0xE0070000)

#define FLEXCOM_UART_OFFSET		UL(0x200)

/*
 * Flexcom UART related constants
 */
#define FLEXCOM_BAUDRATE            UL(115200)
#define FLEXCOM_UART_CLK_IN_HZ      UL(19200000)

/*
 * Offset value for GPIO interface
 */
#define GCB_GPIO_ADDR           UL(0xE2004064)
#define GCB_ADDR_BASE           UL(0xE2004000)

/* QSPI controller(s) */
#define QSPI_0_ADDR	0xe0804000
#define QSPI_1_ADDR	0xe0054000
#define QSPI_2_ADDR	0xe0834000

#define QSPI_SIZE	0x00000100

/* ToDo: Check defines */
#define FACTORY_CLK     UL(30000000)    /* Factory CLK used on sunrise board */


/*
 * Timer
 */
#define SYS_COUNTER_FREQ_IN_TICKS	(60000000) /* XXX - note - adjust x10 for silicon */

#endif /* LAN966X_DEF_H */
