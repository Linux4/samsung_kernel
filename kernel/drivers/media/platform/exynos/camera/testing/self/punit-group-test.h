// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PUNIT_GROUP_TEST_H
#define PUNIT_GROUP_TEST_H

#include <linux/kthread.h>

#include "is_groupmgr_config.h"
#include "is-video.h"

#define PGT_DQ_WAIT_TIME 3000
#define PGT_PASS true
#define PGT_FAIL false
#define PGT_UNCHECKED -1

enum punit_group_test_act {
	PUNIT_GROUP_TEST_ACT_STOP,
	PUNIT_GROUP_TEST_ACT_START,
	PUNIT_GROUP_TEST_ACT_STATIC_INFO,
	PUNIT_GROUP_TEST_ACT_OPEN,
	PUNIT_GROUP_TEST_ACT_CLOSE,
	PUNIT_GROUP_TEST_ACT_CRITERIA_INFO,
	PUNIT_GROUP_TEST_ACT_SET_PARAM,
	PUNIT_GROUP_TEST_ACT_MAX,
};

static const char *group_act[PUNIT_GROUP_TEST_ACT_MAX] = {
	"PUNIT_GROUP_TEST_ACT_STOP",
	"PUNIT_GROUP_TEST_ACT_START",
	"PUNIT_GROUP_TEST_ACT_STATIC_INFO",
	"PUNIT_GROUP_TEST_ACT_OPEN",
	"PUNIT_GROUP_TEST_ACT_CLOSE",
	"PUNIT_GROUP_TEST_ACT_CRITERIA_INFO",
	"PUNIT_GROUP_TEST_ACT_SET_PARAM",
};

struct punit_group_test_map {
	const u32 idx;
	const char *key;
	const u32 count;
};

struct punit_group_test_static_info {
	u32 instance;
	u32 sensor_id;
	u32 sensor_position;
	u32 sensor_width;
	u32 sensor_height;
	u32 sensor_fps;
	u32 lic_ch;
	u32 buf_maxcount;
	u32 enable[GROUP_SLOT_MAX];
	u32 input[GROUP_SLOT_MAX];
};

static struct punit_group_test_map map_static_info[] = {
	[0].idx = 0,
	[0].key = "instance",
	[0].count = 1,

	[1].idx = 1,
	[1].key = "sensor_id",
	[1].count = 1,

	[2].idx = 2,
	[2].key = "sensor_position",
	[2].count = 1,

	[3].idx = 3,
	[3].key = "sensor_width",
	[3].count = 1,

	[4].idx = 4,
	[4].key = "sensor_height",
	[4].count = 1,

	[5].idx = 5,
	[5].key = "sensor_fps",
	[5].count = 1,

	[6].idx = 6,
	[6].key = "lic_ch",
	[6].count = 1,

	[7].idx = 7,
	[7].key = "buf_maxcount",
	[7].count = 1,

	[8].idx = 8,
	[8].key = "enable",
	[8].count = GROUP_SLOT_MAX,

	[9].idx = 8 + GROUP_SLOT_MAX,
	[9].key = "input",
	[9].count = GROUP_SLOT_MAX,
};

struct punit_group_test_work_info {
	struct kthread_work work;
	u32 instance;
	u32 slot;
};

struct punit_group_test_ctx {
	bool stop;
	struct is_video_ctx vctx[GROUP_SLOT_MAX];
	struct task_struct *task_dq[GROUP_SLOT_MAX];
	struct kthread_worker worker[GROUP_SLOT_MAX];
	struct punit_group_test_work_info work_info[GROUP_SLOT_MAX];
};

#define CROP_IN(x, y, w, h) (in[0] = x, in[1] = y, in[2] = w, in[3] = h)
#define CROP_OUT(x, y, w, h) (out[0] = x, out[1] = y, out[2] = w, out[3] = h)

#endif /* PUNIT_GROUP_TEST_H */
