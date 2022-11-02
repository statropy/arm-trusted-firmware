/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_PRIVATE_H
#define LAN966X_PRIVATE_H

#include <stdint.h>
#include <plat_crypto.h>
#include <lan96xx_common.h>

typedef struct {
	void *fw_config;
	void *mbedtls_heap_addr;
	size_t mbedtls_heap_size;
} shared_memory_desc_t;

extern shared_memory_desc_t shared_memory_desc;

void lan966x_set_max_trace_level(void);
void lan966x_reset_max_trace_level(void);

void lan966x_console_init(void);
void lan966x_timer_init(void);
void lan966x_ddr_init(void);
void lan966x_tz_init(void);
void lan966x_tz_finish(void);
void lan966x_crash_console(console_t *console);

#if defined(LAN966X_AES_TESTS)
void lan966x_crypto_tests(void);
#endif

void lan966x_crypto_ecdsa_tests(void);

void lan966x_mbed_heap_set(shared_memory_desc_t *d);

#if defined(LAN966X_EMMC_TESTS)
void lan966x_emmc_tests(void);
#endif

#endif /* LAN966X_PRIVATE_H */
