/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_HW_YUVSC_H
#define PABLO_HW_YUVSC_H

#include "is-param.h"
#include "pablo-internal-subdev-ctrl.h"
#include "pablo-hw-api-common-ctrl.h"

struct pablo_hw_yuvsc {
	struct is_yuvsc_config config[IS_STREAM_COUNT];
	struct yuvsc_param_set param_set[IS_STREAM_COUNT];

	struct pablo_internal_subdev subdev_cloader;
	struct pablo_common_ctrl pcc;

	const struct yuvsc_hw_ops *ops;
};

#endif /* PABLO_HW_YUVSC_H */
