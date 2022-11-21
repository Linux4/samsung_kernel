/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_BINARY_H__
#define __VERTEX_BINARY_H__

#include <linux/device.h>

#define VERTEX_DEBUG_BIN_PATH	"/data"

#define VERTEX_FW_DRAM_NAME	"CC_DRAM_CODE_FLASH_HIFI.bin"
#define VERTEX_FW_ITCM_NAME	"CC_ITCM_CODE_FLASH_HIFI.bin"
#define VERTEX_FW_DTCM_NAME	"CC_DTCM_CODE_FLASH_HIFI.bin"

#define VERTEX_FW_NAME_LEN	(100)
#define VERTEX_VERSION_SIZE	(42)

struct vertex_system;

struct vertex_binary {
	struct device		*dev;
};

int vertex_binary_read(struct vertex_binary *bin, const char *path,
		const char *name, void *target, size_t size);
int vertex_binary_write(struct vertex_binary *bin, const char *path,
		const char *name, void *target, size_t size);

int vertex_binary_init(struct vertex_system *sys);
void vertex_binary_deinit(struct vertex_binary *bin);

#endif
