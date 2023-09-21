/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_NL80211_NAN_H__
#define __KUNIT_MOCK_NL80211_NAN_H__

#include "../nl80211_vendor_nan.h"

#define slsi_nan_service_ind(args...)			kunit_mock_slsi_nan_service_ind(args)
#define slsi_nan_get_capabilities(args...)		kunit_mock_slsi_nan_get_capabilities(args)
#define slsi_nan_del_peer(args...)			kunit_mock_slsi_nan_del_peer(args)
#define slsi_nan_ndp_setup_ind(args...)			kunit_mock_slsi_nan_ndp_setup_ind(args)
#define slsi_nan_set_config(args...)			kunit_mock_slsi_nan_set_config(args)
#define slsi_nan_ndp_new_entry(args...)			kunit_mock_slsi_nan_ndp_new_entry(args)
#define slsi_nan_get_ndp_from_ndl_local_ndi(args...)	kunit_mock_slsi_nan_get_ndp_from_ndl_local_ndi(args)
#define slsi_nan_followup_ind(args...)			kunit_mock_slsi_nan_followup_ind(args)
#define slsi_nan_ndp_end(args...)			kunit_mock_slsi_nan_ndp_end(args)
#define slsi_nan_enable(args...)			kunit_mock_slsi_nan_enable(args)
#define slsi_nan_ndp_termination_ind(args...)		kunit_mock_slsi_nan_ndp_termination_ind(args)
#define slsi_nan_publish(args...)			kunit_mock_slsi_nan_publish(args)
#define slsi_nan_pop_followup_ids(args...)		kunit_mock_slsi_nan_pop_followup_ids(args)
#define slsi_nan_get_netdev(args...)			kunit_mock_slsi_nan_get_netdev(args)
#define slsi_nan_subscribe_cancel(args...)		kunit_mock_slsi_nan_subscribe_cancel(args)
#define slsi_rx_nan_range_ind(args...)			kunit_mock_slsi_rx_nan_range_ind(args)
#define slsi_nan_ndp_del_entry(args...)			kunit_mock_slsi_nan_ndp_del_entry(args)
#define slsi_nan_publish_cancel(args...)		kunit_mock_slsi_nan_publish_cancel(args)
#define slsi_nan_transmit_followup(args...)		kunit_mock_slsi_nan_transmit_followup(args)
#define slsi_send_nan_range_config(args...)		kunit_mock_slsi_send_nan_range_config(args)
#define slsi_nan_ndp_initiate(args...)			kunit_mock_slsi_nan_ndp_initiate(args)
#define slsi_nan_subscribe(args...)			kunit_mock_slsi_nan_subscribe(args)
#define slsi_send_nan_range_cancel(args...)		kunit_mock_slsi_send_nan_range_cancel(args)
#define slsi_nan_data_iface_delete(args...)		kunit_mock_slsi_nan_data_iface_delete(args)
#define slsi_nan_event(args...)				kunit_mock_slsi_nan_event(args)
#define slsi_nan_data_iface_create(args...)		kunit_mock_slsi_nan_data_iface_create(args)
#define slsi_nan_push_followup_ids(args...)		kunit_mock_slsi_nan_push_followup_ids(args)
#define slsi_nan_disable(args...)			kunit_mock_slsi_nan_disable(args)
#define slsi_nan_ndp_termination_handler(args...)	kunit_mock_slsi_nan_ndp_termination_handler(args)
#define slsi_nan_get_mac(args...)			kunit_mock_slsi_nan_get_mac(args)
#define slsi_nan_ndp_requested_ind(args...)		kunit_mock_slsi_nan_ndp_requested_ind(args)
#define slsi_nan_ndp_respond(args...)			kunit_mock_slsi_nan_ndp_respond(args)
#define slsi_nan_send_disabled_event(args...)		kunit_mock_slsi_nan_send_disabled_event(args)


static void kunit_mock_slsi_nan_service_ind(struct slsi_dev *sdev, struct net_device *dev,
					    struct sk_buff *skb)
{
	return;
}

static int kunit_mock_slsi_nan_get_capabilities(struct wiphy *wiphy, struct wireless_dev *wdev,
						const void *data, int len)
{
	return 0;
}

static void kunit_mock_slsi_nan_del_peer(struct slsi_dev *sdev, struct net_device *dev,
					 u8 *local_ndi, u16 ndp_instance_id)
{
	return;
}

static void kunit_mock_slsi_nan_ndp_setup_ind(struct slsi_dev *sdev, struct net_device *dev,
					      struct sk_buff *skb, bool is_req_ind)
{
	return;
}

static int kunit_mock_slsi_nan_set_config(struct wiphy *wiphy, struct wireless_dev *wdev,
					  const void *data, int len)
{
	return 0;
}

static int kunit_mock_slsi_nan_ndp_new_entry(struct slsi_dev *sdev, struct net_device *dev,
					     u32 ndp_instance_id, u16 ndl_vif_id,
					     u8 *local_ndi, u8 *peer_nmi)
{
	return 0;
}

static u32 kunit_mock_slsi_nan_get_ndp_from_ndl_local_ndi(struct net_device *dev,
							  u16 ndl_vif_id, u8 *local_ndi)
{
	return 0;
}

static void kunit_mock_slsi_nan_followup_ind(struct slsi_dev *sdev, struct net_device *dev,
					     struct sk_buff *skb)
{
	return;
}

static int kunit_mock_slsi_nan_ndp_end(struct wiphy *wiphy, struct wireless_dev *wdev,
				       const void *data, int len)
{
	return 0;
}

static int kunit_mock_slsi_nan_enable(struct wiphy *wiphy, struct wireless_dev *wdev,
				      const void *data, int len)
{
	return 0;
}

static void kunit_mock_slsi_nan_ndp_termination_ind(struct slsi_dev *sdev, struct net_device *dev,
						    struct sk_buff *skb)
{
	return;
}

static int kunit_mock_slsi_nan_publish(struct wiphy *wiphy, struct wireless_dev *wdev,
				       const void *data, int len)
{
	return 0;
}

static void kunit_mock_slsi_nan_pop_followup_ids(struct slsi_dev *sdev, struct net_device *dev,
						 u16 match_id)
{
	return;
}

static struct net_device *kunit_mock_slsi_nan_get_netdev(struct slsi_dev *sdev)
{
#if CONFIG_SCSC_WLAN_MAX_INTERFACES >= SLSI_NET_INDEX_NAN
	return sdev->netdev[SLSI_NET_INDEX_NAN];
#else
	return NULL;
#endif
}

static int kunit_mock_slsi_nan_subscribe_cancel(struct wiphy *wiphy, struct wireless_dev *wdev,
						const void *data, int len)
{
	return 0;
}

static void kunit_mock_slsi_rx_nan_range_ind(struct slsi_dev *sdev, struct net_device *dev,
					     struct sk_buff *skb)
{
	return;
}

static void kunit_mock_slsi_nan_ndp_del_entry(struct slsi_dev *sdev, struct net_device *dev,
					      u32 ndp_instance_id, const bool ndl_vif_locked)
{
	return;
}

static int kunit_mock_slsi_nan_publish_cancel(struct wiphy *wiphy, struct wireless_dev *wdev,
					      const void *data, int len)
{
	return 0;
}

static int kunit_mock_slsi_nan_transmit_followup(struct wiphy *wiphy, struct wireless_dev *wdev,
						 const void *data, int len)
{
	return 0;
}

static int kunit_mock_slsi_send_nan_range_config(struct slsi_dev *sdev, u8 count,
						 struct slsi_rtt_config *nl_rtt_params, int rtt_id)
{
	return 0;
}

static int kunit_mock_slsi_nan_ndp_initiate(struct wiphy *wiphy, struct wireless_dev *wdev,
					    const void *data, int len)
{
	return 0;
}

static int kunit_mock_slsi_nan_subscribe(struct wiphy *wiphy, struct wireless_dev *wdev,
					 const void *data, int len)
{
	return 0;
}

static int kunit_mock_slsi_send_nan_range_cancel(struct slsi_dev *sdev)
{
	return 0;
}

static int kunit_mock_slsi_nan_data_iface_delete(struct wiphy *wiphy, struct wireless_dev *wdev,
						 const void *data, int len)
{
	return 0;
}

static void kunit_mock_slsi_nan_event(struct slsi_dev *sdev, struct net_device *dev,
				      struct sk_buff *skb)
{
	return;
}

static int kunit_mock_slsi_nan_data_iface_create(struct wiphy *wiphy, struct wireless_dev *wdev,
						 const void *data, int len)
{
	return 0;
}

static int kunit_mock_slsi_nan_push_followup_ids(struct slsi_dev *sdev, struct net_device *dev,
						 u16 match_id, u16 trans_id)
{
	return 0;
}

static int kunit_mock_slsi_nan_disable(struct wiphy *wiphy, struct wireless_dev *wdev,
				       const void *data, int len)
{
	return 0;
}

static void kunit_mock_slsi_nan_ndp_termination_handler(struct slsi_dev *sdev, struct net_device *dev,
							u16 ndp_instance_id, u8 *ndi)
{
	return;
}

static void kunit_mock_slsi_nan_get_mac(struct slsi_dev *sdev, char *nan_mac_addr)
{
	return;
}

static void kunit_mock_slsi_nan_ndp_requested_ind(struct slsi_dev *sdev, struct net_device *dev,
						  struct sk_buff *skb)
{
	return;
}

static int kunit_mock_slsi_nan_ndp_respond(struct wiphy *wiphy, struct wireless_dev *wdev,
					   const void *data, int len)
{
	return 0;
}

static void kunit_mock_slsi_nan_send_disabled_event(struct slsi_dev *sdev, struct net_device *dev,
						    u32 reason)
{
	return;
}
#endif
