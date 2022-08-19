/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2018 Microsemi Corporation
 */

#define _DDR3_1R_2X8_8GB_SG15E 0
#define _DDR3_SVB_VAL_BOARD    1
#define _DDR3_EVB_VAL_BOARD    2

#define CONFIG_TARGET_MASERATI_REAL
#define CONFIG_DDR_MASERATI_CLK_DIV 23
#define CONFIG_DDR_MASERATI_EVB

#if defined(CONFIG_TARGET_MASERATI_REAL)
#define _DDR_DUMMY 0
#define params_data_training 1
#endif

#if defined (CONFIG_DDR_MASERATI_SVB)
#define _MODELNAME _DDR3_SVB_VAL_BOARD
#elif defined (CONFIG_DDR_MASERATI_EVB)
#define _MODELNAME _DDR3_EVB_VAL_BOARD
#endif

#ifndef _MODELNAME
#define _MODELNAME _DDR3_1R_2X8_8GB_SG15E
#endif

#define DDR3 3
#define DDR4 4

#define params_PHY_init   1
#define params_DRAM_init  1
#define params_init_by_pub   1
#define params_dq_bits_used 16

#define DDR_BASE_ADDR 0x60000000

#if   _MODELNAME == _DDR3_1R_2X8_8GB_SG15E
#include "ddr3_micron_8Gb_x8_sg15E.h"
#define DDR_MEM_SIZE 0x80000000
#elif _MODELNAME == _DDR3_SVB_VAL_BOARD
#include "ddr3_svb_board_config.h"
#define DDR_MEM_SIZE 0x80000000
#elif _MODELNAME == _DDR3_EVB_VAL_BOARD
#include "ddr3_evb_board_config.h"
#define DDR_MEM_SIZE 0x40000000
#endif


#ifndef params_data_training
#error Need data_training define
#endif

#ifndef params_init_by_pub
#error Need init_by_pub define
#endif
