/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLAT_OTP_H
#define PLAT_OTP_H

#include <drivers/microchip/otp.h>

/*
 * Note: The first part of the OTP is reserved for secure data.	 The
 * data is protected solely by policy, as the OTP device in itself is
 * a secure device (by default).
 */

/* (TBBR) Root of trust Public Key Hash (ROTPK) */
#define OTP_OFF_ROTPK	0
#define OTP_LEN_ROTPK	256

/* (TBBR) Hardware Unique Key (HUK) */
#define OTP_OFF_HUK	(OTP_OFF_ROTPK + OTP_LEN_ROTPK)
#define OTP_LEN_HUK	128

/* (TBBR) Endorsement Key (EK, optional) */
#define OTP_OFF_EK	(OTP_OFF_HUK + OTP_LEN_HUK)
#define OTP_LEN_EK	256

/* (TBBR) Secret Symmetric Key (SSK, Optional) */
#define OTP_OFF_SSK	(OTP_OFF_EK + OTP_LEN_EK)
#define OTP_LEN_SSK	128

/* (TBBR) NV counters - trusted */
#define OTP_OFF_TNVCT	(OTP_OFF_SSK + OTP_LEN_SSK)
#define OTP_LEN_TNVCT	64	/* Min 31 states plus overflow */

/* (TBBR) NV counters - non-trusted */
#define OTP_OFF_NTNVCT	(OTP_OFF_TNVCT + OTP_LEN_TNVCT)
#define OTP_LEN_NTNVCT	256	/* Min 223 states plus overflow */

/* (MCHP) Flags1 */
#define OTP_OFF_FLAGS1	(OTP_OFF_NTNVCT + OTP_LEN_NTNVCT)
#define OTP_LEN_FLAGS1	32

#define OTP_FLAGS1_ENABLE_OTP_EMU		BIT(0)

/* NON-SECURE DATA AREA @ 4kb */
#define OTP_NS_OFF	(4096 * 8)

/* NS (MCHP) MAC address */
#define OTP_OFF_MACADDR	OTP_NS_OFF
#define OTP_LEN_MACADDR	48

/* bitfield.h compatibility */

#define __bf_shf(x) (__builtin_ffsll(x) - 1)

/**
 * FIELD_PREP() - prepare a bitfield element
 * @_mask: shifted mask defining the field's length and position
 * @_val:  value to put in the field
 *
 * FIELD_PREP() masks and shifts up the value.	The result should
 * be combined with other fields of the bitfield using logical OR.
 */
#define FIELD_PREP(_mask, _val)						\
	({								\
		((typeof(_mask))(_val) << __bf_shf(_mask)) & (_mask);	\
	})

/**
 * FIELD_GET() - extract a bitfield element
 * @_mask: shifted mask defining the field's length and position
 * @_reg:  value of entire bitfield
 *
 * FIELD_GET() extracts the field specified by @_mask from the
 * bitfield passed in as @_reg by masking and shifting it down.
 */
#define FIELD_GET(_mask, _reg)						\
	({								\
		(typeof(_mask))(((_reg) & (_mask)) >> __bf_shf(_mask)); \
	})

#endif	/* PLAT_OTP_H */
