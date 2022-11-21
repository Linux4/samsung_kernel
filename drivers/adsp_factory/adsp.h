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
#ifndef __ADSP_SENSOR_H__
#define __ADSP_SENSOR_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sensors.h>
#include <linux/adsp/adsp_ft_common.h>
#define TIMEOUT_CNT 50

/* Main struct containing all the data */
struct adsp_data {
	struct device *adsp;
	struct device *sensor_device[ADSP_FACTORY_SENSOR_MAX];
	struct device *mobeam_device;
	struct device_attribute **sensor_attr[ADSP_FACTORY_SENSOR_MAX];
	struct sensor_value sensor_data[ADSP_FACTORY_SENSOR_MAX];
	struct sensor_calib_value sensor_calib_data[ADSP_FACTORY_SENSOR_MAX];
	struct sensor_calib_store_result sensor_calib_result[ADSP_FACTORY_SENSOR_MAX];
	struct sensor_selftest_show_result sensor_selftest_result[ADSP_FACTORY_SENSOR_MAX];
	struct sensor_mag_factory_value sensor_mag_factory_result;
	struct sensor_gyro_st_value gyro_st_result;
	struct sensor_accel_lpf_value accel_lpf_result;
	struct sock *adsp_skt;
	struct work_struct timer_stop_data_work;
	struct timer_list command_timer[ADSP_FACTORY_SENSOR_MAX];
	struct mutex raw_stream_lock[ADSP_FACTORY_SENSOR_MAX];
	struct prox_th_value pth;
	struct dump_register dump_registers[ADSP_FACTORY_PROX + 1];
	unsigned int stop_raw_data_flag;
	unsigned int raw_data_stream;
	unsigned int data_ready_flag;
	unsigned int calib_ready_flag;
	unsigned int calib_store_ready_flag;
	unsigned int selftest_ready_flag;
	unsigned int magtest_ready_flag;
	unsigned int data_ready;
	unsigned int dump_ready_flag;
	unsigned int dump_status;
	unsigned int th_read_flag;
	unsigned int dump_reg_ready_flag;
	void *pdata;
	bool sysfs_created[ADSP_FACTORY_SENSOR_MAX];
	struct mutex remove_sysfs_mutex;
};

int adsp_get_sensor_data(int sensor_type);
int adsp_factory_register(unsigned int type, struct device_attribute *attributes[]);
int adsp_factory_unregister(unsigned int type);
int adsp_mobeam_register(struct device_attribute *attributes[]);
int adsp_mobeam_unregister(struct device_attribute *attributes[]);
bool adsp_start_raw_data(int sensor_type);
void adsp_stop_raw_data(int sensor_type);
int adsp_unicast(void *param, int param_size, int type, u32 portid, int flags);
#endif
