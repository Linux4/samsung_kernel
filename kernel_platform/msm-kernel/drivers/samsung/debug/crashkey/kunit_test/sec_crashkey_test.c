// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include <linux/delay.h>
#include <linux/input.h>

#include <linux/samsung/debug/sec_debug.h>
#include <linux/samsung/sec_of_kunit.h>

#include "sec_crashkey_test.h"

struct crashkey_kunit_context {
	struct sec_of_kunit_data testdata;
	unsigned int msecs;
	struct list_head head;
	struct notifier_block nb;
	bool notified;
};

static void test__crashkey_parse_dt_name(struct kunit *test)
{
	struct crashkey_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct crashkey_drvdata *drvdata;
	int err;

	err = __crashkey_parse_dt_name(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct crashkey_drvdata, bd);

	KUNIT_EXPECT_STREQ(test, "crashkey_test", drvdata->name);
}

static void test__crashkey_test_dt_debug_level(struct kunit *test)
{
	struct crashkey_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
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
	KUNIT_EXPECT_EQ(test, -ENODEV, err);
	KUNIT_EXPECT_NE(test, 0, err);
}

static void test__crashkey_parse_dt_panic_msg(struct kunit *test)
{
	struct crashkey_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct crashkey_drvdata *drvdata;
	struct crashkey_notify *notify;
	int err;

	err = __crashkey_parse_dt_panic_msg(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct crashkey_drvdata, bd);
	notify = &drvdata->notify;

	KUNIT_EXPECT_STREQ(test, "Crash Key Test", notify->panic_msg);
}

static void test__crashkey_parse_dt_interval(struct kunit *test)
{
	struct crashkey_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct crashkey_drvdata *drvdata;
	struct crashkey_timer *timer;
	int err;

	err = __crashkey_parse_dt_interval(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct crashkey_drvdata, bd);
	timer = &drvdata->timer;

	KUNIT_EXPECT_EQ(test, 1 * HZ, timer->interval);

	testctx->msecs = timer->interval / HZ * 1000;
}

static void test__crashkey_parse_dt_desired_pattern(struct kunit *test)
{
	struct crashkey_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct crashkey_drvdata *drvdata;
	struct crashkey_kelog *keylog;
	int err;

	err = __crashkey_parse_dt_desired_pattern(testdata->bd, testdata->of_node);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct crashkey_drvdata, bd);
	keylog = &drvdata->keylog;

	KUNIT_EXPECT_EQ(test, 5, keylog->nr_pattern);

	KUNIT_EXPECT_EQ(test, KEY_VOLUMEDOWN, keylog->desired[0].keycode);
	KUNIT_EXPECT_TRUE(test, keylog->desired[0].down);

	KUNIT_EXPECT_EQ(test, KEY_VOLUMEUP, keylog->desired[1].keycode);
	KUNIT_EXPECT_TRUE(test, keylog->desired[1].down);
	KUNIT_EXPECT_EQ(test, KEY_VOLUMEUP, keylog->desired[2].keycode);
	KUNIT_EXPECT_FALSE(test, keylog->desired[2].down);

	KUNIT_EXPECT_EQ(test, KEY_POWER, keylog->desired[3].keycode);
	KUNIT_EXPECT_TRUE(test, keylog->desired[3].down);
	KUNIT_EXPECT_EQ(test, KEY_POWER, keylog->desired[4].keycode);
	KUNIT_EXPECT_FALSE(test, keylog->desired[4].down);
}

static void test__crashkey_probe_prolog(struct kunit *test)
{
	struct crashkey_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	int err;

	err = __crashkey_probe_prolog(testdata->bd);
	KUNIT_EXPECT_EQ(test, 0, err);
}

static void test__crashkey_init_used_key(struct kunit *test)
{
	struct crashkey_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct crashkey_drvdata *drvdata;
	struct crashkey_kelog *keylog;
	int err;

	err = __crashkey_init_used_key(testdata->bd);
	KUNIT_EXPECT_EQ(test, 0, err);

	drvdata = container_of(testdata->bd, struct crashkey_drvdata, bd);
	keylog = &drvdata->keylog;

	KUNIT_EXPECT_EQ(test, 3, keylog->nr_used_key);

	/* TODO: testing 'used_key' array should be implemented at here. */
}

static void test__crashkey_find_by_name_locked(struct kunit *test)
{
	struct crashkey_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct crashkey_drvdata *drvdata;
	struct crashkey_drvdata *found;

	found = __crashkey_find_by_name_locked("crashkey_test",
			&testctx->head);
	KUNIT_EXPECT_PTR_EQ(test, ERR_PTR(-ENOENT), found);

	drvdata = container_of(testdata->bd, struct crashkey_drvdata, bd);

	__crashkey_add_to_crashkey_dev_list_locked(drvdata, &testctx->head);
	found = __crashkey_find_by_name_locked("crashkey_test",
			&testctx->head);
	KUNIT_EXPECT_PTR_EQ(test, drvdata, found);

	__crashkey_del_from_crashkey_dev_list_locked(drvdata);
	found = __crashkey_find_by_name_locked("crashkey_test",
			&testctx->head);
	KUNIT_EXPECT_PTR_EQ(test, ERR_PTR(-ENOENT), found);
}



static void __test__crashkey_notifier_call(struct crashkey_drvdata *drvdata,
		unsigned int msecs)
{
	const struct crashkey_kelog *keylog = &drvdata->keylog;
	size_t i;

	/* NOTE: before tesing, inject an incomplete pattern */
	for (i = 0; i < keylog->nr_pattern - 1; i++)
		__crashkey_notifier_call(drvdata, &keylog->desired[i]);

	/* NOTE: inject a desired pattern */
	for (i = 0; i < keylog->nr_pattern - 1; i++)
		__crashkey_notifier_call(drvdata, &keylog->desired[i]);

	msleep(msecs);

	__crashkey_notifier_call(drvdata, &keylog->desired[i]);
}

static void test__crashkey_notifier_call(struct kunit *test)
{
	struct crashkey_kunit_context *testctx = test->priv;
	struct sec_of_kunit_data *testdata = &testctx->testdata;
	struct crashkey_drvdata *drvdata = container_of(testdata->bd,
			struct crashkey_drvdata, bd);
	struct notifier_block *nb = &testctx->nb;

	__crashkey_add_preparing_panic_locked(drvdata, nb);

	testctx->notified = false;
	__test__crashkey_notifier_call(drvdata, testctx->msecs * 9 / 10);
	KUNIT_EXPECT_TRUE(test, testctx->notified);

	testctx->notified = false;
	__test__crashkey_notifier_call(drvdata, testctx->msecs * 12 / 10);
	KUNIT_EXPECT_FALSE(test, testctx->notified);

	__crashkey_del_preparing_panic_locked(drvdata, nb);

	testctx->notified = false;
	__test__crashkey_notifier_call(drvdata, testctx->msecs * 9 / 10);
	KUNIT_EXPECT_FALSE(test, testctx->notified);
}

static struct crashkey_kunit_context *sec_crashkey_testctx;
SEC_OF_KUNIT_DTB_INFO_EXTERN(sec_crashkey_test);
static SEC_OF_KUNIT_DTB_INFO(sec_crashkey_test);

static int __crashkey_test_suite_set_up(struct sec_of_kunit_data *testdata)
{
	struct crashkey_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	return sec_of_kunit_data_init(testdata,
			"kunit:samsung,crashkey_test", &drvdata->bd,
			"samsung,crashkey", &sec_crashkey_test_info);
}

static void __crashkey_test_suite_tear_down(struct sec_of_kunit_data *testdata)
{
	struct crashkey_drvdata *drvdata = container_of(testdata->bd,
			struct crashkey_drvdata, bd);

	kfree(drvdata);

	sec_of_kunit_data_exit(testdata);
}

static int __crashkey_kunit_context_set_notified_true(struct notifier_block *this,
		unsigned long type, void *data)
{
	struct crashkey_kunit_context *testctx = container_of(this,
			struct crashkey_kunit_context, nb);

	testctx->notified = true;

	return NOTIFY_OK;
}

static void __crashkey_test_init_notifier(struct crashkey_kunit_context *testctx)
{
	struct notifier_block *nb = &testctx->nb;

	nb->notifier_call = __crashkey_kunit_context_set_notified_true;
}

static int sec_crashkey_test_case_init(struct kunit *test)
{
	struct crashkey_kunit_context *testctx = sec_crashkey_testctx;
	struct sec_of_kunit_data *testdata;

	if (!testctx) {
		int err;

		testctx = kzalloc(sizeof(*testctx), GFP_KERNEL);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, testctx);

		testdata = &testctx->testdata;
		err = __crashkey_test_suite_set_up(testdata);
		KUNIT_EXPECT_EQ(test, 0, err);

		__crashkey_test_init_notifier(testctx);
		INIT_LIST_HEAD(&testctx->head);

		sec_crashkey_testctx = testctx;
	}

	test->priv = testctx;

	return 0;
}

static void tear_down_sec_crashkey_test(struct kunit *test)
{
	struct crashkey_kunit_context *testctx = sec_crashkey_testctx;
	struct sec_of_kunit_data *testdata = &testctx->testdata;

	__crashkey_test_suite_tear_down(testdata);
	kfree_sensitive(testctx);

	sec_crashkey_testctx = NULL;
}

static struct kunit_case sec_crashkey_test_cases[] = {
	KUNIT_CASE(test__crashkey_parse_dt_name),
	KUNIT_CASE(test__crashkey_test_dt_debug_level),
	KUNIT_CASE(test__crashkey_parse_dt_panic_msg),
	KUNIT_CASE(test__crashkey_parse_dt_interval),
	KUNIT_CASE(test__crashkey_parse_dt_desired_pattern),
	KUNIT_CASE(test__crashkey_probe_prolog),
	KUNIT_CASE(test__crashkey_init_used_key),
	KUNIT_CASE(test__crashkey_find_by_name_locked),
	KUNIT_CASE(test__crashkey_notifier_call),
	KUNIT_CASE(tear_down_sec_crashkey_test),
	{},
};

struct kunit_suite sec_crashkey_test_suite = {
	.name = "sec_crashkey_test",
	.init = sec_crashkey_test_case_init,
	.test_cases = sec_crashkey_test_cases,
};

kunit_test_suites(&sec_crashkey_test_suite);
