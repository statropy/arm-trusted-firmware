/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MICROCHIP_AES
#define MICROCHIP_AES

void aes_init(void);

int aes_gcm_decrypt(void *data_ptr, size_t len, const void *key,
		    unsigned int key_len, const void *iv,
		    unsigned int iv_len, const void *tag,
		    unsigned int tag_len);

int aes_gcm_encrypt(void *data_ptr, size_t len,
		    const void *key, unsigned int key_len,
		    const void *iv, unsigned int iv_len,
		    void *tag, unsigned int tag_len);

#endif  /* MICROCHIP_AES */
