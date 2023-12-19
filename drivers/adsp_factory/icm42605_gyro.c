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
#define VENDOR "TDK"
#define CHIP_ID "ICM42605"
#define ST_PASS 1
#define ST_FAIL 0
#define ST_MAX_DPS 10
#define ST_MIN_DPS -10
#define ST_MIN_RATIO 50
/* Scale Factor = 32768lsb(MAX) / 250dps(ST_FS) */
#define ST_SENS_DEF 131 
#define SELFTEST_REVISED 1

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
	uint8_t cnt = 0;

	adsp_unicast(NULL, 0, MSG_GYRO_TEMP, 0, MSG_TYPE_GET_RAW_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_RAW_DATA] & 1 << MSG_GYRO_TEMP)
		&& cnt++ < TIMEOUT_CNT)
		msleep(20);

	data->ready_flag[MSG_TYPE_GET_RAW_DATA] &= ~(1 << MSG_GYRO_TEMP);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "-99\n");
	}

	pr_info("[FACTORY] %s: gyro_temp = %d\n", __func__,
		data->msg_buf[MSG_GYRO_TEMP][0]);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		data->msg_buf[MSG_GYRO_TEMP][0]);
}

static ssize_t gyro_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint8_t cnt = 0;
	int st_nost_data[3] = {0,};
	int st_data[3] = {0,};
	int st_fifo_data[3] = {0,};
	int st_zro_res = ST_FAIL;
	int st_ratio_res = ST_FAIL;

	pr_info("[FACTORY] %s - start", __func__);
	adsp_unicast(NULL, 0, MSG_GYRO, 0, MSG_TYPE_ST_SHOW_DATA);

	while (!(data->ready_flag[MSG_TYPE_ST_SHOW_DATA] & 1 << MSG_GYRO) &&
		cnt++ < TIMEOUT_CNT)
		msleep(20);

	data->ready_flag[MSG_TYPE_ST_SHOW_DATA] &= ~(1 << MSG_GYRO);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE,
			"0,0,0,0,0,0,0,0,0,0,0,0,%d,%d\n",
			ST_FAIL, ST_FAIL);
	}

	/* TDK : selftest fifo data check */
	st_fifo_data[0] = data->msg_buf[MSG_GYRO][2] / ST_SENS_DEF;
	st_fifo_data[1] = data->msg_buf[MSG_GYRO][3] / ST_SENS_DEF;
	st_fifo_data[2] = data->msg_buf[MSG_GYRO][4] / ST_SENS_DEF;

	/* TDK : selftest zro(NOST) check */
	st_nost_data[0] = data->msg_buf[MSG_GYRO][6] / ST_SENS_DEF;
	st_nost_data[1] = data->msg_buf[MSG_GYRO][7] / ST_SENS_DEF;
	st_nost_data[2] = data->msg_buf[MSG_GYRO][8] / ST_SENS_DEF;

	if((ST_MIN_DPS <= st_nost_data[0]) && (st_nost_data[0] <= ST_MAX_DPS)
		&& (ST_MIN_DPS <= st_nost_data[1]) && (st_nost_data[1] <= ST_MAX_DPS)
		&& (ST_MIN_DPS <= st_nost_data[2]) && (st_nost_data[2] <= ST_MAX_DPS))
		st_zro_res = ST_PASS;

	/* TDK : selftest ST data */
	st_data[0] = data->msg_buf[MSG_GYRO][9] / ST_SENS_DEF;
	st_data[1] = data->msg_buf[MSG_GYRO][10] / ST_SENS_DEF;
	st_data[2] = data->msg_buf[MSG_GYRO][11] / ST_SENS_DEF;

	/* TDK : selftest ratio check */
	if(ST_MIN_RATIO < data->msg_buf[MSG_GYRO][12]
		&& ST_MIN_RATIO < data->msg_buf[MSG_GYRO][13]
		&& ST_MIN_RATIO < data->msg_buf[MSG_GYRO][14])
		st_ratio_res = ST_PASS;

	if (data->msg_buf[MSG_GYRO][1] == ST_FAIL) {
		pr_info("[FACTORY]: %s - %d,%d,%d\n", __func__,
			st_fifo_data[0], st_fifo_data[1], st_fifo_data[2]);

		return snprintf(buf, PAGE_SIZE,	"%d,%d,%d\n",
			st_fifo_data[0], st_fifo_data[1], st_fifo_data[2]);
	} else {
		pr_info("[FACTORY]: %s - %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			__func__,
			st_fifo_data[0], st_fifo_data[1], st_fifo_data[2],
			st_nost_data[0], st_nost_data[1], st_nost_data[2],
			st_data[0], st_data[1], st_data[2],
			data->msg_buf[MSG_GYRO][12],
			data->msg_buf[MSG_GYRO][13], data->msg_buf[MSG_GYRO][14],
			st_ratio_res, st_zro_res);

		return snprintf(buf, PAGE_SIZE,
			"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			st_fifo_data[0], st_fifo_data[1], st_fifo_data[2],
			st_nost_data[0], st_nost_data[1], st_nost_data[2],
			st_data[0], st_data[1], st_data[2],
			data->msg_buf[MSG_GYRO][12],
			data->msg_buf[MSG_GYRO][13], data->msg_buf[MSG_GYRO][14],
			st_ratio_res, st_zro_res);
	}
}

static DEVICE_ATTR(name, 0444, gyro_name_show, NULL);
static DEVICE_ATTR(vendor, 0444, gyro_vendor_show, NULL);
static DEVICE_ATTR(selftest, 0440, gyro_selftest_show, NULL);
static DEVICE_ATTR(power_on, 0444, gyro_power_on, NULL);
static DEVICE_ATTR(power_off, 0444, gyro_power_off, NULL);
static DEVICE_ATTR(temperature, 0440, gyro_temp_show, NULL);
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
	adsp_factory_register(MSG_GYRO, gyro_attrs);

	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit lsm6dsl_gyro_factory_exit(void)
{
	adsp_factory_unregister(MSG_GYRO);

	pr_info("[FACTORY] %s\n", __func__);
}

module_init(lsm6dsl_gyro_factory_init);
module_exit(lsm6dsl_gyro_factory_exit);
