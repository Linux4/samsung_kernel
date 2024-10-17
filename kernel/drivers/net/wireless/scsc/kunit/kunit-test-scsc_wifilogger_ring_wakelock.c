// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-scsc_wifilogger_core.h"
#include "kunit-mock-scsc_wifilogger_internal.h"
#include "kunit-mock-scsc_wifilogger_debugfs.h"
#include "../scsc_wifilogger_ring_wakelock.c"


/* unit test function definition */

static void test_scsc_wifilogger_ring_wakelock_init(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, scsc_wifilogger_ring_wakelock_init());

	KUNIT_EXPECT_TRUE(test, scsc_wifilogger_ring_wakelock_init());

	KUNIT_EXPECT_FALSE(test, scsc_wifilogger_ring_wakelock_init());

	snprintf(wr[0].st.name, RING_NAME_SZ - 1, "");
	KUNIT_EXPECT_FALSE(test, scsc_wifilogger_ring_wakelock_init());

	snprintf(wr[1].st.name, RING_NAME_SZ - 1, "");
	wr[1].registered = false;
	KUNIT_EXPECT_TRUE(test, scsc_wifilogger_ring_wakelock_init());
}


static void test_scsc_wifilogger_ring_wakelock_action(struct kunit *test)
{
	u32 verbose_level = 0;
	int status = 0;
	char *wl_name = "wlan0";
	int reason = 0;
	scsc_wifilogger_ring_wakelock_action(verbose_level, status, wl_name, reason);
}


/* Test fictures */
static int scsc_wifilogger_ring_wakelock_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void scsc_wifilogger_ring_wakelock_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case scsc_wifilogger_ring_wakelock_test_cases[] = {
	KUNIT_CASE(test_scsc_wifilogger_ring_wakelock_init),
	KUNIT_CASE(test_scsc_wifilogger_ring_wakelock_action),
	{}
};

static struct kunit_suite scsc_wifilogger_ring_wakelock_test_suite[] = {
	{
		.name = "scsc_wifilogger_ring_wakelock-test",
		.test_cases = scsc_wifilogger_ring_wakelock_test_cases,
		.init = scsc_wifilogger_ring_wakelock_test_init,
		.exit = scsc_wifilogger_ring_wakelock_test_exit,
	}
};

kunit_test_suites(scsc_wifilogger_ring_wakelock_test_suite);
