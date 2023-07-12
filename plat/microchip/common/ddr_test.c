/*
 * Copyright (C) 2023 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch_helpers.h>
#include <common/debug.h>
#include <ddr_init.h>
#include <ddr_test.h>
#include <lib/mmio.h>
#include <platform_def.h>

#define DDR_PATTERN1 0xAAAAAAAAU
#define DDR_PATTERN2 0x55555555U

/*******************************************************************************
 * This function tests the DDR data bus wiring.
 * This is inspired from the Data Bus Test algorithm written by Michael Barr
 * in "Programming Embedded Systems in C and C++" book.
 * resources.oreilly.com/examples/9781565923546/blob/master/Chapter6/
 * File: memtest.c - This source code belongs to Public Domain.
 * Returns 0 if success, and address value else.
 ******************************************************************************/
uintptr_t ddr_test_data_bus(uintptr_t ddr_base_addr, bool cache)
{
	uint32_t pattern;

	INFO("DDR data bus begin, start %08lx, cache %d\n", ddr_base_addr, cache);

	if (cache)
		inv_dcache_range(ddr_base_addr, PLATFORM_CACHE_LINE_SIZE);

	for (pattern = 1U; pattern != 0U; pattern <<= 1) {
		mmio_write_32(ddr_base_addr, pattern);

		if (mmio_read_32(ddr_base_addr) != pattern) {
			return (uintptr_t) ddr_base_addr;
		}
	}

	if (cache)
		clean_dcache_range(ddr_base_addr, PLATFORM_CACHE_LINE_SIZE);

	INFO("DDR data bus end\n");

	return 0;
}

/*******************************************************************************
 * This function tests the DDR address bus wiring.
 * This is inspired from the Data Bus Test algorithm written by Michael Barr
 * in "Programming Embedded Systems in C and C++" book.
 * resources.oreilly.com/examples/9781565923546/blob/master/Chapter6/
 * File: memtest.c - This source code belongs to Public Domain.
 * Returns 0 if success, and address value else.
 ******************************************************************************/
uintptr_t ddr_test_addr_bus(uintptr_t ddr_base_addr, size_t ddr_size, bool cache)
{
	size_t offset;
	size_t testoffset = 0;

	INFO("DDR addr bus begin, start %08lx, size 0x%08zx, cache %d\n",
	     ddr_base_addr, ddr_size, cache);

	if (cache)
		inv_dcache_range(ddr_base_addr, ddr_size);

	/* Write the default pattern at each of the power-of-two offsets. */
	for (offset = sizeof(uint32_t); offset < ddr_size; offset <<= 1U) {
		mmio_write_32(ddr_base_addr + offset, DDR_PATTERN1);
	}

	/* Check for address bits stuck high. */
	mmio_write_32(ddr_base_addr + testoffset, DDR_PATTERN2);

	for (offset = sizeof(uint32_t); offset < ddr_size; offset <<= 1U) {
		if (mmio_read_32(ddr_base_addr + offset) != DDR_PATTERN1) {
			return (ddr_base_addr + offset);
		}
	}

	mmio_write_32(ddr_base_addr + testoffset, DDR_PATTERN1);

	/* Check for address bits stuck low or shorted. */
	for (testoffset = sizeof(uint32_t); testoffset < ddr_size; testoffset <<= 1U) {
		mmio_write_32(ddr_base_addr + testoffset, DDR_PATTERN2);

		if (mmio_read_32(ddr_base_addr) != DDR_PATTERN1) {
			return ddr_base_addr;
		}

		for (offset = sizeof(uint32_t); offset < ddr_size; offset <<= 1U) {
			if ((mmio_read_32(ddr_base_addr + offset) != DDR_PATTERN1) &&
			    (offset != testoffset)) {
				return (ddr_base_addr + offset);
			}
		}

		mmio_write_32(ddr_base_addr + testoffset, DDR_PATTERN1);
	}

	if (cache)
		clean_dcache_range(ddr_base_addr, ddr_size);

	INFO("DDR addr bus end\n");

	return 0;
}

static inline unsigned int ps_rnd(unsigned int x)
{
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

uintptr_t ddr_test_rnd(uintptr_t ddr_base_addr, size_t ddr_size, bool cache, uint32_t seed)
{
	size_t offset;
	unsigned int value;

	if (cache)
		inv_dcache_range(ddr_base_addr, ddr_size);

	for (offset = 0, value = seed; offset < ddr_size; offset += sizeof(uint32_t)) {
		value = ps_rnd(value);
		mmio_write_32(ddr_base_addr + offset, (uint32_t) value);
	}

	for (offset = 0, value = seed; offset < ddr_size; offset += sizeof(uint32_t)) {
		value = ps_rnd(value);
		if (mmio_read_32(ddr_base_addr + offset) != (uint32_t) value) {
			return (ddr_base_addr + offset);
		}
	}

	if (cache)
		clean_dcache_range(ddr_base_addr, ddr_size);

	return 0;
}
