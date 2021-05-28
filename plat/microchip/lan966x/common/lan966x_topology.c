/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <plat/common/platform.h>
#include <platform_def.h>

/* The LAN966X power domain tree descriptor */
static const unsigned char lan966x_power_domain_tree_desc[] = {
	1,
	/* No of children for the root node */
	PLATFORM_CLUSTER_COUNT,
	/* No of children for the first cluster node */
	PLATFORM_CORE_COUNT,
};

/*******************************************************************************
 * This function returns the topology according to PLATFORM_CLUSTER_COUNT.
 ******************************************************************************/
const unsigned char *plat_get_power_domain_tree_desc(void)
{
	return lan966x_power_domain_tree_desc;
}

/*******************************************************************************
 * Currently FVP VE has only been tested with one core, therefore 0 is returned.
 ******************************************************************************/
int plat_core_pos_by_mpidr(u_register_t mpidr)
{
	return 0;
}
