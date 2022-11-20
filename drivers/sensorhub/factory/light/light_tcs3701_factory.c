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

#include <linux/device.h>
#include <linux/slab.h>

#include "../../comm/shub_comm.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../utility/shub_utility.h"

/*************************************************************************/
/* factory Test                                                          */
/*************************************************************************/

#define TCS3701_NAME	"TCS3701"

struct light_debug_info {
	uint32_t stdev;
	uint32_t moving_stdev;
	uint32_t mode;
	uint32_t brightness;
	uint32_t min_div_max;
	uint32_t lux;
} __attribute__((__packed__));

static ssize_t debug_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	char *buffer = NULL;
	int buffer_len = 0;
	struct light_debug_info debug_info;

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_LIGHT, DEBUG_INFO, 1000, NULL, 0, &buffer, &buffer_len,
				     false);

	if (ret < 0) {
		shub_errf("shub_send_command_wait fail %d", ret);
		return ret;
	}

	if (buffer == NULL) {
		shub_errf("buffer is null");
		return -EINVAL;
	}

	if (buffer_len != sizeof(debug_info)) {
		shub_errf("buffer length error %d", buffer_len);
		kfree(buffer);
		return -EINVAL;
	}

	memcpy(&debug_info, buffer, sizeof(debug_info));

	return snprintf(buf, PAGE_SIZE, "%u, %u, %u, %u, %u, %u\n", debug_info.stdev, debug_info.moving_stdev,
			debug_info.mode, debug_info.brightness, debug_info.min_div_max, debug_info.lux);
}

static DEVICE_ATTR_RO(debug_info);

static struct device_attribute *light_tcs3701_attrs[] = {
	&dev_attr_debug_info,
	NULL,
};

struct device_attribute **get_light_tcs3701_dev_attrs(char *name)
{
	if (strcmp(name, TCS3701_NAME) != 0)
		return NULL;

	return light_tcs3701_attrs;
}
