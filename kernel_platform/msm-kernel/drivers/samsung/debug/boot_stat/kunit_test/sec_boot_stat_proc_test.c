// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>
#include <linux/sched/clock.h>

#include <linux/samsung/sec_of_kunit.h>

#include "sec_boot_stat_test.h"

static struct message_head boot_stat_test_param[] = {
	{ true, "!@Boot: ...SOME_MESSAGES...", },
	{ true, "!@Boot_SVC : ...SOME_MESSAGES...", },
	{ true, "!@Boot_DEBUG: ...SOME_MESSAGES...", },
	{ true, "!@Boot_SystemServer: ...SOME_MESSAGES...", },
};

KUNIT_ARRAY_PARAM(boot_stat_test, boot_stat_test_param, NULL);

static void test__boot_stat_is_boot_event(struct kunit *test)
{
	const struct message_head *param =
			(struct message_head *)test->param_value;
	bool is_boot_stat;

	is_boot_stat = __boot_stat_is_boot_event(param->log);
	KUNIT_EXPECT_TRUE_MSG(test, (param->expected == !!is_boot_stat),
			"failed log : %s", param->log);
}

struct boot_stat_message {
	size_t expected_type;
	const char *expected_msg;
	const char *log;
};

static struct boot_stat_message boot_stat_message_param[] = {
	{
		.expected_type = EVT_PLATFORM,
		.expected_msg = "This is '!@Boot:'",
		.log = "!@Boot: This is '!@Boot:'",
	},
	{
		.expected_type = EVT_RIL,
		.expected_msg = "This is '!@Boot_SVC:'",
		.log ="!@Boot_SVC : This is '!@Boot_SVC:'",
	},
	{
		.expected_type = EVT_DEBUG,
		.expected_msg = "This is '!@Boot_DEBUG:'",
		.log = "!@Boot_DEBUG: This is '!@Boot_DEBUG:'",
	},
	{
		.expected_type = EVT_SYSTEMSERVER,
		.expected_msg = "This is '!@Boot_SystemServer:'",
		.log = "!@Boot_SystemServer: This is '!@Boot_SystemServer:'",
	},
};

KUNIT_ARRAY_PARAM(boot_stat_message_test, boot_stat_message_param, NULL);

static void test__boot_stat_get_message_offset_from_plog(struct kunit *test)
{
	const struct boot_stat_message *param =
			(struct boot_stat_message *)test->param_value;
	size_t offset;
	ssize_t type;

	type = __boot_stat_get_message_offset_from_plog(param->log, &offset);
	KUNIT_EXPECT_NE(test, -EINVAL, type);
	KUNIT_EXPECT_EQ(test, param->expected_type, type);
	KUNIT_EXPECT_STREQ(test, param->expected_msg, &param->log[offset]);
}

KUNIT_ARRAY_PARAM(boot_event_test, boot_events, NULL);

static void test__boot_stat_record_boot_event_locked(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = test->priv;
	struct boot_stat_drvdata *drvdata = container_of(testdata->bd,
			struct boot_stat_drvdata, bd);
	struct boot_event *event = (struct boot_event *)test->param_value;
	struct boot_stat_proc *boot_stat = &drvdata->boot_stat;
	struct list_head *boot_event_head = &boot_stat->boot_event_head;
	struct boot_event *last_event;
	u64 begin, end;
	u64 __begin, __end;
	static u64 __total;
	size_t nr_trial = 1000000;
	size_t i;

	KUNIT_EXPECT_EQ(test, 0, event->ktime);

	begin = local_clock();
	__begin = get_jiffies_64();
	for (i = 0; i < nr_trial; i++)
		__boot_stat_record_boot_event_locked(boot_stat, event->message);
	__end = get_jiffies_64();
	end = local_clock();

	last_event = list_last_entry(boot_event_head, struct boot_event, list);

	KUNIT_EXPECT_NE(test, 0, event->ktime);
	KUNIT_EXPECT_LE(test, begin, event->ktime);
	KUNIT_EXPECT_GE(test, end, event->ktime);
	KUNIT_EXPECT_STREQ(test, event->message, last_event->message);

	__total += (__end - __begin);

	kunit_info(test, "total = %llu\n", (unsigned long long)__total);
}

static struct sec_of_kunit_data *sec_boot_stat_proc_testdata;

static int __boot_stat_proc_test_suite_set_up(struct sec_of_kunit_data *testdata)
{
	struct boot_stat_drvdata *drvdata;

	drvdata = kzalloc(sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	return sec_of_kunit_data_init(testdata,
			"kunit:samsung,boot_stat_test", &drvdata->bd,
			NULL, NULL);
}

static void __boot_stat_proc_test_suite_tear_down(struct sec_of_kunit_data *testdata)
{
	struct boot_stat_drvdata *drvdata = container_of(testdata->bd,
			struct boot_stat_drvdata, bd);

	kfree(drvdata);

	sec_of_kunit_data_exit(testdata);
}

static int sec_boot_stat_proc_test_case_init(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_boot_stat_proc_testdata;

	if (!testdata) {
		int err;

		testdata = kzalloc(sizeof(*testdata), GFP_KERNEL);
		KUNIT_EXPECT_NOT_ERR_OR_NULL(test, testdata);

		err = __boot_stat_proc_test_suite_set_up(testdata);
		KUNIT_EXPECT_EQ(test, 0, err);

		err = sec_boot_stat_proc_init(testdata->bd);
		KUNIT_EXPECT_EQ(test, 0, err);

		sec_boot_stat_proc_testdata = testdata;
	}

	test->priv = testdata;

	return 0;
}

static void tear_down_sec_boot_stat_test(struct kunit *test)
{
	struct sec_of_kunit_data *testdata = sec_boot_stat_proc_testdata;

	__boot_stat_proc_test_suite_tear_down(testdata);
	kfree_sensitive(testdata);

	sec_boot_stat_proc_testdata = NULL;
}

static struct kunit_case sec_boot_stat_proc_test_cases[] = {
	KUNIT_CASE_PARAM(test__boot_stat_is_boot_event, boot_stat_test_gen_params),
	KUNIT_CASE_PARAM(test__boot_stat_get_message_offset_from_plog, boot_stat_message_test_gen_params),
	KUNIT_CASE_PARAM(test__boot_stat_record_boot_event_locked, boot_event_test_gen_params),
	KUNIT_CASE(tear_down_sec_boot_stat_test),
	{},
};

struct kunit_suite sec_boot_stat_proc_test_suite = {
	.name = "sec_boot_stat_proc_test",
	.init = sec_boot_stat_proc_test_case_init,
	.test_cases = sec_boot_stat_proc_test_cases,
};

kunit_test_suites(&sec_boot_stat_proc_test_suite);
