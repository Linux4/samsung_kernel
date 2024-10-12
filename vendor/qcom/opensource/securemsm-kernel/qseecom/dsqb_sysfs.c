/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include<linux/slab.h>
#include<linux/uaccess.h>
#include<linux/sysfs.h>
#include<linux/kobject.h>
#include <linux/err.h>

#define _INIT_ATTRIBUTE(_name, _mode) \
{ \
	.name = __stringify(_name), \
	.mode = (_mode), \
}

#define STATUS_CFG_ATTR_RW(_name) \
	struct kobj_attribute status_cfg_attr_##_name = { \
		.attr = _INIT_ATTRIBUTE(_name, 0660), \
		.show = dsqb_status_show, \
		.store = dsqb_status_store, \
	}
static ssize_t dsqb_status_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf);
static ssize_t dsqb_status_store(struct kobject *kobj,
					struct kobj_attribute *attr, const char *buf, size_t count);
static STATUS_CFG_ATTR_RW(dsqb_status);
int qseecom_set_ds_state(uint32_t state);

static uint32_t dsqb_status;

struct dsqb_sysfs_struct {
	struct kobject kobj;
	struct kobj_type kobj_type;
};

static ssize_t dsqb_status_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	if (!strcmp(attr->attr.name, "dsqb_status")) {
		return scnprintf(buf, PAGE_SIZE, "%d\n", dsqb_status);
	}
	pr_err("%s: Unhandled attribute %s\n", __func__, buf);
	return -EFAULT;
}

static ssize_t dsqb_status_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = count;
	int op_result = -EFAULT;

	if (!strcmp(attr->attr.name, "dsqb_status")){
		op_result = kstrtoint(buf, 0, &dsqb_status);
	}

	if (!op_result) {
		pr_info("%s: Change %s:%s=%s\n", __func__, buf);
		kobject_uevent(kobj, KOBJ_CHANGE);
		qseecom_set_ds_state(dsqb_status);
	} else {
		pr_err("%s: Failed %s : %d\n", __func__, buf, op_result);
		ret = op_result;
	}
	return ret;
}

static struct attribute *dsqb_status_cfg_attr[] = {
	&status_cfg_attr_dsqb_status.attr,
	NULL,
};

const struct attribute_group dsqb_status_cfg_attr_group = {
	.attrs = dsqb_status_cfg_attr
};

/*
 * Stub function to satisfy kobject lifecycle
 */
static void dsqb_status_release_kobj(struct kobject *kobj)
{
	char const *name = kobject_name(kobj);
	if (name)
		pr_debug("Releasing %s\n", name);
	/* Nothing to do */
}

int dsqb_sysfs_init(void)
{
	struct dsqb_sysfs_struct *elem = NULL;
	int ret = 0;
	struct kobject *kobj_ref;

	kobj_ref = kobject_create_and_add("etx_sysfs",kernel_kobj);
	ret = sysfs_create_group(kobj_ref, &dsqb_status_cfg_attr_group);
	if (ret){
		pr_err("Failed to create qseecom sysfs group for ds_qb state\n");
		goto out;
	}
	elem = kzalloc(sizeof(struct dsqb_sysfs_struct), GFP_KERNEL);
	if (!elem) {
		ret = -ENOMEM;
		pr_err("%s: Error %d allocating memory for partition ds_state\n",
			__func__, ret);
		goto out;
	}

	/* Set up the sysfs node */
	elem->kobj_type.release = dsqb_status_release_kobj;
	elem->kobj_type.sysfs_ops = &kobj_sysfs_ops;
	elem->kobj_type.default_attrs = dsqb_status_cfg_attr;
	ret = kobject_init_and_add(&elem->kobj, &elem->kobj_type,
		NULL, "ds_state");
	if (ret) {
		pr_err("%s: Error %d adding sysfs for ds_state\n",
			__func__, ret);
		kobject_put(&elem->kobj);
		kfree(elem);
	}

out:
	return ret;
}
