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

#include "is-subdev-ctrl.h"
#include "is-core.h"
#include "is-device-ischain.h"
#include "is-device-sensor.h"

/* Define the test cases. */

static struct test_ctx {
	struct is_core core;
	struct platform_device pdev;
	struct is_subdev subdev;
	struct is_device_sensor device_s;
	struct is_device_ischain device_i;
	struct is_video_ctx vctx;
	struct is_video video;
} test_ctx;

static void pablo_subdev_ctrl_probe_kunit_test(struct kunit *test)
{
	int ret;
	struct is_subdev *subdev = &test_ctx.subdev;
	u32 instance = 0;
	u32 id = 0;
	const struct is_subdev_ops *sops = NULL;

	ret = is_subdev_probe(subdev, instance, id, "TEST_SUBDEV", sops);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_subdev_ctrl_sensor_subdev_open_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *device = &test_ctx.device_s;
	struct is_video_ctx *vctx = &test_ctx.vctx;

	vctx->subdev = &test_ctx.subdev;
	vctx->video = &test_ctx.video;
	device->instance = 0;

	ret = is_sensor_subdev_open(device, vctx);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = is_sensor_subdev_close(device, vctx);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_subdev_ctrl_sensor_subdev_open_negative_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *device = &test_ctx.device_s;
	struct is_video_ctx *vctx = &test_ctx.vctx;

	vctx->subdev = &test_ctx.subdev;
	vctx->video = &test_ctx.video;
	device->instance = 0;

	/* already open */
	set_bit(IS_SUBDEV_OPEN, &test_ctx.subdev.state);
	ret = is_sensor_subdev_open(device, vctx);
	KUNIT_EXPECT_EQ(test, -EPERM, ret);

	/* subdev is null */
	vctx->subdev = NULL;
	ret = is_sensor_subdev_close(device, vctx);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	vctx->subdev = &test_ctx.subdev;

	/* sudden close */
	set_bit(IS_SENSOR_FRONT_START, &device->state);
	ret = is_sensor_subdev_close(device, vctx);
	KUNIT_EXPECT_EQ(test, 0, ret);
	clear_bit(IS_SENSOR_FRONT_START, &device->state);

	/* subdev is already close */
	vctx->subdev = &test_ctx.subdev;
	clear_bit(IS_SUBDEV_OPEN, &test_ctx.subdev.state);
	ret = is_sensor_subdev_close(device, vctx);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_subdev_ctrl_ischain_subdev_open_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *device = &test_ctx.device_i;
	struct is_video_ctx *vctx = &test_ctx.vctx;

	vctx->subdev = &test_ctx.subdev;
	vctx->video = &test_ctx.video;
	device->instance = 0;
	device->pdev = &test_ctx.pdev;
	platform_set_drvdata(device->pdev, &test_ctx.core);

	ret = is_ischain_subdev_open(device, vctx);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = is_ischain_subdev_close(device, vctx);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_subdev_ctrl_ischain_subdev_open_negative_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *device = &test_ctx.device_i;
	struct is_video_ctx *vctx = &test_ctx.vctx;

	vctx->subdev = &test_ctx.subdev;
	vctx->video = &test_ctx.video;
	device->instance = 0;
	device->pdev = &test_ctx.pdev;
	platform_set_drvdata(device->pdev, &test_ctx.core);

	/* already open */
	set_bit(IS_SUBDEV_OPEN, &test_ctx.subdev.state);
	ret = is_ischain_subdev_open(device, vctx);
	KUNIT_EXPECT_EQ(test, -EPERM, ret);
	clear_bit(IS_SUBDEV_OPEN, &test_ctx.subdev.state);

	/* is_ischain_open_wrap is fail */
	set_bit(IS_ISCHAIN_CLOSING, &device->state);
	ret = is_ischain_subdev_open(device, vctx);
	KUNIT_EXPECT_EQ(test, -EPERM, ret);
	clear_bit(IS_ISCHAIN_CLOSING, &device->state);

	/* subdev is NULL */
	vctx->subdev = NULL;
	ret = is_ischain_subdev_close(device, vctx);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
	vctx->subdev = &test_ctx.subdev;

	/* subdev is already close */
	clear_bit(IS_SUBDEV_OPEN, &test_ctx.subdev.state);
	ret = is_ischain_subdev_close(device, vctx);
	KUNIT_EXPECT_EQ(test, -ENOENT, ret);

	/* is_ischain_close_wrap is fail */
	vctx->subdev = &test_ctx.subdev;
	set_bit(IS_SUBDEV_OPEN, &test_ctx.subdev.state);
	set_bit(IS_ISCHAIN_OPENING, &device->state);
	ret = is_ischain_subdev_close(device, vctx);
	KUNIT_EXPECT_EQ(test, -ENOENT, ret);
}

static void __set_subdev_test_vector(struct is_subdev *subdev)
{
	set_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);

	subdev->output.width = 1920;
	subdev->output.height = 1080;
	subdev->output.crop.x = 0;
	subdev->output.crop.y = 0;
	subdev->output.crop.w = subdev->output.width;
	subdev->output.crop.h = subdev->output.height;
	subdev->bits_per_pixel = 10;
	subdev->buffer_num = 4;
	subdev->batch_num = 1;
	subdev->memory_bitwidth = 12;
	subdev->sbwc_type = NONE;
	subdev->vid = IS_VIDEO_SS0_NUM;
	snprintf(subdev->data_type, sizeof(subdev->data_type), "%s", "UNIT_TEST");
	set_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state);
}

static void pablo_subdev_ctrl_internal_start_kunit_test(struct kunit *test)
{
	int ret;
	struct is_subdev *subdev = &test_ctx.subdev;

	__set_subdev_test_vector(subdev);

	ret = is_subdev_internal_start(subdev);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = is_subdev_internal_stop(subdev);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_subdev_ctrl_internal_start_negative_kunit_test(struct kunit *test)
{
	int ret;
	struct is_subdev *subdev = &test_ctx.subdev;

	clear_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);

	ret = is_subdev_internal_start(subdev);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	set_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);
	clear_bit(IS_SUBDEV_INTERNAL_S_FMT, &subdev->state);

	ret = is_subdev_internal_start(subdev);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	/* sbwc type test */
	/* wrong internal subdev buffer size */
	subdev->sbwc_type = COMP;
	ret = is_subdev_internal_start(subdev);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	subdev->sbwc_type = COMP_LOSS;
	ret = is_subdev_internal_start(subdev);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
}

static void pablo_subdev_ctrl_internal_stop_negative_kunit_test(struct kunit *test)
{
	int ret;
	struct is_subdev *subdev = &test_ctx.subdev;

	clear_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);

	ret = is_subdev_internal_stop(subdev);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	set_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);
	clear_bit(IS_SUBDEV_START, &subdev->state);

	ret = is_subdev_internal_stop(subdev);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
}

static void pablo_subdev_ctrl_buffer_init_kunit_test(struct kunit *test)
{
	int ret;
	struct is_subdev *subdev = &test_ctx.subdev;
	struct is_video_ctx *vctx = &test_ctx.vctx;
	struct vb2_buffer vb = { 0, };

	vctx->device = &test_ctx.device_i;
	vctx->queue.framemgr.num_frames = 4;
	subdev->vctx = vctx;

	ret = pablo_subdev_buffer_init(subdev, &vb);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_subdev_ctrl_buffer_init_negative_kunit_test(struct kunit *test)
{
	int ret;
	struct is_subdev *subdev = &test_ctx.subdev;
	struct is_video_ctx *vctx = &test_ctx.vctx;
	struct vb2_buffer vb = { 0, };

	vctx->device = &test_ctx.device_i;
	vctx->queue.framemgr.num_frames = 4;
	subdev->vctx = vctx;

	/* out of range */
	vb.index = 4;
	ret = pablo_subdev_buffer_init(subdev, &vb);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
}

static void pablo_subdev_ctrl_buffer_queue_kunit_test(struct kunit *test)
{
	int ret;
	struct is_subdev *subdev = &test_ctx.subdev;
	struct is_video_ctx *vctx = &test_ctx.vctx;
	struct vb2_buffer vb = { 0, };
	struct is_frame *frame;

	vctx->device = &test_ctx.device_i;
	vctx->queue.framemgr.num_frames = 4;
	vctx->queue.framemgr.frames = kunit_kzalloc(test, array_size(sizeof(struct is_frame),
				vctx->queue.framemgr.num_frames), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, vctx->queue.framemgr.frames);

	subdev->vctx = vctx;

	frame = &vctx->queue.framemgr.frames[vb.index];
	set_bit(FRAME_MEM_INIT, &frame->mem_state);
	frame->state = FS_FREE;

	ret = is_subdev_buffer_queue(subdev, &vb);
	KUNIT_EXPECT_EQ(test, 0, ret);

	kunit_kfree(test, vctx->queue.framemgr.frames);
}

static void pablo_subdev_ctrl_buffer_queue_negative_kunit_test(struct kunit *test)
{
	int ret;
	struct is_subdev *subdev = &test_ctx.subdev;
	struct is_video_ctx *vctx = &test_ctx.vctx;
	struct vb2_buffer vb = { 0, };
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	int i;

	vctx->device = &test_ctx.device_i;
	framemgr = &vctx->queue.framemgr;
	framemgr->num_frames = 4;
	framemgr->frames = kunit_kzalloc(test, array_size(sizeof(struct is_frame), framemgr->num_frames), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, framemgr->frames);

	subdev->vctx = vctx;

	frame = &framemgr->frames[vb.index];

	/* frame is not init */
	ret = is_subdev_buffer_queue(subdev, &vb);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	set_bit(FRAME_MEM_INIT, &frame->mem_state);

	/* frame is invalid state */
	frame->state = FS_REQUEST;
	for (i = 0; i < NR_FRAME_STATE; i++) {
		framemgr->queued_count[i] = 0;
		INIT_LIST_HEAD(&framemgr->queued_list[i]);
	}
	ret = is_subdev_buffer_queue(subdev, &vb);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	kunit_kfree(test, framemgr->frames);
}

static void pablo_subdev_ctrl_buffer_finish_kunit_test(struct kunit *test)
{
	int ret;
	struct is_subdev *subdev = &test_ctx.subdev;
	struct is_video_ctx *vctx = &test_ctx.vctx;
	struct vb2_buffer vb = { 0, };
	struct is_frame *frame;

	vctx->device = &test_ctx.device_i;
	vctx->queue.framemgr.num_frames = 4;
	vctx->queue.framemgr.frames = kunit_kzalloc(test, array_size(sizeof(struct is_frame),
				vctx->queue.framemgr.num_frames), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, vctx->queue.framemgr.frames);

	subdev->vctx = vctx;
	set_bit(IS_SUBDEV_OPEN, &subdev->state);

	frame = &vctx->queue.framemgr.frames[vb.index];
	frame->state = FS_COMPLETE;

	ret = is_subdev_buffer_finish(subdev, &vb);
	KUNIT_EXPECT_EQ(test, 0, ret);

	kunit_kfree(test, vctx->queue.framemgr.frames);
}

static void pablo_subdev_ctrl_buffer_finish_negative_kunit_test(struct kunit *test)
{
	int ret;
	struct is_subdev *subdev = &test_ctx.subdev;
	struct is_video_ctx *vctx = &test_ctx.vctx;
	struct vb2_buffer vb = { 0, };
	struct is_framemgr *framemgr;
	struct is_frame *frame;
	int i;

	vctx->device = &test_ctx.device_i;
	framemgr = &vctx->queue.framemgr;
	framemgr->num_frames = 4;
	framemgr->frames = kunit_kzalloc(test, array_size(sizeof(struct is_frame), framemgr->num_frames), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, framemgr->frames);

	subdev->vctx = vctx;

	frame = &framemgr->frames[vb.index];

	/* subdev is NULL */
	ret = is_subdev_buffer_finish(NULL, &vb);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	/* subdev was clsed */
	ret = is_subdev_buffer_finish(subdev, &vb);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	set_bit(IS_SUBDEV_OPEN, &subdev->state);

	/* frame is not com state */
	frame->state = FS_FREE;
	for (i = 0; i < NR_FRAME_STATE; i++) {
		framemgr->queued_count[i] = 0;
		INIT_LIST_HEAD(&framemgr->queued_list[i]);
	}
	ret = is_subdev_buffer_finish(subdev, &vb);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	kunit_kfree(test, vctx->queue.framemgr.frames);
}

static void pablo_subdev_ctrl_sensor_subdev_qops_kunit_test(struct kunit *test)
{
	int ret;
	struct is_queue_ops *ops = is_get_sensor_subdev_qops();
	struct is_subdev *subdev = &test_ctx.subdev;
	struct is_device_sensor *device = &test_ctx.device_s;
	struct is_video_ctx *vctx = &test_ctx.vctx;
	struct is_queue *queue = &vctx->queue;
	struct is_fmt format = { 0, };

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->start_streaming);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->stop_streaming);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->s_fmt);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->reqbufs);

	vctx->subdev = subdev;
	subdev->vctx = vctx;

	/* start streaming */
	set_bit(IS_SENSOR_S_INPUT, &device->state);
	KUNIT_EXPECT_EQ(test, 0, test_bit(IS_SUBDEV_START, &subdev->state));

	ret = ops->start_streaming(device, queue);
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 1, test_bit(IS_SUBDEV_START, &subdev->state));

	/* stop streaming */
	ret = ops->stop_streaming(device, queue);
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0, test_bit(IS_SUBDEV_START, &subdev->state));

	/* s_fmt */
	queue->framecfg.format = &format;

	ret = ops->s_fmt(device, queue);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* reqbufs */
	vctx->queue.framemgr.num_frames = 4;
	vctx->queue.framemgr.frames = kunit_kzalloc(test, array_size(sizeof(struct is_frame),
				vctx->queue.framemgr.num_frames), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, vctx->queue.framemgr.frames);

	ret = ops->reqbufs(device, queue, vctx->queue.framemgr.num_frames);
	KUNIT_EXPECT_EQ(test, 0, ret);

	kunit_kfree(test, vctx->queue.framemgr.frames);
}

static void pablo_subdev_ctrl_sensor_subdev_qops_negative_kunit_test(struct kunit *test)
{
	int ret;
	struct is_queue_ops *ops = is_get_sensor_subdev_qops();
	struct is_subdev *subdev = &test_ctx.subdev;
	struct is_device_sensor *device = &test_ctx.device_s;
	struct is_video_ctx *vctx = &test_ctx.vctx;
	struct is_queue *queue = &vctx->queue;
	struct is_fmt format = { 0, };

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->start_streaming);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->stop_streaming);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->s_fmt);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->reqbufs);

	subdev->vctx = vctx;

	/* start streaming */

	/* subdev is null */
	ret = ops->start_streaming(device, queue);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	vctx->subdev = subdev;

	/* device is not yet init */
	ret = ops->start_streaming(device, queue);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	set_bit(IS_SENSOR_S_INPUT, &device->state);

	/* already start */
	set_bit(IS_SUBDEV_START, &subdev->state);
	ret = ops->start_streaming(device, queue);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* stop streaming */

	/* subdev is null */
	vctx->subdev = NULL;
	ret = ops->stop_streaming(device, queue);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	vctx->subdev = subdev;

	/* already stop */
	clear_bit(IS_SUBDEV_START, &subdev->state);
	ret = ops->stop_streaming(device, queue);
	KUNIT_EXPECT_EQ(test, 0, ret);
	set_bit(IS_SUBDEV_START, &subdev->state);

	/* framemgr is NULL */
	subdev->vctx = NULL;
	ret = ops->stop_streaming(device, queue);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	subdev->vctx = vctx;

	/* s_fmt */

	queue->framecfg.format = &format;

	/* subdev is null */
	vctx->subdev = NULL;
	ret = ops->s_fmt(device, queue);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	vctx->subdev = subdev;

	/* It is shareing with internal use */
	set_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);
	ret = ops->s_fmt(device, queue);
	KUNIT_EXPECT_EQ(test, 0, ret);
	clear_bit(IS_SUBDEV_INTERNAL_USE, &subdev->state);

	/* subdev s_format is fail */
	queue->framecfg.format = NULL;
	ret = ops->s_fmt(device, queue);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	/* reqbufs */

	/* subdev is null */
	vctx->subdev = NULL;
	ret = ops->reqbufs(device, queue, vctx->queue.framemgr.num_frames);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	vctx->subdev = subdev;

	/* framemgr is NULL */
	subdev->vctx = NULL;
	ret = ops->reqbufs(device, queue, vctx->queue.framemgr.num_frames);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);
}

static void pablo_subdev_ctrl_ischain_subdev_qops_kunit_test(struct kunit *test)
{
	int ret;
	struct is_queue_ops *ops = is_get_ischain_subdev_qops();
	struct is_subdev *subdev = &test_ctx.subdev;
	struct is_subdev leader = { 0, };
	struct is_device_ischain *device = &test_ctx.device_i;
	struct is_video_ctx *vctx = &test_ctx.vctx;
	struct is_queue *queue = &vctx->queue;
	struct is_fmt format = { 0, };

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->start_streaming);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->stop_streaming);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops->s_fmt);
	KUNIT_EXPECT_PTR_EQ(test, (void *)ops->reqbufs, NULL);

	vctx->subdev = subdev;
	subdev->vctx = vctx;
	subdev->leader = &leader;

	/* start streaming */
	set_bit(IS_ISCHAIN_INIT, &device->state);

	ret = ops->start_streaming(device, queue);
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 1, test_bit(IS_SUBDEV_START, &subdev->state));

	/* stop streaming */
	ret = ops->stop_streaming(device, queue);
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, 0, test_bit(IS_SUBDEV_START, &subdev->state));

	/* s_fmt */
	queue->framecfg.format = &format;

	ret = ops->s_fmt(device, queue);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_subdev_ctrl_internal_g_bpp_kunit_test(struct kunit *test)
{
	int ret;
	struct is_subdev *subdev = &test_ctx.subdev;
	struct is_sensor_cfg cfg = { 0, };

	/* ENTRY_YPP */
	subdev->id = ENTRY_YPP;
	ret = is_subdev_internal_g_bpp(subdev, NULL);
	KUNIT_EXPECT_EQ(test, 10, ret);

	/* ENTRY_MCS */
	subdev->id = ENTRY_MCS;
	ret = is_subdev_internal_g_bpp(subdev, NULL);
	KUNIT_EXPECT_EQ(test, 10, ret);

	/* ENTRY_PAF */
	subdev->id = ENTRY_PAF;
	ret = is_subdev_internal_g_bpp(subdev, NULL);
	KUNIT_EXPECT_EQ(test, 16, ret);

	ret = is_subdev_internal_g_bpp(subdev, &cfg);
	KUNIT_EXPECT_EQ(test, 16, ret);

	cfg.output[CSI_VIRTUAL_CH_0].hwformat = HW_FORMAT_RAW10;
	ret = is_subdev_internal_g_bpp(subdev, &cfg);
	KUNIT_EXPECT_EQ(test, 10, ret);

	cfg.output[CSI_VIRTUAL_CH_0].hwformat = HW_FORMAT_RAW12;
	ret = is_subdev_internal_g_bpp(subdev, &cfg);
	KUNIT_EXPECT_EQ(test, 12, ret);

	/* default */
	subdev->id = 0;
	ret = is_subdev_internal_g_bpp(subdev, NULL);
	KUNIT_EXPECT_EQ(test, 16, ret);
}

static struct kunit_case pablo_subdev_ctrl_kunit_test_cases[] = {
	KUNIT_CASE(pablo_subdev_ctrl_probe_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_sensor_subdev_open_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_sensor_subdev_open_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_ischain_subdev_open_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_ischain_subdev_open_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_internal_start_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_internal_start_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_internal_stop_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_buffer_init_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_buffer_init_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_buffer_queue_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_buffer_queue_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_buffer_finish_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_buffer_finish_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_sensor_subdev_qops_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_sensor_subdev_qops_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_ischain_subdev_qops_kunit_test),
	KUNIT_CASE(pablo_subdev_ctrl_internal_g_bpp_kunit_test),
	{},
};

static int pablo_subdev_ctrl_kunit_test_init(struct kunit *test)
{
	memset(&test_ctx, 0, sizeof(test_ctx));

	return 0;
}

static void pablo_subdev_ctrl_kunit_test_exit(struct kunit *test)
{
}

struct kunit_suite pablo_subdev_ctrl_kunit_test_suite = {
	.name = "pablo-subdev-ctrl-kunit-test",
	.init = pablo_subdev_ctrl_kunit_test_init,
	.exit = pablo_subdev_ctrl_kunit_test_exit,
	.test_cases = pablo_subdev_ctrl_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_subdev_ctrl_kunit_test_suite);

MODULE_LICENSE("GPL");
