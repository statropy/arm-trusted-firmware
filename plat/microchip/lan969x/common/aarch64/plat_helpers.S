/*
 * Copyright (c) 2018-2021, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <assert_macros.S>
#include <platform_def.h>
#include <flexcom_uart.h>

	.globl	platform_mem_init
	.globl	plat_get_my_entrypoint
	.globl	plat_reset_handler
	.globl	plat_my_core_pos
	.globl	plat_crash_console_init
	.globl	plat_crash_console_putc
	.globl	plat_crash_console_flush

	/* -----------------------------------------------------
	 * void platform_mem_init (void);
	 * -----------------------------------------------------
	 */
func platform_mem_init
	ret
endfunc platform_mem_init

/* -----------------------------------------------------------------------
 * Indicate a cold boot for every CPU - warm boot is unsupported.
 * -----------------------------------------------------------------------
 */
func plat_get_my_entrypoint
	mov	x0, #0
	ret
endfunc plat_get_my_entrypoint

	/* ---------------------------------------------
	 * void plat_reset_handler(void);
	 * ---------------------------------------------
	 */
func plat_reset_handler
	ret
endfunc plat_reset_handler

func plat_my_core_pos
	mov	x0, #0
	ret
endfunc plat_my_core_pos

func plat_crash_console_init
	mov_imm x0, (LAN969X_FLEXCOM_0_BASE + FLEXCOM_UART_OFFSET)
	mov_imm w1, FLEXCOM_DIVISOR(PERIPHERAL_CLK, FLEXCOM_BAUDRATE)
	b console_flexcom_core_init
endfunc plat_crash_console_init

func plat_crash_console_putc
	mov_imm x1, (LAN969X_FLEXCOM_0_BASE + FLEXCOM_UART_OFFSET)
	b console_flexcom_core_putc
endfunc plat_crash_console_putc

func plat_crash_console_flush
	mov_imm x0, (LAN969X_FLEXCOM_0_BASE + FLEXCOM_UART_OFFSET)
	b console_flexcom_core_flush
endfunc plat_crash_console_flush
