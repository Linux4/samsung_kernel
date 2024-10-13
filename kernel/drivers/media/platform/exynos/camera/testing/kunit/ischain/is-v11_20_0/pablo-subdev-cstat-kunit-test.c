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
#include "is-device-sensor.h"

const struct is_subdev_ops *pablo_get_is_subdev_cstat_ops(void);

static struct test_ctx {
	struct is_video_ctx vctx;
	struct is_group group;
	struct is_device_ischain idi;
	struct is_device_sensor ids;
	struct is_region is_region;
	struct is_frame frame;
	struct camera2_shot_ext	shot_ext;
	struct is_param_region parameter;
	struct is_sensor_cfg sensor_cfg;
	const struct is_subdev_ops *ops;
} test_ctx;

#define KUNIT_TEST_SIZE_WIDTH 1920
#define KUNIT_TEST_SIZE_HEIGHT 1080

static void __set_test_vector_cropRegion(u32 *dst)
{
	dst[0] = 0;
	dst[1] = 0;
	dst[2] = KUNIT_TEST_SIZE_WIDTH;
	dst[3] = KUNIT_TEST_SIZE_HEIGHT;
}

static void __clear_test_vector_cropRegion(u32 *dst)
{
	dst[0] = 0;
	dst[1] = 0;
	dst[2] = 0;
	dst[3] = 0;
}

static enum is_video_dev_num _test_vector[] = {
	/* dma in */
	IS_VIDEO_CSTAT0,
	IS_VIDEO_CSTAT1,
	IS_VIDEO_CSTAT2,
	/* dma out */
	IS_LVN_CSTAT0_LME_DS0,
	IS_LVN_CSTAT0_LME_DS1,
	IS_LVN_CSTAT0_FDPIG,
	IS_LVN_CSTAT0_DRC,
	IS_LVN_CSTAT0_CDS,
	0,
};

static void pablo_subdev_cstat_tag_dma_kunit_test(struct kunit *test)
{
	int ret, k;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_3AA]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *l_node = &frame->shot_ext->node_group.leader;
	struct camera2_node *cap_node;

	for (k = 0; k < sizeof(_test_vector)/sizeof(enum is_video_dev_num); k++) {
		cap_node = &frame->shot_ext->node_group.capture[k];
		cap_node->vid = _test_vector[k];
		__set_test_vector_cropRegion(cap_node->input.cropRegion);
		__set_test_vector_cropRegion(cap_node->output.cropRegion);
		cap_node->pixelformat = V4L2_PIX_FMT_NV21;
		cap_node->request = 0;

		frame->cap_node.sframe[k].id = _test_vector[k];
		frame->cap_node.sframe[k].num_planes = 1;
	}

	/* strip processing */
	frame->stripe_info.region_num = 2;
	ret = ops->tag(subdev, idi, frame, l_node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* dma in */
	/* request is not set */
	l_node->request = 0;
	ret = ops->tag(subdev, idi, frame, l_node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	l_node->request = 1;

	/* crop is NULL */
	__clear_test_vector_cropRegion(l_node->input.cropRegion);
	__clear_test_vector_cropRegion(l_node->output.cropRegion);
	ret = ops->tag(subdev, idi, frame, l_node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	__set_test_vector_cropRegion(l_node->input.cropRegion);
	__set_test_vector_cropRegion(l_node->output.cropRegion);

	/* pixelformat is not found */
	l_node->pixelformat = 0;
	ret = ops->tag(subdev, idi, frame, l_node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	l_node->pixelformat = V4L2_PIX_FMT_NV21;

	/* dma out */
	cap_node = &frame->shot_ext->node_group.capture[3]; /* LME_DS0 */

	/* request is not set */
	ret = ops->tag(subdev, idi, frame, l_node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cap_node->request = 1;

	/* in crop is NULL */
	__clear_test_vector_cropRegion(cap_node->input.cropRegion);
	subdev->input.width = 320;
	subdev->input.height = 240;
	ret = ops->tag(subdev, idi, frame, l_node);
	KUNIT_EXPECT_EQ(test, cap_node->input.cropRegion[2], subdev->input.width);
	KUNIT_EXPECT_EQ(test, cap_node->input.cropRegion[3], subdev->input.height);

	__set_test_vector_cropRegion(cap_node->input.cropRegion);

	/* out crop is NULL */
	__clear_test_vector_cropRegion(cap_node->output.cropRegion);
	ret = ops->tag(subdev, idi, frame, l_node);
	KUNIT_EXPECT_EQ(test, cap_node->output.cropRegion[2], subdev->input.width);
	KUNIT_EXPECT_EQ(test, cap_node->output.cropRegion[3], subdev->input.height);

	__set_test_vector_cropRegion(cap_node->output.cropRegion);

	ret = ops->tag(subdev, idi, frame, l_node);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_subdev_cstat_tag_otf_kunit_test(struct kunit *test)
{
	int ret, k;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_3AA]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;

	/* otf in */
	set_bit(IS_GROUP_OTF_INPUT, &idi->group[GROUP_SLOT_3AA]->state);
	set_bit(IS_GROUP_START, &idi->group[GROUP_SLOT_3AA]->state);

	/* incrop is NULL */
	for (k = _test_vector[0]; k <= _test_vector[2]; k++) {
		subdev->vid = k;
		node->vid = k;
		__clear_test_vector_cropRegion(node->input.cropRegion);
		ret = ops->tag(subdev, idi, frame, node);
		KUNIT_EXPECT_EQ(test, ret, -EINVAL);

		__set_test_vector_cropRegion(node->input.cropRegion);

		ret = ops->tag(subdev, idi, frame, node);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}
}


static void pablo_subdev_cstat_tag_kunit_test(struct kunit *test)
{
	int ret, k;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_3AA]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;
	struct camera2_node *cap_node;

	for (k = 0; k < sizeof(_test_vector)/sizeof(enum is_video_dev_num); k++) {
		cap_node = &frame->shot_ext->node_group.capture[k];
		cap_node->vid = _test_vector[k];
		__set_test_vector_cropRegion(cap_node->input.cropRegion);
		__set_test_vector_cropRegion(cap_node->output.cropRegion);
		cap_node->pixelformat = V4L2_PIX_FMT_NV21;
		cap_node->request = 1;

		frame->cap_node.sframe[k].id = _test_vector[k];
		frame->cap_node.sframe[k].num_planes = 1;
	}

	set_bit(IS_GROUP_OTF_INPUT, &idi->group[GROUP_SLOT_3AA]->state);

	frame->stripe_info.region_num = 0;
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* otf_in_cfg is failed */
	__clear_test_vector_cropRegion(node->input.cropRegion);
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	__set_test_vector_cropRegion(node->input.cropRegion);

	/* cap_node dma is failed */
	cap_node = &frame->shot_ext->node_group.capture[3]; /* LME_DS0 */
	cap_node->pixelformat = 0;
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	cap_node->pixelformat = V4L2_PIX_FMT_NV21;
}

static int pablo_subdev_cstat_kunit_test_init(struct kunit *test)
{
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev;
	struct is_device_sensor *ids = &test_ctx.ids;
	struct is_frame *frame = &test_ctx.frame;
	struct is_sensor_cfg *cfg = &test_ctx.sensor_cfg;
	struct camera2_node *node;

	test_ctx.ops = pablo_get_is_subdev_cstat_ops();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.ops);

	test_ctx.vctx.group = &test_ctx.group;
	frame->shot_ext = &test_ctx.shot_ext;
	frame->shot = &test_ctx.shot_ext.shot;
	frame->parameter = &test_ctx.parameter;
	ids->image.window.width = KUNIT_TEST_SIZE_WIDTH;
	ids->image.window.height = KUNIT_TEST_SIZE_HEIGHT;
	ids->cfg = cfg;
	idi->sensor = ids;
	idi->is_region = &test_ctx.is_region;
	test_ctx.group.next = &test_ctx.group;
	idi->group[GROUP_SLOT_3AA] = &test_ctx.group;
	subdev = &idi->group[GROUP_SLOT_3AA]->leader;
	subdev->vid = IS_VIDEO_CSTAT0;
	subdev->leader = subdev;
	subdev->vctx = &test_ctx.vctx;
	node = &test_ctx.frame.shot_ext->node_group.leader;

	node->vid = IS_VIDEO_CSTAT0;
	node->request = 0;
	node->pixelformat = V4L2_PIX_FMT_NV21;
	__set_test_vector_cropRegion(node->input.cropRegion);
	__set_test_vector_cropRegion(node->output.cropRegion);

	return 0;
}

static void pablo_subdev_cstat_kunit_test_exit(struct kunit *test)
{
	struct camera2_node *node;

	node = &test_ctx.frame.shot_ext->node_group.leader;

	__clear_test_vector_cropRegion(node->input.cropRegion);
	__clear_test_vector_cropRegion(node->output.cropRegion);
}

static struct kunit_case pablo_subdev_cstat_kunit_test_cases[] = {
	KUNIT_CASE(pablo_subdev_cstat_tag_dma_kunit_test),
	KUNIT_CASE(pablo_subdev_cstat_tag_otf_kunit_test),
	KUNIT_CASE(pablo_subdev_cstat_tag_kunit_test),
	{},
};

struct kunit_suite pablo_subdev_cstat_kunit_test_suite = {
	.name = "pablo-subdev-cstat-kunit-test",
	.init = pablo_subdev_cstat_kunit_test_init,
	.exit = pablo_subdev_cstat_kunit_test_exit,
	.test_cases = pablo_subdev_cstat_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_subdev_cstat_kunit_test_suite);

MODULE_LICENSE("GPL");
