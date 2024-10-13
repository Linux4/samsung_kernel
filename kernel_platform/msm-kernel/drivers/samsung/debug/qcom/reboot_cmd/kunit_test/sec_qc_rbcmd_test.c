// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include <linux/samsung/sec_of_kunit.h>

#include "sec_qc_rbcmd_test.h"

struct sec_qc_rbcmd_kunit_context {
	struct sec_of_kunit_data testdata;
	struct notifier_block pon_rr_nb;
	unsigned int pon_rr_result;
	struct notifier_block sec_rr_nb;
	unsigned int sec_rr_result;
};

static void test__rbcmd_parse_dt_use_on_reboot(struct kunit *test)
{
	struct sec_qc_rbcmd_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct qc_rbcmd_drvdata *drvdata;
	int err;

	err = __rbcmd_parse_dt_use_on_reboot(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct qc_rbcmd_drvdata, bd);

	KUNIT_EXPECT_TRUE(test, drvdata->use_on_reboot);
}

static void test__rbcmd_parse_dt_use_on_restart(struct kunit *test)
{
	struct sec_qc_rbcmd_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct qc_rbcmd_drvdata *drvdata;
	int err;

	err = __rbcmd_parse_dt_use_on_restart(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct qc_rbcmd_drvdata, bd);

	KUNIT_EXPECT_TRUE(test, drvdata->use_on_restart);
}

static void test__rbcmd_write_pon_rr(struct kunit *test)
{
	struct sec_qc_rbcmd_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct qc_rbcmd_drvdata *drvdata =
			container_of(testdata->bd, struct qc_rbcmd_drvdata, bd);
	int err;

	err = __rbcmd_register_pon_rr_writer(drvdata,
			&testctx->pon_rr_nb);

	__rbcmd_write_pon_rr(drvdata, 0, NULL);
	KUNIT_EXPECT_EQ(test, 0, testctx->pon_rr_result);

	__rbcmd_write_pon_rr(drvdata, 1, NULL);
	KUNIT_EXPECT_EQ(test, 1, testctx->pon_rr_result);

	__rbcmd_unregister_pon_rr_writer(drvdata, &testctx->pon_rr_nb);

	testctx->pon_rr_result = (unsigned int)-1;

	__rbcmd_write_pon_rr(drvdata, 0, NULL);
	KUNIT_EXPECT_NE(test, 0, testctx->pon_rr_result);
	KUNIT_EXPECT_EQ(test, (unsigned int)-1, testctx->pon_rr_result);

	__rbcmd_write_pon_rr(drvdata, 1, NULL);
	KUNIT_EXPECT_NE(test, 1, testctx->pon_rr_result);
	KUNIT_EXPECT_EQ(test, (unsigned int)-1, testctx->pon_rr_result);
}

static void test__rbcmd_write_sec_rr(struct kunit *test)
{
	struct sec_qc_rbcmd_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct qc_rbcmd_drvdata *drvdata =
			container_of(testdata->bd, struct qc_rbcmd_drvdata, bd);
	int err;

	err = __rbcmd_register_sec_rr_writer(drvdata,
			&testctx->sec_rr_nb);

	__rbcmd_write_sec_rr(drvdata, 0, NULL);
	KUNIT_EXPECT_EQ(test, 0, testctx->sec_rr_result);

	__rbcmd_write_sec_rr(drvdata, 1, NULL);
	KUNIT_EXPECT_EQ(test, 1, testctx->sec_rr_result);

	__rbcmd_unregister_sec_rr_writer(drvdata, &testctx->sec_rr_nb);

	testctx->sec_rr_result = (unsigned int)-1;

	__rbcmd_write_sec_rr(drvdata, 0, NULL);
	KUNIT_EXPECT_NE(test, 0, testctx->sec_rr_result);
	KUNIT_EXPECT_EQ(test, (unsigned int)-1, testctx->sec_rr_result);

	__rbcmd_write_sec_rr(drvdata, 1, NULL);
	KUNIT_EXPECT_NE(test, 1, testctx->sec_rr_result);
	KUNIT_EXPECT_EQ(test, (unsigned int)-1, testctx->sec_rr_result);
}

static struct sec_qc_rbcmd_kunit_context *sec_qc_rbcmd_testctx;
SEC_OF_KUNIT_DTB_INFO_EXTERN(sec_qc_rbcmd_test);
static SEC_OF_KUNIT_DTB_INFO(sec_qc_rbcmd_test);

static int __qc_rbcmd_main_test_suite_set_up(struct sec_of_kunit_data *testdata)
{
	struct qc_rbcmd_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	return sec_of_kunit_data_init(testdata,
			"kunit:samsung,qcom-reboot_cmd_test", &drvdata->bd,
			"samsung,qcom-reboot_cmd", &sec_qc_rbcmd_test_info);
}

static void __qc_rbcmd_main_test_suite_tear_down(struct sec_of_kunit_data *testdata)
{
	struct qc_rbcmd_drvdata *drvdata = container_of(testdata->bd,
			struct qc_rbcmd_drvdata, bd);

	kfree(drvdata);

	sec_of_kunit_data_exit(testdata);
}

static int test_write_pon_rr(struct notifier_block *this,
		unsigned long pon_rr, void *data)
{
	struct sec_qc_rbcmd_kunit_context *testctx = container_of(this,
			struct sec_qc_rbcmd_kunit_context, pon_rr_nb);

	testctx->pon_rr_result = (unsigned int)pon_rr;

	return NOTIFY_OK;
}

static int test_write_sec_rr(struct notifier_block *this,
		unsigned long sec_rr, void *data)
{
	struct sec_qc_rbcmd_kunit_context *testctx = container_of(this,
			struct sec_qc_rbcmd_kunit_context, sec_rr_nb);

	testctx->sec_rr_result = (unsigned int)sec_rr;

	return NOTIFY_OK;
}

static void __qc_rbcmd_test_init_notifier(struct sec_qc_rbcmd_kunit_context *testctx)
{
	testctx->pon_rr_nb.notifier_call = test_write_pon_rr;
	testctx->sec_rr_nb.notifier_call = test_write_sec_rr;
}

static int sec_qc_rbcmd_test_case_init(struct kunit *test)
{
	struct sec_qc_rbcmd_kunit_context *testctx = sec_qc_rbcmd_testctx;
	struct sec_of_kunit_data *testdata;

	/* FIXME: 'misc_register' can't be called in 'suite_init' call-back */
	if (!testctx) {
		int err;

		testctx = kzalloc(sizeof(*testctx), GFP_KERNEL);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, testctx);

		testdata = &testctx->testdata;
		err = __qc_rbcmd_main_test_suite_set_up(testdata);
		KUNIT_EXPECT_EQ(test, 0, err);

		__qc_rbcmd_test_init_notifier(testctx);

		sec_qc_rbcmd_testctx = testctx;
	}

	test->priv = testctx;

	return 0;
}

static void tear_down_sec_qc_rbcmd_test(struct kunit *test)
{
	struct sec_qc_rbcmd_kunit_context *testctx = sec_qc_rbcmd_testctx;
	struct sec_of_kunit_data *testdata = &testctx->testdata;

	__qc_rbcmd_main_test_suite_tear_down(testdata);
	kfree_sensitive(testdata);

	sec_qc_rbcmd_testctx = NULL;
}

static struct kunit_case sec_qc_rbcmd_test_cases[] = {
	KUNIT_CASE(test__rbcmd_parse_dt_use_on_reboot),
	KUNIT_CASE(test__rbcmd_parse_dt_use_on_restart),
	KUNIT_CASE(test__rbcmd_write_pon_rr),
	KUNIT_CASE(test__rbcmd_write_sec_rr),
	KUNIT_CASE(tear_down_sec_qc_rbcmd_test),
	{},
};

struct kunit_suite sec_qc_rbcmd_test_suite = {
	.name = "sec_qc_rbcmd_test",
	.init = sec_qc_rbcmd_test_case_init,
	.test_cases = sec_qc_rbcmd_test_cases,
};

kunit_test_suites(&sec_qc_rbcmd_test_suite);
