/*
 * Copyright (C) 2012-2018 Samsung Electronics, Inc.
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

#define pr_fmt(fmt) "tz_scma_test: " fmt
#define DEBUG

#include <linux/module.h>

#include "core/cdev.h"
#include "core/subsystem.h"
#include "core/sysdep.h"

#define TZ_SCMA_SET_PHYS_ADDR 0

static int tz_scma_open(struct inode *inode, struct file *filp)
{
	phys_addr_t *physaddr;

	(void)inode;

	physaddr = kzalloc(sizeof(phys_addr_t), GFP_KERNEL);
	if (!physaddr)
		return -ENOMEM;

	filp->private_data = physaddr;

	return 0;
}

static long tz_scma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	phys_addr_t *physaddr;

	switch(cmd) {
	case TZ_SCMA_SET_PHYS_ADDR:
		physaddr = (phys_addr_t *)filp->private_data;
		*physaddr = arg;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tz_scma_mmap(struct file *filp, struct vm_area_struct *vma)
{
	phys_addr_t *addr = (phys_addr_t *)filp->private_data;

	if (*addr == 0)
		return -EFAULT;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	return remap_pfn_range(vma, vma->vm_start, __phys_to_pfn(*addr),
			vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

static int tz_scma_release(struct inode *inode, struct file *filp)
{
	(void)inode;

	kfree(filp->private_data);

	return 0;
}

static const struct file_operations tz_scma_fops = {
	.open = tz_scma_open,
	.unlocked_ioctl = tz_scma_ioctl,
	.compat_ioctl = tz_scma_ioctl,
	.mmap = tz_scma_mmap,
	.release = tz_scma_release,
};

static struct tz_cdev tz_scma_cdev = {
	.name = "tz_scma_test",
	.fops = &tz_scma_fops,
	.owner = THIS_MODULE,
};

int tz_scma_test_init(void)
{
	return tz_cdev_register(&tz_scma_cdev);
}

void tz_scma_test_exit(void)
{
	tz_cdev_unregister(&tz_scma_cdev);
}

tzdev_initcall(tz_scma_test_init);
tzdev_exitcall(tz_scma_test_exit);

MODULE_LICENSE("GPL");
