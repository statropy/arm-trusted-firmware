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

/* calculation routine for baudrate */
#define FLEXCOM_DIVISOR(mck, baud)					\
	(((((mck) * 10u) / ((baud) * 16u)) % 10u) >= 5u) ?		\
	(mck / (baud * 16u) + 1u) : ((mck) / (baud * 16u))

/*
 * Initialize a new flexcom console instance and register it with the console
 * framework. The |console| pointer must point to storage that will be valid
 * for the lifetime of the console, such as a global or static local variable.
 * Its contents will be reinitialized from scratch.
 */
int console_flexcom_register(console_t *console, uintptr_t baseaddr, uint32_t divisor);

#endif /*__ASSEMBLER__*/

#endif /* FLEXCOM_UART_H */
