/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/console.h>
#include <drivers/io/io_storage.h>
#include <drivers/microchip/emmc.h>
#include <drivers/microchip/flexcom_uart.h>
#include <drivers/microchip/lan966x_clock.h>
#include <drivers/microchip/qspi.h>
#include <drivers/microchip/sha.h>
#include <drivers/microchip/tz_matrix.h>
#include <drivers/microchip/vcore_gpio.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>
#include <platform_def.h>

#include <plat/common/platform.h>
#include <plat/arm/common/arm_config.h>
#include <plat/arm/common/plat_arm.h>

#include "lan966x_regs.h"
#include "lan966x_private.h"
#include "plat_otp.h"

CASSERT((BL1_RW_SIZE + BL2_SIZE) <= LAN966X_SRAM_SIZE, assert_sram_depletion);

static console_t lan966x_console;
shared_memory_desc_t shared_memory_desc;

#define FW_CONFIG_INIT_8(offset, value)		\
	.config[offset] = (uint8_t) (value)

#define FW_CONFIG_INIT_32(offset, value)				\
	.config[offset + 0] = (uint8_t) (value),			\
	.config[offset + 1] = (uint8_t) ((value) >> 8),			\
	.config[offset + 2] = (uint8_t) ((value) >> 16),		\
	.config[offset + 3] = (uint8_t) ((value) >> 24)

/* Define global fw_config, set default MMC settings */
lan966x_fw_config_t lan966x_fw_config = {
	FW_CONFIG_INIT_32(LAN966X_FW_CONF_MMC_CLK_RATE, MMC_DEFAULT_SPEED),
	FW_CONFIG_INIT_8(LAN966X_FW_CONF_MMC_BUS_WIDTH, MMC_BUS_WIDTH_1),
	FW_CONFIG_INIT_8(LAN966X_FW_CONF_QSPI_CLK, 25), /* 25Mhz */
};

#define LAN966X_MAP_QSPI0						\
	MAP_REGION_FLAT(						\
		LAN966X_QSPI0_MMAP,					\
		LAN966X_QSPI0_RANGE,					\
		MT_MEMORY | MT_RO | MT_SECURE)

#define LAN966X_MAP_QSPI0_RW						\
	MAP_REGION_FLAT(						\
		LAN966X_QSPI0_MMAP,					\
		LAN966X_QSPI0_RANGE,					\
		MT_DEVICE | MT_RW | MT_SECURE)

#define LAN966X_MAP_AXI							\
	MAP_REGION_FLAT(						\
		LAN966X_DEV_BASE,					\
		LAN966X_DEV_SIZE,					\
		MT_DEVICE | MT_RW | MT_SECURE)

#define LAN966X_MAP_BL32					\
	MAP_REGION_FLAT(					\
		BL32_BASE,					\
		BL32_SIZE,					\
		MT_MEMORY | MT_RW | MT_SECURE)

#define LAN966X_MAP_NS_MEM					\
	MAP_REGION_FLAT(					\
		PLAT_LAN966X_NS_IMAGE_BASE,			\
		PLAT_LAN966X_NS_IMAGE_SIZE,			\
		MT_MEMORY | MT_RW | MT_NS)

#ifdef IMAGE_BL1
const mmap_region_t plat_arm_mmap[] = {
	LAN966X_MAP_QSPI0,
	LAN966X_MAP_AXI,
	{0}
};
#endif
#if defined(IMAGE_BL2)
const mmap_region_t plat_arm_mmap[] = {
	LAN966X_MAP_QSPI0,
	LAN966X_MAP_AXI,
	LAN966X_MAP_BL32,
	LAN966X_MAP_NS_MEM,
	{0}
};
#endif
#if defined(IMAGE_BL2U)
const mmap_region_t plat_arm_mmap[] = {
	LAN966X_MAP_QSPI0_RW,
	LAN966X_MAP_AXI,
	LAN966X_MAP_BL32,
	LAN966X_MAP_NS_MEM,
	{0}
};
#endif
#ifdef IMAGE_BL32
const mmap_region_t plat_arm_mmap[] = {
	LAN966X_MAP_QSPI0,
	LAN966X_MAP_AXI,
	LAN966X_MAP_BL32,
	LAN966X_MAP_NS_MEM,
	{0}
};
#endif

enum lan966x_flexcom_id {
	FLEXCOM0 = 0,
	FLEXCOM1,
	FLEXCOM2,
	FLEXCOM3,
	FLEXCOM4,
};

static struct lan966x_flexcom_args {
	uintptr_t base;
	int rx_gpio;
	int tx_gpio;
} lan966x_flexcom_map[] = {
	[FLEXCOM0] = {
		LAN966X_FLEXCOM_0_BASE, 25, 26
	},
	[FLEXCOM1] = { 0 },
	[FLEXCOM2] = {
		LAN966X_FLEXCOM_2_BASE, 44, 45
	},
	[FLEXCOM3] = {
		LAN966X_FLEXCOM_3_BASE, 52, 53
	},
	[FLEXCOM4] = {
		LAN966X_FLEXCOM_4_BASE, 57, 58
	},
};

/*******************************************************************************
 * Returns ARM platform specific memory map regions.
 ******************************************************************************/
const mmap_region_t *plat_arm_get_mmap(void)
{
	return plat_arm_mmap;
}

static void lan966x_flexcom_init(int idx)
{
	struct lan966x_flexcom_args *fc;

	if (idx < 0)
		return;

	fc = &lan966x_flexcom_map[idx];

	if (fc->base == 0)
		return;

	/* GPIOs for RX and TX */
	vcore_gpio_set_alt(fc->rx_gpio, 1);
	vcore_gpio_set_alt(fc->tx_gpio, 1);

	/* Initialize the console to provide early debug support */
	console_flexcom_register(&lan966x_console,
				 fc->base + FLEXCOM_UART_OFFSET,
				 FLEXCOM_DIVISOR(PERIPHERAL_CLK, FLEXCOM_BAUDRATE));
	console_set_scope(&lan966x_console,
			  CONSOLE_FLAG_BOOT | CONSOLE_FLAG_RUNTIME);
	lan966x_crash_console(&lan966x_console);
}

void lan966x_console_init(void)
{
	vcore_gpio_init(GCB_GPIO_OUT_SET(LAN966X_GCB_BASE));

	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC_FC:
	case LAN966X_STRAP_BOOT_QSPI_FC:
	case LAN966X_STRAP_BOOT_SD_FC:
	case LAN966X_STRAP_BOOT_MMC_TFAMON_FC:
	case LAN966X_STRAP_BOOT_QSPI_TFAMON_FC:
	case LAN966X_STRAP_BOOT_SD_TFAMON_FC:
		lan966x_flexcom_init(FC_DEFAULT);
		break;
	case LAN966X_STRAP_TFAMON_FC0:
		lan966x_flexcom_init(FLEXCOM0);
		break;
	case LAN966X_STRAP_TFAMON_FC2:
		lan966x_flexcom_init(FLEXCOM2);
		break;
	case LAN966X_STRAP_TFAMON_FC3:
		lan966x_flexcom_init(FLEXCOM3);
		break;
	case LAN966X_STRAP_TFAMON_FC4:
		lan966x_flexcom_init(FLEXCOM4);
		break;
	default:
		/* No console */
		break;
	}
}

void lan966x_io_init_dev(boot_source_type boot_source)
{
	switch (boot_source) {
	case BOOT_SOURCE_EMMC:
	case BOOT_SOURCE_SDMMC:
		/* Setup MMC */
		lan966x_mmc_plat_config(boot_source);
		break;

	case BOOT_SOURCE_QSPI:
		/* We own SPI */
		mmio_setbits_32(CPU_GENERAL_CTRL(LAN966X_CPU_BASE),
				CPU_GENERAL_CTRL_IF_SI_OWNER_M);

		/* Enable memmap access */
		qspi_init(LAN966X_QSPI_0_BASE);

		/* Ensure we have ample reach on QSPI mmap area */
		/* 16M should be more than adequate - EVB/SVB have 2M */
		matrix_configure_srtop(MATRIX_SLAVE_QSPI0,
				       MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_16M) |
				       MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_16M));

		/* Enable QSPI0 for NS access */
		matrix_configure_slave_security(MATRIX_SLAVE_QSPI0,
						MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_16M) |
						MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_16M),
						MATRIX_SASPLIT(0, MATRIX_SRTOP_VALUE_16M),
						MATRIX_LANSECH_NS(0));
	default:
		break;
	}
}

void lan966x_io_bootsource_init(void)
{
	lan966x_io_init_dev(lan966x_get_boot_source());
}

void lan966x_timer_init(void)
{
	uintptr_t syscnt = LAN966X_CPU_SYSCNT_BASE;

	mmio_write_32(CPU_SYSCNT_CNTCVL(syscnt), 0);	/* Low */
	mmio_write_32(CPU_SYSCNT_CNTCVU(syscnt), 0);	/* High */
	mmio_write_32(CPU_SYSCNT_CNTCR(syscnt),
		      CPU_SYSCNT_CNTCR_CNTCR_EN(1));	/*Enable */

        /* Program the counter frequency */
        write_cntfrq_el0(plat_get_syscnt_freq2());
}

unsigned int plat_get_syscnt_freq2(void)
{
	return SYS_COUNTER_FREQ_IN_TICKS;
}

#define GPR0_STRAPPING_SET	BIT(20) /* 0x100000 */

/*
 * Read strapping into GPR(0) to allow override
 */
void lan966x_init_strapping(void)
{
	uint32_t status;
	uint8_t strapping;

	status = mmio_read_32(CPU_GENERAL_STAT(LAN966X_CPU_BASE));
	strapping = CPU_GENERAL_STAT_VCORE_CFG_X(status);
	mmio_write_32(CPU_GPR(LAN966X_CPU_BASE, 0), GPR0_STRAPPING_SET | strapping);
}

uint8_t lan966x_get_strapping(void)
{
	uint32_t status;
	uint8_t strapping;

	status = mmio_read_32(CPU_GPR(LAN966X_CPU_BASE, 0));
	assert(status & GPR0_STRAPPING_SET);
	strapping = CPU_GENERAL_STAT_VCORE_CFG_X(status);

	VERBOSE("VCORE_CFG = %d\n", strapping);

	return strapping;
}

void lan966x_set_strapping(uint8_t value)
{

	/* To override strapping previous boot src must be 'none' */
	if (lan966x_get_boot_source() == BOOT_SOURCE_NONE) {
		/* And new strapping should be limited as below */
		if (value == LAN966X_STRAP_BOOT_MMC ||
		    value == LAN966X_STRAP_BOOT_QSPI ||
		    value == LAN966X_STRAP_BOOT_SD ||
		    value == LAN966X_STRAP_PCIE_ENDPOINT) {
			NOTICE("OVERRIDE strapping = 0x%08x\n", value);
			mmio_write_32(CPU_GPR(LAN966X_CPU_BASE, 0), GPR0_STRAPPING_SET | value);
			/* Do initialization according to new source */
			lan966x_io_bootsource_init();
		} else {
			ERROR("Strap override %d illegal\n", value);
		}
	} else {
		ERROR("Strap override is illegal if boot source is already valid\n");
	}
}

bool lan966x_monitor_enabled(void)
{
	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC_TFAMON_FC:
	case LAN966X_STRAP_BOOT_QSPI_TFAMON_FC:
	case LAN966X_STRAP_BOOT_SD_TFAMON_FC:
	case LAN966X_STRAP_TFAMON_FC0:
	case LAN966X_STRAP_TFAMON_FC2:
	case LAN966X_STRAP_TFAMON_FC3:
	case LAN966X_STRAP_TFAMON_FC4:
		return true;
	default:
		break;
	}

	return false;
}

boot_source_type lan966x_get_boot_source(void)
{
	boot_source_type boot_source;

	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC:
	case LAN966X_STRAP_BOOT_MMC_FC:
	case LAN966X_STRAP_BOOT_MMC_TFAMON_FC:
		boot_source = BOOT_SOURCE_EMMC;
		break;
	case LAN966X_STRAP_BOOT_QSPI:
	case LAN966X_STRAP_BOOT_QSPI_FC:
	case LAN966X_STRAP_BOOT_QSPI_TFAMON_FC:
		boot_source = BOOT_SOURCE_QSPI;
		break;
	case LAN966X_STRAP_BOOT_SD:
	case LAN966X_STRAP_BOOT_SD_FC:
	case LAN966X_STRAP_BOOT_SD_TFAMON_FC:
		boot_source = BOOT_SOURCE_SDMMC;
		break;
	default:
		boot_source = BOOT_SOURCE_NONE;
		break;
	}

	return boot_source;
}

void lan966x_fwconfig_apply(void)
{
	boot_source_type boot_source = lan966x_get_boot_source();

	/* Update storage drivers with new values from fw_config */
	switch (boot_source) {
	case BOOT_SOURCE_QSPI:
		qspi_reinit();
		break;
	case BOOT_SOURCE_SDMMC:
	case BOOT_SOURCE_EMMC:
		lan966x_mmc_plat_config(boot_source);
		break;
	default:
		break;
	}
}

int lan966x_load_fw_config_raw(unsigned int image_id)
{
	uintptr_t dev_handle, image_handle, image_spec = 0;
	size_t bytes_read;
	int result;

	result = plat_get_image_source(image_id, &dev_handle, &image_spec);
	if (result != 0) {
		WARN("Failed to obtain reference to image id=%u (%i)\n",
		     image_id, result);
		return result;
	}

	result = io_open(dev_handle, image_spec, &image_handle);
	if (result != 0) {
		WARN("Failed to access image id=%u (%i)\n", image_id, result);
		return result;
	}

	result = io_read(image_handle, (uintptr_t)&lan966x_fw_config,
			 sizeof(lan966x_fw_config), &bytes_read);
	if (result != 0)
		WARN("Failed to read data (%i)\n", result);

	io_close(image_handle);

#ifdef IMAGE_BL1
	/* This is fwd to BL2 */
	flush_dcache_range((uintptr_t)&lan966x_fw_config, sizeof(lan966x_fw_config));
#endif

	return result;
}

int lan966x_load_fw_config(unsigned int image_id)
{
	uint8_t config[FW_CONFIG_MAX_DATA];
	int result = 0;

	/* Save the current config */
	memcpy(config, lan966x_fw_config.config, sizeof(config));

	/* Make sure OTP emu is initialized */
	otp_emu_init();

	/* If OTP emu active, read fw_config raw/unauthenticated */
	if (otp_in_emulation()) {
		result = lan966x_load_fw_config_raw(image_id);
	}

#ifdef IMAGE_BL1
	/* Only authenticate at BL1 */
	{
		image_desc_t *desc = bl1_plat_get_image_desc(image_id);

		if (desc == NULL) {
			result = -ENOENT;
		} else {
			result = load_auth_image(image_id, &desc->image_info);
			if (result != 0)
				ERROR("FW_CONFIG did not authenticate: rc %d\n", result);
		}
	}
#endif

	/* If something went wrong, restore fw_config.config, zero OTP emu */
	if (result != 0) {
		memset(&lan966x_fw_config, 0, sizeof(lan966x_fw_config));
		memcpy(lan966x_fw_config.config, config, sizeof(config));
	}

	return result;
}

static int fw_config_read_bytes(unsigned int offset,
				unsigned int num_bytes,
				uint8_t *dst)
{
	int ret_val = -1;
	int cnt;
	uint8_t data;

	assert(num_bytes > 0);
	assert((offset + num_bytes) < (sizeof(lan966x_fw_config.config)));

	if (offset < LAN966X_FW_CONF_NUM_OF_ITEMS) {
		for (cnt = 0; cnt < num_bytes; cnt++) {
			data = lan966x_fw_config.config[offset + cnt];
			*dst++ = data;
		}

		ret_val = 0;
	} else {
		ERROR("Illegal offset access to fw_config structure\n");
	}

	return ret_val;
}

int lan966x_fw_config_read_uint8(unsigned int offset, uint8_t *dst)
{
	return fw_config_read_bytes(offset, 1, (uint8_t *)dst);
}

int lan966x_fw_config_read_uint16(unsigned int offset, uint16_t *dst)
{
	return fw_config_read_bytes(offset, 2, (uint8_t *)dst);
}

int lan966x_fw_config_read_uint32(unsigned int offset, uint32_t *dst)
{
	return fw_config_read_bytes(offset, 4, (uint8_t *)dst);
}

/*
 * Derive a 32 byte key with a 32 byte salt, output a 32 byte key
 */
int lan966x_derive_key(const lan966x_key32_t *in,
		       const lan966x_key32_t *salt,
		       lan966x_key32_t *out)
{
	uint8_t buf[LAN966X_KEY32_LEN * 2];
	int ret;

	/* Use one contiguous buffer for now */
	memcpy(buf, in->b, LAN966X_KEY32_LEN);
	memcpy(buf + LAN966X_KEY32_LEN, salt->b, LAN966X_KEY32_LEN);

	ret = sha_calc(SHA_MR_ALGO_SHA256, buf, sizeof(buf), out->b);

	/* Don't leak */
	memset(buf, 0, sizeof(buf));

	return ret;
}

/*
 * Some release build strapping modes will only show error traces by default
 */
void lan966x_set_max_trace_level(void)
{
#if !DEBUG
	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC:
	case LAN966X_STRAP_BOOT_QSPI:
	case LAN966X_STRAP_BOOT_SD:
	case LAN966X_STRAP_PCIE_ENDPOINT:
	case LAN966X_STRAP_TFAMON_FC0:
	case LAN966X_STRAP_TFAMON_FC2:
	case LAN966X_STRAP_TFAMON_FC3:
	case LAN966X_STRAP_TFAMON_FC4:
	case LAN966X_STRAP_SPI_SLAVE:
		tf_log_set_max_level(LOG_LEVEL_ERROR);
		break;
	default:
		/* No change in trace level */
		break;
	}
#endif
}

/*
 * Restore the loglevel for certain strapping modes release builds.
 */
void lan966x_reset_max_trace_level(void)
{
#if !DEBUG
	switch (lan966x_get_strapping()) {
	case LAN966X_STRAP_BOOT_MMC:
	case LAN966X_STRAP_BOOT_QSPI:
	case LAN966X_STRAP_BOOT_SD:
	case LAN966X_STRAP_PCIE_ENDPOINT:
	case LAN966X_STRAP_TFAMON_FC0:
	case LAN966X_STRAP_TFAMON_FC2:
	case LAN966X_STRAP_TFAMON_FC3:
	case LAN966X_STRAP_TFAMON_FC4:
	case LAN966X_STRAP_SPI_SLAVE:
		tf_log_set_max_level(LOG_LEVEL);
		break;
	default:
		/* No change in trace level */
		break;
	}
#endif
}

/*
 * Check if the current boot strapping mode has been masked out by the
 * OTP strapping mask and abort if this is the case.
 */
void lan966x_validate_strapping(void)
{
	union {
		uint8_t b[2];
		uint16_t w;
	} mask;
	uint16_t strapmask = BIT(lan966x_get_strapping());

	if (otp_read_otp_strap_disable_mask(mask.b, OTP_STRAP_DISABLE_MASK_SIZE)) {
		return;
	}
	if (strapmask & mask.w) {
		ERROR("Bootstrapping masked: %u\n", lan966x_get_strapping());
		plat_error_handler(-EINVAL);
	}
}
