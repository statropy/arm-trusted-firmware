/*
 * Copyright (c) 2016-2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <asm_macros.S>
#include <platform_def.h>
#include <smccc_helpers.h>
#include <smccc_macros.S>

	.globl	plat_bootstrap_exec

func plat_bootstrap_exec
	/*
	 * Get the smc_context for next BL image,
	 * program the gp/system registers and save it in `r4`.
	 */
	bl	smc_get_next_ctx
	mov	r4, r0

	/* Only turn-off MMU if going to secure world */
	ldr	r5, [r4, #SMC_CTX_SCR]
	tst	r5, #SCR_NS_BIT
	bne	skip_mmu_off

	/*
	 * MMU needs to be disabled because both BL1 and BL2/BL2U execute
	 * in PL1, and therefore share the same address space.
	 * BL2/BL2U will initialize the address space according to its
	 * own requirement.
	 */
	bl	disable_mmu_icache_secure
	stcopr	r0, TLBIALL
	dsb	sy
	isb

skip_mmu_off:
	/* Restore smc_context from `r4` and exit secure monitor mode. */
	mov	r0, r4
	monitor_exit
endfunc plat_bootstrap_exec
