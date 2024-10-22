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
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>

#include "../../../sensor/proximity.h"
#include "../../../sensormanager/shub_sensor.h"
#include "../../../sensormanager/shub_sensor_manager.h"
#include "../../../comm/shub_comm.h"
#include "../../../sensorhub/shub_device.h"
#include "../../../utility/shub_utility.h"
#include "../../../utility/shub_file_manager.h"
#include "../../../others/shub_panel.h"

#define TMD4914_NAME "TMD4914"

int init_proximity_tmd4914(void)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	data->cal_data_len = sizeof(u16);
	data->setting_mode = 2;

	return 0;
}

void parse_dt_proximity_tmd4914(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (of_property_read_u16_array(np, "prox-tmd4914-thresh", data->prox_threshold, PROX_THRESH_SIZE))
		shub_err("no prox-tmd4914-thresh, set as 0");

	shub_info("thresh %u, %u", data->prox_threshold[PROX_THRESH_HIGH], data->prox_threshold[PROX_THRESH_LOW]);
}

int proximity_open_calibration_tmd4914(void)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	open_default_proximity_setting_mode();
	open_default_proximity_calibration();
	shub_infof(" mode:%d base:%d", data->setting_mode, *((u16 *)data->cal_data));

	return 0;
}

void set_proximity_state_tmd4914(struct proximity_data *data)
{
	if (is_panel_ubid_changed())
		return;
	set_proximity_setting_mode();
	set_proximity_calibration();
}

struct proximity_chipset_funcs prox_tmd4914_funcs = {
	.open_calibration_file = proximity_open_calibration_tmd4914,
	.sync_proximity_state = set_proximity_state_tmd4914,
};

void *get_proximity_tmd4914_chipset_funcs(void)
{
	return &prox_tmd4914_funcs;
}

struct sensor_chipset_init_funcs prox_tmd4914_ops = {
	.init = init_proximity_tmd4914,
	.parse_dt = parse_dt_proximity_tmd4914,
	.get_chipset_funcs = get_proximity_tmd4914_chipset_funcs,
};

struct sensor_chipset_init_funcs *get_proximity_tmd4914_function_pointer(char *name)
{
	if (strcmp(name, TMD4914_NAME) != 0)
		return NULL;

	return &prox_tmd4914_ops;
}
