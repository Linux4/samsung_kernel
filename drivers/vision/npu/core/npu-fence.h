/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _NPU_FENCE_H_
#define _NPU_FENCE_H_

#include <linux/dma-fence.h>

#define FENCE_NUM_DEVICES	(256)
#define FENCE_MAJOR	(83)
#define FENCE_MINOR	(11)
#define FENCE_NAME "npufence"

struct npu_fence_file_ops {
	struct module *owner;
	int (*open)(struct file *file);
	int (*release)(struct file *file);
	long (*ioctl)(struct file *file, unsigned int cmd, unsigned long arg);
	long (*compat_ioctl)(struct file *file, unsigned int cmd, unsigned long arg);
	int (*flush)(struct file *file);
};

struct npu_fence_device {
	int minor;
	int index;
	int type;
	char name[16];
	unsigned long flags;

	/* device ops */
	const struct npu_fence_file_ops	*fops;

	/* sysfs */
	struct device dev;
	struct cdev *cdev;

	/* for sync CMDQ */
	struct npu_system *system;
};

struct npu_sync_timeline {
	struct kref kref;
	char name[32];

	/* protected by lock */
	u64 context;
	int value;

	struct rb_root pt_tree;
	struct list_head pt_list;
	spinlock_t lock;

	struct list_head sync_timeline_list;
};

struct npu_sync_pt {
	struct dma_fence base;
	struct list_head link;
	struct rb_node node;
	u32 timestamp;
};

static inline struct npu_sync_timeline *npu_dma_fence_parent(struct dma_fence *fence)
{
	return container_of(fence->lock, struct npu_sync_timeline, lock);
}

int npu_fence_probe(struct npu_fence_device *fence, struct npu_system *npu_system);
void npu_fence_remove(struct npu_fence_device *fence);
int npu_sync_inc_fence(struct npu_sync_timeline *obj, unsigned long arg);
int npu_sync_create_out_fence(struct npu_sync_timeline *obj, int value);
int npu_sync_remove_out_fence(struct npu_sync_timeline *obj, int fd);
void npu_sync_update_out_fence(struct npu_sync_timeline *obj, int value, int fd);
void npu_sync_timeline_signal(struct npu_sync_timeline *obj, unsigned int inc);
struct npu_sync_timeline *npu_sync_timeline_create(const char *name);
inline struct npu_sync_pt *npu_dma_fence_to_sync_pt(struct dma_fence *fence);
void npu_sync_pt_in_cb_func(struct dma_fence *f, struct dma_fence_cb *cb);
struct npu_sync_timeline *npu_sync_get_in_fence(int fd);
void npu_sync_pt_out_cb_func(struct dma_fence *f, struct dma_fence_cb *cb);
void npu_sync_pt_update(struct npu_sync_timeline *obj, struct npu_sync_pt *pt, unsigned int value);
u64 npu_get_sync_pt_timestamp(int fd);

#define SYNC_IOC_CREATE_FENCE			_IOW('V', 0, struct npu_fence_data)
#define SYNC_IOC_INC_FENCE				_IOW('V', 1, struct npu_fence_data)

#endif // _NPU_FENCE_H_
