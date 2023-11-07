/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <common/debug.h>
#include <common/fdt_wrappers.h>
#include <drivers/delay_timer.h>
#include <drivers/io/io_driver.h>
#include <drivers/io/io_mtd.h>
#include <drivers/microchip/lan966x_clock.h>
#include <drivers/microchip/qspi.h>
#include <drivers/microchip/xdmac.h>
#include <drivers/spi_mem.h>
#include <drivers/spi_nor.h>
#include <plat/microchip/common/duff_memcpy.h>
#include <fw_config.h>
#include <lib/libfdt/libfdt.h>
#include <lib/mmio.h>
#include <lib/utils.h>
#include <platform_def.h>

#define MHZ	1000000U
#define MHZ_NS	(1000U * MHZ)

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
#define QSPI_WPKEY			 0x515350
#define QSPI_TIMEOUT			 0x1000U

/* Non-std Command codes */
#define SPI_NOR_OP_BE_4K_PMC	 0xd7	 /* Erase 4KiB block on PMC chips */
#define SPI_NOR_OP_BE_32K	 0x52	 /* Erase 32KiB block */
#define SPI_NOR_OP_CHIP_ERASE	 0xc7	 /* Erase whole flash chip */
#define SPI_NOR_OP_SE		 0xd8	 /* Sector erase (usually 64KiB) */

#define DT_QSPI_COMPAT	"microchip,lan966x-qspi"

static uintptr_t reg_base = LAN969X_QSPI_0_BASE;

static unsigned int qspi_mode;
static unsigned int qspi_dlycs;

/* Assumed worst case for 'slow' speed on ST flash */
#define MIN_DLYCS_NS	25

#pragma weak plat_qspi_init_clock
void plat_qspi_init_clock(void)
{
}

#pragma weak plat_qspi_default_clock_mhz
uint32_t plat_qspi_default_clock_mhz(void)
{
	return QSPI_DEFAULT_SPEED_MHZ;
}

#pragma weak plat_qspi_default_mode
int plat_qspi_default_mode(void)
{
	return 0;		/* Single SPI mode */
}

static void mchp_qspi_write(const unsigned int reg,
			    const uint32_t value)
{
	VERBOSE("qspi: WRITE(%08lx, %08x)\n", reg_base + reg, value);
	mmio_write_32(reg_base + reg, value);
}

static uint32_t mchp_qspi_read(const unsigned int reg)
{
	uint32_t value = mmio_read_32(reg_base + reg);
	VERBOSE("qspi: READ(%08lx) = %08x)\n", reg_base + reg, value);
	return value;
}

/*
 * Returns 'true' iff timeout occurred, 'false' otherwise.
 */
static bool mchp_qspi_wait_flag(const unsigned int reg,
			   const uint32_t flag,
			   const uint32_t value,
			   const char *fname)
{
	uint64_t timeout = timeout_init_us(QSPI_TIMEOUT);
	uint32_t w;

	do {
		w = mchp_qspi_read(reg);
		if ((w & flag) == value)
			return false;
	} while (!timeout_elapsed(timeout));

	ERROR("QSPI: Timeout waiting for %s %s (%08x, %08x)\n", fname,
	      value ? "set" : "clear", w, flag);

	return true;
}

static bool mchp_qspi_wait_flag_set(const unsigned int reg,
				    const uint32_t flag,
				    const char *fname)
{
	return mchp_qspi_wait_flag(reg, flag, flag, fname);
}

static bool mchp_qspi_wait_flag_clear(const unsigned int reg,
				      const uint32_t flag,
				      const char *fname)
{
	return mchp_qspi_wait_flag(reg, flag, 0x0, fname);
}

static int mchp_qspi_do_update(void)
{
	/* Synchronize configuration */
	if (mchp_qspi_wait_flag_clear(QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -EIO;

	mchp_qspi_write(QSPI_CR, QSPI_CR_UPDCFG);

	/* Wait end of QSPI sync busy */
	if (mchp_qspi_wait_flag_clear(QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -EIO;

	return 0;
}

static int mchp_qspi_change_ifr(uint32_t ifr)
{
	VERBOSE("qspi: set IFR = %0x\n", ifr);

	/* Write instruction frame */
	mchp_qspi_write(QSPI_IFR, ifr);

	/* Perform dummy read */
	(void) mchp_qspi_read(QSPI_SR);

	/* Wait for QSPI_SR_RBUSY */
	if (mchp_qspi_wait_flag_clear(QSPI_SR, QSPI_SR_RBUSY, "SR:RBUSY"))
		return -ETIMEDOUT;

	/* Wait for SR.RBUSY == 0 */
	if (mchp_qspi_wait_flag_clear(QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -ETIMEDOUT;

	mchp_qspi_write(QSPI_CR, QSPI_CR_LASTXFER);

	/* wait for SR.HIDLE == 1 */
	if (mchp_qspi_wait_flag_set(QSPI_SR, QSPI_SR_HIDLE, "SR:HIDLE"))
		return -ETIMEDOUT;

	if (mchp_qspi_do_update())
		return -ETIMEDOUT;

	return 0;
}

static int mchp_qspi_init_controller(void)
{
	unsigned long clk;

	/* Init clock */
	plat_qspi_init_clock();

	/* Set DLYCS */
	clk = lan966x_clk_get_rate(LAN966X_CLK_ID_QSPI0);
	INFO("QSPI0 running at %lu Mhz\n", clk / MHZ);
	/* Calc minimum DLYCS - in clocks */
	qspi_dlycs = DIV_ROUND_UP_2EVAL(MIN_DLYCS_NS, (MHZ_NS / clk));

	/* Disable in a failsafe way */
	mchp_qspi_write(QSPI_CR, QSPI_CR_DLLOFF);

#ifdef LAN966X_ASIC
	if (mchp_qspi_wait_flag_clear(QSPI_SR, QSPI_SR_DLOCK, "SR:DLOCK"))
		return -EIO;
#endif

	/* Set DLLON and STPCAL register */
	mchp_qspi_write(QSPI_CR, QSPI_CR_DLLON | QSPI_CR_STPCAL);

#ifdef LAN966X_ASIC
	if (mchp_qspi_wait_flag_set(QSPI_SR, QSPI_SR_DLOCK, "SR:DLOCK"))
		return -EIO;
#endif

	/* Disable QSPI controller */
	mchp_qspi_write(QSPI_CR, QSPI_CR_QSPIDIS);

	/* Synchronize configuration */
	if (mchp_qspi_wait_flag_clear(QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -EIO;

	/* Reset the QSPI controller */
	mchp_qspi_write(QSPI_CR, QSPI_CR_SWRST);

	/* Synchronize configuration */
	if (mchp_qspi_wait_flag_clear(QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -EIO;

	/* Disable write protection */
	mchp_qspi_write(QSPI_WPMR, QSPI_WPMR_WPKEY(QSPI_WPKEY));

	/* Set DLLON and STPCAL register */
	mchp_qspi_write(QSPI_CR, QSPI_CR_DLLON | QSPI_CR_STPCAL);

#if defined(LAN966X_ASIC)
	if (mchp_qspi_wait_flag_set(QSPI_SR, QSPI_SR_DLOCK, "SR:DLOCK"))
		return -EIO;
#endif

	/* Set the QSPI controller by default in Serial Memory Mode */
	mchp_qspi_write(QSPI_MR, QSPI_MR_SMM | QSPI_MR_DLYCS(qspi_dlycs));

	/* Set DLYBS */
	mchp_qspi_write(QSPI_SCR, QSPI_SCR_DLYBS(QSPI_DLYBS));

	/* Synchronize configuration */
	if (mchp_qspi_do_update())
		return -EIO;

	/* Enable the QSPI controller */
	mchp_qspi_write(QSPI_CR, QSPI_CR_QSPIEN);

	/* Wait effective enable */
	if (mchp_qspi_wait_flag_set(QSPI_SR, QSPI_SR_QSPIENS, "SR:QSPIENS"))
		return -EIO;

	return 0;
}

static const struct mchp_qspi_mode {
	uint8_t cmd_buswidth;
	uint8_t addr_buswidth;
	uint8_t data_buswidth;
	uint32_t config;
} mchp_qspi_modes[] = {
	{ 1, 1, 1, QSPI_IFR_WIDTH_SINGLE_BIT_SPI },
	{ 1, 1, 2, QSPI_IFR_WIDTH_DUAL_OUTPUT },
	{ 1, 1, 4, QSPI_IFR_WIDTH_QUAD_OUTPUT },
	{ 1, 2, 2, QSPI_IFR_WIDTH_DUAL_IO },
	{ 1, 4, 4, QSPI_IFR_WIDTH_QUAD_IO },
	{ 2, 2, 2, QSPI_IFR_WIDTH_DUAL_CMD },
	{ 4, 4, 4, QSPI_IFR_WIDTH_QUAD_CMD },
};

static bool mchp_qspi_is_compatible(const struct spi_mem_op *op,
				    const struct mchp_qspi_mode *mode)
{
	if (op->cmd.buswidth != mode->cmd_buswidth)
		return false;

	if (op->addr.nbytes && op->addr.buswidth != mode->addr_buswidth)
		return false;

	if (op->data.nbytes && op->data.buswidth != mode->data_buswidth)
		return false;

	return true;
}

static int mchp_qspi_find_mode(const struct spi_mem_op *op)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mchp_qspi_modes); i++)
		if (mchp_qspi_is_compatible(op, &mchp_qspi_modes[i]))
			return i;

	return -ENOTSUP;
}

static bool mchp_qspi_op_is_smr(const struct spi_mem_op *op)
{
	if (op->cmd.opcode == SPI_NOR_OP_READ_FAST ||
	    op->cmd.opcode == SPI_NOR_OP_READ_1_1_4 ||
	    op->cmd.opcode == SPI_NOR_OP_READ_1_4_4 ||
	    op->cmd.opcode == SPI_NOR_OP_PP)
		return false;
	return true;
}

static int mchp_qspi_set_cfg(const struct spi_mem_op *op,
			     uint32_t *offset,
			     uint32_t *ifr,
			     uint32_t *data_nbytes)
{
	uint32_t dummy_cycles = 0;
	int mode;

	mode = mchp_qspi_find_mode(op);
	if (mode < 0)
		return mode;
	*ifr = mchp_qspi_modes[mode].config;
	*offset = 0;

	if (mchp_qspi_op_is_smr(op)) {
		if (op->data.dir == SPI_MEM_DATA_IN)
			*ifr |= QSPI_IFR_SMRM | QSPI_IFR_APBTFRTYP;
		else
			*ifr |= QSPI_IFR_SMRM;
	} else {
		*ifr |= QSPI_IFR_TFRTYP;
	}

	/* Write op command */
	if (op->data.dir == SPI_MEM_DATA_IN) {
		mchp_qspi_write(QSPI_RICR, QSPI_RICR_RDINST(op->cmd.opcode));
		mchp_qspi_write(QSPI_WICR, 0);
	} else if (op->data.dir == SPI_MEM_DATA_OUT) {
		mchp_qspi_write(QSPI_WICR, QSPI_WICR_WRINST(op->cmd.opcode));
		mchp_qspi_write(QSPI_RICR, 0);
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

		*data_nbytes = 0;
		*offset = iar;
	}

	/* Set number of dummy cycles */
	if (dummy_cycles)
		*ifr |= QSPI_IFR_NBDUM(dummy_cycles);

	/* Set data enable */
	if (*data_nbytes)
		*ifr |= QSPI_IFR_DATAEN;

	/*
	 * Set QSPI controller in Serial Memory Mode (SMM).
	 */
	mchp_qspi_write(QSPI_MR, QSPI_MR_SMM | QSPI_MR_DLYCS(qspi_dlycs));

	return 0;
}

static int mchp_qspi_reg_acc(const struct spi_mem_op *op,
			     uint32_t offset,
			     uint32_t ifr,
			     uint32_t data_nbytes)
{
	VERBOSE("qspi: reg %s, cmd %02x, ifr %08x, offset %08x, len %d\n",
		op->data.dir == SPI_MEM_DATA_IN ? "in" : "out", op->cmd.opcode, ifr, offset, op->data.nbytes);

	mchp_qspi_write(QSPI_IAR, offset);

	/* Write instruction frame - if changed */
	mchp_qspi_change_ifr(ifr);

	/* Skip to the final step if there is no data */
	if (data_nbytes == 0) {
		mchp_qspi_write(QSPI_CR, QSPI_CR_STTFR);
		goto no_data;
	}

	/* if read reg operation */
	if (ifr & QSPI_IFR_APBTFRTYP) {
		uint32_t i = 0;
		uint32_t remaining = op->data.nbytes;
		uint8_t *rx = op->data.buf;

		/* always wait for SR.SYNCBSY == 0 */
		if (mchp_qspi_wait_flag_clear(QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
			return -1;

		/* start transfer */
		mchp_qspi_write(QSPI_CR, QSPI_CR_STTFR);

		/* read buffer_len - 1 data */
		while (remaining > 1) {
			/* wait for ISFR.RDRF */
			if (mchp_qspi_wait_flag_set(QSPI_ISR, QSPI_ISR_RDRF, "ISR:RDRF"))
				return -1;

			/* wait end of QSPI sync busy */
			if (mchp_qspi_wait_flag_clear(QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
				return -1;

			rx[i] = mchp_qspi_read(QSPI_RDR);

			remaining--;
			i++;
		}

		/* Wait for ISFR.RDRF */
		if (mchp_qspi_wait_flag_set(QSPI_ISR, QSPI_ISR_RDRF, "ISR:RDRF"))
			return -1;

		/* wait end of QSPI sync busy */
		if (mchp_qspi_wait_flag_clear(QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
			return -1;

		/* chip select release */
		mchp_qspi_write(QSPI_CR, QSPI_CR_LASTXFER);

		if (mchp_qspi_wait_flag_clear(QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
			return -1;

		rx[i] = mchp_qspi_read(QSPI_RDR);
	} else {
		uint32_t i = 0;
		const uint8_t *tx = op->data.buf;

		for (i = 0; i < op->data.nbytes; ++i) {
			/* wait for ISR.TDRE */
			if (mchp_qspi_wait_flag_set(QSPI_ISR, QSPI_ISR_TDRE, "ISR:TDRE"))
				return -1;

			/* write data */
			mchp_qspi_write(QSPI_TDR, tx[i]);
		}

		/* wait for ISR.TXEMPTY */
		if (mchp_qspi_wait_flag_set(QSPI_ISR, QSPI_ISR_TXEMPTY, "ISR:TXEMPTY"))
			return -1;

		/* wait end of QSPI sync busy */
		if (mchp_qspi_wait_flag_clear(QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
			return -1;

		mchp_qspi_write(QSPI_CR, QSPI_CR_LASTXFER);
	}

no_data:
	if (mchp_qspi_wait_flag_set(QSPI_ISR, QSPI_ISR_CSRA, "ISR:CSRA"))
		return -1;

	VERBOSE("qspi: reg success\n");

	return 0;
}

static int mchp_qspi_mem(const struct spi_mem_op *op,
			 uint32_t offset,
			 uint32_t ifr)
{
	VERBOSE("qspi: mem %s, cmd %02x, ifr %08x, off %08x, len %d\n",
		op->data.dir == SPI_MEM_DATA_IN ? "in" : "out",
		op->cmd.opcode, ifr, offset, op->data.nbytes);

        if (op->data.dir == SPI_MEM_DATA_OUT) {
		mchp_qspi_write(QSPI_WRACNT, op->data.nbytes);
        }

	/* Write instruction frame - if changed */
	mchp_qspi_change_ifr(ifr);

	/* Move the data */
	if (op->data.dir == SPI_MEM_DATA_IN) {
		xdmac_memcpy(op->data.buf, (void *) (LAN969X_QSPI0_MMAP + offset), op->data.nbytes,
			     XDMA_TO_MEM, XDMA_QSPI0_RX);

		if (mchp_qspi_wait_flag_clear(QSPI_SR, QSPI_SR_RBUSY, "SR:RBUSY"))
			return -ETIMEDOUT;
	} else {
		xdmac_memcpy((void *) (LAN969X_QSPI0_MMAP + offset), op->data.buf, op->data.nbytes,
			    XDMA_FROM_MEM, XDMA_QSPI0_TX);

		/* Wait 'Last Write Access' */
		if (mchp_qspi_wait_flag_set(QSPI_ISR, QSPI_ISR_LWRA, "ISR:LWRA"))
			return -ETIMEDOUT;
	}

	/* Wait sync to clear */
	if (mchp_qspi_wait_flag_clear(QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -ETIMEDOUT;

	/* Release the chip-select */
	mchp_qspi_write(QSPI_CR, QSPI_CR_LASTXFER);

	/* Poll Instruction End and Chip Select Rise flags. */
	if (mchp_qspi_wait_flag_set(QSPI_ISR, QSPI_ISR_CSRA, "ISR:CSRA"))
		return -ETIMEDOUT;

	VERBOSE("qspi: mem success\n");

	return 0;
}

static int mchp_qspi_exec_op(const struct spi_mem_op *op)
{
	uint32_t ifr, offset;
	uint32_t data_nbytes = op->data.nbytes;
	int err;

	offset = ifr = 0;
	err = mchp_qspi_set_cfg(op, &offset, &ifr, &data_nbytes);
	if (err) {
		NOTICE("qspi: cmd %02x cfg fails: %d\n", op->cmd.opcode, err);
		return err;
	}

	if (ifr & QSPI_IFR_SMRM)
		return mchp_qspi_reg_acc(op, offset, ifr, data_nbytes);

	return mchp_qspi_mem(op, offset, ifr);
}

static int mchp_qspi_claim_bus(unsigned int cs)
{
	return 0;
}

static void mchp_qspi_release_bus(void)
{
}

static int mchp_qspi_set_speed(unsigned int hz)
{
	return 0;
}

static int mchp_qspi_set_mode(unsigned int mode)
{
	uint32_t scr, mask, new_value = 0;

	/* Save the new mode */
	qspi_mode = mode;

	if (mode & SPI_CPOL)
		new_value = QSPI_SCR_CPOL;
	if (mode & SPI_CPHA)
		new_value = QSPI_SCR_CPHA;

	mask = QSPI_SCR_CPOL | QSPI_SCR_CPHA;

	scr = mchp_qspi_read(QSPI_SCR);
	if ((scr & mask) == new_value)
		return 0;

	scr = (scr & ~mask) | new_value;
	mchp_qspi_write(QSPI_SCR, scr);

	return mchp_qspi_do_update();
}

static const struct spi_bus_ops mchp_qspi_bus_ops = {
	.claim_bus = mchp_qspi_claim_bus,
	.release_bus = mchp_qspi_release_bus,
	.set_speed = mchp_qspi_set_speed,
	.set_mode = mchp_qspi_set_mode,
	.exec_op = mchp_qspi_exec_op,
};

int qspi_write(uint32_t offset, const void *buf, size_t len)
{
	int ret;

	/* Erase area */
	ret = spi_nor_erase(offset, len);
	if (ret == 0) {
		/* Write data */
		ret = spi_nor_write(offset, (uintptr_t) buf, len);
	}

	return ret;
}

int qspi_read(unsigned int offset, uintptr_t buffer, size_t length,
	      size_t *length_read)
{
	return spi_nor_read(offset, buffer, length, length_read);
}

int qspi_init(void)
{
	int qspi_node;
	void *fdt;

	/* Init HW */
	mchp_qspi_init_controller();

	/* Have DT? */
	fdt = lan966x_get_dt();
	if (fdt == NULL || fdt_check_header(fdt) != 0)
		goto default_spi_mem;

	qspi_node = fdt_node_offset_by_compatible(fdt, -1, DT_QSPI_COMPAT);
	if (qspi_node < 0) {
		/* Instantiate default QSPI */
		ERROR("No QSPI in DT, using default values\n");
		goto default_spi_mem;
	}

	return spi_mem_init_slave(fdt, qspi_node, &mchp_qspi_bus_ops);

default_spi_mem:
	/* Instantiate default QSPI */
	return spi_mem_init_slave_default(&mchp_qspi_bus_ops,
					  plat_qspi_default_mode(),
					  plat_qspi_default_clock_mhz() * MHZ);
}

unsigned int qspi_get_spi_mode(void)
{
	return qspi_mode;
}
