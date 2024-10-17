// SPDX-License-Identifier: GPL-2.0
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

#include "pablo-kunit-test.h"
#include "is-groupmgr.h"
#include "is-device-ischain.h"
#include "is-device-sensor.h"

static struct pablo_kunit_groupmgr_func *func;

static struct test_ctx {
	struct is_device_ischain ischain;
	struct is_device_sensor sensor;
	struct is_groupmgr groupmgr;
	struct is_video_ctx video_ctx;
	struct is_queue queue;
	struct is_subdev subdev;
	struct is_frame frame;
	struct camera2_shot_ext shot_ext;
	struct camera2_shot shot;
} test_ctx;

static int dummy_shot_callback(struct is_device_ischain *device,
				struct is_group *group,
				struct is_frame *check_frame)
{
	return 0;
}

static void pablo_group_lock_unlock_kunit_test(struct kunit *test)
{
	int ret;
	unsigned long flags;
	struct is_group *group = &test_ctx.sensor.group_sensor;
	struct is_subdev *subdev = &test_ctx.subdev;
	struct is_queue *t_q_ssvc0 = kunit_kzalloc(test, sizeof(struct is_queue), 0);
	struct is_video_ctx *t_vctx_ssvc0 = kunit_kzalloc(test, sizeof(struct is_video_ctx), 0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, t_q_ssvc0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, t_vctx_ssvc0);

	/* Sub TC: !group */
	ret = func->group_lock(NULL, IS_DEVICE_MAX, false, &flags);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	func->group_unlock(NULL, flags, IS_DEVICE_MAX, false);

	/* Sub TC: no leader_lock */
	group->locked_sub_framemgr[ENTRY_SSVC0] = (struct is_framemgr *)0xF;
	ret = func->group_lock(group, IS_DEVICE_MAX, false, &flags);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, flags, 0UL);
	KUNIT_EXPECT_EQ(test, (ulong)group->locked_sub_framemgr[ENTRY_SSVC0], 0UL);

	func->group_unlock(group, flags, IS_DEVICE_MAX, false);

	/* Sub TC: leader_lock */
	frame_manager_probe(&test_ctx.queue.framemgr, "KUGRLD");
	group->leader.vctx = &test_ctx.video_ctx;
	group->head = group;

	frame_manager_probe(&t_q_ssvc0->framemgr, "KUGRSD");
	t_vctx_ssvc0->queue = t_q_ssvc0;
	subdev->vctx = t_vctx_ssvc0;
	set_bit(IS_SUBDEV_START, &subdev->state);
	group->subdev[ENTRY_SSVC0] = subdev;

	ret = func->group_lock(group, IS_DEVICE_MAX, true, &flags);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, flags, 0UL);
	KUNIT_EXPECT_EQ(test, (ulong)group->locked_sub_framemgr[ENTRY_SSVC0],
			(ulong)&subdev->vctx->queue->framemgr);

	func->group_unlock(group, flags, IS_DEVICE_MAX, true);
	KUNIT_EXPECT_EQ(test, (ulong)group->locked_sub_framemgr[ENTRY_SSVC0], 0UL);

	/* restore */
	kunit_kfree(test, t_q_ssvc0);
	kunit_kfree(test, t_vctx_ssvc0);
}

static void pablo_group_subdev_cancel_kunit_test(struct kunit *test)
{
	struct is_group *group = &test_ctx.sensor.group_sensor;
	struct is_video_ctx *vctx = &test_ctx.video_ctx;
	struct is_framemgr *sub_framemgr;
	struct is_frame *ldr_frame = &test_ctx.frame, *sub_frame;
	struct is_subdev *subdev;
	struct camera2_stream *t_stream = kunit_kzalloc(test, sizeof(struct camera2_stream), 0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, t_stream);

	/* Sub TC: invalid input */
	func->group_subdev_cancel(NULL, NULL, IS_DEVICE_MAX, FS_REQUEST, false);

	/* Sub TC: !sub_vctx */
	subdev = &group->leader;
	list_add_tail(&subdev->list, &group->subdev_list);
	func->group_subdev_cancel(group, ldr_frame, IS_DEVICE_MAX, FS_REQUEST, false);

	/* Sub TC: !sub_framemgr */
	subdev->vctx = vctx;
	func->group_subdev_cancel(group, ldr_frame, IS_DEVICE_MAX, FS_REQUEST, false);

	/* Sub TC: !test_bit(IS_SUBDEV_START, &subdev->state) */
	sub_framemgr = &subdev->vctx->queue->framemgr;
	frame_manager_probe(sub_framemgr, "KUGRLD");
	func->group_subdev_cancel(group, ldr_frame, IS_DEVICE_MAX, FS_REQUEST, false);

	/* Sub TC: cid >= CAPTURE_NODE_MAX || !cap_node->request */
	set_bit(IS_SUBDEV_START, &subdev->state);
	func->group_subdev_cancel(group, ldr_frame, IS_DEVICE_MAX, FS_REQUEST, false);

	/* Sub TC: cap_node->vid == subdev->vid */
	ldr_frame->shot_ext->node_group.capture[0].request = 1;
	ldr_frame->shot_ext->node_group.capture[0].vid = subdev->vid = 2;
	func->group_subdev_cancel(group, ldr_frame, IS_DEVICE_MAX, FS_REQUEST, false);

	/* Sub TC: sub_frame */
	frame_manager_open(sub_framemgr, 8, false);
	sub_frame = peek_frame(sub_framemgr, FS_FREE);
	trans_frame(sub_framemgr, sub_frame, FS_REQUEST);
	set_bit(subdev->id, &ldr_frame->out_flag);
	sub_frame->stream = t_stream;
	func->group_subdev_cancel(group, ldr_frame, IS_DEVICE_MAX, FS_REQUEST, false);
	KUNIT_EXPECT_EQ(test, test_bit(subdev->id, &ldr_frame->out_flag), false);
	KUNIT_EXPECT_TRUE(test, sub_frame->state == FS_COMPLETE);
	trans_frame(sub_framemgr, sub_frame, FS_FREE);

	/* Sub TC: sub_frame->fcount > ldr_frame->fcount */
	sub_frame = peek_frame(sub_framemgr, FS_FREE);
	sub_frame->fcount = 10;
	ldr_frame->fcount = 9;
	trans_frame(sub_framemgr, sub_frame, FS_REQUEST);
	set_bit(subdev->id, &ldr_frame->out_flag);
	func->group_subdev_cancel(group, ldr_frame, IS_DEVICE_MAX, FS_REQUEST, false);
	KUNIT_EXPECT_EQ(test, test_bit(subdev->id, &ldr_frame->out_flag), true);
	KUNIT_EXPECT_TRUE(test, sub_frame->state == FS_REQUEST);
	trans_frame(sub_framemgr, sub_frame, FS_FREE);

	/* restore */
	kunit_kfree(test, t_stream);
	frame_manager_close(sub_framemgr);
}

static void pablo_group_cancel_kunit_test(struct kunit *test)
{
	struct is_group *group = &test_ctx.sensor.group_sensor;
	struct is_framemgr *framemgr = &test_ctx.queue.framemgr;
	struct is_frame *frame, *ret_frame, *prev_frame;

	/* Sub TC: !ldr_framemgr->num_frames */
	group->leader.vctx = &test_ctx.video_ctx;
	group->head = group;
	func->group_cancel(group, NULL);

	/* Sub TC: no prev_frame & ldr_frame->state == FS_REQUEST */
	frame_manager_probe(framemgr, "KUGRLD");
	frame_manager_open(framemgr, 8, false);
	frame = peek_frame(framemgr, FS_FREE);
	trans_frame(framemgr, frame, FS_REQUEST);
	set_bit(group->leader.id, &frame->out_flag);
	func->group_cancel(group, frame);
	KUNIT_EXPECT_EQ(test, test_bit(group->leader.id, &frame->out_flag), false);
	ret_frame = peek_frame(framemgr, FS_COMPLETE);
	KUNIT_EXPECT_TRUE(test, frame == ret_frame);
	trans_frame(framemgr, ret_frame, FS_FREE);

	/* Sub TC: no prev_frame & ldr_frame->state == FS_FREE */
	frame = peek_frame(framemgr, FS_FREE);
	set_bit(group->leader.id, &frame->out_flag);
	func->group_cancel(group, frame);
	KUNIT_EXPECT_EQ(test, test_bit(group->leader.id, &frame->out_flag), true);
	clear_bit(group->leader.id, &frame->out_flag);

	/* Sub TC: no prev_frame & ldr_frame->state == FS_COMPLETE */
	frame = peek_frame(framemgr, FS_FREE);
	trans_frame(framemgr, frame, FS_COMPLETE);
	set_bit(group->leader.id, &frame->out_flag);
	func->group_cancel(group, frame);
	KUNIT_EXPECT_EQ(test, test_bit(group->leader.id, &frame->out_flag), true);
	clear_bit(group->leader.id, &frame->out_flag);
	trans_frame(framemgr, frame, FS_FREE);

	/* Sub TC: prev_frame == FS_PROCESS & ldr_frame->state == FS_REQUEST */
	prev_frame = peek_frame(framemgr, FS_FREE);
	trans_frame(framemgr, prev_frame, FS_PROCESS);
	frame = peek_frame(framemgr, FS_FREE);
	trans_frame(framemgr, frame, FS_REQUEST);
	set_bit(group->leader.id, &frame->out_flag);
	func->group_cancel(group, frame);
	KUNIT_EXPECT_EQ(test, test_bit(group->leader.id, &frame->out_flag), false);
	ret_frame = peek_frame(framemgr, FS_COMPLETE);
	KUNIT_EXPECT_TRUE(test, frame == ret_frame);
	trans_frame(framemgr, ret_frame, FS_FREE);

	/* restore */
	frame_manager_close(framemgr);
}

static void pablo_wait_req_frame_before_stop_kunit_test(struct kunit *test)
{
	bool ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_groupmgr *groupmgr = &test_ctx.groupmgr;
	struct is_framemgr *framemgr = &test_ctx.queue.framemgr;
	struct is_group *head;
	struct is_group_task *gtask;
	struct is_frame *frame;

	head = idi->group[GROUP_SLOT_SENSOR + 1];
	head->id = 0;
	gtask = &groupmgr->gtask[head->id];
	frame_manager_probe(framemgr, "KUNIT");
	frame_manager_open(framemgr, 8, false);

	/* Sub TC: !framemgr->queued_count[FS_REQUEST] */
	clear_bit(IS_GROUP_OTF_INPUT, &head->state);
	ret = func->wait_req_frame_before_stop(head, gtask, framemgr);
	KUNIT_EXPECT_EQ(test, ret, (bool)false);

	/* Sub TC: framemgr->queued_count[FS_REQUEST] && !test_bit(IS_GROUP_OTF_INPUT, &head->state) */
	frame = peek_frame(framemgr, FS_FREE);
	trans_frame(framemgr, frame, FS_REQUEST);
	ret = func->wait_req_frame_before_stop(head, gtask, framemgr);
	KUNIT_EXPECT_EQ(test, ret, (bool)true);

	/* Sub TC: test_bit(IS_GROUP_OTF_INPUT, &head->state) */
	/* TODO: it needs mocking for semapore down.  */

	/* restore */
	frame_manager_close(framemgr);
}

static void pablo_wait_shot_thread_kunit_test(struct kunit *test)
{
	bool ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_group *head = idi->group[GROUP_SLOT_SENSOR + 1];

	/* Sub TC: !test_bit(IS_GROUP_SHOT, &head->state) */
	clear_bit(IS_GROUP_SHOT, &head->state);
	ret = func->wait_shot_thread(head);
	KUNIT_EXPECT_EQ(test, ret, (bool)false);

	/* Sub TC: test_bit(IS_GROUP_SHOT, &head->state) */
	set_bit(IS_GROUP_SHOT, &head->state);
	ret = func->wait_shot_thread(head);
	KUNIT_EXPECT_EQ(test, ret, (bool)true);
}

static void pablo_wait_pro_frame_kunit_test(struct kunit *test)
{
	bool ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_group *head = idi->group[GROUP_SLOT_SENSOR + 1];
	struct is_framemgr *framemgr = &test_ctx.queue.framemgr;
	struct is_frame *frame;

	frame_manager_probe(framemgr, "KUNIT");
	frame_manager_open(framemgr, 8, false);

	/* Sub TC: !framemgr->queued_count[FS_PROCESS] */
	clear_bit(IS_GROUP_SHOT, &head->state);
	ret = func->wait_pro_frame(head, framemgr);
	KUNIT_EXPECT_EQ(test, ret, (bool)false);

	/* Sub TC: framemgr->queued_count[FS_PROCESS] */
	frame = peek_frame(framemgr, FS_FREE);
	trans_frame(framemgr, frame, FS_PROCESS);
	set_bit(IS_GROUP_SHOT, &head->state);
	ret = func->wait_pro_frame(head, framemgr);
	KUNIT_EXPECT_EQ(test, ret, (bool)true);

	/* restore */
	frame_manager_close(framemgr);
}

static void pablo_wait_req_frame_after_stop_kunit_test(struct kunit *test)
{
	bool ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_group *head = idi->group[GROUP_SLOT_SENSOR + 1];
	struct is_framemgr *framemgr = &test_ctx.queue.framemgr;
	struct is_frame *frame;

	frame_manager_probe(framemgr, "KUNIT");
	frame_manager_open(framemgr, 8, false);

	/* Sub TC: !framemgr->queued_count[FS_REQUEST] */
	clear_bit(IS_GROUP_SHOT, &head->state);
	ret = func->wait_req_frame_after_stop(head, framemgr);
	KUNIT_EXPECT_EQ(test, ret, (bool)false);

	/* Sub TC: framemgr->queued_count[FS_REQUEST] */
	frame = peek_frame(framemgr, FS_FREE);
	trans_frame(framemgr, frame, FS_REQUEST);
	set_bit(IS_GROUP_SHOT, &head->state);
	ret = func->wait_req_frame_after_stop(head, framemgr);
	KUNIT_EXPECT_EQ(test, ret, (bool)true);
	KUNIT_EXPECT_TRUE(test, frame == peek_frame(framemgr, FS_COMPLETE));

	/* restore */
	frame_manager_close(framemgr);
}

static void pablo_check_remain_req_count_kunit_test(struct kunit *test)
{
	bool ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_group *head = idi->group[GROUP_SLOT_SENSOR + 1];
	struct is_groupmgr *groupmgr = &test_ctx.groupmgr;
	struct is_group_task *gtask;

	head->id = 0;
	gtask = &groupmgr->gtask[head->id];
	spin_lock_init(&gtask->gtask_slock);

	/* Sub TC: !rcount */
	atomic_set(&head->rcount, 0);
	ret = func->check_remain_req_count(head, gtask);
	KUNIT_EXPECT_EQ(test, ret, (bool)false);

	/* Sub TC: rcount */
	atomic_set(&head->rcount, 1);
	ret = func->check_remain_req_count(head, gtask);
	KUNIT_EXPECT_EQ(test, ret, (bool)true);
	KUNIT_EXPECT_EQ(test, atomic_read(&head->rcount), 0);
}

static void pablo_group_override_meta_kunit_test(struct kunit *test)
{
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_device_sensor *ids = &test_ctx.sensor;
	struct is_group *group = idi->group[GROUP_SLOT_SENSOR + 1];
	struct is_frame *frame = &test_ctx.frame;
	struct is_resourcemgr *t_rscmgr;
	u32 t_magic = 1234;

	t_rscmgr = kunit_kzalloc(test, sizeof(struct is_resourcemgr), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, t_rscmgr);

	ids->cfg = kunit_kzalloc(test, sizeof(struct is_sensor_cfg), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ids->cfg);

	/* Sub TC: resourcemgr->limited_fps */
	t_rscmgr->limited_fps = t_magic;
	ids->cfg->pd_mode = PD_NONE;
	func->group_override_meta(group, frame, t_rscmgr, ids);
	KUNIT_EXPECT_EQ(test, frame->shot->ctl.aa.aeTargetFpsRange[0], t_magic);
	KUNIT_EXPECT_EQ(test, frame->shot->ctl.aa.aeTargetFpsRange[1], t_magic);
	KUNIT_EXPECT_EQ(test, frame->shot->ctl.aa.ispHwTargetFpsRange[0], t_magic);
	KUNIT_EXPECT_EQ(test, frame->shot->ctl.aa.ispHwTargetFpsRange[1], t_magic);
	KUNIT_EXPECT_TRUE(test, frame->shot->uctl.isModeUd.paf_mode == CAMERA_PAF_OFF);
	t_rscmgr->limited_fps = 0;

	/* Sub TC: ex_mode == EX_DUALFPS_960 */
	ids->cfg->ex_mode = EX_DUALFPS_960;
	func->group_override_meta(group, frame, t_rscmgr, ids);
	KUNIT_EXPECT_EQ(test, frame->shot->ctl.aa.ispHwTargetFpsRange[0], 60U);
	KUNIT_EXPECT_EQ(test, frame->shot->ctl.aa.ispHwTargetFpsRange[1], 60U);
	ids->cfg->ex_mode = EX_NONE;

	/* Sub TC: frame->shot->ctl.aa.vendor_fpsHintResult != 0 */
	frame->shot->ctl.aa.vendor_fpsHintResult = t_magic;
	func->group_override_meta(group, frame, t_rscmgr, ids);
	KUNIT_EXPECT_EQ(test, frame->shot->ctl.aa.ispHwTargetFpsRange[0], t_magic);
	KUNIT_EXPECT_EQ(test, frame->shot->ctl.aa.ispHwTargetFpsRange[1], t_magic);
	frame->shot->ctl.aa.vendor_fpsHintResult = 0;

	/* Sub TC: sensor->cfg->pd_mode != PD_NONE */
	ids->cfg->pd_mode = PD_MOD1;
	func->group_override_meta(group, frame, t_rscmgr, ids);
	KUNIT_EXPECT_TRUE(test, frame->shot->uctl.isModeUd.paf_mode == CAMERA_PAF_ON);

	/* restore */
	kunit_kfree(test, ids->cfg);
	kunit_kfree(test, t_rscmgr);
}

static void compare_shot_eq(struct kunit *test, struct camera2_shot *r_shot,
			    struct camera2_shot *p_shot)
{
	KUNIT_EXPECT_EQ(test, r_shot->ctl.sensor.exposureTime, p_shot->ctl.sensor.exposureTime);
	KUNIT_EXPECT_EQ(test, r_shot->ctl.sensor.frameDuration, p_shot->ctl.sensor.frameDuration);
	KUNIT_EXPECT_EQ(test, r_shot->ctl.sensor.sensitivity, p_shot->ctl.sensor.sensitivity);
	KUNIT_EXPECT_EQ(test, r_shot->ctl.aa.vendor_isoValue, p_shot->ctl.aa.vendor_isoValue);
	KUNIT_EXPECT_EQ(test, r_shot->ctl.aa.vendor_isoMode, p_shot->ctl.aa.vendor_isoMode);
	KUNIT_EXPECT_EQ(test, r_shot->ctl.aa.aeMode, p_shot->ctl.aa.aeMode);
	KUNIT_EXPECT_EQ(test, r_shot->ctl.aa.vendor_aeflashMode, p_shot->ctl.aa.vendor_aeflashMode);
	KUNIT_EXPECT_EQ(test, r_shot->ctl.aa.aeExpCompensation, p_shot->ctl.aa.aeExpCompensation);
	KUNIT_EXPECT_EQ(test, r_shot->ctl.aa.aeLock, p_shot->ctl.aa.aeLock);
	KUNIT_EXPECT_EQ(test, r_shot->ctl.lens.opticalStabilizationMode,
			p_shot->ctl.lens.opticalStabilizationMode);
	KUNIT_EXPECT_EQ(test, r_shot->ctl.aa.sceneMode, p_shot->ctl.aa.sceneMode);
	KUNIT_EXPECT_EQ(test, r_shot->ctl.aa.captureIntent, p_shot->ctl.aa.captureIntent);
	KUNIT_EXPECT_EQ(test, r_shot->ctl.aa.vendor_captureExposureTime,
			p_shot->ctl.aa.vendor_captureExposureTime);
	KUNIT_EXPECT_EQ(test, r_shot->ctl.aa.vendor_captureCount,
			p_shot->ctl.aa.vendor_captureCount);
}

static void compare_shot_ne(struct kunit *test, struct camera2_shot *r_shot,
			    struct camera2_shot *p_shot)
{
	KUNIT_EXPECT_NE(test, r_shot->ctl.sensor.exposureTime, p_shot->ctl.sensor.exposureTime);
	KUNIT_EXPECT_NE(test, r_shot->ctl.sensor.frameDuration, p_shot->ctl.sensor.frameDuration);
	KUNIT_EXPECT_NE(test, r_shot->ctl.sensor.sensitivity, p_shot->ctl.sensor.sensitivity);
	KUNIT_EXPECT_NE(test, r_shot->ctl.aa.vendor_isoValue, p_shot->ctl.aa.vendor_isoValue);
	KUNIT_EXPECT_NE(test, r_shot->ctl.aa.vendor_isoMode, p_shot->ctl.aa.vendor_isoMode);
	KUNIT_EXPECT_NE(test, r_shot->ctl.aa.aeMode, p_shot->ctl.aa.aeMode);
	KUNIT_EXPECT_NE(test, r_shot->ctl.aa.vendor_aeflashMode, p_shot->ctl.aa.vendor_aeflashMode);
	KUNIT_EXPECT_NE(test, r_shot->ctl.aa.aeExpCompensation, p_shot->ctl.aa.aeExpCompensation);
	KUNIT_EXPECT_NE(test, r_shot->ctl.aa.aeLock, p_shot->ctl.aa.aeLock);
	KUNIT_EXPECT_NE(test, r_shot->ctl.lens.opticalStabilizationMode,
			p_shot->ctl.lens.opticalStabilizationMode);
	KUNIT_EXPECT_NE(test, r_shot->ctl.aa.sceneMode, p_shot->ctl.aa.sceneMode);
	KUNIT_EXPECT_NE(test, r_shot->ctl.aa.captureIntent, p_shot->ctl.aa.captureIntent);
	KUNIT_EXPECT_NE(test, r_shot->ctl.aa.vendor_captureExposureTime,
			p_shot->ctl.aa.vendor_captureExposureTime);
	KUNIT_EXPECT_NE(test, r_shot->ctl.aa.vendor_captureCount,
			p_shot->ctl.aa.vendor_captureCount);
}

static void pablo_group_override_sensor_req_kunit_test(struct kunit *test)
{
	struct is_framemgr *framemgr = &test_ctx.queue.framemgr;
	struct is_frame *frame = &test_ctx.frame;
	struct is_frame *req_frame;
	struct camera2_shot *r_shot[SENSOR_REQUEST_DELAY + 1], *p_shot;
	u32 t_magic = 1234, i;

	for (i = 0; i <= SENSOR_REQUEST_DELAY; i++) {
		r_shot[i] = kunit_kzalloc(test, sizeof(struct camera2_shot), 0);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, r_shot[i]);
	}

	frame_manager_probe(framemgr, "KUNIT");
	frame_manager_open(framemgr, SENSOR_REQUEST_DELAY + 1, false);

	for (i = 0; i <= SENSOR_REQUEST_DELAY; i++) {
		req_frame = peek_frame(framemgr, FS_FREE);
		req_frame->shot = r_shot[i];
		trans_frame(framemgr, req_frame, FS_REQUEST);
	}

	p_shot = frame->shot;
	p_shot->ctl.aa.aeMode = AA_AEMODE_OFF;
	p_shot->ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
	p_shot->ctl.aa.sceneMode = AA_SCENE_MODE_PRO_MODE;
	p_shot->ctl.aa.captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_EXPOSURE_DYNAMIC_SHOT;

	p_shot->ctl.sensor.exposureTime = t_magic;
	p_shot->ctl.sensor.frameDuration = t_magic;
	p_shot->ctl.sensor.sensitivity = t_magic;
	p_shot->ctl.aa.vendor_isoValue = t_magic;
	p_shot->ctl.aa.vendor_isoMode = t_magic;
	p_shot->ctl.aa.aeExpCompensation = t_magic;
	p_shot->ctl.aa.aeLock = t_magic;
	p_shot->ctl.lens.opticalStabilizationMode = t_magic;
	p_shot->ctl.aa.vendor_captureExposureTime = t_magic;
	p_shot->ctl.aa.vendor_captureCount = t_magic;

	func->group_override_sensor_req(framemgr, frame);

	for (i = SENSOR_REQUEST_DELAY; i > 0; i--)
		compare_shot_eq(test, r_shot[i], p_shot);

	compare_shot_ne(test, r_shot[0], p_shot);

	/* restore */
	frame_manager_close(framemgr);
	for (i = 0; i <= SENSOR_REQUEST_DELAY; i++)
		kunit_kfree(test, r_shot[i]);
}

static void pablo_group_has_child_kunit_test(struct kunit *test)
{
	bool ret;
	u32 gid;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_group *head, *child;

	head = idi->group[GROUP_SLOT_SENSOR + 1];
	child = idi->group[GROUP_SLOT_SENSOR + 2];
	head->id = 0;
	child->id = 1;
	gid = 2;
	head->child = child;

	/* pos->id != id */
	ret = func->group_has_child(head, gid);
	KUNIT_EXPECT_EQ(test, ret, (bool)false);

	/* pos->id == id */
	child->id = gid;
	ret = func->group_has_child(head, gid);
	KUNIT_EXPECT_EQ(test, ret, (bool)true);
}

static void pablo_group_throttling_tick_check_kunit_test(struct kunit *test)
{
	bool ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_group *group = idi->group[GROUP_SLOT_SENSOR + 1];
	struct is_frame *frame = &test_ctx.frame;
	struct is_resourcemgr *t_rscmgr = kunit_kzalloc(test, sizeof(struct is_resourcemgr), 0);
	struct is_dvfs_scenario_ctrl *t_static_ctrl =
		kunit_kzalloc(test, sizeof(struct is_dvfs_scenario_ctrl), 0);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, t_rscmgr);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, t_static_ctrl);
	t_rscmgr->dvfs_ctrl.static_ctrl = t_static_ctrl;
	idi->resourcemgr = t_rscmgr;

	/* Sub TC: !resourcemgr->limited_fps */
	t_rscmgr->limited_fps = 0;
	ret = func->group_throttling_tick_check(idi, group, frame);
	KUNIT_EXPECT_EQ(test, ret, (bool)true);

	/* Sub TC: !group->gnext */
	t_rscmgr->limited_fps = 1;
	group->next = NULL;
	frame->shot->ctl.aa.aeTargetFpsRange[1] = 3;
	group->thrott_tick = 2;
	ret = func->group_throttling_tick_check(idi, group, frame);
	KUNIT_EXPECT_EQ(test, ret, (bool)false);
	KUNIT_EXPECT_EQ(test, group->thrott_tick, 1U);

	/* Sub TC: !group->thrott_tick */
	ret = func->group_throttling_tick_check(idi, group, frame);
	KUNIT_EXPECT_EQ(test, ret, (bool)true);
	KUNIT_EXPECT_EQ(test, group->thrott_tick, 0U);

	/* restore */
	kunit_kfree(test, t_static_ctrl);
	kunit_kfree(test, t_rscmgr);
}

static void pablo_group_shot_recovery_kunit_test(struct kunit *test)
{
	int ret;
	bool try_sdown, try_rdown, try_gdown[GROUP_ID_MAX];
	struct is_group *head, *child;
	struct is_group_task *gtask_h, *gtask_c;
	struct is_groupmgr *groupmgr = &test_ctx.groupmgr;

	head = test_ctx.ischain.group[GROUP_SLOT_SENSOR + 1];
	child = test_ctx.ischain.group[GROUP_SLOT_SENSOR + 2];
	head->id = 0;
	child->id = 1;
	try_sdown = false;
	try_rdown = false;
	try_gdown[child->id] = false;
	gtask_h = &groupmgr->gtask[head->id];
	gtask_c = &groupmgr->gtask[child->id];
	head->child = child;

	sema_init(&gtask_h->smp_resource, 1);
	ret = down_interruptible(&gtask_h->smp_resource);
	sema_init(&gtask_c->smp_resource, 1);
	ret = down_interruptible(&gtask_c->smp_resource);

	/* Sub TC: test_bit(IS_GROUP_OTF_INPUT, &group->state) */
	set_bit(IS_GROUP_SHOT, &head->state);
	set_bit(IS_GROUP_OTF_INPUT, &head->state);
	func->group_shot_recovery(groupmgr, head, gtask_h, try_sdown, try_rdown, try_gdown);
	KUNIT_EXPECT_EQ(test, test_bit(IS_GROUP_SHOT, &head->state), 0);

	/* Sub TC: !test_bit(IS_GROUP_OPEN, &pos->state) */
	set_bit(IS_GROUP_SHOT, &head->state);
	clear_bit(IS_GROUP_OTF_INPUT, &head->state);
	func->group_shot_recovery(groupmgr, head, gtask_h, try_sdown, try_rdown, try_gdown);
	KUNIT_EXPECT_EQ(test, test_bit(IS_GROUP_SHOT, &head->state), 0);

	/* Sub TC: test_bit(IS_GROUP_OPEN, &pos->state) */
	set_bit(IS_GROUP_SHOT, &head->state);
	set_bit(IS_GROUP_OPEN, &child->state);
	func->group_shot_recovery(groupmgr, head, gtask_h, try_sdown, try_rdown, try_gdown);
	KUNIT_EXPECT_EQ(test, gtask_c->smp_resource.count, 0U);
	KUNIT_EXPECT_EQ(test, atomic_read(&head->smp_shot_count), 0);
	KUNIT_EXPECT_EQ(test, gtask_h->smp_resource.count, 0U);
	KUNIT_EXPECT_EQ(test, test_bit(IS_GROUP_SHOT, &head->state), 0);

	/* Sub TC: down true */
	set_bit(IS_GROUP_SHOT, &head->state);
	try_sdown = true;
	try_rdown = true;
	try_gdown[child->id] = true;
	func->group_shot_recovery(groupmgr, head, gtask_h, try_sdown, try_rdown, try_gdown);
	KUNIT_EXPECT_EQ(test, gtask_c->smp_resource.count, 1U);
	KUNIT_EXPECT_EQ(test, atomic_read(&head->smp_shot_count), 1);
	KUNIT_EXPECT_EQ(test, gtask_h->smp_resource.count, 1U);
	KUNIT_EXPECT_EQ(test, test_bit(IS_GROUP_SHOT, &head->state), 0);
}

static void pablo_group_change_chain_kunit_test(struct kunit *test)
{
	int ret;
	u32 next_id = 0;
	struct is_device_sensor *ids = &test_ctx.sensor;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_group *g_sensor, *g_pdp, *g_cstat;
	struct is_groupmgr *grpmgr;

	g_sensor = &ids->group_sensor;
	g_pdp = idi->group[GROUP_SLOT_PAF];
	g_cstat = idi->group[GROUP_SLOT_3AA];

	/* Sub TC: group->slot != GROUP_SLOT_SENSOR */
	ret = CALL_GROUP_OPS(g_pdp, change_chain, next_id);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !test_bit(IS_GROUP_OTF_OUTPUT, &group->state) */
	ret = CALL_GROUP_OPS(g_sensor, change_chain, next_id);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: "A child group is invalid" */
	set_bit(IS_GROUP_OTF_OUTPUT, &g_sensor->state);
	g_sensor->child = g_sensor;
	ret = CALL_GROUP_OPS(g_sensor, change_chain, next_id);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Sub TC: curr_id == next_id */
	set_bit(IS_GROUP_STANDBY, &g_sensor->state);
	g_sensor->child = g_pdp;
	g_pdp->child = g_cstat;
	next_id = 1;
	g_pdp->id = GROUP_ID_PAF0 + next_id;
	g_cstat->id = GROUP_ID_3AA0 + next_id;
	ret = CALL_GROUP_OPS(g_sensor, change_chain, next_id);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, test_bit(IS_GROUP_STANDBY, &g_sensor->state), false);

	/* Sub TC: fail is_group_task_stop() */
	next_id = 0;
	ret = CALL_GROUP_OPS(g_sensor, change_chain, next_id);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: success is_group_task_stop() */
	next_id = 0;
	grpmgr = is_get_groupmgr();
	ret = func->group_task_open(&grpmgr->gtask[g_pdp->id], g_pdp->slot);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = func->group_task_open(&grpmgr->gtask[g_cstat->id], g_cstat->slot);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = CALL_GROUP_OPS(g_sensor, change_chain, next_id);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, g_pdp->id, GROUP_ID_PAF0 + next_id);
	KUNIT_EXPECT_EQ(test, g_cstat->id, GROUP_ID_3AA0 + next_id);

	ret = func->group_task_close(&grpmgr->gtask[g_pdp->id]);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = func->group_task_close(&grpmgr->gtask[g_cstat->id]);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_group_probe_kunit_test(struct kunit *test)
{
	int ret, slot, i;
	struct is_device_ischain *idi = kunit_kzalloc(test, sizeof(struct is_device_ischain), 0);
	struct is_device_sensor *ids = kunit_kzalloc(test, sizeof(struct is_device_sensor), 0);
	is_shot_callback shot_cb = dummy_shot_callback;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, idi);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ids);

	/* negative test */
	ret = func->group_probe(NULL, idi, NULL, GROUP_SLOT_MAX);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	for (slot = 0; slot < GROUP_SLOT_MAX; slot++)
		KUNIT_EXPECT_TRUE(test, idi->group[slot] == NULL);
	func->group_release(idi, GROUP_SLOT_MAX);

	ret = func->group_probe(NULL, NULL, NULL, GROUP_SLOT_SENSOR);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	func->group_release(NULL, GROUP_SLOT_SENSOR);

	/* positive test with ischain */
	for (slot = GROUP_SLOT_SENSOR + 1; slot < GROUP_SLOT_MAX; slot++) {
		ret = func->group_probe(NULL, idi, shot_cb, slot);
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_TRUE(test, idi->group[slot] != NULL);
		KUNIT_EXPECT_EQ(test, idi->group[slot]->id, (u32)GROUP_ID_MAX);
		KUNIT_EXPECT_EQ(test, idi->group[slot]->slot, (u32)slot);
		KUNIT_EXPECT_TRUE(test, idi->group[slot]->shot_callback == shot_cb);
		KUNIT_EXPECT_TRUE(test, idi->group[slot]->device == idi);
		KUNIT_EXPECT_TRUE(test, idi->group[slot]->sensor == NULL);
		KUNIT_EXPECT_EQ(test, idi->group[slot]->instance, idi->instance);
		KUNIT_EXPECT_EQ(test, idi->group[slot]->device_type, (u32)IS_DEVICE_ISCHAIN);
		KUNIT_EXPECT_TRUE(test, idi->group[slot]->ops != NULL);
		KUNIT_EXPECT_EQ(test, idi->group[slot]->state, 0UL);
		for (i = ENTRY_SENSOR; i < ENTRY_END; i++)
			KUNIT_EXPECT_TRUE(test, idi->group[slot]->subdev[i] == NULL);
		func->group_release(idi, slot);
	}

	/* positive test with sensor */
	slot = GROUP_SLOT_SENSOR;
	func->group_probe(ids, NULL, shot_cb, slot);
	KUNIT_EXPECT_EQ(test, ids->group_sensor.id, (u32)GROUP_ID_MAX);
	KUNIT_EXPECT_EQ(test, ids->group_sensor.slot, (u32)slot);
	KUNIT_EXPECT_TRUE(test, ids->group_sensor.shot_callback == shot_cb);
	KUNIT_EXPECT_TRUE(test, ids->group_sensor.device == NULL);
	KUNIT_EXPECT_TRUE(test, ids->group_sensor.sensor == ids);
	KUNIT_EXPECT_EQ(test, ids->group_sensor.instance, (u32)IS_STREAM_COUNT);
	KUNIT_EXPECT_EQ(test, ids->group_sensor.device_type, (u32)IS_DEVICE_SENSOR);
	KUNIT_EXPECT_TRUE(test, ids->group_sensor.ops != NULL);
	KUNIT_EXPECT_EQ(test, ids->group_sensor.state, 0UL);
	for (i = ENTRY_SENSOR; i < ENTRY_END; i++)
		KUNIT_EXPECT_TRUE(test, ids->group_sensor.subdev[i] == NULL);

	kunit_kfree(test, idi);
	kunit_kfree(test, ids);
}

static int pablo_groupmgr_kunit_test_init(struct kunit *test)
{
	int ret, slot;
	struct is_device_sensor *ids = &test_ctx.sensor;
	struct is_device_ischain *idi = &test_ctx.ischain;
	is_shot_callback shot_cb = dummy_shot_callback;

	func = pablo_kunit_get_groupmgr_func();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, func);

	memset(&test_ctx, 0x00, sizeof(struct test_ctx));

	slot = GROUP_SLOT_SENSOR;
	ret = func->group_probe(ids, NULL, shot_cb, slot);
	KUNIT_ASSERT_EQ(test, ret, 0);

	for (slot = GROUP_SLOT_SENSOR + 1; slot < GROUP_SLOT_MAX; slot++) {
		ret = func->group_probe(NULL, idi, shot_cb, slot);
		KUNIT_ASSERT_EQ(test, ret, 0);
	}

	test_ctx.video_ctx.queue = &test_ctx.queue;
	test_ctx.frame.shot_ext = &test_ctx.shot_ext;
	test_ctx.frame.shot = &test_ctx.shot;

	return 0;
}

static void pablo_groupmgr_kunit_test_exit(struct kunit *test)
{
	int slot;
	struct is_device_ischain *idi = &test_ctx.ischain;

	for (slot = GROUP_SLOT_SENSOR + 1; slot < GROUP_SLOT_MAX; slot++) {
		func->group_release(idi, slot);
		KUNIT_EXPECT_TRUE(test, idi->group[slot] == NULL);
	}
}

static struct kunit_case pablo_groupmgr_kunit_test_cases[] = {
	KUNIT_CASE(pablo_group_lock_unlock_kunit_test),
	KUNIT_CASE(pablo_group_subdev_cancel_kunit_test),
	KUNIT_CASE(pablo_group_cancel_kunit_test),
	KUNIT_CASE(pablo_wait_req_frame_before_stop_kunit_test),
	KUNIT_CASE(pablo_wait_shot_thread_kunit_test),
	KUNIT_CASE(pablo_wait_pro_frame_kunit_test),
	KUNIT_CASE(pablo_wait_req_frame_after_stop_kunit_test),
	KUNIT_CASE(pablo_check_remain_req_count_kunit_test),
	KUNIT_CASE(pablo_group_override_meta_kunit_test),
	KUNIT_CASE(pablo_group_override_sensor_req_kunit_test),
	KUNIT_CASE(pablo_group_has_child_kunit_test),
	KUNIT_CASE(pablo_group_throttling_tick_check_kunit_test),
	KUNIT_CASE(pablo_group_shot_recovery_kunit_test),
	KUNIT_CASE(pablo_group_change_chain_kunit_test),
	KUNIT_CASE(pablo_group_probe_kunit_test),
	{},
};

struct kunit_suite pablo_groupmgr_kunit_test_suite = {
	.name = "pablo-groupmgr-kunit-test",
	.init = pablo_groupmgr_kunit_test_init,
	.exit = pablo_groupmgr_kunit_test_exit,
	.test_cases = pablo_groupmgr_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_groupmgr_kunit_test_suite);

MODULE_LICENSE("GPL");
