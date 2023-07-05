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

#include "otp.h"

#define DDR_PATTERN1 0xAAAAAAAAU
#define DDR_PATTERN2 0x55555555U

#define MAX_OTP_DATA	1024

#define PAGE_ALIGN(x, a)	(((x) + (a) - 1) & ~((a) - 1))

static const uintptr_t fip_base_addr = LAN966X_DDR_BASE;
static uint32_t data_rcv_length;
static struct ddr_config current_ddr_config;
static const uintptr_t ddr_base_addr = LAN966X_DDR_BASE;
static bool ddr_was_initialized;

#if defined(MCHP_SOC_LAN969X)
extern const struct ddr_config lan969x_ddr_config;
#define  default_ddr_config lan969x_ddr_config
#elif defined(MCHP_SOC_LAN966X)
extern const struct ddr_config lan966x_ddr_config;
#define  default_ddr_config lan966x_ddr_config
#endif

/*******************************************************************************
 * This function tests the DDR data bus wiring.
 * This is inspired from the Data Bus Test algorithm written by Michael Barr
 * in "Programming Embedded Systems in C and C++" book.
 * resources.oreilly.com/examples/9781565923546/blob/master/Chapter6/
 * File: memtest.c - This source code belongs to Public Domain.
 * Returns 0 if success, and address value else.
 ******************************************************************************/
static uintptr_t ddr_test_data_bus(bool cache)
{
	uint32_t pattern;

	INFO("DDR data bus begin\n");

	if (cache)
		inv_dcache_range(LAN966X_DDR_BASE, LAN966X_DDR_MAX_SIZE);

	for (pattern = 1U; pattern != 0U; pattern <<= 1) {
		mmio_write_32(ddr_base_addr, pattern);

		if (mmio_read_32(ddr_base_addr) != pattern) {
			return (uintptr_t) ddr_base_addr;
		}
	}

	if (cache)
		clean_dcache_range(LAN966X_DDR_BASE, LAN966X_DDR_MAX_SIZE);

	INFO("DDR data bus end\n");

	return 0;
}

/*******************************************************************************
 * This function tests the DDR address bus wiring.
 * This is inspired from the Data Bus Test algorithm written by Michael Barr
 * in "Programming Embedded Systems in C and C++" book.
 * resources.oreilly.com/examples/9781565923546/blob/master/Chapter6/
 * File: memtest.c - This source code belongs to Public Domain.
 * Returns 0 if success, and address value else.
 ******************************************************************************/
static uintptr_t ddr_test_addr_bus(struct ddr_config *config, bool cache)
{
	uint64_t addressmask = (MIN(config->info.size, (uint32_t) LAN966X_DDR_MAX_SIZE) - 1U);
	uint64_t offset;
	uint64_t testoffset = 0;

	INFO("DDR addr bus begin\n");

	if (cache)
		inv_dcache_range(LAN966X_DDR_BASE, LAN966X_DDR_MAX_SIZE);

	/* Write the default pattern at each of the power-of-two offsets. */
	for (offset = sizeof(uint32_t); (offset & addressmask) != 0U; offset <<= 1U) {
		mmio_write_32(ddr_base_addr + offset, DDR_PATTERN1);
	}

	/* Check for address bits stuck high. */
	mmio_write_32(ddr_base_addr + testoffset, DDR_PATTERN2);

	for (offset = sizeof(uint32_t); (offset & addressmask) != 0U; offset <<= 1U) {
		if (mmio_read_32(ddr_base_addr + offset) != DDR_PATTERN1) {
			return (ddr_base_addr + offset);
		}
	}

	mmio_write_32(ddr_base_addr + testoffset, DDR_PATTERN1);

	/* Check for address bits stuck low or shorted. */
	for (testoffset = sizeof(uint32_t); (testoffset & addressmask) != 0U; testoffset <<= 1U) {
		mmio_write_32(ddr_base_addr + testoffset, DDR_PATTERN2);

		if (mmio_read_32(ddr_base_addr) != DDR_PATTERN1) {
			return ddr_base_addr;
		}

		for (offset = sizeof(uint32_t); (offset & addressmask) != 0U; offset <<= 1U) {
			if ((mmio_read_32(ddr_base_addr +
					  offset) != DDR_PATTERN1) &&
			    (offset != testoffset)) {
				return (ddr_base_addr + offset);
			}
		}

		mmio_write_32(ddr_base_addr + testoffset, DDR_PATTERN1);
	}

	if (cache)
		clean_dcache_range(LAN966X_DDR_BASE, LAN966X_DDR_MAX_SIZE);

	INFO("DDR addr bus end\n");

	return 0;
}

static inline unsigned int ps_rnd(unsigned int x)
{
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

static uintptr_t ddr_test_rnd(struct ddr_config *config, bool cache, uint32_t seed)
{
	uint32_t max_size = MIN(config->info.size, (uint32_t) LAN966X_DDR_MAX_SIZE);
	uint32_t offset;
	unsigned int value;

	if (cache)
		inv_dcache_range(LAN966X_DDR_BASE, LAN966X_DDR_MAX_SIZE);

	for (offset = 0, value = seed; offset < max_size; offset += sizeof(uint32_t)) {
		value = ps_rnd(value);
		mmio_write_32(ddr_base_addr + offset, (uint32_t) value);
	}

	for (offset = 0, value = seed; offset < max_size; offset += sizeof(uint32_t)) {
		value = ps_rnd(value);
		if (mmio_read_32(ddr_base_addr + offset) != (uint32_t) value) {
			return (ddr_base_addr + offset);
		}
	}

	if (cache)
		clean_dcache_range(LAN966X_DDR_BASE, LAN966X_DDR_MAX_SIZE);

	return 0;
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

static void handle_load_data(const bootstrap_req_t *req)
{
	uint32_t length = req->arg0;
	uint8_t *ptr;
	int num_bytes, offset;
	data_rcv_length = 0;

	VERBOSE("BL2U handle load data\n");

	if (length == 0 || length > default_ddr_config.info.size) {
		bootstrap_TxNack("Length Error");
		return;
	}

	/* Initialize DDR, possibly with defaults */
	if (!ddr_was_initialized) {
		ddr_was_initialized = ddr_init(&current_ddr_config) == 0;
		if (!ddr_was_initialized) {
			bootstrap_TxNack("DDR initialization error");
			return;
		}
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

static void handle_unzip_data(const bootstrap_req_t *req)
{
	uint8_t *ptr = (uint8_t *)fip_base_addr;
	const char *resp = "Plain data";

	/* See if this is compressed data */
	if (ptr[0] == 0x1f && ptr[1] == 0x8b) {
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
 * NOTE: Instead of calling mmc_write_blocks() directly we have to
 * spoon feed individual blocks. This is needed due to a constraint at a
 * lower level of the MMC driver.
 */
static uint32_t single_mmc_write_blocks(uint32_t lba, uintptr_t buf_ptr, uint32_t length)
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

int lan966x_bl2u_emmc_verify(uint32_t lba, uintptr_t buf_ptr, size_t length)
{
	uintptr_t workbuf =  buf_ptr + PAGE_ALIGN(length, SIZE_M(1));
	uint8_t *data;
	size_t nread;

	for (nread = 0, data = (uint8_t *) buf_ptr; nread < length; ) {
		     size_t chunk = MIN((size_t) SIZE_M(1),
					(size_t) (length - nread));
		     size_t round_len = DIV_ROUND_UP_2EVAL(chunk, MMC_BLOCK_SIZE) * MMC_BLOCK_SIZE;

		     size_t size_read = mmc_read_blocks(lba, workbuf, round_len);

		     if (size_read != round_len) {
			     NOTICE("emmc_verify: Read %zd bytes, expected %zd\n", size_read, round_len);
			     return -EPIPE;
		     }

		     /* NB: Only compare actual chunk size */
		     if (memcmp((uint8_t *) workbuf, data, chunk) != 0) {
			     NOTICE("emmc_verify: Mismatch at address %p vs %p, block size %zd\n",
				    (uint8_t *) workbuf, data, chunk);
			     return -ENXIO;
		     }

		     nread += chunk;
		     data += chunk;
		     lba += (chunk / MMC_BLOCK_SIZE);
	}

	NOTICE("emmc: Verified %zd bytes of data\n", length);

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

	written = single_mmc_write_blocks(lba, buf_ptr, round_len);

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
	uintptr_t workbuf =  buf_ptr + PAGE_ALIGN(length, SIZE_M(1));
	uint8_t *data;
	size_t nread, act_read;

	for (nread = 0, data = (uint8_t *) buf_ptr; nread < length; ) {
		     size_t chunk = MIN((size_t) SIZE_M(1),
					(size_t) (length - nread));

		     int ret = qspi_read(offset + nread, workbuf, chunk, &act_read);

		     if (ret) {
			     NOTICE("qspi_verify: Read returns %d\n", ret);
			     return ret;
		     }

		     if (act_read != chunk) {
			     NOTICE("qspi_verify: Read %zd bytes, expected %zd\n", act_read, chunk);
			     return -EPIPE;
		     }

		     /* NB: Only compare actual chunk size */
		     if (memcmp((uint8_t *) workbuf, data, chunk) != 0) {
			     NOTICE("qspi_verify: Mismatch at address %p vs %p, block size %zd\n",
				    (uint8_t *) workbuf, data, chunk);
			     return -ENXIO;
		     }

		     nread += chunk;
		     data += chunk;
	}

	NOTICE("qspi: Verified %zd bytes of data\n", length);

	return 0;
}

static int fip_update(const char *name, uintptr_t buf_ptr, uint32_t len, bool verify)
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

static bool valid_write_dev(boot_source_type dev)
{
	if (dev == BOOT_SOURCE_EMMC ||
	    dev == BOOT_SOURCE_SDMMC ||
	    dev == BOOT_SOURCE_QSPI)
		return true;

	return false;
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

		/* All OK to start */
		ret = 0;

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
	result = lan966x_bind_fip(fip_base_addr, data_rcv_length);
	if (result) {
		bootstrap_TxNack(lan966x_bind_err2str(result));
	} else {
		VERBOSE("FIP image successfully accessed\n");
		bootstrap_TxAck();
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
				bootstrap_TxNack("DDR initialization failed");
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
	uintptr_t err_off;
	uint32_t attr;
	bool cache;
#if defined(PLAT_XLAT_TABLES_DYNAMIC)
	int ret;
#endif

	if (!ddr_was_initialized) {
		bootstrap_TxNack("DDR not initialized");
		return;
	}

	attr = MT_RW | MT_NS | MT_EXECUTE_NEVER;
	cache = !!(req->arg0 & 1);
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
		return;
	}
#else
	if (cache) {
		bootstrap_TxNack("No support for enabling the cache");
		return;
	}
#endif

	/* Now, do tests */

	err_off = ddr_test_data_bus(cache);
	if (err_off != 0) {
		bootstrap_TxNack_rc("DDR data bus test", err_off);
		return;
	}

	err_off = ddr_test_addr_bus(&current_ddr_config, cache);
	if (err_off != 0) {
		bootstrap_TxNack_rc("DDR data bus test", err_off);
		return;
	}

	err_off = ddr_test_rnd(&current_ddr_config, cache, 0xdeadbeef);
	if (err_off != 0) {
		bootstrap_TxNack_rc("DDR sweep test", err_off);
		return;
	}

	/* All good */
	bootstrap_TxAckStr("Test succeeded");
}

static void handle_data_hash(bootstrap_req_t *req)
{
	lan966x_key32_t data_sig;

	if (data_rcv_length == 0) {
		bootstrap_TxNack("No downloaded data");
		return;
	}

	/* Calc hash */
	sha_calc(SHA_MR_ALGO_SHA256, (void*) fip_base_addr, data_rcv_length, data_sig.b);
	/* Return hash, length */
	bootstrap_TxAckData_arg(data_sig.b, sizeof(data_sig.b), data_rcv_length);
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
		else if (is_cmd(&req, BOOTSTRAP_OTPD))
			handle_otp_data(&req);
		else if (is_cmd(&req, BOOTSTRAP_OTPR))
			handle_otp_random(&req);
		else if (is_cmd(&req, BOOTSTRAP_OTP_READ))	// L - Read OTP data
			handle_otp_read(&req);
		else if (is_cmd(&req, BOOTSTRAP_DDR_CFG_SET))	// C - Set DDR config
			handle_ddr_cfg_set(&req);
		else if (is_cmd(&req, BOOTSTRAP_DDR_CFG_GET))	// c - Get DDR config
			handle_ddr_cfg_get(&req);
		else if (is_cmd(&req, BOOTSTRAP_DDR_TEST))	// T - Perform DDR test
			handle_ddr_test(&req);
		else if (is_cmd(&req, BOOTSTRAP_DATA_HASH))	// H - Get data hash
			handle_data_hash(&req);
		else
			bootstrap_TxNack("Unknown command");
	}

	INFO("*** EXITING BL2U BOOTSTRAP MONITOR ***\n");
}
