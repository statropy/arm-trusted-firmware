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
//      File: CryptoLib_mapping_pb.h
//------------------------------------------------------------------------
//      Description: mapping of the Crypto Memory
//------------------------------------------------------------------------
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!! The Files CryptoLib_mapping_pb.h and CryptoLib_mapping_pb.inc !!!
// !!! must always contain the same values 							 !!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#ifndef _CRYPTOLIB_MAPPING_PB_INCLUDED
#define _CRYPTOLIB_MAPPING_PB_INCLUDED
// Memory definitions
#define nu1CRYPTORAM_BASE            (nu1)(AT91C_BASE_CRYPTO_RAM & 0x0000FFFF)
#define u2CRYPTORAM_LENGTH           (u2)0x1000
#define nu1CRYPTORAM_LAST            (nu1)(nu1CRYPTORAM_BASE + (u2CRYPTORAM_LENGTH - 1))
#define MSB_EXTENT_CRYPTORAM         (AT91C_BASE_CRYPTO_RAM & 0xFFFF0000)
#endif // _CRYPTOLIB_MAPPING_PB_INCLUDED
