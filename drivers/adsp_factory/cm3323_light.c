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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/dirent.h>
#include "adsp.h"
#define VENDOR "CAPELLA"
#define CHIP_ID "CM3323"

/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/
static ssize_t light_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t light_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static ssize_t light_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);

	if (adsp_start_raw_data(ADSP_FACTORY_LIGHT) == false)
		return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d\n",
				0, 0, 0, 0, 0, 0);

	return snprintf(buf, PAGE_SIZE, "%u,%u,%u,%u,%u,%u\n",
		data->sensor_data[ADSP_FACTORY_LIGHT].r,
		data->sensor_data[ADSP_FACTORY_LIGHT].g,
		data->sensor_data[ADSP_FACTORY_LIGHT].b,
		data->sensor_data[ADSP_FACTORY_LIGHT].w,
		data->sensor_data[ADSP_FACTORY_LIGHT].a_time,
		data->sensor_data[ADSP_FACTORY_LIGHT].a_gain);
}

static ssize_t light_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);

	if (adsp_start_raw_data(ADSP_FACTORY_LIGHT) == false)
		return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d\n",
				0, 0, 0, 0, 0, 0);

	return snprintf(buf, PAGE_SIZE, "%u,%u,%u,%u,%u,%u\n",
		data->sensor_data[ADSP_FACTORY_LIGHT].r,
		data->sensor_data[ADSP_FACTORY_LIGHT].g,
		data->sensor_data[ADSP_FACTORY_LIGHT].b,
		data->sensor_data[ADSP_FACTORY_LIGHT].w,
		data->sensor_data[ADSP_FACTORY_LIGHT].a_time,
		data->sensor_data[ADSP_FACTORY_LIGHT].a_gain);
}


static DEVICE_ATTR(vendor, S_IRUGO, light_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, light_name_show, NULL);
static DEVICE_ATTR(lux, S_IRUGO, light_lux_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, light_data_show, NULL);

static struct device_attribute *light_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_lux,
	&dev_attr_raw_data,
	NULL,
};

static int __init cm3323_light_factory_init(void)
{
	adsp_factory_register(ADSP_FACTORY_LIGHT, light_attrs);
	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit cm3323_light_factory_exit(void)
{
	adsp_factory_unregister(ADSP_FACTORY_LIGHT);
	pr_info("[FACTORY] %s\n", __func__);
}
module_init(cm3323_light_factory_init);
module_exit(cm3323_light_factory_exit);

