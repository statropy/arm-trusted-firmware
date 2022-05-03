/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef EMMC_H
#define EMMC_H

#include <drivers/mmc.h>
#include <lib/mmio.h>

#define MMC_INIT_SPEED		375000u
#define MMC_DEFAULT_SPEED	10000000u
#define SD_NORM_SPEED		25000000u
#define SD_HIGH_SPEED		50000000u
#define EMMC_NORM_SPEED		26000000u
#define EMMC_HIGH_SPEED		52000000u

#define SDMMC_CLK_CTRL_DIV_MODE		0
#define SDMMC_CLK_CTRL_PROG_MODE	1

#define ALL_FLAGS	(0xFFFFu)
#define SD_CARD_SUPP_VOLT		0x00FF8000

#define EMMC_POLL_LOOP_DELAY	8u		/* 8µs */
#define EMMC_POLLING_TIMEOUT	2000000u	/* 2sec */
#define EMMC_POLLING_VALUE	10000u

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

typedef struct lan966x_mmc_params {
	uintptr_t reg_base;
	uintptr_t desc_base;
	size_t desc_size;
	int clk_rate;
	int bus_width;
	unsigned int flags;
	enum mmc_device_type mmc_dev_type;
} lan966x_mmc_params_t;

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

static inline void mmc_setbits_16(uintptr_t addr, uint16_t set)
{
	mmio_write_16(addr, mmio_read_16(addr) | set);
}

static inline void mmc_clrbits_16(uintptr_t addr, uint16_t clear)
{
	mmio_write_16(addr, mmio_read_16(addr) & ~clear);
}

static inline void mmc_setbits_8(uintptr_t addr, uint8_t set)
{
	mmio_write_8(addr, mmio_read_8(addr) | set);
}

void lan966x_mmc_init(lan966x_mmc_params_t * params,
		      struct mmc_device_info *info);

#endif	/* EMMC_H */
