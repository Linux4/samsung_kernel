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

#include <linux/slab.h>

int init_super(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_SENSORHUB);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "sensorhub_sensor");
		sensor->receive_event_size = 0;
		sensor->report_event_size = 3;
		sensor->event_buffer.value = kzalloc(sensor->report_event_size, GFP_KERNEL);
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

int get_hub_debugger_value(char *dataframe, int *index, struct sensor_event *event, int frame_len)
{
	u16 length = 0;
	u8 *buf;

	memcpy(&length, dataframe + *index, 2);
	if (*index + length > frame_len)
		return -EINVAL;

	*index += 2;

	buf = kzalloc(length, GFP_KERNEL);
	memcpy(buf, dataframe + *index, length);

	shub_info("[M] %s", buf);
	*index += length;

	kfree(buf);

	return 0;
}

int init_hub_debugger(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_HUB_DEBUGGER);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "hub_debugger");
		sensor->receive_event_size = 0;
		sensor->report_event_size = 0;

		sensor->funcs = kzalloc(sizeof(struct sensor_funcs), GFP_KERNEL);
		if (!sensor->funcs)
			goto err_no_mem;

		sensor->funcs->get_sensor_value = get_hub_debugger_value;
	} else {
		kfree(sensor->funcs);
		sensor->funcs = NULL;
	}
	return 0;

err_no_mem:
	kfree(sensor->funcs);
	sensor->funcs = NULL;

	return -ENOMEM;
}
