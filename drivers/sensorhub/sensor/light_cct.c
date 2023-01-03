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

#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_utility.h"
#include "light.h"

#include <linux/slab.h>

static int light_cct_log_cnt;

static int enable_light_cct(void)
{
	light_cct_log_cnt = 0;

	return 0;
}

static void report_event_light_cct(void)
{
	struct light_cct_event *sensor_value =
	    (struct light_cct_event *)(get_sensor_event(SENSOR_TYPE_LIGHT_CCT)->value);

	if (light_cct_log_cnt < 3) {
		shub_info("Light cct Sensor : lux=%u r=%d g=%d b=%d c=%d atime=%d again=%d", sensor_value->lux,
			  sensor_value->r, sensor_value->g, sensor_value->b, sensor_value->w, sensor_value->a_time,
			  sensor_value->a_gain);

		light_cct_log_cnt++;
	}
}

int get_light_cct_sensor_value(char *dataframe, int *index, struct sensor_event *event, int frame_len)
{
	struct light_data *data = get_sensor(SENSOR_TYPE_LIGHT)->data;
	struct light_cct_event *sensor_value = (struct light_cct_event *)event->value;
	int offset_raw_data = offsetof(struct light_cct_event, r);

	if (!data->ddi_support)
		offset_raw_data -= sizeof(sensor_value->roi);

	memcpy(&sensor_value->lux, dataframe + *index, offset_raw_data);
	*index += offset_raw_data;

	if (data->ddi_support) {
		memcpy(&sensor_value->roi, dataframe + *index, sizeof(sensor_value->roi));
		*index += sizeof(sensor_value->roi);
	}

	memcpy(&sensor_value->r, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;
	memcpy(&sensor_value->g, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;
	memcpy(&sensor_value->b, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;
	memcpy(&sensor_value->w, dataframe + *index, data->raw_data_size);
	*index += data->raw_data_size;

	memcpy(&sensor_value->a_time, dataframe + *index, sizeof(sensor_value->a_time));
	*index += sizeof(sensor_value->a_time);
	memcpy(&sensor_value->a_gain, dataframe + *index, sizeof(sensor_value->a_gain));
	*index += sizeof(sensor_value->a_gain);

	return 0;
}

void print_light_cct_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT_CCT);
	struct sensor_event *event = &(sensor->event_buffer);
	struct light_cct_event *sensor_value = (struct light_cct_event *)(event->value);

	shub_info("%s(%u) : %u, %u, %u (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_LIGHT_CCT, sensor_value->lux,
		  sensor_value->cct, sensor_value->raw_lux, event->timestamp, sensor->sampling_period,
		  sensor->max_report_latency);
}

int init_light_cct(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT_CCT);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "light_cct_sensor");
		sensor->receive_event_size = 24;
		sensor->report_event_size = 14;
		sensor->event_buffer.value = kzalloc(sizeof(struct light_cct_event), GFP_KERNEL);
		if (!sensor->event_buffer.value)
			goto err_no_mem;

		sensor->funcs = kzalloc(sizeof(struct sensor_funcs), GFP_KERNEL);
		if (!sensor->funcs)
			goto err_no_mem;

		sensor->funcs->enable = enable_light_cct;
		sensor->funcs->report_event = report_event_light_cct;
		sensor->funcs->print_debug = print_light_cct_debug;
		sensor->funcs->get_sensor_value = get_light_cct_sensor_value;
	} else {
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

	return -ENOMEM;
}
