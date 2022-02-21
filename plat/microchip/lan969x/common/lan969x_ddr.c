/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <assert.h>
#include <drivers/delay_timer.h>
#include <lib/mmio.h>

#include "lan969x_regs_sr.h"
#include "lan969x_ddr_sr.h"

#define EXB_FPGA
#define SKIP_TRAINING	0
#define INIT_MODE	1

// defining DDR single addr region
#define VTSS_MASERATI_DDR LAN969X_DDR_BASE

#define debug(x...) VERBOSE(x)
#define error(x...) ERROR(x)

static uint32_t t = LAN969X_DDR_PHY_BASE;

static inline void my_wait(uint32_t t_nsec, const uint16_t msg)
{
	mmio_write_32(CPU_GPR(LAN969X_CPU_BASE, 1), msg);
	debug("Wait start %04x\n", msg);
	if (t_nsec < 1000)
		udelay(1);
	else
		udelay(t_nsec / 1000);
	debug("Wait end   %04x\n", msg);
}

static inline uint32_t reg_read(uintptr_t addr)
{
	uint32_t val = mmio_read_32(addr);
	debug("RD(%08x) => %08x\n", (uint32_t) addr, val);
	return val;
}

static inline void reg_write(uint32_t val, uintptr_t addr)
{
	debug("WR(%08x) <= %08x\n", (uint32_t) addr, val);
	mmio_write_32(addr, val);
}

static inline void wr_reg_rev(uintptr_t addr, uint32_t val)
{
	reg_write(val, addr);
}

// read register
#define rd_reg(tgt,grp,reg) \
	reg_read(tgt##_##reg (LAN969X_##tgt##_BASE))

// write register
#define wr_reg(tgt,grp,reg,val) \
	reg_write(val,tgt##_##reg (LAN969X_##tgt##_BASE))

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

void ddr_sanity(void)
{
  uint32_t *var_ptr;
  uint32_t var_check[4];

  //Test starts here
  // pointer to DDR base address
  var_ptr  = (uint32_t *)(VTSS_MASERATI_DDR);

  // write single word (16bytes) to DDR
  //*var_ptr = 0x0F0E0D0C0B0A09080706050403020100;
  const uint32_t pat[4] = { 0x03020100, 0x07060504, 0x0B0A0908, 0x0F0E0D0C};

  *var_ptr++ = pat[0];
  *var_ptr++ = pat[1];
  *var_ptr++ = pat[2];
  *var_ptr++ = pat[3];

  // wait for data write
  my_wait(1000,0xC0); // 1us

  // Read single word (16bytes) from DDR
  var_ptr  = (uint32_t *)(VTSS_MASERATI_DDR);

  var_check[0]  = *var_ptr++;
  var_check[1]  = *var_ptr++;
  var_check[2]  = *var_ptr++;
  var_check[3]  = *var_ptr++;

  // check for read data correctness
  if (var_check[3] == pat[3] &&
      var_check[2] == pat[2] &&
      var_check[1] == pat[1] &&
      var_check[0] == pat[0]) {
	  NOTICE("DDR INIT PASS\n");
  } else {
	  int i;
	  ERROR("DDR INIT FAILURE\n");
	  for(i = 0; i < 4; i++)
		  ERROR("%d: Expect %08x got %08x\n", i, pat[i], var_check[i]);
  }
}

void lan966x_ddr_init(void)
{
  uint32_t var, stat;
  int32_t timeout;

  var = rd_reg(     CPU, DDRCTRL, DDRCTRL_CLK);

  // Perform DDR init and training
  //var = DDR_initialization();
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
  // my_wait(10000,0xA0); // 1000ns, should be >180ns as per function
  my_wait(295,0xA0); // 1000ns, should be >180ns as per function

  // Configure DDR controller
  // config_ddrc_init_reg();
  // PWRCTL register configuration
  //var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,PWRCTL,POWERDOWN_EN,0x1);                 // Enables power down mode
  //wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,PWRCTL,SELFREF_EN,0x1);                         // Enables self refresh

  // INIT0 register configuration
  var = wr_fld_r_s(     DDR_UMCTL2,UMCTL2_REGS,INIT0,PRE_CKE_X1024, 0xF); // PRE_CKE_1024);     // Reset deassert to CKE high
  var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,INIT0,POST_CKE_X1024, 0x1); // POST_CKE_1024);   // CKE high to SDRAM init start
  #if INIT_MODE==0
  wr_fld_s_r(var, DDR_UMCTL2,UMCTL2_REGS,INIT0,SKIP_DRAM_INIT,0x0); // controller performs dram init
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
  var = wr_fld_r_s(    DDR_UMCTL2,UMCTL2_REGS,MSTR,DDR3ENA, 0x1); // MEM_DDR3);                           // select DDR3
  var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,MSTR,EN_2T_TIMING_MODE,0x0); // cfg.timing_2t_en);
  // var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,MSTR,DLL_OFF_MODE,cfg.dll_off_en);              // dll off enabled for fpga only
  var = wr_fld_s_s(var, DDR_UMCTL2,UMCTL2_REGS,MSTR,DLL_OFF_MODE,0x0);                           // dll enabled during initialization
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

  // Ignoring assertion of dfi_init_complete signal until completion of SDRAM initialization
  // and it also resolves the issue with init_by_pub = 0
  wr_fld_r_r ( DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_COMPLETE_EN, 0);

  // DFIUPD0 register configuration
  //wr_fld_r_r(DDR_UMCTL2,UMCTL2_REGS,DFIUPD0,DIS_AUTO_CTRLUPD_SRX,0x1); // disable the auto generation of dfi_ctrlupd_req

  var = wr_fld_r_s (      DDR_UMCTL2, UMCTL2_REGS, DFIUPD0, DIS_AUTO_CTRLUPD_SRX, 1);
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


  // Configure System Addr region mapping
  wr_fld_r_r(   DDR_UMCTL2,UMCTL2_MP,SARBASE0,BASE_ADDR,(VTSS_MASERATI_DDR)>>29);  // for 512MB block size
  wr_fld_r_r(   DDR_UMCTL2,UMCTL2_MP,SARSIZE0,NBLOCKS,4);                          // 512*4=2GB space allocated


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

  my_wait(1000,0xA1); // 1000ns

  // release MEM_RST, DDRC core, AXI resets
  wr_fld_r_r(   CPU, CPU_REGS, RESET, MEM_RST, 0);

  // var = wr_fld_r_s(     CPU, DDRCTRL, DDRCTRL_RST, DDRC_RST,             0);
  var = wr_fld_r_s(CPU, DDRCTRL, DDRCTRL_RST, DDR_AXI0_RST,         0);
  var = wr_fld_s_s(var, CPU, DDRCTRL, DDRCTRL_RST, DDR_AXI1_RST,         0);
  wr_fld_s_r(var, CPU, DDRCTRL, DDRCTRL_RST, DDR_AXI2_RST,         0);
  my_wait(1000,0xA2); // 1000ns

  // release DDRPHY, DDRPHY APB resets for both FPGA, ASIC
  //var = wr_fld_r_s(     CPU, DDRCTRL, DDRCTRL_RST, DDRPHY_APB_RST,       0); // only needed for ASIC
  var = wr_fld_r_s(CPU, DDRCTRL, DDRCTRL_RST, DDRPHY_CTL_RST,       0); // PUB/PHY reset, only needed for ASIC
  wr_fld_s_r(var, CPU, DDRCTRL, DDRCTRL_RST, FPGA_DDRPHY_SOFT_RST, 0);
  my_wait(1000,0xA3); // 1000ns

  wr_fld_r_r(CPU, DDRCTRL, DDRCTRL_RST, DDRPHY_APB_RST,       0);

   // Initialize PUB (ASIC) or FPGAPHY (for FPGA) registers
   // if (cfg.skip_phyinit == 0) {
   #ifdef EXB_FPGA
     // config_ddr_fpgaphy_init_reg();
     // for(i=0; i<320; i=i+1) {// only iterate depth of a multi dimentional array (to cover all addr)
     //   reg_idx = pe_img_init_ary[i][0];       // fetch addr for converting into reg index
     //   reg_idx = (reg_idx & 0x7FF) >> 2;                  // [10:0] >> 2; converting into byte addr or register index {base:5 bits, offset: 11bits}
     //   wr_reg(DDR_PHY, DDR_PHY_REGS, PLAYBACK_ENGINE_CODE(reg_idx), pe_img_init_ary[i][1]);
     // }

     //(*(volatile uint32_t*)((((0x0e0000000 + 0x00084000))))) -> PHY target map
   #if INIT_MODE!=0

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*0 + 0),  0x000000f0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*0 + 1),  0x000000f0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*0 + 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*0 + 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*0 + 4),  0x00001771); // 6001*33.333 (30MHz phy clock)

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*1 + 0),  0x800000f0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*1 + 1),  0x800000f0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*1 + 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*1 + 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*1 + 4),  0x00003a99); // 15001*33.333 (30Mhz phy clock)

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*2 + 0),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*2 + 1),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*2 + 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*2 + 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*2 + 4),  0x0000001c);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*3 + 0),  0x900008c3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*3 + 1),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*3 + 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*3 + 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*3 + 4),  0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*4 + 0),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*4 + 1),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*4 + 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*4 + 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*4 + 4),  0x00000004);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*5 + 0),  0x980000c3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*5 + 1),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*5 + 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*5 + 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*5 + 4),  0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*6 + 0),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*6 + 1),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*6 + 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*6 + 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*6 + 4),  0x00000004);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*7 + 0),  0x880000c3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*7 + 1),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*7 + 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*7 + 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*7 + 4),  0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*8 + 0),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*8 + 1),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*8 + 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*8 + 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*8 + 4),  0x00000004);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*9 + 0),  0x800320c3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*9 + 1),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*9 + 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*9 + 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*9 + 4),  0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*10+ 0),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*10+ 1),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*10+ 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*10+ 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*10+ 4),  0x0000000c);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*11+ 0),  0x860400c3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*11+ 1),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*11+ 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*11+ 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*11+ 4),  0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*12+ 0),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*12+ 1),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*12+ 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*12+ 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*12+ 4),  0x00000200);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*13+ 0),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*13+ 1),  0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*13+ 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*13+ 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*13+ 4),  0x00080000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*14+ 0),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*14+ 1),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*14+ 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*14+ 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*14+ 4),  0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*15+ 0),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*15+ 1),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*15+ 2),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*15+ 3),  0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*15+ 4),  0x00000000);

     for (int i =16; i < 64; i++) {// only iterate depth of a multi dimentional array (to cover all addr)
       wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*i +0),  0x000000c0);
       wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*i +1),  0x000000c0);
       wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*i +2),  0x000000c0);
       wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*i +3),  0x000000c0);
       wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*i +4),  0x00000000);
     }
     #endif

     // WRITE_PIPE_DELAY register configuration
     var = wr_fld_r_s(   DDR_PHY, DDR_PHY_REGS, WRITE_PIPE_DELAY, WR_PIPE_DELAY, 0x6); // cfg.phy_cfg.wpipe_dly.wp_dly);
     wr_fld_s_r(var, DDR_PHY, DDR_PHY_REGS, WRITE_PIPE_DELAY, MEMCLK_DLY_OFFSET, 0x0); // cfg.phy_cfg.wpipe_dly.effect);


     // CA_PIPE_DELAY register configuration
     wr_reg(DDR_PHY, DDR_PHY_REGS ,CA_PIPE_DELAY,0x1); // cfg.phy_cfg.cp_dly);


     // DATA_BYTE_SEL register configuration (get max Dbyte value)
     wr_reg(DDR_PHY, DDR_PHY_REGS ,DATA_BYTE_SEL, 0x1); // (DRAM_DATA_WIDTH/8)-1);  // for 16, configure 1, rank = 0 only


     // CTRLUPD_CONFIG register configuration
     var = wr_fld_r_s(   DDR_PHY, DDR_PHY_REGS ,CTRLUPD_CONFIG, REQ_TO_ACK_DLY, 0x2); // cfg.phy_cfg.ctrlupd_cfg.t_req_to_ack);
     wr_fld_s_r(var, DDR_PHY, DDR_PHY_REGS ,CTRLUPD_CONFIG, ACK_VLD_CLKS, 0x5); // cfg.phy_cfg.ctrlupd_cfg.t_ack_dassert);


     // INTERRUPT_MASK register configuration
     var = wr_fld_r_s(    DDR_PHY, DDR_PHY_REGS ,INTERRUPT_MASK, PB_INTR_MASK, 0x0); // cfg.phy_cfg.intr_mask.pb_intr);
     wr_fld_s_r(var, DDR_PHY, DDR_PHY_REGS ,INTERRUPT_MASK, DRQ_ERR_INTR_MASK, 0x0); // cfg.phy_cfg.intr_mask.dqs_err_intr);


     // INTERNAL_ENABLE register configuration
     var = wr_fld_r_s(    DDR_PHY, DDR_PHY_REGS ,INTERRUPT_ENABLE, PB_INTR_EN, 0x1); // cfg.phy_cfg.intr_en.pb_intr);
     wr_fld_s_r(var, DDR_PHY, DDR_PHY_REGS ,INTERRUPT_ENABLE, DQS_ERR_INTR_EN, 0x0); // cfg.phy_cfg.intr_en.dqs_err_intr);


     // INTERRUPT_CLEAR register configuration
     wr_reg(DDR_PHY, DDR_PHY_REGS ,INTERRUPT_CLEAR, 0x0); //cfg.phy_cfg.intr_clr);


     // DFICLK_READ_DELAY_RANK_0 register configuration
     var = wr_fld_r_s(     DDR_PHY, DDR_PHY_REGS ,DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_0_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte0);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_1_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte1);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_2_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte2);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_3_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte3);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_4_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte4);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_5_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte5);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_6_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte6);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_7_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte7);
     wr_fld_s_r(var, DDR_PHY, DDR_PHY_REGS, DFICLK_READ_DELAY_RANK_0, RD_EN_VLD_DLY_8_R0, 0x2); // cfg.phy_cfg.dfi_rd_dly_cfg.rd_dly_byte8);


     // FCLK_READ_DELAY_RANK_0 registers configuration (value is 0x6 as per sec 5.3, but assumed it should do +1 inside IP)
     var = wr_fld_r_s(     DDR_PHY, DDR_PHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_0_R0, 0x5); // 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte0);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_1_R0, 0x5); // 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte1);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_2_R0, 0x5); // 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte2);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_3_R0, 0x5); // 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte3);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_4_R0, 0x5); // 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte4);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_5_R0, 0x5); // 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte5);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_6_R0, 0x5); // 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte6);
     wr_fld_s_r(var, DDR_PHY, DDR_PHY_REGS, FCLK_READ_DELAY_RANK_0_BYTES_7_0, RD_DATA_PIPE_7_R0, 0x5);       // 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte7);
     wr_fld_r_r(DDR_PHY, DDR_PHY_REGS, FCLK_READ_DELAY_RANK_0_BYTE_8, RD_DATA_PIPE_8_R0, 0x5);               // 0x6); // cfg.phy_cfg.fclk_rd_dly_cfg.rd_dly_byte8);


     // AUTOTRAIN_LOOP_ADDR register configuration
     // cfg.phy_cfg.autotrain_addr = 0;
     wr_reg(DDR_PHY, DDR_PHY_REGS ,AUTOTRAIN_LOOP_ADDR,0x0); // cfg.phy_cfg.autotrain_addr);


     // GENERAL_SETUP register configuration
     // cfg.phy_cfg.gen_setup.base_addr    = 0;
     // cfg.phy_cfg.gen_setup.auto_train   = 0;   //  auto train = 0
     // cfg.phy_cfg.gen_setup.pbmux        = 2;  // playback engine selected during init

     var = wr_fld_r_s(     DDR_PHY, DDR_PHY_REGS ,GENERAL_SETUP, FORMAT,           0x0); // cfg.phy_cfg.gen_setup.format);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS ,GENERAL_SETUP, PHY_CHANNEL_EN,   0x1); // cfg.phy_cfg.gen_setup.phy_channel_en);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS ,GENERAL_SETUP, WRITE_DQS_ADJUST, 0x1); // cfg.phy_cfg.gen_setup.wdqs_adj);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS ,GENERAL_SETUP, AUTOTRAIN_EN,     0x0); // cfg.phy_cfg.gen_setup.auto_train);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS ,GENERAL_SETUP, PB_MUX_SEL,       0x2); // cfg.phy_cfg.gen_setup.pbmux);
     var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS ,GENERAL_SETUP, TRAINING_BASE,    0x0); // cfg.phy_cfg.gen_setup.base_addr);
     wr_fld_s_r(var, DDR_PHY, DDR_PHY_REGS ,GENERAL_SETUP, CA_BUS_VAL_EDGE,  0x0); // cfg.phy_cfg.gen_setup.ca_valid);

     #else
     config_ddr_phy_init_reg();
     #endif
     // } else {
     //   printf("DDR_INIT, #### skipping PHY initialization ####");
     // }

     wr_fld_r_r ( CPU, DDRCTRL, DDRCTRL_RST, DDRC_RST, 0);
   // Initialize SDRAM either through controller or PUB/Playback Engine
   // If (cfg.skip_draminit == 0) {
     #ifdef EXB_FPGA
     // dram_fpga_init();   // DRAM init only (training happens later)

     #if INIT_MODE!=0
     wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, SWCTL, SW_DONE, 0);

     var = wr_fld_r_s (      DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_START,       1);
     wr_fld_s_r       ( var, DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_COMPLETE_EN, 1);

     wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, SWCTL, SW_DONE, 1);

     timeout = 1000;
     do {
       my_wait(1000, 0XB5);
       stat = rd_fld_r(DDR_UMCTL2, UMCTL2_REGS, SWSTAT, SW_DONE_ACK);
       timeout--;
     } while (stat != 1 && timeout != 0);

     // Poll for Playback Running status through playback done interrupt
     //printf("DRAM_FPGA_INIT, #### Waiting for DRAM init done by PHY ####");
     // poll_pbdone(2500);
     timeout = 2500; // timeout_in;  // assign max timeout expected
     do {
       my_wait(1000,0xB2); // 1us
       stat = rd_fld_r(DDR_PHY, DDR_PHY_REGS, INTERRUPT_STATUS, PB_INTR_STS);
       timeout--;
     } while (stat == 0 && timeout != 0); // break loop when init done = 1 or timeout reached

     // Also poll for playback running status = 0 (ensure that PB is stopped running)
     timeout = 2500; //timeout_in;
     do {
       my_wait(1000,0xB3); // 1US
       stat = rd_fld_r(DDR_PHY, DDR_PHY_REGS, PLAYBACK_STATUS,PLAYBACK_RUNNING);
       timeout--;
     } while (stat == 1 && timeout != 0); // break when running = 0
     #else

     wr_reg(DDR_PHY,DDR_PHY_REGS,PLAYBACK_ENABLE,1);     // dummy playback enable
     wr_reg(DDR_PHY,DDR_PHY_REGS,PLAYBACK_ENABLE,0);

     #endif

     #else
     dram_phypub_init(); // DRAM init by PHY-PUB logic
     #endif
   // } else {
     //printf("DDR_INIT, #### skipping DRAM initialization ####");
   // }

   // poll_opmode(2500);
   // Assert DFI_INIT_COMPLETE_EN to '1', if not set previously
   stat = rd_fld_r(DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_COMPLETE_EN);

   if (stat == 0) {
     wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, SWCTL, SW_DONE, 0);
     wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_COMPLETE_EN, 1);
     wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, SWCTL, SW_DONE, 1);

     timeout = 1000;
     do {
       my_wait(1000, 0XB5);
       stat = rd_fld_r(DDR_UMCTL2, UMCTL2_REGS, SWSTAT, SW_DONE_ACK);
       timeout--;
     } while (stat != 1 && timeout != 0);
   }

   timeout = 2500; // timeout_in;
   do {
     my_wait(1000, 0XB6); // #1us;
     stat = rd_fld_r(DDR_UMCTL2, UMCTL2_REGS, STAT,OPERATING_MODE);
     timeout--;
   } while (stat != 1 && timeout != 0);



   // Keep device in DLL OFF mode (only needed for FPGA case)
   #ifdef EXB_FPGA
     // MRCTRL1
     //printf("DDR_INIT, #### Updating MR1 data from controller ####");

     wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, MRCTRL1,MR_DATA, 3);

     // MRCTRL0
     var = wr_fld_r_s(   DDR_UMCTL2, UMCTL2_REGS, MRCTRL0,MR_ADDR, 1);
     wr_fld_s_r(var, DDR_UMCTL2, UMCTL2_REGS, MRCTRL0,MR_WR,1U);      // Triggers mode register write

     // poll for clear of MR_WR_BUSY
     timeout = 1000;
     do {
       my_wait(1000, 0XA4); // #1us;
       stat = rd_fld_r(DDR_UMCTL2, UMCTL2_REGS, MRSTAT, MR_WR_BUSY);
       timeout--;
     } while (stat == 1 && timeout != 0);

     // Enable DLL OFF mode
     wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, SWCTL, SW_DONE, 0);
     wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, MSTR, DLL_OFF_MODE, 1); // keeping controller in dll off mode
     wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, SWCTL, SW_DONE, 1);

     timeout = 1000;
     do {
       my_wait(1000, 0XB5);
       stat = rd_fld_r(DDR_UMCTL2, UMCTL2_REGS, SWSTAT, SW_DONE_ACK);
       timeout--;
     } while (stat != 1 && timeout != 0);

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
     #ifdef EXB_FPGA
     // ddr_fpga_training();   // DATA training by FPGA playback engine

     #if SKIP_TRAINING==0

     // Disable dfi_init_complete_en (so UMCTL2 waits for training to complete)
     // quasi_write(DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_COMPLETE_EN, 0)
     wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, SWCTL, SW_DONE, 0);
     wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_COMPLETE_EN, 0);
     wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, SWCTL, SW_DONE, 1);

     timeout = 1000;
     do {
       my_wait(1000, 0XB5);
       stat = rd_fld_r(DDR_UMCTL2, UMCTL2_REGS, SWSTAT, SW_DONE_ACK);
       timeout--;
     } while (stat != 1 && timeout != 0);

     // Loading PLAYBACK_ENGINE_CODE (or PE image) into RAM using indirect register
     //printf("DDR_FPGAPHY_CFG, Loading PE image into Playback RAM");
     // for(i=0; i<320; i=i+1) {                      // only iterate depth of a multi dimentional array (to cover all addr)
     //   reg_idx = pe_img_train_ary[i][0];       // fetch addr for converting into reg index
     //   reg_idx = (reg_idx & 0x7FF) >> 2;                   // [10:0] >> 2; converting into byte addr or register index {base:5 bits, offset: 11bits}
     //   wr_reg(DDR_PHY, DDR_PHY_REGS, PLAYBACK_ENGINE_CODE(reg_idx), pe_img_train_ary[i][1]);
     // }

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*0 + 0), 0x8b0000c3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*0 + 1), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*0 + 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*0 + 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*0 + 4), 0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*1 + 0), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*1 + 1), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*1 + 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*1 + 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*1 + 4), 0x00000002);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*2 + 0), 0x8c0000e3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*2 + 1), 0x8c0000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*2 + 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*2 + 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*2 + 4), 0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*3 + 0), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*3 + 1), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*3 + 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*3 + 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*3 + 4), 0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*4 + 0), 0x8c0008e3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*4 + 1), 0x8c0008f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*4 + 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*4 + 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*4 + 4), 0x00040000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*5 + 0), 0x8c0000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*5 + 1), 0x8c0000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*5 + 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*5 + 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*5 + 4), 0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*6 + 0), 0x8c0010e3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*6 + 1), 0x8c0010f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*6 + 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*6 + 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*6 + 4), 0x00050000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*7 + 0), 0x8c0008f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*7 + 1), 0x8c0008f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*7 + 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*7 + 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*7 + 4), 0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*8 + 0), 0x880010f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*8 + 1), 0x880010f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*8 + 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*8 + 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*8 + 4), 0x00040000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*9 + 0), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*9 + 1), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*9 + 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*9 + 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*9 + 4), 0x00000003);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*10+ 0), 0x8d0000e3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*10+ 1), 0x8d0000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*10+ 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*10+ 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*10+ 4), 0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*11+ 0), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*11+ 1), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*11+ 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*11+ 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*11+ 4), 0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*12+ 0), 0x8d0008e3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*12+ 1), 0x8d0008f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*12+ 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*12+ 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*12+ 4), 0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*13+ 0), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*13+ 1), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*13+ 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*13+ 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*13+ 4), 0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*14+ 0), 0x8d0010e3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*14+ 1), 0x8d0010f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*14+ 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*14+ 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*14+ 4), 0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*15+ 0), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*15+ 1), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*15+ 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*15+ 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*15+ 4), 0x00000001);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*16+ 0), 0x8d0008f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*16+ 1), 0x800008f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*16+ 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*16+ 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*16+ 4), 0x00030000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*17+ 0), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*17+ 1), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*17+ 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*17+ 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*17+ 4), 0x00000004);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*18+ 0), 0x8a0000c3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*18+ 1), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*18+ 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*18+ 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*18+ 4), 0x00000000);

     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*19+ 0), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*19+ 1), 0x800000f3);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*19+ 2), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*19+ 3), 0x000000c0);
     wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*19+ 4), 0x00080000);

     for(int i = 20; i < 64; i++) {// only iterate depth of a multi dimentional array (to cover all addr)
       wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*i + 0), 0x000000c0);
       wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*i + 1), 0x000000c0);
       wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*i + 2), 0x000000c0);
       wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*i + 3), 0x000000c0);
       wr_reg_rev(DDR_PHY_PLAYBACK_ENGINE_CODE(t, 8*i + 4), 0x00000000);
     }

  // DATA_BYTE_SEL register configuration (get max Dbyte value)
  wr_reg(DDR_PHY,DDR_PHY_REGS,DATA_BYTE_SEL,0x1); // (DRAM_DATA_WIDTH/8)-1);  // for 16, configure 1

  // AUTOTRAIN_LOOP_ADDR register configuration
  // cfg.phy_cfg.autotrain_addr         = 2;      // as per Gallardo input
  wr_reg(DDR_PHY,DDR_PHY_REGS,AUTOTRAIN_LOOP_ADDR,0x2); // cfg.phy_cfg.autotrain_addr);

  // GENERAL_SETUP register configuration
  // cfg.phy_cfg.gen_setup.base_addr    = 0;  // as per Gallardo input
  // cfg.phy_cfg.gen_setup.auto_train   = 1;  //  auto train = 1
  // cfg.phy_cfg.gen_setup.pbmux        = 2;  // playback engine selected during init

  var = wr_fld_r_s(     DDR_PHY, DDR_PHY_REGS ,GENERAL_SETUP, AUTOTRAIN_EN,     0x1); // cfg.phy_cfg.gen_setup.auto_train);
  var = wr_fld_s_s(var, DDR_PHY, DDR_PHY_REGS ,GENERAL_SETUP, PB_MUX_SEL,       0x2); // cfg.phy_cfg.gen_setup.pbmux);
  wr_fld_s_r(var, DDR_PHY, DDR_PHY_REGS ,GENERAL_SETUP, TRAINING_BASE,    0x0); // cfg.phy_cfg.gen_setup.base_addr);

  wr_reg(DDR_PHY,DDR_PHY_REGS,PLAYBACK_ENABLE,1);     // for PE= init

     // Poll for Playback Running status and training done status
     // poll_pbdone(2500);
     timeout = 2500; // timeout_in;  // assign max timeout expected
     do {
       my_wait(1000,0xB2); // 1us
       stat = rd_fld_r(DDR_PHY, DDR_PHY_REGS, INTERRUPT_STATUS,PB_INTR_STS);
       timeout--;
     } while (stat == 0 && timeout != 0); // break loop when init done = 1 or timeout reached

     // Also poll for playback running status = 0 (ensure that PB is stopped running)
     timeout = 2500; //timeout_in;
     do {
       my_wait(1000,0xB3); // 1US
       stat = rd_fld_r(DDR_PHY, DDR_PHY_REGS, PLAYBACK_STATUS,PLAYBACK_RUNNING);
       timeout--;
     } while (stat == 1 && timeout != 0); // break when running = 0

     // poll_trdone(2500);
     timeout = 2500; // timeout_in;  // assign max timeout expected
     do {
       my_wait(1000,0xB4); // 1us
       stat = rd_reg(DDR_PHY, DDR_PHY_REGS, TRAIN_STATUS);
       timeout--;
     } while (stat != 1 && timeout != 0);

     // Stop PE and disable AutoTrain
     wr_reg(DDR_PHY,DDR_PHY_REGS,PLAYBACK_ENABLE,0);     // for PE= init

     // cfg.phy_cfg.gen_setup.auto_train   = 0;
     wr_fld_r_r(DDR_PHY, DDR_PHY_REGS ,GENERAL_SETUP, AUTOTRAIN_EN, 0x0); // cfg.phy_cfg.gen_setup.auto_train);

     // Enable dfi_init_complete_en to indicate UMCTL2 that training is done
     // quasi_write(DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_COMPLETE_EN, 1)
     wr_fld_r_r( DDR_UMCTL2, UMCTL2_REGS, SWCTL, SW_DONE, 0);
     wr_fld_r_r( DDR_UMCTL2, UMCTL2_REGS, DFIMISC, DFI_INIT_COMPLETE_EN, 1 );
     wr_fld_r_r( DDR_UMCTL2, UMCTL2_REGS, DFIUPD0, DIS_AUTO_CTRLUPD,     0 );
     wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, SWCTL, SW_DONE, 1);

     timeout = 1000;
     do {
       my_wait(1000, 0XB5);
       stat = rd_fld_r(DDR_UMCTL2, UMCTL2_REGS, SWSTAT, SW_DONE_ACK);
       timeout--;
     } while (stat != 1 && timeout != 0);

     #endif

     #else
     ddr_phypub_training(); // DATA training by PUB
     #endif
   // } else {
   //   //printf("DDR_INIT, #### skipping DATA training ####");
   // }

   // Re-enable power down, self refresh and auto refresh
   //printf("DDR_TRAINING, Enable back auto refresh, self refresh, power down in the controller");

   wr_fld_r_r(     DDR_UMCTL2, UMCTL2_REGS, RFSHCTL3,DIS_AUTO_REFRESH, 0);

   var = wr_fld_r_s(     DDR_UMCTL2, UMCTL2_REGS, PWRCTL, SELFREF_EN, 0);
   wr_fld_s_r(var, DDR_UMCTL2, UMCTL2_REGS, PWRCTL, POWERDOWN_EN, 0);


   // Enanle AXI ports
   wr_fld_r_r(    DDR_UMCTL2, UMCTL2_MP, PCTRL_0, PCTRL_0_PORT_EN, 0x1); // cfg.axi_port0_en);
   wr_fld_r_r(    DDR_UMCTL2, UMCTL2_MP, PCTRL_1, PCTRL_1_PORT_EN, 0x1); // cfg.axi_port1_en);
   wr_fld_r_r(    DDR_UMCTL2, UMCTL2_MP, PCTRL_2, PCTRL_2_PORT_EN, 0x1); // cfg.axi_port2_en);

   my_wait(1000,0xC4); // 1us

   ddr_sanity();
}
