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
#include "is-device-ischain.h"

const struct is_subdev_ops *pablo_get_is_subdev_rgbp_ops(void);

static struct test_ctx {
	struct is_group group;
	struct is_device_ischain idi;
	struct is_region is_region;
	struct is_frame frame;
	struct camera2_shot_ext shot_ext;
	struct is_param_region parameter;
	const struct is_subdev_ops *ops;
} test_ctx;

static void __set_test_vector_cropRegion(u32 *dst)
{
	dst[0] = 1920;
	dst[1] = 1080;
	dst[2] = 1920;
	dst[3] = 1080;
}

static void __clear_test_vector_cropRegion(u32 *dst)
{
	dst[0] = 0;
	dst[1] = 0;
	dst[2] = 0;
	dst[3] = 0;
}

static void pablo_subdev_rgbp_get_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_RGBP]->leader;
	struct is_frame *frame = &test_ctx.frame;
	enum pablo_subdev_get_type type;
	int result;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops);

	type =  PSGT_REGION_NUM;
	frame->shot_ext = &test_ctx.shot_ext;

	ret = ops->get(subdev, frame, type, &result);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_subdev_rgbp_tag_basic_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_RGBP]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops);

	frame->shot_ext = &test_ctx.shot_ext;
	frame->parameter = &test_ctx.parameter;
	node = &frame->shot_ext->node_group.leader;
	idi->is_region = &test_ctx.is_region;
	subdev->leader = subdev;

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_subdev_rgbp_tag_otf_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_RGBP]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops);

	frame->shot_ext = &test_ctx.shot_ext;
	frame->parameter = &test_ctx.parameter;
	node = &frame->shot_ext->node_group.leader;
	idi->is_region = &test_ctx.is_region;
	subdev->leader = subdev;

	/* otf in */
	set_bit(IS_GROUP_OTF_INPUT, &idi->group[GROUP_SLOT_RGBP]->state);
	__set_test_vector_cropRegion(node->input.cropRegion);

	/* otf out */
	set_bit(IS_GROUP_OTF_OUTPUT, &idi->group[GROUP_SLOT_RGBP]->state);
	__set_test_vector_cropRegion(node->output.cropRegion);
	node->pixelformat = V4L2_PIX_FMT_NV21;

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_subdev_rgbp_tag_otf_negative_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_RGBP]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops);

	frame->shot_ext = &test_ctx.shot_ext;
	frame->parameter = &test_ctx.parameter;
	node = &frame->shot_ext->node_group.leader;
	idi->is_region = &test_ctx.is_region;
	subdev->leader = subdev;

	/* otf in */
	set_bit(IS_GROUP_OTF_INPUT, &idi->group[GROUP_SLOT_RGBP]->state);
	__clear_test_vector_cropRegion(node->input.cropRegion);

	/* incrop is NULL */
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	__set_test_vector_cropRegion(node->input.cropRegion);

	/* otf out */
	set_bit(IS_GROUP_OTF_OUTPUT, &idi->group[GROUP_SLOT_RGBP]->state);
	__clear_test_vector_cropRegion(node->output.cropRegion);

	/* otcrip is NULL */
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	__set_test_vector_cropRegion(node->output.cropRegion);
}

static void pablo_subdev_rgbp_tag_dma_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_RGBP]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops);

	frame->shot_ext = &test_ctx.shot_ext;
	frame->parameter = &test_ctx.parameter;
	node = &frame->shot_ext->node_group.leader;
	idi->is_region = &test_ctx.is_region;
	subdev->leader = subdev;

	/* dma in */
	frame->shot_ext->node_group.capture[0].vid = IS_VIDEO_RGBP;
	frame->shot_ext->node_group.capture[0].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[0].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[0].output.cropRegion);
	frame->shot_ext->node_group.capture[0].pixelformat = V4L2_PIX_FMT_NV21;

	/* dma out */
	frame->shot_ext->node_group.capture[1].vid = IS_LVN_RGBP_HF;
	frame->shot_ext->node_group.capture[1].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[1].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[1].output.cropRegion);
	frame->shot_ext->node_group.capture[1].pixelformat = V4L2_PIX_FMT_NV21;

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	frame->shot_ext->node_group.capture[2].vid = IS_LVN_RGBP_SF;
	frame->shot_ext->node_group.capture[2].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[2].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[2].output.cropRegion);
	frame->shot_ext->node_group.capture[2].pixelformat = V4L2_PIX_FMT_NV21;

	frame->shot_ext->node_group.capture[3].vid = IS_LVN_RGBP_YUV;
	frame->shot_ext->node_group.capture[3].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[3].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[3].output.cropRegion);
	frame->shot_ext->node_group.capture[3].pixelformat = V4L2_PIX_FMT_NV21;

	frame->shot_ext->node_group.capture[4].vid = IS_LVN_RGBP_RGB;
	frame->shot_ext->node_group.capture[4].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[4].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[4].output.cropRegion);
	frame->shot_ext->node_group.capture[4].pixelformat = V4L2_PIX_FMT_NV21;

	//negative node
	frame->shot_ext->node_group.capture[5].vid = -1;
	frame->shot_ext->node_group.capture[5].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[5].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[5].output.cropRegion);
	frame->shot_ext->node_group.capture[5].pixelformat = V4L2_PIX_FMT_NV21;

	frame->shot_ext->node_group.capture[6].vid = IS_VIDEO_RGBP;
	frame->shot_ext->node_group.capture[6].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[6].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[6].output.cropRegion);
	frame->shot_ext->node_group.capture[6].pixelformat = V4L2_PIX_FMT_SBGGR12P;
	frame->shot_ext->node_group.capture[6].flags = 0x45;

	frame->shot_ext->node_group.capture[7].vid = IS_LVN_RGBP_SF;
	frame->shot_ext->node_group.capture[7].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[7].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[7].output.cropRegion);
	frame->shot_ext->node_group.capture[7].pixelformat = V4L2_PIX_FMT_SBGGR12P;
	frame->shot_ext->node_group.capture[7].flags = 0x45;

	frame->shot_ext->node_group.capture[8].vid = IS_VIDEO_RGBP;
	frame->shot_ext->node_group.capture[8].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[8].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[8].output.cropRegion);
	frame->shot_ext->node_group.capture[8].pixelformat = V4L2_PIX_FMT_SBGGR12;

	frame->shot_ext->node_group.capture[9].vid = IS_LVN_RGBP_SF;
	frame->shot_ext->node_group.capture[9].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[9].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[9].output.cropRegion);
	frame->shot_ext->node_group.capture[9].pixelformat = V4L2_PIX_FMT_SBGGR12;

	frame->shot_ext->node_group.capture[10].vid = IS_LVN_RGBP_RGB;
	frame->shot_ext->node_group.capture[10].request = 0;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[10].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[10].output.cropRegion);
	frame->shot_ext->node_group.capture[10].pixelformat = V4L2_PIX_FMT_NV21;

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* strip processing */
	frame->stripe_info.region_num = 2;
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_subdev_rgbp_tag_dma_negative_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_RGBP]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node;

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ops);

	frame->shot_ext = &test_ctx.shot_ext;
	frame->parameter = &test_ctx.parameter;
	node = &frame->shot_ext->node_group.leader;
	idi->is_region = &test_ctx.is_region;
	subdev->leader = subdev;

	/* dma in */
	frame->shot_ext->node_group.capture[0].vid = IS_VIDEO_RGBP;

	/* request is not set */
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	frame->shot_ext->node_group.capture[0].request = 1;
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[0].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[0].output.cropRegion);

	/* pixel format is not found */
	frame->shot_ext->node_group.capture[0].pixelformat = 0;
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	frame->shot_ext->node_group.capture[0].pixelformat = V4L2_PIX_FMT_NV21;

	/* dma out */
	frame->shot_ext->node_group.capture[1].vid = IS_LVN_RGBP_HF;
	frame->shot_ext->node_group.capture[1].request = 1;

	/* crop region is NULL */
	__clear_test_vector_cropRegion(frame->shot_ext->node_group.capture[1].input.cropRegion);
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[1].input.cropRegion);
	__set_test_vector_cropRegion(frame->shot_ext->node_group.capture[1].output.cropRegion);
}

static void pablo_subdev_rgbp_tag_sframe_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_RGBP]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;

	frame->cap_node.sframe[0].id = IS_LVN_RGBP_RGB;
	frame->cap_node.sframe[0].num_planes = 1;

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static int pablo_subdev_rgbp_kunit_test_init(struct kunit *test)
{
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node;

	test_ctx.ops = pablo_get_is_subdev_rgbp_ops();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.ops);

	frame->shot_ext = &test_ctx.shot_ext;
	frame->parameter = &test_ctx.parameter;
	idi->is_region = &test_ctx.is_region;
	idi->group[GROUP_SLOT_RGBP] = &test_ctx.group;
	subdev = &idi->group[GROUP_SLOT_RGBP]->leader;
	subdev->leader = subdev;
	node = &test_ctx.frame.shot_ext->node_group.leader;

	__set_test_vector_cropRegion(node->input.cropRegion);
	__set_test_vector_cropRegion(node->output.cropRegion);

	return 0;
}

static void pablo_subdev_rgbp_kunit_test_exit(struct kunit *test)
{
}

static struct kunit_case pablo_subdev_rgbp_kunit_test_cases[] = {
	KUNIT_CASE(pablo_subdev_rgbp_get_kunit_test),
	KUNIT_CASE(pablo_subdev_rgbp_tag_basic_kunit_test),
	KUNIT_CASE(pablo_subdev_rgbp_tag_otf_kunit_test),
	KUNIT_CASE(pablo_subdev_rgbp_tag_otf_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_rgbp_tag_dma_kunit_test),
	KUNIT_CASE(pablo_subdev_rgbp_tag_dma_negative_kunit_test),
	KUNIT_CASE(pablo_subdev_rgbp_tag_sframe_kunit_test),
	{},
};

struct kunit_suite pablo_subdev_rgbp_kunit_test_suite = {
	.name = "pablo-subdev-rgbp-kunit-test",
	.init = pablo_subdev_rgbp_kunit_test_init,
	.exit = pablo_subdev_rgbp_kunit_test_exit,
	.test_cases = pablo_subdev_rgbp_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_subdev_rgbp_kunit_test_suite);

MODULE_LICENSE("GPL");
