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
#include "is-hw.h"

static struct test_ctx {
	struct device test_dev;
} test_ctx;

static void pablo_hw_pwr_ischain_resume_post_kunit_test(struct kunit *test)
{
	int ret = 0;
	struct device *dev = &test_ctx.test_dev;

	dev->driver_data = NULL;
	ret = is_ischain_runtime_resume_post(dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_pwr_ischain_suspend_post_kunit_test(struct kunit *test)
{
	int ret = 0;
	struct device *dev = &test_ctx.test_dev;

	dev->driver_data = NULL;
	ret = is_ischain_runtime_suspend_post(dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static int pablo_hw_pwr_kunit_test_init(struct kunit *test)
{
    return 0;
}

static void pablo_hw_pwr_kunit_test_exit(struct kunit *test)
{
}

static struct kunit_case pablo_hw_pwr_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_pwr_ischain_resume_post_kunit_test),
	KUNIT_CASE(pablo_hw_pwr_ischain_suspend_post_kunit_test),
	{},
};

struct kunit_suite pablo_hw_pwr_kunit_test_suite = {
	.name = "pablo-hw-pwr-kunit-test",
	.init = pablo_hw_pwr_kunit_test_init,
	.exit = pablo_hw_pwr_kunit_test_exit,
	.test_cases = pablo_hw_pwr_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_pwr_kunit_test_suite);

MODULE_LICENSE("GPL v2");
