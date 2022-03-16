/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _DRIVERS_USB_H
#define _DRIVERS_USB_H

void lan966x_usb_init(uint32_t bias, uint32_t rbias);
void lan966x_usb_register_console(void);

#endif	/* _DRIVERS_USB_H */
