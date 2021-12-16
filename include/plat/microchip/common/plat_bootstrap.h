
/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLAT_BOOTSTRAP_H
#define PLAT_BOOTSTRAP_H

void plat_bl1_bootstrap_monitor(void);

void plat_bootstrap_set_strapping(uint8_t value);
void plat_bootstrap_trigger_fwu(void);
void plat_bootstrap_io_enable_ram_fip(size_t offset, size_t length);

#endif	/* PLAT_BOOTSTRAP_H */
