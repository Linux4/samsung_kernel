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

#ifndef IS_GROUP_MGR_CONFIG_H
#define IS_GROUP_MGR_CONFIG_H

/* #define DEBUG_AA */
/* #define DEBUG_FLASH */

#define GROUP_STREAM_INVALID	0xFFFFFFFF

#define GROUP_ID_3AA_MAX       4

#define GROUP_ID_SS0            0
#define GROUP_ID_SS1            1
#define GROUP_ID_SS2            2
#define GROUP_ID_SS3            3
#define GROUP_ID_SS4            4
#define GROUP_ID_SS5            5
#define GROUP_ID_SS6            6
#define GROUP_ID_PAF0           7
#define GROUP_ID_PAF1           8
#define GROUP_ID_PAF2           9
#define GROUP_ID_PAF3           10
#define GROUP_ID_3AA0           11
#define GROUP_ID_3AA1           12
#define GROUP_ID_3AA2           13
#define GROUP_ID_3AA3           14
#define GROUP_ID_BYRP           15
#define GROUP_ID_BYRP0          GROUP_ID_BYRP
#define GROUP_ID_BYRP1          16
#define GROUP_ID_BYRP2          17
#define GROUP_ID_BYRP3          18
#define GROUP_ID_BYRP4          19
#define GROUP_ID_RGBP           20
#define GROUP_ID_RGBP0          GROUP_ID_RGBP
#define GROUP_ID_RGBP1          21
#define GROUP_ID_RGBP2          22
#define GROUP_ID_RGBP3          23
#define GROUP_ID_YUVSC0         24
#define GROUP_ID_YUVSC1         25
#define GROUP_ID_YUVSC2         26
#define GROUP_ID_YUVSC3         27
#define GROUP_ID_MLSC0          28
#define GROUP_ID_MLSC1          29
#define GROUP_ID_MLSC2          30
#define GROUP_ID_MLSC3          31
#define GROUP_ID_MTNR0          32
#define GROUP_ID_MTNR1          33
#define GROUP_ID_MSNR           34
#define GROUP_ID_YUVP           35
#define GROUP_ID_MCS0           36
#define GROUP_ID_MAX            37
#define GROUP_ID(id)            (1UL << (id))

#define GROUP_SLOT_SENSOR 0
#define GROUP_SLOT_PAF 1
#define GROUP_SLOT_BYRP 2
#define GROUP_SLOT_RGBP 3
#define GROUP_SLOT_YUVSC 4
#define GROUP_SLOT_MLSC 5
#define GROUP_SLOT_MTNR 6
#define GROUP_SLOT_MSNR 7
#define GROUP_SLOT_YUVP 8
#define GROUP_SLOT_MCS 9
#define GROUP_SLOT_MAX 10

/* TODO: Change name to remove the HW dependency */
#define GROUP_SLOT_3AA GROUP_SLOT_BYRP

#define IS_SENSOR_GROUP(id)	\
	((id) >= GROUP_ID_SS0 && (id) <= GROUP_ID_SS6)

static const u32 slot_to_gid[GROUP_SLOT_MAX] = {
	[GROUP_SLOT_SENSOR] = GROUP_ID_SS0,
	[GROUP_SLOT_PAF] = GROUP_ID_PAF0,
	[GROUP_SLOT_3AA] = GROUP_ID_3AA0,
	[GROUP_SLOT_BYRP] = GROUP_ID_BYRP0,
	[GROUP_SLOT_RGBP] = GROUP_ID_RGBP0,
	[GROUP_SLOT_YUVSC] = GROUP_ID_YUVSC0,
	[GROUP_SLOT_MLSC] = GROUP_ID_MLSC0,
	[GROUP_SLOT_MTNR] = GROUP_ID_MTNR0,
	[GROUP_SLOT_MSNR] = GROUP_ID_MSNR,
	[GROUP_SLOT_YUVP] = GROUP_ID_YUVP,
	[GROUP_SLOT_MCS] = GROUP_ID_MCS0,
};

static const char * const group_id_name[GROUP_ID_MAX + 1] = {
	[GROUP_ID_SS0] = "G:SS0",
	[GROUP_ID_SS1] = "G:SS1",
	[GROUP_ID_SS2] = "G:SS2",
	[GROUP_ID_SS3] = "G:SS3",
	[GROUP_ID_SS4] = "G:SS4",
	[GROUP_ID_SS5] = "G:SS5",
	[GROUP_ID_SS6] = "G:SS6",
	[GROUP_ID_PAF0] = "G:PDP0",
	[GROUP_ID_PAF1] = "G:PDP1",
	[GROUP_ID_PAF2] = "G:PDP2",
	[GROUP_ID_PAF3] = "G:PDP3",
	[GROUP_ID_3AA0] = "G:CSTAT0",
	[GROUP_ID_3AA1] = "G:CSTAT1",
	[GROUP_ID_3AA2] = "G:CSTAT2",
	[GROUP_ID_3AA3] = "G:CSTAT3",
	[GROUP_ID_BYRP0] = "G:BYRP0",
	[GROUP_ID_BYRP1] = "G:BYRP1",
	[GROUP_ID_BYRP2] = "G:BYRP2",
	[GROUP_ID_BYRP3] = "G:BYRP3",
	[GROUP_ID_BYRP4] = "G:BYRP4",
	[GROUP_ID_RGBP0] = "G:RGBP0",
	[GROUP_ID_RGBP1] = "G:RGBP1",
	[GROUP_ID_RGBP2] = "G:RGBP2",
	[GROUP_ID_RGBP3] = "G:RGBP3",
	[GROUP_ID_YUVSC0] = "G:YUVSC0",
	[GROUP_ID_YUVSC1] = "G:YUVSC1",
	[GROUP_ID_YUVSC2] = "G:YUVSC2",
	[GROUP_ID_YUVSC3] = "G:YUVSC3",
	[GROUP_ID_MLSC0] = "G:MLSC0",
	[GROUP_ID_MLSC1] = "G:MLSC1",
	[GROUP_ID_MLSC2] = "G:MLSC2",
	[GROUP_ID_MLSC3] = "G:MLSC3",
	[GROUP_ID_MTNR0] = "G:MTNR0",
	[GROUP_ID_MTNR1] = "G:MTNR1",
	[GROUP_ID_MSNR] = "G:MSNR",
	[GROUP_ID_YUVP] = "G:YUVP",
	[GROUP_ID_MCS0] = "G:MCS",
	[GROUP_ID_MAX] = "G:MAX"
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
