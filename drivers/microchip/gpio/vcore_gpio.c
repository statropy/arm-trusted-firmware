/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>
#include <assert.h>
#include <lib/mmio.h>
#include <drivers/delay_timer.h>
#include <drivers/microchip/vcore_gpio.h>

/* GPIO standard registers */
#define OCELOT_GPIO_OUT_SET	0x0
#define OCELOT_GPIO_OUT_CLR	0x4
#define OCELOT_GPIO_OUT		0x8
#define OCELOT_GPIO_IN		0xc
#define OCELOT_GPIO_OE		0x10
#define OCELOT_GPIO_INTR	0x14
#define OCELOT_GPIO_INTR_ENA	0x18
#define OCELOT_GPIO_INTR_IDENT	0x1c
#define OCELOT_GPIO_ALT0	0x20
#define OCELOT_GPIO_ALT1	0x24
#define OCELOT_GPIO_SD_MAP	0x28

/* LAN966x */
#define VCORE_GPIO_STRIDE	3
#define VCORE_GPIO_NGPIO	(VCORE_GPIO_STRIDE * 32)

#define REG(r, p) ((r) * VCORE_GPIO_STRIDE + (4 * ((p) / 32)))
#define REG_ALT(msb, p) (OCELOT_GPIO_ALT0 * VCORE_GPIO_STRIDE + 4 * ((msb) + (VCORE_GPIO_STRIDE * ((p) / 32))))

static uintptr_t reg_base;

static int vcore_gpio_get_direction(int gpio);
static void vcore_gpio_set_direction(int gpio, int direction);
static int vcore_gpio_get_value(int gpio);
static void vcore_gpio_set_value(int gpio, int value);
static int vcore_gpio_get_pull(int gpio);
static void vcore_gpio_set_pull(int gpio, int pull);

static const gpio_ops_t vcore_gpio_ops = {
	.get_direction  = vcore_gpio_get_direction,
	.set_direction  = vcore_gpio_set_direction,
	.get_value      = vcore_gpio_get_value,
	.set_value      = vcore_gpio_set_value,
	.get_pull	= vcore_gpio_get_pull,
	.set_pull	= vcore_gpio_set_pull,
};

static void vcore_gpio_set_bits(uint32_t reg, uint32_t mask, uint32_t value)
{
	uint32_t val = mmio_read_32(reg_base + reg);
	val &= ~mask;
	val |= (value & mask);
	mmio_write_32(reg_base + reg, val);
}

static void vcore_gpio_write(uint32_t reg, uint32_t value)
{
	mmio_write_32(reg_base + reg, value);
}

static uint32_t vcore_gpio_read(uint32_t reg)
{
	return mmio_read_32(reg_base + reg);
}

/**
 * Get selection of GPIO pinmux settings.
 *
 * @param gpio The pin number of GPIO. From 0 to 53.
 * @return The selection of pinmux.
 */
int vcore_gpio_get_alt(int gpio)
{
	unsigned int p = gpio % 32;
	int ret = 0;

	if (vcore_gpio_read(REG_ALT(0, gpio)) & BIT(p))
		ret |= BIT(0);
	if (vcore_gpio_read(REG_ALT(1, gpio)) & BIT(p))
		ret |= BIT(1);
#if (VCORE_GPIO_STRIDE >= 3)
	if (vcore_gpio_read(REG_ALT(2, gpio)) & BIT(p))
		ret |= BIT(2);
#endif

	return ret;
}

/**
 * Set selection of GPIO pinmux settings.
 *
 * @param gpio The pin number of GPIO. From 0 to 53.
 * @param fsel The selection of pinmux.
 */
void vcore_gpio_set_alt(int gpio, int f)
{
	unsigned int p = gpio % 32;

	/*
	 * f is encoded on two (or three) bits.
	 * bit 0 of f goes in BIT(pin) of ALT[0], bit 1 of f goes in BIT(pin) of
	 * ALT[1], bit 2 of f goes in BIT(pin) of ALT[2]
	 * This is racy because both registers can't be updated at the same time
	 * but it doesn't matter much for now.
	 * Note: ALT0/ALT1/ALT2 are organized specially for 78 gpio targets
	 */
	vcore_gpio_set_bits(REG_ALT(0, gpio), BIT(p), f << p);
	vcore_gpio_set_bits(REG_ALT(1, gpio), BIT(p), f << (p - 1));
#if (VCORE_GPIO_STRIDE >= 3)
	vcore_gpio_set_bits(REG_ALT(2, gpio), BIT(p), f << (p - 2));
#endif
}

static int vcore_gpio_get_direction(int gpio)
{
	unsigned int p = gpio % 32;

	if (vcore_gpio_read(REG(OCELOT_GPIO_OE, gpio)) & BIT(p))
		return GPIO_DIR_OUT;

	return GPIO_DIR_IN;
}

static void vcore_gpio_set_direction(int gpio, int direction)
{
	unsigned int p = gpio % 32;

	vcore_gpio_set_bits(REG(OCELOT_GPIO_OE, gpio), BIT(p),
			    direction == GPIO_DIR_IN ? 0 : BIT(p));
}

static int vcore_gpio_get_value(int gpio)
{
	unsigned int p = gpio % 32;

	if (vcore_gpio_read(REG(OCELOT_GPIO_IN, gpio)) & BIT(p))
		return GPIO_LEVEL_HIGH;
	return GPIO_LEVEL_LOW;
}

static void vcore_gpio_set_value(int gpio, int value)
{
	unsigned int p = gpio % 32;

	if (value)
		vcore_gpio_write(REG(OCELOT_GPIO_OUT_SET, gpio), BIT(p));
	else
		vcore_gpio_write(REG(OCELOT_GPIO_OUT_CLR, gpio), BIT(p));
}

static void vcore_gpio_set_pull(int gpio, int pull)
{
	/* n/a */
}

static int vcore_gpio_get_pull(int gpio)
{
	return GPIO_PULL_NONE;
}

void vcore_gpio_init(uintptr_t base)
{
	/* Check if GPIO is already initialized */
	if (reg_base != base) {
		reg_base = base;
		gpio_init(&vcore_gpio_ops);
	}
}
