// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include <linux/samsung/sec_of_kunit.h>
#include <linux/samsung/debug/qcom/sec_qc_summary.h>

#include "sec_qc_dbg_partition_test.h"

struct qc_dbg_part_kunit_context {
	struct sec_of_kunit_data testdata;
	struct qc_dbg_part_info dbg_part_info[DEBUG_PART_MAX_TABLE];
};

static void test__dbg_part_parse_dt_bdev_path(struct kunit *test)
{
	struct qc_dbg_part_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct qc_dbg_part_drvdata *drvdata = container_of(testdata->bd,
			struct qc_dbg_part_drvdata, bd);
	int err;

	err = __dbg_part_parse_dt_bdev_path(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);
	KUNIT_EXPECT_STREQ(test, "PARTUUID=eb1e1dc7-3487-4854-a04f-097cc0c43c1d",
			drvdata->bdev_path);
}

static void test____dbg_part_parse_dt_part_table(struct kunit *test)
{
	struct qc_dbg_part_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct qc_dbg_part_info *info = testctx->dbg_part_info;
	int err;

	err = ____dbg_part_parse_dt_part_table(testdata->bd, testdata->of_node,
			info);
	KUNIT_EXPECT_EQ(test, 0, err);

	KUNIT_EXPECT_EQ(test, 0x0,
			info[debug_index_reset_header].offset);
	KUNIT_EXPECT_EQ(test, sizeof(struct debug_reset_header),
			info[debug_index_reset_header].size);

	KUNIT_EXPECT_EQ(test, 0x1000,
			info[debug_index_reset_ex_info].offset);
	KUNIT_EXPECT_EQ(test, sizeof(rst_exinfo_t),
			info[debug_index_reset_ex_info].size);

	KUNIT_EXPECT_EQ(test, 0x2000,
			info[debug_index_ap_health].offset);
	KUNIT_EXPECT_EQ(test, sizeof(ap_health_t),
			info[debug_index_ap_health].size);

	KUNIT_EXPECT_EQ(test, 0x3000,
			info[debug_index_lcd_debug_info].offset);
	KUNIT_EXPECT_EQ(test, sizeof(struct lcd_debug_t),
			info[debug_index_lcd_debug_info].size);

	KUNIT_EXPECT_EQ(test, 0x4000,
			info[debug_index_reset_history].offset);
	KUNIT_EXPECT_EQ(test, SEC_DEBUG_RESET_HISTORY_SIZE,
			info[debug_index_reset_history].size);

	KUNIT_EXPECT_EQ(test, 0x8F000,
			info[debug_index_onoff_history].offset);
	KUNIT_EXPECT_EQ(test, sizeof(onoff_history_t),
			info[debug_index_onoff_history].size);

	KUNIT_EXPECT_EQ(test, 0x90000,
			info[debug_index_reset_tzlog].offset);
	KUNIT_EXPECT_EQ(test, 0x3000,
			info[debug_index_reset_tzlog].size);

	KUNIT_EXPECT_EQ(test, 0x93000,
			info[debug_index_reset_extrc_info].offset);
	KUNIT_EXPECT_EQ(test, SEC_DEBUG_RESET_EXTRC_SIZE,
			info[debug_index_reset_extrc_info].size);

	KUNIT_EXPECT_EQ(test, 0x94000,
			info[debug_index_auto_comment].offset);
	KUNIT_EXPECT_EQ(test, SEC_DEBUG_AUTO_COMMENT_SIZE,
			info[debug_index_auto_comment].size);

	KUNIT_EXPECT_EQ(test, 0x95000,
			info[debug_index_reset_rkplog].offset);
	KUNIT_EXPECT_EQ(test, SEC_DEBUG_RESET_ETRM_SIZE,
			info[debug_index_reset_rkplog].size);

	KUNIT_EXPECT_EQ(test, 0xFD000,
			info[debug_index_modem_info].offset);
	KUNIT_EXPECT_EQ(test, sizeof(struct sec_qc_summary_data_modem),
			info[debug_index_modem_info].size);

	KUNIT_EXPECT_EQ(test, 0x100000,
			info[debug_index_reset_klog].offset);
	KUNIT_EXPECT_EQ(test, SEC_DEBUG_RESET_KLOG_SIZE,
			info[debug_index_reset_klog].size);

	KUNIT_EXPECT_EQ(test, 0x300000,
			info[debug_index_reset_lpm_klog].offset);
	KUNIT_EXPECT_EQ(test, SEC_DEBUG_RESET_LPM_KLOG_SIZE,
			info[debug_index_reset_lpm_klog].size);

	KUNIT_EXPECT_EQ(test, 0x500000,
			info[debug_index_reset_summary].offset);
	KUNIT_EXPECT_EQ(test, 0x300000,
			info[debug_index_reset_summary].size);

	/* RESERVED entries */
	KUNIT_EXPECT_EQ(test, 0x0,
			info[debug_index_reserve_0].offset);
	KUNIT_EXPECT_EQ(test, 0x0,
			info[debug_index_reserve_0].size);

	KUNIT_EXPECT_EQ(test, 0x0,
			info[debug_index_reserve_1].offset);
	KUNIT_EXPECT_EQ(test, 0x0,
			info[debug_index_reserve_1].size);
}

static void test____dbg_part_is_valid_index(struct kunit *test)
{
	struct qc_dbg_part_kunit_context *testctx = test->priv;
	const struct qc_dbg_part_info *info = testctx->dbg_part_info;
	bool is_valid;

	is_valid = ____dbg_part_is_valid_index(debug_index_reset_summary_info, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_reset_header, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_reset_ex_info, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_ap_health, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_lcd_debug_info, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_reset_history, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_onoff_history, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_reset_tzlog, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_reset_extrc_info, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_auto_comment, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_reset_rkplog, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_modem_info, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_reset_klog, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_reset_lpm_klog, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_reset_summary, info);
	KUNIT_EXPECT_TRUE(test, is_valid);

	/* RESERVED entries */
	is_valid = ____dbg_part_is_valid_index(debug_index_reserve_0, info);
	KUNIT_EXPECT_FALSE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_reserve_1, info);
	KUNIT_EXPECT_FALSE(test, is_valid);

	is_valid = ____dbg_part_is_valid_index(debug_index_max, info);
	KUNIT_EXPECT_FALSE(test, is_valid);
}


static void test__dbg_part_get_size(struct kunit *test)
{
	struct qc_dbg_part_kunit_context *testctx = test->priv;
	struct qc_dbg_part_info *info = testctx->dbg_part_info;
	ssize_t size;

	size = __dbg_part_get_size(debug_index_reset_header, info);
	KUNIT_EXPECT_EQ(test, sizeof(struct debug_reset_header), size);

	size = __dbg_part_get_size(debug_index_reset_ex_info, info);
	KUNIT_EXPECT_EQ(test, sizeof(rst_exinfo_t), size);

	size = __dbg_part_get_size(debug_index_ap_health, info);
	KUNIT_EXPECT_EQ(test, sizeof(ap_health_t), size);

	size = __dbg_part_get_size(debug_index_lcd_debug_info, info);
	KUNIT_EXPECT_EQ(test, sizeof(struct lcd_debug_t), size);

	size = __dbg_part_get_size(debug_index_reset_history, info);
	KUNIT_EXPECT_EQ(test, SEC_DEBUG_RESET_HISTORY_SIZE, size);

	size = __dbg_part_get_size(debug_index_onoff_history, info);
	KUNIT_EXPECT_EQ(test, sizeof(onoff_history_t), size);

	size = __dbg_part_get_size(debug_index_reset_tzlog, info);
	KUNIT_EXPECT_EQ(test, 0x3000, size);

	size = __dbg_part_get_size(debug_index_reset_extrc_info, info);
	KUNIT_EXPECT_EQ(test, SEC_DEBUG_RESET_EXTRC_SIZE, size);

	size = __dbg_part_get_size(debug_index_auto_comment, info);
	KUNIT_EXPECT_EQ(test, SEC_DEBUG_AUTO_COMMENT_SIZE, size);

	size = __dbg_part_get_size(debug_index_reset_rkplog, info);
	KUNIT_EXPECT_EQ(test, SEC_DEBUG_RESET_ETRM_SIZE, size);

	size = __dbg_part_get_size(debug_index_modem_info, info);
	KUNIT_EXPECT_EQ(test, sizeof(struct sec_qc_summary_data_modem), size);

	size = __dbg_part_get_size(debug_index_reset_klog, info);
	KUNIT_EXPECT_EQ(test, SEC_DEBUG_RESET_KLOG_SIZE, size);

	size = __dbg_part_get_size(debug_index_reset_lpm_klog, info);
	KUNIT_EXPECT_EQ(test, SEC_DEBUG_RESET_LPM_KLOG_SIZE, size);

	size = __dbg_part_get_size(debug_index_reset_summary, info);
	KUNIT_EXPECT_EQ(test, 0x300000, size);

	/* RESERVED entries */
	size = __dbg_part_get_size(debug_index_reserve_0, info);
	KUNIT_EXPECT_EQ(test, -EINVAL, size);

	size = __dbg_part_get_size(debug_index_reserve_1, info);
	KUNIT_EXPECT_EQ(test, -EINVAL, size);
}

static struct qc_dbg_part_kunit_context *sec_qc_dbg_partition_testctx;
SEC_OF_KUNIT_DTB_INFO_EXTERN(sec_qc_dbg_partition_test);
static SEC_OF_KUNIT_DTB_INFO(sec_qc_dbg_partition_test);

static int __qc_dbg_partition_test_suite_set_up(struct sec_of_kunit_data *testdata)
{
	struct qc_dbg_part_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	return sec_of_kunit_data_init(testdata,
			"kunit:samsung,qcom-debug_partition_test", &drvdata->bd,
			"samsung,qcom-debug_partition", &sec_qc_dbg_partition_test_info);
}

static void __qc_dbg_partition_test_suite_tear_down(struct sec_of_kunit_data *testdata)
{
	struct qc_dbg_part_drvdata *drvdata = container_of(testdata->bd,
			struct qc_dbg_part_drvdata, bd);

	kfree(drvdata);

	sec_of_kunit_data_exit(testdata);
}

static void __qc_dbg_partition_test_init_dbg_part_info(
		struct qc_dbg_part_kunit_context *testctx)
{
	struct qc_dbg_part_info *info = testctx->dbg_part_info;
	size_t i;

	memcpy(info, dbg_part_info, sizeof(dbg_part_info));

	for (i = 0; i < DEBUG_PART_MAX_TABLE; i++)
		info[i].offset = DEBUG_PART_OFFSET_FROM_DT;

	info[debug_index_reserve_0].offset = 0x0;
	info[debug_index_reserve_1].offset = 0x0;

	info[debug_index_reset_tzlog].size = DEBUG_PART_SIZE_FROM_DT;
	info[debug_index_reset_summary].size = DEBUG_PART_SIZE_FROM_DT;
}

static int sec_qc_dbg_partition_test_case_init(struct kunit *test)
{
	struct qc_dbg_part_kunit_context *testctx = sec_qc_dbg_partition_testctx;
	struct sec_of_kunit_data *testdata;

	if (!testctx) {
		int err;

		testctx = kzalloc(sizeof(*testctx), GFP_KERNEL);
		if (!testctx)
			return -ENOMEM;

		testdata = &testctx->testdata;
		err = __qc_dbg_partition_test_suite_set_up(testdata);
		KUNIT_EXPECT_EQ(test, 0, err);

		__qc_dbg_partition_test_init_dbg_part_info(testctx);

		sec_qc_dbg_partition_testctx = testctx;
	}

	test->priv = testctx;

	return 0;
}

static void tear_down_sec_qc_dbg_partition_test(struct kunit* test)
{
	struct qc_dbg_part_kunit_context *testctx = sec_qc_dbg_partition_testctx;
	struct sec_of_kunit_data *testdata = &testctx->testdata;

	__qc_dbg_partition_test_suite_tear_down(testdata);
	kfree_sensitive(testctx);

	sec_qc_dbg_partition_testctx = NULL;
}

static struct kunit_case sec_qc_dbg_partition_test_cases[] = {
	KUNIT_CASE(test__dbg_part_parse_dt_bdev_path),
	KUNIT_CASE(test____dbg_part_parse_dt_part_table),
	KUNIT_CASE(test____dbg_part_is_valid_index),
	KUNIT_CASE(test__dbg_part_get_size),
	KUNIT_CASE(tear_down_sec_qc_dbg_partition_test),
	{},
};

struct kunit_suite sec_qc_dbg_partition_test_suite = {
	.name = "sec_qc_dbg_partition_test",
	.init = sec_qc_dbg_partition_test_case_init,
	.test_cases = sec_qc_dbg_partition_test_cases,
};

kunit_test_suites(&sec_qc_dbg_partition_test_suite);
