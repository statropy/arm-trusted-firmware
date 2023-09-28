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
//      File: CryptoLib_Services_pb.h
//------------------------------------------------------------------------
//      Description: Services of the Cryptographic library
//------------------------------------------------------------------------

#ifndef _CRYPTOLIB_SERVICES_PB_INCLUDED
#define _CRYPTOLIB_SERVICES_PB_INCLUDED

// Services definition
#define CPKCL_SERVICE_SelfTest                    		0xA0
#define CPKCL_SERVICE_Rng                         		0xA1
#define CPKCL_SERVICE_Smult                       		0xA2
#define CPKCL_SERVICE_Comp                        		0xA3
#define CPKCL_SERVICE_Fmult                       		0xA4
#define CPKCL_SERVICE_Square                      		0xA5
#define CPKCL_SERVICE_Swap                        		0xA6
#define CPKCL_SERVICE_FastCopy                    		0xA7
#define CPKCL_SERVICE_CondCopy                    		0xA8
#define CPKCL_SERVICE_ClearFlags                  		0xA9
#define CPKCL_SERVICE_Fill                        		0xAA
#define CPKCL_SERVICE_NOP                         		0xAB
//=======================================================
#define CPKCL_SERVICE_Div                         		0xC0
#define CPKCL_SERVICE_CRT                         		0xC1
#define CPKCL_SERVICE_PrimeGen                    		0xC2
#define CPKCL_SERVICE_ExpMod                      		0xC3
#define CPKCL_SERVICE_GCD                         		0xC4
#define CPKCL_SERVICE_RedMod                      		0xC5
//=======================================================
#define CPKCL_SERVICE_ZpEcRandomiseCoordinate     		0xE0
#define CPKCL_SERVICE_ZpEccAddSubFast			 		0xE1
#define CPKCL_SERVICE_ZpEcDsaVerifyFast           		0xE2
#define CPKCL_SERVICE_ZpEccDblFast                		0xE3
#define CPKCL_SERVICE_ZpEccQuickDualMulFast	 	 		0xE4
#define CPKCL_SERVICE_ZpEcDsaQuickVerify			 		0xE5
#define CPKCL_SERVICE_ZpEccMulFast                		0xE8
#define CPKCL_SERVICE_ZpEcConvAffineToProjective  		0xE9
#define CPKCL_SERVICE_ZpEcDsaGenerateFast         		0xEA
#define CPKCL_SERVICE_ZpEccAddFast                		0xEB
#define CPKCL_SERVICE_ZpEcPointIsOnCurve          		0xEC
#define CPKCL_SERVICE_ZpEcConvProjToAffine        		0xED

//=======================================================
#define CPKCL_SERVICE_GF2NEcRandomiseCoordinate     	0x90
#define CPKCL_SERVICE_GF2NEcDsaVerifyFast           	0x92
#define CPKCL_SERVICE_GF2NEccDblFast                	0x93
#define CPKCL_SERVICE_GF2NEccMulFast                	0x98
#define CPKCL_SERVICE_GF2NEcConvAffineToProjective  	0x99
#define CPKCL_SERVICE_GF2NEcDsaGenerateFast         	0x9A
#define CPKCL_SERVICE_GF2NEccAddFast                	0x9B
#define CPKCL_SERVICE_GF2NEcPointIsOnCurve          	0x9C
#define CPKCL_SERVICE_GF2NEcConvProjToAffine        	0x9D


#endif //_CRYPTOLIB_SERVICES_PB_INCLUDED

