// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_LIB_H
#define PABLO_LIB_H

#include <linux/of_platform.h>

#include "pablo-debug.h"
#include "pablo-internal-subdev-ctrl.h"

struct pablo_lib_data {
	int (*probe)(struct platform_device *pdev,
			struct pablo_lib_data *data);
	int (*remove)(struct platform_device *pdev,
			struct pablo_lib_data *data);
};

enum pablo_lib_cluster_id {
	PABLO_CPU_CLUSTER_LIT = 0,
	PABLO_CPU_CLUSTER_MID,
	PABLO_CPU_CLUSTER_BIG,
	PABLO_CPU_CLUSTER_MAX,
};

enum pablo_lib_multi_target_range {
	PABLO_MTARGET_RSTART = 0,
	PABLO_MTARGET_REND,
	PABLO_MTARGET_RMAX,
};

enum pablo_lib_i_subdev_votf {
	PABLO_LIB_I_SUBDEV_VOTF_HF = 0,
	PABLO_LIB_I_SUBDEV_VOTF_MAX,
};

struct pablo_lib_platform_data {
	u32 cpu_cluster[PABLO_CPU_CLUSTER_MAX];
	u32 memlog_size[PABLO_MEMLOG_MAX];
	u32 mtarget_range[PABLO_MTARGET_RMAX];

	struct pablo_internal_subdev sd_votf[PABLO_LIB_I_SUBDEV_VOTF_MAX];
};

struct pablo_lib_platform_data *pablo_lib_get_platform_data(void);
char *pablo_lib_get_stream_prefix(int instance);
void pablo_lib_set_stream_prefix(int instance, const char *fmt, ...);

#endif
