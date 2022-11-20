/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_BINARY_H
#define FIMC_IS_BINARY_H

#include "fimc-is-config.h"

#define SDCARD_FW

#define VENDER_PATH

#ifdef VENDER_PATH
#define FIMC_IS_FW_PATH				"/system/vendor/firmware/"
#define FIMC_IS_FW_DUMP_PATH			"/data/"
#define FIMC_IS_SETFILE_SDCARD_PATH		"/data/media/0/"
#define FIMC_IS_FW_SDCARD			"/data/media/0/fimc_is_fw2.bin"
#define FIMC_IS_FW				"fimc_is_fw2.bin"
#define FIMC_IS_ISP_LIB_SDCARD_PATH		"/data/media/0/"
#define FIMC_IS_ISP_LIB				"fimc_is_lib_isp.bin"
#define FIMC_IS_REAR_CAL_SDCARD_PATH	"/data/media/0/"
#define FIMC_IS_FRONT_CAL_SDCARD_PATH	"/data/media/0/"
#define FIMC_IS_REAR_CAL				"rear_cal_data.bin"
#define FIMC_IS_FRONT_CAL				"front_cal_data.bin"
#else
#define FIMC_IS_FW_PATH				"/data/"
#define FIMC_IS_FW_DUMP_PATH			"/data/"
#define FIMC_IS_SETFILE_SDCARD_PATH		"/data/"
#define FIMC_IS_FW_SDCARD			"/data/fimc_is_fw2.bin"
#define FIMC_IS_FW				"fimc_is_fw2.bin"
#define FIMC_IS_ISP_LIB_SDCARD_PATH		"/data/"
#define FIMC_IS_ISP_LIB				"fimc_is_lib_isp.bin"
#define FIMC_IS_REAR_CAL_SDCARD_PATH	"/data/"
#define FIMC_IS_FRONT_CAL_SDCARD_PATH	"/data/"
#define FIMC_IS_REAR_CAL				"rear_cal_data.bin"
#define FIMC_IS_FRONT_CAL				"front_cal_data.bin"
#endif

#define FIMC_IS_FW_BASE_MASK			((1 << 26) - 1)
#define FIMC_IS_VERSION_SIZE			42
#define FIMC_IS_SETFILE_VER_OFFSET		0x40
#define FIMC_IS_SETFILE_VER_SIZE		52

#ifdef ENABLE_IS_CORE
#ifdef SUPPORTED_A5_MEMORY_SIZE_UP
#define FIMC_IS_CAL_START_ADDR		(0x01FD0000)
#else
#define FIMC_IS_CAL_START_ADDR		(0x013D0000)
#endif
#else /* #ifdef ENABLE_IS_CORE */
#define FIMC_IS_CAL_START_ADDR		(FIMC_IS_REAR_CALDATA_OFFSET)
#define FIMC_IS_CAL_START_ADDR_FRONT	(FIMC_IS_FRONT_CALDATA_OFFSET)
#endif

#define FIMC_IS_CAL_RETRY_CNT			(2)
#define FIMC_IS_FW_RETRY_CNT			(2)

#define FIMC_IS_MAX_COMPANION_FW_SIZE		(120 * 1024)

#if defined(CONFIG_USE_HOST_FD_LIBRARY)
#define FD_SW_BIN_NAME				"fimc_is_fd.bin"
#ifdef VENDER_PATH
#define FD_SW_SDCARD_PATH				"/data/media/0/"
#else
#define FD_SW_SDCARD_PATH				"/data/"
#endif
#endif

enum fimc_is_bin_type {
	FIMC_IS_BIN_FW = 0,
	FIMC_IS_BIN_SETFILE,
	FIMC_IS_BIN_LIBRARY,
};

struct fimc_is_binary {
	void *data;
	size_t size;

	const struct firmware *fw;

	unsigned long customized;

	/* request_firmware retry */
	unsigned int retry_cnt;
	int	retry_err;

	/* custom memory allocation */
	void *(*alloc)(unsigned long size);
	void (*free)(const void *buf);
};

void setup_binary_loader(struct fimc_is_binary *bin,
				unsigned int retry_cnt, int retry_err,
				void *(*alloc)(unsigned long size),
				void (*free)(const void *buf));
int request_binary(struct fimc_is_binary *bin, const char *path,
				const char *name, struct device *device);
void release_binary(struct fimc_is_binary *bin);
int was_loaded_by(struct fimc_is_binary *bin);

#endif
