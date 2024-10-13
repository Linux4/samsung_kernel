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
#include "is-subdev-ctrl.h"
#include "is-device-sensor.h"

const struct is_subdev_ops *pablo_get_is_subdev_sensor_ops(void);
static struct test_ctx {
	struct is_subdev subdev;
	struct is_video_ctx vctx;
	struct is_device_sensor ids;
	struct is_frame frame;
	struct camera2_shot_ext	shot_ext;
	struct is_param_region parameter;
} test_ctx;

static void __set_test_vector_cropRegion(u32 *dst)
{
	dst[0] = 1920;
	dst[1] = 1080;
	dst[2] = 1920;
	dst[3] = 1080;
}

static void pablo_subdev_sensor_tag_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = pablo_get_is_subdev_sensor_ops();
	struct is_subdev *subdev = &test_ctx.subdev;
	struct is_device_sensor *ids = &test_ctx.ids;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = NULL;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops);

	frame->shot_ext = &test_ctx.shot_ext;
	frame->parameter = &test_ctx.parameter;
	subdev->leader = subdev;
	subdev->vctx = &test_ctx.vctx;

	/* dma */
	frame->shot_ext->node_group.capture[0].vid = IS_LVN_SS0_VC0;
	frame->shot_ext->node_group.capture[0].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[0].output.cropRegion);
	frame->shot_ext->node_group.capture[0].pixelformat = V4L2_PIX_FMT_SBGGR10P;
	frame->cap_node.sframe[0].id = IS_LVN_SS0_VC0;

	ret = ops->tag(subdev, ids, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_subdev_sensor_tag_negative_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = pablo_get_is_subdev_sensor_ops();
	struct is_subdev *subdev = &test_ctx.subdev;
	struct is_device_sensor *ids = &test_ctx.ids;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = NULL;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops);

	frame->shot_ext = &test_ctx.shot_ext;
	frame->parameter = &test_ctx.parameter;
	subdev->leader = subdev;
	subdev->vctx = &test_ctx.vctx;

	/* frame is null */
	ret = ops->tag(subdev, ids, NULL, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	frame->shot_ext->node_group.capture[0].vid = IS_LVN_SS0_VC0;

	/* request is not set */
	ret = ops->tag(subdev, ids, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	frame->shot_ext->node_group.capture[0].request = 1;

	/* crop region is NULL */
	ret = ops->tag(subdev, ids, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[0].output.cropRegion);

	/* pixel format is not found */
	ret = ops->tag(subdev, ids, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_subdev_sensor_tag_seamless_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = pablo_get_is_subdev_sensor_ops();
	struct is_subdev *subdev = &test_ctx.subdev;
	struct is_device_sensor *ids = &test_ctx.ids;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = NULL;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops);

	frame->shot_ext = &test_ctx.shot_ext;
	frame->parameter = &test_ctx.parameter;
	subdev->leader = subdev;
	subdev->vctx = &test_ctx.vctx;

	frame->shot_ext->node_group.capture[0].vid = IS_LVN_SS0_VC0;
	frame->shot_ext->node_group.capture[0].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[0].output.cropRegion);
	frame->shot_ext->node_group.capture[0].pixelformat = V4L2_PIX_FMT_SBGGR10P;
	frame->cap_node.sframe[0].id = IS_LVN_SS0_VC0;

	frame->shot_ext->node_group.capture[1].vid = IS_LVN_SS0_VC1;
	frame->shot_ext->node_group.capture[1].request = 1;
	frame->shot_ext->node_group.capture[1].pixelformat = V4L2_PIX_FMT_SBGGR10P;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[1].output.cropRegion);
	frame->cap_node.sframe[1].id = IS_LVN_SS0_VC1;

	/* seamless mode: 2exp mode and switching*/
	ids->aeb_state = BIT(IS_SENSOR_2EXP_MODE) | BIT(IS_SENSOR_AEB_SWITCHING);
	ret = ops->tag(subdev, ids, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* seamless mode: single mode */
	ids->aeb_state = BIT(IS_SENSOR_SINGLE_MODE);
	ret = ops->tag(subdev, ids, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* seamless mode: single and switching */
	ids->aeb_state = BIT(IS_SENSOR_SINGLE_MODE) | BIT(IS_SENSOR_AEB_SWITCHING);
	ret = ops->tag(subdev, ids, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static struct kunit_case pablo_subdev_sensor_kunit_test_cases[] = {
	KUNIT_CASE(pablo_subdev_sensor_tag_kunit_test),
	KUNIT_CASE(pablo_subdev_sensor_tag_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_sensor_tag_seamless_kunit_test),
	{},
};

static int pablo_subdev_sensor_kunit_test_init(struct kunit *test)
{
	memset(&test_ctx, 0, sizeof(test_ctx));

	test_ctx.vctx.queue = kunit_kzalloc(test, sizeof(struct is_queue), 0);
	spin_lock_init(&test_ctx.vctx.queue->framemgr.slock);

	return 0;
}

static void pablo_subdev_sensor_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.vctx.queue);
}

struct kunit_suite pablo_subdev_sensor_kunit_test_suite = {
	.name = "pablo-subdev-sensor-kunit-test",
	.init = pablo_subdev_sensor_kunit_test_init,
	.exit = pablo_subdev_sensor_kunit_test_exit,
	.test_cases = pablo_subdev_sensor_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_subdev_sensor_kunit_test_suite);

MODULE_LICENSE("GPL");
