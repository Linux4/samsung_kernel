/*
 * Copyright (c) 2014-2017 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN fmm code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/sec_class.h>
#include <linux/sec_ext.h>
#include <linux/sec_debug.h>
#include <linux/moduleparam.h>


static unsigned long fmm_lock_offset;
struct device *secdbg_dev;
EXPORT_SYMBOL(secdbg_dev);

static int __init sec_debug_fmm_lock_offset(char *arg)
{
	fmm_lock_offset = simple_strtoul(arg, NULL, 10);
	return 0;
}

early_param("sec_debug.fmm_lock_offset", sec_debug_fmm_lock_offset);

static ssize_t store_FMM_lock(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t count)
{
       	char lock;

	sscanf(buf, "%c", &lock);
	pr_info("%s: store %c in FMM_lock\n", __func__, lock);
	sec_set_param(fmm_lock_offset, lock);

        return count;
}

static struct kobj_attribute FMM_lock_attr =
	__ATTR(FMM_lock, 0220, NULL, store_FMM_lock);

static struct attribute *FMM_lock_attributes[] = {
	&FMM_lock_attr.attr,
	NULL,
};

static struct attribute_group FMM_lock_attr_group = {
	.attrs = FMM_lock_attributes,
};

static int __init sec_fmm_init(void)
{
	int ret = 0;

	secdbg_dev = sec_device_create(NULL, "sec_debug");

	ret = sysfs_create_group(&secdbg_dev->kobj, &FMM_lock_attr_group);
	if (ret)
		pr_err("%s : could not create sysfs noden", __func__);

	return 0;
}
device_initcall(sec_fmm_init);
