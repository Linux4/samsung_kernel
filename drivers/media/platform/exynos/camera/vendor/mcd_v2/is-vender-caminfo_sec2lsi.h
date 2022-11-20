/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VENDER_CAMINFO_SEC2LSI_H
#define IS_VENDER_CAMINFO_SEC2LSI_H

#include <linux/vmalloc.h>
#include "is-core.h"

#ifndef _LINUX_TYPES_H
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#endif

static bool sec2lsi_conversion_done[8] = {false, false, false, false,
										false, false, false, false};

#define IS_CAMINFO_IOCTL_MAGIC 			0xFB
#define CAM_MAX_SUPPORTED_LIST			20

#define IS_CAMINFO_IOCTL_COMMAND		_IOWR(IS_CAMINFO_IOCTL_MAGIC, 0x01, caminfo_sec2lsi_ioctl_cmd *)

#define SEC2LSI_AWB_DATA_SIZE			8
#define SEC2LSI_LSC_DATA_SIZE			6632
#define SEC2LSI_MODULE_INFO_SIZE		11
#define SEC2LSI_CHECKSUM_SIZE			4

typedef struct
{
	uint32_t cmd;
	void *data;
} caminfo_sec2lsi_ioctl_cmd;

enum ioctl_command
{
	CAMINFO_IOCTL_GET_SEC2LSI_BUFF = 0,
	CAMINFO_IOCTL_SET_SEC2LSI_BUFF = 1,
	CAMINFO_CMD_ID_GET_FACTORY_SUPPORTED_ID = 2,
	CAMINFO_IOCTL_GET_MODULE_INFO = 3,
};

typedef struct
{
	uint32_t camID;
	unsigned char *secBuf;
	unsigned char *lsiBuf;
	unsigned char *mdInfo; //Module information data in the calmap Header area
	uint32_t awb_size;
	uint32_t lsc_size;
} caminfo_romdata_sec2lsi;

typedef struct
{
	uint32_t size;
	uint32_t data[CAM_MAX_SUPPORTED_LIST];
} caminfo_supported_list_sec2lsi;

typedef struct
{
	struct mutex	mlock;
} is_vender_caminfo_sec2lsi;

#endif /* IS_VENDER_CAMINFO_SEC2LSI_H */