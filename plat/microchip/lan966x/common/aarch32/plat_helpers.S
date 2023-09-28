/*
 * Copyright (c) 2016-2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <platform_def.h>

	.globl	plat_secondary_cold_boot_setup
	.globl	plat_get_my_entrypoint
	.globl	plat_arm_calc_core_pos
	.globl	plat_my_core_pos
	.globl	plat_error_handler
	.globl	plat_report_exception
	.globl	plat_panic_handler
	.globl	lan966x_crash_console
	.globl	plat_crash_console_init
	.globl	plat_crash_console_putc
	.globl	plat_crash_console_flush

	/* --------------------------------------------------------------------
	 * void plat_secondary_cold_boot_setup (void);
	 *
	 * For AArch32, cold-booting secondary CPUs is not yet
	 * implemented and they panic.
	 * --------------------------------------------------------------------
	 */
func plat_secondary_cold_boot_setup
cb_panic:
	b	cb_panic
endfunc plat_secondary_cold_boot_setup

	/* ---------------------------------------------------------------------
	 * unsigned long plat_get_my_entrypoint (void);
	 *
	 * Main job of this routine is to distinguish between a cold and warm
	 * boot. On FVP, this information can be queried from the power
	 * controller. The Power Control SYS Status Register (PSYSR) indicates
	 * the wake-up reason for the CPU.
	 *
	 * For a cold boot, return 0.
	 * For a warm boot, read the mailbox and return the address it contains.
	 *
	 * TODO: PSYSR is a common register and should be
	 * 	accessed using locks. Since it is not possible
	 * 	to use locks immediately after a cold reset
	 * 	we are relying on the fact that after a cold
	 * 	reset all cpus will read the same WK field
	 * ---------------------------------------------------------------------
	 */
func plat_get_my_entrypoint
	/* TODO support warm boot */
	mov	r0, #0
	bx 	lr
endfunc plat_get_my_entrypoint

	/* ------------------------------------
	 *  unsigned int plat_my_core_pos(void)
	 * ------------------------------------
	 */
func plat_my_core_pos
	ldcopr	r0, MPIDR
	and	r1, r0, #MPIDR_CPU_MASK
	and	r0, r0, #MPIDR_CLUSTER_MASK
	add	r0, r1, r0, LSR #6
	bx	lr
endfunc plat_my_core_pos

	/* ---------------------------------------------------------------------
	 * unsigned int plat_arm_calc_core_pos(u_register_t mpidr)
	 *
	 * Function to calculate the core position on FVP.
	 *
	 * (ClusterId * LAN966x_MAX_CPUS_PER_CLUSTER * LAN966x_MAX_PE_PER_CPU) +
	 * (CPUId * LAN966x_MAX_PE_PER_CPU) +
	 * ThreadId
	 *
	 * which can be simplified as:
	 *
	 * ((ClusterId * LAN966x_MAX_CPUS_PER_CLUSTER + CPUId) * LAN966x_MAX_PE_PER_CPU)
	 * + ThreadId
	 * ---------------------------------------------------------------------
	 */
func plat_arm_calc_core_pos
	mov	r3, r0

	/*
	 * Check for MT bit in MPIDR. If not set, shift MPIDR to left to make it
	 * look as if in a multi-threaded implementation
	 */
	tst	r0, #MPIDR_MT_MASK
	lsleq	r3, r0, #MPIDR_AFFINITY_BITS

	/* Extract individual affinity fields from MPIDR */
	ubfx	r0, r3, #MPIDR_AFF0_SHIFT, #MPIDR_AFFINITY_BITS
	ubfx	r1, r3, #MPIDR_AFF1_SHIFT, #MPIDR_AFFINITY_BITS
	ubfx	r2, r3, #MPIDR_AFF2_SHIFT, #MPIDR_AFFINITY_BITS

	/* Compute linear position */
	mov	r3, #LAN966x_MAX_CPUS_PER_CLUSTER
	mla	r1, r2, r3, r1
	mov	r3, #LAN966x_MAX_PE_PER_CPU
	mla	r0, r1, r3, r0

	bx	lr
endfunc plat_arm_calc_core_pos

.section .data.crash_console, "aw"
	.align 4
console_base: .fill 1, 4, 0	/* zero fill one 4 byte word */
reg_dump: .fill 16, 4, 0	/* zero fill 16 4 byte words (r0-r12, lr, cpsr,
				 * mode)
				 */

	/* Save a single register via sp and increment pointer */
	.macro saver _reg
	str	\_reg, [sp]
	add	sp, #4
	.endm

	/* Save important registers, must be room in reg_dump array  */
	.macro save_registers
	ldr	sp, =reg_dump
	saver	r0
	saver	r1
	saver	r2
	saver	r3
	saver	r4
	saver	r5
	saver	r6
	saver	r7
	saver	r8
	saver	r9
	saver	r10
	saver	r11
	saver	r12
	saver	lr
	mrs	r1, cpsr
	saver	r1
	and	r1, #MODE32_MASK
	saver	r1
	ldr	sp, =reg_dump
	.endm

	/* Print a single register */
	.macro	prreg _name, _areg
	ldr	r4, =\_name
	bl	asm_print_str
	ldr	r4, [\_areg]
	bl	asm_print_hex
	add	\_areg, #4
	.endm

	/* Print exception mode (except for MON = 22) */
	.macro	prexc _name, _areg
	ldr	r4, [\_areg]
	cmp	r4, #22
	beq	1f
	ldr	r4, =\_name
	bl	asm_print_str
	ldr	r4, [\_areg]
	bl	asm_print_hex
	add	\_areg, #4
1:
	.endm

	/*
	 * lan966x_crash_console
	 * Store the console_t pointer
	 * for later use
	 * r0: console_t *con
	 * Clobber: r1
	 */
func lan966x_crash_console
	ldr	r1, =console_base
	str	r0, [r1]
	bx	lr
endfunc lan966x_crash_console

	/*
	 * plat_crash_console_init
	 * Dummy function
	 */
func plat_crash_console_init
	bx	lr
endfunc plat_crash_console_init

	/*
	 * plat_crash_console_putc
	 * Use the flexcom console putc
	 * r0: char ch
	 * Clobber: r1
	 */
func plat_crash_console_putc
	ldr	r1, =console_base
	ldr	r1, [r1]
	/* check console base */
	cmp	r1, #0
	beq	crash_error_putc
	b	console_flexcom_putc
crash_error_putc:
	mov r0, #-1
	bx  lr
endfunc plat_crash_console_putc

	/*
	 * plat_crash_console_flush
	 * Use the flexcom console flush
	 * Clobber: r1
	 */
func plat_crash_console_flush
	ldr	r1, =console_base
	ldr	r1, [r1]
	/* check console base */
	cmp	r1, #0
	beq	crash_error_flush
	b	console_flexcom_flush
crash_error_flush:
	mov r0, #-1
	bx  lr
endfunc plat_crash_console_flush

.section .rodata.error_handler_msg, "aS"
	error_no:	.asciz "\nError: 0x"
	excp_no: 	.asciz "\nException: 0x"
	r0name:		.asciz "\nR0   = 0x"
	r1name:		.asciz "\nR1   = 0x"
	r2name:		.asciz "\nR2   = 0x"
	r3name:		.asciz "\nR3   = 0x"
	r4name:		.asciz "\nR4   = 0x"
	r5name:		.asciz "\nR5   = 0x"
	r6name:		.asciz "\nR6   = 0x"
	r7name:		.asciz "\nR7   = 0x"
	r8name:		.asciz "\nR8   = 0x"
	r9name:		.asciz "\nR9   = 0x"
	r10name:	.asciz "\nR10  = 0x"
	r11name:	.asciz "\nR11  = 0x"
	r12name:	.asciz "\nR12  = 0x"
	lrname:		.asciz "\nLR   = 0x"
	cpsrname:	.asciz "\nCPSR = 0x"

	/*
	 * print_registers
	 * Print important registers
	 * Registers must have been saved previously
	 * Clobbers: sp, and whatever the asm_print routines clobbers
	 */
func print_registers
	ldr	sp, =reg_dump
	prreg	r0name, sp
	prreg	r1name, sp
	prreg	r2name, sp
	prreg	r3name, sp
	prreg	r4name, sp
	prreg	r5name, sp
	prreg	r6name, sp
	prreg	r7name, sp
	prreg	r8name, sp
	prreg	r9name, sp
	prreg	r10name, sp
	prreg	r11name, sp
	prreg	r12name, sp
	prreg	lrname, sp
	prreg	cpsrname, sp
	prexc	excp_no, sp
	ldr	sp, =reg_dump
end_here:
	wfi
	b end_here
endfunc print_registers

	/*
	 * plat_error_handler
	 * r0: error value argument
	 * Clobbers: r0, r4, sp
	 */
func plat_error_handler
	save_registers
	mov	r5, r0
	ldr	r4, =error_no
	bl	asm_print_str
	mov	r4, r5
	bl	asm_print_hex
	b	print_registers
endfunc plat_error_handler

	/*
	 * plat_report_exception
	 * Clobbers: r0, r4, sp
	 */
func plat_report_exception
	save_registers
	b	print_registers
endfunc plat_report_exception

	/*
	 * plat_panic_handler
	 * Clobbers: r0, r4, sp
	 */
func plat_panic_handler
	save_registers
	b	print_registers
endfunc plat_panic_handler
