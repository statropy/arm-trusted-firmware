/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <lib/mmio.h>
#include <drivers/microchip/xdmac.h>
#include <drivers/delay_timer.h>

#include "lan966x_regs.h"
#include "xdmac_priv.h"

static uintptr_t base = LAN969X_XDMAC_BASE;

#define CH_SZ		(XDMAC_XDMAC_CIE_CH1(0) - XDMAC_XDMAC_CIE_CH0(0))
#define CH_OFF(b, c)	(b + (c * CH_SZ))

static uint32_t cc_memset =
	XDMAC_XDMAC_CC_CH0_PERID_CH0(0x7f)				|
	XDMAC_XDMAC_CC_CH0_TYPE_CH0(AT_XDMAC_CC_TYPE_MEM_TRAN)  	|
	XDMAC_XDMAC_CC_CH0_MEMSET_CH0(AT_XDMAC_CC_MEMSET_HW_MODE)	|
	XDMAC_XDMAC_CC_CH0_DAM_CH0(AT_XDMAC_CC_DAM_UBS_AM)		|
	XDMAC_XDMAC_CC_CH0_SAM_CH0(AT_XDMAC_CC_SAM_INCREMENTED_AM)	|
	XDMAC_XDMAC_CC_CH0_MBSIZE_CH0(AT_XDMAC_CC_MBSIZE_SIXTEEN);

static uint32_t cc_memcpy =
	XDMAC_XDMAC_CC_CH0_PERID_CH0(0x7f)				|
	XDMAC_XDMAC_CC_CH0_TYPE_CH0(AT_XDMAC_CC_TYPE_MEM_TRAN)		|
	XDMAC_XDMAC_CC_CH0_SAM_CH0(AT_XDMAC_CC_SAM_INCREMENTED_AM)	|
	XDMAC_XDMAC_CC_CH0_DAM_CH0(AT_XDMAC_CC_DAM_INCREMENTED_AM)	|
	XDMAC_XDMAC_CC_CH0_MBSIZE_CH0(AT_XDMAC_CC_MBSIZE_SIXTEEN);

#define MAX_TIMOUT	5

static void xdmac_channel_clear(uint8_t ch)
{
	uint32_t w;

	mmio_write_32(XDMAC_XDMAC_GD(base), BIT(ch));
	w = mmio_read_32(XDMAC_XDMAC_CIS_CH0(CH_OFF(base, ch)));
	INFO("CH%d: CIS = %08x\n", ch, w);
}

static int xdmac_go(uint8_t ch,
		    uint32_t dst, uint32_t src,
		    uint32_t cc, uint32_t cds_msp,
		    uint32_t align, uint32_t len)
{
	int i;
	uint32_t w, dwidth, cubc;

	/* Determine data width */
	dwidth = xdmac_align_width(align);
	cc |= XDMAC_XDMAC_CC_CH0_DWIDTH_CH0(dwidth);

	/* Convert length to *one* microblock by data width */
	cubc = len >> dwidth;
	assert(cubc <= AT_XDMAC_MBR_UBC_UBLEN_MAX);

	INFO("DMA %d algn %x => dwidth %d bytes cubc %d blks, cc = 0x%08x\n", len, align, 1 << dwidth, cubc, cc);

	/* Clear channel */
	xdmac_channel_clear(ch);

	/* Set up transfer registers */
	mmio_write_32(XDMAC_XDMAC_CDA_CH0(CH_OFF(base, ch)), dst);
	mmio_write_32(XDMAC_XDMAC_CSA_CH0(CH_OFF(base, ch)), src);
	mmio_write_32(XDMAC_XDMAC_CDS_MSP_CH0(CH_OFF(base, ch)), cds_msp);
	mmio_write_32(XDMAC_XDMAC_CUBC_CH0(CH_OFF(base, ch)), cubc);
	mmio_write_32(XDMAC_XDMAC_CC_CH0(CH_OFF(base, ch)), cc);

	/* Enable Block End Interrupt */
	mmio_setbits_32(XDMAC_XDMAC_CIE_CH0(CH_OFF(base, ch)), AT_XDMAC_CIE_BIE);
	/* Enable GIE channel Interrupt */
	mmio_setbits_32(XDMAC_XDMAC_GIE(base), BIT(ch));
	/* Enable Channel: GE */
	mmio_setbits_32(XDMAC_XDMAC_GE(base), BIT(ch));

	/* Wait not busy */
	i = 0;
	while (true) {
		w = mmio_read_32(XDMAC_XDMAC_GS(base));
		INFO("XDMAC: GS: %08x\n", w);
		if ((w & BIT(ch)) == 0)
			break;	/* Not busy, continue */
		if (++i == MAX_TIMOUT) {
			ERROR("XDMAC: Timout awaiting GS_STX\n");
			return -1;
		}
		/* Continue polling */
		udelay(1);
	}

	/* Wait EOL/error */
	i = 0;
	while (true) {
		w = mmio_read_32(XDMAC_XDMAC_CIS_CH0(CH_OFF(base, ch)));
		INFO("XDMAC: CIS(%d): %08x\n", ch, w);
		if (w & AT_XDMAC_CIS_LIS)
			break;
		if (w & AT_XDMAC_CIS_ERROR) {
			ERROR("XDMAC: Copy error: %08x\n", w);
			return -1;
		}
		if (++i == MAX_TIMOUT) {
			ERROR("XDMAC: Timout awaiting CIS_LIS\n");
			return -1;
		}
		/* Continue polling */
		udelay(1);
	}

	return 0;
}

void *xdmac_memset(void *_dst, int val, size_t len)
{
	uint8_t ch = 0, b = val;
	uint32_t dst, msp;

	/* Cache cleaning, XDMAC is *not* cache aware */
	inv_dcache_range((uintptr_t) _dst, len);

	/* Convert args from 64 bit */
	dst = (uint64_t) _dst;

	/* Pattern */
	msp = b | (b << 8) | (b << 16) | (b << 24);

	/* Start the operation */
	return xdmac_go(ch,
			dst, 0,
			cc_memset, msp,
			(dst | len), len) ? NULL : _dst;
}

void *xdmac_memcpy(void *_dst, const void *_src, size_t len)
{
	uint8_t ch = 0;
	uint32_t dst, src;

	/* Cache cleaning, XDMAC is *not* cache aware */
	flush_dcache_range((uintptr_t) _src, len);
	inv_dcache_range((uintptr_t) _dst, len);

	/* Convert args from 64 bit */
	src = (uint64_t) _src;
	dst = (uint64_t) _dst;

	/* Start the operation */
	return xdmac_go(ch,
			dst, src,
			cc_memcpy, 0,
			(src | dst | len), len) ? NULL : _dst;
}

void xdmac_init(void)
{
	uint32_t w = mmio_read_32(XDMAC_XDMAC_VERSION(base));
	INFO("XDMAC: version 0x%lx, mfn %ld\n",
	     XDMAC_XDMAC_VERSION_VERSION_X(w),
	     XDMAC_XDMAC_VERSION_MFN_X(w));
}
