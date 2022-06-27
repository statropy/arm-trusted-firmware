/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QSPI_H
#define QSPI_H

#include <stdint.h>

/* Default clock speed */
#define QSPI_DEFAULT_SPEED_MHZ	25

void qspi_init(void);
void qspi_reinit(void);
int qspi_write(uint32_t offset, const void *buf, size_t len);

/*
 * Platform can implement this to override default QSPI clock setup.
 *
 */
void plat_qspi_init_clock(void);

#endif  /* QSPI_H */
