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

static void xdmac_start(int ch)
{
	/* Enable Block End Interrupt */
	mmio_setbits_32(XDMAC_XDMAC_CIE_CH0(CH_OFF(base, ch)), AT_XDMAC_CIE_BIE);
	/* Enable GIE channel Interrupt */
	mmio_setbits_32(XDMAC_XDMAC_GIE(base), BIT(ch));
	/* Enable Channel: GE */
	mmio_setbits_32(XDMAC_XDMAC_GE(base), BIT(ch));
}

static void xdmac_exec(int ch)
{
	xdmac_start(ch);
	xdmac_wait_idle(ch);
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
	xdmac_exec(ch);
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
	xdmac_exec(ch);
}

void xdmac_setup_xfer(int ch, void *dst, const void *src, size_t len, int dir, int periph)
{
	struct xdmac_req req;
	xdmac_make_req(&req, ch, dir, periph, (uintptr_t) dst, (uintptr_t) src, len);
	xdmac_setup_req(&req);
}

void xdmac_execute_xfers(uint32_t mask)
{
	uint32_t w;

	w = mask;
	while (w) {
		int i = __builtin_ffs(w) - 1;
		xdmac_start(i);
		w &= ~BIT(i);
	}

	w = mask;
	while (w) {
		int i = __builtin_ffs(w) - 1;
		xdmac_wait_idle(i);
		w &= ~BIT(i);
	}
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
