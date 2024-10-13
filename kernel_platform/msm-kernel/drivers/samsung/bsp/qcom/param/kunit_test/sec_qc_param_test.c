// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include <linux/sizes.h>

#include <linux/samsung/debug/sec_debug.h>
#include <linux/samsung/sec_of_kunit.h>

#include "sec_qc_param_test.h"

static void test__qc_param_parse_dt_bdev_path(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct qc_param_drvdata *drvdata;
	int err;

	err = __qc_param_parse_dt_bdev_path(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct qc_param_drvdata, bd);

	KUNIT_EXPECT_STREQ(test, "PARTUUID=b9c644b3-1ad5-48e6-a7ce-a300b6da8211",
			drvdata->bdev_path);
}

static void test__qc_param_parse_dt_negative_offset(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct qc_param_drvdata *drvdata;
	int err;

	err = __qc_param_parse_dt_negative_offset(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct qc_param_drvdata, bd);

	KUNIT_EXPECT_EQ(test, SZ_2M, drvdata->negative_offset);
}

static void test__qc_param_verify_debuglevel(struct kunit *test)
{
	struct qc_param_info _info =
			__QC_PARAM_INFO(debuglevel, __qc_param_verify_debuglevel);
	struct qc_param_info *info = &_info;
	bool is_verified;
	unsigned int debuglevel;

	debuglevel = 0x0;
	is_verified = info->verify_input(info, &debuglevel);
	KUNIT_EXPECT_FALSE(test, is_verified);

	debuglevel = (unsigned int)-1;
	is_verified = info->verify_input(info, &debuglevel);
	KUNIT_EXPECT_FALSE(test, is_verified);

	debuglevel = SEC_DEBUG_LEVEL_LOW;
	is_verified = info->verify_input(info, &debuglevel);
	KUNIT_EXPECT_TRUE(test, is_verified);

	debuglevel = SEC_DEBUG_LEVEL_MID;
	is_verified = info->verify_input(info, &debuglevel);
	KUNIT_EXPECT_TRUE(test, is_verified);

	debuglevel = SEC_DEBUG_LEVEL_HIGH;
	is_verified = info->verify_input(info, &debuglevel);
	KUNIT_EXPECT_TRUE(test, is_verified);
}

static void test__qc_param_verify_sapa(struct kunit *test)
{
	struct qc_param_info _info =
			__QC_PARAM_INFO(sapa, __qc_param_verify_sapa);
	struct qc_param_info *info = &_info;
	bool is_verified;
	unsigned int sapa;

	sapa = (unsigned int) -1;
	is_verified = info->verify_input(info, &sapa);
	KUNIT_EXPECT_FALSE(test, is_verified);

	sapa = 0;
	is_verified = info->verify_input(info, &sapa);
	KUNIT_EXPECT_TRUE(test, is_verified);

	sapa = SAPA_KPARAM_MAGIC;
	is_verified = info->verify_input(info, &sapa);
	KUNIT_EXPECT_TRUE(test, is_verified);
}

static void test__qc_param_verify_afc_disable(struct kunit *test)
{
	struct qc_param_info _info =
			__QC_PARAM_INFO(sapa, __qc_param_verify_afc_disable);
	struct qc_param_info *info = &_info;
	bool is_verified;
	char mode;

	mode = 0;
	is_verified = info->verify_input(info, &mode);
	KUNIT_EXPECT_FALSE(test, is_verified);

	mode = 1;
	is_verified = info->verify_input(info, &mode);
	KUNIT_EXPECT_FALSE(test, is_verified);

	mode = '0';
	is_verified = info->verify_input(info, &mode);
	KUNIT_EXPECT_TRUE(test, is_verified);

	mode = '1';
	is_verified = info->verify_input(info, &mode);
	KUNIT_EXPECT_TRUE(test, is_verified);
}

static void test__qc_param_verify_pd_disable(struct kunit *test)
{
	struct qc_param_info _info =
			__QC_PARAM_INFO(sapa, __qc_param_verify_pd_disable);
	struct qc_param_info *info = &_info;
	bool is_verified;
	char mode;

	mode = 0;
	is_verified = info->verify_input(info, &mode);
	KUNIT_EXPECT_FALSE(test, is_verified);

	mode = 1;
	is_verified = info->verify_input(info, &mode);
	KUNIT_EXPECT_FALSE(test, is_verified);

	mode = '0';
	is_verified = info->verify_input(info, &mode);
	KUNIT_EXPECT_TRUE(test, is_verified);

	mode = '1';
	is_verified = info->verify_input(info, &mode);
	KUNIT_EXPECT_TRUE(test, is_verified);
}

static void test__qc_param_verify_cp_reserved_mem(struct kunit *test)
{
	struct qc_param_info _info =
			__QC_PARAM_INFO(sapa, __qc_param_verify_cp_reserved_mem);
	struct qc_param_info *info = &_info;
	bool is_verified;
	unsigned int cp_reserved_mem;

	cp_reserved_mem = (unsigned int)-1;
	is_verified = info->verify_input(info, &cp_reserved_mem);
	KUNIT_EXPECT_FALSE(test, is_verified);

	cp_reserved_mem = CP_MEM_RESERVE_OFF;
	is_verified = info->verify_input(info, &cp_reserved_mem);
	KUNIT_EXPECT_TRUE(test, is_verified);

	cp_reserved_mem = CP_MEM_RESERVE_ON_1;
	is_verified = info->verify_input(info, &cp_reserved_mem);
	KUNIT_EXPECT_TRUE(test, is_verified);

	cp_reserved_mem = CP_MEM_RESERVE_ON_2;
	is_verified = info->verify_input(info, &cp_reserved_mem);
	KUNIT_EXPECT_TRUE(test, is_verified);
}

static void test__qc_param_verify_FMM_lock(struct kunit *test)
{
	struct qc_param_info _info =
			__QC_PARAM_INFO(sapa, __qc_param_verify_FMM_lock);
	struct qc_param_info *info = &_info;
	bool is_verified;
	unsigned int fmm_lock_magic;

	fmm_lock_magic = (unsigned int)-1;
	is_verified = info->verify_input(info, &fmm_lock_magic);
	KUNIT_EXPECT_FALSE(test, is_verified);

	fmm_lock_magic = 0;
	is_verified = info->verify_input(info, &fmm_lock_magic);
	KUNIT_EXPECT_TRUE(test, is_verified);

	fmm_lock_magic = FMMLOCK_MAGIC_NUM;
	is_verified = info->verify_input(info, &fmm_lock_magic);
	KUNIT_EXPECT_TRUE(test, is_verified);
}

static void test__qc_param_verify_fiemap_update(struct kunit *test)
{
	struct qc_param_info _info =
			__QC_PARAM_INFO(sapa, __qc_param_verify_fiemap_update);
	struct qc_param_info *info = &_info;
	bool is_verified;
	unsigned int edtbo_fiemap_magic;

	edtbo_fiemap_magic = (unsigned int)-1;
	is_verified = info->verify_input(info, &edtbo_fiemap_magic);
	KUNIT_EXPECT_FALSE(test, is_verified);

	edtbo_fiemap_magic = 0;
	is_verified = info->verify_input(info, &edtbo_fiemap_magic);
	KUNIT_EXPECT_TRUE(test, is_verified);

	edtbo_fiemap_magic = EDTBO_FIEMAP_MAGIC;
	is_verified = info->verify_input(info, &edtbo_fiemap_magic);
	KUNIT_EXPECT_TRUE(test, is_verified);
}

static void test__qc_param_is_valid_index(struct kunit *test)
{
	bool is_valid;

	is_valid = __qc_param_is_valid_index((size_t)-1);
	KUNIT_EXPECT_FALSE(test, is_valid);

	is_valid = __qc_param_is_valid_index(param_num_of_param_index);
	KUNIT_EXPECT_FALSE(test, is_valid);

	is_valid = __qc_param_is_valid_index(0);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = __qc_param_is_valid_index(param_index_debuglevel);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = __qc_param_is_valid_index(param_vib_le_est);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = __qc_param_is_valid_index(param_num_of_param_index - 1);
	KUNIT_EXPECT_TRUE(test, is_valid);
}

static struct sec_of_kunit_data *sec_qc_param_testdata;
SEC_OF_KUNIT_DTB_INFO_EXTERN(sec_qc_param_test);
static SEC_OF_KUNIT_DTB_INFO(sec_qc_param_test);

static int __qc_param_test_suite_set_up(struct sec_of_kunit_data *testdata)
{
	struct qc_param_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	return sec_of_kunit_data_init(testdata,
			"kunit:samsung,qcom-param_test", &drvdata->bd,
			"samsung,qcom-param", &sec_qc_param_test_info);
}

static void __qc_param_test_suite_tear_down(struct sec_of_kunit_data *testdata)
{
	struct qc_param_drvdata *drvdata = container_of(testdata->bd,
			struct qc_param_drvdata, bd);

	kfree(drvdata);

	sec_of_kunit_data_exit(testdata);
}

static int sec_qc_param_test_init(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_qc_param_testdata;

	/* FIXME: 'misc_register' can't be call in 'suite_init' call-back */
	if (!testdata) {
		int err;

		testdata = kzalloc(sizeof(*testdata), GFP_KERNEL);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, testdata);

		err = __qc_param_test_suite_set_up(testdata);
		KUNIT_EXPECT_EQ(test, 0, err);

		sec_qc_param_testdata = testdata;
	}

	test->priv = testdata;

	return 0;
}

static void tear_down_sec_qc_param_test(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_qc_param_testdata;

	__qc_param_test_suite_tear_down(testdata);
	kfree_sensitive(testdata);

	sec_qc_param_testdata = NULL;
}

static struct kunit_case sec_qc_param_test_cases[] = {
	KUNIT_CASE(test__qc_param_parse_dt_bdev_path),
	KUNIT_CASE(test__qc_param_parse_dt_negative_offset),
	KUNIT_CASE(test__qc_param_verify_debuglevel),
	KUNIT_CASE(test__qc_param_verify_sapa),
	KUNIT_CASE(test__qc_param_verify_afc_disable),
	KUNIT_CASE(test__qc_param_verify_pd_disable),
	KUNIT_CASE(test__qc_param_verify_cp_reserved_mem),
	KUNIT_CASE(test__qc_param_verify_FMM_lock),
	KUNIT_CASE(test__qc_param_verify_fiemap_update),
	KUNIT_CASE(test__qc_param_is_valid_index),
	KUNIT_CASE(tear_down_sec_qc_param_test),
	{},
};

struct kunit_suite sec_qc_param_test_suite = {
	.name = "sec_qc_param_test",
	.init = sec_qc_param_test_init,
	.test_cases = sec_qc_param_test_cases,
};

kunit_test_suites(&sec_qc_param_test_suite);
