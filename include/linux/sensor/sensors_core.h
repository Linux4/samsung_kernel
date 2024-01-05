/*
 * Driver model for sensor
 *
 * Copyright (C) 2008 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __LINUX_SENSORS_CORE_H_INCLUDED
#define __LINUX_SENSORS_CORE_H_INCLUDED

#include "sensors_axes.h"
#include <linux/input.h>

extern struct device *sensors_classdev_register(char *sensors_name);
extern void sensors_classdev_unregister(struct device *dev);
extern int sensors_register(struct device *dev,
	void *drvdata, struct device_attribute *attributes[], char *name);
extern void sensors_unregister(struct device *dev);
#if defined(CONFIG_SENSOR_USE_SYMLINK)
extern int sensors_initialize_symlink(struct input_dev *input_dev);
extern void sensors_delete_symlink(struct input_dev *input_dev);
#endif

void remap_sensor_data(s16 *, int);

struct accel_platform_data {
	u8 (*accel_get_position) (void);
	 /* Change axis or not for user-level
	 * If it is true, driver reports adjusted axis-raw-data
	 * to user-space based on accel_get_position() value,
	 * or if it is false, driver reports original axis-raw-data */
	bool axis_adjust;
	axes_func_s16 (*select_func) (u8);
	int *vibrator_on;
#ifdef CONFIG_SENSORS_K330
	bool *gyro_en;
#endif
};

struct gyro_platform_data {
	u8 (*gyro_get_position) (void);
	 /* Change axis or not for user-level
	 * If it is true, driver reports adjusted axis-raw-data
	 * to user-space based on gyro_get_position() value,
	 * or if it is false, driver reports original axis-raw-data */
	bool axis_adjust;
	axes_func_s16 (*select_func) (u8);
#ifdef CONFIG_SENSORS_K330
	bool *gyro_en;
#endif
};

#ifdef CONFIG_SENSORS_AK8963C
struct akm8963_platform_data {
	int gpio_data_ready_int;
	int gpio_reset;
	u8 (*mag_get_position) (void);
	axes_func_s16 (*select_func) (u8);
};
#endif
#endif	/* __LINUX_SENSORS_CORE_H_INCLUDED */
