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
//      File: CryptoLib_Div_pb.h
//------------------------------------------------------------------------
//      Description: definition of types for the Cryptographic library
//------------------------------------------------------------------------
#ifndef _CRYPTOLIB_DIV_PB_INCLUDED
#define _CRYPTOLIB_DIV_PB_INCLUDED

// Structure definition
typedef struct _CPKCL_div {
               nu1       nu1ModBase;         // Denominator
               nu1       nu1WorkSpace;       // Workspace
               u2        u2ModLength;        // Denominator length

               nu1       nu1RBase;           // Result copy
               u2        __Padding0;
               nu1       nu1QuoBase;         // Quotient
               nu1       nu1NumBase;         // Numerator, Remainder
               u2        __Padding1;
               u2        __Padding2;
               u2        u2NumLength;        // Numerator length
               } _CPKCL_DIV, *_PCPKCL_DIV;

// Options definition
// None


#endif // _CRYPTOLIB_DIV_PB_INCLUDED
