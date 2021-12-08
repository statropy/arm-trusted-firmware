/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <drivers/auth/mbedtls/mbedtls_config.h>
#include <plat/common/platform.h>
#include <platform_def.h>

#include "lan966x_private.h"

/*
 * This function is the implementation of the shared Mbed TLS heap between
 * BL1 and BL2 for Arm platforms. The shared heap address is passed from BL1
 * to BL2 with a structure pointer. This pointer is passsed in arg2.
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
