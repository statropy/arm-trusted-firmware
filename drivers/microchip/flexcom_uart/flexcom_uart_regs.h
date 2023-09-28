/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2010, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _USART_H_
#define _USART_H_


/* Configure GPIO pins for the FLEXCOM interface
 * 0: disable, 1: enable */
#define CFG_FLEXCOM_GPIO_PINS   1


/* 32-Bit mask for interrupts */
#define INTERRUPT_MASK   0xFFFFFFFFu

/* FLEXCOM interface definitions */
#define FLEXCOM0    0u
#define FLEXCOM1    1u
#define FLEXCOM2    2u
#define FLEXCOM3    3u
#define FLEXCOM4    4u


/* Configure FLEXCOM operating mode */
#define FLEXCOM_MODE_NO_COM     (0x0u << 0x0u)
#define FLEXCOM_MODE_USART      (0x1u << 0x0u)
#define FLEXCOM_MODE_SPI        (0x2u << 0x0u)
#define FLEXCOM_MODE_TWI        (0x3u << 0x0u)


/* *** Register offset in FLEXCOM structure ***/
#define USART_REG_CR         0x00u      /* Control Register */
#define USART_REG_MR         0x04u      /* Mode Register */
#define USART_REG_IER        0x08u      /* Interrupt Enable Register */
#define USART_REG_IDR        0x0Cu      /* Interrupt Disable Register */
#define USART_REG_IMR        0x10u      /* Interrupt Mask Register */
#define USART_REG_CSR        0x14u      /* Channel Status Register */
#define USART_REG_RHR        0x18u      /* Receiver Holding Register */
#define USART_REG_THR        0x1Cu      /* Transmitter Holding Register */
#define USART_REG_BRGR       0x20u      /* Baud Rate Generator Register */
#define USART_REG_CIDR       0x40u      /* Chip ID Register */
#define USART_REG_EXID       0x44u      /* Chip ID Extension Register */
#define USART_REG_FNTR       0x48u      /* Force NTRST Register */
#define USART_REG_ADDRSIZE   0xECu      /* DBGU ADDRSIZE REGISTER */
#define USART_REG_IPNAME1    0xF0u      /* DBGU IPNAME1 REGISTER */
#define USART_REG_IPNAME2    0xF4u      /* DBGU IPNAME2 REGISTER */
#define USART_REG_FEATURES   0xF8u      /* DBGU FEATURES REGISTER */
#define USART_REG_VER        0xFCu      /* DBGU VERSION REGISTER */


/* -------- (USART Offset: 0x0) FLEXCOM USART Control Register --------*/
#define USART_CR_RSTRX      (0x1UL << 2u)
#define USART_CR_RSTTX      (0x1UL << 3u)
#define USART_CR_RXEN       (0x1UL << 4u)
#define USART_CR_RXDIS      (0x1UL << 5u)
#define USART_CR_TXEN       (0x1UL << 6u)
#define USART_CR_TXDIS      (0x1UL << 7u)
#define USART_CR_RSTSTA     (0x1UL << 8u)
#define USART_CR_TXFCLR     (0x1UL << 24u)
#define USART_CR_RXFCLR     (0x1UL << 25u)
#define USART_CR_FIFOEN     (0x1UL << 30u)
#define USART_CR_FIFIDIS    (0x1UL << 31u)



/* -------- (USART Offset: 0x4) FLEXCOM USART Mode Register --------*/
#define USART_MR_CHRL               (0x3UL << 6u)
#define USART_MR_CHRL_5BIT          (0x0UL << 6u)
#define USART_MR_CHRL_6BIT          (0x1UL << 6u)
#define USART_MR_CHRL_7BIT          (0x2UL << 6u)
#define USART_MR_CHRL_8BIT          (0x3UL << 6u)
#define USART_MR_SYNCHRON_MODE      (0x1UL << 8u)
#define USART_MR_PAR                (0x7UL << 9u)
#define USART_MR_PAR_EVEN           (0x0UL << 9u)
#define USART_MR_PAR_ODD            (0x1UL << 9u)
#define USART_MR_PAR_SPACE          (0x2UL << 9u)
#define USART_MR_PAR_MARK           (0x3UL << 9u)
#define USART_MR_PAR_NONE           (0x4UL << 9u)
#define USART_MR_NBSTOP             (0x3UL << 12u)
#define USART_MR_NBSTOP_1BIT        (0x0UL << 12u)
#define USART_MR_NBSTOP_1_5BIT      (0x1UL << 12u)
#define USART_MR_NBSTOP_2BIT        (0x2UL << 12u)
#define USART_MR_CHMODE             (0x3UL << 14u)
#define USART_MR_CHMODE_NORMAL      (0x0UL << 14u)
#define USART_MR_CHMODE_AUTO        (0x1UL << 14u)
#define USART_MR_CHMODE_LOCAL       (0x2UL << 14u)
#define USART_MR_CHMODE_REMOTE      (0x3UL << 14u)
#define USART_MR_USCLK_SEL(x)	    (x << 4u)


/* -------- (USART Offset: 0x8) FLEXCOM USART Interrupt Enable Register -------- */
#define USART_IER_RXRDY     BIT(0)
#define USART_IER_TXRDY     (0x1UL <<  1u)
#define USART_IER_ENDRX     (0x1UL <<  3u)
#define USART_IER_ENDTX     (0x1UL <<  4u)
#define USART_IER_OVRE      (0x1UL <<  5u)
#define USART_IER_FRAME     (0x1UL <<  6u)
#define USART_IER_PARE      (0x1UL <<  7u)
#define USART_IER_TXEMPTY   (0x1UL <<  9u)
#define USART_IER_TXBUFE    (0x1UL << 11u)
#define USART_IER_RXBUFF    (0x1UL << 12u)
#define USART_IER_COMM_TX   (0x1UL << 30u)
#define USART_IER_COMM_RX   (0x1UL << 31u)


/* -------- (USART Offset: 0x14) FLEXCOM USART Channel Status Register ------ */
#define USART_CSR_OVRE      (0x1UL <<  8u)


// ToDo: add register defines for interrupt functionality
/* -------- DBGU_IDR : (USART Offset: 0xc)  FLEXCOM USART Interrupt Disable Register---- */
/* -------- DBGU_IMR : (USART Offset: 0x10) FLEXCOM USART Interrupt Mask Register ------ */

#endif // _USART_H_
