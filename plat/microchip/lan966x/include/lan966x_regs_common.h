/*
 * Copyright (c) 2015-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLATFORM_DEF_H
#define PLATFORM_DEF_H

#include <stdint.h>
#include "lan966x_baremetal_cpu_regs.h"

extern uint32_t maserati_regs[NUM_TARGETS];

/* -------------------------  Interrupt Number Definition  ------------------------ */

#if 0
/** Device specific Interrupt IDs */
typedef enum IRQn {
    TIMER1_SPI_IRQn   = 71u,    /* TIMER1 shared peripheral interrupt */
    TIMER2_SPI_IRQn   = 72u,    /* TIMER2 shared peripheral interrupt */
    TIMER3_SPI_IRQn   = 73u,    /* TIMER2 shared peripheral interrupt */
    FLEXCOM0_SPI_IRQn = 80u,    /* FLEXCOM0 shared peripheral interrupt */
    FLEXCOM1_SPI_IRQn = 81u,    /* FLEXCOM1 shared peripheral interrupt */
    FLEXCOM2_SPI_IRQn = 82u,    /* FLEXCOM2 shared peripheral interrupt */
    FLEXCOM3_SPI_IRQn = 83u,    /* FLEXCOM3 shared peripheral interrupt */
    FLEXCOM4_SPI_IRQn = 84u     /* FLEXCOM4 shared peripheral interrupt */

} IRQn_Type;
#endif

/* Factory CLK used on sunrise board */
#define FACTORY_CLK     30000000u

/* UART interface speed grade */
#define UART_BAUDRATE   115000u

/*  I/O Function Macro */
#define writel(value, addr)     (*(volatile unsigned int *)(addr)) = (value)
#define readl(addr)             (*(volatile unsigned int *)(addr))

/* Macros for VML based register access */
#define LAN_RD_(id, tinst, tcnt,                        \
                gbase, ginst, gcnt, gwidth,             \
                raddr, rinst, rcnt, rwidth)             \
                readl(maserati_regs[id + (tinst)] +     \
                gbase + ((ginst) * gwidth) +            \
                raddr + ((rinst) * rwidth))

#define LAN_WR_(val, id, tinst, tcnt,                       \
                gbase, ginst, gcnt, gwidth,                 \
                raddr, rinst, rcnt, rwidth)                 \
                writel(val, maserati_regs[id + (tinst)] +   \
                gbase + ((ginst) * gwidth) +                \
                raddr + ((rinst) * rwidth))

#define LAN_RMW_(val, mask, id, tinst, tcnt,                \
                gbase, ginst, gcnt, gwidth,                 \
                raddr, rinst, rcnt, rwidth) do {            \
                u32 _v_;                                    \
                _v_ = readl(maserati_regs[id + (tinst)] +   \
                gbase + ((ginst) * gwidth) +                \
                raddr + ((rinst) * rwidth));                \
                _v_ = ((_v_ & ~(mask)) | ((val) & (mask))); \
                writel(_v_, maserati_regs[id + (tinst)] +   \
                gbase + ((ginst) * gwidth) +                \
                raddr + ((rinst) * rwidth)); } while (0)


/* Macros for VML based register access */
#define LAN_RD(...) LAN_RD_(__VA_ARGS__)
#define LAN_WR(...) LAN_WR_(__VA_ARGS__)
#define LAN_RMW(...) LAN_RMW_(__VA_ARGS__)

#endif /* PLATFORM_DEF_H */
