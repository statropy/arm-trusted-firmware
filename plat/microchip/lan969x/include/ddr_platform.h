/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __DDR_PLATFORM_H
#define __DDR_PLATFORM_H

#include <assert.h>
#include <common/debug.h>
#include <drivers/delay_timer.h>
#include <lib/mmio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define PANIC(...) do { ERROR(__VA_ARGS__); panic(); } while(0)

static inline void ddr_usleep(int us)
{
	udelay(us);
}

static inline void ddr_nsleep(int ns)
{
	ddr_usleep(1);
}

#if !defined(ARRAY_SIZE)
# define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#if !defined(FIELD_PREP)
#define __bf_shf(x) (__builtin_ffsll(x) - 1)
#define FIELD_PREP(_mask, _val)						\
	({								\
		((typeof(_mask))(_val) << __bf_shf(_mask)) & (_mask);	\
	})
#endif

#if !defined(FIELD_GET)
#define FIELD_GET(_mask, _reg)                                          \
        ({                                                              \
                (typeof(_mask))(((_reg) & (_mask)) >> __bf_shf(_mask)); \
        })
#endif

#endif /* __DDR_PLATFORM_H */
