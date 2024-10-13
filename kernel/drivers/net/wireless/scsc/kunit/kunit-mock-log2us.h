/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_LOG2US_H__
#define __KUNIT_MOCK_LOG2US_H__

#include "../log2us.h"

#define slsi_conn_log2us_roam_scan_start(args...)		kunit_mock_slsi_conn_log2us_roam_scan_start(args)
#define slsi_conn_log2us_roam_result(args...)			kunit_mock_slsi_conn_log2us_roam_result(args)
#define slsi_conn_log2us_eapol_tx(args...)			kunit_mock_slsi_conn_log2us_eapol_tx(args)
#define slsi_conn_log2us_nr_frame_resp(args...)			kunit_mock_slsi_conn_log2us_nr_frame_resp(args)
#define slsi_conn_log2us_deinit(args...)			kunit_mock_slsi_conn_log2us_deinit(args)
#define slsi_conn_log2us_eap_tx(args...)			kunit_mock_slsi_conn_log2us_eap_tx(args)
#define slsi_conn_log2us_beacon_report_response(args...)	kunit_mock_slsi_conn_log2us_beacon_report_response(args)
#define slsi_conn_log2us_eapol_ptk(args...)			kunit_mock_slsi_conn_log2us_eapol_ptk(args)
#define get_eap_type_from_val(args...)				kunit_mock_get_eap_type_from_val(args)
#define slsi_conn_log2us_disconnect(args...)			kunit_mock_slsi_conn_log2us_disconnect(args)
#define slsi_conn_log2us_btm_query(args...)			kunit_mock_slsi_conn_log2us_btm_query(args)
#define slsi_conn_log2us_ncho_mode(args...)			kunit_mock_slsi_conn_log2us_ncho_mode(args)
#define slsi_conn_log2us_connecting(args...)			kunit_mock_slsi_conn_log2us_connecting(args)
#define slsi_conn_log2us_roam_scan_result(args...)		kunit_mock_slsi_conn_log2us_roam_scan_result(args)
#define slsi_conn_log2us_roam_scan_save(args...)		kunit_mock_slsi_conn_log2us_roam_scan_save(args)
#define slsi_conn_log2us_btm_resp(args...)			kunit_mock_slsi_conn_log2us_btm_resp(args)
#define slsi_conn_log2us_connecting_fail(args...)		kunit_mock_slsi_conn_log2us_connecting_fail(args)
#define slsi_conn_log2us_dhcp_tx(args...)			kunit_mock_slsi_conn_log2us_dhcp_tx(args)
#define slsi_conn_log2us_btm_cand(args...)			kunit_mock_slsi_conn_log2us_btm_cand(args)
#define slsi_conn_log2us_nr_frame_req(args...)			kunit_mock_slsi_conn_log2us_nr_frame_req(args)
#define slsi_conn_log2us_roam_scan_done(args...)		kunit_mock_slsi_conn_log2us_roam_scan_done(args)
#define slsi_conn_log2us_auth_req(args...)			kunit_mock_slsi_conn_log2us_auth_req(args)
#define slsi_conn_log2us_assoc_resp(args...)			kunit_mock_slsi_conn_log2us_assoc_resp(args)
#define slsi_eapol_eap_handle_tx_status(args...)		kunit_mock_slsi_eapol_eap_handle_tx_status(args)
#define slsi_conn_log2us_auth_resp(args...)			kunit_mock_slsi_conn_log2us_auth_resp(args)
#define slsi_conn_log2us_btm_req(args...)			kunit_mock_slsi_conn_log2us_btm_req(args)
#define slsi_conn_log2us_dhcp(args...)				kunit_mock_slsi_conn_log2us_dhcp(args)
#define slsi_conn_log2us_init(args...)				kunit_mock_slsi_conn_log2us_init(args)
#define slsi_conn_log2us_eapol_gtk_tx(args...)			kunit_mock_slsi_conn_log2us_eapol_gtk_tx(args)
#define slsi_conn_log2us_disassoc(args...)			kunit_mock_slsi_conn_log2us_disassoc(args)
#define slsi_conn_log2us_assoc_req(args...)			kunit_mock_slsi_conn_log2us_assoc_req(args)
#define slsi_conn_log2us_eap_with_len(args...)			kunit_mock_slsi_conn_log2us_eap_with_len(args)
#define slsi_conn_log2us_eapol_gtk(args...)			kunit_mock_slsi_conn_log2us_eapol_gtk(args)
#define slsi_conn_log2us_beacon_report_request(args...)		kunit_mock_slsi_conn_log2us_beacon_report_request(args)
#define slsi_conn_log2us_eapol_ptk_tx(args...)			kunit_mock_slsi_conn_log2us_eapol_ptk_tx(args)
#define slsi_conn_log2us_eap(args...)				kunit_mock_slsi_conn_log2us_eap(args)
#define slsi_conn_log2us_deauth(args...)			kunit_mock_slsi_conn_log2us_deauth(args)


static void kunit_mock_slsi_conn_log2us_roam_scan_start(struct slsi_dev *sdev, struct net_device *dev,
							int reason, int roam_rssi_val,
							short chan_utilisation,
							int rssi_thresh, u64 timestamp)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_roam_result(struct slsi_dev *sdev, struct net_device *dev,
						    char *bssid, u64 timestamp, bool roam_candidate)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_eapol_tx(struct slsi_dev *sdev, struct net_device *dev,
						 u32 status_code)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_nr_frame_resp(struct slsi_dev *sdev, struct net_device *dev,
						      int dialog_token, int freq_count, int *freq_list,
						      int report_number)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_deinit(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_eap_tx(struct slsi_dev *sdev, struct netdev_vif *ndev_vif,
					       int eap_length, int eap_type, char *tx_status_str)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_beacon_report_response(struct slsi_dev *sdev, struct net_device *dev,
							       int dialog_token, int report_number)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_eapol_ptk(struct slsi_dev *sdev, struct net_device *dev,
				int eapol_msg_type)
{
	return;
}

static u8 *kunit_mock_get_eap_type_from_val(int val, u8 *str)
{
	return NULL;
}

static void kunit_mock_slsi_conn_log2us_disconnect(struct slsi_dev *sdev, struct net_device *dev,
				 const unsigned char *bssid, int reason)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_btm_query(struct slsi_dev *sdev, struct net_device *dev,
				int dialog, int reason)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_ncho_mode(struct slsi_dev *sdev, struct net_device *dev,
						  int enable)
{
	return;
}

static void slsi_conn_log2us_connecting(struct slsi_dev *sdev, struct net_device *dev,
					struct cfg80211_connect_params *sme)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_roam_scan_result(struct slsi_dev *sdev,
							 struct net_device *dev,
							 bool curr,
							 char *bssid,
							 int freq,
							 int rssi,
							 short cu,
							 int score,
							 int tp_score,
							 bool eligible)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_roam_scan_save(struct slsi_dev *sdev, struct net_device *dev,
						       int scan_type, int freq_count, int *freq_list)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_btm_resp(struct slsi_dev *sdev, struct net_device *dev,
						 int dialog, int btm_mode, int delay, char *bssid)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_connecting_fail(struct slsi_dev *sdev, struct net_device *dev,
							const unsigned char *bssid,
							int freq, int reason)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_dhcp_tx(struct slsi_dev *sdev, struct net_device *dev,
						char *str, char *tx_status)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_btm_cand(struct slsi_dev *sdev, struct net_device *dev,
						 char *bssid, int prefer)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_nr_frame_req(struct slsi_dev *sdev, struct net_device *dev,
						     int dialog_token, char *ssid)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_roam_scan_done(struct slsi_dev *sdev, struct net_device *dev,
						       u64 timestamp)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_auth_req(struct slsi_dev *sdev, struct net_device *dev,
						 const unsigned char *bssid,
						 int auth_algo, int sae_type,
						 int sn, int status, u32 tx_status,
						 int is_roaming)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_assoc_resp(struct slsi_dev *sdev, struct net_device *dev,
						   const unsigned char *bssid, int sn, int status,
						   int mgmt_frame_subtype, int aid)
{
	return;
}

static void kunit_mock_slsi_eapol_eap_handle_tx_status(struct slsi_dev *sdev,
						       struct netdev_vif *ndev_vif,
						       u16 a, u16 b)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_auth_resp(struct slsi_dev *sdev, struct net_device *dev,
						  const unsigned char *bssid, int auth_algo,
						  int sae_type, int sn, int status, int is_roaming)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_btm_req(struct slsi_dev *sdev, struct net_device *dev,
						int dialog, int btm_mode, int disassoc_timer,
						int validity_time, int candidate_count)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_dhcp(struct slsi_dev *sdev, struct net_device *dev, char *str)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_init(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_eapol_gtk_tx(struct slsi_dev *sdev, u32 status_code)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_disassoc(struct slsi_dev *sdev, struct net_device *dev,
						 char *str_type, const unsigned char *bssid,
						 int sn, int status, char *vs_ie)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_assoc_req(struct slsi_dev *sdev, struct net_device *dev,
						  const unsigned char *bssid, int sn,
						  int tx_status, int mgmt_frame_subtype)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_eap_with_len(struct slsi_dev *sdev, struct net_device *dev,
						     u8 *eap_type, int eap_length)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_eapol_gtk(struct slsi_dev *sdev, struct net_device *dev,
						  int eapol_msg_type)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_beacon_report_request(struct slsi_dev *sdev, struct net_device *dev,
							      int dialog_token, int operating_class,
							      char *string, int measure_duration,
							      char *measure_mode, u8 request_mode)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_eapol_ptk_tx(struct slsi_dev *sdev, u32 status_code)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_eap(struct slsi_dev *sdev, struct net_device *dev,
					    u8 *eap_type)
{
	return;
}

static void kunit_mock_slsi_conn_log2us_deauth(struct slsi_dev *sdev, struct net_device *dev,
					       char *str_type, const unsigned char *bssid,
					       int sn, int status, char *vs_ie)
{
	return;
}
#endif
