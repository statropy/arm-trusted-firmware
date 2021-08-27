// Timing paramters for all DDR3 speed bins 
//                               //      7-7-7  5-5-5   9-9-9  11-11-11  13-13-13  14-14-14            

#define SPD_BIN         1600     //    800,     1066,     1333,     1600,     1866,     2133
#define __TCK_MIN     1250.0     // 2500.0,   1875.0,   1500.0,   1250.0,   1071.0,    938.0  // ps,  Min Clock cycle time 
#define TJIT_PER          70     //    100,       90,       80,       70,       60,       50  // ps,  Period jitter
#define TJIT_CC          140     //    200,      180,      160,      140,      120,      100  // ps,  Cycle to Cycle jitter
#define TERR_2PER        103     //    147,      132,      118,      103,       88,       74  // ps,  Accumulated Error (2-cycle)
#define TERR_3PER        122     //    175,      157,      140,      122,      105,       87  // ps,  Accumulated Error (3-cycle)
#define TERR_4PER        136     //    194,      175,      155,      136,      117,       97  // ps,  Accumulated Error (4-cycle)
#define TERR_5PER        147     //    209,      188,      168,      147,      126,      105  // ps,  Accumulated Error (5-cycle)
#define TERR_6PER        155     //    222,      200,      177,      155,      133,      111  // ps,  Accumulated Error (6-cycle)
#define TERR_7PER        163     //    232,      209,      186,      163,      139,      116  // ps,  Accumulated Error (7-cycle)
#define TERR_8PER        169     //    241,      217,      193,      169,      145,      121  // ps,  Accumulated Error (8-cycle)
#define TERR_9PER        175     //    249,      224,      200,      175,      150,      125  // ps,  Accumulated Error (9-cycle)
#define TERR_10PER       180     //    257,      231,      205,      180,      154,      128  // ps,  Accumulated Error (10-cycle)
#define TERR_11PER       184     //    263,      237,      210,      184,      158,      132  // ps,  Accumulated Error (11-cycle)
#define TERR_12PER       188     //    269,      242,      215,      188,      161,      134  // ps,  Accumulated Error (12-cycle)
#define TDS               10     //    125,       75,       30,       10,       10,        5  // ps,  DQ and DM input setup time relative to DQS 
#define TDH               45     //    150,      100,       65,       45,       20,       20  // ps,  DQ and DM input hold time relative to DQS
#define TDQSQ            100     //    200,      150,      125,      100,       80,       70  // ps,  DQS-DQ skew, DQS to last DQ valid  
#define TDQSS           0.27     //   0.25,     0.25,     0.25,     0.27,     0.27,     0.27  // tCK, DQS rising to CK rising edge
#define TDSS            0.18     //   0.20,     0.20,     0.20,     0.18,     0.18,     0.18  // tCK, DQS falling to CK rising edge (setup time)
#define TDSH            0.18     //   0.20,     0.20,     0.20,     0.18,     0.18,     0.18  // tCK, DQS falling from CK rising edge (hold time)
#define TDQSCK           225     //    400,      300,      255,      225,      200,      180  // ps,  DQS output access time from CK/CK#
#define TQSH            0.40     //   0.38,     0.38,     0.40,     0.40,     0.40,     0.40  // tCK, DQS Output High Pulse Width
#define TQSL            0.40     //   0.38,     0.38,     0.40,     0.40,     0.40,     0.40  // tCK, DQS Output Low Pulse Width
#define TDIPW            360     //    600,      490,      400,      360,      320,      280  // ps,  DQ and DM input Pulse Width
#define TIPW             560     //    900,      780,      620,      560,      535,      470  // ps,  Control and Address input Pulse Width  
#define TIS              170     //    350,      275,      190,      170,       50,       35  // ps,  Input Setup Time
#define TIH              120     //    275,      200,      140,      120,      100,       75  // ps,  Input Hold Time
#define TRAS_MIN       35000     //  37500,    37500,    36000,    35000,    34000,    33000  // ps,  Minimum Active to Precharge command time
#define TRC            48750     //  50000,    50625,    49500,    48750,    47910,    46090  // ps,  Active to Active/Auto Refresh command time
#define TRCD           13750     //  12500,    13125,    13500,    13750,    13910,    13090  // ps,  Active to Read/Write command time
#define TRP            13750     //  12500,    13125,    13500,    13750,    13910,    13090  // ps,  Precharge command period
#define TXP             6000     //   7500,     7500,     6000,     6000,     6000,     6000  // ps,  Exit power down to a valid command
#define TCKE            5000     //   7500,     5625,     5625,     5000,     5000,     5000  // ps,  CKE minimum high or low pulse width
#define TAON             250     //    400,      300,      250,      250,      200,      180  // ps,  RTT turn-on from ODTLon reference
#define TWLS             165     //    325,      245,      195,      165,      140,      122  // ps,  Setup time for tDQS flop (write leveling)
#define TWLH             165     //    325,      245,      195,      165,      140,      122  // ps,  Hold time of tDQS flop (write leveling)
#define TWLO            7500     //   9000,     9000,     9000,     7500,     7500,     7500  // ps,  Write levelization output delay
#define TAA_MIN        13750     //  12500,    13125,    13500,    13750,    13910,    13090  // ps,  Internal READ command to first data
#define CL_TIME        13750     //  12500,    13125,    13500,    13750,    13910,    13090  // ps,  Minimum CAS Latency
#define CL                11     //      5,        7,        9,       11,       13,       14  // tck, Minimum CAS Latency
#define CWL                8     //      5,        6,        7,        8,        9,       10  // tck, Minimum CWL Latency

#define TDQSCK_DLLDIS    225     //    400,      300,      255,      225,      200,      180  // ps,  for DLLDIS mode, timing not guaranteed

#ifdef X16 
#define TRRD       7500          //  10000,    10000,     7500,     7500,     6000,     6000  // ps, (2KB page size) Active bank a to Active bank b command time 
#define TFAW      40000          //  50000,    50000,    45000,    40000,    35000,    35000  // ps, (2KB page size) Four bank Activate window
#else // x4, x8        
#define TRRD       6000          //  10000,     7500,     6000,     6000,     5000,     5000  // ps, (1KB page size) Active bank a to Active bank b command time 
#define TFAW      30000          //  40000,    37500,    30000,    30000,    25000,    25000  // ps, (1KB page size) Four bank Activate window
#endif


// Mode register 
#define CL_MIN            5 // CL         tCK   Minimum CAS Latency
#define CL_MAX           14 // CL         tCK   Maximum CAS Latency
#define AL_MIN            0 // AL         tCK   Minimum Additive Latency
#define AL_MAX            2 // AL         tCK   Maximum Additive Latency
#define WR_MIN            5 // WR         tCK   Minimum Write Recovery
#define WR_MAX           16 // WR         tCK   Maximum Write Recovery
#define BL_MIN            4 // BL         tCK   Minimum Burst Length
#define BL_MAX            8 // BL         tCK   Minimum Burst Length
#define CWL_MIN           5 // CWL        tCK   Minimum CAS Write Latency
#define CWL_MAX          10 // CWL        tCK   Maximum CAS Write Latency

// Clock
#define __TCK_MAX      3300 // tCK        ps    Maximum Clock Cycle Time
#define TCH_AVG_MIN    0.47 // tCH        tCK   Minimum Clock High-Level Pulse Width
#define TCL_AVG_MIN    0.47 // tCL        tCK   Minimum Clock Low-Level Pulse Width
#define TCH_AVG_MAX    0.53 // tCH        tCK   Maximum Clock High-Level Pulse Width
#define TCL_AVG_MAX    0.53 // tCL        tCK   Maximum Clock Low-Level Pulse Width
#define TCH_ABS_MIN    0.43 // tCH        tCK   Minimum Clock High-Level Pulse Width
#define TCL_ABS_MIN    0.43 // tCL        tCK   Maximum Clock Low-Level Pulse Width
#define TCKE_TCK          3 // tCKE       tCK   CKE minimum high or low pulse width
#define TAA_MAX       20000 // TAA        ps    Internal READ command to first data

#define FREQ_RATIO        2 // 2:1 (keep 1 for 1:1)

// Override TCK for FPGA case, since the required speeds are scaled down
#ifdef EXB_FPGA
  // #define TCK_MIN    8000     // 8ns for DLL OFF mode  
  // #define TCK_MIN    18761.0     // 18.761 ns (53.3 MHz, Core DDRC clk frequency = 26.65 MHz)
  // #define TCK_MAX    18761.0     // > 8ns for DLL OFF mode 
  #define TCK_MIN    16666.7        // 16.666 ns (60MHz, Core DDRC frequency = 30.0 MHz)
  #define TCK_MAX    16666.7
#else 
  #define TCK_MIN    __TCK_MIN
  #define TCK_MAX    __TCK_MAX
#endif

#define T_FASK_CLK   ((TCK_MIN/2)/1000)  // ns 

// Data OUT
#define TQH           0.38 // tQH        ps    DQ output hold time from DQS, DQS#

// Data Strobe OUT
#define TRPRE         0.90 // tRPRE      tCK   DQS Read Preamble
#define TRPST         0.30 // tRPST      tCK   DQS Read Postamble

// Data Strobe IN
#define TDQSH         0.45 // tDQSH      tCK   DQS input High Pulse Width
#define TDQSL         0.45 // tDQSL      tCK   DQS input Low Pulse Width
#define TWPRE         0.90 // tWPRE      tCK   DQS Write Preamble
#define TWPST         0.30 // tWPST      tCK   DQS Write Postamble

// Command and Address
// #define TZQCS;                   // tZQCS      tCK   ZQ Cal (Short) time
#define TZQINIT      640000           // tZQinit    ns   ZQ Cal (Long) time
#define TZQINIT_TCK  (TCK_MIN < 1500.0)? __ceil(TZQINIT/TCK_MIN) : 512  //  tCK (512nCK for lower speeds (>1500 ps))
#define TZQINIT_CLK  __ceil(1.0*(TZQINIT_TCK/FREQ_RATIO))

// #define TZQOPER;                 // tZQoper    tCK   ZQ Cal (Long) time
#define TCCD              4 // tCCD       tCK   Cas to Cas command delay
#define TCCD_DG           2 // tCCD_DG    tCK   Cas to Cas command delay to different group
#define TRAS_MAX       60e9 // tRAS       ps    Maximum Active to Precharge command time
#define TWR           15000 // tWR        ps    Write recovery time
#define TWR_TCK       __ceil(TWR/TCK_MIN)  // in tCK (__ceil may give invalid value for 1071 speed)
#define TMRD              4 // tMRD       tCK   Load Mode Register command cycle time
#define TMOD          15000 // tMOD       ps    LOAD MODE to non-LOAD MODE command cycle time
#define TMOD_TCK         12 // tMOD       tCK   LOAD MODE to non-LOAD MODE command cycle time
#define TRRD_TCK          4 // tRRD       tCK   Active bank a to Active bank b command time
#define TRRD_DG        3000 // tRRD_DG    ps    Active bank a to Active bank b command time to different group
#define TRRD_DG_TCK       2 // tRRD_DG    tCK   Active bank a to Active bank b command time to different group
#define TRTP           7500 // tRTP       ps    Read to Precharge command delay
#define TRTP_TCK          4 // tRTP       tCK   Read to Precharge command delay
#define TWTR           7500 // tWTR       ps    Write to Read command delay
#define TWTR_DG        3750 // tWTR_DG    ps    Write to Read command delay to different group
#define TWTR_TCK          4 // tWTR       tCK   Write to Read command delay
#define TWTR_DG_TCK       2 // tWTR_DG    tCK   Write to Read command delay to different group
#define TDLLK           512 // tDLLK      tCK   DLL locking time

// Refresh - 4Gb
#define TRFC_MIN     260000 // tRFC       ps    Refresh to Refresh Command interval minimum value
#define TRFC_MAX   70200000 // tRFC       ps    Refresh to Refresh Command Interval maximum value (max 9*tRFCI (7.8us))
#define TREFI       7800000 // tREFI      ps    Average refresh internal (7.8us for all density 0<Tcase<85 degrees)
#define TRFC_MIN_CLK __ceil((TRFC_MIN/TCK_MIN)/FREQ_RATIO)   // ctl_clks (104 ctl_clks for 1600Mbps)

// Power Down
#define TXP_CLK      __ceil((TXP/TCK_MIN)/FREQ_RATIO) // tXP, CLK  Exit power down to a valid command
#define TXPDLL        24000 // tXPDLL     ps    Exit precharge power down to READ or WRITE command (DLL-off mode)
#define TXPDLL_TCK   __ceil(TXPDLL/TCK_MIN)     // 10 // tXPDLL     tCK   Exit precharge power down to READ or WRITE command (DLL-off mode)
#define TACTPDEN          1 // tACTPDEN   tCK   Timing of last ACT command to power down entry
#define TPRPDEN           1 // tPREPDEN   tCK   Timing of last PRE command to power down entry
#define TREFPDEN          1 // tARPDEN    tCK   Timing of last REFRESH command to power down entry
#define TCPDED            1 // tCPDED     tCK   Command pass disable/enable delay
#define TPD_MAX   TRFC_MAX // tPD        ps    Power-down entry-to-exit timing

// Self Refresh
#define TXS          270000 // tXS        ps    Exit self refesh to a non-read or write command (max (5*TCK_MIN, TRFC_MIN+10 ns))
#define TXS_TCK           5 // tXS        tCK   Exit self refesh to a non-read or write command
#define TXSDLL       TDLLK // tXSRD      tCK   Exit self refresh to a read or write command
#define TISXR          TIS // tISXR      ps    CKE setup time during self refresh exit.
#define TCKSRE        10000 // tCKSRE     ps    Valid Clock requirement after self refresh entry (SRE)
#define TCKSRE_TCK        5 // tCKSRE     tCK   Valid Clock requirement after self refresh entry (SRE)
#define TCKSRX        10000 // tCKSRX     ps    Valid Clock requirement prior to self refresh exit (SRX)
#define TCKSRX_TCK        5 // tCKSRX     tCK   Valid Clock requirement prior to self refresh exit (SRX)
#define TCKESR_TCK        4 // tCKESR     tCK   Minimum CKE low width for Self Refresh entry to exit timing

#define TXPR         270000 // tXPR       ps    Exit Reset from CKE assertion to a valid command (max (5*TCK_MIN,TXS)) 
//#define TXPR_TCK          5 // tXPR       tCK   Exit Reset from CKE assertion to a valid command
#define TXPR_TCK     __ceil(TXPR/TCK_MIN) // tCK

// ODT
#define TAOF            0.7 // tAOF       tCK   RTT turn-off from ODTLoff reference
#define TAONPD         8500 // tAONPD     ps    Asynchronous RTT turn-on delay (Power-Down with DLL frozen)
#define TAOFPD         8500 // tAONPD     ps    Asynchronous RTT turn-off delay (Power-Down with DLL frozen)
#define ODTH4             4 // ODTH4      tCK   ODT minimum HIGH time after ODT assertion or write (BL4)
#define ODTH8             6 // ODTH8      tCK   ODT minimum HIGH time after write (BL8)
#define TADC            0.7 // tADC       tCK   RTT dynamic change skew

// Write Levelization
#define TWLMRD           40 // tWLMRD     tCK   First DQS pulse rising edge after tDQSS margining mode is programmed
#define TWLDQSEN         25 // tWLDQSEN   tCK   DQS/DQS delay after tDQSS margining mode is programmed
#define TWLOE          2000 // tWLOE      ps    Write levelization output error
#define TWLO_TCK       __ceil(TWLO/TCK_MIN) // tCK 

// Size Parameters based on Part Width
#ifdef x4
#define DM_BITS          1 // Set this parameter to control how many Data Mask bits are used
#define ADDR_BITS       16 // MAX Address Bits
#define ROW_BITS        16 // Set this parameter to control how many Address bits are used
#define COL_BITS        14 // Set this parameter to control how many Column bits are used
#define DQ_BITS          4 // Set this parameter to control how many Data bits are used       **Same as part bit width**
#define DQS_BITS         1 // Set this parameter to control how many Dqs bits are used
#define DEV_CONFIG       0 // used for controller MSTR config
#elif defined x8
#define DM_BITS          1 // Set this parameter to control how many Data Mask bits are used
#define ADDR_BITS       16 // MAX Address Bits
#define ROW_BITS        16 // Set this parameter to control how many Address bits are used
#define COL_BITS        11 // Set this parameter to control how many Column bits are used
#define DQ_BITS          8 // Set this parameter to control how many Data bits are used       **Same as part bit width**
#define DQS_BITS         1 // Set this parameter to control how many Dqs bits are used
#define DEV_CONFIG       1 // used for controller MSTR config
#elif defined x16 
#define DM_BITS          2 // Set this parameter to control how many Data Mask bits are used
#define ADDR_BITS       16 // MAX Address Bits
#define ROW_BITS        16 // Set this parameter to control how many Address bits are used
#define COL_BITS        10 // Set this parameter to control how many Column bits are used
#define DQ_BITS         16 // Set this parameter to control how many Data bits are used       **Same as part bit width**
#define DQS_BITS         2 // Set this parameter to control how many Dqs bits are used
#define DEV_CONFIG       2 // used for controller MSTR config
#else // X32
#define DEV_CONFIG       3 // used for controller MSTR config
#endif

// Size Parameters
#define BA_BITS          3 // Set this parmaeter to control how many Bank Address bits are used
#define MEM_BITS        10 // Set this parameter to control how many write data bursts can be stored in memory.  The default is 2^10=1024.
#define AP              10 // (A10) the address bit that controls auto-precharge and precharge-all
#define BC              12 // (A12) the address bit that controls burst chop
#define BL_BITS          3 // the number of bits required to count to BL_MAX
#define BO_BITS          2 // the number of Burst Order Bits

#ifdef QUAD_RANK
#define CS_BITS         4 // Number of Chip Select Bits
#define RANKS           4 // Number of Chip Selects
#elif defined DUAL_RANK
#define CS_BITS         2 // Number of Chip Select Bits
#define RANKS           2 // Number of Chip Selects
#else
#define CS_BITS         1 // Number of Chip Select Bits
#define RANKS           1 // Number of Chip Selects
#endif

// Simulation parameters
#define RZQ                240 // termination resistance
#define PRE_DEF_PAT       0xAA // value returned during mpr pre-defined pattern readout
#define STOP_ON_ERROR        1 // If set to 1, the model will halt on command sequence/major errors
#define BUS_DELAY            0 // delay in nanoseconds
#define RANDOM_OUT_DELAY     0 // If set to 1, the model will put a random amount of delay on DQ/DQS during reads
#define RANDOM_SEED      31913 //seed value for random generator.

#define RDQSEN_PRE           2 // DQS driving time prior to first read strobe
#define RDQSEN_PST           1 // DQS driving time after last read strobe
#define RDQS_PRE             2 // DQS low time prior to first read strobe
#define RDQS_PST             1 // DQS low time after last read strobe
#define RDQEN_PRE            0 // DQ/DM driving time prior to first read data
#define RDQEN_PST            0 // DQ/DM driving time after last read data
#define WDQS_PRE             2 // DQS half clock periods prior to first write strobe
#define WDQS_PST             1 // DQS half clock periods after last write strobe


#define DFI_CLOCK_PERIOD_PS  (TCK_MIN*FREQ_RATIO)        // in ps
#define DFI_UNIT_1024        (DFI_CLOCK_PERIOD_PS*1024)   // 1024 DFI clocks, 2560 ns for 1600Mbps
// #define DFI_UNIT_1024        DFI_CLOCK_PERIOD_PS*1024  // 1024 DFI clocks, 2560 ns for 1600Mbps
#define DFI_UNIT_32          (DFI_CLOCK_PERIOD_PS*32)     // 32 DFI clocks 

// Derived paramters
#define PRE_CKE_1024         __ceil(1.0*(500000000.0/DFI_UNIT_1024))         // 196 for 1600Mbps
#define POST_CKE_1024        __ceil(1.0*(TXPR/DFI_UNIT_1024))              // 1 or 0.105 for 1600Mbps
#define DRAM_RSTN_1024       __ceil(1.0*(200000000.0/DFI_UNIT_1024))        // 79 for 1600Mbps

#define PRE_CKE_1024_SIM     30                                             // TCK clocks (reduced for simulation purpose)
#define DRAM_RSTN_1024_SIM   12                                             // TCK clocks (reduced for simulation purpose)

#define DEV_ZQINIT_32        __ceil(1.0*(TZQINIT_CLK/32)) 

#define TFAW_CLK             __ceil((TFAW/TCK_MIN)/FREQ_RATIO)           //0x10 for 1600Mbps  
#define TRAS_MIN_CLK         __ceil((TRAS_MIN/TCK_MIN)/FREQ_RATIO)       //0xE for 1600Mbps
#define TRAS_MAX_CLK         __ceil((TRFC_MAX/DFI_UNIT_1024))             //0x1C for 1600Mbps

// #define TXP_CLK
#define TRTP_CLK             4 // max( __ceil((TRTP/TCK_MIN)/FREQ_RATIO),4)      
#define RD2PRE               AL_MIN + TRTP_CLK                           // ~4 for all cases
#define TRC_CLK              __ceil((TRC/TCK_MIN)/FREQ_RATIO)             // 20 ctl_clks for 1600Mbps

#define TMOD_CLK             (TCK_MIN < 1500.0)? __ceil((TMOD/TCK_MIN)/FREQ_RATIO) : __ceil(TMOD_TCK/FREQ_RATIO)  // use tck for >= 1500 ps
#define TMRD_CLK             __ceil(TMRD/FREQ_RATIO)                      // 2 ctl_clks (MRS to next MRS command)

#define TRRD_CLK             __ceil((TRRD/TCK_MIN)/FREQ_RATIO)           // 3 for 1600Mbps  (act between bank 'a' to bank 'b')      
#define TRP_CLK              __ceil((TRP/TCK_MIN)/FREQ_RATIO)            // 6 for 1600Mbps  (pre to act for same bank)
#define TCCD_CLK             __ceil(TCCD/FREQ_RATIO)                      // 2 ctl_clks      (min time bw two reads or two writes)
#define TRCD_CLK             __ceil((TRCD/TCK_MIN)/FREQ_RATIO)           // 6 for 1600Mbps  (act to cas of same bank) 

#define TCKE_CLK             __ceil((TCKE/TCK_MIN)/FREQ_RATIO)           // 2 ctl_clks (min low or high width) 
#define TCKESR_CLK           __ceil(((TCKE/TCK_MIN)+1)/FREQ_RATIO)       // 3 ctl_clks (min low width when exiting from self refresh) 
#define TCKSRE_CLK           (TCK_MIN >= 2500)? 3 : __ceil((TCKSRE/TCK_MIN)/FREQ_RATIO)  // 4 for 1600Mbps (min valid CK after self refresh down)
#define TCKSRX_CLK           __ceil((TCKSRX/TCK_MIN)/FREQ_RATIO)         // 4 for 1600Mbps (min valid CK before self refresh exit)


#define TXS_CLK              __ceil(TXS/DFI_UNIT_32)                      // ~4 (216 tck clocks for 1600Mbps)
#define TDLLK_CLK            __ceil(TDLLK*TCK_MIN/DFI_UNIT_32)           // (wait for 512 TCK clocks for DLL lock after self refresh exit) 

#define TWL                  AL_MIN + CWL
#define TRL                  AL_MIN + CL

// DFI related timing paramters (FPGA paramters taken from section 5.3, FPGA DDR MultiPHY databook, ver2.0)
#define TPHY_WRLAT       __ceil(((TWL%2==0)? TWL-4 : TWL-3)/FREQ_RATIO)   // diff for even and odd, ASIC case (PUB3/2)  
#define TPHY_WRLAT_FPGA  2                                                  // here dfi phy clk freq = dfi clk freq (should be 4 otherwise)

#define TRDDATA_EN       __ceil(((TRL%2==0)? TRL-4 : TRL-3)/FREQ_RATIO)   // diff for even and odd, ASIC case (PUB3/2)  
#define TRDDATA_EN_FPGA  5                                                  

#define TPHY_WRDATA        1
#define TPHY_WRDATA_FPGA   0

#define TCTRL_DELAY        2
#define TCTRL_DELAY_FPGA   2

#define TDRAM_CLK_ENA      1
#define TDRAM_CLK_ENA_FPGA 1 

#define TDRAM_CLK_DIS      2
#define TDRAM_CLK_DIS_FPGA 1

#define TWRDATA_DELAY      3
#define TWRDATA_DELAY_FPGA 5

#define TPHY_RDCSLAT_FPGA       TRDDATA_EN_FPGA

#define TREFI_CLK  __ceil(TREFI/DFI_UNIT_32)      // 0x62 or 98 for 1600Mbps 
#define TREFI_TCK  __ceil(TREFI/TCK_MIN)          // TREFI in TCK clocks


#define __WR_PIPE_DELAY    6 // 6 Fast Clocks (only for FPGA) -> changing the name since same name was there in fld also (macro expanded with 6) 
#define CA_PIPE_DELAY    1 // 1 Fast Clocks (only for FPGA)

#ifdef ONE_X16
  #define DRAM_DATA_WIDTH 16

#elif defined TWO_X8
  #define DRAM_DATA_WIDTH 16

#elif defined NO_MEMORY_MODEL
  #define DRAM_DATA_WIDTH 0 

#endif 
