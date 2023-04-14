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

DEFINE_TC_CMD_TO_FUNCTION(appdogbark, __simulate_apps_wdog_bark);
DEFINE_TC_CMD_TO_FUNCTION(appdogbite, __simulate_apps_wdog_bite);
DEFINE_TC_CMD_TO_FUNCTION(dabort, __simulate_dabort);
DEFINE_TC_CMD_TO_FUNCTION(pabort, __simulate_pabort);
DEFINE_TC_CMD_TO_FUNCTION(undef, __simulate_undef);
DEFINE_TC_CMD_TO_FUNCTION(bushang, __simulate_bus_hang);
DEFINE_TC_CMD_TO_FUNCTION(dblfree, __simulate_dblfree);
DEFINE_TC_CMD_TO_FUNCTION(danglingref, __simulate_danglingref);
DEFINE_TC_CMD_TO_FUNCTION(lowmem, __simulate_lowmem);
DEFINE_TC_CMD_TO_FUNCTION(memcorrupt, __simulate_memcorrupt);
DEFINE_TC_CMD_TO_FUNCTION(KP, __simulate_dabort);
DEFINE_TC_CMD_TO_FUNCTION(DP, __simulate_apps_wdog_bark);

static int sec_force_err_test_init(struct kunit *test)
{
	int err;

	err = sec_force_err_init();
	KUNIT_EXPECT_EQ(test, err, 0);

	return 0;
}

static void sec_force_err_test_exit(struct kunit *test)
{
	sec_force_err_exit();
}

static struct kunit_case sec_force_err_test_cases[] = {
	KUNIT_CASE(test_case_0_appdogbark),
	KUNIT_CASE(test_case_0_appdogbite),
	KUNIT_CASE(test_case_0_dabort),
	KUNIT_CASE(test_case_0_pabort),
	KUNIT_CASE(test_case_0_undef),
	KUNIT_CASE(test_case_0_bushang),
	KUNIT_CASE(test_case_0_dblfree),
	KUNIT_CASE(test_case_0_danglingref),
	KUNIT_CASE(test_case_0_lowmem),
	KUNIT_CASE(test_case_0_memcorrupt),
	KUNIT_CASE(test_case_0_KP),
	KUNIT_CASE(test_case_0_DP),
	{}
};

static struct kunit_suite sec_force_err_test_suite = {
	.name = "SEC Generating force errors",
	.init = sec_force_err_test_init,
	.exit = sec_force_err_test_exit,
	.test_cases = sec_force_err_test_cases,
};

kunit_test_suites(&sec_force_err_test_suite);
