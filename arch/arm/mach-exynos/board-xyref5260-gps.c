/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/stat.h>
#include <linux/string.h>

#include <plat/gpio-cfg.h>

#include "board-xyref5260.h"

#define GPIO_GPS_EN EXYNOS5260_GPD0(2)
#define GPIO_GPS_RST_N EXYNOS5260_GPD0(3)
#define GPIO_GPS_POWER_EN EXYNOS5260_GPA2(3)

static ssize_t gps_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	if (!strcmp(attr->attr.name, "gps_on_off"))
		return snprintf(buf, 4, "%d\n", gpio_get_value(GPIO_GPS_EN));
	else if (!strcmp(attr->attr.name, "gps_nrst"))
		return snprintf(buf, 4, "%d\n", gpio_get_value(GPIO_GPS_RST_N));
	else
		return snprintf(buf, 4, "%d\n", gpio_get_value(GPIO_GPS_POWER_EN));
}

static ssize_t gps_store(struct device *dev, struct device_attribute *attr,
			    const char *buff, size_t size)
{
	int gpio, gpio_value;

	if (!strcmp(attr->attr.name, "gps_on_off"))
		gpio = GPIO_GPS_EN;
	else if (!strcmp(attr->attr.name, "gps_nrst"))
		gpio = GPIO_GPS_RST_N;
	else
		gpio = GPIO_GPS_POWER_EN;

	if (!sscanf(buff, "%d", &gpio_value))
		return -EINVAL;

	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	gpio_set_value(gpio, gpio_value);

	return size;
}

static DEVICE_ATTR(gps_on_off, S_IRUGO | S_IWUSR | S_IWGRP, gps_show, gps_store);
static DEVICE_ATTR(gps_nrst, S_IRUGO | S_IWUSR | S_IWGRP, gps_show, gps_store);
static DEVICE_ATTR(gps_pwr_en, S_IRUGO | S_IWUSR | S_IWGRP, gps_show, gps_store);

static struct platform_device gps_device = {
	.name		= "csr_gps",
	.id		= -1,
};

void __init exynos5_xyref5260_gps_init(void)
{
	platform_device_register(&gps_device);

	device_create_file(&gps_device.dev, &dev_attr_gps_on_off);
	device_create_file(&gps_device.dev, &dev_attr_gps_nrst);
	device_create_file(&gps_device.dev, &dev_attr_gps_pwr_en);
}
