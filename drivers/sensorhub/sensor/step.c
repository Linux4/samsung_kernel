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

#include <linux/slab.h>

#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_utility.h"

struct step_counter_event {
	u32 step_diff;
	u64 step_total;
} __attribute__((__packed__));

static void report_event_step_counter(void)
{
	struct step_counter_event *sensor_value =
	    (struct step_counter_event *)(get_sensor_event(SENSOR_TYPE_STEP_COUNTER)->value);

	sensor_value->step_total += sensor_value->step_diff;
}

void print_step_counter_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_STEP_COUNTER);
	struct sensor_event *event = &(sensor->event_buffer);
	struct step_counter_event *sensor_value = (struct step_counter_event *)(event->value);

	shub_info("%s(%u) : %u (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_STEP_COUNTER,
		  sensor_value->step_diff, event->timestamp,
		  sensor->sampling_period, sensor->max_report_latency);
}

int init_step_counter(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_STEP_COUNTER);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "step_cnt_sensor");
		sensor->receive_event_size = 4;
		sensor->report_event_size = 12;
		sensor->event_buffer.value = kzalloc(sizeof(struct step_counter_event), GFP_KERNEL);
		if (!sensor->event_buffer.value)
			goto err_no_mem;

		sensor->funcs = kzalloc(sizeof(struct sensor_funcs), GFP_KERNEL);
		if (!sensor->funcs)
			goto err_no_mem;

		sensor->funcs->report_event = report_event_step_counter;
		sensor->funcs->print_debug = print_step_counter_debug;
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;

		kfree(sensor->funcs);
		sensor->funcs = NULL;
	}
	return 0;

err_no_mem:
	kfree(sensor->event_buffer.value);
	sensor->event_buffer.value = NULL;

	kfree(sensor->funcs);
	sensor->funcs = NULL;

	return -ENOMEM;
}

int init_step_detector(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_STEP_DETECTOR);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "step_det_sensor");
		sensor->receive_event_size = 1;
		sensor->report_event_size = 1;
		sensor->event_buffer.value = kzalloc(sensor->receive_event_size, GFP_KERNEL);
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
