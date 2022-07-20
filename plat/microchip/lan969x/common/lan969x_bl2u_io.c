/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>

#include <platform_def.h>

#include <drivers/io/io_block.h>
#include <drivers/io/io_driver.h>
#include <drivers/io/io_mtd.h>
#include <drivers/microchip/qspi.h>
#include <drivers/microchip/tz_matrix.h>
#include <drivers/mmc.h>
#include <drivers/partition/partition.h>
#include <drivers/spi_nor.h>
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

/* Data will be fetched from the GPT */
static io_block_spec_t fip_mmc_block_spec;

static const io_dev_connector_t *mmc_dev_con;
static const io_dev_connector_t *spi_dev_con;
static uintptr_t mmc_dev_handle;
static uintptr_t mtd_dev_handle;

static uint8_t mmc_buf[MMC_BUF_SIZE] __attribute__ ((aligned (512)));

static const io_block_dev_spec_t mmc_dev_spec = {
	.buffer = {
		.offset = (uintptr_t) mmc_buf,
		.length = MMC_BUF_SIZE,
	},
	.ops = {
		.read = mmc_read_blocks,
		.write = NULL,
		},
	.block_size = MMC_BLOCK_SIZE,
};

static io_mtd_dev_spec_t spi_nor_dev_spec = {
	.ops = {
		.init = spi_nor_init,
		.read = spi_nor_read,
	},
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

static void lan966x_io_setup_mmc(void)
{
	int result;

	result = register_io_dev_block(&mmc_dev_con);
	assert(result == 0);

	result = io_dev_open(mmc_dev_con,
			     (uintptr_t)&mmc_dev_spec,
			     &mmc_dev_handle);
	if(result != 0) {
		NOTICE("eMMc init failed");
	}
}

static void lan966x_io_setup_spi(void)
{
	int result __unused;

	result = register_io_dev_mtd(&spi_dev_con);
	assert(result == 0);

	/* Open connections to device */
	result = io_dev_open(spi_dev_con,
			     (uintptr_t)&spi_nor_dev_spec,
			     &mtd_dev_handle);
	assert(result == 0);
}

void lan966x_bl2u_io_init_dev(boot_source_type boot_source)
{
	static bool mmc_init_done, spi_init_done;

	switch (boot_source) {
	case BOOT_SOURCE_EMMC:
	case BOOT_SOURCE_SDMMC:
		if (mmc_init_done)
			return;

		/* Setup MMC */
		lan966x_mmc_plat_config(boot_source);
		lan966x_io_setup_mmc();

		/* Record state */
		mmc_init_done = true;
		break;

	case BOOT_SOURCE_QSPI:
		if (spi_init_done)
			return;

		/* We own SPI */
		mmio_setbits_32(CPU_GENERAL_CTRL(LAN966X_CPU_BASE),
				CPU_GENERAL_CTRL_IF_SI_OWNER_M);

		/* Enable memmap access */
		qspi_init();

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

		lan966x_io_setup_spi();

		/* Record state */
		spi_init_done = true;

	default:
		break;
	}
}

void lan966x_io_setup(void)
{
	/* Nop - no default IO devices to set up */
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

int lan966x_set_fip_addr(unsigned int image_id, const char *name)
{
	const partition_entry_t *entry;

	if (fip_mmc_block_spec.length == 0) {
		partition_init(image_id);
		entry = get_partition_entry(name);
		if (entry == NULL) {
			INFO("Could not find the '%s' partition! Seek for "
							"fallback partition !\n",
							name);
			entry = get_partition_entry(FW_BACKUP_PARTITION_NAME);
			if (entry == NULL) {
				ERROR("No valid partition found !\n");
				plat_error_handler(-ENOTBLK);
			}
		} else {
			INFO("Find the '%s' partition, fetch FIP configuration "
							"data\n", name);
		}

		fip_mmc_block_spec.offset = entry->start;
		fip_mmc_block_spec.length = entry->length;
	}

	return 0;
}
