// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Copyright (C) 2021 Microchip Technology Inc. and its subsidiaries.
 */

#define FITIMG_IMAGES_PATH		"/images"
#define FITIMG_CONFS_PATH		"/configurations"

#define FITIMG_DATA_PROP_STR		"data"
#define FITIMG_DESC_PROP_STR		"description"
#define FITIMG_COMP_PROP_STR		"compression"
#define FITIMG_ENTRY_PROP_STR		"entry"
#define FITIMG_LOAD_PROP_STR		"load"

#define FITIMG_KERNEL_PROP_STR		"kernel"
#define FITIMG_RAMDISK_PROP_STR		"ramdisk"
#define FITIMG_FDT_PROP_STR		"fdt"
#define FITIMG_DEFAULT_PROP_STR		"default"

#define MAX_FDT_SIZE		UL(128 * 1024) /* 128k */

static inline const char *fit_get_name(const void *fit_hdr,
				       int node_offset, int *len)
{
        return fdt_get_name(fit_hdr, node_offset, len);
}

static inline unsigned long fit_get_size(const void *fit)
{
        return fdt_totalsize(fit);
}
