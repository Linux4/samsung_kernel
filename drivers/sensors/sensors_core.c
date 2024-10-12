/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/input.h>
#include <linux/sensor/sensors_core.h>

struct class *sensors_class;
static struct device *sensor_dev;
static struct input_dev *meta_input_dev;

static atomic_t sensor_count;

static ssize_t set_flush(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	int64_t dTemp;
	u8 sensor_type = 0;

	if (kstrtoll(buf, 10, &dTemp) < 0)
		return -EINVAL;

	sensor_type = (u8)dTemp;

	input_report_rel(meta_input_dev, REL_DIAL,
		1);	/*META_DATA_FLUSH_COMPLETE*/
	input_report_rel(meta_input_dev, REL_HWHEEL, sensor_type + 1);
	input_sync(meta_input_dev);

	pr_info("[SENSOR] flush %d\n", sensor_type);
	return size;
}

static DEVICE_ATTR(flush, S_IWUSR | S_IWGRP, NULL, set_flush);

static struct device_attribute *sensor_attr[] = {
	&dev_attr_flush,
	NULL,
};

int sensors_register(struct device **dev, void *drvdata,
		struct device_attribute *attributes[], char *name)
{
	int ret = 0;
	int i;

	if (sensors_class == NULL) {
		sensors_class = class_create(THIS_MODULE, "sensors");
		if (IS_ERR(sensors_class))
			return PTR_ERR(sensors_class);
	}

	*dev = device_create(sensors_class, NULL, 0, drvdata, "%s", name);
	if (IS_ERR(*dev)) {
		ret = PTR_ERR(*dev);
		pr_err("[SENSORS CORE] device_create failed![%d]\n", ret);
		return ret;
	}

	for (i = 0; attributes[i] != NULL; i++)
		if ((device_create_file(*dev, attributes[i])) < 0)
			pr_err("[SENSOR CORE] fail device_create_file"\
				"(dev, attributes[%d])\n", i);
	atomic_inc(&sensor_count);

	return ret;
}

void sensors_unregister(struct device *dev,
		struct device_attribute *attributes[])
{
	int i;

	for (i = 0; attributes[i] != NULL; i++)
		device_remove_file(dev, attributes[i]);
}

int sensors_input_init(void)
{
	int ret;

	/* Meta Input Event Initialization */
	meta_input_dev = input_allocate_device();
	if (!meta_input_dev) {
		pr_err("[SENSOR CORE] failed alloc meta dev\n");
		return -ENOMEM;
	}

	meta_input_dev->name = "meta_event";
	input_set_capability(meta_input_dev, EV_REL, REL_HWHEEL);
	input_set_capability(meta_input_dev, EV_REL, REL_DIAL);

	ret = input_register_device(meta_input_dev);
	if (ret < 0) {
		pr_err("[SENSOR CORE] failed register meta dev\n");
		input_free_device(meta_input_dev);
		return ret;
	}

	return ret;
}

void sensors_input_clean(void)
{
	input_unregister_device(meta_input_dev);
	input_free_device(meta_input_dev);
}

static int __init sensors_class_init(void)
{
	pr_info("[SENSORS CORE] sensors_class_init\n");

	sensors_class = class_create(THIS_MODULE, "sensors");
	if (IS_ERR(sensors_class)) {
		pr_err("%s, create sensors_class is failed.(err=%d)\n",
			__func__, IS_ERR(sensors_class));
		return PTR_ERR(sensors_class);
	}

	/* For flush sysfs */
	sensor_dev = device_create(sensors_class, NULL, 0, NULL,
		"%s", "sensor_dev");
	if (IS_ERR(sensor_dev)) {
		pr_err("[SENSORS CORE] sensor_dev create failed![%d]\n",
			IS_ERR(sensor_dev));

		class_destroy(sensors_class);
		return PTR_ERR(sensor_dev);
	} else {
		if ((device_create_file(sensor_dev, *sensor_attr)) < 0)
			pr_err("[SENSOR CORE] failed flush device_file\n");
	}

	atomic_set(&sensor_count, 0);
	sensors_class->dev_uevent = NULL;

	sensors_input_init();

	return 0;
}

static void __exit sensors_class_exit(void)
{
	if (meta_input_dev)
		sensors_input_clean();

	if (sensors_class) {
		class_destroy(sensors_class);
		sensors_class = NULL;
	}
}

subsys_initcall(sensors_class_init);
module_exit(sensors_class_exit);

MODULE_DESCRIPTION("Universal sensors core class");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");