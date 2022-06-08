/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/auth/crypto_mod.h>
#include <tools_share/firmware_encrypted.h>
#include <drivers/io/io_storage.h>
#include <plat/common/platform.h>

#include "lan966x_fw_bind.h"
#include "lan966x_private.h"
#include "aes.h"

static inline bool is_aligned(const void *ptr)
{
	return ((uintptr_t)(ptr) & 3U) == 0U;
}

static inline int is_enc_img_hdr(struct fw_enc_hdr *header)
{
	if (header->magic == ENC_HEADER_MAGIC) {
		return 1;
	} else {
		return 0;
	}
}

fw_bind_res_t handle_bind_encrypt(const uintptr_t fip_base_addr,
				  struct fw_enc_hdr *img_header,
				  const fip_toc_entry_t *toc_entry)
{
	int result;
	uint16_t i;
	uint32_t iv[IV_SIZE / sizeof(uint32_t)] = { 0 };
	uint8_t tag[TAG_SIZE] = { 0 };
	uint8_t key[KEY_SIZE] = { 0 };
	size_t key_len = sizeof(key);
	unsigned int key_flags;
	uintptr_t fip_img_data;
	const io_uuid_spec_t uuid_spec = { 0 };

	VERBOSE("BL2U handle bind encrypt\n");

	/* Check if magic header info is available */
	if (is_enc_img_hdr(img_header)) {
		VERBOSE("Encrypt(), found magic header\n");

		/* Retrieve key data (BSSK) */
		result = plat_get_enc_key_info(FW_ENC_WITH_BSSK,
					       key,
					       &key_len,
					       &key_flags,
					       (uint8_t *)&uuid_spec.uuid,
					       sizeof(uuid_t));
		if (result != 0) {
			return FW_BSSK_FAILURE;
		} else {
			VERBOSE("Encryption key successfully retrieved\n");
		}

		/* Set pointer to the Data[] payload section of the current image */
		fip_img_data =
		    fip_base_addr + toc_entry->offset_address + sizeof(struct fw_enc_hdr);

		/* Initialize iv array with random data */
		for (i = 0; i < ARRAY_SIZE(iv); i++) {
			iv[i] = lan966x_trng_read();
		}

		/* Encrypt image data on-the-fly in memory  */
		result = aes_gcm_encrypt((uint8_t *)fip_img_data,
					 (toc_entry->size - sizeof(struct fw_enc_hdr)),
					 key,
					 key_len,
					 (uint8_t *)iv,
					 sizeof(iv),
					 tag,
					 sizeof(tag));

		/* Wipe out key data */
		memset(key, 0, key_len);

		if (result != 0) {
			return FW_ENCRYPT;
		} else {
			/* Update firmware image header data */
			img_header->dec_algo = CRYPTO_GCM_DECRYPT;
			img_header->flags |= FW_ENC_STATUS_FLAG_MASK;
			memcpy(img_header->tag, tag, TAG_SIZE);
			memcpy(img_header->iv, iv, IV_SIZE);

			VERBOSE("Enryption of FIP image done\n");
		}
	} else {
		VERBOSE("Found no image for encrypting\n");
	}

	return FW_BIND_OK;
}

fw_bind_res_t handle_bind_decrypt(const uintptr_t fip_base_addr,
				  struct fw_enc_hdr *img_header,
				  const fip_toc_entry_t * toc_entry)
{
	fw_bind_res_t result;
	uint8_t key[KEY_SIZE] = { 0 };
	size_t key_len = sizeof(key);
	unsigned int key_flags;
	uintptr_t fip_img_data;
	const io_uuid_spec_t uuid_spec = { 0 };

	VERBOSE("BL2U handle bind decrypt\n");

	/* Check if magic header info is available */
	if (is_enc_img_hdr(img_header)) {
		VERBOSE("Decrypt(), found magic header\n");

		/* Check its SSK encrypted */
		if (img_header->flags != FW_ENC_WITH_SSK)
			return FW_NOT_SSK_ENCRYPTED;

		/* Retrieve key data (SSK) */
		result = plat_get_enc_key_info(img_header->flags,
					       key,
					       &key_len,
					       &key_flags,
					       (uint8_t *)&uuid_spec.uuid,
					       sizeof(uuid_t));
		if (result != 0) {
			return FW_SSK_FAILURE;
		} else {
			VERBOSE("Decryption key successfully retrieved\n");
		}

		/* Set address to the Data[] payload section of the current image */
		fip_img_data =
			fip_base_addr + toc_entry->offset_address + sizeof(struct fw_enc_hdr);

		/* Decrypt image data on-the-fly in memory */
		result = aes_gcm_decrypt((uint8_t *)fip_img_data,
					 (toc_entry->size - sizeof(struct fw_enc_hdr)),
					 key,
					 key_len,
					 img_header->iv,
					 img_header->iv_len,
					 img_header->tag,
					 img_header->tag_len);

		/* Wipe out key data */
		memset(key, 0, key_len);

		if (result != 0) {
			return FW_DECRYPT;
		} else {
			VERBOSE("Decryption of FIP image done\n");
		}

	} else {
		VERBOSE("Found no image for decrypting\n");
	}

	return FW_BIND_OK;
}

fw_bind_res_t lan966x_bind_fip(const uintptr_t fip_base_addr, size_t fip_length)
{
	void *fip_max = (void*) (fip_base_addr + fip_length);
	const fip_toc_header_t *toc_header;
	const fip_toc_entry_t *toc_entry;
	struct fw_enc_hdr *enc_img_hdr;
	const uuid_t uuid_null = { 0 };
	uintptr_t toc_end_addr;
	bool exit_parsing = 0;
	fw_bind_res_t result;

	VERBOSE("BL2U handle parsing of fip\n");

	/* The data offset value inside the ToC Entry 0, declares the start of the first image
	 * payload (Data 0). Therefore, the memory area in between is used for the ToC Entry(s). We
	 * will scan this region for all available ToC Entry(s), till the zero loaded ToC End Marker
	 * is reached.
	 *
	 *  fip_base_addr --->  ------------------
	 *                      | ToC Header     |
	 *                      |----------------|
	 *                      | ToC Entry 0    |
	 *                      |----------------|
	 *                      | ToC Entry 1    |
	 *                      |----------------|
	 *                      | ToC End Marker |
	 *                      |----------------|
	 *                      |                |
	 *                      |     Data 0     |
	 *                      |                |
	 *                      |----------------|
	 *                      |                |
	 *                      |     Data 1     |
	 *                      |                |
	 *                      ------------------
	 */


	/* Setup reference pointer to ToC header */
	toc_header = (fip_toc_header_t *)fip_base_addr;

	/* Address ought to be 4-byte aligned */
	if (!is_aligned(toc_header))
		return FW_FIP_ALIGN;

	/* Setup reference pointer to ToC Entry 0 */
	toc_entry = (fip_toc_entry_t *)(fip_base_addr + sizeof(fip_toc_header_t));

	/* Address ought to be 4-byte aligned */
	if (!is_aligned(toc_entry))
                return FW_FIP_ALIGN;

	/* Check for valid FIP data */
	if (!is_valid_fip_hdr(toc_header)) {
		return FW_FIP_HDR;
	} else {
		VERBOSE("FIP header looks OK\n");
	}

	/* Set address to Data 0 element. This address is usually right after the last ToC (end)
	 * marker and defines the end address of our parsing loop */
	toc_end_addr = fip_base_addr + toc_entry->offset_address;

	/* Iterate now over all ToC Entries in the FIP file */
	while ((uintptr_t)toc_entry < toc_end_addr) {

		if ((void*)&toc_entry[1] > fip_max)
			return FW_FIP_INCOMPLETE;

		/* If ToC End Marker is found (zero terminated), exit parsing loop */
		if (memcmp(&toc_entry->uuid, &uuid_null, sizeof(uuid_t)) == 0) {
			exit_parsing = true;
			break;
		}

		/* Map image pointer to encoded header structure for retrieving data */
		enc_img_hdr = (struct fw_enc_hdr *) (fip_base_addr + (uint32_t) toc_entry->offset_address);

		/* Address ought to be 4-byte aligned */
		if (!is_aligned(enc_img_hdr))
			return FW_FIP_ALIGN;

		/* Are we below top? */
		if ((void*)&enc_img_hdr[1] > fip_max)
			return FW_FIP_INCOMPLETE;

		/* Decrypt image for upcoming encryption step */
		result = handle_bind_decrypt(fip_base_addr, enc_img_hdr, toc_entry);
		if (result) {
			VERBOSE("Decryption of FIP failed: %d\n", result);
			return result;
		}

		/* Encrypt previously decrypted image file */
		result = handle_bind_encrypt(fip_base_addr, enc_img_hdr, toc_entry);
		if (result) {
			VERBOSE("Encryption of FIP failed: %d\n", result);
			return result;
		}

		/* Increment pointer to next ToC entry */
		toc_entry++;
	}

	if (!exit_parsing)
		return FW_TOC_TERM_MISSING;

	return FW_BIND_OK;
}
