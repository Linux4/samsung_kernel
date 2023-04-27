/*
 *sec_pcie.c
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/sec_class.h>
#include <linux/exynos-pci-ctrl.h>

static ssize_t sec_pcie0_l1ss_store(struct kobject *kobj,
					   struct kobj_attribute *attr,
					   const char *buf, size_t count)
{
	int l1ss = -1;

	if (sscanf(buf, "%10d", &l1ss) == 0)
		return -EINVAL;

	switch ( l1ss ) {
		case 0:
			pr_info("L1.2 Disable....\n");
			exynos_pcie_l1ss_ctrl(0, 0x40000000);
			break;
		case 1:
			pr_info("L1.2 Enable....\n");
			exynos_pcie_l1ss_ctrl(1, 0x40000000);
			break;	
		default:
			pr_err("Unsupported Test Number(%d)...\n", l1ss);
	}
		

	return count;
}

static struct kobj_attribute sec_pcie0_l1ss_attr =
	__ATTR(pcie0_l1ss_ctrl, 0660, NULL, sec_pcie0_l1ss_store);

static struct attribute *sec_pcie_attributes[] = {
	&sec_pcie0_l1ss_attr.attr,
	NULL,
};

static struct attribute_group sec_pcie_attr_group = {
	.attrs = sec_pcie_attributes,
};

static int __init sec_pcie_init(void)
{
	int ret = 0;
	struct device *dev;

	dev = sec_device_create(NULL, "sec_pcie");

	ret = sysfs_create_group(&dev->kobj, &sec_pcie_attr_group);
	if (ret)
		pr_err("%s : could not create sysfs noden", __func__);

	return 0;
}

device_initcall(sec_pcie_init);
