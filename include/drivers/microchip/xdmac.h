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

#if defined(XDMAC_PIPELINE_SUPPPORT)
void xdmac_qspi_pipeline_read(void *dst, const void *src, size_t len);
int xdmac_qspi_get_sha(const void *data, size_t len, int sha_type, void *hash, size_t hash_len);
bool xdmac_qspi_is_decrypted(const void *dst, size_t len,
			     const void *key, unsigned int key_len,
			     const void *tag, unsigned int tag_len);
#else
static inline
int xdmac_qspi_get_sha(const void *data, size_t len, int sha_type, void *hash, size_t hash_len)
{
	return -1;
}
static inline
bool xdmac_qspi_is_decrypted(const void *dst, size_t len,
			     const void *key, unsigned int key_len,
			     const void *tag, unsigned int tag_len)
{
	return false;
}
#endif

void xdmac_make_req(struct xdmac_req *req, int ch, int dir, int periph, uintptr_t dst, uintptr_t src, size_t len);
void xdmac_setup_xfer(int ch, void *dst, const void *src, size_t len, int dir, int periph);
void xdmac_execute_xfers(uint32_t mask);

#endif  /* MICROCHIP_XDMAC */
