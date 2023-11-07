/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MICROCHIP_XDMAC
#define MICROCHIP_XDMAC

void xdmac_show_version(void);

/* Cache handling */
enum {
	XDMA_FROM_MEM = 1,
	XDMA_TO_MEM = 2,
};

/* Peripheral IDs */
enum {
#if defined(MCHP_SOC_LAN969X)
	XDMA_QSPI0_RX = 0,
	XDMA_QSPI0_TX,
	XDMA_FC0_RX,
	XDMA_FC0_TX,
	XDMA_FC1_RX,
	XDMA_FC1_TX,
	XDMA_FC2_RX,
	XDMA_FC2_TX,
	XDMA_FC3_RX,
	XDMA_FC3_TX,
	XDMA_AES_TX,
	XDMA_AES_RX,
	XDMA_QSPI2_RX,
	XDMA_QSPI2_TX,
	XDMA_NONE = 0x7f,
#endif
};

void *xdmac_memset(void *dst, int val, size_t count);
void *xdmac_memcpy(void *dst, const void *src, size_t len, int flags, int periph);

#endif  /* MICROCHIP_XDMAC */
