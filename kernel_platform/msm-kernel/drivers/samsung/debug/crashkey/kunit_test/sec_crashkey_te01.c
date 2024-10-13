// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include <linux/delay.h>
#include <linux/input.h>

#include <linux/samsung/debug/sec_debug.h>
#include <linux/samsung/sec_of_kunit.h>

#include "sec_crashkey_test.h"

static void te01__crashkey_test_dt_debug_level(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct crashkey_drvdata *drvdata = container_of(testdata->bd,
			struct crashkey_drvdata, bd);
	int err;

	err = __crashkey_test_dt_debug_level(drvdata, testdata->of_node,
			SEC_DEBUG_LEVEL_LOW);
	KUNIT_EXPECT_NE(test, -ENODEV, err);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __crashkey_test_dt_debug_level(drvdata, testdata->of_node,
			SEC_DEBUG_LEVEL_MID);
	KUNIT_EXPECT_NE(test, -ENODEV, err);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __crashkey_test_dt_debug_level(drvdata, testdata->of_node,
			SEC_DEBUG_LEVEL_HIGH);
	KUNIT_EXPECT_NE(test, -ENODEV, err);
	KUNIT_EXPECT_EQ(test, 0, err);
}

static struct sec_of_kunit_data *sec_crashkey_te01data;
SEC_OF_KUNIT_DTB_INFO_EXTERN(sec_crashkey_te01);
static SEC_OF_KUNIT_DTB_INFO(sec_crashkey_te01);

static int __crashkey_te01_suite_set_up(struct sec_of_kunit_data *testdata)
{
	struct crashkey_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	return sec_of_kunit_data_init(testdata,
			"kunit:samsung,crashkey_te01", &drvdata->bd,
			"samsung,crashkey", &sec_crashkey_te01_info);
}

static void __crashkey_te01_suite_tear_down(struct sec_of_kunit_data *testdata)
{
	struct crashkey_drvdata *drvdata = container_of(testdata->bd,
			struct crashkey_drvdata, bd);

	kfree(drvdata);

	sec_of_kunit_data_exit(testdata);
}

static int sec_crashkey_te01_case_init(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_crashkey_te01data;

	if (!testdata) {
		int err;

		testdata = kzalloc(sizeof(*testdata), GFP_KERNEL);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, testdata);

		err = __crashkey_te01_suite_set_up(testdata);
		KUNIT_EXPECT_EQ(test, 0, err);

		sec_crashkey_te01data = testdata;
	}

	test->priv = testdata;

	return 0;
}

static void tear_down_sec_crashkey_te01(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_crashkey_te01data;

	__crashkey_te01_suite_tear_down(testdata);
	kfree_sensitive(testdata);

	sec_crashkey_te01data = NULL;
}

static struct kunit_case sec_crashkey_te01_cases[] = {
	KUNIT_CASE(te01__crashkey_test_dt_debug_level),
	KUNIT_CASE(tear_down_sec_crashkey_te01),
	{},
};

struct kunit_suite sec_crashkey_te01_module = {
	.name = "sec_crashkey_te01",
	.init = sec_crashkey_te01_case_init,
	.test_cases = sec_crashkey_te01_cases,
};

kunit_test_suites(&sec_crashkey_te01_module);
