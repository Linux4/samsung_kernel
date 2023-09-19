// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "../slsi_tracepoint_debug.c"

static int slsi_tracepoint_debug_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void slsi_tracepoint_debug_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case slsi_tracepoint_debug_test_cases[] = {
	{}
};

static struct kunit_suite slsi_tracepoint_debug_test_suite[] = {
	{
		.name = "kunit-slsi_tracepoint_debug-test",
		.test_cases = slsi_tracepoint_debug_test_cases,
		.init = slsi_tracepoint_debug_test_init,
		.exit = slsi_tracepoint_debug_test_exit,
	}
};

kunit_test_suites(slsi_tracepoint_debug_test_suite);
