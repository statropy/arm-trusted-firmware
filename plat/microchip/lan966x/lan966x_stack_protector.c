/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>

#include <arch_helpers.h>
#include <plat/common/platform.h>

#define RANDOM_CANARY_VALUE ((u_register_t) 3288486913995823360ULL)

u_register_t plat_get_stack_protector_canary(void)
{
	/*
	 * Ideally, a random number should be returned instead of the
	 * combination of a timer's value and a compile-time constant.
	 */
	return RANDOM_CANARY_VALUE ^ read_cntpct_el0();
}
