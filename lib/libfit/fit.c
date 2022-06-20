// SPDX-License-Identifier: (GPL-2.0-or-later OR BSD-2-Clause)
/*
 * Copyright (C) 2022 Microchip Technology Inc. and its subsidiaries.
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <common/debug.h>
#include <errno.h>

#include <lib/libfdt/libfdt.h>
#include <lib/libfit/libfit.h>

#include "fit.h"

static inline bool is_ns_ddr(uintptr_t addr)
{
	if (addr < PLAT_LAN966X_NS_IMAGE_BASE || addr >= PLAT_LAN966X_NS_IMAGE_LIMIT)
		return false;

	return true;
}

static int fit_check_image_format(const void *fit)
{
	if (fdt_getprop(fit, 0, FITIMG_DESC_PROP_STR, NULL) == NULL) {
		return 0;
	}

	if (fdt_path_offset(fit, FITIMG_IMAGES_PATH) < 0) {
		return 0;
	}

	return 1;
}

static int fit_verify_header(const unsigned char *ptr)
{
	if (fdt_check_header(ptr) != EXIT_SUCCESS || !fit_check_image_format(ptr))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

int fit_init_context(struct fit_context *context, uintptr_t fit_addr)
{
	if (!is_ns_ddr(fit_addr))
		return -EINVAL;

	memset(context, 0, sizeof(*context));
	context->fit = (void*) fit_addr;

	return fit_verify_header(context->fit);
}

static int fit_image_get_node(const void *fit, const char *img_name)
{
	int images_node_offset;

	images_node_offset = fdt_path_offset(fit, FITIMG_IMAGES_PATH);
	if (images_node_offset < 0)
		return images_node_offset;

	return fdt_subnode_offset(fit, images_node_offset, img_name);
}

static int fit_conf_get_property_node_no(const void *fit, int node_offset,
					 const char *prop_name, int index)
{
	const char *name;
	int node_length;

	name = fdt_stringlist_get(fit, node_offset, prop_name, index, &node_length);
	if (name == NULL)
		return node_length;

	return fit_image_get_node(fit, name);
}

static int fit_conf_get_property_node(const void *fit, int node_offset,
				      const char *prop_name)
{
	return fit_conf_get_property_node_no(fit, node_offset, prop_name, 0);
}

static int fit_conf_get_default_node(const void *fit)
{
	const char *cfg_name;
	int node_offset, confs_node_offset;
	int node_length;

	confs_node_offset = fdt_path_offset(fit, FITIMG_CONFS_PATH);
	if (confs_node_offset < 0) {
		ERROR("fit: Can't find configurations parent node '%s' (%s)\n",
		       FITIMG_CONFS_PATH, fdt_strerror(confs_node_offset));
		return confs_node_offset;
	}

	/* Get default */
	cfg_name = (char *)fdt_getprop(fit, confs_node_offset, FITIMG_DEFAULT_PROP_STR, &node_length);
	if (cfg_name == NULL)
		return -ENOENT;

	node_offset = fdt_subnode_offset(fit, confs_node_offset, cfg_name);
	if (node_offset < 0) {
		ERROR("fit: No node found for config name: '%s' (%s)\n",
		      cfg_name, fdt_strerror(node_offset));
	}

	return node_offset;
}

static int fit_image_check_comp(const void *fit, int node_offset)
{
	const char *val;

        /* Check compression property */
        val = (char *) fdt_getprop(fit, node_offset, FITIMG_COMP_PROP_STR, NULL);
        if (val == NULL || strcmp("none", val) == 0)
		return 0;	/* No compression */
	INFO("fit: Compression '%s' is NOT supported\n", val);
	return -EPROTONOSUPPORT;
}

static int fit_image_get_data(const void *fit, int node_offset,
			      const void **data, size_t *size)
{
	int node_length;

	*data = fdt_getprop(fit, node_offset, FITIMG_DATA_PROP_STR, &node_length);
	if (*data == NULL) {
		*size = 0;
		return -1;
	}

	*size = node_length;
	return 0;
}

static int fit_image_get_address(const void *fit, int node_offset, char *name,
			  uintptr_t *load)
{
	int node_length, cell_len;
	const fdt32_t *cell;
	uint64_t load64 = 0;

	cell = fdt_getprop(fit, node_offset, name, &node_length);
	if (cell == NULL) {
		ERROR("fit: Cell %s not found at offset 0x%08x\n", name, node_offset);
		return -1;
	}

	if (node_length > sizeof(uintptr_t)) {
		ERROR("fit: Unsupported %s address size\n", name);
		return -1;
	}

	cell_len = node_length >> 2;
	/* Use load64 to avoid compiling warning for 32-bit target */
	while (cell_len--) {
		load64 = (load64 << 32) | fdt32_to_cpu(*cell);
		cell++;
	}
	*load = (uintptr_t)load64;

	return is_ns_ddr(*load) ? 0 : -EINVAL;
}

int fit_image_get_load(const void *fit, int node_offset, uintptr_t *load)
{
	return fit_image_get_address(fit, node_offset, FITIMG_LOAD_PROP_STR, load);
}

int fit_image_get_entry(const void *fit, int node_offset, uintptr_t *entry)
{
	return fit_image_get_address(fit, node_offset, FITIMG_ENTRY_PROP_STR, entry);
}

int fit_select(struct fit_context *context, const char *cfg)
{
	if (fit_verify_header(context->fit) != EXIT_SUCCESS) {
		INFO("fit: Not a FIT image\n");
		return -ENODEV;
	}

	INFO("fit: Looking for '%s' config\n", cfg ? cfg : "default");

	if (cfg) {
		const char *cfg_name;
		int confs_node_offset, fdt_offset;

		confs_node_offset = fdt_path_offset(context->fit, FITIMG_CONFS_PATH);
		if (confs_node_offset < 0) {
			ERROR("fit: Can't find configurations parent node '%s' (%s)\n",
			       FITIMG_CONFS_PATH, fdt_strerror(confs_node_offset));
			return confs_node_offset;
		}

		fdt_for_each_subnode(fdt_offset, context->fit, confs_node_offset) {
			cfg_name = fdt_get_name(context->fit, fdt_offset, NULL);
			if (strcmp(cfg, cfg_name) == 0) {
				break;
			}
		}

		/* Found? */
		if (fdt_offset < 0) {
			ERROR("fit: Could not find '%s' configuration node\n", cfg);
			return -ENOENT;
		}

		/* Remember config name and offset */
		context->cfg = cfg;
		context->cfg_node_offset = fdt_offset;
	} else {
		context->cfg_node_offset = fit_conf_get_default_node(context->fit);
		if (context->cfg_node_offset < 0) {
			ERROR("fit: Could not find default configuration node\n");
			return -ENOENT;
		}
		context->cfg = fdt_get_name(context->fit, context->cfg_node_offset, NULL);
	}

	INFO("fit: Using '%s' config\n", context->cfg);

	return EXIT_SUCCESS;
}

const char *fit_property_2_str(fit_prop_t prop)
{
	switch (prop) {
	case FITIMG_PROP_KERNEL_TYPE:
		return FITIMG_KERNEL_PROP_STR;
	case FITIMG_PROP_DT_TYPE:
		return FITIMG_FDT_PROP_STR;
	case FITIMG_PROP_RAMDISK_TYPE:
		return FITIMG_RAMDISK_PROP_STR;
	}
	ERROR("fit: Invalid prop type: %d\n", prop);
	return NULL;
}

/*
 * Inspired by fit_extract_contents() form U-Boot
 */
int fit_load(struct fit_context *context, fit_prop_t prop)
{
	const char *prop_name;
	const void *buf;
	int node_offset, ret;
	size_t size;
	uintptr_t load_start, load_end, image_start, image_end;

	prop_name = fit_property_2_str(prop);
	if (!prop_name)
		return -EINVAL;

	node_offset = fit_conf_get_property_node(context->fit, context->cfg_node_offset, prop_name);
	if (node_offset < 0) {
		ERROR("fit: Could not find subimage node: %s\n", prop_name);
		return -ENOENT;
	}

	/* Check compression */
	if ((ret = fit_image_check_comp(context->fit, node_offset)))
		return ret;

	/* get image data address and length */
	if (fit_image_get_data(context->fit, node_offset, &buf, &size)) {
		ERROR("fit: Could not find %s subimage data!\n", prop_name);
		return -ENOENT;
	}

	ret = fit_image_get_load(context->fit, node_offset, &load_start);
	if (ret == 0) {
		/* Check for overwriting */
		image_start = (uintptr_t) buf;
		image_end = image_start + fit_get_size(context->fit);
		load_end = load_start + size;

		if (!is_ns_ddr(load_start) || !is_ns_ddr(load_end)) {
			ERROR("fit: Only load to NS DDR allowed: %p-%p\n", (void*) load_start, (void*) load_end);
			ret = -EACCES;
		} else if (prop != FITIMG_PROP_KERNEL_TYPE &&
			   load_start < image_end && load_end > image_start) {
			/* NB: Kernel allowed to overwrite - it's last */
			ERROR("fit: %s is overwriting FIT image\n", prop_name);
			ret = -EXDEV;
		} else {
			/* Special prop handling */
			switch (prop) {
			case FITIMG_PROP_KERNEL_TYPE:
				ret = fit_image_get_entry(context->fit, node_offset, &context->entry);
				break;
			case FITIMG_PROP_DT_TYPE:
				context->dtb = load_start;
				break;
			default:
				break;
			}

			INFO("fit: Loading %s from %p to 0x%08lx, %zd bytes\n",
			     prop_name, buf, load_start, size);

			/* Move the data in place */
			memmove((void*) load_start, buf, size);

			/* We need to flush as we'll be in another CPU domain */
			flush_dcache_range(load_start, size);
		}
	}

	return ret;
}

/* Assumes size/addr cell "1" */
static int fit_fdt_set_memory(void *fdt, uintptr_t start, size_t length)
{
	uint32_t fdtsize;
	int ret, fdt_offset;

	fdtsize = cpu_to_fdt32(length);

	fdt_offset = fdt_add_subnode(fdt, 0, "memory");
	if (fdt_offset == -FDT_ERR_EXISTS) {
		fdt_offset = fdt_path_offset(fdt, "/memory");
	}
	if (fdt_offset < 0)
		return fdt_offset;

	ret = fdt_setprop_string(fdt, fdt_offset, "device_type", "memory");
	if (ret < 0)
		return ret;

	ret = fdt_setprop_u32(fdt, fdt_offset, "reg", start);
	if (ret < 0)
		return ret;

	ret = fdt_appendprop(fdt, fdt_offset, "reg", &fdtsize,
			     sizeof(fdtsize));
	return ret;
}

void fit_fdt_update(struct fit_context *context,
		    uintptr_t mem_start,
		    size_t mem_length,
		    const char *bootargs)
{
	void *fdt = (void*) context->dtb;
	int ret;

	ret = fdt_open_into(fdt, fdt, MAX_FDT_SIZE);
	if (ret < 0) {
		ERROR("Invalid Device Tree at %p: error %d\n", fdt, ret);
		return;
	}

	ret = fit_fdt_set_memory(fdt, mem_start, mem_length);
	if (ret < 0) {
		ERROR("Failed to add memory mode in Device Tree: %d\n", ret);
		return;
	}

	if (bootargs) {
		int chosen;

		chosen = fdt_add_subnode(fdt, 0, "chosen");
		if (chosen == -FDT_ERR_EXISTS) {
			chosen = fdt_path_offset(fdt, "/chosen");
		}
		if (chosen < 0) {
			ERROR("cannot find /chosen node: %d\n", chosen);
		} else {
			const void *existing = fdt_getprop(fdt, chosen, "bootargs", NULL);

			/* Only set 'bootargs' if not already present */
			if (existing == NULL) {
				INFO("fit: Adding DT bootargs '%s'\n", bootargs);
				ret = fdt_setprop_string(fdt, chosen, "bootargs", bootargs);
				if (ret)
					ERROR("fit: Could not set command line: %d\n", ret);
			}
		}
	}

	ret = fdt_pack(fdt);
	if (ret < 0)
		ERROR("Failed to pack Device Tree at %p: error %d\n", fdt, ret);

	/* We need to flush as we'll be in another CPU domain */
	flush_dcache_range(context->dtb, fdt_totalsize(fdt));
}
