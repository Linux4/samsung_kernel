/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/anon_inodes.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h>

#include "common/npu-log.h"
#include "npu-fence-sync.h"
#include "npu-fence.h"

static const struct file_operations sync_file_fops;

struct npu_sync_data {
	__u64	value;
	__u64	reserved1;
	__u64	reserved2;
};

static struct npu_sync_file *npu_sync_file_alloc(void)
{
	struct npu_sync_file *sync_file;

	sync_file = kzalloc(sizeof(*sync_file), GFP_KERNEL);
	if (!sync_file)
		return NULL;

	sync_file->file = anon_inode_getfile("sync_file", &sync_file_fops,
					     sync_file, 0);
	if (IS_ERR(sync_file->file))
		goto err;

	init_waitqueue_head(&sync_file->wq);

	INIT_LIST_HEAD(&sync_file->cb.node);

	return sync_file;

err:
	kfree(sync_file);
	return NULL;
}

struct npu_sync_file *npu_sync_file_fdget(int fd)
{
	struct file *file = fget(fd);

	if (!file)
		return NULL;

	if (file->f_op != &sync_file_fops)
		goto err;

	return file->private_data;

err:
	fput(file);
	return NULL;
}

/**
 * sync_file_get_fence - get the fence related to the sync_file fd
 * @fd:		sync_file fd to get the fence from
 *
 * Ensures @fd references a valid sync_file and returns a fence that
 * represents all fence in the sync_file. On error NULL is returned.
 */
struct dma_fence *npu_sync_file_get_fence(int fd)
{
	struct npu_sync_file *sync_file;
	struct dma_fence *fence;

	sync_file = npu_sync_file_fdget(fd);
	if (!sync_file)
		return NULL;

	fence = dma_fence_get(sync_file->fence);
	fput(sync_file->file);

	return fence;
}

struct npu_sync_file *npu_sync_file_create(struct dma_fence *fence)
{
	struct npu_sync_file *sync_file;

	sync_file = npu_sync_file_alloc();
	if (!sync_file)
		return NULL;

	sync_file->fence = dma_fence_get(fence);

	return sync_file;
}
EXPORT_SYMBOL(npu_sync_file_create);

static int npu_sync_file_release(struct inode *inode, struct file *file)
{
	struct npu_sync_file *sync_file = file->private_data;

	if (!list_empty(&sync_file->cb.node))
		dma_fence_remove_callback(sync_file->fence, &sync_file->cb);
	dma_fence_put(sync_file->fence);
	kfree(sync_file);

	return 0;
}

static __poll_t npu_sync_file_poll(struct file *file, poll_table *wait)
{
	struct npu_sync_file *sync_file = file->private_data;

	poll_wait(file, &sync_file->wq, wait);

	return dma_fence_is_signaled(sync_file->fence) ? EPOLLIN : 0;
}

static int npu_sync_file_update(struct npu_sync_file *sync_file, unsigned long arg)
{
	int ret = 0;
	struct npu_sync_timeline *obj;
	struct npu_sync_pt *pt;
	struct dma_fence *fence;
	struct npu_sync_data data;

	if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
		return -EFAULT;
	}

	fence = dma_fence_get(sync_file->fence);
	obj = npu_dma_fence_parent(fence);
	pt = npu_dma_fence_to_sync_pt(fence);
	npu_sync_pt_update(obj, pt, data.value);

	dma_fence_add_callback(sync_file->fence, &sync_file->cb, npu_sync_pt_in_cb_func);

	return ret;
}

static long npu_sync_file_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	int ret = 0;
	struct npu_sync_file *sync_file = file->private_data;

	switch (cmd) {
	case SYNC_IOC_UPDATE_PT:
		ret = npu_sync_file_update(sync_file, arg);
		break;

	default:
		npu_err("sync file ioctl(%u) is not supported(usr arg: %lx)\n", cmd, arg);
		ret = -ENOTTY;
	}

	return ret;
}

static const struct file_operations sync_file_fops = {
	.release = npu_sync_file_release,
	.poll = npu_sync_file_poll,
	.unlocked_ioctl = npu_sync_file_ioctl,
	.compat_ioctl = npu_sync_file_ioctl,
};
