
// #define DDRC_REGS         top_reg_blk_inst.top_ddr_umctl2.GLOB_DDR_UMCTL2
// #define DDR_PHY_REGS      top_reg_blk_inst.top_ddr_32phy.GLOB_DDR_PHY
// #define DDR_FPGAPHY_REGS  top_reg_blk_inst.top_ddr_multiphy.GLOB_DDR_MULTIPHY
#define DDRC_REGS         DDR_CTL
#define DDR_PHY_REGS      DDR_PHY
#define DDR_FPGAPHY_REGS  DDR_PHY

#ifdef HOST_MODE
  #define QVIP_ACTIVE      // QVIP as controller 
#else 
  #define QVIP_PASSIVE     // QVIP as passive slave 
#endif 

#define DDR_QVIP_ALIVE    0    // Indicates whether QVIP to be enabled

#define DLL_OFF_ENABLE
#define DDR_MODEL_AVAIL   1    

#ifdef DDR_MODE0            // single rank, 2x8, DDR3, 1600Mbps
  #define SINGLE_RANK       
  #define MEM_DDR3    1     // Mem type = DDR3
  #define x8                // defines no of DQ pins to DRAM
  #define TWO_X8            // defines memory module configuration (or arrangment)  
  #define DDR3_8G_1600      // defines Mem type and speed grade 
  #define DRAM_SIZE   2048  // 2048MB (2GBytes)
  #define den8192Mb         // density of each DDR device = 8Gb 
  #define sg125
  #define DDR_MODE    0    
  #define NDDR        2     // no of DDR chips per one module/rank 
#elif defined DDR_MODE1            // single rank, 2x8, DDR3, 1866Mbps
  #define SINGLE_RANK       
  #define MEM_DDR3    1     // Mem type = DDR3
  #define x8                // defines no of DQ pins to DRAM
  #define TWO_X8            // defines memory module configuration (or arrangment)  
  #define DDR3_8G_1866      // defines Mem type and speed grade 
  #define DRAM_SIZE   2048  // 2048MB (2GBytes)
  #define den8192Mb         // density of each DDR device = 8Gb 
  #define sg107             
  #define DDR_MODE    1     
  #define NDDR        2     // no of DDR chips per one module/rank 
#elif defined DDR_MODE2            // single rank, 1x16, DDR3, 1600Mbps 
  #define SINGLE_RANK       
  #define MEM_DDR3    1     // Mem type = DDR3
  #define x16               // defines no of DQ pins to DRAM
  #define ONE_X16           // defines memory module configuration (or arrangment)  
  #define DDR3_8G_1600      // defines Mem type and speed grade 
  #define DRAM_SIZE   2048  // 2048MB (2GBytes)
  #define den16384Mb        // density of each DDR device = 16Gb 
  #define sg125              
  #define DDR_MODE    2     
  #define NDDR        1     // no of DDR chips per one module/rank 
#elif defined DDR_MODE3           


#else                       // default DDR_MODE0
  #define SINGLE_RANK       
  #define MEM_DDR3    1     // Mem type = DDR3
  #define x8                // defines no of DQ pins to DRAM
  #define TWO_X8            // defines memory module configuration (or arrangment)  
  #define DDR3_8G_1600      // defines Mem type and speed grade 
  #define DRAM_SIZE   2048  // 2048MB (2GBytes)
  #define den8192Mb         // density of each DDR device = 8Gb 
  #define sg125      
  #define DDR_MODE    0     
  #define NDDR        2     // no of DDR chips per one module/rank 
#endif


// global defines
#ifdef EXB_FPGA
  #define APB_CLK_PERIOD      33.333 // in ns, 30MHz
  #define CPU_AXI_CLK_PERIOD  33.333 // in ns, 30MHz
#else 
  #define APB_CLK_PERIOD      4 // in ns, 250MHz
  #define CPU_AXI_CLK_PERIOD  4 // in ns, 250MHz
#endif
#define NCLKS_1US           ceil(1000.0/CPU_AXI_CLK_PERIOD)  // 1us/clk period
#define NCLKS_10US          ceil(10000.0/CPU_AXI_CLK_PERIOD) // 10us/clk period


// Only Used by FPGA
#define PE_RAM_SIZE      64*8   // 512 {64 blocks of 8 each} 
#define PE_UNUSED_SIZE   64*3   // 3 unused out of 8
#define PE_USED_SIZE     (PE_RAM_SIZE-PE_UNUSED_SIZE)   // 320 


// Address map specific paramters 
#define RANK_RANGE     30:30 // Logical addr's Rank bits
#define BA_RANGE       29:27 // Logical addr's Bank bits  
#define ROW_RANGE      26:11 // Logical addr's Row bits 
#define COL_RANGE      10:2  // Logical addr's Column bits

// #define DDRC_REGS         top_reg_blk_inst.top_ddr_umctl2.GLOB_DDR_UMCTL2
// #define DDR_PHY_REGS      top_reg_blk_inst.top_ddr_32phy.GLOB_DDR_PHY
// #define DDR_FPGAPHY_REGS  top_reg_blk_inst.top_ddr_multiphy.GLOB_DDR_MULTIPHY
#define DDRC_REGS         DDR_CTL
#define DDR_PHY_REGS      DDR_PHY
#define DDR_FPGAPHY_REGS  DDR_PHY

#ifdef HOST_MODE
  #define QVIP_ACTIVE      // QVIP as controller 
#else 
  #define QVIP_PASSIVE     // QVIP as passive slave 
#endif 

#define DDR_QVIP_ALIVE    0    // Indicates whether QVIP to be enabled

#define DLL_OFF_ENABLE
#define DDR_MODEL_AVAIL   1    

#ifdef DDR_MODE0            // single rank, 2x8, DDR3, 1600Mbps
  #define SINGLE_RANK       
  #define MEM_DDR3    1     // Mem type = DDR3
  #define x8                // defines no of DQ pins to DRAM
  #define TWO_X8            // defines memory module configuration (or arrangment)  
  #define DDR3_8G_1600      // defines Mem type and speed grade 
  #define DRAM_SIZE   2048  // 2048MB (2GBytes)
  #define den8192Mb         // density of each DDR device = 8Gb 
  #define sg125
  #define DDR_MODE    0    
  #define NDDR        2     // no of DDR chips per one module/rank 
#elif defined DDR_MODE1            // single rank, 2x8, DDR3, 1866Mbps
  #define SINGLE_RANK       
  #define MEM_DDR3    1     // Mem type = DDR3
  #define x8                // defines no of DQ pins to DRAM
  #define TWO_X8            // defines memory module configuration (or arrangment)  
  #define DDR3_8G_1866      // defines Mem type and speed grade 
  #define DRAM_SIZE   2048  // 2048MB (2GBytes)
  #define den8192Mb         // density of each DDR device = 8Gb 
  #define sg107             
  #define DDR_MODE    1     
  #define NDDR        2     // no of DDR chips per one module/rank 
#elif defined DDR_MODE2            // single rank, 1x16, DDR3, 1600Mbps 
  #define SINGLE_RANK       
  #define MEM_DDR3    1     // Mem type = DDR3
  #define x16               // defines no of DQ pins to DRAM
  #define ONE_X16           // defines memory module configuration (or arrangment)  
  #define DDR3_8G_1600      // defines Mem type and speed grade 
  #define DRAM_SIZE   2048  // 2048MB (2GBytes)
  #define den16384Mb        // density of each DDR device = 16Gb 
  #define sg125              
  #define DDR_MODE    2     
  #define NDDR        1     // no of DDR chips per one module/rank 
#elif defined DDR_MODE3           


#else                       // default DDR_MODE0
  #define SINGLE_RANK       
  #define MEM_DDR3    1     // Mem type = DDR3
  #define x8                // defines no of DQ pins to DRAM
  #define TWO_X8            // defines memory module configuration (or arrangment)  
  #define DDR3_8G_1600      // defines Mem type and speed grade 
  #define DRAM_SIZE   2048  // 2048MB (2GBytes)
  #define den8192Mb         // density of each DDR device = 8Gb 
  #define sg125      
  #define DDR_MODE    0     
  #define NDDR        2     // no of DDR chips per one module/rank 
#endif


// global defines
#ifdef EXB_FPGA
  #define APB_CLK_PERIOD      33.333 // in ns, 30MHz
  #define CPU_AXI_CLK_PERIOD  33.333 // in ns, 30MHz
#else 
  #define APB_CLK_PERIOD      4 // in ns, 250MHz
  #define CPU_AXI_CLK_PERIOD  4 // in ns, 250MHz
#endif
#define NCLKS_1US           ceil(1000.0/CPU_AXI_CLK_PERIOD)  // 1us/clk period
#define NCLKS_10US          ceil(10000.0/CPU_AXI_CLK_PERIOD) // 10us/clk period


// Only Used by FPGA
#define PE_RAM_SIZE      64*8   // 512 {64 blocks of 8 each} 
#define PE_UNUSED_SIZE   64*3   // 3 unused out of 8
#define PE_USED_SIZE     (PE_RAM_SIZE-PE_UNUSED_SIZE)   // 320 


// Address map specific paramters 
#define RANK_RANGE     30:30 // Logical addr's Rank bits
#define BA_RANGE       29:27 // Logical addr's Bank bits  
#define ROW_RANGE      26:11 // Logical addr's Row bits 
#define COL_RANGE      10:2  // Logical addr's Column bits
