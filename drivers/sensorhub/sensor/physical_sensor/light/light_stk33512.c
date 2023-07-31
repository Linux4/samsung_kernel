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

#include "../../../sensor/light.h"
#include "../../../sensormanager/shub_sensor.h"
#include "../../../sensormanager/shub_sensor_manager.h"
#include "../../../comm/shub_comm.h"
#include "../../../sensorhub/shub_device.h"
#include "../../../utility/shub_utility.h"
#include "../../../utility/shub_file_manager.h"
#include "../../../others/shub_panel.h"

#define STK33512_NAME   "STK33512"
#define STK33512_VENDOR "Sitronix"

int init_light_stk33512(void)
{
	struct light_data *data = get_sensor(SENSOR_TYPE_LIGHT)->data;

	data->use_cal_data = true;

	return 0;
}

struct sensor_chipset_init_funcs light_stk33512_ops = {
	.init = init_light_stk33512,
};

struct sensor_chipset_init_funcs *get_light_stk33512_function_pointer(char *name)
{
	if (strcmp(name, STK33512_NAME) != 0)
		return NULL;

	return &light_stk33512_ops;
}
