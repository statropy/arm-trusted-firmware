/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <drivers/auth/mbedtls/mbedtls_config.h>
#include <lib/mmio.h>
#include <plat/common/platform.h>
#include <platform_def.h>
#include <tools_share/firmware_encrypted.h>

#include "plat_otp.h"
#include "lan966x_private.h"

static const uint8_t lan966x_rotpk_header[] = {
	/* DER header */
	0x30, 0x31, 0x30, 0x0D, 0x06, 0x09, 0x60, 0x86, 0x48,
	0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20
};

#define LAN966X_ROTPK_HASH_LEN	(OTP_TBBR_ROTPK_SIZE)
#define LAN966X_ROTPK_HEADER	sizeof(lan966x_rotpk_header)

static uint8_t rotpk_hash_der[LAN966X_ROTPK_HEADER + LAN966X_ROTPK_HASH_LEN];

int plat_get_rotpk_info(void *cookie, void **key_ptr, unsigned int *key_len,
			unsigned int *flags)
{
	uint8_t *rotpk = &rotpk_hash_der[LAN966X_ROTPK_HEADER];
	int ret;

	memcpy(rotpk_hash_der, lan966x_rotpk_header, sizeof(lan966x_rotpk_header));

	ret = otp_read_otp_tbbr_rotpk(rotpk, LAN966X_ROTPK_HASH_LEN);

	if (ret < 0 || otp_all_zero(rotpk, LAN966X_ROTPK_HASH_LEN)) {
		*flags = ROTPK_NOT_DEPLOYED;
	} else {
		*key_ptr = (void *)rotpk_hash_der;
		*key_len = sizeof(rotpk_hash_der);
		*flags = ROTPK_IS_HASH;
	}

	return 0;
}

int plat_get_enc_key_info(enum fw_enc_status_t fw_enc_status, uint8_t *key,
                          size_t *key_len, unsigned int *flags,
                          const uint8_t *img_id, size_t img_id_len)
{
	int ret;
	size_t otp_keylen = OTP_TBBR_SSK_SIZE;

	assert(*key_len >= otp_keylen);
	assert(fw_enc_status == FW_ENC_WITH_SSK);

	otp_keylen = MIN(otp_keylen, *key_len);
	ret = otp_read_otp_tbbr_ssk(key, otp_keylen);
	if (ret < 0) {
		return ret;
	} else {
		*key_len = otp_keylen;
		*flags = 0;
	}

        return 0;
}

int plat_get_nv_ctr(void *cookie, unsigned int *nv_ctr)
{
	*nv_ctr = 0;

	return 0;
}

int plat_set_nv_ctr(void *cookie, unsigned int nv_ctr)
{
	return 1;
}

/*
 * This function is the implementation of the shared Mbed TLS heap between
 * BL1 and BL2 for Arm platforms. The shared heap address is passed from BL1
 * to BL2 with a pointer. This pointer is passsed in arg3.
 */
int plat_get_mbedtls_heap(void **heap_addr, size_t *heap_size)
{
	assert(heap_addr != NULL);
	assert(heap_size != NULL);

#if defined(IMAGE_BL1) || BL2_AT_EL3

	/* If in BL1 or BL2_AT_EL3 define a heap */
	static unsigned char heap[TF_MBEDTLS_HEAP_SIZE];

	*heap_addr = heap;
	*heap_size = sizeof(heap);
	shared_memory_desc.mbedtls_heap_addr = heap;
	shared_memory_desc.mbedtls_heap_size = sizeof(heap);

#elif defined(IMAGE_BL2)

	/* If in BL2, retrieve the already allocated heap's info from descriptor */
	*heap_addr = shared_memory_desc.mbedtls_heap_addr;
	*heap_size = shared_memory_desc.mbedtls_heap_size;

#endif

	return 0;
}

void lan966x_mbed_heap_set(shared_memory_desc_t *d)
{
	shared_memory_desc.mbedtls_heap_addr = d->mbedtls_heap_addr;
	shared_memory_desc.mbedtls_heap_size = d->mbedtls_heap_size;
}
