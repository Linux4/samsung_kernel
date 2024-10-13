// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ALSA SoC - Samsung Abox Internal Buffer Dumping driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/pm_runtime.h>
#include <linux/sched/clock.h>
#include <sound/samsung/abox.h>

#include "abox_util.h"
#include "abox.h"
#include "abox_proc.h"
#include "abox_log.h"
#include "abox_dump.h"
#include "abox_memlog.h"

#define NAME_LENGTH (SZ_32)

struct abox_dump_info {
	struct device *dev;
	struct list_head list;
	int gid;
	int id;
	char name[NAME_LENGTH];
	bool registered;
	struct mutex lock;
	struct snd_dma_buffer buffer;
	struct snd_pcm_substream *substream;
	size_t pointer;
	bool started;

	struct proc_dir_entry *file;
	bool file_started;
	size_t file_pointer;
	wait_queue_head_t file_waitqueue;
};

static struct proc_dir_entry *dir_dump;
static struct device *abox_dump_dev_abox;
static LIST_HEAD(abox_dump_list_head);
static DEFINE_SPINLOCK(abox_dump_lock);

static struct abox_dump_info *abox_dump_get_info(int id)
{
	struct abox_dump_info *info;

	list_for_each_entry(info, &abox_dump_list_head, list) {
		if (info->id == id)
			return info;
	}

	return NULL;
}

static void abox_dump_request_dump(int id)
{
	struct abox_dump_info *info = abox_dump_get_info(id);
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system = &msg.msg.system;
	bool start = info->started || info->file_started;

	abox_dbg(info->dev, "%s(%d)\n", __func__, id);

	msg.ipcid = IPC_SYSTEM;
	system->msgtype = ABOX_REQUEST_DUMP;
	system->param1 = info->id;
	system->param2 = start ? 1 : 0;
	system->param3 = info->gid;
	abox_request_ipc(abox_dump_dev_abox, msg.ipcid, &msg, sizeof(msg),
			1, 0);
}

static ssize_t abox_dump_file_read(struct file *file, char __user *data,
		size_t count, loff_t *ppos)
{
	struct abox_dump_info *info = file->private_data;
	struct device *dev = info->dev;
	size_t end, pointer;
	ssize_t size;
	int ret;

	abox_dbg(dev, "%s(%#zx)\n", __func__, count);

	do {
		pointer = READ_ONCE(info->pointer);
		end = (info->file_pointer <= pointer) ? pointer :
				info->buffer.bytes;
		size = min(end - info->file_pointer, count);
		abox_dbg(dev, "pointer=%#zx file_pointer=%#zx size=%#zx\n",
				pointer, info->file_pointer, size);
		if (!size) {
			if (file->f_flags & O_NONBLOCK)
				return -EAGAIN;

			ret = wait_event_interruptible(info->file_waitqueue,
					pointer != READ_ONCE(info->pointer));
			if (ret < 0)
				return ret;
		}
	} while (!size);

	if (copy_to_user(data, info->buffer.area + info->file_pointer, size))
		return -EFAULT;

	info->file_pointer += size;
	info->file_pointer %= info->buffer.bytes;

	return size;
}

static int abox_dump_file_open(struct inode *i, struct file *f)
{
	struct abox_dump_info *info = abox_dump_get_data(f);
	struct device *dev = info->dev;

	abox_dbg(dev, "%s\n", __func__);

	if (info->file_started)
		return -EBUSY;

	pm_runtime_get(dev);

	f->private_data = info;
	info->file_started = true;
	info->pointer = info->file_pointer = 0;
	abox_dump_request_dump(info->id);

	return 0;
}

static int abox_dump_file_release(struct inode *i, struct file *f)
{
	struct abox_dump_info *info = f->private_data;
	struct device *dev = info->dev;

	abox_dbg(dev, "%s\n", __func__);

	info->file_started = false;
	abox_dump_request_dump(info->id);

	pm_runtime_put(dev);

	return 0;
}

static unsigned int abox_dump_file_poll(struct file *file, poll_table *wait)
{
	struct abox_dump_info *info = file->private_data;

	abox_dbg(info->dev, "%s\n", __func__);

	poll_wait(file, &info->file_waitqueue, wait);
	return POLLIN | POLLRDNORM;
}

static const struct proc_ops abox_dump_fops = {
	.proc_lseek = generic_file_llseek,
	.proc_read = abox_dump_file_read,
	.proc_poll = abox_dump_file_poll,
	.proc_open = abox_dump_file_open,
	.proc_release = abox_dump_file_release,
};

void abox_dump_period_elapsed(int id, size_t pointer)
{
	struct abox_dump_info *info = abox_dump_get_info(id);
	struct device *dev = info->dev;

	abox_dbg(dev, "%s[%d](%zx)\n", __func__, id, pointer);

	info->pointer = pointer;
	if (info->file_started)
		wake_up_interruptible(&info->file_waitqueue);
}

struct proc_dir_entry *abox_dump_register_file(const char *name, void *data,
		const struct proc_ops *fops)
{
	return abox_proc_create_file(name, 0664, dir_dump, fops, data, 0);
}

void abox_dump_unregister_file(struct proc_dir_entry *file)
{
	abox_proc_remove_file(file);
}

void *abox_dump_get_data(const struct file *file)
{
	return abox_proc_data(file);
}

static int abox_dump_register_work_single(void)
{
	struct abox_dump_info *info, *_info;
	unsigned long flags;

	abox_dbg(abox_dump_dev_abox, "%s\n", __func__);

	spin_lock_irqsave(&abox_dump_lock, flags);
	list_for_each_entry_reverse(_info, &abox_dump_list_head, list) {
		if (!_info->registered)
			break;
	}
	spin_unlock_irqrestore(&abox_dump_lock, flags);

	if (&_info->list == &abox_dump_list_head)
		return -EINVAL;

	info = devm_kmemdup(_info->dev, _info, sizeof(*_info), GFP_KERNEL);
	if (!info) {
		abox_err(abox_dump_dev_abox, "Memory allocation failed!\n");
		return -ENOMEM;
	}
	abox_info(info->dev, "%s(%d, %s, %#zx)\n", __func__, info->id,
			info->name, info->buffer.bytes);
	info->file = abox_dump_register_file(info->name, info, &abox_dump_fops);
	init_waitqueue_head(&info->file_waitqueue);
	info->registered = true;

	spin_lock_irqsave(&abox_dump_lock, flags);
	list_replace(&_info->list, &info->list);
	spin_unlock_irqrestore(&abox_dump_lock, flags);

	platform_device_register_data(info->dev, "abox-dump",
			info->id, NULL, 0);

	kfree(_info);
	return 0;
}

static void abox_dump_register_work_func(struct work_struct *work)
{
	abox_dbg(abox_dump_dev_abox, "%s\n", __func__);

	do {} while (abox_dump_register_work_single() >= 0);
}

static DECLARE_WORK(abox_dump_register_work, abox_dump_register_work_func);

int abox_dump_register(struct abox_data *data, int gid, int id,
		const char *name, void *area, phys_addr_t addr, size_t bytes)
{
	struct device *dev = data->dev;
	struct abox_dump_info *info;
	unsigned long flags;

	abox_dbg(dev, "%s[%d](%s, %#zx)\n", __func__, id, name, bytes);

	spin_lock_irqsave(&abox_dump_lock, flags);
	info = abox_dump_get_info(id);
	spin_unlock_irqrestore(&abox_dump_lock, flags);
	if (info) {
		abox_dbg(dev, "already registered dump: %d\n", id);
		return 0;
	}

	info = kzalloc(sizeof(*info), GFP_ATOMIC);
	if (!info)
		return -ENOMEM;

	mutex_init(&info->lock);
	info->gid = gid;
	info->id = id;
	strlcpy(info->name, name, sizeof(info->name));
	info->buffer.area = area;
	info->buffer.addr = addr;
	info->buffer.bytes = bytes;
	abox_dump_dev_abox = info->dev = dev;

	spin_lock_irqsave(&abox_dump_lock, flags);
	list_add_tail(&info->list, &abox_dump_list_head);
	spin_unlock_irqrestore(&abox_dump_lock, flags);

	schedule_work(&abox_dump_register_work);

	return 0;
}

static int samsung_abox_dump_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int id = to_platform_device(dev)->id;
	struct abox_dump_info *info = abox_dump_get_info(id);
	int ret = 0;

	abox_dbg(dev, "%s[%d]\n", __func__, id);

	info->dev = dev;
	pm_runtime_no_callbacks(dev);
	pm_runtime_enable(dev);

	return ret;
}

static int samsung_abox_dump_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int id = to_platform_device(dev)->id;

	abox_dbg(dev, "%s[%d]\n", __func__, id);

	return 0;
}

static void samsung_abox_dump_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	abox_dbg(dev, "%s\n", __func__);
	pm_runtime_disable(dev);
}

static const struct platform_device_id samsung_abox_dump_driver_ids[] = {
	{
		.name = "abox-dump",
	},
	{},
};
MODULE_DEVICE_TABLE(platform, samsung_abox_dump_driver_ids);

struct platform_driver samsung_abox_dump_driver = {
	.probe  = samsung_abox_dump_probe,
	.remove = samsung_abox_dump_remove,
	.shutdown = samsung_abox_dump_shutdown,
	.driver = {
		.name = "abox-dump",
		.owner = THIS_MODULE,
	},
	.id_table = samsung_abox_dump_driver_ids,
};

void abox_dump_init(struct device *dev_abox)
{
	abox_info(dev_abox, "%s\n", __func__);

	abox_dump_dev_abox = dev_abox;

	if (IS_ERR_OR_NULL(dir_dump))
		dir_dump = abox_proc_mkdir("dump", NULL);
}
