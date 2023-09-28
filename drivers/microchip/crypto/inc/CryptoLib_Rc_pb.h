//========================================================================
//
//        Copyright (c) Atmel Corporation 2012 All rigths reserved
//                     ATMEL CONFIDENTIAL PROPRIETARY
//
//========================================================================
//                             DISCLAIMER
//
//    Atmel reserves the right to make changes without further notice
//    to any product herein to improve reliability, function or design.
//    Atmel does not assume any liability arising out of the
//    application or use of any product, circuit, or software described
//    herein; neither does it convey any license under its patent rights
//    nor the rights of others. Atmel products are not designed,
//    intended or authorized for use as components in systems intended
//    for surgical implant into the body or other applications intended
//    to support life, or for any other application in which the failure
//    of the Atmel product could create a situation where personal
//    injury or death may occur. Should Buyer purchase or use Atmel
//    products for any such intended or unauthorized application, Buyer
//    shall indemnify and hold Atmel and its officers, employees,
//    subsidiaries, affiliates, and distributors harmless against all
//    claims, costs, damages and expenses, and reasonable attorney fees
//    arising out of, directly or indirectly, any claim of personal
//    injury or death associated with such unintended or unauthorized
//    use, even if such claim alleges that Atmel was negligent
//    regarding the design or manufacture of the part. Atmel and the
//    Atmel logo are registered trademarks of the Atmel corporation.
//========================================================================
//      ATMEL Crypto Library
//------------------------------------------------------------------------
//      File: CryptoLib_Rc_pb.h
//------------------------------------------------------------------------
//      Description: definition of the Return Codes
//------------------------------------------------------------------------
#ifndef _CRYPTOLIB_RC_PB_INCLUDED
#define _CRYPTOLIB_RC_PB_INCLUDED




// Standard Return and Severity Codes
#define CPKCL_SEVERE(a)                           (a | 0xC000)
#define CPKCL_WARNING(a)                          (a | 0x8000)
#define CPKCL_INFO(a)                             (a | 0x4000)
#define CPKCL_SEVERITY_MASK(a)                    (a | 0xC000)

// Generic Return Codes
#define CPKCL_OK                                   0x0000
#define CPKCL_COMPUTATION_NOT_STARTED              CPKCL_SEVERE(0x0001)
#define CPKCL_UNKNOWN_SERVICE                      CPKCL_SEVERE(0x0002)
#define CPKCL_UNEXPLOITABLE_OPTIONS                CPKCL_SEVERE(0x0003)
#define CPKCL_HARDWARE_ISSUE                       CPKCL_SEVERE(0x0004)
#define CPKCL_WRONG_HARDWARE                       CPKCL_SEVERE(0x0005)
#define CPKCL_LIBRARY_MALFORMED                    CPKCL_SEVERE(0x0006)
#define CPKCL_ERROR                                CPKCL_SEVERE(0x0007)
#define CPKCL_UNKNOWN_SUBSERVICE                   CPKCL_SEVERE(0x0008)

// Preliminary tests Return Codes (when not in release)
#define CPKCL_OVERLAP_NOT_ALLOWED                  CPKCL_SEVERE(0x0010)
#define CPKCL_PARAM_NOT_IN_CPKCCRAM                CPKCL_SEVERE(0x0011)
#define CPKCL_PARAM_NOT_IN_RAM                     CPKCL_SEVERE(0x0012)
#define CPKCL_PARAM_NOT_IN_CPURAM                  CPKCL_SEVERE(0x0013)
#define CPKCL_PARAM_WRONG_LENGTH                   CPKCL_SEVERE(0x0014)
#define CPKCL_PARAM_BAD_ALIGNEMENT                 CPKCL_SEVERE(0x0015)
#define CPKCL_PARAM_X_BIGGER_THAN_Y                CPKCL_SEVERE(0x0016)
#define CPKCL_PARAM_LENGTH_TOO_SMALL               CPKCL_SEVERE(0x0017)

// Run time errors (even when in release)
#define CPKCL_DIVISION_BY_ZERO                     CPKCL_SEVERE(0x0101)
#define CPKCL_MALFORMED_MODULUS                    CPKCL_SEVERE(0x0102)
#define CPKCL_FAULT_DETECTED                       CPKCL_SEVERE(0x0103)
#define CPKCL_MALFORMED_KEY                        CPKCL_SEVERE(0x0104)
#define CPKCL_APRIORI_OK                           CPKCL_SEVERE(0x0105)
#define CPKCL_WRONG_SERVICE                        CPKCL_SEVERE(0x0106)

// Run time events (not obviously severe)
#define CPKCL_POINT_AT_INFINITY                    CPKCL_WARNING(0x0001)
#define CPKCL_WRONG_SIGNATURE                      CPKCL_WARNING(0x0002)
#define CPKCL_WRONG_SELECTNUMBER                   CPKCL_WARNING(0x0003)
#define CPKCL_POINT_IS_NOT_ON_CURVE                CPKCL_WARNING(0x0004)
// Run time informations (even when in release)
#define CPKCL_NUMBER_IS_NOT_PRIME                  CPKCL_INFO  (0x0001)
#define CPKCL_NUMBER_IS_PRIME                      CPKCL_INFO  (0x0002)

#endif // _CRYPTOLIB_RC_PB_INCLUDED
