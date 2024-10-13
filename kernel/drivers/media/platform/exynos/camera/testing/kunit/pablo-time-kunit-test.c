// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"

#include "is-time.h"
#include "is-groupmgr.h"
#include "is-core.h"

/* Define the test cases. */

static struct pablo_time_test_ctx {
	struct is_group group;
	struct is_frame time_frame;
	struct is_video_ctx vctx;
	struct is_device_ischain device;
	struct is_device_sensor sensor;
	struct is_video video;
	struct is_queue queue;
} pkt_ctx;
#define MPOINT_TIME 6000 * 1000

static struct is_group *pablo_set_group_device(struct kunit *test)
{
	struct is_core *core = is_get_is_core();
	struct is_group *group = &pkt_ctx.group;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, core);

	group->device = &core->ischain[0];

	return group;
}

static void pablo_time_monitor_time_shot_kunit_test(struct kunit *test)
{
	struct is_group *group = pablo_set_group_device(test);
	struct is_frame *time_frame = &pkt_ctx.time_frame;

	monitor_time_shot((void *)group, (void *)time_frame, 0);
}

static void pablo_time_monitor_time_queue_kunit_test(struct kunit *test)
{
	struct is_video_ctx *vctx = &pkt_ctx.vctx;
	unsigned long time_cnt;
	unsigned int avg_cnt;
	u32 mpoint;

	monitor_time_queue((void *)vctx, 0);

	vctx->next_device = &pkt_ctx.device;
	vctx->video = &pkt_ctx.video;
	vctx->queue = &pkt_ctx.queue;

	mpoint = TMQ_DQ;
	vctx->time[mpoint] = 0;
	time_cnt = vctx->time_cnt + 1;
	monitor_time_queue((void *)vctx, mpoint);
	KUNIT_EXPECT_EQ(test, vctx->time_cnt, time_cnt);

	mpoint = TMQ_QE;
	vctx->time[mpoint - 1] = MPOINT_TIME;
	avg_cnt = is_get_debug_param(IS_DEBUG_PARAM_TIME_QUEUE);
	is_set_debug_param(IS_DEBUG_PARAM_TIME_QUEUE, 1);
	monitor_time_queue((void *)vctx, mpoint);
	KUNIT_EXPECT_EQ(test, vctx->time_total[mpoint], (unsigned long long)0);

	is_set_debug_param(IS_DEBUG_PARAM_TIME_QUEUE, avg_cnt);
}

static void pablo_time_start_end_kunit_test(struct kunit *test)
{
	TIME_STR(0);
	TIME_END(0, "KUNIT_TIME_TEST");
}

static void pablo_time_monitor_report_kunit_test(struct kunit *test)
{
	struct is_group *group = pablo_set_group_device(test);
	struct is_frame *time_frame = &pkt_ctx.time_frame;
	int mp_idx;

	pablo_kunit_test_monitor_report((void *)group, (void *)time_frame);

	/* test case for error handling */

	group->device = &pkt_ctx.device;
	group->device->sensor = &pkt_ctx.sensor;

	time_frame->result = 0;
	for (mp_idx = TMS_Q; mp_idx < TMS_END; mp_idx++)
		time_frame->mpoint[mp_idx].check = true;

	/* Shot kthread */
	time_frame->mpoint[TMS_SHOT1].time = MPOINT_TIME;
	clear_bit(IS_GROUP_OTF_INPUT, &group->state);
	group->time.t_dq[0] = time_frame->mpoint[TMS_SHOT1].time;

	/* Shot - Done */
	time_frame->mpoint[TMS_SDONE].time = MPOINT_TIME;
	group->device->sensor->image.framerate = 1000000;

	/* Done - Deque */
	time_frame->mpoint[TMS_DQ].time = MPOINT_TIME;
	group->gnext = &pkt_ctx.group;
	pablo_kunit_test_monitor_report((void *)group, (void *)time_frame);

	/* reprocessing case */
	set_bit(IS_GROUP_OTF_INPUT, &group->state);
	set_bit(IS_ISCHAIN_REPROCESSING, &group->device->state);
	for (mp_idx = TMS_Q; mp_idx < TMS_END; mp_idx++)
		time_frame->mpoint[mp_idx].check = true;
	pablo_kunit_test_monitor_report((void *)group, (void *)time_frame);
}

static void pablo_time_is_jitter_kunit_test(struct kunit *test)
{
	u64 cnt = GET_JITTER_CNT();

	is_jitter(1000);

	SET_JITTER_CNT(1);
	is_jitter(1000);

	SET_JITTER_CNT(50);
	is_jitter(1000);

	SET_JITTER_CNT(cnt);
}

static int pablo_time_kunit_test_init(struct kunit *test)
{
	memset(&pkt_ctx, 0, sizeof(pkt_ctx));

	return 0;
}

static void pablo_time_kunit_test_exit(struct kunit *test)
{
}

static struct kunit_case pablo_time_kunit_test_cases[] = {
	KUNIT_CASE(pablo_time_monitor_time_shot_kunit_test),
	KUNIT_CASE(pablo_time_monitor_time_queue_kunit_test),
	KUNIT_CASE(pablo_time_start_end_kunit_test),
	KUNIT_CASE(pablo_time_monitor_report_kunit_test),
	KUNIT_CASE(pablo_time_is_jitter_kunit_test),
	{},
};

struct kunit_suite pablo_time_kunit_test_suite = {
	.name = "pablo-time-kunit-test",
	.init = pablo_time_kunit_test_init,
	.exit = pablo_time_kunit_test_exit,
	.test_cases = pablo_time_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_time_kunit_test_suite);

MODULE_LICENSE("GPL");
