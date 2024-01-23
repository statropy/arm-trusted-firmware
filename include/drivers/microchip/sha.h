/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MICROCHIP_SHA
#define MICROCHIP_SHA

#include <stddef.h>

typedef enum {
	SHA_MR_ALGO_SHA1   = 0,
	SHA_MR_ALGO_SHA256 = 1,
	SHA_MR_ALGO_SHA384 = 2,
	SHA_MR_ALGO_SHA512 = 3,
	SHA_MR_ALGO_SHA224 = 4,
} lan966x_sha_type_t;

void sha_init(void);
int sha_calc(lan966x_sha_type_t hash_type, const void *input, size_t len, void *hash, size_t hash_len);
int sha_verify(lan966x_sha_type_t hash_type, const void *input, size_t len, const void *hash, size_t hash_len);

void *sha_calc_init(lan966x_sha_type_t hash_type, size_t data_len, size_t hash_len);
void sha_update(void *state, const void *input, size_t len);
int sha_calc_finish(void *state, void *hash);

#endif  /* MICROCHIP_SHA */
