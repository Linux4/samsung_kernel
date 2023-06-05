/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/io.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/printk.h>
#include <linux/of_fdt.h>
#include <linux/init.h>

#define AP_ID_LEN 6
static char soc_ap_id[AP_ID_LEN+1]="MTxxxx";

static void get_ap_id(void)
{
	const char *machine;
	struct device_node *root;
	int ret;

	root = of_find_node_by_path("/");
	ret = of_property_read_string(root, "model", &machine);
	of_node_put(root);
	if(ret) /* error to read machine name */
		strncpy(soc_ap_id, "MTxxxx", AP_ID_LEN);
	else
		strncpy(soc_ap_id, machine, AP_ID_LEN);

	soc_ap_id[AP_ID_LEN] = '\0';

	pr_info("%s: ap_id is %s\n", __func__, soc_ap_id);
}

#define UNIQUE_ID_LEN 16
static char uniqueid_str[UNIQUE_ID_LEN+1]="UNIQUE_ID_TEST";
static char* __read_mostly uniqueno;
module_param(uniqueno, charp, 0440);

/*
	NOTE: get_uniqueid()

	'saved_command_line' is not available due to GKI limitation.
  char *src_ptr = strstr(saved_command_line, "uniqueno=");
	
	Thus, we should pass sec_ext.uniqueno by bootconfig	
*/
static void get_uniqueid(void)
{
	if(!uniqueno)
		strncpy(uniqueid_str, "Empty_Unique_no!", UNIQUE_ID_LEN);
	else
		strncpy(uniqueid_str, uniqueno, UNIQUE_ID_LEN);

	uniqueid_str[UNIQUE_ID_LEN] = '\0';

	pr_info("%s: unique_id is %s\n", __func__, uniqueid_str);
}

static struct bus_type chipid_subsys = {
	.name = "chip-id",
	.dev_name = "chip-id",
};

static ssize_t ap_id_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%s\n", soc_ap_id);
}

static ssize_t unique_id_show(struct device *dev,
			         struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%s\n", uniqueid_str);
}

static DEVICE_ATTR_RO(ap_id);
static DEVICE_ATTR_RO(unique_id);

static struct attribute *chipid_sysfs_attrs[] = {
		&dev_attr_ap_id.attr,
		&dev_attr_unique_id.attr,
		NULL,
};

static struct attribute_group chipid_sysfs_group = {
	.attrs = chipid_sysfs_attrs,
};

static const struct attribute_group *chipid_sysfs_groups[] = {
	&chipid_sysfs_group,
	NULL,
};

static ssize_t SVC_AP_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%s\n", uniqueid_str);
}

static DEVICE_ATTR_RO(SVC_AP);

static struct attribute *svc_ap_attrs[] = {
	&dev_attr_SVC_AP.attr,
	NULL,
};

static struct attribute_group svc_ap_group = {
	.attrs = svc_ap_attrs,
};

static const struct attribute_group *svc_ap_groups[] = {
	&svc_ap_group,
	NULL,
};

static void svc_ap_release(struct device *dev)
{
	kfree(dev);
}

extern struct platform_device *secdbg_pdev;

int sysfs_create_svc_ap(void)
{
	struct kernfs_node *svc_sd;
	struct kobject *data;
	struct kobject *top_kobj = NULL;
	struct device *dev;
	int err;

	/* To find svc kobject */
	top_kobj = &secdbg_pdev->dev.kobj.kset->kobj;
	if (!top_kobj) {
		pr_err("No soc kobject\n");
		return -ENODEV;
	}

	/* To find svc kobject */
	svc_sd = sysfs_get_dirent(top_kobj->sd, "svc");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* try to create svc kobject */
		data = kobject_create_and_add("svc", top_kobj);
		if (IS_ERR_OR_NULL(data))
			pr_info("Existing path sys/devices/svc : 0x%pK\n", data);
		else
			pr_info("Created sys/devices/svc svc : 0x%pK\n", data);
	} else {
		data = (struct kobject *)svc_sd->priv;
		pr_info("Found svc_sd : 0x%pK svc : 0x%pK\n", svc_sd, data);
	}

	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!dev) {
		pr_err("Error allocating svc_ap device\n");
		return -ENOMEM;
	}

	err = dev_set_name(dev, "AP");
	if (err < 0)
		goto err_name;

	dev->kobj.parent = data;
	dev->groups = svc_ap_groups;
	dev->release = svc_ap_release;

	err = device_register(dev);
	if (err < 0)
		goto err_dev_reg;

	return 0;

err_dev_reg:
	put_device(dev);
err_name:
	kfree(dev);
	dev = NULL;
	return err;
}

int chipid_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&chipid_subsys, chipid_sysfs_groups);
	if (ret)
		pr_err("%s: fail to register chip id subsys\n", __func__);

	sysfs_create_svc_ap();

	get_ap_id();
	get_uniqueid();

	return ret;
}
EXPORT_SYMBOL(chipid_sysfs_init);

