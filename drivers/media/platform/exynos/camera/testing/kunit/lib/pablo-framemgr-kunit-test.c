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

#include <linux/vmalloc.h>

#include "pablo-kunit-test.h"
#include "pablo-framemgr.h"
#include "is-groupmgr.h"

#define PKT_FRAMEMGR_ID		0x12345678
#define PKT_FRAMEMGR_NAME	"PKT_FRMGR"
#define PKT_FRAMEMGR_FRM_NUM	3

static struct pablo_kunit_test_ctx {
	struct is_framemgr *framemgr;
	u32 grp_shot_cnt;
} pkt_ctx;

static int pkt_group_shot(struct is_groupmgr *groupmgr, struct is_group *group, struct is_frame *frame)
{
	pkt_ctx.grp_shot_cnt++;

	return 0;
}

/* Define test cases */
static void pablo_framemgr_probe_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr;
	int ret;

	framemgr = vzalloc(sizeof(struct is_framemgr));
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, framemgr);

	ret = frame_manager_probe(framemgr, PKT_FRAMEMGR_ID, PKT_FRAMEMGR_NAME);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, framemgr->id, (unsigned long)PKT_FRAMEMGR_ID);
	KUNIT_EXPECT_STREQ(test, (char *)framemgr->name, PKT_FRAMEMGR_NAME);
	KUNIT_EXPECT_TRUE(test, (framemgr->frames == NULL));

	pkt_ctx.framemgr = framemgr;
}

static void pablo_framemgr_open_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr = pkt_ctx.framemgr;
	int ret;
	u32 i;
	void *ptr;

	/* TC #1. When it already has frames */
	framemgr->frames = vzalloc(array_size(sizeof(struct is_frame), PKT_FRAMEMGR_FRM_NUM));
	ptr = (void *)framemgr->frames;

	ret = frame_manager_open(framemgr, PKT_FRAMEMGR_FRM_NUM);
	KUNIT_ASSERT_EQ(test, ret, 0);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, (void *)framemgr->frames);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, (void *)framemgr->parameters);
	KUNIT_EXPECT_EQ(test, framemgr->num_frames, (u32)PKT_FRAMEMGR_FRM_NUM);
	KUNIT_EXPECT_PTR_NE(test, (void *)framemgr->frames, ptr);

	for (i = 0; i < PKT_FRAMEMGR_FRM_NUM; i++) {
		KUNIT_EXPECT_EQ(test, framemgr->frames[i].index, i);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, (void *)framemgr->frames[i].parameter);
		KUNIT_EXPECT_EQ(test, framemgr->frames[i].state, (u32)FS_FREE);
	}
}

static void pablo_framemgr_frame_fcount_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr = pkt_ctx.framemgr;
	struct is_frame *frame;
	ulong ret, data;

	frame = &framemgr->frames[0];
	frame->fcount = 5;
	data = 2;

	ret = frame_fcount(frame, (void *)data);
	KUNIT_EXPECT_EQ(test, ret, (frame->fcount - data));
}

static void pablo_framemgr_put_frame_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr = pkt_ctx.framemgr;
	struct is_frame *frame, *prev_f;
	int ret;
	u32 state;

	frame = get_frame(framemgr, FS_FREE);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, frame);

	/* TC #1. Put INVALID state frame */
	state = FS_INVALID;
	ret = put_frame(framemgr, frame, state);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_EQ(test, framemgr->queued_count[FS_FREE], (framemgr->num_frames - 1));

	/* TC #2. Put NULL frame */
	state = FS_FREE;
	ret = put_frame(framemgr, NULL, state);
	KUNIT_EXPECT_EQ(test, ret, -EFAULT);
	KUNIT_EXPECT_EQ(test, framemgr->queued_count[state], (framemgr->num_frames - 1));

	/* TC #3. Put REQUEST frame */
	state = FS_REQUEST;
	ret = put_frame(framemgr, frame, state);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, frame->state, state);
	KUNIT_EXPECT_EQ(test, framemgr->queued_count[state], (u32)1);
	KUNIT_EXPECT_EQ(test, framemgr->queued_count[FS_FREE], (framemgr->num_frames - 1));

	while (framemgr->queued_count[FS_FREE]) {
		prev_f = frame = get_frame(framemgr, FS_FREE);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, frame);

		ret = put_frame(framemgr, frame, FS_REQUEST);
		KUNIT_EXPECT_EQ(test, ret, 0);

		frame = list_first_entry(&framemgr->queued_list[FS_REQUEST], struct is_frame, list);
		KUNIT_EXPECT_EQ(test, frame->index, (u32)0);
		frame = list_last_entry(&framemgr->queued_list[FS_REQUEST], struct is_frame, list);
		KUNIT_EXPECT_PTR_EQ(test, frame, prev_f);
	}

	/* Clean-up framemgr */
	ret = frame_manager_flush(pkt_ctx.framemgr);
	KUNIT_EXPECT_EQ(test, ret, (int)0);
}

static void pablo_framemgr_get_frame_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr = pkt_ctx.framemgr;
	struct is_frame *frame;
	u32 state, i;
	int ret;

	/* TC #1. Get INVALID state frame */
	state = FS_INVALID;
	frame = get_frame(framemgr, state);
	KUNIT_EXPECT_TRUE(test, (frame == NULL));
	KUNIT_EXPECT_EQ(test, framemgr->queued_count[FS_FREE], framemgr->num_frames);

	/* TC #2. Get frame from empty list */
	state = FS_REQUEST;
	frame = get_frame(framemgr, state);
	KUNIT_EXPECT_TRUE(test, (frame == NULL));
	KUNIT_EXPECT_EQ(test, framemgr->queued_count[FS_FREE], framemgr->num_frames);
	KUNIT_EXPECT_TRUE(test, !framemgr->queued_count[state]);

	/* TC #3. Get FREE state frame */
	state = FS_FREE;
	for (i = 0; i < framemgr->num_frames; i++) {
		frame = get_frame(framemgr, state);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, frame);
		KUNIT_EXPECT_EQ(test, frame->index, i);
		KUNIT_EXPECT_EQ(test, frame->state, (u32)FS_INVALID);
		KUNIT_EXPECT_EQ(test, framemgr->queued_count[state], (framemgr->num_frames - 1));

		ret = put_frame(framemgr, frame, state);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}

	/* Clean-up framemgr */
	ret = frame_manager_flush(pkt_ctx.framemgr);
	KUNIT_EXPECT_EQ(test, ret, (int)0);
}

static void pablo_framemgr_trans_frame_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr = pkt_ctx.framemgr;
	struct is_frame *frame;
	u32 state, i, num_frame;
	int ret;

	/* TC #1. Trans NULL frame */
	state = FS_REQUEST;
	ret = trans_frame(framemgr, NULL, state);
	KUNIT_EXPECT_EQ(test, ret, -EFAULT);
	KUNIT_EXPECT_EQ(test, framemgr->queued_count[FS_FREE], framemgr->num_frames);
	KUNIT_EXPECT_EQ(test, framemgr->queued_count[state], (u32)0);

	/* TC #2. Trans frame into INVALID state */
	state = FS_FREE;
	frame = peek_frame(framemgr, state);
	ret = trans_frame(framemgr, frame, FS_INVALID);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_EQ(test, framemgr->queued_count[state], framemgr->num_frames);

	/* TC #3. Trans INVALID frame */
	state = FS_REQUEST;
	frame = get_frame(framemgr, FS_FREE);
	ret = trans_frame(framemgr, frame, state);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_EQ(test, framemgr->queued_count[FS_FREE], (framemgr->num_frames - 1));
	KUNIT_EXPECT_EQ(test, framemgr->queued_count[state], (u32)0);
	put_frame(framemgr, frame, FS_REQUEST);

	/* TC #4. Trans frame */
	state = FS_PROCESS;
	num_frame = framemgr->queued_count[FS_FREE];
	for (i = 0; i < num_frame; i++) {
		frame = peek_frame(framemgr, FS_FREE);
		ret = trans_frame(framemgr, frame, state);
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, framemgr->queued_count[FS_FREE], (framemgr->num_frames - 2 - i));
		KUNIT_EXPECT_EQ(test, framemgr->queued_count[state], (1 + i));
		KUNIT_EXPECT_EQ(test, frame->bak_flag, frame->out_flag);
	}

	/* Clean-up framemgr */
	ret = frame_manager_flush(pkt_ctx.framemgr);
	KUNIT_EXPECT_EQ(test, ret, (int)0);
}

static void pablo_framemgr_peek_frame_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr = pkt_ctx.framemgr;
	struct is_frame *frame;
	u32 state, i;

	/* TC #1. Peek INVALID frame */
	frame = peek_frame(framemgr, FS_INVALID);
	KUNIT_EXPECT_TRUE(test, (frame == NULL));

	/* TC #2. Peek frame from empty list */
	frame = peek_frame(framemgr, FS_REQUEST);
	KUNIT_EXPECT_TRUE(test, (frame == NULL));

	/* TC #3. Peek FREE frame */
	state = FS_FREE;
	for (i = 0; i < framemgr->queued_count[state]; i++) {
		frame = peek_frame(framemgr, state);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, frame);
		KUNIT_EXPECT_EQ(test, frame->state, (u32)FS_FREE);
		KUNIT_EXPECT_EQ(test, frame->index, (u32)0);
	}
}

static void pablo_framemgr_peek_frame_tail_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr = pkt_ctx.framemgr;
	struct is_frame *frame;
	u32 state, i;

	/* TC #1. Peek INVALID frame */
	frame = peek_frame_tail(framemgr, FS_INVALID);
	KUNIT_EXPECT_TRUE(test, (frame == NULL));

	/* TC #2. Peek frame from empty list */
	frame = peek_frame_tail(framemgr, FS_REQUEST);
	KUNIT_EXPECT_TRUE(test, (frame == NULL));

	/* TC #3. Peek FREE frame */
	state = FS_FREE;
	for (i = 0; i < framemgr->queued_count[state]; i++) {
		frame = peek_frame_tail(framemgr, state);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, frame);
		KUNIT_EXPECT_EQ(test, frame->state, (u32)FS_FREE);
		KUNIT_EXPECT_EQ(test, frame->index, (u32)2);
	}
}

static void pablo_framemgr_find_frame_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr = pkt_ctx.framemgr;
	struct is_frame *frame;
	u32 fcount;

	/* TC #1. Find INVALID frame */
	frame = find_frame(framemgr, FS_INVALID, NULL, NULL);
	KUNIT_EXPECT_TRUE(test, (frame == NULL));

	/* TC #2. Find frame from empty list */
	frame = find_frame(framemgr, FS_REQUEST, NULL, NULL);
	KUNIT_EXPECT_TRUE(test, (frame == NULL));

	/* TC #3. Find frame with function */
	fcount = 0;
	frame = find_frame(framemgr, FS_FREE, frame_fcount, (void *)(ulong)fcount);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, frame);
	KUNIT_EXPECT_EQ(test, frame->fcount, fcount);
}

static void pablo_framemgr_print_queues_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr = pkt_ctx.framemgr;
	u32 num_frames;

	/* TC #1. Empty framemgr */
	num_frames = framemgr->num_frames;
	framemgr->num_frames = 0;
	frame_manager_print_queues(framemgr);

	/* TC #2. Normal framemgr */
	framemgr->num_frames = num_frames;
	frame_manager_print_queues(framemgr);
}

static void pablo_framemgr_dump_queues_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr = pkt_ctx.framemgr;
	u32 num_frames;

	/* TC #1. Empty framemgr */
	num_frames = framemgr->num_frames;
	framemgr->num_frames = 0;
	frame_manager_dump_queues(framemgr);

	/* TC #2. Normal framemgr */
	framemgr->num_frames = num_frames;
	frame_manager_dump_queues(framemgr);
}

static void pablo_framemgr_print_info_queues_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr = pkt_ctx.framemgr;

	frame_manager_print_info_queues(framemgr);
}

static void pablo_framemgr_default_frame_work_fn_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr = pkt_ctx.framemgr;
	struct is_frame *frame;
	struct is_groupmgr *groupmgr;
	struct is_group *group;
	struct kthread_work *work;
	struct kthread_delayed_work *dwork;

	/* Setup the required data structures */
	groupmgr = kunit_kzalloc(test, sizeof(struct is_groupmgr), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, groupmgr);
	group = kunit_kzalloc(test, sizeof(struct is_group), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, group);
	frame = peek_frame(framemgr, FS_FREE);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, frame);
	work = &frame->work;
	dwork = &frame->dwork;

	pkt_ctx.grp_shot_cnt = 0;
	groupmgr->fn_group_shot = &pkt_group_shot;
	frame->groupmgr = groupmgr;

	/* TC #1. Frame has group */
	atomic_set(&group->rcount, 2);
	frame->group = group;
	work->func(work);
	dwork->work.func(&dwork->work);

	KUNIT_EXPECT_EQ(test, atomic_read(&group->rcount), 0);
	KUNIT_EXPECT_EQ(test, pkt_ctx.grp_shot_cnt, (u32)2);

	/* TC #2. Frame has repeat_info */
	frame->repeat_info.num = 1;
	frame->stripe_info.region_num = 2;
	work->func(work);
	dwork->work.func(&dwork->work);
	KUNIT_EXPECT_EQ(test, pkt_ctx.grp_shot_cnt, (u32)6);

	kunit_kfree(test, group);
	kunit_kfree(test, groupmgr);
}

static void pablo_framemgr_close_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr = pkt_ctx.framemgr;
	int ret;

	ret = frame_manager_close(framemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_TRUE(test, framemgr->frames == NULL);
	KUNIT_EXPECT_TRUE(test, framemgr->parameters == NULL);
	KUNIT_EXPECT_EQ(test, framemgr->num_frames, (u32)0);
}

static void pablo_framemgr_release_kunit_test(struct kunit *test)
{
	vfree(pkt_ctx.framemgr);
}

/**
 * Below order should be kept to maintain valid framemgr object for running TCs.
 *  - probe()
 *  - open()
 *  - Other TCs...
 *  - release()
 */
static struct kunit_case pablo_framemgr_kunit_test_cases[] = {
	KUNIT_CASE(pablo_framemgr_probe_kunit_test),
	KUNIT_CASE(pablo_framemgr_open_kunit_test),
	KUNIT_CASE(pablo_framemgr_frame_fcount_kunit_test),
	KUNIT_CASE(pablo_framemgr_put_frame_kunit_test),
	KUNIT_CASE(pablo_framemgr_get_frame_kunit_test),
	KUNIT_CASE(pablo_framemgr_trans_frame_kunit_test),
	KUNIT_CASE(pablo_framemgr_peek_frame_kunit_test),
	KUNIT_CASE(pablo_framemgr_peek_frame_tail_kunit_test),
	KUNIT_CASE(pablo_framemgr_find_frame_kunit_test),
	KUNIT_CASE(pablo_framemgr_print_queues_kunit_test),
	KUNIT_CASE(pablo_framemgr_dump_queues_kunit_test),
	KUNIT_CASE(pablo_framemgr_print_info_queues_kunit_test),
	KUNIT_CASE(pablo_framemgr_default_frame_work_fn_kunit_test),
	KUNIT_CASE(pablo_framemgr_close_kunit_test),
	KUNIT_CASE(pablo_framemgr_release_kunit_test),
	{},
};

struct kunit_suite pablo_framemgr_kunit_test_suite = {
	.name = "pablo-framemgr-kunit-test",
	.test_cases = pablo_framemgr_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_framemgr_kunit_test_suite);

MODULE_LICENSE("GPL v2");
