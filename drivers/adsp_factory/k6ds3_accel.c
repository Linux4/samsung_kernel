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
#ifdef CONFIG_ADSP_MOTOR
#include <linux/adsp/slpi_motor.h>
#endif
#define VENDOR "STM"
#define CHIP_ID "K6DS3TR"

#define RAWDATA_TIMER_MS 200
#define RAWDATA_TIMER_MARGIN_MS	20
#define ACCEL_SELFTEST_TRY_CNT 3

/* Haptic Pattern A vibrate during 7ms.
 * touch, touchkey, operation feedback use this.
 * Do not call motor_workfunc when duration is 7ms.
 */
#define DURATION_SKIP 10
#define MOTOR_OFF 0

#ifdef CONFIG_ADSP_MOTOR
struct accel_motor_data {
	struct workqueue_struct *slpi_motor_wq;
	struct work_struct work_slpi_motor;
	int motor_state;
};

struct accel_motor_data *pdata;

void slpi_motor_work_func(struct work_struct *work);
#endif
static ssize_t accel_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t accel_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static ssize_t sensor_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", "ADSP");
}

static ssize_t accel_calibration_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	struct msg_data message;
	int iCount = 0;
	uint8_t cnt = 0;

	message.sensor_type = ADSP_FACTORY_ACCEL;
	data->calib_ready_flag &= ~(1 << ADSP_FACTORY_ACCEL);
	adsp_unicast(&message, sizeof(message),
		NETLINK_MESSAGE_GET_CALIB_DATA, 0, 0);

	while (!(data->calib_ready_flag & 1 << ADSP_FACTORY_ACCEL) &&
		cnt++ < TIMEOUT_CNT)
		msleep(20);

	data->calib_ready_flag &= ~(1 << ADSP_FACTORY_ACCEL);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d\n", -1, 0, 0, 0);
	}

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
	struct msg_data message;
	struct adsp_data *data = dev_get_drvdata(dev);
	unsigned long enable = 0;
	uint8_t cnt = 0;

	if (kstrtoul(buf, 10, &enable)) {
		pr_err("[FACTORY] %s: strict_strtoul fail\n", __func__);
		return -EINVAL;
	}

	if (enable > 0)
		enable = 1;

	message.sensor_type = ADSP_FACTORY_ACCEL;
	message.param1 = enable;
	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	data->calib_store_ready_flag &= ~(1 << ADSP_FACTORY_ACCEL);
	adsp_unicast(&message, sizeof(message),
		NETLINK_MESSAGE_CALIB_STORE_DATA, 0, 0);

	while (!(data->calib_store_ready_flag & 1 << ADSP_FACTORY_ACCEL) &&
		cnt++ < TIMEOUT_CNT)
		msleep(20);

	data->calib_store_ready_flag &= ~(1 << ADSP_FACTORY_ACCEL);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return size;
	}

	if (data->sensor_calib_result[ADSP_FACTORY_ACCEL].result < 0)
		pr_info("[FACTORY] %s: failed\n", __func__);

	pr_info("[FACTORY] %s: result(%d)\n", __func__,
		data->sensor_calib_result[ADSP_FACTORY_ACCEL].result);

	return size;
}

static ssize_t accel_selftest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	struct msg_data message;
	int init_status = 0;
	int accel_result = 0;
	int temp[4] = {0, };
	int retry = 0;
	uint8_t cnt = 0;

retry_accel_selftest:
	message.sensor_type = ADSP_FACTORY_ACCEL;

	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	data->selftest_ready_flag &= ~(1 << ADSP_FACTORY_ACCEL);
	adsp_unicast(&message, sizeof(message),
		NETLINK_MESSAGE_SELFTEST_SHOW_DATA, 0, 0);

	while (!(data->selftest_ready_flag & 1 << ADSP_FACTORY_ACCEL) &&
		cnt++ < TIMEOUT_CNT)
		msleep(20);

	data->selftest_ready_flag &= ~(1 << ADSP_FACTORY_ACCEL);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);

	if (data->sensor_selftest_result[ADSP_FACTORY_ACCEL].result1 < 0)
		pr_info("[FACTORY] %s: accel_selftest failed\n", __func__);

	init_status = data->sensor_selftest_result[ADSP_FACTORY_ACCEL].result1;
	accel_result = data->sensor_selftest_result[ADSP_FACTORY_ACCEL].result2;

	temp[0] = data->sensor_selftest_result[ADSP_FACTORY_ACCEL].ratio_x;
	temp[1] = data->sensor_selftest_result[ADSP_FACTORY_ACCEL].ratio_y;
	temp[2] = data->sensor_selftest_result[ADSP_FACTORY_ACCEL].ratio_z;

	if (accel_result == 1) {
		pr_info("[FACTORY] %s : Pass - result = %d, retry = %d\n",
			__func__, accel_result, retry);
	} else {
		accel_result = -5;
		pr_err("[FACTORY] %s : Fail - result = %d, retry = %d\n",
			__func__, accel_result, retry);
	}

	pr_info("[FACTORY] %s : init = %d, result = %d, XYZ = %d, %d, %d\n",
		__func__, init_status, accel_result, temp[0], temp[1], temp[2]);

	if (accel_result != 1) {
		if (retry < ACCEL_SELFTEST_TRY_CNT && temp[0] == 0) {
			retry++;
			msleep(RAWDATA_TIMER_MS * 2);
			pr_info("[FACTORY] %s: retry\n", __func__);
			goto retry_accel_selftest;
		}
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d\n", accel_result,
			(int)abs(temp[0]),
			(int)abs(temp[1]),
			(int)abs(temp[2]));
}

static ssize_t accel_raw_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	static uint8_t sample_cnt;

	if (adsp_start_raw_data(ADSP_FACTORY_ACCEL) == false)
		return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
			data->sensor_data[ADSP_FACTORY_ACCEL].x,
			data->sensor_data[ADSP_FACTORY_ACCEL].y,
			data->sensor_data[ADSP_FACTORY_ACCEL].z);

	if (++sample_cnt > 40) {
		pr_info("[FACTORY] %s: x(%d), y(%d), z(%d)\n",
			__func__,
			data->sensor_data[ADSP_FACTORY_ACCEL].x,
			data->sensor_data[ADSP_FACTORY_ACCEL].y,
			data->sensor_data[ADSP_FACTORY_ACCEL].z);
		sample_cnt = 0;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->sensor_data[ADSP_FACTORY_ACCEL].x,
		data->sensor_data[ADSP_FACTORY_ACCEL].y,
		data->sensor_data[ADSP_FACTORY_ACCEL].z);
}

static ssize_t accel_reactive_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	bool success = false;

	if (adsp_start_raw_data(ADSP_FACTORY_ACCEL) == true)
		if (data->sensor_data[ADSP_FACTORY_ACCEL].x != 0 ||
			data->sensor_data[ADSP_FACTORY_ACCEL].y != 0 ||
			data->sensor_data[ADSP_FACTORY_ACCEL].z != 0)
			success = true;

	pr_info("[FACTORY] %s: %d - x(%d), y(%d), z(%d)\n", __func__, success,
		data->sensor_data[ADSP_FACTORY_ACCEL].x,
		data->sensor_data[ADSP_FACTORY_ACCEL].y,
		data->sensor_data[ADSP_FACTORY_ACCEL].z);

	return snprintf(buf, PAGE_SIZE, "%d\n", success);
}


static ssize_t accel_reactive_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (sysfs_streq(buf, "1"))
		pr_info("[FACTORY]: %s - on\n", __func__);
	else if (sysfs_streq(buf, "0"))
		pr_info("[FACTORY]: %s - off\n", __func__);
	else if (sysfs_streq(buf, "2"))
		pr_info("[FACTORY]: %s - factory\n", __func__);

	return size;
}

static ssize_t accel_lowpassfilter_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	struct msg_data message;
	int lpf_on_off = 1;
	uint8_t cnt = 0;

	if (sysfs_streq(buf, "1")) {
		lpf_on_off = NETLINK_MESSAGE_ACCEL_LPF_ON;
		message.sensor_type = ADSP_FACTORY_ACCEL_LPF_ON;
	} else if (sysfs_streq(buf, "0")) {
		lpf_on_off = NETLINK_MESSAGE_ACCEL_LPF_OFF;
		message.sensor_type = ADSP_FACTORY_ACCEL_LPF_OFF;
	}

	pr_info("[FACTORY] %s: lpf_on_off = %d\n", __func__, lpf_on_off);

	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	data->selftest_ready_flag &= ~(1 << message.sensor_type);
	adsp_unicast(&message, sizeof(message), lpf_on_off, 0, 0);

	while (!(data->selftest_ready_flag & 1 << message.sensor_type) &&
		cnt++ < TIMEOUT_CNT)
		msleep(20);

	data->selftest_ready_flag &= ~(1 << message.sensor_type);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return size;
	}

	pr_info("[FACTORY] %s: lpf_on_off done (%d)(0x%x)\n", __func__,
		data->accel_lpf_result.result,
		data->accel_lpf_result.lpf_on_off);

	return size;
}

#ifdef CONFIG_ADSP_MOTOR
int setSensorCallback(int state, int duration)
{
	pr_info("[FACTORY] %s: state = %d, duration = %d\n",
		__func__, pdata->motor_state, duration);

	if (duration > MOTOR_OFF && duration <= DURATION_SKIP)
		return 0;

	if (pdata->motor_state != state) {
		pdata->motor_state = state;
		queue_work(pdata->slpi_motor_wq, &pdata->work_slpi_motor);
	}

	return 0;
}

void slpi_motor_work_func(struct work_struct *work)
{
	struct msg_data message;
	int motor = 0;

	if (pdata->motor_state == 1) {
		motor = NETLINK_MESSAGE_ACCEL_MOTOR_ON;
		message.sensor_type = ADSP_FACTORY_ACCEL_MOTOR_ON;
	} else if (pdata->motor_state == 0) {
		motor = NETLINK_MESSAGE_ACCEL_MOTOR_OFF;
		message.sensor_type = ADSP_FACTORY_ACCEL_MOTOR_OFF;
	}

	pr_info("[FACTORY] %s: state = %d\n", __func__, pdata->motor_state);

	adsp_unicast(&message, sizeof(message), motor, 0, 0);
}
#endif

static ssize_t accel_dhr_sensor_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	struct msg_data message;
	char ctrl1_xl = 0;
	uint8_t fullscale = 0;
	uint8_t cnt = 0;

	message.sensor_type = ADSP_FACTORY_ACCEL;
	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	data->dump_reg_ready_flag &= ~(1 << ADSP_FACTORY_ACCEL);
	adsp_unicast(&message, sizeof(message),
		NETLINK_MESSAGE_DUMP_REGISTER, 0, 0);

	while (!(data->dump_reg_ready_flag & 1 << ADSP_FACTORY_ACCEL) &&
		cnt++ < TIMEOUT_CNT)
		msleep(20);

	data->dump_reg_ready_flag &= ~(1 << ADSP_FACTORY_ACCEL);

	if (cnt >= TIMEOUT_CNT) {
		pr_info("[FACTORY] %s: Timeout!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "\"FULL_SCALE\":\"8G\"\n");
	}

	ctrl1_xl = data->dump_registers[ADSP_FACTORY_ACCEL].reg[0x10];

	ctrl1_xl &= 0xC;

	switch (ctrl1_xl) {
	case 0xC:
		fullscale = 8;
		break;
	case 0x8:
		fullscale = 4;
		break;
	case 0x4:
		fullscale = 16;
		break;
	case 0:
		fullscale = 2;
		break;
	default:
		break;
	}

	pr_info("[FACTORY] %s: f/s %u\n", __func__, fullscale);

	return snprintf(buf, PAGE_SIZE, "\"FULL_SCALE\":\"%uG\"\n", fullscale);
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
static DEVICE_ATTR(lowpassfilter, S_IWUSR | S_IWGRP,
	NULL, accel_lowpassfilter_store);
static DEVICE_ATTR(dhr_sensor_info, S_IRUSR | S_IRGRP,
	accel_dhr_sensor_info_show, NULL);

static struct device_attribute *acc_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_type,
	&dev_attr_calibration,
	&dev_attr_selftest,
	&dev_attr_raw_data,
	&dev_attr_reactive_alert,
	&dev_attr_lowpassfilter,
	&dev_attr_dhr_sensor_info,
	NULL,
};

static int __init k6ds3_accel_factory_init(void)
{
	adsp_factory_register(ADSP_FACTORY_ACCEL, acc_attrs);
#ifdef CONFIG_ADSP_MOTOR
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);

	if (pdata == NULL) {
		pr_err("[FACTORY]: %s - could not allocate memory\n", __func__);
		return 0;
	}

	pdata->slpi_motor_wq =
		create_singlethread_workqueue("slpi_motor_wq");

	if (pdata->slpi_motor_wq == NULL) {
		pr_err("[FACTORY]: %s - could not create motor wq\n", __func__);
		return 0;
	}

	INIT_WORK(&pdata->work_slpi_motor, slpi_motor_work_func);

	pdata->motor_state = 0;
#endif
	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit k6ds3_accel_factory_exit(void)
{
	adsp_factory_unregister(ADSP_FACTORY_ACCEL);
#ifdef CONFIG_ADSP_MOTOR
	if (pdata != NULL && pdata->slpi_motor_wq != NULL) {
		cancel_work_sync(&pdata->work_slpi_motor);
		destroy_workqueue(pdata->slpi_motor_wq);
		pdata->slpi_motor_wq = NULL;
	}
#endif
	pr_info("[FACTORY] %s\n", __func__);
}
module_init(k6ds3_accel_factory_init);
module_exit(k6ds3_accel_factory_exit);
