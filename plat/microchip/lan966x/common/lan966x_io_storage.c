/*
 * Copyright (c) 2017-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <platform_def.h>

#include <arch_helpers.h>
#include <common/debug.h>
#include <drivers/io/io_driver.h>
#include <drivers/io/io_encrypted.h>
#include <drivers/io/io_fip.h>
#include <drivers/io/io_block.h>
#include <drivers/io/io_memmap.h>
#include <drivers/io/io_storage.h>
#include <drivers/mmc.h>
#include <drivers/partition/partition.h>
#include <lib/mmio.h>
#include <tools_share/firmware_image_package.h>

#include "lan966x_private.h"

struct plat_io_policy {
	uintptr_t *dev_handle;
	uintptr_t image_spec;
	int (*check)(const uintptr_t spec);
};

static const io_dev_connector_t *fip_dev_con;
static const io_dev_connector_t *mmc_dev_con;
static const io_dev_connector_t *memmap_dev_con;
static uintptr_t fip_dev_handle;
static uintptr_t mmc_dev_handle;
static uintptr_t memmap_dev_handle;
#ifndef DECRYPTION_SUPPORT_none
static const io_dev_connector_t *enc_dev_con;
static uintptr_t enc_dev_handle;
#endif

#if defined(IMAGE_BL2)
static uint8_t mmc_buf[MMC_BUF_SIZE] __attribute__ ((aligned (512)));
#endif

static const io_block_dev_spec_t mmc_dev_spec = {
	.buffer = {
#if defined(IMAGE_BL1)
		.offset = (uintptr_t) BL1_MMC_BUF_BASE,
#elif defined(IMAGE_BL2)
		.offset = (uintptr_t) mmc_buf,
#endif
		.length = MMC_BUF_SIZE,
	},
	.ops = {
		.read = mmc_read_blocks,
		.write = NULL,
		},
	.block_size = MMC_BLOCK_SIZE,
};

/* Data will be fetched from the GPT */
static io_block_spec_t fip_mmc_block_spec;

/* 80k BL2/SPL + 2 * 256 U-Boot Env */
#define FLASH_FIP_OFFSET	(1024 * (80 + 2 * 256))
static const io_block_spec_t fip_qspi_block_spec = {
	.offset = LAN996X_QSPI0_MMAP + FLASH_FIP_OFFSET,
	.length = LAN996X_QSPI0_RANGE - FLASH_FIP_OFFSET,
};

static const io_block_spec_t mmc_gpt_spec = {
	.offset	= LAN966X_GPT_BASE,
	.length	= LAN966X_GPT_SIZE,
};

static const io_uuid_spec_t bl2_uuid_spec = {
	.uuid = UUID_TRUSTED_BOOT_FIRMWARE_BL2,
};

static const io_uuid_spec_t bl31_uuid_spec = {
	.uuid = UUID_EL3_RUNTIME_FIRMWARE_BL31,
};

static const io_uuid_spec_t bl32_uuid_spec = {
	.uuid = UUID_SECURE_PAYLOAD_BL32,
};

static const io_uuid_spec_t bl32_extra1_uuid_spec = {
	.uuid = UUID_SECURE_PAYLOAD_BL32_EXTRA1,
};

static const io_uuid_spec_t bl32_extra2_uuid_spec = {
	.uuid = UUID_SECURE_PAYLOAD_BL32_EXTRA2,
};

static const io_uuid_spec_t bl33_uuid_spec = {
	.uuid = UUID_NON_TRUSTED_FIRMWARE_BL33,
};

static const io_uuid_spec_t fw_config_uuid_spec = {
	.uuid = UUID_FW_CONFIG,
};

#if TRUSTED_BOARD_BOOT
static const io_uuid_spec_t tb_fw_cert_uuid_spec = {
	.uuid = UUID_TRUSTED_BOOT_FW_CERT,
};

static const io_uuid_spec_t trusted_key_cert_uuid_spec = {
	.uuid = UUID_TRUSTED_KEY_CERT,
};

static const io_uuid_spec_t scp_fw_key_cert_uuid_spec = {
	.uuid = UUID_SCP_FW_KEY_CERT,
};

static const io_uuid_spec_t soc_fw_key_cert_uuid_spec = {
	.uuid = UUID_SOC_FW_KEY_CERT,
};

static const io_uuid_spec_t tos_fw_key_cert_uuid_spec = {
	.uuid = UUID_TRUSTED_OS_FW_KEY_CERT,
};

static const io_uuid_spec_t nt_fw_key_cert_uuid_spec = {
	.uuid = UUID_NON_TRUSTED_FW_KEY_CERT,
};

static const io_uuid_spec_t scp_fw_cert_uuid_spec = {
	.uuid = UUID_SCP_FW_CONTENT_CERT,
};

static const io_uuid_spec_t soc_fw_cert_uuid_spec = {
	.uuid = UUID_SOC_FW_CONTENT_CERT,
};

static const io_uuid_spec_t tos_fw_cert_uuid_spec = {
	.uuid = UUID_TRUSTED_OS_FW_CONTENT_CERT,
};

static const io_uuid_spec_t nt_fw_cert_uuid_spec = {
	.uuid = UUID_NON_TRUSTED_FW_CONTENT_CERT,
};
#endif /* TRUSTED_BOARD_BOOT */

static int check_fip(const uintptr_t spec);
static int check_mmc(const uintptr_t spec);
static int check_memmap(const uintptr_t spec);
static int check_error(const uintptr_t spec);
#ifndef DECRYPTION_SUPPORT_none
static int open_enc_fip(const uintptr_t spec);
#endif

static const struct plat_io_policy policies[] = {
	[FIP_IMAGE_ID] = {
		&mmc_dev_handle,
		(uintptr_t)&fip_mmc_block_spec,
		check_mmc
	},
	[ENC_IMAGE_ID] = {
		&fip_dev_handle,
		(uintptr_t)&bl32_uuid_spec, /* Dummy */
		check_fip
	},
	[BL2_IMAGE_ID] = {
		&fip_dev_handle,
		(uintptr_t)&bl2_uuid_spec,
		check_fip
	},
	[BL31_IMAGE_ID] = {
		&fip_dev_handle,
		(uintptr_t)&bl31_uuid_spec,
		check_fip
	},
#if ENCRYPT_BL32 && !defined(DECRYPTION_SUPPORT_none)
	[BL32_IMAGE_ID] = {
		&enc_dev_handle,
		(uintptr_t)&bl32_uuid_spec,
		open_enc_fip
	},
	[BL32_EXTRA1_IMAGE_ID] = {
		&enc_dev_handle,
		(uintptr_t)&bl32_extra1_uuid_spec,
		open_enc_fip
	},
	[BL32_EXTRA2_IMAGE_ID] = {
		&enc_dev_handle,
		(uintptr_t)&bl32_extra2_uuid_spec,
		open_enc_fip
	},
#else
	[BL32_IMAGE_ID] = {
		&fip_dev_handle,
		(uintptr_t)&bl32_uuid_spec,
		check_fip
	},
	[BL32_EXTRA1_IMAGE_ID] = {
		&fip_dev_handle,
		(uintptr_t)&bl32_extra1_uuid_spec,
		check_fip
	},
	[BL32_EXTRA2_IMAGE_ID] = {
		&fip_dev_handle,
		(uintptr_t)&bl32_extra2_uuid_spec,
		check_fip
	},
#endif
	[BL33_IMAGE_ID] = {
		&fip_dev_handle,
		(uintptr_t)&bl33_uuid_spec,
		check_fip
	},
	[FW_CONFIG_ID] = {
		&fip_dev_handle,
		(uintptr_t)&fw_config_uuid_spec,
		check_fip
	},
#if TRUSTED_BOARD_BOOT
	[TRUSTED_BOOT_FW_CERT_ID] = {
		&fip_dev_handle,
		(uintptr_t)&tb_fw_cert_uuid_spec,
		check_fip
	},
	[TRUSTED_KEY_CERT_ID] = {
		&fip_dev_handle,
		(uintptr_t)&trusted_key_cert_uuid_spec,
		check_fip
	},
	[SCP_FW_KEY_CERT_ID] = {
		&fip_dev_handle,
		(uintptr_t)&scp_fw_key_cert_uuid_spec,
		check_fip
	},
	[SOC_FW_KEY_CERT_ID] = {
		&fip_dev_handle,
		(uintptr_t)&soc_fw_key_cert_uuid_spec,
		check_fip
	},
	[TRUSTED_OS_FW_KEY_CERT_ID] = {
		&fip_dev_handle,
		(uintptr_t)&tos_fw_key_cert_uuid_spec,
		check_fip
	},
	[NON_TRUSTED_FW_KEY_CERT_ID] = {
		&fip_dev_handle,
		(uintptr_t)&nt_fw_key_cert_uuid_spec,
		check_fip
	},
	[SCP_FW_CONTENT_CERT_ID] = {
		&fip_dev_handle,
		(uintptr_t)&scp_fw_cert_uuid_spec,
		check_fip
	},
	[SOC_FW_CONTENT_CERT_ID] = {
		&fip_dev_handle,
		(uintptr_t)&soc_fw_cert_uuid_spec,
		check_fip
	},
	[TRUSTED_OS_FW_CONTENT_CERT_ID] = {
		&fip_dev_handle,
		(uintptr_t)&tos_fw_cert_uuid_spec,
		check_fip
	},
	[NON_TRUSTED_FW_CONTENT_CERT_ID] = {
		&fip_dev_handle,
		(uintptr_t)&nt_fw_cert_uuid_spec,
		check_fip
	},
#endif /* TRUSTED_BOARD_BOOT */

	[GPT_IMAGE_ID] = {
		&mmc_dev_handle,
		(uintptr_t)&mmc_gpt_spec,
		check_mmc
	},
};

/* Set io_policy structures for allowing boot from MMC or QSPI */
static const struct plat_io_policy boot_source_policies[] = {
	[BOOT_SOURCE_EMMC] = {
		&mmc_dev_handle,
		(uintptr_t)&fip_mmc_block_spec,
		check_mmc
	},
	[BOOT_SOURCE_QSPI] = {
		&memmap_dev_handle,
		(uintptr_t)&fip_qspi_block_spec,
		check_memmap
	},
	[BOOT_SOURCE_SDMMC] = {
		&mmc_dev_handle,
		(uintptr_t)&fip_mmc_block_spec,
		check_mmc
	},
	[BOOT_SOURCE_NONE] = { 0, 0, check_error }
};

#ifndef DECRYPTION_SUPPORT_none
static int open_enc_fip(const uintptr_t spec)
{
	int result;
	uintptr_t local_image_handle;

	/* See if an encrypted FIP is available */
	result = io_dev_init(enc_dev_handle, (uintptr_t)ENC_IMAGE_ID);
	if (result == 0) {
		result = io_open(enc_dev_handle, spec, &local_image_handle);
		if (result == 0) {
			VERBOSE("Using encrypted FIP\n");
			io_close(local_image_handle);
		}
	}
	return result;
}
#endif

static int check_fip(const uintptr_t spec)
{
	int result;
	uintptr_t local_image_handle;

	/* See if a Firmware Image Package is available */
	result = io_dev_init(fip_dev_handle, (uintptr_t)FIP_IMAGE_ID);
	if (result == 0) {
		result = io_open(fip_dev_handle, spec, &local_image_handle);
		if (result == 0) {
			VERBOSE("Using FIP\n");
			io_close(local_image_handle);
		}
	}
	return result;
}

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

static int check_memmap(const uintptr_t spec)
{
	int result;
	uintptr_t local_image_handle;

	result = io_dev_init(memmap_dev_handle, (uintptr_t)NULL);
	if (result == 0) {
		result = io_open(memmap_dev_handle, spec, &local_image_handle);
		if (result == 0) {
			VERBOSE("Using QSPI\n");
			io_close(local_image_handle);
		}
	}
	return result;
}

static int check_error(const uintptr_t spec)
{
	return -1;
}

void lan966x_io_setup(void)
{
	int result;
	boot_source_type boot_source;

	lan966x_io_bootsource_init();

	result = register_io_dev_fip(&fip_dev_con);
	assert(result == 0);

	result = io_dev_open(fip_dev_con, (uintptr_t)NULL, &fip_dev_handle);
	assert(result == 0);

	boot_source = lan966x_get_boot_source();

	switch (boot_source) {
	case BOOT_SOURCE_EMMC:
	case BOOT_SOURCE_SDMMC:
		result = register_io_dev_block(&mmc_dev_con);
		assert(result == 0);

		result = io_dev_open(mmc_dev_con, (uintptr_t)&mmc_dev_spec,
				     &mmc_dev_handle);
		assert(result == 0);

		lan966x_set_fip_addr(GPT_IMAGE_ID, FW_PARTITION_NAME);
		break;

	case BOOT_SOURCE_QSPI:
		result = register_io_dev_memmap(&memmap_dev_con);
		assert(result == 0);

		result = io_dev_open(memmap_dev_con, (uintptr_t)NULL,
				     &memmap_dev_handle);
		assert(result == 0);
		break;

	case BOOT_SOURCE_NONE:
		NOTICE("Boot source NONE selected\n");
		break;

	default:
		ERROR("Unknown boot source \n");
		panic();
		break;
	}

#ifndef DECRYPTION_SUPPORT_none
	result = register_io_dev_enc(&enc_dev_con);
	assert(result == 0);

	result = io_dev_open(enc_dev_con, (uintptr_t)NULL,
			     &enc_dev_handle);
	assert(result == 0);
#endif

	/* Ignore improbable errors in release builds */
	(void)result;
}

/* Return an IO device handle and specification which can be used to access
 * an image. Use this to enforce platform load policy
 */
int plat_get_image_source(unsigned int image_id, uintptr_t *dev_handle,
			  uintptr_t *image_spec)
{
	int result;
	uint32_t boot_source;
	const struct plat_io_policy *policy;

	assert(image_id < ARRAY_SIZE(policies));

	boot_source = lan966x_get_boot_source();
	if (image_id == FIP_IMAGE_ID)
		policy = &boot_source_policies[boot_source];
	else
		policy = &policies[image_id];

	result = policy->check(policy->image_spec);
	if (result != 0)
		return result;

	*image_spec = policy->image_spec;
	*dev_handle = *(policy->dev_handle);

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
				panic();
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
