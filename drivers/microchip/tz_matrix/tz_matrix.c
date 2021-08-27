// SPDX-License-Identifier: BSD-Source-Code
/*
 * Copyright (c) 2013, Atmel Corporation
 * Copyright (c) 2017, Timesys Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include <assert.h>
#include <lib/mmio.h>
#include <drivers/microchip/tz_matrix.h>

static uintptr_t reg_base = LAN966X_HMATRIX2_BASE;

static void matrix_write(unsigned int offset,
			 const uint32_t value)
{
	mmio_write_32(reg_base + offset, value);
}

#if 0
static uint32_t matrix_read(unsigned int offset)
{
	return mmio_read_32(reg_base + offset);
}
#endif

void matrix_write_protect_enable(void)
{
	matrix_write(MATRIX_WPMR,
		     (MATRIX_WPMR_WPKEY_PASSWD | MATRIX_WPMR_WPEN_ENABLE));
}

void matrix_write_protect_disable(void)
{
	matrix_write(MATRIX_WPMR, MATRIX_WPMR_WPKEY_PASSWD);
}

void matrix_configure_slave_security(unsigned int slave,
				     uint32_t srtop_setting,
				     uint32_t srsplit_setting,
				     uint32_t ssr_setting)
{
	matrix_write(MATRIX_SSR(slave), ssr_setting);
	matrix_write(MATRIX_SRTSR(slave), srtop_setting);
	matrix_write(MATRIX_SASSR(slave), srsplit_setting);
}

void matrix_configure_srtop(unsigned int slave,
			    uint32_t srtop_setting)
{
	matrix_write(MATRIX_SRTSR(slave), srtop_setting);
}
