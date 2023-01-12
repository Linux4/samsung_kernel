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
#include "adsp.h"

#define VENDOR "SEMTECH"
#define CHIP_ID "SX93xx"

#define BUFFER_MAX      128

struct sx93xx_grip_data {
	int onoff;
};
static struct sx93xx_grip_data *pdata;

static ssize_t grip_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t grip_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static ssize_t grip_onoff_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct msg_data message;
	int val = 0;

	if (kstrtoint(buf, 10, &val)) {
		pr_err("[FACTORY] %s: Invalid Argument\n", __func__);
		return -EINVAL;
	}
	if (val > 0)
		val = 1;

	message.sensor_type = ADSP_FACTORY_GRIP;
	message.param1 = val;
	msleep(50);
	if(val > 0)
	{
		pdata->onoff = 1;
		adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_GRIP_ON, 0, 0);
	}
	else
	{
		pdata->onoff = 0;
		adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_GRIP_OFF, 0, 0);
	}

	pr_info("[FACTORY] %s: onoff(%d)\n", __func__, pdata->onoff);

	return size;
}

static ssize_t grip_onoff_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",	pdata->onoff);
}

static DEVICE_ATTR(vendor, S_IRUGO, grip_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, grip_name_show, NULL);
static DEVICE_ATTR(onoff, S_IRUGO | S_IWUSR | S_IWGRP,
	grip_onoff_show, grip_onoff_store);

static struct device_attribute *grip_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_onoff,
	NULL,
};

static int __init sx93xx_grip_factory_init(void)
{
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	adsp_factory_register(ADSP_FACTORY_GRIP, grip_attrs);
	pr_info("[FACTORY] %s\n", __func__);

	pdata->onoff = 1;
	return 0;
}

static void __exit sx93xx_grip_factory_exit(void)
{
	adsp_factory_unregister(ADSP_FACTORY_GRIP);
	kfree(pdata);
	pr_info("[FACTORY] %s\n", __func__);
}

module_init(sx93xx_grip_factory_init);
module_exit(sx93xx_grip_factory_exit);
