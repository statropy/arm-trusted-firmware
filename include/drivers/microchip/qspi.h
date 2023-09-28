/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QSPI_H
#define QSPI_H

#include <stdint.h>

/* Default clock speed */
#define QSPI_DEFAULT_SPEED_MHZ	25U
#define QSPI_HS_SPEED_MHZ	100U

int qspi_init(void);
void qspi_reinit(void);
int qspi_write(uint32_t offset, const void *buf, size_t len);
int qspi_read(unsigned int offset, uintptr_t buffer, size_t length,
	      size_t *length_read);
unsigned int qspi_get_spi_mode(void);

/*
 * Platform can implement this to override default QSPI clock setup.
 *
 */
void plat_qspi_init_clock(void);

/*
 * Platform can implement this to override default QSPI mode
 *
 */
int plat_qspi_default_mode(void);

/*
 * Platform can implement this to override default QSPI clock speed
 *
 */
uint32_t plat_qspi_default_clock_mhz(void);


#endif  /* QSPI_H */
