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
#include <drivers/io/io_block.h>
#include <drivers/io/io_driver.h>
#include <drivers/io/io_fip.h>
#include <drivers/io/io_memmap.h>
#include <drivers/io/io_storage.h>
#include <drivers/mmc.h>
#include <drivers/microchip/emmc.h>
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
static uintptr_t fip_dev_handle;
static const io_dev_connector_t *memmap_dev_con;
static uintptr_t memmap_dev_handle;
static const io_dev_connector_t *emmc_dev_con;
static uintptr_t emmc_dev_handle;

static uint32_t block_buffer[MMC_BLOCK_SIZE] __aligned(MMC_BLOCK_SIZE);
static const io_block_dev_spec_t emmc_dev_spec = {
	.buffer = {
		   .offset = (size_t)&block_buffer,
		   .length = MMC_BLOCK_SIZE,
		   },
	.ops = {
		.read = lan966x_read_single_block,
		.write = NULL,
		},
	.block_size = MMC_BLOCK_SIZE,
};

/* ToDo: check parameters */
static const io_block_spec_t emmc_gpt_spec = {
	.offset		= 0,
	.length		= MMC_BLOCK_SIZE,
};

#define FLASH_FIP_OFFSET	0x180000 /* 1.5M for BL2/SPL + U-Boot */
static const io_block_spec_t fip_block_spec = {
	.offset = LAN996X_QSPI0_MMAP + FLASH_FIP_OFFSET,
	.length = LAN996X_QSPI0_RANGE - FLASH_FIP_OFFSET,
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

static const io_uuid_spec_t tb_fw_config_uuid_spec = {
	.uuid = UUID_TB_FW_CONFIG,
};

static const io_uuid_spec_t hw_config_uuid_spec = {
	.uuid = UUID_HW_CONFIG,
};

static const io_uuid_spec_t soc_fw_config_uuid_spec = {
	.uuid = UUID_SOC_FW_CONFIG,
};

static const io_uuid_spec_t tos_fw_config_uuid_spec = {
	.uuid = UUID_TOS_FW_CONFIG,
};

static const io_uuid_spec_t nt_fw_config_uuid_spec = {
	.uuid = UUID_NT_FW_CONFIG,
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
static int check_memmap(const uintptr_t spec);
static int check_emmc(const uintptr_t spec);

static const struct plat_io_policy policies[] = {
	[FIP_IMAGE_ID] = {
		&memmap_dev_handle,
		(uintptr_t)&fip_block_spec,
		check_memmap
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
	[TB_FW_CONFIG_ID] = {
		&fip_dev_handle,
		(uintptr_t)&tb_fw_config_uuid_spec,
		check_fip
	},
	[HW_CONFIG_ID] = {
		&fip_dev_handle,
		(uintptr_t)&hw_config_uuid_spec,
		check_fip
	},
	[SOC_FW_CONFIG_ID] = {
		&fip_dev_handle,
		(uintptr_t)&soc_fw_config_uuid_spec,
		check_fip
	},
	[TOS_FW_CONFIG_ID] = {
		&fip_dev_handle,
		(uintptr_t)&tos_fw_config_uuid_spec,
		check_fip
	},
	[NT_FW_CONFIG_ID] = {
		&fip_dev_handle,
		(uintptr_t)&nt_fw_config_uuid_spec,
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
#if 1
	/* ToDo: check GPT_IMAGE_ID value and image_spec name */
	[GPT_IMAGE_ID] = {
		&emmc_dev_handle,
		(uintptr_t)&emmc_gpt_spec,
		check_emmc
	},
#endif
};

static int check_memmap(const uintptr_t spec)
{
	int result;
	uintptr_t local_image_handle;

	result = io_dev_init(memmap_dev_handle, (uintptr_t)NULL);
	if (result == 0) {
		result = io_open(memmap_dev_handle, spec, &local_image_handle);
		if (result == 0) {
			VERBOSE("Using Memmap\n");
			io_close(local_image_handle);
		}
	}
	return result;
}

static int check_emmc(const uintptr_t spec)
{
	int result;
	uintptr_t local_handle;

	result = io_dev_init(emmc_dev_handle, (uintptr_t) NULL);
	if (result == 0) {
		result = io_open(emmc_dev_handle, spec, &local_handle);
		if (result == 0)
			io_close(local_handle);
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

void lan966x_io_setup(void)
{
	int result;

	lan966x_io_init();

	result = register_io_dev_fip(&fip_dev_con);
	assert(result == 0);

	result = register_io_dev_memmap(&memmap_dev_con);
	assert(result == 0);

	result = register_io_dev_block(&emmc_dev_con);
	assert(result == 0);
	
	result = io_dev_open(fip_dev_con, (uintptr_t)NULL, &fip_dev_handle);
	assert(result == 0);

	result = io_dev_open(memmap_dev_con, (uintptr_t)NULL, &memmap_dev_handle);
	assert(result == 0);

	result = io_dev_open(emmc_dev_con, (uintptr_t)&emmc_dev_spec, &emmc_dev_handle);
	assert(result == 0);

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
	const struct plat_io_policy *policy;

	assert(image_id < ARRAY_SIZE(policies));

	policy = &policies[image_id];
	result = policy->check(policy->image_spec);
	assert(result == 0);

	*image_spec = policy->image_spec;
	*dev_handle = *(policy->dev_handle);

	return result;
}
