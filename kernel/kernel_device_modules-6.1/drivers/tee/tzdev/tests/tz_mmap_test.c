/*
 * Copyright (C) 2012-2017 Samsung Electronics, Inc.
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

#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include "core/cdev.h"
#include "core/subsystem.h"
#include "tz_mmap_test.h"

MODULE_AUTHOR("Alex Matveev <alex.matveev@samsung.com>");
MODULE_LICENSE("GPL");

static char tz_mmap_test_stat[PAGE_SIZE] __aligned(PAGE_SIZE);
static void *tz_mmap_test_dyn_v;
static struct page *tz_mmap_test_page;

static long tz_mmap_test_unlocked_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	uint64_t phys_addr;

	switch (cmd) {
	case TZ_MMAP_GET_PADDR_STATIC:
#ifndef MODULE
		phys_addr = virt_to_phys(tz_mmap_test_stat);
#else
		phys_addr = page_to_phys(vmalloc_to_page(tz_mmap_test_stat));
#endif

		return put_user(phys_addr, (uint64_t *) arg);
	case TZ_MMAP_GET_PADDR_DYNAMIC:
		phys_addr = page_to_phys(tz_mmap_test_page);
		return put_user(phys_addr, (uint64_t *) arg);
	case TZ_MMAP_FILL_STATIC:
		memset(tz_mmap_test_stat, arg, PAGE_SIZE);
		return 0;
	case TZ_MMAP_FILL_DYNAMIC:
		memset(tz_mmap_test_dyn_v, arg, PAGE_SIZE);
		return 0;
	}

	return -ENOTTY;
}

static const struct file_operations tz_mmap_test_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tz_mmap_test_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tz_mmap_test_unlocked_ioctl,
#endif /* CONFIG_COMPAT */
};

static struct tz_cdev tz_mmap_test_cdev = {
	.name = "tz_mmap_test",
	.fops = &tz_mmap_test_fops,
	.owner = THIS_MODULE,
};

int tz_mmap_test_init(void)
{
	int rc;

	rc = tz_cdev_register(&tz_mmap_test_cdev);
	if (rc)
		return rc;

	tz_mmap_test_page = alloc_page(GFP_KERNEL);
	if (!tz_mmap_test_page) {
		rc = -ENOMEM;
		goto unregister_dev;
	}

	tz_mmap_test_dyn_v = page_address(tz_mmap_test_page);

	return 0;

unregister_dev:
	tz_cdev_unregister(&tz_mmap_test_cdev);

	return rc;
}

void tz_mmap_test_exit(void)
{
	__free_page(tz_mmap_test_page);
	tz_cdev_unregister(&tz_mmap_test_cdev);
}

tzdev_initcall(tz_mmap_test_init);
tzdev_exitcall(tz_mmap_test_exit);
