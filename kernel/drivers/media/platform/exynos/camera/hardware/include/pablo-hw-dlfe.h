// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_HW_DLFE_H
#define PABLO_HW_DLFE_H

#include "is-param.h"
#include "pablo-internal-subdev-ctrl.h"
#include "pablo-hw-api-common-ctrl.h"

struct dlfe_hw_ops;

struct pablo_hw_dlfe {
	struct is_mcfp_config config[IS_STREAM_COUNT];
	struct dlfe_param_set param_set[IS_STREAM_COUNT];

	struct pablo_internal_subdev subdev_cloader;
	struct pablo_common_ctrl pcc;

	const struct dlfe_hw_ops *ops;
};

#endif /* PABLO_HW_DLFE_H */
