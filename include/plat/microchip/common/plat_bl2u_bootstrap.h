/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_BL2U_BOOTSTRAP_H
#define LAN966X_BL2U_BOOTSTRAP_H

#include <lan96xx_common.h>

void lan966x_bl2u_bootstrap_monitor(void);

void lan966x_bl2u_io_init_dev(boot_source_type boot_source);

int lan966x_bl2u_fip_update(boot_source_type boot_source, uintptr_t buf, uint32_t len, bool verify);

int lan966x_bl2u_emmc_write(uint32_t offset, uintptr_t buf_ptr, uint32_t length, bool verify);

int lan966x_bl2u_qspi_verify(uint32_t offset, uintptr_t buf_ptr, size_t length);

#endif
