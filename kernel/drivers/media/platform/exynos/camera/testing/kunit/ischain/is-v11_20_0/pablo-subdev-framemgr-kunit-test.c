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


static struct test_ctx {
	struct is_subdev subdev;
	u32 sbwc_type;
	u32 lossy_byte32num;
	u32 width, height;
	u32 sbwc_block_width, sbwc_block_height;
} test_ctx;

static void pablo_subdev_framemgr_internal_get_sbwc_type_kunit_test(struct kunit *test)
{
	struct is_subdev *subdev = &test_ctx.subdev;
	u32 sbwc_type = test_ctx.sbwc_type;
	u32 lossy_byte32num = test_ctx.lossy_byte32num;

	/*get sbwc_type is COMP or NONE*/
	for (subdev->id = ENTRY_SENSOR; subdev->id < ENTRY_END; subdev->id++) {
		if (subdev->id == ENTRY_SSVC0) {
			is_subdev_internal_get_sbwc_type(subdev, &sbwc_type, &lossy_byte32num);
			KUNIT_EXPECT_TRUE(test, sbwc_type == COMP);
		} else {
			is_subdev_internal_get_sbwc_type(subdev, &sbwc_type, &lossy_byte32num);
			KUNIT_EXPECT_TRUE(test, sbwc_type == NONE);
		}
	}
}

static void pablo_subdev_framemgr_internal_get_buffer_size_kunit_test(struct kunit *test)
{
	int ret = 0;
	struct is_subdev *subdev = &test_ctx.subdev;
	u32 *width = &test_ctx.width;
	u32 *height = &test_ctx.height;
	u32 sbwc_block_width = test_ctx.sbwc_block_width;
	u32 sbwc_block_height = test_ctx.sbwc_block_height;

	width = NULL;
	ret = is_subdev_internal_get_buffer_size(subdev, width, height, &sbwc_block_width, &sbwc_block_height);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	width = &test_ctx.width;
	height = NULL;
	ret = is_subdev_internal_get_buffer_size(subdev, width, height, &sbwc_block_width, &sbwc_block_height);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	subdev->output.width = 1920;
	subdev->output.height = 1080;
	width = &test_ctx.width;
	height = &test_ctx.height;
	for (subdev->id = ENTRY_SENSOR; subdev->id < ENTRY_END; subdev->id++) {
		if (subdev->id == ENTRY_SSVC0) {
			ret = is_subdev_internal_get_buffer_size(subdev, width, height, &sbwc_block_width, &sbwc_block_height);
			KUNIT_EXPECT_EQ(test, ret, 0);
			KUNIT_EXPECT_TRUE(test, *width == subdev->output.width);
			KUNIT_EXPECT_TRUE(test, *height == subdev->output.height);
			KUNIT_EXPECT_TRUE(test, sbwc_block_width == CSIS_COMP_BLOCK_WIDTH);
			KUNIT_EXPECT_TRUE(test, sbwc_block_height == CSIS_COMP_BLOCK_HEIGHT);
		} else {
			ret = is_subdev_internal_get_buffer_size(subdev, width, height, &sbwc_block_width, &sbwc_block_height);
			KUNIT_EXPECT_EQ(test, ret, 0);
			KUNIT_EXPECT_TRUE(test, *width == subdev->output.width);
			KUNIT_EXPECT_TRUE(test, *height == subdev->output.height);
		}
	}
}

static int pablo_subdev_framemgr_kunit_test_init(struct kunit *test)
{
	return 0;
}

static void pablo_subdev_framemgr_kunit_test_exit(struct kunit *test)
{

}

static struct kunit_case pablo_subdev_framemgr_kunit_test_cases[] = {
	KUNIT_CASE(pablo_subdev_framemgr_internal_get_sbwc_type_kunit_test),
	KUNIT_CASE(pablo_subdev_framemgr_internal_get_buffer_size_kunit_test),
	{},
};

struct kunit_suite pablo_subdev_framemgr_kunit_test_suite = {
	.name = "pablo-subdev-framemgr-kunit-test",
	.init = pablo_subdev_framemgr_kunit_test_init,
	.exit = pablo_subdev_framemgr_kunit_test_exit,
	.test_cases = pablo_subdev_framemgr_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_subdev_framemgr_kunit_test_suite);

MODULE_LICENSE("GPL v2");
