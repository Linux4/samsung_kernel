/*
 *  Copyright (C) 2019, Samsung Electronics Co. Ltd. All Rights Reserved.
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
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/mm.h>

#include "ssp.h"

enum fw_type {
	FW_TYPE_NONE,
	FW_TYPE_BIN,
	FW_TYPE_PUSH,
	FW_TYPE_SPU,
};

#define UPDATE_BIN_PATH	   "/vendor/firmware/shub.bin"
#define UPDATE_BIN_FW_NAME "shub.bin"
#define SPU_FW_FILE "/spu/sensorhub/shub_spu.bin"

#define FW_VER_LEN 8

#define FW_VERSION 22093000
unsigned int get_module_rev(struct ssp_data *data)
{
	return FW_VERSION;
}
