// Base DDR register access
#ifndef EXB_FPGA // for ASIC PHY 

// Task to poll for PHY init done
__inline static void poll_idone (int32_t timeout_in)
{
int32_t timeout;      
uint8_t stat; 

  timeout = timeout_in;    // assign max timeout expected 
  do {
    udelay(1); // 1us 
    stat = rd_fld_r(DDR_PHY, DDR_MULTIPHY_REGS, PGSR0,IDONE); 
    timeout--;
  } while (stat == 0 && timeout != 0); // break loop when init done = 1 or timeout reached 

  if (stat==1) { 
    //puts("PUB_PHY_CFG, Init done is set to 1\r\n");
  } else {
    puts("ERROR:PUB_PHY_CFG, Timeout reached while waiting for Init done\r\n");
  }
}


// Task to poll for SDRAM init done 
__inline static void poll_draminit(int32_t timeout_in) {
int32_t timeout;      
uint8_t stat; 

  timeout = timeout_in;  // assign max timeout expected 
  do {
    udelay(1); // 1us 
    stat = rd_fld_r(DDR_PHY, DDR_MULTIPHY_REGS, PGSR0, DIDONE); 
    timeout--;
  } while (stat == 0 && timeout != 0); // break loop when init done = 1 or timeout reached 

  if (stat==1) { 
    //puts("PUB_PHY_CFG, DRAM init done is set to 1\r\n");
  } else {
    puts("ERROR:PUB_PHY_CFG, Timeout reached while waiting for DRAM init done\r\n");
  }
}

#else // for FPGA PHY 

// Task to poll for Playback done interrupt 
__inline static void poll_pbdone(int32_t timeout_in)
{
	int32_t timeout;
	uint8_t stat; 

	timeout = timeout_in;  // assign max timeout expected 
	do {
		udelay(1); // 1us 
		stat = rd_fld_r(DDR_PHY, DDR_MULTIPHY_REGS, INTERRUPT_STATUS,PB_INTR_STS); 
		timeout--;
	} while (stat == 0 && timeout != 0); // break loop when init done = 1 or timeout reached 

	if (stat != 1) { 
		puts("ERROR:DDR_FPGAPHY_CFG, Timeout reached while waiting for playback done\r\n");
	}

	// Also poll for playback running status = 0 (ensure that PB is stopped running)
	timeout = timeout_in;
	do {
		udelay(1); // 1US
		stat = rd_fld_r(DDR_PHY, DDR_MULTIPHY_REGS, PLAYBACK_STATUS,PLAYBACK_RUNNING); 
		timeout--; 
	} while (stat == 1 && timeout != 0); // break when running = 0

	if (stat) {
		puts("ERROR:DDR_FPGAPHY_CFG, Playback running status is still not set to 0\r\n");
	}
}


// Task to poll for Training completion status 
__inline static void poll_trdone(int32_t timeout_in)
{
	int32_t timeout=0; 
	uint32_t stat;
	uint32_t offset;

	timeout = timeout_in;  // assign max timeout expected 
	do {
	udelay(1); // 1us 
	stat = rd_reg(DDR_PHY, DDR_MULTIPHY_REGS, TRAIN_STATUS); 
	timeout--;
	} while (stat != 1 && timeout != 0);

	if (stat != 1) {
		if (timeout == 0) { 
		puts("ERROR:DDR_FPGAPHY_CFG, Timeout reached while waiting for Autotraining to succeed\r\n");
		}

		stat = (stat & 0x4) >> 2;  // extract bit 2

		//if (stat[2]) begin
		if (stat == 1) {
		puts("ERROR:DDR_FPGAPHY_CFG, Training is failed and reading back offset registers\r\n");
		offset = rd_reg(DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_OFFSET_RANK_0_BYTES_7_0); // only [7:4] = byte1, [3:0] = byte0 is valid
		//puts("ERROR:DDR_FPGAPHY_CFG, FCLK_READ_OFFSET_RANK_0_BYTES_7_0 = %x\r\n",offset);
		rd_reg(DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_OFFSET_RANK_0_BYTES_7_0); // only [7:4] = byte1, [3:0] = byte0 is valid

		offset = rd_reg(DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_OFFSET_RANK_0_BYTE_8);  // not valid
		//puts("ERROR:DDR_FPGAPHY_CFG, FCLK_READ_OFFSET_RANK_0_BYTE_8 = %x\r\n",offset);
		rd_reg(DDR_PHY, DDR_MULTIPHY_REGS, FCLK_READ_OFFSET_RANK_0_BYTE_8);  // not valid
		}
	}
}

#endif

// Uncomment below once printf linking available
// // Define to enable and write quasi dynamic group registers in DDRC 
#define quasi_write(tgt, grp, reg, fld, value) \
	wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, SWCTL, SW_DONE, 0); \
	wr_fld_r_r(tgt, grp, reg, fld, value); \
	wr_fld_r_r(DDR_UMCTL2, UMCTL2_REGS, SWCTL, SW_DONE, 1); \
	timeout = 1000; \
	do { \
		udelay(1); \
		stat = rd_fld_r(DDR_UMCTL2, UMCTL2_REGS, SWSTAT, SW_DONE_ACK); \
		timeout--; \
	} while (stat != 1 && timeout != 0); \
	if (stat == 1) {\
		timeout = 0; \
	} else { \
		timeout = 0; \
		puts("ERROR:DDRC_CFG, Timeout reached while waiting for SW DONE ACK\r\n"); \
	}


// Task to poll for operating_mode status 
__inline static void poll_opmode(int32_t timeout_in)
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
		udelay(1); // #1us; 
		stat = rd_fld_r(DDR_UMCTL2, UMCTL2_REGS, STAT,OPERATING_MODE); 
		timeout--;
	} while (stat != 1 && timeout != 0);

	if (stat == 1) { 
		puts("DDRC_CFG, STAT.operating_mode is set to normal\r\n");
	} else {
		puts("ERROR:DDRC_CFG, Timeout reached while waiting for operating mode as normal\r\n");
	}
}
