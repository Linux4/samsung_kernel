/*
*  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#ifndef __ADSP_FT_COMMON_H__
#define __ADSP_FT_COMMON_H__

#define PID 20000
#define NETLINK_ADSP_FAC 23
/* ENUMS for Selecting the current sensor being used */
enum {
	ADSP_FACTORY_GRIP,
	ADSP_FACTORY_SENSOR_MAX
};

enum {
	NETLINK_ATTR_SENSOR_TYPE,
	NETLINK_ATTR_MAX
};

/* Netlink ENUMS Message Protocols */
enum {
	NETLINK_MESSAGE_GRIP_ON,
	NETLINK_MESSAGE_GRIP_OFF,
	NETLINK_MESSAGE_MAX
};

struct msg_data {
	int sensor_type;
	int param1;
	int param2;
	int param3;
};

struct sensor_on_off_result {
	int result;
};

struct sensor_onoff {
	int onoff;
};
#endif
