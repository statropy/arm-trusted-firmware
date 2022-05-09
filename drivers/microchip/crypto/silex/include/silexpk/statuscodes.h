/** SilexPK status codes
 *
 * @file
 */
/*
 * Copyright (c) 2018-2020 Silex Insight sa
 * Copyright (c) 2018-2020 Beerten Engineering scs
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef SX_PK_STATUSCODES_HEADER_FILE
#define SX_PK_STATUSCODES_HEADER_FILE

/**
 * @addtogroup SX_PK_STATUS
 *
 * @{
 */

/** The function or operation succeeded */
#define SX_OK  0

/** The function or operation was given an invalid parameter */
#define SX_ERR_INVALID_PARAM 1

/** Unknown error */
#define SX_ERR_UNKNOWN_ERROR 2

/** The operation is still executing */
#define SX_ERR_BUSY 3

/** The input operand is not a quadratic residue */
#define SX_ERR_NOT_QUADRATIC_RESIDUE 4

/** The input value for Rabin-Miller test is a composite value */
#define SX_ERR_COMPOSITE_VALUE 5

/** Inversion of non-invertible value */
#define SX_ERR_NOT_INVERTIBLE 6

/** The signature is not valid
 *
 * This error can happen during signature generation
 * and signature verification
 */
#define SX_ERR_INVALID_SIGNATURE 7

/** The functionality or operation is not supported */
#define SX_ERR_NOT_IMPLEMENTED 8

/** An unexpected micro-code instruction (in the PK engine)
 *
 * On hardware older than december 2018, this error means a
 * ::SX_ERR_POINT_AT_INFINITY error
*/
#define SX_ERR_INVALID_MICROCODE 9

/** The output operand is a point at infinity */
#define SX_ERR_POINT_AT_INFINITY (SX_ERR_INVALID_MICROCODE)

/** The input value is outside the expected range */
#define SX_ERR_OUT_OF_RANGE 10

/** The modulus has an unexpected value
 *
 * This error happens when the modulus is zero or
 * even when odd modulus is expected
 */
#define SX_ERR_INVALID_MODULUS 11

/** The input point is not on the defined elliptic curve */
#define SX_ERR_POINT_NOT_ON_CURVE 12

/** The input operand is too large
 *
 * Happens when an operand is bigger than one of the following:
 * - The hardware operand size: Check ::sx_pk_capabilities
 * to see the hardware operand size limit.
 * - The operation size: Check sx_pk_get_opsize() to get
 * the operation size limit.
 */
#define SX_ERR_OPERAND_TOO_LARGE 13

/** A platform specific error */
#define SX_ERR_PLATFORM_ERROR 14

/** The evaluation period for the product expired */
#define SX_ERR_EXPIRED 15

/** The hardware is still in IK mode
 *
 * This error happens when a normal operation
 * is started and the hardware is still in IK mode.
 * Run command ::SX_PK_CMD_IK_EXIT to exit the IK
 * mode and to run normal operations again
*/
#define SX_ERR_IK_MODE 16

/** The parameters of the elliptic curve are not valid. */
#define SX_ERR_INVALID_CURVE_PARAM 17

/** The IK module is not ready
 *
 * This error happens when a IK operation is started
 * but the IK module has not yet generated the internal keys.
 * Call sx_pk_ik_derive_keys() before starting IK operations.
*/
#define SX_ERR_IK_NOT_READY 18

/** Resources not available for a new operation. Retry later */
#define SX_ERR_PK_RETRY 19

/** Low-order point during point multiplication
 *  or check on point order failed
 */
#define SX_ERR_BAD_ORDER 20

/** Return a brief text string describing the given status code.
 *
 * @param[in] code Value of status code
 * @return Text string describing the status code
*/
const char *sx_describe_statuscode(int code);

/** @} */

#endif
