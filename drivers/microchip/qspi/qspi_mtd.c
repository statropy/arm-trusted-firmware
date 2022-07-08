/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <platform_def.h>

#include <common/debug.h>
#include <common/fdt_wrappers.h>
#include <drivers/io/io_driver.h>
#include <drivers/io/io_mtd.h>
#include <drivers/spi_mem.h>
#include <fw_config.h>
#include <lib/utils.h>
#include <lib/libfdt/libfdt.h>
#include <drivers/delay_timer.h>
#include <drivers/microchip/lan966x_clock.h>
#include <lib/mmio.h>

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
#define SPI_NOR_OP_PP		 0x02	 /* Page program (up to 256 bytes) */
#define SPI_NOR_OP_BE_4K	 0x20	 /* Erase 4KiB block */
#define SPI_NOR_OP_BE_4K_PMC	 0xd7	 /* Erase 4KiB block on PMC chips */
#define SPI_NOR_OP_BE_32K	 0x52	 /* Erase 32KiB block */
#define SPI_NOR_OP_CHIP_ERASE	 0xc7	 /* Erase whole flash chip */
#define SPI_NOR_OP_SE		 0xd8	 /* Sector erase (usually 64KiB) */
#define SPI_NOR_OP_WRDI		 0x04	 /* Write disable */

#define DT_QSPI_COMPAT	"microchip,lan966x-qspi"

static struct {
	uintptr_t reg_base;
} qspi_ctl = {
	LAN969X_QSPI_0_BASE,
};

/*
 * Returns 'true' iff timeout occurred, 'false' otherwise.
 */
static bool mchp_qspi_wait_flag(const uintptr_t reg,
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

static bool mchp_qspi_wait_flag_set(const uintptr_t reg,
			       const uint32_t flag,
			       const char *fname)
{
	return mchp_qspi_wait_flag(reg, flag, flag, fname);
}

static bool mchp_qspi_wait_flag_clear(const uintptr_t reg,
				 const uint32_t flag,
				 const char *fname)
{
	return mchp_qspi_wait_flag(reg, flag, 0x0, fname);
}

static int mchp_qspi_do_update(uintptr_t reg_base)
{
	/* Synchronize configuration */
	if (mchp_qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -EIO;

	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_UPDCFG);

	/* Wait end of QSPI sync busy */
	if (mchp_qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -EIO;

	return 0;
}

static int mchp_qspi_init_controller(uintptr_t reg_base)
{
	/* Disable in a failsafe way */
	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_DLLOFF);

#ifdef LAN966X_ASIC
	if (mchp_qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_DLOCK, "SR:DLOCK"))
		return -EIO;
#endif

	/* Set DLLON and STPCAL register */
	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_DLLON | QSPI_CR_STPCAL);

#ifdef LAN966X_ASIC
	if (mchp_qspi_wait_flag_set(reg_base + QSPI_SR, QSPI_SR_DLOCK, "SR:DLOCK"))
		return -EIO;
#endif

	/* Disable QSPI controller */
	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_QSPIDIS);

	/* Synchronize configuration */
	if (mchp_qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -EIO;

	/* Reset the QSPI controller */
	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_SWRST);

	/* Synchronize configuration */
	if (mchp_qspi_wait_flag_clear(reg_base + QSPI_SR, QSPI_SR_SYNCBSY, "SR:SYNCBSY"))
		return -EIO;

	/* Disable write protection */
	mmio_write_32(reg_base + QSPI_WPMR, QSPI_WPMR_WPKEY(QSPI_WPKEY));

	/* Set DLLON and STPCAL register */
	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_DLLON | QSPI_CR_STPCAL);

#if defined(LAN966X_ASIC)
	if (mchp_qspi_wait_flag_set(reg_base + QSPI_SR, QSPI_SR_DLOCK, "SR:DLOCK"))
		return -EIO;
#endif

	/* Set the QSPI controller by default in Serial Memory Mode */
	mmio_write_32(reg_base + QSPI_MR, QSPI_MR_SMM | QSPI_MR_DLYCS(QSPI_DLYCS));

	/* Set DLYBS */
	mmio_write_32(reg_base + QSPI_SCR, QSPI_SCR_DLYBS(QSPI_DLYBS));

	/* Synchronize configuration */
	if (mchp_qspi_do_update(reg_base))
		return -EIO;

	/* Enable the QSPI controller */
	mmio_write_32(reg_base + QSPI_CR, QSPI_CR_QSPIEN);

	/* Wait effective enable */
	if (mchp_qspi_wait_flag_set(reg_base + QSPI_SR, QSPI_SR_QSPIENS, "SR:QSPIENS"))
		return -EIO;

	return 0;
}

static int mchp_qspi_exec_op(const struct spi_mem_op *op)
{
	return -1;
}

static int mchp_qspi_claim_bus(unsigned int cs)
{
	return -1;
}

static void mchp_qspi_release_bus(void)
{
}

static int mchp_qspi_set_speed(unsigned int hz)
{
	return -1;
}

static int mchp_qspi_set_mode(unsigned int mode)
{
	return -1;
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
	return -ENOTSUP;
}

int qspi_init(void)
{
	int qspi_node;
	void *fdt = NULL;

	fdt = lan966x_get_dt();
	if (fdt == NULL) {
		ERROR("qspi: No DT?\n");
		return -FDT_ERR_NOTFOUND;
	}

	qspi_node = fdt_node_offset_by_compatible(fdt, -1, DT_QSPI_COMPAT);
	if (qspi_node < 0) {
		ERROR("No QSPI ctrl found\n");
		return -FDT_ERR_NOTFOUND;
	}

	mchp_qspi_init_controller(qspi_ctl.reg_base);

	return spi_mem_init_slave(fdt, qspi_node, &mchp_qspi_bus_ops);
}

void qspi_reinit(void)
{
}
