/** Asymmetric cryptographic acceleration core interface
 *
 * @file
 */
/*
 * Copyright (c) 2018-2021 Silex Insight sa
 * Copyright (c) 2014-2021 Beerten Engineering scs
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef CORE_HEADER_FILE
#define CORE_HEADER_FILE

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SX_PK_OP_DEFAULT_ENDIANNESS
/** Default operand endianness
 *
 * 0: little endian
 * 1: big endian
 */
#define SX_PK_OP_DEFAULT_ENDIANNESS 1
#endif

struct sx_pk_cmd_def;

/**
 * @addtogroup SX_PK_CORE
 *
 * @{
 */


/** Acceleration request
 *
 * A public key operation for offload on a hardware accelerator
 *
*/
typedef struct sx_pk_req sx_pk_req;


/** SilexPK and hardware constraints */
struct sx_pk_constraints {
   /** Maximum simultaneous requests any application can configure. */
    int maxpending;
};


/** Return SilexPK constraints
 *
 * If maxpending in sx_pk_constraints is 0, than SilexPK cannot be used
 * with the given environment.
 *
 * @param[in] index Index should always be 0
 * @param[in] usermemsz User memory size provided in bytes. Should be non zero for
 * BA414 AHBMaster and BA414 baremetal with no static memory
 * @return Constraints of SilexPK
 */
struct sx_pk_constraints sx_pk_list_constraints(int index, size_t usermemsz);


/** Description of the (hardware) accelerator capabilities and features.*/
struct sx_pk_capabilities {
   /** Maximum pending requests at any time. */
   int maxpending;
   /** Maximum operand size for prime field operands. 0 when not supported. */
   int max_gfp_opsz;
   /** Maximum operand size for elliptic curve operands. 0 when not supported. */
   int max_ecc_opsz;
   /** Maximum operand size for binary field operands. 0 when not supported. */
   int max_gfb_opsz;
   /** Operand size for IK operands. 0 when not supported. */
   int ik_opsz;
   /** Hardware support for blinding countermeasures. */
   int blinding;
   /** The size of the blinding factor in bytes. */
   int blinding_sz;
};


/** Opaque structure for offload with SilexPK library */
struct sx_pk_cnx;


/** Return the crypto acceleration capabilities and features
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return NULL on failure
 * @return Capabilities of the accelerator on success
 */
const struct sx_pk_capabilities *sx_pk_fetch_capabilities(struct sx_pk_cnx *cnx);


/** Library configuration for sx_pk_open() */
struct sx_pk_config {
    /** Maximum simultaneous requests the application will start.
     *
     * Set to 0 to use library default.
     */
    int maxpending;

    /** Device index to use.
     *
     * For special use cases with multiple independent hardware
     * accelerators. In case of doubt, leave to 0.
     */
    int devidx;

    /** User memory */
    long long *usrmem;

    /** Size of user provided memory */
    size_t usrmemsz;
};


/** Open SilexPK and related hardware and allocate internal resources
 *
 * @param[in] cfg Configuration to customize SilexPK to
 * application specific needs.
 * @return NULL on failure
 * @return Connection structure for future SilexPK calls on success.
 * Can be used with SilexPK functions to run operations.
 *
 * @see sx_pk_close()
 */
struct sx_pk_cnx *sx_pk_open(struct sx_pk_config *cfg);


/** Finish using any public key acceleration.
 *
 * No other hardware acceleration function can be called after this.
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 */
void sx_pk_close(struct sx_pk_cnx *cnx);


/** Encapsulated acquired acceleration request
 *
 * This structure is used by sx_pk_acquire_req()
 * and *_go() functions as a return value
 */
struct sx_pk_acq_req
{
   /** The acquired acceleration request **/
   sx_pk_req *req;
   /** The status of sx_pk_acquire_req() **/
   int status;
};


/** Get a SilexPK request instance locked to perform the given operation
 *
 * The returned sx_pk_acq_req structure contains a status and a pointer to
 * a reserved hardware accelerator instance. The pointer is valid and
 * usable only if status is ::SX_OK. Otherwise it's NULL.
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @param[in] cmd The command definition (for example ::SX_PK_CMD_MOD_EXP)
 * @return The acquired acceleration request for this operation
 *
 * @see sx_pk_release_req()
 */
struct sx_pk_acq_req sx_pk_acquire_req(struct sx_pk_cnx *cnx,
   const struct sx_pk_cmd_def *cmd);


struct sx_pk_ecurve;

/** Least significant bit of Ax is 1 (flag A) */
#define PK_OP_FLAGS_EDDSA_AX_LSB (1 << 29)

/** Least significant bit of Rx is 1 (flag B) */
#define PK_OP_FLAGS_EDDSA_RX_LSB (1 << 30)


/** Return the instance number of the hardware accelerator for this request
 *
 * @param[in] req The acceleration request obtained
 * through sx_pk_acquire_req()
 * @return instance number of the hardware accelerator for this request
 */
unsigned int sx_pk_get_req_id(sx_pk_req *req);


/** Add the context pointer to an acceleration request
 *
 * The attached pointer can be retrieved by sx_pk_get_user_context().
 * After sx_pk_release_req(), the context pointer is not available
 * anymore.
 *
 * The context can represent any content the user may associate with the PK request.
 *
 * @param[inout] req The acquired acceleration request
 * @param[in] context Pointer to context
 *
 * @see sx_pk_get_user_context()
 */
void sx_pk_set_user_context(sx_pk_req *req, void *context);


/** Get the context pointer from an acceleration request
 *
 * @param[in] req The acquired acceleration request
 * @return Context pointer from an acceleration request
 *
 * @see sx_pk_set_user_context()
*/
void *sx_pk_get_user_context(sx_pk_req *req);


/** Return the global operands size detected for the request
 *
 * @param[in] req The acquired acceleration request
 * @return Operand size for the request
 */
int sx_pk_get_opsize(sx_pk_req *req);


/** Operand slot structure */
struct sx_pk_slot
{
   /** Memory address of the operand slot **/
   char *addr;
};

/** Pair of slots
 *
 * Used to store large values
 */
struct sx_pk_dblslot
{
   /** First slot **/
   struct sx_pk_slot a;
   /** Second slot **/
   struct sx_pk_slot b;
};

/** List slots for input operands for an ECC operation
 *
 * Once the function completes with ::SX_OK,
 * write each operand to the address
 * found in the corresponding slot.
 *
 * That applies to all ECC operations.
 *
 * @param[inout] req The acceleration request obtained
 * through sx_pk_acquire_req()
 * @param[in] curve The curve used for that ECC operation
 * @param[in] flags Operation specific flags
 * @param[out] inputs List of input slots that will be filled in.
 * See inputslots.h for predefined lists of input slots per operation.
 *
 * @return ::SX_OK
 * @return ::SX_ERR_OPERAND_TOO_LARGE
 * @return ::SX_ERR_PK_RETRY
 * @return ::SX_ERR_BUSY
 */
int sx_pk_list_ecc_inslots(sx_pk_req *req,
    const struct sx_pk_ecurve *curve, int flags, struct sx_pk_slot *inputs);

/** List slots for input operands for a GF(p) modular operation.
 *
 * Once the function completes with ::SX_OK,
 * write each operands to the address
 * found in the corresponding slot.
 *
 * That applies to all operations except ECC.
 *
 * @param[inout] req The acceleration request obtained
 * through sx_pk_acquire_req()
 * @param[in] opsizes List of operand sizes in bytes
 * @param[out] inputs List of input slots that will be filled in.
 * See inputslots.h for predefined lists of input slots per operation.
 *
 * @return ::SX_OK
 * @return ::SX_ERR_OPERAND_TOO_LARGE
 * @return ::SX_ERR_PK_RETRY
 * @return ::SX_ERR_BUSY
 */
int sx_pk_list_gfp_inslots(sx_pk_req *req, const int *opsizes,
   struct sx_pk_slot *inputs);

/** Run the operation request in hardware.
 *
 * @pre To be called after all operands have been written in the slots
 * obtainded with sx_pk_list_ecc_inslots() or sx_pk_list_gfp_inslots().
 * After that the acceleration request is pending.
 *
 * @post The caller should wait until the operation finishes with
 * sx_pk_wait() or do polling by using sx_pk_has_finished().
 *
 * @param[in] req The acceleration request obtained
 * through sx_pk_acquire_req()
 */
void sx_pk_run(sx_pk_req *req);


/** Check if the current operation is still ongoing or finished
 *
 * Read the status of a request. When the accelerator is still
 * busy, return ::SX_ERR_BUSY. When the accelerator
 * finished, return ::SX_OK. May only be called after sx_pk_run().
 * May not be called after sx_pk_has_finished() or other functions
 * like sx_pk_wait() reported that the operation finished.
 *
 * @param[in] req The acceleration request obtained
 * through sx_pk_acquire_req()
 * @return Any \ref SX_PK_STATUS "status code"
 */
int sx_pk_get_status(sx_pk_req *req);

/** Legacy name of sx_pk_get_status() **/
#define sx_pk_has_finished(req) sx_pk_get_status(req)


/** Wait until the current operation finishes.
 *
 * After the operation finishes, return the operation status code.
 *
 * @param[in] req The acceleration request obtained
 * through sx_pk_acquire_req()
 * @return Any \ref SX_PK_STATUS "status code"
 */
int sx_pk_wait(sx_pk_req *req);


/** Return the public key acceleration request which finished its operation.
 *
 * If no public key accelerator finished or none has an operation, return NULL.
 * The results from the returned sx_pk_req should be used as soon as
 * possible and the instances given back with sx_pk_release_req().
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return NULL when no public key accelerator finished or none has an operation
 * @return The acceleration request which finished its operation
 */
sx_pk_req *sx_pk_pop_finished_req(struct sx_pk_cnx *cnx);


/** Return a platform notification that notifies when a request completed.
 *
 * The platform notification id is:
 * - -1 if not available
 * - a fd on POSIX systems
 * - an interrupt line number
 *
 * When a request completes the notification will activate.
 * The fd will become readable and the interrupt line will
 * generate an interrupt. The returned platform notification can
 * be used once. After the platform notification or after
 * sx_pk_pop_finished_req(), the notification id shall not
 * be used anymore.
 *
 * May not be used in combination with sx_pk_wait().
 *
 * @param[inout] cnx Connection structure obtained through sx_pk_open() at startup
 * @return platform notification id
 */
int sx_pk_get_global_notification_id(struct sx_pk_cnx *cnx);

/** Legacy name of sx_pk_get_global_notification_id() **/
#define sx_pk_completion_fd(cnx) sx_pk_get_global_notification_id(cnx)


/** Return a platform notification id of completion of this request
 *
 * The platform notification id is:
 * - -1 if not available
 * - a fd on POSIX systems
 * - an interrupt line number
 *
 * When the request completes the notification will activate.
 * The fd will become readable and the interrupt line will
 * generate an interrupt. After it was used or after
 * sx_pk_pop_finished_req(), the fd or interrupt line
 * shall not be used anymore.
 *
 * May not be used in combination with sx_pk_wait().
 *
 * @param[inout] req The acceleration request obtained
 * through sx_pk_acquire_req()
 * @return platform notification id
 */
int sx_pk_get_req_completion_id(sx_pk_req *req);


/** Fetch array of addresses to output operands
 *
 * @param[inout] req The acceleration request obtained
 * through sx_pk_acquire_req()
 * @return Array of addresses to output operands
*/
const char **sx_pk_get_output_ops(sx_pk_req *req);

/** Give back the public key acceleration request.
 *
 * Release the reserved resources
 *
 * @pre sx_pk_acquire_req() should have been called
 * before this function is called and not
 * being in use by the hardware
 *
 * @param[inout] req The acceleration request obtained
 * through sx_pk_acquire_req()
 * operation has finished
 */
void sx_pk_release_req(sx_pk_req *req);


/** Elliptic curve configuration and parameters.
 *
 * To be used only via the functions in ec_curves.h The internal members of the
 * structure should not be accessed directly.
 */
struct sx_pk_ecurve {
    uint32_t curveflags;
    int sz;
    const char *params;
    int offset;
    struct sx_pk_cnx *cnx;
};

/** @} */

#ifdef __cplusplus
}
#endif

#endif
