/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/atomic.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#include "core/cdev.h"
#include "core/iw_shmem.h"
#include "core/log.h"
#include "core/notifier.h"
#include "core/subsystem.h"
#include "debug/pmf.h"
#include "tzdev_internal.h"

struct tz_uiwshmem_priv {
	struct mutex mutex;
	unsigned int id;
};

static atomic_t tz_uiwshmem_device_ready = ATOMIC_INIT(0);

static int tz_uiwshmem_open(struct inode *inode, struct file *filp)
{
	struct tz_uiwshmem_priv *priv;

	(void)inode;

	priv = kmalloc(sizeof(struct tz_uiwshmem_priv), GFP_KERNEL);
	if (!priv) {
		log_error(tzdev_uiwshmem, "Failed to allocate iwshmem private data.\n");
		return -ENOMEM;
	}

	mutex_init(&priv->mutex);
	priv->id = 0;

	filp->private_data = priv;

	return 0;
}

static int tz_uiwshmem_release(struct inode *inode, struct file *filp)
{
	struct tz_uiwshmem_priv *priv;

	(void)inode;

	priv = filp->private_data;
	if (priv->id)
		tzdev_iw_shmem_release(priv->id);

	mutex_destroy(&priv->mutex);
	kfree(priv);

	return 0;
}

static long tz_uiwshmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct tz_uiwshmem_priv *priv = filp->private_data;
	struct tzio_mem_register __user *argp = (struct tzio_mem_register __user *)arg;
	struct tzio_mem_register memreg;
	unsigned int flags = 0;
	int ret = 0;

	if (cmd != TZIO_MEM_REGISTER)
		return -ENOTTY;

	if (copy_from_user(&memreg, argp, sizeof(struct tzio_mem_register)))
		return -EFAULT;

	mutex_lock(&priv->mutex);

	if (priv->id) {
		ret = -EEXIST;
		goto out;
	}

	if (memreg.write)
		flags = TZDEV_IW_SHMEM_FLAG_WRITE;

	tzdev_pmf_stamp(PMF_TZDEV_MEM_REGISTER);
	ret = tzdev_iw_shmem_register_user(PAGE_ALIGN(memreg.size), flags);
	tzdev_pmf_stamp(PMF_TZDEV_MEM_REGISTER_STOP);

	if (ret > 0)
		priv->id = ret;

out:
	mutex_unlock(&priv->mutex);

	return ret;
}

static int tz_uiwshmem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct tz_uiwshmem_priv *priv = filp->private_data;
	int ret = 0;

	if (vma->vm_pgoff)
		return -EINVAL;

	mutex_lock(&priv->mutex);

	if (!priv->id) {
		ret = -ENXIO;
		goto out;
	}

	vma->vm_flags |= VM_DONTCOPY | VM_DONTEXPAND;
	vma->vm_flags &= ~VM_MAYEXEC;

	ret = tzdev_iw_shmem_map_user(priv->id, vma);
out:
	mutex_unlock(&priv->mutex);

	return ret;
}

static const struct file_operations tz_uiwshmem_fops = {
	.owner = THIS_MODULE,
	.open = tz_uiwshmem_open,
	.release = tz_uiwshmem_release,
	.unlocked_ioctl = tz_uiwshmem_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tz_uiwshmem_ioctl,
#endif /* CONFIG_COMPAT */
	.mmap = tz_uiwshmem_mmap,
};

static struct tz_cdev tz_uiwshmem_cdev = {
	.name = "tziwshmem",
	.fops = &tz_uiwshmem_fops,
	.owner = THIS_MODULE,
};

static int tz_uiwshmem_notify_init(struct notifier_block *cb, unsigned long code, void *unused)
{
	int ret;

	(void)cb;
	(void)code;
	(void)unused;

	ret = tz_cdev_register(&tz_uiwshmem_cdev);
	if (ret) {
		log_error(tzdev_uiwshmem, "Failed to create iwshmem device, error=%d\n", ret);
		return NOTIFY_DONE;
	}

	atomic_set(&tz_uiwshmem_device_ready, 1);

	log_info(tzdev_uiwshmem, "Iwshmem user interface initialization done.\n");

	return NOTIFY_DONE;
}

static int tz_uiwshmem_notify_fini(struct notifier_block *cb, unsigned long code, void *unused)
{
	(void)cb;
	(void)code;
	(void)unused;

	if (!atomic_cmpxchg(&tz_uiwshmem_device_ready, 1, 0)) {
		log_info(tzdev_uiwshmem, "Iwshmem user interface was not initialized.\n");
		return NOTIFY_DONE;
	}

	tz_cdev_unregister(&tz_uiwshmem_cdev);

	log_info(tzdev_uiwshmem, "Iwshmem user interface finalization done.\n");

	return NOTIFY_DONE;
}

static struct notifier_block tz_uiwshmem_init_notifier = {
	.notifier_call = tz_uiwshmem_notify_init,
};

static struct notifier_block tz_uiwshmem_fini_notifier = {
	.notifier_call = tz_uiwshmem_notify_fini,
};

int tz_uiwshmem_init_call(void)
{
	int ret;

	ret = tzdev_blocking_notifier_register(TZDEV_INIT_NOTIFIER, &tz_uiwshmem_init_notifier);
	if (ret) {
		log_error(tzdev_uiwshmem, "Failed to register init notifier, error=%d\n", ret);
		return ret;
	}

	ret = tzdev_blocking_notifier_register(TZDEV_FINI_NOTIFIER, &tz_uiwshmem_fini_notifier);
	if (ret) {
		log_error(tzdev_uiwshmem, "Failed to register fini notifier, error=%d\n", ret);
		goto out_unregister;
	}

	log_info(tzdev_uiwshmem, "Iwshmem callbacks registration done\n");

	return 0;

out_unregister:
	tzdev_blocking_notifier_unregister(TZDEV_INIT_NOTIFIER, &tz_uiwshmem_init_notifier);

	return ret;
}

tzdev_early_initcall(tz_uiwshmem_init_call);
