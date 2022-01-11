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
#define AES_IV_LEN	(3 * 4)	/* 3 32-bits unit */

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
		if (s & AES_AES_IER_SECE(1)) {
			s = mmio_read_32(AES_AES_WPSR(base));
			WARN("WPSR: %08x\n", s);
		}
	}

	assert(tmo > 0);

	return s;
}

void aes_init(void)
{
	mmio_write_32(AES_AES_CR(base), AES_AES_CR_SWRST(1));
}

static int aes_setkey(bool cipher, const unsigned char *key,
		      unsigned int keylen, const unsigned char *iv)
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
		      AES_AES_MR_SMOD(0) | /* Manual start */
		      AES_AES_MR_CIPHER(cipher) | /* Decrypt = 0 / Encrypt = 1 */
		      AES_AES_MR_KEYSIZE(kc) |
		      AES_AES_MR_OPMOD(AES_OPMOD_GCM) |
		      AES_AES_MR_GTAGEN(1));

	/* kw in 32-bit units */
	kw = keylen / 4;
	for (i = 0; i < kw; i++)
		mmio_write_32(AES_AES_KEYWR(base, i), unaligned_get32(key + (i * 4)));

	/* Wait for Hash subkey */
	(void) aes_wait_flag(AES_AES_ISR_DATRDY_ISR(1));

	/* IVR - 12 bytes + (initial) counter */
	for (i = 0; i < (AES_IV_LEN / 4); i++)
		mmio_write_32(AES_AES_IVR(base, i), unaligned_get32(iv + (i * 4)));
	mmio_write_32(AES_AES_IVR(base, 3), __htonl(2)); /* Inc32(j0) */

	return CRYPTO_SUCCESS;
}

static void aes_process(uint8_t *data, size_t len)
{
	size_t nb;
	int nw, i;

	while (len > 0) {
		nb = MIN((size_t)AES_BLOCK_LEN, len);
		nw = div_round_up(nb, 4); /* To word count */
		/* Write input */
		VERBOSE("AES: len %d, bytes %d, words %d\n", len, nb, nw);
		for (i = 0; i < nw; i++)
			mmio_write_32(AES_AES_IDATAR(base, i),
				      unaligned_get32(data +  + (i * 4)));
		/* Start processing */
		mmio_write_32(AES_AES_CR(base), AES_AES_CR_START(1));
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

static int aes_get_tag(uint8_t *tag, size_t tag_len)
{
	int i;

	if (tag_len != AES_BLOCK_LEN)
		return CRYPTO_ERR_DECRYPTION;

	VERBOSE("AES: get tag - %d bytes\n", tag_len);
	(void) aes_wait_flag(AES_AES_ISR_TAGRDY_ISR(1));
	for (i = 0; i < 4; i++) {
		uint32_t w = mmio_read_32(AES_AES_TAGR(base, i));

		unaligned_put32(tag + (i * 4), w);
	}

	return 0;
}

static int aes_check_tag(const uint8_t *tag, size_t tag_len)
{
	int i, diff, rc;
	uint8_t tag_buf[AES_BLOCK_LEN];

	(void) aes_get_tag(tag_buf, sizeof(tag_buf));

	/* Check tag in "constant-time" */
	for (diff = 0, i = 0; i < tag_len; i++) {
		VERBOSE("TAG[%d]: %02x vs %02x\n", i, tag[i], tag_buf[i]);
		diff |= tag[i] ^ tag_buf[i];
	}

	rc = (diff != 0) ? CRYPTO_ERR_DECRYPTION : CRYPTO_SUCCESS;

	return rc;
}

int aes_setup(bool encrypt, size_t len,
	      const void *key, unsigned int key_len,
	      const void *iv, unsigned int iv_len)
{
	int rc;

	/* NIST recommendation: 96 bits/12 bytes */
	if (iv_len != AES_IV_LEN) {
		return CRYPTO_ERR_DECRYPTION;
	}

	/* Reset state */
	mmio_write_32(AES_AES_CR(base), AES_AES_CR_SWRST(1));

	rc = aes_setkey(encrypt, key, key_len, iv);
	if (rc != 0)
		return CRYPTO_ERR_DECRYPTION;

	/* Set AADLEN, CLEN */
	mmio_write_32(AES_AES_AADLENR(base), 0);
	mmio_write_32(AES_AES_CLENR(base), len);

	return 0;
}

int aes_gcm_decrypt(void *data_ptr, size_t len, const void *key,
		    unsigned int key_len, const void *iv,
		    unsigned int iv_len, const void *tag,
		    unsigned int tag_len)
{
	int rc;

	VERBOSE("aes-gcm-decrypt: data_len %d, key_len %d, iv_len %d, tag_len %d\n",
		len, key_len, iv_len, tag_len);

	rc = aes_setup(false, len, key, key_len, iv, iv_len);
	if (rc)
		return rc;

	/* Now run data through decrypt */
	aes_process(data_ptr, len);

	/* Check GTAG */
	rc = aes_check_tag(tag, tag_len);

	return rc;
}

int aes_gcm_encrypt(void *data_ptr, size_t len,
		    const void *key, unsigned int key_len,
		    const void *iv, unsigned int iv_len,
		    void *tag, unsigned int tag_len)
{
	int rc;

	VERBOSE("aes-gcm-encrypt: data_len %d, key_len %d, iv_len %d, tag_len %d\n",
		len, key_len, iv_len, tag_len);

	rc = aes_setup(true, len, key, key_len, iv, iv_len);
	if (rc)
		return rc;

	/* Now run data through encrypt */
	aes_process(data_ptr, len);

	/* Get auth tag as well */
	rc = aes_get_tag(tag, tag_len);

	return rc;
}
