/*
 * sec_debug_auto_comment.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#include <linux/memblock.h>
#include <linux/sec_debug.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <asm/sections.h>
#include <linux/memblock.h>
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

#define AC_SIZE 0x2000

static u32 sec_auto_comment_base;
static char *auto_comment_buf;

extern struct reserved_mem *secdbg_log_get_rmem(const char *compatible);

static void sec_debug_auto_comment_base_init(void)
{
	struct reserved_mem *rmem;

	rmem = secdbg_log_get_rmem("samsung,secdbg-auto-comment");

	sec_auto_comment_base = rmem->base;
	auto_comment_buf = (char *)phys_to_virt(sec_auto_comment_base);

	if (!auto_comment_buf) {
		pr_crit("%s: no buffer for auto comment\n", __func__);
	}

	pr_info("%s: done\n", __func__);
}

static ssize_t sec_debug_auto_comment_proc_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (!auto_comment_buf) {
		pr_err("%s : buffer is null\n", __func__);
		return -ENODEV;
	}

	if (pos > AC_SIZE) {
		pr_err("%s : pos 0x%llx\n", __func__, pos);
		return 0;
	}

	if (strncmp(auto_comment_buf, "@ Ramdump", 9)) {
		pr_err("%s : no data in auto_comment\n", __func__);
		return 0;
	}

	count = min(len, (size_t)(AC_SIZE - pos));
	if (copy_to_user(buf, auto_comment_buf + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct proc_ops sec_debug_auto_comment_proc_fops = {
	.proc_lseek = default_llseek,
	.proc_read = sec_debug_auto_comment_proc_read,
};

int sec_debug_auto_comment_init(void)
{
	struct proc_dir_entry *entry;

	sec_debug_auto_comment_base_init();

	entry = proc_create("auto_comment", 0400, NULL,
					&sec_debug_auto_comment_proc_fops);

	if (!entry)
		return -ENOMEM;

	proc_set_size(entry, AC_SIZE);

	pr_info("%s: done\n", __func__);

	return 0;
}
EXPORT_SYMBOL(sec_debug_auto_comment_init);
