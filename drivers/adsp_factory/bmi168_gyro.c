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
#define VENDOR "BOSCH"
#define CHIP_ID "BMI168"
#define GYRO_SELFTEST_TRY_CNT	7

#define RAWDATA_TIMER_MS 200
#define RAWDATA_TIMER_MARGIN_MS	20
#define SELFTEST_MAX_LIMITATION (172)
#define SELFTEST_MIN_LIMITATION SELFTEST_MAX_LIMITATION * -1

s64 get_time_nanossec(void)
{
	struct timespec ts;
	ktime_get_ts(&ts);
	return timespec_to_ns(&ts);
}

static ssize_t gyro_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t gyro_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}

static ssize_t gyro_raw_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);

	if (adsp_start_raw_data(ADSP_FACTORY_GYRO) == false)
		return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", 0, 0, 0);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->sensor_data[ADSP_FACTORY_GYRO].x,
		data->sensor_data[ADSP_FACTORY_GYRO].y,
		data->sensor_data[ADSP_FACTORY_GYRO].z);
}

static ssize_t gyro_power_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 1);
}

static ssize_t gyro_temp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	unsigned long timeout;
	struct msg_data message;
	short gyro_temperature = 0;

	message.sensor_type = ADSP_FACTORY_GYRO_TEMP;

	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_GYRO_TEMP, 0, 0);
	timeout = jiffies + (10 * HZ);

	while (!(data->selftest_ready_flag & 1 << ADSP_FACTORY_GYRO_TEMP)) {
		msleep(20);
		if (time_after(jiffies, timeout)) {
			pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
			return snprintf(buf, PAGE_SIZE, "%d\n", 0);
		}
	}

	data->selftest_ready_flag &= 0 << ADSP_FACTORY_GYRO_TEMP;

	gyro_temperature = 
		(short)data->sensor_selftest_result[ADSP_FACTORY_GYRO_TEMP].gyro_temp;

	pr_info("[FACTORY] %s: temperature = %d\n", __func__,
		gyro_temperature);
	return sprintf(buf, "%d\n", gyro_temperature);
}

static ssize_t gyro_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	u8 bist = 0, zro_result = 0;
	int data_check[3] = { 0, };
	unsigned long timeout;
	struct msg_data message;

	message.sensor_type = ADSP_FACTORY_GYRO;

	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_SELFTEST_SHOW_DATA, 0, 0);
	timeout = jiffies + (10 * HZ);

	while (!(data->selftest_ready_flag & 1 << ADSP_FACTORY_GYRO)) {
		msleep(20);
		if (time_after(jiffies, timeout)) {
			pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
			return snprintf(buf, PAGE_SIZE, "%d, %d, %d, %d, %d\n",
				-1, -1, 0, 0, 0);
		}
	}

	data->selftest_ready_flag &= 0 << ADSP_FACTORY_GYRO;
	/* bist: 1 -> pass, 0 -> fail */
	bist = data->sensor_selftest_result[ADSP_FACTORY_GYRO].result1;
	/* zro_result: 0: pass */
	zro_result = data->sensor_selftest_result[ADSP_FACTORY_GYRO].result2;

	data_check[0] = data->sensor_selftest_result[ADSP_FACTORY_GYRO].bias_x;
	data_check[1] =	data->sensor_selftest_result[ADSP_FACTORY_GYRO].bias_y;
	data_check[2] =	data->sensor_selftest_result[ADSP_FACTORY_GYRO].bias_z;

	pr_info("[FACTORY] %s: bist = %u, zro_result = %u\n",
		__func__, bist, zro_result);

	pr_info("[FACTORY] %s: X = %d, Y = %d, Z = %d\n", __func__,
		data_check[0], data_check[1], data_check[2]);

	if ((data_check[0] <= SELFTEST_MAX_LIMITATION)
		&& (data_check[1] <= SELFTEST_MAX_LIMITATION)
		&& (data_check[2] <= SELFTEST_MAX_LIMITATION)
		&& (data_check[0] >= SELFTEST_MIN_LIMITATION)
		&& (data_check[1] >= SELFTEST_MIN_LIMITATION)
		&& (data_check[2] >= SELFTEST_MIN_LIMITATION)) {
		pr_info("[FACTORY] %s : Gyro zero rate OK!- Pass\n", __func__);
		/* save_gyro_caldata(data, iCalData); */
	} else {
		pr_info("[FACTORY] %s : Gyro zero rate NG!- Fail!\n", __func__);
		zro_result |= 1;
	}

	pr_info("[SSP] %s - %u,%u,%d.%03d,%d.%03d,%d.%03d\n", __func__,
			bist, zro_result,
			(data_check[0] / 1000), (int)abs(data_check[0] % 1000),
			(data_check[1] / 1000), (int)abs(data_check[1] % 1000),
			(data_check[2] / 1000), (int)abs(data_check[2] % 1000));

	return sprintf(buf, "%d,%d,%d.%03d,%d.%03d,%d.%03d\n",
			(int)bist, (int)zro_result,
			(data_check[0] / 1000), (int)abs(data_check[0] % 1000),
			(data_check[1] / 1000), (int)abs(data_check[1] % 1000),
			(data_check[2] / 1000), (int)abs(data_check[2] % 1000));
}

static DEVICE_ATTR(name, S_IRUGO, gyro_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, gyro_vendor_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, gyro_raw_data_read, NULL);
static DEVICE_ATTR(selftest, S_IRUSR | S_IRGRP,
	gyro_selftest_show, NULL);
static DEVICE_ATTR(temperature, S_IRUSR | S_IRGRP,
	gyro_temp_show, NULL);
static DEVICE_ATTR(power_on, S_IRUSR | S_IRGRP,
	gyro_power_show, NULL);

static struct device_attribute *gyro_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_raw_data,
	&dev_attr_selftest,
	&dev_attr_temperature,
	&dev_attr_power_on,
	NULL,
};

static int __init bmi168_gyro_factory_init(void)
{
	adsp_factory_register(ADSP_FACTORY_GYRO, gyro_attrs);

	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit bmi168_gyro_factory_exit(void)
{
	adsp_factory_unregister(ADSP_FACTORY_GYRO);

	pr_info("[FACTORY] %s\n", __func__);
}

module_init(bmi168_gyro_factory_init);
module_exit(bmi168_gyro_factory_exit);
