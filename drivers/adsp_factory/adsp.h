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


#define PID 20000
#define NETLINK_ADSP_FAC 22

#define PROX_READ_NUM	10


/* ENUMS for Selecting the current sensor being used */
enum{
	ADSP_FACTORY_MODE_ACCEL,
	ADSP_FACTORY_MODE_GYRO,
	ADSP_FACTORY_MODE_MAG,
	ADSP_FACTORY_MODE_LIGHT,
	ADSP_FACTORY_MODE_PROX,
	ADSP_FACTORY_SENSOR_MAX
};

enum {
	NETLINK_ATTR_SENSOR_TYPE,
	NETLINK_ATTR_MAX
};

/* Netlink ENUMS Message Protocols */
enum {
	NETLINK_MESSAGE_GET_RAW_DATA,
	NETLINK_MESSAGE_RAW_DATA_RCVD,
	NETLINK_MESSAGE_GET_CALIB_DATA,
	NETLINK_MESSAGE_CALIB_DATA_RCVD,
	NETLINK_MESSAGE_CALIB_STORE_DATA,
	NETLINK_MESSAGE_CALIB_STORE_RCVD,
	NETLINK_MESSAGE_SELFTEST_SHOW_DATA,
	NETLINK_MESSAGE_SELFTEST_SHOW_RCVD,
	NETLINK_MESSAGE_STOP_RAW_DATA,
	NETLINK_MESSAGE_MAX
};

struct msg_data{
	int sensor_type;
};

struct sensor_value {
	unsigned int sensor_type;
	struct {
		s16 x;
		s16 y;
		s16 z;

	};
};

struct sensor_stop_value {
  unsigned int sensor_type;
  int result;
};

/* Structs used in calibration show and store */
struct sensor_calib_value {
	unsigned int sensor_type;
	struct {
		uint8_t x;
		uint8_t y;
		uint8_t z;
	};
	int result;
};

struct sensor_calib_store_result{
	unsigned int sensor_type;
	int nCalibstoreresult;
};

struct trans_value{
	unsigned int sensor_type;
};

/* Struct used for selftest */
struct sensor_selftest_show_result{
	unsigned int sensor_type;
	int nSelftestresult1;
	int nSelftestresult2;
};

/* Main struct containing all the data */
struct adsp_data {
	struct device *adsp;
	struct device *sensor_device[ADSP_FACTORY_SENSOR_MAX];
	struct sensor_value sensor_data[ADSP_FACTORY_SENSOR_MAX];
	struct sensor_calib_value sensor_calib_data[ADSP_FACTORY_SENSOR_MAX];
	struct sensor_calib_store_result sensor_calib_result[ADSP_FACTORY_SENSOR_MAX];
	struct sensor_selftest_show_result sensor_selftest_result[ADSP_FACTORY_SENSOR_MAX];
    struct sensor_stop_value sensor_stop_data[ADSP_FACTORY_SENSOR_MAX];
	struct adsp_fac_ctl *ctl[ADSP_FACTORY_SENSOR_MAX];
	struct sock		*adsp_skt;
    struct timer_list our_timer;
	unsigned int data_ready_flag;
	unsigned int calib_ready_flag;
	unsigned int calib_store_ready_flag;
	unsigned int selftest_ready_flag;
	int prox_average[PROX_READ_NUM];
};

struct adsp_fac_ctl {
	int (*init_fnc) (struct adsp_data *data);
	int (*exit_fnc) (struct adsp_data *data);
	int (*receive_data_fnc) (struct adsp_data *data);
};

int adsp_factory_register( char *dev_name,int type, struct device_attribute *attributes[],struct adsp_fac_ctl *fact_ctl );
int adsp_unicast( int sensor_type , int message, int flags, u32 pid);

extern struct mutex factory_mutex;
#endif
