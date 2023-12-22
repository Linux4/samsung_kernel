
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
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>

#include <linux/ipc_logging.h>
#include <uapi/linux/msm_geni_serial.h>
#include <linux/mm.h>
#include <linux/list.h>

#include "msm_geni_serial_proc_log.h"



struct ipc_log_context_entry {
	struct list_head list;
	struct ipc_log_context *ctxt;
};

#define IPC_ALL_DATA_MAX_SIZE (1024*1024)

struct serial_ipc_ctxt_export_data {
	char *ipc_all_data_buffer;
	size_t ipc_all_data_buffer_len;
	atomic_t serial_proc_usage;
};


static DEFINE_MUTEX(ipc_log_context_list_lock);
static struct list_head serial_ipc_log_ctxt_list  =
				LIST_HEAD_INIT(serial_ipc_log_ctxt_list);

static struct serial_ipc_ctxt_export_data proc_data;

static int debug_log(struct ipc_log_context *ctxt,
			 char *buff, int size)
{
	if (size < MAX_MSG_DECODED_SIZE) {
		pr_err("%s: buffer size %d < %d\n", __func__, size,
			MAX_MSG_DECODED_SIZE);
		return -ENOMEM;
	}
	return	ipc_log_extract(ctxt, buff, size - 1);
}

static ssize_t uart_serial_log_file_read(struct file *file, char __user *buf,
				  size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;

	if (pos > proc_data.ipc_all_data_buffer_len)
		return 0;

	count = min(len, (size_t)(proc_data.ipc_all_data_buffer - pos));
	if (copy_to_user(buf, proc_data.ipc_all_data_buffer + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static int uart_serial_log_file_open(struct inode *inode, struct file *file)
{
#define IPC_NAME_MARK_LEN 15
#define IPC_ONETIME_CHUCK_SIZE (4*PAGE_SIZE)

	int bsize;
	int offset = 0;
	struct ipc_log_context_entry *entry;

	if (atomic_read(&proc_data.serial_proc_usage) > 0)
		return -ENODEV;

	atomic_inc(&proc_data.serial_proc_usage);

	proc_data.ipc_all_data_buffer = kvmalloc(IPC_ALL_DATA_MAX_SIZE, GFP_KERNEL);
	if (!proc_data.ipc_all_data_buffer)
		return -ENOMEM;

    mutex_lock(&ipc_log_context_list_lock);
	list_for_each_entry(entry, &serial_ipc_log_ctxt_list, list) {
		offset += snprintf((char *)proc_data.ipc_all_data_buffer + offset,
						IPC_LOG_MAX_CONTEXT_NAME_LEN + IPC_NAME_MARK_LEN, "[*****%s*****]\n", entry->ctxt->name);
		do {
			bsize = debug_log(entry->ctxt, (char *)proc_data.ipc_all_data_buffer + offset, IPC_ONETIME_CHUCK_SIZE);
			offset += bsize;
		} while(bsize > 0 && offset < IPC_ALL_DATA_MAX_SIZE);
	}
    mutex_unlock(&ipc_log_context_list_lock);

	if (offset >= IPC_ALL_DATA_MAX_SIZE) {
		pr_info("[%s] ipc data is over %d\n", __func__, IPC_ALL_DATA_MAX_SIZE);
		return -ENOMEM;
	}


	proc_data.ipc_all_data_buffer_len = offset;
	return 0;
}

static int uart_serial_log_file_release(struct inode *inode, struct file *file)
{
	if (proc_data.ipc_all_data_buffer)
		kvfree(proc_data.ipc_all_data_buffer);

	atomic_dec(&proc_data.serial_proc_usage);
	return 0;
}

static const struct proc_ops proc_log_file_ops = {
	.proc_read	= uart_serial_log_file_read,
	.proc_open	= uart_serial_log_file_open,
	.proc_release	= uart_serial_log_file_release,
};

int register_serial_ipc_log_context(struct ipc_log_context *ctxt)
{

	struct ipc_log_context_entry *entry;

	if (!ctxt)
		return -EINVAL;

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	entry->ctxt = ctxt;

    mutex_lock(&ipc_log_context_list_lock);
	list_add(&entry->list, &serial_ipc_log_ctxt_list);
    mutex_unlock(&ipc_log_context_list_lock);

	return 0;
}

int create_proc_log_file(void)
{
	struct proc_dir_entry *entry;
	struct proc_dir_entry *proc_serial;
	struct proc_dir_entry *proc_serial_uart;

	atomic_set(&proc_data.serial_proc_usage, 0);
	proc_serial = proc_mkdir("serial", NULL);
	if (!proc_serial) {
		pr_info("[%s] failed to make directory /proc/serial, ret:%d\n", proc_serial);
		return -ENOENT;
	}

	proc_serial_uart = proc_mkdir("uart", proc_serial);
	if (!proc_serial_uart) {
		pr_info("[%s] failed to make directory /proc/serial/uart, ret:%d\n", proc_serial_uart);
		return -ENOENT;
	}

	entry = proc_create("log", S_IFREG | 0444, proc_serial_uart, &proc_log_file_ops);
	if (!entry) {
		pr_info("[%s] failed to proc file /proc/serial/uart/log ret:%d\n", entry);
		return -ENOENT;
	}

	pr_info("%s: success to create proc entry\n", __func__);
	return 0;
}
MODULE_LICENSE("GPL v2");