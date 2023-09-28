// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 */

#ifndef _LIBFIT_H
#define _LIBFIT_H

#include <libfdt.h>

typedef enum {
	FITIMG_PROP_KERNEL_TYPE,
	FITIMG_PROP_DT_TYPE,
	FITIMG_PROP_RAMDISK_TYPE
} fit_prop_t;

struct fit_context {
	uint8_t *fit;
	const char *cfg;
	int cfg_node_offset;
	uintptr_t entry;
	uintptr_t dtb;
};

bool fit_plat_is_ns_addr(uintptr_t addr);

int fit_plat_default_address(const struct fit_context *fit, fit_prop_t prop, uintptr_t *addr);

int fit_init_context(struct fit_context *context, uintptr_t fit_addr);

int fit_select(struct fit_context *context, const char *cfg);

int fit_load(struct fit_context *context, fit_prop_t prop);

void fit_fdt_update(struct fit_context *context,
		    uintptr_t mem_start,
		    size_t mem_length,
		    const char *bootargs);

size_t fit_size(const struct fit_context *context);

#endif /* _LIBFIT_H */
