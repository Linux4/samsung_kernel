/*
 *  /driver/sensors/sensors_core.c
 *
 *  Copyright (C) 2011 Samsung Electronics Co.Ltd
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include "sensors_core.h"

struct class *sensors_class;
EXPORT_SYMBOL_GPL(sensors_class);
struct class *event_class;
EXPORT_SYMBOL_GPL(event_class);
struct device *event_dev;
EXPORT_SYMBOL_GPL(event_dev);

struct axis_remap {
	int src_x:3;
	int src_y:3;
	int src_z:3;
	int sign_x:2;
	int sign_y:2;
	int sign_z:2;
};
/*!
  * @Description:
  * definitions of placement of sensors
  * P0 - P3: view from top of device
  * P4 - P7: view from bottom of device
  *
  * P0 / P4:
  * Y of device aligned with Y of OS (i.e: Android)
  *                  y
  *                  ^
  *                  |
  *                  |
  *                  |
  *                  o------> x
  *
  *
  * P1 / P5:
  * Y of device aligned with Y of OS (i.e.: Android)
  * rotated by 90 degrees clockwise
  *
  *                  o------> y
  *                  |
  *                  |
  *                  |
  *                  v x
  *
  *
  * P2 / P6:
  * Y of device aligned with Y of OS (i.e.: Android)
  * rotated by 180 degrees clockwise
  *
  *         x <------o
  *                  |
  *                  |
  *                  |
  *                  v
  *                  y
  *
  *
  * P3 / P7:
  * Y of device aligned with Y of OS (i.e.: Android)
  * rotated by 270 degrees clockwise
  *
  *                  x
  *                  ^
  *                  |
  *                  |
  *                  |
  *         y <------o
  *
  */
#define MAX_AXIS_REMAP_TAB_SZ 8 /* P0~P7 */
static const struct axis_remap axis_table[MAX_AXIS_REMAP_TAB_SZ] = {
	/* src_x src_y src_z  sign_x  sign_y  sign_z */
	{  0,    1,    2,     1,      1,      1 }, /* P0 */
	{  1,    0,    2,     1,     -1,      1 }, /* P1 */
	{  0,    1,    2,    -1,     -1,      1 }, /* P2 */
	{  1,    0,    2,    -1,      1,      1 }, /* P3 */
	{  0,    1,    2,    -1,      1,     -1 }, /* P4 */
	{  1,    0,    2,    -1,     -1,     -1 }, /* P5 */
	{  0,    1,    2,     1,     -1,     -1 }, /* P6 */
	{  1,    0,    2,     1,      1,     -1 }, /* P7 */
};

void remap_sensor_data(s16 *val, u32 idx)
{
	s16 tmp[3];

	if (idx < MAX_AXIS_REMAP_TAB_SZ) {
		tmp[0] = val[axis_table[idx].src_x] * axis_table[idx].sign_x;
		tmp[1] = val[axis_table[idx].src_y] * axis_table[idx].sign_y;
		tmp[2] = val[axis_table[idx].src_z] * axis_table[idx].sign_z;
		memcpy(val, &tmp, sizeof(tmp));
	}
}

/**
 * Create sysfs interface
 */
static void set_sensor_attr(struct device *dev,
	struct device_attribute *attributes[])
{
	int i;
	for (i = 0; attributes[i] != NULL; i++) {
		if ((device_create_file(dev, attributes[i])) < 0)
			pr_err("[SENSOR CORE] create_file attributes %d\n", i);
	}
}
struct device *sensors_classdev_register(char *sensors_name)
{
	struct device *dev;
	int retval = -ENODEV;

	dev = device_create(sensors_class, NULL, 0,
					NULL, "%s", sensors_name);
	if (IS_ERR(dev))
		return ERR_PTR(retval);

	pr_info("Registered sensors device: %s\n", sensors_name);
	return dev;
}
EXPORT_SYMBOL_GPL(sensors_classdev_register);

/**
* sensors_classdev_unregister - unregisters a object of sensor device.
*
*/
void sensors_classdev_unregister(struct device *dev)
{
	device_unregister(dev);
}
EXPORT_SYMBOL_GPL(sensors_classdev_unregister);

int sensors_create_symlink(struct kobject *target, const char *name)
{
	int err = 0;

	if (event_dev == NULL) {
		pr_err("%s, event_dev is NULL!!!\n", __func__);
		return err;
	}

	err = sysfs_create_link(&event_dev->kobj, target, name);
	if (err < 0) {
		pr_err("%s, %s failed!(%d)\n", __func__, name, err);
		return err;
	}

	return err;
}
EXPORT_SYMBOL_GPL(sensors_create_symlink);

void sensors_remove_symlink(struct kobject *target, const char *name)
{
	if (event_dev == NULL)
		pr_err("%s, event_dev is NULL!!!\n", __func__);
	else
		sysfs_delete_link(&event_dev->kobj, target, name);
}
EXPORT_SYMBOL_GPL(sensors_remove_symlink);

int sensors_register(struct device *dev, void *drvdata,
		     struct device_attribute *attributes[], char *name)
{
	int ret = 0;
	if (!sensors_class) {
		sensors_class = class_create(THIS_MODULE, "sensors");
		if (IS_ERR(sensors_class))
			return PTR_ERR(sensors_class);
	}

	dev = device_create(sensors_class, NULL, 0, drvdata, "%s", name);

	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		pr_err("[SENSORS CORE] device_create failed! [%d]\n",
		       ret);
		return ret;
	}

	set_sensor_attr(dev, attributes);

	return 0;
}
EXPORT_SYMBOL_GPL(sensors_register);

void sensors_unregister(struct device *dev,
		struct device_attribute *attributes[])
{
	int i;

	for (i = 0; attributes[i] != NULL; i++)
		device_remove_file(dev, attributes[i]);
}
EXPORT_SYMBOL_GPL(sensors_unregister);

static int __init sensors_class_init(void)
{
	pr_info("[SENSORS CORE] sensors_class_init\n");
	sensors_class = class_create(THIS_MODULE, "sensors");

	if (IS_ERR(sensors_class))
		return PTR_ERR(sensors_class);

	event_class = class_create(THIS_MODULE, "sensor_event");

	if (IS_ERR(event_class)) {
		class_destroy(sensors_class);
		return PTR_ERR(event_class);
	}

	event_dev = device_create(event_class, NULL, 0, NULL, "%s", "symlink");

	if (IS_ERR(event_dev)) {
		class_destroy(sensors_class);
		class_destroy(event_class);
		return PTR_ERR(event_dev);
	}
	sensors_class->dev_uevent = NULL;

	return 0;
}

static void __exit sensors_class_exit(void)
{
	class_destroy(sensors_class);
#ifdef CONFIG_SENSOR_USE_SYMLINK
	class_destroy(event_class);
	device_unregister(event_dev);
#endif
}

subsys_initcall(sensors_class_init);
module_exit(sensors_class_exit);

MODULE_DESCRIPTION("Universal sensors core class");
MODULE_AUTHOR("Ryunkyun Park <ryun.park@samsung.com>");
MODULE_LICENSE("GPL");
