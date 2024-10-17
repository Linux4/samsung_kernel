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
#include "is-core.h"
#include "is-hw-chain.h"

const struct is_subdev_ops *pablo_get_is_subdev_lme_ops(void);

static struct test_ctx {
	struct is_video_ctx vctx;
	struct is_group group;
	struct is_device_ischain idi;
	struct is_region is_region;
	struct is_frame frame;
	struct camera2_shot_ext	shot_ext;
	struct is_param_region parameter;
	const struct is_subdev_ops *ops;
} test_ctx;

static void __set_test_vector_cropRegion(u32 *dst)
{
	dst[0] = 0;
	dst[1] = 0;
	dst[2] = 672;
	dst[3] = 504;
}

static void __clear_test_vector_cropRegion(u32 *dst)
{
	dst[0] = 0;
	dst[1] = 0;
	dst[2] = 0;
	dst[3] = 0;
}

static int pablo_subdev_lme_kunit_test_init(struct kunit *test)
{
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node;

	test_ctx.ops = pablo_get_is_subdev_lme_ops();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.ops);

	frame->shot_ext = &test_ctx.shot_ext;
	frame->shot = &test_ctx.shot_ext.shot;
	frame->parameter = &test_ctx.parameter;
	idi->is_region = &test_ctx.is_region;
	idi->group[GROUP_SLOT_LME] = &test_ctx.group;
	subdev = &idi->group[GROUP_SLOT_LME]->leader;
	subdev->leader = subdev;
	subdev->vctx = &test_ctx.vctx;
	node = &test_ctx.frame.shot_ext->node_group.leader;

	node->request = 1;
	node->pixelformat = V4L2_PIX_FMT_NV21;
	__set_test_vector_cropRegion(node->input.cropRegion);
	__set_test_vector_cropRegion(node->output.cropRegion);

	return 0;
}

static void pablo_subdev_lme_kunit_test_exit(struct kunit *test)
{
	struct camera2_node *node;

	node = &test_ctx.frame.shot_ext->node_group.leader;

	__clear_test_vector_cropRegion(node->input.cropRegion);
	__clear_test_vector_cropRegion(node->output.cropRegion);
}

static void pablo_subdev_lme_tag_dma_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_LME]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;
	struct is_group *group;

	node->vid = IS_VIDEO_LME;
	group = idi->group[GROUP_SLOT_LME];
	clear_bit(IS_GROUP_START, &group->state);

	/* dma in */
	frame->shot_ext->node_group.capture[0].vid = IS_VIDEO_LME;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[0].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[0].output.cropRegion);
	frame->shot_ext->node_group.capture[0].pixelformat = V4L2_PIX_FMT_NV21;

	/* dma out */
	frame->shot_ext->node_group.capture[1].vid = IS_LVN_LME_PREV;
	frame->shot_ext->node_group.capture[1].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[1].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[1].output.cropRegion);
	frame->shot_ext->node_group.capture[1].pixelformat = V4L2_PIX_FMT_NV21;

	frame->shot_ext->node_group.capture[2].vid = IS_LVN_LME_PURE;
	frame->shot_ext->node_group.capture[2].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[2].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[2].output.cropRegion);
	frame->shot_ext->node_group.capture[2].pixelformat = V4L2_PIX_FMT_NV21;

	frame->shot_ext->node_group.capture[3].vid = IS_LVN_LME_SAD;
	frame->shot_ext->node_group.capture[3].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[3].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[3].output.cropRegion);
	frame->shot_ext->node_group.capture[3].pixelformat = V4L2_PIX_FMT_NV21;

	frame->shot_ext->node_group.capture[4].vid = IS_LVN_LME_RTA_INFO;
	frame->shot_ext->node_group.capture[4].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[4].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[4].output.cropRegion);
	frame->shot_ext->node_group.capture[4].pixelformat = V4L2_PIX_FMT_NV21;

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(IS_GROUP_START, &group->state);

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_subdev_lme_tag_dma_negative_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_LME]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;

	node->vid = IS_VIDEO_LME;

	/* capture node vid is invalid */
	frame->shot_ext->node_group.capture[0].vid = IS_VIDEO_LME1S_NUM;
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	frame->shot_ext->node_group.capture[0].vid = IS_LVN_LME_PREV;

	/* pixelformat is NULL */
	node->pixelformat = 0;
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	node->pixelformat = V4L2_PIX_FMT_NV21;

	/* node request is NULL */
	node->request = 0;
	node->vid = IS_LVN_LME_PURE;
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);
	node->vid = IS_VIDEO_LME;
	node->request = 1;

	/* output node vid is invalid */
	node->vid = IS_VIDEO_LME1_NUM;
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	node->vid = IS_VIDEO_LME;
}

static void pablo_subdev_lme_tag_dma_negative_crop_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_LME]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;

	node->vid = IS_VIDEO_LME;

	/* input crop is NULL */
	__clear_test_vector_cropRegion(node->input.cropRegion);
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	__set_test_vector_cropRegion(node->input.cropRegion);

	/* output crop is NULL */
	__clear_test_vector_cropRegion(node->output.cropRegion);
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	__set_test_vector_cropRegion(node->output.cropRegion);
}

static void pablo_subdev_lme_tag_sframe_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_LME]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;

	frame->cap_node.sframe[0].id = IS_LVN_LME_PURE;
	frame->cap_node.sframe[0].num_planes = 1;

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static struct kunit_case pablo_subdev_lme_kunit_test_cases[] = {
	KUNIT_CASE(pablo_subdev_lme_tag_dma_kunit_test),
	KUNIT_CASE(pablo_subdev_lme_tag_dma_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_lme_tag_dma_negative_crop_kunit_test),
	KUNIT_CASE(pablo_subdev_lme_tag_sframe_kunit_test),
	{},
};

struct kunit_suite pablo_subdev_lme_kunit_test_suite = {
	.name = "pablo-subdev-lme-kunit-test",
	.init = pablo_subdev_lme_kunit_test_init,
	.exit = pablo_subdev_lme_kunit_test_exit,
	.test_cases = pablo_subdev_lme_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_subdev_lme_kunit_test_suite);

MODULE_LICENSE("GPL v2");
