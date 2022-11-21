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
#define CHIP_ID "LSM6DSL"
#define ST_PASS 1
#define ST_FAIL 0
#define ST_ZRO_MIN (-40)
#define ST_ZRO_MAX 40
#define SELFTEST_REVISED 1

#define RAWDATA_TIMER_MS 200
#define RAWDATA_TIMER_MARGIN_MS 20
#define ST_TIMEOUT_CNT 200

static ssize_t gyro_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t gyro_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static ssize_t selftest_revised_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", SELFTEST_REVISED);
}

static ssize_t gyro_power_off(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("[FACTORY]: %s\n", __func__);

	return snprintf(buf, PAGE_SIZE, "%d\n", 1);
}

static ssize_t gyro_power_on(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("[FACTORY]: %s\n", __func__);

	return snprintf(buf, PAGE_SIZE, "%d\n", 1);
}

static ssize_t gyro_temp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	struct msg_data message;
	int gyro_temp = -99;
	uint8_t cnt = 0;

	message.sensor_type = ADSP_FACTORY_GYRO_TEMP;
	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	data->selftest_ready_flag &= ~(1 << ADSP_FACTORY_GYRO_TEMP);
	adsp_unicast(&message, sizeof(message),
		NETLINK_MESSAGE_GYRO_TEMP, 0, 0);

	while (!(data->selftest_ready_flag & 1 << ADSP_FACTORY_GYRO_TEMP) &&
		cnt++ < TIMEOUT_CNT)
		msleep(20);

	data->selftest_ready_flag &= ~(1 << ADSP_FACTORY_GYRO_TEMP);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%d\n", gyro_temp);
	}

	gyro_temp = data->gyro_st_result.result1;
	pr_info("[FACTORY] %s: gyro_temp = %d\n", __func__, gyro_temp);

	return snprintf(buf, PAGE_SIZE, "%d\n", gyro_temp);
}

static ssize_t gyro_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	struct msg_data message;
	int gyro_fifo_avg[3] = {0,}, gyro_self_zro[3] = {0,};
	int gyro_self_bias[3] = {0,}, gyro_self_diff[3] = {0,};
	int st_diff_res = ST_FAIL;
	int st_zro_res = ST_FAIL;
	uint8_t cnt = 0;

	message.sensor_type = ADSP_FACTORY_GYRO;

	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	data->selftest_ready_flag &= ~(1 << ADSP_FACTORY_GYRO);
	adsp_unicast(&message, sizeof(message),
		NETLINK_MESSAGE_SELFTEST_SHOW_DATA, 0, 0);

	while (!(data->selftest_ready_flag & 1 << ADSP_FACTORY_GYRO) &&
		cnt++ < ST_TIMEOUT_CNT)
		msleep(20);

	data->selftest_ready_flag &= ~(1 << ADSP_FACTORY_GYRO);

	if (cnt >= ST_TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);

		return snprintf(buf, PAGE_SIZE,
			"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			gyro_fifo_avg[0], gyro_fifo_avg[1],
			gyro_fifo_avg[2], gyro_self_zro[0],
			gyro_self_zro[1], gyro_self_zro[2],
			gyro_self_bias[0], gyro_self_bias[1],
			gyro_self_bias[2], gyro_self_diff[0],
			gyro_self_diff[1], gyro_self_diff[2],
			ST_FAIL, ST_FAIL);
	}

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
		if (!data->gyro_st_result.result2)
			st_diff_res = ST_PASS;

		if (((ST_ZRO_MIN <= gyro_self_zro[0])
			&& (gyro_self_zro[0] <= ST_ZRO_MAX))
			&& ((ST_ZRO_MIN <= gyro_self_zro[1])
			&& (gyro_self_zro[1] <= ST_ZRO_MAX))
			&& ((ST_ZRO_MIN <= gyro_self_zro[2])
			&& (gyro_self_zro[2] <= ST_ZRO_MAX)))
			st_zro_res = ST_PASS;
        
		pr_info("[FACTORY]: %s - "
			"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
			gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2],
			gyro_self_zro[0], gyro_self_zro[1], gyro_self_zro[2],
			gyro_self_bias[0], gyro_self_bias[1], gyro_self_bias[2],
			gyro_self_diff[0], gyro_self_diff[1], gyro_self_diff[2],
			st_diff_res, st_zro_res);

		return snprintf(buf, PAGE_SIZE,
			"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2],
			gyro_self_zro[0], gyro_self_zro[1], gyro_self_zro[2],
			gyro_self_bias[0], gyro_self_bias[1], gyro_self_bias[2],
			gyro_self_diff[0], gyro_self_diff[1], gyro_self_diff[2],
			st_diff_res, st_zro_res);
	} else {
		pr_info("[FACTORY] %s - failed(%d, %d)\n", __func__,
			data->gyro_st_result.result1,
			data->gyro_st_result.result2);

		pr_info("[FACTORY]: %s - %d,%d,%d\n", __func__,
			gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2]);

		return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
			gyro_fifo_avg[0], gyro_fifo_avg[1], gyro_fifo_avg[2]);
	}
}

static DEVICE_ATTR(name, S_IRUGO, gyro_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, gyro_vendor_show, NULL);
static DEVICE_ATTR(selftest, S_IRUSR | S_IRGRP,
	gyro_selftest_show, NULL);
static DEVICE_ATTR(power_on, S_IRUGO, gyro_power_on, NULL);
static DEVICE_ATTR(power_off, S_IRUGO, gyro_power_off, NULL);
static DEVICE_ATTR(temperature, S_IRUSR | S_IRGRP,
	gyro_temp_show, NULL);
static DEVICE_ATTR(selftest_revised, 0440, selftest_revised_show, NULL);

static struct device_attribute *gyro_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_selftest,
	&dev_attr_power_on,
	&dev_attr_power_off,
	&dev_attr_temperature,
	&dev_attr_selftest_revised,
	NULL,
};

static int __init lsm6dsl_gyro_factory_init(void)
{
	adsp_factory_register(ADSP_FACTORY_GYRO, gyro_attrs);

	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit lsm6dsl_gyro_factory_exit(void)
{
	adsp_factory_unregister(ADSP_FACTORY_GYRO);

	pr_info("[FACTORY] %s\n", __func__);
}

module_init(lsm6dsl_gyro_factory_init);
module_exit(lsm6dsl_gyro_factory_exit);
