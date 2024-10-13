/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Authors	:
 * Baolei Yuan <baolei.yuan@spreadtrum.com>
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

#ifndef __SPRDWL_VENDOR_H__
#define __SPRDWL_VENDOR_H__

#include <net/netlink.h>
#include <net/cfg80211.h>
#include <linux/ctype.h>

#define OUI_SPREAD 6673

enum sprdwl_vendor_subcommand_id {
	SPRDWL_VENDOR_SET_COUNTRY_CODE = 0x100E,
	/*gscan start*/
	GSCAN_GET_CAPABILITIES = 0x1000,
	GSCAN_SET_CONFIG,
	GSCAN_SET_SCAN_CONFIG,
	GSCAN_ENABLE_GSCAN,
	GSCAN_GET_SCAN_RESULTS,
	GSCAN_SCAN_RESULTS,
	GSCAN_SET_HOTLIST,
	GSCAN_SET_SIGNIFICANT_CHANGE_CONFIG,
	GSCAN_ENABLE_FULL_SCAN_RESULTS,
	GSCAN_GET_CHANNEL_LIST,
	WIFI_GET_FEATURE_SET,
	WIFI_GET_FEATURE_SET_MATRIX,
	WIFI_SET_PNO_RANDOM_MAC_OUI,
	WIFI_NODFS_SET,
	WIFI_SET_COUNTRY_CODE,
	/* Add more sub commands here */
	GSCAN_SET_EPNO_SSID,
	WIFI_SET_SSID_WHITE_LIST,
	WIFI_SET_ROAM_PARAMS,
	WIFI_ENABLE_LAZY_ROAM,
	WIFI_SET_BSSID_PREF,
	WIFI_SET_BSSID_BLACKLIST,
	GSCAN_ANQPO_CONFIG,
	WIFI_SET_RSSI_MONITOR,
	/*gscan end*/
	SPRDWL_VENDOR_SET_LLSTAT = 0x1200,
	SPRDWL_VENDOR_GET_LLSTAT,
	SPRDWL_VENDOR_CLR_LLSTAT,
	SPRDWL_VENDOR_SUBCOMMAND_MAX
};

/*link layer stats*/
enum sprdwl_vendor_attribute {
	SPRDWL_VENDOR_ATTR_UNSPEC,
	SPRDWL_VENDOR_ATTR_GET_LLSTAT,
	SPRDWL_VENDOR_ATTR_CLR_LLSTAT,
};

enum wifi_connection_state {
	WIFI_DISCONNECTED = 0,
	WIFI_AUTHENTICATING = 1,
	WIFI_ASSOCIATING = 2,
	WIFI_ASSOCIATED = 3,
	WIFI_EAPOL_STARTED = 4,
	WIFI_EAPOL_COMPLETED = 5,
};

enum wifi_roam_state {
	WIFI_ROAMING_IDLE = 0,
	WIFI_ROAMING_ACTIVE = 1,
};

/* access categories */
enum wifi_traffic_ac {
	WIFI_AC_VO = 0,
	WIFI_AC_VI = 1,
	WIFI_AC_BE = 2,
	WIFI_AC_BK = 3,
	WIFI_AC_MAX = 4,
};

/* configuration params */
struct wifi_link_layer_params {
	u32 mpdu_size_threshold;
	u32 aggressive_statistics_gathering;
} __packed;

struct wifi_clr_llstat_rsp {
	u32 stats_clear_rsp_mask;
	u8 stop_rsp;
};

/* wifi rate */
struct wifi_rate {
	u32 preamble:3;
	u32 nss:2;
	u32 bw:3;
	u32 ratemcsidx:8;
	u32 reserved:16;
	u32 bitrate;
};

struct wifi_rate_stat {
	struct wifi_rate rate;
	u32 tx_mpdu;
	u32 rx_mpdu;
	u32 mpdu_lost;
	u32 retries;
	u32 retries_short;
	u32 retries_long;
};

/* per peer statistics */
struct wifi_peer_info {
	u8 type;
	u8 peer_mac_address[6];
	u32 capabilities;
	u32 num_rate;
	struct wifi_rate_stat rate_stats[];
};

struct wifi_interface_link_layer_info {
	enum sprdwl_mode mode;
	u8 mac_addr[6];
	enum wifi_connection_state state;
	enum wifi_roam_state roaming;
	u32 capabilities;
	u8 ssid[33];
	u8 bssid[6];
	u8 ap_country_str[3];
	u8 country_str[3];
};

/* Per access category statistics */
struct wifi_wmm_ac_stat {
	enum wifi_traffic_ac ac;
	u32 tx_mpdu;
	u32 rx_mpdu;
	u32 tx_mcast;
	u32 rx_mcast;
	u32 rx_ampdu;
	u32 tx_ampdu;
	u32 mpdu_lost;
	u32 retries;
	u32 retries_short;
	u32 retries_long;
	u32 contention_time_min;
	u32 contention_time_max;
	u32 contention_time_avg;
	u32 contention_num_samples;
};

/* interface statistics */
struct wifi_iface_stat {
	void *iface;
	struct wifi_interface_link_layer_info info;
	u32 beacon_rx;
	u64 average_tsf_offset;
	u32 leaky_ap_detected;
	u32 leaky_ap_avg_num_frames_leaked;
	u32 leaky_ap_guard_time;
	u32 mgmt_rx;
	u32 mgmt_action_rx;
	u32 mgmt_action_tx;
	u32 rssi_mgmt;
	u32 rssi_data;
	u32 rssi_ack;
	struct wifi_wmm_ac_stat ac[WIFI_AC_MAX];
	u32 num_peers;
	struct wifi_peer_info peer_info[];
};

/* WiFi Common definitions */
/* channel operating width */
enum wifi_channel_width {
	WIFI_CHAN_WIDTH_20 = 0,
	WIFI_CHAN_WIDTH_40 = 1,
	WIFI_CHAN_WIDTH_80 = 2,
	WIFI_CHAN_WIDTH_160 = 3,
	WIFI_CHAN_WIDTH_80P80 = 4,
	WIFI_CHAN_WIDTH_5 = 5,
	WIFI_CHAN_WIDTH_10 = 6,
	WIFI_CHAN_WIDTH_INVALID = -1
};

/* channel information */
struct wifi_channel_info {
	enum wifi_channel_width width;
	u32 center_freq;
	u32 center_freq0;
	u32 center_freq1;
};

/* channel statistics */
struct wifi_channel_stat {
	struct wifi_channel_info channel;
	u32 on_time;
	u32 cca_busy_time;
};

/* radio statistics */
struct wifi_radio_stat {
	u32 radio;
	u32 on_time;
	u32 tx_time;
	u32 rx_time;
	u32 on_time_scan;
	u32 on_time_nbd;
	u32 on_time_gscan;
	u32 on_time_roam_scan;
	u32 on_time_pno_scan;
	u32 on_time_hs20;
	u32 num_channels;
	struct wifi_channel_stat channels[];
};

struct sprdwl_wmm_ac_stat {
	u8 ac_num;
	u32 tx_mpdu;
	u32 rx_mpdu;
	u32 mpdu_lost;
	u32 retries;
} __packed;

struct sprdwl_llstat_data {
	u32 beacon_rx;
	u8 rssi_mgmt;
	struct sprdwl_wmm_ac_stat ac[WIFI_AC_MAX];
	u32 on_time;
	u32 tx_time;
	u32 rx_time;
	u32 on_time_scan;
} __packed;

struct sprdwl_vendor_data {
	struct wifi_radio_stat radio_st;
	struct wifi_iface_stat iface_st;
};

/*end of link layer stats*/

int sprdwl_vendor_init(struct wiphy *wiphy);
int sprdwl_vendor_deinit(struct wiphy *wiphy);

#define MAX_CHANNELS 16
#define MAX_BUCKETS 4
#define MAX_HOTLIST_APS 16
#define MAX_SIGNIFICANT_CHANGE_APS 16
#define MAX_AP_CACHE_PER_SCAN 32
#define MAX_CHANNLES_NUM 14

enum sprdwl_gscan_attribute {
	GSCAN_ATTRIBUTE_NUM_BUCKETS = 10,
	GSCAN_ATTRIBUTE_BASE_PERIOD,
	GSCAN_ATTRIBUTE_BUCKETS_BAND,
	GSCAN_ATTRIBUTE_BUCKET_ID,
	GSCAN_ATTRIBUTE_BUCKET_PERIOD,
	GSCAN_ATTRIBUTE_BUCKET_NUM_CHANNELS,
	GSCAN_ATTRIBUTE_BUCKET_CHANNELS,
	GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN,
	GSCAN_ATTRIBUTE_REPORT_THRESHOLD,
	GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE,
	GSCAN_ATTRIBUTE_BAND = GSCAN_ATTRIBUTE_BUCKETS_BAND,
	GSCAN_ATTRIBUTE_ENABLE_FEATURE = 20,
	GSCAN_ATTRIBUTE_SCAN_RESULTS_COMPLETE,
	GSCAN_ATTRIBUTE_FLUSH_FEATURE,
	GSCAN_ATTRIBUTE_ENABLE_FULL_SCAN_RESULTS,
	GSCAN_ATTRIBUTE_REPORT_EVENTS,
	/* remaining reserved for additional attributes */
	GSCAN_ATTRIBUTE_NUM_OF_RESULTS = 30,
	GSCAN_ATTRIBUTE_FLUSH_RESULTS,
	GSCAN_ATTRIBUTE_SCAN_RESULTS,
	GSCAN_ATTRIBUTE_SCAN_ID,
	GSCAN_ATTRIBUTE_SCAN_FLAGS,
	GSCAN_ATTRIBUTE_AP_FLAGS,
	GSCAN_ATTRIBUTE_NUM_CHANNELS,
	GSCAN_ATTRIBUTE_CHANNEL_LIST,
	/* remaining reserved for additional attributes */
	GSCAN_ATTRIBUTE_SSID = 40,
	GSCAN_ATTRIBUTE_BSSID,
	GSCAN_ATTRIBUTE_CHANNEL,
	GSCAN_ATTRIBUTE_RSSI,
	GSCAN_ATTRIBUTE_TIMESTAMP,
	GSCAN_ATTRIBUTE_RTT,
	GSCAN_ATTRIBUTE_RTTSD,
	/* remaining reserved for additional attributes */
	GSCAN_ATTRIBUTE_HOTLIST_BSSIDS = 50,
	GSCAN_ATTRIBUTE_RSSI_LOW,
	GSCAN_ATTRIBUTE_RSSI_HIGH,
	GSCAN_ATTRIBUTE_HOTLIST_ELEM,
	GSCAN_ATTRIBUTE_HOTLIST_FLUSH,
	/* remaining reserved for additional attributes */
	GSCAN_ATTRIBUTE_RSSI_SAMPLE_SIZE = 60,
	GSCAN_ATTRIBUTE_LOST_AP_SAMPLE_SIZE,
	GSCAN_ATTRIBUTE_MIN_BREACHING,
	GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_BSSIDS,
	GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_FLUSH,
	/* EPNO */
	GSCAN_ATTRIBUTE_EPNO_SSID_LIST = 70,
	GSCAN_ATTRIBUTE_EPNO_SSID,
	GSCAN_ATTRIBUTE_EPNO_SSID_LEN,
	GSCAN_ATTRIBUTE_EPNO_RSSI,
	GSCAN_ATTRIBUTE_EPNO_FLAGS,
	GSCAN_ATTRIBUTE_EPNO_AUTH,
	GSCAN_ATTRIBUTE_EPNO_SSID_NUM,
	GSCAN_ATTRIBUTE_EPNO_FLUSH,
	/* remaining reserved for additional attributes */
	GSCAN_ATTRIBUTE_WHITELIST_SSID = 80,
	GSCAN_ATTRIBUTE_NUM_WL_SSID,
	GSCAN_ATTRIBUTE_WL_SSID_LEN,
	GSCAN_ATTRIBUTE_WL_SSID_FLUSH,
	GSCAN_ATTRIBUTE_WHITELIST_SSID_ELEM,
	GSCAN_ATTRIBUTE_NUM_BSSID,
	GSCAN_ATTRIBUTE_BSSID_PREF_LIST,
	GSCAN_ATTRIBUTE_BSSID_PREF_FLUSH,
	GSCAN_ATTRIBUTE_BSSID_PREF,
	GSCAN_ATTRIBUTE_RSSI_MODIFIER,
	/* remaining reserved for additional attributes */
	GSCAN_ATTRIBUTE_A_BAND_BOOST_THRESHOLD = 90,
	GSCAN_ATTRIBUTE_A_BAND_PENALTY_THRESHOLD,
	GSCAN_ATTRIBUTE_A_BAND_BOOST_FACTOR,
	GSCAN_ATTRIBUTE_A_BAND_PENALTY_FACTOR,
	GSCAN_ATTRIBUTE_A_BAND_MAX_BOOST,
	GSCAN_ATTRIBUTE_LAZY_ROAM_HYSTERESIS,
	GSCAN_ATTRIBUTE_ALERT_ROAM_RSSI_TRIGGER,
	GSCAN_ATTRIBUTE_LAZY_ROAM_ENABLE,
	/* BSSID blacklist */
	GSCAN_ATTRIBUTE_BSSID_BLACKLIST_FLUSH = 100,
	GSCAN_ATTRIBUTE_BLACKLIST_BSSID,
	/* ANQPO */
	GSCAN_ATTRIBUTE_ANQPO_HS_LIST = 110,
	GSCAN_ATTRIBUTE_ANQPO_HS_LIST_SIZE,
	GSCAN_ATTRIBUTE_ANQPO_HS_NETWORK_ID,
	GSCAN_ATTRIBUTE_ANQPO_HS_NAI_REALM,
	GSCAN_ATTRIBUTE_ANQPO_HS_ROAM_CONSORTIUM_ID,
	GSCAN_ATTRIBUTE_ANQPO_HS_PLMN,
	/* Adaptive scan attributes */
	GSCAN_ATTRIBUTE_BUCKET_STEP_COUNT = 120,
	GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD,
	GSCAN_ATTRIBUTE_MAX
};

enum sprdwl_gscan_wifi_band {
	WIFI_BAND_UNSPECIFIED,
	WIFI_BAND_BG = 1,
	WIFI_BAND_A = 2,
	WIFI_BAND_A_DFS = 4,
	WIFI_BAND_A_WITH_DFS = 6,
	WIFI_BAND_ABG = 3,
	WIFI_BAND_ABG_WITH_DFS = 7,
};

enum sprdwl_gscan_wifi_event {
	SPRD_RESERVED1,
	SPRD_RESERVED2,
	GSCAN_EVENT_SIGNIFICANT_CHANGE_RESULTS,
	GSCAN_EVENT_HOTLIST_RESULTS_FOUND,
	GSCAN_EVENT_SCAN_RESULTS_AVAILABLE,
	GSCAN_EVENT_FULL_SCAN_RESULTS,
	RTT_EVENT_COMPLETE,
	GSCAN_EVENT_COMPLETE_SCAN,
	GSCAN_EVENT_HOTLIST_RESULTS_LOST,
	GSCAN_EVENT_EPNO_EVENT,
	GOOGLE_DEBUG_RING_EVENT,
	GOOGLE_DEBUG_MEM_DUMP_EVENT,
	GSCAN_EVENT_ANQPO_HOTSPOT_MATCH,
	GOOGLE_RSSI_MONITOR_EVENT
};

struct sprdwl_gscan_channel_spec {
	int channel;
	int dwelltimems;
	int passive;
};

#define REPORT_EVENTS_BUFFER_FULL      0
#define REPORT_EVENTS_EACH_SCAN        1
#define REPORT_EVENTS_FULL_RESULTS     2
#define REPORT_EVENTS_NO_BATCH         4

enum sprdwl_gscan_event {
	WIFI_SCAN_BUFFER_FULL,
	WIFI_SCAN_COMPLETE,
};

struct sprdwl_gscan_bucket_spec {
	int bucket;
	enum sprdwl_gscan_wifi_band band;
	int period;
	u8 report_events;
	int max_period;
	int exponent;
	int step_count;
	int num_channels;
	struct sprdwl_gscan_channel_spec channels[MAX_CHANNELS];
};

struct sprdwl_gscan_cmd_params {
	int base_period;
	int max_ap_per_scan;
	int report_threshold_percent;
	int report_threshold_num_scans;
	int num_buckets;
	struct sprdwl_gscan_bucket_spec buckets[MAX_BUCKETS];
};

struct sprdwl_cmd_gscan_set_config {
	int base_period;
	int num_buckets;
	struct sprdwl_gscan_bucket_spec buckets[MAX_BUCKETS];
};

struct sprdwl_cmd_gscan_set_scan_config {
	int max_ap_per_scan;
	int report_threshold_percent;
	int report_threshold_num_scans;
};

struct sprdwl_cmd_gscan_rsp_header {
	u8 subcmd;
	u8 status;
	u16 data_len;
} __packed;

struct sprdwl_cmd_gscan_channel_list {
	int num_channels;
	int channels[MAX_CHANNLES_NUM];
};

struct sprdwl_gscan_capabilities {
	int max_scan_cache_size;
	int max_scan_buckets;
	int max_ap_cache_per_scan;
	int max_rssi_sample_size;
	int max_scan_reporting_threshold;
	int max_hotlist_bssids;
	int max_hotlist_ssids;
	int max_significant_wifi_change_aps;
	int max_bssid_history_entries;
	int max_number_epno_networks;
	int max_number_epno_networks_by_ssid;
	int max_number_of_white_listed_ssid;
};

struct sprdwl_gscan_result {
	u64 ts;
	char ssid[32 + 1];
	char bssid[ETH_ALEN];
	int channel;
	int rssi;
	u64 rtt;
	u64 rtt_sd;
	unsigned short beacon_period;
	unsigned short capability;
	unsigned int ie_length;
	char ie_data[1];
};

struct sprdwl_gscan_cached_results {
	int scan_id;
	u8 flags;
	int num_results;
	struct sprdwl_gscan_result results[MAX_AP_CACHE_PER_SCAN];
};

void sprdwl_report_gscan_result(struct sprdwl_vif *vif,
				u32 report_event, u8 bucketid,
				u16 chan, s16 rssi, const u8 *buf, u16 len);
int sprdwl_gscan_done(struct sprdwl_vif *vif, u8 bucketid);
int sprdwl_buffer_full_event(struct sprdwl_vif *vif);
int sprdwl_available_event(struct sprdwl_vif *vif);
#endif
