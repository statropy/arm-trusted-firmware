/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>

#include <common/debug.h>
#include <common/runtime_svc.h>
#include <drivers/microchip/sha.h>
#include <lib/mmio.h>
#include <platform_def.h>
#include <tools_share/uuid.h>

#include <lan966x_sip_svc.h>
#include <lan966x_private.h>
#include <lan966x_fw_bind.h>
#include <lan966x_regs.h>

/* MCHP SiP Service UUID */
DEFINE_SVC_UUID2(microchip_sip_svc_uid,
		 0x10c149b6, 0xd31c, 0x4626, 0xaa, 0x79,
		 0x15, 0x5c, 0xe7, 0x50, 0xbb, 0xf3);

static bool is_ns_ddr(uint32_t size, uintptr_t addr)
{
	if (size > PLAT_LAN966X_NS_IMAGE_SIZE)
		return false;
	if (addr < PLAT_LAN966X_NS_IMAGE_BASE)
		return false;
	if (addr + size > PLAT_LAN966X_NS_IMAGE_LIMIT)
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
	ERROR("%s: unhandled SMC (0x%x)\n", __func__, smc_fid);
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
