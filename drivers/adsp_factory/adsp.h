/*
*  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
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
*
*/
#ifndef __ADSP_SENSOR_H__
#define __ADSP_SENSOR_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sensors.h>
#include <linux/adsp/adsp_ft_common.h>
#define TIMEOUT_CNT 50

/* Main struct containing all the data */
struct adsp_data {
	struct device *adsp;
	struct device *sensor_device[ADSP_FACTORY_SENSOR_MAX];
	struct sensor_on_off_result sensor_onoff_result[ADSP_FACTORY_SENSOR_MAX];
	struct device_attribute **sensor_attr[ADSP_FACTORY_SENSOR_MAX];
	struct sock *adsp_skt;
	unsigned int onoff_flag;
	void *pdata;
	bool sysfs_created[ADSP_FACTORY_SENSOR_MAX];
	struct mutex remove_sysfs_mutex;
};

int adsp_factory_register(unsigned int type, struct device_attribute *attributes[]);
int adsp_factory_unregister(unsigned int type);
int adsp_unicast(void *param, int param_size, int type, u32 portid, int flags);
#endif
