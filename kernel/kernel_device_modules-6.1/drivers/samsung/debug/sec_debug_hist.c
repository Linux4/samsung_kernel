/*
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
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
#include <linux/of.h>
#include <linux/of_reserved_mem.h>

/* 
	--------------------------------------------  sec-initlog
	-             INIT_LOG_SIZE 1M             -
	--------------------------------------------  sec-autocomment
	-      AC_BUFFER_SIZE+AC_INFO_SIZE 64K     -  
	--------------------------------------------  debug-history
	-           DEBUG_HIST_SIZE 512K           -
	--------------------------------------------  dbore
	-           DEBUG_BORE_SIZE 512K           -
	--------------------------------------------  sec-extrainfo
    -	           REMAINED_SIZE			   -
    --------------------------------------------  end of reserved sec_debug
*/

static u32 dhist_base;
static u32 dhist_size;

extern struct reserved_mem *secdbg_log_get_rmem(const char *compatible);

static void sec_debug_history_base_init(void)
{
	struct reserved_mem *rmem;

	rmem = secdbg_log_get_rmem("samsung,secdbg-debug-history");

	dhist_base = rmem->base;
	dhist_size = rmem->size;
}

static ssize_t sec_debug_hist_history_read(struct file *file, char __user *buf,
				  size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count, ret = 0;
	char *base = NULL;

	if (pos >= dhist_size) {
		pr_crit("%s: pos %x , dhist: %x\n", __func__, (u32)pos, dhist_size);
		ret = 0;
		goto fail;
	}

	count = min(len, (size_t)(dhist_size - pos));

	base = (char *)phys_to_virt((phys_addr_t)dhist_base);
	if (!base) {
		pr_crit("%s: fail to get va (%x)\n", __func__, dhist_base);
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

static const struct proc_ops debug_history_proc_ops = {
	.proc_lseek = default_llseek,
	.proc_read = sec_debug_hist_history_read,
};

int sec_debug_history_init(void)
{
	struct proc_dir_entry *entry;

	sec_debug_history_base_init();

	pr_info("%s: base: %x size: %x\n", __func__, dhist_base, dhist_size);

	if (!dhist_base || !dhist_size)
		return 0;

	entry = proc_create("debug_history", S_IFREG | 0444, NULL, &debug_history_proc_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry debug hist\n", __func__);
		return 0;
	}

	pr_info("%s: success to create proc entry\n", __func__);

	proc_set_size(entry, (size_t)dhist_size);

	return 0;
}
EXPORT_SYMBOL(sec_debug_history_init);
