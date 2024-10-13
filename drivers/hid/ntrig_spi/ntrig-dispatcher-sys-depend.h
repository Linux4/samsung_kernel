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


#ifndef _NTRIG_DISPATCHER_CONFIG_H
#define _NTRIG_DISPATCHER_CONFIG_H

#include "ntrig-dispatcher.h"

void config_multi_touch(struct _ntrig_bus_device *dev,
	struct input_dev *input_device);
void ntrig_simulate_single_touch(struct input_dev *input,
	struct device_finger_s *finger);

#endif /* _NTRIG_DISPATCHER_CONFIG_H */
