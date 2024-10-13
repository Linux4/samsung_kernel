/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Authors	:
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
 * Jingxiang Li <Jingxiang.li@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SPRDWL_CFG80211_H__
#define __SPRDWL_CFG80211_H__

#include <net/cfg80211.h>

/* auth type */
#define SPRDWL_AUTH_OPEN		0
#define SPRDWL_AUTH_SHARED		1
/* parise or group key type */
#define SPRDWL_GROUP			0
#define SPRDWL_PAIRWISE			1
/* cipher suite */
#define WLAN_CIPHER_SUITE_PMK           0x000FACFF
/* AKM suite */
#define WLAN_AKM_SUITE_FT_8021X		0x000FAC03
#define WLAN_AKM_SUITE_FT_PSK		0x000FAC04
#define WLAN_AKM_SUITE_WAPI_CERT	0x00147201
#define WLAN_AKM_SUITE_WAPI_PSK		0x00147202

#define SPRDWL_AKM_SUITE_NONE		(0)
#define SPRDWL_AKM_SUITE_8021X		(1)
#define SPRDWL_AKM_SUITE_PSK		(2)
#define SPRDWL_AKM_SUITE_FT_8021X	(3)
#define SPRDWL_AKM_SUITE_FT_PSK		(4)
#define SPRDWL_AKM_SUITE_WAPI_PSK	(4)
#define SPRDWL_AKM_SUITE_8021X_SHA256	(5)
#define SPRDWL_AKM_SUITE_PSK_SHA256	(6)
#define SPRDWL_AKM_SUITE_WAPI_CERT	(12)

/* determine the actual values for the macros below*/
#define SPRDWL_MAX_SCAN_SSIDS		12
#define SPRDWL_MAX_SCAN_IE_LEN		2304
#define SPRDWL_MAX_NUM_PMKIDS		4
#define SPRDWL_MAX_KEY_INDEX		3
#define SPRDWL_SCAN_TIMEOUT_MS		8000
#define SPRDWL_MAX_PFN_LIST_COUNT	16

enum sprdwl_mode {
	SPRDWL_MODE_NONE,
	SPRDWL_MODE_STATION,
	SPRDWL_MODE_AP,

	SPRDWL_MODE_P2P_DEVICE = 4,
	SPRDWL_MODE_P2P_CLIENT,
	SPRDWL_MODE_P2P_GO,

	SPRDWL_MODE_MAX,
};

enum sm_state {
	SPRDWL_UNKNOWN = 0,
	SPRDWL_SCANNING,
	SPRDWL_SCAN_ABORTING,
	SPRDWL_DISCONNECTING,
	SPRDWL_DISCONNECTED,
	SPRDWL_CONNECTING,
	SPRDWL_CONNECTED
};

enum connect_result {
	SPRDWL_CONNECT_SUCCESS,
	SPRDWL_CONNECT_FAILED,
	SPRDWL_ROAM_SUCCESS,
	SPRDWL_ROAM_FAILED
};

enum acl_mode {
	SPRDWL_ACL_MODE_DISABLE,
	SPRDWL_ACL_MODE_WHITELIST,
	SPRDWL_ACL_MODE_BLACKLIST,
};

struct sprdwl_scan_ssid {
	u8 len;
	u8 ssid[0];
} __packed;

struct sprdwl_sched_scan_buf {
	u32 interval;
	u32 flags;
	s32 rssi_thold;
	u8 channel[16];

	u32 n_ssids;
	u8 *ssid[16];
	u32 n_match_ssids;
	u8 *mssid[16];

	const u8 *ie;
	size_t ie_len;
};

struct sprdwl_ieee80211_regdomain {
	u32 n_reg_rules;
	char alpha2[2];
	struct ieee80211_reg_rule reg_rules[];
};

struct sprdwl_vif;
struct sprdwl_priv;

void sprdwl_setup_wiphy(struct wiphy *wiphy, struct sprdwl_priv *priv);

int sprdwl_init_fw(struct sprdwl_vif *vif);
int sprdwl_uninit_fw(struct sprdwl_vif *vif);

struct sprdwl_vif *mode_to_vif(struct sprdwl_priv *priv, u8 vif_mode);
void sprdwl_put_vif(struct sprdwl_vif *vif);

void sprdwl_report_softap(struct sprdwl_vif *vif, u8 is_connect, u8 *addr,
			  u8 *req_ie, u16 req_ie_len);
void sprdwl_scan_timeout(unsigned long data);
void sprdwl_scan_done(struct sprdwl_vif *vif, bool abort);
void sprdwl_sched_scan_done(struct sprdwl_vif *vif, bool abort);
void sprdwl_report_scan_result(struct sprdwl_vif *vif, u16 chan, s16 rssi,
			       u8 *frame, u16 len);
void sprdwl_report_connection(struct sprdwl_vif *vif, u8 *bssid,
			      u8 chanel_num, s8 signal,
			      u8 *beacon_ie, u16 beacon_ie_len,
			      u8 *req_ie, u16 req_ie_len, u8 *resp_ie,
			      u16 resp_ie_len, u8 status_code);
void sprdwl_report_disconnection(struct sprdwl_vif *vif, u16 reason_code);
void sprdwl_report_mic_failure(struct sprdwl_vif *vif, u8 is_mcast, u8 key_id);
void sprdwl_cfg80211_dump_frame_prot_info(int send, int freq,
					  const unsigned char *buf, int len);
void sprdwl_report_remain_on_channel_expired(struct sprdwl_vif *vif);
void sprdwl_report_mgmt_tx_status(struct sprdwl_vif *vif, u64 cookie,
				  const u8 *buf, u32 len, u8 ack);
void sprdwl_report_rx_mgmt(struct sprdwl_vif *vif, u8 chan, const u8 *buf,
			   size_t len);
void sprdwl_report_mgmt_deauth(struct sprdwl_vif *vif, const u8 *buf,
			       size_t len);
void sprdwl_report_mgmt_disassoc(struct sprdwl_vif *vif, const u8 *buf,
				 size_t len);
void sprdwl_report_cqm(struct sprdwl_vif *vif, u8 rssi_event);
void sprdwl_report_tdls(struct sprdwl_vif *vif, const u8 *peer,
			u8 oper, u16 reason_code);
#endif
