#include "ddr_defines.h"

#define __ceil(x) (uint32_t)(ceil(x))   // needed for C only, since ceil can't be used directly in binary operations

#ifdef DDR3_8G_1600
  #include "ddr3_8Gb_1600_timing_parameters.h"
#elif defined DDR3_8G_1866
  #include <ddr3_8Gb_1866_timing_parameters.h>
#elif defined DDR3_4G_1600
  #include <ddr3_4Gb_1866_timing_parameters.h>
#endif
