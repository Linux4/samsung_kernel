/* sound/soc/samsung/vts/vts_pcm_dump.c
 *
 * ALSA SoC Audio Layer - Samsung VTS Internal Buffer Dumping driver
 *
 * Copyright (c) 2019 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <sound/samsung/abox.h>
#include <sound/samsung/vts.h>

#include "vts.h"
#include "vts_log.h"

#define BUFFER_MAX (SZ_64)
#define NAME_LENGTH (SZ_32)

struct vts_pcm_dump_info {
	struct device *dev;
	struct list_head list;
	int id;
	char name[NAME_LENGTH];
	struct mutex lock;
	struct snd_dma_buffer buffer;
	size_t pointer;

	struct dentry *file;
	bool file_started;
	size_t file_pointer;
	wait_queue_head_t file_waitqueue;

	bool file_created;
	struct file *filp;
};

static struct dentry *dir_dump;
static struct device *vts_pcm_dump_dev_vts;
static struct vts_pcm_dump_info vts_pcm_dump_list[BUFFER_MAX];
static LIST_HEAD(vts_pcm_dump_list_head);

static struct vts_pcm_dump_info *vts_pcm_dump_get_info(int id)
{
	struct vts_pcm_dump_info *info;

	list_for_each_entry(info, &vts_pcm_dump_list_head, list) {
		if (info->id == id)
			return info;
	}

	return NULL;
}

static void vts_pcm_dump_request_dump(int id, int set)
{
	u32 values[3] = {0,0,0};
	struct vts_data *data = dev_get_drvdata(vts_pcm_dump_dev_vts);

	dev_dbg(vts_pcm_dump_dev_vts, "%s(%d)\n", __func__, id);
	if(id == 0) {
		if(set == 1)
			vts_start_ipc_transaction(vts_pcm_dump_dev_vts, data, VTS_IRQ_AP_START_REC, &values, 1, 1);
		else
			vts_start_ipc_transaction(vts_pcm_dump_dev_vts, data, VTS_IRQ_AP_STOP_REC, &values, 1, 1);
	}
}

static ssize_t vts_pcm_dump_file_read(struct file *file, char __user *data,
		size_t count, loff_t *ppos)
{
	struct vts_pcm_dump_info *info = file->private_data;
	struct device *dev = info->dev;
	size_t end, pointer;
	ssize_t size;
	int ret;

	dev_dbg(dev, "%s(%#zx)\n", __func__, count);

	do {
		pointer = READ_ONCE(info->pointer);
		end = (info->file_pointer <= pointer) ? pointer :
				info->buffer.bytes;
		size = min(end - info->file_pointer, count);
		dev_dbg(dev, "pointer=%#zx file_pointer=%#zx size=%#zx\n",
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

static int vts_pcm_dump_file_open(struct inode *i, struct file *f)
{
	struct vts_pcm_dump_info *info = i->i_private;
	struct device *dev = info->dev;

	dev_dbg(dev, "%s\n", __func__);
	f->private_data = info;
	info->file_started = true;
	info->file_pointer = 0;

	vts_pcm_dump_request_dump(info->id, 1);
	return 0;
}

static int vts_pcm_dump_file_release(struct inode *i, struct file *f)
{
	struct vts_pcm_dump_info *info = i->i_private;
	struct device *dev = info->dev;

	dev_dbg(dev, "%s\n", __func__);
	info->file_started = false;

	vts_pcm_dump_request_dump(info->id, 0);
	return 0;
}

static unsigned int vts_pcm_dump_file_poll(struct file *file, poll_table *wait)
{
	struct vts_pcm_dump_info *info = file->private_data;

	dev_dbg(info->dev, "%s\n", __func__);

	poll_wait(file, &info->file_waitqueue, wait);
	return POLLIN | POLLRDNORM;
}

static const struct file_operations vts_pcm_dump_fops = {
	.llseek = generic_file_llseek,
	.read = vts_pcm_dump_file_read,
	.poll = vts_pcm_dump_file_poll,
	.open = vts_pcm_dump_file_open,
	.release = vts_pcm_dump_file_release,
	.owner = THIS_MODULE,
};

void vts_pcm_dump_period_elapsed(int id, size_t pointer)
{
	struct vts_pcm_dump_info *info = vts_pcm_dump_get_info(id);
	struct device *dev = info->dev;

	dev_dbg(dev, "%s[%d](%zx)\n", __func__, id, pointer);

	info->pointer = pointer;
	wake_up_interruptible(&info->file_waitqueue);
}

void vts_pcm_dump_transfer(int id, const char *buf, size_t bytes)
{
	struct vts_pcm_dump_info *info = vts_pcm_dump_get_info(id);
	struct device *dev = info->dev;
	size_t size, pointer;

	dev_dbg(dev, "%s[%d](%pK, %zx): %zx\n", __func__, id, buf, bytes,
			info->pointer);

	size = min(bytes, info->buffer.bytes - info->pointer);
	memcpy(info->buffer.area + info->pointer, buf, size);
	if (bytes - size > 0)
		memcpy(info->buffer.area, buf + size, bytes - size);

	pointer = (info->pointer + bytes) % info->buffer.bytes;
	vts_pcm_dump_period_elapsed(id, pointer);
}


static void vts_pcm_dump_check_buffer(struct snd_dma_buffer *buffer)
{
	if (!buffer->bytes)
		buffer->bytes = SZ_64K;
	if (!buffer->area) {
		/* area will be used in kernel only.
		 * kmalloc and virt_to_phys are just enough.
		 */
		buffer->area = devm_kmalloc(vts_pcm_dump_dev_vts, buffer->bytes,
				GFP_KERNEL);
		buffer->addr = virt_to_phys(buffer->area);
	}
}

struct dentry *vts_pcm_dump_register_file(const char *name, void *data,
		const struct file_operations *fops)
{
	return debugfs_create_file(name, 0664, dir_dump, data, fops);
}

void vts_pcm_dump_unregister_file(struct dentry *file)
{
	debugfs_remove(file);
}

void vts_pcm_dump_register_work_func(struct work_struct *work)
{
	int id;
	struct vts_pcm_dump_info *info;

	for (info = &vts_pcm_dump_list[0]; (info - &vts_pcm_dump_list[0]) <
			ARRAY_SIZE(vts_pcm_dump_list); info++) {
		id = info->id;
		if (info->dev && !vts_pcm_dump_get_info(id)) {
			vts_pcm_dump_check_buffer(&info->buffer);
			dev_dbg(info->dev, "%s(%d, %s, %#zx)\n", __func__,
					id, info->name, info->buffer.bytes);
			info->file = vts_pcm_dump_register_file(info->name,
				info, &vts_pcm_dump_fops);
			init_waitqueue_head(&info->file_waitqueue);
			list_add_tail(&info->list, &vts_pcm_dump_list_head);
		}
	}
}

static DECLARE_WORK(vts_pcm_dump_register_work,
	vts_pcm_dump_register_work_func);

int vts_pcm_dump_register(struct device *dev, int id, const char *name,
		void *area, phys_addr_t addr, size_t bytes)
{
	struct vts_pcm_dump_info *info;

	dev_dbg(dev, "%s[%d](%s, %#zx)\n", __func__, id, name, bytes);

	if (id < 0 || id >= ARRAY_SIZE(vts_pcm_dump_list)) {
		dev_err(dev, "invalid id: %d\n", id);
		return -EINVAL;
	}

	info = &vts_pcm_dump_list[id];
	if (!strcmp(info->name, name)) {
		dev_dbg(dev, "already registered dump: %d\n", id);
		return 0;
	}

	mutex_init(&info->lock);
	info->id = id;
	strncpy(info->name, name, sizeof(info->name) - 1);
	info->buffer.area = area;
	info->buffer.addr = addr;
	info->buffer.bytes = bytes;
	vts_pcm_dump_dev_vts = info->dev = dev;
	schedule_work(&vts_pcm_dump_register_work);

	return 0;
}

void vts_pcm_dump_init(struct device *dev_vts)
{
	struct dentry *dbg_dir = vts_dbg_get_root_dir();

	dev_dbg(dev_vts, "%s\n", __func__);

	vts_pcm_dump_dev_vts = dev_vts;
	if (IS_ERR_OR_NULL(dir_dump))
		dir_dump = debugfs_create_dir("dump", dbg_dir);
}

bool vts_pcm_dump_get_file_started(int id)
{
	struct vts_pcm_dump_info *info = vts_pcm_dump_get_info(id);

	return info->file_started;
}
