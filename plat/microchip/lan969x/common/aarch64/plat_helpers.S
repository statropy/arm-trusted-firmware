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
	.globl	plat_report_exception

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

	/* ---------------------------------------------
	 * void plat_report_exception(unsigned int type)
	 * Function to report an unhandled exception
	 * with platform-specific means.
	 * ---------------------------------------------
	 */
func plat_report_exception
	mov	x8, x30

	mrs	x2, currentel
	and	x2, x2, #0xc0
	/* Check EL1 */
	cmp	x2, #0x04
	beq	plat_report_el1

	adr	x4, plat_err_str
	bl	asm_print_str

	adr	x4, esr_el3_str
	bl	asm_print_str

	mrs	x4, esr_el3
	bl	asm_print_hex

	adr	x4, elr_el3_str
	bl	asm_print_str

	mrs	x4, elr_el3
	bl	asm_print_hex
	b	plat_report_end

plat_report_el1:
	adr	x4, plat_err_str
	bl	asm_print_str

	adr	x4, esr_el1_str
	bl	asm_print_str

	mrs	x4, esr_el1
	bl	asm_print_hex

	adr	x4, elr_el1_str
	bl	asm_print_str

	mrs	x4, elr_el1
	bl	asm_print_hex
plat_report_end:
	mov	x30, x8
	ret
endfunc plat_report_exception

	.section .rodata.rev_err_str, "aS"
plat_err_str:
	.asciz "\Exception:"
esr_el3_str:
	.asciz "\nESR_EL3: "
elr_el3_str:
	.asciz "\nELR_EL3: "
esr_el1_str:
	.asciz "\nESR_EL1: "
elr_el1_str:
	.asciz "\nELR_EL1: "
