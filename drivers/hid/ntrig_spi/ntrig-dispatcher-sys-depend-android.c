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

#ifdef ROTATE_ANDROID
static void exchange(__u16 *v1, __u16 *v2)
{
	__u16 tmp = *v1;
	*v1 = *v2;
	*v2 = tmp;
}
#endif /* ROTATE_ANDROID */

void config_multi_touch(struct _ntrig_bus_device *dev,
	struct input_dev *input_device)
{
	int i;
	__u16 min_x = dev->logical_min_x;
	__u16 max_x = dev->logical_max_x;
	__u16 min_y = dev->logical_min_y;
	__u16 max_y = dev->logical_max_y;
#ifdef ROTATE_ANDROID
	__u16 tmp;
#endif

	__set_bit(EV_ABS, input_device->evbit);

	__set_bit(ABS_MT_POSITION_X, input_device->absbit);
	__set_bit(ABS_MT_POSITION_Y, input_device->absbit);
	__set_bit(ABS_MT_WIDTH_MAJOR, input_device->absbit);
#ifdef MT_REPORT_TYPE_B
	__set_bit(ABS_MT_WIDTH_MINOR, input_device->absbit);
	__set_bit(ABS_MT_PRESSURE, input_device->absbit);
	__set_bit(ABS_MT_TOOL_TYPE, input_device->absbit);
#else
	__set_bit(ABS_MT_TOUCH_MAJOR, input_device->absbit);
#endif
	/**
	 *   [48/0x30] - ABS_MT_TOUCH_MAJOR  0 .. 40
	 *   [50/0x32] - ABS_MT_WIDTH_MAJOR  0 .. 8000
	 *   [53/0x35] - ABS_MT_POSITION_X   0 .. 1023
	 *   [54/0x36] - ABS_MT_POSITION_Y   0.. 599
	 *   ABS_MT_POSITION_Y =
	 */
	ntrig_dbg_lvl(NTRIG_DEBUG_LEVEL_ONCE,
		"inside %s before VIRTUAL_KEYS_SUPPORTED\n", __func__);

	if (virtual_keys_supported()) {
		ntrig_dbg("inside %s with VIRTUAL_KEYS_SUPPORTED1\n", __func__);

		__set_bit(EV_KEY, input_device->evbit);
		for (i = 0; i < get_virtual_keys_num(); i++)
			__set_bit(get_virt_keys_scan_code(i),
				input_device->keybit);

		min_x += get_touch_screen_border_left();
		max_x -= get_touch_screen_border_right();
		min_y += get_touch_screen_border_down();
		max_y -= get_touch_screen_border_up();
		ntrig_dbg("inside %s with VIRTUAL_KEYS_SUPPORTED2\n", __func__);
	} else {
		ntrig_dbg("inside %s with VIRTUAL_KEYS_SUPPORTED undefined\n",
			__func__);
	}

#ifdef ROTATE_ANDROID
	/* Exchange the x/y boundaries to make the rotation code simpler */
	exchange(&min_x, &min_y);
	exchange(&max_x, &max_y);
#endif
	input_set_abs_params(input_device, ABS_MT_POSITION_X,
		min_x, max_x, 0, 0);
	input_set_abs_params(input_device, ABS_MT_POSITION_Y,
		min_y, max_y, 0, 0);
	input_set_abs_params(input_device, ABS_MT_WIDTH_MAJOR, 0,
		ABS_MT_WIDTH_MAX, 0, 0);
	input_set_abs_params(input_device, ABS_MT_WIDTH_MINOR, 0,
		ABS_MT_WIDTH_MAX, 0, 0);
#ifdef MT_REPORT_TYPE_B
	input_set_abs_params(input_device, ABS_MT_PRESSURE, 1, 1023, 0, 0); //255
	input_set_abs_params(input_device, ABS_MT_TOOL_TYPE, 0, 1, 0, 0);
#else
	input_set_abs_params(input_device, ABS_MT_TOUCH_MAJOR, 0,
		ABS_MT_TOUCH_MAJOR_MAX, 0, 0);
#endif
	/** ABS_MT_ORIENTATION: we use 0 or 1 values, we only detect
	 *  90 degree rotation (by looking at the maximum of dx and
	 *  dy reported by sensor */
	input_set_abs_params(input_device, ABS_MT_ORIENTATION, 0, 1, 0, 0);
	input_set_abs_params(input_device, ABS_MT_TRACKING_ID, 0,
		ABS_MT_TRACKING_ID_MAX, 0, 0);

	ntrig_dbg("inside %s after VIRTUAL_KEYS_SUPPORTED\n", __func__);
}

void ntrig_simulate_single_touch(struct input_dev *input,
	struct device_finger_s *finger)
{
}


