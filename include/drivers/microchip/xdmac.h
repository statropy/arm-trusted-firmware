/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MICROCHIP_XDMAC
#define MICROCHIP_XDMAC

void xdmac_init(void);

void *xdmac_memset(void *dst, int val, size_t count);
void *xdmac_memcpy(void *dst, const void *src, size_t len);

#endif  /* MICROCHIP_XDMAC */
