// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include "ips-helper.h"

static ssize_t ips_worst_vmin_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ips_drv_data *ips_data = dev_get_drvdata(dev);

	return sprintf(buf, "ips worst vmin = %d\n", mtkips_worst_vmin(ips_data));
}
DEVICE_ATTR_RO(ips_worst_vmin);

static ssize_t ips_read_vmin_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ips_drv_data *ips_data = dev_get_drvdata(dev);

	return sprintf(buf, "ips vmin = %d\n", mtkips_getvmin(ips_data));
}
DEVICE_ATTR_RO(ips_read_vmin);

static ssize_t ips_read_vmin_clear_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ips_drv_data *ips_data = dev_get_drvdata(dev);

	return sprintf(buf, "ips vmin = %d\n", mtkips_getvmin_clear(ips_data));
}
DEVICE_ATTR_RO(ips_read_vmin_clear);

static inline ssize_t ips_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ips_drv_data *ips_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", ips_data->enable);
}

static inline ssize_t ips_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int en = 0;
	struct ips_drv_data *ips_data = dev_get_drvdata(dev);

	if (kstrtouint(buf, 0, &en) != 0)
		return -EINVAL;

	if (en)
		mtkips_enable(ips_data);
	else
		mtkips_disable(ips_data);

	return count;
}
DEVICE_ATTR_RW(ips_enable);

static inline ssize_t ips_set_clk_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ips_drv_data *ips_data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", ips_data->enable);
}

static inline ssize_t ips_set_clk_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int clk = 0;
	struct ips_drv_data *ips_data = dev_get_drvdata(dev);

	if (kstrtouint(buf, 0, &clk) != 0)
		return -EINVAL;

	mtkips_set_clk(ips_data, clk);

	return count;
}
DEVICE_ATTR_RW(ips_set_clk);

static struct attribute *ips_sysfs_attrs[] = {
	&dev_attr_ips_enable.attr,
	&dev_attr_ips_read_vmin.attr,
	&dev_attr_ips_read_vmin_clear.attr,
	&dev_attr_ips_worst_vmin.attr,
	&dev_attr_ips_set_clk.attr,
	NULL,
};

static struct attribute_group ips_sysfs_attr_group = {
	.attrs = ips_sysfs_attrs,
};

int ips_register_sysfs(struct device *dev)
{
	int ret;

	ret = sysfs_create_group(&dev->kobj, &ips_sysfs_attr_group);
	if (ret)
		return ret;

	return ret;
}

void ips_unregister_sysfs(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &ips_sysfs_attr_group);
}

