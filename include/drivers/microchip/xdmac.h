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
	XDMA_DIR_MEM_TO_DEV,
	XDMA_DIR_DEV_TO_MEM,
	XDMA_DIR_MEM_TO_MEM,
	XDMA_DIR_BZERO,
};

/* Peripheral IDs */
enum {
#if defined(MCHP_SOC_LAN966X)
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
	XDMA_FC4_RX,
	XDMA_FC4_TX,
	XDMA_AES_TX,
	XDMA_AES_RX,
	XDMA_SHA_TX,
	XDMA_QSPI1_RX,
	XDMA_QSPI1_TX,
	XDMA_QSPI2_RX,
	XDMA_QSPI2_TX,
#elif defined(MCHP_SOC_LAN969X)
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
	XDMA_SHA_TX,
	XDMA_QSPI2_RX,
	XDMA_QSPI2_TX,
#endif
	XDMA_NONE = 0x7f,
};

struct xdmac_req {
	uint8_t ch;
	uint8_t dir;
	uint8_t periph;
	uint32_t dst;
	uint32_t src;
	uint32_t len;
};

void xdmac_bzero(void *dst, size_t count);
void xdmac_memcpy(void *dst, const void *src, size_t len, int dir, int periph);

#endif  /* MICROCHIP_XDMAC */
