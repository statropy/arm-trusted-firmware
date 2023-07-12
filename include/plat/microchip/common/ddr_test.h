/*
 * Copyright (C) 2023 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _DDR_TEST_H
#define _DDR_TEST_H

#include <stdint.h>
#include <stdbool.h>

uintptr_t ddr_test_data_bus(uintptr_t ddr_base_addr, bool cache);
uintptr_t ddr_test_addr_bus(uintptr_t ddr_base_addr, size_t ddr_size, bool cache);
uintptr_t ddr_test_rnd(uintptr_t ddr_base_addr, size_t ddr_size, bool cache, uint32_t seed);

#endif /* _DDR_TEST_H */
