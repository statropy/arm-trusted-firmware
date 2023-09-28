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
//      File: CryptoLib_Hash_pb.h
//------------------------------------------------------------------------
//      Description: definition of types for the Cryptographic library
//------------------------------------------------------------------------
#ifndef _CRYPTOLIB_HASH_INCLUDED
#define _CRYPTOLIB_HASH_INCLUDED

// Structure definition
typedef struct _CPKCL_hash {
               pu1       pu1WorkSpace;
               pu1       pu1MBase;
               pu1       pu1HBase;
               pu1       pu1WorkSpace2;
               u2        __Padding1;
               u2        __Padding2;
               } _CPKCL_HASH, *_PCPKCL_HASH, _CPKCL_HASH, *_PCPKCL_HASH;

// Service definition
// SubServices definition
#define CPKCL_HASH_VOID			   0x00

// Options definition
// Choose function
#define CPKCL_HASH_INITIALISE       0x0001
#define CPKCL_HASH_UPDATE           0x0002
// Choose MSB/LSB
#define CPKCL_HASH_LSBFIRST         0x0008
#define CPKCL_HASH_MSBFIRST         0x0000
#define CPKCL_HASH_MSBLSBFIRST_MASK 0x0008

#endif // _CRYPTOLIB_HASH_INCLUDED
