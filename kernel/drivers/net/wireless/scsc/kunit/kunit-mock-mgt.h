/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_MGT_H__
#define __KUNIT_MOCK_MGT_H__

#include "../mib.h"
#include "../mgt.h"
#include "../cfg80211_ops.h"

#define slsi_set_packet_filters(args...)				kunit_mock_slsi_set_packet_filters(args)
#define slsi_wakeup_time_work(args...)					kunit_mock_slsi_wakeup_time_work(args)
#define slsi_get_hw_mac_address(args...)				kunit_mock_slsi_get_hw_mac_address(args)
#define slsi_stop_chip(args...)						kunit_mock_slsi_stop_chip(args)
#define slsi_ps_port_control(args...)					kunit_mock_slsi_ps_port_control(args)
#define slsi_purge_scan_results_locked(args...)				kunit_mock_slsi_purge_scan_results_locked(args)
#define slsi_failure_reset(args...)					kunit_mock_slsi_failure_reset(args)
#define slsi_fill_enhanced_arp_out_of_order_drop_counter(args...)	kunit_mock_slsi_fill_enhanced_arp_out_of_order_drop_counter(args)
#define slsi_subsystem_reset(args...)					kunit_mock_slsi_subsystem_reset(args)
#define slsi_system_error_recovery(args...)				kunit_mock_slsi_system_error_recovery(args)
#define slsi_if_valid_wifi_sharing_channel(args...)			kunit_mock_slsi_if_valid_wifi_sharing_channel(args)
#define slsi_rx_update_wake_stats(args...)				kunit_mock_slsi_rx_update_wake_stats(args)
#define slsi_band_cfg_update(args...)					kunit_mock_slsi_band_cfg_update(args)
#define slsi_is_dns_packet(args...)					kunit_mock_slsi_is_dns_packet(args)
#define slsi_mib_get_apf_cap(args...)					kunit_mock_slsi_mib_get_apf_cap(args)
#define slsi_vif_cleanup(args...)					kunit_mock_slsi_vif_cleanup(args)
#define slsi_read_default_country(args...)				kunit_mock_slsi_read_default_country(args)
#define slsi_destroy_sysfs_macaddr(args...)				kunit_mock_slsi_destroy_sysfs_macaddr(args)
#define slsi_roam_channel_cache_get_channels_int(args...)		kunit_mock_slsi_roam_channel_cache_get_channels_int(args)
#define slsi_is_wes_action_frame(args...)				kunit_mock_slsi_is_wes_action_frame(args)
#define slsi_remove_duplicates(args...)					kunit_mock_slsi_remove_duplicates(args)
#define slsi_p2p_vif_activate(args...)					kunit_mock_slsi_p2p_vif_activate(args)
#define slsi_test_send_hanged_vendor_event(args...)			kunit_mock_slsi_test_send_hanged_vendor_event(args)
#define slsi_get_ps_entry_counter(args...)				kunit_mock_slsi_get_ps_entry_counter(args)
#define slsi_configure_tx_power_sar_scenario(args...)			kunit_mock_slsi_configure_tx_power_sar_scenario(args)
#if !(defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION < 11) || defined(CONFIG_SCSC_WLAN_SUPPORT_6G)
#define slsi_bss_connect_type_get(args...)				kunit_mock_slsi_bss_connect_type_get(args)
#endif
#define slsi_set_mac_randomisation_mask(args...)			kunit_mock_slsi_set_mac_randomisation_mask(args)
#define slsi_get_mhs_ws_chan_vsdb(args...)				kunit_mock_slsi_get_mhs_ws_chan_vsdb(args)
#define slsi_get_beacon_cu(args...)					kunit_mock_slsi_get_beacon_cu(args)
#define slsi_peer_remove(args...)					kunit_mock_slsi_peer_remove(args)
#define slsi_peer_reset_stats(args...)					kunit_mock_slsi_peer_reset_stats(args)
#define slsi_chip_recovery(args...)					kunit_mock_slsi_chip_recovery(args)
#define slsi_send_hanged_vendor_event(args...)				kunit_mock_slsi_send_hanged_vendor_event(args)
#define slsi_p2p_get_go_neg_rsp_status(args...)				kunit_mock_slsi_p2p_get_go_neg_rsp_status(args)
#define slsi_sort_array(args...)					kunit_mock_slsi_sort_array(args)
#define slsi_peer_update_assoc_rsp(args...)				kunit_mock_slsi_peer_update_assoc_rsp(args)
#define slsi_p2p_deinit(args...)					kunit_mock_slsi_p2p_deinit(args)
#define slsi_set_mib_rssi_boost(args...)				kunit_mock_slsi_set_mib_rssi_boost(args)
#define slsi_create_sysfs_debug_dump(args...)				kunit_mock_slsi_create_sysfs_debug_dump(args)
#define slsi_read_enhanced_arp_rx_count_by_lower_mac(args...)		kunit_mock_slsi_read_enhanced_arp_rx_count_by_lower_mac(args)
#define slsi_wlan_unsync_vif_deactivate(args...)			kunit_mock_slsi_wlan_unsync_vif_deactivate(args)
#define slsi_is_bssid_in_ioctl_blacklist(args...)			kunit_mock_slsi_is_bssid_in_ioctl_blacklist(args)
#define slsi_send_forward_beacon_abort_vendor_event(args...)		kunit_mock_slsi_send_forward_beacon_abort_vendor_event(args)
#define slsi_merge_lists(args...)					kunit_mock_slsi_merge_lists(args)
#define slsi_start(args...)						kunit_mock_slsi_start(args)
#define slsi_extract_valid_wifi_sharing_channels(args...)		kunit_mock_slsi_extract_valid_wifi_sharing_channels(args)
#define slsi_stop(args...)						kunit_mock_slsi_stop(args)
#define slsi_p2p_dev_null_ies(args...)					kunit_mock_slsi_p2p_dev_null_ies(args)
#define slsi_roam_channel_cache_add(args...)				kunit_mock_slsi_roam_channel_cache_add(args)
#define slsi_vif_deactivated(args...)					kunit_mock_slsi_vif_deactivated(args)
#define slsi_set_ext_cap(args...)					kunit_mock_slsi_set_ext_cap(args)
#define slsi_scan_cleanup(args...)					kunit_mock_slsi_scan_cleanup(args)
#define slsi_retry_connection(args...)					kunit_mock_slsi_retry_connection(args)
#define slsi_destroy_sysfs_debug_dump(args...)				kunit_mock_slsi_destroy_sysfs_debug_dump(args)
#define slsi_set_wifisharing_permitted_channels(args...)		kunit_mock_slsi_set_wifisharing_permitted_channels(args)
#define slsi_set_mib_roam(args...)					kunit_mock_slsi_set_mib_roam(args)
#define slsi_dynamic_interface_create(args...)				kunit_mock_slsi_dynamic_interface_create(args)
#define slsi_set_enhanced_pkt_filter(args...)				kunit_mock_slsi_set_enhanced_pkt_filter(args)
#define slsi_modify_ies_on_channel_switch(args...)			kunit_mock_slsi_modify_ies_on_channel_switch(args)
#define slsi_set_mib_soft_roaming_enabled(args...)			kunit_mock_slsi_set_mib_soft_roaming_enabled(args)
#define slsi_is_bssid_in_hal_blacklist(args...)				kunit_mock_slsi_is_bssid_in_hal_blacklist(args)
#define slsi_roaming_scan_configure_channels(args...)			kunit_mock_slsi_roaming_scan_configure_channels(args)
#define slsi_find_chan_idx(args...)					kunit_mock_slsi_find_chan_idx(args)
#define slsi_check_if_channel_restricted_already(args...)		kunit_mock_slsi_check_if_channel_restricted_already(args)
#define slsi_ap_prepare_add_info_ies(args...)				kunit_mock_slsi_ap_prepare_add_info_ies(args)
#define slsi_set_latency_crt_data(args...)				kunit_mock_slsi_set_latency_crt_data(args)
#define slsi_get_mib_roam(args...)					kunit_mock_slsi_get_mib_roam(args)
#define slsi_add_probe_ies_request(args...)				kunit_mock_slsi_add_probe_ies_request(args)
#define slsi_set_uint_mib(args...)					kunit_mock_slsi_set_uint_mib(args)
#define slsi_set_boost(args...)						kunit_mock_slsi_set_boost(args)
#define slsi_check_if_non_indoor_non_dfs_channel(args...)		kunit_mock_slsi_check_if_non_indoor_non_dfs_channel(args)
#define slsi_clear_packet_filters(args...)				kunit_mock_slsi_clear_packet_filters(args)
#define slsi_dump_eth_packet(args...)					kunit_mock_slsi_dump_eth_packet(args)
#define slsi_band_update(args...)					kunit_mock_slsi_band_update(args)
#define slsi_free_connection_params(args...)				kunit_mock_slsi_free_connection_params(args)
#define slsi_send_rcl_event(args...)					kunit_mock_slsi_send_rcl_event(args)
#define slsi_scan_ind_timeout_handle(args...)				kunit_mock_slsi_scan_ind_timeout_handle(args)
#define slsi_update_supported_channels_regd_flags(args...)		kunit_mock_slsi_update_supported_channels_regd_flags(args)
#define slsi_dequeue_cached_scan_result(args...)			kunit_mock_slsi_dequeue_cached_scan_result(args)
#define slsi_sta_ieee80211_mode(args...)				kunit_mock_slsi_sta_ieee80211_mode(args)
#define slsi_ip_address_changed(args...)				kunit_mock_slsi_ip_address_changed(args)
#define slsi_stop_net_dev(args...)					kunit_mock_slsi_stop_net_dev(args)
#define slsi_start_monitor_mode(args...)				kunit_mock_slsi_start_monitor_mode(args)
#define slsi_roam_channel_cache_get(args...)				kunit_mock_slsi_roam_channel_cache_get(args)
#define slsi_arp_q_stuck_work_handle(args...)				kunit_mock_slsi_arp_q_stuck_work_handle(args)
#define slsi_dump_stats(args...)					kunit_mock_slsi_dump_stats(args)
#define slsi_read_preferred_antenna_from_file(args...)			kunit_mock_slsi_read_preferred_antenna_from_file(args)
#define slsi_get_mhs_ws_chan_rsdb(args...)				kunit_mock_slsi_get_mhs_ws_chan_rsdb(args)
#define slsi_mib_get_rtt_cap(args...)					kunit_mock_slsi_mib_get_rtt_cap(args)
#define slsi_set_mib_preferred_antenna(args...)				kunit_mock_slsi_set_mib_preferred_antenna(args)
#define slsi_set_country_update_regd(args...)				kunit_mock_slsi_set_country_update_regd(args)
#define slsi_rx_update_mlme_stats(args...)				kunit_mock_slsi_rx_update_mlme_stats(args)
#define slsi_blacklist_del_work_handle(args...)				kunit_mock_slsi_blacklist_del_work_handle(args)
#define slsi_create_sysfs_macaddr(args...)				kunit_mock_slsi_create_sysfs_macaddr(args)
#define slsi_peer_update_assoc_req(args...)				kunit_mock_slsi_peer_update_assoc_req(args)
#define slsi_create_sysfs_version_info(args...)				kunit_mock_slsi_create_sysfs_version_info(args)
#define slsi_send_acs_event(args...)					kunit_mock_slsi_send_acs_event(args)
#define slsi_destroy_sysfs_version_info(args...)			kunit_mock_slsi_destroy_sysfs_version_info(args)
#define slsi_handle_disconnect(args...)					kunit_mock_slsi_handle_disconnect(args)
#define slsi_collect_chipset_logs(args...)				kunit_mock_slsi_collect_chipset_logs(args)
#define slsi_roam_channel_cache_prune(args...)				kunit_mock_slsi_roam_channel_cache_prune(args)
#define slsi_send_twt_notification(args...)				kunit_mock_slsi_send_twt_notification(args)
#define slsi_trigger_service_failure(args...)				kunit_mock_slsi_trigger_service_failure(args)
#define slsi_send_max_transmit_msdu_lifetime(args...)			kunit_mock_slsi_send_max_transmit_msdu_lifetime(args)
#define slsi_set_latency_mode(args...)					kunit_mock_slsi_set_latency_mode(args)
#define slsi_send_forward_beacon_vendor_event(args...)			kunit_mock_slsi_send_forward_beacon_vendor_event(args)
#define slsi_abort_sta_scan(args...)					kunit_mock_slsi_abort_sta_scan(args)
#define slsi_clear_offchannel_data(args...)				kunit_mock_slsi_clear_offchannel_data(args)
#define slsi_send_twt_setup_event(args...)				kunit_mock_slsi_send_twt_setup_event(args)
#define slsi_get_public_action_subtype(args...)				kunit_mock_slsi_get_public_action_subtype(args)
#define slsi_read_regulatory_rules_fw(args...)				kunit_mock_slsi_read_regulatory_rules_fw(args)
#define slsi_send_twt_teardown(args...)					kunit_mock_slsi_send_twt_teardown(args)
#define slsi_is_bssid_in_blacklist(args...)				kunit_mock_slsi_is_bssid_in_blacklist(args)
#define slsi_p2p_dev_probe_rsp_ie(args...)				kunit_mock_slsi_p2p_dev_probe_rsp_ie(args)
#define slsi_wlan_unsync_vif_activate(args...)				kunit_mock_slsi_wlan_unsync_vif_activate(args)
#define slsi_peer_add(args...)						kunit_mock_slsi_peer_add(args)
#define slsi_is_tcp_sync_packet(args...)				kunit_mock_slsi_is_tcp_sync_packet(args)
#define slsi_set_reset_connect_attempted_flag(args...)			kunit_mock_slsi_set_reset_connect_attempted_flag(args)
#define slsi_p2p_group_start_remove_unsync_vif(args...)			kunit_mock_slsi_p2p_group_start_remove_unsync_vif(args)
#define slsi_select_ap_for_connection(args...)				kunit_mock_slsi_select_ap_for_connection(args)
#define slsi_auto_chan_select_scan(args...)				kunit_mock_slsi_auto_chan_select_scan(args)
#define slsi_send_gratuitous_arp(args...)				kunit_mock_slsi_send_gratuitous_arp(args)
#define slsi_get_ps_disabled_duration(args...)				kunit_mock_slsi_get_ps_disabled_duration(args)
#define slsi_reset_throughput_stats(args...)				kunit_mock_slsi_reset_throughput_stats(args)
#define slsi_hs2_unsync_vif_delete_work(args...)			kunit_mock_slsi_hs2_unsync_vif_delete_work(args)
#define slsi_read_disconnect_ind_timeout(args...)			kunit_mock_slsi_read_disconnect_ind_timeout(args)
#define slsi_set_ito(args...)						kunit_mock_slsi_set_ito(args)
#define slsi_p2p_vif_deactivate(args...)				kunit_mock_slsi_p2p_vif_deactivate(args)
#define slsi_vif_activated(args...)					kunit_mock_slsi_vif_activated(args)
#define slsi_wlan_dump_public_action_subtype(args...)			kunit_mock_slsi_wlan_dump_public_action_subtype(args)
#define slsi_get_exp_peer_frame_subtype(args...)			kunit_mock_slsi_get_exp_peer_frame_subtype(args)
#define slsi_read_regulatory_rules(args...)				kunit_mock_slsi_read_regulatory_rules(args)
#define slsi_sched_scan_stopped(args...)				kunit_mock_slsi_sched_scan_stopped(args)
#define slsi_set_multicast_packet_filters(args...)			kunit_mock_slsi_set_multicast_packet_filters(args)
#define slsi_update_packet_filters(args...)				kunit_mock_slsi_update_packet_filters(args)
#define slsi_remove_bssid_blacklist(args...)				kunit_mock_slsi_remove_bssid_blacklist(args)
#define slsi_reset_channel_flags(args...)				kunit_mock_slsi_reset_channel_flags(args)
#define slsi_get_scan_extra_ies(args...)				kunit_mock_slsi_get_scan_extra_ies(args)
#define slsi_enable_ito(args...)					kunit_mock_slsi_enable_ito(args)
#define slsi_mib_get_sta_tlds_activated(args...)			kunit_mock_slsi_mib_get_sta_tlds_activated(args)
#define slsi_is_mdns_packet(args...)					kunit_mock_slsi_is_mdns_packet(args)
#define slsi_purge_blacklist(args...)					kunit_mock_slsi_purge_blacklist(args)
#define slsi_is_dhcp_packet(args...)					kunit_mock_slsi_is_dhcp_packet(args)
#define slsi_read_unifi_countrylist(args...)				kunit_mock_slsi_read_unifi_countrylist(args)
#define slsi_p2p_init(args...)						kunit_mock_slsi_p2p_init(args)
#define slsi_mib_get_gscan_cap(args...)					kunit_mock_slsi_mib_get_gscan_cap(args)
#define slsi_add_ioctl_blacklist(args...)				kunit_mock_slsi_add_ioctl_blacklist(args)
#define slsi_purge_scan_results(args...)				kunit_mock_slsi_purge_scan_results(args)
#define slsi_read_max_transmit_msdu_lifetime(args...)			kunit_mock_slsi_read_max_transmit_msdu_lifetime(args)
#define slsi_send_power_measurement_vendor_event(args...)		kunit_mock_slsi_send_power_measurement_vendor_event(args)
#define slsi_is_tdls_peer(args...)					kunit_mock_slsi_is_tdls_peer(args)
#define slsi_get_peer_from_qs(args...)					kunit_mock_slsi_get_peer_from_qs(args)
#define slsi_mib_get_sta_tdls_activated(args...)			kunit_mock_slsi_mib_get_sta_tdls_activated(args)
#define slsi_mib_get_sta_tdls_max_peer(args...)				kunit_mock_slsi_mib_get_sta_tdls_max_peer(args)
#define slsi_p2p_queue_unsync_vif_del_work
#define slsi_twt_update_ctrl_flags(args...)				0
#define slsi_create_sysfs_pm(args...)					kunit_mock_slsi_create_sysfs_pm(args)
#define slsi_create_sysfs_ant(args...)					kunit_mock_slsi_create_sysfs_ant(args)
#define slsi_destroy_sysfs_pm(args...)					kunit_mock_slsi_destroy_sysfs_pm(args)
#define slsi_destroy_sysfs_ant(args...)					kunit_mock_slsi_destroy_sysfs_ant(args)
#define slsi_fill_ap_sta_info(args...)					kunit_mock_slsi_fill_ap_sta_info(args)
#define slsi_is_rf_test_mode_enabled(args...)				kunit_mock_slsi_is_rf_test_mode_enabled(args)


static void kunit_mock_slsi_set_packet_filters(struct slsi_dev *sdev, struct net_device *dev)
{
	return;
}

static void kunit_mock_slsi_wakeup_time_work(struct work_struct *work)
{
	return;
}

static void kunit_mock_slsi_get_hw_mac_address(struct slsi_dev *sdev, u8 *addr)
{
	return;
}

static void kunit_mock_slsi_stop_chip(struct slsi_dev *sdev)
{
	return;
}

static int kunit_mock_slsi_ps_port_control(struct slsi_dev *sdev, struct net_device *dev,
					   struct slsi_peer *peer, enum slsi_sta_conn_state s)
{
	return 0;
}

static void kunit_mock_slsi_purge_scan_results_locked(struct netdev_vif *ndev_vif, u16 scan_id)
{
	return;
}

static void kunit_mock_slsi_failure_reset(struct work_struct *work)
{
	return;
}

static void kunit_mock_slsi_fill_enhanced_arp_out_of_order_drop_counter(struct net_device *dev,
									struct sk_buff *skb)
{
	return;
}

static void kunit_mock_slsi_subsystem_reset(struct work_struct *work)
{
	return;
}

static void kunit_mock_slsi_system_error_recovery(struct work_struct *work)
{
	return;
}

static bool kunit_mock_slsi_if_valid_wifi_sharing_channel(struct slsi_dev *sdev, int freq)
{
	return 0;
}

static void kunit_mock_slsi_rx_update_wake_stats(struct slsi_dev *sdev, struct ethhdr *ehdr,
						 size_t buff_len, struct sk_buff *skb)
{
	return;
}

static void kunit_mock_slsi_band_cfg_update(struct slsi_dev *sdev, int band)
{
	return;
}

static int kunit_mock_slsi_is_dns_packet(u8 *data)
{
	return 0;
}

static int kunit_mock_slsi_mib_get_apf_cap(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static void kunit_mock_slsi_vif_cleanup(struct slsi_dev *sdev, struct net_device *dev,
					bool hw_available, bool is_recovery)
{
	return;
}

static int kunit_mock_slsi_read_default_country(struct slsi_dev *sdev, u8 *alpha2, u16 index)
{
	return 0;
}

static void kunit_mock_slsi_destroy_sysfs_macaddr(void)
{
	return;
}

static int kunit_mock_slsi_roam_channel_cache_get_channels_int(struct net_device *dev,
							       struct slsi_roaming_network_map_entry *network_map,
							       u16 *channels)
{
	return 0;
}

static int kunit_mock_slsi_is_wes_action_frame(const struct ieee80211_mgmt *mgmt)
{
	if (mgmt->u.action.category == WLAN_CATEGORY_SPECTRUM_MGMT)
		return 1;
	return 0;
}

static int kunit_mock_slsi_remove_duplicates(u16 arr[], int n)
{
	return 0;
}

static int kunit_mock_slsi_p2p_vif_activate(struct slsi_dev *sdev, struct net_device *dev,
					    struct ieee80211_channel *chan, u16 duration,
					    bool set_probe_rsp_ies)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (ndev_vif->activated && (ndev_vif->ifnum == SLSI_NET_INDEX_P2P ||
			ndev_vif->ifnum == SLSI_NET_INDEX_P2PX_SWLAN))
		return 0;

	return 0;
}

static int kunit_mock_slsi_test_send_hanged_vendor_event(struct net_device *dev)
{
	return 0;
}

static int kunit_mock_slsi_get_ps_entry_counter(struct slsi_dev *sdev, struct net_device *dev, int *mib_value)
{
	return 0;
}

static int kunit_mock_slsi_configure_tx_power_sar_scenario(struct net_device *dev, int mode)
{
	return 0;
}

#if !(defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION < 11) || defined(CONFIG_SCSC_WLAN_SUPPORT_6G)
static u8 kunit_mock_slsi_bss_connect_type_get(struct slsi_dev *sdev, const u8 *ie, size_t ie_len, u8 *ie_type)
{
	const u8 *rsn;
	u8 akm_type = 0;

	rsn = cfg80211_find_ie(WLAN_EID_RSN, ie, ie_len);
	if (rsn) {
		int pos = 0;
		int ie_len = rsn[1] + 2;
		int akm_count = 0;
		int i = 0;

		/* Calculate the position of AKM suite in RSNIE
		 * RSNIE TAG(1 byte) + length(1 byte) + version(2 byte) + Group cipher suite(4 bytes)
		 * pairwise suite count(2 byte) + pairwise suite count * 4 + AKM suite count(2 byte)
		 * pos is the array index not length
		 */
		if (ie_len < 9)
			return akm_type;
		pos = 7 + 2 + (rsn[8] * 4);
		akm_count = rsn[pos + 1];
		pos += 2;
		if (ie_len < (pos + 1))
			return akm_type;
		for (i = 0; i < akm_count; i++) {
			if (ie_len < (pos + 1))
				return akm_type;
			if (rsn[pos + 1] == 0x00 && rsn[pos + 2] == 0x0f && rsn[pos + 3] == 0xac) {
				if (rsn[pos + 4] == 0x08 || rsn[pos + 4] == 0x09)
					akm_type |= SLSI_BSS_SECURED_SAE;
				else if (rsn[pos + 4] == 0x02 || rsn[pos + 4] == 0x04 || rsn[pos + 4] == 0x06)
					akm_type |= SLSI_BSS_SECURED_PSK;
				else if (rsn[pos + 4] == 0x01 || rsn[pos + 4] == 0x03)
					akm_type |= SLSI_BSS_SECURED_1x;
			}
			pos += 4;
		}
	}
	return akm_type;
}
#endif

static int kunit_mock_slsi_set_mac_randomisation_mask(struct slsi_dev *sdev, u8 *mac_address_mask)
{
	return 0;
}

static int kunit_mock_slsi_get_mhs_ws_chan_vsdb(struct wiphy *wiphy, struct net_device *dev,
						struct cfg80211_ap_settings *settings,
						struct slsi_dev *sdev, int *wifi_sharing_channel_switched)
{
	return 0;
}

static int kunit_mock_slsi_get_beacon_cu(struct slsi_dev *sdev, struct net_device *dev, int *mib_value)
{
	return 0;
}

static int kunit_mock_slsi_peer_remove(struct slsi_dev *sdev, struct net_device *dev, struct slsi_peer *peer)
{
	return 0;
}

static void kunit_mock_slsi_peer_reset_stats(struct slsi_dev *sdev, struct net_device *dev, struct slsi_peer *peer)
{
	return;
}

static void kunit_mock_slsi_chip_recovery(struct work_struct *work)
{
	return;
}

static int kunit_mock_slsi_send_hanged_vendor_event(struct slsi_dev *sdev, u16 scsc_panic_code)
{
	if (sdev->device_config.host_state == 111) {
		return -EINVAL;
	}

	return 0;
}

static int kunit_mock_slsi_p2p_get_go_neg_rsp_status(struct net_device *dev, const struct ieee80211_mgmt *mgmt)
{
	return SLSI_P2P_STATUS_CODE_SUCCESS;
}

static void kunit_mock_slsi_sort_array(u16 arr[], int n)
{
	return;
}

static void kunit_mock_slsi_peer_update_assoc_rsp(struct slsi_dev *sdev, struct net_device *dev,
						  struct slsi_peer *peer, struct sk_buff *skb)
{
	if (skb)
		kfree_skb(skb);

	return;
}

static void kunit_mock_slsi_p2p_deinit(struct slsi_dev *sdev, struct netdev_vif *ndev_vif)
{
	return;
}

static int kunit_mock_slsi_set_mib_rssi_boost(struct slsi_dev *sdev, struct net_device *dev,
					      u16 psid, int index, int boost)
{
	return 0;
}

static void kunit_mock_slsi_create_sysfs_debug_dump(void)
{
	return;
}

static int kunit_mock_slsi_read_enhanced_arp_rx_count_by_lower_mac(struct slsi_dev *sdev, struct net_device *dev,
								   u16 psid)
{
	return 0;
}

static void kunit_mock_slsi_wlan_unsync_vif_deactivate(struct slsi_dev *sdev, struct net_device *dev,
						       bool hw_available)
{
	return;
}

static bool kunit_mock_slsi_is_bssid_in_ioctl_blacklist(struct net_device *dev, u8 *bssid)
{
	return 0;
}

static int kunit_mock_slsi_send_forward_beacon_abort_vendor_event(struct slsi_dev *sdev, struct net_device *dev,
								  u16 reason_code)
{
	return 0;
}

static int kunit_mock_slsi_merge_lists(u16 ar1[], int len1, u16 ar2[], int len2, u16 result[])
{
	return 0;
}

static int kunit_mock_slsi_start(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static void kunit_mock_slsi_extract_valid_wifi_sharing_channels(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_stop(struct slsi_dev *sdev)
{
	return;
}

static int kunit_mock_slsi_p2p_dev_null_ies(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static void kunit_mock_slsi_roam_channel_cache_add(struct slsi_dev *sdev, struct net_device *dev,
						   struct sk_buff *skb)
{
	return;
}

static void kunit_mock_slsi_vif_deactivated(struct slsi_dev *sdev, struct net_device *dev)
{
	return;
}

static int kunit_mock_slsi_set_ext_cap(struct slsi_dev *sdev, struct net_device *dev,
				       const u8 *ies, int ie_len, const u8 *ext_cap_mask)
{
	return 0;
}

static void kunit_mock_slsi_scan_cleanup(struct slsi_dev *sdev, struct net_device *dev)
{
	return;
}

static int kunit_mock_slsi_retry_connection(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static void kunit_mock_slsi_destroy_sysfs_debug_dump(void)
{
	return;
}

static int kunit_mock_slsi_set_wifisharing_permitted_channels(struct net_device *dev,
							      char *buffer, int buf_len)
{
	return 0;
}

static int kunit_mock_slsi_set_mib_roam(struct slsi_dev *dev, struct net_device *ndev,
					u16 psid, int value)
{
	return 0;
}

static struct net_device *kunit_mock_slsi_dynamic_interface_create(struct wiphy *wiphy,
								   const char *name,
								   enum nl80211_iftype type,
								   struct vif_params *params,
								   bool is_cfg80211)
{
	struct slsi_dev *sdev = SDEV_FROM_WIPHY(wiphy);
	struct netdev_vif *ndev_vif;

	if (sdev->netdev[type]) {
		ndev_vif = netdev_priv(sdev->netdev[type]);
		ndev_vif->iftype = type;
		sdev->netdev[type]->ieee80211_ptr->iftype = type;
	}

	return sdev->netdev[type];
}

static int kunit_mock_slsi_set_enhanced_pkt_filter(struct net_device *dev, char *command, int buf_len)
{
	return 0;
}

static void kunit_mock_slsi_modify_ies_on_channel_switch(struct net_device *dev,
							 struct cfg80211_ap_settings *settings,
							 struct ieee80211_mgmt *mgmt,
							 u16 beacon_ie_head_len)
{
	return;
}

static int kunit_mock_slsi_set_mib_soft_roaming_enabled(struct slsi_dev *sdev, struct net_device *dev,
							bool enable)
{
	return 0;
}

static bool kunit_mock_slsi_is_bssid_in_hal_blacklist(struct net_device *dev, u8 *bssid)
{
	return 0;
}

static int kunit_mock_slsi_roaming_scan_configure_channels(struct slsi_dev *sdev, struct net_device *dev,
							   const u8 *ssid, u16 *channels)
{
	return 0;
}

static int kunit_mock_slsi_find_chan_idx(u16 chan, u8 hw_mode, int band)
{
	return 0;
}

static int kunit_mock_slsi_check_if_channel_restricted_already(struct slsi_dev *sdev, int channel)
{
	return 0;
}

static int kunit_mock_slsi_ap_prepare_add_info_ies(struct netdev_vif *ndev_vif, const u8 *ies, size_t ies_len)
{
	return 0;
}

static int kunit_mock_slsi_set_latency_crt_data(struct net_device *dev, int latency_mode)
{
	return 0;
}

static int kunit_mock_slsi_get_mib_roam(struct slsi_dev *sdev, u16 psid, int *mib_value)
{
	if (sdev->nan_enabled)
		return 1;
	return 0;
}

static int kunit_mock_slsi_add_probe_ies_request(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static int kunit_mock_slsi_set_uint_mib(struct slsi_dev *sdev, struct net_device *dev, u16 psid, int value)
{
	if (psid == SLSI_PSID_UNIFI_DTIM_MULTIPLIER) {
		value = (value & 0x08) >> 3;

		if (value)
			return 1;
	}

	return 0;
}

static int kunit_mock_slsi_set_boost(struct slsi_dev *sdev, struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (!ndev_vif->activated && sdev->device_state == SLSI_DEVICE_STATE_STARTED &&
			sdev->chip_info_mib.chip_version == 22)
		return 0;

	if (!ndev_vif->activated)
		return 1;

	return 0;
}

static int kunit_mock_slsi_check_if_non_indoor_non_dfs_channel(struct slsi_dev *sdev, int freq)
{
	return 0;
}

static int kunit_mock_slsi_clear_packet_filters(struct slsi_dev *sdev, struct net_device *dev)
{
	if (sdev->nan_enabled)
		return 1;
	return 0;
}

static int kunit_mock_slsi_dump_eth_packet(struct slsi_dev *sdev, struct sk_buff *skb)
{
	return 0;
}

static int kunit_mock_slsi_band_update(struct slsi_dev *sdev, int band)
{
	return 0;
}

static void kunit_mock_slsi_free_connection_params(struct slsi_dev *sdev, struct net_device *dev)
{
	return;
}

static int kunit_mock_slsi_send_rcl_event(struct slsi_dev *sdev, u32 channel_count, u16 *channel_list,
					  u8 *ssid, u8 ssid_len)
{
	return 0;
}

static void kunit_mock_slsi_scan_ind_timeout_handle(struct work_struct *work)
{
	return;
}

static void kunit_mock_slsi_update_supported_channels_regd_flags(struct slsi_dev *sdev)
{
	return;
}

static struct sk_buff *kunit_mock_slsi_dequeue_cached_scan_result(struct slsi_scan *scan, int *count)
{
	struct sk_buff *res = NULL;

	if (scan && scan[SLSI_SCAN_HW_ID].scan_results) {
		if (scan[SLSI_SCAN_HW_ID].scan_results->hidden == 76) {
			if (scan[SLSI_SCAN_HW_ID].scan_results->beacon) {
				res = scan[SLSI_SCAN_HW_ID].scan_results->beacon;
				scan[SLSI_SCAN_HW_ID].scan_results->hidden = 0;
				if (count) {
					*count = 10;
				}
			}
		}
	}

	return res;
}

static int kunit_mock_slsi_sta_ieee80211_mode(struct net_device *dev, u16 current_bss_channel_frequency)
{
	return 0;
}

static int kunit_mock_slsi_ip_address_changed(struct slsi_dev *sdev, struct net_device *dev,
					      __be32 ipaddress)
{
	return 0;
}

static void kunit_mock_slsi_stop_net_dev(struct slsi_dev *sdev, struct net_device *dev)
{
	return;
}

static int kunit_mock_slsi_start_monitor_mode(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static struct slsi_roaming_network_map_entry *kunit_mock_slsi_roam_channel_cache_get(struct net_device *dev,
										     const u8 *ssid)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_roaming_network_map_entry *network_map;

	if (!ssid) {
		return 0;
	} else {
		INIT_LIST_HEAD(&ndev_vif->sta.network_map);
		network_map = kmalloc(sizeof(struct slsi_roaming_network_map_entry), GFP_KERNEL);
		list_add(&(network_map->list), &ndev_vif->sta.network_map);

		network_map->channels_24_ghz = 1;
		network_map->channels_5_ghz = 100;
		return network_map;
	}
}

static void kunit_mock_slsi_arp_q_stuck_work_handle(struct work_struct *work)
{
	return;
}

static void kunit_mock_slsi_dump_stats(struct net_device *dev)
{
	return;
}

static bool kunit_mock_slsi_read_preferred_antenna_from_file(struct slsi_dev *sdev, char *antenna_file_path)
{
	return 0;
}

static int kunit_mock_slsi_get_mhs_ws_chan_rsdb(struct wiphy *wiphy, struct net_device *dev,
						struct cfg80211_ap_settings *settings,
						struct slsi_dev *sdev, int *wifi_sharing_channel_switched)
{
	return 0;
}

static int kunit_mock_slsi_mib_get_rtt_cap(struct slsi_dev *sdev, struct net_device *dev,
					   struct slsi_rtt_capabilities *cap)
{
	return 0;
}

static int kunit_mock_slsi_set_mib_preferred_antenna(struct slsi_dev *dev, u16 value)
{
	return 0;
}

static int kunit_mock_slsi_set_country_update_regd(struct slsi_dev *sdev, const char *alpha2_code, int size)
{
	if (sdev->wlan_unsync_vif_state == WLAN_UNSYNC_VIF_ACTIVE && sdev->current_tspec_id == 12)
		return -EINVAL;

	return 0;
}

static void kunit_mock_slsi_rx_update_mlme_stats(struct slsi_dev *sdev, struct sk_buff *skb)
{
	return;
}

static void kunit_mock_slsi_blacklist_del_work_handle(struct work_struct *work)
{
	return;
}

static void kunit_mock_slsi_create_sysfs_macaddr(void)
{
	return;
}

static void kunit_mock_slsi_peer_update_assoc_req(struct slsi_dev *sdev, struct net_device *dev,
						  struct slsi_peer *peer, struct sk_buff *skb)
{
	return;
}

static void kunit_mock_slsi_create_sysfs_version_info(void)
{
	return;
}

static int kunit_mock_slsi_send_acs_event(struct slsi_dev *sdev, struct net_device *dev,
			struct slsi_acs_selected_channels acs_selected_channels)
{
	return 0;
}

static void kunit_mock_slsi_destroy_sysfs_version_info(void)
{
	return;
}

static int kunit_mock_slsi_handle_disconnect(struct slsi_dev *sdev, struct net_device *dev,
					     u8 *peer_address, u16 reason,
					     u8 *disassoc_rsp_ie, u32 disassoc_rsp_ie_len)
{
	if (sdev->device_config.user_suspend_mode == ERANGE)
		return -ERANGE;

	return 0;
}

static void kunit_mock_slsi_collect_chipset_logs(struct work_struct *work)
{
	return;
}

static void kunit_mock_slsi_roam_channel_cache_prune(struct net_device *dev, int seconds, char *ssid)
{
	return;
}

static int kunit_mock_slsi_send_twt_notification(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static void kunit_mock_slsi_trigger_service_failure(struct work_struct *work)
{
	return;
}

static int kunit_mock_slsi_send_max_transmit_msdu_lifetime(struct slsi_dev *dev, struct net_device *ndev,
							   u32 msdu_lifetime)
{
	return 0;
}

static int kunit_mock_slsi_set_latency_mode(struct net_device *dev, int latency_mode, int cmd_len)
{
	return 0;
}

static int kunit_mock_slsi_send_forward_beacon_vendor_event(struct slsi_dev *sdev, struct net_device *dev,
							    const u8 *ssid, const int ssid_len,
							    const u8 *bssid, u8 channel,
							    const u16 beacon_int, const u64 timestamp,
							    const u64 sys_time)
{
	return 0;
}

static void kunit_mock_slsi_abort_sta_scan(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_clear_offchannel_data(struct slsi_dev *sdev, bool acquire_lock)
{
	return;
}

static int kunit_mock_slsi_send_twt_setup_event(struct slsi_dev *sdev, struct net_device *dev,
						struct slsi_twt_setup_event setup_event)
{
	return 0;
}

static int kunit_mock_slsi_get_public_action_subtype(const struct ieee80211_mgmt *mgmt)
{
	int subtype = SLSI_PA_INVALID;
	/* Vendor specific Public Action (0x09), P2P OUI (0x50, 0x6f, 0x9a), P2P Subtype (0x09) */
	u8 p2p_pa_frame[5] = { 0x09, 0x50, 0x6f, 0x9a, 0x09 };
	/* Vendor specific Public Action (0x09), P2P OUI (0x50, 0x6f, 0x9a), DPP Subtype (0x1a) */
	u8 dpp_pa_frame[5] = { 0x09, 0x50, 0x6f, 0x9a, 0x1a};
	u8 *action = (u8 *)&mgmt->u.action.u;

	if (memcmp(&action[0], p2p_pa_frame, 5) == 0) {
		subtype = action[5];
	} else if (memcmp(&action[0], dpp_pa_frame, 5) == 0) {
		/* 6th pos will be having Crypto Suite, 7th pos is subtype of DPP action frame */
		subtype = action[6] | SLSI_PA_DPP_DUMMY_SUBTYPE_MASK;
	} else {
		/* For service discovery action frames dummy subtype is used */
		switch (action[0]) {
		case SLSI_PA_GAS_INITIAL_REQ:
		case SLSI_PA_GAS_INITIAL_RSP:
		case SLSI_PA_GAS_COMEBACK_REQ:
		case SLSI_PA_GAS_COMEBACK_RSP:
			subtype = (action[0] | SLSI_PA_GAS_DUMMY_SUBTYPE_MASK);
			break;
		}
	}

	return subtype;
}

static int kunit_mock_slsi_read_regulatory_rules_fw(struct slsi_dev *sdev,
						    struct slsi_802_11d_reg_domain *domain_info,
						    const char *alpha2)
{
	return 0;
}

static int kunit_mock_slsi_send_twt_teardown(struct slsi_dev *sdev, struct net_device *dev,
					     u16 setup_id, u8 result_code)
{
	return 0;
}

static bool kunit_mock_slsi_is_bssid_in_blacklist(struct slsi_dev *sdev, struct net_device *dev,
						  u8 *bssid)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (sdev->device_state == SLSI_DEVICE_STATE_STARTED && ndev_vif->power_mode == 12 &&
			ndev_vif->set_power_mode == 43)
		return true;
	else if (sdev->device_state == SLSI_DEVICE_STATE_STARTED && !ndev_vif->acl_data_supplicant)
		return false;
	return false;
}

static int kunit_mock_slsi_p2p_dev_probe_rsp_ie(struct slsi_dev *sdev, struct net_device *dev,
						u8 *probe_rsp_ie, size_t probe_rsp_ie_len)
{
	return 0;
}

static int kunit_mock_slsi_wlan_unsync_vif_activate(struct slsi_dev *sdev, struct net_device *dev,
						    struct ieee80211_channel *chan, u16 wait)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (sdev->device_config.qos_info == 267)
		return 0;

	return 0;
}

static struct slsi_peer *kunit_mock_slsi_peer_add(struct slsi_dev *sdev, struct net_device *dev,
						  u8 *peer_address, u16 aid)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = NULL;
	u16 queueset = 0;

	if (WLBT_WARN_ON(!aid)) {
		SLSI_NET_ERR(dev, "Invalid aid(0) received\n");
		return NULL;
	}
	queueset = MAP_AID_TO_QS(aid);

	if (!peer_address) {
		SLSI_NET_WARN(dev, "Peer without address\n");
		return NULL;
	}

	if (WLBT_WARN_ON(!ndev_vif->activated))
		return NULL;

	peer = ndev_vif->peer_sta_record[0];

	if (peer) {
		peer->aid = aid;
		peer->queueset = queueset;
		SLSI_ETHER_COPY(peer->address, peer_address);
		peer->assoc_resp_ie = NULL;
		peer->is_wps = false;
		peer->connected_state = SLSI_STA_CONN_STATE_DISCONNECTED;
		peer->ndp_count = 1;
		peer->valid = true;
	}

	if (ndev_vif->sta.tdls_enabled)
		ndev_vif->sta.tdls_peer_sta_records++;
	else
		ndev_vif->peer_sta_records++;

	ndev_vif->cfg80211_sinfo_generation++;

	return peer;
}

static int kunit_mock_slsi_is_tcp_sync_packet(struct net_device *dev, struct sk_buff *skb)
{
	return 0;
}

static void kunit_mock_slsi_set_reset_connect_attempted_flag(struct slsi_dev *sdev, struct net_device *dev,
							     const u8 *bssid)
{
	return;
}

static void kunit_mock_slsi_p2p_group_start_remove_unsync_vif(struct slsi_dev *sdev)
{
	return;
}

static bool kunit_mock_slsi_select_ap_for_connection(struct slsi_dev *sdev, struct net_device *dev,
						     const u8 **bssid, struct ieee80211_channel **channel,
						     bool retry)
{
	return 0;
}

static int kunit_mock_slsi_auto_chan_select_scan(struct slsi_dev *sdev, int n_channels,
						 struct ieee80211_channel *channels[])
{
	return 0;
}

static int kunit_mock_slsi_send_gratuitous_arp(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static int kunit_mock_slsi_get_ps_disabled_duration(struct slsi_dev *sdev, struct net_device *dev,
						    int *mib_value)
{
	return 0;
}

static void kunit_mock_slsi_reset_throughput_stats(struct net_device *dev)
{
	return;
}

static void kunit_mock_slsi_hs2_unsync_vif_delete_work(struct work_struct *work)
{
	return;
}

static int kunit_mock_slsi_read_disconnect_ind_timeout(struct slsi_dev *sdev, u16 psid)
{
	return 0;
}

static int kunit_mock_slsi_set_ito(struct net_device *dev, char *command, int buf_len)
{
	return 0;
}

static void kunit_mock_slsi_p2p_vif_deactivate(struct slsi_dev *sdev, struct net_device *dev,
					       bool hw_available)
{
	return;
}

static int kunit_mock_slsi_vif_activated(struct slsi_dev *sdev, struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	/* MUST have cleared any peer records previously */
	WLBT_WARN_ON(ndev_vif->peer_sta_records);

	if (WLBT_WARN_ON(ndev_vif->activated))
		return -EALREADY;

	if (ndev_vif->sta.vif_status == SLSI_VIF_STATUS_CONNECTING)
		return 1;

	if (ndev_vif->vif_type == FAPI_VIFTYPE_AP) {
#ifdef CONFIG_SCSC_WLAN_MAC_ACL_PER_MAC
		kfree(ndev_vif->ap.acl_data_blacklist);
		ndev_vif->ap.acl_data_blacklist = NULL;
#endif
	}

	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
		/* MUST have cleared any tdls peer records previously */
		WLBT_WARN_ON(ndev_vif->sta.tdls_peer_sta_records);

		ndev_vif->sta.tdls_peer_sta_records = 0;
		ndev_vif->sta.tdls_enabled = false;
		ndev_vif->sta.roam_in_progress = false;
		ndev_vif->sta.nd_offload_enabled = true;

		memset(ndev_vif->sta.keepalive_host_tag, 0, sizeof(ndev_vif->sta.keepalive_host_tag));
	}

	ndev_vif->cfg80211_sinfo_generation = 0;
	ndev_vif->peer_sta_records = 0;
	ndev_vif->mgmt_tx_data.exp_frame = SLSI_PA_INVALID;
	ndev_vif->set_tid_attr.mode = SLSI_NETIF_SET_TID_OFF;
	ndev_vif->set_tid_attr.uid = 0;
	ndev_vif->set_tid_attr.tid = 0;
	ndev_vif->activated = true;

	return 0;
}

static void kunit_mock_slsi_wlan_dump_public_action_subtype(struct slsi_dev *sdev,
							    struct ieee80211_mgmt *mgmt,
							    bool tx)
{
	return;
}

static u8 kunit_mock_slsi_get_exp_peer_frame_subtype(u8 subtype)
{
	switch (subtype) {
	/* Peer response is expected for following frames */
	case SLSI_P2P_PA_GO_NEG_REQ:
	case SLSI_P2P_PA_GO_NEG_RSP:
	case SLSI_P2P_PA_INV_REQ:
	case SLSI_P2P_PA_DEV_DISC_REQ:
	case SLSI_P2P_PA_PROV_DISC_REQ:
	case SLSI_PA_GAS_INITIAL_REQ_SUBTYPE:
	case SLSI_PA_GAS_COMEBACK_REQ_SUBTYPE:
	case SLSI_PA_DPP_AUTHENTICATION_REQ_SUBTYPE:
		return subtype + 1;
	default:
		return SLSI_PA_INVALID;
	}
}

static int kunit_mock_slsi_read_regulatory_rules(struct slsi_dev *sdev,
						 struct slsi_802_11d_reg_domain *domain_info,
						 const char *alpha2)
{
	return 0;
}

static void kunit_mock_slsi_sched_scan_stopped(struct work_struct *work)
{
	return;
}

static int kunit_mock_slsi_set_multicast_packet_filters(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static int kunit_mock_slsi_update_packet_filters(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static int kunit_mock_slsi_remove_bssid_blacklist(struct slsi_dev *sdev, struct net_device *dev,
						  u8 *addr)
{
	return 0;
}

static void kunit_mock_slsi_reset_channel_flags(struct slsi_dev *sdev)
{
	return;
}

static u8 *kunit_mock_slsi_get_scan_extra_ies(struct slsi_dev *sdev, const u8 *ies,
					      int total_len, int *extra_len)
{
	u8 *res;

	if (total_len) {
		res = "\x23\x43\x56\x99";
		return res;
	}

	return NULL;
}

static int kunit_mock_slsi_enable_ito(struct net_device *dev, char *command, int buf_len)
{
	return 0;
}

static int kunit_mock_slsi_mib_get_sta_tlds_activated(struct slsi_dev *sdev, struct net_device *dev,
						      bool *tdls_supported)
{
	if (sdev == NULL || dev == NULL)
		return 1;
	else
		*tdls_supported = true;

	return 0;
}

static int kunit_mock_slsi_is_mdns_packet(u8 *data)
{
	return 0;
}

static void kunit_mock_slsi_purge_blacklist(struct netdev_vif *ndev_vif)
{
	return;
}

static int kunit_mock_slsi_is_dhcp_packet(u8 *data)
{
	return 0;
}

static int kunit_mock_slsi_read_unifi_countrylist(struct slsi_dev *sdev, u16 psid)
{
	return 0;
}

static int kunit_mock_slsi_p2p_init(struct slsi_dev *sdev, struct netdev_vif *ndev_vif)
{
	if (ndev_vif->ifnum == SLSI_NET_INDEX_P2P && sdev->netdev_up_count == 99)
		return 1;
	return 0;
}

static int kunit_mock_slsi_mib_get_gscan_cap(struct slsi_dev *sdev, struct slsi_nl_gscan_capabilities *cap)
{
	return 0;
}

static int kunit_mock_slsi_add_ioctl_blacklist(struct slsi_dev *sdev, struct net_device *dev, u8 *addr)
{
	return 0;
}

static void kunit_mock_slsi_purge_scan_results(struct netdev_vif *ndev_vif, u16 scan_id)
{
	return;
}

static int kunit_mock_slsi_read_max_transmit_msdu_lifetime(struct slsi_dev *dev, struct net_device *ndev,
							   u32 *msdu_lifetime)
{
	return 0;
}

static int kunit_mock_slsi_send_power_measurement_vendor_event(struct slsi_dev *sdev, s16 power_in_db)
{
	return 0;
}

static bool kunit_mock_slsi_is_tdls_peer(struct net_device *dev, struct slsi_peer *peer)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	return (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) && (peer->aid >= SLSI_TDLS_PEER_INDEX_MIN);
}

static struct slsi_peer *kunit_mock_slsi_get_peer_from_qs(struct slsi_dev *sdev,
							  struct net_device *dev, u16 queueset)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (sdev && sdev->device_state < 0)
		return NULL;

	if (!ndev_vif->peer_sta_record[queueset] || !ndev_vif->peer_sta_record[queueset]->valid)
		return NULL;

	return ndev_vif->peer_sta_record[queueset];
}

static int slsi_mib_get_sta_tdls_activated(struct slsi_dev *sdev, struct net_device *dev, bool *tdls_supported)
{
	return 0;
}

static int slsi_mib_get_sta_tdls_max_peer(struct slsi_dev *sdev, struct net_device *dev, struct netdev_vif *ndev_vif)
{
	if (!ndev_vif)
		return -EINVAL;

	ndev_vif->sta.tdls_max_peer = 4;

	return 0;
}

static void slsi_create_sysfs_pm(void)
{
}

static void slsi_create_sysfs_ant(void)
{
}

static void slsi_destroy_sysfs_pm(void)
{
}

static void slsi_destroy_sysfs_ant(void)
{
}

static int kunit_mock_slsi_fill_ap_sta_info(struct slsi_dev *sdev, struct net_device *dev,
					    const u8 *peer_mac, struct slsi_ap_sta_info *ap_sta_info,
					    const u16 reason_code)
{
	return 0;
}

static bool kunit_mock_slsi_is_rf_test_mode_enabled(void)
{
	return false;
}
#endif
