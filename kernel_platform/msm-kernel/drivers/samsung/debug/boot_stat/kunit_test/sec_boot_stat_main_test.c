// SPDX-License-Identifier: GPL-2.0

#include <kunit/test.h>

#include "sec_boot_stat_test.h"

static struct message_head enh_boot_test_param[] = {
	{ true, "!@Boot_EBS_D ...SOME_MESSAGES...", },
	{ true, "!@Boot_EBS_F ...SOME_MESSAGES...", },
	{ true, "!@Boot_EBS_N ...SOME_MESSAGES...", },
	{ true, "  Boot_EBS_? ...SOME_MESSAGES...", },
	{ false, "!@BOOT_EBS_x ...SOME_MESSAGES....", },
	{ false, "!BooT_EBS_X ...SOME_MESSAGES...", },
	{ false, "!@Boot: ...SOME_MESSAGES...", },
	{ false, "!@Boot_SVC : ...SOME_MESSAGES...", },
	{ false, "!@Boot_DEBUG: ...SOME_MESSAGES...", },
	{ false, "!@Boot_SystemServer: ...SOME_MESSAGES...", },
};

KUNIT_ARRAY_PARAM(enh_boot_test, enh_boot_test_param, NULL);

static void test__is_for_enh_boot_time(struct kunit *test)
{
	const struct message_head *param =
	    (struct message_head *)test->param_value;
	bool is_enh;

	is_enh = __is_for_enh_boot_time(param->log);
	KUNIT_EXPECT_TRUE_MSG(test, (param->expected == !!is_enh),
			"failed log : %s", param->log);
}

static struct kunit_case sec_boot_stat_main_test_cases[] = {
	KUNIT_CASE_PARAM(test__is_for_enh_boot_time, enh_boot_test_gen_params),
	{},
};

struct kunit_suite sec_boot_stat_main_test_suite = {
	.name = "sec_boot_stat_main_test",
	.test_cases = sec_boot_stat_main_test_cases,
};

kunit_test_suites(&sec_boot_stat_main_test_suite);
