/*
 * Copyright (c) 2018-2020 Silex Insight sa
 * Copyright (c) 2018-2020 Beerten Engineering scs
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <silexpk/core.h>

static const char *descriptions[] = {
    "OK",
    "Invalid paramter",
    "Unknwon error",
    "Busy",
    "Non quadratic residue",
    "Composite value",
    "Not invertible",
    "Invalid signature",
    "Not implemented",
    "Unexpected micro-code instruction",
    "Out of range",
    "Invalid modulus (must be odd)",
    "Point not on curve",
    "Operand too large",
    "Platform error",
    "Expired",
    "Still in IK mode",
    "Invalid elliptic curve parameters",
    "IK not ready. Please initialize IK first",
    "Resources not available for a new operation. Retry later",
    "Low order point or check on point order failed",
};


const char *sx_describe_statuscode(unsigned int code)
{
    if (code >= sizeof(descriptions)/sizeof(descriptions[0]))
        return "Invalid status code";
    return descriptions[code];
}
