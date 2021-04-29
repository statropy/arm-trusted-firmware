/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>
#include <drivers/auth/crypto_mod.h>
#include <endian.h>
#include <lib/mmio.h>
#include <mbedtls/md.h>
#include <aes.h>

#include "lan966x_regs.h"

#define AES_KEYLEN_128	0
#define AES_KEYLEN_192	1
#define AES_KEYLEN_256	2

#define AES_OPMOD_ECB	0	/* Electronic Codebook mode */
#define AES_OPMOD_CBC	1	/* Cipher Block Chaining mode */
#define AES_OPMOD_OFB	2	/* Output Feedback mode */
#define AES_OPMOD_CFB	3	/* Cipher Feedback mode */
#define AES_OPMOD_CTR	4	/* Counter mode (16-bit internal counter) */
#define AES_OPMOD_GCM	5	/* Galois/Counter mode */
#define AES_OPMOD_XTS	6	/* XEX-based tweaked-codebook mode */

#define AES_BLOCK_LEN	(4 * 4)	/* 4 32-bits unit */

#define AES_AES_KEYWR(b, n)	(AES_AES_KEYWR0(b) + (n * 4))
#define AES_AES_IVR(b, n)	(AES_AES_IVR0(b) + (n * 4))
#define AES_AES_IDATAR(b, n)	(AES_AES_IDATAR0(b) + (n * 4))
#define AES_AES_ODATAR(b, n)	(AES_AES_ODATAR0(b) + (n * 4))
#define AES_AES_TAGR(b, n)	(AES_AES_TAGR0(b) + (n * 4))

static const uint32_t base = LAN966X_AES_BASE;

static uint32_t unaligned_get32(const void *p)
{
        const uint8_t *bp = (const uint8_t *)p;

        return ((uint32_t)bp[3] << 24)
                | ((uint32_t)bp[2] << 16)
                | ((uint32_t)bp[1] << 8)
                | bp[0];
}

static void unaligned_put32(void *p, uint32_t w)
{
        uint8_t *bp = (uint8_t *)p;

	bp[0] = w;
	bp[1] = w >> 8;
	bp[2] = w >> 16;
	bp[3] = w >> 24;
}

#define MAX_WAIT_LOOP 10000
static uint32_t aes_wait_flag(uint32_t mask)
{
	int tmo;
	uint32_t s;

	for (tmo = MAX_WAIT_LOOP; tmo > 0; tmo--) {
		s = mmio_read_32(AES_AES_ISR(base));
		if (mask & s)
			break;
		/* Hummmm */
		//udelay(1);
	}

	assert(tmo > 0);

	INFO("Wait(%d): %08x\n", tmo, s);
	return s;
}

void aes_init(void)
{
	mmio_write_32(AES_AES_CR(base), AES_AES_CR_SWRST(1));
}

static int aes_setkey(const unsigned char *key, unsigned int keylen,
		      const unsigned char *iv)
{
	int kc, kw, i;

	switch (keylen) {
	case 16:
		kc = AES_KEYLEN_128;
		break;
	case 24:
		kc = AES_KEYLEN_192;
		break;
	case 32:
		kc = AES_KEYLEN_256;
		break;
	default:
		return CRYPTO_ERR_DECRYPTION;
	}

	mmio_write_32(AES_AES_MR(base),
		      AES_AES_MR_KEYSIZE(kc) |
		      AES_AES_MR_OPMOD(AES_OPMOD_GCM) |
		      AES_AES_MR_GTAGEN(1));

	/* kw in 32-bit units */
	kw = keylen / 4;
	for (i = 0; i < kw; i++)
		mmio_write_32(AES_AES_KEYWR(base, i), unaligned_get32(key + (i * 4)));

	/* Wait for Hash subkey */
	(void) aes_wait_flag(AES_AES_ISR_DATRDY_ISR(1));

	/* IVR */
	for (i = 0; i < 4; i++)
		mmio_write_32(AES_AES_IVR(base, i), unaligned_get32(iv + (i * 4)));

	return CRYPTO_SUCCESS;
}

static void aes_decrypt(uint8_t *data, size_t len)
{
	size_t nb;
	int nw, i;

	while (len > 0) {
		nb = MIN((size_t)AES_BLOCK_LEN, len);
		nw = div_round_up(nb, 4); /* To word count */
		/* Write input */
		for (i = 0; i < nw; i++)
			mmio_write_32(AES_AES_IDATAR(base, i),
				      unaligned_get32(data +  + (i * 4)));
		/* Start processing */
		mmio_setbits_32(AES_AES_CR(base), AES_AES_CR_START(1));
		/* Wait until processed */
		(void) aes_wait_flag(AES_AES_ISR_DATRDY_ISR(1));
		/* Get output data */
		for (i = 0; i < nw; i++) {
			uint32_t w = mmio_read_32(AES_AES_ODATAR(base, i));
			unaligned_put32(data + (i * 4), w);
		}

		/* Next block */
		data += nb;
		len -= nb;
	}
}

static int aes_check_tag(const uint8_t *tag, size_t tag_len)
{
	int i, diff, rc;
	uint8_t tag_buf[AES_BLOCK_LEN];

	(void) aes_wait_flag(AES_AES_ISR_TAGRDY_ISR(1));
	for (i = 0; i < 4; i++) {
		uint32_t w = mmio_read_32(AES_AES_TAGR(base, i));
		unaligned_put32(tag_buf + (i * 4), w);
	}

	/* Check tag in "constant-time" */
	for (diff = 0, i = 0; i < tag_len; i++)
		diff |= ((const unsigned char *)tag)[i] ^ tag_buf[i];

	rc = (diff != 0) ? CRYPTO_ERR_DECRYPTION : CRYPTO_SUCCESS;

	return rc;
}

int aes_gcm_decrypt(void *data_ptr, size_t len, const void *key,
		    unsigned int key_len, const void *iv,
		    unsigned int iv_len, const void *tag,
		    unsigned int tag_len)
{
	int rc;

	/* For GCM, iv is 128 bits/16 bytes */
	if (iv_len != AES_BLOCK_LEN) {
		rc = CRYPTO_ERR_DECRYPTION;
		goto exit_gcm;
	}

	rc = aes_setkey(key, key_len, iv);
	if (rc != 0) {
		rc = CRYPTO_ERR_DECRYPTION;
		goto exit_gcm;
	}

	/* Set AADLEN, CLEN */
	mmio_write_32(AES_AES_AADLENR(base), len);
	mmio_write_32(AES_AES_CLENR(base), len);

	/* Now run data through decrypt */
	aes_decrypt(data_ptr, len);

	/* Check GTAG */
	rc = aes_check_tag(tag, tag_len);

exit_gcm:
	return rc;
}
