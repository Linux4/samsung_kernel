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

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>

#include "pablo-kunit-test.h"
#include <exynos-is-sensor.h>
#include <exynos-is-module.h>
#include "is-core.h"
#include "is-device-sensor.h"
#include "is-dt.h"

static struct pablo_kunit_test_ctx {
	struct device *dev;
	struct exynos_platform_is_sensor *pdata;
	u32 scenario;
	u32 channel;
	u32 freq;
} test_ctx;

static int pablo_setup_is_sensor_kunit_kunit_test_init(struct kunit *test)
{
	test_ctx.dev = is_get_is_dev();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.dev);

	test_ctx.pdata = kunit_kzalloc(test, sizeof(struct exynos_platform_is_sensor), 0);
	is_sensor_get_clk_ops(test_ctx.pdata);

	test_ctx.scenario = SENSOR_SCENARIO_NORMAL;
	test_ctx.freq = 26*1000000;
	test_ctx.channel = 0;

	return 0;
}

static void pablo_setup_is_sensor_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, test_ctx.pdata);
}

static void pablo_setup_is_sensor_mclk_kunit_test(struct kunit *test)
{
	struct exynos_platform_is_sensor *pdata;
	int ret = 0;

	pdata = test_ctx.pdata;

	/* mclk on/off/force_off */
	ret = pdata->mclk_on(test_ctx.dev, test_ctx.scenario, test_ctx.channel, test_ctx.freq);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pdata->mclk_off(test_ctx.dev, test_ctx.scenario, test_ctx.channel);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pdata->mclk_force_off(test_ctx.dev, test_ctx.channel);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_setup_is_sensor_iclk_kunit_test(struct kunit *test)
{
	struct exynos_platform_is_sensor *pdata;
	int ret = 0;

	pdata = test_ctx.pdata;

	/* iclk cfg/on/off */
	for (test_ctx.channel = 0; test_ctx.channel < 8; test_ctx.channel++) {
		ret = pdata->iclk_cfg(test_ctx.dev, test_ctx.scenario, test_ctx.channel);
		KUNIT_EXPECT_EQ(test, ret, 0);

		ret = pdata->iclk_on(test_ctx.dev, test_ctx.scenario, test_ctx.channel);
		KUNIT_EXPECT_EQ(test, ret, 0);

		ret = pdata->iclk_off(test_ctx.dev, test_ctx.scenario, test_ctx.channel);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}
}

static struct kunit_case pablo_setup_is_sensor_kunit_test_cases[] = {
	KUNIT_CASE(pablo_setup_is_sensor_mclk_kunit_test),
	KUNIT_CASE(pablo_setup_is_sensor_iclk_kunit_test),
	{},
};

struct kunit_suite pablo_setup_is_sensor_kunit_test_suite = {
	.name = "pablo-setup-is-sensor-kunit-test",
	.init = pablo_setup_is_sensor_kunit_kunit_test_init,
	.exit = pablo_setup_is_sensor_kunit_test_exit,
	.test_cases = pablo_setup_is_sensor_kunit_test_cases,
};

define_pablo_kunit_test_suites(&pablo_setup_is_sensor_kunit_test_suite);

MODULE_LICENSE("GPL v2");
