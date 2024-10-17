/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#pragma once

#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/kernel_stat.h>

#define MAXIMUM_PREDEFINED_VALUE_COUNT			(10)

struct hts_private_data {
	int			cpu;
	struct hts_pmu		*pmu;
	atomic_t		mmap_count;
};

struct hts_core_util {
	struct kernel_cpustat	stat;
};

int initialize_hts_devfs(struct platform_device *pdev);
