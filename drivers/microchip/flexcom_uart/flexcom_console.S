/*
 * Copyright (c) 2016-2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>
#include <console_macros.S>

#include "flexcom_uart_regs.h"

	.globl  console_flexcom_register
	.globl  console_flexcom_putc
	.globl  console_flexcom_getc
	.globl  console_flexcom_flush

	/* -----------------------------------------------
	* int console_flexcom_register(uintptr_t baseaddr,
	*     uint32_t clock, uint32_t baud,
	*     console_t *console);
	* Function to initialize and register a new FLEXCOM
	* console. Storage passed in for the console struct
	* *must* be persistent (i.e. not from the stack).
	* In: r0 - pointer to empty console_t struct
	*     r1 - UART register base address
	*     r2 - UART clock divisor
	* Out: return 1 on success, 0 on error
	* -----------------------------------------------
	*/
func console_flexcom_register

	cmp r0, #0
	beq register_fail
	cmp r1, #0
	beq register_fail
	cmp r2, #0
	beq register_fail

	str r1, [r0, #CONSOLE_T_BASE]

	/* Disable the UART interrupt */
	ldr r3, [r1, #USART_REG_IDR]
	bic r3, r3, #INTERRUPT_MASK // InterruptMask
	str r3, [r1, #USART_REG_IDR]

	/* Reset the receiver and transmitter */
	mov r3, #(USART_CR_RSTRX|USART_CR_RSTTX|USART_CR_RXDIS|USART_CR_TXDIS)
	str r3, [r1, #USART_REG_CR]

	/* Enable FIFO */
	ldr r3, [r1, #USART_REG_CR]
	and r3, r3, #(USART_CR_FIFOEN)
	str r3, [r1, #USART_REG_CR]

	/* Configure the baudrate */
	str r2, [r1, #USART_REG_BRGR]

	/* Configure USART in Asynchronous mode */
	mov r3, #(USART_MR_PAR_NONE | USART_MR_CHMODE_NORMAL | USART_MR_CHRL_8BIT | USART_MR_NBSTOP_1BIT)
	str r3, [r1, #USART_REG_MR]

	/* Enable RX and Tx */
	mov r3, #(USART_CR_RXEN | USART_CR_TXEN)
	str r3, [r1, #USART_REG_CR]

	/*
	* Macro to finish up registration and return (needs valid r0 + lr).
	* If any of the argument is unspecified, then the corresponding
	* entry in console_t is set to 0.
	*/
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

	/* Wait for tx rdy, base in r1, clobber r2 */
	.macro	waitrdy_tx
1:
	/* Check transmit FIFO */
	ldr r2, [r1, #USART_REG_CSR]
	tst r2, #USART_IER_TXRDY
	beq 1b
	.endm

func console_flexcom_putc

	ldr r1, [r1, #CONSOLE_T_BASE]

	/* check console instance pointer */
	cmp r1, #0
	beq putc_error

	/* Prepend '\r' to '\n' */
	cmp r0, #0xA
	bne 2f

	waitrdy_tx

	mov r2, #0xD
	str r2, [r1, #USART_REG_THR]

2:
	waitrdy_tx
	str r0, [r1, #USART_REG_THR]

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

	/* check console instance pointer */
	cmp r0, #0
	beq getc_error

	ldr r0, [r0, #CONSOLE_T_BASE]

	/* Check transmit FIFO */
	ldr r1, [r0, #USART_REG_CSR]
	tst r1, #USART_IER_RXRDY
	beq getc_no_char

	/* Get the char, return */
	ldr r0, [r0, #USART_REG_RHR]
	bx  lr

getc_error:
	/* common error handler */
	mov r0, #ERROR_NO_VALID_CONSOLE
	bx  lr

getc_no_char:
	/* Jump here if there is no character available at this time. */
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

	/* check console instance pointer */
	cmp r0, #0
	beq flush_error

	ldr r1, [r0, #CONSOLE_T_BASE]

	waitrdy_tx

	bx  lr

flush_error:
	mov r0, #-1
	bx  lr
endfunc console_flexcom_flush
