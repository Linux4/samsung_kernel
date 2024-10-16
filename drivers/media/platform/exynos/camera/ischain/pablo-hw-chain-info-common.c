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

#include "pablo-hw-chain-info.h"

void pablo_hw_cmn_get_ctrl(struct is_hardware *hw, int hw_id, enum hw_get_ctrl_id id, void *val)
{
	switch (id) {
	case HW_G_CTRL_HAS_VRA_CH1_ONLY:
		*(bool *)val = true;
		break;
	default:
		break;
	}
}
