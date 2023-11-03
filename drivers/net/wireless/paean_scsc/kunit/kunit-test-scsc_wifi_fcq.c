// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "../scsc_wifi_fcq.c"

static int scsc_wifi_fcq_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void scsc_wifi_fcq_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case scsc_wifi_fcq_test_cases[] = {
	{}
};

static struct kunit_suite scsc_wifi_fcq_test_suite[] = {
	{
		.name = "kunit-scsc_wifi_fcq-test",
		.test_cases = scsc_wifi_fcq_test_cases,
		.init = scsc_wifi_fcq_test_init,
		.exit = scsc_wifi_fcq_test_exit,
	}
};

kunit_test_suites(scsc_wifi_fcq_test_suite);
