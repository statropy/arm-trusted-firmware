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
//      File: CryptoLib_JumpTable_pb.h
//------------------------------------------------------------------------
//      Description: definition of services for the Cryptographic library
//------------------------------------------------------------------------

#ifndef _CRYPTOLIB_JUMPTABLE_PB_INCLUDED_
#define _CRYPTOLIB_JUMPTABLE_PB_INCLUDED_

typedef void (*PCPKCL_FUNC) (PCPKCL_PARAM);

// JumpTable address + 1 as it is thumb code

#define 	__vCPKCLCsJumpTableStart							 0x00040001
#define 	__vCPKCLCsNOP			                             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x00 ))
#define 	__vCPKCLCsSelfTest		                             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x04 ))
#define 	__vCPKCLCsFill                                       ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x08 ))
#define 	__vCPKCLCsSwap                                       ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x0c ))
#define 	__vCPKCLCsFastCopy                                   ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x10 ))
#define 	__vCPKCLCsCondCopy                                   ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x14 ))
#define 	__vCPKCLCsClearFlags                                 ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x18 ))
#define 	__vCPKCLCsComp                                       ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x1c ))
#define 	__vCPKCLCsFmult                                      ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x20 ))
#define 	__vCPKCLCsSmult                                      ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x24 ))
#define 	__vCPKCLCsSquare                                     ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x28 ))
#define 	__vCPKCLCsDiv                                        ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x2c ))
#define 	__vCPKCLCsRedMod                                     ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x30 ))
#define 	__vCPKCLCsExpMod                                     ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x34 ))
#define 	__vCPKCLCsCRT                                        ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x38 ))
#define 	__vCPKCLCsGCD                                        ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x3c ))
#define 	__vCPKCLCsPrimeGen                                   ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x40 ))
#define 	__vCPKCLCsZpEcPointIsOnCurve			             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x44 ))
#define 	__vCPKCLCsZpEcRandomiseCoordinate                    ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x48 ))
#define 	__vCPKCLCsZpEcConvAffineToProjective                 ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x4c ))
#define 	__vCPKCLCsZpEccAddFast                               ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x50 ))
#define 	__vCPKCLCsZpEccDblFast                               ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x54 ))
#define 	__vCPKCLCsZpEccMulFast                               ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x58 ))
#define 	__vCPKCLCsZpEcDsaGenerateFast                        ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x5c ))
#define 	__vCPKCLCsZpEcDsaVerifyFast                          ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x60 ))
#define 	__vCPKCLCsZpEcConvProjToAffine			             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x64 ))
#define 	__vCPKCLCsGF2NEcRandomiseCoordinate	  	             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x68 ))
#define 	__vCPKCLCsGF2NEcConvProjToAffine       	             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x6c ))
#define 	__vCPKCLCsGF2NEcConvAffineToProjective 	             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x70 ))
#define 	__vCPKCLCsGF2NEcPointIsOnCurve         	             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x74 ))
#define 	__vCPKCLCsGF2NEccAddFast               	             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x78 ))
#define 	__vCPKCLCsGF2NEccDblFast               	             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x7c ))
#define 	__vCPKCLCsGF2NEccMulFast               	             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x80 ))
#define 	__vCPKCLCsGF2NEcDsaGenerateFast        	             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x84 ))
#define 	__vCPKCLCsGF2NEcDsaVerifyFast          	             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x88 ))
#define 	__vCPKCLCsZpEccAddSubFast 				             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x8c ))
#define 	__vCPKCLCsZpEccQuickDualMulFast 				     ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x90 ))
#define 	__vCPKCLCsZpEcDsaQuickVerify 			             ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x94 ))
#define 	__vCPKCLCsRng                                        ((PCPKCL_FUNC)(__vCPKCLCsJumpTableStart + 0x98 ))
#endif


