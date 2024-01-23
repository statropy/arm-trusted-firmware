/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/delay_timer.h>
#include <drivers/microchip/aes.h>
#include <drivers/microchip/sha.h>
#include <drivers/microchip/xdmac.h>
#include <lib/libc/errno.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>
#include <platform_def.h>
#include <tools_share/firmware_encrypted.h>

#include "lan966x_regs.h"
#include "xdmac_priv.h"

#if defined(LAN966X_XDMAC_BASE)
static uintptr_t base = LAN966X_XDMAC_BASE;
#elif defined(LAN969X_XDMAC_BASE)
static uintptr_t base = LAN969X_XDMAC_BASE;
#endif

#define CH_SZ		(XDMAC_XDMAC_CIE_CH1(0) - XDMAC_XDMAC_CIE_CH0(0))
#define CH_OFF(b, c)	(b + (c * CH_SZ))

#define MAX_TIMEOUT_US	(400 * 1000U)	/* 400ms */

#define MAX_CHANNEL	16U

static inline int xdmac_compute_cc(int dir, int periph)
{
	int cc = 0;

	if (dir == XDMA_DIR_DEV_TO_MEM) {
		cc =
			XDMAC_XDMAC_CC_CH0_DAM_CH0(AT_XDMAC_CC_DAM_INCREMENTED_AM) |
			XDMAC_XDMAC_CC_CH0_MBSIZE_CH0(AT_XDMAC_CC_MBSIZE_SIXTEEN) |
			XDMAC_XDMAC_CC_CH0_TYPE_CH0(AT_XDMAC_CC_TYPE_PER_TRAN) ;
	} else if (dir == XDMA_DIR_MEM_TO_DEV) {
		cc =
			XDMAC_XDMAC_CC_CH0_SAM_CH0(AT_XDMAC_CC_SAM_INCREMENTED_AM) |
			XDMAC_XDMAC_CC_CH0_MBSIZE_CH0(AT_XDMAC_CC_MBSIZE_SIXTEEN) |
			XDMAC_XDMAC_CC_CH0_DSYNC_CH0(1U) |
			XDMAC_XDMAC_CC_CH0_TYPE_CH0(AT_XDMAC_CC_TYPE_PER_TRAN) ;
	} else if (dir == XDMA_DIR_MEM_TO_MEM) {
		cc =
			XDMAC_XDMAC_CC_CH0_TYPE_CH0(AT_XDMAC_CC_TYPE_MEM_TRAN)		|
			XDMAC_XDMAC_CC_CH0_SAM_CH0(AT_XDMAC_CC_SAM_INCREMENTED_AM)	|
			XDMAC_XDMAC_CC_CH0_DAM_CH0(AT_XDMAC_CC_DAM_INCREMENTED_AM)	|
			XDMAC_XDMAC_CC_CH0_MBSIZE_CH0(AT_XDMAC_CC_MBSIZE_SIXTEEN);
	} else if (dir == XDMA_DIR_BZERO) {
		cc =
			XDMAC_XDMAC_CC_CH0_TYPE_CH0(AT_XDMAC_CC_TYPE_MEM_TRAN)		|
			XDMAC_XDMAC_CC_CH0_MEMSET_CH0(AT_XDMAC_CC_MEMSET_HW_MODE)	|
			XDMAC_XDMAC_CC_CH0_DAM_CH0(AT_XDMAC_CC_DAM_UBS_AM)		|
			XDMAC_XDMAC_CC_CH0_SAM_CH0(AT_XDMAC_CC_SAM_INCREMENTED_AM)	|
			XDMAC_XDMAC_CC_CH0_MBSIZE_CH0(AT_XDMAC_CC_MBSIZE_SIXTEEN);

	}

	cc |= XDMAC_XDMAC_CC_CH0_PERID_CH0(periph);

	return cc;
}

void xdmac_make_req(struct xdmac_req *req, int ch, int dir, int periph, uintptr_t dst, uintptr_t src, size_t len)
{
	req->ch = ch;
	req->dir = dir;
	req->periph = periph;
	req->dst = dst;
	req->src = src;
	req->len = len;
}

static uint32_t xdmac_setup_req(const struct xdmac_req *req)
{
	int ch = req->ch;
	int csize = AT_XDMAC_CSIZE_16;
	int dwidth;
	uint32_t cfg;

	VERBOSE("%d: dir %d periph %d dst %08x src %08x len %d\n",
		req->ch, req->dir, req->periph,
		req->dst, req->src, req->len);

	assert(req->len <= AT_XDMAC_MBR_UBC_UBLEN_MAX);

	if (req->periph == XDMA_SHA_TX) {
		dwidth = AT_XDMAC_CC_DWIDTH_WORD;
	} else if (req->periph == XDMA_AES_RX || req->periph == XDMA_AES_TX) {
		dwidth = AT_XDMAC_CC_DWIDTH_WORD;
		csize = AT_XDMAC_CSIZE_4; /* DS mandates this for CTR, GCM */
	} else {
		dwidth = xdmac_align_width(req->src | req->dst);
		if (!is_aligned(req->len, 1 << dwidth))
			dwidth = AT_XDMAC_CC_DWIDTH_BYTE;
	}
	cfg = XDMAC_XDMAC_CC_CH0_DWIDTH_CH0(dwidth) |
		XDMAC_XDMAC_CC_CH0_CSIZE_CH0(csize) |
		xdmac_compute_cc(req->dir, req->periph);

	/* Disable channel by Global Channel Disable Register */
	mmio_write_32(XDMAC_XDMAC_GD(base), BIT(ch));

	/* Clear pending irq(s) by reading channel status register */
	(void) mmio_read_32(XDMAC_XDMAC_CIS_CH0(CH_OFF(base, ch)));

	/* Set up transfer registers */
	mmio_write_32(XDMAC_XDMAC_CDA_CH0(CH_OFF(base, ch)), req->dst);
	mmio_write_32(XDMAC_XDMAC_CSA_CH0(CH_OFF(base, ch)), req->src);
	mmio_write_32(XDMAC_XDMAC_CDS_MSP_CH0(CH_OFF(base, ch)), 0); /* Used for bzero */
	mmio_write_32(XDMAC_XDMAC_CUBC_CH0(CH_OFF(base, ch)), req->len >> dwidth);
	mmio_write_32(XDMAC_XDMAC_CC_CH0(CH_OFF(base, ch)), cfg);

	return BIT(ch);		/* Return channel mask to wait for */
}

static void xdmac_wait_idle(int ch)
{
	uint64_t timeout;
	uint32_t w;

	/* Wait not busy */
	timeout = timeout_init_us(MAX_TIMEOUT_US);
	while (true) {
		w = mmio_read_32(XDMAC_XDMAC_GS(base));
		VERBOSE("XDMAC: GS: %08x\n", w);
		if ((w & BIT(ch)) == 0)
			break;	/* Not busy, continue */
		if (timeout_elapsed(timeout)) {
			ERROR("XDMAC(%d): Timeout awaiting GS_STX clear, 0x%08x\n", ch, w);
			plat_error_handler(-ETIMEDOUT);
		}
	}

	/* Check channel status */
	w = mmio_read_32(XDMAC_XDMAC_CIS_CH0(CH_OFF(base, ch)));
	VERBOSE("XDMAC: CIS(%d): %08x\n", ch, w);
	if (w & AT_XDMAC_CIS_BIS)
		return;	/* Block End Irq: We're done */
	if (w & AT_XDMAC_CIS_ERROR) {
		ERROR("XDMAC(%d): Transfer error: %08x\n", ch, w);
		plat_error_handler(-EIO);
	}

	ERROR("XDMAC(%d): Channel has no status: %08x\n", ch, w);
	plat_error_handler(-EIO);
}

static void xdmac_start(uint32_t channel_list)
{
	uint32_t w = channel_list;

	/* Set Enable Block End Interrupt for channels */
	while (w) {
		int i = __builtin_ffs(w) - 1;
		mmio_setbits_32(XDMAC_XDMAC_CIE_CH0(CH_OFF(base, i)), AT_XDMAC_CIE_BIE);
		w &= ~BIT(i);
	}

	/* Enable GIE channel Interrupts - write-only register */
	mmio_write_32(XDMAC_XDMAC_GIE(base), channel_list);
	/* Enable Channels: GE - write-only register */
	mmio_write_32(XDMAC_XDMAC_GE(base), channel_list);
}

/* channel_list: Bitmask of channels to complete */
void xdmac_execute_xfers(uint32_t channel_list)
{
	/* First start the channels */
	xdmac_start(channel_list);

	while (channel_list) {
		int i = __builtin_ffs(channel_list) - 1;
		xdmac_wait_idle(i);
		channel_list &= ~BIT(i);
	}
}

void xdmac_bzero(void *dst, size_t len)
{
	int ch = 0; /* Always use channel 0 */
	uintptr_t dst_va = (uintptr_t) dst;
	struct xdmac_req req;

	/* Cache cleaning, XDMAC is *not* cache aware */
	inv_dcache_range(dst_va, len);

	xdmac_make_req(&req, ch, XDMA_DIR_BZERO, XDMA_NONE, dst_va, 0, len);
	xdmac_setup_req(&req);

	/* Start and await the operation completion */
	xdmac_execute_xfers(BIT(ch));
}

static void _xdmac_memcpy(int ch, void *dst, const void *src, size_t len, int dir, int periph)
{
	uintptr_t src_va = (uintptr_t) src;
	uintptr_t dst_va = (uintptr_t) dst;

	/* Cache cleaning, XDMAC is *not* cache aware */
	if (dir == XDMA_DIR_MEM_TO_DEV || dir == XDMA_DIR_MEM_TO_MEM) {
		flush_dcache_range(src_va, len);
	}
	if (dir == XDMA_DIR_DEV_TO_MEM || dir == XDMA_DIR_MEM_TO_MEM) {
		inv_dcache_range(dst_va, len);
	}

	struct xdmac_req req;
	xdmac_make_req(&req, ch, dir, periph, dst_va, src_va, len);
	xdmac_setup_req(&req);

	/* Start and await the operation completion */
	xdmac_execute_xfers(BIT(ch));
}

void xdmac_setup_xfer(int ch, void *dst, const void *src, size_t len, int dir, int periph)
{
	struct xdmac_req req;
	xdmac_make_req(&req, ch, dir, periph, (uintptr_t) dst, (uintptr_t) src, len);
	xdmac_setup_req(&req);
}

void xdmac_memcpy(void *dst, const void *src, size_t len, int dir, int periph)
{
	_xdmac_memcpy(0, dst, src, len, dir, periph);
}

void xdmac_show_version(void)
{
	uint32_t w = mmio_read_32(XDMAC_XDMAC_VERSION(base));
	INFO("XDMAC: version 0x%x, mfn %d\n",
	     (uint32_t) XDMAC_XDMAC_VERSION_VERSION_X(w),
	     (uint32_t) XDMAC_XDMAC_VERSION_MFN_X(w));
}

#if defined(XDMAC_PIPELINE_SUPPPORT)

#define PDMA_BLOCK_SIZE	SIZE_K(16U)

static struct pipeline_state {
	uintptr_t dst, src;
	size_t len;
	int sha_type;
	uint8_t hash[32];
	/* Decryption */
	bool streaming_decrypt, was_decrypted;
	struct {
		struct fw_enc_hdr fw_hdr;
		uint8_t key[ENC_MAX_KEY_SIZE];
		size_t key_len;
	} decrypt;
} xdma_pipeline;

static void _xdmac_qspi_pipeline_read(void *dst, const void *src, size_t len)
{
	struct pipeline_state *pdma = &xdma_pipeline;
	struct xdmac_req dmas[4],
		*mem = &dmas[0],
		*dec_tx = &dmas[1],
		*dec_rx = &dmas[2],
		*sha = &dmas[3];
	size_t read = 0, decrypted = 0, hashed = 0;

	pdma->src = (uintptr_t) src;
	pdma->dst = (uintptr_t) dst;
	pdma->len = len;
	/* We make an educated guess on which SHA may be requested -
	 * since we are calculating this ahead of time. */
	pdma->sha_type = SHA_MR_ALGO_SHA256;

	/* Invalidate dst cache */
	inv_dcache_range(pdma->dst, len);

	/* Setup DMA chain */
	xdmac_make_req(mem, 0, XDMA_DIR_MEM_TO_MEM, XDMA_NONE, pdma->dst, pdma->src, len);
	xdmac_make_req(dec_tx, 1, XDMA_DIR_MEM_TO_DEV, XDMA_AES_TX, AES_AES_IDATAR0(LAN966X_AES_BASE), pdma->dst, len);
	xdmac_make_req(dec_rx, 2, XDMA_DIR_DEV_TO_MEM, XDMA_AES_RX, pdma->dst, AES_AES_ODATAR0(LAN966X_AES_BASE), len);
	xdmac_make_req(sha, 3, XDMA_DIR_MEM_TO_DEV, XDMA_SHA_TX, SHA_SHA_IDATAR0(LAN966X_SHA_BASE), pdma->dst, len);

	void *sha_ctx = sha_calc_init(pdma->sha_type, len, sizeof(pdma->hash));

	if (pdma->streaming_decrypt) {
		aes_gcm_decrypt_start(len,
				      pdma->decrypt.key, pdma->decrypt.key_len,
				      pdma->decrypt.fw_hdr.iv, pdma->decrypt.fw_hdr.iv_len);
	}

	while (hashed < len) {
		uint32_t channel_list = 0;

		/* Need to read more data? */
		if (read < len) {
			mem->len = MIN((size_t) PDMA_BLOCK_SIZE, len - read);
			/* Ready QSPI read to memory block transfer */
			channel_list |= xdmac_setup_req(mem);
			VERBOSE("QSPI read start: %d bytes, DST offset %08x\n", mem->len, mem->dst);
		}

		if (pdma->streaming_decrypt) {
			/* Need to decrypt data? */
			if (decrypted < read) {
				uint32_t chunk = MIN((size_t) PDMA_BLOCK_SIZE, read - decrypted);
				/* Ready tx/rx inplace decrypt */
				dec_tx->len = chunk;
				channel_list |= xdmac_setup_req(dec_tx);
				dec_rx->len = chunk;
				channel_list |= xdmac_setup_req(dec_rx);
				VERBOSE("AES decrypt start: %d bytes, SRC offset %08x\n", dec_tx->len, dec_tx->src);
			}
		} else {
			decrypted = read; /* Cleartext */
		}

		/* Need to hash data? */
		if (hashed < decrypted) {
			sha->len = MIN((size_t) PDMA_BLOCK_SIZE, decrypted - hashed);
			/* Ready SHA update round */
			channel_list |= xdmac_setup_req(sha);
			VERBOSE("SHA hash start: %d bytes, SRC offset %08x\n", sha->len, sha->src);
		}

		assert(channel_list != 0); /* Inside loop we always have xfers to do */

		VERBOSE("DMA start: mask %02x: read %zd, decrypted %zd, hashed %zd\n",
			channel_list, read, decrypted, hashed);

		xdmac_execute_xfers(channel_list);

		/* Update read state */
		if (channel_list & BIT(mem->ch)) {
			VERBOSE("QSPI read done: %d bytes, DST offset %08x\n", mem->len, mem->dst);
			read += mem->len;
			mem->dst += mem->len;
			mem->src += mem->len;
		}

		/* Update SHA hash state */
		if (channel_list & BIT(dec_tx->ch)) {
			VERBOSE("AES decrypt done: %d bytes, SRC offset %08x\n", dec_tx->len, dec_tx->src);
			decrypted += dec_tx->len;
			dec_tx->src += dec_tx->len;
			dec_rx->dst += dec_tx->len;
		}

		/* Update SHA hash state */
		if (channel_list & BIT(sha->ch)) {
			VERBOSE("SHA hash done: %d bytes, SRC offset %08x\n", sha->len, sha->src);
			hashed += sha->len;
			sha->src += sha->len;
		}
	}

	if (pdma->streaming_decrypt) {
		pdma->was_decrypted =
			aes_gcm_decrypt_finish(pdma->decrypt.fw_hdr.tag, pdma->decrypt.fw_hdr.tag_len) == 0;
	}

	sha_calc_finish(sha_ctx, pdma->hash);
}

void xdmac_qspi_pipeline_read(void *dst, const void *src, size_t len)
{
	struct pipeline_state *pdma = &xdma_pipeline;
	if (len >= 2*PDMA_BLOCK_SIZE) {
		_xdmac_qspi_pipeline_read(dst, src, len);
	} else {
		pdma->len = 0;	/* Invalid */
		_xdmac_memcpy(0, dst, src, len, XDMA_DIR_MEM_TO_MEM, XDMA_QSPI0_RX);
	}
}

void plat_decrypt_context_enter(const struct fw_enc_hdr *hdr,
				const uint8_t *key, size_t key_len, unsigned int key_flags)
{
	struct pipeline_state *pdma = &xdma_pipeline;
	assert(key_len <= ENC_MAX_KEY_SIZE);
	pdma->streaming_decrypt = true;
	pdma->was_decrypted = false;
	pdma->decrypt.key_len = key_len;
	memcpy(pdma->decrypt.key, key, key_len);
	pdma->decrypt.fw_hdr = *hdr;
}

void plat_decrypt_context_leave(void)
{
	struct pipeline_state *pdma = &xdma_pipeline;
	pdma->streaming_decrypt = false;
	/* Clear all metadata and key */
	memset(&pdma->decrypt, 0, sizeof(pdma->decrypt));
}

int xdmac_qspi_get_sha(const void *data, size_t len, int sha_type, void *hash, size_t hash_len)
{
	struct pipeline_state *pdma = &xdma_pipeline;

	if (!pdma->streaming_decrypt &&
	    len > 0 &&
	    pdma->len == len &&
	    pdma->sha_type == sha_type &&
	    pdma->dst == (uintptr_t) data &&
	    sizeof(pdma->hash) <= hash_len) {
		memcpy(hash, pdma->hash, sizeof(pdma->hash));
		return 0;
	}

	/* No data */
	return -1;
}

bool xdmac_qspi_is_decrypted(const void *dst, size_t len,
			     const void *key, unsigned int key_len,
			     const void *tag, unsigned int tag_len)
{
	struct pipeline_state *pdma = &xdma_pipeline;

	if (pdma->was_decrypted &&
	    pdma->len == len &&
	    pdma->dst == (uintptr_t) dst &&
	    pdma->decrypt.key_len == key_len &&
	    memcmp(pdma->decrypt.key, key, key_len) == 0 &&
	    pdma->decrypt.fw_hdr.tag_len == tag_len &&
	    memcmp(pdma->decrypt.fw_hdr.tag, tag, tag_len) == 0) {
		/* Data is ALREADY decrypted */
		return true;
	}
	/* Data is NOT decrypted */
	return false;
}
#endif
