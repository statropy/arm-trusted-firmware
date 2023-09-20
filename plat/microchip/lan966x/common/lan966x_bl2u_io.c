/*
 * Copyright (c) 2017-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <platform_def.h>

#include <drivers/io/io_block.h>
#include <drivers/io/io_driver.h>
#include <drivers/microchip/qspi.h>
#include <drivers/microchip/tz_matrix.h>
#include <drivers/mmc.h>
#include <drivers/partition/partition.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>

#include <lan966x_regs.h>
#include <lan96xx_common.h>
#include <lan96xx_mmc.h>

struct plat_io_policy {
	uintptr_t *dev_handle;
	uintptr_t image_spec;
	int (*check)(const uintptr_t spec);
};

static const io_dev_connector_t *mmc_dev_con;
static uintptr_t mmc_dev_handle;

static uint8_t mmc_buf[MMC_BLOCK_SIZE * 8] __attribute__ ((aligned (MMC_BLOCK_SIZE)));

static const io_block_dev_spec_t mmc_dev_spec = {
	.buffer = {
		.offset = (uintptr_t) mmc_buf,
		.length = sizeof(mmc_buf),
	},
	.ops = {
		.read = mmc_read_blocks,
		.write = NULL,
		},
	.block_size = MMC_BLOCK_SIZE,
};

static const io_block_spec_t mmc_gpt_spec = {
	.offset	= LAN966X_GPT_BASE,
	.length	= LAN966X_GPT_SIZE,
};

static int check_mmc(const uintptr_t spec)
{
	int result;
	uintptr_t local_image_handle;

	result = io_dev_init(mmc_dev_handle, (uintptr_t) NULL);
	if (result == 0) {
		result = io_open(mmc_dev_handle, spec, &local_image_handle);
		if (result == 0)
			VERBOSE("Using eMMC/SDMMC\n");
			io_close(local_image_handle);
	}
	return result;
}

static const struct plat_io_policy gpt_policy = {
	&mmc_dev_handle,
	(uintptr_t)&mmc_gpt_spec,
	check_mmc
};

void lan966x_bl2u_io_init_dev(boot_source_type boot_source)
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
		qspi_init();

		/* Ensure we have ample reach on QSPI mmap area */
		matrix_configure_srtop(MATRIX_SLAVE_QSPI0,
				       MATRIX_SRTOP(0, MATRIX_SRTOP_VALUE_128M) |
				       MATRIX_SRTOP(1, MATRIX_SRTOP_VALUE_128M));
	default:
		break;
	}
}

void lan966x_io_setup(void)
{
	int result;

	lan966x_io_bootsource_init();

	result = register_io_dev_block(&mmc_dev_con);
	assert(result == 0);

	result = io_dev_open(mmc_dev_con,
			     (uintptr_t)&mmc_dev_spec,
			     &mmc_dev_handle);
	if(result != 0) {
		NOTICE("eMMc init failed");
	}
}

/* Return an IO device handle and specification which can be used to access
 * an image. Use this to enforce platform load policy
 */
int plat_get_image_source(unsigned int image_id, uintptr_t *dev_handle,
			  uintptr_t *image_spec)
{
	int result = -EINVAL;

	if (image_id == GPT_IMAGE_ID) {
		const struct plat_io_policy *policy = &gpt_policy;

		result = policy->check(policy->image_spec);
		if (result == 0) {
			*image_spec = policy->image_spec;
			*dev_handle = *(policy->dev_handle);
		}
	}

	return result;
}
