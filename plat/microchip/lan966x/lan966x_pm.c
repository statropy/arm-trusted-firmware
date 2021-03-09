/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <lib/psci/psci.h>
#include <plat/arm/common/plat_arm.h>
#include <platform_def.h>

/*******************************************************************************
 * Export the platform handlers via lan966x_psci_pm_ops. The ARM Standard
 * platform layer will take care of registering the handlers with PSCI.
 ******************************************************************************/
plat_psci_ops_t lan966x_psci_pm_ops = {
	/* dummy struct */
	.validate_ns_entrypoint = NULL,
};

int __init plat_setup_psci_ops(uintptr_t sec_entrypoint,
			       const plat_psci_ops_t **psci_ops)
{
	*psci_ops = &lan966x_psci_pm_ops;

	return 0;
}
