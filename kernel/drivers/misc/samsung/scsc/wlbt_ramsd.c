/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include "wlbt_ramsd.h"

#define DEVICE_NAME "wlbt_ramsd"
#define N_MINORS	1

static struct class *wlbt_ramsd_class;
static struct cdev wlbt_ramsd_dev[N_MINORS];
static dev_t dev_num;
static unsigned long ramsd_address = 0;
static unsigned long ramsd_offset = 0;

static int wlbt_ramsd_open(struct inode *inode, struct file *filp)
{
	pr_info("wlbt_ramsd_open\n");
	return 0;
}

static int wlbt_ramsd_release(struct inode *inode, struct file *filp)
{
	pr_info("wlbt_ramsd_release\n");
	return 0;
}

static int wlbt_ramsd_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long pos = ramsd_address + ramsd_offset;
	unsigned long page;
	int ret;

	if (!pos) {
		pr_err("ramsd base address is NULL\n");
		return -1;
	}

	while (size > 0) {
		page = PHYS_PFN(pos);
		ret = remap_pfn_range(vma, start, page, PAGE_SIZE, PAGE_SHARED);
		if (ret) {
			pr_err("remap failed, ret = %d\n", ret);
			return -EAGAIN;
		}
		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

	return 0;
}

static const struct file_operations wlbt_ramsd_fops = {
	.owner          = THIS_MODULE,
	.open           = wlbt_ramsd_open,
	.mmap           = wlbt_ramsd_mmap,
	.release        = wlbt_ramsd_release,
};

void wlbt_ramsd_set_ramrp_address(unsigned long mem_start, unsigned long offset)
{
	ramsd_address = mem_start;
	ramsd_offset = offset;
}

int wlbt_ramsd_create(void)
{
	struct device *dev;
	int i;
	int ret;
	dev_t curr_dev;

	/* Request the kernel for N_MINOR devices */
	ret = alloc_chrdev_region(&dev_num, 0, N_MINORS, "wlbt_ramsd");
	if (ret) {
		pr_err("alloc_chrdev_region failed");
		goto error;
	}

	/* Create a class : appears at /sys/class */
	wlbt_ramsd_class = class_create(THIS_MODULE, "wlbt_ramsd_class");
	if (IS_ERR(wlbt_ramsd_class)) {
		ret = PTR_ERR(wlbt_ramsd_class);
		goto error_class;
	}

	/* Initialize and create each of the device(cdev) */
	for (i = 0; i < N_MINORS; i++) {
		/* Associate the cdev with a set of file_operations */
		cdev_init(&wlbt_ramsd_dev[i], &wlbt_ramsd_fops);

		ret = cdev_add(&wlbt_ramsd_dev[i], dev_num, 1);
		if (ret)
			pr_err("cdev_add failed");

		wlbt_ramsd_dev[i].owner = THIS_MODULE;
		/* Build up the current device number. To be used further */
		dev = device_create(wlbt_ramsd_class, NULL, dev_num, NULL, "wlbt_ramsd_%d", i);
		if (IS_ERR(dev)) {
			pr_err("device_create failed");
			ret = PTR_ERR(dev);
			cdev_del(&wlbt_ramsd_dev[i]);
			continue;
		}
		curr_dev = MKDEV(MAJOR(dev_num), MINOR(dev_num) + i);
	}
	return 0;

error_class:
	unregister_chrdev_region(dev_num, N_MINORS);
error:
	return 0;
}

int wlbt_ramsd_destroy(void)
{
	int i;

	device_destroy(wlbt_ramsd_class, dev_num);
	for (i = 0; i < N_MINORS; i++)
		cdev_del(&wlbt_ramsd_dev[i]);
	class_destroy(wlbt_ramsd_class);
	unregister_chrdev_region(dev_num, N_MINORS);
	return 0;
}
