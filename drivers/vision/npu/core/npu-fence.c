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


#include <linux/string.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rbtree.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#include "common/npu-log.h"
#include "npu-fence-sync.h"
#include "npu-device.h"
#include "npu-util-regs.h"
#include "npu-fence.h"

static struct npu_fence_device *fence_device[FENCE_NUM_DEVICES];
static const struct dma_fence_ops timeline_fence_ops;

struct npu_fence_data {
	__u64	value;
	char	name[32] __attribute__((aligned(8)));
	__s64	fence; /* fd of new fence */
	__u64	reserved1;
	__u64	reserved2;
};

inline struct npu_sync_pt *npu_dma_fence_to_sync_pt(struct dma_fence *fence)
{
	if (fence->ops != &timeline_fence_ops)
		return NULL;
	return container_of(fence, struct npu_sync_pt, base);
}

static void npu_sync_timeline_free(struct kref *kref)
{
	struct npu_sync_timeline *obj = container_of(kref, struct npu_sync_timeline, kref);

	// sync_timeline_debug_remove(obj);

	kfree(obj);
}

struct npu_sync_timeline *npu_sync_timeline_create(const char *name)
{
	struct npu_sync_timeline *obj;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return NULL;

	kref_init(&obj->kref);
	obj->context = dma_fence_context_alloc(1);
	strlcpy(obj->name, name, sizeof(obj->name));

	obj->pt_tree = RB_ROOT;
	INIT_LIST_HEAD(&obj->pt_list);
	spin_lock_init(&obj->lock);

	// sync_timeline_debug_add(obj);

	return obj;
}

static void npu_sync_timeline_get(struct npu_sync_timeline *obj)
{
	kref_get(&obj->kref);
}

static void npu_sync_timeline_put(struct npu_sync_timeline *obj)
{
	kref_put(&obj->kref, npu_sync_timeline_free);
}

static const char *npu_timeline_fence_get_driver_name(struct dma_fence *fence)
{
	return "npu_fence";
}

static const char *npu_timeline_fence_get_timeline_name(struct dma_fence *fence)
{
	struct npu_sync_timeline *parent = npu_dma_fence_parent(fence);

	return parent->name;
}

static void npu_timeline_fence_release(struct dma_fence *fence)
{
	struct npu_sync_pt *pt = npu_dma_fence_to_sync_pt(fence);
	struct npu_sync_timeline *parent = npu_dma_fence_parent(fence);
	unsigned long flags;

	spin_lock_irqsave(fence->lock, flags);
	if (!list_empty(&pt->link)) {
		list_del(&pt->link);
		rb_erase(&pt->node, &parent->pt_tree);
	}
	spin_unlock_irqrestore(fence->lock, flags);

	npu_sync_timeline_put(parent);
	dma_fence_free(fence);
}

static bool npu_timeline_fence_signaled(struct dma_fence *fence)
{
	struct npu_sync_timeline *parent = npu_dma_fence_parent(fence);

	return !__dma_fence_is_later(fence->seqno, parent->value, fence->ops);
}

static bool npu_timeline_fence_enable_signaling(struct dma_fence *fence)
{
	return true;
}

static void npu_timeline_fence_value_str(struct dma_fence *fence,
				    char *str, int size)
{
	snprintf(str, size, "%lld", fence->seqno);
}

static void npu_timeline_fence_timeline_value_str(struct dma_fence *fence,
					     char *str, int size)
{
	struct npu_sync_timeline *parent = npu_dma_fence_parent(fence);

	snprintf(str, size, "%d", parent->value);
}

static const struct dma_fence_ops timeline_fence_ops = {
	.get_driver_name = npu_timeline_fence_get_driver_name,
	.get_timeline_name = npu_timeline_fence_get_timeline_name,
	.enable_signaling = npu_timeline_fence_enable_signaling,
	.signaled = npu_timeline_fence_signaled,
	.release = npu_timeline_fence_release,
	.fence_value_str = npu_timeline_fence_value_str,
	.timeline_value_str = npu_timeline_fence_timeline_value_str,
};

u64 npu_get_sync_pt_timestamp(int fd)
{
	struct npu_sync_pt *pt;
	struct dma_fence *fence;

	fence = npu_sync_file_get_fence(fd);
	pt = npu_dma_fence_to_sync_pt(fence);

	return pt->timestamp;
}

void npu_sync_timeline_signal(struct npu_sync_timeline *obj, unsigned int inc)
{
	struct npu_sync_pt *pt, *next;

	// trace_sync_timeline(obj);

	spin_lock_irq(&obj->lock);

	obj->value = inc;

	list_for_each_entry_safe(pt, next, &obj->pt_list, link) {
		if (!npu_timeline_fence_signaled(&pt->base))
			break;

		list_del_init(&pt->link);
		rb_erase(&pt->node, &obj->pt_tree);

		/*
		 * A signal callback may release the last reference to this
		 * fence, causing it to be freed. That operation has to be
		 * last to avoid a use after free inside this loop, and must
		 * be after we remove the fence from the timeline in order to
		 * prevent deadlocking on timeline->lock inside
		 * timeline_fence_release().
		 */
		dma_fence_signal_locked(&pt->base);
	}

	spin_unlock_irq(&obj->lock);
}

void npu_sync_pt_in_cb_func(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct npu_system *npu_system = fence_device[FENCE_MINOR]->system;
	struct npu_device *device;
	struct npu_sync_timeline *in_fence;
	struct npu_sync_pt *pt;
	struct npu_iomem_area *gnpu0;
	struct npu_iomem_area *gnpu1;
	int i = 0, uid = 0;

	in_fence = npu_dma_fence_parent(f);
	pt = npu_dma_fence_to_sync_pt(f);
	device = container_of(npu_system, struct npu_device, system);

	pt->timestamp = npu_cmd_map(&device->system, "fwpwm");

	gnpu0 = npu_get_io_area(npu_system, "sfrgnpu0");
	gnpu1 = npu_get_io_area(npu_system, "sfrgnpu1");

	mutex_lock(&device->sessionmgr.mlock);
	for (i = 0; i < NPU_MAX_SESSION; i++) {
		if ((device->sessionmgr.session[i] != NULL) && (device->sessionmgr.session[i]->in_fence == in_fence)) {
			uid = device->sessionmgr.session[i]->uid;
			break;
		}
	}
	mutex_unlock(&device->sessionmgr.mlock);

	// while(npu_read_hw_reg(gnpu0, 0x20A0, 0xFFFFFFFF, 1) & (1 << uid));
	// while(npu_read_hw_reg(gnpu1, 0x20A0, 0xFFFFFFFF, 1) & (1 << uid));

	npu_write_hw_reg(gnpu0, 0x20A4, 1 << uid, 0xFFFFFFFF, 0);
	npu_write_hw_reg(gnpu1, 0x20A4, 1 << uid, 0xFFFFFFFF, 0);

}

void npu_sync_pt_update(struct npu_sync_timeline *obj, struct npu_sync_pt *pt, unsigned int value)
{
	npu_sync_timeline_get(obj);
	dma_fence_init(&pt->base, &timeline_fence_ops, &obj->lock,
		       obj->context, value);
	INIT_LIST_HEAD(&pt->link);

	spin_lock_irq(&obj->lock);
	if (!dma_fence_is_signaled_locked(&pt->base)) {
		struct rb_node **p = &obj->pt_tree.rb_node;
		struct rb_node *parent = NULL;

		while (*p) {
			struct npu_sync_pt *other;
			int cmp;

			parent = *p;
			other = rb_entry(parent, typeof(*pt), node);
			cmp = value - other->base.seqno;

			if (cmp > 0) {
				p = &parent->rb_right;
			} else if (cmp < 0) {
				p = &parent->rb_left;
			} else {
				if (dma_fence_get_rcu(&other->base)) {
					npu_sync_timeline_put(obj);
					// kfree(pt);
					// pt = other;
					goto unlock;
				}
				p = &parent->rb_left;
			}
		}
		rb_link_node(&pt->node, parent, p);
		rb_insert_color(&pt->node, &obj->pt_tree);
		parent = rb_next(&pt->node);
		list_add_tail(&pt->link,
			      parent ? &rb_entry(parent, typeof(*pt), node)->link : &obj->pt_list);
	}

unlock:
	spin_unlock_irq(&obj->lock);
}

void npu_sync_pt_out_cb_func(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct npu_sync_file *sync_file;

	sync_file = container_of(cb, struct npu_sync_file, cb);
	wake_up(&sync_file->wq);
}


void npu_sync_update_out_fence(struct npu_sync_timeline *obj, int value, int fd)
{
	struct npu_sync_file *sync_file;
	struct npu_sync_pt *pt;
	struct dma_fence *fence;

	fence = npu_sync_file_get_fence(fd);
	pt = npu_dma_fence_to_sync_pt(fence);
	sync_file = npu_sync_file_fdget(fd);

	npu_sync_pt_update(obj, pt, value);

	dma_fence_add_callback(sync_file->fence, &sync_file->cb, npu_sync_pt_out_cb_func);
}

static struct npu_sync_pt *npu_sync_pt_create(struct npu_sync_timeline *obj,
				      unsigned int value)
{
	struct npu_sync_pt *pt;

	pt = kzalloc(sizeof(*pt), GFP_KERNEL);
	if (!pt)
		return NULL;

	npu_sync_pt_update(obj, pt, value);

	return pt;
}

struct npu_sync_timeline *npu_sync_get_in_fence(int fd)
{
	struct dma_fence *fence = NULL;

	fence = npu_sync_file_get_fence(fd);
	return npu_dma_fence_parent(fence);
}

static int npu_sync_create_in_fence(struct npu_sync_timeline *obj, unsigned long arg)
{
	int ret = 0;
	int fd = get_unused_fd_flags(O_CLOEXEC);
	struct npu_sync_pt *pt;
	struct npu_sync_file *sync_file;
	struct npu_fence_data data;

	if (fd < 0)
		return fd;

	if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
		ret = -EFAULT;
		goto err;
	}

	pt = npu_sync_pt_create(obj, data.value);
	if (!pt) {
		ret = -ENOMEM;
		goto err;
	}

	sync_file = npu_sync_file_create(&pt->base);
	dma_fence_put(&pt->base);
	if (!sync_file) {
		ret = -ENOMEM;
		goto err;
	}

	dma_fence_add_callback(sync_file->fence, &sync_file->cb, npu_sync_pt_in_cb_func);

	data.fence = fd;
	if (copy_to_user((void __user *)arg, &data, sizeof(data))) {
		fput(sync_file->file);
		ret = -EFAULT;
		goto err;
	}

	fd_install(fd, sync_file->file);
	return ret;

err:
	put_unused_fd(fd);
	return ret;
}

int npu_sync_create_out_fence(struct npu_sync_timeline *obj, int value)
{
	int ret = 0;
	int fd = get_unused_fd_flags(O_CLOEXEC);
	struct npu_sync_pt *pt;
	struct npu_sync_file *sync_file;

	if (fd < 0)
		return fd;

	pt = npu_sync_pt_create(obj, value);
	if (!pt) {
		ret = -ENOMEM;
		goto err;
	}

	sync_file = npu_sync_file_create(&pt->base);
	dma_fence_put(&pt->base);
	if (!sync_file) {
		ret = -ENOMEM;
		goto err;
	}

	dma_fence_add_callback(sync_file->fence, &sync_file->cb, npu_sync_pt_out_cb_func);

	fd_install(fd, sync_file->file);
	return fd;

err:
	put_unused_fd(fd);
	return ret;
}

int npu_sync_inc_fence(struct npu_sync_timeline *obj, unsigned long arg)
{
	u32 value;

	if (copy_from_user(&value, (void __user *)arg, sizeof(value)))
		return -EFAULT;

	while (value > INT_MAX)  {
		npu_sync_timeline_signal(obj, INT_MAX);
		value -= INT_MAX;
	}

	npu_sync_timeline_signal(obj, value);

	return 0;
}

static int npu_fence_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	// struct fence_device *fence = fence_device[iminor(file_inode(file))];
	struct npu_sync_timeline *obj;

	obj = npu_sync_timeline_create("enn_in_fence");
	if (!obj)
		return -ENOMEM;

	file->private_data = obj;

	return ret;
}

static int npu_fence_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	// struct fence_device *fence = fence_device[iminor(file_inode(file))];
	struct npu_sync_timeline *obj;
	// struct npu_sync_pt *pt, *next;

	obj = file->private_data;

	// spin_lock_irq(&obj->lock);

	// list_for_each_entry_safe(pt, next, &obj->pt_list, link) {
	// 	dma_fence_set_error(&pt->base, -ENOENT);
	// 	dma_fence_signal_locked(&pt->base);
	// }

	// spin_unlock_irq(&obj->lock);
	npu_sync_timeline_put(obj);

	return ret;
}

static long npu_fence_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct npu_sync_timeline *obj = file->private_data;

	switch (cmd) {
	case SYNC_IOC_CREATE_FENCE:
		ret = npu_sync_create_in_fence(obj, arg);
		break;

	case SYNC_IOC_INC_FENCE:
		ret = npu_sync_inc_fence(obj, arg);
		break;

	default:
		npu_err("fence ioctl(%u) is not supported(usr arg: %lx)\n", cmd, arg);
		ret = -ENOTTY;
	}

	return ret;
}

static unsigned int npu_fence_poll(struct file *file, struct poll_table_struct *poll)
{
	return EPOLLIN;
}

static const struct file_operations fence_fops = {
	.owner = THIS_MODULE,
	.open = npu_fence_open,
	.release = npu_fence_release,
	.unlocked_ioctl = npu_fence_ioctl,
	.compat_ioctl = npu_fence_ioctl,
	.poll = npu_fence_poll,
};

struct class npu_fence_class = {
	.name = FENCE_NAME,
};

int npu_fence_probe(struct npu_fence_device *fence, struct npu_system *npu_system)
{
	int ret = 0;
	dev_t dev_f = MKDEV(FENCE_MAJOR, 0);

	ret = register_chrdev_region(dev_f, FENCE_NUM_DEVICES, FENCE_NAME);
	if (ret < 0) {
		probe_err("complete: can't register chrdev\n");
		return ret;
	}

	ret = class_register(&npu_fence_class);
	if (ret < 0) {
		unregister_chrdev_region(dev_f, FENCE_NUM_DEVICES);
		probe_err("video_dev: class_register failed(%d)\n", ret);
		return -EIO;
	}

	fence->cdev = cdev_alloc();
	if (fence->cdev == NULL) {
		ret = -ENOMEM;
		return ret;
	}

	fence->cdev->ops = &fence_fops;
	ret = cdev_add(fence->cdev, MKDEV(FENCE_MAJOR, FENCE_MINOR), 1);
	if (ret < 0) {
		probe_err("cdev_add failed (%d)\n", ret);
		goto cdev_add_err;
	}

	fence->dev.class = &npu_fence_class;
	fence->dev.devt = MKDEV(FENCE_MAJOR, FENCE_MINOR);
	dev_set_name(&fence->dev, "%s%d", "vertex", FENCE_MINOR);
	ret = device_register(&fence->dev);
	if (ret < 0) {
		probe_err("device_register failed (%d)\n", ret);
		goto cleanup;
	}

	fence_device[FENCE_MINOR] = fence;
	fence_device[FENCE_MINOR]->system = npu_system;

	return ret;

cleanup:
	cdev_del(fence->cdev);

cdev_add_err:
	kfree(fence->cdev);

	return ret;
}
EXPORT_SYMBOL(npu_fence_probe);

void npu_fence_remove(struct npu_fence_device *fence)
{
	dev_t dev_f = MKDEV(FENCE_MAJOR, 0);

	device_unregister(&fence->dev);

	if (fence->cdev) {
		cdev_del(fence->cdev);
		kfree(fence->cdev);
	}

	class_unregister(&npu_fence_class);
	unregister_chrdev_region(dev_f, FENCE_NUM_DEVICES);
}
