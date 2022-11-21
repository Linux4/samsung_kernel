/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include "sec_debug_internal.h"

static unsigned long summ_base;
static unsigned long summ_size;

static ssize_t secdbg_summ_read(struct file *file, char __user *buf,
				  size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count, ret = 0;
	char *base = NULL;

	if (!summ_size) {
		pr_crit("%s: size 0? %lx\n", __func__, summ_size);

		ret = -ENXIO;

		goto fail;
	}

	if (!summ_base) {
		pr_crit("%s: no base? %lx\n", __func__, summ_base);

		ret = -ENXIO;

		goto fail;
	}

	if (pos >= summ_size) {
		pr_crit("%s: pos %llx , summ_size: %lx\n", __func__, pos, summ_size);

		ret = 0;

		goto fail;
	}

	count = min(len, (size_t)(summ_size - pos));

	base = (char *)phys_to_virt((phys_addr_t)summ_base);
	if (!base) {
		pr_crit("%s: fail to get va (%lx)\n", __func__, summ_base);

		ret = -EFAULT;

		goto fail;
	}

	if (copy_to_user(buf, base + pos, count)) {
		pr_crit("%s: fail to copy to use\n", __func__);

		ret = -EFAULT;
	} else {
		*offset += count;
		ret = count;
	}

fail:
	return ret;
}

static const struct file_operations summ_file_ops = {
	.owner = THIS_MODULE,
	.read = secdbg_summ_read,
};

static int __init secdbg_summ_late_init(void)
{
	struct proc_dir_entry *entry;
	char *base;

	summ_base = secdbg_base_get_buf_base(SDN_MAP_DUMP_SUMMARY);
	base = (char *)phys_to_virt((phys_addr_t)summ_base);
	summ_size = secdbg_base_get_buf_size(SDN_MAP_DUMP_SUMMARY);

	pr_info("%s: base: %p(%lx) size: %lx\n", __func__, base, summ_base, summ_size);

	entry = proc_create("reset_summary", S_IFREG | 0444, NULL, &summ_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry dump summary\n", __func__);

		return 0;
	}

	pr_info("%s: success to create proc entry\n", __func__);

	proc_set_size(entry, (size_t)summ_size);

	return 0;
}
late_initcall(secdbg_summ_late_init);
