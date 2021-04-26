/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <common/debug.h>

#include "inc/CryptoLib_Headers_pb.h"
#include "inc/CryptoLib_JumpTable_pb.h"

#include <pkcl.h>

void pkcl_init(void)
{
	CPKCL_PARAM CPKCLParam;
	PCPKCL_PARAM pvCPKCLParam = &CPKCLParam;
	// vCPKCL_Process() is a macro command which fills-in the Command field
	// and then calls the library
	vCPKCL_Process(SelfTest, pvCPKCLParam);
	if (CPKCL(u2Status) == CPKCL_OK) {
		INFO("CPKCL V: 0x%08lx\n", CPKCL_SelfTest(u4Version));
	}
}
