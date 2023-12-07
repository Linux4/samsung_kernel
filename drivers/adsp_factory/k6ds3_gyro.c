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
#define VENDOR "STM"
#define CHIP_ID "K6DS3TR"
#define GYRO_SELFTEST_TRY_CNT	7

#define RAWDATA_TIMER_MS 200
#define RAWDATA_TIMER_MARGIN_MS	20
#define SELFTEST_MAX_LIMITATION (172)
#define SELFTEST_MIN_LIMITATION SELFTEST_MAX_LIMITATION * -1

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

static ssize_t gyro_power_off(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("[FACTORY]: %s\n", __func__);

	return sprintf(buf, "%d\n", 1);
}

static ssize_t gyro_power_on(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("[FACTORY]: %s\n", __func__);

	return sprintf(buf, "%d\n", 1);
}

static ssize_t gyro_temp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	unsigned long timeout;
	struct msg_data message;
	int gyro_temp = -99;

	message.sensor_type = ADSP_FACTORY_GYRO_TEMP;
	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_GYRO_TEMP, 0, 0);

	timeout = jiffies + (10 * HZ);
	while (!(data->selftest_ready_flag & 1 << ADSP_FACTORY_GYRO_TEMP)) {
		msleep(20);
		if (time_after(jiffies, timeout)) {
			pr_info("[FACTORY] %s: Timeout!!!\n", __func__);
			return -1;
		}
	}

	data->selftest_ready_flag &= 0 << ADSP_FACTORY_GYRO_TEMP;
	gyro_temp = data->gyro_st_result.result1;
	pr_info("[FACTORY] %s: gyro_temp = %d\n", __func__, gyro_temp);

	return sprintf(buf, "%d\n", gyro_temp);
}

static ssize_t gyro_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	unsigned long timeout;
	struct msg_data message;
	int gyro_fifo_avg[3] = {0,}, gyro_self_zro[3] = {0,};
	int gyro_self_bias[3] = {0,}, gyro_self_diff[3] = {0,};
	int fifo_ret = 0;
	int cal_ret = 0;

	message.sensor_type = ADSP_FACTORY_GYRO;

	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_SELFTEST_SHOW_DATA, 0, 0);
	timeout = jiffies + (10 * HZ);

	while (!(data->selftest_ready_flag & 1 << ADSP_FACTORY_GYRO)) {
		msleep(20);
		if (time_after(jiffies, timeout)) {
			pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
			return sprintf(buf,
				"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
				gyro_fifo_avg[0], gyro_fifo_avg[1],
				gyro_fifo_avg[2], gyro_self_zro[0],
				gyro_self_zro[1], gyro_self_zro[2],
				gyro_self_bias[0], gyro_self_bias[1],
				gyro_self_bias[2], gyro_self_diff[0],
				gyro_self_diff[1], gyro_self_diff[2],
				fifo_ret, cal_ret);
		}
	}

	data->selftest_ready_flag &= 0 << ADSP_FACTORY_GYRO;

	gyro_fifo_avg[0] = data->gyro_st_result.fifo_zro_x;
	gyro_fifo_avg[1] = data->gyro_st_result.fifo_zro_y;
	gyro_fifo_avg[2] = data->gyro_st_result.fifo_zro_z;

	gyro_self_zro[0] = data->gyro_st_result.nost_x / 1000;
	gyro_self_zro[1] = data->gyro_st_result.nost_y / 1000;
	gyro_self_zro[2] = data->gyro_st_result.nost_z / 1000;

	gyro_self_bias[0] = data->gyro_st_result.st_x / 1000;
	gyro_self_bias[1] = data->gyro_st_result.st_y / 1000;
	gyro_self_bias[2] = data->gyro_st_result.st_z / 1000;

	gyro_self_diff[0] = data->gyro_st_result.st_diff_x / 1000;
	gyro_self_diff[1] = data->gyro_st_result.st_diff_y / 1000;
	gyro_self_diff[2] = data->gyro_st_result.st_diff_z / 1000;

	if (data->gyro_st_result.result1 == 0) {
		fifo_ret = 1;
		cal_ret = 1;
	} else
		pr_info("[FACTORY] %s - failed(%d, %d)\n", __func__,
			data->gyro_st_result.result1,
			data->gyro_st_result.result2);

	pr_info("[FACTORY]: %s - "
		"%d,%d,%d,"
		"%d,%d,%d,"
		"%d,%d,%d,"
		"%d,%d,%d,%d,%d\n", __func__,
		gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2],
		gyro_self_zro[0], gyro_self_zro[1], gyro_self_zro[2],
		gyro_self_bias[0], gyro_self_bias[1], gyro_self_bias[2],
		gyro_self_diff[0], gyro_self_diff[1], gyro_self_diff[2],
		fifo_ret,
		cal_ret);

	return sprintf(buf,
		"%d,%d,%d,"
		"%d,%d,%d,"
		"%d,%d,%d,"
		"%d,%d,%d,%d,%d\n",
		gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2],
		gyro_self_zro[0], gyro_self_zro[1], gyro_self_zro[2],
		gyro_self_bias[0], gyro_self_bias[1], gyro_self_bias[2],
		gyro_self_diff[0], gyro_self_diff[1], gyro_self_diff[2],
		fifo_ret,
		cal_ret);
}

static DEVICE_ATTR(name, S_IRUGO, gyro_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, gyro_vendor_show, NULL);
static DEVICE_ATTR(selftest, S_IRUSR | S_IRGRP,
	gyro_selftest_show, NULL);
static DEVICE_ATTR(power_on, S_IRUGO, gyro_power_on, NULL);
static DEVICE_ATTR(power_off, S_IRUGO, gyro_power_off, NULL);
static DEVICE_ATTR(temperature, S_IRUSR | S_IRGRP,
	gyro_temp_show, NULL);

static struct device_attribute *gyro_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_selftest,
	&dev_attr_power_on,
	&dev_attr_power_off,
	&dev_attr_temperature,
	NULL,
};

static int __init k6ds3_gyro_factory_init(void)
{
	adsp_factory_register(ADSP_FACTORY_GYRO, gyro_attrs);

	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit k6ds3_gyro_factory_exit(void)
{
	adsp_factory_unregister(ADSP_FACTORY_GYRO);

	pr_info("[FACTORY] %s\n", __func__);
}

module_init(k6ds3_gyro_factory_init);
module_exit(k6ds3_gyro_factory_exit);
