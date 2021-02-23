/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VCORE_GPIO_H
#define VCORE_GPIO_H

#include <stdint.h>
#include <drivers/gpio.h>

void vcore_gpio_init(uintptr_t base);
int vcore_gpio_get_alt(int gpio);
void vcore_gpio_set_alt(int gpio, int fsel);

#endif  /* VCORE_GPIO_H */
