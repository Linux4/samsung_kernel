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
#include "is-param.h"

const struct is_subdev_ops *pablo_get_is_subdev_paf_ops(void);

static struct test_ctx {
	struct is_video_ctx vctx;
	struct is_group group;
	struct is_device_ischain idi;
	struct is_device_sensor ids;
	struct is_region is_region;
	struct is_frame frame;
	struct is_fmt format;
	struct camera2_shot_ext shot_ext;
	struct is_param_region parameter;
	const struct is_subdev_ops *ops;
} test_ctx;

static void __set_test_vector_cropRegion(u32 *dst)
{
	dst[0] = 0;
	dst[1] = 0;
	dst[2] = 1920;
	dst[3] = 1080;
}

static void __set_error_test_vector_cropRegion(u32 *dst)
{
	dst[0] = 100;
	dst[1] = 100;
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

static void pablo_subdev_paf_cfg_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_PAF]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;
	struct is_crop *incrop, *otcrop;
	unsigned long pmap[BITS_TO_LONGS(IS_PARAM_NUM)];

	incrop = (struct is_crop *)node->input.cropRegion;
	otcrop = (struct is_crop *)node->output.cropRegion;
	set_bit(IS_GROUP_OTF_INPUT, &idi->group[GROUP_SLOT_PAF]->state);
	IS_INIT_PMAP(pmap);

	/*frame is null*/
	ret = ops->cfg(subdev, idi, NULL, incrop, otcrop, pmap);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/*as group start case*/
	set_bit(IS_GROUP_START, &idi->group[GROUP_SLOT_PAF]->state);
	ret = ops->cfg(subdev, idi, NULL, incrop, otcrop, pmap);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/*group votf input*/
	set_bit(IS_GROUP_VOTF_INPUT, &idi->group[GROUP_SLOT_PAF]->state);
	ret = ops->cfg(subdev, idi, frame, incrop, otcrop, pmap);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_subdev_paf_tag_kunit_test(struct kunit *test)
{
	int ret;
	const struct is_subdev_ops *ops = test_ctx.ops;
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_PAF]->leader;
	struct is_frame *frame = &test_ctx.frame;
	struct camera2_node *node = &frame->shot_ext->node_group.leader;
	struct is_queue *queue = subdev->vctx->queue;

	set_bit(IS_GROUP_OTF_INPUT, &idi->group[GROUP_SLOT_PAF]->state);

	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* otf_in_cfg with error case1 */
	__clear_test_vector_cropRegion(node->input.cropRegion);
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* otf_in_cfg with error case2 */
	__set_error_test_vector_cropRegion(node->input.cropRegion);
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	__set_test_vector_cropRegion(node->input.cropRegion);
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/*group->state is not OTF is fail*/
	clear_bit(IS_GROUP_OTF_INPUT, &idi->group[GROUP_SLOT_PAF]->state);
	set_bit(IS_SUBDEV_FORCE_SET, &subdev->state);
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/*format is exist*/
	queue->framecfg.format = &test_ctx.format;
	ret = ops->tag(subdev, idi, frame, node);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static int pablo_subdev_paf_kunit_test_init(struct kunit *test)
{
	struct is_device_ischain *idi = &test_ctx.idi;
	struct is_device_sensor *ids = &test_ctx.ids;
	struct is_frame *frame = &test_ctx.frame;
	struct is_subdev *subdev = &idi->group[GROUP_SLOT_PAF]->leader;
	struct camera2_node *node;

	test_ctx.ops = pablo_get_is_subdev_paf_ops();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.ops);

	test_ctx.vctx.queue = kunit_kzalloc(test, sizeof(struct is_queue), 0);

	test_ctx.vctx.group = &test_ctx.group;
	frame->shot_ext = &test_ctx.shot_ext;
	frame->parameter = &test_ctx.parameter;
	idi->is_region = &test_ctx.is_region;
	idi->sensor = ids;
	idi->group[GROUP_SLOT_PAF] = &test_ctx.group;
	subdev = &idi->group[GROUP_SLOT_PAF]->leader;
	subdev->leader = subdev;
	subdev->vctx = &test_ctx.vctx;
	node = &test_ctx.frame.shot_ext->node_group.leader;

	__set_test_vector_cropRegion(node->input.cropRegion);
	__set_test_vector_cropRegion(node->output.cropRegion);

	return 0;
}

static void pablo_subdev_paf_kunit_test_exit(struct kunit *test)
{
	struct camera2_node *node = &test_ctx.frame.shot_ext->node_group.leader;

	__clear_test_vector_cropRegion(node->input.cropRegion);
	__clear_test_vector_cropRegion(node->output.cropRegion);

	kunit_kfree(test, test_ctx.vctx.queue);
	memset(&test_ctx, 0, sizeof(test_ctx));
}

static struct kunit_case pablo_subdev_paf_kunit_test_cases[] = {
	KUNIT_CASE(pablo_subdev_paf_cfg_kunit_test),
	KUNIT_CASE(pablo_subdev_paf_tag_kunit_test),
	{},
};

struct kunit_suite pablo_subdev_paf_kunit_test_suite = {
	.name = "pablo-subdev-paf-kunit-test",
	.init = pablo_subdev_paf_kunit_test_init,
	.exit = pablo_subdev_paf_kunit_test_exit,
	.test_cases = pablo_subdev_paf_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_subdev_paf_kunit_test_suite);

MODULE_LICENSE("GPL");
