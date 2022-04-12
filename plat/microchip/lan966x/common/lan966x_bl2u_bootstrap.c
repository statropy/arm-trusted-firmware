/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/auth/crypto_mod.h>
#include <drivers/io/io_storage.h>
#include <drivers/mmc.h>
#include <drivers/partition/partition.h>
#include <endian.h>
#include <errno.h>
#include <plat/common/platform.h>
#include <platform_def.h>

#include "lan966x_private.h"
#include "lan966x_bootstrap.h"
#include "lan966x_bl2u_bootstrap.h"
#include "aes.h"
#include "ddr.h"
#include "otp.h"

#include <tools_share/firmware_encrypted.h>
#include <tools_share/firmware_image_package.h>

static const uintptr_t fip_base_addr = DDR_BASE_ADDR;
static const uintptr_t fip_max_size = DDR_MEM_SIZE;
static uint32_t data_rcv_length;

static inline int is_valid_fip_hdr(const fip_toc_header_t *header)
{
	if ((header->name == TOC_HEADER_NAME) && (header->serial_number != 0)) {
		return 1;
	} else {
		return 0;
	}
}

static inline int is_enc_img_hdr(struct fw_enc_hdr *header)
{
	if (header->magic == ENC_HEADER_MAGIC) {
		return 1;
	} else {
		return 0;
	}
}

static const char *handle_bind_encrypt(struct fw_enc_hdr *img_header, const fip_toc_entry_t *toc_entry)
{
	int result = 0;
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
			return "Failed to obtain BSSK key";
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
			return "Failed to encrypt FIP image";
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

	return NULL;
}

static const char *handle_bind_decrypt(struct fw_enc_hdr *img_header, const fip_toc_entry_t * toc_entry)
{
	int result;
	uint8_t key[KEY_SIZE] = { 0 };
	size_t key_len = sizeof(key);
	unsigned int key_flags;
	uintptr_t fip_img_data;
	const io_uuid_spec_t uuid_spec = { 0 };

	VERBOSE("BL2U handle bind decrypt\n");

	/* Check if magic header info is available */
	if (is_enc_img_hdr(img_header)) {
		VERBOSE("Decrypt(), found magic header\n");

		/* Retrieve key data (SSK) */
		result = plat_get_enc_key_info(FW_ENC_WITH_SSK,
					       key,
					       &key_len,
					       &key_flags,
					       (uint8_t *)&uuid_spec.uuid,
					       sizeof(uuid_t));
		if (result != 0) {
			return "Failed to obtain SSK key";
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
			return "Failed to decrypt FIP image";
		} else {
			VERBOSE("Decryption of FIP image done\n");
		}

	} else {
		VERBOSE("Found no image for decrypting\n");
	}

	return NULL;
}

static const char *handle_bind_parse_fip(void)
{
	const fip_toc_header_t *toc_header;
	const fip_toc_entry_t *toc_entry;
	struct fw_enc_hdr *enc_img_hdr;
	const uuid_t uuid_null = { 0 };
	uintptr_t toc_end_addr;
	bool exit_parsing = 0;
	const char *result = NULL;

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
	assert((((uintptr_t)toc_header) & 3U) == 0U);

	/* Setup reference pointer to ToC Entry 0 */
	toc_entry = (fip_toc_entry_t *)(fip_base_addr + sizeof(fip_toc_header_t));

	/* Address ought to be 4-byte aligned */
	assert((((uintptr_t)toc_entry) & 3U) == 0U);

	/* Check for valid FIP data */
	if (!is_valid_fip_hdr(toc_header)) {
		return "Header check of FIP failed";
	} else {
		VERBOSE("FIP header looks OK\n");
	}

	/* Set address to Data 0 element. This address is usually right after the last ToC (end)
	 * marker and defines the end address of our parsing loop */
	toc_end_addr = fip_base_addr + toc_entry->offset_address;

	/* Iterate now over all ToC Entries in the FIP file */
	while ((uint32_t)toc_entry < toc_end_addr) {

		/* If ToC End Marker is found (zero terminated), exit parsing loop */
		if (memcmp(&toc_entry->uuid, &uuid_null, sizeof(uuid_t)) == 0) {
			exit_parsing = true;
			break;
		}

		/* Map image pointer to encoded header structure for retrieving data */
		enc_img_hdr = (struct fw_enc_hdr *) (fip_base_addr + (uint32_t) toc_entry->offset_address);

		/* Address ought to be 4-byte aligned */
		if ((((uintptr_t) enc_img_hdr) & 3U) != 0U)
			return "FIP needs to be produced with FIP_ALIGN";

		/* Decrypt image for upcoming encryption step */
		result = handle_bind_decrypt(enc_img_hdr, toc_entry);
		if (result) {
			VERBOSE("Decryption of FIP failed: %s\n", result);
			return result;
		}

		/* Encrypt previously decrypted image file */
		result = handle_bind_encrypt(enc_img_hdr, toc_entry);
		if (result) {
			VERBOSE("Encryption of FIP failed: %s\n", result);
			return result;
		}

		/* Increment pointer to next ToC entry */
		toc_entry++;
	}

	if (!exit_parsing)
		return "FIP does not have a ToC terminator entry";

	return NULL;
}

static void handle_otp_read(bootstrap_req_t *req, bool raw)
{
	uint8_t data[256];
	uint32_t datalen;

	if (req->len == sizeof(uint32_t) && bootstrap_RxDataCrc(req, (uint8_t *)&datalen)) {
		datalen = __ntohl(datalen);
		if (datalen > 0 && datalen < sizeof(data) &&
		    req->arg0 >= 0 && (req->arg0 + datalen) <= OTP_MEM_SIZE) {
			int rc = (raw ?
				  otp_read_bytes_raw(req->arg0, datalen, data) :
				  otp_read_bytes(req->arg0, datalen, data));
			if (rc < 0)
				bootstrap_TxNack_rc("OTP read fails", rc);
			else
				bootstrap_TxAckData(data, datalen);
		} else
			bootstrap_TxNack("OTP read illegal length");
	}
}

static void handle_read_rom_version(const bootstrap_req_t *req)
{
	VERBOSE("BL2U handle read rom version\n");
	bootstrap_TxAckData(version_string, strlen(version_string));
}

static void handle_load_data(const bootstrap_req_t *req)
{
	uint32_t length = req->arg0;
	uint8_t *ptr;
	int num_bytes, offset;
	data_rcv_length = 0;

	VERBOSE("BL2U handle load data\n");

	if (length == 0 || length > fip_max_size) {
		bootstrap_TxNack("Length Error");
		return;
	}

	/* Store data at start address of DDR memory (offset 0x0) */
	ptr = (uint8_t *)fip_base_addr;

	// Go ahead, receive data
	bootstrap_TxAck();

	/* Gobble up the data chunks */
	num_bytes = 0;
	offset = 0;

	while (offset < length && (num_bytes = bootstrap_RxData(ptr, offset, length - offset)) > 0) {
		ptr += num_bytes;
		offset += num_bytes;
	}

	if (offset != length) {
		ERROR("RxData Error: n = %d, l = %d, o = %d\n", num_bytes, length, offset);
		return;
	}

	/* Store data length of received data */
	data_rcv_length = length;

	VERBOSE("Received %d bytes\n", length);
}

static int handle_load_data_chunk(const bootstrap_req_t * req)
{
	VERBOSE("BL2U handle load data chunk, not supported yet!\n");
	bootstrap_TxAck();
	return 0;
}

/*
 * NOTE: Instead of calling mmc_write_blocks() directly we have to
 * spoon feed individual blocks. This is needed due to a constraint at a
 * lower level of the MMC driver.
 */
static uint32_t single_mmc_write_blocks(uint32_t lba, uintptr_t buf_ptr, uint32_t length)
{
	uint32_t written;

	for (written = 0; written < length; ) {
		if (mmc_write_blocks(lba, buf_ptr, MMC_BLOCK_SIZE) != MMC_BLOCK_SIZE) {
			ERROR("Incomplete write at LBA 0x%x, writted %d of %d bytes\n", lba, written, length);
			break;
		}
		buf_ptr += MMC_BLOCK_SIZE;
		written += MMC_BLOCK_SIZE;
		lba++;
	}

	return written;
}

static bool emmc_write(uint32_t offset, uintptr_t buf_ptr, uint32_t length)
{
	uint32_t round_len, lba, written;

	/* Check multiple number of MMC_BLOCK_SIZE */
	assert((offset % MMC_BLOCK_SIZE) == 0);

	/* Convert offset to LBA */
	lba = offset / MMC_BLOCK_SIZE;

	/* Calculate whole pages for mmc_write_blocks() */
	round_len = DIV_ROUND_UP_2EVAL(length, MMC_BLOCK_SIZE) * MMC_BLOCK_SIZE;

	VERBOSE("Write image to offset: 0x%x, length: 0x%x, LBA 0x%x, round_len 0x%x\n",
		offset, length, lba, round_len);

	written = single_mmc_write_blocks(lba, buf_ptr, round_len);

	VERBOSE("Written 0x%0x of the requested 0x%0x bytes\n", written, round_len);

	return written == round_len;
}

static bool fip_update(const char *name, uintptr_t buf_ptr, uint32_t len)
{
	const partition_entry_t *entry = get_partition_entry(name);

	if (entry) {
		if (len > entry->length) {
			NOTICE("Partition %s only can hold %d bytes, %d uploaded\n",
			       name, (uint32_t) entry->length, len);
			return false;
		}
		return emmc_write(entry->start, buf_ptr, len);
	}

	NOTICE("Partition %s not found\n", name);
	return false;
}

/* This routine will write the image data flash device */
static void handle_write_image(const bootstrap_req_t * req)
{
	VERBOSE("BL2U handle write image\n");

	if (data_rcv_length == 0) {
		bootstrap_TxNack("Flash Image not loaded");
		return;
	}

	/* Write Flash */
	if (emmc_write(req->arg0, fip_base_addr, data_rcv_length))
		bootstrap_TxAck();
	else
		bootstrap_TxNack("Image write failed");
}

/* This routine will write the previous encrypted FIP data to the flash device */
static void handle_write_fip(const bootstrap_req_t * req)
{
	int ret = 0;

	VERBOSE("BL2U handle write data\n");

	if (data_rcv_length == 0) {
		bootstrap_TxNack("FIP Image not loaded");
		return;
	}

	/* Init GPT */
	partition_init(GPT_IMAGE_ID);

	/* Update primary FIP */
	if (!fip_update(FW_PARTITION_NAME, fip_base_addr, data_rcv_length))
		ret++;

	/* Update backup FIP */
	if (!fip_update(FW_BACKUP_PARTITION_NAME, fip_base_addr, data_rcv_length))
		ret++;

	if (ret == 0)
		bootstrap_TxAck();
	else if (ret == 1)
		bootstrap_TxNack("One partition failed to update");
	else
		bootstrap_TxNack("Both partitions failed to update");

}

static void handle_bind(const bootstrap_req_t *req)
{
	const char *result;

	VERBOSE("BL2U handle bind operation\n");

	if (data_rcv_length == 0 || data_rcv_length > fip_max_size) {
		bootstrap_TxNack("Image not loaded, length error");
		return;
	}

	/* Parse FIP for encrypted image files */
	result = handle_bind_parse_fip();
	if (result) {
		bootstrap_TxNack(result);
	} else {
		VERBOSE("FIP image successfully accessed\n");
		bootstrap_TxAck();
	}
}

void lan966x_bl2u_bootstrap_monitor(void)
{
	bool exit_monitor = false;
	bootstrap_req_t req = { 0 };

	lan966x_reset_max_trace_level();
	INFO("*** ENTERING BL2U BOOTSTRAP MONITOR ***\n");

	while (!exit_monitor) {
		if (!bootstrap_RxReq(&req)) {
			bootstrap_TxNack("Garbled command");
			continue;
		}

		if (is_cmd(&req, BOOTSTRAP_RESET)) {		// e - Reset (by exit)
			bootstrap_TxAck();
			exit_monitor = true;
		} else if (is_cmd(&req, BOOTSTRAP_VERS))	// V - Get bootstrap version
			handle_read_rom_version(&req);
		else if (is_cmd(&req, BOOTSTRAP_SEND))		// S - Load data file
			handle_load_data(&req);
		else if (is_cmd(&req, BOOTSTRAP_DATA))          // U - Load data chunk
			handle_load_data_chunk(&req);
		else if (is_cmd(&req, BOOTSTRAP_IMAGE))		// I - Copy uploaded raw image from DDR memory to flash device
			handle_write_image(&req);
		else if (is_cmd(&req, BOOTSTRAP_WRITE))		// W - Copy uploaded fip from DDR memory to flash device
			handle_write_fip(&req);
		else if (is_cmd(&req, BOOTSTRAP_BIND))		// B - FW binding operation (decrypt and encrypt)
			handle_bind(&req);
		else if (is_cmd(&req, BOOTSTRAP_OTP_READ_EMU))	// L - Read OTP data
			handle_otp_read(&req, false);
		else if (is_cmd(&req, BOOTSTRAP_OTP_READ_RAW))	// l - Read RAW OTP data
			handle_otp_read(&req, true);
		else
			bootstrap_TxNack("Unknown command");
	}

	INFO("*** EXITING BL2U BOOTSTRAP MONITOR ***\n");
	lan966x_set_max_trace_level();
}
