/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <platform_def.h>
#include <drivers/console.h>
#include <drivers/microchip/vcore_gpio.h>
#include <drivers/microchip/flexcom_uart.h>

#include "lan966x_private.h"
#include "mchp,lan966x_icpu.h"

static console_t lan966x_console;

void lan966x_console_init(void)
{
	int console_scope = CONSOLE_FLAG_BOOT | CONSOLE_FLAG_RUNTIME;

	vcore_gpio_init(GCB_GPIO_ADDR);

	vcore_gpio_set_alt(25, 1);
	vcore_gpio_set_alt(26, 1);

	/* Initialize the console to provide early debug support */
	console_flexcom_register(FLEXCOM0_BASE, FLEXCOM_UART_CLK_IN_HZ,
				 FLEXCOM_BAUDRATE, &lan966x_console);

	console_set_scope(&lan966x_console, console_scope);
}
