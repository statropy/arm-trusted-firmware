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

#include "usart.h"
#include "platform.h"
#include "mchp,lan966x_icpu.h"

/*
 * Configure the GPIO pin for FLEXCOM0
 */
static inline void usart_init_gpioPins(void)
{
    uint32_t gpio;

    // ALT1: gpio25(RXD), gpio26(TXD)
    gpio = LAN_RD(GCB_GPIO_ALT(0)) | ( ( 1u << 25u ) | ( 1u << 26u ) );
    LAN_WR( gpio, GCB_GPIO_ALT(0) );

    gpio = LAN_RD(GCB_GPIO_ALT(1)) & ~( ( 1u << 25u ) | ( 1u << 26u ) );
    LAN_WR( gpio, GCB_GPIO_ALT(1) );

    gpio = LAN_RD(GCB_GPIO_ALT(2)) & ~( ( 1u << 25u ) | ( 1u << 26u ) );
    LAN_WR( gpio, GCB_GPIO_ALT(2) );

    /* Set FLEXCOM0 to USART mode */
    LAN_WR(FLEXCOM_MODE_USART, FLEXCOM_FLEX_MR(FLEXCOM0));
}


/*
 * Configure the GIC and the FLEXCOM0 interface
 */
void usart_init( unsigned int baudrate )
{
#ifdef CFG_FLEXCOM_GPIO_PINS
    usart_init_gpioPins();
#endif

    uint32_t cfgValue = 0u;

    //Enable interrupt distributor, GIC_EnableDistributor()
    cfgValue = LAN_RD(GIC400_GICD_CTLR);
    cfgValue |= 0x01u;
    LAN_WR(cfgValue, GIC400_GICD_CTLR);

    //Set priority mask, GIC_SetInterfacePriorityMask(0xF0U)
    LAN_WR(0xF0U, GIC400_GICC_PMR);

    //Enable CPU interrupt interface, GIC_EnableInterface()
    cfgValue = LAN_RD(GIC400_GICC_CTLR);
    cfgValue |= 0x01u;
    LAN_WR(cfgValue, GIC400_GICC_CTLR);

    // Enable interrupt, GIC_EnableIRQ(FLEXCOM0_SPI_IRQn)
    cfgValue = LAN_RD(GIC400_GICD_ISENABLER2);
    cfgValue |= 1U << (FLEXCOM0_SPI_IRQn % 32U);
    LAN_WR(cfgValue, GIC400_GICD_ISENABLER2);

    /* Disable interrupts */
    LAN_WR(INTERRUPT_MASK, FLEXCOM_FLEX_US_IDR(FLEXCOM0) );

    /* Reset the receiver and transmitter */
    LAN_WR( USART_CR_RSTRX  |
            USART_CR_RSTTX  |
            USART_CR_RXDIS  |
            USART_CR_TXDIS  |
            USART_CR_TXFCLR |
            USART_CR_RXFCLR,
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
    LAN_WR( USART_CR_RXEN | USART_CR_TXEN, FLEXCOM_FLEX_US_CR(FLEXCOM0));

    /* Enable interrupt for RX direction */
    LAN_WR( USART_IER_RXRDY, FLEXCOM_FLEX_US_IER(FLEXCOM0) );

}


/*
 * Write one character to the FLEXCOM0
 */
static void usart_putc( const char c )
{
    while (!( LAN_RD(FLEXCOM_FLEX_US_CSR(FLEXCOM0) ) & USART_IER_TXRDY ) )
        ;

    LAN_WR(c, FLEXCOM_FLEX_US_THR(FLEXCOM0));
}


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


/*
 * Read one character from the FLEXCOM0
 */
char usart_getc( void )
{
    while (!( LAN_RD(FLEXCOM_FLEX_US_CSR(FLEXCOM0) ) & USART_IER_RXRDY ) )
        ;

    return (char)LAN_RD(FLEXCOM_FLEX_US_RHR(FLEXCOM0));
}
