/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/err.h>

/* CAUTION : Do not be declared as external sec_class  */
static struct class *sec_sysfs_class;
static atomic_t sec_sysfs_dev;

static int __init sec_sysfs_class_create(void)
{
	sec_sysfs_class = class_create(THIS_MODULE, "sec_sysfs");
	if (IS_ERR(sec_sysfs_class)) {
		pr_err("Failed to create class(sec_sysfs) %ld\n", PTR_ERR(sec_sysfs_class));
		return PTR_ERR(sec_sysfs_class);
	}
	return 0;
}

struct device *sec_sysfs_device_create(void *drvdata, const char *fmt)
{
	struct device *dev;

	if (IS_ERR(sec_sysfs_class)) {
		pr_err("Failed to create class(sec_sysfs) %ld\n", PTR_ERR(sec_sysfs_class));
		BUG();
	}

	if (!sec_sysfs_class) {
		pr_err("Not yet created class(sec_sysfs)!\n");
		BUG();
	}

	dev = device_create(sec_sysfs_class, NULL, atomic_inc_return(&sec_sysfs_dev),
			drvdata, fmt);
	if (IS_ERR(dev))
		pr_err("Failed to create device %s %ld\n", fmt, PTR_ERR(dev));
	else
		pr_debug("%s : %s : %d\n", __func__, fmt, dev->devt);

	return dev;
}
EXPORT_SYMBOL(sec_sysfs_device_create);

void sec_sysfs_device_destroy(dev_t devt)
{
	pr_info("%s : %d\n", __func__, devt);
	device_destroy(sec_sysfs_class, devt);
}
EXPORT_SYMBOL(sec_sysfs_device_destroy);

subsys_initcall(sec_sysfs_class_create);
