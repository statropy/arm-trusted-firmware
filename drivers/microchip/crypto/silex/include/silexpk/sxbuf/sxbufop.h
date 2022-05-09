/** Basic "sxops" operand definition
 *
 * @file
 *
 * Copyright (c) 2018-2021 Silex Insight sa
 * Copyright (c) 2018-2021 Beerten Engineering scs
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ADAPTER_TYPES_HEADER
#define ADAPTER_TYPES_HEADER

#include <stddef.h>
#include <stdint.h>

/** Basic operand representation **/
struct sx_buf {
     /** Size in bytes of operand **/
     size_t sz;
     /** Memory of operand bytes in big endian **/
     char *bytes;
};
/** Simple "sxops" implementation based on sx_buf**/
typedef struct sx_buf sx_op;

/** Simple Elliptic Curve operand implementation based on sx_buf**/
typedef struct sx_buf sx_ecop;

/** Affine point for ECC **/
typedef struct sx_pk_affine_point {
     /** x-coordinate of affine point */
     sx_ecop *x;
     /** y-coordinate of affine point */
     sx_ecop *y;
} sx_pk_affine_point;

#endif
