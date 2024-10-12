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
#include <linux/sec_debug.h>

static u32 __initdata dhist_base = SEC_HIST_LOG_BASE;
static u32 __initdata dhist_size = SZ_512K;

static ssize_t sec_debug_hist_hist_read(struct file *file, char __user *buf,
				  size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count, ret = 0;
	char *base = NULL;

	if (pos >= dhist_size) {
		pr_crit("%s: pos %llx , dhist: %lx\n", __func__, pos, dhist_size);

		ret = 0;

		goto fail;
	}

	count = min(len, (size_t)(dhist_size - pos));

	base = (char *)phys_to_virt((phys_addr_t)dhist_base);
	if (!base) {
		pr_crit("%s: fail to get va (%lx)\n", __func__, dhist_base);

		ret = -EFAULT;

		goto fail;
	}

	if (copy_to_user(buf, base + pos, count)) {
		pr_crit("%s: fail to copy to use\n", __func__);

		ret = -EFAULT;
	} else {
		pr_debug("%s: base: %p\n", __func__, base);

		*offset += count;
		ret = count;
	}

fail:
	return ret;
}

static const struct file_operations dhist_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_debug_hist_hist_read,
};

static int __init sec_debug_hist_late_init(void)
{
	struct proc_dir_entry *entry;
	char *p, *base;

	base = (char *)phys_to_virt((phys_addr_t)dhist_base);

	pr_info("%s: base: %p(%lx) size: %lx\n", __func__, base, dhist_base, dhist_size);

	if (!dhist_base || !dhist_size)
		return 0;

	p = base;
	pr_info("%s: dummy: %x\n", __func__, *p);
	pr_info("%s: magic: %x\n", __func__, *(unsigned int *)(p + 4));
	pr_info("%s: version: %x\n", __func__, *(unsigned int *)(p + 8));

	entry = proc_create("debug_history", S_IFREG | 0444, NULL, &dhist_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry debug hist\n", __func__);

		return 0;
	}

	pr_info("%s: success to create proc entry\n", __func__);

	proc_set_size(entry, (size_t)dhist_size);

	return 0;
}
late_initcall(sec_debug_hist_late_init);
