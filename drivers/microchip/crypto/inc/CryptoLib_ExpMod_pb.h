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
//      File: CryptoLib_ExpMod_pb.h
//------------------------------------------------------------------------
//      Description: definition of types for the Cryptographic library
//------------------------------------------------------------------------
#ifndef _CRYPTOLIB_EXPMOD_PB_INCLUDED
#define _CRYPTOLIB_EXPMOD_PB_INCLUDED

// Structure definition
typedef struct _CPKCL_expmod {
               nu1       nu1ModBase;
               nu1       nu1CnsBase;
               u2        u2ModLength;

               nu1       nu1XBase;           // (3*u2NLength + 6) words LSW is always zero
               nu1       nu1PrecompBase;     // xxx words LSW is always zero
               u2        u2ExpLength;
               pfu1      pfu1ExpBase;        // u2ExpLength words
               u1        u1Blinding;         // Exponent blinding using a 32-bits Xor
               u1        __Padding0;
               u2        __Padding1;
               } _CPKCL_EXPMOD, *_PCPKCL_EXPMOD;

// Options definition
#define CPKCL_EXPMOD_REGULARRSA       0x01
#define CPKCL_EXPMOD_EXPINCPKCCRAM    0x02
#define CPKCL_EXPMOD_FASTRSA         0x04
#define CPKCL_EXPMOD_OPERATIONMASK   0x07
#define CPKCL_EXPMOD_MODEMASK        0x05     // For faults protection

#define CPKCL_EXPMOD_WINDOWSIZE_MASK 0x18
#define CPKCL_EXPMOD_WINDOWSIZE_1    0x00
#define CPKCL_EXPMOD_WINDOWSIZE_2    0x08
#define CPKCL_EXPMOD_WINDOWSIZE_3    0x10
#define CPKCL_EXPMOD_WINDOWSIZE_4    0x18
#define CPKCL_EXPMOD_WINDOWSIZE_BIT(a)   (u2)((a) & CPKCL_EXPMOD_WINDOWSIZE_MASK) >> 3


#endif // _CRYPTOLIB_EXPMOD_PB_INCLUDED
