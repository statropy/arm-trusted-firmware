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

#include "emmc_defs.h"

static card p_card;
static uintptr_t reg_base;
static uint16_t eistr = 0u;	/* Holds the error interrupt status */
static lan966x_mmc_params_t lan966x_params;
static bool use_dma;

static const unsigned int TAAC_TimeExp[8] =
	{ 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000 };
static const unsigned int TAAC_TimeMant[16] =
	{ 0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80 };

#define EMMC_RESET_TIMEOUT_US		(1000 * 500) /* 500 ms */

#pragma weak plat_mmc_use_dma
bool plat_mmc_use_dma(void)
{
	return false;
}

static void lan966x_mmc_reset(lan966x_reset_type type)
{
	uint64_t timeout = timeout_init_us(EMMC_RESET_TIMEOUT_US);
	unsigned int state;

	VERBOSE("MMC: Software reset type: %d\n", type);

	switch (type) {
	case SDMMC_SW_RST_ALL:
		mmc_setbits_8(reg_base, SDMMC_SRR, SDMMC_SRR_SWRSTALL);
		break;
	case SDMMC_SW_RST_CMD_LINE:
		mmc_setbits_8(reg_base, SDMMC_SRR, SDMMC_SRR_SWRSTCMD);
		break;
	case SDMMC_SW_RST_DATA_CMD_LINES:
		sdhci_write_8(reg_base, SDMMC_SRR, (SDMMC_SRR_SWRSTCMD | SDMMC_SRR_SWRSTDAT));
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
			state = sdhci_read_8(reg_base, SDMMC_SRR);
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
	uint16_t new_ccr, old_ccr;
	uint32_t clk_div, base_clock, mult_clock;
	uint32_t state;

	timeout = EMMC_POLLING_VALUE;
	do {
		if (timeout > 0) {
			state = sdhci_read_32(reg_base, SDMMC_PSR);
			timeout--;
			udelay(10);
		} else {
			ERROR("Timeout, CMD and DATn lines not low \n");
			return 1;
		}
	} while (state & (SDMMC_PSR_CMDINHD | SDMMC_PSR_CMDINHC));

	/* Save current value of clock control register */
	old_ccr = sdhci_read_16(reg_base, SDMMC_CCR);

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
	sdhci_write_16(reg_base, SDMMC_CCR, (old_ccr & ~(SDMMC_CCR_SDCLKEN)));

	/* Set new clock configuration */
	sdhci_write_16(reg_base, SDMMC_CCR, new_ccr);

	/* Wait for internal clock to be stable */
	timeout = EMMC_POLLING_VALUE;
	do {
		if (timeout > 0) {
			state = sdhci_read_16(reg_base, SDMMC_CCR);
			timeout--;
			udelay(10);
		} else {
			return 1;
		}
	} while ((state & SDMMC_CCR_INTCLKS) == 0);

	/* Enable SD clock */
	mmc_setbits_16(reg_base, SDMMC_CCR, SDMMC_CCR_SDCLKEN);
	lan966x_params.clk_rate = SD_clock_freq;

	/* Wait for 74 SD clock cycles (according SD Card specification) */
	lan966x_clock_delay(74u);

	return 0;
}

static int lan966x_host_init(void)
{
	unsigned int timeout;
	uint32_t state;

	lan966x_params.clk_rate = 0u;

	/* Set default values for eMMC/SDMMC */
	if (lan966x_params.mmc_dev_type == MMC_IS_EMMC) {
		p_card.card_type = MMC_CARD;
		p_card.card_capacity = MMC_NORM_DENSITY;
	} else if (lan966x_params.mmc_dev_type == MMC_IS_SD) {
		p_card.card_type = SD_CARD;
		p_card.card_capacity = SD_CARD_SDSC;
	}

	lan966x_clk_disable(LAN966X_CLK_ID_SDMMC0);
	lan966x_clk_set_rate(LAN966X_CLK_ID_SDMMC0, LAN966X_CLK_FREQ_SDMMC);
	lan966x_clk_enable(LAN966X_CLK_ID_SDMMC0);

	/* Reset entire SDMMC host hardware */
	lan966x_mmc_reset(SDMMC_SW_RST_ALL);

	/* "SD Bus Power" init. */
	mmc_setbits_8(reg_base, SDMMC_PCR, SDMMC_PCR_SDBPWR);

	/* Reset the SDMMC CMD and DATn lines */
	lan966x_mmc_reset(SDMMC_SW_RST_DATA_CMD_LINES);

	/* Set host controller data bus width to 1 bit */
	mmc_setbits_8(reg_base, SDMMC_HC1R, SDMMC_HC1R_DW_1_BIT);

	if (lan966x_set_clk_freq(MMC_INIT_SPEED, SDMMC_CLK_CTRL_PROG_MODE)) {
		return -1;
	}

	/* Check if sd-card is physically present */
	if (lan966x_params.mmc_dev_type == MMC_IS_SD) {
		/* Set debounce value register and check if sd-card is inserted */
		mmc_setbits_8(reg_base, SDMMC_DEBR, SDMMC_DEBR_CDDVAL(3));

		/* Wait for sd-card stable present bit */
		timeout = EMMC_POLLING_VALUE;
		do {
			if (timeout > 0) {
				state = sdhci_read_32(reg_base, SDMMC_PSR);
				timeout--;
				udelay(10);
			} else {
				return -1;
			}
		} while (!((state & SDMMC_PSR_CARDSS) && (state & SDMMC_PSR_CARDINS)));

		/* Enable SD clock */
		mmc_setbits_16(reg_base, SDMMC_CCR, SDMMC_CCR_SDCLKEN);
	}

	return 0;
}

static void lan966x_mmc_initialize(void)
{
	int retVal;

	VERBOSE("MMC: ATF CB init() \n");

	use_dma = plat_mmc_use_dma();

	retVal = lan966x_host_init();
	if (retVal != 0) {
		ERROR("MMC host initialization failed !\n");
		panic();
	}
}

static void lan966x_get_cid_register(void)
{
	p_card.card_type = (sdhci_read_32(reg_base, SDMMC_RR3) >> 8) & 0x03;
}

static void lan966x_get_csd_register(void)
{
	uint32_t m, e;
	uint32_t csd_struct;
	uint32_t *p_resp;

	/* Initialize pointer to response register RR[0] */
	p_resp = (uint32_t *)(reg_base + SDMMC_RR0);

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
	uint64_t timeout = timeout_init_us(EMMC_POLLING_TIMEOUT);
	uint16_t nistr = 0u;

	eistr = 0u;
	do {
		nistr = sdhci_read_16(reg_base, SDMMC_NISTR);

		/* Check errors */
		if (nistr & SDMMC_NISTR_ERRINT) {
			eistr = sdhci_read_16(reg_base, SDMMC_EISTR);

			/* Dump interrupt status values and variables */
			ERROR(" NISTR: 0x%x \n", nistr);
			ERROR(" NISTR expected: 0x%x \n", expected);
			ERROR(" EISTR: 0x%x \n", eistr);

			/* Clear Normal/Error Interrupt Status Register flags */
			sdhci_write_16(reg_base, SDMMC_EISTR, eistr);
			sdhci_write_16(reg_base, SDMMC_NISTR, nistr);
			return 1;
		}

		/* May need to DMA across boundaries */
		if (nistr & SDMMC_NISTR_DMAINT) {
			uint32_t buf;

			/* Ack DMAINT */
			sdhci_write_16(reg_base, SDMMC_NISTR, SDMMC_NISTR_DMAINT);

			/* SSAR is updated to next DMA address */
			buf = sdhci_read_32(reg_base, SDMMC_SSAR);
			VERBOSE("MMC: DMA irq @ %08x\n", buf);
			/* Writing register restarts DMA */
			sdhci_write_32(reg_base, SDMMC_SSAR, buf);
		}

		/* Wait for any expected flags */
		if (nistr & expected) {
			/* Clear only expected flags */
			sdhci_write_16(reg_base, SDMMC_NISTR, expected);
			return 0;
		}

	} while(!timeout_elapsed(timeout));

	ERROR("MMC: Timeout waiting for %08x - have %08x\n", expected, nistr);

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

	mmc_clrbits_16(reg_base, SDMMC_EISTER, SDMMC_EISTER_DATTEO);
	mmc_clrbits_16(reg_base, SDMMC_EISIER, SDMMC_EISIER_DATTEO);

	if (p_card.card_type == MMC_CARD
	    || (p_card.card_type == SD_CARD && p_card.card_capacity == SD_CARD_SDSC)) {
		/* Set base clock frequency in MHz */
		clock_frequency =
		    (sdhci_read_32(reg_base, SDMMC_CA0R) &
		     SDMMC_CA0R_BASECLKF_Msk) >> SDMMC_CA0R_BASECLKF_Pos;

		clock_div =
		    ((sdhci_read_16(reg_base, SDMMC_CCR) &
		      SDMMC_CCR_SDCLKFSEL_Msk) >> SDMMC_CCR_SDCLKFSEL_Pos) |
		    (((sdhci_read_16(reg_base, SDMMC_CCR) &
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

	if ((sdhci_read_32(reg_base, SDMMC_CA0R) & SDMMC_CA0R_TEOCLKU) == SDMMC_CA0R_TEOCLKU) {
		/* Set value of 1000, because "timeout_val" is in ms */
		timeout_freq_fact = 1000u;
	} else {
		/* Set value of 1, because "timeout_val" is in ms */
		timeout_freq_fact = 1u;
	}

	timeout_freq = (sdhci_read_32(reg_base, SDMMC_CA0R) &
			SDMMC_CA0R_TEOCLKF_Msk) * timeout_freq_fact;
	timeout_cyc = timeout_val * timeout_freq;

	/* timeout_val will be re-used for the data timeout counter */
	timeout_val = 0;
	while (timeout_cyc > 0) {
		timeout_val++;
		timeout_cyc >>= 1;
	}

	/* Clearing Data Time Out Error Status bit before. */
	mmc_setbits_16(reg_base, SDMMC_EISTR, SDMMC_EISTR_DATTEO);
	sdhci_write_8(reg_base, SDMMC_TCR, SDMMC_TCR_DTCVAL(timeout_val - 13));
}

static int lan966x_mmc_send_cmd(struct mmc_cmd *cmd)
{
	uint16_t mc1r_reg_value, cr_reg_value;
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
	case 23:
		op = SDMMC_MMC_CMD23;
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
	sdhci_write_16(reg_base, SDMMC_NISTER, ALL_FLAGS);
	sdhci_write_16(reg_base, SDMMC_EISTER, ALL_FLAGS);

	/* Clear all the Normal/Error Interrupt Status Register flags */
	sdhci_write_16(reg_base, SDMMC_NISTR, ALL_FLAGS);
	sdhci_write_16(reg_base, SDMMC_EISTR, ALL_FLAGS);

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
			state = sdhci_read_32(reg_base, SDMMC_PSR);
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
		mmc_clrbits_16(reg_base, SDMMC_TMR, SDMMC_TMR_DTDSEL_READ);
	} else {
		lan966x_set_data_timeout(SD_DATA_READ);
		mmc_setbits_16(reg_base, SDMMC_TMR, SDMMC_TMR_DTDSEL_READ);
	}

	/* Send command */
	mc1r_reg_value = sdhci_read_8(reg_base, SDMMC_MC1R);
	mc1r_reg_value &= ~(SDMMC_MC1R_CMDTYP_Msk | SDMMC_MC1R_OPD);	// Clear MMC command type and Open Drain fields
	mc1r_reg_value |= ((op >> 16) & 0xFFFF);

	/* When using eMMC, the FCD (Force Card Detect) bit will be set to 1 to bypass the card
	 * detection procedure by using the SDMMC_CD signal */
	if (lan966x_params.mmc_dev_type == MMC_IS_EMMC) {
		mc1r_reg_value |= SDMMC_MC1R_FCD;
	}

#if defined(LAN966X_EMMC_TESTS) || defined(IMAGE_BL2U)
	/* When e.g the EVB board is used and LAN966X_EMMC_TESTS is enabled,
	 * the previously called lan966x_get_boot_source() function will
	 * return BOOT_SOURCE_QSPI. Since this is a special test mode,
	 * the value for the FCD needs to be set accordingly */
	mc1r_reg_value |= SDMMC_MC1R_FCD;
#endif

	sdhci_write_8(reg_base, SDMMC_MC1R, mc1r_reg_value);
	sdhci_write_32(reg_base, SDMMC_ARG1R, cmd->cmd_arg);
	sdhci_write_16(reg_base, SDMMC_CR, cr_reg_value);

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
		cmd->resp_data[0] = sdhci_read_32(reg_base, SDMMC_RR0);

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
	case 23:
	case 24:
	case 25:
	case 51:
	case 55:
		cmd->resp_data[0] = sdhci_read_32(reg_base, SDMMC_RR0);
		break;
	case 9:
		cmd->resp_data[3] =
		    (sdhci_read_32(reg_base, SDMMC_RR3) << 8) |
		    (sdhci_read_32(reg_base, SDMMC_RR2) >> 24);
		cmd->resp_data[2] =
		    (sdhci_read_32(reg_base, SDMMC_RR2) << 8) |
		    (sdhci_read_32(reg_base, SDMMC_RR1) >> 24);
		cmd->resp_data[1] =
		    (sdhci_read_32(reg_base, SDMMC_RR1) << 8) |
		    (sdhci_read_32(reg_base, SDMMC_RR0) >> 24);
		cmd->resp_data[0] = (sdhci_read_32(reg_base, SDMMC_RR0) << 8);
		lan966x_get_csd_register();
		break;
	case 41:
		cmd->resp_data[0] = sdhci_read_32(reg_base, SDMMC_RR0);

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

			sd_status = sdhci_read_32(reg_base, SDMMC_RR0);

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

	if ((sdhci_read_32(reg_base, SDMMC_PSR) & SDMMC_PSR_DATLL_Msk) != SDMMC_PSR_DATLL_Msk) {
		return 1;
	}

	return 0;
}

static int lan966x_mmc_set_ios(unsigned int clk, unsigned int width)
{
	static uint8_t width_codes[] = {1, 4, 8};
	uint8_t bus_width = 0u;
	uint32_t clock = 0u;

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
		if ((sdhci_read_32(reg_base, SDMMC_CA0R) & SDMMC_CA0R_ED8SUP) == SDMMC_CA0R_ED8SUP) {
			bus_width = SDMMC_HC1R_EXTDW;
		}
		break;
	default:
		ERROR("Unsupported mmc bus width of %d \n", width);
		assert(false);
		return -1;
	}

	mmc_setbits_8(reg_base, SDMMC_HC1R, bus_width);

	/* Mainly, the desired clock rate should be adjusted by the fw_config
	 * parameter. This check prevents that the maximum allowed clock
	 * settings are exceeded depending on the respective mmc device. */
	if (lan966x_params.mmc_dev_type == MMC_IS_EMMC) {
		if (clk >= EMMC_HIGH_SPEED) {
			clock = EMMC_HIGH_SPEED;
		} else {
			clock = clk;
		}
	} else if (lan966x_params.mmc_dev_type == MMC_IS_SD) {
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

	INFO("MMC: %d MHz, width %d bits, %s\n",
	     clock / 1000000U, width_codes[width],
	     use_dma ? "SDMA" : "No DMA");

	if (lan966x_set_clk_freq(clock, SDMMC_CLK_CTRL_PROG_MODE)) {
		return -1;
	}

	return 0;
}

static int lan966x_mmc_prepare(int lba, uintptr_t buf, size_t size, bool is_write)
{
	uint16_t mode = SDMMC_TMR_BCEN;
	size_t blocks, blocksize;

	VERBOSE("MMC: prepare - lba %08x, buf %08lx, size %zd, %s\n",
		lba, buf, size, is_write ? "write" : "read");

	/* Check preconditions for block size and block count */
	if (size <= MMC_BLOCK_SIZE) {
		blocksize = size;
		blocks = 1u;
	} else {
		blocksize = MMC_BLOCK_SIZE;
		blocks = DIV_ROUND_UP_2EVAL(size, blocksize);
		/* We have more than one block */
		mode |= SDMMC_TMR_MSBSEL;
	}

	sdhci_write_16(reg_base, SDMMC_BSR,
		       SDMMC_BSR_BOUNDARY(SDMMC_BSR_BOUNDARY_512K) |
		       SDMMC_BSR_BLKSIZE(blocksize));
	sdhci_write_16(reg_base, SDMMC_BCR, blocks);

	mmc_setbits_16(reg_base, SDMMC_EISTER, SDMMC_EISTER_DATTEO);
	mmc_setbits_16(reg_base, SDMMC_EISIER, SDMMC_EISIER_DATTEO);

	if (use_dma) {
		mode |= SDMMC_TMR_DMAEN;
		sdhci_write_32(reg_base, SDMMC_SSAR, buf);
		if (is_write) {
			/* Flush cache -> memory */
			flush_dcache_range(buf, size);
		} else {
			/* Invalidate cache entries */
			inv_dcache_range(buf, size);
		}
	}

	/* Set mode register */
	sdhci_write_16(reg_base, SDMMC_TMR, mode);

	return 0;
}

static int lan966x_mmc_read(int lba, uintptr_t buf, size_t size)
{
	VERBOSE("MMC: read - lba %08x, buf %08lx, size %zd\n", lba, buf, size);

	if (!SD_CARD_STATUS_SUCCESS(sdhci_read_32(reg_base, SDMMC_RR0))) {
		ERROR("Error on CMD8 command : SD Card Status = 0x%x\n",
		      sdhci_read_32(reg_base, SDMMC_RR0));
		return 1;
	}

	/* Need to xfer by CPU? */
	if (!use_dma) {
		uint32_t *pExtBuffer = (__typeof(pExtBuffer)) buf;

		while (size > 0) {
			ssize_t i, blklen = MIN((ssize_t)MMC_BLOCK_SIZE, (ssize_t)size);

			if (lan966x_emmc_poll(SDMMC_NISTR_BRDRDY)) {
				ERROR("PIO read signal not seen\n");
				return 1;
			}

			for (i = 0; i < blklen; i += 4)
				*pExtBuffer++ = sdhci_read_32(reg_base, SDMMC_BDPR);

			/* Done with block */
			size -= blklen;
		}

	}

	if (lan966x_emmc_poll(SDMMC_NISTR_TRFC)) {
		lan966x_recover_error(eistr);
		return 1;
	}

	return 0;
}

static int lan966x_mmc_write(int lba, uintptr_t buf, size_t size)
{
	VERBOSE("MMC: write - lba %08x, buf %08lx, size %zd\n", lba, buf, size);

	/* Need to xfer by CPU? */
	if (!use_dma) {
		uint32_t *pExtBuffer = (__typeof(pExtBuffer)) buf;

		while (size > 0) {
			ssize_t i, blklen = MIN((ssize_t)MMC_BLOCK_SIZE, (ssize_t)size);

			if (lan966x_emmc_poll(SDMMC_NISTR_BWRRDY)) {
				ERROR("PIO write signal not seen\n");
				return 1;
			}

			for (i = 0; i < blklen; i += 4)
				sdhci_write_32(reg_base, SDMMC_BDPR, *pExtBuffer++);

			/* Done with block */
			size -= blklen;
		}
	}

	if (lan966x_emmc_poll(SDMMC_NISTR_TRFC)) {
		lan966x_recover_error(eistr);
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

#if defined(LAN966X_EMMC_TESTS)
	/* When the EVB board is used and LAN966X_EMMC_TESTS is enabled, the
	 * previously called lan966x_get_boot_source() function will return
	 * BOOT_SOURCE_QSPI. Since this is a special test mode, this boot_source
	 * is not considered and supported here. Therefore, the default eMMC
	 * values needs to be set here. */
	lan966x_params.mmc_dev_type = MMC_IS_EMMC;
#endif

	assert((params != 0) &&
	       ((params->reg_base & MMC_BLOCK_MASK) == 0) &&
	       ((params->desc_base & MMC_BLOCK_MASK) == 0) &&
	       ((params->desc_size & MMC_BLOCK_MASK) == 0) &&
	       (params->desc_size > 0));

	if (params->clk_rate == 0) {
		params->clk_rate = MMC_DEFAULT_SPEED;
		WARN("MMC: Clock speed not set - using %d\n", params->clk_rate);
	}

	if ((params->bus_width != MMC_BUS_WIDTH_1) &&
	    (params->bus_width != MMC_BUS_WIDTH_4) &&
	    (params->bus_width != MMC_BUS_WIDTH_8)) {
		WARN("MMC: Bus width %d not valid - using %d\n",
		     params->bus_width, MMC_BUS_WIDTH_1);
		params->bus_width = MMC_BUS_WIDTH_1;
	}

	memcpy(&lan966x_params, params, sizeof(lan966x_mmc_params_t));
	lan966x_params.mmc_dev_type = info->mmc_dev_type;
	reg_base = lan966x_params.reg_base;

	retVal = mmc_init(&lan966x_ops, params->clk_rate, params->bus_width, params->flags, info);

	if (retVal != 0) {
		ERROR("MMC initialization phase with default parameters failed!\n");
	}
}
