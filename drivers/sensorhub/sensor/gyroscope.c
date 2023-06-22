/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#include "../comm/shub_comm.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_dev_core.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_file_manager.h"
#include "../utility/shub_wakelock.h"
#include "accelerometer.h"
#include "gyroscope.h"

#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

#define GYRO_CALIBRATION_FILE_PATH "/efs/FactoryApp/gyro_cal_data"

void parse_dt_gyroscope(struct device *dev)
{
	struct gyroscope_data *data = get_sensor(SENSOR_TYPE_GYROSCOPE)->data;

	if (data->chipset_funcs->parse_dt)
		data->chipset_funcs->parse_dt(dev);
}

int set_gyro_position(int position)
{
	int ret = 0;
	struct gyroscope_data *data = get_sensor(SENSOR_TYPE_GYROSCOPE)->data;

	data->position = position;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_GYROSCOPE, SENSOR_AXIS, (char *)&(data->position),
				sizeof(data->position));
	if (ret < 0) {
		shub_errf("CMD fail %d\n", ret);
		return ret;
	}

	shub_infof("G : %u", data->position);

	return ret;
}

int get_gyro_position(void)
{
	struct gyroscope_data *data = get_sensor(SENSOR_TYPE_GYROSCOPE)->data;

	return data->position;
}

static int open_gyro_calibration_file(void)
{
	int ret = 0;
	struct gyroscope_data *data = get_sensor(SENSOR_TYPE_GYROSCOPE)->data;

	shub_infof();

	ret = shub_file_read(GYRO_CALIBRATION_FILE_PATH, (char *)&data->cal_data, sizeof(data->cal_data), 0);
	if (ret != sizeof(data->cal_data))
		ret = -EIO;

	shub_infof("open gyro calibration %d, %d, %d", data->cal_data.x, data->cal_data.y, data->cal_data.z);

	return ret;
}

int save_gyro_calibration_file(s16 *cal_data)
{
	int ret;
	struct gyroscope_data *data = get_sensor(SENSOR_TYPE_GYROSCOPE)->data;

	data->cal_data.x = cal_data[0];
	data->cal_data.y = cal_data[1];
	data->cal_data.z = cal_data[2];

	shub_info("do gyro calibrate %d, %d, %d", data->cal_data.x, data->cal_data.y, data->cal_data.z);

	ret = shub_file_write_no_wait(GYRO_CALIBRATION_FILE_PATH, (char *)&data->cal_data, sizeof(data->cal_data), 0);
	if (ret != sizeof(data->cal_data)) {
		shub_err("Can't write gyro cal to file");
		ret = -EIO;
	}

	return ret;
}

int parsing_gyro_calibration(char *dataframe, int *index, int frame_len)
{
	s16 caldata[3] = {0, };

	if (*index + sizeof(caldata) > frame_len) {
		shub_errf("parsing error");
		return -EINVAL;
	}

	shub_infof("Gyro caldata received from MCU");
	memcpy(caldata, dataframe + (*index), sizeof(caldata));
	shub_wake_lock();
	save_gyro_calibration_file(caldata);
	shub_wake_unlock();
	(*index) += sizeof(caldata);

	return 0;
}

int set_gyro_cal(struct gyroscope_data *data)
{
	int ret = 0;
	s16 gyro_cal[3] = {0, };

	if (!get_sensor_probe_state(SENSOR_TYPE_GYROSCOPE)) {
		shub_infof("[SHUB] Skip this function!!!, gyro sensor is not connected\n");
		return ret;
	}

	gyro_cal[0] = data->cal_data.x;
	gyro_cal[1] = data->cal_data.y;
	gyro_cal[2] = data->cal_data.z;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_GYROSCOPE, CAL_DATA, (char *)gyro_cal, sizeof(gyro_cal));

	if (ret < 0) {
		shub_errf("CMD Fail %d", ret);
		return ret;
	}

	shub_infof("set temp gyro cal data %d, %d, %d\n", gyro_cal[0], gyro_cal[1], gyro_cal[2]);
	shub_infof("set gyro cal data %d, %d, %d\n", data->cal_data.x, data->cal_data.y, data->cal_data.z);

	return ret;
}

typedef struct gyroscope_chipset_funcs *(get_gyroscope_function_pointer)(char *);

get_gyroscope_function_pointer *get_gyro_funcs_ary[] = {
	get_gyroscope_icm42605m_function_pointer,
	get_gyroscope_lsm6dsl_function_pointer,
	get_gyroscope_lsm6dsotr_function_pointer,
	get_gyroscope_icm42632m_function_pointer,
};

int init_gyroscope_chipset(void)
{
	uint64_t i;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GYROSCOPE);
	struct gyroscope_data *data = sensor->data;
	struct gyroscope_chipset_funcs *funcs;

	if (data->chipset_funcs)
		return 0;

	shub_infof("");

	for (i = 0; i < ARRAY_SIZE(get_gyro_funcs_ary); i++) {
		funcs = get_gyro_funcs_ary[i](sensor->spec.name);
		if (funcs) {
			data->chipset_funcs = funcs;
			break;
		}
	}

	if (!data->chipset_funcs) {
		shub_errf("cannot find gyroscope sensor chipset");
		return -EINVAL;
	}

	parse_dt_gyroscope(get_shub_device());

	return 0;
}

int sync_gyroscope_status(void)
{
	int ret = 0;
	struct gyroscope_data *data = get_sensor(SENSOR_TYPE_GYROSCOPE)->data;

	shub_infof();
	ret = set_gyro_position(data->position);
	if (ret < 0) {
		shub_errf("set_position failed");
		return ret;
	}

	ret = set_gyro_cal(data);
	if (ret < 0) {
		shub_errf("set_gyro_cal failed");
		return ret;
	}

	return ret;
}

void print_gyroscope_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GYROSCOPE);
	struct sensor_event *event = &(sensor->event_buffer);
	struct gyro_event *sensor_value = (struct gyro_event *)(event->value);

	shub_info("%s(%u) : %d, %d, %d (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_GYROSCOPE, sensor_value->x,
		  sensor_value->y, sensor_value->z, event->timestamp, sensor->sampling_period,
		  sensor->max_report_latency);
}

int init_gyroscope(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GYROSCOPE);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "gyro_sensor");
		sensor->report_mode_continuous = true;
		sensor->receive_event_size = 6;
		sensor->report_event_size = 6;
		sensor->event_buffer.value = kzalloc(sizeof(struct gyro_event), GFP_KERNEL);
		if (!sensor->event_buffer.value)
			goto err_no_mem;

		sensor->data = kzalloc(sizeof(struct gyroscope_data), GFP_KERNEL);
		if (!sensor->data)
			goto err_no_mem;

		sensor->funcs = kzalloc(sizeof(struct sensor_funcs), GFP_KERNEL);
		if (!sensor->funcs)
			goto err_no_mem;

		sensor->funcs->sync_status = sync_gyroscope_status;
		sensor->funcs->set_position = set_gyro_position;
		sensor->funcs->get_position = get_gyro_position;
		sensor->funcs->print_debug = print_gyroscope_debug;
		sensor->funcs->parsing_data = parsing_gyro_calibration;
		sensor->funcs->init_chipset = init_gyroscope_chipset;
		sensor->funcs->open_calibration_file = open_gyro_calibration_file;

	} else {
		kfree(sensor->data);
		sensor->data = NULL;

		kfree(sensor->funcs);
		sensor->funcs = NULL;

		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}

	return 0;

err_no_mem:
	kfree(sensor->event_buffer.value);
	sensor->event_buffer.value = NULL;

	kfree(sensor->data);
	sensor->data = NULL;

	kfree(sensor->funcs);
	sensor->funcs = NULL;

	return -ENOMEM;
}

int init_interrupt_gyroscope(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_INTERRUPT_GYRO);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "interrupt_gyro_sensor");
		sensor->receive_event_size = 6;
		sensor->report_event_size = 6;
		sensor->event_buffer.value = kzalloc(sizeof(struct gyro_event), GFP_KERNEL);
		if (!sensor->event_buffer.value)
			goto err_no_mem;

	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}

	return 0;

err_no_mem:
	kfree(sensor->event_buffer.value);
	sensor->event_buffer.value = NULL;

	return -ENOMEM;
}

int init_vdis_gyroscope(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_VDIS_GYROSCOPE);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "vdis_gyro_sensor");
		sensor->receive_event_size = 6;
		sensor->report_event_size = 6;
		sensor->event_buffer.value = kzalloc(sizeof(struct gyro_event), GFP_KERNEL);
		if (!sensor->event_buffer.value)
			goto err_no_mem;
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}

	return 0;

err_no_mem:
	kfree(sensor->event_buffer.value);
	sensor->event_buffer.value = NULL;

	return -ENOMEM;
}

