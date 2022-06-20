// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 */

#ifndef _LIBFIT_H
#define _LIBFIT_H

#include <libfdt.h>

struct fit_context {
	uint8_t *fit;
	const char *cfg;
	int cfg_node_offset;
	uintptr_t entry;
	uintptr_t dtb;
};

typedef enum {
	FITIMG_PROP_KERNEL_TYPE,
	FITIMG_PROP_DT_TYPE,
	FITIMG_PROP_RAMDISK_TYPE
} fit_prop_t;

int fit_init_context(struct fit_context *context, uintptr_t fit_addr);

int fit_select(struct fit_context *context, const char *cfg);

int fit_load(struct fit_context *context, fit_prop_t prop);

void fit_fdt_update(struct fit_context *context,
		    uintptr_t mem_start,
		    size_t mem_length,
		    const char *bootargs);

#endif /* _LIBFIT_H */
