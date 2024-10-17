// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "../dpd_mmap.c"

static int dpd_mmap_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void dpd_mmap_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case dpd_mmap_test_cases[] = {
	{}
};

static struct kunit_suite dpd_mmap_test_suite[] = {
	{
		.name = "kunit-dpd_mmap-test",
		.test_cases = dpd_mmap_test_cases,
		.init = dpd_mmap_test_init,
		.exit = dpd_mmap_test_exit,
	}
};

kunit_test_suites(dpd_mmap_test_suite);
