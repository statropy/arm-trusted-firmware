/*
 * Copyright (c) 2016-2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <flexcom_uart.h>
#include <lan966x_def.h>

#include <asm_macros.S>
#include <console_macros.S>
#include <assert_macros.S>
#include <arch.h>


#define INTERRUPT_MASK   0xFFFFFFFFu


    /*
     * This file contains a skeleton console driver that can be used as a
     * basis for a real console driver. Console drivers in Trusted Firmware
     * can be instantiated multiple times. Each instance is described by a
     * separate console_t structure which must be registered with the common
     * console framework via console_register(). Console drivers should
     * define a console_flexcom_register() function that initializes a new
     * console_t structure passed in from the caller and registers it after
     * initializing the console hardware. Drivers may define their own
     * structures extending console_t to store private driver information.
     * Console drivers *MUST* ensure that the console callbacks they
     * implement only change registers allowed in the clobber lists defined
     * in this file. (Note that in addition to the explicit clobber lists,
     * any function may always clobber the intra-procedure-call register
     * r12, but may never depend on it retaining its value across any
     * function call.)
     */

    .globl  console_flexcom_register
    .globl  console_flexcom_putc
    .globl  console_flexcom_getc
    .globl  console_flexcom_flush


/* -----------------------------------------------
     * int console_init(uintptr_t base_addr,
     * unsigned int uart_clk, unsigned int baud_rate)
     * Function to initialize the console without a
     * C Runtime to print debug information. This
     * function will be accessed by console_init and
     * crash reporting.
     * In: r0 - console base address
     *     r1 - Uart clock in Hz
     *     r2 - Baud rate
     * Out: return 1 on success else 0 on error
     * Clobber list : r1, r2, r3
     * -----------------------------------------------
     */
func console_flexcom_init

    /* Check the input base address */
    cmp r0, #0
    beq core_init_fail

    /* Check uart clock */
    cmp r1, #0
    beq core_init_fail

     /* Check baud rate */
    cmp r2, #0
    beq core_init_fail

    /* Disable the UART interrupt */
    ldr r3, [r0, #0x20C]        // FLEXCOM0_USART_OFFSET + USART_REG_IDR
    bic r3, r3, #INTERRUPT_MASK // InterruptMask
    str r3, [r0, #0x20C]

    /* Reset the receiver and transmitter */
    mov r3, #0xAC           // USART_CR_RSTRX|USART_CR_RSTTX|USART_CR_RXDIS|USART_CR_TXDIS
    str r3, [r0, #0x200]    // FLEXCOM0_USART_OFFSET + USART_REG_CR,

    /* Configure the baudrate */
    mov r3, #0xA3           // (mck / (baud * 16u) + 1u)
    str r3, [r0, #0x220]    // FLEXCOM0_USART_OFFSET + USART_REG_BRGR

    /* Configure USART in Asynchronous mode */
    mov r4, #0x08C0         // USART_MR_PAR_NONE | USART_MR_CHMODE_NORMAL | USART_MR_CHRL_8BIT | USART_MR_NBSTOP_1BIT
    str r4, [r0, #0x204]    // FLEXCOM0_USART_OFFSET + USART_REG_MR

    /* Return with retVal 1 */
    mov r0, #1
    bx  lr

    core_init_fail:
    mov r0, #0
    bx  lr
endfunc console_flexcom_init


    /* -----------------------------------------------
     * int console_flexcom_register(uintptr_t baseaddr,
     *     uint32_t clock, uint32_t baud,
     *     console_t *console);
     * Function to initialize and register a new FLEXCOM
     * console. Storage passed in for the console struct
     * *must* be persistent (i.e. not from the stack).
     * In: r0 - UART register base address
     *     r1 - UART clock in Hz
     *     r2 - Baud rate
     *     r3 - pointer to empty console_t struct
     * Out: return 1 on success, 0 on error
     * -----------------------------------------------
     */
func console_flexcom_register

    push {r4, lr}
    mov r4, r3
    cmp r4, #0
    beq register_fail

    str r0, [r3, #CONSOLE_T_BASE]

    bl console_flexcom_init
    cmp r0, #0
    beq register_fail

    /*
     * Macro to finish up registration and return (needs valid r0 + lr).
     * If any of the argument is unspecified, then the corresponding
     * entry in console_t is set to 0.
     */
    mov r0, r4
    pop {r4, lr}
    finish_console_register flexcom putc=1, getc=1, flush=1

    /* Hardware init fails or parameters are invalid. */
register_fail:
    mov r0, #0
    bx  lr
endfunc console_flexcom_register

    /* --------------------------------------------------------
     * int console_flexcom_putc(int c, console_xxx_t *console)
     * Function to output a character over the console. It
     * returns the character printed on success or -1 on error.
     * In : r0 - character to be printed
     *      r1 - pointer to console_t struct
     * Out: r0 - printed character on success, < 0 on error.
     * Clobber list : r0, r1, r2
     * --------------------------------------------------------
     */
func console_flexcom_putc

    ldr r1, [r1, #CONSOLE_T_BASE]

    /* check console instance pointer */
    cmp r1, #0
    beq putc_error

    /* Prepend '\r' to '\n' */
    cmp r0, #0xA
    bne 2f
1:
    /* Check transmit FIFO */
    ldr r2, [r1, #0x214]    // FLEXCOM0_USART_OFFSET + USART_REG_CSR
    tst r2, #0x2            // USART_IER_TXRDY
    bne 1b
    mov r2, #0xD
    str r2, [r1, #0x21C]   // FLEXCOM0_USART_OFFSET + USART_REG_THR
2:
    /* Check transmit FIFO */
    ldr r2, [r1, #0x214]    // FLEXCOM0_USART_OFFSET + USART_REG_CSR
    tst r2, #0x2            // USART_IER_TXRDY
    bne 2b
    str r0, [r1, #0x21C]    // FLEXCOM0_USART_OFFSET + USART_REG_THR

    bx lr

    /* Jump here if output fails for any reason. */
putc_error:
    mov r0, #-1
    bx  lr
endfunc console_flexcom_putc

    /* ---------------------------------------------
     * int console_flexcom_getc(console_xxx_t *console)
     * Function to get a character from the console.
     * Even though console_getc() is blocking, this
     * callback has to be non-blocking and always
     * return immediately to allow polling multiple
     * drivers concurrently.
     * Returns the character grabbed on success,
     * ERROR_NO_PENDING_CHAR if no character was
     * available at this time, or any value
     * between -2 and -127 if there was an error.
     * In : r0 - pointer to console_t struct
     * Out: r0 - character on success,
     *           ERROR_NO_PENDING_CHAR if no char,
     *           < -1 on error
     * Clobber list : r0, r1
     * ---------------------------------------------
     */
func console_flexcom_getc

    ldr r0, [r0, #CONSOLE_T_BASE]

    /* check console instance pointer */
    cmp r0, #0
    beq getc_error
1:
    /* Check if receive FIFO is empty */
    ldr r1, [r0, #0x214]    // FLEXCOM0_USART_OFFSET + USART_REG_CSR
    tst r1, #0x1            // USART_IER_RXRDY
    bne 1b
    ldr r1, [r0, #0x218]    // FLEXCOM0_USART_OFFSET + USART_REG_RHR
    mov r0, r1

    /* ToDo: check if this code is working */
    cmp r0, #-1
    beq getc_no_char

    bx  lr

    /* common error handler */
getc_error:
    mov r0, #-1
    bx  lr

    /* Jump here if there is no character available at this time. */
getc_no_char:
    mov r0, #ERROR_NO_PENDING_CHAR
    bx  lr
endfunc console_flexcom_getc

    /* ---------------------------------------------
     * void console_flexcom_flush(console_t *console)
     * Function to force a write of all buffered
     * data that hasn't been output.
     * In : r0 - pointer to console_t structure
     * Out : void
     * Clobber list: r0, r1
     * ---------------------------------------------
     */
func console_flexcom_flush

    ldr r1, [r0, #CONSOLE_T_BASE]

    /* check console instance pointer */
    cmp r1, #0
    beq flush_error

1:
    /* Loop while the transmit FIFO is busy */
    ldr r1, [r0, #0x214]    // FLEXCOM0_USART_OFFSET + USART_REG_CSR
    tst r1, #0x200          // USART_IER_TXEMPTY
    bne 1b

    bx  lr

flush_error:
    mov r0, #-1
    bx  lr
endfunc console_flexcom_flush
