// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include <linux/samsung/sec_of_kunit.h>

#include "sec_qc_rst_exinfo_test.h"

static void test__rst_exinfo_dt_die_notifier_priority(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct rst_exinfo_drvdata *drvdata;
	struct notifier_block *nb_die;
	int err;

	err = __rst_exinfo_dt_die_notifier_priority(testdata->bd,
			testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct rst_exinfo_drvdata, bd);
	nb_die = &drvdata->nb_die;
	KUNIT_EXPECT_EQ(test, 128, nb_die->priority);
}

static void test__rst_exinfo_dt_panic_notifier_priority(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct rst_exinfo_drvdata *drvdata;
	struct notifier_block *nb_panic;
	int err;

	err = __rst_exinfo_dt_panic_notifier_priority(testdata->bd,
			testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct rst_exinfo_drvdata, bd);
	nb_panic = &drvdata->nb_panic;
	KUNIT_EXPECT_EQ(test, 64, nb_panic->priority);
}

static void test__rst_exinfo_init_panic_extra_info(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct rst_exinfo_drvdata *drvdata;
	rst_exinfo_t *rst_exinfo;
	_kern_ex_info_t *kern_ex_info;
	int err;

	err = __rst_exinfo_init_panic_extra_info(testdata->bd);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct rst_exinfo_drvdata, bd);
	rst_exinfo = drvdata->rst_exinfo;
	kern_ex_info = &rst_exinfo->kern_ex_info.info;
	KUNIT_EXPECT_EQ(test, -1, kern_ex_info->cpu);
}

static void test__qc_rst_exinfo_save_dying_msg(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct rst_exinfo_drvdata *drvdata = container_of(testdata->bd,
			struct rst_exinfo_drvdata, bd);
	rst_exinfo_t *rst_exinfo = drvdata->rst_exinfo;
	_kern_ex_info_t *kern_ex_info = &rst_exinfo->kern_ex_info.info;

	__qc_rst_exinfo_save_dying_msg(drvdata,
			"Raccoon's Cave", (void *)0xcafebabe, (void *)0xcab0cafe);
	KUNIT_EXPECT_NE(test, -1, kern_ex_info->cpu);
	KUNIT_EXPECT_NE(test, 0, kern_ex_info->ktime);
	KUNIT_EXPECT_STREQ(test, "0xcafebabe", kern_ex_info->pc);
	KUNIT_EXPECT_STREQ(test, "0xcab0cafe", kern_ex_info->lr);
	KUNIT_EXPECT_STREQ(test, "Raccoon's Cave", kern_ex_info->panic_buf);
}

static struct sec_of_kunit_data *sec_qc_rst_exinfo_testdata;
SEC_OF_KUNIT_DTB_INFO_EXTERN(sec_qc_rst_exinfo_test);
static SEC_OF_KUNIT_DTB_INFO(sec_qc_rst_exinfo_test);

static int __qc_rst_exinfo_test_suite_set_up(struct sec_of_kunit_data *testdata)
{
	struct rst_exinfo_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	return sec_of_kunit_data_init(testdata,
			"kunit:samsung,qcom-rst_exinfo_test", &drvdata->bd,
			"samsung,qcom-rst_exinfo", &sec_qc_rst_exinfo_test_info);
}

static void __qc_rst_exinfo_test_suite_tear_down(struct sec_of_kunit_data *testdata)
{
	struct rst_exinfo_drvdata *drvdata = container_of(testdata->bd,
			struct rst_exinfo_drvdata, bd);

	kfree(drvdata);

	sec_of_kunit_data_exit(testdata);
}

static int __rst_exinfo_init_rst_exinfo_reserved_region(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_qc_rst_exinfo_testdata;
	struct rst_exinfo_drvdata *drvdata = container_of(testdata->bd,
			struct rst_exinfo_drvdata, bd);
	rst_exinfo_t *rst_exinfo;

	rst_exinfo = kunit_kzalloc(test, PAGE_SIZE, GFP_KERNEL);
	if (!rst_exinfo)
		return -ENOMEM;

	drvdata->size = PAGE_SIZE;
	drvdata->rst_exinfo = rst_exinfo;

	return 0;
}

static int sec_qc_rst_exinfo_test_case_init(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_qc_rst_exinfo_testdata;

	if (!testdata) {
		int err;

		testdata = kzalloc(sizeof(*testdata), GFP_KERNEL);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, testdata);

		err = __qc_rst_exinfo_test_suite_set_up(testdata);
		KUNIT_EXPECT_EQ(test, 0, err);

		sec_qc_rst_exinfo_testdata = testdata;

		err = __rst_exinfo_init_rst_exinfo_reserved_region(test);
		KUNIT_EXPECT_EQ(test, 0, err);
	}

	test->priv = testdata;

	return 0;
}

static void tear_down_sec_qc_rst_exinfo_test(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_qc_rst_exinfo_testdata;

	__qc_rst_exinfo_test_suite_tear_down(testdata);
	kfree_sensitive(testdata);

	sec_qc_rst_exinfo_testdata = NULL;
}

static struct kunit_case sec_qc_rst_exinfo_test_cases[] = {
	KUNIT_CASE(test__rst_exinfo_dt_die_notifier_priority),
	KUNIT_CASE(test__rst_exinfo_dt_panic_notifier_priority),
	KUNIT_CASE(test__rst_exinfo_init_panic_extra_info),
	KUNIT_CASE(test__qc_rst_exinfo_save_dying_msg),
	KUNIT_CASE(tear_down_sec_qc_rst_exinfo_test),
	{},
};

struct kunit_suite sec_qc_rst_exinfo_test_suite = {
	.name = "sec_qc_rst_exinfo_test",
	.init = sec_qc_rst_exinfo_test_case_init,
	.test_cases = sec_qc_rst_exinfo_test_cases,
};

kunit_test_suites(&sec_qc_rst_exinfo_test_suite);
