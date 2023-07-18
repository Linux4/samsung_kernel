// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos pablo group manager configurations
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VOTF_ID_TABLE_H
#define IS_VOTF_ID_TABLE_H

#include "is_groupmgr_config.h"
#include "is-subdev-ctrl.h"

const unsigned int is_votf_ip[GROUP_ID_MAX] = {
	[GROUP_ID_3AA0 ... GROUP_ID_MAX-1]	= 0xFFFFFFFF,

	[GROUP_ID_SS0] = 0,
	[GROUP_ID_SS1] = 0,
	[GROUP_ID_SS2] = 0,
	[GROUP_ID_SS3] = 0,
	[GROUP_ID_SS4] = 0,
	[GROUP_ID_SS5] = 0,

	[GROUP_ID_PAF0] = 1,
	[GROUP_ID_PAF1] = 1,
	[GROUP_ID_PAF2] = 1,
};

const unsigned int is_votf_id[GROUP_ID_MAX][ENTRY_END] = {
	[GROUP_ID_3AA0 ... GROUP_ID_MAX-1][ENTRY_SENSOR ... ENTRY_END-1]	= 0xFFFFFFFF,

	[GROUP_ID_SS0][ENTRY_SSVC0] = 8,
	[GROUP_ID_SS0][ENTRY_SSVC1] = 9,
	[GROUP_ID_SS1][ENTRY_SSVC0] = 10,
	[GROUP_ID_SS1][ENTRY_SSVC1] = 11,
	[GROUP_ID_SS2][ENTRY_SSVC0] = 12,
	[GROUP_ID_SS2][ENTRY_SSVC1] = 13,
	[GROUP_ID_SS3][ENTRY_SSVC0] = 14,
	[GROUP_ID_SS3][ENTRY_SSVC1] = 15,

	[GROUP_ID_PAF0][ENTRY_PAF] = 0,
	[GROUP_ID_PAF0][ENTRY_PDAF] = 3,
	[GROUP_ID_PAF1][ENTRY_PAF] = 1,
	[GROUP_ID_PAF1][ENTRY_PDAF] = 4,
	[GROUP_ID_PAF2][ENTRY_PAF] = 2,
	[GROUP_ID_PAF2][ENTRY_PDAF] = 5,
};

#endif
