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

#define MAX_DDR_VENDOR 16
#define DATA_SIZE 1024

#define HW_PARAM_CHECK_SIZE(size)	\
	if (size >= 1024)		\
	return 1024;			\

extern char *sec_debug_extra_info_buf;
extern struct sec_debug_panic_extra_info *sec_debug_extra_info_backup;

unsigned int sec_reset_cnt;

static int __init sec_hw_param_get_reset_count(char *arg)
{
	get_option(&arg, &sec_reset_cnt);
	return 0;
}

early_param("sec_debug.reset_rwc", sec_hw_param_get_reset_count);

static ssize_t sec_hw_param_extra_info_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	if (!sec_debug_extra_info_buf)
		return -ENOENT;

	if (reset_reason == RR_K || reset_reason == RR_D || reset_reason == RR_P) {
		sec_debug_store_extra_info_A();
		strncpy(buf, sec_debug_extra_info_buf, SZ_1K);
		info_size = strlen(buf);
	}

	return info_size;
}

static ssize_t sec_hw_param_extrb_info_show(struct kobject *kobj,
					    struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	if (!sec_debug_extra_info_buf)
		return -ENOENT;

	if (reset_reason == RR_K || reset_reason == RR_D || reset_reason == RR_P) {
		sec_debug_store_extra_info_B();
		strncpy(buf, sec_debug_extra_info_buf, SZ_1K);
		info_size = strlen(buf);
	}

	return info_size;
}

static ssize_t sec_hw_param_extrc_info_show(struct kobject *kobj,
					    struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	if (!sec_debug_extra_info_buf)
		return -ENOENT;

	if (reset_reason == RR_K || reset_reason == RR_D || reset_reason == RR_P) {
		sec_debug_store_extra_info_C();
		strncpy(buf, sec_debug_extra_info_buf, SZ_1K);
		info_size = strlen(buf);
	}

	return info_size;
}

static ssize_t sec_hw_param_extrm_info_show(struct kobject *kobj,
					    struct kobj_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	if (!sec_debug_extra_info_buf)
		return -ENOENT;

	if (reset_reason == RR_K || reset_reason == RR_D || reset_reason == RR_P) {
		sec_debug_store_extra_info_M();
		strncpy(buf, sec_debug_extra_info_buf, SZ_1K);
		info_size = strlen(buf);
	}

	return info_size;
}

static struct kobj_attribute sec_hw_param_extra_info_attr =
		__ATTR(extra_info, 0440, sec_hw_param_extra_info_show, NULL);

static struct kobj_attribute sec_hw_param_extrb_info_attr =
		__ATTR(extrb_info, 0440, sec_hw_param_extrb_info_show, NULL);

static struct kobj_attribute sec_hw_param_extrc_info_attr =
		__ATTR(extrc_info, 0440, sec_hw_param_extrc_info_show, NULL);

static struct kobj_attribute sec_hw_param_extrm_info_attr =
		__ATTR(extrm_info, 0440, sec_hw_param_extrm_info_show, NULL);

static struct attribute *sec_hw_param_attributes[] = {
	&sec_hw_param_extra_info_attr.attr,
	&sec_hw_param_extrb_info_attr.attr,
	&sec_hw_param_extrc_info_attr.attr,
	&sec_hw_param_extrm_info_attr.attr,
	NULL,
};

static struct attribute_group sec_hw_param_attr_group = {
	.attrs = sec_hw_param_attributes,
};

static int __init sec_hw_param_init(void)
{
	int ret = 0;
	struct device *dev;

	dev = sec_device_create(NULL, "sec_hw_param");

	ret = sysfs_create_group(&dev->kobj, &sec_hw_param_attr_group);
	if (ret)
		pr_err("%s : could not create sysfs noden", __func__);

	return 0;
}

device_initcall(sec_hw_param_init);
