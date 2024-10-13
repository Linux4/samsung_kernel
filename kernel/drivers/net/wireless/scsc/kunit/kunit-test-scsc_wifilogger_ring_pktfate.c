// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-scsc_wifilogger_core.h"
#include "kunit-mock-scsc_wifilogger_internal.h"
#include "kunit-mock-scsc_wifilogger_debugfs.h"
#include "../scsc_wifilogger_ring_pktfate.c"


/* unit test function definition*/
static void test_is_pktfate_monitor_started(struct kunit *test)
{
	is_pktfate_monitor_started();
}

static void test_scsc_wifilogger_ring_pktfate_init(struct kunit *test)
{
	snprintf(wr[0].st.name, RING_NAME_SZ - 1, "test1");
	snprintf(wr[1].st.name, RING_NAME_SZ - 1, "test2");
	KUNIT_EXPECT_FALSE(test, scsc_wifilogger_ring_pktfate_init());

	snprintf(wr[0].st.name, RING_NAME_SZ - 1, "");
	KUNIT_EXPECT_FALSE(test, scsc_wifilogger_ring_pktfate_init());

	snprintf(wr[0].st.name, RING_NAME_SZ - 1, "");
	snprintf(wr[1].st.name, RING_NAME_SZ - 1, "");
	wr[0].registered = true;
	KUNIT_EXPECT_FALSE(test, scsc_wifilogger_ring_pktfate_init());

	snprintf(wr[0].st.name, RING_NAME_SZ - 1, "");
	snprintf(wr[1].st.name, RING_NAME_SZ - 1, "");
	wr[1].registered = true;
	KUNIT_EXPECT_FALSE(test, scsc_wifilogger_ring_pktfate_init());

	snprintf(wr[0].st.name, RING_NAME_SZ - 1, "");
	snprintf(wr[1].st.name, RING_NAME_SZ - 1, "");
	wr[0].registered = false;
	wr[1].registered = false;
	KUNIT_EXPECT_TRUE(test, scsc_wifilogger_ring_pktfate_init());
}

static void test_scsc_wifilogger_ring_pktfate_start_monitoring(struct kunit *test)
{
	scsc_wifilogger_ring_pktfate_start_monitoring();
}

static void test_scsc_wifilogger_ring_pktfate_new_assoc(struct kunit *test)
{
	scsc_wifilogger_ring_pktfate_new_assoc();
}

static void test_scsc_wifilogger_ring_pktfate_get_fates(struct kunit *test)
{
	void *report_bufs = NULL;
	size_t n_requested_fates = 0;

	scsc_wifilogger_ring_pktfate_get_fates(TX_FATE, report_bufs,
						n_requested_fates, &n_requested_fates, true);
	scsc_wifilogger_ring_pktfate_get_fates(RX_FATE, report_bufs,
						n_requested_fates, &n_requested_fates, true);
}

static void test_scsc_wifilogger_ring_pktfate_log_tx_frame(struct kunit *test)
{
	wifi_tx_packet_fate fate = TX_PKT_FATE_DRV_QUEUED;
	u16 htag = 0;
	struct scsc_frame_info *frame = kunit_kzalloc(test, sizeof(struct scsc_frame_info), GFP_KERNEL);
	size_t len = MAX_FRAME_LEN_ETHERNET;

	scsc_wifilogger_ring_pktfate_log_tx_frame(fate, htag, (void *)frame, len, true);

	len += 1;
	scsc_wifilogger_ring_pktfate_log_tx_frame(fate, htag, (void *)frame, len, false);
}

static void test_scsc_wifilogger_ring_pktfate_log_rx_frame(struct kunit *test)
{
	wifi_rx_packet_fate fate = RX_PKT_FATE_DRV_QUEUED;
	u16 du_desc = SCSC_DUD_80211_FRAME;
	struct scsc_frame_info *frame = kunit_kzalloc(test, sizeof(struct scsc_frame_info), GFP_KERNEL);
	size_t len = MAX_FRAME_LEN_80211_MGMT;

	scsc_wifilogger_ring_pktfate_log_rx_frame(fate, du_desc, frame, len, true);

	len += 1;
	scsc_wifilogger_ring_pktfate_log_rx_frame(fate, du_desc, frame, len, false);
}


/* Test fictures */
static int scsc_wifiogger_ring_pktfate_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void scsc_wifiogger_ring_pktfate_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case scsc_wifiogger_ring_pktfate_test_cases[] = {
	KUNIT_CASE(test_is_pktfate_monitor_started),
	KUNIT_CASE(test_scsc_wifilogger_ring_pktfate_init),
	KUNIT_CASE(test_scsc_wifilogger_ring_pktfate_start_monitoring),
	KUNIT_CASE(test_scsc_wifilogger_ring_pktfate_new_assoc),
	KUNIT_CASE(test_scsc_wifilogger_ring_pktfate_get_fates),
	KUNIT_CASE(test_scsc_wifilogger_ring_pktfate_log_tx_frame),
	KUNIT_CASE(test_scsc_wifilogger_ring_pktfate_log_rx_frame),
	{}
};

static struct kunit_suite scsc_wifiogger_ring_pktfate_test_suite[] = {
	{
		.name = "scsc_wifilogger_ring_pktfate-test",
		.test_cases = scsc_wifiogger_ring_pktfate_test_cases,
		.init = scsc_wifiogger_ring_pktfate_test_init,
		.exit = scsc_wifiogger_ring_pktfate_test_exit,
	}
};

kunit_test_suites(scsc_wifiogger_ring_pktfate_test_suite);
