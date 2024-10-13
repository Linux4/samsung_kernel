// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "../dev.h"
#include "kunit-mock-scsc_wifilogger_core.h"
#include "kunit-mock-scsc_wifilogger_internal.h"
#include "kunit-mock-scsc_wifilogger_debugfs.h"
#include "../scsc_wifilogger_ring_connectivity.c"

/* unit test function definition*/
static void test_scsc_wifilogger_ring_connectivity_init(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, scsc_wifilogger_ring_connectivity_init());

	wr[1].registered = true;
	KUNIT_EXPECT_FALSE(test, scsc_wifilogger_ring_connectivity_init());

	//cannot create scsc_wlog_ring more than two.
	KUNIT_EXPECT_FALSE(test, scsc_wifilogger_ring_connectivity_init());
}

/**** Producer API ******/
static void test_scsc_wifilogger_ring_connectivity_fw_event(struct kunit *test)
{
	wlog_verbose_level lev = WLOG_NORMAL;
	u16 fw_event_id = MLME_CONNECTED_IND;
	u64 fw_timestamp = 0;
	char *fw_bulk_data = "";

	KUNIT_EXPECT_NE(test,
			0,
			scsc_wifilogger_ring_connectivity_fw_event(lev, fw_event_id, fw_timestamp,
								   (void *)fw_bulk_data, strlen(fw_bulk_data)));
}

static void test_scsc_wifilogger_ring_connectivity_driver_event(struct kunit *test)
{
	wlog_verbose_level lev = WLOG_NORMAL;
	u16 driver_event_id = WIFI_EVENT_ASSOCIATION_REQUESTED;
	unsigned int tag_count = 1;
	u8 tlv = 255;

	KUNIT_EXPECT_NE(test, 0,
			scsc_wifilogger_ring_connectivity_driver_event(lev, driver_event_id, tag_count, 1, 2, &tlv));
}


/* Test fictures */
static int scsc_wifilogger_ring_connectivity_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void scsc_wifilogger_ring_connectivity_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case scsc_wifilogger_ring_connectivity_test_cases[] = {
	KUNIT_CASE(test_scsc_wifilogger_ring_connectivity_init),
	KUNIT_CASE(test_scsc_wifilogger_ring_connectivity_fw_event),
	KUNIT_CASE(test_scsc_wifilogger_ring_connectivity_driver_event),
	{}
};

static struct kunit_suite scsc_wifilogger_ring_connectivity_test_suite[] = {
	{
		.name = "scsc_wifilogger_ring_connectivity-test",
		.test_cases = scsc_wifilogger_ring_connectivity_test_cases,
		.init = scsc_wifilogger_ring_connectivity_test_init,
		.exit = scsc_wifilogger_ring_connectivity_test_exit,
	}
};

kunit_test_suites(scsc_wifilogger_ring_connectivity_test_suite);
