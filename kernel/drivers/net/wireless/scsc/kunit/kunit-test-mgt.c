// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-misc.h"
#include "../mgt.c"

static int mgt_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void mgt_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case mgt_test_cases[] = {
	{}
};

static struct kunit_suite mgt_test_suite[] = {
	{
		.name = "kunit-mgt-test",
		.test_cases = mgt_test_cases,
		.init = mgt_test_init,
		.exit = mgt_test_exit,
	}
};

kunit_test_suites(mgt_test_suite);
