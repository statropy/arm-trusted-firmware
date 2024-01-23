/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _XDMAC_PRIV_H_
#define _XDMAC_PRIV_H_

#define AT_XDMAC_CC_TYPE_MEM_TRAN	0x0	/* Memory to Memory Transfer */
#define AT_XDMAC_CC_TYPE_PER_TRAN	0x1	/* Peripheral to Memory or Memory to Peripheral Transfer */
#define AT_XDMAC_CC_MBSIZE_SINGLE	0x0
#define AT_XDMAC_CC_MBSIZE_FOUR		0x1
#define AT_XDMAC_CC_MBSIZE_EIGHT	0x2
#define AT_XDMAC_CC_MBSIZE_SIXTEEN	0x3
#define AT_XDMAC_CC_DSYNC_PER2MEM	0x0
#define AT_XDMAC_CC_DSYNC_MEM2PER	0x1
#define AT_XDMAC_CC_PROT_SEC		0x0
#define AT_XDMAC_CC_PROT_UNSEC		0x1
#define AT_XDMAC_CC_SWREQ_HWR_CONNECTED	0x0
#define AT_XDMAC_CC_SWREQ_SWR_CONNECTED	0x1
#define AT_XDMAC_CC_MEMSET_NORMAL_MODE	0x0
#define AT_XDMAC_CC_MEMSET_HW_MODE	0x1
#define AT_XDMAC_CC_DWIDTH_BYTE		0x0
#define AT_XDMAC_CC_DWIDTH_HALFWORD	0x1
#define AT_XDMAC_CC_DWIDTH_WORD		0x2
#define AT_XDMAC_CC_DWIDTH_DWORD	0x3
#define AT_XDMAC_CC_SAM_FIXED_AM	0x0
#define AT_XDMAC_CC_SAM_INCREMENTED_AM	0x1
#define AT_XDMAC_CC_SAM_UBS_AM		0x2
#define AT_XDMAC_CC_SAM_UBS_DS_AM	0x3
#define AT_XDMAC_CC_DAM_FIXED_AM	0x0
#define AT_XDMAC_CC_DAM_INCREMENTED_AM	0x1
#define AT_XDMAC_CC_DAM_UBS_AM		0x2
#define AT_XDMAC_CC_DAM_UBS_DS_AM	0x3
#define AT_XDMAC_CC_INITD_TERMINATED	0x0
#define AT_XDMAC_CC_INITD_IN_PROGRESS	0x1
#define AT_XDMAC_CC_RDIP_DONE		0x0
#define AT_XDMAC_CC_RDIP_IN_PROGRESS	0x1
#define AT_XDMAC_CC_WRIP_DONE		0x0
#define AT_XDMAC_CC_WRIP_IN_PROGRESS	0x1

#define AT_XDMAC_CIE_BIE	BIT(0)	/* End of Block Interrupt Enable Bit */
#define AT_XDMAC_CIE_LIE	BIT(1)	/* End of Linked List Interrupt Enable Bit */
#define AT_XDMAC_CIE_DIE	BIT(2)	/* End of Disable Interrupt Enable Bit */
#define AT_XDMAC_CIE_FIE	BIT(3)	/* End of Flush Interrupt Enable Bit */
#define AT_XDMAC_CIE_RBEIE	BIT(4)	/* Read Bus Error Interrupt Enable Bit */
#define AT_XDMAC_CIE_WBEIE	BIT(5)	/* Write Bus Error Interrupt Enable Bit */
#define AT_XDMAC_CIE_ROIE	BIT(6)	/* Request Overflow Interrupt Enable Bit */

#define AT_XDMAC_CIS_BIS	BIT(0)	/* End of Block Interrupt Status Bit */
#define AT_XDMAC_CIS_LIS	BIT(1)	/* End of Linked List Interrupt Status Bit */
#define AT_XDMAC_CIS_DIS	BIT(2)	/* End of Disable Interrupt Status Bit */
#define AT_XDMAC_CIS_FIS	BIT(3)	/* End of Flush Interrupt Status Bit */
#define AT_XDMAC_CIS_RBEIS	BIT(4)	/* Read Bus Error Interrupt Status Bit */
#define AT_XDMAC_CIS_WBEIS	BIT(5)	/* Write Bus Error Interrupt Status Bit */
#define AT_XDMAC_CIS_ROIS	BIT(6)	/* Request Overflow Interrupt Status Bit */
#define AT_XDMAC_CIS_TCIS	BIT(7)	/* Transfer Count Overflow Interrupt Status Bit */
#define AT_XDMAC_CIS_ERROR	GENMASK(7, 4) /* Error conditions 7-4 */

#define AT_XDMAC_MBR_UBC_UBLEN_MAX      0xFFFFFFUL      /* Maximum Microblock Length */

#define AT_XDMAC_MAX_CHAN       16
#define AT_XDMAC_MAX_CSIZE      16      /* 16 data */
#define AT_XDMAC_MAX_DWIDTH     8       /* 64 bits */

#define AT_XDMAC_CSIZE_1	0
#define AT_XDMAC_CSIZE_2	1
#define AT_XDMAC_CSIZE_4	2
#define AT_XDMAC_CSIZE_8	3
#define AT_XDMAC_CSIZE_16	4

static inline uint32_t xdmac_align_width(uintptr_t addr)
{
	uint32_t width;

	/*
	 * Check address alignment to select the greater data width we
	 * can use.
	 *
	 * Some XDMAC implementations don't provide dword transfer, in
	 * this case selecting dword has the same behavior as
	 * selecting word transfers.
	 */
	if (!(addr & 7)) {
		width = AT_XDMAC_CC_DWIDTH_DWORD;
	} else if (!(addr & 3)) {
		width = AT_XDMAC_CC_DWIDTH_WORD;
	} else if (!(addr & 1)) {
		width = AT_XDMAC_CC_DWIDTH_HALFWORD;
	} else {
		width = AT_XDMAC_CC_DWIDTH_BYTE;
	}

	return width;
}

#endif /* _XDMAC_PRIV_H_ */
