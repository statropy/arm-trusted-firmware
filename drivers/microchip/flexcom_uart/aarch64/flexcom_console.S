/*
 * Copyright (c) 2016-2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <asm_macros.S>
#include <console_macros.S>

#include "../flexcom_uart_regs.h"

	.globl  console_flexcom_core_init
	.globl  console_flexcom_core_putc
	.globl  console_flexcom_core_flush

	.globl  console_flexcom_register
	.globl  console_flexcom_putc
	.globl  console_flexcom_getc
	.globl  console_flexcom_flush

	/* -----------------------------------------------
	* int console_flexcom_core_init(uintptr_t baseaddr, uint32_t divisor);
	* Function to initialize UART
	* In: x0 - console base address
	*     w1 - Baud rate divisor
	* Out: return 1 on success else 0 on error
	* Clobber list : w1
	* -----------------------------------------------
	*/
func console_flexcom_core_init
	/* Configure the baudrate */
	str	w1, [x0, #USART_REG_BRGR]

	/* Disable the UART interrupt */
	mov	w1, #0
	str	w1, [x0, #USART_REG_IDR]

	/* Reset the receiver and transmitter */
	mov	w1, #(USART_CR_RSTRX|USART_CR_RSTTX|USART_CR_RXDIS|USART_CR_TXDIS)
	str	w1, [x0, #USART_REG_CR]

	/* Enable FIFO */
	ldr	w1, [x0, #USART_REG_CR]
	and	w1, w1, #(USART_CR_FIFOEN)
	str	w1, [x0, #USART_REG_CR]

	/* Configure USART in Asynchronous mode */
	mov	w1, #(USART_MR_PAR_NONE | USART_MR_CHMODE_NORMAL | USART_MR_CHRL_8BIT | USART_MR_NBSTOP_1BIT)
	str	w1, [x0, #USART_REG_MR]

	/* Enable RX and Tx */
	mov	w1, #(USART_CR_RXEN | USART_CR_TXEN)
	str	w1, [x0, #USART_REG_CR]
	ret
endfunc console_flexcom_core_init

	/* Wait for tx rdy, base in <base>, clobber <tmp> */
	.macro	waitrdy, base, tmp
1:
	/* Check transmit FIFO */
	ldr	\tmp, [\base, #USART_REG_CSR]
	tst	\tmp, #USART_IER_TXRDY
	beq	1b
	.endm

	/* --------------------------------------------------------
	 * int console_flexcom_core_putc(int c, uintptr_t base_addr)
	 * Function to output a character over the console. It
	 * returns the character printed on success or -1 on error.
	 * In : w0 - character to be printed
	 *      x1 - console base address
	 * Out : void
	 * Clobber list : w2
	 * --------------------------------------------------------
	 */
func console_flexcom_core_putc
	/* Prepend '\r' to '\n' */
	cmp	w0, #0xA
	bne	2f

	waitrdy	x1, w2

	mov	w2, #0xD
	str	w2, [x1, #USART_REG_THR]

2:
	waitrdy	x1, w2
	str	w0, [x1, #USART_REG_THR]

	ret
endfunc console_flexcom_core_putc

	/* ---------------------------------------------
	 * void console_flexcom_core_flush(uintptr_t base_addr)
	 * Function to force a write of all buffered
	 * data that hasn't been output.
	 * In : x0 - console base address
	 * Out : void.
	 * Clobber list : w1
	 * ---------------------------------------------
	 */
func console_flexcom_core_flush

	waitrdy	x0, w1

	ret
endfunc console_flexcom_core_flush

	/* -----------------------------------------------
	* int console_flexcom_register(console_t *console,
	* uintptr_t baseaddr, uint32_t divisor);
	* Function to initialize and register a new FLEXCOM
	* console. Storage passed in for the console struct
	* *must* be persistent (i.e. not from the stack).
	* In: x0 - pointer to empty console_t struct
	*     x1 - console base address
	*     w2 - Baud rate divisor
	* Out: return 1 on success else 0 on error
	* Clobber list : x1, x2, x3
	* -----------------------------------------------
	*/
func console_flexcom_register

	cbz	x0, register_fail
	cbz	x1, register_fail
	cbz	w2, register_fail

	/* Preserve lr and x0 */
	mov	x7, x30
	mov	x6, x0

	str	x1, [x0, #CONSOLE_T_BASE]

	mov	x0, x1
	mov	w1, w2
	bl	console_flexcom_core_init

	/* Restore lr and x0 */
	mov	x0, x6
	mov	x30, x7

	/*
	 * Macro to finish up registration and return (needs valid x0 + x30).
	 * If any of the argument is unspecified, then the corresponding
	 * entry in console_t is set to 0.
	 */
	finish_console_register flexcom putc=1, getc=1, flush=1

	/* Hardware init fails or parameters are invalid. */
register_fail:
	mov	w0, #0
	ret

endfunc console_flexcom_register

	/* --------------------------------------------------------
	* int console_flexcom_putc(int c, console_xxx_t *console)
	* Function to output a character over the console. It
	* returns the character printed on success or -1 on error.
	* In : w0 - character to be printed
	*      x1 - pointer to console_t struct
	* Out: w0 - printed character on success, < 0 on error.
	* Clobber list : x0, x1, x2
	* --------------------------------------------------------
	*/

func console_flexcom_putc

	ldr	x1, [x1, #CONSOLE_T_BASE]
	b	console_flexcom_core_putc

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
	* In : x0 - pointer to console_t struct
	* Out: w0 - character on success,
	*           ERROR_NO_PENDING_CHAR if no char,
	*           < -1 on error
	* Clobber list : x0, x1
	* ---------------------------------------------
	*/
func console_flexcom_getc

	ldr x0, [x0, #CONSOLE_T_BASE]

	/* Check transmit FIFO */
	ldr w1, [x0, #USART_REG_CSR]
	tst w1, #USART_IER_RXRDY
	beq getc_no_char

	/* Get the char, return */
	ldr w0, [x0, #USART_REG_RHR]
	ret

getc_no_char:
	/* Jump here if there is no character available at this time. */
	mov	w0, #ERROR_NO_PENDING_CHAR
	ret

endfunc console_flexcom_getc

	/* ---------------------------------------------
	* void console_flexcom_flush(console_t *console)
	* Function to force a write of all buffered
	* data that hasn't been output.
	* In : x0 - pointer to console_t structure
	* Out : void
	* Clobber list: x1, w2
	* ---------------------------------------------
	*/
func console_flexcom_flush

	ldr x0, [x0, #CONSOLE_T_BASE]
	b console_flexcom_core_flush

endfunc console_flexcom_flush
