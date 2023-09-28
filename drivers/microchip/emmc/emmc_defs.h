/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef EMMC_DEFS_H
#define EMMC_DEFS_H

#include <drivers/mmc.h>
#include <lib/mmio.h>

#define SDMMC_CLK_CTRL_DIV_MODE		0
#define SDMMC_CLK_CTRL_PROG_MODE	1

#define EMMC_POLLING_TIMEOUT	2000000u	/* 2sec */
#define EMMC_POLLING_VALUE	10000u

#define ALL_FLAGS	(0xFFFFu) /* All Irq's */

#define TIME_MSEC(x)	(x * 1000)	/* Converts arg from ms to microsec */

#define MMC_HIGH_DENSITY	(3)
#define MMC_NORM_DENSITY	(2)
#define SD_CARD_SDHC_SDXC	(1)
#define SD_CARD_SDSC		(0)

#define MAX_BLOCK_SIZE		512
#define SD_DATA_WRITE		1
#define SD_DATA_READ		0

#define SD_CARD			(0)
#define MMC_CARD		(1)
#define SD_CARD_PHY_SPEC_1_0X	(0)
#define SD_CARD_PHY_SPEC_1_10	(1)
#define SD_CARD_PHY_SPEC_2_00	(2)
#define SD_CARD_PHY_SPEC_3_0X	(3)
#define MMC_CARD_PHY_SPEC_4_X	(4)
#define MMC_CARD_PHY_SPEC_OLD	(5)

#define MMC_CARD_POWER_UP_STATUS	(0x1u << 31)
#define MMC_CARD_ACCESS_MODE_pos	29
#define MMC_CARD_ACCESS_MODE_msk	(0x3u << MMC_CARD_ACCESS_MODE_pos)
#define MMC_CARD_ACCESS_MODE_BYTE	(0x0u << MMC_CARD_ACCESS_MODE_pos)
#define MMC_CARD_ACCESS_MODE_SECTOR	(0x2u << MMC_CARD_ACCESS_MODE_pos)

#define SD_STATUS_ERROR_MASK		0xFFF90008
#define SD_CARD_STATUS_SUCCESS(s)	(0 == ((s) & SD_STATUS_ERROR_MASK))
#define SD_STATUS_CURRENT_STATE		(0xFu << 9)
#define SD_CARD_CCS_STATUS		(0x1u << 30)	// HCS (Host Capacity Support)
#define SD_CARD_HCS_HIGH		(0x1u << 30)	// High capacity support by host
#define SD_CARD_HCS_STANDARD		(0x0u << 30)	// Standard capacity support by host

/****************************************************************************************/
/*      MMC registers                                                                   */
/****************************************************************************************/

/* -------- SDMMC_SSAR: (SDMMC Offset: 0x00) SDAM System Address Register ---------- */
#define SDMMC_SSAR	0x00	/* uint16_t */

/* -------- SDMMC_BSR : (SDMMC Offset: 0x04) Block Size Register ---------- */
#define SDMMC_BSR	0x04	/* uint16_t */
#define   SDMMC_BSR_BLKSIZE_Pos 0
#define   SDMMC_BSR_BLKSIZE_Msk (0x3ffu << SDMMC_BSR_BLKSIZE_Pos)	/* Transfer Block Size */
#define   SDMMC_BSR_BLKSIZE(value) ((SDMMC_BSR_BLKSIZE_Msk & ((value) << SDMMC_BSR_BLKSIZE_Pos)))
#define   SDMMC_BSR_BOUNDARY(value) ((value) << 12)
#define   SDMMC_BSR_BOUNDARY_4K   0
#define   SDMMC_BSR_BOUNDARY_8K   1
#define   SDMMC_BSR_BOUNDARY_16K  2
#define   SDMMC_BSR_BOUNDARY_32K  3
#define   SDMMC_BSR_BOUNDARY_64K  4
#define   SDMMC_BSR_BOUNDARY_128K 5
#define   SDMMC_BSR_BOUNDARY_256K 6
#define   SDMMC_BSR_BOUNDARY_512K 7
/* -------- SDMMC_BCR : (SDMMC Offset: 0x06) Block Count Register --------- */
#define  SDMMC_BCR	0x06	/* uint16_t */
/* -------- SDMMC_ARG1R : (SDMMC Offset: 0x08) Argument 1 Register -------- */
#define  SDMMC_ARG1R	0x08	/* uint32_t */
/* -------- SDMMC_TMR : (SDMMC Offset: 0x0C) Transfer Mode Register ------- */
#define SDMMC_TMR	0x0C	/* uint16_t */
#define   SDMMC_TMR_DMAEN (1) /* DMA Enable */
#define   SDMMC_TMR_BCEN BIT(1) /* Block Count Enable */
#define   SDMMC_TMR_DTDSEL (0x1u << 4)	/* Data Transfer Direction Selection */
#define   SDMMC_TMR_DTDSEL_WRITE (0x0u << 4)	/* Writes data from the SDMMC to the device. */
#define   SDMMC_TMR_DTDSEL_READ (0x1u << 4)	/* Reads data from the device to the SDMMC. */
#define   SDMMC_TMR_MSBSEL (0x1u << 5)	/* Multi/Single Block Selection */
/* -------- SDMMC_CR : (SDMMC Offset: 0x0E) Command Register -------- */
#define SDMMC_CR	0x0E	/* uint16_t */
#define   SDMMC_CR_RESPTYP_Pos 0
#define   SDMMC_CR_RESPTYP_Msk (0x3u << SDMMC_CR_RESPTYP_Pos)	/* Response Type */
#define   SDMMC_CR_RESPTYP_NORESP (0x0u << 0)	/* No Response */
#define   SDMMC_CR_RESPTYP_RL136 (0x1u << 0)	/* Response Length 136 */
#define   SDMMC_CR_RESPTYP_RL48 (0x2u << 0)	/* Response Length 48 */
#define   SDMMC_CR_RESPTYP_RL48BUSY (0x3u << 0)	/* Response Length 48 with Busy */
#define   SDMMC_CR_CMDCCEN (0x1u << 3)	/* Command CRC Check Enable */
#define   SDMMC_CR_CMDICEN (0x1u << 4)	/* Command Index Check Enable */
#define   SDMMC_CR_DPSEL (0x1u << 5)	/* Data Present Select */
#define   SDMMC_CR_CMDTYP_NORMAL (0x0u << 6)	/* Other commands */
#define   SDMMC_CR_CMDTYP_ABORT (0x3u << 6)	/* CMD12, CMD52 to write "I/O Abort" in the Card Common Control Registers (CCCR) (for SDIO only) */
#define   SDMMC_CR_CMDIDX_Pos 8
#define   SDMMC_CR_CMDIDX_Msk (0x3fu << SDMMC_CR_CMDIDX_Pos)	/* Command Index */
#define   SDMMC_CR_CMDIDX(value) ((SDMMC_CR_CMDIDX_Msk & ((value) << SDMMC_CR_CMDIDX_Pos)))
/* -------- SDMMC_RR[4] : (SDMMC Offset: 0x10) Response Register ----------- */
#define SDMMC_RR0	0x10	/* uint32_t / Response Reg[0] */
#define SDMMC_RR1	0x14	/* uint32_t / Response Reg[1] */
#define SDMMC_RR2	0x18	/* uint32_t / Response Reg[2] */
#define SDMMC_RR3	0x1C	/* uint32_t / Response Reg[3] */
/* -------- SDMMC_BDPR : (SDMMC Offset: 0x20) Buffer Data Port Register ---- */
#define SDMMC_BDPR	0x20	/* uint32_t */
/* -------- SDMMC_PSR : (SDMMC Offset: 0x24) Present State Register -------- */
#define SDMMC_PSR	0x24	/* uint32_t */
#define   SDMMC_PSR_CMDINHC (0x1u << 0)	/* Command Inhibit (CMD) */
#define   SDMMC_PSR_CMDINHD (0x1u << 1)	/* Command Inhibit (DAT) */
#define   SDMMC_PSR_WTACT (0x1u << 8)	/* Write Transfer Active */
#define   SDMMC_PSR_BUFWREN (0x1u << 10)	/* Buffer Write Enable */
#define   SDMMC_PSR_BUFRDEN (0x1u << 11)	/* Buffer Read Enable */
#define   SDMMC_PSR_CARDINS (0x1u << 16)	/* Card Inserted */
#define   SDMMC_PSR_CARDSS (0x1u << 17)	/* Card State Stable */
#define   SDMMC_PSR_DATLL_Pos 20
#define   SDMMC_PSR_DATLL_Msk (0xfu << SDMMC_PSR_DATLL_Pos)	/* DAT[3:0] Line Level */
/* -------- SDMMC_HC1R : (SDMMC Offset: 0x28) Host Control 1 Register ------ */
#define SDMMC_HC1R	0x28	/* uint8_t */
#define   SDMMC_HC1R_DW (0x1u << 1)	/* Data Width */
#define   SDMMC_HC1R_DW_1_BIT (0x0u << 1)	/* 1-bit mode. */
#define   SDMMC_HC1R_DW_4_BIT (0x1u << 1)	/* 4-bit mode. */
#define   SDMMC_HC1R_EXTDW (0x1u << 5)	/* Extended Data Width */
/* -------- SDMMC_PCR : (SDMMC Offset: 0x29) Power Control Register -------- */
#define SDMMC_PCR	0x29	/* uint8_t */
#define   SDMMC_PCR_SDBPWR (0x1u << 0)	/* SD Bus Power */
/* -------- SDMMC_CCR : (SDMMC Offset: 0x2C) Clock Control Register -------- */
#define SDMMC_CCR	0x2C	/* uint16_t */
#define   SDMMC_CCR_INTCLKEN (0x1u << 0)	/* (SDMMC_CCR) Internal Clock Enable */
#define   SDMMC_CCR_INTCLKS (0x1u << 1)	/* Internal Clock Stable */
#define   SDMMC_CCR_SDCLKEN (0x1u << 2)	/* SD Clock Enable */
#define   SDMMC_CCR_CLKGSEL (0x1u << 5)	/* Clock Generator Select */
#define   SDMMC_CCR_USDCLKFSEL_Pos 6
#define   SDMMC_CCR_USDCLKFSEL_Msk (0x3u << SDMMC_CCR_USDCLKFSEL_Pos)	/* Upper Bits of SDCLK Frequency Select */
#define   SDMMC_CCR_USDCLKFSEL(value) ((SDMMC_CCR_USDCLKFSEL_Msk & ((value) << SDMMC_CCR_USDCLKFSEL_Pos)))
#define   SDMMC_CCR_SDCLKFSEL_Pos 8
#define   SDMMC_CCR_SDCLKFSEL_Msk (0xffu << SDMMC_CCR_SDCLKFSEL_Pos)	/* SDCLK Frequency Select */
#define   SDMMC_CCR_SDCLKFSEL(value) ((SDMMC_CCR_SDCLKFSEL_Msk & ((value) << SDMMC_CCR_SDCLKFSEL_Pos)))
/* -------- SDMMC_TCR : (SDMMC Offset: 0x2E) Timeout Control Register -------- */
#define SDMMC_TCR	0x2E	/* uint8_t */
#define   SDMMC_TCR_DTCVAL_Pos 0
#define   SDMMC_TCR_DTCVAL_Msk (0xfu << SDMMC_TCR_DTCVAL_Pos)	/* (SDMMC_TCR) Data Timeout Counter Value */
#define   SDMMC_TCR_DTCVAL(value) ((SDMMC_TCR_DTCVAL_Msk & ((value) << SDMMC_TCR_DTCVAL_Pos)))
/* -------- SDMMC_SRR : (SDMMC Offset: 0x2F) Software Reset Register ------- */
#define SDMMC_SRR	0x2F	/* uint8_t */
#define   SDMMC_SRR_SWRSTALL (0x1u << 0)	/* Software reset for All */
#define   SDMMC_SRR_SWRSTCMD (0x1u << 1)	/* Software reset for CMD line */
#define   SDMMC_SRR_SWRSTDAT (0x1u << 2)	/* Software reset for DAT line */
/* -------- SDMMC_NISTR : (SDMMC Offset: 0x30) Normal Interrupt Status Register - */
#define SDMMC_NISTR	0x30	/* uint16_t  */
#define   SDMMC_NISTR_CMDC (0x1u << 0)	/* Command Complete */
#define   SDMMC_NISTR_TRFC (0x1u << 1)	/* Transfer Complete */
#define   SDMMC_NISTR_DMAINT BIT(3)	/* DMA end/boundary */
#define   SDMMC_NISTR_BWRRDY (0x1u << 4)	/* Buffer Write Ready */
#define   SDMMC_NISTR_BRDRDY (0x1u << 5)	/* Buffer Read Ready */
#define   SDMMC_NISTR_ERRINT (0x1u << 15)	/* Error Interrupt */
/* -------- SDMMC_EISTR : (SDMMC Offset: 0x32) Error Interrupt Status Register -------- */
#define SDMMC_EISTR	0x32	/* uint16_t */
#define   SDMMC_EISTR_CMDTEO (0x1u << 0)	/* Command Timeout Error */
#define   SDMMC_EISTR_DATTEO (0x1u << 4)	/* Data Timeout Error */
/* -------- SDMMC_NISTER : (SDMMC Offset: 0x34) Normal Interrupt Status Enable Register -------- */
#define SDMMC_NISTER	0x34	/* uint16_t */
#define   SDMMC_NISTER_CMDC (0x1u << 0)	/* Command Complete Status Enable */
#define   SDMMC_NISTER_TRFC (0x1u << 1)	/* Transfer Complete Status Enable */
#define   SDMMC_NISTER_BWRRDY (0x1u << 4)	/* Buffer Write Ready Status Enable */
#define   SDMMC_NISTER_BRDRDY (0x1u << 5)	/* Buffer Read Ready Status Enable */
/* -------- SDMMC_EISTER : (SDMMC Offset: 0x36) Error Interrupt Status Enable Register -------- */
#define SDMMC_EISTER	0x36	/* uint16_t */
#define   SDMMC_EISTER_DATTEO (0x1u << 4)	/* Data Timeout Error Status Enable */
#define   SDMMC_EISTER_DATCRC (0x1u << 5)	/* Data CRC Error Status Enable */
#define   SDMMC_EISTER_DATEND (0x1u << 6)	/* Data End Bit Error Status Enable */
/* -------- SDMMC_NISIER : (SDMMC Offset: 0x38) Normal Interrupt Signal Enable Register -------- */
#define SDMMC_NISIER	0x38	/* uint16_t */
#define   SDMMC_NISIER_BRDRDY (0x1u << 5)	/* Buffer Read Ready Signal Enable */
/* -------- SDMMC_EISIER : (SDMMC Offset: 0x3A) Error Interrupt Signal Enable Register -------- */
#define SDMMC_EISIER	0x3A	/* uint16_t */
#define   SDMMC_EISIER_DATTEO (0x1u << 4)	/* Data Timeout Error Signal Enable */
#define   SDMMC_EISIER_DATCRC (0x1u << 5)	/* Data CRC Error Signal Enable */
#define   SDMMC_EISIER_DATEND (0x1u << 6)	/* Data End Bit Error Signal Enable */
/* -------- SDMMC_CA0R : (SDMMC Offset: 0x40) Capabilities 0 Register ------ */
#define SDMMC_CA0R	0x40	/* uint32_t */
#define   SDMMC_CA0R_TEOCLKF_Pos 0
#define   SDMMC_CA0R_TEOCLKF_Msk (0x3fu << SDMMC_CA0R_TEOCLKF_Pos)	/* Timeout Clock Frequency */
#define   SDMMC_CA0R_TEOCLKU (0x1u << 7)	/* (SDMMC_CA0R) Timeout Clock Unit */
#define   SDMMC_CA0R_BASECLKF_Pos 8
#define   SDMMC_CA0R_BASECLKF_Msk (0xffu << SDMMC_CA0R_BASECLKF_Pos)	/* Base Clock Frequency */
#define   SDMMC_CA0R_ED8SUP (0x1u << 18)	/* 8-Bit Support for Embedded Device */
#define   SDMMC_CA0R_HSSUP (0x1u << 21)	/* High Speed Support */
/* -------- SDMMC_MC1R : (SDMMC Offset: 0x204) e.MMC Control 1 Register ---- */
#define SDMMC_MC1R	0x204	/* uint8_t */
#define   SDMMC_MC1R_CMDTYP_Pos 0
#define   SDMMC_MC1R_CMDTYP_Msk (0x3u << SDMMC_MC1R_CMDTYP_Pos)	/* e.MMC Command Type */
#define   SDMMC_MC1R_CMDTYP_NORMAL (0x0u << 0)	/* The command is not an e.MMC specific command. */
#define   SDMMC_MC1R_OPD (0x1u << 4)	/* e.MMC Open Drain Mode */
#define   SDMMC_MC1R_FCD (0x1u << 7)	/* e.MMC Force Card Detect */
/* -------- SDMMC_DEBR : (SDMMC Offset: 0x207) Debounce Register -------- */
#define SDMMC_DEBR	0x207	/* uint8_t */
#define SDMMC_DEBR_CDDVAL_Pos 0
#define SDMMC_DEBR_CDDVAL_Msk (0x3u << SDMMC_DEBR_CDDVAL_Pos)	/*(SDMMC_DEBR) Card Detect Debounce Value */
#define SDMMC_DEBR_CDDVAL(value) ((SDMMC_DEBR_CDDVAL_Msk & ((value) << SDMMC_DEBR_CDDVAL_Pos)))

/****************************************************************************************/
/*      SD Card Commands Definitions                                                    */
/****************************************************************************************/
#define SDMMC_CMD0		(SDMMC_CR_CMDIDX(0) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_RESPTYP_NORESP | ((SDMMC_MC1R_CMDTYP_NORMAL | SDMMC_MC1R_OPD) << 16))
#define SDMMC_SD_CMD0		(SDMMC_CR_CMDIDX(0) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_RESPTYP_NORESP)
#define SDMMC_MMC_CMD0		(SDMMC_CR_CMDIDX(0) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_RESPTYP_NORESP | ((SDMMC_MC1R_CMDTYP_NORMAL | SDMMC_MC1R_OPD) << 16))

#define SDMMC_CMD1		(SDMMC_CR_CMDIDX(1) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_RESPTYP_RL48 | ((SDMMC_MC1R_CMDTYP_NORMAL | SDMMC_MC1R_OPD) << 16))
#define SDMMC_SD_CMD1		(SDMMC_CR_CMDIDX(1) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_RESPTYP_RL48)
#define SDMMC_MMC_CMD1		(SDMMC_CR_CMDIDX(1) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_RESPTYP_RL48 | ((SDMMC_MC1R_CMDTYP_NORMAL | SDMMC_MC1R_OPD) << 16))

#define SDMMC_SD_CMD2		(SDMMC_CR_CMDIDX(2) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL136)
#define SDMMC_MMC_CMD2		(SDMMC_CR_CMDIDX(2) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL136 | ((SDMMC_MC1R_CMDTYP_NORMAL | SDMMC_MC1R_OPD) << 16))

#define SDMMC_SD_CMD3		(SDMMC_CR_CMDIDX(3) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48)
#define SDMMC_MMC_CMD3		(SDMMC_CR_CMDIDX(3) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | ((SDMMC_MC1R_CMDTYP_NORMAL | SDMMC_MC1R_OPD) << 16))

#define SDMMC_SD_CMD4		(SDMMC_CR_CMDIDX(4) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_RESPTYP_NORESP)

#define SDMMC_SD_CMD6		(SDMMC_CR_CMDIDX(6) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL)
#define SDMMC_MMC_CMD6		(SDMMC_CR_CMDIDX(6) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48BUSY | (SDMMC_MC1R_CMDTYP_NORMAL << 16))

#define SDMMC_SD_CMD7		(SDMMC_CR_CMDIDX(7) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48BUSY)
#define SDMMC_MMC_CMD7		(SDMMC_CR_CMDIDX(7) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48BUSY | (SDMMC_MC1R_CMDTYP_NORMAL << 16))

#define SDMMC_SD_CMD8		(SDMMC_CR_CMDIDX(8) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48)
#define SDMMC_MMC_CMD8		(SDMMC_CR_CMDIDX(8) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL | (SDMMC_MC1R_CMDTYP_NORMAL << 16))

#define SDMMC_SD_CMD9		(SDMMC_CR_CMDIDX(9) | SDMMC_CR_CMDTYP_NORMAL  | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL136)
#define SDMMC_MMC_CMD9		(SDMMC_CR_CMDIDX(9) | SDMMC_CR_CMDTYP_NORMAL  | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL136 | (SDMMC_MC1R_CMDTYP_NORMAL << 16))

#define SDMMC_SD_CMD10		(SDMMC_CR_CMDIDX(10) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL136)

#define SDMMC_SD_CMD11		(SDMMC_CR_CMDIDX(11) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48)
#define SDMMC_MMC_CMD11		(SDMMC_CR_CMDIDX(11) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL | (SDMMC_MC1R_CMDTYP_STREAM << 16))

#define SDMMC_SD_CMD12		(SDMMC_CR_CMDIDX(12) | SDMMC_CR_CMDTYP_ABORT | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48BUSY)
#define SDMMC_MMC_CMD12		(SDMMC_CR_CMDIDX(12) | SDMMC_CR_CMDTYP_ABORT | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48BUSY | (SDMMC_MC1R_CMDTYP_NORMAL << 16))

#define SDMMC_SD_CMD13		(SDMMC_CR_CMDIDX(13) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48)
#define SDMMC_MMC_CMD13		(SDMMC_CR_CMDIDX(13) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | (SDMMC_MC1R_CMDTYP_NORMAL << 16))

#define SDMMC_SD_CMD16		(SDMMC_CR_CMDIDX(16) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48)
#define SDMMC_MMC_CMD16		(SDMMC_CR_CMDIDX(16) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | (SDMMC_MC1R_CMDTYP_NORMAL << 16))

#define SDMMC_SD_CMD17		(SDMMC_CR_CMDIDX(17) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL)
#define SDMMC_MMC_CMD17		(SDMMC_CR_CMDIDX(17) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL | (SDMMC_MC1R_CMDTYP_NORMAL << 16))

#define SDMMC_SD_CMD18		(SDMMC_CR_CMDIDX(18) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL)
#define SDMMC_MMC_CMD18		(SDMMC_CR_CMDIDX(18) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL | (SDMMC_MC1R_CMDTYP_NORMAL << 16))

#define SDMMC_MMC_CMD20		(SDMMC_CR_CMDIDX(20) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL | (SDMMC_MC1R_CMDTYP_STREAM << 16))

#define SDMMC_MMC_CMD23		(SDMMC_CR_CMDIDX(23) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | (SDMMC_MC1R_CMDTYP_NORMAL << 16))

#define SDMMC_SD_CMD24		(SDMMC_CR_CMDIDX(24) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL)
#define SDMMC_MMC_CMD24		(SDMMC_CR_CMDIDX(24) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL | (SDMMC_MC1R_CMDTYP_NORMAL << 16))

#define SDMMC_SD_CMD25		(SDMMC_CR_CMDIDX(25) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL)
#define SDMMC_MMC_CMD25		(SDMMC_CR_CMDIDX(25) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL | (SDMMC_MC1R_CMDTYP_NORMAL << 16))

#define SDMMC_SD_CMD32		(SDMMC_CR_CMDIDX(32) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48)
#define SDMMC_SD_CMD33		(SDMMC_CR_CMDIDX(33) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48)

#define SDMMC_MMC_CMD35		(SDMMC_CR_CMDIDX(35) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | (SDMMC_MC1R_CMDTYP_NORMAL << 16))
#define SDMMC_MMC_CMD36		(SDMMC_CR_CMDIDX(36) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | (SDMMC_MC1R_CMDTYP_NORMAL << 16))

#define SDMMC_SD_CMD38		(SDMMC_CR_CMDIDX(38) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48BUSY)
#define SDMMC_MMC_CMD38		(SDMMC_CR_CMDIDX(38) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48BUSY | (SDMMC_MC1R_CMDTYP_NORMAL << 16))

#define SDMMC_SD_CMD55		(SDMMC_CR_CMDIDX(55) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48)

#define SDMMC_SD_ACMD6		(SDMMC_CR_CMDIDX(6) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48)
#define SDMMC_SD_ACMD13		(SDMMC_CR_CMDIDX(13) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL)
#define SDMMC_SD_CMD19		(SDMMC_CR_CMDIDX(19) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL)
#define SDMMC_SD_ACMD23		(SDMMC_CR_CMDIDX(23) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_CMDICEN | SDMMC_CR_CMDCCEN | SDMMC_CR_RESPTYP_RL48)
#define SDMMC_SD_ACMD41		(SDMMC_CR_CMDIDX(41) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_RESPTYP_RL48)
#define SDMMC_SD_ACMD51		(SDMMC_CR_CMDIDX(51) | SDMMC_CR_CMDTYP_NORMAL | SDMMC_CR_RESPTYP_RL48 | SDMMC_CR_DPSEL)

typedef struct _card {
	uint8_t card_type;
	uint8_t card_capacity;
	uint32_t card_taac_ns;
	uint32_t card_nsac;
	uint8_t card_r2w_factor;
	uint8_t card_max_rd_blk_len;
	uint8_t card_phy_spec_rev;
} card;

typedef enum {
	SDMMC_SW_RST_ALL = 0,
	SDMMC_SW_RST_CMD_LINE,
	SDMMC_SW_RST_DATA_CMD_LINES
} lan966x_reset_type;

static inline uint32_t get_CSD_field(const uint32_t *p_resp,
				     uint32_t index, uint32_t size)
{
	uint32_t shift;
	uint32_t res;
	uint32_t off;

	index -= 8;		//Remove CRC section
	shift = index % 32;
	off = index / 32;

	res = p_resp[off] >> shift;

	if ((size + shift) > 32) {
		res |= (p_resp[off + 1] << (32 - shift));
	}

	res &= ((1 << size) - 1);

	return res;
}

static const char *mmc_reg(int reg)
{
	switch (reg) {
	case SDMMC_SSAR:
		return "SSAR";
	case SDMMC_BSR:
		return "BSR";
	case SDMMC_BCR:
		return "BCR";
	case SDMMC_ARG1R:
		return "ARG1R";
	case SDMMC_TMR:
		return "TMR";
	case SDMMC_CR:
		return "CR";
	case SDMMC_RR0:
		return "RR0";
	case SDMMC_RR1:
		return "RR1";
	case SDMMC_RR2:
		return "RR2";
	case SDMMC_RR3:
		return "RR4";
	case SDMMC_BDPR:
		return "BDPR";
	case SDMMC_PSR:
		return "PSR";
	case SDMMC_HC1R:
		return "HC1R";
	case SDMMC_PCR:
		return "PCR";
	case SDMMC_CCR:
		return "CCR";
	case SDMMC_TCR:
		return "TCR";
	case SDMMC_SRR:
		return "SRR";
	case SDMMC_NISTR:
		return "NISTR";
	case SDMMC_EISTR:
		return "EISTR";
	case SDMMC_NISIER:
		return "NISIER";
	case SDMMC_EISIER:
		return "EISIER";
	case SDMMC_NISTER:
		return "NISTER";
	case SDMMC_EISTER:
		return "EISTER";
	case SDMMC_CA0R:
		return "CA0R";
	case SDMMC_MC1R:
		return "MC1R";
	case SDMMC_DEBR:
		return "DEBR";
	default:
		return "???";
	}
}

static inline void sdhci_write_8(uintptr_t addr, int reg, uint8_t val)
{
	VERBOSE("WR(%04x, %s) = %02x\n", reg, mmc_reg(reg), val);
	mmio_write_8(addr + reg, val);
}

static inline uint8_t sdhci_read_8(uintptr_t addr, int reg)
{
	uint8_t val = mmio_read_8(addr + reg);
	VERBOSE("RD(%04x, %s) = %02x\n", reg, mmc_reg(reg), val);
	return val;
}

static inline void sdhci_write_16(uintptr_t addr, int reg, uint16_t val)
{
	VERBOSE("WR(%04x, %s) = %04x\n", reg, mmc_reg(reg), val);
	mmio_write_16(addr + reg, val);
}

static inline uint16_t sdhci_read_16(uintptr_t addr, int reg)
{
	uint16_t val = mmio_read_16(addr + reg);
	VERBOSE("RD(%04x, %s) = %04x\n", reg, mmc_reg(reg), val);
	return val;
}

static inline void sdhci_write_32(uintptr_t addr, int reg, uint32_t val)
{
	VERBOSE("WR(%04x, %s) = %08x\n", reg, mmc_reg(reg), val);
	mmio_write_32(addr + reg, val);
}

static inline uint32_t sdhci_read_32(uintptr_t addr, int reg)
{
	uint32_t val = mmio_read_32(addr + reg);
	VERBOSE("RD(%04x, %s) = %08x\n", reg, mmc_reg(reg), val);
	return val;
}

static inline void mmc_setbits_16(uintptr_t addr, int reg, uint16_t set)
{
	sdhci_write_16(addr, reg, sdhci_read_16(addr, reg) | set);
}

static inline void mmc_clrbits_16(uintptr_t addr, int reg, uint16_t clear)
{
	sdhci_write_16(addr, reg, sdhci_read_16(addr, reg) & ~clear);
}

static inline void mmc_setbits_8(uintptr_t addr, int reg, uint8_t set)
{
	sdhci_write_8(addr, reg, sdhci_read_8(addr, reg) | set);
}

#endif	/* EMMC_DEFS_H */
