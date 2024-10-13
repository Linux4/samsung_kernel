/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

struct CAM_CAL_OIS_GYRO_CAL {
	char efsData[30];
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
