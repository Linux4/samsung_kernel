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

#include "pablo-kunit-test.h"
#include "is-stripe.h"
#include "pablo-debug.h"

static struct pablo_kunit_test_ctx {
	void *base;
	struct is_subdev subdev;
	struct is_frame frame;
	struct camera2_node node;
	const struct is_crop in, incrop, otcrop;
} pkt_ctx;

static void pablo_lib_strip_calc_region_num_kunit_test(struct kunit *test)
{
	int region_num;
	u32 input_width = 9248;
	u32 constraint_width = 4880;

	pkt_ctx.subdev.constraints_width = constraint_width;
	region_num = is_calc_region_num(input_width, &pkt_ctx.subdev);
	KUNIT_EXPECT_GT(test, region_num, 2);
}

static void pablo_lib_strip_g_stripe_cfg_kunit_test(struct kunit *test)
{
	int ret, k;

	pkt_ctx.frame.stripe_info.region_num = 3;
	for (k = 0; k < 3; k++) {
		pkt_ctx.frame.stripe_info.region_id = k;
		ret = is_ischain_g_stripe_cfg(&pkt_ctx.frame, &pkt_ctx.node, &pkt_ctx.in,
			&pkt_ctx.incrop, &pkt_ctx.otcrop);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}
}

static struct kunit_case pablo_lib_strip_kunit_test_cases[] = {
	KUNIT_CASE(pablo_lib_strip_calc_region_num_kunit_test),
	KUNIT_CASE(pablo_lib_strip_g_stripe_cfg_kunit_test),
	{},
};

struct kunit_suite pablo_lib_strip_kunit_test_suite = {
	.name = "pablo-lib_strip-kunit-test",
	.test_cases = pablo_lib_strip_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_lib_strip_kunit_test_suite);

MODULE_LICENSE("GPL");
