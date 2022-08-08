/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <lib/libc/errno.h>
#include <lib/mmio.h>
#include <lib/utils.h>
#include <string.h>

#include <drivers/delay_timer.h>
#include <drivers/microchip/lan966x_clock.h>
#include <drivers/microchip/qspi.h>
#include <drivers/spi_nor.h>

#include "lan966x_private.h"

#define SPI_READY_TIMEOUT_US	40000U

#define ERASE_BLOCK_SIZE	(4 * 1024U)
#define WRITE_BLOCK_SIZE	256U

static uintptr_t reg_base;
static bool qspi_init_done;

static struct spi_mem_op default_read_op;

#define SST_ID			0xBFU

/* QSPI register offsets */
#define QSPI_CR	     0x0000  /* Control Register */
#define QSPI_MR	     0x0004  /* Mode Register */
#define QSPI_RDR     0x0008  /* Receive Data Register */
#define QSPI_TDR     0x000c  /* Transmit Data Register */
#define QSPI_ISR     0x0010  /* Interrupt Status Register */
#define QSPI_IER     0x0014  /* Interrupt Enable Register */
#define QSPI_IDR     0x0018  /* Interrupt Disable Register */
#define QSPI_IMR     0x001c  /* Interrupt Mask Register */
#define QSPI_SCR     0x0020  /* Serial Clock Register */
#define QSPI_SR	     0x0024  /* Status Register */

#define QSPI_IAR     0x0030  /* Instruction Address Register */
#define QSPI_WICR    0x0034  /* Write Instruction Code Register */
#define QSPI_IFR     0x0038  /* Instruction Frame Register */
#define QSPI_RICR    0x003C  /* Read Instruction Code Register */

#define QSPI_SMR     0x0040  /* Scrambling Mode Register */
#define QSPI_SKR     0x0044  /* Scrambling Key Register */

#define QSPI_REFRESH 0x0050  /* Refresh Register */
#define QSPI_WRACNT  0x0054  /* Write Access Counter Register */
#define QSPI_DLLCFG  0x0058  /* DLL Configuration Register */
#define QSPI_PCALCFG 0x005c  /* Pad Calibration Configuration Register */
#define QSPI_PCALBP  0x0060  /* Pad Calibration Bypass Register */
#define QSPI_TOUT    0x0064  /* Timeout Register */

#define QSPI_DEBUG   0x00D0  /* Debug Register */

#define QSPI_WPMR    0x00E4  /* Write Protection Mode Register */
#define QSPI_WPSR    0x00E8  /* Write Protection Status Register */

#define QSPI_VERSION 0x00FC  /* Version Register */

/* Bitfields in QSPI_CR (Control Register) */
#define QSPI_CR_QSPIEN			BIT(0)
#define QSPI_CR_QSPIDIS			BIT(1)
#define QSPI_CR_DLLON			BIT(2)
#define QSPI_CR_DLLOFF			BIT(3)
#define QSPI_CR_STPCAL			BIT(4)
#define QSPI_CR_SRFRSH			BIT(5)
#define QSPI_CR_SWRST			BIT(7)
#define QSPI_CR_UPDCFG			BIT(8)
#define QSPI_CR_STTFR			BIT(9)
#define QSPI_CR_RTOUT			BIT(10)
#define QSPI_CR_LASTXFER		BIT(24)

/* Bitfields in QSPI_MR (Mode Register) */
#define QSPI_MR_SMM			BIT(0)
#define QSPI_MR_WDRBT			BIT(2)
#define QSPI_MR_DQSDLYEN		BIT(3)
#define QSPI_MR_CSMODE_MASK		GENMASK(5, 4)
#define QSPI_MR_CSMODE_NOT_RELOADED	(0 << 4)
#define QSPI_MR_CSMODE_LASTXFER		(1 << 4)
#define QSPI_MR_CSMODE_SYSTEMATICALLY	(2 << 4)
#define QSPI_MR_TAMCLR			BIT(7)
#define QSPI_MR_NBBITS_MASK		GENMASK(11, 8)
#define QSPI_MR_NBBITS(n)		((((n) - 8) << 8) & QSPI_MR_NBBITS_MASK)
#define QSPI_MR_QICMEN			BIT(13)
#define QSPI_MR_PHYCR			BIT(14)
#define QSPI_MR_OENSD			BIT(15)
#define QSPI_MR_DLYBCT_MASK		GENMASK(23, 16)
#define QSPI_MR_DLYBCT(n)		(((n) << 16) & QSPI_MR_DLYBCT_MASK)
#define QSPI_MR_DLYCS_MASK		GENMASK(31, 24)
#define QSPI_MR_DLYCS(n)		(((n) << 24) & QSPI_MR_DLYCS_MASK)

/* Bitfields in QSPI_ISR (Interrupt Status Register) */
#define QSPI_ISR_RDRF			 BIT(0)
#define QSPI_ISR_TDRE			 BIT(1)
#define QSPI_ISR_TXEMPTY		 BIT(2)
#define QSPI_ISR_OVRES			 BIT(3)
#define QSPI_ISR_CSR			 BIT(8)
#define QSPI_ISR_INSTRE			 BIT(10)
#define QSPI_ISR_LWRA			 BIT(11)
#define QSPI_ISR_QITF			 BIT(12)
#define QSPI_ISR_QITR			 BIT(13)
#define QSPI_ISR_CSFA			 BIT(14)
#define QSPI_ISR_CSRA			 BIT(15)
#define QSPI_ISR_RFRSHD			 BIT(16)
#define QSPI_ISR_TOUT			 BIT(17)

/* Bitfields in QSPI_SCR (Serial Clock Register) */
#define QSPI_SCR_CPOL			 BIT(0)
#define QSPI_SCR_CPHA			 BIT(1)
#define QSPI_SCR_DLYBS_MASK		 GENMASK(23, 16)
#define QSPI_SCR_DLYBS(n)		 (((n) << 16) & QSPI_SCR_DLYBS_MASK)

/* Bitfields in QSPI_SR (Status Register) */
#define QSPI_SR_SYNCBSY			 BIT(0)
#define QSPI_SR_QSPIENS			 BIT(1)
#define QSPI_SR_CSS			 BIT(2)
#define QSPI_SR_RBUSY			 BIT(3)
#define QSPI_SR_HIDLE			 BIT(4)
#define QSPI_SR_DLOCK			 BIT(5)
#define QSPI_SR_CALBSY			 BIT(6)

/* Bitfields in QSPI_WICR (Write Instruction Code Register) */
#define QSPI_WICR_WRINST_MASK		 GENMASK(15, 0)
#define QSPI_WICR_WRINST(inst)		 (((inst) << 0) & QSPI_WICR_WRINST_MASK)
#define QSPI_WICR_WROPT_MASK		 GENMASK(23, 16)
#define QSPI_WICR_WROPT(opt)		 (((opt) << 16) & QSPI_WICR_WROPT_MASK)

/* Bitfields in QSPI_IFR (Instruction Frame Register) */
#define QSPI_IFR_WIDTH_MASK		 GENMASK(3, 0)
#define QSPI_IFR_WIDTH_SINGLE_BIT_SPI	 (0 << 0)
#define QSPI_IFR_WIDTH_DUAL_OUTPUT	 (1 << 0)
#define QSPI_IFR_WIDTH_QUAD_OUTPUT	 (2 << 0)
#define QSPI_IFR_WIDTH_DUAL_IO		 (3 << 0)
#define QSPI_IFR_WIDTH_QUAD_IO		 (4 << 0)
#define QSPI_IFR_WIDTH_DUAL_CMD		 (5 << 0)
#define QSPI_IFR_WIDTH_QUAD_CMD		 (6 << 0)
#define QSPI_IFR_WIDTH_OCT_OUTPUT	 (7 << 0)
#define QSPI_IFR_WIDTH_OCT_IO		 (8 << 0)
#define QSPI_IFR_WIDTH_OCT_CMD		 (9 << 0)
#define QSPI_IFR_INSTEN			 BIT(4)
#define QSPI_IFR_ADDREN			 BIT(5)
#define QSPI_IFR_OPTEN			 BIT(6)
#define QSPI_IFR_DATAEN			 BIT(7)
#define QSPI_IFR_OPTL_MASK		 GENMASK(9, 8)
#define QSPI_IFR_OPTL_1BIT		 (0 << 8)
#define QSPI_IFR_OPTL_2BIT		 (1 << 8)
#define QSPI_IFR_OPTL_4BIT		 (2 << 8)
#define QSPI_IFR_OPTL_8BIT		 (3 << 8)
#define QSPI_IFR_ADDRL_MASK		 GENMASK(11, 10)
#define QSPI_IFR_ADDRL_8BIT		 (0 << 10)
#define QSPI_IFR_ADDRL_16BIT		 (1 << 10)
#define QSPI_IFR_ADDRL_24BIT		 (2 << 10)
#define QSPI_IFR_ADDRL_32BIT		 (3 << 10)
#define QSPI_IFR_TFRTYP			 BIT(12)
#define QSPI_IFR_CRM			 BIT(14)
#define QSPI_IFR_DDREN			 BIT(15)
#define QSPI_IFR_NBDUM_MASK		 GENMASK(20, 16)
#define QSPI_IFR_NBDUM(n)		 (((n) << 16) & QSPI_IFR_NBDUM_MASK)
#define QSPI_IFR_END			 BIT(22)
#define QSPI_IFR_SMRM			 BIT(23)
#define QSPI_IFR_APBTFRTYP		 BIT(24)
#define QSPI_IFR_DQSEN			 BIT(25)
#define QSPI_IFR_DDRCMDEN		 BIT(26)
#define QSPI_IFR_HFWBEN			 BIT(27)
#define QSPI_IFR_PROTTYP_MASK		 GENMASK(29, 28)
#define QSPI_IFR_PROTTYP_STD_SPI	 (0 << 28)
#define QSPI_IFR_PROTTYP_TWIN_QUAD	 (1 << 28)
#define QSPI_IFR_PROTTYP_OCTAFLASH	 (2 << 28)
#define QSPI_IFR_PROTTYP_HYPERFLASH	 (3 << 28)

/* Bitfields in QSPI_RICR (Read Instruction Code Register) */
#define QSPI_RICR_RDINST_MASK		 GENMASK(15, 0)
#define QSPI_RICR_RDINST(inst)		 (((inst) << 0) & QSPI_RICR_RDINST_MASK)
#define QSPI_RICR_RDOPT_MASK		 GENMASK(23, 16)
#define QSPI_RICR_RDOPT(opt)		 (((opt) << 16) & QSPI_RICR_RDOPT_MASK)

/* Bitfields in QSPI_SMR (Scrambling Mode Register) */
#define QSPI_SMR_SCREN			 BIT(0)
#define QSPI_SMR_RVDIS			 BIT(1)
#define QSPI_SMR_SCRKL			 BIT(2)

/* Bitfields in QSPI_WPMR (Write Protection Mode Register) */
#define QSPI_WPMR_WPEN			 BIT(0)
#define QSPI_WPMR_WPITEN		 BIT(1)
#define QSPI_WPMR_WPCREB		 BIT(2)
#define QSPI_WPMR_WPKEY_MASK		 GENMASK(31, 8)
#define QSPI_WPMR_WPKEY(wpkey)		 (((wpkey) << 8) & QSPI_WPMR_WPKEY_MASK)

/* Bitfields in QSPI_WPSR (Write Protection Status Register) */
#define QSPI_WPSR_WPVS			 BIT(0)
#define QSPI_WPSR_WPVSRC_MASK		 GENMASK(15, 8)
#define QSPI_WPSR_WPVSRC(src)		 (((src) << 8) & QSPI_WPSR_WPVSRC)

#define QSPI_DLYBS			 0x2
#define QSPI_DLYCS			 0x7
#define QSPI_WPKEY			 0x515350
#define QSPI_TIMEOUT			 0x1000U

/* Non-std Command codes */
#define SPI_NOR_OP_BE_4K_PMC	 0xd7	 /* Erase 4KiB block on PMC chips */
#define SPI_NOR_OP_BE_32K	 0x52	 /* Erase 32KiB block */
#define SPI_NOR_OP_CHIP_ERASE	 0xc7	 /* Erase whole flash chip */
#define SPI_NOR_OP_SE		 0xd8	 /* Sector erase (usually 64KiB) */
#define SPI_NOR_OP_WRDI		 0x04	 /* Write disable */

/* SPI_MEM utility functions */
static void qspi_set_op(struct spi_mem_op *op, uint8_t cmd, enum spi_mem_data_dir dir)
{
	zeromem(op, sizeof(*op));

	op->cmd.buswidth = 1;
	op->cmd.opcode = cmd;

	op->data.dir = dir;
}

static void qspi_set_op_data(struct spi_mem_op *op, uint8_t cmd, enum spi_mem_data_dir dir, uint8_t *buf, unsigned int len)
{
	qspi_set_op(op, cmd, dir);

	op->addr.buswidth = 1;
	op->addr.nbytes = 3;

	op->data.buswidth = 1;
	op->data.buf = buf;
	op->data.nbytes = len;
}

/*
 * Returns 'true' iff timeout occurred, 'false' otherwise.
 */
static bool qspi_wait_flag(const uintptr_t reg,
			   const uint32_t flag,
			   const uint32_t value,
			   const char *fname)
{
	uint64_t timeout = timeout_init_us(QSPI_TIMEOUT);
	uint32_t w;

	do {
		w = mmio_read_32(reg);
		if ((w & flag) == value)
			return false;
	} while (!timeout_elapsed(timeout));

	ERROR("QSPI: Timeout waiting for %s %s (%08x, %08x)\n", fname,
	      value ? "set" : "clear", w, flag);

	return true;
}

static bool qspi_wait_flag_set(const uintptr_t reg,
			       const uint32_t flag,
			       const char *fname)
{
	return qspi_wait_flag(reg, flag, flag, fname);
}

static bool qspi_wait_flag_clear(const uintptr_t reg,
				 const uint32_t flag,
				 const char *fname)
{
	return qspi_wait_flag(reg, flag, 0x0, fname);
}

static const struct lan966x_qspi_mode {
	uint8_t cmd_buswidth;
	uint8_t addr_buswidth;
	uint8_t data_buswidth;
	uint32_t config;
} lan966x_qspi_modes[] = {
	{ 1, 1, 1, QSPI_IFR_WIDTH_SINGLE_BIT_SPI },
	{ 1, 1, 2, QSPI_IFR_WIDTH_DUAL_OUTPUT },
	{ 1, 1, 4, QSPI_IFR_WIDTH_QUAD_OUTPUT },
	{ 1, 2, 2, QSPI_IFR_WIDTH_DUAL_IO },
	{ 1, 4, 4, QSPI_IFR_WIDTH_QUAD_IO },
	{ 2, 2, 2, QSPI_IFR_WIDTH_DUAL_CMD },
	{ 4, 4, 4, QSPI_IFR_WIDTH_QUAD_CMD },
};

static inline bool lan966x_qspi_is_compatible(const struct spi_mem_op *op,
					      const struct lan966x_qspi_mode *mode)
{
	if (op->cmd.buswidth != mode->cmd_buswidth)
		return false;

	if (op->addr.nbytes && op->addr.buswidth != mode->addr_buswidth)
		return false;

	if (op->data.nbytes && op->data.buswidth != mode->data_buswidth)
		return false;

	return true;
}

static int lan966x_qspi_find_mode(const struct spi_mem_op *op)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(lan966x_qspi_modes); i++)
		if (lan966x_qspi_is_compatible(op, &lan966x_qspi_modes[i]))
			return i;

	return -ENOTSUP;
}

static int lan966x_qspi_set_cfg(struct spi_mem_op *op, uint32_t *ifr, uint32_t *offset)
{
	uint32_t dummy_cycles = 0;
	int mode;

	mode = lan966x_qspi_find_mode(op);
	if (mode < 0)
		return mode;
	*ifr = lan966x_qspi_modes[mode].config;
	*offset = 0;

	if (op->cmd.opcode != SPI_NOR_OP_READ_FAST &&
	    op->cmd.opcode != SPI_NOR_OP_PP) {
		if (op->data.dir == SPI_MEM_DATA_IN)
			*ifr |= QSPI_IFR_SMRM | QSPI_IFR_APBTFRTYP;
		else
			*ifr |= QSPI_IFR_SMRM;
	} else {
		*ifr |= QSPI_IFR_TFRTYP;
	}

	/* Write op command */
	if (op->data.dir == SPI_MEM_DATA_IN) {
		mmio_write_32(reg_base + QSPI_RICR, QSPI_RICR_RDINST(op->cmd.opcode));
		mmio_write_32(reg_base + QSPI_WICR, 0);
	} else if (op->data.dir == SPI_MEM_DATA_OUT) {
		mmio_write_32(reg_base + QSPI_WICR, QSPI_WICR_WRINST(op->cmd.opcode));
		mmio_write_32(reg_base + QSPI_RICR, 0);
	}
	*ifr |= QSPI_IFR_INSTEN;

	if (op->dummy.buswidth && op->dummy.nbytes)
		dummy_cycles = op->dummy.nbytes * 8 / op->dummy.buswidth;

	if (op->addr.buswidth) {
		switch (op->addr.nbytes) {
		case 0:
			break;
		case 1:
			*ifr |= QSPI_IFR_ADDREN | QSPI_IFR_ADDRL_8BIT;
			*offset = op->addr.val & 0xff;
			break;
		case 2:
			*ifr |= QSPI_IFR_ADDREN | QSPI_IFR_ADDRL_16BIT;
			*offset = op->addr.val & 0xffff;
			break;
		case 3:
			*ifr |= QSPI_IFR_ADDREN | QSPI_IFR_ADDRL_24BIT;
			*offset = op->addr.val & 0xffffff;
			break;
		case 4:
			*ifr |= QSPI_IFR_ADDREN | QSPI_IFR_ADDRL_32BIT;
			*offset = op->addr.val;
			break;
		default:
			return -ENOTSUP;
		}
	}

	/*
	 * if it is an erase operation the address is in data and it should
	 * expect any data written
	 */
	if (op->cmd.opcode == SPI_NOR_OP_BE_4K ||
	    op->cmd.opcode == SPI_NOR_OP_BE_4K_PMC ||
	    op->cmd.opcode == SPI_NOR_OP_BE_32K ||
	    op->cmd.opcode == SPI_NOR_OP_CHIP_ERASE ||
	    op->cmd.opcode == SPI_NOR_OP_SE) {
		int iar = 0, i = 0;

		for (i = 0; i < op->data.nbytes; ++i) {
			iar <<= 8;
			iar += ((uint8_t *)op->data.buf)[i];
		}

		*ifr |= QSPI_IFR_ADDREN;
		*ifr |= QSPI_IFR_ADDRL_24BIT;

		op->data.nbytes = 0;
		*offset = iar;
	}

	/* Set number of dummy cycles */
	if (dummy_cycles)
		*ifr |= QSPI_IFR_NBDUM(dummy_cycles);

	/* Set data enable */
	if (op->data.nbytes)
		*ifr |= QSPI_IFR_DATAEN;

	/*
	 * Set QSPI controller in Serial Memory Mode (SMM).
	 */
	mmio_write_32(reg_base + QSPI_MR, QSPI_MR_SMM);

	return 0;
}

static int qspi_do_update(void)
{
	/* Synchronize configuration */
	if (qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -EIO;

	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_UPDCFG);

	/* Wait end of QSPI sync busy */
	if (qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -EIO;

	return 0;
}


static int qspi_init_controller(void)
{
	/* Disable in a failsafe way */
	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_DLLOFF);

#ifdef LAN966X_ASIC
	if (qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_DLOCK, "SR:DLOCK"))
		return -EIO;
#endif

	/* Set DLLON and STPCAL register */
	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_DLLON | QSPI_CR_STPCAL);

#ifdef LAN966X_ASIC
	if (qspi_wait_flag_set(reg_base + QSPI_SR, QSPI_SR_DLOCK, "SR:DLOCK"))
		return -EIO;
#endif

	/* Disable QSPI controller */
	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_QSPIDIS);

	/* Synchronize configuration */
	if (qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -EIO;

	/* Reset the QSPI controller */
	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_SWRST);

	/* Synchronize configuration */
	if (qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -EIO;

	/* Disable write protection */
	mmio_write_32(reg_base + QSPI_WPMR, QSPI_WPMR_WPKEY(QSPI_WPKEY));

	/* Set DLLON and STPCAL register */
	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_DLLON | QSPI_CR_STPCAL);

#if defined(LAN966X_ASIC)
	if (qspi_wait_flag_set(reg_base + QSPI_SR, QSPI_SR_DLOCK, "SR:DLOCK"))
		return -EIO;
#endif

	/* Set the QSPI controller by default in Serial Memory Mode */
	mmio_write_32(reg_base + QSPI_MR, QSPI_MR_SMM | QSPI_MR_DLYCS(QSPI_DLYCS));

	/* Set DLYBS */
	mmio_write_32(reg_base + QSPI_SCR, QSPI_SCR_DLYBS(QSPI_DLYBS));

	/* Synchronize configuration */
	if (qspi_do_update())
		return -EIO;

	/* Enable the QSPI controller */
	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_QSPIEN);

	/* Wait effective enable */
	if (qspi_wait_flag_set(reg_base + QSPI_SR, QSPI_SR_QSPIENS, "SR:QSPIENS"))
		return -EIO;

	return 0;
}

static int qspi_change_ifr(uint32_t ifr)
{
	/* Write instruction frame */
	mmio_write_32(reg_base + QSPI_IFR, ifr);

	/* Perform dummy read */
	mmio_read_32(reg_base + QSPI_SR);

	/* Wait for QSPI_SR_RBUSY */
	if (qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_RBUSY, "SR:RBUSY"))
		return -ETIMEDOUT;

	/* Wait for SR.RBUSY == 0 */
	if (qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -ETIMEDOUT;

	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_LASTXFER);

	/* wait for SR.HIDLE == 1 */
	if (qspi_wait_flag_set(reg_base + QSPI_SR, QSPI_SR_HIDLE, "SR:HIDLE"))
		return -ETIMEDOUT;

	if (qspi_do_update())
		return -ETIMEDOUT;

	return 0;
}

static int lan966x_qspi_reg_acc(const struct spi_mem_op *op, uint32_t ifr, uint32_t iar)
{
	VERBOSE("qspi: reg %s, cmd %02x, ifr %08x, iar %08x\n",
		op->data.dir == SPI_MEM_DATA_IN ? "in" : "out", op->cmd.opcode, ifr, iar);

	mmio_write_32(reg_base + QSPI_IAR, iar);

	/* Write instruction frame */
	qspi_change_ifr(ifr);

	/* Skip to the final step if there is no data */
	if (op->data.nbytes == 0) {
		mmio_write_32(reg_base + QSPI_CR, QSPI_CR_STTFR);
		goto no_data;
	}

	/* if read reg operation */
	if (ifr & QSPI_IFR_APBTFRTYP) {
		uint32_t i = 0;
		uint32_t remaining = op->data.nbytes;
		uint8_t *rx = op->data.buf;

		/* always wait for SR.SYNCBSY == 0 */
		if (qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
			return -1;

		/* start transfer */
		mmio_write_32(reg_base + QSPI_CR, QSPI_CR_STTFR);

		/* read buffer_len - 1 data */
		while (remaining > 1) {
			/* wait for ISFR.RDRF */
			if (qspi_wait_flag_set(reg_base + QSPI_ISR, QSPI_ISR_RDRF, "ISR:RDRF"))
				return -1;

			/* wait end of QSPI sync busy */
			if (qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
				return -1;

			rx[i] = mmio_read_32(reg_base + QSPI_RDR);

			remaining--;
			i++;
		}

		/* Wait for ISFR.RDRF */
		if (qspi_wait_flag_set(reg_base + QSPI_ISR, QSPI_ISR_RDRF, "ISR:RDRF"))
			return -1;

		/* wait end of QSPI sync busy */
		if (qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
			return -1;

		/* chip select release */
		mmio_write_32(reg_base + QSPI_CR, QSPI_CR_LASTXFER);

		if (qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
			return -1;

		rx[i] = mmio_read_32(reg_base + QSPI_RDR);
	} else {
		uint32_t i = 0;
		const uint8_t *tx = op->data.buf;

		for (i = 0; i < op->data.nbytes; ++i) {
			/* wait for ISR.TDRE */
			if (qspi_wait_flag_set(reg_base + QSPI_ISR, QSPI_ISR_TDRE, "ISR:TDRE"))
				return -1;

			/* write data */
			mmio_write_32(reg_base + QSPI_TDR, tx[i]);
		}

		/* wait for ISR.TXEMPTY */
		if (qspi_wait_flag_set(reg_base + QSPI_ISR, QSPI_ISR_TXEMPTY, "ISR:TXEMPTY"))
			return -1;

		/* wait end of QSPI sync busy */
		if (qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
			return -1;

		mmio_write_32(reg_base + QSPI_SR, QSPI_CR_LASTXFER);
	}

no_data:
	if (qspi_wait_flag_set(reg_base + QSPI_ISR, QSPI_ISR_CSRA, "ISR:CSRA"))
		return -1;

	VERBOSE("qspi: reg success\n");

	return 0;
}

static int qspi_exec_op(struct spi_mem_op *op)
{
	uint32_t ifr, iar;
	int err;

	err = lan966x_qspi_set_cfg(op, &ifr, &iar);
	if (err) {
		NOTICE("qspi: cmd %02x cfg fails: %d\n", op->cmd.opcode, err);
		return err;
	}

	if (op->cmd.opcode != SPI_NOR_OP_READ_FAST &&
	    op->cmd.opcode != SPI_NOR_OP_PP)
		return lan966x_qspi_reg_acc(op, ifr, iar);

	VERBOSE("qspi: mem %s, cmd %02x, ifr %08x, off %08x\n",
		op->data.dir == SPI_MEM_DATA_IN ? "in" : "out", op->cmd.opcode, ifr, iar);

	if (op->data.nbytes == 0)
		return 0;

	if (op->data.dir == SPI_MEM_DATA_OUT) {
		/* Write count */
		mmio_write_32(reg_base + QSPI_WRACNT, op->data.nbytes);
	}

	/* Write instruction frame */
	qspi_change_ifr(ifr);

	/* Move the data */
	if (op->data.dir == SPI_MEM_DATA_IN) {
		memcpy(op->data.buf, (void *) (LAN966X_QSPI0_MMAP + iar), op->data.nbytes);

		if (qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_RBUSY, "SR:RBUSY"))
			return -ETIMEDOUT;
	} else {
		memcpy((void *) (LAN966X_QSPI0_MMAP + iar), op->data.buf, op->data.nbytes);

		/* Wait 'Last Write Access' */
		if (qspi_wait_flag_set(reg_base + QSPI_ISR, QSPI_ISR_LWRA, "ISR:LWRA"))
			return -ETIMEDOUT;
	}

	/* Wait sync to clear */
	if (qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -ETIMEDOUT;

	/* Release the chip-select */
	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_LASTXFER);

	/* Poll Instruction End and Chip Select Rise flags. */
	if (qspi_wait_flag_set(reg_base + QSPI_ISR, QSPI_ISR_CSRA, "ISR:CSRA"))
		return -ETIMEDOUT;

	VERBOSE("qspi: mem success\n");

	return 0;
}

static int spi_nor_reg(uint8_t reg, uint8_t *buf, size_t len,
		       enum spi_mem_data_dir dir)
{
	struct spi_mem_op op;

	qspi_set_op(&op, reg, dir);

	op.data.buswidth = 1;
	op.data.buf = buf;
	op.data.nbytes = len;

	return qspi_exec_op(&op);
}

static inline int spi_nor_read_id(uint8_t *id)
{
	return spi_nor_reg(SPI_NOR_OP_READ_ID, id, 1U, SPI_MEM_DATA_IN);
}

static inline int spi_nor_read_sr(uint8_t *sr)
{
	return spi_nor_reg(SPI_NOR_OP_READ_SR, sr, sizeof(*sr), SPI_MEM_DATA_IN);
}

static inline int spi_nor_write_en(void)
{
	return spi_nor_reg(SPI_NOR_OP_WREN, NULL, 0U, SPI_MEM_DATA_OUT);
}

static inline int spi_nor_write_dis(void)
{
	return spi_nor_reg(SPI_NOR_OP_WRDI, NULL, 0U, SPI_MEM_DATA_OUT);
}

static int spi_nor_ready(void)
{
#define SR_WIP			BIT(0)	/* Write in progress */
	uint8_t sr;
	int ret;

	ret = spi_nor_read_sr(&sr);
	if (ret != 0) {
		return ret;
	}

	return (((sr & SR_WIP) != 0U) ? 1 : 0);
}

static int spi_nor_wait_ready(void)
{
	int ret;
	uint64_t timeout = timeout_init_us(SPI_READY_TIMEOUT_US);

	while (!timeout_elapsed(timeout)) {
		ret = spi_nor_ready();
		if (ret <= 0) {
			return ret;
		}
	}

	return -ETIMEDOUT;
}

static int spi_nor_global_unlock(void)
{
	int ret;

	ret = spi_nor_write_en();
	if (ret != 0) {
		return ret;
	}

	ret = spi_nor_reg(SPI_NOR_OP_ULBPR, NULL, 0U, SPI_MEM_DATA_OUT);
	if (ret != 0) {
		return -EINVAL;
	}

	ret = spi_nor_wait_ready();
	if (ret != 0) {
		return ret;
	}

	return 0;
}

int qspi_write_enable(void)
{
	struct spi_mem_op op;

	qspi_set_op(&op, SPI_NOR_OP_WREN, SPI_MEM_DATA_OUT);
	return qspi_exec_op(&op);
}

int qspi_write_disable(void)
{
	struct spi_mem_op op;

	qspi_set_op(&op, SPI_NOR_OP_WRDI, SPI_MEM_DATA_OUT);
	return qspi_exec_op(&op);
}

static int qspi_erase_sector(uint32_t addr)
{
	uint8_t buf[3];
	int i, ret;

	VERBOSE("qspi: Erase 4k @ %08x\n", addr);

	ret = qspi_write_enable();
	if (ret)
		return ret;

	/* Prepare sector offset */
	for (i = sizeof(buf) - 1; i >= 0; i--) {
		buf[i] = addr & 0xff;
		addr >>= 8;
	}

	return spi_nor_reg(SPI_NOR_OP_BE_4K, buf, sizeof(buf), SPI_MEM_DATA_OUT);
}

int qspi_erase(uint32_t offset, size_t len)
{
	uint32_t addr;
	int ret;

	if (offset & (ERASE_BLOCK_SIZE-1))
		return -EINVAL;

	for (addr = offset; addr < (offset + len); addr += ERASE_BLOCK_SIZE) {
		ret = qspi_erase_sector(addr);
		if (ret)
			break;
		ret = spi_nor_wait_ready();
		if (ret)
			break;
	}

	(void) qspi_write_disable();

	return ret;
}

int qspi_write_data_page(uint32_t offset, const void *buf, size_t len)
{
	struct spi_mem_op op;
	int ret;

	/* Write enable */
	ret = qspi_write_enable();
	if (ret)
		goto fail;

	/* Now write */
	qspi_set_op_data(&op, SPI_NOR_OP_PP, SPI_MEM_DATA_OUT, (void *) buf, len);
	op.addr.val = offset;
	ret = qspi_exec_op(&op);
	if (ret)
		goto fail;

	/* Wait for write to complete */
	ret = spi_nor_wait_ready();
	if (ret)
		goto fail;

	VERBOSE("qspi: Write %d bytes succeeds\n", len);
	return 0;

fail:
	NOTICE("qspi: Write %d bytes fail: %d\n", len, ret);
	return ret;
}

int qspi_write_data(uint32_t offset, const void *buf, size_t len)
{
	uint32_t addr;
	int ret = 0;

	if (offset & (WRITE_BLOCK_SIZE-1))
		return -EINVAL;

	for (addr = offset; addr < (offset + len); addr += WRITE_BLOCK_SIZE, buf += WRITE_BLOCK_SIZE) {
		uint32_t block = MIN(WRITE_BLOCK_SIZE, len);

		ret = qspi_write_data_page(addr, buf, block);
		if (ret)
			break;
		ret = spi_nor_wait_ready();
		if (ret)
			break;
	}

	/* Disable write */
	(void) qspi_write_disable();

	return ret;
}

int qspi_write(uint32_t offset, const void *buf, size_t len)
{
	uint32_t ifr, iar;
	int ret, ret1;
	uint8_t id;

	ret = spi_nor_read_id(&id);
	if (ret != 0) {
		ERROR("QSPI: read JEDEC failed - %d\n", ret);
		return ret;
	}

	if (id == SST_ID) {
		ret = spi_nor_global_unlock();
		if (ret != 0) {
			ERROR("QSPI: Unlock failed - %d\n", ret);
			return ret;
		}
	}

	/* Erase area */
	ret = qspi_erase(offset, len);
	if (ret == 0) {
		/* Write data */
		ret = qspi_write_data(offset, buf, len);
	}

	/* Reset for memory read support */
	ret1 = lan966x_qspi_set_cfg(&default_read_op, &ifr, &iar);
	if (ret1) {
		ERROR("lan966x_qspi_set_cfg() error: %d", ret1);
		panic();
	}

	/* Write instruction frame */
	qspi_change_ifr(ifr);

	return ret;
}

void qspi_init(uintptr_t base)
{
	int ret;
	uint8_t clk = 0;
	uint32_t ifr, iar;

	reg_base = base;

	/* Already initialized? */
	if (qspi_init_done) {
		VERBOSE("QSPI: Already enabled\n");
		return;		/* Already initialized */
	}

	lan966x_fw_config_read_uint8(LAN966X_FW_CONF_QSPI_CLK, &clk);
	/* Clamp to [5MHz ; 100MHz] */
	clk = MAX(clk, (uint8_t) 5);
	clk = MIN(clk, (uint8_t) 100);
	VERBOSE("QSPI: Using clock %u Mhz\n", clk);
	lan966x_clk_disable(LAN966X_CLK_ID_QSPI0);
	lan966x_clk_set_rate(LAN966X_CLK_ID_QSPI0, clk * 1000 * 1000);
	lan966x_clk_enable(LAN966X_CLK_ID_QSPI0);

	/* Do actual QSPI init */
	ret = qspi_init_controller();
	if (ret != 0) {
		ERROR("QSPI init error: %d", ret);
		panic();
	}

	/* Default op = fast read, with dummy bytes */
	qspi_set_op_data(&default_read_op, SPI_NOR_OP_READ_FAST, SPI_MEM_DATA_IN, NULL, 1);
	default_read_op.dummy.buswidth = 1;
	default_read_op.dummy.nbytes = 1;

	/* Calculate the ifr - instruction frame */
	ret = lan966x_qspi_set_cfg(&default_read_op, &ifr, &iar);
	if (ret) {
		ERROR("lan966x_qspi_set_cfg() error: %d", ret);
		panic();
	}

	/* Write instruction frame */
	qspi_change_ifr(ifr);

	qspi_init_done = true;
}

void qspi_reinit(void)
{
	/* If not enabled - don't do anything */
	if (!qspi_init_done)
		return;

	/* Force initialize again */
	qspi_init_done = false;
	qspi_init(reg_base);
}
