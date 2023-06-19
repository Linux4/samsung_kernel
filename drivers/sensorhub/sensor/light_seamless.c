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
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_utility.h"
#include "light_seamless.h"

#include <linux/slab.h>

static int inject_light_seamless_additional_data(char *buf, int count)
{
	char *token;
    
    int ret = 0;
	int index = 0;
	int thd[2] = {200, 1000000};

	if (count < 4) {
		shub_errf("length error %d", count);
		return -EINVAL;
	}
    token = strsep(&buf, ",");
    while (token != NULL && index < 2) {
        ret = kstrtoint(token, 10, &thd[index++]);
        if (ret < 0) {
            shub_errf("%s - kstrtoint failed.(%d)\n", __func__, ret);
            return ret;
        }
        token = strsep(&buf, ",");
    }

    shub_info("%s - thd[0] = %d thd[1] = %d", __func__, thd[0], thd[1]);

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_LIGHT_SEAMLESS, LIGHT_SUBCMD_THRESHOLD, (char *)&thd, sizeof(thd));

	return ret;
}

int init_light_seamless(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LIGHT_SEAMLESS);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "light_seamless_sensor");
		sensor->receive_event_size = 4;
		sensor->report_event_size = 4;
		sensor->event_buffer.value = kzalloc(sizeof(struct light_seamless_event), GFP_KERNEL);
		if (!sensor->event_buffer.value)
			goto err_no_mem;

		sensor->funcs = kzalloc(sizeof(struct sensor_funcs), GFP_KERNEL);
		if (!sensor->funcs)
			goto err_no_mem;

        sensor->funcs->inject_additional_data = inject_light_seamless_additional_data;
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
