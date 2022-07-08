/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <platform_def.h>

#include <common/debug.h>
#include <drivers/io/io_driver.h>
#include <drivers/io/io_mtd.h>
#include <lib/utils.h>

int qspi_write(uint32_t offset, const void *buf, size_t len)
{
	return -ENOTSUP;
}

void qspi_init(void)
{
}

void qspi_reinit(void)
{
}
