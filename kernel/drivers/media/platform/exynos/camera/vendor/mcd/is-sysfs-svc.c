/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>

#include "is-sysfs.h"
#include "is-core.h"
#include "is-sec-define.h"
#include "is-device-sensor-peri.h"
#include "is-sysfs-svc.h"

static ssize_t SVC_rear_module_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(buf, CAM_INFO_REAR);
}
static DEVICE_ATTR_RO(SVC_rear_module);

static ssize_t SVC_rear_sensor_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensor_type_show(buf, CAM_INFO_REAR);
}
static DEVICE_ATTR_RO(SVC_rear_sensor_type);

static ssize_t SVC_rear_sensor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(buf, CAM_INFO_REAR);
}
static DEVICE_ATTR_RO(SVC_rear_sensor);

static ssize_t SVC_rear_module2_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(buf, CAM_INFO_REAR2);
}
static DEVICE_ATTR_RO(SVC_rear_module2);

static ssize_t SVC_rear_sensor2_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensor_type_show(buf, CAM_INFO_REAR2);
}
static DEVICE_ATTR_RO(SVC_rear_sensor2_type);

static ssize_t SVC_rear_sensor2_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(buf, CAM_INFO_REAR2);
}
static DEVICE_ATTR_RO(SVC_rear_sensor2);

static ssize_t SVC_rear_module3_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(SVC_rear_module3);

static ssize_t SVC_rear_sensor3_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensor_type_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(SVC_rear_sensor3_type);

static ssize_t SVC_rear_sensor3_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(buf, CAM_INFO_REAR3);
}
static DEVICE_ATTR_RO(SVC_rear_sensor3);

static ssize_t SVC_rear_module4_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(buf, CAM_INFO_REAR4);
}
static DEVICE_ATTR_RO(SVC_rear_module4);

static ssize_t SVC_rear_sensor4_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensor_type_show(buf, CAM_INFO_REAR4);
}
static DEVICE_ATTR_RO(SVC_rear_sensor4_type);

static ssize_t SVC_rear_sensor4_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(buf, CAM_INFO_REAR4);
}
static DEVICE_ATTR_RO(SVC_rear_sensor4);

static ssize_t SVC_front_module_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(buf, CAM_INFO_FRONT);
}
static DEVICE_ATTR_RO(SVC_front_module);

static ssize_t SVC_front_sensor_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensor_type_show(buf, CAM_INFO_FRONT);
}
static DEVICE_ATTR_RO(SVC_front_sensor_type);

static ssize_t SVC_front_sensor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(buf, CAM_INFO_FRONT);
}
static DEVICE_ATTR_RO(SVC_front_sensor);

static ssize_t SVC_front_module2_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_moduleid_show(buf, CAM_INFO_FRONT2);
}
static DEVICE_ATTR_RO(SVC_front_module2);

static ssize_t SVC_front_sensor2_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensor_type_show(buf, CAM_INFO_FRONT2);
}
static DEVICE_ATTR_RO(SVC_front_sensor2_type);

static ssize_t SVC_front_sensor2_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return camera_sensorid_show(buf, CAM_INFO_FRONT2);
}
static DEVICE_ATTR_RO(SVC_front_sensor2);

static struct attribute *svc_cam_attrs[] = {
	&dev_attr_SVC_rear_module.attr,
	&dev_attr_SVC_rear_sensor_type.attr,
	&dev_attr_SVC_rear_sensor.attr,
	&dev_attr_SVC_front_module.attr,
	&dev_attr_SVC_front_sensor_type.attr,
	&dev_attr_SVC_front_sensor.attr,
	NULL,
};

static const struct attribute_group svc_cam_group = {
	.attrs = svc_cam_attrs,
};

static const struct attribute_group *svc_cam_groups[] = {
	&svc_cam_group,
	NULL,
};

static void svc_cam_release(struct device *dev)
{
	kfree(dev);
}

int svc_cheating_prevent_device_file_create(void)
{
	struct kernfs_node *svc_sd;
	struct kobject *data;
	struct device *dev;
	int ret;
	struct device *is_dev;
	struct kobject *top_kobj;
	struct is_sysfs_config *sysfs_config = is_vendor_get_sysfs_config();

	is_dev = is_get_is_dev();

	/* To find svc kobject */
	top_kobj = &is_dev->kobj.kset->kobj;

	svc_sd = sysfs_get_dirent(top_kobj->sd, "svc");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* Try to create svc kobject */
		data = kobject_create_and_add("svc", top_kobj);
		if (IS_ERR_OR_NULL(data))
			pr_info("Failed to create sys/devices/svc already exist svc : 0x%pK\n", data);
		else
			pr_info("Success to create sys/devices/svc svc : 0x%pK\n", data);
	} else {
		data = (struct kobject *)svc_sd->priv;
		pr_info("Success to find svc_sd : 0x%pK svc : 0x%pK\n", svc_sd, data);
	}

	dev = kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	ret = dev_set_name(dev, "Camera");
	if (ret < 0)
		goto err_name;

	dev->kobj.parent = data;
	dev->groups = svc_cam_groups;
	dev->release = svc_cam_release;

	ret = device_register(dev);
	if (ret < 0)
		goto err_dev_reg;

	if (sysfs_config->rear2) {
		if (sysfs_config->rear2_moduleid) {
			ret = device_create_file(dev, &dev_attr_SVC_rear_module2);
			if (ret != 0) {
				pr_err("failed to create device file %s\n", dev_attr_SVC_rear_module2.attr.name);
				goto err_dev_reg;
			}

			ret = device_create_file(dev, &dev_attr_SVC_rear_sensor2_type);
			if (ret != 0) {
				pr_err("failed to create device file %s\n", dev_attr_SVC_rear_sensor2_type.attr.name);
				goto err_dev_reg;
			}

			ret = device_create_file(dev, &dev_attr_SVC_rear_sensor2);
			if (ret != 0) {
				pr_err("failed to create device file %s\n", dev_attr_SVC_rear_sensor2.attr.name);
				goto err_dev_reg;
			}
		}
	}

	if (sysfs_config->rear3) {
		if (sysfs_config->rear3_moduleid) {
			ret = device_create_file(dev, &dev_attr_SVC_rear_module3);
			if (ret != 0) {
				pr_err("failed to create device file %s\n", dev_attr_SVC_rear_module3.attr.name);
				goto err_dev_reg;
			}

			ret = device_create_file(dev, &dev_attr_SVC_rear_sensor3_type);
			if (ret != 0) {
				pr_err("failed to create device file %s\n", dev_attr_SVC_rear_sensor3_type.attr.name);
				goto err_dev_reg;
			}

			ret = device_create_file(dev, &dev_attr_SVC_rear_sensor3);
			if (ret != 0) {
				pr_err("failed to create device file %s\n", dev_attr_SVC_rear_sensor3.attr.name);
				goto err_dev_reg;
			}
		}
	}

	if (sysfs_config->rear4) {
		if (sysfs_config->rear4_moduleid) {
			ret = device_create_file(dev, &dev_attr_SVC_rear_module4);
			if (ret != 0) {
				pr_err("failed to create device file %s\n", dev_attr_SVC_rear_module4.attr.name);
				goto err_dev_reg;
			}

			ret = device_create_file(dev, &dev_attr_SVC_rear_sensor4_type);
			if (ret != 0) {
				pr_err("failed to create device file %s\n", dev_attr_SVC_rear_sensor4_type.attr.name);
				goto err_dev_reg;
			}

			ret = device_create_file(dev, &dev_attr_SVC_rear_sensor4);
			if (ret != 0) {
				pr_err("failed to create device file %s\n", dev_attr_SVC_rear_sensor4.attr.name);
				goto err_dev_reg;
			}
		}
	}

	if (sysfs_config->front2) {
		if (sysfs_config->front2_moduleid) {
			ret = device_create_file(dev, &dev_attr_SVC_front_module2);
			if (ret != 0) {
				pr_err("failed to create device file %s\n", dev_attr_SVC_front_module2.attr.name);
				goto err_dev_reg;
			}

			ret = device_create_file(dev, &dev_attr_SVC_front_sensor2_type);
			if (ret != 0) {
				pr_err("failed to create device file %s\n", dev_attr_SVC_front_sensor2_type.attr.name);
				goto err_dev_reg;
			}

			ret = device_create_file(dev, &dev_attr_SVC_front_sensor2);
			if (ret != 0) {
				pr_err("failed to create device file %s\n", dev_attr_SVC_front_sensor2.attr.name);
				goto err_dev_reg;
			}
		}
	}

	return 0;

err_dev_reg:
	put_device(dev);
err_name:
	kfree(dev);
	dev = NULL;
	return ret;
}
