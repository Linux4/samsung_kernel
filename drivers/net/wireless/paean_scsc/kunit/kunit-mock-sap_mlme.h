/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SAP_RX_H__
#define __KUNIT_MOCK_SAP_RX_H__
#include "../sap_mlme.h"

#define sap_mlme_init(args...)			kunit_mock_sap_mlme_init(args)
#define slsi_get_fapi_version_string(args...)	kunit_mock_slsi_get_fapi_version_string(args)
#define slsi_rx_enqueue_netdev_mlme(args...)	kunit_mock_slsi_rx_enqueue_netdev_mlme(args)
#define slsi_rx_netdev_mlme_work(args...)	kunit_mock_slsi_rx_netdev_mlme_work(args)
#define sap_mlme_deinit(args...)		kunit_mock_sap_mlme_deinit(args)
#define slsi_rx_blacklisted_ind(args...)		kunit_mock_slsi_rx_blacklisted_ind(args)
#define slsi_rx_start_detect_ind(args...)		kunit_mock_slsi_rx_start_detect_ind(args)
#define slsi_rx_beacon_reporting_event_ind(args...)	kunit_mock_slsi_rx_beacon_reporting_event_ind(args)
#define slsi_rx_connected_ind(args...)			kunit_mock_slsi_rx_connected_ind(args)
#define slsi_rx_procedure_started_ind(args...)		kunit_mock_slsi_rx_procedure_started_ind(args)
#define slsi_rx_frame_transmission_ind(args...)		kunit_mock_slsi_rx_frame_transmission_ind(args)
#define slsi_rx_ma_to_mlme_delba_req(args...)		kunit_mock_slsi_rx_ma_to_mlme_delba_req(args)
#define slsi_rx_received_frame_ind(args...)		kunit_mock_slsi_rx_received_frame_ind(args)
#define slsi_rx_rcl_ind(args...)			kunit_mock_slsi_rx_rcl_ind(args)
#define slsi_tdls_peer_ind(args...)			kunit_mock_slsi_tdls_peer_ind(args)
#define slsi_rx_scan_done_ind(args...)			kunit_mock_slsi_rx_scan_done_ind(args)
#define slsi_rx_twt_setup_info_event(args...)		kunit_mock_slsi_rx_twt_setup_info_event(args)
#define slsi_rx_disconnected_ind(args...)		kunit_mock_slsi_rx_disconnected_ind(args)
#define slsi_rx_roamed_ind(args...)			kunit_mock_slsi_rx_roamed_ind(args)
#define slsi_rx_scan_ind(args...)			kunit_mock_slsi_rx_scan_ind(args)
#define slsi_rx_twt_notification_indication(args...)	kunit_mock_slsi_rx_twt_notification_indication(args)
#define slsi_rx_blockack_ind(args...)			kunit_mock_slsi_rx_blockack_ind(args)
#define slsi_rx_reassoc_ind(args...)			kunit_mock_slsi_rx_reassoc_ind(args)
#define slsi_rx_channel_switched_ind(args...)		kunit_mock_slsi_rx_channel_switched_ind(args)
#define slsi_rx_twt_teardown_indication(args...)	kunit_mock_slsi_rx_twt_teardown_indication(args)
#define slsi_rx_listen_end_ind(args...)			kunit_mock_slsi_rx_listen_end_ind(args)
#define slsi_rx_synchronised_ind(args...)		kunit_mock_slsi_rx_synchronised_ind(args)
#define slsi_rx_roam_ind(args...)			kunit_mock_slsi_rx_roam_ind(args)
#define slsi_rx_connect_ind(args...)			kunit_mock_slsi_rx_connect_ind(args)
#define slsi_rx_mic_failure_ind(args...)		kunit_mock_slsi_rx_mic_failure_ind(args)


static int kunit_mock_sap_mlme_init(void)
{
	return 0;
}

static void kunit_mock_slsi_get_fapi_version_string(char *res)
{
	return;
}

static int kunit_mock_slsi_rx_enqueue_netdev_mlme(struct slsi_dev *sdev, struct sk_buff *skb, u16 vif)
{
	if (skb)
		kfree_skb(skb);
	return 0;
}

static void kunit_mock_slsi_rx_netdev_mlme_work(struct work_struct *work)
{
}

static int kunit_mock_sap_mlme_deinit(void)
{
	return 0;
}

static void kunit_mock_slsi_rx_blacklisted_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_start_detect_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_beacon_reporting_event_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_connected_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_procedure_started_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_frame_transmission_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_ma_to_mlme_delba_req(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_received_frame_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_rcl_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_tdls_peer_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_scan_done_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_twt_setup_info_event(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_disconnected_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_roamed_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_scan_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_twt_notification_indication(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_blockack_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_reassoc_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_channel_switched_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_twt_teardown_indication(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_listen_end_ind(struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_synchronised_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_roam_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_connect_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}

static void kunit_mock_slsi_rx_mic_failure_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb) {
		kfree_skb(skb);
		skb = NULL;
	}
}
#endif
