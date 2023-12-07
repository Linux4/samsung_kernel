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
#define CHIP_ID "BMP280"

#define RAWDATA_TIMER_MS 200
#define RAWDATA_TIMER_MARGIN_MS	20

#define CALIBRATION_FILE_PATH		"/efs/FactoryApp/baro_delta"

#define	PR_MAX	8388607		/* 24 bit 2'compl */
#define	PR_MIN	-8388608

static int sea_level_pressure = 0;

static ssize_t pressure_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t pressure_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}

static ssize_t sea_level_pressure_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sea_level_pressure);
}

static ssize_t sea_level_pressure_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	sscanf(buf, "%d", &sea_level_pressure);

	sea_level_pressure = sea_level_pressure / 100;

	pr_info("[FACTORY] %s: sea_level_pressure = %d\n", __func__,
		sea_level_pressure);
	return size;
}

int pressure_open_calibration(struct adsp_data *data)
{
	char buf[10] = {0,};
	int error = 0;
	mm_segment_t old_fs;
	struct file *cal_filp = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		error = PTR_ERR(cal_filp);
		if (error != -ENOENT)
			pr_err("[FACTORY] %s : Can't open cal file(%d)\n",
				__func__, error);
		set_fs(old_fs);
		return error;
	}
	error = cal_filp->f_op->read(cal_filp,
		buf, 10 * sizeof(char), &cal_filp->f_pos);
	if (error < 0) {
		pr_err("[FACTORY] %s : Can't read the cal data from file(%d)\n",
			__func__, error);
		return error;
	}
	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	error = kstrtoint(buf, 10,
		&data->sensor_data[ADSP_FACTORY_PRESSURE].pressure_cal);

	if (error < 0) {
		pr_err("[FACTORY] %s : kstrtoint failed. %d", __func__, error);
		return error;
	}

	pr_info("[FACTORY] %s: Open barometer calibration %d\n", __func__,
		data->sensor_data[ADSP_FACTORY_PRESSURE].pressure_cal);


	if (data->sensor_data[ADSP_FACTORY_PRESSURE].pressure_cal < PR_MIN ||
		data->sensor_data[ADSP_FACTORY_PRESSURE].pressure_cal > PR_MAX)
		pr_err("[FACTORY] %s : wrong offset value!!!\n", __func__);

	return error;
}

static ssize_t pressure_cabratioin_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int pressure_cal = 0, error = 0;

	error = kstrtoint(buf, 10, &pressure_cal);
	if (error < 0) {
		pr_err("[FACTORY] %s : kstrtoint failed.(%d)", __func__, error);
		return error;
	}

	if (pressure_cal < PR_MIN || pressure_cal > PR_MAX)
		return -EINVAL;

	data->sensor_data[ADSP_FACTORY_PRESSURE].pressure_cal =
		(s32)pressure_cal;

	return size;
}

static ssize_t pressure_cabratioin_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);

	pressure_open_calibration(data);

	return sprintf(buf, "%d\n",
		data->sensor_data[ADSP_FACTORY_PRESSURE].pressure_cal);
}

static ssize_t eeprom_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	struct msg_data message;
	unsigned long timeout;
	char eeprom_data = 0;

	message.sensor_type = ADSP_FACTORY_PRESSURE;

	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_SELFTEST_SHOW_DATA, 0, 0);
	timeout = jiffies + (10 * HZ);

	while (!(data->selftest_ready_flag & 1 << ADSP_FACTORY_PRESSURE)) {
		msleep(20);
		if (time_after(jiffies, timeout)) {
			pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
			return snprintf(buf, PAGE_SIZE, "%d\n", eeprom_data);
		}
	}

	data->selftest_ready_flag &= 0 << ADSP_FACTORY_PRESSURE;

	eeprom_data =
		(char)data->sensor_selftest_result[ADSP_FACTORY_PRESSURE].result1;

	pr_info("[FACTORY] %s: eeprom_data = 0x%x\n", __func__, eeprom_data);

	return snprintf(buf, PAGE_SIZE, "%d\n", eeprom_data);
}

static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	struct msg_data message;
	unsigned long timeout;
	int temperature = 0;

	message.sensor_type = ADSP_FACTORY_PRESSURE;

	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_SELFTEST_SHOW_DATA, 0, 0);
	timeout = jiffies + (10 * HZ);

	while (!(data->selftest_ready_flag & 1 << ADSP_FACTORY_PRESSURE)) {
		msleep(20);
		if (time_after(jiffies, timeout)) {
			pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
			return snprintf(buf, PAGE_SIZE, "%d\n", temperature);
		}
	}

	data->selftest_ready_flag &= 0 << ADSP_FACTORY_PRESSURE;

	temperature =
		data->sensor_selftest_result[ADSP_FACTORY_PRESSURE].result2;

	pr_info("[FACTORY] %s: temperature = 0x%x\n", __func__, temperature);

	return snprintf(buf, PAGE_SIZE, "%d\n", temperature);
}

static DEVICE_ATTR(vendor,  S_IRUGO, pressure_vendor_show, NULL);
static DEVICE_ATTR(name,  S_IRUGO, pressure_name_show, NULL);
static DEVICE_ATTR(eeprom_check, S_IRUGO, eeprom_check_show, NULL);
static DEVICE_ATTR(calibration,  S_IRUGO | S_IWUSR | S_IWGRP,
	pressure_cabratioin_show, pressure_cabratioin_store);
static DEVICE_ATTR(sea_level_pressure, S_IRUGO | S_IWUSR | S_IWGRP,
	sea_level_pressure_show, sea_level_pressure_store);
static DEVICE_ATTR(temperature, S_IRUGO, temperature_show, NULL);

static struct device_attribute *pressure_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_calibration,
	&dev_attr_sea_level_pressure,
	&dev_attr_eeprom_check,
	&dev_attr_temperature,
	NULL,
};

static int __init bmp280_pressure_factory_init(void)
{
	adsp_factory_register(ADSP_FACTORY_PRESSURE, pressure_attrs);

	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit bmp280_pressure_factory_exit(void)
{
	adsp_factory_unregister(ADSP_FACTORY_PRESSURE);

	pr_info("[FACTORY] %s\n", __func__);
}

module_init(bmp280_pressure_factory_init);
module_exit(bmp280_pressure_factory_exit);
