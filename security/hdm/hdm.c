/*
 * @file hdm.c
 * @brief HDM Support
 * Copyright (c) 2020, Samsung Electronics Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/mm.h>
#if defined(CONFIG_ARCH_QCOM)
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/resource.h>
#endif
#include <linux/hdm.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/uh.h>
#if defined(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#else
#include <linux/sec_sysfs.h>
#endif
#if defined(CONFIG_ARCH_QCOM)
#include <linux/sched/signal.h>
#include <soc/qcom/secure_buffer.h>
#endif

static ssize_t store_hdm_policy(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long mode = HDM_CMD_MAX;
	int c, p;

	if (count == 0) {
		pr_err("%s count = 0\n", __func__);
		goto error;
	}

	if (kstrtoul(buf, 0, &mode)) {
		goto error;
	};

	if (mode > HDM_CMD_MAX) {
		pr_err("%s command size max fail. %d\n", __func__, mode);
		goto error;
	}
	pr_info("%s: command id: 0x%x\n", __func__, (int)mode);

	c = (int)(mode & HDM_C_BITMASK);
	p = (int)(mode & HDM_P_BITMASK);

	pr_info("%s m:0x%x c:0x%x p:0x%x\n", __func__, (int)mode, c, p);
	switch (c) {
#if defined(CONFIG_ARCH_QCOM)
	case HDM_HYP_CALL:
		pr_info("%s HDM_HYP_CALL\n", __func__);
		uh_call(UH_APP_HDM, 9, 0, p, 0, 0);
		break;
	case HDM_HYP_CALLP:
		pr_info("%s HDM_HYP_CALLP\n", __func__);
		uh_call(UH_APP_HDM, 2, 0, p, 0, 0);
		break;
#endif
	default:
		goto error;
	}
error:
	return count;
}
static DEVICE_ATTR(hdm_policy, 0220, NULL, store_hdm_policy);

#if defined(CONFIG_ARCH_QCOM)
static int __init __hdm_init_of(void)
{
	struct device_node *node;
	struct resource r;
	int ret;
	phys_addr_t addr;
	u64 size;

	int src_vmids[1] = {VMID_HLOS};
	int dest_vmids[1] = {VMID_CP_BITSTREAM};
	int dest_perms[1] = {PERM_READ | PERM_WRITE};

	pr_info("%s start\n", __func__);

	node = of_find_node_by_name(NULL, "samsung,sec_hdm");
	if (!node) {
		pr_err("%s failed of_find_node_by_name\n", __func__);
		return -ENODEV;
	}

	node = of_parse_phandle(node, "memory-region", 0);
	if (!node) {
		pr_err("%s no memory-region specified\n", __func__);
		return -EINVAL;
	}


	ret = of_address_to_resource(node, 0, &r);
	if (ret) {
		pr_err("%s failed of_address_to_resource\n", __func__);
		return ret;
	}

	addr = r.start;
	size = resource_size(&r);

	ret = hyp_assign_phys(addr, size, src_vmids, 1, dest_vmids, dest_perms, 1);
	if (ret) {
		pr_err("%s failed for %pa address of size %zx rc:%d\n",
				__func__, &addr, size, ret);
	}

	pr_info("%s done.\n", __func__);
	return 0;
}
#endif

static int __init hdm_test_init(void)
{
	struct device *dev;
#if defined(CONFIG_ARCH_QCOM)
	int err;
#endif

#if defined(CONFIG_DRV_SAMSUNG)
	dev = sec_device_create(0, NULL, "hdm");
#else
	dev = sec_device_create(NULL, "hdm");
#endif
	WARN_ON(!dev);
	if (IS_ERR(dev))
		pr_err("%s Failed to create devce\n", __func__);

	if (device_create_file(dev, &dev_attr_hdm_policy) < 0)
		pr_err("%s Failed to create device file\n", __func__);

#if defined(CONFIG_ARCH_QCOM)
	err = __hdm_init_of();
	if (err)
		return err;
#endif

	pr_info("%s done.\n", __func__);
	return 0;
}

module_init(hdm_test_init);
