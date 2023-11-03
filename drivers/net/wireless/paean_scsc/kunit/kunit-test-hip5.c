// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "../hip5.c"

static int hip5_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void hip5_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case hip5_test_cases[] = {
	{}
};

static struct kunit_suite hip5_test_suite[] = {
	{
		.name = "kunit-hip5-test",
		.test_cases = hip5_test_cases,
		.init = hip5_test_init,
		.exit = hip5_test_exit,
	}
};

kunit_test_suites(hip5_test_suite);
