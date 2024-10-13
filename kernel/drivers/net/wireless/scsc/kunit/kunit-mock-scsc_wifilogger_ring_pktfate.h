/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SCSC_WIFILOGGER_RING_PKTFATE_H__
#define __KUNIT_MOCK_SCSC_WIFILOGGER_RING_PKTFATE_H__

#include "../scsc_wifilogger_ring_pktfate.h"

#define scsc_wifilogger_ring_pktfate_log_rx_frame(args...)	kunit_mock_scsc_wifilogger_ring_pktfate_log_rx_frame(args)
#define scsc_wifilogger_ring_pktfate_start_monitoring(args...)	kunit_mock_scsc_wifilogger_ring_pktfate_start_monitoring(args)
#define scsc_wifilogger_ring_pktfate_init(args...)		kunit_mock_scsc_wifilogger_ring_pktfate_init(args)
#define scsc_wifilogger_ring_pktfate_get_fates(args...)		kunit_mock_scsc_wifilogger_ring_pktfate_get_fates(args)
#define is_pktfate_monitor_started(args...)			kunit_mock_is_pktfate_monitor_started(args)
#define scsc_wifilogger_ring_pktfate_new_assoc(args...)		kunit_mock_scsc_wifilogger_ring_pktfate_new_assoc(args)
#define scsc_wifilogger_ring_pktfate_log_tx_frame(args...)	kunit_mock_scsc_wifilogger_ring_pktfate_log_tx_frame(args)


static void kunit_mock_scsc_wifilogger_ring_pktfate_log_rx_frame(wifi_rx_packet_fate fate, u16 du_desc,
								 void *frame, size_t len, bool ma_unitdata)
{
	return;
}

static void kunit_mock_scsc_wifilogger_ring_pktfate_start_monitoring(void)
{
	return;
}

static bool kunit_mock_scsc_wifilogger_ring_pktfate_init(void)
{
	return false;
}

static void kunit_mock_scsc_wifilogger_ring_pktfate_get_fates(int fate, void *report_bufs,
							      size_t n_requested_fates,
							      size_t *n_provided_fates,
							      bool is_user)
{
	return;
}

static bool kunit_mock_is_pktfate_monitor_started(void)
{
	return false;
}

static void kunit_mock_scsc_wifilogger_ring_pktfate_new_assoc(void)
{
	return;
}

static void kunit_mock_scsc_wifilogger_ring_pktfate_log_tx_frame(wifi_tx_packet_fate fate,
								 u16 htag, void *frame,
								 size_t len, bool ma_unitdata)
{
	return;
}
#endif
