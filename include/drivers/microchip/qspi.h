/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QSPI_H
#define QSPI_H

#include <stdint.h>

void qspi_init(void);
void qspi_reinit(void);

/*
 * Platform can implement this to override default QSPI clock setup.
 *
 */
void plat_qspi_init_clock(void);

#endif  /* QSPI_H */
