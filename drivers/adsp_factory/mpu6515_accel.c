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

static ssize_t accel_calibration_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int iCount = 0;
	struct msg_data message;
	message.sensor_type = ADSP_FACTORY_MODE_ACCEL;
	adsp_unicast(message,NETLINK_MESSAGE_GET_CALIB_DATA,0,0);

	while( !(data->calib_ready_flag & 1 << ADSP_FACTORY_MODE_ACCEL) )
		msleep(20);

	iCount = snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d\n",  data->sensor_calib_data[ADSP_FACTORY_MODE_ACCEL].result,
		data->sensor_calib_data[ADSP_FACTORY_MODE_ACCEL].x,
		data->sensor_calib_data[ADSP_FACTORY_MODE_ACCEL].y,
		data->sensor_calib_data[ADSP_FACTORY_MODE_ACCEL].z);
	data->calib_ready_flag &= 0 << ADSP_FACTORY_MODE_ACCEL;

	return iCount;
}

static ssize_t accel_calibration_store(struct device *dev,
struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	struct msg_data message;
	message.sensor_type = ADSP_FACTORY_MODE_ACCEL;
	adsp_unicast(message,NETLINK_MESSAGE_CALIB_STORE_DATA,0,0);

	while( !(data->calib_store_ready_flag & 1 << ADSP_FACTORY_MODE_ACCEL) )
		msleep(20);

	if(data->sensor_calib_result[ADSP_FACTORY_MODE_ACCEL].nCalibstoreresult < 0)
		pr_err("[FACTORY]: %s - accel_do_calibrate() failed\n", __func__);
	data->calib_store_ready_flag |= 0 << ADSP_FACTORY_MODE_ACCEL;
	return size;
}

static ssize_t accel_selftest_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int result1 = 0;
	int result2 = 0;
	unsigned long timeout = jiffies + (2*HZ);
	struct msg_data message;
	message.sensor_type = ADSP_FACTORY_MODE_ACCEL;
	pr_err("[FACTORY]: %s - \n", __func__);

	while( !time_after(jiffies, timeout)){
		msleep(20);
		}
	adsp_unicast(message,NETLINK_MESSAGE_SELFTEST_SHOW_DATA,0,0);
	timeout = jiffies + (20*HZ);
	while( !(data->selftest_ready_flag & 1 << ADSP_FACTORY_MODE_ACCEL) ){
		msleep(20);
		if (time_after(jiffies, timeout)) {
			return sprintf(buf, "%d,"
		"%d.%01d,%d.%01d,%d.%01d\n",1,0,0,0,0,0,0);
		}
	}
	if(data->sensor_selftest_result[ADSP_FACTORY_MODE_ACCEL].nSelftestresult1< 0)
		pr_err("[FACTORY]: %s - accel_do_calibrate() failed\n", __func__);
	data->selftest_ready_flag &= 0 << ADSP_FACTORY_MODE_ACCEL;
	result1 = data->sensor_selftest_result[ADSP_FACTORY_MODE_ACCEL].nSelftestresult1;
	result2 = data->sensor_selftest_result[ADSP_FACTORY_MODE_ACCEL].nSelftestresult2;
	pr_err("[FACTORY]: %s - result - %d \n", __func__,result1);
	return sprintf(buf, "%d,"
		"%d.%01d,%d.%01d,%d.%01d\n",
			result1,
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_ACCEL].ratio_x),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_ACCEL].ratio_x_dec),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_ACCEL].ratio_y),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_ACCEL].ratio_y_dec),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_ACCEL].ratio_z),
			(int)abs(data->sensor_selftest_result[ADSP_FACTORY_MODE_ACCEL].ratio_z_dec));
}

static ssize_t raw_data_read(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct msg_data message;
	unsigned long timeout = jiffies + (2*HZ);
	struct adsp_data *data = dev_get_drvdata(dev);
	if( !(data->raw_data_stream &  ADSP_RAW_ACCEL) ){
		pr_err("Starting raw data  \n");
		data->raw_data_stream |= ADSP_RAW_ACCEL;
		message.sensor_type = ADSP_FACTORY_MODE_ACCEL;
		adsp_unicast(message,NETLINK_MESSAGE_GET_RAW_DATA,0,0);
	}
	while( !(data->data_ready_flag & 1 << ADSP_FACTORY_MODE_ACCEL) ){
		msleep(20);
		if (time_after(jiffies, timeout)) {
			return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",0,0,0);
		}
	}
	adsp_factory_start_timer(2000);
	printk("raw_data_read x=%d y=%d z=%d \n",data->sensor_data[ADSP_FACTORY_MODE_ACCEL].x,data->sensor_data[ADSP_FACTORY_MODE_ACCEL].y,data->sensor_data[ADSP_FACTORY_MODE_ACCEL].z);
	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->sensor_data[ADSP_FACTORY_MODE_ACCEL].x,
		data->sensor_data[ADSP_FACTORY_MODE_ACCEL].y,
		data->sensor_data[ADSP_FACTORY_MODE_ACCEL].z);
}

static ssize_t accel_reactive_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int alert_val;
	struct adsp_data *data = dev_get_drvdata(dev);
	/*while( !(data->alert_ready_flag & 1 << ADSP_FACTORY_MODE_ACCEL) )
		msleep(20);*/
	pr_err("%s data->reactive_alert = %d \n", __func__,data->reactive_alert);
	data->alert_ready_flag &= 0 << ADSP_FACTORY_MODE_ACCEL;
	if ( data->reactive_alert == 2 ){
		struct msg_data message;
		alert_val = 1;
		message.sensor_type = ADSP_FACTORY_MODE_ACCEL;
		message.param1 = 0;
		adsp_unicast(message,NETLINK_MESSAGE_REACTIVE_ALERT_DATA,0,0);
		}
	else if ( data->reactive_alert == 1 ){
		alert_val = 0;
	}
	else if ( data->reactive_alert == 3 ){
		struct msg_data message;
		alert_val = 1;
		message.sensor_type = ADSP_FACTORY_MODE_ACCEL;
		message.param1 = 3;
		adsp_unicast(message,NETLINK_MESSAGE_REACTIVE_ALERT_DATA,0,0);
	}

	return snprintf(buf,PAGE_SIZE, "%d\n",alert_val);
}


static ssize_t accel_reactive_store(struct device *dev,
				struct device_attribute *attr,
					const char *buf, size_t size)
{
	unsigned long enable = 0;
	struct adsp_data *data = dev_get_drvdata(dev);
	if (strict_strtoul(buf, 10, &enable)) {
		pr_err("[SENSOR] %s, kstrtoint fail\n", __func__);
		return -EINVAL;
	}
	pr_err("[SENSOR] %s: enable = %ld\n", __func__, enable);
	if( !(data->alert_ready_flag & 1 << ADSP_FACTORY_MODE_ACCEL) ){
		struct msg_data message;
		message.sensor_type = ADSP_FACTORY_MODE_ACCEL;
		message.param1 = enable;
		adsp_unicast(message,NETLINK_MESSAGE_REACTIVE_ALERT_DATA,0,0);
	}
	return size;
}

static void accel_stop_raw_data_worker(struct work_struct *work)
{
	struct msg_data message;
	pr_err("(%s):  \n", __func__);
	message.sensor_type = ADSP_FACTORY_MODE_ACCEL;
	adsp_unicast(message,NETLINK_MESSAGE_STOP_RAW_DATA,0,0);
	return;
}


int accel_factory_init(struct adsp_data *data)
{
	return 0;
}
int accel_factory_exit(struct adsp_data *data)
{
	return 0;
}
int accel_factory_receive_data(struct adsp_data *data, int cmd )
{
	pr_err("(%s): factory \n", __func__);
	switch(cmd)
	{
		case CALLBACK_REGISTER_SUCCESS:
			pr_info("[SENSOR] %s: mpu6515 registration success \n", __func__);
			break;
		case CALLBACK_TIMER_EXPIRED:
			pr_err("Raw data flag = %d\n",data->raw_data_stream);
			if( (data->raw_data_stream &  ADSP_RAW_ACCEL) ){
				data->raw_data_stream &= ~ADSP_RAW_ACCEL;
				schedule_work(&timer_stop_data_work);
			}
			break;
		default:
			break;
	}
	return 0;
}

static struct adsp_fac_ctl adsp_fact_cb = {
	.init_fnc = accel_factory_init,
	.exit_fnc = accel_factory_exit,
	.receive_data_fnc = accel_factory_receive_data
};

static DEVICE_ATTR(name, S_IRUGO, accel_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, accel_vendor_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
	accel_calibration_show, accel_calibration_store);
static DEVICE_ATTR(selftest, S_IRUSR | S_IRGRP,
	accel_selftest_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, raw_data_read, NULL);
static DEVICE_ATTR(reactive_alert, S_IRUGO | S_IWUSR | S_IWGRP,
		accel_reactive_show, accel_reactive_store);

static struct device_attribute *acc_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_calibration,
	&dev_attr_selftest,
	&dev_attr_raw_data,
	&dev_attr_reactive_alert,
	NULL,
};

static int __devinit mpu6515accel_factory_init(void)
{
	adsp_factory_register("accelerometer_sensor",ADSP_FACTORY_MODE_ACCEL,acc_attrs,&adsp_fact_cb);
	INIT_WORK(&timer_stop_data_work,
				  accel_stop_raw_data_worker );
	pr_err("(%s): factory \n", __func__);
	return 0;
}
static void __devexit mpu6515accel_factory_exit(void)
{
	pr_err("(%s): factory \n", __func__);
}
module_init(mpu6515accel_factory_init);
module_exit(mpu6515accel_factory_exit);
