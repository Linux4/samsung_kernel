/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <soc/samsung/exynos/memlogger.h>

#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "log.h"
#include "utils.h"
#include "dump.h"

#define MEMLOG_FILE_NUMBER     10

// The belows are likely to be modified,
// as the dump region has not been dedicated yet
#define DUMP_ADDR 0xC4800000
#define DUMP_SIZE 0x400000

struct device camellia_coredump_memlog_dev;
struct memlog *camellia_coredump_memlog_desc;
struct memlog_obj *camellia_coredump_memlog_obj;
struct memlog_obj *camellia_coredump_memlog_file;
void *coredump_base_vaddr;

static const char dump_device_name[] = "camellia_dump";

static struct miscdevice dump_dev;

static void camellia_coredump_work(struct work_struct *work)
{
	int ret = 0;

	if (camellia_coredump_memlog_file && coredump_base_vaddr) {
		ret = memlog_write_file(camellia_coredump_memlog_file, coredump_base_vaddr, DUMP_SIZE);
		if (ret < 0) {
			LOG_ERR("Failed to write mem log file, ret: %d", ret);
			return;
		}
	} else {
		LOG_ERR("coredump memlog file does not exist or base address not set");
	}
}

int camellia_coredump_memlog_register(void)
{
	int ret;

	device_initialize(&camellia_coredump_memlog_dev);
	ret = memlog_register("camellia_coredump", &camellia_coredump_memlog_dev, &camellia_coredump_memlog_desc);
	if (ret) {
		LOG_ERR("Failed to register memlog, ret:%d", ret);
		return ret;
	}

	camellia_coredump_memlog_file =
			memlog_alloc_file(camellia_coredump_memlog_desc, "camellia-coredump-file", SZ_512K * 8, 4
, 200, MEMLOG_FILE_NUMBER);
	if (camellia_coredump_memlog_file) {
		memlog_obj_set_sysfs_mode(camellia_coredump_memlog_file, true);
	}

	coredump_base_vaddr = camellia_request_region(DUMP_ADDR, DUMP_SIZE);

	if (coredump_base_vaddr == NULL) {
		LOG_ERR("Failed to map dump area");
	}

	camellia_dev.coredump_wq = alloc_workqueue("camellia_coredump", WQ_UNBOUND | WQ_SYSFS, 0);
	if (!camellia_dev.coredump_wq) {
		LOG_ERR("Failed to allocate workqueue");
	}
	INIT_WORK(&(camellia_dev.coredump_work.work), camellia_coredump_work);

	return ret;
}

void camellia_call_coredump_work(u8 event)
{
	mutex_lock(&camellia_dev.coredump_lock);
	if (camellia_dev.coredump_wq) {
		camellia_dev.coredump_work.event = event;
		queue_work(camellia_dev.coredump_wq, &(camellia_dev.coredump_work.work));
	}
	mutex_unlock(&camellia_dev.coredump_lock);
}

static int camellia_dump_open(struct inode *inode, struct file *file)
{
	struct camellia_client *client;

	// We assume that only the legitimate program (e.g., debug terminal) is allowed
	// for accessing /dev/camellia_iwc and /dev/camellia_dump.
	// Thus, it would be better to obtain the client with `camellia_lookup_client()`.
	// Currently, it always fails because of the nature of tokio's async function
	// where different threads having different pids are used for accessing the devices.
	// If a proper way to lookup client is found, the below could be replaced with it.
	client = camellia_create_client(task_pid_nr(current));
	if (client == NULL) {
		LOG_ERR("Failed to create client");
		return -EINVAL;
	}
	client->current_dump_offset = 0;
	// The below would map the target dram as a non-cacheable region.
	// If a cacheable region is used (e.g., phys_to_virt()),
	// the processor could read the cache instead of dram,
	// because it is not aware that the dram is written by
	// another processor (Strong) or not.
	client->dump_vbase = camellia_request_region(DUMP_ADDR, DUMP_SIZE);
	if (client->dump_vbase == NULL) {
		LOG_ERR("Failed to map dump area");
		return -EINVAL;
	}
	file->private_data = client;
	return 0;
}

static ssize_t camellia_dump_read(struct file *file_ptr,
					char __user *user_buffer, size_t cnt,
					loff_t *pos)
{
	struct camellia_client *p_client =
		(struct camellia_client *)file_ptr->private_data;
	unsigned long *target_addr =
		p_client->dump_vbase + p_client->current_dump_offset;

	LOG_ENTRY;
	LOG_INFO(
		"coredump memory (/dev/camellia_dump) is being read by %d bytes",
		cnt);
	if (copy_to_user(user_buffer, (void *)target_addr, cnt)) {
		LOG_ERR("Unable to read camellia coredump device");
		return -EFAULT;
	} else
		p_client->current_dump_offset += cnt;
	LOG_EXIT;
	return cnt;
}

static int camellia_dump_release(struct inode *inode_ptr,
					struct file *file_ptr)
{
	struct camellia_client *p_client =
		(struct camellia_client *)file_ptr->private_data;
	unsigned long *target_addr = p_client->dump_vbase;

	LOG_ENTRY;
	LOG_INFO("coredump memory (/dev/camellia_dump) is being released");
	// Should we make the area available for the next access or free it?
	// We currently assume that the next access is possible with another open.
	// Thus, memset() is not called.

	// camellia_request_region() uses vmap, which needs to be unmapped
	vunmap(target_addr);
	p_client->current_dump_offset = 0;
	p_client->dump_vbase = NULL;

	// The below could be removed if we found a proper way to use
	// for lookup client, as it would be handled altogether
	// inside camellia_release().
	camellia_destroy_client(p_client);

	LOG_EXIT;
	return 0;
}

static struct file_operations camellia_dump_fops = {
	.owner = THIS_MODULE,
	.read = camellia_dump_read,
	.open = camellia_dump_open,
	.release = camellia_dump_release,
};

int camellia_dump_init(void)
{
	int result = 0;

	dump_dev.minor = MISC_DYNAMIC_MINOR;
	dump_dev.fops = &camellia_dump_fops;
	dump_dev.name = dump_device_name;

	result = misc_register(&dump_dev);
	if (result == 0)
		LOG_DBG("Registered camellia coredump device %d", dump_dev.minor);

	return result;
}

void camellia_dump_deinit(void)
{
	LOG_DBG("Deregistered camellia coredump device %d", dump_dev.minor);
	misc_deregister(&dump_dev);
}
