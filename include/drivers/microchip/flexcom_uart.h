/*
 * Copyright (c) 2013-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FLEXCOM_UART_H
#define FLEXCOM_UART_H

#include <drivers/console.h>

#ifndef __ASSEMBLER__

#include <stdint.h>

/*
 * Initialize a new flexcom console instance and register it with the console
 * framework. The |console| pointer must point to storage that will be valid
 * for the lifetime of the console, such as a global or static local variable.
 * Its contents will be reinitialized from scratch.
 */
int console_flexcom_register(uintptr_t baseaddr, uint32_t clock, uint32_t baud,
			     console_t *console);

#endif /*__ASSEMBLER__*/

#endif /* FLEXCOM_UART_H */
