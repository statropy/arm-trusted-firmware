/** Predefined and custom elliptic curve definitions
 *
 * @file
 */
/*
 * Copyright (c) 2018-2021 Silex Insight sa
 * Copyright (c) 2018-2021 Beerten Engineering scs
 * SPDX-License-Identifier: BSD-3-Clause
 */



#ifndef EC_CURVES_HEADER_FILE
#define EC_CURVES_HEADER_FILE

#include <stdint.h>
#include <silexpk/core.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @addtogroup SX_PK_CURVES
 *
 * @{
 */

/** Slots to write custom curve parameters into. */
struct sx_curve_slots {
    struct sx_pk_slot p; /**< Field size of curve **/
    struct sx_pk_slot n; /**< Order of curve **/
    struct sx_pk_slot gx; /**< x-coordinate of generator point of curve **/
    struct sx_pk_slot gy; /**< y-coordinate of generator point of curve **/
    struct sx_pk_slot a; /**< Curve parameter a **/
    struct sx_pk_slot b; /**< Curve parameter b **/
};

/** Create a prime short weierstrass elliptic curve
 *
 * @remark When this function returns, copy the paramters of the
 * curve into the slots returned in 'slots'. After that, you can
 * use the prime elliptic curve
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @param[in] curve Curve to initialise as a prime elliptic curve
 * @param[in] mem Memory for the parameters of the curve. The size of
 * the memory should be 6 time sz as there are 6 curve parameters of size sz
 * @param[in] sz Size of the curve in bytes
 * @param[out] slots Slots for the curve parameters
 *
 * @see sx_pk_create_shortwss_gf2m_curve() and sx_pk_destroy_ec_curve()
 */
void sx_pk_create_shortwss_gfp_curve(
        struct sx_pk_cnx *cnx,
        struct sx_pk_ecurve *curve,
        char *mem,
        int sz,
        struct sx_curve_slots *slots);

#define sx_pk_create_ecp_curve sx_pk_create_shortwss_gfp_curve

/** Create a binary short weierstrass elliptic curve
 *
 * @remark When this function returns, copy the paramters of the
 * curve into the slots returned in 'slots'. After that, you can
 * use the binary elliptic curve
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @param[in] curve Curve to initialise as a binary elliptic curve
 * @param[in] mem Memory for the parameters of the curve. The size of
 * the memory should be 6 time sz as there are 6 curve parameters of size sz
 * @param[in] sz Size of the curve in bytes
 * @param[out] slots Slots for the curve parameters
 *
 * @see sx_pk_create_shortwss_gfp_curve() and sx_pk_destroy_ec_curve()
 */
void sx_pk_create_shortwss_gf2m_curve(
        struct sx_pk_cnx *cnx,
        struct sx_pk_ecurve *curve,
        char *mem,
        int sz,
        struct sx_curve_slots *slots);

#define sx_pk_create_ecb_curve sx_pk_create_shortwss_gf2m_curve

/** Destroy a custom ecp or ecb curve
 *
 * Destroy a curve created previously by sx_pk_create_shortwss_gfp_curve()
 * or sx_pk_create_shortwss_gf2m_curve().
 *
 * @param[in] curve Initialised curve to destroy
 */
void sx_pk_destroy_ec_curve(struct sx_pk_ecurve *curve);


/** Get a reference to the predefined NIST P192 elliptic curve
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return Curve structure for P192 curve
 */
struct sx_pk_ecurve sx_pk_get_curve_nistp192(struct sx_pk_cnx *cnx);

/** Get a reference to the predefined NIST P224 elliptic curve
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return Curve structure for P224 curve
 */
struct sx_pk_ecurve sx_pk_get_curve_nistp224(struct sx_pk_cnx *cnx);

/** Get a reference to the predefined NIST P256 elliptic curve
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return Curve structure for P256 curve
 */
struct sx_pk_ecurve sx_pk_get_curve_nistp256(struct sx_pk_cnx *cnx);

/** Get a reference to the predefined NIST P384 elliptic curve
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return Curve structure for P384 curve
 */
struct sx_pk_ecurve sx_pk_get_curve_nistp384(struct sx_pk_cnx *cnx);

/** Get a reference to the predefined NIST P521 elliptic curve
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return Curve structure for P521 curve
 */
struct sx_pk_ecurve sx_pk_get_curve_nistp521(struct sx_pk_cnx *cnx);

/** Get a reference to the predefined ED25519 elliptic curve
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return Curve structure for ED25519 curve
 */
struct sx_pk_ecurve sx_pk_get_curve_ed25519(struct sx_pk_cnx *cnx);

/** Get a reference to the predefined ED448 elliptic curve
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return Curve structure for ED448 curve
 */
struct sx_pk_ecurve sx_pk_get_curve_ed448(struct sx_pk_cnx *cnx);

/** Get a reference to the predefined X25519 elliptic curve
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return Curve structure for X25519 curve
 */
struct sx_pk_ecurve sx_pk_get_curve_x25519(struct sx_pk_cnx *cnx);

/** Get a reference to the predefined X448 elliptic curve
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return Curve structure for X448 curve
 */
struct sx_pk_ecurve sx_pk_get_curve_x448(struct sx_pk_cnx *cnx);

/** Get a reference to the predefined SEC p256k1 elliptic curve
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return Curve structure for SEC p256k1 curve
 */
struct sx_pk_ecurve sx_pk_get_curve_secp256k1(struct sx_pk_cnx *cnx);

/** Get a reference to the predefined fp256 elliptic curve
 *
 * This curve is used in SM2 for testing purpose only.
 * For a practical SM2 curve, see sx_pk_get_curve_curvesm2()
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return Curve structure for fp256 curve (used in SM2)
 */
struct sx_pk_ecurve sx_pk_get_curve_fp256(struct sx_pk_cnx *cnx);

/** Get a reference to the predefined curveSM2 elliptic curve
 *
 * This is the recommended curve for SM2
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return Curve structure for curveSM2
 */
struct sx_pk_ecurve sx_pk_get_curve_curvesm2(struct sx_pk_cnx *cnx);

/** Get a reference to the predefined SM9 elliptic curve
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return Curve structure for the SM9 curve
 */
struct sx_pk_ecurve sx_pk_get_curve_sm9(struct sx_pk_cnx *cnx);

/** Write the generator point of the curve into the slots (internal)
 *
 * Write the parameter gx & gy from curve to px.addr & py.addr respectively
 *
 * @param[in] pk The accelerator request
 * @param[in] curve Initialised curve to get generator point from.
 * @param[in] px x-coordinate slot of generator point. The curve generator
 * (x-coordinate) will be written to this address
 * @param[in] py y-coordinate slot of generator point. The curve generator
 * (y-coordinate) will be written to this address
 */
void sx_pk_write_curve_gen(sx_pk_req *pk, const struct sx_pk_ecurve *curve,
                           struct sx_pk_slot px, struct sx_pk_slot py);

/** Return the operand size in bytes for the given curve
 *
 * @param[in] curve Initialised curve to get operand size from
 * @return Operand size in bytes for the given curve
*/
int sx_pk_curve_opsize(const struct sx_pk_ecurve *curve);

/** Return a pointer to the order of the base point of the curve
 *
 * This function works only for Weierstrass curves
 *
 * @param[in] curve Initialised curve
 * @return Pointer to the order of the base point of the curve
*/
const char *sx_pk_curve_order(const struct sx_pk_ecurve *curve);

/** Export the (x, y) coordinates of the base point of an elliptic curve
 *
 * This function works only for Weierstrass curves
 *
 * @param[in] curve Initialised curve
 * @param[out] basepoint Buffer where the coordinates of the base point are
 * written.
 *
 * The x and y coordinates are written consecutively in the basepoint output
 * buffer. The size of this buffer must be 2*sx_pk_curve_opsize().
 *
 * The coordinates values are exported in big endian format.
*/
void sx_pk_curve_read_base_point(const struct sx_pk_ecurve *curve,
                        char *basepoint);

/** @} */

#ifdef __cplusplus
}
#endif

#endif
