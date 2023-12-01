// SPDX-License-Identifier: GPL-2.0-only
/*
 * sec_debug_hw_param.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/sec_class.h>
#include <linux/sec_ext.h>
#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos/exynos-soc.h>

#include "sec_debug_internal.h"

/* maximum size of sysfs */
#define DATA_SIZE 1024
#define LOT_STRING_LEN 5

/* function name prefix: secdbg_hprm */
char __read_mostly *dram_size;
module_param(dram_size, charp, 0440);

/* this is same with androidboot.dram_info */
char __read_mostly *dram_info;
module_param(dram_info, charp, 0440);

static ssize_t secdbg_hprm_extra_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	secdbg_exin_get_extra_info_A(buf);
	info_size = strlen(buf);

	return info_size;
}

static ssize_t secdbg_hprm_extrb_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	secdbg_exin_get_extra_info_B(buf);
	info_size = strlen(buf);

	return info_size;
}

static ssize_t secdbg_hprm_extrc_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	secdbg_exin_get_extra_info_C(buf);
	info_size = strlen(buf);

	return info_size;
}

static ssize_t secdbg_hprm_extrm_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	secdbg_exin_get_extra_info_M(buf);
	info_size = strlen(buf);

	return info_size;
}

static ssize_t secdbg_hprm_extrf_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	secdbg_exin_get_extra_info_F(buf);
	info_size = strlen(buf);

	return info_size;
}

static ssize_t secdbg_hprm_extrt_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;

	secdbg_exin_get_extra_info_T(buf);
	info_size = strlen(buf);

	return info_size;
}

static DEVICE_ATTR(extra_info, 0440, secdbg_hprm_extra_info_show, NULL);
static DEVICE_ATTR(extrb_info, 0440, secdbg_hprm_extrb_info_show, NULL);
static DEVICE_ATTR(extrc_info, 0440, secdbg_hprm_extrc_info_show, NULL);
static DEVICE_ATTR(extrm_info, 0440, secdbg_hprm_extrm_info_show, NULL);
static DEVICE_ATTR(extrf_info, 0440, secdbg_hprm_extrf_info_show, NULL);
static DEVICE_ATTR(extrt_info, 0440, secdbg_hprm_extrt_info_show, NULL);

static struct attribute *secdbg_hprm_attributes[] = {
	&dev_attr_extra_info.attr,
	&dev_attr_extrb_info.attr,
	&dev_attr_extrc_info.attr,
	&dev_attr_extrm_info.attr,
	&dev_attr_extrf_info.attr,
	&dev_attr_extrt_info.attr,
	NULL,
};

static struct attribute_group secdbg_hprm_attr_group = {
	.attrs = secdbg_hprm_attributes,
};

static void secdbg_hprm_set_hw_exin(void)
{
	secdbg_exin_set_hwid(id_get_asb_ver(), id_get_product_line(), dram_info);
}

static int __init secdbg_hw_param_init(void)
{
	int ret = 0;
	struct device *dev;

	pr_info("%s: start\n", __func__);
	pr_info("%s: from cmdline: dram_size: %s\n", __func__, dram_size);
	pr_info("%s: from cmdline: dram_info: %s\n", __func__, dram_info);

	secdbg_hprm_set_hw_exin();

	dev = sec_device_create(NULL, "sec_hw_param");

	ret = sysfs_create_group(&dev->kobj, &secdbg_hprm_attr_group);
	if (ret)
		pr_err("%s : could not create sysfs noden", __func__);

	return 0;
}
module_init(secdbg_hw_param_init);

MODULE_DESCRIPTION("Samsung Debug HW Parameter driver");
MODULE_LICENSE("GPL v2");
