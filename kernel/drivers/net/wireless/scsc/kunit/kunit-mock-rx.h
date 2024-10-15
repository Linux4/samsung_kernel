/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SAP_RX_H__
#define __KUNIT_MOCK_SAP_RX_H__

#include <linux/skbuff.h>

#include "../dev.h"
#include "../sap_mlme.h"

#define slsi_rx_scan_ind(args...)			kunit_mock_slsi_rx_scan_ind(args)
#define slsi_rx_scan_done_ind(args...)			kunit_mock_slsi_rx_scan_done_ind(args)
#define slsi_rx_connect_ind(args...)			kunit_mock_slsi_rx_connect_ind(args)
#define slsi_rx_connected_ind(args...)			kunit_mock_slsi_rx_connected_ind(args)
#define slsi_rx_received_frame_ind(args...)		kunit_mock_slsi_rx_received_frame_ind(args)
#define slsi_rx_disconnected_ind(args...)		kunit_mock_slsi_rx_disconnected_ind(args)
#define slsi_rx_procedure_started_ind(args...)		kunit_mock_slsi_rx_procedure_started_ind(args)
#define slsi_rx_frame_transmission_ind(args...)		kunit_mock_slsi_rx_frame_transmission_ind(args)
#define slsi_rx_blockack_ind(args...)			kunit_mock_slsi_rx_blockack_ind(args)
#define slsi_rx_roamed_ind(args...)			kunit_mock_slsi_rx_roamed_ind(args)
#define slsi_rx_roam_ind(args...)			kunit_mock_slsi_rx_roam_ind(args)
#define slsi_rx_mic_failure_ind(args...)		kunit_mock_slsi_rx_mic_failure_ind(args)
#define slsi_rx_reassoc_ind(args...)			kunit_mock_slsi_rx_reassoc_ind(args)
#define slsi_tdls_peer_ind(args...)			kunit_mock_slsi_tdls_peer_ind(args)
#define slsi_rx_listen_end_ind(args...)			kunit_mock_slsi_rx_listen_end_ind(args)
#define slsi_rx_channel_switched_ind(args...)		kunit_mock_slsi_rx_channel_switched_ind(args)
#define slsi_rx_synchronised_ind(args...)		kunit_mock_slsi_rx_synchronised_ind(args)
#define slsi_rx_beacon_reporting_event_ind(args...)	kunit_mock_slsi_rx_beacon_reporting_event_ind(args)
#define slsi_rx_blacklisted_ind(args...)		kunit_mock_slsi_rx_blacklisted_ind(args)
#define slsi_rx_rcl_ind(args...)			kunit_mock_slsi_rx_rcl_ind(args)
#define slsi_rx_start_detect_ind(args...)		kunit_mock_slsi_rx_start_detect_ind(args)
#define slsi_rx_ma_to_mlme_delba_req(args...)		kunit_mock_slsi_rx_ma_to_mlme_delba_req(args)
#define slsi_rx_twt_setup_info_event(args...)		kunit_mock_slsi_rx_twt_setup_info_event(args)
#define slsi_rx_twt_teardown_indication(args...)	kunit_mock_slsi_rx_twt_teardown_indication(args)
#define slsi_rx_twt_notification_indication(args...)	kunit_mock_slsi_rx_twt_notification_indication(args)
#define slsi_rx_scheduled_pm_teardown_indication
#define slsi_rx_scheduled_pm_leaky_ap_detect_indication
#define slsi_rx_send_frame_cfm_async(args...)		kunit_mock_slsi_rx_send_frame_cfm_async(args)
#define slsi_rx_buffered_frames(args...)		kunit_mock_slsi_rx_buffered_frames(args)
#define slsi_rx_blocking_signals(args...)		kunit_mock_slsi_rx_blocking_signals(args)
#define slsi_scan_complete(args...)			kunit_mock_slsi_scan_complete(args)
#define slsi_rx_scan_pass_to_cfg80211(args...)		kunit_mock_slsi_rx_scan_pass_to_cfg80211(args)
#define slsi_find_scan_channel(args...)			kunit_mock_slsi_find_scan_channel(args)
#define slsi_set_acl(args...)				kunit_mock_slsi_set_acl(args)


static void kunit_mock_slsi_rx_scan_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_scan_done_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_connect_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_connected_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_received_frame_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_disconnected_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_procedure_started_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_frame_transmission_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_blockack_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_roamed_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_roam_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_mic_failure_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_reassoc_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_tdls_peer_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_listen_end_ind(struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_channel_switched_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_synchronised_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_beacon_reporting_event_ind(struct slsi_dev *sdev, struct net_device *dev,
							  struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_blacklisted_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_rcl_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_start_detect_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_ma_to_mlme_delba_req(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_twt_setup_info_event(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_twt_teardown_indication(struct slsi_dev *sdev, struct net_device *dev,
						       struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_twt_notification_indication(struct slsi_dev *sdev, struct net_device *dev,
							   struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static void kunit_mock_slsi_rx_send_frame_cfm_async(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
	return;
}

static inline int kunit_mock_slsi_set_acl(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static void kunit_mock_slsi_rx_buffered_frames(struct slsi_dev *sdev, struct net_device *dev, struct slsi_peer *peer)
{
	return;
}

static int kunit_mock_slsi_rx_blocking_signals(struct slsi_dev *sdev, struct sk_buff *skb)
{
	if (skb) {
		if (!sdev)
			return -EINVAL;
		else
			return 1;
	}

	return 0;
}

static void kunit_mock_slsi_scan_complete(struct slsi_dev *sdev, struct net_device *dev, u16 scan_id, bool aborted,
			bool flush_scan_results)
{
	return;
}

static struct ieee80211_channel *kunit_mock_slsi_rx_scan_pass_to_cfg80211(struct slsi_dev *sdev, struct net_device *dev,
									  struct sk_buff *skb, bool release_skb)
{
	if (release_skb) {
		kfree_skb(skb);
		skb = NULL;
	}

	return NULL;
}

static struct ieee80211_channel *kunit_mock_slsi_find_scan_channel(struct slsi_dev *sdev, struct ieee80211_mgmt *mgmt,
								   size_t mgmt_len, u16 freq)
{
	return NULL;
}
#endif
