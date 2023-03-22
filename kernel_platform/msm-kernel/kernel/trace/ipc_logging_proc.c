// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, The Linux Foundation. All rights reserved.
 *
 * This code copied from ipc_logging_debug.c
 */
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/idr.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/ipc_logging.h>

#include "ipc_logging_private.h"

static DEFINE_MUTEX(ipc_log_debugfs_init_lock);
static struct proc_dir_entry *root_dent;

static int debug_log(struct ipc_log_context *ilctxt,
		     char *buff, int size, int cont)
{
	int i = 0;
	int ret;

	if (size < MAX_MSG_DECODED_SIZE) {
		pr_err("%s: buffer size %d < %d\n", __func__, size,
			MAX_MSG_DECODED_SIZE);
		return -ENOMEM;
	}
	do {
		i = ipc_log_extract(ilctxt, buff, size - 1);
		if (cont && i == 0) {
			ret = wait_for_completion_interruptible(
				&ilctxt->read_avail);
			if (ret < 0)
				return ret;
		}
	} while (cont && i == 0);

	return i;
}

/*
 * VFS Read operation helper which dispatches the call to the procfs
 * read command stored in file->private_data.
 *
 * @file  File structure
 * @buff   user buffer
 * @count size of user buffer
 * @ppos  file position to read from (only a value of 0 is accepted)
 * @cont  1 = continuous mode (don't return 0 to signal end-of-file)
 *
 * @returns ==0 end of file
 *           >0 number of bytes read
 *           <0 error
 */
static ssize_t debug_read_helper(struct file *file, char __user *buff,
				 size_t count, loff_t *ppos, int cont)
{
	struct ipc_log_context *ilctxt = PDE_DATA(file_inode(file));
	char *buffer;
	int bsize;
	int r;

	r = kref_get_unless_zero(&ilctxt->refcount) ? 0 : -EIO;
	if (r)
		return r;

	buffer = kmalloc(count, GFP_KERNEL);
	if (!buffer) {
		bsize = -ENOMEM;
		goto done;
	}

	bsize = debug_log(ilctxt, buffer, count, cont);
	if (bsize > 0) {
		if (copy_to_user(buff, buffer, bsize)) {
			bsize = -EFAULT;
			kfree(buffer);
			goto done;
		}
		*ppos += bsize;
	}
	kfree(buffer);

done:
	ipc_log_context_put(ilctxt);
	return bsize;
}

static ssize_t debug_read(struct file *file, char __user *buff,
			  size_t count, loff_t *ppos)
{
	return debug_read_helper(file, buff, count, ppos, 0);
}

static ssize_t debug_read_cont(struct file *file, char __user *buff,
			       size_t count, loff_t *ppos)
{
	return debug_read_helper(file, buff, count, ppos, 1);
}

static const struct proc_ops debug_ops = {
	.proc_read = debug_read,
	.proc_open = simple_open,
};

static const struct proc_ops debug_ops_cont = {
	.proc_read = debug_read_cont,
	.proc_open = simple_open,
};

static void debug_create(const char *name, mode_t mode,
			 struct proc_dir_entry *dent,
			 struct ipc_log_context *ilctxt,
			 const struct proc_ops *fops)
{
	proc_create_data(name, mode, dent, fops, ilctxt);
}

static void dfunc_string(struct encode_context *ectxt,
			 struct decode_context *dctxt)
{
	tsv_timestamp_read(ectxt, dctxt, "");
	tsv_qtimer_read(ectxt, dctxt, " ");
	tsv_byte_array_read(ectxt, dctxt, "");

	/* add trailing \n if necessary */
	if (*(dctxt->buff - 1) != '\n') {
		if (dctxt->size) {
			++dctxt->buff;
			--dctxt->size;
		}
		*(dctxt->buff - 1) = '\n';
	}
}

void check_and_create_procfs(void)
{
	mutex_lock(&ipc_log_debugfs_init_lock);
	if (!root_dent) {
		root_dent = proc_mkdir("ipc_logging", 0);

		if (IS_ERR(root_dent)) {
			pr_err("%s: unable to create procfs %ld\n",
				__func__, PTR_ERR(root_dent));
			root_dent = NULL;
		}
	}
	mutex_unlock(&ipc_log_debugfs_init_lock);
}
EXPORT_SYMBOL(check_and_create_procfs);

void create_ctx_procfs(struct ipc_log_context *ctxt,
			const char *mod_name)
{
	if (!root_dent)
		check_and_create_procfs();

	if (root_dent) {
		ctxt->proc_dent = proc_mkdir(mod_name, root_dent);
		if (!IS_ERR(ctxt->dent)) {
			debug_create("log", 0444, ctxt->proc_dent,
				     ctxt, &debug_ops);
			debug_create("log_cont", 0444, ctxt->proc_dent,
				     ctxt, &debug_ops_cont);
		}
	}
	add_deserialization_func((void *)ctxt,
				 TSV_TYPE_STRING, dfunc_string);
}
EXPORT_SYMBOL(create_ctx_procfs);
