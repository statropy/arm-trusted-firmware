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
#include <drivers/auth/auth_mod.h>
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
#include <common/desc_image_load.h>
#include <lib/fconf/fconf_tbbr_getter.h>

#include "lan966x_private.h"
#include "lan966x_regs.h"
#include "fw_config.h"

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

/* QSPI NOR layout for T/NT dual bl33 */
#define T_FW_FIP_SIZE	120
#define NOR_SEL_SIZE	8
#define CONFIG_SIZE	256
#define NT_FIP_SIZE	((2048 - T_FW_FIP_SIZE - NOR_SEL_SIZE - CONFIG_SIZE) / 2) /* 832K */

#define LAN966X_QSPI0_SEL_OFFSET	(LAN966X_QSPI0_MMAP + (1024 * (T_FW_FIP_SIZE)))
#define LAN966X_QSPI0_FIP_OFFSET	(LAN966X_QSPI0_SEL_OFFSET + (1024 * NOR_SEL_SIZE))
#define LAN966X_QSPI0_FIP1_OFFSET	(LAN966X_QSPI0_FIP_OFFSET)
#define LAN966X_QSPI0_FIP2_OFFSET	(LAN966X_QSPI0_FIP_OFFSET + (1024 * NT_FIP_SIZE))

#if defined(IMAGE_BL2)
static uint8_t mmc_buf[MMC_BUF_SIZE] __attribute__ ((aligned (MMC_BLOCK_SIZE)));
#endif

static const io_block_dev_spec_t mmc_dev_spec = {
	.buffer = {
#if defined(IMAGE_BL1)
		.offset = (uintptr_t) BL1_MMC_BUF_BASE,
		.length = MMC_BUF_SIZE,
#elif defined(IMAGE_BL2)
		.offset = (uintptr_t) mmc_buf,
		.length = sizeof(mmc_buf),
#endif
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
	/* Note - the 'sources' below are not available in BL1 as its
	 * already taped out. If we were to produce a new BL1 ROM it
	 * should be included in the same manner as for BL2.
	 */
	FIP_SELECT_FALLBACK,	/* eMMC/SD */
	FIP_SELECT_NOR_NT_FIP1,	/* NOR */
	FIP_SELECT_NOR_NT_FIP2,	/* NOR */
};
static int fip_select;

/* Data will be fetched from the GPT */
static io_block_spec_t fip_mmc_block_spec;

static io_block_spec_t fip_qspi_block_spec = {
	.offset = LAN966X_QSPI0_MMAP,
	.length = LAN966X_QSPI0_RANGE,
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
static int check_mmc_raw(const uintptr_t spec);
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
		check_mmc_raw
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

void plat_bootstrap_io_enable_ram_fip(size_t offset, size_t length)
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

#define NOR_IMAGE_SEL_MAGIC 0x638945a5
struct nor_image_select {
	uint32_t magic[4];	/* magic - ~magic - magic - ~magic */
	uint32_t data[16];
};

static int nor_get_dual_fip_offset(bool primary)
{
	/* We are memory mapped */
	struct nor_image_select *sel = (void*) LAN966X_QSPI0_SEL_OFFSET;

	/* Do we have an initialized FIP select marker? */
	if (sel->magic[0] == NOR_IMAGE_SEL_MAGIC &&
	    sel->magic[1] == (uint32_t) ~NOR_IMAGE_SEL_MAGIC &&
	    sel->magic[2] == NOR_IMAGE_SEL_MAGIC &&
	    sel->magic[3] == (uint32_t) ~NOR_IMAGE_SEL_MAGIC) {
		int i, ct;

		/* Then count one bits */
		for (ct = i = 0; i < ARRAY_SIZE(sel->data); i++)
			ct += __builtin_popcount(sel->data[i]);

		INFO("QSPI0: Have marker - count %d\n", ct);
		if ((ct % 2) == 0)
			goto natural;

		/* Odd = flipped: backwards order */
		return primary ? LAN966X_QSPI0_FIP2_OFFSET : LAN966X_QSPI0_FIP1_OFFSET;
	} else {
		INFO("QSPI0: No FIP select marker\n");
	}

	/* No marker or even count: natural order */
natural:
	return primary ? LAN966X_QSPI0_FIP1_OFFSET : LAN966X_QSPI0_FIP2_OFFSET;
}

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

static int check_mmc_raw(const uintptr_t spec)
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

static int check_mmc(const uintptr_t spec)
{
	const partition_entry_t *entry;
	const char *fipname;

	/* Set FIP offset */
	switch (fip_select) {
	case FIP_SELECT_DEFAULT:
		fipname = FW_PARTITION_NAME;
		break;
	case FIP_SELECT_FALLBACK:
		fipname = FW_BACKUP_PARTITION_NAME;
		break;
	default:
		assert(false);
		fipname = NULL;
		break;
	}

	entry = get_partition_entry(fipname);
	if (entry == NULL) {
		ERROR("MMC: Source %d - no \"%s\" FIP partition\n", fip_select, fipname);
		return -ENOTBLK;
	}

	fip_mmc_block_spec.offset = entry->start;
	fip_mmc_block_spec.length = entry->length;

	INFO("MMC: Try source %d, %s offset: %08x\n", fip_select, fipname, fip_mmc_block_spec.offset);

	return check_mmc_raw(spec);
}

static int check_memmap(const uintptr_t spec)
{
	int result;
	uintptr_t local_image_handle;

	/* Set FIP offset */
	switch (fip_select) {
	case FIP_SELECT_DEFAULT:
		/* Start of flash */
		fip_qspi_block_spec.offset = LAN966X_QSPI0_MMAP;
		break;
	case FIP_SELECT_NOR_NT_FIP1:
	case FIP_SELECT_NOR_NT_FIP2:
		fip_qspi_block_spec.offset =
			nor_get_dual_fip_offset(fip_select == FIP_SELECT_NOR_NT_FIP1);
		break;
	default:
		assert(false);
	}

	INFO("QSPI: Try source %d, offset: %08x\n", fip_select, fip_qspi_block_spec.offset);

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

		partition_init(GPT_IMAGE_ID);
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
#elif defined(IMAGE_BL2)
int bl2_plat_handle_pre_image_load(unsigned int image_id)
{
	fip_select = FIP_SELECT_DEFAULT;

	return 0;
}

#if defined(LAN966X_DUAL_BL33)
static bool is_non_trusted_image(unsigned int image_id)
{
        return (image_id == BL33_IMAGE_ID ||
                image_id == NON_TRUSTED_FW_CONTENT_CERT_ID ||
                image_id == NON_TRUSTED_FW_KEY_CERT_ID);
}

void bl2_plat_handle_image_error(unsigned int image_id, int err)
{
        if (!is_non_trusted_image(image_id)) {
                VERBOSE("Image(%d) ignore non-NT load error: %d\n", image_id, err);
                return;
        }

        NOTICE("NT Image(%d) load error: %d\n", image_id, err);

        /* NOTE: We must remove authentication from NT parents. This
         * is due to the fact we will be retrying to load from an
         * alternate source later and must avoid mixing FIP
         * elements. Clearing the NT parent authentication flags
         * will trigger reloading them.
         */
        for (;;) {
                /* Get the image descriptor */
                const auth_img_desc_t *img_desc = FCONF_GET_PROPERTY(tbbr, cot, image_id);
                if (img_desc->parent == NULL)
                        /* Stop if no parents */
                        break;

                image_id = img_desc->parent->img_id;
                if (!is_non_trusted_image(image_id))
                        /* Stop once we exit NT world */
                        break;

                INFO("Invalidate NT parent image: %d\n", image_id);
                auth_img_flags[image_id] &= ~IMG_FLAG_AUTHENTICATED;
        }
}
#endif

int plat_try_next_boot_source(void)
{
	switch (lan966x_get_boot_source()) {
	case BOOT_SOURCE_QSPI:
#if defined(LAN966X_DUAL_BL33)
		if (fip_select == FIP_SELECT_DEFAULT) {
			fip_select = FIP_SELECT_NOR_NT_FIP1;
			return 1;	/* Try again */
		}
		if (fip_select == FIP_SELECT_NOR_NT_FIP1) {
			fip_select = FIP_SELECT_NOR_NT_FIP2;
			return 1;	/* Try again */
		}
#endif
		break;
	case BOOT_SOURCE_EMMC:
	case BOOT_SOURCE_SDMMC:
		if (fip_select == FIP_SELECT_DEFAULT) {
			fip_select = FIP_SELECT_FALLBACK;
			return 1;	/* Try again */
		}
		break;
	default:
		break;
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

int bl2_plat_handle_post_image_load(unsigned int image_id)
{
	bl_mem_params_node_t *bl_mem_params = get_bl_mem_params_node(image_id);
	uint32_t off, src;

	assert(bl_mem_params);

	switch (image_id) {
	case BL32_IMAGE_ID:
		//bl_mem_params->ep_info.args.arg0 = 0xdeadbeef;
		bl_mem_params->ep_info.args.arg1 = (uintptr_t) &lan966x_fw_config;
		break;
	case BL33_IMAGE_ID:
		src = lan966x_get_boot_source();
		switch (src) {
		case BOOT_SOURCE_EMMC:
		case BOOT_SOURCE_SDMMC:
			off = fip_mmc_block_spec.offset;
			break;
		case BOOT_SOURCE_QSPI:
			off = fip_qspi_block_spec.offset - LAN966X_QSPI0_MMAP;
			break;
		default:
			off = 0xFFFFFFFF; /* Undefined offset */
		}
		/* Use GPR(1) and GPR(2) as BL33 might be linux */
		/* src 31:16 is 'afea' as 'magic' */
		mmio_write_32(CPU_GPR(LAN966X_CPU_BASE, 1), 0xAFEA0000 | src);
		mmio_write_32(CPU_GPR(LAN966X_CPU_BASE, 2), off);
		break;
	default:
		/* Do nothing in default case */
		break;
	}

	return 0;
}
