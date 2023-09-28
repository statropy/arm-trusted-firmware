//========================================================================
//
//   NOTE :    THIS SOFTWARE IS SUPPLIED FOR DEMONSTRATION PURPOSES ONLY.
//             THERE IS NO SECURITY BUILT INTO THIS SOFTWARE
//
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
//      ATMEL CryptoLib Library
//------------------------------------------------------------------------
//      File: CryptoLib_ZpEcDsa_pb.h
//------------------------------------------------------------------------
//      Description: definition of types for the Cryptographic library
//------------------------------------------------------------------------
#ifndef _CRYPTOLIBZPECDSA_INCLUDED
#define _CRYPTOLIBZPECDSA_INCLUDED

// Structure definition
typedef struct _CPKCL_ZpEcDsaGenerate {
               nu1       nu1ModBase;
               nu1       nu1CnsBase;
               u2        u2ModLength;

               nu1       nu1PointABase;
               nu1       nu1PrivateKey;
               nu1       nu1ScalarNumber;
               nu1       nu1OrderPointBase;
               nu1       nu1ABase;
               nu1       nu1Workspace;
               nu1       nu1HashBase;
               u2        u2ScalarLength;
               u2        __Padding0;
               } _CPKCL_ZPECDSAGENERATE, *_P_CPKCL_ZPECDSAGENERATE;

typedef struct _CPKCL_ZpEcDsaVerify {
               nu1       nu1ModBase;
               nu1       nu1CnsBase;
               u2        u2ModLength;

               nu1       nu1PointABase;
               nu1       nu1PointPublicKeyGen;
               nu1       nu1PointSignature;
               nu1       nu1OrderPointBase;
               nu1       nu1ABase;
               nu1       nu1Workspace;
               nu1       nu1HashBase;
               u2        u2ScalarLength;
               u2        __Padding0;
               } _CPKCL_ZPECDSAVERIFY, *_P_CPKCL_ZPECDSAVERIFY;


typedef struct _CPKCL_ZpEcDsaQuickVerify {
				pu1       pu1ModCnsBase;  		// ModBase and CnsBase at (ModBase + u2ModLength + 4)
				pu1       pu1PointABase;
				pu1       pu1PointPublicKeyGen;
				pu1       pu1PointSignature;
				pu1       pu1OrderPointBase;
				pu1       pu1AWorkBase;			 // ABase and Workspace at (ABase + u2ModLength + 4)
				pu1       pu1HashBase;
				u2        u2ModLength;
				u2        u2ScalarLength;
			  } _CPKCL_ZPECDSAQUICKVERIFY, *_P_CPKCL_ZPECDSAQUICKVERIFY;


#endif // _CRYPTOLIBZPECDSA_INCLUDED
