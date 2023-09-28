/*
 * Copyright (c) 2020 Silex Insight sa
 * Copyright (c) 2020 Beerten Engineering
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "../../src/debug.h"
#include <silexpk/core.h>
#include "../hw/ba414/regs_addr.h"
#include "../hw/ba414/ba414_status.h"
#include <internal.h>

#define PK_BUSY_MASK             0x00010000

#ifdef SX_PK_WAIT_ON_IRQ
extern void SX_PK_WAIT_ON_IRQ(sx_pk_req *req);
#endif

int sx_pk_wait(sx_pk_req *req)
{
   uint32_t status = PK_BUSY_MASK;

   while (status & PK_BUSY_MASK) {
      /* CUSTOMIZATION: write custom wait on ba414ep interrupt here, or define SX_PK_WAIT_ON_IRQ */
#ifdef SX_PK_WAIT_ON_IRQ
      SX_PK_WAIT_ON_IRQ(req);
#endif
      status = sx_pk_rdreg(&req->regs, PK_REG_STATUS);
   }
   // clear interrupt
   sx_pk_wrreg(&req->regs, PK_REG_CONTROL, PK_RB_CONTROL_CLEAR_IRQ);

   return convert_ba414_status(status);
}
