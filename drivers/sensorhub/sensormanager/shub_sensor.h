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

#ifndef __SHUB_SENSOR_H_
#define __SHUB_SENSOR_H_

#include "shub_sensor_type.h"
#include "shub_vendor_type.h"

#include <linux/rtc.h>

#define SENSOR_NAME_MAX 40

struct sensor_event {
	u64 received_timestamp;
	u64 timestamp;
	u8 *value;
};

struct meta_event {
	s32 what;
	s32 sensor;
} __attribute__((__packed__));

struct sensor_spec_t {
	uint8_t uid;
	uint8_t name[15];
	uint8_t vendor;
	uint32_t version;
	uint8_t is_wake_up;
	int32_t min_delay;
	uint32_t max_delay;
	uint16_t max_fifo;
	uint16_t reserved_fifo;
	float resolution;
	float max_range;
	float power;
} __attribute__((__packed__));

struct sensor_funcs {
	int (*sync_status)(void); /* this is called when sensorhub ready to work or reset */
	int (*enable)(void);
	int (*disable)(void);
	int (*batch)(int, int);
	void (*report_event)(void);
	int (*inject_additional_data)(char *, int);
	void (*print_debug)(void);
	int (*parsing_data)(char *, int *, int);
	int (*set_position)(int);
	int (*get_position)(void);
	int (*init_chipset)(void);
	int (*open_calibration_file)(void);
	/* if receive_event_size is 0, you can check parsing error in this func */
	int (*get_sensor_value)(char *, int *, struct sensor_event *, int);
};

struct shub_sensor {
	struct sensor_spec_t spec;
	int type;
	char name[SENSOR_NAME_MAX];
	bool report_mode_continuous;

	bool enabled;
	int enabled_cnt;
	struct mutex enabled_mutex;

	uint32_t sampling_period;
	uint32_t max_report_latency;

	int receive_event_size;
	int report_event_size;

	u64 enable_timestamp;
	u64 disable_timestamp;
	struct rtc_time enable_time;
	struct rtc_time disable_time;

	struct sensor_event event_buffer;

	void *data;
	struct sensor_funcs *funcs;
};

typedef int (*init_sensor)(bool en);

#endif /* __SHUB_SENSOR_H_ */