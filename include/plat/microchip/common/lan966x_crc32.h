/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_CRC32_H
#define LAN966X_CRC32_H

#include <stdint.h>

uint32_t Crc32c(uint32_t crc, const void *data, size_t size);

#endif /* LAN966X_CRC32_H */
