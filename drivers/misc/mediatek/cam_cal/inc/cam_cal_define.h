/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _CAM_CAL_DATA_H
#define _CAM_CAL_DATA_H

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

enum CAM_CAL_COMMAND {
	CAM_CAL_COMMAND_NONE,
	CAM_CAL_COMMAND_EEPROM_LIMIT_SIZE,
	CAM_CAL_COMMAND_CAL_SIZE,
	CAM_CAL_COMMAND_CONVERTED_CAL_SIZE,
	CAM_CAL_COMMAND_AWB_ADDR,
	CAM_CAL_COMMAND_CONVERTED_AWB_ADDR,
	CAM_CAL_COMMAND_LSC_ADDR,
	CAM_CAL_COMMAND_CONVERTED_LSC_ADDR,
	CAM_CAL_COMMAND_MEMTYPE,
	CAM_CAL_COMMAND_BAYERFORMAT,
	CAM_CAL_COMMAND_MODULE_INFO_ADDR,
};

struct CAM_CAL_SENSOR_INFO {
	enum CAM_CAL_COMMAND command;
	unsigned int sensor_id;
	unsigned int device_id;
	unsigned int *info;
};


struct stCAM_CAL_INFO_STRUCT {
	unsigned int u4Offset;
	unsigned int u4Length;
	unsigned int sensorID;
	/*
	 * MAIN = 0x01,
	 * SUB  = 0x02,
	 * MAIN_2 = 0x04,
	 * SUB_2 = 0x08,
	 * MAIN_3 = 0x10,
	 */
	unsigned int deviceID;
	unsigned char *pu1Params;
	enum CAM_CAL_COMMAND command;
};

#ifdef CONFIG_COMPAT

struct COMPAT_CAM_CAL_SENSOR_INFO {
	enum CAM_CAL_COMMAND command;
	unsigned int sensor_id;
	unsigned int device_id;
	compat_uptr_t info;
};

struct COMPAT_stCAM_CAL_INFO_STRUCT {
	unsigned int u4Offset;
	unsigned int u4Length;
	unsigned int sensorID;
	/*
	 * MAIN = 0x01,
	 * SUB  = 0x02,
	 * MAIN_2 = 0x04,
	 * SUB_2 = 0x08,
	 * MAIN_3 = 0x10,
	 */
	unsigned int deviceID;
	compat_uptr_t pu1Params;
	enum CAM_CAL_COMMAND command;
};
#endif

#endif/*_CAM_CAL_DATA_H*/
