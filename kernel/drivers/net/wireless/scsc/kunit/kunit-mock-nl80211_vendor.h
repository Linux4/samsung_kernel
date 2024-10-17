/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_NL80211_VENDOR_H__
#define __KUNIT_MOCK_NL80211_VENDOR_H__

#include "../nl80211_vendor.h"

#define slsi_print_event_name(args...)		kunit_mock_slsi_print_event_name(args)
#define slsi_gscan_hash_remove(args...)		kunit_mock_slsi_gscan_hash_remove(args)
#define slsi_gscan_get_scan_policy(args...)	kunit_mock_slsi_gscan_get_scan_policy(args)
#define slsi_rx_rssi_report_ind(args...)	kunit_mock_slsi_rx_rssi_report_ind(args)
#define slsi_nl80211_vendor_deinit(args...)	kunit_mock_slsi_nl80211_vendor_deinit(args)
#define slsi_tx_rate_calc(args...)		kunit_mock_slsi_tx_rate_calc(args)
#define slsi_rx_event_log_indication(args...)	kunit_mock_slsi_rx_event_log_indication(args)
#define slsi_gscan_alloc_buckets(args...)	kunit_mock_slsi_gscan_alloc_buckets(args)
#define slsi_gscan_handle_scan_result(args...)	kunit_mock_slsi_gscan_handle_scan_result(args)
#define slsi_nl80211_vendor_init(args...)	kunit_mock_slsi_nl80211_vendor_init(args)
#define slsi_rx_range_done_ind(args...)		kunit_mock_slsi_rx_range_done_ind(args)
#define slsi_vendor_event(args...)		kunit_mock_slsi_vendor_event(args)
#define slsi_rx_range_ind(args...)		kunit_mock_slsi_rx_range_ind(args)


static char *kunit_mock_slsi_print_event_name(int event_id)
{
	return NULL;
}

static void kunit_mock_slsi_gscan_hash_remove(struct slsi_dev *sdev, u8 *mac)
{
	return;
}

static u8 kunit_mock_slsi_gscan_get_scan_policy(enum wifi_band band)
{
	return 0;
}

static void kunit_mock_slsi_rx_rssi_report_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	return;
}

static void kunit_mock_slsi_nl80211_vendor_deinit(struct slsi_dev *sdev)
{
	return;
}

static int kunit_mock_slsi_tx_rate_calc(struct sk_buff *nl_skb, u16 fw_rate, int res, bool tx_rate)
{
	return 0;
}

static void kunit_mock_slsi_rx_event_log_indication(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	return;
}

static int kunit_mock_slsi_gscan_alloc_buckets(struct slsi_dev *sdev, struct slsi_gscan *gscan, int num_buckets)
{
	return 0;
}

static void kunit_mock_slsi_gscan_handle_scan_result(struct slsi_dev *sdev, struct net_device *dev,
						     struct sk_buff *skb, u16 scan_id, bool scan_done)
{
	return;
}

static void kunit_mock_slsi_nl80211_vendor_init(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_rx_range_done_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	return;
}

static int kunit_mock_slsi_vendor_event(struct slsi_dev *sdev, int event_id, const void *data, int len)
{
	return 0;
}

static void kunit_mock_slsi_rx_range_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	return;
}
#endif
