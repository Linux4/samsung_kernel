/*
 * Copyright (C) 2017 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef FINGERPRINT_H_
#define FINGERPRINT_H_

#include <linux/clk.h>
#include "fingerprint_sysfs.h"

/* fingerprint debug timer */
#define FPSENSOR_DEBUG_TIMER_SEC (10 * HZ)

enum {
	DETECT_NORMAL = 0,
	DETECT_ADM,			/* Always on Detect Mode */
};

/* For Sensor Type Check */
enum {
	SENSOR_OOO = -2,
	SENSOR_UNKNOWN,
	SENSOR_FAILED,
	SENSOR_VIPER,
	SENSOR_RAPTOR,
	SENSOR_EGIS,
	SENSOR_VIPER_WOG,
	SENSOR_NAMSAN,
	SENSOR_GW32J,
	SENSOR_QBT2000,
	SENSOR_EGISOPTICAL,
	SENSOR_MAXIMUM,
};

#define SENSOR_STATUS_SIZE 11
static char sensor_status[SENSOR_STATUS_SIZE][10] = {"ooo", "unknown", "failed",
	"viper", "raptor", "egis", "viper_wog", "namsan", "gw32j", "qbt2000", "et7xx"};

#endif
