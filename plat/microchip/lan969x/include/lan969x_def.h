/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN969X_DEF_H
#define LAN969X_DEF_H

#include "lan969x_targets.h"

#define SIZE_K(n)               ((n) * UL(1024))
#define SIZE_M(n)               (SIZE_K(n) * UL(1024))
#define SIZE_G(n)               (SIZE_M(n) * UL(1024))

/* LAN969X defines */
#define LAN969X_BOOTROM_BASE	UL(0x00000000)
#define LAN969X_BOOTROM_SIZE	SIZE_K(128)
#define LAN969X_SRAM_BASE	UL(0x00100000)
#define LAN969X_SRAM_SIZE	SIZE_M(2)
#define LAN969X_PKCL_ROM_BASE	UL(0x00040000)
#define LAN969X_PKCL_ROM_SIZE	SIZE_K(64)
#define LAN969X_PKCL_RAM_BASE	UL(0x00051000)
#define LAN969X_PKCL_RAM_SIZE	SIZE_K(4)
#define LAN969X_QSPI0_MMAP	UL(0x20000000)
#define LAN969X_QSPI0_RANGE	SIZE_M(16)
#define LAN969X_DDR_BASE	UL(0x60000000)
#define LAN969X_DDR_SIZE	SIZE_M(512)

#define LAN966X_GPT_BASE	UL(0x00000000)
#define LAN966X_GPT_SIZE	SIZE_K(32)

/* NB: Using LAN966X names */
#define LAN966X_PKCL_ROM_BASE	UL(0x00040000)
#define LAN966X_PKCL_ROM_SIZE	SIZE_K(64)
#define LAN966X_PKCL_RAM_BASE	UL(0x00051000)
#define LAN966X_PKCL_RAM_SIZE	SIZE_K(4)

/*
 * Was:
 * LAN969X_DEV_BASE	UL(0xE0000000)
 * LAN969X_DEV_SIZE	UL(0x10000000)
 *
 * - But changed to below in order to reduce page table SRAM usage.
 *   It actually includes (part of the) DDR VA range but is not
 *   de-referenced (the DDR part).
 */
#define LAN969X_DEV_BASE	UL(0xC0000000)
#define LAN969X_DEV_SIZE	SIZE_G(1)

#define LAN969X_USB_BASE	UL(0x00200000)
#define LAN969X_USB_SIZE	UL(0x00200000) /* Really 256k, but increased for MMU */

/*
 * GIC-400
 */
#define PLAT_LAN966X_GICD_BASE	(LAN969X_GIC400_BASE + 0x1000)
#define PLAT_LAN966X_GICC_BASE	(LAN969X_GIC400_BASE + 0x2000)

#define LAN966X_IRQ_SEC_PHY_TIMER	29

#define LAN966X_IRQ_SEC_SGI_0	8
#define LAN966X_IRQ_SEC_SGI_1	9
#define LAN966X_IRQ_SEC_SGI_2	10
#define LAN966X_IRQ_SEC_SGI_3	11
#define LAN966X_IRQ_SEC_SGI_4	12
#define LAN966X_IRQ_SEC_SGI_5	13
#define LAN966X_IRQ_SEC_SGI_6	14
#define LAN966X_IRQ_SEC_SGI_7	15

/*
 * HMATRIX2, TZ
 */

#define MATRIX_SLAVE_QSPI0	0
#define MATRIX_SLAVE_QSPI1	1
#define MATRIX_SLAVE_TZAESB	2
#define MATRIX_SLAVE_DDR_HSS	3
#define MATRIX_SLAVE_HSS_APB	4
#define MATRIX_SLAVE_FLEXRAM0	5
#define MATRIX_SLAVE_FLEXRAM1	6
#define MATRIX_SLAVE_USB	7

/*
 * Flexcom
 */

#define FLEXCOM_UART_OFFSET		UL(0x200)

/*
 * Flexcom UART related constants
 */
#define FLEXCOM_BAUDRATE            UL(115200)
#define FLEXCOM_UART_CLK_IN_HZ      UL(19200000)

#if defined(LAN969X_ASIC)
#define PERIPHERAL_CLK  UL(250000000) /* Peripheral CLK used on silicon */
#else
#define PERIPHERAL_CLK  UL(20000000)  /* Peripheral CLK used on sunrise board */
#endif

/* QSPI controller(s) */

#define QSPI_SIZE	0x00000100

/* Sunrise FPGA base clock settings
 * On the real hardware this value will be calculated by the using the proper
 * CPU clock and prescaler factor read out of the chip.
 * */
#define FPGA_SDMMC0_SRC_CLOCK		UL(12000000)
#define FPGA_SDMMC0_MULTI_SRC_CLOCK	UL(48000000)

/*
 * Timer
 */
#if defined(LAN969X_ASIC)
#define SYS_COUNTER_FREQ_IN_TICKS	250000000
#else
#define SYS_COUNTER_FREQ_IN_TICKS	 20000000
#endif

#endif /* LAN969X_DEF_H */
