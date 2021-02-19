/*
 * Copyright (c) 2015-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>

#include <platform_def.h>

#include <plat/arm/common/plat_arm.h>
#include <plat/common/platform.h>


/*
 * ARM common implementation for error handler
 * ToDo: implementation not complete
 */
void __dead2 plat_arm_error_handler(int err)
{
    switch (err) {
    case -ENOENT:
    case -EAUTH:
        break;
    default:
        /* Unexpected error */
        break;
    }

    /* Loop until the watchdog resets the system */
    for (;;)
        wfi();
}


void __dead2 plat_error_handler(int err)
{
	plat_arm_error_handler(err);
}
