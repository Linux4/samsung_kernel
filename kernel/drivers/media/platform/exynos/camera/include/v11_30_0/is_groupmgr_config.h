// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_GROUP_MGR_CONFIG_H
#define IS_GROUP_MGR_CONFIG_H

/* #define DEBUG_AA */
/* #define DEBUG_FLASH */

#define GROUP_STREAM_INVALID	0xFFFFFFFF

#define GROUP_ID_3AA0           0
#define GROUP_ID_3AA1           1
#define GROUP_ID_3AA_MAX        2
#define GROUP_ID_BYRP           2
#define GROUP_ID_RGBP           3
#define GROUP_ID_MCS0           4
#define GROUP_ID_PAF0           5
#define GROUP_ID_PAF1           6
#define GROUP_ID_MCFP           7
#define GROUP_ID_YUVP           8
#define GROUP_ID_LME0           9
#define GROUP_ID_SS0            10
#define GROUP_ID_SS1            11
#define GROUP_ID_SS2            12
#define GROUP_ID_SS3            13
#define GROUP_ID_SS4            14
#define GROUP_ID_SS5            15
#define GROUP_ID_SS6            16
#define GROUP_ID_SS7            17
#define GROUP_ID_SS8            18
#define GROUP_ID_MAX            19
#define GROUP_ID(id)            (1UL << (id))

#define GROUP_SLOT_SENSOR       0
#define GROUP_SLOT_PAF          1
#define GROUP_SLOT_3AA          2
#define GROUP_SLOT_LME          3
#define GROUP_SLOT_BYRP         4
#define GROUP_SLOT_RGBP         5
#define GROUP_SLOT_MCFP         6
#define GROUP_SLOT_YUVP         7
#define GROUP_SLOT_MCS          8
#define GROUP_SLOT_MAX          9

#define IS_SENSOR_GROUP(id)	\
	((id) >= GROUP_ID_SS0 && (id) <= GROUP_ID_SS8)

static const u32 slot_to_gid[GROUP_SLOT_MAX] = {
	[GROUP_SLOT_SENSOR] = GROUP_ID_SS0,
	[GROUP_SLOT_PAF] = GROUP_ID_PAF0,
	[GROUP_SLOT_3AA] = GROUP_ID_3AA0,
	[GROUP_SLOT_LME] = GROUP_ID_LME0,
	[GROUP_SLOT_BYRP] = GROUP_ID_BYRP,
	[GROUP_SLOT_RGBP] = GROUP_ID_RGBP,
	[GROUP_SLOT_MCFP] = GROUP_ID_MCFP,
	[GROUP_SLOT_YUVP] = GROUP_ID_YUVP,
	[GROUP_SLOT_MCS] = GROUP_ID_MCS0,
};

static const char * const group_id_name[GROUP_ID_MAX + 1] = {
	[GROUP_ID_3AA0] = "G:CSTAT0",
	[GROUP_ID_3AA1] = "G:CSTAT1",
	[GROUP_ID_BYRP] = "G:BYRP",
	[GROUP_ID_RGBP] = "G:RGBP",
	[GROUP_ID_MCS0] = "G:MCS",
	[GROUP_ID_PAF0] = "G:PDP0",
	[GROUP_ID_PAF1] = "G:PDP1",
	[GROUP_ID_MCFP] = "G:MCFP",
	[GROUP_ID_YUVP] = "G:YUVP",
	[GROUP_ID_LME0] = "G:LME",
	[GROUP_ID_SS0 ] = "G:SS0",
	[GROUP_ID_SS1 ] = "G:SS1",
	[GROUP_ID_SS2 ] = "G:SS2",
	[GROUP_ID_SS3 ] = "G:SS3",
	[GROUP_ID_SS4 ] = "G:SS4",
	[GROUP_ID_SS5 ] = "G:SS5",
	[GROUP_ID_SS6 ] = "G:SS6",
	[GROUP_ID_SS6 ] = "G:SS7",
	[GROUP_ID_SS8 ] = "G:SS8",
	[GROUP_ID_MAX ] = "G:MAX"
};

/*
 * <LINE_FOR_SHOT_VALID_TIME>
 * If valid time is too short when image height is small, use this feature.
 * If height is smaller than this value, async_shot is increased.
 */
#define LINE_FOR_SHOT_VALID_TIME	500

#define IS_MAX_GFRAME	(VIDEO_MAX_FRAME) /* max shot buffer of F/W : 32 */
#define MIN_OF_ASYNC_SHOTS	1
#define MIN_OF_SYNC_SHOTS	2

#define MIN_OF_SHOT_RSC		(1)
#define MIN_OF_ASYNC_SHOTS_240FPS	(MIN_OF_ASYNC_SHOTS + 0)

#endif
