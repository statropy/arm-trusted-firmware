/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <platform_def.h>

	.weak	plat_get_my_stack
	.weak	plat_set_my_stack
	.weak	bl2_setup
	.weak	bl2_main

	/* -----------------------------------------------------
	 * unsigned long plat_get_my_stack ()
	 *
	 * For cold-boot BL images, only the primary CPU needs
	 * a stack. This function returns the stack pointer for
	 * a stack allocated in normal memory.
	 * -----------------------------------------------------
	 */
func plat_get_my_stack
	get_up_stack platform_normal_stacks, PLATFORM_STACK_SIZE
	ret
endfunc	plat_get_my_stack

	/* -----------------------------------------------------
	 * void plat_set_my_stack ()
	 *
	 * For cold-boot BL images, only the primary CPU needs
	 * a stack. This function sets the stack pointer to a
	 * stack allocated in normal memory.
	 * -----------------------------------------------------
	 */
func plat_set_my_stack
	/* GPR[0] = 0xcafebeef */
	ldr	x3, =LAN969X_CPU_BASE
	ldr	x2, =0xcafebeef
	str	x2, [x3,#0]

	get_up_stack platform_normal_stacks, PLATFORM_STACK_SIZE
	mov	sp, x0
	ret
endfunc plat_set_my_stack

	/* -----------------------------------------------------
	 * Platform stub functions
	 * -----------------------------------------------------
	 */
func bl2_setup
	ret
endfunc	bl2_setup
func bl2_main
	ret
endfunc	bl2_main

	/* -----------------------------------------------------
	 * Per-cpu stacks in normal memory. Each cpu gets a
	 * stack of PLATFORM_STACK_SIZE bytes.
	 * -----------------------------------------------------
	 */
declare_stack platform_normal_stacks, tzfw_normal_stacks, \
		PLATFORM_STACK_SIZE, 1, CACHE_WRITEBACK_GRANULE
