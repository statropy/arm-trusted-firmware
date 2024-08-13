/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/auth/crypto_mod.h>
#include <drivers/io/io_storage.h>
#include <drivers/microchip/lan966x_trng.h>
#include <drivers/microchip/qspi.h>
#include <drivers/microchip/sha.h>
#include <drivers/mmc.h>
#include <drivers/partition/partition.h>
#include <endian.h>
#include <errno.h>
#include <lib/mmio.h>
#include <lib/xlat_tables/xlat_tables_compat.h>
#include <plat/common/platform.h>
#include <platform_def.h>
#include <tf_gunzip.h>

#include <lan96xx_common.h>
#include <plat_bl2u_bootstrap.h>
#include <plat_crypto.h>
#include <lan966x_fw_bind.h>
#include <ddr_init.h>
#include "lan966x_bootstrap.h"
#include "lan966x_regs.h"
#include "aes.h"
#include "ddr_test.h"

#include "otp.h"

#define MAX_OTP_DATA	1024

#define PAGE_ALIGN(x, a)	(((x) + (a) - 1) & ~((a) - 1))

static const uintptr_t fip_base_addr = LAN966X_DDR_BASE;
static uint32_t data_rcv_length;
static struct ddr_config current_ddr_config;
static const uintptr_t ddr_base_addr = LAN966X_DDR_BASE;
static bool ddr_was_initialized, cur_cache;

#if defined(LAN969X_SRAM_SIZE)
#define SRAM_BUFFER BL2_LIMIT
#define SRAM_SIZE   (LAN969X_SRAM_SIZE - BL1_RW_SIZE - BL2U_SIZE)
#else
#define SRAM_BUFFER NULL
#define SRAM_SIZE   0U
#endif
/* SRAM buffer is divided in 2, download and decompress, each 1/2 of available */
static uint8_t *sram_buffer = (uint8_t *) SRAM_BUFFER;
/* Announce half the SRAM as available */
static const uint32_t sram_available = (SRAM_SIZE / 2);

#if defined(MCHP_SOC_LAN969X)
extern const struct ddr_config lan969x_evb_ddr4_ddr_config;
#define  default_ddr_config lan969x_evb_ddr4_ddr_config
#elif defined(MCHP_SOC_LAN966X)
extern const struct ddr_config lan966x_ddr_config;
#define  default_ddr_config lan966x_ddr_config
#endif

/* Check for GZIP header */
static bool is_gzip(uint8_t *data)
{
	return data[0] == 0x1f && data[1] == 0x8b;
}

static void handle_otp_read(bootstrap_req_t *req)
{
	uint8_t data[256];
	uint32_t datalen;

	if (req->len == sizeof(uint32_t) && bootstrap_RxDataCrc(req, (uint8_t *)&datalen)) {
		datalen = __ntohl(datalen);
		if (datalen > 0 && datalen < sizeof(data) &&
		    req->arg0 >= 0 && (req->arg0 + datalen) <= OTP_MEM_SIZE) {
			int rc = otp_read_bytes_raw(req->arg0, datalen, data);
			if (rc < 0)
				bootstrap_TxNack_rc("OTP read fails", rc);
			else
				bootstrap_TxAckData(data, datalen);
		} else
			bootstrap_TxNack("OTP read illegal length");
	}
}

static void handle_otp_data(bootstrap_req_t *req)
{
	uint8_t data[MAX_OTP_DATA];

	if (req->len > 0 && req->len < MAX_OTP_DATA &&
	    bootstrap_RxDataCrc(req, data)) {
		if (otp_write_bytes(req->arg0, req->len, data) == 0)
			bootstrap_Tx(BOOTSTRAP_ACK, req->arg0, 0, NULL);
		else
			bootstrap_TxNack("OTP program failed");

		/* Wipe data */
		memset(data, 0, req->len);
	} else
		bootstrap_TxNack("OTP rx data failed or illegal data size");
}

static void handle_otp_random(bootstrap_req_t *req)
{
	/* Note: We keep work buffers on stack - must be large enough */
	uint32_t datalen, data[MAX_OTP_DATA / 4], cur_data[MAX_OTP_DATA / 4];
	int i;

	if (req->len == sizeof(uint32_t) && bootstrap_RxDataCrc(req, (uint8_t *)&datalen)) {
		datalen = __ntohl(datalen);
		if (datalen > 0 &&
		    datalen < MAX_OTP_DATA) {
			if (otp_read_bytes_raw(req->arg0, datalen, (uint8_t *)cur_data) != 0) {
				bootstrap_TxNack("Unable to read OTP data");
				return;
			}

			if (!otp_all_zero((uint8_t*) cur_data, datalen)) {
				bootstrap_TxNack("OTP data already non-zero");
				goto wipe_cur;
			}

			/* Read TRNG data */
			for (i = 0; i < div_round_up(datalen, sizeof(uint32_t)); i++)
				data[i] = lan966x_trng_read();

			/* Write to OTP */
			if (otp_write_bytes(req->arg0, datalen, (uint8_t *)data) == 0)
				bootstrap_Tx(BOOTSTRAP_ACK, req->arg0, 0, NULL);
			else
				bootstrap_TxNack("OTP program random failed");

			/* Wipe data */
			memset(data, 0, datalen);
		wipe_cur:
			memset(cur_data, 0, datalen);
		} else
			bootstrap_TxNack("OTP random data illegal length");
	} else
		bootstrap_TxNack("OTP random data illegal req length length");
}

static void handle_read_rom_version(const bootstrap_req_t *req)
{
	char ident[256] = { "BL2:" };
	uint32_t chip;

	/* Add version */
	strlcat(ident, version_string, sizeof(ident));

	/* Get Chip */
	chip = mmio_read_32(GCB_CHIP_ID(LAN966X_GCB_BASE));

	/* Send response */
	bootstrap_TxAckData_arg(ident, strlen(ident), chip);
}

static bool recv_data(uint8_t *ptr, uint32_t length)
{
	uint32_t num_bytes, offset;

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
		return false;
	}

	return true;
}

static void handle_load_data(const bootstrap_req_t *req)
{
	uint32_t length = req->arg0;
	data_rcv_length = 0;

	VERBOSE("BL2U handle load data\n");

	if (length == 0 || length > default_ddr_config.info.size) {
		bootstrap_TxNack("Length Error");
		return;
	}

	/* Make sure DDR is ready for data */
	if (!ddr_was_initialized) {
		bootstrap_TxNack("DDR must be initialized before data is sent");
	}

	/* Store data at start address of DDR memory (offset 0x0) */
	if (recv_data((uint8_t *)fip_base_addr, length))
		data_rcv_length = length;

	VERBOSE("Received %d bytes\n", length);
}

static void handle_send_sram(bootstrap_req_t *req)
{
	uint32_t length = req->arg0;

	if (length == 0 || sram_available == 0 || length > sram_available) {
		bootstrap_TxNack("SRAM Length Error");
		return;
	}

	/* Store data at start address of SRAM memory (offset 0x0) */
	if (recv_data(sram_buffer, length))
		VERBOSE("SRAM: Received %d bytes\n", length);
}

static void bootstrap_do_sha(const void * data, uint32_t data_len)
{
	lan966x_key32_t data_sig;

	/* Calculate hash */
	sha_calc(SHA_MR_ALGO_SHA256, data, data_len, data_sig.b, sizeof(data_sig.b));

	/* Return hash, length */
	bootstrap_TxAckData_arg(data_sig.b, sizeof(data_sig.b), data_len);
}

static bool valid_write_dev(boot_source_type dev)
{
	if (dev == BOOT_SOURCE_EMMC ||
	    dev == BOOT_SOURCE_SDMMC ||
	    dev == BOOT_SOURCE_QSPI)
		return true;

	return false;
}

int lan966x_bl2u_emmc_block_write_read(uint32_t offset, void *buf, uint32_t length)
{
	uint32_t round_len, lba, written_bytes, read_bytes;

	/* Check multiple number of MMC_BLOCK_SIZE */
	assert((offset % MMC_BLOCK_SIZE) == 0);

	/* Convert offset to LBA */
	lba = offset / MMC_BLOCK_SIZE;

	/* Calculate whole pages for mmc_write_blocks() */
	round_len = DIV_ROUND_UP_2EVAL(length, MMC_BLOCK_SIZE) * MMC_BLOCK_SIZE;

	VERBOSE("Write image to offset: 0x%x, length: 0x%x, LBA 0x%x, round_len 0x%x\n",
		offset, length, lba, round_len);

	written_bytes = mmc_write_blocks(lba, (uintptr_t) buf, round_len);

	if (written_bytes != round_len) {
		WARN("Only wrote 0x%0x of the requested 0x%0x bytes\n", written_bytes, round_len);
		return -EIO;
	}

	read_bytes = mmc_read_blocks(lba, (uintptr_t) buf, round_len);

	if (read_bytes != round_len) {
		WARN("Only read 0x%0x of the requested 0x%0x bytes\n", read_bytes, round_len);
		return -EIO;
	}
	return 0;
}

static void handle_write_readback(bootstrap_req_t *req)
{

	lan966x_key32_t data_write, data_read;
	uint32_t length = req->arg0;
	uint32_t args[2], dev, offset;
	uint32_t ret;
	uint8_t *sram_write_buffer;

	if (!bootstrap_RxDataCrc(req, (uint8_t *)args)) {
		bootstrap_TxNack("Incremental data args error");
		return;
	}

	if (length == 0 || sram_available == 0 || length > sram_available) {
		bootstrap_TxNack("SRAM Length Error");
		return;
	}

	/* Extra command args */
	dev = args[0];
	offset = args[1];

	if (!valid_write_dev(dev)) {
		bootstrap_TxNack("Unsupported target device");
		return;
	}

	/* Check if we have compressed data */
	if (is_gzip(sram_buffer)) {
		uintptr_t in_buf, work_buf, out_buf, out_start;
		size_t in_len, work_len, out_len;

		/* Set up decompress params */
		in_buf = (uintptr_t) sram_buffer;
		in_len = length;
		work_buf = (uintptr_t) (sram_buffer + length);
		work_len = sram_available - length;
		out_start = out_buf = (uintptr_t) (sram_buffer + sram_available);
		out_len = sram_available;
		VERBOSE("gunzip(%p, %zd, %p, %zd, %p, %zd)\n",
			(void*) in_buf, in_len, (void*) work_buf, work_len, (void*) out_buf, out_len);
		if (gunzip(&in_buf, in_len, &out_buf, out_len, work_buf, work_len) == 0) {
			sram_write_buffer = (void*) out_start;
			length = out_buf - out_start;
			VERBOSE("Unzipped data, length now %d bytes\n", length);
		} else {
			bootstrap_TxNack("Decompression failure");
			return;
		}
	} else {
		VERBOSE("Uncompressed data, length is %d bytes\n", length);
		sram_write_buffer = sram_buffer;
	}

	/* Init IO layer */
	lan966x_bl2u_io_init_dev(dev);

	/* Calculate SHA before write */
	sha_calc(SHA_MR_ALGO_SHA256, sram_write_buffer, length, data_write.b, sizeof(data_write.b));

	/* Write Flash */
	switch (dev) {
	case BOOT_SOURCE_EMMC:
	case BOOT_SOURCE_SDMMC:
		ret = lan966x_bl2u_emmc_block_write_read(offset, sram_write_buffer, length);
		break;
	case BOOT_SOURCE_QSPI: {
		size_t act_read;
		ret = qspi_write(offset, sram_write_buffer, length);
		if (ret == 0) {
			ret = qspi_read(offset, (uintptr_t) sram_write_buffer, length, &act_read);
			if (ret != 0 || act_read != length) {
				bootstrap_TxNack("Data readback failed");
				return;
			}
		}
		break;
	}
	default:
		ret = -ENOTSUP;
	}

	if (ret) {
		bootstrap_TxNack("Write of SRAM data failed");
		return;
	}

	/* Calculate SHA when read back write */
	sha_calc(SHA_MR_ALGO_SHA256, sram_write_buffer, length, data_read.b, sizeof(data_read.b));

	if (memcmp(data_write.b, data_read.b, sizeof(data_write.b)) != 0) {
		bootstrap_TxNack("SHA mismatch, chunk write failed");
		return;
	}

	/* Return hash, length */
	bootstrap_TxAckData_arg(data_read.b, sizeof(data_read.b), length);
}

static void handle_unzip_data(const bootstrap_req_t *req)
{
	uint8_t *ptr = (uint8_t *)fip_base_addr;
	const char *resp = "Plain data";

	/* See if this is compressed data */
	if (is_gzip(ptr)) {
		uintptr_t in_buf, work_buf, out_buf, out_start;
		size_t in_len, work_len, out_len;

		/* GZIP 'magic' seen, try to decompress */
		INFO("Looks like GZIP data\n");

		/* Set up decompress params */
		in_buf = fip_base_addr;
		in_len = data_rcv_length;
		work_buf = in_buf + PAGE_ALIGN(data_rcv_length, SIZE_M(1));
		work_len = SIZE_M(16);
		out_start = out_buf = work_buf + work_len;
		out_len = default_ddr_config.info.size - (out_buf - in_buf);
		VERBOSE("gunzip(%p, %zd, %p, %zd, %p, %zd)\n",
			(void*) in_buf, in_len, (void*) work_buf, work_len, (void*) out_buf, out_len);
		if (gunzip(&in_buf, in_len, &out_buf, out_len, work_buf, work_len) == 0) {
			out_len = out_buf - out_start;
			memmove((void *)fip_base_addr, (const void *) out_start, out_len);
			data_rcv_length = out_len;
			INFO("Unzipped data, length now %d bytes\n", data_rcv_length);
			resp = "Decompressed data";
		} else {
			INFO("Non-zipped data, length %d bytes\n", data_rcv_length);
		}
	}

	/* Send response */
	bootstrap_TxAckData_arg(resp, strlen(resp), data_rcv_length);
}

/*
 * Chunked MMC write for better diagnostics and resilient to size-dependent timeouts
 */
static uint32_t chunked_mmc_write_blocks(uint32_t lba, uintptr_t buf_ptr, uint32_t length)
{
	uint32_t written;

	for (written = 0; written < length; ) {
		size_t chunk = MIN((size_t) SIZE_M(1),
				   (size_t) (length - written));
		if (mmc_write_blocks(lba, buf_ptr, chunk) != chunk) {
			ERROR("Incomplete write at LBA 0x%x, wrote %d of %d bytes\n", lba, written, length);
			break;
		}
		buf_ptr += chunk;
		written += chunk;
		lba += (chunk / MMC_BLOCK_SIZE);
		INFO("emmc: Wrote %ldMb @ lba 0x%x\n", written / SIZE_M(1), lba);
	}

	INFO("emmc: Done at %d bytes, %d blocks\n", written, written / MMC_BLOCK_SIZE);

	return written;
}

/*
 * Chunked MMC read for better diagnostics and resilient to size-dependent timeouts
 */
static uint32_t chunked_mmc_read_blocks(uint32_t lba, uintptr_t buf_ptr, uint32_t length)
{
	uint32_t nread;

	for (nread = 0; nread < length; ) {
		size_t chunk = MIN((size_t) SIZE_M(1),
				   (size_t) (length - nread));
		if (mmc_read_blocks(lba, buf_ptr, chunk) != chunk) {
			ERROR("Incomplete read at LBA 0x%x, wrote %d of %d bytes\n", lba, nread, length);
			break;
		}
		buf_ptr += chunk;
		nread += chunk;
		lba += (chunk / MMC_BLOCK_SIZE);
		INFO("emmc: Read %ldMb @ lba 0x%x\n", nread / SIZE_M(1), lba);
	}

	INFO("emmc: Read done at %d bytes, %d blocks\n", nread, nread / MMC_BLOCK_SIZE);

	return nread;
}

int lan966x_bl2u_emmc_read(uint32_t offset, uintptr_t buf_ptr, uint32_t length)
{
	uint32_t lba;
	size_t round_len, size_read;

	/* Check multiple number of MMC_BLOCK_SIZE */
	assert((offset % MMC_BLOCK_SIZE) == 0);

	/* Convert offset to LBA */
	lba = offset / MMC_BLOCK_SIZE;

	/* Calculate whole pages for chunked_mmc_read_blocks() */
	round_len = DIV_ROUND_UP_2EVAL(length, MMC_BLOCK_SIZE) * MMC_BLOCK_SIZE;

	size_read = chunked_mmc_read_blocks(lba, buf_ptr, round_len);

	if (size_read != round_len) {
		NOTICE("emmc_read: Read %zd bytes, expected %zd\n", size_read, round_len);
		return -EPIPE;
	}

	return 0;
}

int lan966x_bl2u_emmc_verify(uint32_t lba, uintptr_t buf_ptr, size_t length)
{
	lan966x_key32_t data_write, data_read;
	uint8_t *data = (void*) buf_ptr;

	/* Calculate SHA of written data */
	sha_calc(SHA_MR_ALGO_SHA256, data, length, data_write.b, sizeof(data_write.b));

	size_t round_len = DIV_ROUND_UP_2EVAL(length, MMC_BLOCK_SIZE) * MMC_BLOCK_SIZE;
	size_t size_read = chunked_mmc_read_blocks(lba, buf_ptr, round_len);

	if (size_read != round_len) {
		NOTICE("emmc_verify: Read %zd bytes, expected %zd\n", size_read, round_len);
		return -EPIPE;
	}

	/* Calculate SHA when read back write */
	sha_calc(SHA_MR_ALGO_SHA256, data, length, data_read.b, sizeof(data_read.b));

	if (memcmp(data_write.b, data_read.b, sizeof(data_write.b)) != 0) {
		NOTICE("SHA mismatch, emmc verify failed");
		return -ENXIO;
	}

	INFO("emmc: Verified %zd bytes of data\n", length);

	return 0;
}

int lan966x_bl2u_emmc_write(uint32_t offset, uintptr_t buf_ptr, uint32_t length, bool verify)
{
	uint32_t round_len, lba, written;
	int ret = 0;

	/* Check multiple number of MMC_BLOCK_SIZE */
	assert((offset % MMC_BLOCK_SIZE) == 0);

	/* Convert offset to LBA */
	lba = offset / MMC_BLOCK_SIZE;

	/* Calculate whole pages for mmc_write_blocks() */
	round_len = DIV_ROUND_UP_2EVAL(length, MMC_BLOCK_SIZE) * MMC_BLOCK_SIZE;

	VERBOSE("Write image to offset: 0x%x, length: 0x%x, LBA 0x%x, round_len 0x%x\n",
		offset, length, lba, round_len);

	written = chunked_mmc_write_blocks(lba, buf_ptr, round_len);

	VERBOSE("Written 0x%0x of the requested 0x%0x bytes\n", written, round_len);

	if (written != round_len) {
		ret = -EIO;
	} else {
		if (verify)
			ret = lan966x_bl2u_emmc_verify(lba, buf_ptr, length);
	}

	return ret;
}

int lan966x_bl2u_qspi_verify(uint32_t offset, uintptr_t buf_ptr, size_t length)
{
	lan966x_key32_t data_write, data_read;
	uint8_t *data = (void*) buf_ptr;
	size_t act_read;
	int ret;

	/* Calculate SHA of written data */
	sha_calc(SHA_MR_ALGO_SHA256, data, length, data_write.b, sizeof(data_write.b));

	ret = qspi_read(offset, buf_ptr, length, &act_read);
	if (ret) {
		NOTICE("qspi_verify: Read returns %d\n", ret);
		return ret;
	}

	if (act_read != length) {
		NOTICE("qspi_verify: Read %zd bytes, expected %zd\n", act_read, length);
		return -EPIPE;
	}

	/* Calculate SHA when read back write */
	sha_calc(SHA_MR_ALGO_SHA256, data, length, data_read.b, sizeof(data_read.b));

	if (memcmp(data_write.b, data_read.b, sizeof(data_write.b)) != 0) {
		NOTICE("SHA mismatch, qspi verify failed");
		return -ENXIO;
	}

	INFO("qspi: Verified %zd bytes of data\n", length);

	return 0;
}

static int fip_read_mmc(const char *name, uintptr_t buf_ptr, uint32_t len)
{
	const partition_entry_t *entry = get_partition_entry(name);

	if (entry) {
		if (entry->length > len) {
			NOTICE("%s FIP is %d bytes, buffer available %d\n",
			       name, (uint32_t) entry->length, len);
			return false;
		}
		return lan966x_bl2u_emmc_read(entry->start, buf_ptr, len);
	}

	NOTICE("Partition %s not found\n", name);
	return -ENOENT;
}

static int fip_read_qspi(uintptr_t buf_ptr, uint32_t len)
{
	return -ENOENT;
}

int fip_update(const char *name, uintptr_t buf_ptr, uint32_t len, bool verify)
{
	const partition_entry_t *entry = get_partition_entry(name);

	if (entry) {
		if (len > entry->length) {
			NOTICE("Partition %s only can hold %d bytes, %d uploaded\n",
			       name, (uint32_t) entry->length, len);
			return false;
		}
		return lan966x_bl2u_emmc_write(entry->start, buf_ptr, len, verify);
	}

	NOTICE("Partition %s not found\n", name);
	return -ENOENT;
}

/* This routine will write the image data flash device */
static void handle_write_image(const bootstrap_req_t * req)
{
	int ret;
	int dev = req->arg0 & 0x7F;
	bool verify = !!(req->arg0 & 0x80);

	VERBOSE("BL2U handle write image\n");

	if (data_rcv_length == 0) {
		bootstrap_TxNack("Flash Image not loaded");
		return;
	}

	if (!valid_write_dev(dev)) {
		bootstrap_TxNack("Unsupported target device");
		return;
	}

	/* Init IO layer */
	lan966x_bl2u_io_init_dev(dev);

	/* Write Flash */
	switch (dev) {
	case BOOT_SOURCE_EMMC:
	case BOOT_SOURCE_SDMMC:
		ret = lan966x_bl2u_emmc_write(0, fip_base_addr, data_rcv_length, verify);
		break;
	case BOOT_SOURCE_QSPI:
		ret = qspi_write(0, (void*) fip_base_addr, data_rcv_length);
		if (ret == 0)
			ret = lan966x_bl2u_qspi_verify(0, fip_base_addr, data_rcv_length);
		break;
	default:
		ret = -ENOTSUP;
	}

	if (ret)
		switch (ret) {
		case -EPIPE:
			bootstrap_TxNack("Image readback failed");
			break;
		case -ENXIO:
			bootstrap_TxNack("Image verify failed");
			break;
		default:
			bootstrap_TxNack_rc("Image write failed", ret);
			break;
		}
	else {
		bootstrap_TxAckStr(verify ? "Image written and verified" : "Image written");
	}
}

#pragma weak lan966x_bl2u_fip_read
int lan966x_bl2u_fip_read(boot_source_type dev,
			  uintptr_t buf,
			  uint32_t len)
{
	int ret = 0;

	/* Generic IO init */
	lan966x_io_setup();

	/* Init IO layer, explicit source */
	if (dev != lan966x_get_boot_source())
		lan966x_bl2u_io_init_dev(dev);

	/* Read Flash */
	switch (dev) {
	case BOOT_SOURCE_EMMC:
	case BOOT_SOURCE_SDMMC:
		INFO("Read FIP %d bytes from %s\n", len, dev == BOOT_SOURCE_EMMC ? "eMMC" : "SD");

		/* Init GPT */
		partition_init(GPT_IMAGE_ID);

		/* Read primary FIP */
		ret = fip_read_mmc(FW_PARTITION_NAME, buf, len);
		if (ret == 0)
			return ret;

		/* Fallback: Read backup FIP */
		ret = fip_read_mmc(FW_BACKUP_PARTITION_NAME, buf, len);
		break;

	case BOOT_SOURCE_QSPI:
		ret = fip_read_qspi(buf, len);
		break;

	default:
		ret = -ENOTSUP;
	}

	return ret;
}

#pragma weak lan966x_bl2u_fip_update
int lan966x_bl2u_fip_update(boot_source_type boot_source,
			    uintptr_t buf,
			    uint32_t len,
			    bool verify)
{
	int ret;

	/* Write Flash */
	switch (boot_source) {
	case BOOT_SOURCE_EMMC:
	case BOOT_SOURCE_SDMMC:
		INFO("Write FIP %d bytes to %s\n", len, boot_source == BOOT_SOURCE_EMMC ? "eMMC" : "SD");

		/* Init GPT */
		partition_init(GPT_IMAGE_ID);

		/* Update primary FIP */
		if ((ret = fip_update(FW_PARTITION_NAME, buf, len, verify)))
			return ret;

		/* Update backup FIP */
		if ((ret = fip_update(FW_BACKUP_PARTITION_NAME, buf, len, verify)))
			return ret;

		break;

	case BOOT_SOURCE_QSPI:
		INFO("Write FIP %d bytes to QSPI NOR\n", len);
		ret = qspi_write(0, (void*) buf, len);
		if (ret == 0)
			ret = lan966x_bl2u_qspi_verify(0, buf, len);
		break;

	default:
		ret = -ENOTSUP;
	}

	return ret;
}

/* This routine will write the previous encrypted FIP data to the flash device */
static void handle_write_fip(const bootstrap_req_t * req)
{
	int ret;
	int dev = req->arg0 & 0x7F;
	bool verify = !!(req->arg0 & 0x80);

	VERBOSE("BL2U handle write data\n");

	if (data_rcv_length == 0) {
		bootstrap_TxNack("FIP Image not loaded");
		return;
	}

	if (!valid_write_dev(dev)) {
		bootstrap_TxNack("Unsupported target device");
		return;
	}

	if (!is_valid_fip_hdr((const fip_toc_header_t *)fip_base_addr)) {
		bootstrap_TxNack("Data is not a valid FIP");
		return;
	}

	/* Generic IO init */
	lan966x_io_setup();

	/* Init IO layer, explicit source */
	if (dev != lan966x_get_boot_source())
		lan966x_bl2u_io_init_dev(dev);

	/* Do the update - platform dependent */
	ret = lan966x_bl2u_fip_update(dev, fip_base_addr, data_rcv_length, verify);

	if (ret < 0) {
		switch (ret) {
		case -EPIPE:
			bootstrap_TxNack("FIP readback failed");
			break;
		case -ENXIO:
			bootstrap_TxNack("FIP verify failed");
			break;
		case -ENOENT:
			bootstrap_TxNack("FIP partition not found");
			break;
		case -EINVAL:
			bootstrap_TxNack("FIP partition too small");
			break;
		default:
			bootstrap_TxNack_rc("Write FIP failed", ret);
			break;
		}
	} else {
		bootstrap_TxAckStr(verify ? "FIP written and verified" : "FIP written");
	}
}

static void handle_bind(const bootstrap_req_t *req)
{
	fw_bind_res_t result;

	VERBOSE("BL2U handle bind operation\n");

	if (data_rcv_length == 0 || data_rcv_length > default_ddr_config.info.size) {
		bootstrap_TxNack("Image not loaded, length error");
		return;
	}

	/* Parse FIP for encrypted image files */
	result = lan966x_bind_fip(fip_base_addr, data_rcv_length, NULL);
	if (result) {
		bootstrap_TxNack(lan966x_bind_err2str(result));
	} else {
		VERBOSE("FIP image successfully accessed\n");
		bootstrap_TxAck();
	}
}

/*
 * Bind FIP in firmware flash.  Reads FIP from device, decrypts with
 * SSK, re-encrypts with BSSK, writes FIP back to flash.
 */
static void handle_bind_flash(const bootstrap_req_t *req)
{
	int dev = req->arg0 & 0x7F;
	bool verify = !!(req->arg0 & 0x80);
	uint32_t buffer_max = (sram_available * 2); /* Use SRAM as single buffer */
	size_t fip_size;
	int ret;

	if (buffer_max == 0) {
		bootstrap_TxNack("No SRAM available");
		return;
	}

	if (!valid_write_dev(dev)) {
		bootstrap_TxNack("Unsupported target device");
		return;
	}

	/* Generic IO init */
	lan966x_io_setup();

	/* Init IO layer, explicit source */
	if (dev != lan966x_get_boot_source())
		lan966x_bl2u_io_init_dev(dev);

	ret = lan966x_bl2u_fip_read(dev, (uintptr_t) sram_buffer, buffer_max);
	if (ret) {
		bootstrap_TxNack("Unable to read FIP");
		return;
	}

	if (!is_valid_fip_hdr((const fip_toc_header_t *) sram_buffer)) {
		bootstrap_TxNack("Data is not a valid FIP");
		return;
	}

	/* Do the bind */
	fw_bind_res_t result = lan966x_bind_fip((uintptr_t) sram_buffer, buffer_max, &fip_size);
	if (result != FW_BIND_OK) {
		bootstrap_TxNack(lan966x_bind_err2str(result));
		return;
	}

	NOTICE("FIP: Seen %zd bytes, max %d\n", fip_size, buffer_max);

	/* Write back 'bound' FIP */
	ret = lan966x_bl2u_fip_update(dev, (uintptr_t) sram_buffer, fip_size, verify);
	if (ret != 0) {
		bootstrap_TxNack("Error writing bound FIP");
		return;
	}

	/* All good */
	bootstrap_TxAck();
}

static bool set_cache(bool cache)
{
	uint32_t attr;
#if defined(PLAT_XLAT_TABLES_DYNAMIC)
	int ret;
#endif

	if (cache == cur_cache) {
		NOTICE("Cache already %s\n", cur_cache ? "enabled" : "disabled");
		return true;
	}

	/* Handle cache state */
	if (cur_cache) {
		/* on -> off: flush */
		flush_dcache_range(LAN966X_DDR_BASE, LAN966X_DDR_MAX_SIZE);
	} else {
		/* off -> on: Invalidate */
		inv_dcache_range(LAN966X_DDR_BASE, LAN966X_DDR_MAX_SIZE);
	}

	attr = MT_RW | MT_NS | MT_EXECUTE_NEVER;
	if (cache) {
		attr |= MT_MEMORY;
	} else {
		attr |= MT_NON_CACHEABLE;
	}

#if defined(PLAT_XLAT_TABLES_DYNAMIC)
	/* 1st remove old mapping */
	mmap_remove_dynamic_region(LAN966X_DDR_BASE, LAN966X_DDR_MAX_SIZE);

	/* Add region so attributes are updated */
	ret = mmap_add_dynamic_region(LAN966X_DDR_BASE, LAN966X_DDR_BASE, LAN966X_DDR_MAX_SIZE, attr);

	if (ret != 0) {
		bootstrap_TxNack("DDR mapping error");
		return false;
	}
#else
	if (cache) {
		bootstrap_TxNack("No support for enabling the cache");
		return false;
	}
#endif

	/* Update cache state */
	cur_cache = cache;
	NOTICE("Cache now %s\n", cur_cache ? "enabled" : "disabled");

	return true;
}

static void handle_ddr_init(bootstrap_req_t *req)
{
	/* Initialize DDR, possibly with defaults */
	/* Anycase, make sure cache is enabled */
	if (!ddr_was_initialized) {
		set_cache(false);
		ddr_was_initialized = ddr_init(&current_ddr_config) == 0;
		if (!ddr_was_initialized) {
			bootstrap_TxNack("DDR initialization error");
		} else {
			if (!set_cache(true))
				bootstrap_TxNack("CPU data cache could not be enabled");
			else
				bootstrap_TxAckStr("DDR was initialized");
		}
	} else {
		if (!set_cache(true))
			bootstrap_TxNack("CPU data cache could not be enabled");
		else
			bootstrap_TxAckStr("DDR already initialized");
	}
}

static void handle_ddr_cfg_set(bootstrap_req_t *req)
{
#if defined(LAN966X_ASIC) || defined(LAN969X_ASIC)
	if (req->len == sizeof(current_ddr_config)) {
		if (bootstrap_RxDataCrc(req, (uint8_t *)&current_ddr_config)) {
			ddr_was_initialized = ddr_init(&current_ddr_config) == 0;
			if (ddr_was_initialized)
				bootstrap_Tx(BOOTSTRAP_ACK, req->arg0, 0, NULL);
			else
				bootstrap_TxNack(ddr_failure_details);
		} else
			bootstrap_TxNack("DDR config rx data failed");
	} else
		bootstrap_TxNack("Illegal DDR config size");
#else
	bootstrap_TxNack("Not supported");
#endif
}

static void handle_ddr_cfg_get(bootstrap_req_t *req)
{
	bootstrap_TxAckData(&current_ddr_config, sizeof(current_ddr_config));
}

static void handle_ddr_test(bootstrap_req_t *req)
{
	bool cache = !!(req->arg0 & 1);
	uintptr_t err_off;

	if (!ddr_was_initialized) {
		bootstrap_TxNack("DDR not initialized");
		return;
	}

	if (!set_cache(cache))
		return;		/* Error */

	/* Now, do tests */

	err_off = ddr_test_data_bus(ddr_base_addr, cache);
	if (err_off != 0) {
		bootstrap_TxNack_rc("DDR data bus test", err_off);
		return;
	}

	err_off = ddr_test_addr_bus(ddr_base_addr, current_ddr_config.info.size, cache);
	if (err_off != 0) {
		bootstrap_TxNack_rc("DDR address bus test", err_off);
		return;
	}

	err_off = ddr_test_rnd(ddr_base_addr, current_ddr_config.info.size, cache, 0xdeadbeef);
	if (err_off != 0) {
		bootstrap_TxNack_rc("DDR sweep test", err_off);
		return;
	}

	/* All good */
	bootstrap_TxAckStr("Test succeeded");
}

static void handle_data_hash(bootstrap_req_t *req)
{
	uint32_t data_len;
	const void *data;

	/* Where is data? */
	if (req->arg0) {
		data = sram_buffer;
		data_len = MIN((req->arg0 & 0x7FFFFFFF), sram_available);
	} else {
		data = (const void*) fip_base_addr;
		data_len = data_rcv_length;
	}

	if (data_len == 0) {
		bootstrap_TxNack("No downloaded data");
		return;
	}

	bootstrap_do_sha(data, data_len);
}

#define MAX_REG_READ	128	/* Arbitrary */
static void handle_read_reg(bootstrap_req_t *req)
{
	static uint32_t regbuf[MAX_REG_READ];
	uint32_t i, nreg;

	if (req->len > (MAX_REG_READ * sizeof(uint32_t))) {
		bootstrap_TxNack("Max register count/request exceeded");
		return;
	}

	if (!bootstrap_RxDataCrc(req, (uint8_t *)regbuf)) {
		bootstrap_TxNack("Read reg rx data failed");
		return;
	}

	/* Do the reads, return value per offset */
	nreg = req->len / sizeof(uint32_t);
	for (i = 0; i < nreg; i++) {
		regbuf[i] = mmio_read_32(regbuf[i]);
	}

	/* Return hash, length */
	bootstrap_TxAckData_arg(regbuf, req->len, req->arg0);
}

static void handle_sram_info(bootstrap_req_t *req)
{
	/* Return SRAM length */
	bootstrap_Tx(BOOTSTRAP_ACK, sram_available, 0, NULL);
}

void lan966x_bl2u_bootstrap_monitor(void)
{
	bool exit_monitor = false;
	bootstrap_req_t req = { 0 };

	INFO("*** ENTERING BL2U BOOTSTRAP MONITOR ***\n");

	/* Initialize DDR config work buffer */
	current_ddr_config = default_ddr_config;

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
		else if (is_cmd(&req, BOOTSTRAP_UNZIP))		// Z - Unzip data
			handle_unzip_data(&req);
		else if (is_cmd(&req, BOOTSTRAP_IMAGE))		// I - Copy uploaded raw image from DDR memory to flash device
			handle_write_image(&req);
		else if (is_cmd(&req, BOOTSTRAP_WRITE))		// W - Copy uploaded fip from DDR memory to flash device
			handle_write_fip(&req);
		else if (is_cmd(&req, BOOTSTRAP_BIND))		// B - FW binding operation (decrypt and encrypt)
			handle_bind(&req);
		else if (is_cmd(&req, BOOTSTRAP_BIND_FLASH))	// b - FW binding operation (decrypt and encrypt)
			handle_bind_flash(&req);
		else if (is_cmd(&req, BOOTSTRAP_OTPD))
			handle_otp_data(&req);
		else if (is_cmd(&req, BOOTSTRAP_OTPR))
			handle_otp_random(&req);
		else if (is_cmd(&req, BOOTSTRAP_OTP_READ))	// L - Read OTP data
			handle_otp_read(&req);
		else if (is_cmd(&req, BOOTSTRAP_DDR_INIT))	// d - DDR init before data
			handle_ddr_init(&req);
		else if (is_cmd(&req, BOOTSTRAP_DDR_CFG_SET))	// C - Set DDR config
			handle_ddr_cfg_set(&req);
		else if (is_cmd(&req, BOOTSTRAP_DDR_CFG_GET))	// c - Get DDR config
			handle_ddr_cfg_get(&req);
		else if (is_cmd(&req, BOOTSTRAP_DDR_TEST))	// T - Perform DDR test
			handle_ddr_test(&req);
		else if (is_cmd(&req, BOOTSTRAP_DATA_HASH))	// H - Get data hash
			handle_data_hash(&req);
		else if (is_cmd(&req, BOOTSTRAP_READ_REG))	// x - Read registers
			handle_read_reg(&req);
		else if (is_cmd(&req, BOOTSTRAP_SRAM_INFO))	// s - Get SRAM info
			handle_sram_info(&req);
		else if (is_cmd(&req, BOOTSTRAP_SEND_SRAM))	// J - Upload SRAM
			handle_send_sram(&req);
		else if (is_cmd(&req, BOOTSTRAP_WRITE_READBACK)) // j - Write SRAM data to device, with readback
			handle_write_readback(&req);
		else
			bootstrap_TxNack("Unknown command");
	}

	INFO("*** EXITING BL2U BOOTSTRAP MONITOR ***\n");
}
