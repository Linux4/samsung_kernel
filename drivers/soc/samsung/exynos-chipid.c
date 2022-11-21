/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - CHIP ID support
 * Author: Pankaj Dubey <pankaj.dubey@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of.h>
#include <linux/platform_device.h>
#include <plat/cpu.h>

#define EXYNOS_LOTID_MASK	0x001FFFFF

/*
 *  sysfs implementation for exynos-snapshot
 *  you can access the sysfs of exynos-snapshot to /sys/devices/system/chip-id
 *  path.
 */
static struct bus_type chipid_subsys = {
	.name = "chip-id",
	.dev_name = "chip-id",
};

static ssize_t chipid_lot_id_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 14, "%08X\n", samsung_chip_id[0] & EXYNOS_LOTID_MASK);
}

static struct kobj_attribute chipid_lot_id_attr =
        __ATTR(lot_id, 0644, chipid_lot_id_show, NULL);

static struct attribute *chipid_sysfs_attrs[] = {
	&chipid_lot_id_attr.attr,
	NULL,
};

static struct attribute_group chipid_sysfs_group = {
	.attrs = chipid_sysfs_attrs,
};

static const struct attribute_group *chipid_sysfs_groups[] = {
	&chipid_sysfs_group,
	NULL,
};

static int __init chipid_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&chipid_subsys, chipid_sysfs_groups);
	if (ret)
		pr_err("fail to register exynos-snapshop subsys\n");

	return ret;
}
late_initcall(chipid_sysfs_init);

