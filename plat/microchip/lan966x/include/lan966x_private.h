/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LAN966X_PRIVATE_H
#define LAN966X_PRIVATE_H

#include <common/bl_common.h>


void lan966x_console_init(void);
void lan966x_io_setup(void);


enum lan966x_target {
    TARGET_CPU = 6,
    TARGET_DDR_PHY = 9,
    TARGET_DDR_UMCTL2 = 10,
    TARGET_FLEXCOM = 20,
    TARGET_GCB = 25,
    TARGET_GIC400 = 26,
    TARGET_HMATRIX2 = 29,
    TARGET_TIMERS = 53,
    NUM_TARGETS = 65
};


#endif /* LAN966X_PRIVATE_H */
