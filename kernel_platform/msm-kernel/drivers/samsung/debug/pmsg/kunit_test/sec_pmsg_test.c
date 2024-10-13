// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include <linux/samsung/debug/sec_debug.h>
#include <linux/samsung/sec_of_kunit.h>

#include "sec_pmsg_test.h"

static void test__pmsg_handle_dt_debug_level(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct pmsg_drvdata *drvdata = container_of(testdata->bd,
			struct pmsg_drvdata, bd);
	int err;

	err = __pmsg_handle_dt_debug_level(drvdata, testdata->of_node,
			SEC_DEBUG_LEVEL_LOW);
	KUNIT_EXPECT_NE(test, 0, err);
	KUNIT_EXPECT_EQ(test, -EPERM, err);

	err = __pmsg_handle_dt_debug_level(drvdata, testdata->of_node,
			SEC_DEBUG_LEVEL_MID);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __pmsg_handle_dt_debug_level(drvdata, testdata->of_node,
			SEC_DEBUG_LEVEL_HIGH);
	KUNIT_EXPECT_EQ(test, 0, err);
}

static void test____logger_level_header(struct kunit *test)
{
	struct pmsg_logger _logger;
	struct pmsg_logger *logger = &_logger;
	struct logger_level_header_ctx _llhc;
	struct logger_level_header_ctx *llhc = &_llhc;
	char *buffer;
	const size_t count = 256;
	int buffer_len;

	buffer = kunit_kzalloc(test, count, GFP_KERNEL);

	logger->tv_sec = 543210;
	logger->tv_nsec = 987654321;
	logger->pid = 12345;
	logger->tid = 6789;

	llhc->cpu = 3;
	llhc->comm = "raccoon";
	llhc->tv_kernel = 123456789012345;
	llhc->buffer = buffer;
	llhc->count = 0;

	buffer_len = ____logger_level_header(logger, llhc);
	KUNIT_EXPECT_EQ(test, 69, buffer_len);
	KUNIT_EXPECT_STREQ(test,
			"\n[123456.789012][3:         raccoon] 01-07 06:53:30.987 12345  6789  ",
			llhc->buffer);
}

static void test____logger_level_prefix(struct kunit *test)
{
	struct pmsg_logger _logger;
	struct pmsg_logger *logger = &_logger;
	char prefix;

	logger->msg[0] = 0;
	prefix = ____logger_level_prefix(logger);
	KUNIT_EXPECT_EQ(test, '!', prefix);

	logger->msg[0] = 1;
	prefix = ____logger_level_prefix(logger);
	KUNIT_EXPECT_EQ(test, '.', prefix);

	logger->msg[0] = 2;
	prefix = ____logger_level_prefix(logger);
	KUNIT_EXPECT_EQ(test, 'V', prefix);

	logger->msg[0] = 3;
	prefix = ____logger_level_prefix(logger);
	KUNIT_EXPECT_EQ(test, 'D', prefix);

	logger->msg[0] = 4;
	prefix = ____logger_level_prefix(logger);
	KUNIT_EXPECT_EQ(test, 'I', prefix);

	logger->msg[0] = 5;
	prefix = ____logger_level_prefix(logger);
	KUNIT_EXPECT_EQ(test, 'W', prefix);

	logger->msg[0] = 6;
	prefix = ____logger_level_prefix(logger);
	KUNIT_EXPECT_EQ(test, 'E', prefix);

	logger->msg[0] = 7;
	prefix = ____logger_level_prefix(logger);
	KUNIT_EXPECT_EQ(test, 'F', prefix);

	logger->msg[0] = 8;
	prefix = ____logger_level_prefix(logger);
	KUNIT_EXPECT_EQ(test, 'S', prefix);

	logger->msg[0] = 9;
	prefix = ____logger_level_prefix(logger);
	KUNIT_EXPECT_EQ(test, '?', prefix);

	logger->msg[0] = 10;
	prefix = ____logger_level_prefix(logger);
	KUNIT_EXPECT_EQ(test, '?', prefix);
}

static struct sec_of_kunit_data *sec_pmsg_testdata;
SEC_OF_KUNIT_DTB_INFO_EXTERN(sec_pmsg_test);
static SEC_OF_KUNIT_DTB_INFO(sec_pmsg_test);

static int __pmsg_test_suite_set_up(struct sec_of_kunit_data *testdata)
{
	struct pmsg_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	return sec_of_kunit_data_init(testdata,
			"kunit:samsung,pstore_pmsg_test", &drvdata->bd,
			"samsung,pstore_pmsg", &sec_pmsg_test_info);
}

static void __pmsg_test_suite_tear_down(struct sec_of_kunit_data *testdata)
{
	struct pmsg_drvdata *drvdata = container_of(testdata->bd,
			struct pmsg_drvdata, bd);

	kfree(drvdata);

	sec_of_kunit_data_exit(testdata);
}

static int sec_pmsg_test_case_init(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_pmsg_testdata;

	if (!testdata) {
		int err;

		testdata = kzalloc(sizeof(*testdata), GFP_KERNEL);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, testdata);

		err = __pmsg_test_suite_set_up(testdata);
		KUNIT_EXPECT_EQ(test, 0, err);

		sec_pmsg_testdata = testdata;
	}

	test->priv = testdata;

	return 0;
}

static void tear_down_sec_pmsg_test(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_pmsg_testdata;

	__pmsg_test_suite_tear_down(testdata);
	kfree_sensitive(testdata);

	sec_pmsg_testdata = NULL;
}

static struct kunit_case sec_pmsg_test_cases[] = {
	KUNIT_CASE(test__pmsg_handle_dt_debug_level),
	KUNIT_CASE(test____logger_level_header),
	KUNIT_CASE(test____logger_level_prefix),
	KUNIT_CASE(tear_down_sec_pmsg_test),
	{},
};

struct kunit_suite sec_pmsg_test_suite = {
	.name = "sec_pmsg_test",
	.init = sec_pmsg_test_case_init,
	.test_cases = sec_pmsg_test_cases,
};

kunit_test_suites(&sec_pmsg_test_suite);
