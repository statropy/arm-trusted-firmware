/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stddef.h>

#include <ddr_init.h>
#include <ddr_reg.h>
#include <ddr_xlist.h>

#define PGSR_ERR_MASK		GENMASK_32(27, 20)
#define PGSR_ALL_DONE		GENMASK_32(11, 0)

#define TIME_MS_TO_US(ms)	(ms * 1000U)
#define PHY_TIMEOUT_US_1S	TIME_MS_TO_US(1000U)

static const struct {
	uint32_t mask;
	const char *desc;
} phyerr[] = {
	{ PGSR0_ZCERR, "Impedance Calibration" },
	{ PGSR0_WLERR, "Write Leveling" },
	{ PGSR0_QSGERR, "DQS Gate Training" },
	{ PGSR0_WLAERR, "Write Leveling Adjustment" },
	{ PGSR0_RDERR, "Read Bit Deskew" },
	{ PGSR0_WDERR, "Write Bit Deskew" },
	{ PGSR0_REERR, "Read Eye Training" },
	{ PGSR0_WEERR, "Write Eye Training" },
};

struct reg_desc {
	uintptr_t reg_addr;
	uint8_t par_offset;	/* Offset for parameter array */
};

#define X(x, y, z)							\
	{								\
		.reg_addr  = y,						\
		.par_offset = offsetof(struct config_ddr_##z, x),	\
	},

static const struct reg_desc ddr_main_reg[] = {
	XLIST_DDR_MAIN
};

static const struct reg_desc ddr_timing_reg[] = {
	XLIST_DDR_TIMING
};

static const struct reg_desc ddr_mapping_reg[] = {
	XLIST_DDR_MAPPING
};

static const struct reg_desc ddr_phy_reg[] = {
	XLIST_DDR_PHY
};

static const struct reg_desc ddr_phy_timing_reg[] = {
	XLIST_DDR_PHY_TIMING
};

static bool wait_reg_set(uintptr_t reg, uint32_t mask, int usec)
{
	uint64_t t = timeout_init_us(usec);
	while ((mmio_read_32(reg) & mask) == 0) {
		if (timeout_elapsed(t)) {
			NOTICE("Timeout waiting for %p mask %08x set\n", (void*)reg, mask);
			return true;
		}
	}
	return false;
}

static bool wait_reg_clr(uintptr_t reg, uint32_t mask, int usec)
{
	uint64_t t = timeout_init_us(usec);
	while ((mmio_read_32(reg) & mask) != 0) {
		if (timeout_elapsed(t)) {
			NOTICE("Timeout waiting for %p mask %08x clr\n", (void*)reg, mask);
			return true;
		}
	}
	return false;
}

static void wait_operating_mode(uint32_t mode, int usec)
{
	uint64_t t = timeout_init_us(usec);
	while ((FIELD_GET(STAT_OPERATING_MODE,
			  mmio_read_32(DDR_UMCTL2_STAT))) != mode) {
		if (timeout_elapsed(t)) {
			VERBOSE("Timeout waiting for mode %d\n", mode);
			PANIC("wait_operating_mode");
		}
	}
}

static void set_regs(const struct ddr_config *ddr_cfg,
		     const void *cfg,
		     const struct reg_desc *reg,
		     size_t ct)
{
	int i;

	for (i = 0; i < ct; i++) {
		uint32_t val = ((const uint32_t *)cfg)[reg[i].par_offset >> 2];
		mmio_write_32(reg[i].reg_addr, val);
	}
}

static void set_static_ctl(void)
{
	/* Disabling update request initiated by DDR controller during
	 * DDR initialization */
	mmio_setbits_32(DDR_UMCTL2_DFIUPD0,
			DFIUPD0_DIS_AUTO_CTRLUPD_SRX | DFIUPD0_DIS_AUTO_CTRLUPD);
}

static void set_static_phy(const struct ddr_config *cfg)
{
	/* Disabling PHY initiated update request during DDR
	 * initialization */
	mmio_clrbits_32(DDR_PHY_DSGCR, DSGCR_PUREN);
}

static void axi_enable_ports(bool enable)
{
	if (enable) {
		/* Enable controller port(s) */
		mmio_setbits_32(DDR_UMCTL2_PCTRL_0, PCTRL_0_PORT_EN);
		mmio_setbits_32(DDR_UMCTL2_PCTRL_1, PCTRL_1_PORT_EN);
		mmio_setbits_32(DDR_UMCTL2_PCTRL_2, PCTRL_2_PORT_EN);
	} else {
		/* Disable controller port(s) */
		mmio_clrbits_32(DDR_UMCTL2_PCTRL_0, PCTRL_0_PORT_EN);
		mmio_clrbits_32(DDR_UMCTL2_PCTRL_1, PCTRL_1_PORT_EN);
		mmio_clrbits_32(DDR_UMCTL2_PCTRL_2, PCTRL_2_PORT_EN);
	}
}

static void ecc_enable_scrubbing(void)
{
	VERBOSE("Enable ECC scrubbing\n");

	/* 1.  Disable AXI port. port_en = 0 */
	axi_enable_ports(false);

	/* 2. scrub_mode = 1 */
	mmio_setbits_32(DDR_UMCTL2_SBRCTL, SBRCTL_SCRUB_MODE);

	/* 3. scrub_interval = 0 */
	mmio_clrbits_32(DDR_UMCTL2_SBRCTL, SBRCTL_SCRUB_INTERVAL);

	/* 4. Data pattern = 0 */
	mmio_write_32(DDR_UMCTL2_SBRWDATA0, 0);

	/* 5. (skip) */

	/* 6. Enable SBR programming */
	mmio_setbits_32(DDR_UMCTL2_SBRCTL, SBRCTL_SCRUB_EN);

	/* 7. Poll SBRSTAT.scrub_done */
	if (wait_reg_set(DDR_UMCTL2_SBRSTAT, SBRSTAT_SCRUB_DONE, 1000000))
		PANIC("Timeout SBRSTAT.scrub_done set");

	/* 8. Poll SBRSTAT.scrub_busy */
	if (wait_reg_clr(DDR_UMCTL2_SBRSTAT, SBRSTAT_SCRUB_BUSY, 50))
		PANIC("Timeout SBRSTAT.scrub_busy clear");

	/* 9. Disable SBR programming */
	mmio_clrbits_32(DDR_UMCTL2_SBRCTL, SBRCTL_SCRUB_EN);

	/* 10. Normal scrub operation, mode = 0, interval = 100 */
	/* 11. Enable SBR programming again  */
	mmio_clrsetbits_32(DDR_UMCTL2_SBRCTL,
			   SBRCTL_SCRUB_MODE | SBRCTL_SCRUB_INTERVAL,
			   FIELD_PREP(SBRCTL_SCRUB_INTERVAL, 100) |
			   FIELD_PREP(SBRCTL_SCRUB_EN, 1));

	/* 12. Enable AXI port */
	axi_enable_ports(true);

	VERBOSE("Enabled ECC scrubbing\n");
}

static void wait_phy_idone(int tmo)
{
	uint32_t pgsr;
	uint64_t t;
	int i;

	t = timeout_init_us(tmo);

	do {
		pgsr = mmio_read_32(DDR_PHY_PGSR0);

		if (pgsr & PGSR_ERR_MASK) {
			for (i = 0; i < ARRAY_SIZE(phyerr); i++) {
				if (pgsr & phyerr[i].mask) {
					NOTICE("PHYERR: %s Error\n", phyerr[i].desc);
					/* Keep going */
				}
			}
		}

		if (pgsr & PGSR0_IDONE)
			return;

	} while(!timeout_elapsed(t));
	PANIC("PHY IDONE timeout\n");
}

static void ddr_phy_init(uint32_t mode, int usec_timout)
{
	mode |= PIR_INIT;

	VERBOSE("ddr_phy_init:start\n");

	/* Now, kick PHY */
	mmio_write_32(DDR_PHY_PIR, mode);

	VERBOSE("pir = 0x%x -> 0x%x\n", mode, mmio_read_32(DDR_PHY_PIR));

	/* Need to wait 10 configuration clock before start polling */
	ddr_nsleep(10);

	wait_phy_idone(usec_timout);

	VERBOSE("ddr_phy_init:done\n");
}

static void PHY_initialization(void)
{
	/* PHY initialization: PLL initialization, Delay line
	 * calibration, PHY reset and Impedance Calibration
	 */
	ddr_phy_init(PIR_ZCAL | PIR_PLLINIT | PIR_DCAL | PIR_PHYRST, PHY_TIMEOUT_US_1S);
}

static void DRAM_initialization_by_memctrl(void)
{
	/* This does a DRAM BIST */
	ddr_phy_init(PIR_DRAMRST | PIR_DRAMINIT, PHY_TIMEOUT_US_1S);
}

static void sw_done_start(void)
{
	/* Enter quasi-dynamic mode */
	mmio_write_32(DDR_UMCTL2_SWCTL, 0);
}

static void sw_done_ack(void)
{
	VERBOSE("sw_done_ack:enter\n");

	/* Signal we're done */
	mmio_write_32(DDR_UMCTL2_SWCTL, SWCTL_SW_DONE);

	/* wait for SWSTAT.sw_done_ack to become set */
	if (wait_reg_set(DDR_UMCTL2_SWSTAT, SWSTAT_SW_DONE_ACK, 50))
		PANIC("Timout SWSTAT.sw_done_ack set");

	VERBOSE("sw_done_ack:exit\n");
}

static void ddr_disable_refresh(void)
{
	sw_done_start();
	mmio_setbits_32(DDR_UMCTL2_RFSHCTL3, RFSHCTL3_DIS_AUTO_REFRESH);
	mmio_clrbits_32(DDR_UMCTL2_PWRCTL, PWRCTL_POWERDOWN_EN);
	mmio_clrbits_32(DDR_UMCTL2_DFIMISC, DFIMISC_DFI_INIT_COMPLETE_EN);
	sw_done_ack();
}

static void ddr_restore_refresh(uint32_t rfshctl3, uint32_t pwrctl)
{
	sw_done_start();
	if ((rfshctl3 & RFSHCTL3_DIS_AUTO_REFRESH) == 0)
		mmio_clrbits_32(DDR_UMCTL2_RFSHCTL3, RFSHCTL3_DIS_AUTO_REFRESH);
	if (pwrctl & PWRCTL_POWERDOWN_EN)
		mmio_setbits_32(DDR_UMCTL2_PWRCTL, PWRCTL_POWERDOWN_EN);
	mmio_setbits_32(DDR_UMCTL2_DFIMISC, DFIMISC_DFI_INIT_COMPLETE_EN);
	sw_done_ack();
}

static void do_data_training(const struct ddr_config *cfg)
{
	uint32_t w;

	VERBOSE("do_data_training:enter\n");

	/* Disable Auto refresh and power down before training */
	ddr_disable_refresh();

	/* Write leveling, DQS gate training, Write_latency adjustment must be executed in
	 * asynchronous read mode. After these training are finished, then static read mode
	 * can be set and rest of the training can be executed with a second PIR register.
	 */

	/* write PHY initialization register for Write leveling, DQS
	 * gate training, Write_latency adjustment
	 */
	ddr_phy_init(PIR_WL | PIR_QSGATE | PIR_WLADJ, PHY_TIMEOUT_US_1S);

	/* Now, actual data training */
	w = PIR_WREYE | PIR_RDEYE | PIR_WRDSKW | PIR_RDDSKW;
	ddr_phy_init(w, PHY_TIMEOUT_US_1S);

	w = mmio_read_32(DDR_PHY_PGSR0);
	if ((w & PGSR_ALL_DONE) != PGSR_ALL_DONE) {
		ERROR("pgsr0: got %08x, want %08x\n", w, PGSR_ALL_DONE);
		PANIC("data training error");
	}

	ddr_restore_refresh(cfg->main.rfshctl3, cfg->main.pwrctl);

	/* Reenabling uMCTL2 initiated update request after executing
	 * DDR initization. Reference: DDR4 MultiPHY PUB databook
	 * (3.11a) PIR.INIT description (pg no. 114)
	 */
	sw_done_start();
	mmio_clrbits_32(DDR_UMCTL2_DFIUPD0, DFIUPD0_DIS_AUTO_CTRLUPD);
	sw_done_ack();

	/* Reenabling PHY update Request after executing DDR
	 * initization. Reference: DDR4 MultiPHY PUB databook (3.11a)
	 * PIR.INIT description (pg no. 114)
	 */
	mmio_setbits_32(DDR_PHY_DSGCR, DSGCR_PUREN);

	/* Enable AXI port(s) */
	axi_enable_ports(true);

	/* Settle */
	ddr_usleep(1);

	VERBOSE("do_data_training:exit\n");
}

int ddr_init(const struct ddr_config *cfg)
{
	VERBOSE("ddr_init:start\n");

	VERBOSE("name = %s\n", cfg->info.name);
	VERBOSE("speed = %d kHz\n", cfg->info.speed);
	VERBOSE("size  = %zdM\n", cfg->info.size / 1024 / 1024);

	/* Reset, start clocks at desired speed */
	ddr_reset(cfg, true);

	/* Set up controller registers */
	set_regs(cfg, &cfg->main, ddr_main_reg, ARRAY_SIZE(ddr_main_reg));
	set_regs(cfg, &cfg->timing, ddr_timing_reg, ARRAY_SIZE(ddr_timing_reg));
	set_regs(cfg, &cfg->mapping, ddr_mapping_reg, ARRAY_SIZE(ddr_mapping_reg));

	/* Static controller settings */
	set_static_ctl();

	/* Release reset */
	ddr_reset(cfg, false);

	/* Set PHY registers */
	set_regs(cfg, &cfg->phy, ddr_phy_reg, ARRAY_SIZE(ddr_phy_reg));
	set_regs(cfg, &cfg->phy_timing, ddr_phy_timing_reg, ARRAY_SIZE(ddr_phy_timing_reg));

	/* Static PHY settings */
	set_static_phy(cfg);

	PHY_initialization();

	DRAM_initialization_by_memctrl();

	/* Start quasi-dynamic programming */
	sw_done_start();

	/* PHY initialization complete enable: trigger SDRAM initialisation */
	mmio_setbits_32(DDR_UMCTL2_DFIMISC, DFIMISC_DFI_INIT_COMPLETE_EN);

	/* Signal quasi-dynamic phase end, await ack */
	sw_done_ack();

	/* wait 2ms for STAT.operating_mode to become "normal" */
	wait_operating_mode(1, TIME_MS_TO_US(2U));

	do_data_training(cfg);

	if (cfg->main.ecccfg0 & ECCCFG0_ECC_MODE)
		ecc_enable_scrubbing();

	VERBOSE("ddr_init:done\n");

	return 0;
}

void ddr_reset(const struct ddr_config *cfg , bool assert)
{
	VERBOSE("Reset: %sassert\n", assert ? "" : "de-");
	if (assert) {
		/* Assert resets */
		mmio_setbits_32(CPU_DDRCTRL_RST,
				DDRCTRL_RST_DDRC_RST |
				DDRCTRL_RST_DDR_AXI0_RST |
				DDRCTRL_RST_DDR_AXI1_RST |
				DDRCTRL_RST_DDR_AXI2_RST |
				DDRCTRL_RST_DDR_APB_RST |
				DDRCTRL_RST_DDRPHY_CTL_RST |
				DDRCTRL_RST_DDRPHY_APB_RST);

		/* Start the clocks */
		mmio_setbits_32(CPU_DDRCTRL_CLK,
				DDRCTRL_CLK_DDR_CLK_ENA |
				DDRCTRL_CLK_DDR_AXI0_CLK_ENA |
				DDRCTRL_CLK_DDR_AXI1_CLK_ENA |
				DDRCTRL_CLK_DDR_AXI2_CLK_ENA |
				DDRCTRL_CLK_DDR_APB_CLK_ENA |
				DDRCTRL_CLK_DDRPHY_CTL_CLK_ENA |
				DDRCTRL_CLK_DDRPHY_APB_CLK_ENA);

		/* Allow clocks to settle */
		ddr_nsleep(100);

		/* Deassert presetn once the clocks are active and stable */
		mmio_clrbits_32(CPU_DDRCTRL_RST, DDRCTRL_RST_DDR_APB_RST);

		ddr_nsleep(50);
	} else {
		ddr_nsleep(200);

		/* Deassert the core_ddrc_rstn reset */
		mmio_clrbits_32(CPU_RESET, RESET_MEM_RST);

		/* Deassert DDRC and AXI RST */
		mmio_clrbits_32(CPU_DDRCTRL_RST,
				DDRCTRL_RST_DDRC_RST |
				DDRCTRL_RST_DDR_AXI0_RST |
				DDRCTRL_RST_DDR_AXI1_RST |
				DDRCTRL_RST_DDR_AXI2_RST);

		/* Settle */
		ddr_nsleep(100);

		/* Deassert DDRPHY_APB_RST and DRPHY_CTL_RST */
		mmio_clrbits_32(CPU_DDRCTRL_RST,
				DDRCTRL_RST_DDRPHY_APB_RST | DDRCTRL_RST_DDRPHY_CTL_RST);

		ddr_nsleep(100);
	}
	VERBOSE("Reset: %sasserted\n", assert ? "" : "de-");
}

void lan966x_ddr_init(void)
{
	extern const struct ddr_config lan966x_ddr_config;
	const struct ddr_config *cfg =	&lan966x_ddr_config;

	ddr_init(cfg);
}
