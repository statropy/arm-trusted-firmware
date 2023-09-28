/** Memory operation functions
 *
 * @file
 */
/*
 * Copyright (c) 2018-2021 Silex Insight sa
 * Copyright (c) 2018-2021 Beerten Engineering scs
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IOMEM_HEADER_FILE
#define IOMEM_HEADER_FILE

/** Clear device memory
 *
 * @param[in] dst Memory to clear.
 * Will be zeroed after this call
 * @param[in] sz Number of bytes to clear
*/
void sx_clrpkmem(void *dst, size_t sz);

/** Write src to device memory at dst.
 *
 * The write to device memory will always use write instructions at naturally
 * aligned addresses.
 *
 * @param[out] dst Destination of write operation.
 * Will be modified after this call
 * @param[in] src Source of write operation
 * @param[in] sz The number of bytes to write from src to dst
 */
void sx_wrpkmem(void *dst, const void *src, size_t sz);

/** Read from device memory at src into normal memory at dst.
 *
 * The read from device memory will always use read instructions at naturally
 * aligned addresses.
 *
 * @param[out] dst Destination of read operation.
 * Will be modified after this call
 * @param[in] src Source of read operation
 * @param[in] sz The number of bytes to read from src to dst
 */
void sx_rdpkmem(void *dst, const void *src, size_t sz);

#endif
