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
	.globl	plat_is_my_cpu_primary
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

	/* -----------------------------------------------------
	 * unsigned int plat_is_my_cpu_primary (void);
	 *
	 * Find out whether the current cpu is the primary
	 * cpu.
	 * -----------------------------------------------------
	 */
func plat_is_my_cpu_primary
	ldcopr	r0, MPIDR
	ldr	r1, =MPIDR_AFFINITY_MASK
	and	r0, r1
	cmp	r0, #LAN966x_PRIMARY_CPU
	moveq	r0, #1
	movne	r0, #0
	bx	lr
endfunc plat_is_my_cpu_primary


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
	error_pc: .asciz "\nPC at: 0x"
	panic_pc: .asciz "\nPanic at PC: 0x"
	error_no: .asciz "\nError: 0x"
	excp_no:  .asciz "\nException: 0x"

	/*
	 * plat_error_handler
	 * Show error value and program counter
	 * r0: error value argument
	 * lr: program counter at call time
	 * Clobbers: r4, r10, r11
	 */
func plat_error_handler
	mov	r10, r0
	mov	r11, lr
	ldr	r4, =error_no
	bl	asm_print_str
	mov	r4, r10
	bl	asm_print_hex
show_pc:
	ldr	r4, =error_pc
	bl	asm_print_str
	mov	r4, r11
	bl	asm_print_hex
wait_here:
	wfi
	b	wait_here
endfunc plat_error_handler

	/*
	 * plat_report_exception
	 * Show error value and program counter
	 * r0: error value argument
	 * r1: program counter at call time
	 * Clobbers: r4, r10, r11, r12
	 */
func plat_report_exception
	mov	r10, r0
	mov	r11, r1
	mov	r12, lr
	ldr	r4, =excp_no
	bl	asm_print_str
	mov	r4, r10
	bl	asm_print_hex
	mov	r0, r10
	mov	r1, r11
	bx	r12
endfunc plat_report_exception

	/*
	 * plat_panic_handler
	 * Use existing trace
	 * lr: program counter at call time
	 * Clobbers: r11
	 */
func plat_panic_handler
	mov	r11, lr
	b	show_pc
endfunc plat_panic_handler
