// SPDX-License-Identifier: GPL-2.0-only
//
// Cirrus Logic CS35L41 MFD driver tests
//
// Copyright (C) 2020 Cirrus Logic, Inc. and
//                    Cirrus Logic International Semiconductor Ltd.
#define DEBUG
#include <kunit/test.h>
#include <linux/device.h>
#include <sound/cirrus/core.h>
#include <sound/cirrus/calibration.h>
#include <sound/cirrus/big_data.h>
#include <sound/cirrus/power.h>
#include <linux/regmap.h>
#include <linux/typecheck.h>

#include "cs35l41.h"

struct cs35l41_test {
	struct device dev;
};
#ifdef CONFIG_UML
#else
static void cs35l41_test_bd_suffix(struct test *test)
{
	struct cs35l41_test *cs35l41 = test->priv;

	dev_dbg(&cs35l41->dev, "%s\n", __func__);
}

static void cs35l41_test_bd_store(struct test *test)
{
	struct cs35l41_test *cs35l41 = test->priv;
	const char *suffix_right = "_r";
	const char *suffix_wrong = "_x";

	dev_dbg(&cs35l41->dev, "%s\n", __func__);

	cirrus_bd_store_values(suffix_right);
	cirrus_bd_store_values(suffix_wrong);
}

static void cs35l41_test_pwr_start(struct test *test)
{
	struct cs35l41_test *cs35l41 = test->priv;
	const char *suffix_right = "_r";
	const char *suffix_wrong = "_x";

	dev_dbg(&cs35l41->dev, "%s\n", __func__);

	cirrus_pwr_start(suffix_right);
	cirrus_pwr_start(suffix_wrong);
}

static void cs35l41_test_pwr_stop(struct test *test)
{
	struct cs35l41_test *cs35l41 = test->priv;
	const char *suffix_right = "_r";
	const char *suffix_wrong = "_x";

	dev_dbg(&cs35l41->dev, "%s\n", __func__);

	cirrus_pwr_stop(suffix_right);
	cirrus_pwr_stop(suffix_wrong);
}

static void cs35l41_test_cal_apply(struct test *test)
{
	struct cs35l41_test *cs35l41 = test->priv;
	const char *suffix_right = "_r";
	int ret;

	dev_dbg(&cs35l41->dev, "%s\n", __func__);

	ret = cirrus_cal_apply(suffix_right);
	EXPECT_EQ(test, (ret == 0) || (ret == -2), true);
}

static void cs35l41_test_amp_add(struct test *test)
{
	struct cs35l41_test *cs35l41 = test->priv;
	const char *suffix_wrong = "_x";
	struct cirrus_amp_config config;
	int ret;

	memset(&config, 0, sizeof(config));

	dev_dbg(&cs35l41->dev, "%s\n", __func__);

	ret = cirrus_amp_add(suffix_wrong, config);
	EXPECT_EQ(test, (ret == 0) || (ret == -22), true);
}

static void cs35l41_test_tables_readable(struct test *test)
{
	struct cs35l41_test *cs35l41 = test->priv;
	int ret;

	dev_dbg(&cs35l41->dev, "%s\n", __func__);

	ret = cs35l41_readable_reg(NULL, 0);
	EXPECT_EQ(test, ret, true);
	ret = cs35l41_readable_reg(NULL, 1);
	EXPECT_EQ(test, ret, false);
}

static void cs35l41_test_tables_volatile(struct test *test)
{
	struct cs35l41_test *cs35l41 = test->priv;
	int ret;

	dev_dbg(&cs35l41->dev, "%s\n", __func__);

	ret = cs35l41_volatile_reg(NULL, 0x20);
	EXPECT_EQ(test, ret, true);
	ret = cs35l41_volatile_reg(NULL, 0);
	EXPECT_EQ(test, ret, false);
}
#endif

static struct test_case cs35l41_test_cases[] = {
#ifdef CONFIG_UML
#else
	TEST_CASE(cs35l41_test_bd_suffix),
	TEST_CASE(cs35l41_test_bd_store),
	TEST_CASE(cs35l41_test_pwr_start),
	TEST_CASE(cs35l41_test_pwr_stop),
	TEST_CASE(cs35l41_test_cal_apply),
	TEST_CASE(cs35l41_test_amp_add),
	TEST_CASE(cs35l41_test_tables_readable),
	TEST_CASE(cs35l41_test_tables_volatile),
#endif
	{}
};

static void cs35l41_test_dev_release(struct device *dev)
{
}

static int cs35l41_test_init(struct test *test)
{
	struct cs35l41_test *priv;
	int ret;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev.init_name = "cs35l41-test";
	priv->dev.release = cs35l41_test_dev_release;

	ret = device_register(&priv->dev);
	if (ret)
		return ret;

	test->priv = priv;

	return 0;
}

static void cs35l41_test_exit(struct test *test)
{
	struct cs35l41_test *priv = test->priv;

	device_unregister(&priv->dev);

	if (priv != NULL)
		kfree(priv);
}

static struct test_module cs35l41_test_suite = {
	.name = "cs35l41-mfd-tests",
	.init = cs35l41_test_init,
	.exit = cs35l41_test_exit,
	.test_cases = cs35l41_test_cases,
};
module_test_for_module(cs35l41_test_suite);
