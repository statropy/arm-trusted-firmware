/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QSPI_H
#define QSPI_H

#include <stdint.h>

int qspi_write(uint32_t offset, const void *buf, size_t len);
void qspi_init(uintptr_t base);
void qspi_reinit(void);

#endif  /* QSPI_H */
