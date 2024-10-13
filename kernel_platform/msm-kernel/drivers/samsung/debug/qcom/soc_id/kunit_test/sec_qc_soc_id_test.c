// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include <linux/samsung/sec_of_kunit.h>

#include "sec_qc_soc_id_test.h"

static void test__qc_soc_id_parse_dt_qfprom_jtag(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct qc_soc_id_drvdata *drvdata;
	int err;

	err = __qc_soc_id_parse_dt_qfprom_jtag(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct qc_soc_id_drvdata, bd);

	KUNIT_EXPECT_TRUE(test, drvdata->use_qfprom_jtag);
	KUNIT_EXPECT_EQ(test, 0x4646A744UL, drvdata->qfprom_jtag_phys);
}

static void test__qc_soc_id_parse_dt_jtag_id(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct qc_soc_id_drvdata *drvdata;
	int err;

	err = __qc_soc_id_parse_dt_jtag_id(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct qc_soc_id_drvdata, bd);

	KUNIT_EXPECT_TRUE(test, drvdata->use_jtag_id);
	KUNIT_EXPECT_EQ(test, 0x830F8B24UL, drvdata->jtag_id_phys);
}

static struct sec_of_kunit_data *sec_qc_soc_id_testdata;
SEC_OF_KUNIT_DTB_INFO_EXTERN(sec_qc_soc_id_test);
static SEC_OF_KUNIT_DTB_INFO(sec_qc_soc_id_test);

static int __qc_soc_id_test_suite_set_up(struct sec_of_kunit_data *testdata)
{
	struct qc_soc_id_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	return sec_of_kunit_data_init(testdata,
			"kunit:samsung,qcom-soc_id_test", &drvdata->bd,
			"samsung,qcom-soc_id", &sec_qc_soc_id_test_info);
}

static void __qc_soc_id_test_suite_tear_down(struct sec_of_kunit_data *testdata)
{
	struct qc_soc_id_drvdata *drvdata = container_of(testdata->bd,
			struct qc_soc_id_drvdata, bd);

	kfree(drvdata);

	sec_of_kunit_data_exit(testdata);
}

static int sec_qc_soc_id_test_case_init(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_qc_soc_id_testdata;

	if (!testdata) {
		int err;

		testdata = kzalloc(sizeof(*testdata), GFP_KERNEL);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, testdata);

		err = __qc_soc_id_test_suite_set_up(testdata);
		KUNIT_EXPECT_EQ(test, 0, err);

		sec_qc_soc_id_testdata = testdata;
	}

	test->priv = testdata;

	return 0;
}

static void tear_down_sec_qc_soc_id_test(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_qc_soc_id_testdata;

	__qc_soc_id_test_suite_tear_down(testdata);
	kfree_sensitive(testdata);

	sec_qc_soc_id_testdata = NULL;
}

static struct kunit_case sec_qc_soc_id_test_cases[] = {
	KUNIT_CASE(test__qc_soc_id_parse_dt_qfprom_jtag),
	KUNIT_CASE(test__qc_soc_id_parse_dt_jtag_id),
	KUNIT_CASE(tear_down_sec_qc_soc_id_test),
	{},
};

struct kunit_suite sec_qc_soc_id_test_suite = {
	.name = "sec_qc_soc_id_test",
	.init = sec_qc_soc_id_test_case_init,
	.test_cases = sec_qc_soc_id_test_cases,
};

kunit_test_suites(&sec_qc_soc_id_test_suite);
