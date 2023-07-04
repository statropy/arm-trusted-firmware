/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _DDR_INIT_H
#define _DDR_INIT_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <ddr_config.h>

int ddr_reset(const struct ddr_config *cfg , bool assert) __attribute__ ((warn_unused_result));

int ddr_init(const struct ddr_config *cfg) __attribute__ ((warn_unused_result));

#endif /* _DDR_INIT_H */
