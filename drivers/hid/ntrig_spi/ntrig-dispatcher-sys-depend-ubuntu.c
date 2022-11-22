/*
 *  HID driver for N-Trig touchscreens
 *
 *  Copyright (c) 2011 N-TRIG
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include "typedef-ntrig.h"
#include "ntrig-common.h"
#include "ntrig-dispatcher.h"
#include "ntrig-dispatcher-sysfs.h"

#include "ntrig-dispatcher-sys-depend.h"

#define FINGERS_HISTORY_LEN 5

void config_multi_touch(struct _ntrig_bus_device *dev,
	struct input_dev *input_device)
{
	int i;

	__set_bit(ABS_X, input_device->absbit);
	__set_bit(ABS_Y, input_device->absbit);
	__set_bit(EV_ABS, input_device->evbit);
	__set_bit(EV_SYN, input_device->evbit);
	__set_bit(EV_KEY, input_device->evbit);

	__set_bit(ABS_MT_POSITION_X, input_device->absbit);
	__set_bit(ABS_MT_POSITION_Y, input_device->absbit);
	__set_bit(ABS_MT_TOUCH_MAJOR, input_device->absbit);
	__set_bit(ABS_MT_WIDTH_MAJOR, input_device->absbit);

	/* Single touch */
	input_set_abs_params(input_device, ABS_X, 0, dev->logical_max_x, 0, 0);
	input_set_abs_params(input_device, ABS_Y, 0, dev->logical_max_y, 0, 0);
	input_set_abs_params(input_device, ABS_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(input_device, ABS_TOOL_WIDTH, 0, 255, 0, 0);

	/**
	 *   [48/0x30] - ABS_MT_TOUCH_MAJOR  0 .. 40
	 *   [50/0x32] - ABS_MT_WIDTH_MAJOR  0 .. 8000
	 *   [53/0x35] - ABS_MT_POSITION_X   0 .. 1023
	 *   [54/0x36] - ABS_MT_POSITION_Y   0.. 599
	 *   ABS_MT_POSITION_Y =
	 */
	ntrig_dbg_lvl(NTRIG_DEBUG_LEVEL_ONCE,
		"inside %s before VIRTUAL_KEYS_SUPPORTED\n", __func__);

	/* Multitouch */
	if (virtual_keys_supported()) {
		ntrig_dbg("inside %s with VIRTUAL_KEYS_SUPPORTED1\n",
			__func__);

		/*__set_bit(EV_KEY, input_device->evbit);*/
		for (i = 0; i < get_virtual_keys_num(); i++)
			__set_bit(get_virt_keys_scan_code(i),
				input_device->keybit);

		input_set_abs_params(input_device, ABS_MT_POSITION_X,
			dev->logical_min_x+get_touch_screen_border_left(),
			dev->logical_max_x-get_touch_screen_border_right(),
			0, 0);
		input_set_abs_params(input_device, ABS_MT_POSITION_Y,
			dev->logical_min_y+get_touch_screen_border_down(),
			dev->logical_max_y-get_touch_screen_border_up(), 0, 0);
		input_set_abs_params(input_device, ABS_MT_TOUCH_MAJOR, 0, 40,
			0, 0);
		input_set_abs_params(input_device, ABS_MT_WIDTH_MAJOR, 0, 8000,
			0, 0);

		ntrig_dbg("inside %s with VIRTUAL_KEYS_SUPPORTED2\n", __func__);
	} else {
		ntrig_dbg("inside %s with VIRTUAL_KEYS_SUPPORTED undefined\n",
			__func__);
		input_set_abs_params(input_device, ABS_MT_POSITION_X,
			dev->logical_min_x, dev->logical_max_x, 0, 0);
		input_set_abs_params(input_device, ABS_MT_POSITION_Y,
			dev->logical_min_y, dev->logical_max_y, 0, 0);
		input_set_abs_params(input_device, ABS_MT_TOUCH_MAJOR, 0, 40,
			0, 0);
		input_set_abs_params(input_device, ABS_MT_WIDTH_MAJOR, 0, 8000,
			0, 0);
	}

	ntrig_dbg("inside %s after VIRTUAL_KEYS_SUPPORTED\n", __func__);
}

bool generate_single_touch_message(struct mr_message_types_s *single_touch_msg,
	int *isSingleTouch, uint16_t x, uint16_t y)
{
	static int lastX, lastY;
		/* statics are automatically initialized to 0 by the compiler */
	bool retVal = false;

	if (single_touch_msg == NULL)
		return false;

	switch (*isSingleTouch) {
	case 1:
		single_touch_msg->msg.pen_event.pressure = 0;
		single_touch_msg->msg.pen_event.btn_code = 0;
		single_touch_msg->msg.pen_event.btn_removed = 330;
		single_touch_msg->msg.pen_event.x_coord = lastX;
		single_touch_msg->msg.pen_event.y_coord = lastY;
		*isSingleTouch = 0;
		retVal = true;
		break;
	case 2:
		single_touch_msg->msg.pen_event.pressure = 50;
		single_touch_msg->msg.pen_event.btn_code = 330;
		single_touch_msg->msg.pen_event.btn_removed = 0;
		lastX = single_touch_msg->msg.pen_event.x_coord = x;
		lastY = single_touch_msg->msg.pen_event.y_coord = y;
		retVal = true;
		break;
	default:
		break;
	}
	return retVal;
}

void ntrig_simulate_single_touch(struct input_dev *input,
	struct device_finger_s *finger)
{
	input_report_abs(input, ABS_X, finger->x_coord);
	input_report_abs(input, ABS_Y, finger->y_coord);
}
