/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stddef.h>
#include <assert.h>

#include <drivers/delay_timer.h>
#include <drivers/microchip/emmc.h>
#include <drivers/microchip/lan966x_clock.h>
#include <drivers/mmc.h>
#include <lib/mmio.h>

#include "lan966x_private.h"

/* -------- SDMMC_BSR : (SDMMC Offset: 0x04) Block Size Register ---------- */
#define SDMMC_BSR	0x04	/* uint16_t */
#define   SDMMC_BSR_BLKSIZE_Pos 0
#define   SDMMC_BSR_BLKSIZE_Msk (0x3ffu << SDMMC_BSR_BLKSIZE_Pos)	/* Transfer Block Size */
#define   SDMMC_BSR_BLKSIZE(value) ((SDMMC_BSR_BLKSIZE_Msk & ((value) << SDMMC_BSR_BLKSIZE_Pos)))
/* -------- SDMMC_BCR : (SDMMC Offset: 0x06) Block Count Register --------- */
#define  SDMMC_BCR	0x06	/* uint16_t */
/* -------- SDMMC_ARG1R : (SDMMC Offset: 0x08) Argument 1 Register -------- */
#define  SDMMC_ARG1R	0x08	/* uint32_t */
/* -------- SDMMC_TMR : (SDMMC Offset: 0x0C) Transfer Mode Register ------- */
#define SDMMC_TMR	0x0C	/* uint16_t */
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

static card p_card;
static uintptr_t reg_base;
static uint16_t eistr = 0u;	/* Holds the error interrupt status */
static lan966x_mmc_params_t lan966x_params;

static const unsigned int TAAC_TimeExp[8] =
	{ 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000 };
static const unsigned int TAAC_TimeMant[16] =
	{ 0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80 };

#define EMMC_RESET_TIMEOUT_US		(1000 * 500) /* 500 ms */

static void lan966x_mmc_reset(lan966x_reset_type type)
{
	uint64_t timeout = timeout_init_us(EMMC_RESET_TIMEOUT_US);
	unsigned int state;

	VERBOSE("MMC: Software reset type: %d\n", type);

	switch (type) {
	case SDMMC_SW_RST_ALL:
		mmc_setbits_8(reg_base + SDMMC_SRR, SDMMC_SRR_SWRSTALL);
		break;
	case SDMMC_SW_RST_CMD_LINE:
		mmc_setbits_8(reg_base + SDMMC_SRR, SDMMC_SRR_SWRSTCMD);
		break;
	case SDMMC_SW_RST_DATA_CMD_LINES:
		mmio_write_8(reg_base + SDMMC_SRR, (SDMMC_SRR_SWRSTCMD | SDMMC_SRR_SWRSTDAT));
		break;
	default:
		ERROR("MMC: NOT supported reset type %d \n", type);
		return;
	}

	do {
		if (timeout_elapsed(timeout)) {
			ERROR("MMC: Reset timeout expired, type %d!\n", type);
			break;
		} else {
			state = mmio_read_8(reg_base + SDMMC_SRR);
		}
	} while (state);

	VERBOSE("MMC: Software reset done\n");
}

static void lan966x_clock_delay(unsigned int sd_clock_cycles)
{
	unsigned int usec;
	usec = DIV_ROUND_UP_2EVAL(sd_clock_cycles * 1000000u, lan966x_params.clk_rate);
	udelay(usec);
}

static unsigned char lan966x_set_clk_freq(unsigned int SD_clock_freq, unsigned int sd_src_clk)
{
	unsigned int timeout;
	unsigned short new_ccr, old_ccr;
	unsigned int clk_div, base_clock, mult_clock;
	unsigned int state;

	timeout = EMMC_POLLING_VALUE;
	do {
		if (timeout > 0) {
			state = mmio_read_32(reg_base + SDMMC_PSR);
			timeout--;
			udelay(10);
		} else {
			ERROR("Timeout, CMD and DATn lines not low \n");
			return 1;
		}
	} while (state & (SDMMC_PSR_CMDINHD | SDMMC_PSR_CMDINHC));

	/* Save current value of clock control register */
	old_ccr = mmio_read_16(reg_base + SDMMC_CCR);

	/* Clear CLK div, SD Clock Enable and Internal Clock Stable */
	new_ccr = old_ccr & ~(SDMMC_CCR_SDCLKFSEL_Msk |
			      SDMMC_CCR_USDCLKFSEL_Msk |
			      SDMMC_CCR_SDCLKEN | SDMMC_CCR_INTCLKS);

	switch (sd_src_clk) {
	case SDMMC_CLK_CTRL_DIV_MODE:
		/* Switch to divided clock mode, only for SR FPGA */
		base_clock = lan966x_clk_get_baseclk_freq();
		new_ccr &= ~SDMMC_CCR_CLKGSEL;

		if (SD_clock_freq == base_clock) {
			clk_div = 0;
		} else {
			clk_div = (((base_clock / SD_clock_freq) % 2) ?
				   (((base_clock / SD_clock_freq) + 1) / 2) :
				   ((base_clock / SD_clock_freq) / 2));
		}
		break;

	case SDMMC_CLK_CTRL_PROG_MODE:
		/* Switch to programmable clock mode, only for SR FPGA */
		mult_clock = lan966x_clk_get_multclk_freq();
		new_ccr |= SDMMC_CCR_CLKGSEL;

		if (SD_clock_freq == mult_clock) {
			clk_div = 0;
		} else {
			clk_div = DIV_ROUND_UP_2EVAL(mult_clock, SD_clock_freq) - 1;
		}
		break;

	default:
		return 1;
	}

	/* Set new clock divisor and enable only the internal clock */
	new_ccr |= (SDMMC_CCR_INTCLKEN |
		    SDMMC_CCR_SDCLKFSEL(clk_div) |
		    SDMMC_CCR_USDCLKFSEL(clk_div >> 8));

	/* New value of clock control reg has been calculated for update */
	/* Disable SD clock before setting new divisor (but not the internal clock) */
	mmio_write_16(reg_base + SDMMC_CCR, (old_ccr & ~(SDMMC_CCR_SDCLKEN)));

	/* Set new clock configuration */
	mmio_write_16(reg_base + SDMMC_CCR, new_ccr);

	/* Wait for internal clock to be stable */
	timeout = EMMC_POLLING_VALUE;
	do {
		if (timeout > 0) {
			state = (unsigned int)mmio_read_16(reg_base + SDMMC_CCR);
			timeout--;
			udelay(10);
		} else {
			return 1;
		}
	} while ((state & SDMMC_CCR_INTCLKS) == 0);

	/* Enable SD clock */
	mmc_setbits_16(reg_base + SDMMC_CCR, SDMMC_CCR_SDCLKEN);
	lan966x_params.clk_rate = SD_clock_freq;

	/* Wait for 74 SD clock cycles (according SD Card specification) */
	lan966x_clock_delay(74u);

	return 0;
}

static int lan966x_host_init(void)
{
	unsigned int timeout;
	unsigned int state;
	boot_source_type boot_source;
	lan966x_params.clk_rate = 0u;

	boot_source = lan966x_get_boot_source();

	/* Set default values for eMMC/SDMMC */
	if (boot_source == BOOT_SOURCE_EMMC) {
		p_card.card_type = MMC_CARD;
		p_card.card_capacity = MMC_NORM_DENSITY;
	} else if (boot_source == BOOT_SOURCE_SDMMC) {
		p_card.card_type = SD_CARD;
		p_card.card_capacity = SD_CARD_SDSC;
	}
#if defined(LAN966X_EMMC_TESTS)
	/* When the EVB board is used and LAN966X_EMMC_TESTS is enabled, the
	 * previously called lan966x_get_boot_source() function will return
	 * BOOT_SOURCE_QSPI. Since this is a special test mode, this boot_source
	 * is not considered and supported here. Therefore, the default eMMC
	 * values needs to be set here. */
	p_card.card_type = MMC_CARD;
	p_card.card_capacity = MMC_NORM_DENSITY;
#endif

	lan966x_clk_disable(LAN966X_CLK_ID_SDMMC0);
	lan966x_clk_set_rate(LAN966X_CLK_ID_SDMMC0, LAN966X_CLK_FREQ_SDMMC);
	lan966x_clk_enable(LAN966X_CLK_ID_SDMMC0);

	/* Reset entire SDMMC host hardware */
	lan966x_mmc_reset(SDMMC_SW_RST_ALL);

	/* "SD Bus Power" init. */
	mmc_setbits_8(reg_base + SDMMC_PCR, SDMMC_PCR_SDBPWR);

	/* Reset the SDMMC CMD and DATn lines */
	lan966x_mmc_reset(SDMMC_SW_RST_DATA_CMD_LINES);

	/* Set host controller data bus width to 1 bit */
	mmc_setbits_8(reg_base + SDMMC_HC1R, SDMMC_HC1R_DW_1_BIT);

	if (lan966x_set_clk_freq(MMC_INIT_SPEED, SDMMC_CLK_CTRL_PROG_MODE)) {
		return -1;
	}

	/* Check if sd-card is physically present */
	if (boot_source == BOOT_SOURCE_SDMMC) {
		/* Set debounce value register and check if sd-card is inserted */
		mmc_setbits_8(reg_base + SDMMC_DEBR, SDMMC_DEBR_CDDVAL(3));

		/* Wait for sd-card stable present bit */
		timeout = EMMC_POLLING_VALUE;
		do {
			if (timeout > 0) {
				state = mmio_read_32(reg_base + SDMMC_PSR);
				timeout--;
				udelay(10);
			} else {
				return -1;
			}
		} while (!((state & SDMMC_PSR_CARDSS) && (state & SDMMC_PSR_CARDINS)));

		/* Enable SD clock */
		mmc_setbits_16(reg_base + SDMMC_CCR, SDMMC_CCR_SDCLKEN);
	}

	return 0;
}

static void lan966x_mmc_initialize(void)
{
	int retVal;

	VERBOSE("MMC: ATF CB init() \n");

	retVal = lan966x_host_init();
	if (retVal != 0) {
		ERROR("MMC host initialization failed !\n");
		panic();
	}
}

static void lan966x_get_cid_register(void)
{
	p_card.card_type = (mmio_read_32(reg_base + SDMMC_RR3) >> 8) & 0x03;
}

static void lan966x_get_csd_register(void)
{
	unsigned int m, e;
	unsigned int csd_struct;
	volatile const unsigned int *p_resp;

	/* Initialize pointer to response register RR[0] */
	p_resp = (unsigned int *)(reg_base + SDMMC_RR0);

	/* Retrieve version of CSD structure information */
	csd_struct = get_CSD_field(p_resp, 126, 2);

	switch (csd_struct) {
	case 0:
	case 2:
	case 3:
		e = get_CSD_field(p_resp, 112, 3);
		m = get_CSD_field(p_resp, 115, 4);
		p_card.card_taac_ns = TAAC_TimeExp[e] * TAAC_TimeMant[m] / 10;

		p_card.card_nsac = get_CSD_field(p_resp, 104, 8);
		p_card.card_r2w_factor = get_CSD_field(p_resp, 26, 3);
		p_card.card_max_rd_blk_len = get_CSD_field(p_resp, 80, 4);

		if (p_card.card_type == MMC_CARD) {
			if (get_CSD_field(p_resp, 122, 4) == 4) {
				p_card.card_phy_spec_rev = MMC_CARD_PHY_SPEC_4_X;
			} else {
				p_card.card_phy_spec_rev = MMC_CARD_PHY_SPEC_OLD;
			}
		}

		break;
	case 1:
		/* Settings from Linux drivers */
		p_card.card_taac_ns = 0;
		p_card.card_nsac = 0;
		p_card.card_r2w_factor = 4;
		p_card.card_max_rd_blk_len = 9;

		break;
	default:
		break;
	}
}

static unsigned char lan966x_emmc_poll(unsigned int expected)
{
	unsigned int trials = DIV_ROUND_UP_2EVAL(EMMC_POLLING_TIMEOUT,
						 EMMC_POLL_LOOP_DELAY);
	uint16_t nistr = 0u;

	eistr = 0u;
	while (trials--) {
		nistr = mmio_read_16(reg_base + SDMMC_NISTR);

		/* Check errors */
		if (nistr & SDMMC_NISTR_ERRINT) {
			eistr = mmio_read_16(reg_base + SDMMC_EISTR);

			/* Dump interrupt status values and variables */
			ERROR(" NISTR: 0x%x \n", nistr);
			ERROR(" NISTR expected: 0x%x \n", expected);
			ERROR(" EISTR: 0x%x \n", eistr);

			/* Clear Normal/Error Interrupt Status Register flags */
			mmio_write_16(reg_base + SDMMC_EISTR, eistr);
			mmio_write_16(reg_base + SDMMC_NISTR, nistr);
			return 1;
		}

		/* Wait for any expected flags */
		if (nistr & expected) {
			/* Clear only expected flags */
			mmio_write_16(reg_base + SDMMC_NISTR, expected);
			return 0;
		}

		udelay(EMMC_POLL_LOOP_DELAY);
	}
	return 1;
}

static void lan966x_set_data_timeout(unsigned int trans_type)
{
	unsigned timeout_freq;
	unsigned timeout_freq_fact;
	unsigned int clock_frequency;
	unsigned int clock_div;
	unsigned int mult;
	unsigned int timeout_val;
	unsigned int limit_us;
	unsigned int timeout_cyc;

	timeout_val = 0;

	/* SD cards use a 100 multiplier rather than 10 */
	mult = (p_card.card_type == SD_CARD) ? 100 : 10;

	mmc_clrbits_16(reg_base + SDMMC_EISTER, SDMMC_EISTER_DATTEO);
	mmc_clrbits_16(reg_base + SDMMC_EISIER, SDMMC_EISIER_DATTEO);

	if (p_card.card_type == MMC_CARD
	    || (p_card.card_type == SD_CARD && p_card.card_capacity == SD_CARD_SDSC)) {
		/* Set base clock frequency in MHz */
		clock_frequency =
		    (mmio_read_32(reg_base + SDMMC_CA0R) &
		     SDMMC_CA0R_BASECLKF_Msk) >> SDMMC_CA0R_BASECLKF_Pos;

		clock_div =
		    ((mmio_read_16(reg_base + SDMMC_CCR) &
		      SDMMC_CCR_SDCLKFSEL_Msk) >> SDMMC_CCR_SDCLKFSEL_Pos) |
		    (((mmio_read_16(reg_base + SDMMC_CCR) &
		       SDMMC_CCR_USDCLKFSEL_Msk) >> SDMMC_CCR_USDCLKFSEL_Pos) << 8);

		/* Convert base clock frequency into sd clock frequency in KHz */
		clock_frequency = (clock_frequency * 1000) / (2 * clock_div);

		if (trans_type == SD_DATA_WRITE) {
			mult <<= p_card.card_r2w_factor;
		}

		/* Set timeout_val in micro seconds */
		timeout_val =
		    mult * ((p_card.card_taac_ns / 1000) +
			    (p_card.card_nsac * 100 * 1000 / clock_frequency));
	}

	if (p_card.card_type == SD_CARD && p_card.card_capacity == SD_CARD_SDSC) {
		if (trans_type == SD_DATA_WRITE) {
			/* The limit is 250 ms, but that is sometimes insufficient */
			limit_us = TIME_MSEC(300);

		} else {
			limit_us = TIME_MSEC(100);
		}

		if (timeout_val > limit_us) {
			/* Max read timeout = 100ms and max write timeout = 300ms */
			timeout_val = limit_us;
		}
	}

	if (p_card.card_type == SD_CARD && p_card.card_capacity == SD_CARD_SDHC_SDXC) {
		if (trans_type == SD_DATA_WRITE) {
			/* Write timeout is 500ms for SDHC and SDXC */
			timeout_val = TIME_MSEC(500);
		} else {
			/* Read timeout is 100ms for SDHC and SDXC */
			timeout_val = TIME_MSEC(100);
		}
	}

	/* Convert timeout_val from micro seconds into ms */
	timeout_val = timeout_val / 1000;

	if ((mmio_read_32(reg_base + SDMMC_CA0R) & SDMMC_CA0R_TEOCLKU) == SDMMC_CA0R_TEOCLKU) {
		/* Set value of 1000, because "timeout_val" is in ms */
		timeout_freq_fact = 1000u;
	} else {
		/* Set value of 1, because "timeout_val" is in ms */
		timeout_freq_fact = 1u;
	}

	timeout_freq = (mmio_read_32(reg_base + SDMMC_CA0R) &
			SDMMC_CA0R_TEOCLKF_Msk) * timeout_freq_fact;
	timeout_cyc = timeout_val * timeout_freq;

	/* timeout_val will be re-used for the data timeout counter */
	timeout_val = 0;
	while (timeout_cyc > 0) {
		timeout_val++;
		timeout_cyc >>= 1;
	}

	/* Clearing Data Time Out Error Status bit before. */
	mmc_setbits_16(reg_base + SDMMC_EISTR, SDMMC_EISTR_DATTEO);
	mmio_write_8(reg_base + SDMMC_TCR, SDMMC_TCR_DTCVAL(timeout_val - 13));
}

static int lan966x_mmc_send_cmd(struct mmc_cmd *cmd)
{
	unsigned short mc1r_reg_value, cr_reg_value;
	unsigned int timeout, not_ready;
	unsigned int op;
	unsigned int state;

	VERBOSE("MMC: ATF CB send_cmd() %d \n", cmd->cmd_idx);

	/* Parse CMD argument and set proper flags */
	switch (cmd->cmd_idx) {
	case 0:
		op = SDMMC_SD_CMD0;
		break;
	case 1:
		op = SDMMC_MMC_CMD1;
		break;
	case 2:
		op = SDMMC_MMC_CMD2;
		break;
	case 3:
		op = SDMMC_MMC_CMD3;
		break;
	case 6:
		op = SDMMC_MMC_CMD6;
		break;
	case 7:
		op = SDMMC_MMC_CMD7;
		break;
	case 8:
		op = SDMMC_MMC_CMD8;
		break;
	case 9:
		op = SDMMC_MMC_CMD9;
		break;
	case 12:
		op = SDMMC_SD_CMD12;
		break;
	case 13:
		op = SDMMC_MMC_CMD13;
		break;
	case 16:
		op = SDMMC_MMC_CMD16;
		break;
	case 17:
		op = SDMMC_MMC_CMD17;
		break;
	case 18:
		op = SDMMC_MMC_CMD18;
		break;
	case 24:
		op = SDMMC_MMC_CMD24;
		break;
	case 25:
		op = SDMMC_MMC_CMD25;
		break;
	case 41:
		op = SDMMC_SD_ACMD41;
		break;
	case 51:
		op = SDMMC_SD_ACMD51;
		break;
	case 55:
		op = SDMMC_SD_CMD55;
		break;
	default:
		op = 0u;
		ERROR("MMC: Unsupported Command ID : %d \n", cmd->cmd_idx);
		break;
	}

	cr_reg_value = op & 0xFFFF;

	/* Configure Normal and Error Interrupt Status Enable Registers */
	mmc_setbits_16(reg_base + SDMMC_NISTER, (SDMMC_NISTER_CMDC | SDMMC_NISTER_TRFC));
	mmio_write_16(reg_base + SDMMC_EISTER, ALL_FLAGS);

	/* Clear all the Normal/Error Interrupt Status Register flags */
	mmio_write_16(reg_base + SDMMC_NISTR, ALL_FLAGS);
	mmio_write_16(reg_base + SDMMC_EISTR, ALL_FLAGS);

	/* Prepare masks */
	not_ready = SDMMC_PSR_CMDINHC;
	if (((cr_reg_value & SDMMC_CR_DPSEL) == SDMMC_CR_DPSEL) ||
	    (cr_reg_value & SDMMC_CR_RESPTYP_Msk) == SDMMC_CR_RESPTYP_RL48BUSY) {
		not_ready |= SDMMC_PSR_CMDINHD;
	}

	/* Wait for CMD and DATn lines becomes ready */
	timeout = EMMC_POLLING_VALUE;
	do {
		if (timeout > 0) {
			state = mmio_read_32(reg_base + SDMMC_PSR);
			timeout--;
			udelay(10);
		} else {
			ERROR("Timeout, CMD and DATn lines not low \n");
			return 1;
		}
	} while (state & not_ready);

	/* Set timeout value, transmission direction and clear accordingly buffer ready bits */
	if (op == SDMMC_MMC_CMD7 || op == SDMMC_MMC_CMD24 || op == SDMMC_MMC_CMD25) {
		lan966x_set_data_timeout(SD_DATA_WRITE);
		mmc_clrbits_16(reg_base + SDMMC_TMR, SDMMC_TMR_DTDSEL_READ);
		mmc_setbits_16(reg_base + SDMMC_NISTER, SDMMC_NISTER_BWRRDY);

	} else {
		lan966x_set_data_timeout(SD_DATA_READ);
		mmc_setbits_16(reg_base + SDMMC_TMR, SDMMC_TMR_DTDSEL_READ);
		mmc_setbits_16(reg_base + SDMMC_NISTER, SDMMC_NISTER_BRDRDY);
	}

	/* Send command */
	mc1r_reg_value = mmio_read_8(reg_base + SDMMC_MC1R);
	mc1r_reg_value &= ~(SDMMC_MC1R_CMDTYP_Msk | SDMMC_MC1R_OPD);	// Clear MMC command type and Open Drain fields
	mc1r_reg_value |= ((op >> 16) & 0xFFFF);

	/* When using eMMC, the FCD (Force Card Detect) bit will be set to 1 to bypass the card
	 * detection procedure by using the SDMMC_CD signal */
	if (lan966x_get_boot_source() == BOOT_SOURCE_EMMC) {
		mc1r_reg_value |= SDMMC_MC1R_FCD;
	}

#if defined(LAN966X_EMMC_TESTS) || defined(IMAGE_BL2U)
	/* When e.g the EVB board is used and LAN966X_EMMC_TESTS is enabled,
	 * the previously called lan966x_get_boot_source() function will
	 * return BOOT_SOURCE_QSPI. Since this is a special test mode,
	 * the value for the FCD needs to be set accordingly */
	mc1r_reg_value |= SDMMC_MC1R_FCD;
#endif

	mmio_write_8(reg_base + SDMMC_MC1R, mc1r_reg_value);
	mmio_write_32(reg_base + SDMMC_ARG1R, cmd->cmd_arg);
	mmio_write_16(reg_base + SDMMC_CR, cr_reg_value);

	/* Poll Interrupt Status Register, wait for command completion */
	if (lan966x_emmc_poll(SDMMC_NISTR_CMDC)) {
		ERROR("Command completion of CMD: %d failed !!! \n", cmd->cmd_idx);
		return 1;
	}

	/* Trigger CMD specific actions after sent */
	switch (cmd->cmd_idx) {
	case 0:
		// No response handling needed
		break;
	case 1:
		cmd->resp_data[0] = mmio_read_32(reg_base + SDMMC_RR0);

		if ((cmd->resp_data[0] & MMC_CARD_POWER_UP_STATUS) == MMC_CARD_POWER_UP_STATUS) {
			p_card.card_type = MMC_CARD;

			if ((cmd->resp_data[0] & MMC_CARD_ACCESS_MODE_msk) ==
			    MMC_CARD_ACCESS_MODE_SECTOR) {
				p_card.card_capacity = MMC_HIGH_DENSITY;
				VERBOSE("HIGH DENSITY MMC \n");
			} else {
				p_card.card_capacity = MMC_NORM_DENSITY;
				VERBOSE("NORMAL DENSITIY MMC \n");
			}
		}
		break;
	case 2:
		lan966x_get_cid_register();
		break;
	case 3:
	case 6:
	case 7:
	case 8:
	case 12:
	case 13:
	case 17:
	case 18:
	case 24:
	case 25:
	case 51:
	case 55:
		cmd->resp_data[0] = mmio_read_32(reg_base + SDMMC_RR0);
		break;
	case 9:
		cmd->resp_data[3] =
		    (mmio_read_32(reg_base + SDMMC_RR3) << 8) |
		    (mmio_read_32(reg_base + SDMMC_RR2) >> 24);
		cmd->resp_data[2] =
		    (mmio_read_32(reg_base + SDMMC_RR2) << 8) |
		    (mmio_read_32(reg_base + SDMMC_RR1) >> 24);
		cmd->resp_data[1] =
		    (mmio_read_32(reg_base + SDMMC_RR1) << 8) |
		    (mmio_read_32(reg_base + SDMMC_RR0) >> 24);
		cmd->resp_data[0] = (mmio_read_32(reg_base + SDMMC_RR0) << 8);
		lan966x_get_csd_register();
		break;
	case 41:
		cmd->resp_data[0] = mmio_read_32(reg_base + SDMMC_RR0);

		if ((cmd->resp_data[0] & SD_CARD_CCS_STATUS) == SD_CARD_HCS_HIGH) {
			p_card.card_capacity = SD_CARD_SDHC_SDXC;
			VERBOSE("SDHC/SDXC (HIGH/EXTENDED CAPACITY) SDMMC\n");
		} else {
			p_card.card_capacity = SD_CARD_SDSC;
			VERBOSE("SDSC (STANDARD CAPACITY) SDMMC\n");
		}
		break;
	default:
		ERROR(" CMD Id %d not supported ! \n", cmd->cmd_idx);
		assert(false);
		break;
	}

	return 0;
}

static int lan966x_recover_error(unsigned int error_int_status)
{
	struct mmc_cmd cmd;
	unsigned int sd_status;

	NOTICE("MMC: recover from error() \n");

	/* Perform software reset CMD and DATn lines */
	lan966x_mmc_reset(SDMMC_SW_RST_DATA_CMD_LINES);

	/* Set data timeout */
	lan966x_set_data_timeout(SD_DATA_WRITE);

	// Send CMD12 : Abort transaction
	cmd.cmd_idx = 12u;
	cmd.cmd_arg = 0u;
	cmd.resp_type = 0u;

	if (lan966x_mmc_send_cmd(&cmd)) {
		/* No response from SD card to CMD12 */
		if (eistr & SDMMC_EISTR_CMDTEO) {
			/* Perform software reset */
			lan966x_mmc_reset(SDMMC_SW_RST_CMD_LINE);

			sd_status = mmio_read_32(reg_base + SDMMC_RR0);

			if ((sd_status & SD_STATUS_CURRENT_STATE) !=
			    STATUS_CURRENT_STATE(MMC_STATE_TRAN)) {
				ERROR("Unrecoverable error: SDMMC_Error_Status = %d\n ", sd_status);
				return 1;
			} else {
				ERROR("Unrecoverable error: SDMMC_Error_Status = %d\n ", sd_status);
				return 1;
			}
		}
	}

	if ((mmio_read_32(reg_base + SDMMC_PSR) & SDMMC_PSR_DATLL_Msk) != SDMMC_PSR_DATLL_Msk) {
		return 1;
	}

	return 0;
}

static int lan966x_mmc_set_ios(unsigned int clk, unsigned int width)
{
	uint8_t bus_width = 0u;
	uint32_t clock = 0u;
	boot_source_type boot_source;

	boot_source = lan966x_get_boot_source();

	VERBOSE("MMC: ATF CB set_ios() \n");

	switch (width) {
	case MMC_BUS_WIDTH_1:
		bus_width = SDMMC_HC1R_DW_1_BIT;
		break;
	case MMC_BUS_WIDTH_4:
		bus_width = SDMMC_HC1R_DW_4_BIT;
		break;
	case MMC_BUS_WIDTH_8:
		/* Check if hardware supports 8-bit bus width capabilities */
		if ((mmio_read_32(reg_base + SDMMC_CA0R) & SDMMC_CA0R_ED8SUP) == SDMMC_CA0R_ED8SUP) {
			bus_width = SDMMC_HC1R_EXTDW;
		}
		break;
	default:
		ERROR("Unsupported mmc bus width of %d \n", width);
		assert(false);
		break;
	}

	INFO("MMC: Set mmc bus_width to: %d\n", width);
	mmc_setbits_8(reg_base + SDMMC_HC1R, bus_width);

	/* Mainly, the desired clock rate should be adjusted by the fw_config
	 * parameter. This check prevents that the maximum allowed clock
	 * settings are exceeded depending on the respective mmc device. */
	if (boot_source == BOOT_SOURCE_EMMC) {
		if (clk >= EMMC_HIGH_SPEED) {
			clock = EMMC_HIGH_SPEED;
		} else {
			clock = clk;
		}
	} else if (boot_source == BOOT_SOURCE_SDMMC) {
		if (clk >= SD_HIGH_SPEED) {
			clock = SD_HIGH_SPEED;
		} else {
			clock = clk;
		}
	}
#if defined(LAN966X_EMMC_TESTS) || defined(IMAGE_BL2U)
	else {
		/* When the EVB board is used and LAN966X_EMMC_TESTS is enabled,
		 * the previously called lan966x_get_boot_source() function will
		 * return BOOT_SOURCE_QSPI. Since this is a special test mode,
		 * this boot_source is not considered and supported here.
		 * Set clock value as requested. */
		clock = clk;
	}
#endif

	INFO("MMC: Set mmc clk_freq to: %d\n", clock);
	if (lan966x_set_clk_freq(clock, SDMMC_CLK_CTRL_PROG_MODE)) {
		return -1;
	}

	return 0;
}

static int lan966x_mmc_prepare(int lba, uintptr_t buf, size_t size)
{
	size_t blocks;
	size_t blocksize;

	VERBOSE("MMC: ATF CB prepare() \n");

	/* Check preconditions for block size and block count */
	if (size < 512) {
		blocksize = size;
		blocks = 1u;
	} else {
		blocksize = MMC_BLOCK_SIZE;
		blocks = size / blocksize;
		if (size % blocksize != 0)
			blocks++;
	}

	/* Setup hardware according identified values */
	if (blocks > 1) {
		mmc_setbits_16(reg_base + SDMMC_TMR, SDMMC_TMR_MSBSEL);
	}

	mmio_write_16(reg_base + SDMMC_BSR, SDMMC_BSR_BLKSIZE(blocksize));
	mmio_write_16(reg_base + SDMMC_BCR, blocks);

	mmc_setbits_16(reg_base + SDMMC_EISTER, SDMMC_EISTER_DATTEO);
	mmc_setbits_16(reg_base + SDMMC_EISIER, SDMMC_EISIER_DATTEO);

	return 0;
}

/*
 * This function is called from the mmc_fill_device_info() callback ->read
 * and provides the data for the mmc_ext_csd structure.
 */
static int lan966x_mmc_read(int lba, uintptr_t buf, size_t size)
{
	unsigned int i;
	unsigned int *pExtBuffer = (unsigned int *)buf;

	VERBOSE("MMC: ATF CB read() \n");

	if (!SD_CARD_STATUS_SUCCESS(mmio_read_32(reg_base + SDMMC_RR0))) {
		ERROR("Error on CMD8 command : SD Card Status = 0x%x\n",
		      mmio_read_32(reg_base + SDMMC_RR0));
		return 1;
	}

	if (lan966x_emmc_poll(SDMMC_NISTR_BRDRDY)) {
		ERROR("Error on CMD8, BRDRDY is not read \n");
		return 1;
	}

	for (i = 0; i < (size / sizeof(uint32_t)); i++) {
		*pExtBuffer = mmio_read_32(reg_base + SDMMC_BDPR);
		pExtBuffer++;
	}

	if (lan966x_emmc_poll(SDMMC_NISTR_TRFC)) {
		lan966x_recover_error(eistr);
		return 1;
	}

	return 0;
}

static int lan966x_mmc_write(int lba, uintptr_t buf, size_t size)
{
	uint32_t i;
	uint32_t *pExtBuffer = (unsigned int *)buf;

	VERBOSE("MMC: ATF CB write() \n");

	if ((mmio_read_32(reg_base + SDMMC_PSR) & SDMMC_PSR_BUFWREN) == SDMMC_PSR_BUFWREN) {

		for (i = 0; i < (size / sizeof(uint32_t)); i++) {
			mmio_write_32(reg_base + SDMMC_BDPR, *pExtBuffer);
			pExtBuffer++;
		}

		if (lan966x_emmc_poll(SDMMC_NISTR_TRFC)) {
			lan966x_recover_error(eistr);
			return 1;
		}
	} else {
		ERROR("MMC write buffer not ready \n");
		return 1;
	}

	return 0;
}

/* Hold the callback information. Map ATF calls to user application code  */
static const struct mmc_ops lan966x_ops = {
	.init = lan966x_mmc_initialize,
	.send_cmd = lan966x_mmc_send_cmd,
	.set_ios = lan966x_mmc_set_ios,
	.prepare = lan966x_mmc_prepare,
	.read = lan966x_mmc_read,
	.write = lan966x_mmc_write,
};

void lan966x_mmc_init(lan966x_mmc_params_t * params, struct mmc_device_info *info)
{
	int retVal;

	VERBOSE("MMC: lan966x_mmc_init() \n");

	assert((params != 0) &&
	       ((params->reg_base & MMC_BLOCK_MASK) == 0) &&
	       ((params->desc_base & MMC_BLOCK_MASK) == 0) &&
	       ((params->desc_size & MMC_BLOCK_MASK) == 0) &&
	       (params->desc_size > 0) &&
	       (params->clk_rate > 0) &&
	       ((params->bus_width == MMC_BUS_WIDTH_1) ||
		(params->bus_width == MMC_BUS_WIDTH_4) ||
		(params->bus_width == MMC_BUS_WIDTH_8)));

	memcpy(&lan966x_params, params, sizeof(lan966x_mmc_params_t));
	lan966x_params.mmc_dev_type = info->mmc_dev_type;
	reg_base = lan966x_params.reg_base;

	retVal = mmc_init(&lan966x_ops, params->clk_rate, params->bus_width, params->flags, info);

	if (retVal != 0) {
		ERROR("MMC initialization phase with default parameters failed!\n");
	}
}
