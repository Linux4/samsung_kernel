// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include <linux/samsung/sec_kunit.h>
#include <linux/samsung/sec_of_kunit.h>

#include "sec_upload_cause_test.h"

#define CAUSE_DFLT_TYPE		0xCAFEBABE
#define CAUSE_00_TYPE		0xDEADBEEF
#define CAUSE_01_TYPE		0xCAB0CAFE

struct upload_cause_kunit_context {
	struct sec_of_kunit_data testdata;
	struct sec_upload_cause cause_dflt;
	unsigned int cause_dflt_type;
	struct sec_upload_cause cause_00;
	unsigned int cause_00_type;
	struct sec_upload_cause cause_01;
	unsigned int cause_01_type;
};

static void test__upldc_parse_dt_panic_notifier_priority(struct kunit *test)
{
	struct upload_cause_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct upload_cause_drvdata *drvdata;
	struct upload_cause_notify *notify;
	int err;

	err = __upldc_parse_dt_panic_notifier_priority(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct upload_cause_drvdata, bd);
	notify = &drvdata->notify;

	KUNIT_EXPECT_EQ(test, 255, notify->nb.priority);
}

static void test__upldc_probe_prolog(struct kunit *test)
{
	struct upload_cause_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	int err;

	err = __upldc_probe_prolog(testdata->bd);
	KUNIT_EXPECT_EQ(test, 0, err);
}

static void test__upldc_set_default_cause(struct kunit *test)
{
	struct upload_cause_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct upload_cause_drvdata *drvdata = container_of(testdata->bd,
			struct upload_cause_drvdata, bd);
	struct sec_upload_cause *cause_dflt = &testctx->cause_dflt;
	int err;

	err = __upldc_set_default_cause(drvdata, cause_dflt);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __upldc_set_default_cause(drvdata, cause_dflt);
	KUNIT_EXPECT_EQ(test, -EINVAL, err);
}

static void test__upldc_unset_default_cause(struct kunit *test)
{
	struct upload_cause_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct upload_cause_drvdata *drvdata = container_of(testdata->bd,
			struct upload_cause_drvdata, bd);
	struct sec_upload_cause *cause_dflt = &testctx->cause_dflt;
	struct sec_upload_cause anonymous_cause;
	int err;

	err = __upldc_unset_default_cause(drvdata, &anonymous_cause);
	KUNIT_EXPECT_EQ(test, -EINVAL, err);

	err = __upldc_unset_default_cause(drvdata, cause_dflt);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __upldc_unset_default_cause(drvdata, cause_dflt);
	KUNIT_EXPECT_EQ(test, -ENOENT, err);
}

static void test__upldc_add_cause(struct kunit *test)
{
	struct upload_cause_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct upload_cause_drvdata *drvdata = container_of(testdata->bd,
			struct upload_cause_drvdata, bd);
	int err;

	err = __upldc_set_default_cause(drvdata, &testctx->cause_dflt);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __upldc_add_cause(drvdata, &testctx->cause_00);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __upldc_add_cause(drvdata, &testctx->cause_01);
	KUNIT_EXPECT_EQ(test, 0, err);
}

static void test__upldc_type_to_cause(struct kunit *test)
{
	struct upload_cause_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct upload_cause_drvdata *drvdata = container_of(testdata->bd,
			struct upload_cause_drvdata, bd);
	char *cause;
	const size_t len = 64;

	cause = kunit_kmalloc(test, len, GFP_KERNEL);

	__upldc_type_to_cause(drvdata, 0, cause, len);
	KUNIT_EXPECT_STREQ(test, "unknown_reset", cause);

	__upldc_type_to_cause(drvdata, CAUSE_DFLT_TYPE, cause, len);
	KUNIT_EXPECT_STREQ(test, "unknown_reset", cause);

	__upldc_type_to_cause(drvdata, CAUSE_00_TYPE, cause, len);
	KUNIT_EXPECT_STREQ(test, "cause_00", cause);

	__upldc_type_to_cause(drvdata, CAUSE_01_TYPE, cause, len);
	KUNIT_EXPECT_STREQ(test, "cause_01", cause);
}

static void test__upldc_handle(struct kunit *test)
{
	struct upload_cause_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct upload_cause_drvdata *drvdata = container_of(testdata->bd,
			struct upload_cause_drvdata, bd);
	struct upload_cause_notify *notify = &drvdata->notify;
	int err;

	testctx->cause_dflt_type = 0;
	testctx->cause_00_type = 0;
	testctx->cause_01_type = 0;

	err = __upldc_handle(notify, "unknown_reset");
	KUNIT_EXPECT_EQ(test, SEC_UPLOAD_CAUSE_HANDLE_OK, err);
	KUNIT_EXPECT_NE(test, 0, testctx->cause_dflt_type);
	KUNIT_EXPECT_EQ(test, CAUSE_DFLT_TYPE, testctx->cause_dflt_type);

	err = __upldc_handle(notify, "cause_00");
	KUNIT_EXPECT_EQ(test, SEC_UPLOAD_CAUSE_HANDLE_OK, err);
	KUNIT_EXPECT_NE(test, 0, testctx->cause_00_type);
	KUNIT_EXPECT_EQ(test, CAUSE_00_TYPE, testctx->cause_00_type);

	err = __upldc_handle(notify, "cause_01");
	KUNIT_EXPECT_EQ(test, SEC_UPLOAD_CAUSE_HANDLE_OK, err);
	KUNIT_EXPECT_NE(test, 0, testctx->cause_01_type);
	KUNIT_EXPECT_EQ(test, CAUSE_01_TYPE, testctx->cause_01_type);
}

static void test__upldc_del_cause(struct kunit *test)
{
	struct upload_cause_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct upload_cause_drvdata *drvdata = container_of(testdata->bd,
			struct upload_cause_drvdata, bd);
	struct upload_cause_notify *notify = &drvdata->notify;
	const unsigned int untouched = 0xAA55AA55;
	int err;

	testctx->cause_dflt_type = untouched;
	testctx->cause_00_type = untouched;
	testctx->cause_01_type = untouched;

	err = __upldc_unset_default_cause(drvdata, &testctx->cause_dflt);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __upldc_handle(notify, "unknown_reset");
	KUNIT_EXPECT_EQ(test, SEC_UPLOAD_CAUSE_HANDLE_DONE, err);
	KUNIT_EXPECT_EQ(test, untouched, testctx->cause_00_type);

	err = __upldc_del_cause(drvdata, &testctx->cause_00);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __upldc_handle(notify, "cause_00");
	KUNIT_EXPECT_EQ(test, SEC_UPLOAD_CAUSE_HANDLE_DONE, err);
	KUNIT_EXPECT_EQ(test, untouched, testctx->cause_00_type);

	err = __upldc_del_cause(drvdata, &testctx->cause_01);
	KUNIT_EXPECT_EQ(test, 0, err);

	err = __upldc_handle(notify, "cause_01");
	KUNIT_EXPECT_EQ(test, SEC_UPLOAD_CAUSE_HANDLE_DONE, err);
	KUNIT_EXPECT_EQ(test, untouched, testctx->cause_01_type);
}

static struct upload_cause_kunit_context *sec_upload_cause_testctx;
SEC_OF_KUNIT_DTB_INFO_EXTERN(sec_upload_cause_test);
static SEC_OF_KUNIT_DTB_INFO(sec_upload_cause_test);

static int __upload_cause_test_suite_set_up(struct sec_of_kunit_data *testdata)
{
	struct upload_cause_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	return sec_of_kunit_data_init(testdata,
			"kunit:samsung,upload_cause_test", &drvdata->bd,
			"samsung,upload_cause", &sec_upload_cause_test_info);
}

static void __upload_cause_test_suite_tear_down(struct sec_of_kunit_data *testdata)
{
	struct upload_cause_drvdata *drvdata = container_of(testdata->bd,
			struct upload_cause_drvdata, bd);

	kfree(drvdata);

	sec_of_kunit_data_exit(testdata);
}

static int test_default_cause_func(const struct sec_upload_cause *uc,
		const char *cause)
{
	/* FIXME: "const cast" is bad */
	struct upload_cause_kunit_context *testctx = (struct upload_cause_kunit_context *)
			container_of(uc, struct upload_cause_kunit_context, cause_dflt);

	testctx->cause_dflt_type = uc->type;

	return SEC_UPLOAD_CAUSE_HANDLE_OK;
}

static int test_cause_00_func(const struct sec_upload_cause *uc,
		const char *cause)
{
	/* FIXME: "const cast" is bad */
	struct upload_cause_kunit_context *testctx = (struct upload_cause_kunit_context *)
			container_of(uc, struct upload_cause_kunit_context, cause_00);

	if (strcmp(uc->cause, cause))
		return SEC_UPLOAD_CAUSE_HANDLE_DONE;

	testctx->cause_00_type = uc->type;

	return SEC_UPLOAD_CAUSE_HANDLE_OK;
}

static int test_cause_01_func(const struct sec_upload_cause *uc,
		const char *cause)
{
	/* FIXME: "const cast" is bad */
	struct upload_cause_kunit_context *testctx = (struct upload_cause_kunit_context *)
			container_of(uc, struct upload_cause_kunit_context, cause_01);

	if (strcmp(uc->cause, cause))
		return SEC_UPLOAD_CAUSE_HANDLE_DONE;

	testctx->cause_01_type = uc->type;

	return SEC_UPLOAD_CAUSE_HANDLE_OK;
}

static void __upload_cause_test_init_command(struct upload_cause_kunit_context *testctx)
{
	struct sec_upload_cause *cause_dflt = &testctx->cause_dflt;
	struct sec_upload_cause *cause_00 = &testctx->cause_00;
	struct sec_upload_cause *cause_01 = &testctx->cause_01;

	cause_dflt->cause = "cafe_babe";
	cause_dflt->type = CAUSE_DFLT_TYPE;
	cause_dflt->func = test_default_cause_func;

	cause_00->cause = "cause_00";
	cause_00->type = CAUSE_00_TYPE;
	cause_00->func = test_cause_00_func;

	cause_01->cause = "cause_01";
	cause_01->type = CAUSE_01_TYPE;
	cause_01->func = test_cause_01_func;
}

static int sec_upload_cause_test_case_init(struct kunit *test)
{
	struct upload_cause_kunit_context *testctx = sec_upload_cause_testctx;
	struct sec_of_kunit_data *testdata;

	if (!testctx) {
		int err;

		testctx = kzalloc(sizeof(*testctx), GFP_KERNEL);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, testctx);

		testdata = &testctx->testdata;
		err =__upload_cause_test_suite_set_up(testdata);
		KUNIT_EXPECT_EQ(test, 0, err);

		__upload_cause_test_init_command(testctx);

		sec_upload_cause_testctx = testctx;
	}

	test->priv = testctx;

	return 0;
}

static void tear_down_sec_upload_cause_test(struct kunit *test)
{
	struct upload_cause_kunit_context *testctx = sec_upload_cause_testctx;
	struct sec_of_kunit_data *testdata = &testctx->testdata;

	__upload_cause_test_suite_tear_down(testdata);
	kfree_sensitive(testctx);

	sec_upload_cause_testctx = NULL;
}

static struct kunit_case sec_upload_cause_test_cases[] = {
	KUNIT_CASE(test__upldc_parse_dt_panic_notifier_priority),
	KUNIT_CASE(test__upldc_probe_prolog),
	KUNIT_CASE(test__upldc_set_default_cause),
	KUNIT_CASE(test__upldc_unset_default_cause),
	KUNIT_CASE(test__upldc_add_cause),
	KUNIT_CASE(test__upldc_type_to_cause),
	KUNIT_CASE(test__upldc_handle),
	KUNIT_CASE(test__upldc_del_cause),
	KUNIT_CASE(tear_down_sec_upload_cause_test),
	{},
};

struct kunit_suite sec_upload_cause_test_suite = {
	.name = "sec_upload_cause_test",
	.init = sec_upload_cause_test_case_init,
	.test_cases = sec_upload_cause_test_cases,
};

kunit_test_suites(&sec_upload_cause_test_suite);
