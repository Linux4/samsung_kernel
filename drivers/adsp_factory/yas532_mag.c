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
#define VENDOR		"YAMAHA"
#define CHIP_ID		"YAS532B"
#define RAWDATA_TIMER_MS	200
#define RAWDATA_TIMER_MARGIN_MS	20
#define MAG_SELFTEST_TRY_CNT	7

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

static ssize_t mag_raw_data_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);

	if (adsp_start_raw_data(ADSP_FACTORY_MAG) == false)
		return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", 0, 0, 0);

	pr_info("[FACTORY] %s: %d,%d,%d\n", __func__,
		data->sensor_data[ADSP_FACTORY_MAG].x,
		data->sensor_data[ADSP_FACTORY_MAG].y,
		data->sensor_data[ADSP_FACTORY_MAG].z);
	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->sensor_data[ADSP_FACTORY_MAG].x,
		data->sensor_data[ADSP_FACTORY_MAG].y,
		data->sensor_data[ADSP_FACTORY_MAG].z);
}

static ssize_t mag_raw_data_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static ssize_t selttest_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int result1 = 0;
	int result2 = 0;
	struct msg_data message;
	s8 err[7] = { 0, };
	int retry = 0;
	int i;

retry_mag_selftest:
	message.sensor_type = ADSP_FACTORY_MAG;

	msleep(RAWDATA_TIMER_MS + RAWDATA_TIMER_MARGIN_MS);
	adsp_unicast(&message, sizeof(message), NETLINK_MESSAGE_SELFTEST_SHOW_DATA, 0, 0);

	while (!(data->selftest_ready_flag & 1 << ADSP_FACTORY_MAG))
		msleep(20);

	data->selftest_ready_flag &= 0 << ADSP_FACTORY_MAG;

	if (data->sensor_selftest_result[ADSP_FACTORY_MAG].nSelftestresult1 == 0)
		result1 = 1;
	else
		result1 = 0;

	if (data->sensor_selftest_result[ADSP_FACTORY_MAG].nSelftestresult2 == 0)
		result2 = 1;
	else
		result2 = 0;

	pr_info("[FACTORY] %s: result = %d, %d\n", __func__, result1, result2);

	if (data->sensor_selftest_result[ADSP_FACTORY_MAG].id != 0x2)
		err[0] = -1;
	if (data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_x < -30 ||
		data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_x > 30)
		err[3] = -1;
	if (data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_y < -30 ||
		data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_y > 30)
		err[3] = -1;
	if (data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_z < -30 ||
		data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_z > 30)
		err[3] = -1;
	if (data->sensor_selftest_result[ADSP_FACTORY_MAG].sx < 17 ||
		data->sensor_selftest_result[ADSP_FACTORY_MAG].sy < 22)
		err[5] = -1;
	if (data->sensor_selftest_result[ADSP_FACTORY_MAG].ohx < -600 ||
		data->sensor_selftest_result[ADSP_FACTORY_MAG].ohx > 600)
		err[6] = -1;
	if (data->sensor_selftest_result[ADSP_FACTORY_MAG].ohy < -600 ||
		data->sensor_selftest_result[ADSP_FACTORY_MAG].ohy > 600)
		err[6] = -1;
	if (data->sensor_selftest_result[ADSP_FACTORY_MAG].ohz < -600 ||
		data->sensor_selftest_result[ADSP_FACTORY_MAG].ohz > 600)
		err[6] = -1;

	pr_info("[FACTORY] Test1: err = %d, id = %d\n"
		"[FACTORY] Test3: err = %d\n"
		"[FACTORY] Test4: err = %d, offset = %d,%d,%d\n"
		"[FACTORY] Test5: err = %d, direction = %d\n"
		"[FACTORY] Test6: err = %d, sensitivity = %d,%d\n"
		"[FACTORY] Test7: err = %d, offset = %d,%d,%d\n"
		"[FACTORY] Test2: err = %d\n",
		err[0],	data->sensor_selftest_result[ADSP_FACTORY_MAG].id,
		err[2], err[3],
		data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_x,
		data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_y,
		data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_z,
		err[4], data->sensor_selftest_result[ADSP_FACTORY_MAG].dir,
		err[5],
		data->sensor_selftest_result[ADSP_FACTORY_MAG].sx,
		data->sensor_selftest_result[ADSP_FACTORY_MAG].sy,
		err[6],
		data->sensor_selftest_result[ADSP_FACTORY_MAG].ohx,
		data->sensor_selftest_result[ADSP_FACTORY_MAG].ohy,
		data->sensor_selftest_result[ADSP_FACTORY_MAG].ohz,
		err[1]);

	if (!result1) {
		if (retry < MAG_SELFTEST_TRY_CNT && data->sensor_selftest_result[ADSP_FACTORY_MAG].id == 0) {
			retry++;
			msleep(RAWDATA_TIMER_MS * 2);
			for (i = 0; i < 7; i++)
				err[i] = 0;
			goto retry_mag_selftest;
		}
	}

	return sprintf(buf,
		"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		err[0],	data->sensor_selftest_result[ADSP_FACTORY_MAG].id,
		err[2], err[3],
		data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_x,
		data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_y,
		data->sensor_selftest_result[ADSP_FACTORY_MAG].offset_z,
		err[4], data->sensor_selftest_result[ADSP_FACTORY_MAG].dir,
		err[5],
		data->sensor_selftest_result[ADSP_FACTORY_MAG].sx,
		data->sensor_selftest_result[ADSP_FACTORY_MAG].sy,
		err[6],
		data->sensor_selftest_result[ADSP_FACTORY_MAG].ohx,
		data->sensor_selftest_result[ADSP_FACTORY_MAG].ohy,
		data->sensor_selftest_result[ADSP_FACTORY_MAG].ohz,
		err[1]);
}

static DEVICE_ATTR(selftest, 0664, selttest_show, NULL);
static DEVICE_ATTR(name, 0440, mag_name_show, NULL);
static DEVICE_ATTR(vendor, 0440, mag_vendor_show, NULL);
static DEVICE_ATTR(raw_data, 0664, mag_raw_data_read, mag_raw_data_store);

static struct device_attribute *mag_attrs[] = {
	&dev_attr_selftest,
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_raw_data,
	NULL,
};

static int __devinit yas532_factory_init(void)
{
	adsp_factory_register(ADSP_FACTORY_MAG, mag_attrs);

	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __devexit yas532_factory_exit(void)
{
	adsp_factory_unregister(ADSP_FACTORY_MAG);

	pr_info("[FACTORY] %s\n", __func__);
}

module_init(yas532_factory_init);
module_exit(yas532_factory_exit);
