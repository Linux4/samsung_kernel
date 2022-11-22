/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include <linux/slab.h>
#include "../ssp.h"
#include "../sensors_core.h"
#include "../ssp_data.h"
#include "../ssp_factory.h"
#include "../ssp_cmd_define.h"
#include "../ssp_comm.h"

enum {
	OFF = 0,
	ON = 1
};

enum {
	X = 0,
	Y = 1,
	Z = 2,
	AXIS_MAX
};

#define ATTACH "ATTACH"
#define DETACH "DETACH"
#define FACTORY_PASS "PASS"
#define FACTORY_FAIL "FAIL"

#define AXIS_SELECT      X
#define THRESHOLD        700
#define DETACH_MARGIN    100
#define SATURATION_VALUE 4900
#define MAG_DELAY_MS     50

struct factory_cover_status_data {
	char cover_status[10];
	uint8_t axis_select;
	int32_t threshold;
	int32_t init[AXIS_MAX];
	int32_t attach[AXIS_MAX];
	int32_t attach_extremum[AXIS_MAX];
	int32_t detach[AXIS_MAX];
	char attach_result[10];
	char detach_result[10];
	char final_result[10];

	uint8_t factory_test_status;
	int32_t attach_diff;
	int32_t detach_diff;
	int32_t failed_attach_max;
	int32_t failed_attach_min;
	uint8_t saturation;

	int event_count;
};

static struct factory_cover_status_data *factory_data;

static ssize_t nfc_cover_status_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return data->flip_cover_status;
}

static ssize_t nfc_cover_status_store(struct device *dev,
				      struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int status = 0;
	int ret = 0;

	ret = kstrtoint(buf, 10, &status);
	if (ret < 0)
		return ret;

	if (status < 0 || status > 1) {
		ssp_errf("invalid status %d", status);
		return -EINVAL;
	}

	data->flip_cover_status = status;
	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_FLIP_COVER_DETECTOR, LIBRARY_DATA, 0,
			 (char *)&data->flip_cover_status, sizeof(data->flip_cover_status), NULL, NULL);
	if (ret < 0)
		ssp_errf("send flip_cover_status failed");

	ssp_infof("flip_cover_status is %d", data->flip_cover_status);

	return size;
}

static void factory_data_init(struct ssp_data *data)
{
	int mag_data[AXIS_MAX];
	int axis = 0;

	ssp_infof("");

	memset(factory_data, 0, sizeof(struct factory_cover_status_data));
	mag_data[X] = data->buf[SENSOR_TYPE_FLIP_COVER_DETECTOR].uncal_mag_x;
	mag_data[Y] = data->buf[SENSOR_TYPE_FLIP_COVER_DETECTOR].uncal_mag_y;
	mag_data[Z] = data->buf[SENSOR_TYPE_FLIP_COVER_DETECTOR].uncal_mag_z;

	ssp_infof("[FACTORY] init data : %d %d %d", mag_data[X], mag_data[Y], mag_data[Z]);

	factory_data->axis_select = AXIS_SELECT;
	factory_data->threshold = THRESHOLD;

	for (axis = X; axis < AXIS_MAX; axis++) {
		factory_data->init[axis] = mag_data[axis];
		factory_data->attach[axis] = mag_data[axis];
	}

	factory_data->failed_attach_max = data->buf[SENSOR_TYPE_FLIP_COVER_DETECTOR].uncal_mag_x;
	factory_data->failed_attach_min = data->buf[SENSOR_TYPE_FLIP_COVER_DETECTOR].uncal_mag_x;

	snprintf(factory_data->cover_status, 10, DETACH);
	snprintf(factory_data->attach_result, 10, FACTORY_FAIL);
	snprintf(factory_data->detach_result, 10, FACTORY_FAIL);
	snprintf(factory_data->final_result, 10, FACTORY_FAIL);
}

void check_cover_detection_factory(struct ssp_data *data)
{
	int axis = 0;
	int axis_select = AXIS_SELECT;
	int mag_data[AXIS_MAX];

	if (factory_data->event_count == 0) {
		factory_data_init(data);
		factory_data->event_count++;
	}

	mag_data[X] = data->buf[SENSOR_TYPE_FLIP_COVER_DETECTOR].uncal_mag_x;
	mag_data[Y] = data->buf[SENSOR_TYPE_FLIP_COVER_DETECTOR].uncal_mag_y;
	mag_data[Z] = data->buf[SENSOR_TYPE_FLIP_COVER_DETECTOR].uncal_mag_z;
	factory_data->saturation = data->buf[SENSOR_TYPE_FLIP_COVER_DETECTOR].saturation;

	ssp_infof("[FACTORY] uncal mag : %d %d %d, saturation : %d",
		  mag_data[X], mag_data[Y], mag_data[Z], factory_data->saturation);

	if (!strcmp(factory_data->cover_status, DETACH)) {
		if (mag_data[axis_select] > factory_data->failed_attach_max) {
			factory_data->failed_attach_max = mag_data[axis_select];

			if (abs(factory_data->failed_attach_max - factory_data->init[axis_select])
				> abs(factory_data->failed_attach_min - factory_data->init[axis_select])) {
				for (axis = X; axis < AXIS_MAX; axis++)
					factory_data->attach[axis] = mag_data[axis];
			}
		} else if (mag_data[axis_select] < factory_data->failed_attach_min) {
			factory_data->failed_attach_min = mag_data[axis_select];

			if (abs(factory_data->failed_attach_max - factory_data->init[axis_select])
				< abs(factory_data->failed_attach_min - factory_data->init[axis_select])) {
				for (axis = X; axis < AXIS_MAX; axis++)
					factory_data->attach[axis] = mag_data[axis];
			}
		}

		ssp_infof("[FACTORY] : failed_attach_max=%d, failed_attach_min=%d",
			  factory_data->failed_attach_max, factory_data->failed_attach_min);

		factory_data->attach_diff = mag_data[axis_select] - factory_data->init[axis_select];

		if (abs(factory_data->attach_diff) > factory_data->threshold) {
			snprintf(factory_data->cover_status, 10, ATTACH);
			snprintf(factory_data->attach_result, 10, FACTORY_PASS);
			for (axis = X; axis < AXIS_MAX; axis++) {
				factory_data->attach[axis] = mag_data[axis];
				factory_data->attach_extremum[axis] = mag_data[axis];
			}
			ssp_infof("[FACTORY] : COVER ATTACHED");
		}
	} else {
		if (factory_data->attach_diff > 0) {
			if (factory_data->saturation) {
				for (axis = X; axis < AXIS_MAX; axis++)
					mag_data[axis] = SATURATION_VALUE;
			}

			if (mag_data[axis_select] > factory_data->attach_extremum[axis_select]) {
				for (axis = X; axis < AXIS_MAX; axis++) {
					factory_data->attach_extremum[axis] = mag_data[axis];
					factory_data->detach[axis] = 0;
				}
			}
		} else {
			if (factory_data->saturation) {
				for (axis = X; axis < AXIS_MAX; axis++)
					mag_data[axis] = -SATURATION_VALUE;
			}

			if (mag_data[axis_select] < factory_data->attach_extremum[axis_select]) {
				for (axis = X; axis < AXIS_MAX; axis++) {
					factory_data->attach_extremum[axis] = mag_data[axis];
					factory_data->detach[axis] = 0;
				}
			}
		}

		factory_data->detach_diff = mag_data[axis_select] - factory_data->attach_extremum[axis_select];

		if (factory_data->attach_diff > 0) {
			if (mag_data[axis_select] < (factory_data->attach_extremum[axis_select] - DETACH_MARGIN)) {
				for (axis = X; axis < AXIS_MAX; axis++)
					factory_data->detach[axis] = mag_data[axis];
			}

			if (factory_data->detach_diff < -factory_data->threshold) {
				snprintf(factory_data->cover_status, 10, DETACH);
				snprintf(factory_data->detach_result, 10, FACTORY_PASS);
				snprintf(factory_data->final_result, 10, FACTORY_PASS);
				factory_data->factory_test_status = OFF;
				ssp_infof("[FACTORY] : COVER_DETACHED");
			}
		} else {
			if (mag_data[axis_select] > (factory_data->attach_extremum[axis_select] + DETACH_MARGIN)) {
				for (axis = X; axis < AXIS_MAX; axis++)
					factory_data->detach[axis] = mag_data[axis];
			}

			if (factory_data->detach_diff > factory_data->threshold) {
				snprintf(factory_data->cover_status, 10, DETACH);
				snprintf(factory_data->detach_result, 10, FACTORY_PASS);
				snprintf(factory_data->final_result, 10, FACTORY_PASS);
				factory_data->factory_test_status = OFF;
				ssp_infof("[FACTORY]: COVER_DETACHED");
			}
		}
	}

	ssp_infof("[FACTORY] : cover_status=%s, axis_select=%d, thd=%d, "	\
		  "x_init=%d, x_attach=%d, x_min_max=%d, x_detach=%d, "		\
		  "y_init=%d, y_attach=%d, y_min_max=%d, y_detach=%d, "		\
		  "z_init=%d, z_attach=%d, z_min_max=%d, z_detach=%d, "		\
		  "attach_result=%s, detach_result=%s, final_result=%s",
		  factory_data->cover_status, factory_data->axis_select, factory_data->threshold,
		  factory_data->init[X], factory_data->attach[X], factory_data->attach_extremum[X],
		  factory_data->detach[X], factory_data->init[Y], factory_data->attach[Y],
		  factory_data->attach_extremum[Y], factory_data->detach[Y], factory_data->init[Z],
		  factory_data->attach[Z], factory_data->attach_extremum[Z], factory_data->detach[Z],
		  factory_data->attach_result, factory_data->detach_result, factory_data->final_result);
}

static void enable_factory_test(struct ssp_data *data, int request)
{
	int ret = 0;

	factory_data->factory_test_status = request;

	data->factory_cover_status = request;
	ret = ssp_send_command(data, CMD_SETVALUE, SENSOR_TYPE_FLIP_COVER_DETECTOR, SENSOR_FACTORY, 0,
			       (char *)&data->factory_cover_status, sizeof(data->factory_cover_status), NULL, NULL);
	if (ret < 0) {
		ssp_errf("send factory_cover_status failed");
		return;
	}

	if (request == ON)
		factory_data->event_count = 0;
}

static ssize_t factory_cover_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssp_infof("[FACTORY] status=%s, axis=%d, thd=%d, "		\
		  "x_init=%d, x_attach=%d, x_min_max=%d, x_detach=%d, "	\
		  "y_init=%d, y_attach=%d, y_min_max=%d, y_detach=%d, "	\
		  "z_init=%d, z_attach=%d, z_min_max=%d, z_detach=%d, "	\
		  "attach_result=%s, detach_result=%s, final_result=%s",
		  factory_data->cover_status, factory_data->axis_select, factory_data->threshold,
		  factory_data->init[X], factory_data->attach[X], factory_data->attach_extremum[X],
		  factory_data->detach[X], factory_data->init[Y], factory_data->attach[Y],
		  factory_data->attach_extremum[Y], factory_data->detach[Y], factory_data->init[Z],
		  factory_data->attach[Z], factory_data->attach_extremum[Z], factory_data->detach[Z],
		  factory_data->attach_result, factory_data->detach_result, factory_data->final_result);

	return snprintf(buf, PAGE_SIZE, "%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s,%s,%s\n",
		factory_data->cover_status, factory_data->axis_select, factory_data->threshold,
		factory_data->init[X], factory_data->attach[X], factory_data->attach_extremum[X],
		factory_data->detach[X], factory_data->init[Y], factory_data->attach[Y],
		factory_data->attach_extremum[Y], factory_data->detach[Y], factory_data->init[Z],
		factory_data->attach[Z], factory_data->attach_extremum[Z], factory_data->detach[Z],
		factory_data->attach_result, factory_data->detach_result, factory_data->final_result);
}

static ssize_t factory_cover_status_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t size)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	int factory_test_request = 0;
	int ret = 0;

	ret = kstrtoint(buf, 10, &factory_test_request);
	if (ret < 0)
		return ret;

	if (factory_test_request < 0 || factory_test_request > 1) {
		ssp_errf("[FACTORY] invalid status %d", factory_test_request);
		return -EINVAL;
	}

	if (factory_test_request == ON /*&& factory_data->factory_test_status == OFF*/)
		enable_factory_test(data, ON);
	else if (factory_test_request == OFF /*&& factory_data->factory_test_status == ON*/)
		enable_factory_test(data, OFF);

	ssp_infof("[FACTORY] factory_cover_status is %d", data->factory_cover_status);
	return size;
}

static DEVICE_ATTR(nfc_cover_status, 0664, nfc_cover_status_show, nfc_cover_status_store);
static DEVICE_ATTR(factory_cover_status, 0664, factory_cover_status_show, factory_cover_status_store);

static struct device_attribute *nfc_cover_attrs[] = {
	&dev_attr_nfc_cover_status,
	&dev_attr_factory_cover_status,
	NULL,
};

void initialize_flip_cover_detector_factorytest(struct ssp_data *data)
{
	sensors_register(data->devices[SENSOR_TYPE_GEOMAGNETIC_FIELD], data, nfc_cover_attrs,
		 "flip_cover_detector_sensor");

	factory_data = kzalloc(sizeof(*factory_data), GFP_KERNEL);
	if (factory_data == NULL)
		ssp_errf("[FACTORY] Memory allocation failed for factory_data");
}

void remove_flip_cover_detector_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->devices[SENSOR_TYPE_GEOMAGNETIC_FIELD], nfc_cover_attrs);

	if (factory_data != NULL) {
		if (factory_data->factory_test_status == ON)
			enable_factory_test(data, OFF);

		kfree(factory_data);
	}
}
