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
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/printk.h>
#include <linux/of_fdt.h>

static const char *soc_ap_id;

static const char * __init chip_id_to_name(void)
{
	const char *soc_name;

	soc_name = of_flat_dt_get_machine_name();

	return soc_name;
}

#define UNIQUE_ID_LEN 16
static char uniqueid_str[UNIQUE_ID_LEN+1];
static char nf_str[10] = "not found";

static char *uniqueid_get(void)
{
	char *src_ptr = strstr(saved_command_line, "uniqueno=");

	if (src_ptr == NULL)
		return nf_str;

	strncpy(uniqueid_str, src_ptr + strlen("uniqueno="), UNIQUE_ID_LEN);
	uniqueid_str[UNIQUE_ID_LEN] = '\0';

	return uniqueid_str;
}

static struct bus_type chipid_subsys = {
	.name = "chip-id",
	.dev_name = "chip-id",
};

static ssize_t chipid_ap_id_show(struct kobject *kobj,
								struct kobj_attribute *attr,
								char *buf)
{
	return snprintf(buf, 15, "%s\n", soc_ap_id);
}

static ssize_t chipid_unique_id_show(struct kobject *kobj,
								struct kobj_attribute *attr,
								char *buf)
{
	return snprintf(buf, 20, "%s\n", uniqueid_get());
}

static struct kobj_attribute chipid_ap_id_attr =
		__ATTR(ap_id, 0644, chipid_ap_id_show, NULL);

static struct kobj_attribute chipid_unique_id_attr =
		__ATTR(unique_id, 0644, chipid_unique_id_show, NULL);

static struct attribute *chipid_sysfs_attrs[] = {
		&chipid_ap_id_attr.attr,
		&chipid_unique_id_attr.attr,
		NULL,
};

static struct attribute_group chipid_sysfs_group = {
	.attrs = chipid_sysfs_attrs,
};

static const struct attribute_group *chipid_sysfs_groups[] = {
	&chipid_sysfs_group,
	NULL,
};

static ssize_t svc_ap_show(struct kobject *kobj,
								struct kobj_attribute *attr,
								char *buf)
{
	return snprintf(buf, 20, "%s\n", uniqueid_get());
}

static struct kobj_attribute svc_ap_attr =
		__ATTR(SVC_AP, 0644, svc_ap_show, NULL);

extern struct kset *devices_kset;

void sysfs_create_svc_ap(void)
{
	struct kernfs_node *svc_sd;
	struct kobject *data;
	struct kobject *ap;

	/* To find svc kobject */
	svc_sd = sysfs_get_dirent(devices_kset->kobj.sd, "svc");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* try to create svc kobject */
		data = kobject_create_and_add("svc", &devices_kset->kobj);
		if (IS_ERR_OR_NULL(data))
			pr_info("Existing path sys/devices/svc : 0x%pK\n", data);
		else
			pr_info("Created sys/devices/svc svc : 0x%pK\n", data);
	} else {
		data = (struct kobject *)svc_sd->priv;
		pr_info("Found svc_sd : 0x%pK svc : 0x%pK\n", svc_sd, data);
	}

	ap = kobject_create_and_add("AP", data);
	if (IS_ERR_OR_NULL(ap))
		pr_info("Failed to create sys/devices/svc/AP : 0x%pK\n", ap);
	else
		pr_info("Success to create sys/devices/svc/AP : 0x%pK\n", ap);

	if (sysfs_create_file(ap, &svc_ap_attr.attr) < 0) {
		pr_err("failed to create sys/devices/svc/AP/SVC_AP, %s\n",
		svc_ap_attr.attr.name);
	}
}

static int __init chipid_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&chipid_subsys, chipid_sysfs_groups);
	if (ret)
		pr_err("%s: fail to register chip id subsys\n", __func__);

	sysfs_create_svc_ap();

#ifdef CONFIG_SEC_MISC
	soc_ap_id = chip_id_to_name();
#endif

	return ret;
}
late_initcall(chipid_sysfs_init);

