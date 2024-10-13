// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-scsc_wifilogger_ring_connectivity.h"
#include "kunit-mock-scsc_wifilogger_ring_wakelock.h"
#include "kunit-mock-scsc_wifilogger_ring_pktfate.h"
#include "kunit-mock-scsc_wifilogger_ring_test.h"
#include "kunit-mock-scsc_wifilogger_internal.h"
#include "../scsc_wifilogger_module.c"

/* unit test function definition */
static void test_scsc_wifilogger_module_init(struct kunit *test)
{
	wifi_logger.initialized = false;
	KUNIT_EXPECT_EQ(test, 0, scsc_wifilogger_module_init());
	KUNIT_EXPECT_EQ(test, -ENOMEM, scsc_wifilogger_module_init());
}

static void test_scsc_wifilogger_module_exit(struct kunit *test)
{
	scsc_wifilogger_module_exit();
}

/* Test fictures */
static int scsc_wifilogger_module_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void scsc_wifilogger_module_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case scsc_wifilogger_module_test_cases[] = {
	KUNIT_CASE(test_scsc_wifilogger_module_init),
	KUNIT_CASE(test_scsc_wifilogger_module_exit),
	{}
};

static struct kunit_suite scsc_wifilogger_module_test_suite[] = {
	{
		.name = "scsc_wifilogger_module-test",
		.test_cases = scsc_wifilogger_module_test_cases,
		.init = scsc_wifilogger_module_test_init,
		.exit = scsc_wifilogger_module_test_exit,
	}
};

kunit_test_suites(scsc_wifilogger_module_test_suite);
