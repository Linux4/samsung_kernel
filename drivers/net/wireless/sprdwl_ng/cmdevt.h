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

#ifndef __SPRDWL_CMD_H__
#define __SPRDWL_CMD_H__

#include "msg.h"

#define SPRDWL_VALID_CONFIG		(0x80)
#define  CMD_WAIT_TIMEOUT		(3000)
/* cipher type */
#define SPRDWL_CIPHER_NONE		0
#define SPRDWL_CIPHER_WEP40		1
#define SPRDWL_CIPHER_WEP104		2
#define SPRDWL_CIPHER_TKIP		3
#define SPRDWL_CIPHER_CCMP		4
#define SPRDWL_CIPHER_AP_TKIP		5
#define SPRDWL_CIPHER_AP_CCMP		6
#define SPRDWL_CIPHER_WAPI		7
#define SPRDWL_CIPHER_AES_CMAC		8
#define SPRDWL_MAX_SDIO_SEND_COUT	1024
#define SPRDWL_SCHED_SCAN_BUF_END	(1<<0)

#define SPRDWL_SEND_FLAG_IFRC		(1<<0)
#define SPRDWL_SEND_FLAG_SSID		(1<<1)
#define SPRDWL_SEND_FLAG_MSSID		(1<<2)
#define SPRDWL_SEND_FLAG_IE		(1<<4)

#define SPRDWL_TDLS_ENABLE_LINK		11
#define SPRDWL_TDLS_DISABLE_LINK	12
#define SPRDWL_TDLS_TEARDOWN		3
#define SPRDWL_TDLS_DISCOVERY_RESPONSE		14
#define SPRDWL_TDLS_START_CHANNEL_SWITCH	13
#define SPRDWL_TDLS_CANCEL_CHANNEL_SWITCH	14

#define SPRDWL_IPV4			1
#define SPRDWL_IPV6			2
#define SPRDWL_IPV4_ADDR_LEN		4
#define SPRDWL_IPV6_ADDR_LEN		16

/* wnm feature */
#define SPRDWL_11V_BTM                  BIT(0)
#define SPRDWL_11V_PARP                 BIT(1)
#define SPRDWL_11V_MIPM                 BIT(2)
#define SPRDWL_11V_DMS                  BIT(3)
#define SPRDWL_11V_SLEEP                BIT(4)
#define SPRDWL_11V_TFS                  BIT(5)
#define SPRDWL_11V_ALL_FEATURE          0xFFFF

enum SPRDWL_CMD_LIST {
	WIFI_CMD_MIN = 0,
	/* All Interface */
	WIFI_CMD_GET_INFO = 1,
	WIFI_CMD_SET_REGDOM,
	WIFI_CMD_OPEN,
	WIFI_CMD_CLOSE,
	WIFI_CMD_POWER_SAVE,
	WIFI_CMD_SET_PARAM,
	WIFI_CMD_SET_CHANNEL,
	WIFI_CMD_REQ_LTE_CONCUR,

	/* Connect */
	WIFI_CMD_CONNECT = 10,

	/* Station */
	WIFI_CMD_SCAN = 11,
	WIFI_CMD_SCHED_SCAN,
	WIFI_CMD_DISCONNECT,
	WIFI_CMD_KEY,
	WIFI_CMD_SET_PMKSA,
	WIFI_CMD_GET_STATION,

	/* SoftAP */
	WIFI_CMD_START_AP = 17,
	WIFI_CMD_DEL_STATION,
	WIFI_CMD_SET_BLACKLIST,
	WIFI_CMD_SET_WHITELIST,

	/* P2P */
	WIFI_CMD_TX_MGMT = 21,
	WIFI_CMD_REGISTER_FRAME,
	WIFI_CMD_REMAIN_CHAN,
	WIFI_CMD_CANCEL_REMAIN_CHAN,

	/* Public/New Feature */
	WIFI_CMD_SET_IE = 25,
	WIFI_CMD_NOTIFY_IP_ACQUIRED,
	/* Roaming */
	WIFI_CMD_SET_CQM,	/* Uplayer Roaming */
	WIFI_CMD_SET_ROAM_OFFLOAD,	/* fw Roaming */
	WIFI_CMD_SET_MEASUREMENT,
	WIFI_CMD_SET_QOS_MAP,
	WIFI_CMD_TDLS,
	WIFI_CMD_11V,

	/* NPI/DEBUG/OTHER */
	WIFI_CMD_NPI_MSG = 33,
	WIFI_CMD_NPI_GET,

	WIFI_CMD_ASSERT,
	WIFI_CMD_FLUSH_SDIO,

	/* WMM Admisson Control */
	WIFI_CMD_ADD_TX_TS = 37,
	WIFI_CMD_DEL_TX_TS = 38,

	/* Multicast filter */
	WIFI_CMD_MULTICAST_FILTER,
	WIFI_CMD_LLSTAT = 56,

	/* gscan */
	WIFI_CMD_GSCAN = 57,
	WIFI_CMD_MAX
};

enum SPRDWL_SUBCMD {
	SPRDWL_SUBCMD_GET = 1,
	SPRDWL_SUBCMD_SET,
	SPRDWL_SUBCMD_ADD,
	SPRDWL_SUBCMD_DEL,
	SPRDWL_SUBCMD_FLUSH,
	SPRDWL_SUBCMD_UPDATE,
	SPRDWL_SUBCMD_ENABLE,
	SPRDWL_SUBCMD_DISABLE,
	SPRDWL_SUBCMD_MAX
};

enum GSCAN_SUB_COMMAND {
	SPRDWL_GSCAN_SUBCMD_GET_CAPABILITIES,
	SPRDWL_GSCAN_SUBCMD_SET_CONFIG,
	SPRDWL_GSCAN_SUBCMD_SET_SCAN_CONFIG,
	SPRDWL_GSCAN_SUBCMD_ENABLE_GSCAN,
	SPRDWL_GSCAN_SUBCMD_GET_SCAN_RESULTS,
	SPRDWL_GSCAN_SUBCMD_SCAN_RESULTS,
	SPRDWL_GSCAN_SUBCMD_SET_HOTLIST,
	SPRDWL_GSCAN_SUBCMD_SET_SIGNIFICANT_CHANGE_CONFIG,
	SPRDWL_GSCAN_SUBCMD_ENABLE_FULL_SCAN_RESULTS,
	SPRDWL_GSCAN_SUBCMD_GET_CHANNEL_LIST,
	SPRDWL_WIFI_SUBCMD_GET_FEATURE_SET,
	SPRDWL_WIFI_SUBCMD_GET_FEATURE_SET_MATRIX,
	SPRDWL_WIFI_SUBCMD_SET_PNO_RANDOM_MAC_OUI,
	SPRDWL_WIFI_SUBCMD_NODFS_SET,
	SPRDWL_WIFI_SUBCMD_SET_COUNTRY_CODE,
	/* Add more sub commands here */
	SPRDWL_GSCAN_SUBCMD_SET_EPNO_SSID,
	SPRDWL_WIFI_SUBCMD_SET_SSID_WHITE_LIST,
	SPRDWL_WIFI_SUBCMD_SET_ROAM_PARAMS,
	SPRDWL_WIFI_SUBCMD_ENABLE_LAZY_ROAM,
	SPRDWL_WIFI_SUBCMD_SET_BSSID_PREF,
	SPRDWL_WIFI_SUBCMD_SET_BSSID_BLACKLIST,
	SPRDWL_GSCAN_SUBCMD_ANQPO_CONFIG,
	SPRDWL_WIFI_SUBCMD_SET_RSSI_MONITOR,
	/* Add more sub commands here */
	SPRDWL_GSCAN_SUBCMD_MAX
};

/* WIFI_CMD_GET_INFO
 * @SPRDWL_STD_11D:  The fw supports regulatory domain.
 * @SPRDWL_STD_11E:  The fw supports WMM/WMM-AC/WMM-PS.
 * @SPRDWL_STD_11K:  The fw supports Radio Resource Measurement.
 * @SPRDWL_STD_11R:  The fw supports FT roaming.
 * @SPRDWL_STD_11U:  The fw supports Interworking Network.
 * @SPRDWL_STD_11V:  The fw supports Wireless Network Management.
 * @SPRDWL_STD_11W:  The fw supports Protected Management Frame.
 *
 * @SPRDWL_CAPA_5G:  The fw supports dual band (2.4G/5G).
 * @SPRDWL_CAPA_MCC:  The fw supports Multi Channel Concurrency.
 * @SPRDWL_CAPA_ACL:  The fw supports ACL.
 * @SPRDWL_CAPA_AP_SME:  The fw integrates AP SME.
 * @SPRDWL_CAPA_PMK_OKC_OFFLOAD:  The fw supports PMK/OKC roaming offload.
 * @SPRDWL_CAPA_11R_ROAM_OFFLOAD:  The fw supports FT roaming offload.
 * @SPRDWL_CAPA_SCHED_SCAN:  The fw supports scheduled scans.
 * @SPRDWL_CAPA_TDLS:  The fw supports TDLS (802.11z) operation.
 * @SPRDWL_CAPA_MC_FILTER:  The fw supports multicast filter operation.
 * @SPRDWL_CAPA_NS_OFFLOAD:  The fw supports ipv6 NS operation.
 * @SPRDWL_CAPA_RA_OFFLOAD:  The fw supports ipv6 RA offload.
 */
struct sprdwl_cmd_fw_info {
	__le32 chip_model;
	__le32 chip_version;
	__le32 fw_version;
#define SPRDWL_STD_11D			BIT(0)
#define SPRDWL_STD_11E			BIT(1)
#define SPRDWL_STD_11K			BIT(2)
#define SPRDWL_STD_11R			BIT(3)
#define SPRDWL_STD_11U			BIT(4)
#define SPRDWL_STD_11V			BIT(5)
#define SPRDWL_STD_11W			BIT(6)
	__le32 fw_std;
#define SPRDWL_CAPA_5G			BIT(0)
#define SPRDWL_CAPA_MCC			BIT(1)
#define SPRDWL_CAPA_ACL			BIT(2)
#define SPRDWL_CAPA_AP_SME		BIT(3)
#define SPRDWL_CAPA_PMK_OKC_OFFLOAD	BIT(4)
#define SPRDWL_CAPA_11R_ROAM_OFFLOAD	BIT(5)
#define SPRDWL_CAPA_SCHED_SCAN		BIT(6)
#define SPRDWL_CAPA_TDLS		BIT(7)
#define SPRDWL_CAPA_MC_FILTER		BIT(8)
#define SPRDWL_CAPA_NS_OFFLOAD		BIT(9)
#define SPRDWL_CAPA_RA_OFFLOAD		BIT(10)
	__le32 fw_capa;
	u8 max_ap_assoc_sta;
	u8 max_acl_mac_addrs;
	u8 max_mc_mac_addrs;
	u8 wnm_ft_support;
} __packed;

/* WIFI_CMD_OPEN */
struct sprdwl_cmd_open {
	u8 mode;
	u8 reserved;
	u8 mac[ETH_ALEN];
} __packed;

/* WIFI_CMD_CLOSE */
struct sprdwl_cmd_close {
	u8 mode;
} __packed;

struct sprdwl_cmd_power_save {
#define SPRDWL_SET_SUSPEND	1
#define SPRDWL_SET_FCC_CHANNEL	2
#define SPRDWL_SET_TX_POWER	3
#define SPRDWL_SET_PS_STATE	4
	u8 sub_type;
	u8 value;
} __packed;

struct sprdwl_cmd_add_key {
	u8 key_index;
	u8 pairwise;
	u8 mac[ETH_ALEN];
	u8 keyseq[8];
	u8 cypher_type;
	u8 key_len;
	u8 value[0];
} __packed;

struct sprdwl_cmd_del_key {
	u8 key_index;
	u8 pairwise;		/* pairwise or group */
	u8 mac[ETH_ALEN];
} __packed;

struct sprdwl_cmd_set_def_key {
	u8 key_index;
} __packed;

/* WIFI_CMD_SET_IE */
struct sprdwl_cmd_set_ie {
#define	SPRDWL_IE_BEACON		0
#define	SPRDWL_IE_PROBE_REQ		1
#define	SPRDWL_IE_PROBE_RESP		2
#define	SPRDWL_IE_ASSOC_REQ		3
#define	SPRDWL_IE_ASSOC_RESP		4
#define	SPRDWL_IE_BEACON_HEAD		5
#define	SPRDWL_IE_BEACON_TAIL		6
	u8 type;
	__le16 len;
	u8 data[0];
} __packed;

/* WIFI_CMD_START_AP */
struct sprdwl_cmd_start_ap {
	__le16 len;
	u8 value[0];
} __packed;

/* WIFI_CMD_DEL_STATION */
struct sprdwl_cmd_del_station {
	u8 mac[ETH_ALEN];
	__le16 reason_code;
} __packed;

/* WIFI_CMD_GET_STATION */
struct sprdwl_cmd_get_station {
	u8 txrate;
	u8 signal;
	u8 noise;
	u8 reserved;
	__le32 txfailed;
} __packed;

/* WIFI_CMD_SET_CHANNEL */
struct sprdwl_cmd_set_channel {
	u8 channel;
} __packed;

/* WIFI_CMD_SCAN */
struct sprdwl_cmd_scan {
	__le32 channels;	/* One bit for one channel */
	__le32 reserved;
	u16 ssid_len;
	u8 ssid[0];
} __packed;

/* WIFI_CMD_SCHED_SCAN */
struct sprdwl_cmd_sched_scan_hd {
	u16 started;
	u16 buf_flags;
} __packed;

struct sprdwl_cmd_sched_scan_ie_hd {
	u16 ie_flag;
	u16 ie_len;
} __packed;

struct sprdwl_cmd_sched_scan_ifrc {
	u32 interval;
	u32 flags;
	s32 rssi_thold;
	u8 chan[16];
} __packed;

struct sprdwl_cmd_connect {
	__le32 wpa_versions;
	u8 bssid[ETH_ALEN];
	u8 channel;
	u8 auth_type;
	u8 pairwise_cipher;
	u8 group_cipher;
	u8 key_mgmt;
	u8 mfp_enable;
	u8 psk_len;
	u8 ssid_len;
	u8 psk[WLAN_MAX_KEY_LEN];
	u8 ssid[IEEE80211_MAX_SSID_LEN];
} __packed;

/* WIFI_CMD_DISCONNECT */
struct sprdwl_cmd_disconnect {
	__le16 reason_code;
} __packed;

/* WIFI_CMD_SET_PARAM */
struct sprdwl_cmd_set_param {
	__le16 rts;
	__le16 frag;
} __packed;

struct sprdwl_cmd_pmkid {
	u8 bssid[ETH_ALEN];
	u8 pmkid[WLAN_PMKID_LEN];
} __packed;

struct sprdwl_cmd_dscp_exception {
	u8 dscp;
	u8 up;
} __packed;

struct sprdwl_cmd_dscp_range {
	u8 low;
	u8 high;
} __packed;

struct sprdwl_cmd_qos_map {
	u8 num_des;
	struct sprdwl_cmd_dscp_exception dscp_exception[21];
	struct sprdwl_cmd_dscp_range up[8];
} __packed;

struct sprdwl_cmd_tx_ts {
	u8 tsid;
	u8 peer[ETH_ALEN];
	u8 user_prio;
	__le16 admitted_time;
} __packed;

/* WIFI_CMD_REMAIN_CHAN */
struct sprdwl_cmd_remain_chan {
	u8 chan;
	u8 chan_type;
	__le32 duraion;
	__le64 cookie;
} __packed;

/* WIFI_CMD_CANCEL_REMAIN_CHAN */
struct sprdwl_cmd_cancel_remain_chan {
	__le64 cookie;		/* cookie */
} __packed;

/* WIFI_CMD_TX_MGMT */
struct sprdwl_cmd_mgmt_tx {
	u8 chan;		/* send channel */
	u8 dont_wait_for_ack;	/*don't wait for ack */
	__le32 wait;		/* wait time */
	__le64 cookie;		/* cookie */
	__le16 len;		/* mac length */
	u8 value[0];		/* mac */
} __packed;

/* WIFI_CMD_REGISTER_FRAME */
struct sprdwl_cmd_register_frame {
	__le16 type;
	u8 reg;
} __packed;

/* WIFI_CMD_SET_CQM */
struct sprdwl_cmd_cqm_rssi {
	__le32 rssih;
	__le32 rssil;
} __packed;

struct sprdwl_cmd_roam_offload_data {
#define	SPRDWL_ROAM_OFFLOAD_SET_FLAG	1
#define	SPRDWL_ROAM_OFFLOAD_SET_FTIE	2
#define	SPRDWL_ROAM_OFFLOAD_SET_PMK	3
	u8 type;
	u8 len;
	u8 value[0];
} __packed;

struct sprdwl_cmd_tdls_mgmt {
	u8 da[ETH_ALEN];
	u8 sa[ETH_ALEN];
	__le16 ether_type;
	u8 payloadtype;
	u8 category;
	u8 action_code;
	union {
		struct {
			u8 dialog_token;
		} __packed setup_req;
		struct {
			__le16 status_code;
			u8 dialog_token;
		} __packed setup_resp;
		struct {
			__le16 status_code;
			u8 dialog_token;
		} __packed setup_cfm;
		struct {
			__le16 reason_code;
		} __packed teardown;
		struct {
			u8 dialog_token;
		} __packed discover_resp;
	} u;
	__le32 len;
	u8 frame[0];
} __packed;

struct sprdwl_cmd_tdls {
	u8 tdls_sub_cmd_mgmt;
	u8 da[ETH_ALEN];
	u8 initiator;
	u8 paylen;
	u8 payload[0];
} __packed;

struct sprdwl_cmd_tdls_channel_switch {
	u8 primary_chan;
	u8 second_chan_offset;
	u8 band;
} __packed;

struct sprdwl_cmd_blacklist {
	u8 sub_type;
	u8 num;
	u8 mac[0];
} __packed;

struct sprdwl_cmd_set_mac_addr {
	u8 sub_type;
	u8 num;
	u8 mac[0];
} __packed;

struct sprdwl_cmd_rsp_state_code {
	__le32 code;
} __packed;

/* 11v cmd struct */
struct sprdwl_cmd_11v {
	u16 cmd;
	u16 len;
	union {
		u32 value;
		u8 buf[0];
	};
} __packed;

enum SPRDWL_EVENT_LIST {
	WIFI_EVENT_MIN = 0x80,
	/* Station/P2P */
	WIFI_EVENT_CONNECT = WIFI_EVENT_MIN,
	WIFI_EVENT_DISCONNECT,
	WIFI_EVENT_SCAN_DONE,
	WIFI_EVENT_MGMT_FRAME,
	WIFI_EVENT_MGMT_TX_STATUS,
	WIFI_EVENT_REMAIN_CHAN_EXPIRED,
	WIFI_EVENT_MIC_FAIL,
	WIFI_EVENT_GSCAN_FRAME = 0X88,

	/* SoftAP */
	WIFI_EVENT_NEW_STATION = 0xA0,

	/* New Feature */
	/* Uplayer Roaming */
	WIFI_EVENT_CQM = 0xB0,
	WIFI_EVENT_MEASUREMENT,
	WIFI_EVENT_TDLS,
	WIFI_EVENT_SDIO_FLOWCON = 0xB3,

	/* DEBUG/OTHER */
	WIFI_EVENT_SDIO_SEQ_NUM = 0xE0,

	WIFI_EVENT_MAX
};

/* WIFI_EVENT_CONNECT */
struct sprdwl_event_connect {
	u8 status_code;
	u8 bssid[ETH_ALEN];
	u8 channel_num;
	s8 signal;
	/* include beacon ie, req ie, resp ie */
	u8 ie[0];
} __packed;

/* WIFI_EVENT_DISCONNECT */
struct sprdwl_event_disconnect {
	u16 reason_code;
} __packed;

/* WIFI_EVENT_MGMT_FRAME */
struct sprdwl_event_mgmt_frame {
#define SPRDWL_FRAME_NORMAL		1
#define	SPRDWL_FRAME_DEAUTH		2
#define	SPRDWL_FRAME_DISASSOC		3
#define	SPRDWL_FRAME_SCAN		4
#define SPRDWL_FRAME_ROAMING		5
	u8 type;
	u8 channel;
	u8 signal;		/* scan frame */
	u8 reserved;
	u8 bssid[ETH_ALEN];	/* roaming frame */
	__le16 len;
	u8 data[0];
} __packed;

/* WIFI_EVENT_SCAN_COMP */
struct sprdwl_event_scan_done {
#define	SPRDWL_SCAN_DONE		1
#define	SPRDWL_SCHED_SCAN_DONE		2
#define SPRDWL_SCAN_ERROR		3
#define SPRDWL_GSCAN_DONE		4
	u8 type;
} __packed;

/* WIFI_EVENT_GSCAN_COMP */
struct sprdwl_event_gscan_done {
	struct sprdwl_event_scan_done evt;
	u8 bucket_id;
} __packed;

/* WIFI_EVENT_MLME_TX_STATUS */
struct sprdwl_event_mgmt_tx_status {
	__le64 cookie;		/* cookie */
	u8 ack;			/* status */
	__le16 len;		/* frame len */
	u8 buf[0];		/* mgmt frame */
} __packed;

/* WIFI_EVENT_NEW_STATION  */
struct sprdwl_event_new_station {
	u8 is_connect;
	u8 mac[ETH_ALEN];
	__le16 ie_len;
	u8 ie[0];
} __packed;

/* WIFI_EVENT_MIC_FAIL */
struct sprdwl_event_mic_failure {
	u8 key_id;
	u8 is_mcast;
} __packed;

/* WIFI_EVENT_CQM  */
struct sprdwl_event_cqm {
#define	SPRDWL_CQM_RSSI_LOW	1
#define	SPRDWL_CQM_RSSI_HIGH	2
#define	SPRDWL_CQM_BEACON_LOSS	3
	u8 status;
} __packed;

struct sprdwl_event_tdls {
	u8 tdls_sub_cmd_mgmt;
	u8 mac[ETH_ALEN];
	u8 payload_len;
	u8 rcpi;
} __packed;

struct sprd_cmd_gscan_header {
	u16 subcmd;
	u16 data_len;
	u8 data[0];
} __packed;

struct sprdwl_llc_hdr {
	u8 dsap;
	u8 ssap;
	u8 cntl;
	u8 org_code[3];
	__be16 eth_type;
} __packed;

void sprdwl_cmd_init(void);
void sprdwl_cmd_wake_upall(void);
void sprdwl_cmd_deinit(void);

struct sprdwl_priv;

int sprdwl_cmd_rsp(struct sprdwl_priv *priv, u8 *msg);

int sprdwl_get_fw_info(struct sprdwl_priv *priv);
int sprdwl_set_regdom(struct sprdwl_priv *priv, u8 *regdom, u32 len);
int sprdwl_set_rts(struct sprdwl_priv *priv, u16 rts_threshold);
int sprdwl_set_frag(struct sprdwl_priv *priv, u16 frag_threshold);
int sprdwl_screen_off(struct sprdwl_priv *priv, bool is_off);
int sprdwl_power_save(struct sprdwl_priv *priv, u8 vif_mode,
		      u8 sub_type, u8 status);
int sprdwl_notify_ip(struct sprdwl_priv *priv, u8 vif_mode,
		     u8 ip_type, u8 *ip_addr);
int sprdwl_add_blacklist(struct sprdwl_priv *priv, u8 vif_mode, u8 *mac_addr);
int sprdwl_del_blacklist(struct sprdwl_priv *priv, u8 vif_mode, u8 *mac_addr);
int sprdwl_set_whitelist(struct sprdwl_priv *priv, u8 vif_mode,
			 u8 sub_type, u8 num, u8 *mac_addr);

int sprdwl_open_fw(struct sprdwl_priv *priv, u8 vif_mode, u8 mode,
		   u8 *mac_addr);
int sprdwl_close_fw(struct sprdwl_priv *priv, u8 vif_mode, u8 mode);
int sprdwl_add_key(struct sprdwl_priv *priv, u8 vif_mode, const u8 *key_data,
		   u8 key_len, u8 pairwise, u8 key_index, const u8 *key_seq,
		   u8 cypher_type, const u8 *mac_addr);
int sprdwl_del_key(struct sprdwl_priv *priv, u8 vif_mode, u16 key_index,
		   const u8 *mac_addr);
int sprdwl_set_key(struct sprdwl_priv *priv, u8 vif_mode, u8 key_index);
int sprdwl_set_p2p_ie(struct sprdwl_priv *priv, u8 vif_mode, u8 type,
		      const u8 *ie, u16 len);
int sprdwl_set_wps_ie(struct sprdwl_priv *priv, u8 vif_mode, u8 type,
		      const u8 *ie, u8 len);
int sprdwl_set_ft_ie(struct sprdwl_priv *priv, u8 vif_mode,
		     const u8 *ie, u16 len);
int sprdwl_start_ap(struct sprdwl_priv *priv, u8 vif_mode, u8 *beacon, u16 len);
int sprdwl_get_rssi(struct sprdwl_priv *priv, u8 vif_mode, u8 *signal,
		    u8 *noise);
int sprdwl_get_txrate_txfailed(struct sprdwl_priv *priv, u8 vif_mode, u32 *rate,
			       u32 *failed);
int sprdwl_set_channel(struct sprdwl_priv *priv, u8 vif_mode, u8 channel);
int sprdwl_scan(struct sprdwl_priv *priv, u8 vif_mode, u32 channels,
		int ssid_len, const u8 *ssid_list);
int sprdwl_sched_scan_start(struct sprdwl_priv *priv, u8 vif_mode,
			    struct sprdwl_sched_scan_buf *buf);
int sprdwl_sched_scan_stop(struct sprdwl_priv *priv, u8 vif_mode);
int sprdwl_disconnect(struct sprdwl_priv *priv, u8 vif_mode, u16 reason_code);
int sprdwl_connect(struct sprdwl_priv *priv, u8 vif_mode,
		   struct sprdwl_cmd_connect *p);
int sprdwl_pmksa(struct sprdwl_priv *priv, u8 vif_mode, const u8 *bssid,
		 const u8 *pmkid, u8 type);
int sprdwl_remain_chan(struct sprdwl_priv *priv, u8 vif_mode,
		       struct ieee80211_channel *channel,
		       enum nl80211_channel_type channel_type,
		       u32 duration, u64 *cookie);
int sprdwl_cancel_remain_chan(struct sprdwl_priv *priv, u8 vif_mode,
			      u64 cookie);
int sprdwl_tx_mgmt(struct sprdwl_priv *priv, u8 vif_mode, u8 channel,
		   u8 dont_wait_for_ack, u32 wait, u64 *cookie,
		   const u8 *mac, size_t mac_len);
int sprdwl_register_frame(struct sprdwl_priv *priv, u8 vif_mode, u16 type,
			  u8 reg);
int sprdwl_tdls_mgmt(struct sprdwl_vif *vif, struct sk_buff *skb);
int sprdwl_tdls_oper(struct sprdwl_priv *priv, u8 vif_mode, const u8 *peer,
		     int oper);
int sprdwl_start_tdls_channel_switch(struct sprdwl_priv *priv, u8 vif_mode,
				     const u8 *peer_mac, u8 primary_chan,
				     u8 second_chan_offset, u8 band);
int sprdwl_cancel_tdls_channel_switch(struct sprdwl_priv *priv, u8 vif_mode,
				      const u8 *peer_mac);
int sprdwl_set_cqm_rssi(struct sprdwl_priv *priv, u8 vif_mode, s32 rssi_thold,
			u32 rssi_hyst);
int sprdwl_set_roam_offload(struct sprdwl_priv *priv, u8 vif_mode,
			    u8 sub_type, const u8 *data, u8 len);
int sprdwl_del_station(struct sprdwl_priv *priv, u8 vif_mode,
		       const u8 *mac_addr, u16 reason_code);
int sprdwl_set_blacklist(struct sprdwl_priv *priv,
			 u8 vif_mode, u8 sub_type, u8 num, u8 *mac_addr);
int sprdwl_set_ie(struct sprdwl_priv *priv, u8 vif_mode, u8 type,
		  const u8 *ie, u16 len);
int sprdwl_set_param(struct sprdwl_priv *priv, u16 rts, u16 frag);
int sprdwl_get_station(struct sprdwl_priv *priv, u8 vif_mode,
		       u8 *signal, u8 *noise, u8 *rate, u32 *failed);
int sprdwl_set_def_key(struct sprdwl_priv *priv, u8 vif_mode, u8 key_index);

int sprdwl_npi_send_recv(struct sprdwl_priv *priv, u8 vif_mode, u8 *s_buf,
			 u16 s_len, u8 *r_buf, u16 *r_len);
int sprdwl_set_qos_map(struct sprdwl_priv *priv, u8 vif_mode, void *qos_map);
int sprdwl_add_tx_ts(struct sprdwl_priv *priv, u8 vif_mode, u8 tsid,
		     const u8 *peer, u8 user_prio, u16 admitted_time);
int sprdwl_del_tx_ts(struct sprdwl_priv *priv, u8 vif_mode, u8 tsid,
		     const u8 *peer);
int sprdwl_set_mc_filter(struct sprdwl_priv *priv,  u8 vif_mode,
			 u8 sub_type, u8 num, u8 *mac_addr);
int sprdwl_set_gscan_config(struct sprdwl_priv *priv, u8 vif_mode, void *data,
			    u16 len, u8 *r_buf, u16 *r_len);
int sprdwl_set_gscan_scan_config(struct sprdwl_priv *priv, u8 vif_mode,
				 void *data, u16 len, u8 *r_buf, u16 *r_len);
int sprdwl_enable_gscan(struct sprdwl_priv *priv, u8 vif_mode, void *data,
			u8 *r_buf, u16 *r_len);
int sprdwl_set_11v_feature_support(struct sprdwl_priv *priv,
				   u8 vif_mode, u16 val);
int sprdwl_set_11v_sleep_mode(struct sprdwl_priv *priv, u8 vif_mode,
			      u8 status, u16 interval);
int sprdwl_xmit_data2mgmt(struct sk_buff *skb, struct net_device *ndev);
struct sprdwl_msg_buf *sprdwl_cmd_getbuf(struct sprdwl_priv *priv,
					 u16 len, enum sprdwl_mode mode,
					 enum sprdwl_head_rsp rsp, u8 cmd_id);
int sprdwl_get_gscan_capabilities(struct sprdwl_priv *priv, u8 vif_mode,
				  u8 *r_buf, u16 *r_len);
int sprdwl_get_gscan_channel_list(struct sprdwl_priv *priv, u8 vif_mode,
				  void *data, u8 *r_buf, u16 *r_len);
int sprdwl_cmd_send_recv(struct sprdwl_priv *priv,
			 struct sprdwl_msg_buf *msg,
			 unsigned int timeout, u8 *rbuf, u16 *rlen);
#endif
