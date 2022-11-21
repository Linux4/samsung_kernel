// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#include <kunit/test.h>

extern bool kunit__log_buf_kmsg_check_level_text(const char *msg);

static void test_case_0__log_buf_kmsg_check_level_text(struct kunit *test)
{
	const char *msg;
	bool result;

	msg = "<0>[   90.599514] Kernel panic - not syncing";
	result = kunit__log_buf_kmsg_check_level_text(msg);
	KUNIT_EXPECT_TRUE(test, result);

	msg = "[   90.599514] Kernel panic - not syncing";
	result = kunit__log_buf_kmsg_check_level_text(msg);
	KUNIT_EXPECT_FALSE(test, result);

	msg = "0>[   90.599514] Kernel panic - not syncing";
	result = kunit__log_buf_kmsg_check_level_text(msg);
	KUNIT_EXPECT_FALSE(test, result);
}

extern void kunit__log_buf_kmsg_split(char *msg, char **head, char **tail);

static void test_case_0__log_buf_kmsg_split(struct kunit *test)
{
	const char *__msg = "<0>[   90.599514] Kernel panic - not syncing";
	const char *head_desired = "<0>[   90.599514]";
	const char *tail_desired = "Kernel panic - not syncing";
	char msg[256];
	char *head_splited;
	char *tail_splited;

	strlcpy(msg, __msg, sizeof(msg));

	kunit__log_buf_kmsg_split(msg, &head_splited, &tail_splited);

	KUNIT_EXPECT_STREQ(test, head_desired, head_splited);
	KUNIT_EXPECT_STREQ(test, tail_desired, tail_splited);
}

static struct kunit_case sec_log_buf_test_cases[] = {
	KUNIT_CASE(test_case_0__log_buf_kmsg_check_level_text),
	KUNIT_CASE(test_case_0__log_buf_kmsg_split),
	{}
};

static struct kunit_suite sec_log_buf_test_suite = {
	.name = "SEC Kernel Log Buffer",
	.test_cases = sec_log_buf_test_cases,
};

kunit_test_suites(&sec_log_buf_test_suite);
