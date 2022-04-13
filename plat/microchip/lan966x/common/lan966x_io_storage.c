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
#include <plat/common/platform.h>

#include "lan966x_private.h"

struct plat_io_policy {
	uintptr_t *dev_handle;
	uintptr_t image_spec;
	int (*check)(const uintptr_t spec);
};

static const io_dev_connector_t *mmc_dev_con;
static uintptr_t fip_dev_handle;
static uintptr_t mmc_dev_handle;
static uintptr_t memmap_dev_handle;
static uintptr_t enc_dev_handle;

static const io_dev_connector_t *fip_dev_con;
static const io_dev_connector_t *memmap_dev_con;
static const io_dev_connector_t *enc_dev_con;

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

/*
 * State data about alternate boot source(s).  Only used in
 * conjunction with BL1 monitor mode.
 */
enum {
	FIP_SELECT_RAM_FIP,
	FIP_SELECT_DEFAULT,
};
static int fip_select;

/* Data will be fetched from the GPT */
static io_block_spec_t fip_mmc_block_spec;

#if BL2_AT_EL3
/* 80k BL2/SPL + 2 * 256 U-Boot Env */
#define FLASH_FIP_OFFSET	(1024 * (80 + 2 * 256))
#else
/* Plain FIP at offset zero */
#define FLASH_FIP_OFFSET	0
#endif
static const io_block_spec_t fip_qspi_block_spec = {
	.offset = LAN966X_QSPI0_MMAP + FLASH_FIP_OFFSET,
	.length = LAN966X_QSPI0_RANGE - FLASH_FIP_OFFSET,
};

static const io_block_spec_t mmc_gpt_spec = {
	.offset	= LAN966X_GPT_BASE,
	.length	= LAN966X_GPT_SIZE,
};

static const io_uuid_spec_t bl2_uuid_spec = {
	.uuid = UUID_TRUSTED_BOOT_FIRMWARE_BL2,
};

static const io_uuid_spec_t bl2u_uuid_spec = {
	.uuid = UUID_TRUSTED_UPDATE_FIRMWARE_BL2U,
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

static const io_uuid_spec_t nt_fw_config_uuid_spec = {
	.uuid = UUID_NT_FW_CONFIG,
};

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

static const io_uuid_spec_t fwu_cert_uuid_spec = {
	.uuid = UUID_TRUSTED_FWU_CERT,
};

static int check_fip(const uintptr_t spec);
static int check_mmc(const uintptr_t spec);
static int check_memmap(const uintptr_t spec);
static int check_error(const uintptr_t spec);
static int check_enc_fip(const uintptr_t spec);

static const struct plat_io_policy policies[] = {
	[ENC_IMAGE_ID] = {
		&fip_dev_handle,
		(uintptr_t)&bl32_uuid_spec, /* Dummy, but must be present in FIP */
		check_fip
	},
	[BL2_IMAGE_ID] = {
		&enc_dev_handle,
		(uintptr_t)&bl2_uuid_spec,
		check_enc_fip
	},
	[BL2U_IMAGE_ID] = {
		&fip_dev_handle,
		(uintptr_t)&bl2u_uuid_spec,
		check_fip
	},
	[BL31_IMAGE_ID] = {
		&fip_dev_handle,
		(uintptr_t)&bl31_uuid_spec,
		check_fip
	},
	[BL32_IMAGE_ID] = {
		&enc_dev_handle,
		(uintptr_t)&bl32_uuid_spec,
		check_enc_fip
	},
	[BL32_EXTRA1_IMAGE_ID] = {
		&enc_dev_handle,
		(uintptr_t)&bl32_extra1_uuid_spec,
		check_enc_fip
	},
	[BL32_EXTRA2_IMAGE_ID] = {
		&enc_dev_handle,
		(uintptr_t)&bl32_extra2_uuid_spec,
		check_enc_fip
	},
	[BL33_IMAGE_ID] = {
		&enc_dev_handle,
		(uintptr_t)&bl33_uuid_spec,
		check_enc_fip
	},
	[FW_CONFIG_ID] = {
		&fip_dev_handle,
		(uintptr_t)&fw_config_uuid_spec,
		check_fip
	},
	[NT_FW_CONFIG_ID] = {
		&fip_dev_handle,
		(uintptr_t)&nt_fw_config_uuid_spec,
		check_fip
	},
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
	[FWU_CERT_ID] = {
		&fip_dev_handle,
		(uintptr_t)&fwu_cert_uuid_spec,
		check_fip
	},
	[GPT_IMAGE_ID] = {
		&mmc_dev_handle,
		(uintptr_t)&mmc_gpt_spec,
		check_mmc
	},
};

static const struct plat_io_policy fallback_policies[] = {
	[BL2_IMAGE_ID] = {
		&fip_dev_handle,
		(uintptr_t)&bl2_uuid_spec,
		check_fip
	},
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
	[BL33_IMAGE_ID] = {
		&fip_dev_handle,
		(uintptr_t)&bl33_uuid_spec,
		check_fip
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

#if defined(IMAGE_BL1)

static io_block_spec_t ram_fip_spec;

void lan966x_bl1_io_enable_ram_fip(size_t offset, size_t length)
{
	ram_fip_spec.offset = offset;
	ram_fip_spec.length = length;
}

static int check_ram_fip(const uintptr_t spec)
{
	int result;
	uintptr_t local_image_handle;

	result = io_dev_init(memmap_dev_handle, (uintptr_t)NULL);
	if (result == 0) {
		result = io_open(memmap_dev_handle, spec, &local_image_handle);
		if (result == 0) {
			VERBOSE("Using RAM FIP\n");
			io_close(local_image_handle);
		}
	}
	return result;
}

static const struct plat_io_policy boot_source_ram_fip = {
	&memmap_dev_handle,
	(uintptr_t) &ram_fip_spec,
	check_ram_fip
};
#endif

/* Check encryption header in payload */
static int check_enc_fip(const uintptr_t spec)
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

	/* Use default FIP from boot source */
	fip_select = FIP_SELECT_DEFAULT;

	lan966x_io_bootsource_init();

	result = register_io_dev_fip(&fip_dev_con);
	assert(result == 0);

	result = io_dev_open(fip_dev_con, (uintptr_t)NULL, &fip_dev_handle);
	assert(result == 0);

	/* These are prepared even if we use SD/MMC */
	result = register_io_dev_memmap(&memmap_dev_con);
	assert(result == 0);

	result = io_dev_open(memmap_dev_con, (uintptr_t)NULL,
			     &memmap_dev_handle);
	assert(result == 0);

	result = register_io_dev_enc(&enc_dev_con);
	assert(result == 0);

	result = io_dev_open(enc_dev_con, (uintptr_t)NULL, &enc_dev_handle);
	assert(result == 0);

	/* Device specific operations */
	switch (lan966x_get_boot_source()) {
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
		break;

	case BOOT_SOURCE_NONE:
		NOTICE("Boot source NONE selected\n");
		break;

	default:
		ERROR("Unknown boot source \n");
		plat_error_handler(-ENOTSUP);
		break;
	}

	/* Ignore improbable errors in release builds */
	(void)result;
}

/*
 * When BL1 has a RAM FIP defined, use that as 1st source.
 */
#if defined(IMAGE_BL1)
bool ram_fip_valid(io_block_spec_t *spec)
{
	if (spec->length == 0 ||
	    spec->offset < BL1_MON_MIN_BASE ||
	    (spec->offset + spec->length) > BL1_MON_LIMIT)
		return false;
	return true;
}

int bl1_plat_handle_pre_image_load(unsigned int image_id)
{
	/* Use RAM FIP only if defined */
	fip_select =
		ram_fip_valid(&ram_fip_spec) ?
		FIP_SELECT_RAM_FIP :
		FIP_SELECT_DEFAULT;

	return 0;
}

int plat_try_next_boot_source(void)
{
	if (fip_select == FIP_SELECT_RAM_FIP) {
		fip_select = FIP_SELECT_DEFAULT;
		return 1;	/* Try again */
	}
	return 0;		/* No more */
}
#endif

static const struct plat_io_policy *current_fip_io_policy(void)
{
#if defined(IMAGE_BL1)
	if (fip_select == FIP_SELECT_RAM_FIP)
		return &boot_source_ram_fip;
#endif
	return &boot_source_policies[lan966x_get_boot_source()];
}

/* Return an IO device handle and specification which can be used to access
 * an image. Use this to enforce platform load policy
 */
int plat_get_image_source(unsigned int image_id, uintptr_t *dev_handle,
			  uintptr_t *image_spec)
{
	int result;
	const struct plat_io_policy *policy;

	assert(image_id < ARRAY_SIZE(policies));

	if (image_id == FIP_IMAGE_ID)
		policy = current_fip_io_policy();
	else
		policy = &policies[image_id];

	result = policy->check(policy->image_spec);
	if (result != 0) {
		if (image_id < ARRAY_SIZE(fallback_policies) &&
		    fallback_policies[image_id].check) {
			policy = &fallback_policies[image_id];
			result = policy->check(policy->image_spec);
			if (result == 0)
				goto done;
		}
		return result;
	}

done:
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
