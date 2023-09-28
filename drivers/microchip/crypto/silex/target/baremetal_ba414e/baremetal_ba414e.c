/*
 * Copyright (c) 2014-2021 Silex Insight sa
 * Copyright (c) 2018-2021 Beerten Engineering
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>

#include "../../src/debug.h"
#include "../hw/ba414/regs_addr.h"
#include <silexpk/core.h>
#include "../hw/ba414/pkhardware_ba414e.h"
#include <silexpk/statuscodes.h>
#include "../hw/ba414/ba414_status.h"
#include <internal.h>

#include <platform_def.h>

#define ADDR_BA414EP_REGS_BASE		LAN969X_SILEX_REGS_BASE
#define ADDR_BA414EP_CRYPTORAM_BASE	LAN969X_SILEX_RAM_BASE

#define NUM_PK_INST 1

#ifndef ADDR_BA414EP_REGS_BASE
#define ADDR_BA414EP_REGS_BASE (0xA4002000)
#endif
#ifndef ADDR_BA414EP_CRYPTORAM_BASE
#define ADDR_BA414EP_CRYPTORAM_BASE (0xA4008000)
#endif

#define ADDR_BA414EP_REGS(instance)      ((char*)(ADDR_BA414EP_REGS_BASE) + 0x10000 * (instance))
#define ADDR_BA414EP_CRYPTORAM(instance) ((char*)ADDR_BA414EP_CRYPTORAM_BASE + 0x10000 * (instance))

/** Ucode memory address */
#ifndef SX_PK_MICROCODE_ADDRESS
#define SX_PK_MICROCODE_ADDRESS  0x50008000
#endif
#ifndef NULL
#define NULL (void*)0
#endif

struct sx_pk_cnx {
   struct sx_pk_req instances[NUM_PK_INST];
   struct sx_pk_capabilities caps;
   struct sx_pk_blinder *b;
};

#if !defined(SX_CNX_IN_USR_MEM)
static struct sx_pk_cnx engine;
#endif

void sx_pk_wrreg(struct sx_regs *regs, uint32_t addr, uint32_t v)
{
   *(volatile uint32_t*)(regs->base + addr) = v;
}

uint32_t sx_pk_rdreg(struct sx_regs *regs, uint32_t addr)
{
   uint32_t v;

   v = *(volatile uint32_t*)(regs->base + addr);
   return v;
}

struct sx_pk_blinder** sx_pk_get_blinder(struct sx_pk_cnx *cnx)
{
   return &cnx->b;
}


struct sx_pk_constraints sx_pk_list_constraints(int index,
   size_t usermemsz)
{
   (void) usermemsz;
   struct sx_pk_constraints c = { 0 };

   if (index)
      return c;
#if defined(SX_CNX_IN_USR_MEM)
   if (usermemsz < sizeof(struct sx_pk_cnx))
      return c;
#endif
   c.maxpending = NUM_PK_INST;

   return c;
}


const struct sx_pk_capabilities *sx_pk_fetch_capabilities(struct sx_pk_cnx *cnx)
{
   uint32_t ba414epfeatures = 0;

   if (cnx->caps.maxpending)
      /* capabilities already fetched from hardware. Return immediately. */
      return &cnx->caps;

   ba414epfeatures = sx_pk_rdreg(&cnx->instances[0].regs, PK_REG_HWCONFIG);
   convert_ba414_capabilities(ba414epfeatures, &cnx->caps);
   cnx->caps.maxpending = NUM_PK_INST;

   return &cnx->caps;
}

/* converts max operand size to the ahbmaster max opsize configuration value */
static int max_op_sz_to_max_op_ahb_sz(int op_sz_bytes)
{
   switch (op_sz_bytes)
   {
   case 32:
      return 256;
   case 128:
      return 521;
   case 256:
      return 2048;
   case 384:
      return 3072;
   case 512:
      return 4096;
   default:
      return 4096;
   }
}

static int get_max_op_sz(size_t data_sz) {
   if (data_sz < 2176) {
      return 0;
   }
   if (data_sz < 4096) {
      return 0x20;
   }
   if (data_sz < 8192 ) {
      return 0x80;
   }
   if (data_sz < 12288 ) {
      return 0x100;
   }
   if (data_sz < 16384 ) {
      return 0x180;
   } else {
      return 0x200;
   }
}

struct sx_pk_cnx *sx_pk_open(struct sx_pk_config *cfg)
{
   struct sx_pk_cnx *cnx;

#if defined(SX_CNX_IN_USR_MEM)
   if (!cfg || !cfg->usrmem || cfg->usrmemsz < sizeof(struct sx_pk_cnx))
      return NULL;
   cnx = (struct sx_pk_cnx *)cfg->usrmem;
#else
   cnx = &engine;
#endif

   /* Zero initialise blinder */
   cnx->b = NULL;

   /* CUSTOMIZATION
    *
    * Write custom minimal initialization code here !
    * This includes at least for each accelerator :
    *  1. Register space.
    *  2. Accelerator CryptoRAM address.
    *
    * Example with the ADDR_BA414EP_REGS and ADDR_BA414EP_CRYPTORAM macros :
    */
   for (int i = 0; i < NUM_PK_INST; i++) {
      cnx->instances[i].regs.base = ADDR_BA414EP_REGS(i);
      cnx->instances[i].cryptoram = ADDR_BA414EP_CRYPTORAM(i);
   }

   /* Verify if the HWConfig register is readable and meaningful */
   for (int i = 0; i < NUM_PK_INST; i++) {
      uint32_t ba414epfeatures = 0;
      uint32_t hwconfig = sx_pk_rdreg(&cnx->instances[i].regs, PK_REG_HWCONFIG);
      if (hwconfig == 0) {
         SX_ERRPRINT("Error during register access to BA414E (HwConfigReg is null)\n");
         return NULL;
      }
      if (!ba414epfeatures)
         ba414epfeatures = hwconfig;
      if (ba414epfeatures != hwconfig) {
         SX_ERRPRINT("Error accelerators have different hw configurations\n");
         return NULL;
      }
   }

   if (!sx_pk_fetch_capabilities(cnx))
       return NULL;
   if (cfg && cfg->maxpending > cnx->caps.maxpending) {
      return NULL;
   }

   /** If cryptoRAM is sized for 4096b slots or less, use 4096b slots */
   int op_offset = 0x200;
   if (cnx->caps.max_gfp_opsz > 0x200) {
      /** If cryptoRAM is sized for 8192b slots use 8192b slots */
      op_offset = 0x400;
   }
   for (int i=0; i<NUM_PK_INST; i++) {
      cnx->instances[i].slot_sz = op_offset;
   }

   uint32_t ba414epfeatures = sx_pk_rdreg(&cnx->instances[0].regs, PK_REG_HWCONFIG);
   if ((ba414epfeatures >> 25) & 0x1) {
#ifndef SX_CNX_IN_USR_MEM
      return NULL;
#endif
      /** AHBMaster supports only 1 instance of BA414 **/
      if (NUM_PK_INST != 1) {
         return NULL;
      }

      op_offset = get_max_op_sz(cfg->usrmemsz - sizeof(struct sx_pk_cnx));
      if (!op_offset) { /* not enough memory provided */
         return NULL;
      }
      cnx->instances[0].slot_sz = op_offset;
      // When op_offset = 0x80, the maximal operand size is 66 bytes!
      int max_op_sz = op_offset;
      if (max_op_sz == 0x80) {
         max_op_sz = 66;
      }
      cnx->caps.max_gfp_opsz = (cnx->caps.max_gfp_opsz > max_op_sz) ? max_op_sz : cnx->caps.max_gfp_opsz;
      cnx->caps.max_ecc_opsz = (cnx->caps.max_ecc_opsz > max_op_sz) ? max_op_sz : cnx->caps.max_ecc_opsz;
      cnx->caps.max_gfb_opsz = (cnx->caps.max_gfb_opsz > max_op_sz) ? max_op_sz : cnx->caps.max_gfb_opsz;

      char *mem = (char *) (cfg->usrmem);
      cnx->instances[0].cryptoram = (char *) (mem + sizeof(struct sx_pk_cnx));
      if (((intptr_t) cnx->instances[0].cryptoram) & 0x3) { /* Memory should be 32-bit aligned */
         return NULL;
      }
      sx_pk_wrreg(&cnx->instances[0].regs, PK_REG_OPSIZE, max_op_sz_to_max_op_ahb_sz(op_offset));
      sx_pk_wrreg(&cnx->instances[0].regs, PK_REG_MICROCODEOFFSET, SX_PK_MICROCODE_ADDRESS);
      sx_pk_wrreg(&cnx->instances[0].regs, PK_REG_MEMOFFSET, ((uintptr_t) cnx->instances[0].cryptoram));
   }

   /* CUSTOMIZATION: if extra initialization needed,
    * write custom initialization code here.
    */
   return cnx;
}


void sx_pk_close(struct sx_pk_cnx *cnx)
{
   (void) cnx;
   /* CUSTOMIZATION: write custom clean up code here. */
}


sx_pk_req *sx_pk_pop_finished_req(struct sx_pk_cnx *cnx)
{
   unsigned int i;
   for (i = 0; i < NUM_PK_INST; i++) {
      if (cnx->instances[i].cmd
          && sx_pk_get_status(&cnx->instances[i]) != SX_ERR_BUSY) {
         return &cnx->instances[i];
      }
   }

   return NULL;
}


struct sx_pk_acq_req sx_pk_acquire_req(struct sx_pk_cnx *cnx,
const struct sx_pk_cmd_def *cmd)
{
   struct sx_pk_acq_req req = {NULL, SX_OK};

#if NUM_PK_INST > 1
   // find which one & reserve it
   for (int i = 0; i < NUM_PK_INST; i++) {
      /* CUSTOMIZATION Write custom reservation of accelerator */
      req.status = SX_ERR_PK_RETRY;
      return req;
   }
#else
   req.req = &cnx->instances[0];
#endif

   req.req->cmd = cmd;
   req.req->cnx = cnx;

   return req;
}

unsigned int sx_pk_get_req_id(sx_pk_req *req)
{
   return req - req->cnx->instances;
}


void sx_pk_set_user_context(sx_pk_req *req, void *context)
{
   req->userctxt = context;
}


void * sx_pk_get_user_context(sx_pk_req *req)
{
   return req->userctxt;
}


void sx_pk_release_req(sx_pk_req *req)
{
   req->cmd = NULL;
   req->userctxt = NULL;
}

int sx_pk_get_global_notification_id(struct sx_pk_cnx *cnx)
{
   (void) cnx;
   return -1;
}

int sx_pk_get_req_completion_id(sx_pk_req *req)
{
   /* CUSTOMIZATION
    *
    * Return the corresponding interrupt line
    * for this request
    */
   (void) req;
   return -1;
}

struct sx_regs *sx_pk_get_regs(struct sx_pk_cnx *cnx)
{
   return &cnx->instances[0].regs;
}

struct sx_pk_capabilities * sx_pk_get_caps(struct sx_pk_cnx *cnx)
{
   return &cnx->caps;
}
