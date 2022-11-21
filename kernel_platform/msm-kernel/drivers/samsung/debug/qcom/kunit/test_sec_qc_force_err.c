// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2020 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/kernel.h>

#include <kunit/test.h>

extern long kunit_force_error(const char *val);
extern int sec_force_err_init(void);
extern void sec_force_err_exit(void);

extern int sec_qc_force_err_init(void);
extern void sec_qc_force_err_exit(void);

#define DEFINE_TC_CMD_TO_FUNCTION(__cmd, __func_desired) \
static void test_case_0_##__cmd(struct kunit *test) \
{ \
	long ret; \
	char *func_called; \
	\
	ret = kunit_force_error(#__cmd); \
	KUNIT_EXPECT_NE(test, ret, 0L); \
	func_called = kasprintf(GFP_KERNEL, "%ps", (void *)ret); \
	KUNIT_EXPECT_STREQ(test, func_called, #__func_desired); \
	kfree(func_called); \
}

DEFINE_TC_CMD_TO_FUNCTION(WP, __qc_simulate_apps_wdog_bite);
DEFINE_TC_CMD_TO_FUNCTION(secdogbite, __qc_simulate_secure_wdog_bite);

/* some functions in 'sec_force_err.c */
DEFINE_TC_CMD_TO_FUNCTION(KP, __simulate_dabort);
DEFINE_TC_CMD_TO_FUNCTION(DP, __simulate_apps_wdog_bark);

static int sec_qc_force_err_test_init(struct kunit *test)
{
	int err;

	err = sec_force_err_init();
	KUNIT_EXPECT_EQ(test, err, 0);

	err = sec_qc_force_err_init();
	KUNIT_EXPECT_EQ(test, err, 0);

	return 0;
}

static void sec_qc_force_err_test_exit(struct kunit *test)
{
	sec_qc_force_err_exit();
	sec_force_err_exit();
}

static struct kunit_case sec_qc_force_err_test_cases[] = {
	KUNIT_CASE(test_case_0_WP),
	KUNIT_CASE(test_case_0_secdogbite),
	KUNIT_CASE(test_case_0_KP),
	KUNIT_CASE(test_case_0_DP),
	{}
};

static struct kunit_suite sec_qc_force_err_test_suite = {
	.name = "SEC Generating force errors for Qualcomm based devices",
	.init = sec_qc_force_err_test_init,
	.exit = sec_qc_force_err_test_exit,
	.test_cases = sec_qc_force_err_test_cases,
};

kunit_test_suites(&sec_qc_force_err_test_suite);
