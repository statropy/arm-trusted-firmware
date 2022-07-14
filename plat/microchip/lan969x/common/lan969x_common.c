/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/console.h>
#include <drivers/microchip/emmc.h>
#include <drivers/microchip/flexcom_uart.h>
#include <drivers/microchip/lan966x_clock.h>
#include <drivers/microchip/qspi.h>
#include <drivers/microchip/usb.h>
#include <drivers/microchip/vcore_gpio.h>
#include <drivers/spi_nor.h>
#include <fw_config.h>
#include <lib/mmio.h>
#include <lib/utils.h>
#include <plat/arm/common/plat_arm.h>

#include "lan969x_regs.h"
#include "lan969x_private.h"
#include "plat_otp.h"

/* Define global fw_config, set default MMC settings */
lan966x_fw_config_t lan966x_fw_config __aligned(CACHE_WRITEBACK_GRANULE);

#define LAN969X_MAP_QSPI0						\
	MAP_REGION_FLAT(						\
		LAN969X_QSPI0_MMAP,					\
		LAN969X_QSPI0_RANGE,					\
		MT_DEVICE | MT_RO | MT_SECURE)

#define LAN969X_MAP_QSPI0_RW						\
	MAP_REGION_FLAT(						\
		LAN969X_QSPI0_MMAP,					\
		LAN969X_QSPI0_RANGE,					\
		MT_DEVICE | MT_RW | MT_SECURE)

#define LAN969X_MAP_AXI							\
	MAP_REGION_FLAT(						\
		LAN969X_DEV_BASE,					\
		LAN969X_DEV_SIZE,					\
		MT_DEVICE | MT_RW | MT_SECURE)

#define LAN969X_MAP_BL31					\
	MAP_REGION_FLAT(					\
		BL31_BASE,					\
		BL31_SIZE,					\
		MT_MEMORY | MT_RW | MT_SECURE)

#define LAN969X_MAP_NS_MEM					\
	MAP_REGION_FLAT(					\
		PLAT_LAN969X_NS_IMAGE_BASE,			\
		PLAT_LAN969X_NS_IMAGE_SIZE,			\
		MT_MEMORY | MT_RW | MT_NS)

#ifdef IMAGE_BL1
const mmap_region_t plat_arm_mmap[] = {
	LAN969X_MAP_QSPI0,
	LAN969X_MAP_AXI,
	{0}
};
#endif
#if defined(IMAGE_BL2)
const mmap_region_t plat_arm_mmap[] = {
	LAN969X_MAP_QSPI0,
	LAN969X_MAP_AXI,
	LAN969X_MAP_BL31,
	LAN969X_MAP_NS_MEM,
	{0}
};
#endif
#if defined(IMAGE_BL2U)
const mmap_region_t plat_arm_mmap[] = {
	LAN969X_MAP_QSPI0_RW,
	LAN969X_MAP_AXI,
	LAN969X_MAP_BL31,
	LAN969X_MAP_NS_MEM,
	{0}
};
#endif
#ifdef IMAGE_BL31
const mmap_region_t plat_arm_mmap[] = {
	LAN969X_MAP_QSPI0,
	LAN969X_MAP_AXI,
	LAN969X_MAP_BL31,
	{0}
};
#endif

static console_t lan969x_console;

enum lan969x_flexcom_id {
	FLEXCOM0 = 0,
	FLEXCOM1,
	FLEXCOM2,
	FLEXCOM3,
	FLEXCOM4,
};

static struct lan969x_flexcom_args {
	uintptr_t base;
	unsigned int clk_id;
	int rx_gpio;
	int tx_gpio;
	int gpio_alt;
} lan969x_flexcom_map[] = {
	[FLEXCOM0] = {
		LAN969X_FLEXCOM_0_BASE, LAN966X_CLK_ID_FLEXCOM0, 3, 4, 1
	},
	[FLEXCOM1] = {
		LAN969X_FLEXCOM_1_BASE, LAN966X_CLK_ID_FLEXCOM1, 28, 29, 2
	},
	[FLEXCOM2] = {
		LAN969X_FLEXCOM_2_BASE, LAN966X_CLK_ID_FLEXCOM2, 65, 66, 1
	},
	[FLEXCOM3] = {
		LAN969X_FLEXCOM_3_BASE, LAN966X_CLK_ID_FLEXCOM3, 55, 56, 2
	},
};

static void lan969x_flexcom_init(int idx, int br)
{
	struct lan969x_flexcom_args *fc;

	if (idx < 0)
		return;

	fc = &lan969x_flexcom_map[idx];

	if (fc->base == 0)
		return;

	/* GPIOs for RX and TX */
	vcore_gpio_set_alt(fc->rx_gpio, fc->gpio_alt);
	vcore_gpio_set_alt(fc->tx_gpio, fc->gpio_alt);

	/* Initialize the console to provide early debug support */
	console_flexcom_register(&lan969x_console,
				 fc->base + FLEXCOM_UART_OFFSET,
				 FLEXCOM_DIVISOR(PERIPHERAL_CLK, br));
	console_set_scope(&lan969x_console,
			  CONSOLE_FLAG_BOOT | CONSOLE_FLAG_RUNTIME);
}

void lan969x_usb_get_trim_values(struct usb_trim *trim)
{
	uint8_t trim_data[TRIM_SIZE];

	memset(trim, 0, sizeof(*trim));

	if (otp_read_trim(trim_data, sizeof(trim_data)) < 0)
		return;		/* OTP read error? */

	if (otp_all_zero(trim_data, sizeof(trim_data)))
		return;		/* Nothing set */

	trim->valid = true;
	trim->bias = otp_read_com_bias_bg_mag_trim();
	trim->rbias = otp_read_com_rbias_mag_trim();
}

void lan969x_console_init(void)
{
	vcore_gpio_init(GCB_GPIO_OUT_SET(LAN969X_GCB_BASE));

	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC_FC:
	case LAN966X_STRAP_BOOT_QSPI_FC:
	case LAN966X_STRAP_BOOT_SD_FC:
	case _LAN966X_STRAP_BOOT_MMC_FC_ALIAS:
	case _LAN966X_STRAP_BOOT_QSPI_FC_ALIAS:
	case _LAN966X_STRAP_BOOT_SD_FC_ALIAS:
		lan969x_flexcom_init(FC_DEFAULT, FLEXCOM_BAUDRATE);
		break;
	case LAN966X_STRAP_TFAMON_FC0:
		lan969x_flexcom_init(FLEXCOM0, FLEXCOM_BAUDRATE);
		break;
	case LAN966X_STRAP_TFAMON_FC0_HS:
		lan969x_flexcom_init(FLEXCOM0, FLEXCOM_BAUDRATE_HS);
		break;
	default:
		/* No console */
		break;
	}
}

uintptr_t plat_get_ns_image_entrypoint(void)
{
	return PLAT_LAN969X_NS_IMAGE_BASE;
}

void lan969x_set_max_trace_level(void)
{
#if !DEBUG
	/* Reduce logging */
	tf_log_set_max_level(LOG_LEVEL_ERROR);
#endif
}

unsigned int plat_get_syscnt_freq2(void)
{
	return SYS_COUNTER_FREQ_IN_TICKS;
	//return read_cntfrq_el0();
}

void lan969x_timer_init(void)
{
	uintptr_t syscnt = LAN969X_CPU_SYSCNT_BASE;

	mmio_write_32(CPU_SYSCNT_CNTCVL(syscnt), 0);	/* Low */
	mmio_write_32(CPU_SYSCNT_CNTCVU(syscnt), 0);	/* High */
	mmio_write_32(CPU_SYSCNT_CNTCR(syscnt),
		      CPU_SYSCNT_CNTCR_CNTCR_EN(1));	/*Enable */

        /* Program the counter frequency */
        write_cntfrq_el0(plat_get_syscnt_freq2());
}

/*******************************************************************************
 * Returns ARM platform specific memory map regions.
 ******************************************************************************/
const mmap_region_t *plat_arm_get_mmap(void)
{
	return plat_arm_mmap;
}

void plat_qspi_init_clock(void)
{
	uint8_t clk = 0;

	lan966x_fw_config_read_uint8(LAN966X_FW_CONF_QSPI_CLK, &clk, QSPI_DEFAULT_SPEED_MHZ);
	/* Clamp to [5MHz ; 100MHz] */
	clk = MAX(clk, (uint8_t) 5);
	clk = MIN(clk, (uint8_t) 100);
	VERBOSE("QSPI: Using clock %u Mhz\n", clk);
	lan966x_clk_disable(LAN966X_CLK_ID_QSPI0);
	lan966x_clk_set_rate(LAN966X_CLK_ID_QSPI0, clk * 1000 * 1000);
	lan966x_clk_enable(LAN966X_CLK_ID_QSPI0);
}

int plat_get_nor_data(struct nor_device *device)
{
	unsigned int mode = qspi_get_spi_mode();

	INFO("QSPI: Using mode 0x%0x\n", mode);

	device->size = SIZE_M(16); /* Normally 2Mb */

	zeromem(&device->read_op, sizeof(struct spi_mem_op));
	if (mode & SPI_RX_QUAD) {
		device->read_op.cmd.opcode = SPI_NOR_OP_READ_1_1_4;
		device->read_op.data.buswidth = SPI_MEM_BUSWIDTH_4_LINE;
	} else {
		device->read_op.cmd.opcode = SPI_NOR_OP_READ_FAST;
		device->read_op.data.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	}
	device->read_op.cmd.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	device->read_op.addr.nbytes = 3U;
	device->read_op.addr.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	device->read_op.dummy.nbytes = 1U;
	device->read_op.dummy.buswidth = SPI_MEM_BUSWIDTH_1_LINE;
	device->read_op.data.dir = SPI_MEM_DATA_IN;

	return 0;
}
