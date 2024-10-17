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
#include "exynos-is.h"

#define CLK_NAME		"DOUT_DIV_CLKCMU_CIS_CLK0"
#define CLK_PARENT_NAME		"DOUT_DIV_CLKCMU_CIS_CLK1"
#define CLK_WRONG_NAME		"WRONG_NAME"
#define ILLEGAL_FREQUENCY	-1

static struct pablo_kunit_test_ctx {
	struct device *dev;
	struct exynos_platform_is *pdata;
} pkt_ctx;

static void pablo_setup_clk_base_negative_kunit_test(struct kunit *test)
{
	int ret;
	ret = is_set_rate(pkt_ctx.dev, CLK_WRONG_NAME, 10000);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = is_set_rate(pkt_ctx.dev, CLK_NAME, ILLEGAL_FREQUENCY);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = is_enable(pkt_ctx.dev, CLK_WRONG_NAME);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	ret = is_disable(pkt_ctx.dev, CLK_WRONG_NAME);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = is_get_rate(pkt_ctx.dev, CLK_WRONG_NAME);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_get_hw_rate(pkt_ctx.dev, CLK_WRONG_NAME);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_enabled_clk_disable(pkt_ctx.dev, CLK_WRONG_NAME);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = is_set_parent_dt(pkt_ctx.dev, CLK_WRONG_NAME, CLK_PARENT_NAME);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	ret = is_set_parent_dt(pkt_ctx.dev, CLK_NAME, CLK_WRONG_NAME);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	ret = is_set_parent_dt(pkt_ctx.dev, CLK_NAME, CLK_NAME);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = is_set_rate_dt(pkt_ctx.dev, CLK_WRONG_NAME, 1000);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = is_enable_dt(pkt_ctx.dev, CLK_WRONG_NAME);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = is_disable_dt(pkt_ctx.dev, CLK_WRONG_NAME);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_setup_clk_base_kunit_test(struct kunit *test)
{
	int ret;
	ret = is_get_rate(pkt_ctx.dev, CLK_NAME);
	KUNIT_EXPECT_GT(test, ret, 0);

	ret = is_get_hw_rate(pkt_ctx.dev, CLK_NAME);
	KUNIT_EXPECT_GT(test, ret, 0);

	ret = is_enabled_clk_disable(pkt_ctx.dev, CLK_NAME);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_set_rate_dt(pkt_ctx.dev, CLK_NAME, 1000);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_enable_dt(pkt_ctx.dev, CLK_NAME);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_disable_dt(pkt_ctx.dev, CLK_NAME);
	KUNIT_EXPECT_EQ(test, ret, 0);

	is_dump_rate_dt(pkt_ctx.dev, CLK_NAME);
}

static void pablo_setup_clk_get_kunit_test(struct kunit *test)
{
	int ret;
	ret = pkt_ctx.pdata->clk_get(pkt_ctx.dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_setup_clk_cfg_kunit_test(struct kunit *test)
{
	int ret;
	ret = pkt_ctx.pdata->clk_cfg(pkt_ctx.dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_setup_clk_on_negative_kunit_test(struct kunit *test)
{
	int ret;
	/* Enable clk for two consecutive times to trigger the exception */
	ret = pkt_ctx.pdata->clk_on(pkt_ctx.dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = pkt_ctx.pdata->clk_on(pkt_ctx.dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_setup_clk_off_negative_kunit_test(struct kunit *test)
{
	int ret;
	ret = pkt_ctx.pdata->clk_off(pkt_ctx.dev);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_setup_clk_on_kunit_test(struct kunit *test)
{
	int ret;
	ret = pkt_ctx.pdata->clk_on(pkt_ctx.dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_setup_clk_off_kunit_test(struct kunit *test)
{
	int ret;
	ret = pkt_ctx.pdata->clk_off(pkt_ctx.dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_setup_clk_get_rate_kunit_test(struct kunit *test)
{
	int ret;
	ret = pkt_ctx.pdata->clk_get_rate(pkt_ctx.dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_setup_clk_print_kunit_test(struct kunit *test)
{
	int ret;
	ret = pkt_ctx.pdata->clk_print(pkt_ctx.dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_setup_clk_dump_kunit_test(struct kunit *test)
{
	int ret;
	ret = pkt_ctx.pdata->clk_dump(pkt_ctx.dev);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static int pablo_setup_kunit_test_init(struct kunit *test)
{
	struct device *dev;
	struct exynos_platform_is *pdata;

	dev = is_get_is_dev();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);

	pdata = dev_get_platdata(dev);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pdata);

	pkt_ctx.dev = dev;
	pkt_ctx.pdata = pdata;

	return 0;
}

static void pablo_setup_kunit_test_exit(struct kunit *test)
{
	memset(&pkt_ctx, 0, sizeof(pkt_ctx));
}

static struct kunit_case pablo_setup_kunit_test_cases[] = {
	KUNIT_CASE(pablo_setup_clk_base_negative_kunit_test),
	KUNIT_CASE(pablo_setup_clk_base_kunit_test),
	KUNIT_CASE(pablo_setup_clk_get_kunit_test),
	KUNIT_CASE(pablo_setup_clk_cfg_kunit_test),
	KUNIT_CASE(pablo_setup_clk_on_negative_kunit_test),
	KUNIT_CASE(pablo_setup_clk_on_kunit_test),
	KUNIT_CASE(pablo_setup_clk_off_kunit_test),
	KUNIT_CASE(pablo_setup_clk_off_negative_kunit_test),
	KUNIT_CASE(pablo_setup_clk_get_rate_kunit_test),
	KUNIT_CASE(pablo_setup_clk_print_kunit_test),
	KUNIT_CASE(pablo_setup_clk_dump_kunit_test),
	{},
};

struct kunit_suite pablo_setup_kunit_test_suite = {
	.name = "setup_pablo-kunit-test",
	.init = pablo_setup_kunit_test_init,
	.exit = pablo_setup_kunit_test_exit,
	.test_cases = pablo_setup_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_setup_kunit_test_suite);
