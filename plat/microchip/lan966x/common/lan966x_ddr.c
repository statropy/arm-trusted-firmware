/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <lib/mmio.h>
#include <platform_def.h>
#include <stdint.h>

#include "lan966x_targets.h"
#include "lan966x_private.h"

#if defined(EVB_9662)
// Already defined: #define LAN966X_ASIC
#define CONFIG_DDR_MASERATI_EVB
#include "lan966x_baremetal_cpu_regs.h"
#include "ddr.h"
#else
#define EXB_FPGA
#define INIT_MODE 1
#include "lan966x_baremetal_cpu_regs_sr.h"
#include "ddr_timing_parameters.h"
#endif

#define debug(x) VERBOSE(x)
#define error(x) ERROR(x)

#define _base_TARGET_DDR_PHY		LAN966X_DDR_PHY_BASE
#define _base_TARGET_DDR_UMCTL2		LAN966X_DDR_UMCTL2_BASE
#define _base_TARGET_CPU		LAN966X_CPU_BASE
#define _base_TARGET_CHIP_TOP		LAN966X_CHIP_TOP_BASE

static inline uint32_t reg_read(uintptr_t addr)
{
	uint32_t val = mmio_read_32(addr);
	return val;
}

static inline void reg_write(uint32_t val, uintptr_t addr)
{
	mmio_write_32(addr, val);
}

#undef __REG
#define __REG(id, tinst, tcnt,                        \
	      gbase, ginst, gcnt, gwidth,             \
	      raddr, rinst, rcnt, rwidth)	      \
	(_base_##id + gbase + ((ginst) * gwidth) + raddr + ((rinst) * rwidth))

// read register
#define rd_reg(tgt,grp,reg) \
	reg_read(tgt##_##reg)

// write register
#define wr_reg(tgt,grp,reg,val) \
	reg_write(val,tgt##_##reg)

// write field, source shadow, destination shadow
#define wr_fld_s_s(shadow_val,tgt,grp,reg,fld,fld_val) \
    ((shadow_val & (~tgt##_##reg##_##fld##_M)) | tgt##_##reg##_##fld(fld_val) )

// write field, source reg, destination shadow
#define wr_fld_r_s(tgt,grp,reg,fld,fld_val) \
    wr_fld_s_s(rd_reg(tgt,grp,reg),tgt,grp,reg,fld,fld_val)

// write field, source shadow, destination reg
#define wr_fld_s_r(shadow_val,tgt,grp,reg,fld,fld_val) \
    wr_reg(tgt,grp,reg,wr_fld_s_s(shadow_val,tgt,grp,reg,fld,fld_val))

// write field, source reg, destination reg
#define wr_fld_r_r(tgt,grp,reg,fld,fld_val) \
    wr_fld_s_r(rd_reg(tgt,grp,reg),tgt,grp,reg,fld,fld_val)

// read field, source shadow
#define rd_fld_s(shadow_val,tgt,grp,reg,fld) \
    tgt##_##reg##_##fld##_X(shadow_val)

// read field, source register
#define rd_fld_r(tgt,grp,reg,fld) \
    rd_fld_s(rd_reg(tgt,grp,reg),tgt,grp,reg,fld)

#define REG_WR(reg, val) reg_write(val, reg)

static inline uint32_t readl(uintptr_t addr)
{
	return mmio_read_32(addr);
}

static inline void writel(uint32_t val, uintptr_t addr)
{
	mmio_write_32(addr, val);
}

static inline void setbits_le32(uintptr_t addr, uint32_t val)
{
	mmio_setbits_32(addr, val);
}

static inline void clrbits_le32(uintptr_t addr, uint32_t val)
{
	mmio_clrbits_32(addr, val);
}

#ifdef LAN966X_ASIC

#define params_clk_div   CONFIG_DDR_MASERATI_CLK_DIV
#define CEIL_VAL(x)  ((uint32_t)((x)+0.999999))

#define DW_APB_LOAD_VAL		(LAN966X_TIMERS_BASE + (0*4))
#define DW_APB_CURR_VAL		(LAN966X_TIMERS_BASE + (1*4))
#define DW_APB_CTRL		(LAN966X_TIMERS_BASE + (2*4))
#define DW_APB_EOI		(LAN966X_TIMERS_BASE + (3*4))
#define DW_APB_INTSTAT		(LAN966X_TIMERS_BASE + (4*4))

#define CLOC_SPEED	(250 * 1000000) /* 250Mhz */
#define NS_PER_TICK	(1000000000/CLOC_SPEED) /* ns = (10 ** 9), ns/tick = 4 */
#define usec2ns(x)	(x * 1000)

#define params_tck_min		16666

#define WAIT_START	"start"
#define WAIT_FINISH	"finish"
#define TIMEOUT		"timeout"
#define ERROR_MSG	"error"
#define SUCCESS		"sucess"

inline static void pause(uint32_t val)
{
	register int i = 0;

	for (i = 0; i < val; ++i)
	    asm volatile("nop;");
}

static inline void my_print(const char *what, const uint16_t msg)
{
	VERBOSE("%s %d\n", what, msg);
}

// Function to implement wait
static inline void my_wait(uint32_t t_nsec, const uint16_t msg)
{
	my_print(WAIT_START, msg);
	/* init timer */
	t_nsec /= NS_PER_TICK;
	writel(t_nsec, DW_APB_LOAD_VAL);
	writel(0xffffffff, DW_APB_CURR_VAL);
	setbits_le32(DW_APB_CTRL, 0x3);
	while (readl(DW_APB_INTSTAT) == 0)
		;
	clrbits_le32(DW_APB_CTRL, 0x3);
	(void) readl(DW_APB_EOI);
	my_print(WAIT_FINISH, msg);
}

// Function to poll for Init done
inline static uint32_t poll_idone(uint16_t msg_id) { // default 1ms
	uint32_t timeout_in = 1000;
	uint32_t timeout;
	uint32_t stat;

  // polling for fld value becomes 1 for 1ms
  // rd fld of PGSR0.IDONE
  timeout = timeout_in;
  do {
    my_wait(1000, msg_id); // 1us
    stat = rd_fld_r(DDR_PHY,DDR3_2_PHY_PUB,PGSR0,IDONE);
    timeout--;
  } while (stat != 1 && timeout != 0);

  return timeout;
}


// Function to poll for sw done ack
inline static uint32_t poll_sw_ack(uint16_t msg_id) { // default 1ms
	uint32_t timeout_in = 1000;
	uint32_t timeout;
	uint32_t stat;

  // polling for fld value becomes 1 for 1ms
  // rd fld of SWSTAT.SW_DONE_ACK
  timeout = timeout_in;
  do {
    my_wait(1000, msg_id);
    stat = rd_fld_r(DDR_UMCTL2,UMCTL2_REGS,SWSTAT,SW_DONE_ACK);
    timeout--;
  } while (stat != 1 && timeout != 0);

  return timeout;
}

#define _DDR_DUMMY 0
uint8_t DDR_initialization(void) {
uint32_t var;
uint32_t stat;
uint32_t timeout;

  // Check if already booting from DDR therefore DDR is initialized
  // if (rd_fld_r(CPU, CPU_REGS, GENERAL_CTRL, BOOT_MODE_ENA) == 0) {
  //   return 1;
  // }

  if (_DDR_DUMMY) {
    return 1;
  } else {

    // clock, reset enables
    // wr fld of DDRCTRL_CLK.DDR_CLK_ENA
    var = wr_fld_r_s(    CPU,DDRCTRL,DDRCTRL_CLK,DDR_CLK_ENA,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_CLK,DDR_APB_CLK_ENA,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_CLK,DDRPHY_CTL_CLK_ENA,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_CLK,DDRPHY_APB_CLK_ENA,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_CLK,DDR_AXI0_CLK_ENA,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_CLK,DDR_AXI1_CLK_ENA,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_CLK,DDR_AXI2_CLK_ENA,0x1);
    wr_fld_s_r(var,CPU,DDRCTRL,DDRCTRL_CLK,DDR_AXI2_CLK_ENA,0x1);

    // wr fld of DDRCTRL_RST.DDRC_RST
    var = wr_fld_r_s(    CPU,DDRCTRL,DDRCTRL_RST,DDRC_RST,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_RST,DDR_AXI0_RST,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_RST,DDR_AXI1_RST,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_RST,DDR_AXI2_RST,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_RST,DDRPHY_CTL_RST,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_RST,FPGA_DDRPHY_SOFT_RST,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_RST,DDRPHY_APB_RST,0x1);
    wr_fld_s_r(var,CPU,DDRCTRL,DDRCTRL_RST,DDRPHY_APB_RST,0x1);

    // wr fld of DDRCTRL_RST.DDR_APB_RST
    var = wr_fld_r_s(    CPU,DDRCTRL,DDRCTRL_RST,DDR_APB_RST,0x1);
    wr_fld_s_r(var,CPU,DDRCTRL,DDRCTRL_RST,DDR_APB_RST,0x1);

    // wr fld of DDRCTRL_CLK.DDR_CLK_ENA
    var = wr_fld_r_s(    CPU,DDRCTRL,DDRCTRL_CLK,DDR_CLK_ENA,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_CLK,DDR_APB_CLK_ENA,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_CLK,DDRPHY_CTL_CLK_ENA,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_CLK,DDRPHY_APB_CLK_ENA,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_CLK,DDR_AXI0_CLK_ENA,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_CLK,DDR_AXI1_CLK_ENA,0x1);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_CLK,DDR_AXI2_CLK_ENA,0x1);
    wr_fld_s_r(var,CPU,DDRCTRL,DDRCTRL_CLK,DDR_AXI2_CLK_ENA,0x1);

    // wr fld of DDRCTRL_RST.DDR_APB_RST
    wr_fld_r_r(    CPU,DDRCTRL,DDRCTRL_RST,DDR_APB_RST,0x0);

    // DDRC configurations
    // wr fld of ECCCFG0.ECC_MODE
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,ECCCFG0,ECC_MODE, params_DDR_UMCTL2_ECC_MODE);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ECCCFG0,ECC_REGION_MAP, params_DDR_UMCTL2_ECC_REGION_MAP);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ECCCFG0,ECC_REGION_MAP_GRANU, params_DDR_UMCTL2_ECC_REGION_MAP_GRANU);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ECCCFG0,ECC_REGION_MAP_OTHER, params_DDR_UMCTL2_ECC_REGION_MAP_OTHER);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,ECCCFG0,ECC_REGION_MAP_OTHER, params_DDR_UMCTL2_ECC_REGION_MAP_OTHER);

    // wr fld of RFSHCTL3.DIS_AUTO_REFRESH
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,RFSHCTL3,DIS_AUTO_REFRESH, 0x1);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,RFSHCTL3,DIS_AUTO_REFRESH, 0x1);

    // wr fld of PWRCTL.SELFREF_EN
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,PWRCTL,SELFREF_EN, 0x0);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,PWRCTL,POWERDOWN_EN, 0x0);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,PWRCTL,POWERDOWN_EN, 0x0);

    // wr fld of INIT0.PRE_CKE_X1024
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,INIT0,PRE_CKE_X1024, params_DDR_UMCTL2_PRE_CKE_X1024);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,INIT0,POST_CKE_X1024,params_DDR_UMCTL2_POST_CKE_X1024);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,INIT0,SKIP_DRAM_INIT,params_DDR_UMCTL2_SKIP_DRAM_INIT);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,INIT0,SKIP_DRAM_INIT, params_DDR_UMCTL2_SKIP_DRAM_INIT);

    // wr fld of INIT1.DRAM_RSTN_X1024
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,INIT1,DRAM_RSTN_X1024, params_DDR_UMCTL2_DRAM_RSTN_X1024);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,INIT1,DRAM_RSTN_X1024, params_DDR_UMCTL2_DRAM_RSTN_X1024);

    // wr fld of INIT3.MR
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,INIT3,MR, params_DDR_UMCTL2_MR);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,INIT3,EMR,params_DDR_UMCTL2_EMR);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,INIT3,EMR, params_DDR_UMCTL2_EMR);

    // wr fld of INIT4.EMR2
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,INIT4,EMR2, params_DDR_UMCTL2_EMR2);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,INIT4,EMR3, params_DDR_UMCTL2_EMR3);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,INIT4,EMR3, params_DDR_UMCTL2_EMR3);

    // wr fld of INIT5.DEV_ZQINIT_X32
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,INIT5,DEV_ZQINIT_X32, params_DDR_UMCTL2_DEV_ZQINIT_X32);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,INIT5,DEV_ZQINIT_X32, params_DDR_UMCTL2_DEV_ZQINIT_X32);

    // wr fld of MSTR.DDR3
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,MSTR,DDR3ENA, params_DDR_UMCTL2_DDR3);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,MSTR,EN_2T_TIMING_MODE, params_DDR_UMCTL2_EN_2T_TIMING_MODE);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,MSTR,DLL_OFF_MODE, params_DDR_UMCTL2_DLL_OFF_MODE);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,MSTR,BURST_RDWR, params_DDR_UMCTL2_BURST_RDWR);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,MSTR,BURSTCHOP, params_DDR_UMCTL2_BURSTCHOP);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,MSTR,ACTIVE_RANKS, params_DDR_UMCTL2_ACTIVE_RANKS);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,MSTR,ACTIVE_RANKS, params_DDR_UMCTL2_ACTIVE_RANKS);

    // wr fld of DRAMTMG0.WR2PRE
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DRAMTMG0,WR2PRE, params_DDR_UMCTL2_WR2PRE);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG0,T_FAW,  params_DDR_UMCTL2_T_FAW);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG0,T_RAS_MAX, params_DDR_UMCTL2_T_RAS_MAX);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG0,T_RAS_MIN, params_DDR_UMCTL2_T_RAS_MIN);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG0,T_RAS_MIN, params_DDR_UMCTL2_T_RAS_MIN);

    // wr fld of DRAMTMG1.T_XP
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DRAMTMG1,T_XP, params_DDR_UMCTL2_T_XP);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG1,RD2PRE, params_DDR_UMCTL2_RD2PRE);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG1,T_RC, params_DDR_UMCTL2_T_RC);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG1,T_RC, params_DDR_UMCTL2_T_RC);

    // wr fld of DRAMTMG2.RD2WR
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DRAMTMG2,RD2WR, params_DDR_UMCTL2_RD2WR);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG2,WR2RD, params_DDR_UMCTL2_WR2RD);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG2,WR2RD, params_DDR_UMCTL2_WR2RD);

    // wr fld of DRAMTMG3.T_MOD
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DRAMTMG3,T_MOD, params_DDR_UMCTL2_T_MOD);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG3,T_MRD, params_DDR_UMCTL2_T_MRD);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG3,T_MRD, params_DDR_UMCTL2_T_MRD);

    // wr fld of DRAMTMG4.T_RCD
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DRAMTMG4,T_RCD, params_DDR_UMCTL2_T_RCD);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG4,T_CCD, params_DDR_UMCTL2_T_CCD);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG4,T_RRD, params_DDR_UMCTL2_T_RRD);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG4,T_RP,  params_DDR_UMCTL2_T_RP);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG4,T_RP, params_DDR_UMCTL2_T_RP);

    // wr fld of DRAMTMG5.T_CKSRX
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DRAMTMG5,T_CKSRX, params_DDR_UMCTL2_T_CKSRX);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG5,T_CKSRE, params_DDR_UMCTL2_T_CKSRE);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG5,T_CKESR, params_DDR_UMCTL2_T_CKESR);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG5,T_CKE,   params_DDR_UMCTL2_T_CKE);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG5,T_CKE, params_DDR_UMCTL2_T_CKE);

    // wr fld of DRAMTMG8.T_XS_X32
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DRAMTMG8,T_XS_X32, params_DDR_UMCTL2_T_XS_X32);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG8,T_XS_DLL_X32, params_DDR_UMCTL2_T_XS_DLL_X32);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DRAMTMG8,T_XS_DLL_X32, params_DDR_UMCTL2_T_XS_DLL_X32);

    // wr fld of DFITMG0.DFI_TPHY_WRLAT
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_TPHY_WRLAT,   params_DDR_UMCTL2_DFI_TPHY_WRLAT);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_TPHY_WRDATA,  params_DDR_UMCTL2_DFI_TPHY_WRDATA);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_T_RDDATA_EN,  params_DDR_UMCTL2_DFI_T_RDDATA_EN);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_T_CTRL_DELAY, params_DDR_UMCTL2_DFI_T_CTRL_DELAY);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_T_CTRL_DELAY, params_DDR_UMCTL2_DFI_T_CTRL_DELAY);

    // wr fld of DFITMG1.DFI_T_DRAM_CLK_ENABLE
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DFITMG1,DFI_T_DRAM_CLK_ENABLE, params_DDR_UMCTL2_DFI_T_DRAM_CLK_ENABLE);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DFITMG1,DFI_T_DRAM_CLK_DISABLE,params_DDR_UMCTL2_DFI_T_DRAM_CLK_DISABLE);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DFITMG1,DFI_T_WRDATA_DELAY, params_DDR_UMCTL2_DFI_T_WRDATA_DELAY);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DFITMG1,DFI_T_WRDATA_DELAY, params_DDR_UMCTL2_DFI_T_WRDATA_DELAY);

    // wr fld of DFIMISC.DFI_INIT_COMPLETE_EN
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DFIMISC,DFI_INIT_COMPLETE_EN, 0x0);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DFIMISC,DFI_INIT_COMPLETE_EN, 0x0);

    // wr fld of DFIUPD0.DIS_AUTO_CTRLUPD_SRX
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DFIUPD0,DIS_AUTO_CTRLUPD_SRX, params_DDR_UMCTL2_DIS_AUTO_CTRLUPD_SRX);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DFIUPD0,DIS_AUTO_CTRLUPD, (unsigned int)params_DDR_UMCTL2_DIS_AUTO_CTRLUPD);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DFIUPD0,DIS_AUTO_CTRLUPD, (unsigned int)params_DDR_UMCTL2_DIS_AUTO_CTRLUPD);

    // wr fld of DFIUPD1.DFI_T_CTRLUPD_INTERVAL_MIN_X1024
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DFIUPD1,DFI_T_CTRLUPD_INTERVAL_MIN_X1024, params_DDR_UMCTL2_DFI_T_CTRLUPD_INTERVAL_MIN_X1024);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,DFIUPD1,DFI_T_CTRLUPD_INTERVAL_MAX_X1024, params_DDR_UMCTL2_DFI_T_CTRLUPD_INTERVAL_MAX_X1024);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DFIUPD1,DFI_T_CTRLUPD_INTERVAL_MAX_X1024, params_DDR_UMCTL2_DFI_T_CTRLUPD_INTERVAL_MAX_X1024);

    // wr fld of RFSHCTL0.REFRESH_BURST
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,RFSHCTL0,REFRESH_BURST, params_DDR_UMCTL2_REFRESH_BURST);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,RFSHCTL0,REFRESH_BURST, params_DDR_UMCTL2_REFRESH_BURST);

    // wr fld of RFSHTMG.T_RFC_NOM_X1_X32
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,RFSHTMG,T_RFC_NOM_X1_X32, params_DDR_UMCTL2_T_RFC_NOM_X1_X32);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,RFSHTMG,T_RFC_MIN, params_DDR_UMCTL2_T_RFC_MIN);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,RFSHTMG,T_RFC_MIN, params_DDR_UMCTL2_T_RFC_MIN);

    // wr fld of ODTCFG.RD_ODT_DELAY
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,ODTCFG,RD_ODT_DELAY, params_DDR_UMCTL2_RD_ODT_DELAY);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ODTCFG,RD_ODT_HOLD,  params_DDR_UMCTL2_RD_ODT_HOLD);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ODTCFG,WR_ODT_DELAY, params_DDR_UMCTL2_WR_ODT_DELAY);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ODTCFG,WR_ODT_HOLD,  params_DDR_UMCTL2_WR_ODT_HOLD);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,ODTCFG,WR_ODT_HOLD, params_DDR_UMCTL2_WR_ODT_HOLD);

    // wr fld of SARBASE0.BASE_ADDR
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,SARBASE0,BASE_ADDR, params_DDR_UMCTL2_BASE_ADDR);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,SARBASE0,BASE_ADDR, params_DDR_UMCTL2_BASE_ADDR);

    // wr fld of SARSIZE0.NBLOCKS
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,SARSIZE0,NBLOCKS, params_DDR_UMCTL2_NBLOCKS);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,SARSIZE0,NBLOCKS, params_DDR_UMCTL2_NBLOCKS);

    // wr fld of ADDRMAP1.ADDRMAP_BANK_B2
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,ADDRMAP1,ADDRMAP_BANK_B2, params_DDR_UMCTL2_ADDRMAP_BANK_B2);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP1,ADDRMAP_BANK_B1, params_DDR_UMCTL2_ADDRMAP_BANK_B1);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP1,ADDRMAP_BANK_B0, params_DDR_UMCTL2_ADDRMAP_BANK_B0);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP1,ADDRMAP_BANK_B0, params_DDR_UMCTL2_ADDRMAP_BANK_B0);

    // wr fld of ADDRMAP2.ADDRMAP_COL_B5
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,ADDRMAP2,ADDRMAP_COL_B5, params_DDR_UMCTL2_ADDRMAP_COL_B5);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP2,ADDRMAP_COL_B4, params_DDR_UMCTL2_ADDRMAP_COL_B4);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP2,ADDRMAP_COL_B3, params_DDR_UMCTL2_ADDRMAP_COL_B3);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP2,ADDRMAP_COL_B2, params_DDR_UMCTL2_ADDRMAP_COL_B2);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP2,ADDRMAP_COL_B2, params_DDR_UMCTL2_ADDRMAP_COL_B2);

    // wr fld of ADDRMAP3.ADDRMAP_COL_B9
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,ADDRMAP3,ADDRMAP_COL_B9, params_DDR_UMCTL2_ADDRMAP_COL_B9);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP3,ADDRMAP_COL_B8, params_DDR_UMCTL2_ADDRMAP_COL_B8);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP3,ADDRMAP_COL_B7, params_DDR_UMCTL2_ADDRMAP_COL_B7);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP3,ADDRMAP_COL_B6, params_DDR_UMCTL2_ADDRMAP_COL_B6);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP3,ADDRMAP_COL_B6, params_DDR_UMCTL2_ADDRMAP_COL_B6);

    // wr fld of ADDRMAP4.ADDRMAP_COL_B11
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,ADDRMAP4,ADDRMAP_COL_B11, params_DDR_UMCTL2_ADDRMAP_COL_B11);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP4,ADDRMAP_COL_B10, params_DDR_UMCTL2_ADDRMAP_COL_B10);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP4,ADDRMAP_COL_B10, params_DDR_UMCTL2_ADDRMAP_COL_B10);

    // wr fld of ADDRMAP5.ADDRMAP_ROW_B0
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,ADDRMAP5,ADDRMAP_ROW_B0, params_DDR_UMCTL2_ADDRMAP_ROW_B0);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP5,ADDRMAP_ROW_B1, params_DDR_UMCTL2_ADDRMAP_ROW_B1);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP5,ADDRMAP_ROW_B2_10, params_DDR_UMCTL2_ADDRMAP_ROW_B2_10);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP5,ADDRMAP_ROW_B11,   params_DDR_UMCTL2_ADDRMAP_ROW_B11);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP5,ADDRMAP_ROW_B11, params_DDR_UMCTL2_ADDRMAP_ROW_B11);

    // wr fld of ADDRMAP6.ADDRMAP_ROW_B12
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,ADDRMAP6,ADDRMAP_ROW_B12, params_DDR_UMCTL2_ADDRMAP_ROW_B12);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP6,ADDRMAP_ROW_B13, params_DDR_UMCTL2_ADDRMAP_ROW_B13);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP6,ADDRMAP_ROW_B14, params_DDR_UMCTL2_ADDRMAP_ROW_B14);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP6,ADDRMAP_ROW_B15, params_DDR_UMCTL2_ADDRMAP_ROW_B15);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP6,ADDRMAP_ROW_B15, params_DDR_UMCTL2_ADDRMAP_ROW_B15);

    // wr fld of ADDRMAP0.ADDRMAP_CS_BIT0
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,ADDRMAP0,ADDRMAP_CS_BIT0, params_DDR_UMCTL2_ADDRMAP_CS_BIT0);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,ADDRMAP0,ADDRMAP_CS_BIT0, params_DDR_UMCTL2_ADDRMAP_CS_BIT0);

    // wr fld of RESET.CPU_CORE_0_COLD_RST
    wr_fld_r_r(    CPU,DDRCTRL,RESET,CPU_CORE_0_COLD_RST,0x0);
    // wr fld of RESET.MEM_RST
    wr_fld_r_r(    CPU,DDRCTRL,RESET,MEM_RST,0x0);


    // Release all other resets
    // wr fld of DDRCTRL_RST.DDR_AXI0_RST
    var = wr_fld_r_s(    CPU,DDRCTRL,DDRCTRL_RST,DDR_AXI0_RST,0x0);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_RST,DDR_AXI1_RST,0x0);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_RST,DDR_AXI2_RST,0x0);
    wr_fld_s_r(var,CPU,DDRCTRL,DDRCTRL_RST,DDR_AXI2_RST,0x0);

    // wr fld of DDRCTRL_RST.DDRPHY_CTL_RST
    var = wr_fld_r_s(    CPU,DDRCTRL,DDRCTRL_RST,DDRPHY_CTL_RST,0x0);
    var = wr_fld_s_s(var,CPU,DDRCTRL,DDRCTRL_RST,FPGA_DDRPHY_SOFT_RST,0x0);
    wr_fld_s_r(var,CPU,DDRCTRL,DDRCTRL_RST,FPGA_DDRPHY_SOFT_RST,0x0);

    // wr fld of DDRCTRL_RST.DDRPHY_APB_RST
    var = wr_fld_r_s(    CPU,DDRCTRL,DDRCTRL_RST,DDRPHY_APB_RST,0x0);
    wr_fld_s_r(var,CPU,DDRCTRL,DDRCTRL_RST,DDRPHY_APB_RST,0x0);

    // wr fld of DDRCTRL_RST.DDRC_RST
    var = wr_fld_r_s(    CPU,DDRCTRL,DDRCTRL_RST,DDRC_RST,0x0);
    wr_fld_s_r(var,CPU,DDRCTRL,DDRCTRL_RST,DDRC_RST,0x0);


    // PHY-PUB initialization
    if (params_PHY_init) {

      // wr fld of DSGCR.RRMODE
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,DSGCR,RRMODE, params_DDR_PHY_RRMODE);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,DSGCR,RRMODE, params_DDR_PHY_RRMODE);

      // wr fld of PGCR2.TREFPRD
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,PGCR2,TREFPRD, params_DDR_PHY_TREFPRD);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,PGCR2,TREFPRD, params_DDR_PHY_TREFPRD);

      // wr fld of PTR0.TPLLGS
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,PTR0,TPLLGS, params_DDR_PHY_TPLLGS);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,PTR0,TPLLPD, params_DDR_PHY_TPLLPD);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,PTR0,TPLLPD, params_DDR_PHY_TPLLPD);

      // wr fld of PTR1.TPLLRST
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,PTR1,TPLLRST,  params_DDR_PHY_TPLLRST);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,PTR1,TPLLLOCK, params_DDR_PHY_TPLLLOCK);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,PTR1,TPLLLOCK, params_DDR_PHY_TPLLLOCK);

      // rd reg of PIR
      stat = rd_reg(DDR_PHY,DDR3_2_PHY_PUB,PIR);

      // wr reg of PIR
      wr_reg(DDR_PHY,DDR3_2_PHY_PUB,PIR,0x73);

      // Configure remaining reg
      // wr fld of DCR.DDR2T
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,DCR,DDR2T, params_DDR_PHY_DDR2T);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DCR,DDRMD, params_DDR_PHY_DDRMD);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,DCR,DDRMD, params_DDR_PHY_DDRMD);

      // wr reg of MR0
      wr_reg(DDR_PHY,DDR3_2_PHY_PUB,MR0, params_DDR_PHY_MR0);

      // wr reg of MR1
      wr_reg(DDR_PHY,DDR3_2_PHY_PUB,MR1, params_DDR_PHY_MR1);

      // wr reg of MR2
      wr_reg(DDR_PHY,DDR3_2_PHY_PUB,MR2, params_DDR_PHY_MR2);

      // wr reg of MR3
      wr_reg(DDR_PHY,DDR3_2_PHY_PUB,MR3, params_DDR_PHY_MR3);

      // wr fld of DTPR0.TRTP
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,DTPR0,TRTP, params_DDR_PHY_TRTP);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR0,TWTR, params_DDR_PHY_TWTR);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR0,TRP,  params_DDR_PHY_TRP);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR0,TRCD, params_DDR_PHY_TRCD);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR0,TRAS, params_DDR_PHY_TRAS);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR0,TRRD, params_DDR_PHY_TRRD);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR0,TRC,  params_DDR_PHY_TRC);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR0,TRC, params_DDR_PHY_TRC);

      // wr fld of DTPR1.TMRD
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,DTPR1,TMRD,  params_DDR_PHY_TMRD);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR1,TMOD,  params_DDR_PHY_TMOD);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR1,TFAW,  params_DDR_PHY_TFAW);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR1,TRFC,  params_DDR_PHY_TRFC);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR1,TWLMRD,params_DDR_PHY_TWLMRD);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR1,TWLO,  params_DDR_PHY_TWLO);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR1,TWLO, params_DDR_PHY_TWLO);

      // wr fld of DTPR2.TXS
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,DTPR2,TXS,  params_DDR_PHY_TXS);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR2,TXP,  params_DDR_PHY_TXP);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR2,TCKE, params_DDR_PHY_TCKE);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR2,TDLLK,params_DDR_PHY_TDLLK);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,DTPR2,TDLLK,params_DDR_PHY_TDLLK);

      // wr fld of PTR3.TDINIT0
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,PTR3,TDINIT0, CEIL_VAL(500000000.0/params_tck_min)); // params_DDR_PHY_TDINIT0);  // 500us pre cke
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,PTR3,TDINIT1, params_DDR_PHY_TDINIT1);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,PTR3,TDINIT1, params_DDR_PHY_TDINIT1);

      // wr fld of PTR4.TDINIT2
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,PTR4,TDINIT2, CEIL_VAL(200000000.0/params_tck_min)); // params_DDR_PHY_TDINIT2);  // 200us DRAM RSTn
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,PTR4,TDINIT3, params_DDR_PHY_TDINIT3);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,PTR4,TDINIT3, params_DDR_PHY_TDINIT3);

      // wr fld of DXCCR.DQSRES
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,DXCCR,DQSRES, params_DDR_PHY_DQSRES);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DXCCR,DQSNRES,params_DDR_PHY_DQSNRES);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,DXCCR,DQSNRES, params_DDR_PHY_DQSNRES);

      // wr fld of DSGCR.CUAEN
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,DSGCR,CUAEN, params_DDR_PHY_CUAEN);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DSGCR,SDRMODE, params_DDR_PHY_SDRMODE);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,DSGCR,SDRMODE, params_DDR_PHY_SDRMODE);

      // polling for init done for 1ms
      timeout = poll_idone(0XB5);

      if (timeout == 0) {
        my_print(TIMEOUT,1);
        my_print(ERROR_MSG,1);
        return 0;
      }
    }

    if (params_DRAM_init) {

      // rd reg of PIR
      stat = rd_reg(DDR_PHY,DDR3_2_PHY_PUB,PIR);

      // wr reg of PIR
      wr_reg(DDR_PHY,DDR3_2_PHY_PUB,PIR,0x181);

      // poll for DRAM init done in case of PUB
      if (params_init_by_pub) {
        // rd fld of PGSR0.DIDONE
        timeout = 1000; // 1ms
        do {
          my_wait(1000, 0XA3);
          stat = rd_fld_r(DDR_PHY,DDR3_2_PHY_PUB,PGSR0,DIDONE);
          timeout--;
        } while (stat != 1 && timeout != 0);

        if (timeout==0) {
          my_print(TIMEOUT,2);
          my_print(ERROR_MSG,2);
          return 0;
        }
      }
    }


    // Watiing for op mode set to normal
    // rd fld of DFIMISC.DFI_INIT_COMPLETE_EN
    stat = rd_fld_r(DDR_UMCTL2,UMCTL2_REGS,DFIMISC,DFI_INIT_COMPLETE_EN);

    // wr fld of SWCTL.SW_DONE
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,SWCTL,SW_DONE,0x0);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,SWCTL,SW_DONE,0x0);

    // wr fld of DFIMISC.DFI_INIT_COMPLETE_EN
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DFIMISC,DFI_INIT_COMPLETE_EN,0x1);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DFIMISC,DFI_INIT_COMPLETE_EN,0x1);

    // wr fld of SWCTL.SW_DONE
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,SWCTL,SW_DONE,0x1);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,SWCTL,SW_DONE,0x1);

    // poll for sw done ack
    timeout = poll_sw_ack(0XD5);

    if (timeout==0) {
      my_print(TIMEOUT,6);
      return 0;
    }

    // polling for fld value becomes 1 for 1ms
    // rd fld of STAT.OPERATING_MODE
    timeout = 1000;
    do {
      my_wait(1000, 0XBC);
      stat = rd_fld_r(DDR_UMCTL2,UMCTL2_REGS,STAT,OPERATING_MODE);
      timeout--;
    } while (stat != 1 && timeout != 0);

    if (timeout==0) {
      my_print(TIMEOUT,7);
      return 0;
    }


    if (params_data_training) {

      // wr fld of SWCTL.SW_DONE
      var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,SWCTL,SW_DONE,0x0);
      wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,SWCTL,SW_DONE,0x0);

      // wr fld of DFIMISC.DFI_INIT_COMPLETE_EN
      var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DFIMISC,DFI_INIT_COMPLETE_EN,0x0);
      wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DFIMISC,DFI_INIT_COMPLETE_EN,0x0);

      // wr fld of SWCTL.SW_DONE
      var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,SWCTL,SW_DONE,0x1);
      wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,SWCTL,SW_DONE,0x1);

      // poll for sw done ack
      timeout = poll_sw_ack(0XD6);

      if (timeout==0) {
        my_print(TIMEOUT,8);
        return 0;
      }

      // DATA training
      // Waiting for Write leveling, DQS gate training, Write_latency adjustment done
      stat = rd_reg(DDR_PHY,DDR3_2_PHY_PUB,PIR);

      // wr fld of DTCR.RANKEN
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,DTCR,RANKEN, params_DDR_PHY_RANKEN);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,DTCR,RANKEN, params_DDR_PHY_RANKEN);

      // wr fld of DTCR.DTMPR
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,DTCR,DTMPR,  0x1);
      var = wr_fld_s_s(var,DDR_PHY,DDR3_2_PHY_PUB,DTCR,DTRPTN, params_DDR_PHY_DTRPTN);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,DTCR,DTRPTN, params_DDR_PHY_DTRPTN);

      // wr reg of PIR
      wr_reg(DDR_PHY,DDR3_2_PHY_PUB,PIR,0xe01);

      // polling for init done for 1ms
      timeout = poll_idone(0XB6);

      if (timeout==0) {
        my_print(TIMEOUT,3);
        return 0;
      }

      // wr fld of DTCR.DTMPR
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,DTCR,DTMPR, 0x0);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,DTCR,DTMPR, 0x0);

      // wr fld of DTCR.DTRANK
      var = wr_fld_r_s(    DDR_PHY,DDR3_2_PHY_PUB,DTCR,DTRANK, params_DDR_PHY_DTRANK);
      wr_fld_s_r(var,DDR_PHY,DDR3_2_PHY_PUB,DTCR,DTRANK, params_DDR_PHY_DTRANK);

      // wr reg of PIR
      wr_reg(DDR_PHY,DDR3_2_PHY_PUB,PIR,0x1001);

      // polling for init done for 1ms
      timeout = poll_idone(0XB7);

      if (timeout==0) {
        my_print(TIMEOUT,4);
        return 0;
      }

      // wr reg of PIR
      wr_reg(DDR_PHY,DDR3_2_PHY_PUB,PIR,0xe001);

      // polling for init done for 1ms
      timeout = poll_idone(0XB8);

      if (timeout==0) {
        my_print(TIMEOUT,5);
        my_print(ERROR_MSG,0xbad);
        return 0;
      } else {
        my_print(SUCCESS,0xeeee);
      }
    }


    // rd reg of DX0GSR0
    stat = rd_reg(DDR_PHY,DDR3_2_PHY_PUB,DX0GSR0);

    // rd reg of PGSR0
    stat = rd_reg(DDR_PHY,DDR3_2_PHY_PUB,PGSR0);

    // wr fld of RFSHCTL3.DIS_AUTO_REFRESH
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,RFSHCTL3,DIS_AUTO_REFRESH,0x0);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,RFSHCTL3,DIS_AUTO_REFRESH,0x0);

    // wr fld of PWRCTL.POWERDOWN_EN
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,PWRCTL,POWERDOWN_EN,0x0);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,PWRCTL,POWERDOWN_EN,0x0);

    // wr fld of SWCTL.SW_DONE
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,SWCTL,SW_DONE,0x0);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,SWCTL,SW_DONE,0x0);

    // wr fld of DFIMISC.DFI_INIT_COMPLETE_EN
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DFIMISC,DFI_INIT_COMPLETE_EN,0x1);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,DFIMISC,DFI_INIT_COMPLETE_EN,0x1);

    // wr fld of SWCTL.SW_DONE
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,SWCTL,SW_DONE,0x1);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,SWCTL,SW_DONE,0x1);

    // poll for sw done ack
    timeout = poll_sw_ack(0XD7);

    if (timeout==0) {
      my_print(TIMEOUT,9);
      return 0;
    }


    // wr fld of RFSHCTL3.DIS_AUTO_REFRESH
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,RFSHCTL3,DIS_AUTO_REFRESH,0x0);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,RFSHCTL3,DIS_AUTO_REFRESH,0x0);

    // wr fld of PWRCTL.SELFREF_EN
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,PWRCTL,SELFREF_EN,0x0);
    var = wr_fld_s_s(var,DDR_UMCTL2,UMCTL2_REGS,PWRCTL,POWERDOWN_EN,0x0);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_REGS,PWRCTL,POWERDOWN_EN,0x0);

    // wr fld of PCTRL_0.PCTRL_0_PORT_EN
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_MP,PCTRL_0,PCTRL_0_PORT_EN,0x1);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_MP,PCTRL_0,PCTRL_0_PORT_EN,0x1);

    // wr fld of PCTRL_1.PCTRL_1_PORT_EN
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_MP,PCTRL_1,PCTRL_1_PORT_EN,0x1);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_MP,PCTRL_1,PCTRL_1_PORT_EN,0x1);

    // wr fld of PCTRL_2.PCTRL_2_PORT_EN
    var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_MP,PCTRL_2,PCTRL_2_PORT_EN,0x1);
    wr_fld_s_r(var,DDR_UMCTL2,UMCTL2_MP,PCTRL_2,PCTRL_2_PORT_EN,0x1);

    return 1;
  }
}


static void ecc_enable_scrubbing(void)
{
	uint32_t size = DDR_BASE_ADDR;
	size += DDR_MEM_SIZE;

	/* 1.  Disable AXI port. port_en = 0 */
	wr_fld_r_r (DDR_UMCTL2, UMCTL2_MP, PCTRL_0, PCTRL_0_PORT_EN, 0);
	wr_fld_r_r (DDR_UMCTL2, UMCTL2_MP, PCTRL_1, PCTRL_1_PORT_EN, 0);
	wr_fld_r_r (DDR_UMCTL2, UMCTL2_MP, PCTRL_2, PCTRL_2_PORT_EN, 0);

    /* 2. Set addr range to scrub */
    /* Here configured full addr range for scrub */
    wr_reg (DDR_UMCTL2, UMCTL2_MP, SBRSTART0, DDR_BASE_ADDR>>1);                // As per HIF addr (offset addr and >>1 (word))
    wr_reg (DDR_UMCTL2, UMCTL2_MP, SBRRANGE0, size>>1); // max 1K (512x1word (2bytes))
    wr_reg (DDR_UMCTL2, UMCTL2_MP, SBRSTART1, 0);
    wr_reg (DDR_UMCTL2, UMCTL2_MP, SBRRANGE1, 0);

	/* 2. scrub_mode = 1 */
	wr_fld_r_r (DDR_UMCTL2, UMCTL2_MP, SBRCTL, SCRUB_MODE, 1);

	/* 3. scrub_interval = 0 */
	wr_fld_r_r (DDR_UMCTL2, UMCTL2_MP, SBRCTL, SCRUB_INTERVAL, 0);

	/* 4. Data pattern = 0 */
	wr_reg     (DDR_UMCTL2, UMCTL2_MP, SBRWDATA0, 0);

	/* 5. (skip) */
	/* 6. Enable SBR programming */
	wr_fld_r_r (DDR_UMCTL2, UMCTL2_MP, SBRCTL, SCRUB_EN, 1);

	/* 7. Poll SBRSTAT.scrub_done */
	while (rd_fld_r(DDR_UMCTL2, UMCTL2_MP, SBRSTAT, SCRUB_DONE) == 0)
		;
	/* 8. Poll SBRSTAT.scrub_busy */
	while (rd_fld_r(DDR_UMCTL2, UMCTL2_MP, SBRSTAT, SCRUB_BUSY) == 1)
		;
	/* 9. Disable SBR programming */
	wr_fld_r_r (DDR_UMCTL2, UMCTL2_MP, SBRCTL, SCRUB_EN, 0);
#if 0
	/* 10. Normal scrub operation, mode = 0, interval = 100 */
	wr_fld_r_r (DDR_UMCTL2, UMCTL2_MP, SBRCTL, SCRUB_MODE, 0);
	wr_fld_r_r (DDR_UMCTL2, UMCTL2_MP, SBRCTL, SCRUB_INTERVAL, 100);
	/* 11. Enable SBR progeramming again  */
	wr_fld_r_r (DDR_UMCTL2, UMCTL2_MP, SBRCTL, SCRUB_EN, 1);
#endif
	/* 12. Enable AXI port */
	wr_fld_r_r (DDR_UMCTL2, UMCTL2_MP, PCTRL_0, PCTRL_0_PORT_EN, 1);
	wr_fld_r_r (DDR_UMCTL2, UMCTL2_MP, PCTRL_1, PCTRL_1_PORT_EN, 1);
	wr_fld_r_r (DDR_UMCTL2, UMCTL2_MP, PCTRL_2, PCTRL_2_PORT_EN, 1);
}

#else
inline static void pause(uint32_t val)
{
	register int i = 0;

	for (i = 0; i < val; ++i)
	    asm volatile("nop;");
}

// Task to poll for Playback done interrupt
inline static void poll_pbdone(int32_t timeout_in)
{
	int32_t timeout;
	uint8_t stat;

	timeout = timeout_in;  // assign max timeout expected
	do {
		pause(1); // 1us
		stat = rd_fld_r(DDR_PHY, DDR_MULTIPHY_REGS, INTERRUPT_STATUS,PB_INTR_STS);
		timeout--;
	} while (stat == 0 && timeout != 0); // break loop when init done = 1 or timeout reached

	if (stat != 1) {
		error("ERROR:DDR_FPGAPHY_CFG, Timeout reached while waiting for playback done\r\n");
	}

	// Also poll for playback running status = 0 (ensure that PB is stopped running)
	timeout = timeout_in;
	do {
		pause(1); // 1US
		stat = rd_fld_r(DDR_PHY, DDR_MULTIPHY_REGS, PLAYBACK_STATUS,PLAYBACK_RUNNING);
		timeout--;
	} while (stat == 1 && timeout != 0); // break when running = 0

	if (stat) {
		error("ERROR:DDR_FPGAPHY_CFG, Playback running status is still not set to 0\r\n");
	}
}

// Task to poll for Training completion status
inline static void poll_trdone(int32_t timeout_in)
{
	int32_t timeout=0;
	uint32_t stat;

	timeout = timeout_in;  // assign max timeout expected
	do {
	pause(1); // 1us
	stat = rd_reg(DDR_PHY, DDR_MULTIPHY_REGS, TRAIN_STATUS);
	timeout--;
	} while (stat != 1 && timeout != 0);

	if (stat != 1) {
		if (timeout == 0) {
		error("ERROR:DDR_FPGAPHY_CFG, Timeout reached while waiting for Autotraining to succeed\r\n");
		}

		stat = (stat & 0x4) >> 2;  // extract bit 2

		//if (stat[2]) begin
		if (stat == 1) {
		error("ERROR:DDR_FPGAPHY_CFG, Training is failed and reading back offset registers\r\n");
		rd_reg(DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_OFFSET_RANK_0_BYTES_7_0); // only [7:4] = byte1, [3:0] = byte0 is valid
		//error("ERROR:DDR_FPGAPHY_CFG, FCLK_READ_OFFSET_RANK_0_BYTES_7_0 = %x\r\n",offset);
		rd_reg(DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_OFFSET_RANK_0_BYTES_7_0); // only [7:4] = byte1, [3:0] = byte0 is valid

		rd_reg(DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_OFFSET_RANK_0_BYTE_8);  // not valid
		//error("ERROR:DDR_FPGAPHY_CFG, FCLK_READ_OFFSET_RANK_0_BYTE_8 = %x\r\n",offset);
		rd_reg(DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_OFFSET_RANK_0_BYTE_8);  // not valid
		}
	}
}

// Uncomment below once printf linking available
// // Define to enable and write quasi dynamic group registers in DDRC
#define quasi_write(tgt, grp, reg, fld, value) \
	wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, SWCTL, SW_DONE, 0); \
	wr_fld_r_r(tgt, grp, reg, fld, value); \
	wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, SWCTL, SW_DONE, 1); \
	timeout = 1000; \
	do { \
		pause(1); \
		stat = rd_fld_r(DDR_UMCTL2, UMCTL2_REGS, SWSTAT, SW_DONE_ACK); \
		timeout--; \
	} while (stat != 1 && timeout != 0); \
	if (stat == 1) {\
		timeout = 0; \
	} else { \
		timeout = 0; \
		error("ERROR:DDRC_CFG, Timeout reached while waiting for SW DONE ACK\r\n"); \
	}


// Task to poll for operating_mode status
inline static void poll_opmode(int32_t timeout_in)
{
	int32_t timeout;
	uint8_t stat;

	// Assert DFI_INIT_COMPLETE_EN to '1', if not set previously
	stat = rd_fld_r(DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_COMPLETE_EN);
	if (stat == 0) {
		quasi_write(DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_COMPLETE_EN, 1)
	}

	timeout = timeout_in;
	do {
		pause(1); // #1us;
		stat = rd_fld_r(DDR_UMCTL2, UMCTL2_REGS, STAT,OPERATING_MODE);
		timeout--;
	} while (stat != 1 && timeout != 0);

	if (stat == 1) {
		debug("DDRC_CFG, STAT.operating_mode is set to normal\r\n");
	} else {
		error("ERROR:DDRC_CFG, Timeout reached while waiting for operating mode as normal\r\n");
	}
}

inline static void dram_fpga_init (void)
{
	register int32_t timeout;
	register uint32_t stat;

	debug("DRAM_FPGA_INIT, #### Started DRAM init for FPGA ####\n");

	// if (cfg.sdram_init_ctrl == 1 || cfg.sdram_init_ctrl == 3) { // PE

#if INIT_MODE!=0
	// Enable PHY init using dfi_init_start = 1 in the UMCTL2 instead of enabling Playback Engine
	quasi_write(DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_COMPLETE_EN, 1)
	quasi_write(DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_START, 1)


	// Poll for Playback Running status through playback done interrupt
	//printf("DRAM_FPGA_INIT, #### Waiting for DRAM init done by PHY ####");
	poll_pbdone(2500);
#else

	// } else { // DDRC
	//   //printf("DRAM_FPGA_INIT, Trigger dummy playback enable for asserting dfi_init_complete");

	wr_reg(DDR_PHY, DDR_MULTIPHY_REGS, PLAYBACK_ENABLE, 1);

	wr_reg(DDR_PHY, DDR_MULTIPHY_REGS, PLAYBACK_ENABLE, 0); // stop dummy playback, to set dfi_init_complete = 1

#endif
	//   //printf("DRAM_FPGA_INIT, #### Waiting for DRAM init done by DDRC ####");
	// }
}

inline static void config_ddr_fpgaphy_init_reg (void)
{
	uint32_t var, i;

#if INIT_MODE!=0
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*0 + 0),0x000000f0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*0 + 1),0x000000f0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*0 + 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*0 + 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*0 + 4),0x00001771); // 0x00004e20;

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*1 + 0),0x800000f0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*1 + 1),0x800000f0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*1 + 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*1 + 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*1 + 4),0x00003a99); // 0x0000c350;

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*2 + 0),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*2 + 1),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*2 + 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*2 + 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*2 + 4),0x0000001c);

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*3 + 0),0x900008c3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*3 + 1),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*3 + 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*3 + 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*3 + 4),0x00000000);

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*4 + 0),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*4 + 1),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*4 + 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*4 + 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*4 + 4),0x00000004);

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*5 + 0),0x980000c3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*5 + 1),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*5 + 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*5 + 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*5 + 4),0x00000000);

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*6 + 0),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*6 + 1),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*6 + 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*6 + 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*6 + 4),0x00000004);

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*7 + 0),0x880000c3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*7 + 1),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*7 + 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*7 + 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*7 + 4),0x00000000);

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*8 + 0),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*8 + 1),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*8 + 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*8 + 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*8 + 4),0x00000004);

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*9 + 0),0x800320c3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*9 + 1),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*9 + 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*9 + 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*9 + 4),0x00000000);

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*10+ 0),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*10+ 1),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*10+ 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*10+ 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*10+ 4),0x0000000c);

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*11+ 0),0x860400c3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*11+ 1),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*11+ 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*11+ 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*11+ 4),0x00000000);

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*12+ 0),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*12+ 1),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*12+ 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*12+ 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*12+ 4),0x00000200);

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*13+ 0),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*13+ 1),0x800000f3);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*13+ 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*13+ 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*13+ 4),0x00080000);

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*14+ 0),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*14+ 1),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*14+ 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*14+ 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*14+ 4),0x00000000);

    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*15+ 0),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*15+ 1),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*15+ 2),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*15+ 3),0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*15+ 4),0x00000000);

    for(i=16; i<64; i=i+1) {// only iterate depth of a multi dimentional array (to cover all addr)
      REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*i +0),0x000000c0);
      REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*i +1),0x000000c0);
      REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*i +2),0x000000c0);
      REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*i +3),0x000000c0);
      REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*i +4),0x00000000);
    }
#endif

  // WRITE_PIPE_DELAY register configuration
  var = wr_fld_r_s(   DDR_PHY, DDR_MULTIPHY_REGS, WRITE_PIPE_DELAY, WR_PIPE_DELAY, 0x6); // cfg.phy_cfg.wpipe_dly.wp_dly);
  wr_fld_s_r(var, DDR_PHY, DDR_MULTIPHY_REGS, WRITE_PIPE_DELAY, MEMCLK_DLY_OFFSET, 0x0); // cfg.phy_cfg.wpipe_dly.effect);


  // CA_PIPE_DELAY register configuration
  wr_reg(DDR_PHY, DDR_MULTIPHY_REGS ,CA_PIPE_DELAY,0x1); // cfg.phy_cfg.cp_dly);


  // DATA_BYTE_SEL register configuration (get max Dbyte value)
  wr_reg(DDR_PHY, DDR_MULTIPHY_REGS ,DATA_BYTE_SEL, 0x1); // (DRAM_DATA_WIDTH/8)-1);  // for 16, configure 1, rank = 0 only


  // CTRLUPD_CONFIG register configuration
  var = wr_fld_r_s(   DDR_PHY, DDR_MULTIPHY_REGS ,CTRLUPD_CONFIG, REQ_TO_ACK_DLY, 0x2); // cfg.phy_cfg.ctrlupd_cfg.t_req_to_ack);
  wr_fld_s_r(var, DDR_PHY, DDR_MULTIPHY_REGS ,CTRLUPD_CONFIG, ACK_VLD_CLKS, 0x5); // cfg.phy_cfg.ctrlupd_cfg.t_ack_dassert);


  // INTERRUPT_MASK register configuration
  var = wr_fld_r_s(    DDR_PHY, DDR_MULTIPHY_REGS ,INTERRUPT_MASK, PB_INTR_MASK, 0x0); // cfg.phy_cfg.intr_mask.pb_intr);
  wr_fld_s_r(var, DDR_PHY, DDR_MULTIPHY_REGS ,INTERRUPT_MASK, DRQ_ERR_INTR_MASK, 0x0); // cfg.phy_cfg.intr_mask.dqs_err_intr);


  // INTERNAL_ENABLE register configuration
  var = wr_fld_r_s(    DDR_PHY, DDR_MULTIPHY_REGS ,INTERRUPT_ENABLE, PB_INTR_EN, 0x1); // cfg.phy_cfg.intr_en.pb_intr);
  wr_fld_s_r(var, DDR_PHY, DDR_MULTIPHY_REGS ,INTERRUPT_ENABLE, DQS_ERR_INTR_EN, 0x0); // cfg.phy_cfg.intr_en.dqs_err_intr);


  // INTERRUPT_CLEAR register configuration
  wr_reg(DDR_PHY, DDR_MULTIPHY_REGS ,INTERRUPT_CLEAR, 0x0); //cfg.phy_cfg.intr_clr);


  // DFICLK_READ_DELAY_RANK_0 register configuration
  var = wr_fld_r_s(     DDR_PHY, DDR_MULTIPHY_REGS ,DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_0_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte0);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_1_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte1);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_2_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte2);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_3_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte3);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_4_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte4);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_5_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte5);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_6_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte6);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_7_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte7);
  wr_fld_s_r(var, DDR_PHY, DDR_MULTIPHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_8_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte8);


  // FCLK_READ_DELAY_RANK_0 registers configuration
  var = wr_fld_r_s(     DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_0_R0, 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte0);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_1_R0, 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte1);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_2_R0, 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte2);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_3_R0, 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte3);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_4_R0, 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte4);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_5_R0, 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte5);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_6_R0, 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte6);
  wr_fld_s_r(var, DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_7_R0, 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte7);

  wr_fld_r_r(DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_DELAY_RANK_0_BYTE_8, RD_DATA_PIPE_8_R0, 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte8);


  // AUTOTRAIN_LOOP_ADDR register configuration
  // cfg.phy_cfg.autotrain_addr = 0;
  wr_reg(DDR_PHY, DDR_MULTIPHY_REGS ,AUTOTRAIN_LOOP_ADDR,0x0); // cfg.phy_cfg.autotrain_addr);


  // GENERAL_SETUP register configuration
  // cfg.phy_cfg.gen_setup.base_addr    = 0;
  // cfg.phy_cfg.gen_setup.auto_train   = 0;   //  auto train = 0
  // cfg.phy_cfg.gen_setup.pbmux        = 2;  // playback engine selected during init

  var = wr_fld_r_s(     DDR_PHY, DDR_MULTIPHY_REGS ,GENERAL_SETUP, FORMAT,           0x0); // cfg.phy_cfg.gen_setup.format);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS ,GENERAL_SETUP, PHY_CHANNEL_EN,   0x1); // cfg.phy_cfg.gen_setup.phy_channel_en);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS ,GENERAL_SETUP, WRITE_DQS_ADJUST, 0x1); // cfg.phy_cfg.gen_setup.wdqs_adj);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS ,GENERAL_SETUP, AUTOTRAIN_EN,     0x0); // cfg.phy_cfg.gen_setup.auto_train);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS ,GENERAL_SETUP, PB_MUX_SEL,       0x2); // cfg.phy_cfg.gen_setup.pbmux);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS ,GENERAL_SETUP, TRAINING_BASE,    0x0); // cfg.phy_cfg.gen_setup.base_addr);
  wr_fld_s_r(var, DDR_PHY, DDR_MULTIPHY_REGS ,GENERAL_SETUP, CA_BUS_VAL_EDGE,  0x0); // cfg.phy_cfg.gen_setup.ca_valid);

}

inline static void config_ddrc_init_reg(void)
{
}

static inline void ddr_fpga_training(void) {
  int32_t timeout;
  uint32_t stat, var, i;

  debug("DDR_FPGAPHY_CFG, #### DDR FPGA PHY based training is started ####\r\n");

  // Disable dfi_init_complete_en (so UMCTL2 waits for training to complete)
  quasi_write(DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_COMPLETE_EN, 0)

  // Loading PLAYBACK_ENGINE_CODE (or PE image) into RAM using indirect register
  //printf("DDR_FPGAPHY_CFG, Loading PE image into Playback RAM");
  // for(i=0; i<PE_USED_SIZE; i=i+1) {                      // only iterate depth of a multi dimentional array (to cover all addr)
  //   reg_idx = pe_img_train_ary[i][0];       // fetch addr for converting into reg index
  //   reg_idx = (reg_idx & 0x7FF) >> 2;                   // [10:0] >> 2; converting into byte addr or register index {base:5 bits, offset: 11bits}
  //   wr_reg(DDR_MULTIPHY, DDR_MULTIPHY_REGS, PLAYBACK_ENGINE_CODE(reg_idx), pe_img_train_ary[i][1]);
  // }

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*0 + 0), 0x8b0000c3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*0 + 1), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*0 + 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*0 + 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*0 + 4), 0x00000000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*1 + 0), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*1 + 1), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*1 + 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*1 + 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*1 + 4), 0x00000002);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*2 + 0), 0x8c0000e3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*2 + 1), 0x8c0000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*2 + 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*2 + 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*2 + 4), 0x00000000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*3 + 0), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*3 + 1), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*3 + 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*3 + 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*3 + 4), 0x00000000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*4 + 0), 0x8c0008e3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*4 + 1), 0x8c0008f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*4 + 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*4 + 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*4 + 4), 0x00040000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*5 + 0), 0x8c0000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*5 + 1), 0x8c0000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*5 + 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*5 + 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*5 + 4), 0x00000000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*6 + 0), 0x8c0010e3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*6 + 1), 0x8c0010f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*6 + 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*6 + 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*6 + 4), 0x00050000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*7 + 0), 0x8c0008f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*7 + 1), 0x8c0008f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*7 + 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*7 + 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*7 + 4), 0x00000000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*8 + 0), 0x880010f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*8 + 1), 0x880010f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*8 + 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*8 + 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*8 + 4), 0x00040000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*9 + 0), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*9 + 1), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*9 + 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*9 + 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*9 + 4), 0x00000003);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*10+ 0), 0x8d0000e3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*10+ 1), 0x8d0000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*10+ 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*10+ 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*10+ 4), 0x00000000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*11+ 0), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*11+ 1), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*11+ 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*11+ 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*11+ 4), 0x00000000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*12+ 0), 0x8d0008e3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*12+ 1), 0x8d0008f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*12+ 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*12+ 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*12+ 4), 0x00000000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*13+ 0), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*13+ 1), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*13+ 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*13+ 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*13+ 4), 0x00000000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*14+ 0), 0x8d0010e3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*14+ 1), 0x8d0010f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*14+ 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*14+ 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*14+ 4), 0x00000000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*15+ 0), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*15+ 1), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*15+ 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*15+ 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*15+ 4), 0x00000001);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*16+ 0), 0x8d0008f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*16+ 1), 0x800008f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*16+ 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*16+ 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*16+ 4), 0x00030000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*17+ 0), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*17+ 1), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*17+ 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*17+ 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*17+ 4), 0x00000004);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*18+ 0), 0x8a0000c3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*18+ 1), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*18+ 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*18+ 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*18+ 4), 0x00000000);

  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*19+ 0), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*19+ 1), 0x800000f3);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*19+ 2), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*19+ 3), 0x000000c0);
  REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*19+ 4), 0x00080000);

  for(i=20; i<64; i=i+1) {// only iterate depth of a multi dimentional array (to cover all addr)
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*i +0), 0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*i +1), 0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*i +2), 0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*i +3), 0x000000c0);
    REG_WR(DDR_PHY_PLAYBACK_ENGINE_CODE(8*i +4), 0x00000000);
  }

  // DATA_BYTE_SEL register configuration (get max Dbyte value)
  wr_reg(DDR_PHY,DDR_MULTIPHY_REGS,DATA_BYTE_SEL,0x1); // (DRAM_DATA_WIDTH/8)-1);  // for 16, configure 1

  // AUTOTRAIN_LOOP_ADDR register configuration
  // cfg.phy_cfg.autotrain_addr         = 2;      // as per Gallardo input
  wr_reg(DDR_PHY,DDR_MULTIPHY_REGS,AUTOTRAIN_LOOP_ADDR,0x2); // cfg.phy_cfg.autotrain_addr);

  // GENERAL_SETUP register configuration
  // cfg.phy_cfg.gen_setup.base_addr    = 0;  // as per Gallardo input
  // cfg.phy_cfg.gen_setup.auto_train   = 1;  //  auto train = 1
  // cfg.phy_cfg.gen_setup.pbmux        = 2;  // playback engine selected during init

  var = wr_fld_r_s(     DDR_PHY, DDR_MULTIPHY_REGS ,GENERAL_SETUP, AUTOTRAIN_EN,     0x1); // cfg.phy_cfg.gen_setup.auto_train);
  var = wr_fld_s_s(var, DDR_PHY, DDR_MULTIPHY_REGS ,GENERAL_SETUP, PB_MUX_SEL,       0x2); // cfg.phy_cfg.gen_setup.pbmux);
  wr_fld_s_r(var, DDR_PHY, DDR_MULTIPHY_REGS ,GENERAL_SETUP, TRAINING_BASE,    0x0); // cfg.phy_cfg.gen_setup.base_addr);

  wr_reg(DDR_PHY,DDR_MULTIPHY_REGS,PLAYBACK_ENABLE,1);     // for PE= init

  // Poll for Playback Running status and training done status
  poll_pbdone(2500);
  poll_trdone(2500);

  // Stop PE and disable AutoTrain
  wr_reg(DDR_PHY,DDR_MULTIPHY_REGS,PLAYBACK_ENABLE,0);     // for PE= init


  // cfg.phy_cfg.gen_setup.auto_train   = 0;
  wr_fld_r_r(DDR_PHY, DDR_MULTIPHY_REGS ,GENERAL_SETUP, AUTOTRAIN_EN, 0x0); // cfg.phy_cfg.gen_setup.auto_train);


  // Enable dfi_init_complete_en to indicate UMCTL2 that training is done
  quasi_write(DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_COMPLETE_EN, 1)

}

static void lan966x_init(void)
{
	register uint32_t var, stat;
	register int timeout;

	debug("STARTING\n");

	// Call and assign to static global member (DDRC, PHY configs are inside cfg)
	// cfg = init_ddr_config();

	// Enable all DDR clocks from CPU reg target
	var = wr_fld_r_s(     CPU, DDRCTRL, DDRCTRL_CLK, DDR_CLK_ENA,         1);
	var = wr_fld_s_s(var, CPU, DDRCTRL, DDRCTRL_CLK, DDR_AXI0_CLK_ENA,    1);
	var = wr_fld_s_s(var, CPU, DDRCTRL, DDRCTRL_CLK, DDR_AXI1_CLK_ENA,    1);
	var = wr_fld_s_s(var, CPU, DDRCTRL, DDRCTRL_CLK, DDR_AXI2_CLK_ENA,    1);
	var = wr_fld_s_s(var, CPU, DDRCTRL, DDRCTRL_CLK, DDR_APB_CLK_ENA,     1);
	var = wr_fld_s_s(var, CPU, DDRCTRL, DDRCTRL_CLK, DDRPHY_CTL_CLK_ENA,  1);
	wr_fld_s_r(var, CPU, DDRCTRL, DDRCTRL_CLK, DDRPHY_APB_CLK_ENA,        1);

	// release DDRC APB reset
	wr_fld_r_r(   CPU, DDRCTRL, DDRCTRL_RST, DDR_APB_RST,           0);

	debug("read ok\n");
	// my_wait(10000,0xA0); // 1000ns, should be >180ns as per function
	pause(1);

	/* config DDR controller */

	debug("starting ddrc_init\n");

	// PWRCTL register configuration
	// var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,PWRCTL,POWERDOWN_EN,0x1);                 // Enables power down mode
	// wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,PWRCTL,SELFREF_EN,0x1);                         // Enables self refresh

	// INIT0 register configuration
	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,INIT0,PRE_CKE_X1024, 0xF); // PRE_CKE_1024);     // Reset deassert to CKE high
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,INIT0,POST_CKE_X1024, 0x1); // POST_CKE_1024);   // CKE high to SDRAM init start
#if INIT_MODE==0
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,INIT0,SKIP_DRAM_INIT,0x0); // dram init by controller
#else
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,INIT0,SKIP_DRAM_INIT,0x1); // cfg.sdram_init_ctrl);    // get value from plusargs, else configure random value
#endif

	// INIT1 register configuration
	wr_fld_r_r(DDR_UMCTL2,UMCTL2_REGS,INIT1,DRAM_RSTN_X1024, 0x6); // DRAM_RSTN_1024);            // Reset assert time during power up

	// INIT3 register configuration
	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,INIT3,MR,0x320); // cfg.mr0_reg);
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,INIT3,EMR,0x0); // cfg.mr1_reg);

	// INIT4 register configuration
	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,INIT4,EMR2,0x8); // cfg.mr2_reg);
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,INIT4,EMR3,0x0); // cfg.mr3_reg);

	// INIT5 register configuration
	wr_fld_r_r(    DDR_UMCTL2,UMCTL2_REGS,INIT5,DEV_ZQINIT_X32, 0x8); // DEV_ZQINIT_32);

	// Updating it after INIT, to program DLL OFF in a sequence
	//sean var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,MSTR,DDR3, 0x1); // MEM_DDR3);                           // select DDR3
	var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,MSTR,DDR3ENA, 0x1); // MEM_DDR3);                           // select DDR3

	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,MSTR,EN_2T_TIMING_MODE,0x0); // cfg.timing_2t_en);
	// var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,MSTR,DLL_OFF_MODE,cfg.dll_off_en);              // dll off enabled for fpga only
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,MSTR,DLL_OFF_MODE,0x1);                           // dll enabled during initialization
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,MSTR,BURST_RDWR,0x4); // cfg.burst_type);                // BL8
	// wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,MSTR,ACTIVE_RANKS,2** RANKS-1);              // '01' for single rank, '11' for dual rank
	// wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,MSTR,DEVICE_CONFIG, DEV_CONFIG);             // any future reference to DDR4 can be enabled from here

	// DRAMTMG0 register configuration
	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,DRAMTMG0,WR2PRE,0x8); // cfg.tmg0.wr2pre);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DRAMTMG0,T_FAW,0x1); // cfg.tmg0.tfaw);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DRAMTMG0,T_RAS_MAX,0x3); // cfg.tmg0.tras_max);
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,DRAMTMG0,T_RAS_MIN,0x2); // cfg.tmg0.tras_min);

	// DRAMTMG1 register configuration
	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,DRAMTMG1,T_XP,0x1); // cfg.tmg1.txp);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DRAMTMG1,RD2PRE,0x4); // cfg.tmg1.rd2pre);
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,DRAMTMG1,T_RC,0x2); // cfg.tmg1.trc);

	// DRAMTMG2 register configuration
	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,DRAMTMG2,RD2WR,0x6); // cfg.tmg2.rd2wr);
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,DRAMTMG2,WR2RD,0x7); // cfg.tmg2.wr2rd);

	// DRAMTMG3 register configuration
	var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DRAMTMG3,T_MOD,0x6); // cfg.tmg3.tmod);
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,DRAMTMG3,T_MRD,0x2); // cfg.tmg3.tmrd);

	// DRAMTMG4 register configuration
	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,DRAMTMG4,T_RCD,0x1); // cfg.tmg4.trcd);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DRAMTMG4,T_CCD,0x2); // cfg.tmg4.tccd);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DRAMTMG4,T_RRD,0x1); // cfg.tmg4.trrd);
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,DRAMTMG4,T_RP,0x1); // cfg.tmg4.trp);

	// DRAMTMG5 register configuration
	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,DRAMTMG5,T_CKSRX,0x1); // cfg.tmg5.tcksrx);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DRAMTMG5,T_CKSRE,0x3); // cfg.tmg5.tcksre);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DRAMTMG5,T_CKESR,0x1); // cfg.tmg5.tckesr);
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,DRAMTMG5,T_CKE,0x1); // cfg.tmg5.tcke);

	// DRAMTMG8 register configuration
	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,DRAMTMG8,T_XS_X32,0x1); // cfg.tmg8.txs);
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,DRAMTMG8,T_XS_DLL_X32,0x8); // cfg.tmg8.txsdll);

	// DFITMG0 register configuration
#ifdef EXB_FPGA
	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_TPHY_WRLAT,  0x2); //TPHY_WRLAT_FPGA);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_TPHY_WRDATA, 0x0); //TPHY_WRDATA_FPGA);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_T_RDDATA_EN, 0x5); //TRDDATA_EN_FPGA);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_T_CTRL_DELAY,0x2); //TCTRL_DELAY_FPGA);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_WRDATA_USE_DFI_PHY_CLK,0x0);   // (default = 0);, set to 0, since FPGA MultiPHY uses same dficlk as MC
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_RDDATA_USE_DFI_PHY_CLK,0x0);         // (default = 0);, set to 0, since FPGA MultiPHY uses same dficlk as MC
#else
	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_TPHY_WRLAT, TPHY_WRLAT);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_TPHY_WRDATA, TPHY_WRDATA);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_T_RDDATA_EN, TRDDATA_EN);
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,DFITMG0,DFI_T_CTRL_DELAY, TCTRL_DELAY);
#endif

	// DFITMG1 register configuration
#ifdef EXB_FPGA
	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,DFITMG1,DFI_T_DRAM_CLK_ENABLE, 0x1); // TDRAM_CLK_ENA_FPGA);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DFITMG1,DFI_T_DRAM_CLK_DISABLE, 0x1); // TDRAM_CLK_DIS_FPGA);
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,DFITMG1,DFI_T_WRDATA_DELAY, 0x5); // TWRDATA_DELAY_FPGA);
#else
	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,DFITMG1,DFI_T_DRAM_CLK_ENABLE, TDRAM_CLK_ENA);
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,DFITMG1,DFI_T_DRAM_CLK_DISABLE, TDRAM_CLK_DIS);
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,DFITMG1,DFI_T_WRDATA_DELAY, TWRDATA_DELAY);
#endif

	// DFITMG2 register configuration (write this register for FPGA only); - commeted out temporarly
	// #ifdef EXB_FPGA
	//   wr_fld_r_r(DDR_UMCTL2,UMCTL2_REGS,DFITMG2,DFI_TPHY_RDCSLAT, TPHY_RDCSLAT_FPGA);  // rddata_cs_n is supported for FPGA
	// #endif

	quasi_write(DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_COMPLETE_EN, 0)

	// DFIUPD0 register configuration
	wr_fld_r_r(DDR_UMCTL2,UMCTL2_REGS,DFIUPD0,DIS_AUTO_CTRLUPD_SRX,0x1); // disable the auto generation of dfi_ctrlupd_req

	// Disabling update request initiated by DDR controller during DDR initialization
	wr_fld_s_r       ( var, DDR_UMCTL2, UMCTL2_REGS, DFIUPD0, DIS_AUTO_CTRLUPD,     1U);

	// DFIUPD1 register configuration
	var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,DFIUPD1,DFI_T_CTRLUPD_INTERVAL_MIN_X1024,0x40); // min internal time
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,DFIUPD1,DFI_T_CTRLUPD_INTERVAL_MAX_X1024,0xFF); // max internal time

	// RFSHCTL0 register configuration
	wr_fld_r_r(DDR_UMCTL2,UMCTL2_REGS,RFSHCTL0,REFRESH_BURST,0x1);   // burst of 2 refresh, for 1x mode, burst of refresh < 8

	// RFSHTMG register configuration
	var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,RFSHTMG,T_RFC_NOM_X1_X32, 0x8); // TREFI_CLK);  // 0x62 (*32); for 1600 Mbps mode
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,RFSHTMG,T_RFC_MIN, 0x8); // TRFC_MIN_CLK);   // 0x68 for 1600 Mbps mode

	// Current:
	// Assumed 2GB with 8 bank (each bank = 128M, row = 16bits, col = 11 bits);, freq_ratio = 2:1
	// Maps HIF[30] => cs, HIF[29:27] = Bank, HIF[26:11] = Row (16bit);, HIF[10:2] = Column
	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,ADDRMAP1,ADDRMAP_BANK_B2,24); // base: 4, HIF[28]
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP1,ADDRMAP_BANK_B1,24); // base: 3, HIF[27]
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP1,ADDRMAP_BANK_B0,24); // base: 2, HIF[26]

	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,ADDRMAP2,ADDRMAP_COL_B5,0);  // base: 5, HIF[5]
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP2,ADDRMAP_COL_B4,0);  // base: 4, HIF[4]
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP2,ADDRMAP_COL_B3,0);  // base: 3, HIF[3]
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP2,ADDRMAP_COL_B2,0);  // base: 2, HIF[2]

	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,ADDRMAP3,ADDRMAP_COL_B9,0); // base: 9, HIF[9]
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP3,ADDRMAP_COL_B8,0); // base: 8, HIF[8]
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP3,ADDRMAP_COL_B7,0); // base: 7, HIF[7]
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP3,ADDRMAP_COL_B6,0); // base: 6, HIF[6]

	var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,ADDRMAP4,ADDRMAP_COL_B11,31); // base: 11, Not used
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP4,ADDRMAP_COL_B10,31);   // HIF[10] (d31); // base: 10, Not used

	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,ADDRMAP5,ADDRMAP_ROW_B0,4);    // base: 6,  HIF[10]
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP5,ADDRMAP_ROW_B1,4);    // base: 7,  HIF[11]
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP5,ADDRMAP_ROW_B2_10,4); // base: 8,  HIF[12..20]
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP5,ADDRMAP_ROW_B11,4);   // base: 17, HIF[21]

	var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,ADDRMAP6,ADDRMAP_ROW_B12,4);   // base: 18,  HIF[22]
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP6,ADDRMAP_ROW_B13,4);   // base: 19,  HIF[23]
	var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP6,ADDRMAP_ROW_B14,4);   // base: 20,  HIF[24]
	wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,ADDRMAP6,ADDRMAP_ROW_B15,4);   // base: 21,  HIF[25]

	debug("after ddr_init\n");

	// release MEM_RST, DDRC core, AXI resets
	wr_fld_r_r(   CPU, CPU_REGS, RESET, MEM_RST, 0);

	var = wr_fld_r_s(CPU, DDRCTRL, DDRCTRL_RST, DDR_AXI0_RST,         0);
	var = wr_fld_s_s(var, CPU, DDRCTRL, DDRCTRL_RST, DDR_AXI1_RST,         0);
	wr_fld_s_r(var, CPU, DDRCTRL, DDRCTRL_RST, DDR_AXI2_RST,         0);
	pause(1); // 1000ns

	// release DDRPHY, DDRPHY APB resets for both FPGA, ASIC
	// var = wr_fld_r_s(     CPU, DDRCTRL, DDRCTRL_RST, DDRPHY_APB_RST,       0); // DDR PHY APB reset
	//sean var = wr_fld_r_s(var, CPU, DDRCTRL, DDRCTRL_RST, DDRPHY_CTL_RST,       0); // PUB/PHY reset, only needed for ASIC
	var = wr_fld_r_s(CPU, DDRCTRL, DDRCTRL_RST, DDRPHY_CTL_RST,       0); // PUB/PHY reset, only needed for ASIC

	wr_fld_s_r(var, CPU, DDRCTRL, DDRCTRL_RST, FPGA_DDRPHY_SOFT_RST, 0);       // reset_n of PHY
	pause(1); // 1000ns

	wr_fld_r_r(CPU, DDRCTRL, DDRCTRL_RST, DDRPHY_APB_RST,       0); // DDR PHY APB reset

	// Initialize PUB (ASIC) or FPGAPHY (for FPGA) registers
	// if (cfg.skip_phyinit == 0) {
	debug("before fpgaphy init\n");
#ifdef EXB_FPGA
	config_ddr_fpgaphy_init_reg();
#else
	config_ddr_phy_init_reg();
#endif
	debug("after fpgaphy init\n");
	// } else {
	//   //printf("DDR_INIT, #### skipping PHY initialization ####");
	// }

	wr_fld_r_r(CPU, DDRCTRL, DDRCTRL_RST, DDRC_RST,             0);

	// Initialize SDRAM either through controller or PUB/Playback Engine
	// If (cfg.skip_draminit == 0) {

	debug("before fpga init\n");
#ifdef EXB_FPGA
	dram_fpga_init();   // DRAM init only (training happens later)
#else
	dram_phypub_init(); // DRAM init by PHY-PUB logic
#endif
	debug("after fpga init\n");
	// } else {
	//printf("DDR_INIT, #### skipping DRAM initialization ####");
	// }

	// poll for operating_mode as normal
	poll_opmode(2500);

	// Keep device in DLL OFF mode (only needed for FPGA case)
#ifdef EXB_FPGA
	// MRCTRL1
	//printf("DDR_INIT, #### Updating MR1 data from controller ####");

	wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, MRCTRL1,MR_DATA, 3);    // RTT_NOM = 0x0

	// MRCTRL0
	var = wr_fld_r_s(   DDR_UMCTL2, UMCTL2_REGS, MRCTRL0,MR_ADDR, 1U);
	wr_fld_s_r(var, DDR_UMCTL2, UMCTL2_REGS, MRCTRL0,MR_WR,1U);      // Triggers mode register write

	// poll for clear of MR_WR_BUSY
	timeout = 1000;
	do {
		pause(1); // #1us;
		stat = rd_fld_r(DDR_UMCTL2, UMCTL2_REGS, MRSTAT, MR_WR_BUSY);
		timeout--;
	} while (stat == 1 && timeout != 0);

	// Enable DLL OFF mode
	quasi_write(DDR_UMCTL2, UMCTL2_REGS, MSTR, DLL_OFF_MODE, 1);
#endif

	// Wait for INIT exit (polling read status of DDRC operating_mode) before moving to next
	// poll_opmode(2500);

	// Disable Power down, self refresh and auto refresh (since FPGA PE may not have idea on either of this during training)
	//printf("DDR_TRAINING, Disable auto refresh, self refresh, power down in the controller");

	wr_fld_r_r(    DDR_UMCTL2, UMCTL2_REGS, RFSHCTL3, DIS_AUTO_REFRESH, 1);

	var = wr_fld_r_s(   DDR_UMCTL2, UMCTL2_REGS, PWRCTL,SELFREF_EN,0);
	wr_fld_s_r(var, DDR_UMCTL2, UMCTL2_REGS, PWRCTL,POWERDOWN_EN,0);


	// Perform training if enabled
	// if (cfg.skip_training == 0) {
	debug("before fpga training\n");
#if SKIP_TRAINING==0
#ifdef EXB_FPGA
	ddr_fpga_training();   // DATA training by FPGA playback engine
#else
	ddr_phypub_training(); // DATA training by PUB
#endif
#endif
	debug("after fpga training\n");
	// } else {
	//   //printf("DDR_INIT, #### skipping DATA training ####");
	// }

	// Re-enable power down, self refresh and auto refresh
	//printf("DDR_TRAINING, Enable back auto refresh, self refresh, power down in the controller");

	wr_fld_r_r(     DDR_UMCTL2, UMCTL2_REGS, RFSHCTL3,DIS_AUTO_REFRESH, 0);

	var = wr_fld_r_s(     DDR_UMCTL2, UMCTL2_REGS, PWRCTL, SELFREF_EN, 0);
	wr_fld_s_r(var, DDR_UMCTL2, UMCTL2_REGS, PWRCTL, POWERDOWN_EN, 0);

	quasi_write(DDR_UMCTL2, UMCTL2_REGS, DFIUPD0, DIS_AUTO_CTRLUPD, 0);


	// Enanle AXI ports
	wr_fld_r_r(    DDR_UMCTL2, UMCTL2_MP, PCTRL_0, PCTRL_0_PORT_EN, 0x1); // cfg.axi_port0_en);
	wr_fld_r_r(    DDR_UMCTL2, UMCTL2_MP, PCTRL_1, PCTRL_1_PORT_EN, 0x1); // cfg.axi_port1_en);
	wr_fld_r_r(    DDR_UMCTL2, UMCTL2_MP, PCTRL_2, PCTRL_2_PORT_EN, 0x1); // cfg.axi_port2_en);

	debug("DDR_INIT, #### Done with the DDR initialization sequence ####\n");
}
#endif /* LAN966X_ASIC */

void lan966x_ddr_init(void)
{
#ifdef LAN966X_ASIC
	wr_fld_r_r (CHIP_TOP, DDR_PLL_CFG, DDR_PLL_CFG, DIVF, params_clk_div);
	DDR_initialization();
	ecc_enable_scrubbing();
#else
	lan966x_init();
#endif
}
