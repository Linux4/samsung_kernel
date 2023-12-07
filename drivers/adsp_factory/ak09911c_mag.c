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
#define VENDOR				"AKM"
#define CHIP_ID				"AK09911C"
#define RAWDATA_TIMER_MS		200
#define RAWDATA_TIMER_MARGIN_MS	20
#define MAG_SELFTEST_TRY_CNT		7
#define AK09911C_MODE_POWERDOWN	0x00

static int mag_read_fuserom(struct device *dev,
	struct device_attribute *attr)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	unsigned long timeout;
	struct msg_data message;

	message.sensor_type = ADSP_FACTORY_MAG;
	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_MAG_READ_FUSE_ROM, 0, 0);

	timeout = jiffies + (20 * HZ);
	while (!(data->magtest_ready_flag & 1 << ADSP_FACTORY_MAG)) {
		msleep(20);
		if (time_after(jiffies, timeout)) {
			pr_info("[FACTORY] %s: Timeout!!!\n", __func__);
			return -1;
		}
	}

	data->magtest_ready_flag &= 0 << ADSP_FACTORY_MAG;

#if 0
	pr_info("[FACTORY] %s: mag_x(%d), mag_y(%d), mag_z(%d)\n", __func__,
		data->sensor_mag_factory_result.fuserom_x,
		data->sensor_mag_factory_result.fuserom_y,
		data->sensor_mag_factory_result.fuserom_z);
#endif
	return 0;
}

static int mag_read_register(struct device *dev,
	struct device_attribute *attr)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	unsigned long timeout;
	struct msg_data message;

	message.sensor_type = ADSP_FACTORY_MAG;
	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_MAG_READ_REGISTERS, 0, 0);

	timeout = jiffies + (20 * HZ);
	while (!(data->magtest_ready_flag & 1 << ADSP_FACTORY_MAG)) {
		msleep(20);
		if (time_after(jiffies, timeout)) {
			pr_info("[FACTORY] %s: Timeout!!!\n", __func__);
			return -1;
		}
	}

	data->magtest_ready_flag &= 0 << ADSP_FACTORY_MAG;

#if 1
	pr_info("[FACTORY] %s: result(%d), mag_x(%d), mag_y(%d), mag_z(%d)\n", __func__,
	data->sensor_mag_factory_result.result,
	data->sensor_mag_factory_result.registers[0],
	data->sensor_mag_factory_result.registers[1],
	data->sensor_mag_factory_result.registers[2]);
#endif
	if(data->sensor_mag_factory_result.result == -1)
		return -1;
	else
		return 0;
}

static ssize_t mag_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", VENDOR);
}

static ssize_t mag_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CHIP_ID);
}

static ssize_t mag_read_adc(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);

	if (adsp_start_raw_data(ADSP_FACTORY_MAG) == false)
		return snprintf(buf, PAGE_SIZE, "%s,%d,%d,%d\n", "NG", 0, 0, 0);

	pr_info("[FACTORY] %s: %d,%d,%d,%d\n", __func__, true,
		data->sensor_data[ADSP_FACTORY_MAG].x,
		data->sensor_data[ADSP_FACTORY_MAG].y,
		data->sensor_data[ADSP_FACTORY_MAG].z);

 	return snprintf(buf, PAGE_SIZE, "%s,%d,%d,%d\n", "OK",
		data->sensor_data[ADSP_FACTORY_MAG].x,
		data->sensor_data[ADSP_FACTORY_MAG].y,
		data->sensor_data[ADSP_FACTORY_MAG].z);
}

static ssize_t mag_check_cntl(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int8_t reg;

	reg = mag_read_register(dev, attr);
	if(reg < 0) {
		data->sensor_mag_factory_result.registers[13] = 0;
		pr_info("[FACTORY] %s: failed!! = %d\n",
			__func__, reg);
	}

	return snprintf(buf, PAGE_SIZE, "%s\n",
			(((data->sensor_mag_factory_result.registers[13] == AK09911C_MODE_POWERDOWN) &&
			(reg == 0)) ? "OK" : "NG"));
}

static ssize_t mag_check_registers(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint8_t temp[13];
	uint8_t ret = 0;
	int8_t reg;

	reg = mag_read_register(dev, attr);

	if (reg < 0) {
		for (ret = 0; ret < 13; ret++) {
			data->sensor_mag_factory_result.registers[ret] = 0;
			temp[ret] = 0;
		}
		pr_info("[FACTORY] %s: failed!! = %d\n",
			__func__, reg);
	} else {
		for (ret = 0; ret < 13; ret++)
			temp[ret] = (uint8_t)(data->sensor_mag_factory_result.registers[ret]);
	}

	return snprintf(buf, PAGE_SIZE,
			"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			temp[0], temp[1], temp[2], temp[3], temp[4], temp[5],
			temp[6], temp[7], temp[8], temp[9], temp[10], temp[11],
			temp[12]);
}

static ssize_t mag_get_asa(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int success;
	success = mag_read_fuserom(dev, attr);

	if (success < 0) {
		data->sensor_mag_factory_result.fuserom_x = 0;
		data->sensor_mag_factory_result.fuserom_y = 0;
		data->sensor_mag_factory_result.fuserom_z = 0;
		pr_info("[FACTORY] %s: failed!! = %d\n",
			__func__, success);
	}
	return snprintf(buf, PAGE_SIZE, "%u,%u,%u\n",
			data->sensor_mag_factory_result.fuserom_x,
			data->sensor_mag_factory_result.fuserom_y,
			data->sensor_mag_factory_result.fuserom_z);
}

static ssize_t mag_get_status(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int success;
	success = mag_read_fuserom(dev, attr);

	if (success < 0) {
		data->sensor_mag_factory_result.fuserom_x = 0;
		data->sensor_mag_factory_result.fuserom_y = 0;
		data->sensor_mag_factory_result.fuserom_z = 0;
		pr_info("[FACTORY] %s: failed!! = %d\n",
			__func__, success);
		success = false;
	}

	if ((data->sensor_mag_factory_result.fuserom_x == 0)
		|| (data->sensor_mag_factory_result.fuserom_x == 0xff)
		|| (data->sensor_mag_factory_result.fuserom_y == 0)
		|| (data->sensor_mag_factory_result.fuserom_y == 0xff)
		|| (data->sensor_mag_factory_result.fuserom_z == 0)
		|| (data->sensor_mag_factory_result.fuserom_z == 0xff))
		success = false;
	else
		success = true;

	return snprintf(buf, PAGE_SIZE, "%s\n", (success ? "OK" : "NG"));
}

static ssize_t mag_raw_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	static uint8_t sample_cnt = 0;
	struct adsp_data *data = dev_get_drvdata(dev);

	if (adsp_start_raw_data(ADSP_FACTORY_MAG) == false)
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);

	sample_cnt++;
	if (sample_cnt > 20) {	/* sample log 1.6s */
		sample_cnt = 0;
		pr_info("[FACTORY] %s: %d,%d,%d\n", __func__,
			data->sensor_data[ADSP_FACTORY_MAG].x,
			data->sensor_data[ADSP_FACTORY_MAG].y,
			data->sensor_data[ADSP_FACTORY_MAG].z);
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->sensor_data[ADSP_FACTORY_MAG].x,
		data->sensor_data[ADSP_FACTORY_MAG].y,
		data->sensor_data[ADSP_FACTORY_MAG].z);
}

static ssize_t mag_selttest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int result1 = 0;
	int temp[10];
	unsigned long timeout;
	struct msg_data message;
	uint8_t ret;
	uint8_t retry = 0;

retry_mag_selftest:
	message.sensor_type = ADSP_FACTORY_MAG;

	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_SELFTEST_SHOW_DATA, 0, 0);

	timeout = jiffies + (20 * HZ);

	while (!(data->selftest_ready_flag & 1 << ADSP_FACTORY_MAG)) {
		msleep(20);
		if (time_after(jiffies, timeout)) {
			data->sensor_selftest_result[ADSP_FACTORY_MAG].result1 = -1;
			pr_info("[FACTORY] %s: Timeout!!!\n", __func__);
		}
	}

	data->selftest_ready_flag &= 0 << ADSP_FACTORY_MAG;

	temp[0] = data->sensor_selftest_result[ADSP_FACTORY_MAG].result1;
	temp[1] = data->sensor_selftest_result[ADSP_FACTORY_MAG].result2;
	temp[2] = data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_x;
	temp[3] = data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_y;
	temp[4] = data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_z;
	temp[5] = data->sensor_selftest_result[ADSP_FACTORY_MAG].dac_ret;
	temp[6] = data->sensor_selftest_result[ADSP_FACTORY_MAG].adc_ret;
	temp[7] = data->sensor_selftest_result[ADSP_FACTORY_MAG].ohx;
	temp[8] = data->sensor_selftest_result[ADSP_FACTORY_MAG].ohy;
	temp[9] = data->sensor_selftest_result[ADSP_FACTORY_MAG].ohz;

	/* Data Process */
	if (temp[0] == 0)
		result1 = 1;
	else
		result1 = 0;

	pr_info("[FACTORY] status=%d, sf_status=%d, sf_x=%d, sf_y=%d, sf_z=%d\n"
		"[FACTORY] dac_status=%d, adc_status=%d, adc_x=%d, adc_y=%d, adc_z=%d\n",
		temp[0], temp[1], temp[2], temp[3], temp[4],
		temp[5], temp[6], temp[7], temp[8], temp[9]);

	if (!result1) {
		if (retry < MAG_SELFTEST_TRY_CNT && temp[0] != 0) {
			retry++;
			msleep(RAWDATA_TIMER_MS * 2);
			for (ret = 0; ret < 10; ret++)
				temp[ret] = 0;
			goto retry_mag_selftest;
		}
	}

	return sprintf(buf,
		"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		temp[0], temp[1], temp[2], temp[3], temp[4],
		temp[5], temp[6], temp[7], temp[8], temp[9]);
}

static DEVICE_ATTR(name, S_IRUGO, mag_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, mag_vendor_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, mag_raw_data_read, NULL);
static DEVICE_ATTR(adc, S_IRUGO, mag_read_adc, NULL);
static DEVICE_ATTR(dac, S_IRUGO, mag_check_cntl, NULL);
static DEVICE_ATTR(chk_registers, S_IRUGO, mag_check_registers, NULL);
static DEVICE_ATTR(selftest, S_IRUSR | S_IRGRP,
	mag_selttest_show, NULL);
static DEVICE_ATTR(asa, S_IRUGO, mag_get_asa, NULL);
static DEVICE_ATTR(status, S_IRUGO, mag_get_status, NULL);


static struct device_attribute *mag_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_raw_data,
	&dev_attr_adc,
	&dev_attr_dac,
	&dev_attr_chk_registers,
	&dev_attr_selftest,
	&dev_attr_asa,
	&dev_attr_status,
	NULL,
};

static int __init ak09911c_factory_init(void)
{
	adsp_factory_register(ADSP_FACTORY_MAG, mag_attrs);

	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit ak09911c_factory_exit(void)
{
	adsp_factory_unregister(ADSP_FACTORY_MAG);

	pr_info("[FACTORY] %s\n", __func__);
}

module_init(ak09911c_factory_init);
module_exit(ak09911c_factory_exit);
