/*
 * Copyright (c) 2013-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lan966x_regs_common.h"
#include "mchp,lan966x_icpu.h"
#include "lan966x_def.h"

#include <drivers/microchip/flexcom_uart.h>
#include <assert.h>


/* Forward declaration of function */
int flexcom_console_putc(int character, struct console *console);
int flexcom_console_getc(struct console *console);
void flexcom_console_flush(struct console *console);


static console_t flexcom_console = {
    .flags = 0u,
    .putc = flexcom_console_putc,
    .getc = flexcom_console_getc,
    .flush = flexcom_console_flush,
    .base = 0u,
};


/*
 * Configure the FLEXCOM interface
 */
void flexcom_console_init( uint32_t baudrate )
{
    /* Reset the receiver and transmitter */
    LAN_WR( USART_CR_RSTRX  |
            USART_CR_RSTTX  |
            USART_CR_RXDIS  |
            USART_CR_TXDIS,
            FLEXCOM_FLEX_US_CR(FLEXCOM0) );

    /* Configure the baudrate */
    LAN_WR( baudrate, FLEXCOM_FLEX_US_BRGR(FLEXCOM0) );

    /* Configure USART in Asynchronous mode */
    LAN_WR( USART_MR_PAR_NONE       |
            USART_MR_CHMODE_NORMAL  |
            USART_MR_CHRL_8BIT      |
            USART_MR_NBSTOP_1BIT,
            FLEXCOM_FLEX_US_MR(FLEXCOM0) );

    /* Enable RX and Tx */
    LAN_WR( USART_CR_RXEN | USART_CR_TXEN, FLEXCOM_FLEX_US_CR(FLEXCOM0) );
}


int console_flexcom_register(uintptr_t baseaddr, uint32_t clock, uint32_t baud, console_t *console)
{
    // argument clock needed ?
    flexcom_console.base = baseaddr;

    /* Initialize console hw */
    flexcom_console_init(baud);

	return 0;
}


/*
 * Write one character to the FLEXCOM
 */
int flexcom_console_putc(int character, struct console *console)
{
    while (!( LAN_RD(FLEXCOM_FLEX_US_CSR(FLEXCOM0) ) & USART_IER_TXRDY ) )
        ;

    LAN_WR(character, FLEXCOM_FLEX_US_THR(FLEXCOM0));

    return 1; // Check for a better solution
}


#if 0
/*
 * Write a string to the FLEXCOM0
 */
void usart_puts( const char *ptr )
{
    int i = 0;

    while ( ptr[i] != '\0' ) {
        if ( ptr[i] == '\n' ) {
            usart_putc( '\r' );
        }

        usart_putc( ptr[i] );
        i++;
    }
}
#endif


/*
 * Read one character from the FLEXCOM
 */
int flexcom_console_getc(struct console *console)
{
    while (!( LAN_RD(FLEXCOM_FLEX_US_CSR(FLEXCOM0) ) & USART_IER_RXRDY ) )
        ;

    return (char)LAN_RD(FLEXCOM_FLEX_US_RHR(FLEXCOM0));
}


void flexcom_console_flush(struct console *console)
{
// ToDo: add content
}


/* Unregister console instance */
void flexcom_console_end(console_t *console)
{
    console_flush();

    (void)console_unregister(console);
}
