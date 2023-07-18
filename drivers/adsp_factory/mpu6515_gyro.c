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
#define VENDOR		"Invensence"
#define CHIP_ID		"MPU6515"

static struct work_struct timer_stop_data_work;

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

static ssize_t gyro_calibration_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int iCount = 0;
	struct msg_data message;
	message.sensor_type = ADSP_FACTORY_MODE_GYRO;
	adsp_unicast(message,NETLINK_MESSAGE_GET_CALIB_DATA,0,0);

	while( !(data->calib_ready_flag & 1 << ADSP_FACTORY_MODE_GYRO) )
		msleep(20);

	iCount = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d\n",  data->sensor_calib_data[ADSP_FACTORY_MODE_GYRO].result,
		data->sensor_calib_data[ADSP_FACTORY_MODE_GYRO].x,
		data->sensor_calib_data[ADSP_FACTORY_MODE_GYRO].y,
		data->sensor_calib_data[ADSP_FACTORY_MODE_GYRO].z);

	return iCount;
}

static ssize_t gyro_calibration_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	struct msg_data message;
	message.sensor_type = ADSP_FACTORY_MODE_GYRO;
	adsp_unicast(message,NETLINK_MESSAGE_CALIB_STORE_DATA,0,0);

	while( !(data->calib_store_ready_flag & 1 << ADSP_FACTORY_MODE_GYRO) )
		msleep(20);

	if(data->sensor_calib_result[ADSP_FACTORY_MODE_ACCEL].nCalibstoreresult < 0)
		pr_err("[FACTORY]: %s - accel_do_calibrate() failed\n", __func__);
	data->calib_store_ready_flag |= 0 << ADSP_FACTORY_MODE_GYRO;
	return size;
}

static ssize_t raw_data_read(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct msg_data message;
	unsigned long timeout = jiffies + (2*HZ);
	struct adsp_data *data = dev_get_drvdata(dev);
	if( !(data->raw_data_stream &  ADSP_RAW_GYRO) ){
		data->raw_data_stream |= ADSP_RAW_GYRO;
		message.sensor_type = ADSP_FACTORY_MODE_GYRO;
		adsp_unicast(message,NETLINK_MESSAGE_GET_RAW_DATA,0,0);
	}
	while( !(data->data_ready_flag & 1 << ADSP_FACTORY_MODE_GYRO) ){
		msleep(20);
		if (time_after(jiffies, timeout)) {
			return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",0,0,0);
		}
	}
	adsp_factory_start_timer(2000);
	printk("raw_data_read x=%d y=%d z=%d \n",data->sensor_data[ADSP_FACTORY_MODE_GYRO].x,data->sensor_data[ADSP_FACTORY_MODE_GYRO].y,data->sensor_data[ADSP_FACTORY_MODE_GYRO].z);
	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->sensor_data[ADSP_FACTORY_MODE_GYRO].x,
		data->sensor_data[ADSP_FACTORY_MODE_GYRO].y,
		data->sensor_data[ADSP_FACTORY_MODE_GYRO].z);
}

static ssize_t gyro_power_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", 1);
}


static ssize_t gyro_temp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct msg_data message;
	unsigned long timeout = jiffies + (2*HZ);
	struct adsp_data *data = dev_get_drvdata(dev);
	if( !(data->data_ready &  ADSP_DATA_GYRO_TEMP) ){
		message.sensor_type = ADSP_FACTORY_MODE_GYRO;
		adsp_unicast(message,NETLINK_MESSAGE_GYRO_TEMP,0,0);
	}
	while( !(data->data_ready & ADSP_DATA_GYRO_TEMP) ){
		msleep(20);
		if (time_after(jiffies, timeout)) {
			return sprintf(buf, "%d \n", 0);
		}
	}
	data->data_ready &= ~ADSP_DATA_GYRO_TEMP;
	return sprintf(buf, "%d\n", data->gyro_temp);
}
static void gyro_stop_raw_data_worker(struct work_struct *work)
{
	struct msg_data message;
	pr_err("(%s):  \n", __func__);
	message.sensor_type = ADSP_FACTORY_MODE_GYRO;
	adsp_unicast(message,NETLINK_MESSAGE_STOP_RAW_DATA,0,0);
	return;
}
static ssize_t gyro_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int result1 = 0;
	int result2 = 0;
	unsigned long timeout = jiffies + (2*HZ);
	struct msg_data message;
	message.sensor_type = ADSP_FACTORY_MODE_GYRO;
	pr_err("[FACTORY]: %s - \n", __func__);

	while( !time_after(jiffies, timeout)){
		msleep(20);
		}
	adsp_unicast(message,NETLINK_MESSAGE_SELFTEST_SHOW_DATA,0,0);
	timeout = jiffies + (20*HZ);
	while( !(data->selftest_ready_flag & 1 << ADSP_FACTORY_MODE_GYRO) ){
		msleep(20);
		if (time_after(jiffies, timeout)) {
			return snprintf(buf, PAGE_SIZE, "%d,"
			"%d.%03d,%d.%03d,%d.%03d,"
			"%d.%03d,%d.%03d,%d.%03d,"
			"%d.%01d,%d.%01d,%d.%01d,"
			"%d.%03d,%d.%03d,%d.%03d\n",	1,0,0,0,0,	0,0,0,0,0,	0,0,0,0,0,	0,0,0,0,0,0,0,0,0,0);
		}
	}
	if(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].nSelftestresult1< 0)
		pr_err("[FACTORY]: %s - accel_do_calibrate() failed\n", __func__);
	data->selftest_ready_flag &= 0 << ADSP_FACTORY_MODE_GYRO;
	result1 = data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].nSelftestresult1;
	result2 = data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].nSelftestresult2;
	pr_err("[FACTORY]: %s - result - %d \n", __func__,result1);
	//return snprintf(buf, PAGE_SIZE, "%d\n", result1);
	return snprintf(buf, PAGE_SIZE, "%d,"
			"%d.%03d,%d.%03d,%d.%03d,"
			"%d.%03d,%d.%03d,%d.%03d,"
			"%d.%01d,%d.%01d,%d.%01d,"
			"%d.%03d,%d.%03d,%d.%03d\n",
			result1 | result2,
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].bias_x / 1000),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].bias_x) % 1000,
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].bias_y / 1000),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].bias_y) % 1000,
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].bias_z / 1000),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].bias_z) % 1000,
			data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].rms_x / 1000,
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].rms_x) % 1000,
			data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].rms_y /1000,
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].rms_y) % 1000,
			data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].rms_z / 1000,
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].rms_z) % 1000,
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].ratio_x),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].ratio_x_dec),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].ratio_y),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].ratio_y_dec),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].ratio_z),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].ratio_z_dec),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].st_x / 100),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].st_x) % 100,
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].st_y / 100),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].st_y) % 100,
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].st_z / 100),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_GYRO].st_z) % 100);
}
int gyro_factory_init(struct adsp_data *data)
{
	return 0;
}
int gyro_factory_exit(struct adsp_data *data)
{
	return 0;
}
int gyro_factory_receive_data(struct adsp_data *data, int cmd)
{
	pr_err("(%s): factory \n", __func__);
	switch(cmd)
	{
		case CALLBACK_REGISTER_SUCCESS:
			pr_info("[SENSOR] %s: mpu6515 registration success \n", __func__);
			break;
		case CALLBACK_TIMER_EXPIRED:
			if( (data->raw_data_stream &  ADSP_RAW_GYRO) ){
				data->raw_data_stream &= ~ADSP_RAW_GYRO;
				schedule_work(&timer_stop_data_work);
			}
			break;
		default:
			break;
	}
	return 0;
}

static struct adsp_fac_ctl adsp_fact_cb = {
	.init_fnc = gyro_factory_init,
	.exit_fnc = gyro_factory_exit,
	.receive_data_fnc = gyro_factory_receive_data
};

static DEVICE_ATTR(name, S_IRUGO, gyro_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, gyro_vendor_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
	gyro_calibration_show, gyro_calibration_store);
static DEVICE_ATTR(raw_data, S_IRUGO, raw_data_read, NULL);
static DEVICE_ATTR(selftest, S_IRUSR | S_IRGRP,
	gyro_selftest_show, NULL);
static DEVICE_ATTR(temperature, S_IRUSR | S_IRGRP,
	gyro_temp_show, NULL);
static DEVICE_ATTR(power_on, S_IRUSR | S_IRGRP,
	gyro_power_show, NULL);

static struct device_attribute *gyro_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_calibration,
	&dev_attr_raw_data,
	&dev_attr_selftest,
	&dev_attr_temperature,
	&dev_attr_power_on,
	NULL,
};

static int __devinit mpu6515gyro_factory_init(void)
{
	adsp_factory_register("gyro_sensor",ADSP_FACTORY_MODE_GYRO,gyro_attrs,&adsp_fact_cb);
	INIT_WORK(&timer_stop_data_work,
				  gyro_stop_raw_data_worker );
	pr_err("(%s): factory \n", __func__);
	return 0;
}
static void __devexit mpu6515gyro_factory_exit(void)
{
	pr_err("(%s): factory \n", __func__);
}
module_init(mpu6515gyro_factory_init);
module_exit(mpu6515gyro_factory_exit);
