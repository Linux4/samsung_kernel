/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_MLME_H__
#define __KUNIT_MOCK_MLME_H__

#include "../mlme.h"
#include "../debug.h"
#include "../fapi.h"

#define slsi_mlme_set_traffic_parameters(args...)		kunit_mock_slsi_mlme_set_traffic_parameters(args)
#define slsi_mlme_read_apf_request(args...)			kunit_mock_slsi_mlme_read_apf_request(args)
#define slsi_mlme_reassociate_resp(args...)			kunit_mock_slsi_mlme_reassociate_resp(args)
#define slsi_mlme_configure_monitor_mode(args...)		kunit_mock_slsi_mlme_configure_monitor_mode(args)
#define slsi_mlme_stop_link_stats_req(args...)			kunit_mock_slsi_mlme_stop_link_stats_req(args)
#define slsi_mlme_connect_resp(args...)				kunit_mock_slsi_mlme_connect_resp(args)
#define slsi_mlme_add_sched_scan(args...)			kunit_mock_slsi_mlme_add_sched_scan(args)
#define slsi_mlme_register_action_frame(args...)		kunit_mock_slsi_mlme_register_action_frame(args)
#define slsi_mlme_set_num_antennas(args...)			kunit_mock_slsi_mlme_set_num_antennas(args)
#define slsi_check_channelization(args...)			kunit_mock_slsi_check_channelization(args)
#define slsi_mlme_del_detect_vif(args...)			kunit_mock_slsi_mlme_del_detect_vif(args)
#define slsi_test_sap_configure_monitor_mode(args...)		kunit_mock_slsi_test_sap_configure_monitor_mode(args)
#define slsi_decode_fw_rate(args...)				kunit_mock_slsi_decode_fw_rate(args)
#define slsi_mlme_reassociate(args...)				kunit_mock_slsi_mlme_reassociate(args)
#define slsi_mlme_set_cached_channels(args...)			kunit_mock_slsi_mlme_set_cached_channels(args)
#define slsi_mlme_del_range_req(args...)			kunit_mock_slsi_mlme_del_range_req(args)
#define slsi_ap_obss_scan_done_ind(args...)			kunit_mock_slsi_ap_obss_scan_done_ind(args)
#define slsi_mlme_get_num_antennas(args...)			kunit_mock_slsi_mlme_get_num_antennas(args)
#define slsi_mlme_set_acl(args...)				kunit_mock_slsi_mlme_set_acl(args)
#define slsi_mlme_set_roaming_parameters(args...)		kunit_mock_slsi_mlme_set_roaming_parameters(args)
#define slsi_mlme_set_channel(args...)				kunit_mock_slsi_mlme_set_channel(args)
#define slsi_mlme_set_scan_mode_req(args...)			kunit_mock_slsi_mlme_set_scan_mode_req(args)
#define slsi_mlme_delba_req(args...)				kunit_mock_slsi_mlme_delba_req(args)
#define slsi_mlme_twt_teardown(args...)				kunit_mock_slsi_mlme_twt_teardown(args)
#define slsi_mlme_connected_resp(args...)			kunit_mock_slsi_mlme_connected_resp(args)
#define slsi_mlme_start(args...)				kunit_mock_slsi_mlme_start(args)
#define slsi_mlme_get_key(args...)				kunit_mock_slsi_mlme_get_key(args)
#define slsi_mlme_send_frame_mgmt(args...)			kunit_mock_slsi_mlme_send_frame_mgmt(args)
#define slsi_mlme_set_country(args...)				kunit_mock_slsi_mlme_set_country(args)
#define slsi_mlme_roaming_channel_list_req(args...)		kunit_mock_slsi_mlme_roaming_channel_list_req(args)
#define slsi_mlme_connect(args...)				kunit_mock_slsi_mlme_connect(args)
#define slsi_mlme_start_link_stats_req(args...)			kunit_mock_slsi_mlme_start_link_stats_req(args)
#define slsi_mlme_add_detect_vif(args...)			kunit_mock_slsi_mlme_add_detect_vif(args)
#define slsi_mlme_connect_scan(args...)				kunit_mock_slsi_mlme_connect_scan(args)
#define slsi_mlme_set_band_req(args...)				kunit_mock_slsi_mlme_set_band_req(args)
#define slsi_mlme_set_packet_filter(args...)			kunit_mock_slsi_mlme_set_packet_filter(args)
#define slsi_mlme_install_apf_request(args...)			kunit_mock_slsi_mlme_install_apf_request(args)
#define slsi_mlme_set_ip_address(args...)			kunit_mock_slsi_mlme_set_ip_address(args)
#define slsi_mlme_add_range_req(args...)			kunit_mock_slsi_mlme_add_range_req(args)
#define slsi_mlme_set_forward_beacon(args...)			kunit_mock_slsi_mlme_set_forward_beacon(args)
#define slsi_mlme_del_traffic_parameters(args...)		kunit_mock_slsi_mlme_del_traffic_parameters(args)
#define slsi_mlme_disconnect(args...)				kunit_mock_slsi_mlme_disconnect(args)
#define slsi_mlme_arp_detect_request(args...)			kunit_mock_slsi_mlme_arp_detect_request(args)
#define slsi_mlme_get(args...)					kunit_mock_slsi_mlme_get(args)
#define slsi_mlme_powermgt_unlocked(args...)			kunit_mock_slsi_mlme_powermgt_unlocked(args)
#define slsi_mlme_send_frame_data(args...)			kunit_mock_slsi_mlme_send_frame_data(args)
#define slsi_mlme_powermgt(args...)				kunit_mock_slsi_mlme_powermgt(args)
#define slsi_mlme_set_rssi_monitor(args...)			kunit_mock_slsi_mlme_set_rssi_monitor(args)
#define slsi_compute_chann_info(args...)			kunit_mock_slsi_compute_chann_info(args)
#define slsi_mlme_req_ind(args...)				kunit_mock_slsi_mlme_req_ind(args)
#define slsi_mlme_synchronised_response(args...)		kunit_mock_slsi_mlme_synchronised_response(args)
#define slsi_mlme_add_vif(args...)				kunit_mock_slsi_mlme_add_vif(args)
#define slsi_mlme_set(args...)					kunit_mock_slsi_mlme_set(args)
#define slsi_mlme_set_multicast_ip(args...)			kunit_mock_slsi_mlme_set_multicast_ip(args)
#define slsi_mlme_twt_status_query(args...)			kunit_mock_slsi_mlme_twt_status_query(args)
#define slsi_read_mibs(args...)					kunit_mock_slsi_read_mibs(args)
#define slsi_get_chann_info(args...)				kunit_mock_slsi_get_chann_info(args)
#define slsi_mlme_reset_dwell_time(args...)			kunit_mock_slsi_mlme_reset_dwell_time(args)
#define slsi_mlme_unset_channel_req(args...)			kunit_mock_slsi_mlme_unset_channel_req(args)
#define slsi_mlme_del_scan(args...)				kunit_mock_slsi_mlme_del_scan(args)
#define slsi_mlme_req_no_cfm(args...)				kunit_mock_slsi_mlme_req_no_cfm(args)
#define slsi_mlme_roamed_resp(args...)				kunit_mock_slsi_mlme_roamed_resp(args)
#define slsi_mlme_twt_setup(args...)				kunit_mock_slsi_mlme_twt_setup(args)
#define slsi_mlme_get_sinfo_mib(args...)			kunit_mock_slsi_mlme_get_sinfo_mib(args)
#define slsi_mlme_set_ctwindow(args...)				kunit_mock_slsi_mlme_set_ctwindow(args)
#define slsi_mlme_set_with_cfm(args...)				kunit_mock_slsi_mlme_set_with_cfm(args)
#define slsi_mlme_wifisharing_permitted_channels(args...)	kunit_mock_slsi_mlme_wifisharing_permitted_channels(args)
#define slsi_mlme_del_vif(args...)				kunit_mock_slsi_mlme_del_vif(args)
#define slsi_mlme_req(args...)					kunit_mock_slsi_mlme_req(args)
#define slsi_mlme_channel_switch(args...)			kunit_mock_slsi_mlme_channel_switch(args)
#define slsi_mlme_req_cfm(args...)				kunit_mock_slsi_mlme_req_cfm(args)
#define slsi_mlme_start_detect_request(args...)			kunit_mock_slsi_mlme_start_detect_request(args)
#define slsi_mlme_set_ipv6_address(args...)			kunit_mock_slsi_mlme_set_ipv6_address(args)
#define slsi_mlme_set_key(args...)				kunit_mock_slsi_mlme_set_key(args)
#define slsi_mlme_add_info_elements(args...)			kunit_mock_slsi_mlme_add_info_elements(args)
#define slsi_modify_ies(args...)				kunit_mock_slsi_modify_ies(args)
#define slsi_mlme_set_p2p_noa(args...)				kunit_mock_slsi_mlme_set_p2p_noa(args)
#define slsi_mlme_tdls_action(args...)				kunit_mock_slsi_mlme_tdls_action(args)
#define slsi_mlme_set_host_state(args...)			kunit_mock_slsi_mlme_set_host_state(args)
#define slsi_mlme_req_cfm_ind(args...)				kunit_mock_slsi_mlme_req_cfm_ind(args)
#define slsi_mlme_set_ext_capab(args...)			kunit_mock_slsi_mlme_set_ext_capab(args)
#define slsi_mlme_wtc_mode_req(args...)				kunit_mock_slsi_mlme_wtc_mode_req(args)
#define slsi_mlme_roam(args...)					kunit_mock_slsi_mlme_roam(args)
#define slsi_mlme_add_scan(args...)				kunit_mock_slsi_mlme_add_scan(args)
#define slsi_mlme_update_connect_params(args...)		kunit_mock_slsi_mlme_update_connect_params(args)
#define slsi_mlme_get_sta_dump(args...)				kunit_mock_slsi_mlme_get_sta_dump(args)
#define slsi_mlme_scheduled_pm_leaky_ap_detect(args...)		kunit_mock_slsi_mlme_scheduled_pm_leaky_ap_detect(args)
#define slsi_mlme_sched_pm_status(args...)			kunit_mock_slsi_mlme_sched_pm_status(args)
#define slsi_mlme_sched_pm_teardown(args...)			kunit_mock_slsi_mlme_sched_pm_teardown(args)
#define slsi_mlme_set_elnabypass_threshold(args...)		kunit_mock_slsi_mlme_set_elnabypass_threshold(args)
#define slsi_mlme_set_tdls_state(args...)			kunit_mock_slsi_mlme_set_tdls_state(args)
#define slsi_mlme_set_delayed_wakeup_type(args...)		kunit_mock_slsi_mlme_set_delayed_wakeup_type(args)
#define slsi_mlme_set_delayed_wakeup(args...)			kunit_mock_slsi_mlme_set_delayed_wakeup(args)
#define slsi_mlme_sched_pm_setup(args...)			kunit_mock_slsi_mlme_sched_pm_setup(args)
#define slsi_mlme_set_adps_state(args...)			kunit_mock_slsi_mlme_set_adps_state(args)
#define slsi_mlme_set_tx_num_ant(args...)			kunit_mock_slsi_mlme_set_tx_num_ant(args)
#define slsi_mlme_set_max_tx_power(args...)			kunit_mock_slsi_mlme_set_max_tx_power(args)

static int kunit_mock_slsi_mlme_set_traffic_parameters(struct slsi_dev *sdev, struct net_device *dev,
						       u16 user_priority, u16 medium_time,
						       u16 minimun_data_rate, u8 *mac)
{
	return 0;
}

static int kunit_mock_slsi_mlme_read_apf_request(struct slsi_dev *sdev, struct net_device *dev,
						 u8 **host_dst, int *datalen)
{
	if (sdev->device_config.fw_apf_supported)
		return 0;

	return 0;
}

static void kunit_mock_slsi_mlme_reassociate_resp(struct slsi_dev *sdev, struct net_device *dev)
{
	return;
}

static int kunit_mock_slsi_mlme_configure_monitor_mode(struct slsi_dev *sdev, struct net_device *dev,
						       struct cfg80211_chan_def *chandef)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (ndev_vif->vif_type != FAPI_VIFTYPE_MONITOR)
		return -EINVAL;

	return 0;
}

static int kunit_mock_slsi_mlme_stop_link_stats_req(struct slsi_dev *sdev, u16 stats_stop_mask)
{
	return 0;
}

static void kunit_mock_slsi_mlme_connect_resp(struct slsi_dev *sdev, struct net_device *dev)
{
	return;
}

static int kunit_mock_slsi_mlme_add_sched_scan(struct slsi_dev *sdev,
					       struct net_device *dev,
					       struct cfg80211_sched_scan_request *request,
					       const u8 *ies,
					       u16 ies_len)
{
	return -1;
}

static int kunit_mock_slsi_mlme_register_action_frame(struct slsi_dev *sdev, struct net_device *dev,
						      u32 af_bitmap_active, u32 af_bitmap_suspended)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (!ndev_vif->activated && ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED)
		return 0;

	return 0;
}

static int kunit_mock_slsi_mlme_set_num_antennas(struct net_device *dev, const u16 num_of_antennas, int frame_type)
{
	return 0;
}

static int kunit_mock_slsi_check_channelization(struct slsi_dev *sdev, struct cfg80211_chan_def *chandef,
						int wifi_sharing_channel_switched)
{
	return 0;
}

static int kunit_mock_slsi_mlme_del_detect_vif(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static int kunit_mock_slsi_test_sap_configure_monitor_mode(struct slsi_dev *sdev, struct net_device *dev,
							   struct cfg80211_chan_def *chandef)
{
	return 0;
}

static void kunit_mock_slsi_decode_fw_rate(u32 fw_rate, struct rate_info *rate, unsigned long *data_rate_mbps)
{
	return;
}

static int kunit_mock_slsi_mlme_reassociate(struct slsi_dev *sdev, struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (ndev_vif->activated)
		return FAPI_RESULTCODE_SUCCESS;
	else
		return -EINVAL;
}

static int kunit_mock_slsi_mlme_set_cached_channels(struct slsi_dev *sdev, struct net_device *dev,
						    u32 channels_count, u16 *channels)
{
	return 0;
}

static int kunit_mock_slsi_mlme_del_range_req(struct slsi_dev *sdev, struct net_device *dev,
					      u16 count, u8 *addr, u16 rtt_id)

{
	return 0;
}

static void kunit_mock_slsi_ap_obss_scan_done_ind(struct net_device *dev, struct netdev_vif *ndev_vif)
{
	return;
}

static int kunit_mock_slsi_mlme_get_num_antennas(struct slsi_dev *sdev, struct net_device *dev, int *num_antennas)
{
	return 0;
}

static int kunit_mock_slsi_mlme_set_acl(struct slsi_dev *sdev, struct net_device *dev, u16 ifnum,
					enum nl80211_acl_policy acl_policy, int max_acl_entries,
					struct mac_address mac_addrs[])
{
	if (ifnum == SLSI_NET_INDEX_P2PX_SWLAN) {
		if (acl_policy == NL80211_ACL_POLICY_DENY_UNLESS_LISTED && max_acl_entries == 54)
			return 0;
		else if (acl_policy == NL80211_ACL_POLICY_DENY_UNLESS_LISTED && max_acl_entries == -54)
			return -EINVAL;
	}

	return 0;
}

static int kunit_mock_slsi_mlme_set_roaming_parameters(struct slsi_dev *sdev, struct net_device *dev,
						       u16 psid, int mib_value, int mib_length)
{
	return 0;
}

static int kunit_mock_slsi_mlme_set_channel(struct slsi_dev *sdev, struct net_device *dev,
					    struct ieee80211_channel *chan, u16 duration,
					    u16 interval, u16 count)
{
	if (sdev->nan_enabled) return 0;

	if (chan->hw_value == 196)
		return -EINVAL;

	if (sdev->p2p_state == P2P_IDLE_NO_VIF && SLSI_P2P_IS_SOCAIL_CHAN(chan->hw_value))
		return 0;

	return 0;
}

static int kunit_mock_slsi_mlme_set_scan_mode_req(struct slsi_dev *sdev, struct net_device *dev,
						  u16 scan_mode, u16 max_channel_time,
						  u16 home_away_time, u16 home_time,
						  u16 max_channel_passive_time)
{
	return 0;
}

static int kunit_mock_slsi_mlme_delba_req(struct slsi_dev *sdev, struct net_device *dev,
					  u8 *peer_qsta_address, u16 priority,
					  u16 direction, u16 sequence_number,
					  u16 reason_code)
{
	return 0;
}

static int kunit_mock_slsi_mlme_twt_teardown(struct slsi_dev *sdev, struct net_device *dev,
					     u16 setup_id, u16 negotiation_type)
{
	return 0;
}

static void kunit_mock_slsi_mlme_connected_resp(struct slsi_dev *sdev, struct net_device *dev, u16 peer_index)
{
	return;
}

static int kunit_mock_slsi_mlme_start(struct slsi_dev *sdev, struct net_device *dev,
				      u8 *bssid, struct cfg80211_ap_settings *settings,
				      const u8 *wpa_ie_pos, const u8 *wmm_ie_pos,
				      bool append_vht_ies)
{
	return 0;
}

static int kunit_mock_slsi_mlme_get_key(struct slsi_dev *sdev, struct net_device *dev,
					u16 key_id, u16 key_type, u8 *seq, int *seq_len)
{
	if (key_id == 7)
		return -EINVAL;
	else
		return FAPI_RESULTCODE_SUCCESS;
}

static int kunit_mock_slsi_mlme_send_frame_mgmt(struct slsi_dev *sdev, struct net_device *dev,
						const u8 *frame, int frame_len,
						u16 data_desc, u16 msg_type, u16 host_tag,
						u16 freq, u32 dwell_time, u32 period)
{
	if (msg_type == FAPI_DATAUNITDESCRIPTOR_IEEE802_3_FRAME && sdev->device_config.qos_info == 267)
		return 0;

	if (sdev->device_config.qos_info == 267)
		return 0;
	else if (sdev->device_config.qos_info == -267)
		return -EINVAL;

	return 0;
}

static int kunit_mock_slsi_mlme_set_country(struct slsi_dev *sdev, char *alpha2)
{
	return 0;
}

static struct sk_buff *kunit_mock_slsi_mlme_roaming_channel_list_req(struct slsi_dev *sdev, struct net_device *dev)
{
	return NULL;
}

static int kunit_mock_slsi_mlme_connect(struct slsi_dev *sdev, struct net_device *dev,
					struct cfg80211_connect_params *sme,
					struct ieee80211_channel *channel,
					const u8 *bssid)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED)
		return 0;
	return 0;
}

static int kunit_mock_slsi_mlme_start_link_stats_req(struct slsi_dev *sdev, u16 mpdu_size_threshold,
						     bool aggressive_stats_enabled)
{
	return 0;
}

static int kunit_mock_slsi_mlme_add_detect_vif(struct slsi_dev *sdev, struct net_device *dev,
					       u8 *interface_address, u8 *device_address)
{
	return 0;
}

static int kunit_mock_slsi_mlme_connect_scan(struct slsi_dev *sdev, struct net_device *dev,
					     u32 n_ssids, struct cfg80211_ssid *ssids,
					     struct ieee80211_channel *channel)

{
	if (sdev->tspec_error_code == 15)
		return 0;
	return 0;
}

static int kunit_mock_slsi_mlme_set_band_req(struct slsi_dev *sdev, struct net_device *dev,
					     uint band, u16 avoid_disconnection)
{
	if (sdev->nan_enabled)
		return 1;
	return -EIO;
}

static int kunit_mock_slsi_mlme_set_packet_filter(struct slsi_dev *sdev, struct net_device *dev,
						  int pkt_filter_len,
						  u8 num_filters,
						  struct slsi_mlme_pkt_filter_elem *pkt_filter_elems)
{
	return 0;
}

static int kunit_mock_slsi_mlme_install_apf_request(struct slsi_dev *sdev, struct net_device *dev,
						    u8 *program, u32 program_len)
{
	return 0;
}

static int kunit_mock_slsi_mlme_set_ip_address(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static int kunit_mock_slsi_mlme_add_range_req(struct slsi_dev *sdev, struct net_device *dev, u8 count,
					      struct slsi_rtt_config *nl_rtt_params, u16 rtt_id, u8 *source_addr)
{
	return 0;
}

static int kunit_mock_slsi_mlme_set_forward_beacon(struct slsi_dev *sdev, struct net_device *dev, int action)
{
	return 0;
}

static int kunit_mock_slsi_mlme_del_traffic_parameters(struct slsi_dev *sdev, struct net_device *dev, u16 user_priority)
{
	if (!sdev || !sdev->sig_wait.cfm)
		return -EINVAL;
	return 0;
}

static int kunit_mock_slsi_mlme_disconnect(struct slsi_dev *sdev, struct net_device *dev,
					   u8 *mac, u16 reason_code, bool wait_ind)
{
	if (sdev) {
		if (sdev->device_state == SLSI_DEVICE_STATE_STOPPED)
			return 1;

		if (sdev->device_config.user_suspend_mode == EINVAL)
			return -EINVAL;
		else if (sdev->device_config.user_suspend_mode == ERANGE)
			return -ERANGE;
	}

	return 0;
}

static int kunit_mock_slsi_mlme_arp_detect_request(struct slsi_dev *sdev, struct net_device *dev,
						   u16 action, u8 *target_ipaddr)
{
	return 0;
}

static int kunit_mock_slsi_mlme_get(struct slsi_dev *sdev, struct net_device *dev,
				    u8 *mib, int mib_len, u8 *resp,
				    int resp_buf_len, int *resp_len)
{
	if (resp_buf_len == *resp_len)
		(*resp_len) += resp_buf_len;

	return 0;
}

static int kunit_mock_slsi_mlme_powermgt_unlocked(struct slsi_dev *sdev, struct net_device *dev, u16 power_mode)
{
	return 0;
}

static int kunit_mock_slsi_mlme_send_frame_data(struct slsi_dev *sdev, struct net_device *dev,
						struct sk_buff *skb, u16 msg_type,
						u16 host_tag, u32 dwell_time, u32 period)
{
	if (msg_type == FAPI_MESSAGETYPE_ANY_OTHER && sdev->device_config.qos_info == 267)
		return 1;

	else if (msg_type == FAPI_MESSAGETYPE_ANY_OTHER && sdev->device_config.qos_info == -267) {
		if (skb)
			kfree_skb(skb);

		return 0;
	}

	return 0;
}

static int kunit_mock_slsi_mlme_powermgt(struct slsi_dev *sdev, struct net_device *dev, u16 power_mode)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (ndev_vif->activated && power_mode)
		return 0;

	return 0;
}

static int kunit_mock_slsi_mlme_set_rssi_monitor(struct slsi_dev *sdev, struct net_device *dev,
						 u8 enable, s8 low_rssi_threshold, s8 high_rssi_threshold)
{
	return 0;
}

static u16 kunit_mock_slsi_compute_chann_info(struct slsi_dev *sdev, u16 width, u16 center_freq0, u16 channel_freq)
{
	return 0;
}

static struct sk_buff *kunit_mock_slsi_mlme_req_ind(struct slsi_dev *sdev, struct net_device *dev,
						    struct sk_buff *skb, u16 ind_id)
{
	return skb;
}

static int kunit_mock_slsi_mlme_synchronised_response(struct slsi_dev *sdev, struct net_device *dev,
						      struct cfg80211_external_auth_params *params)
{
	struct netdev_vif	*ndev_vif = netdev_priv(dev);

	if (ndev_vif->sta.wpa3_sae_reconnection)
		return 1;
	return 0;
}

static int kunit_mock_slsi_mlme_add_vif(struct slsi_dev *sdev, struct net_device *dev,
					u8 *interface_address, u8 *device_address)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (!ndev_vif->activated && ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTED && interface_address)
		return 0;

	if (!ndev_vif->activated && ndev_vif->sta.vif_status == SLSI_VIF_STATUS_DISCONNECTING)
		return 1;

	if (ndev_vif->ap.add_info_ies_len == 548 && !ndev_vif->activated && sdev->device_config.supported_roam_band == 522)
		return 0;

	if (ndev_vif->iftype == NL80211_IFTYPE_P2P_GO && sdev->device_config.supported_roam_band == 522)
		return -EINVAL;

	return 0;
}

static int kunit_mock_slsi_mlme_set(struct slsi_dev *sdev, struct net_device *dev, u8 *mib, int mib_len)
{
	return 0;
}

static int kunit_mock_slsi_mlme_set_multicast_ip(struct slsi_dev *sdev, struct net_device *dev,
						 __be32 multicast_ip_list[], int count)
{
	return 0;
}

static int kunit_mock_slsi_mlme_twt_status_query(struct slsi_dev *sdev, struct net_device *dev,
						 char *command, int command_len, int *command_pos, u16 setup_id)

{
	return 0;
}

static struct slsi_mib_value *kunit_mock_slsi_read_mibs(struct slsi_dev *sdev, struct net_device *dev,
							struct slsi_mib_get_entry *mib_entries,
							int mib_count, struct slsi_mib_data *mibrsp)
{
	struct slsi_mib_value *values = kmalloc_array(mib_count, sizeof(struct slsi_mib_value), GFP_KERNEL);
	int i;

	if (!sdev) return NULL;
	for (i = 0; i < mib_count; i++) {
		if (sdev->recovery_timeout == 1) {
			values[i].type = SLSI_MIB_TYPE_OCTET;
			values[i].u.octetValue.data = mibrsp->data;
			values[i].u.octetValue.dataLength = mibrsp->dataLength;
			if (i == 4) {
				values[i].u.octetValue.data[0] = 1;
				values[i].u.octetValue.data[1] = 16;
			}

		} else if (sdev->recovery_timeout == 2) {
			values[i].type = SLSI_MIB_TYPE_UINT;
			values[i].u.uintValue = i;

		} else if (sdev->recovery_timeout == 3) {
			values[0].type = SLSI_MIB_TYPE_OCTET;
			values[0].u.octetValue.data[i * 2] = 1;
			values[0].u.octetValue.data[i * 2 + 1] = 184 + i;
			values[0].u.octetValue.dataLength = mibrsp->dataLength;

		} else if (sdev->recovery_timeout == 4) {
			values[i].type = SLSI_MIB_TYPE_OCTET;
			values[i].u.octetValue.data = mibrsp->data;
			if (i < 8)
				values[i].u.octetValue.dataLength = 2;
			else
				values[i].u.octetValue.dataLength = 28;

		} else {
			if (i == 0 && mib_entries[0].index[0] == 0 && mib_entries[0].index[1] == 8) {
				values[i].type = SLSI_MIB_TYPE_OCTET;
				values[i].u.octetValue.data = mibrsp->data;
				values[i].u.octetValue.dataLength = mibrsp->dataLength;
			} else {
				values[i].type = SLSI_MIB_TYPE_INT;
				values[i].u.intValue = i;
			}
		}
	}

	return values;
}

static u16 kunit_mock_slsi_get_chann_info(struct slsi_dev *sdev, struct cfg80211_chan_def *chandef)
{
	u16 chann_info = 0;

	switch (chandef->width) {
	case NL80211_CHAN_WIDTH_20:
	case NL80211_CHAN_WIDTH_20_NOHT:
		chann_info = 20;
		break;
	case NL80211_CHAN_WIDTH_40:
		chann_info = 40;
		break;
	case NL80211_CHAN_WIDTH_80:
		chann_info = 80;
		break;
	case NL80211_CHAN_WIDTH_160:
		chann_info = 160;
		break;
	default:
		SLSI_WARN(sdev, "Invalid chandef.width(0x%x)\n", chandef->width);
		chann_info = 0;
		break;
	}
	return chann_info;
}

static int kunit_mock_slsi_mlme_reset_dwell_time(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static int kunit_mock_slsi_mlme_unset_channel_req(struct slsi_dev *sdev, struct net_device *dev)
{
	return 1;
}

static int kunit_mock_slsi_mlme_del_scan(struct slsi_dev *sdev, struct net_device *dev,
					 u16 scan_id, bool scan_timed_out)
{
	if (scan_id == 257)
		return 1;
	else
		return 0;
}

static struct sk_buff *kunit_mock_slsi_mlme_req_no_cfm(struct slsi_dev *sdev, struct net_device *dev,
						       struct sk_buff *skb)
{
	return NULL;
}

static void kunit_mock_slsi_mlme_roamed_resp(struct slsi_dev *sdev, struct net_device *dev)
{
	return;
}

static int kunit_mock_slsi_mlme_twt_setup(struct slsi_dev *sdev, struct net_device *dev, struct twt_setup *tsetup)
{
	return 0;
}

static int kunit_mock_slsi_mlme_get_sinfo_mib(struct slsi_dev *sdev, struct net_device *dev,
			    struct slsi_peer *peer)
{
	if (!sdev->device_config.user_suspend_mode)
		return 0;

	return 0;
}

static int kunit_mock_slsi_mlme_set_ctwindow(struct slsi_dev *sdev, struct net_device *dev, unsigned int ct_param)
{
	return 0;
}

static struct sk_buff *kunit_mock_slsi_mlme_set_with_cfm(struct slsi_dev *sdev, struct net_device *dev,
							 u8 *mib, int mib_len)
{
	if (!sdev)
		return NULL;
	else
		return sdev->sig_wait.cfm;
}

static int kunit_mock_slsi_mlme_wifisharing_permitted_channels(struct slsi_dev *sdev, struct net_device *dev,
							       u8 *permitted_channels)
{
	return 0;
}

static int kunit_mock_slsi_mlme_del_vif(struct slsi_dev *sdev, struct net_device *dev)
{
	if (sdev->device_config.user_suspend_mode == EINVAL)
		return -EINVAL;
	else if (sdev->device_config.user_suspend_mode == ERANGE)
		return -ERANGE;

	return 0;
}

static int kunit_mock_slsi_mlme_req(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (skb)
		kfree_skb(skb);

	return 0;
}

static int kunit_mock_slsi_mlme_channel_switch(struct slsi_dev *sdev, struct net_device *dev,
					       u16 center_freq, u16 chan_info)
{
	return 0;
}

static struct sk_buff *kunit_mock_slsi_mlme_req_cfm(struct slsi_dev *sdev, struct net_device *dev,
						    struct sk_buff *skb, u16 cfm_id)
{
	return skb;
}

static int kunit_mock_slsi_mlme_start_detect_request(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static int kunit_mock_slsi_mlme_set_ipv6_address(struct slsi_dev *sdev, struct net_device *dev)
{
	struct netdev_vif	*ndev_vif = netdev_priv(dev);

	if (ndev_vif->sta.nd_offload_enabled)
		return 0;

	return 0;
}

static int kunit_mock_slsi_mlme_set_key(struct slsi_dev *sdev, struct net_device *dev,
					u16 key_id, u16 key_type, const u8 *address,
					struct key_params *key)
{
	if (key_id == 7)
		return -EINVAL;
	else
		return FAPI_RESULTCODE_SUCCESS;
}

static int kunit_mock_slsi_mlme_add_info_elements(struct slsi_dev *sdev, struct net_device *dev,
						  u16 purpose, const u8 *ies, const u16 ies_len)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (ndev_vif->sta.rsn_ie && ndev_vif->sta.use_set_pmksa)
		return 0;

	if (purpose == FAPI_PURPOSE_PROBE_RESPONSE && ies)
		return 0;
	else if (purpose == FAPI_PURPOSE_PROBE_RESPONSE && ies_len == 548)
		return -EINVAL;
	else if (purpose == FAPI_PURPOSE_LOCAL)
		return -EINVAL;
	else if (purpose == FAPI_PURPOSE_ASSOCIATION_REQUEST && ies)
		return 0;

	return 0;
}

static int kunit_mock_slsi_modify_ies(struct net_device *dev, u8 eid, u8 *ies, int ies_len,
				      u8 ie_index, u8 ie_value)
{
	return 0;
}

static int kunit_mock_slsi_mlme_set_p2p_noa(struct slsi_dev *sdev, struct net_device *dev,
					    unsigned int noa_count, unsigned int interval,
					    unsigned int duration)

{
	return 0;
}

static int kunit_mock_slsi_mlme_tdls_action(struct slsi_dev *sdev, struct net_device *dev,
					    const u8 *peer, int action)
{
	if (peer) {
		if (is_broadcast_ether_addr(peer))
			return -EOPNOTSUPP;
		if (is_zero_ether_addr(peer))
			return -EINVAL;
	}

	return 0;
}

static int kunit_mock_slsi_mlme_set_host_state(struct slsi_dev *sdev, struct net_device *dev, u16 host_state)
{
	return 0;
}

static struct sk_buff *kunit_mock_slsi_mlme_req_cfm_ind(struct slsi_dev *sdev, struct net_device *dev,
							struct sk_buff *skb, u16 cfm_id, u16 ind_id,
							bool (*validate_cfm_wait_ind)(struct slsi_dev *sdev,
										      struct net_device *dev,
										      struct sk_buff *cfm))
{
	return skb;
}

static int kunit_mock_slsi_mlme_set_ext_capab(struct slsi_dev *sdev, struct net_device *dev,
					      u8 *data, int datalength)
{
	return 0;
}

static int kunit_mock_slsi_mlme_wtc_mode_req(struct slsi_dev *sdev, struct net_device *dev,
					     int wtc_mode, int scan_mode,
					     int rssi, int rssi_th_2g,
					     int rssi_th_5g, int rssi_th_6g)
{
	return 0;
}

static int kunit_mock_slsi_mlme_roam(struct slsi_dev *sdev, struct net_device *dev, const u8 *bssid, u16 freq)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (!ndev_vif->activated) return -EINVAL;

	if (ndev_vif->is_fw_test) return 0;

	if (ndev_vif->mgmt_tx_cookie == 0x1234) return -EINVAL;

	if (ndev_vif->chan) {
		if (ndev_vif->chan->center_freq == freq)
			return -EINVAL;
		else
			return 0;
	}

	return 0;
}

static int kunit_mock_slsi_mlme_add_scan(struct slsi_dev *sdev,
					 struct net_device *dev,
					 u16 scan_type,
					 u16 report_mode,
					 u32 n_ssids,
					 struct cfg80211_ssid *ssids,
					 u32 n_channels,
					 struct ieee80211_channel *channels[],
					 void *gscan,
					 const u8 *ies,
					 u16 ies_len,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
					 u32 n_6ghz_params,
					 struct cfg80211_scan_6ghz_params *scan_6ghz_params,
					 u32 scan_flags,
#endif
					 bool wait_for_ind)
{
	int i = 0;
	struct netdev_vif *ndev_vif;

	if (!wait_for_ind) {
		ndev_vif = netdev_priv(dev);
		if (ndev_vif->scan[SLSI_SCAN_HW_ID].acs_request)
			kfree(ndev_vif->scan[SLSI_SCAN_HW_ID].acs_request);
	}

	if (n_channels > 1) {
		for (i = 0; i < n_channels; i++)
			channels[i] = NULL;
		return 1;
	}
	else {
		return 0;
	}
}

static int kunit_mock_slsi_mlme_update_connect_params(struct slsi_dev *sdev, struct net_device *dev,
						      struct cfg80211_connect_params *sme, u32 changed)
{
	return 0;
}

static void kunit_mock_slsi_mlme_get_sta_dump(struct netdev_vif *ndev_vif, struct net_device *dev, int *dump_params)
{
}

static int kunit_mock_slsi_mlme_scheduled_pm_leaky_ap_detect(struct slsi_dev *sdev, struct net_device *dev,
							     struct slsi_sched_pm_leaky_ap *leaky_ap)
{
	return 0;
}

static int kunit_mock_slsi_mlme_sched_pm_status(struct slsi_dev *sdev, struct net_device *dev,
						struct slsi_scheduled_pm_status *status)
{
	return 0;
}

static int kunit_mock_slsi_mlme_sched_pm_teardown(struct slsi_dev *sdev, struct net_device *dev)
{
	return FAPI_RESULTCODE_SUCCESS;
}

static int kunit_mock_slsi_mlme_set_elnabypass_threshold(struct slsi_dev *sdev, struct net_device *elna_net_device,
							 int elna_value)
{
	return 0;
}

static int kunit_mock_slsi_mlme_set_tdls_state(struct slsi_dev *sdev, struct net_device *dev, u16 enabled_flag)
{
	return 0;
}

static int kunit_mock_slsi_mlme_set_delayed_wakeup_type(struct slsi_dev  *sdev, struct net_device *dev, int type,
							u8 macaddrlist[][ETH_ALEN], int  mac_count,
							u8 ipv4addrlist[][4], int ipv4_count,
							u8 ipv6addrlist[][16], int ipv6_count)
{
	return 0;
}

static int kunit_mock_slsi_mlme_set_delayed_wakeup(struct slsi_dev *sdev, struct net_device *dev,
						   int delayed_wakeup_on, int timeout)
{
	return 0;
}

static int kunit_mock_slsi_mlme_sched_pm_setup(struct slsi_dev *sdev, struct net_device *dev,
					       struct slsi_scheduled_pm_setup *sched_pm_setup)
{
	int result_code = 0;

	switch (sched_pm_setup->maximum_duration) {
	case 2:
		result_code =  FAPI_RESULTCODE_INVALID_PARAMETERS;
		break;
	case 3:
		result_code =  FAPI_RESULTCODE_INVALID_VIRTUAL_INTERFACE_INDEX;
		break;
	case 4:
		result_code =  FAPI_RESULTCODE_NOT_SUPPORTED;
		break;
	case 5:
		result_code =  FAPI_RESULTCODE_INVALID_STATE;
		break;
	default:
		break;
	}
	return result_code;
}

static int kunit_mock_slsi_mlme_set_adps_state(struct slsi_dev *sdev, struct net_device *dev, int adps_state)
{
	return 0;
}

static int kunit_mock_slsi_mlme_set_tx_num_ant(struct slsi_dev *sdev, struct net_device *dev, int cfg)
{
	return 0;
}

static int kunit_mock_slsi_mlme_set_max_tx_power(struct slsi_dev *sdev, struct net_device *dev, int *pwr_limit)
{
	return 0;
}
#endif
