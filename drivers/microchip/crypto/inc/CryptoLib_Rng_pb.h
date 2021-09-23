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
//      File: CryptoLib_Rng_pb.h
//------------------------------------------------------------------------
//      Description: definition of types for the Cryptographic library
//------------------------------------------------------------------------
#ifndef _CRYPTOLIB_RNG_PB_INCLUDED
#define _CRYPTOLIB_RNG_PB_INCLUDED

// Structure definition
typedef struct _CPKCL_rng {
               nu1       nu1XKeyBase;        // Pointer to the Input and Output XKEY value of length u2XKeyLength bytes
               nu1       nu1WorkSpace;       // Pointer to the workspace of length 64 bytes
               u2        u2XKeyLength;       // Length in bytes multiple of 4 of XKEY, XSEED[0], XSEED[1]

               nu1       nu1XSeedBase;       // Pointer to the Input value of XSEED[0] and XSEED[1] of length (2*u1XKeyLength + 4)
               nu1       nu1WorkSpace2;		 // Pointer to the WorkSpace2 Of SHA (HICM)
               nu1       nu1QBase;           // Pointer to the Input prime number Q of length 160 bits = 20 bytes
               nu1       nu1RBase;           // (Significant length of 'N' without the padding word)
               u2        u2RLength;          // length of the resulting RNG
               u2        u2X9_31Rounds;
               u2        u2RNGOption;		 // When u2 RNGOption is filled with 0xFFFF, the last modular reduction is not performed
               } _CPKCL_RNG, *_PCPKCL_RNG;

// Options definition
#define CPKCL_RNG_SEED              0x01
#define CPKCL_RNG_GET               0x02
#define CPKCL_RNG_GETSEED           0x03
#define CPKCL_RNG_X931_GET          0x04
#define CPKCL_RNG_WITH_REDUCTION    0x0000
#define CPKCL_RNG_WITHOUT_REDUCTION 0xFFFF


#endif //_CRYPTOLIB_RNG_PB_INCLUDED

