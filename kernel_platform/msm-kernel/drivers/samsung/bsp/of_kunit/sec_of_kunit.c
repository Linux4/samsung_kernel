// SPDX-License-Identifier: GPL-2.0

#include <linux/libfdt.h>
#include <linux/of.h>
#include <linux/slab.h>

#include <linux/samsung/sec_of_kunit.h>

#include "of_private.h"

static void *__dt_alloc(u64 __size, u64 align)
{
	u64 size = ALIGN(__size, align);
	void *ptr = kmalloc(size, GFP_KERNEL);

	if (!ptr)
		panic("%s: Failed to allocate %llu bytes align=0x%llx\n",
		      __func__, size, align);

	return ptr;
}

/* NOTE: Inspired from 'drivers/of/unittest.c'. */
struct device_node *sec_of_kunit_dtb_to_fdt(struct sec_of_dtb_info *info)
{
	struct device_node *root;
	u32 data_size;
	u32 size;

	data_size = info->dtb_end - info->dtb_begin;
	if (!data_size) {
		pr_err("No dtb 'overlay_base' to attach\n");
		return ERR_PTR(-ENOENT);
	}

	size = fdt_totalsize(info->dtb_begin);
	if (size != data_size) {
		pr_err("dtb 'overlay_base' header totalsize != actual size");
		return ERR_PTR(-EINVAL);
	}

	__unflatten_device_tree((void *)info->dtb_begin, NULL, &root,
				__dt_alloc, true);

	return root;
}

EXPORT_SYMBOL_GPL(sec_of_kunit_dtb_to_fdt);
