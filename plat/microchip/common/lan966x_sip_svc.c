/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>

#include <common/debug.h>
#include <common/runtime_svc.h>
#include <drivers/microchip/lan966x_trng.h>
#include <drivers/microchip/sha.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>
#include <platform_def.h>
#include <tools_share/firmware_encrypted.h>
#include <tools_share/uuid.h>

#include <lan96xx_common.h>
#include <lan966x_sip_svc.h>
#include <lan966x_sjtag.h>
#include <lan966x_fw_bind.h>
#include <lan966x_regs.h>
#include <lan966x_ns_enc.h>

#include <drivers/microchip/aes.h>

#if defined(PLAT_LAN969X_NS_IMAGE_BASE)
#define PLAT_LAN966X_NS_IMAGE_BASE	PLAT_LAN969X_NS_IMAGE_BASE
#define PLAT_LAN966X_NS_IMAGE_SIZE	PLAT_LAN969X_NS_IMAGE_SIZE
#define PLAT_LAN966X_NS_IMAGE_LIMIT	PLAT_LAN969X_NS_IMAGE_LIMIT
#endif

/* MCHP SiP Service UUID */
DEFINE_SVC_UUID2(microchip_sip_svc_uid,
		 0x10c149b6, 0xd31c, 0x4626, 0xaa, 0x79,
		 0x15, 0x5c, 0xe7, 0x50, 0xbb, 0xf3);

#pragma weak microchip_plat_ns_ddr_base
uintptr_t microchip_plat_ns_ddr_base(void)
{
	return PLAT_LAN966X_NS_IMAGE_BASE;
}

#pragma weak microchip_plat_ns_ddr_size
size_t microchip_plat_ns_ddr_size(void)
{
	return PLAT_LAN966X_NS_IMAGE_SIZE;
}

#pragma weak microchip_plat_board_number
u_register_t microchip_plat_board_number(void)
{
	return 0;
}

#pragma weak microchip_plat_boot_offset
u_register_t microchip_plat_boot_offset(void)
{
	return 0;
}

#pragma weak microchip_plat_sram_info
u_register_t microchip_plat_sram_info(u_register_t index,
				      u_register_t *addr,
				      u_register_t *size)
{
	return SMC_ARCH_CALL_NOT_SUPPORTED;
}

#pragma weak microchip_plat_bl2_version
u_register_t microchip_plat_bl2_version(void)
{
	return 0;
}

static bool is_ns_ddr(uint32_t size, uintptr_t addr)
{
	size_t my_size = microchip_plat_ns_ddr_size();
	uintptr_t my_base = microchip_plat_ns_ddr_base();

	if (size > my_size)
		return false;
	if (addr < my_base)
		return false;
	if ((addr + size) > (my_base + my_size))
		return false;

	return true;
}

#pragma weak microchip_plat_sip_handler
uintptr_t microchip_plat_sip_handler(uint32_t smc_fid,
				     u_register_t x1,
				     u_register_t x2,
				     u_register_t x3,
				     u_register_t x4,
				     void *cookie,
				     void *handle,
				     u_register_t flags)
{
	SMC_RET1(handle, SMC_UNK);
}

static uintptr_t sip_sjtag_get_challenge(uintptr_t challenge, size_t size, void *handle)
{
	lan966x_key32_t key;

	if (!is_ns_ddr(size, challenge) ||
	    size != ARRAY_SIZE(key.b)) {
		SMC_RET1(handle, SMC_ARCH_CALL_INVAL_PARAM);
	} else {
		int ret = lan966x_sjtag_read_challenge(&key);
		if (ret) {
			/* SJTAG not enabled */
			SMC_RET1(handle, SMC_ARCH_CALL_NOT_SUPPORTED);
		} else {
			/* Return data in provided buffer */
			memcpy((void*) challenge, key.b, ARRAY_SIZE(key.b));
			flush_dcache_range(challenge, size);
			SMC_RET2(handle, SMC_ARCH_CALL_SUCCESS, challenge);
		}
	}
}

static uintptr_t sip_sjtag_unlock(uintptr_t response, size_t size, void *handle)
{
	if (!is_ns_ddr(size, response) ||
	    size != LAN966X_KEY32_LEN) {
		SMC_RET1(handle, SMC_ARCH_CALL_INVAL_PARAM);
	} else {
		int ret;

		inv_dcache_range(response, size);
		ret = lan966x_sjtag_write_response((void*) response);

		SMC_RET1(handle, ret ? SMC_UNK : SMC_OK);
	}
}

static uintptr_t sip_fw_bind(uintptr_t fip, uint32_t size, void *handle)
{
	if (!is_ns_ddr(size, fip)) {
		SMC_RET1(handle, SMC_ARCH_CALL_INVAL_PARAM);
	} else {
		lan966x_key32_t sha_in, sha_out;
		fw_bind_res_t res;

		sha_calc(SHA_MR_ALGO_SHA256, (void*) fip, size, sha_in.b);

		res = lan966x_bind_fip(fip, size);
		if (res) {
			SMC_RET1(handle, -res);
		} else {
			sha_calc(SHA_MR_ALGO_SHA256, (void*) fip, size, sha_out.b);
			flush_dcache_range(fip, size);
			SMC_RET3(handle, SMC_ARCH_CALL_SUCCESS, sha_in.w[0], sha_out.w[0]);
		}
	}
}

static uintptr_t sip_ns_encrypt(uintptr_t enc, uintptr_t data, void *handle)
{
	struct ns_enc_hdr *encp = (void*) enc;
	uint8_t key[KEY_SIZE] = { 0 };
	uint32_t iv[IV_SIZE / sizeof(uint32_t)] = { 0 };
	uint8_t tag[TAG_SIZE] = { 0 };
	size_t key_len = sizeof(key);
	int result;

	if (!is_ns_ddr(sizeof(*encp), enc))
		SMC_RET1(handle, SMC_ARCH_CALL_INVAL_PARAM);

	/* Invalidate cache for args */
	inv_dcache_range(enc, sizeof(*encp));

	if (!is_ns_ddr(encp->data_length, data))
		SMC_RET1(handle, SMC_ARCH_CALL_INVAL_PARAM);

	if (!is_valid_enc_hdr(encp))
		SMC_RET1(handle, SMC_ARCH_CALL_INVAL_PARAM);

	/* Invalidate cache for data */
	inv_dcache_range(data, encp->data_length);

	/* Retrieve key data */
	result = lan96xx_get_ns_enc_key(encp->flags, key, &key_len);

	/* Initialize iv array with random data */
	for (int i = 0; i < ARRAY_SIZE(iv); i++) {
		iv[i] = lan966x_trng_read();
	}

	/* Encrypt image data on-the-fly in memory  */
	result = aes_gcm_encrypt((void*) data, encp->data_length,
				 key, key_len,
				 (uint8_t *)iv, sizeof(iv),
				 tag, sizeof(tag));

	if (result)
		SMC_RET1(handle, SMC_UNK);

	/* Update header data */
	encp->algo = CRYPTO_GCM_DECRYPT;
	encp->iv_len = sizeof(iv);
	encp->tag_len = sizeof(tag);
	memcpy(encp->iv, iv, sizeof(iv));
	memcpy(encp->tag, tag, sizeof(tag));

	/* Flush cache, both data and encryption header */
	flush_dcache_range(data, encp->data_length);
	flush_dcache_range(enc, sizeof(*encp));

	SMC_RET1(handle, SMC_ARCH_CALL_SUCCESS);
}

static uintptr_t sip_ns_decrypt(uintptr_t enc, uintptr_t data, void *handle)
{
	const struct ns_enc_hdr *encp = (void*) enc;
	uint8_t key[KEY_SIZE] = { 0 };
	size_t key_len = sizeof(key);
	int result;

	if (!is_ns_ddr(sizeof(*encp), enc))
		SMC_RET1(handle, SMC_ARCH_CALL_INVAL_PARAM);

	/* Invalidate cache for args */
	inv_dcache_range(enc, sizeof(*encp));

	if (!is_ns_ddr(encp->data_length, data))
		SMC_RET1(handle, SMC_ARCH_CALL_INVAL_PARAM);

	if (!is_valid_enc_hdr(encp) ||
	    encp->iv_len != IV_SIZE ||
	    encp->tag_len != TAG_SIZE)
		SMC_RET1(handle, SMC_ARCH_CALL_INVAL_PARAM);

	/* Invalidate cache for data */
	inv_dcache_range(data, encp->data_length);

	/* Retrieve key data */
	result = lan96xx_get_ns_enc_key(encp->flags, key, &key_len);

	/* Decrypt image data on-the-fly in memory  */
	result = aes_gcm_decrypt((void*) data, encp->data_length,
				 key, key_len,
				 encp->iv,
				 encp->iv_len,
				 encp->tag,
				 encp->tag_len);

	if (result)
		SMC_RET1(handle, SMC_UNK);

	/* Flush data from cache */
	flush_dcache_range(data, encp->data_length);

	SMC_RET1(handle, SMC_ARCH_CALL_SUCCESS);
}

/*
 * This function is responsible for handling all SiP calls from the NS world
 */
static uintptr_t sip_smc_handler(uint32_t smc_fid,
				 u_register_t x1,
				 u_register_t x2,
				 u_register_t x3,
				 u_register_t x4,
				 void *cookie,
				 void *handle,
				 u_register_t flags)
{
	switch (smc_fid) {
	case SIP_SVC_UID:
		/* Return UID to the caller */
		SMC_UUID_RET(handle, microchip_sip_svc_uid);

	case SIP_SVC_VERSION:
		/* Return the version of current implementation */
		SMC_RET2(handle, SIP_SVC_VERSION_MAJOR,
			 SIP_SVC_VERSION_MINOR);

	case SIP_SVC_SJTAG_STATUS:
	{
		uint32_t status, int_st;

		status = mmio_read_32(SJTAG_CTL(LAN966X_SJTAG_BASE));
		int_st = mmio_read_32(SJTAG_INT_STATUS(LAN966X_SJTAG_BASE));

		/* Return SJTAG status info */
		SMC_RET3(handle, SMC_OK, status, int_st);
	}

	case SIP_SVC_SJTAG_CHALLENGE:
		/* Return challenge data */
		return sip_sjtag_get_challenge(x1, x2, handle);

	case SIP_SVC_SJTAG_UNLOCK:
		/* Perform unlock */
		return sip_sjtag_unlock(x1, x2, handle);

	case SIP_SVC_FW_BIND:
		/* Handle bind firmware re-encryption with BSSK */
		return sip_fw_bind(x1, x2, handle);

	case SIP_SVC_NS_ENCRYPT:
		/* Handle NS encryption */
		return sip_ns_encrypt(x1, x2, handle);

	case SIP_SVC_NS_DECRYPT:
		/* Handle NS encryption */
		return sip_ns_decrypt(x1, x2, handle);

	case SIP_SVC_GET_BOOTSRC:
		SMC_RET2(handle, SMC_OK, lan966x_get_boot_source());
		/* break is not required as SMC_RETx return */

	case SIP_SVC_GET_DDR_SIZE:
		SMC_RET2(handle, SMC_OK, microchip_plat_ns_ddr_size());
		/* break is not required as SMC_RETx return */

	case SIP_SVC_GET_BOARD_NO:
		SMC_RET2(handle, SMC_OK, microchip_plat_board_number());
		/* break is not required as SMC_RETx return */

	case SIP_SVC_GET_BOOT_OFF:
		SMC_RET3(handle, SMC_OK,
			 lan966x_get_boot_source(),
			 microchip_plat_boot_offset());
		/* break is not required as SMC_RETx return */

	case SIP_SVC_SRAM_INFO:
	{
		u_register_t ret, addr, size;

		ret = microchip_plat_sram_info(x1, &addr, &size);
		if (ret == SMC_OK) {
			SMC_RET3(handle, ret, addr, size);
		}
		/* Failure */
		SMC_RET3(handle, ret, 0, 0);
		/* break is not required as SMC_RETx return */
	}

	case SIP_SVC_BL2_VERSION:
		SMC_RET2(handle, SMC_OK,
			 microchip_plat_bl2_version());
		/* break is not required as SMC_RETx return */

	default:
		return microchip_plat_sip_handler(smc_fid, x1, x2, x3, x4,
						  cookie, handle, flags);
	}
}

/* Define a runtime service descriptor for fast SMC calls */
DECLARE_RT_SVC(
	microchip_sip_svc,
	OEN_SIP_START,
	OEN_SIP_END,
	SMC_TYPE_FAST,
	NULL,
	sip_smc_handler
);
