// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include <linux/samsung/sec_of_kunit.h>

#include "sec_qc_rbcmd_test.h"

static void test__rbcmd_parse_dt_use_on_reboot(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct qc_rbcmd_drvdata *drvdata;
	int err;

	drvdata = container_of(testdata->bd, struct qc_rbcmd_drvdata, bd);
	drvdata->use_on_reboot = true;

	err = __rbcmd_parse_dt_use_on_reboot(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	KUNIT_EXPECT_FALSE(test, drvdata->use_on_reboot);
}

static void test__rbcmd_parse_dt_use_on_restart(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct qc_rbcmd_drvdata *drvdata;
	int err;

	drvdata = container_of(testdata->bd, struct qc_rbcmd_drvdata, bd);
	drvdata->use_on_restart = true;

	err = __rbcmd_parse_dt_use_on_restart(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	KUNIT_EXPECT_FALSE(test, drvdata->use_on_restart);
}

static struct sec_of_kunit_data *sec_qc_rbcmd_te01data;
SEC_OF_KUNIT_DTB_INFO_EXTERN(sec_qc_rbcmd_te01);
static SEC_OF_KUNIT_DTB_INFO(sec_qc_rbcmd_te01);

static int __qc_rbcmd_main_te01_suite_set_up(struct sec_of_kunit_data *testdata)
{
	struct qc_rbcmd_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	return sec_of_kunit_data_init(testdata,
			"kunit:samsung,qcom-reboot_cmd_te01", &drvdata->bd,
			"samsung,qcom-reboot_cmd", &sec_qc_rbcmd_te01_info);
}

static void __qc_rbcmd_main_te01_suite_tear_down(struct sec_of_kunit_data *testdata)
{
	struct qc_rbcmd_drvdata *drvdata = container_of(testdata->bd,
			struct qc_rbcmd_drvdata, bd);

	kfree(drvdata);

	sec_of_kunit_data_exit(testdata);
}

static int sec_qc_rbcmd_te01_case_init(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_qc_rbcmd_te01data;

	/* FIXME: 'misc_register' can't be called in 'suite_init' call-back */
	if (!testdata) {
		int err;

		testdata = kzalloc(sizeof(*testdata), GFP_KERNEL);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, testdata);

		err = __qc_rbcmd_main_te01_suite_set_up(testdata);
		KUNIT_EXPECT_EQ(test, 0, err);

		sec_qc_rbcmd_te01data = testdata;
	}

	test->priv = testdata;

	return 0;
}

static void tear_donw_sec_qc_rbcmd_te01(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_qc_rbcmd_te01data;

	__qc_rbcmd_main_te01_suite_tear_down(testdata);
	kfree_sensitive(testdata);

	sec_qc_rbcmd_te01data = NULL;
}

static struct kunit_case sec_qc_rbcmd_te01_cases[] = {
	KUNIT_CASE(test__rbcmd_parse_dt_use_on_reboot),
	KUNIT_CASE(test__rbcmd_parse_dt_use_on_restart),
	KUNIT_CASE(tear_donw_sec_qc_rbcmd_te01),
	{},
};

struct kunit_suite sec_qc_rbcmd_te01_suite = {
	.name = "sec_qc_rbcmd_te01",
	.init = sec_qc_rbcmd_te01_case_init,
	.test_cases = sec_qc_rbcmd_te01_cases,
};

kunit_test_suites(&sec_qc_rbcmd_te01_suite);
