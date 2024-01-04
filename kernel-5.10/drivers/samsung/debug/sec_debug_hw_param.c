/*
 *sec_debug_hw_param.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sec_debug.h>
#include <linux/sec_class.h>
#include <linux/sec_ext.h>
#include <linux/uaccess.h>

#define DATA_SIZE 1024

extern char *sec_debug_extra_info_buf;
extern struct sec_debug_panic_extra_info *sec_debug_extra_info_backup;

static ssize_t secdbg_hw_param_extra_info_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	if (!sec_debug_extra_info_buf)
		return -ENOENT;

	sec_debug_store_extra_info_A();
	strncpy(buf, sec_debug_extra_info_buf, SZ_1K);
	info_size = strlen(buf);

	return info_size;
}

static ssize_t secdbg_hw_param_extrb_info_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	if (!sec_debug_extra_info_buf)
		return -ENOENT;

	sec_debug_store_extra_info_B();
	strncpy(buf, sec_debug_extra_info_buf, SZ_1K);
	info_size = strlen(buf);

	return info_size;
}

static ssize_t secdbg_hw_param_extrc_info_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	if (!sec_debug_extra_info_buf)
		return -ENOENT;

	sec_debug_store_extra_info_C();
	strncpy(buf, sec_debug_extra_info_buf, SZ_1K);
	info_size = strlen(buf);

	return info_size;
}

static ssize_t secdbg_hw_param_extrm_info_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	if (!sec_debug_extra_info_buf)
		return -ENOENT;

	sec_debug_store_extra_info_M();
	strncpy(buf, sec_debug_extra_info_buf, SZ_1K);
	info_size = strlen(buf);

	return info_size;
}

static DEVICE_ATTR(extra_info, 0440, secdbg_hw_param_extra_info_show, NULL);
static DEVICE_ATTR(extrb_info, 0440, secdbg_hw_param_extrb_info_show, NULL);
static DEVICE_ATTR(extrc_info, 0440, secdbg_hw_param_extrc_info_show, NULL);
static DEVICE_ATTR(extrm_info, 0440, secdbg_hw_param_extrm_info_show, NULL);

static struct attribute *secdbg_hw_param_attributes[] = {
	&dev_attr_extra_info.attr,
	&dev_attr_extrb_info.attr,
	&dev_attr_extrc_info.attr,
	&dev_attr_extrm_info.attr,
	NULL,
};

static struct attribute_group secdbg_hw_param_attr_group = {
	.attrs = secdbg_hw_param_attributes,
};

int secdbg_hw_param_init(void)
{
	int ret = 0;
	struct device *dev;

	dev = sec_device_create(NULL, "sec_hw_param");

	ret = sysfs_create_group(&dev->kobj, &secdbg_hw_param_attr_group);
	if (ret)
		pr_err("%s : could not create sysfs noden", __func__);

	return 0;
}
EXPORT_SYMBOL(secdbg_hw_param_init);
