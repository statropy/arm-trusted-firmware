/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QSPI_H
#define QSPI_H

#include <stdint.h>

void qspi_init(uintptr_t base, size_t len);
int qspi_set_dly(uint32_t delay);
void qspi_plat_configure(void);

#endif  /* QSPI_H */
