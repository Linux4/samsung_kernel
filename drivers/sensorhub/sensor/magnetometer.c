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
#include "../debug/shub_debug.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_dev_core.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_wakelock.h"
#include "../utility/shub_file_manager.h"
#include "flip_cover_detector.h"
#include "magnetometer.h"

#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

#define MAG_CALIBRATION_FILE_PATH "/efs/FactoryApp/mag_cal_data"

#define MAG_RECEIVE_EVENT_SIZE(x) (((x) * 3) + 1)

typedef struct magnetometer_chipset_funcs *(get_magnetometer_function_pointer)(char *);
get_magnetometer_function_pointer *get_mag_funcs_ary[] = {
	get_magnetic_ak09918c_function_pointer,
	get_magnetic_yas539_function_pointer,
	get_magnetic_mmc5633_function_pointer,
};

static void parse_dt_magnetometer(struct device *dev)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	shub_infof("");
	if (data->chipset_funcs->parse_dt)
		data->chipset_funcs->parse_dt(dev);
}

static int set_mag_position(int position)
{
	int ret = 0;
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	data->position = position;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, SENSOR_AXIS, (char *)&(data->position),
				sizeof(data->position));
	if (ret < 0) {
		shub_errf("CMD fail %d\n", ret);
		return ret;
	}

	shub_infof("%u", data->position);

	return ret;
}

static int get_mag_position(void)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	return data->position;
}

int set_mag_matrix(struct magnetometer_data *data)
{
	int ret = 0;

	shub_infof();

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, MAGNETIC_STATIC_MATRIX,
				(char *)data->mag_matrix, data->mag_matrix_len);
	shub_infof("%u", data->position);

	if (ret < 0) {
		shub_errf("failed %d", ret);
		return ret;
	}

	return 0;
}

int set_mag_cover_matrix(struct magnetometer_data *data)
{
	int ret = 0;

	if (!data->cover_matrix)
		return 0;

	shub_infof();

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, MAGNETIC_COVER_MATRIX,
				(char *)data->cover_matrix, data->mag_matrix_len);
	shub_infof("%u", data->position);

	if (ret < 0) {
		shub_errf("failed %d", ret);
		return ret;
	}

	return 0;
}

int get_mag_sensor_value(char *dataframe, int *index, struct sensor_event *event, int frame_len)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD);
	struct mag_event *sensor_value = (struct mag_event *)event->value;

	if (sensor->receive_event_size == sizeof(struct mag_event)) {
		memcpy(sensor_value, dataframe + *index, sizeof(struct mag_event));
		*index += sensor->receive_event_size;
	} else {
		s16 temp_mag_value[3];

		memcpy(&temp_mag_value, dataframe + *index, sizeof(temp_mag_value));
		*index += sizeof(temp_mag_value);
		sensor_value->x = (s32) temp_mag_value[0];
		sensor_value->y = (s32) temp_mag_value[1];
		sensor_value->z = (s32) temp_mag_value[2];
		memcpy(&sensor_value->accuracy, dataframe + *index, sizeof(sensor_value->accuracy));
		*index += sizeof(sensor_value->accuracy);
	}
	return 0;
}

static int open_mag_calibration_file(void)
{
	int ret = 0;
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	ret = shub_file_read(MAG_CALIBRATION_FILE_PATH, data->cal_data, data->cal_data_len, 0);
	if (ret != data->cal_data_len) {
		ret = -EIO;
		shub_errf("Can't read calibration file %d", ret);
		memset(data->cal_data, 0, data->cal_data_len);
	}

	set_open_cal_result(SENSOR_TYPE_GEOMAGNETIC_FIELD, ret);
	return ret;
}

static int save_mag_calibration_file(void)
{
	int ret = 0;
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	ret = shub_file_write_no_wait(MAG_CALIBRATION_FILE_PATH, data->cal_data, data->cal_data_len, 0);
	if (ret != data->cal_data_len) {
		shub_errf("Can't write mag cal to file");
		ret = -EIO;
	}

	return ret;
}

static int parsing_mag_calibration(char *dataframe, int *index, int frame_len)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	if (*index + data->cal_data_len > frame_len) {
		shub_errf("parssing error");
		return -EINVAL;
	}

	shub_infof("Mag caldata received from MCU(%d)", data->cal_data_len);
	memcpy(data->cal_data, dataframe + (*index), data->cal_data_len);
	shub_wake_lock();
	save_mag_calibration_file();
	shub_wake_unlock();
	(*index) += data->cal_data_len;

	return 0;
}

static int set_mag_cal(struct magnetometer_data *data)
{
	int ret = 0;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, CAL_DATA,
				(char *)data->cal_data, data->cal_data_len);
	if (ret < 0)
		shub_errf("shub_send_command fail %d", ret);

	return ret;
}

int init_magnetometer_chipset(void)
{
	uint64_t i;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD);
	struct magnetometer_data *data = sensor->data;
	struct magnetometer_chipset_funcs *funcs;

	shub_infof("");

	if (data->chipset_funcs)
		return 0;

	for (i = 0; i < ARRAY_SIZE(get_mag_funcs_ary); i++) {
		funcs = get_mag_funcs_ary[i](sensor->spec.name);
		if (funcs) {
			data->chipset_funcs = funcs;
			if (data->chipset_funcs->init)
				data->chipset_funcs->init();
			break;
		}
	}

	if (!data->chipset_funcs) {
		shub_errf("cannot find magnetometer sensor chipset");
		return -EINVAL;
	}

	if (data->cal_data_len) {
		data->cal_data = kzalloc(data->cal_data_len, GFP_KERNEL);
		if (!data->cal_data)
			return -ENOMEM;
	}

	if (data->mag_matrix_len) {
		data->mag_matrix = kzalloc(data->mag_matrix_len, GFP_KERNEL);
		if (!data->mag_matrix)
			return -ENOMEM;

		if (get_sensor(SENSOR_TYPE_FLIP_COVER_DETECTOR) && check_flip_cover_detector_supported()) {
			data->cover_matrix = kzalloc(data->mag_matrix_len, GFP_KERNEL);
			if (!data->cover_matrix)
				return -ENOMEM;
		}
	}

	parse_dt_magnetometer(get_shub_device());
	return 0;
}

static int sync_magnetometer_status(void)
{
	int ret = 0;
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	shub_infof();
	ret = set_mag_position(data->position);
	if (ret < 0) {
		shub_errf("set_position failed");
		return ret;
	}

	ret = set_mag_matrix(data);
	if (ret < 0) {
		shub_errf("initialize magnetic sensor failed");
		return ret;
	}

	ret = set_mag_cal(data);
	if (ret < 0)
		shub_errf("set_mag_cal failed");

	set_mag_cover_matrix(data);

	return ret;
}

static void print_magnetometer_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD);
	struct sensor_event *event = &(sensor->event_buffer);
	struct mag_event *sensor_value = (struct mag_event *)(event->value);

	shub_info("%s(%u) : %d, %d, %d, %d (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_GEOMAGNETIC_FIELD,
		  sensor_value->x, sensor_value->y, sensor_value->z, sensor_value->accuracy, event->timestamp,
		  sensor->sampling_period, sensor->max_report_latency);
}

int init_magnetometer(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "geomagnetic_sensor");
		sensor->report_mode_continuous = true;

		if (sensor->spec.version >= MAG_EVENT_SIZE_4BYTE_VERSION)
			sensor->receive_event_size = MAG_RECEIVE_EVENT_SIZE(sizeof(s32));
		else
			sensor->receive_event_size = MAG_RECEIVE_EVENT_SIZE(sizeof(s16));

		shub_infof("receive_event_size : %d", sensor->receive_event_size);

		sensor->report_event_size = sizeof(struct mag_event);
		sensor->event_buffer.value = kzalloc(sizeof(struct mag_event), GFP_KERNEL);
		if (!sensor->event_buffer.value)
			goto err_no_mem;

		sensor->data = kzalloc(sizeof(struct magnetometer_data), GFP_KERNEL);
		if (!sensor->data)
			goto err_no_mem;

		sensor->funcs = kzalloc(sizeof(struct sensor_funcs), GFP_KERNEL);
		if (!sensor->funcs)
			goto err_no_mem;

		sensor->funcs->sync_status = sync_magnetometer_status;
		sensor->funcs->set_position = set_mag_position;
		sensor->funcs->get_position = get_mag_position;
		sensor->funcs->print_debug = print_magnetometer_debug;
		sensor->funcs->parsing_data = parsing_mag_calibration;
		sensor->funcs->init_chipset = init_magnetometer_chipset;
		sensor->funcs->open_calibration_file = open_mag_calibration_file;
		sensor->funcs->get_sensor_value = get_mag_sensor_value;
	} else {
		struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

		kfree(data->cal_data);
		data->cal_data = NULL;

		kfree(data->mag_matrix);
		data->mag_matrix = NULL;

		kfree(data->cover_matrix);
		data->cover_matrix = NULL;

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

	kfree(sensor->funcs);
	sensor->funcs = NULL;

	kfree(sensor->data);
	sensor->data = NULL;

	return -ENOMEM;
}

int init_magnetometer_power(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GEOMAGNETIC_POWER);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "geomagnetic_power");
		sensor->receive_event_size = sizeof(struct mag_power_event);
		sensor->report_event_size = sizeof(struct mag_power_event);
		sensor->event_buffer.value = kzalloc(sizeof(struct mag_power_event), GFP_KERNEL);
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
