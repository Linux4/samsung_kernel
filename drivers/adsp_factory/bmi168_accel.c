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

#define RAWDATA_TIMER_MS 200
#define RAWDATA_TIMER_MARGIN_MS	20
#define ACCEL_SELFTEST_TRY_CNT 7

static ssize_t accel_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t accel_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}

static ssize_t sensor_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", "ADSP");
}

static ssize_t accel_calibration_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int iCount = 0;

	struct msg_data message;
	message.sensor_type = ADSP_FACTORY_ACCEL;
	adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_GET_CALIB_DATA, 0, 0);

	while (!(data->calib_ready_flag & 1 << ADSP_FACTORY_ACCEL))
		msleep(20);

	data->calib_ready_flag &= 0 << ADSP_FACTORY_ACCEL;

	pr_info("[FACTORY] %s: %d,%d,%d,%d\n", __func__,
		data->sensor_calib_data[ADSP_FACTORY_ACCEL].result,
		data->sensor_calib_data[ADSP_FACTORY_ACCEL].x,
		data->sensor_calib_data[ADSP_FACTORY_ACCEL].y,
		data->sensor_calib_data[ADSP_FACTORY_ACCEL].z);

	iCount = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d\n",
		data->sensor_calib_data[ADSP_FACTORY_ACCEL].result,
		data->sensor_calib_data[ADSP_FACTORY_ACCEL].x,
		data->sensor_calib_data[ADSP_FACTORY_ACCEL].y,
		data->sensor_calib_data[ADSP_FACTORY_ACCEL].z);
	return iCount;
}

static ssize_t accel_calibration_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
#if 1
	struct msg_data message;
	unsigned long enable = 0;
	struct adsp_data *data = dev_get_drvdata(dev);

	if (strict_strtoul(buf, 10, &enable)) {
		pr_err("[FACTORY] %s: strict_strtoul fail\n", __func__);
		return -EINVAL;
	}
	if (enable > 0)
		enable = 1;

	message.sensor_type = ADSP_FACTORY_ACCEL;
	message.param1 = enable;
	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_CALIB_STORE_DATA, 0, 0);

	while (!(data->calib_store_ready_flag & 1 << ADSP_FACTORY_ACCEL))
		msleep(20);

	if (data->sensor_calib_result[ADSP_FACTORY_ACCEL].result < 0)
		pr_err("[FACTORY] %s: failed\n", __func__);

	data->calib_store_ready_flag |= 0 << ADSP_FACTORY_ACCEL;

	pr_info("[FACTORY] %s: result(%d)\n", __func__,
		data->sensor_calib_result[ADSP_FACTORY_ACCEL].result);
#else
	struct adsp_data *data = dev_get_drvdata(dev);
	struct msg_data message;
	message.sensor_type = ADSP_FACTORY_ACCEL;
	adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_CALIB_STORE_DATA, 0, 0);

	while (!(data->calib_store_ready_flag & 1 << ADSP_FACTORY_ACCEL))
		msleep(20);

	if (data->sensor_calib_result[ADSP_FACTORY_ACCEL].result < 0)
		pr_err("[FACTORY] %s: accel_do_calibrate() failed\n", __func__);

	data->calib_store_ready_flag |= 0 << ADSP_FACTORY_ACCEL;

	pr_info("[FACTORY] %s: result(%d)\n", __func__,
		data->sensor_calib_result[ADSP_FACTORY_ACCEL].result);
#endif
	return size;
}

static ssize_t accel_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int init_status = 0;
	int accel_result = 0;
	unsigned long timeout;
	struct msg_data message;
	int retry = 0;

retry_accel_selftest:
	message.sensor_type = ADSP_FACTORY_ACCEL;

	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);

	adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_SELFTEST_SHOW_DATA, 0, 0);

	timeout = jiffies + (20 * HZ);

	while (!(data->selftest_ready_flag & 1 << ADSP_FACTORY_ACCEL)) {
		msleep(20);
		if (time_after(jiffies, timeout)) {
			pr_info("[FACTORY] %s: Timeout!!!\n", __func__);
			return sprintf(buf, "%d,%d,%d,%d\n", -5, 0, 0, 0);
		}
	}

	if (data->sensor_selftest_result[ADSP_FACTORY_ACCEL].result1 < 0)
		pr_err("[FACTORY] %s: accel_do_calibrate() failed\n", __func__);

	data->selftest_ready_flag &= 0 << ADSP_FACTORY_ACCEL;
	init_status = data->sensor_selftest_result[ADSP_FACTORY_ACCEL].result1;
	accel_result = data->sensor_selftest_result[ADSP_FACTORY_ACCEL].result2;

	if (accel_result != 1)
         	pr_info("[FACTORY] %s : Accel Selftest OK!, result = %d, retry = %d\n",
         		__func__,accel_result, retry);
	else
           	pr_info("[FACTORY] %s : Accel Selftest Fail!, result = %d, retry = %d\n",
           		__func__, accel_result, retry);

	pr_info("[FACTORY] init = %d, result = %d, X = %d, Y = %d, Z = %d\n",
         	init_status,
         	accel_result,
		(int)(data->sensor_selftest_result[ADSP_FACTORY_ACCEL].ratio_x),
		(int)(data->sensor_selftest_result[ADSP_FACTORY_ACCEL].ratio_y),
		(int)(data->sensor_selftest_result[ADSP_FACTORY_ACCEL].ratio_z));

	if (accel_result != 1) {
		if (retry < ACCEL_SELFTEST_TRY_CNT && data->sensor_selftest_result[ADSP_FACTORY_ACCEL].ratio_x == 0) {
			retry++;
			msleep(RAWDATA_TIMER_MS * 2);
			goto retry_accel_selftest;
		}
	}

	return sprintf(buf, "%d,%d,%d,%d\n", (int)abs(data->sensor_selftest_result[ADSP_FACTORY_ACCEL].result2),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_ACCEL].ratio_x),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_ACCEL].ratio_y),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_ACCEL].ratio_z));
}

static ssize_t accel_raw_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);

	if (adsp_start_raw_data(ADSP_FACTORY_ACCEL) == false)
		return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", 0, 0, 0);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->sensor_data[ADSP_FACTORY_ACCEL].x,
		data->sensor_data[ADSP_FACTORY_ACCEL].y,
		data->sensor_data[ADSP_FACTORY_ACCEL].z);
}

static ssize_t accel_reactive_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int alert_val = 0;
	pr_info("[FACTORY] %s: alert_val %d\n",
		__func__, alert_val);

	return snprintf(buf, PAGE_SIZE, "%d\n", alert_val);
}


static ssize_t accel_reactive_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long enable = 0;
	pr_info("[FACTORY] %s: enable = %ld\n",
		__func__, enable);

	return size;
}

static DEVICE_ATTR(name, S_IRUGO, accel_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, accel_vendor_show, NULL);
static DEVICE_ATTR(type, S_IRUGO, sensor_type_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
	accel_calibration_show, accel_calibration_store);
static DEVICE_ATTR(selftest, S_IRUSR | S_IRGRP,
	accel_selftest_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, accel_raw_data_read, NULL);
static DEVICE_ATTR(reactive_alert, S_IRUGO | S_IWUSR | S_IWGRP,
		accel_reactive_show, accel_reactive_store);

static struct device_attribute *acc_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_type,
	&dev_attr_calibration,
	&dev_attr_selftest,
	&dev_attr_raw_data,
	&dev_attr_reactive_alert,
	NULL,
};

static int __init bmi168_accel_factory_init(void)
{
	adsp_factory_register(ADSP_FACTORY_ACCEL, acc_attrs);

	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit bmi168_accel_factory_exit(void)
{
	adsp_factory_unregister(ADSP_FACTORY_ACCEL);

	pr_info("[FACTORY] %s\n", __func__);
}
module_init(bmi168_accel_factory_init);
module_exit(bmi168_accel_factory_exit);
