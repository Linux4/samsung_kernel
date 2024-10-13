// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include <linux/delay.h>
#include <linux/input.h>

#include <linux/samsung/debug/sec_debug.h>
#include <linux/samsung/debug/sec_crashkey_long.h>
#include <linux/samsung/sec_of_kunit.h>

#include "sec_crashkey_long_test.h"

struct crashkey_long_kunit_context {
	struct sec_of_kunit_data testdata;
	struct notifier_block nb;
	unsigned long notify_type;
};

static void test__crashkey_long_parse_dt_panic_msg(struct kunit *test)
{
	struct crashkey_long_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct crashkey_long_drvdata *drvdata;
	struct crashkey_long_notify *notify;
	int err;

	err = __crashkey_long_parse_dt_panic_msg(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct crashkey_long_drvdata, bd);
	notify = &drvdata->notify;

	KUNIT_EXPECT_STREQ(test, "Test Key Press", notify->panic_msg);
}

static void test__crashkey_long_parse_dt_expire_msec(struct kunit *test)
{
	struct crashkey_long_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct crashkey_long_drvdata *drvdata;
	struct crashkey_long_notify *notify;
	int err;

	err = __crashkey_long_parse_dt_expire_msec(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct crashkey_long_drvdata, bd);
	notify = &drvdata->notify;

	KUNIT_EXPECT_EQ(test, 100, notify->expire_msec);
}

static void test__crashkey_long_parse_dt_used_key(struct kunit *test)
{
	struct crashkey_long_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct crashkey_long_drvdata *drvdata;
	struct crashkey_long_keylog *keylog;
	int err;

	err = __crashkey_long_parse_dt_used_key(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct crashkey_long_drvdata, bd);
	keylog = &drvdata->keylog;

	KUNIT_EXPECT_EQ(test, 2, keylog->nr_used_key);
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, keylog->used_key);
	KUNIT_EXPECT_EQ(test, KEY_VOLUMEDOWN, keylog->used_key[0]);
	KUNIT_EXPECT_EQ(test, KEY_VOLUMEUP, keylog->used_key[1]);
}

static void test__crashkey_long_probe_prolog(struct kunit *test)
{
	struct crashkey_long_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	int err;

	err = __crashkey_long_probe_prolog(testdata->bd);
	KUNIT_EXPECT_EQ(test, 0, err);
}

static void test__crashkey_long_alloc_bitmap_received(struct kunit *test)
{
	struct crashkey_long_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct crashkey_long_drvdata *drvdata;
	struct crashkey_long_keylog *keylog;
	int err;

	err = __crashkey_long_alloc_bitmap_received(testdata->bd);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct crashkey_long_drvdata, bd);
	keylog = &drvdata->keylog;

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, keylog->bitmap_received);
	KUNIT_EXPECT_LE(test, 0, keylog->sz_bitmap);
}

static void test__crashkey_long_is_mached_received_pattern(struct kunit *test)
{
	struct crashkey_long_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct crashkey_long_drvdata *drvdata = container_of(testdata->bd,
			struct crashkey_long_drvdata, bd);
	struct sec_key_notifier_param param_press[] = {
		{ .keycode = KEY_VOLUMEDOWN, .down = 1, },
		{ .keycode = KEY_VOLUMEUP, .down = 1, },
	};
	struct sec_key_notifier_param param_release[] = {
		{ .keycode = KEY_VOLUMEDOWN, .down = 0, },
		{ .keycode = KEY_VOLUMEUP, .down = 0, },
	};
	bool is_matched;

	is_matched = __crashkey_long_is_mached_received_pattern(drvdata);
	KUNIT_EXPECT_FALSE(test, is_matched);

	__crashkey_long_update_bitmap_received(drvdata, &param_press[0]);
	is_matched = __crashkey_long_is_mached_received_pattern(drvdata);
	KUNIT_EXPECT_FALSE(test, is_matched);

	__crashkey_long_update_bitmap_received(drvdata, &param_press[1]);
	is_matched = __crashkey_long_is_mached_received_pattern(drvdata);
	KUNIT_EXPECT_TRUE(test, is_matched);

	__crashkey_long_update_bitmap_received(drvdata, &param_release[0]);
	is_matched = __crashkey_long_is_mached_received_pattern(drvdata);
	KUNIT_EXPECT_FALSE(test, is_matched);

	__crashkey_long_update_bitmap_received(drvdata, &param_release[1]);
	is_matched = __crashkey_long_is_mached_received_pattern(drvdata);
	KUNIT_EXPECT_FALSE(test, is_matched);
}

static void test__crashkey_long_key_event(struct kunit *test)
{
	struct crashkey_long_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct crashkey_long_drvdata *drvdata = container_of(testdata->bd,
			struct crashkey_long_drvdata, bd);
	struct crashkey_long_notify *notify = &drvdata->notify;
	struct notifier_block *nb = &testctx->nb;

	testctx->notify_type = (unsigned long)-1;

	__crashkey_long_add_preparing_panic(drvdata, nb);

	__crashkey_long_invoke_notifier_on_matched(notify);
	KUNIT_EXPECT_EQ(test, SEC_CRASHKEY_LONG_NOTIFY_TYPE_MATCHED,
			testctx->notify_type);

	__crashkey_long_invoke_notifier_on_unmatched(notify);
	KUNIT_EXPECT_EQ(test, SEC_CRASHKEY_LONG_NOTIFY_TYPE_UNMATCHED,
			testctx->notify_type);

	__crashkey_long_invoke_timer_on_matched(notify);
	KUNIT_EXPECT_NE(test, SEC_CRASHKEY_LONG_NOTIFY_TYPE_EXPIRED,
			testctx->notify_type);

	msleep(notify->expire_msec + 100);
	KUNIT_EXPECT_EQ(test, SEC_CRASHKEY_LONG_NOTIFY_TYPE_EXPIRED,
			testctx->notify_type);

	__crashkey_long_del_preparing_panic(drvdata, nb);

	__crashkey_long_invoke_notifier_on_matched(notify);
	KUNIT_EXPECT_NE(test, SEC_CRASHKEY_LONG_NOTIFY_TYPE_MATCHED,
			testctx->notify_type);

	__crashkey_long_invoke_notifier_on_unmatched(notify);
	KUNIT_EXPECT_NE(test, SEC_CRASHKEY_LONG_NOTIFY_TYPE_UNMATCHED,
			testctx->notify_type);
}

static struct crashkey_long_kunit_context *sec_crashkey_long_testctx;
SEC_OF_KUNIT_DTB_INFO_EXTERN(sec_crashkey_long_test);
static SEC_OF_KUNIT_DTB_INFO(sec_crashkey_long_test);

static int __crashkey_long_test_suite_set_up(struct sec_of_kunit_data *testdata)
{
	struct crashkey_long_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	return sec_of_kunit_data_init(testdata,
			"kunit:samsung,crashkey-long_test", &drvdata->bd,
			"samsung,crashkey-long", &sec_crashkey_long_test_info);
}

static void __crashkey_long_test_suite_tear_down(struct sec_of_kunit_data *testdata)
{
	struct crashkey_long_drvdata *drvdata = container_of(testdata->bd,
			struct crashkey_long_drvdata, bd);

	kfree(drvdata);

	sec_of_kunit_data_exit(testdata);
}

static int __crashkey_long_kunit_set_notify_type(struct notifier_block *this,
		unsigned long type, void *data)
{
	struct crashkey_long_kunit_context *testctx = container_of(this,
			struct crashkey_long_kunit_context, nb);

	testctx->notify_type = type;

	return NOTIFY_OK;
}

static void __crashkey_long_test_init_notifier(struct crashkey_long_kunit_context *testctx)
{
	struct notifier_block *nb = &testctx->nb;

	nb->notifier_call = __crashkey_long_kunit_set_notify_type;
}

static int sec_crashkey_long_test_case_init(struct kunit *test)
{
	struct crashkey_long_kunit_context *testctx = sec_crashkey_long_testctx;
	struct sec_of_kunit_data *testdata;

	if (!testctx) {
		int err;

		testctx = kzalloc(sizeof(*testctx), GFP_KERNEL);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, testctx);

		testdata = &testctx->testdata;
		err = __crashkey_long_test_suite_set_up(testdata);
		KUNIT_EXPECT_EQ(test, 0, err);

		__crashkey_long_test_init_notifier(testctx);

		sec_crashkey_long_testctx = testctx;
	}

	test->priv = testctx;

	return 0;
}

static void tear_down_sec_crashkey_long_test(struct kunit *test)
{
	struct crashkey_long_kunit_context *testctx = sec_crashkey_long_testctx;
	struct sec_of_kunit_data *testdata = &testctx->testdata;

	__crashkey_long_test_suite_tear_down(testdata);
	kfree_sensitive(testctx);

	sec_crashkey_long_testctx = NULL;
}

static struct kunit_case sec_crashkey_long_test_cases[] = {
	KUNIT_CASE(test__crashkey_long_parse_dt_panic_msg),
	KUNIT_CASE(test__crashkey_long_parse_dt_expire_msec),
	KUNIT_CASE(test__crashkey_long_parse_dt_used_key),
	KUNIT_CASE(test__crashkey_long_probe_prolog),
	KUNIT_CASE(test__crashkey_long_alloc_bitmap_received),
	KUNIT_CASE(test__crashkey_long_is_mached_received_pattern),
	KUNIT_CASE(test__crashkey_long_key_event),
	KUNIT_CASE(tear_down_sec_crashkey_long_test),
	{},
};

struct kunit_suite sec_crashkey_long_test_suite = {
	.name = "sec_crashkey_long_test",
	.init = sec_crashkey_long_test_case_init,
	.test_cases = sec_crashkey_long_test_cases,
};

kunit_test_suites(&sec_crashkey_long_test_suite);
