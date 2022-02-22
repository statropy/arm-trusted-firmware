
/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLAT_DUFF_MEMCPY_H
#define PLAT_DUFF_MEMCPY_H

#include <stddef.h>
#include <stdint.h>

size_t duff_qword_copy(uint128_t *dst_typed, const uint128_t *src_typed, size_t qword_count)
size_t duff_dword_copy(uint64_t *dst_typed, const uint64_t *src_typed, size_t dword_count);
size_t duff_word_copy(uint32_t *dst_typed, const uint32_t *src_typed, size_t word_count);
size_t duff_hword_copy(uint16_t *dst_typed, const uint16_t *src_typed, size_t hword_count);
size_t duff_byte_copy(uint8_t *dst_typed, const uint8_t *src_typed, size_t bytes);
void *duff_memcpy(void *dst, const void *src, size_t bytes);

#endif	/* PLAT_DUFF_MEMCPY_H */
