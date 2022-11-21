/*****************************************************************************
 *
 * Copyright (c) 2012 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#ifndef __SLSI_NL80211_VENDOR_H_
#define __SLSI_NL80211_VENDOR_H_

#define OUI_GOOGLE                                      0x001A11
#define OUI_SAMSUNG                                     0x0000f0
#define SLSI_NL80211_GSCAN_SUBCMD_RANGE_START           0x1000
#define SLSI_NL80211_GSCAN_EVENT_RANGE_START            0x01
#define SLSI_GSCAN_SCAN_ID_START                        0x410
#define SLSI_GSCAN_SCAN_ID_END                          0x500

#define SLSI_GSCAN_MAX_BUCKETS                          (8)
#define SLSI_GSCAN_MAX_CHANNELS                         (16) /* As per gscan.h */
#define SLSI_GSCAN_MAX_HOTLIST_APS                      (64)
#define SLSI_GSCAN_MAX_BUCKETS_PER_GSCAN                (SLSI_GSCAN_MAX_BUCKETS)
#define SLSI_GSCAN_MAX_SCAN_CACHE_SIZE                  (12000)
#define SLSI_GSCAN_MAX_AP_CACHE_PER_SCAN                (16)
#define SLSI_GSCAN_MAX_SCAN_REPORTING_THRESHOLD         (100)
#define SLSI_GSCAN_MAX_SIGNIFICANT_CHANGE_APS           (64)
#define SLSI_GSCAN_MAX_EPNO_SSIDS                       (32)
#define SLSI_GSCAN_MAX_EPNO_HS2_PARAM                   (8) /* Framework is not using this. Tune when needed */

#define SLSI_REPORT_EVENTS_NONE                         (0)
#define SLSI_REPORT_EVENTS_EACH_SCAN                    (1)
#define SLSI_REPORT_EVENTS_FULL_RESULTS                 (2)
#define SLSI_REPORT_EVENTS_NO_BATCH                 (4)

#define SLSI_NL_ATTRIBUTE_U32_LEN                       (NLA_HDRLEN + 4)
#define SLSI_NL_ATTRIBUTE_COUNTRY_CODE                  (4)
#define SLSI_NL_VENDOR_ID_OVERHEAD                      SLSI_NL_ATTRIBUTE_U32_LEN
#define SLSI_NL_VENDOR_SUBCMD_OVERHEAD                  SLSI_NL_ATTRIBUTE_U32_LEN
#define SLSI_NL_VENDOR_DATA_OVERHEAD                    (NLA_HDRLEN)

#define SLSI_NL_VENDOR_REPLY_OVERHEAD                   (SLSI_NL_VENDOR_ID_OVERHEAD + \
							 SLSI_NL_VENDOR_SUBCMD_OVERHEAD + \
							 SLSI_NL_VENDOR_DATA_OVERHEAD)

#define SLSI_GSCAN_RTT_UNSPECIFIED                      (-1)
#define SLSI_GSCAN_HASH_TABLE_SIZE                      (32)
#define SLSI_GSCAN_HASH_KEY_MASK                        (0x1F)
#define SLSI_GSCAN_GET_HASH_KEY(_key)                   (_key & SLSI_GSCAN_HASH_KEY_MASK)

#define SLSI_KEEP_SCAN_RESULT                           (0)
#define SLSI_DISCARD_SCAN_RESULT                        (1)

#define SLSI_GSCAN_MAX_BSSID_PER_IE                     (20)

#define SLSI_LLS_CAPABILITY_QOS          0x00000001     /* set for QOS association */
#define SLSI_LLS_CAPABILITY_PROTECTED    0x00000002     /* set for protected association (802.11 beacon frame control protected bit set)*/
#define SLSI_LLS_CAPABILITY_INTERWORKING 0x00000004     /* set if 802.11 Extended Capabilities element interworking bit is set*/
#define SLSI_LLS_CAPABILITY_HS20         0x00000008     /* set for HS20 association*/
#define SLSI_LLS_CAPABILITY_SSID_UTF8    0x00000010     /* set is 802.11 Extended Capabilities element UTF-8 SSID bit is set*/
#define SLSI_LLS_CAPABILITY_COUNTRY      0x00000020     /* set is 802.11 Country Element is present*/

#define TIMESPEC_TO_US(ts)  (((u64)(ts).tv_sec * USEC_PER_SEC) + (ts).tv_nsec / NSEC_PER_USEC)

/* Feature enums */
#define SLSI_WIFI_HAL_FEATURE_INFRA              0x000001      /* Basic infrastructure mode */
#define SLSI_WIFI_HAL_FEATURE_INFRA_5G           0x000002      /* Support for 5 GHz Band */
#define SLSI_WIFI_HAL_FEATURE_HOTSPOT            0x000004      /* Support for GAS/ANQP */
#define SLSI_WIFI_HAL_FEATURE_P2P                0x000008      /* Wifi-Direct */
#define SLSI_WIFI_HAL_FEATURE_SOFT_AP            0x000010      /* Soft AP */
#define SLSI_WIFI_HAL_FEATURE_GSCAN              0x000020      /* Google-Scan APIs */
#define SLSI_WIFI_HAL_FEATURE_NAN                0x000040      /* Neighbor Awareness Networking */
#define SLSI_WIFI_HAL_FEATURE_D2D_RTT            0x000080      /* Device-to-device RTT */
#define SLSI_WIFI_HAL_FEATURE_D2AP_RTT           0x000100      /* Device-to-AP RTT */
#define SLSI_WIFI_HAL_FEATURE_BATCH_SCAN         0x000200      /* Batched Scan (legacy) */
#define SLSI_WIFI_HAL_FEATURE_PNO                0x000400      /* Preferred network offload */
#define SLSI_WIFI_HAL_FEATURE_ADDITIONAL_STA     0x000800      /* Support for two STAs */
#define SLSI_WIFI_HAL_FEATURE_TDLS               0x001000      /* Tunnel directed link setup */
#define SLSI_WIFI_HAL_FEATURE_TDLS_OFFCHANNEL    0x002000      /* Support for TDLS off channel */
#define SLSI_WIFI_HAL_FEATURE_EPR                0x004000      /* Enhanced power reporting */
#define SLSI_WIFI_HAL_FEATURE_AP_STA             0x008000      /* Support for AP STA Concurrency */
#define SLSI_WIFI_HAL_FEATURE_LINK_LAYER_STATS   0x010000      /* Link layer stats collection */
#define SLSI_WIFI_HAL_FEATURE_LOGGER             0x020000      /* WiFi Logger */
#define SLSI_WIFI_HAL_FEATURE_HAL_EPNO           0x040000      /* WiFi PNO enhanced */
#define SLSI_WIFI_HAL_FEATURE_RSSI_MONITOR       0x080000      /* RSSI Monitor */
#define SLSI_WIFI_HAL_FEATURE_MKEEP_ALIVE        0x100000      /* WiFi mkeep_alive */

enum GSCAN_ATTRIBUTE {
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
	GSCAN_ATTRIBUTE_REPORT_THRESHOLD_NUM_SCANS,
	GSCAN_ATTRIBUTE_BAND = GSCAN_ATTRIBUTE_BUCKETS_BAND,

	GSCAN_ATTRIBUTE_ENABLE_FEATURE = 20,
	GSCAN_ATTRIBUTE_SCAN_RESULTS_COMPLETE,              /* indicates no more results */
	GSCAN_ATTRIBUTE_REPORT_EVENTS,

	/* remaining reserved for additional attributes */
	GSCAN_ATTRIBUTE_NUM_OF_RESULTS = 30,
	GSCAN_ATTRIBUTE_SCAN_RESULTS,                       /* flat array of wifi_scan_result */
	GSCAN_ATTRIBUTE_NUM_CHANNELS,
	GSCAN_ATTRIBUTE_CHANNEL_LIST,
	GSCAN_ATTRIBUTE_SCAN_ID,
	GSCAN_ATTRIBUTE_SCAN_FLAGS,
	GSCAN_ATTRIBUTE_SCAN_BUCKET_BIT,

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
	GSCAN_ATTRIBUTE_CHANNEL_NUMBER,

	/* remaining reserved for additional attributes */
	GSCAN_ATTRIBUTE_RSSI_SAMPLE_SIZE = 60,
	GSCAN_ATTRIBUTE_LOST_AP_SAMPLE_SIZE,
	GSCAN_ATTRIBUTE_MIN_BREACHING,
	GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_BSSIDS,

	GSCAN_ATTRIBUTE_BUCKET_STEP_COUNT = 70,
	GSCAN_ATTRIBUTE_BUCKET_EXPONENT,
	GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD,

	GSCAN_ATTRIBUTE_NUM_BSSID,
	GSCAN_ATTRIBUTE_BLACKLIST_BSSID,

	GSCAN_ATTRIBUTE_MAX
};

enum epno_ssid_attribute {
	SLSI_ATTRIBUTE_EPNO_SSID_LIST,
	SLSI_ATTRIBUTE_EPNO_SSID_NUM,
	SLSI_ATTRIBUTE_EPNO_SSID,
	SLSI_ATTRIBUTE_EPNO_SSID_LEN,
	SLSI_ATTRIBUTE_EPNO_RSSI,
	SLSI_ATTRIBUTE_EPNO_FLAGS,
	SLSI_ATTRIBUTE_EPNO_AUTH,
	SLSI_ATTRIBUTE_EPNO_MAX
};

enum epno_hs_attribute {
	SLSI_ATTRIBUTE_EPNO_HS_PARAM_LIST,
	SLSI_ATTRIBUTE_EPNO_HS_NUM,
	SLSI_ATTRIBUTE_EPNO_HS_ID,
	SLSI_ATTRIBUTE_EPNO_HS_REALM,
	SLSI_ATTRIBUTE_EPNO_HS_CONSORTIUM_IDS,
	SLSI_ATTRIBUTE_EPNO_HS_PLMN,
	SLSI_ATTRIBUTE_EPNO_HS_MAX
};

enum gscan_bucket_attributes {
	GSCAN_ATTRIBUTE_CH_BUCKET_1,
	GSCAN_ATTRIBUTE_CH_BUCKET_2,
	GSCAN_ATTRIBUTE_CH_BUCKET_3,
	GSCAN_ATTRIBUTE_CH_BUCKET_4,
	GSCAN_ATTRIBUTE_CH_BUCKET_5,
	GSCAN_ATTRIBUTE_CH_BUCKET_6,
	GSCAN_ATTRIBUTE_CH_BUCKET_7,
	GSCAN_ATTRIBUTE_CH_BUCKET_8
};

enum wifi_band {
	WIFI_BAND_UNSPECIFIED,
	WIFI_BAND_BG = 1,                       /* 2.4 GHz */
	WIFI_BAND_A = 2,                        /* 5 GHz without DFS */
	WIFI_BAND_A_DFS = 4,                    /* 5 GHz DFS only */
	WIFI_BAND_A_WITH_DFS = 6,               /* 5 GHz with DFS */
	WIFI_BAND_ABG = 3,                      /* 2.4 GHz + 5 GHz; no DFS */
	WIFI_BAND_ABG_WITH_DFS = 7,             /* 2.4 GHz + 5 GHz with DFS */
};

enum wifi_scan_event {
	WIFI_SCAN_RESULTS_AVAILABLE,
	WIFI_SCAN_THRESHOLD_NUM_SCANS,
	WIFI_SCAN_THRESHOLD_PERCENT,
	WIFI_SCAN_FAILED,
};

enum wifi_mkeep_alive_attribute {
	MKEEP_ALIVE_ATTRIBUTE_ID,
	MKEEP_ALIVE_ATTRIBUTE_IP_PKT,
	MKEEP_ALIVE_ATTRIBUTE_IP_PKT_LEN,
	MKEEP_ALIVE_ATTRIBUTE_SRC_MAC_ADDR,
	MKEEP_ALIVE_ATTRIBUTE_DST_MAC_ADDR,
	MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC
};

enum wifi_rssi_monitor_attr {
	SLSI_RSSI_MONITOR_ATTRIBUTE_MAX_RSSI,
	SLSI_RSSI_MONITOR_ATTRIBUTE_MIN_RSSI,
	SLSI_RSSI_MONITOR_ATTRIBUTE_START
};

enum lls_attribute {
	LLS_ATTRIBUTE_SET_MPDU_SIZE_THRESHOLD = 1,
	LLS_ATTRIBUTE_SET_AGGR_STATISTICS_GATHERING,
	LLS_ATTRIBUTE_CLEAR_STOP_REQUEST_MASK,
	LLS_ATTRIBUTE_CLEAR_STOP_REQUEST,
	LLS_ATTRIBUTE_MAX
};

enum slsi_hal_vendor_subcmds {
	SLSI_NL80211_VENDOR_SUBCMD_GET_CAPABILITIES = SLSI_NL80211_GSCAN_SUBCMD_RANGE_START,
	SLSI_NL80211_VENDOR_SUBCMD_GET_VALID_CHANNELS,
	SLSI_NL80211_VENDOR_SUBCMD_ADD_GSCAN,
	SLSI_NL80211_VENDOR_SUBCMD_DEL_GSCAN,
	SLSI_NL80211_VENDOR_SUBCMD_GET_SCAN_RESULTS,
	SLSI_NL80211_VENDOR_SUBCMD_SET_BSSID_HOTLIST,
	SLSI_NL80211_VENDOR_SUBCMD_RESET_BSSID_HOTLIST,
	SLSI_NL80211_VENDOR_SUBCMD_GET_HOTLIST_RESULTS,
	SLSI_NL80211_VENDOR_SUBCMD_SET_SIGNIFICANT_CHANGE,
	SLSI_NL80211_VENDOR_SUBCMD_RESET_SIGNIFICANT_CHANGE,
	SLSI_NL80211_VENDOR_SUBCMD_SET_GSCAN_OUI,
	SLSI_NL80211_VENDOR_SUBCMD_SET_NODFS,
	SLSI_NL80211_VENDOR_SUBCMD_START_KEEP_ALIVE_OFFLOAD,
	SLSI_NL80211_VENDOR_SUBCMD_STOP_KEEP_ALIVE_OFFLOAD,
	SLSI_NL80211_VENDOR_SUBCMD_SET_BSSID_BLACKLIST,
	SLSI_NL80211_VENDOR_SUBCMD_SET_EPNO_LIST,
	SLSI_NL80211_VENDOR_SUBCMD_SET_HS_LIST,
	SLSI_NL80211_VENDOR_SUBCMD_RESET_HS_LIST,
	SLSI_NL80211_VENDOR_SUBCMD_SET_RSSI_MONITOR,
	SLSI_NL80211_VENDOR_SUBCMD_LSTATS_SUBCMD_SET_STATS,
	SLSI_NL80211_VENDOR_SUBCMD_LSTATS_SUBCMD_GET_STATS,
	SLSI_NL80211_VENDOR_SUBCMD_LSTATS_SUBCMD_CLEAR_STATS,
	SLSI_NL80211_VENDOR_SUBCMD_GET_FEATURE_SET,
	SLSI_NL80211_VENDOR_SUBCMD_SET_COUNTRY_CODE
};

enum slsi_supp_vendor_subcmds {
	SLSI_NL80211_VENDOR_SUBCMD_UNSPEC = 0,
	SLSI_NL80211_VENDOR_SUBCMD_KEY_MGMT_SET_KEY,
};

enum slsi_vendor_event_values {
	SLSI_NL80211_SIGNIFICANT_CHANGE_EVENT,
	SLSI_NL80211_HOTLIST_AP_FOUND_EVENT,
	SLSI_NL80211_SCAN_RESULTS_AVAILABLE_EVENT,
	SLSI_NL80211_FULL_SCAN_RESULT_EVENT,
	SLSI_NL80211_SCAN_EVENT,
	SLSI_NL80211_HOTLIST_AP_LOST_EVENT,
	SLSI_NL80211_VENDOR_SUBCMD_KEY_MGMT_ROAM_AUTH,
	SLSI_NL80211_VENDOR_HANGED_EVENT,
	SLSI_NL80211_EPNO_EVENT,
	SLSI_NL80211_HOTSPOT_MATCH,
	SLSI_NL80211_RSSI_REPORT_EVENT
};

enum slsi_lls_interface_mode {
	SLSI_LLS_INTERFACE_STA = 0,
	SLSI_LLS_INTERFACE_SOFTAP = 1,
	SLSI_LLS_INTERFACE_IBSS = 2,
	SLSI_LLS_INTERFACE_P2P_CLIENT = 3,
	SLSI_LLS_INTERFACE_P2P_GO = 4,
	SLSI_LLS_INTERFACE_NAN = 5,
	SLSI_LLS_INTERFACE_MESH = 6,
	SLSI_LLS_INTERFACE_UNKNOWN = -1
};

enum slsi_lls_connection_state {
	SLSI_LLS_DISCONNECTED = 0,
	SLSI_LLS_AUTHENTICATING = 1,
	SLSI_LLS_ASSOCIATING = 2,
	SLSI_LLS_ASSOCIATED = 3,
	SLSI_LLS_EAPOL_STARTED = 4,   /* if done by firmware/driver*/
	SLSI_LLS_EAPOL_COMPLETED = 5, /* if done by firmware/driver*/
};

enum slsi_lls_roam_state {
	SLSI_LLS_ROAMING_IDLE = 0,
	SLSI_LLS_ROAMING_ACTIVE = 1,
};

/* access categories */
enum slsi_lls_traffic_ac {
	SLSI_LLS_AC_VO  = 0,
	SLSI_LLS_AC_VI  = 1,
	SLSI_LLS_AC_BE  = 2,
	SLSI_LLS_AC_BK  = 3,
	SLSI_LLS_AC_MAX = 4,
};

/* channel operating width */
enum slsi_lls_channel_width {
	SLSI_LLS_CHAN_WIDTH_20    = 0,
	SLSI_LLS_CHAN_WIDTH_40    = 1,
	SLSI_LLS_CHAN_WIDTH_80    = 2,
	SLSI_LLS_CHAN_WIDTH_160   = 3,
	SLSI_LLS_CHAN_WIDTH_80P80 = 4,
	SLSI_LLS_CHAN_WIDTH_5     = 5,
	SLSI_LLS_CHAN_WIDTH_10    = 6,
	SLSI_LLS_CHAN_WIDTH_INVALID = -1
};

/* wifi peer type */
enum slsi_lls_peer_type {
	SLSI_LLS_PEER_STA,
	SLSI_LLS_PEER_AP,
	SLSI_LLS_PEER_P2P_GO,
	SLSI_LLS_PEER_P2P_CLIENT,
	SLSI_LLS_PEER_NAN,
	SLSI_LLS_PEER_TDLS,
	SLSI_LLS_PEER_INVALID,
};

struct slsi_nl_gscan_capabilities {
	int max_scan_cache_size;
	int max_scan_buckets;
	int max_ap_cache_per_scan;
	int max_rssi_sample_size;
	int max_scan_reporting_threshold;
	int max_hotlist_aps;
	int max_hotlist_ssids;
	int max_significant_wifi_change_aps;
	int max_bssid_history_entries;
	int max_number_epno_networks;
	int max_number_epno_networks_by_ssid;
	int max_number_of_white_listed_ssid;
};

struct slsi_nl_channel_param {
	int channel;
	int dwell_time_ms;
	int passive;         /* 0 => active, 1 => passive scan; ignored for DFS */
};

struct slsi_nl_bucket_param {
	int                          bucket_index;
	enum wifi_band               band;
	int                          period; /* desired period in millisecond */
	u8                           report_events;
	int                          max_period; /* If non-zero: scan period will grow exponentially to a maximum period of max_period */
	int                          exponent;    /* multiplier: new_period = old_period ^ exponent */
	int                          step_count; /* number of scans performed at a given period and until the exponent is applied */
	int                          num_channels;
	struct slsi_nl_channel_param channels[SLSI_GSCAN_MAX_CHANNELS];
};

struct slsi_nl_gscan_param {
	int                         base_period;     /* base timer period in ms */
	int                         max_ap_per_scan; /* number of APs to store in each scan in the BSSID/RSSI history buffer */
	int                         report_threshold_percent; /* when scan_buffer  is this much full, wake up application processor */
	int                         report_threshold_num_scans; /* wake up application processor after these many scans */
	int                         num_buckets;
	struct slsi_nl_bucket_param nl_bucket[SLSI_GSCAN_MAX_BUCKETS];
};

struct slsi_nl_scan_result_param {
	u64 ts;                               /* time since boot (in microsecond) when the result was retrieved */
	u8  ssid[IEEE80211_MAX_SSID_LEN + 1]; /* NULL terminated */
	u8  bssid[6];
	int channel;                          /* channel frequency in MHz */
	int rssi;                             /* in db */
	s64 rtt;                              /* in nanoseconds */
	s64 rtt_sd;                           /* standard deviation in rtt */
	u16 beacon_period;                    /* period advertised in the beacon */
	u16 capability;                       /* capabilities advertised in the beacon */
	u32 ie_length;                        /* size of the ie_data blob */
	u8  ie_data[1];                       /* beacon IE */
};

struct slsi_nl_ap_threshold_param {
	u8  bssid[6];          /* AP BSSID */
	s16 low;               /* low threshold */
	s16 high;              /* high threshold */
};

struct slsi_nl_hotlist_param {
	u8                                lost_ap_sample_size;
	u8                                num_bssid;                          /* number of hotlist APs */
	struct slsi_nl_ap_threshold_param ap[SLSI_GSCAN_MAX_HOTLIST_APS];  /* hotlist APs */
};

struct slsi_bucket {
	bool              used;                /* to identify if this entry is free */
	bool              for_change_tracking; /* Indicates if this scan_id is used for change_tracking */
	u8                report_events;       /* this is received from HAL/Framework */
	u16               scan_id;             /* SLSI_GSCAN_SCAN_ID_START + <offset in the array> */
	int               scan_cycle;          /* To find the current scan cycle */
	struct slsi_gscan *gscan;              /* gscan ref in which this bucket belongs */
};

struct slsi_gscan {
	int                         max_ap_per_scan;  /* received from HAL/Framework */
	int                         report_threshold_percent; /* received from HAL/Framework */
	int                         report_threshold_num_scans; /* received from HAL/Framework */
	int                         num_scans;
	int                         num_buckets;      /* received from HAL/Framework */
	struct slsi_nl_bucket_param nl_bucket;        /* store the first bucket params. used in tracking*/
	struct slsi_bucket          *bucket[SLSI_GSCAN_MAX_BUCKETS_PER_GSCAN];
	struct slsi_gscan           *next;
};

struct slsi_gscan_param {
	struct slsi_nl_bucket_param *nl_bucket;
	struct slsi_bucket          *bucket;
};

struct slsi_nl_significant_change_params {
	int                               rssi_sample_size;    /* number of samples for averaging RSSI */
	int                               lost_ap_sample_size; /* number of samples to confirm AP loss */
	int                               min_breaching;       /* number of APs breaching threshold */
	int                               num_bssid;              /* max 64 */
	struct slsi_nl_ap_threshold_param ap[SLSI_GSCAN_MAX_SIGNIFICANT_CHANGE_APS];
};

struct slsi_gscan_result {
	struct slsi_gscan_result         *hnext;
	int                              scan_cycle;
	int                              scan_res_len;
	int                              anqp_length;
	struct slsi_nl_scan_result_param nl_scan_res;
};

struct slsi_hotlist_result {
	struct list_head                 list;
	int                              scan_res_len;
	struct slsi_nl_scan_result_param nl_scan_res;
};

struct slsi_epno_ssid_param {
	u8  ssid[32];
	u8  ssid_len;
	s16 rssi_thresh;
	u16 flags;
};

struct slsi_epno_hs2_param {
	u32 id;                          /* identifier of this network block, report this in event */
	u8  realm[256];                  /* null terminated UTF8 encoded realm, 0 if unspecified */
	s64 roaming_consortium_ids[16];  /* roaming consortium ids to match, 0s if unspecified */
	u8  plmn[3];                     /* mcc/mnc combination as per rules, 0s if unspecified */
};

struct slsi_rssi_monitor_evt {
	s16 rssi;
	u8  bssid[ETH_ALEN];
};

/* channel information */
struct slsi_lls_channel_info {
	enum slsi_lls_channel_width width;   /* channel width (20, 40, 80, 80+80, 160)*/
	int center_freq;   /* primary 20 MHz channel */
	int center_freq0;  /* center frequency (MHz) first segment */
	int center_freq1;  /* center frequency (MHz) second segment */
};

/* channel statistics */
struct slsi_lls_channel_stat {
	struct slsi_lls_channel_info channel;
	u32 on_time;                /* msecs the radio is awake (32 bits number accruing over time) */
	u32 cca_busy_time;          /* msecs the CCA register is busy (32 bits number accruing over time) */
};

/* wifi rate */
struct slsi_lls_rate {
	u32 preamble   :3;   /* 0: OFDM, 1:CCK, 2:HT 3:VHT 4..7 reserved*/
	u32 nss        :2;   /* 0:1x1, 1:2x2, 3:3x3, 4:4x4*/
	u32 bw         :3;   /* 0:20MHz, 1:40Mhz, 2:80Mhz, 3:160Mhz*/
	u32 rate_mcs_idx :8; /* OFDM/CCK rate code mcs index*/
	u32 reserved  :16;   /* reserved*/
	u32 bitrate;         /* units of 100 Kbps*/
};

/* per rate statistics */
struct slsi_lls_rate_stat {
	struct slsi_lls_rate rate;     /* rate information*/
	u32 tx_mpdu;        /* number of successfully transmitted data pkts (ACK rcvd)*/
	u32 rx_mpdu;        /* number of received data pkts*/
	u32 mpdu_lost;      /* number of data packet losses (no ACK)*/
	u32 retries;        /* total number of data pkt retries*/
	u32 retries_short;  /* number of short data pkt retries*/
	u32 retries_long;   /* number of long data pkt retries*/
};

/* radio statistics */
struct slsi_lls_radio_stat {
	int radio;               /* wifi radio (if multiple radio supported)*/
	u32 on_time;                    /* msecs the radio is awake (32 bits number accruing over time)*/
	u32 tx_time;                    /* msecs the radio is transmitting (32 bits number accruing over time)*/
	u32 rx_time;                    /* msecs the radio is in active receive (32 bits number accruing over time)*/
	u32 on_time_scan;               /* msecs the radio is awake due to all scan (32 bits number accruing over time)*/
	u32 on_time_nbd;                /* msecs the radio is awake due to NAN (32 bits number accruing over time)*/
	u32 on_time_gscan;              /* msecs the radio is awake due to G?scan (32 bits number accruing over time)*/
	u32 on_time_roam_scan;          /* msecs the radio is awake due to roam?scan (32 bits number accruing over time)*/
	u32 on_time_pno_scan;           /* msecs the radio is awake due to PNO scan (32 bits number accruing over time)*/
	u32 on_time_hs20;               /* msecs the radio is awake due to HS2.0 scans and GAS exchange (32 bits number accruing over time)*/
	u32 num_channels;               /* number of channels*/
	struct slsi_lls_channel_stat channels[];   /* channel statistics*/
};

struct slsi_lls_interface_link_layer_info {
	enum slsi_lls_interface_mode mode;     /* interface mode*/
	u8   mac_addr[6];                  /* interface mac address (self)*/
	enum slsi_lls_connection_state state;  /* connection state (valid for STA, CLI only)*/
	enum slsi_lls_roam_state roaming;      /* roaming state*/
	u32 capabilities;                  /* WIFI_CAPABILITY_XXX (self)*/
	u8 ssid[33];                       /* null terminated SSID*/
	u8 bssid[6];                       /* bssid*/
	u8 ap_country_str[3];              /* country string advertised by AP*/
	u8 country_str[3];                 /* country string for this association*/
};

/* per peer statistics */
struct slsi_lls_peer_info {
	enum slsi_lls_peer_type type;         /* peer type (AP, TDLS, GO etc.)*/
	u8 peer_mac_address[6];           /* mac address*/
	u32 capabilities;                 /* peer WIFI_CAPABILITY_XXX*/
	u32 num_rate;                     /* number of rates*/
	struct slsi_lls_rate_stat rate_stats[]; /* per rate statistics, number of entries  = num_rate*/
};

/* Per access category statistics */
struct slsi_lls_wmm_ac_stat {
	enum slsi_lls_traffic_ac ac;      /* access category (VI, VO, BE, BK)*/
	u32 tx_mpdu;                  /* number of successfully transmitted unicast data pkts (ACK rcvd)*/
	u32 rx_mpdu;                  /* number of received unicast data packets*/
	u32 tx_mcast;                 /* number of successfully transmitted multicast data packets*/
	u32 rx_mcast;                 /* number of received multicast data packets*/
	u32 rx_ampdu;                 /* number of received unicast a-mpdus; support of this counter is optional*/
	u32 tx_ampdu;                 /* number of transmitted unicast a-mpdus; support of this counter is optional*/
	u32 mpdu_lost;                /* number of data pkt losses (no ACK)*/
	u32 retries;                  /* total number of data pkt retries*/
	u32 retries_short;            /* number of short data pkt retries*/
	u32 retries_long;             /* number of long data pkt retries*/
	u32 contention_time_min;      /* data pkt min contention time (usecs)*/
	u32 contention_time_max;      /* data pkt max contention time (usecs)*/
	u32 contention_time_avg;      /* data pkt avg contention time (usecs)*/
	u32 contention_num_samples;   /* num of data pkts used for contention statistics*/
};

/* interface statistics */
struct slsi_lls_iface_stat {
	void *iface;                          /* wifi interface*/
	struct slsi_lls_interface_link_layer_info info;  /* current state of the interface*/
	u32 beacon_rx;                        /* access point beacon received count from connected AP*/
	u64 average_tsf_offset;               /* average beacon offset encountered (beacon_TSF - TBTT)*/
	u32 leaky_ap_detected;                /* indicate that this AP typically leaks packets beyond the driver guard time.*/
	u32 leaky_ap_avg_num_frames_leaked;   /* average number of frame leaked by AP after frame with PM bit set was ACK'ed by AP*/
	u32 leaky_ap_guard_time;
	u32 mgmt_rx;                          /* access point mgmt frames received count from connected AP (including Beacon)*/
	u32 mgmt_action_rx;                   /* action frames received count*/
	u32 mgmt_action_tx;                   /* action frames transmit count*/
	int rssi_mgmt;                        /* access Point Beacon and Management frames RSSI (averaged)*/
	int rssi_data;                        /* access Point Data Frames RSSI (averaged) from connected AP*/
	int rssi_ack;                         /* access Point ACK RSSI (averaged) from connected AP*/
	struct slsi_lls_wmm_ac_stat ac[SLSI_LLS_AC_MAX];     /* per ac data packet statistics*/
	u32 num_peers;                        /* number of peers*/
	struct slsi_lls_peer_info peer_info[];           /* per peer statistics*/
};

void slsi_nl80211_vendor_init(struct slsi_dev *sdev);
void slsi_nl80211_vendor_deinit(struct slsi_dev *sdev);
u8 slsi_gscan_get_scan_policy(enum wifi_band band);
void slsi_gscan_handle_scan_result(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb, u16 scan_id, bool scan_done);
int slsi_mlme_set_bssid_hotlist_req(struct slsi_dev *sdev, struct net_device *dev, struct slsi_nl_hotlist_param *nl_hotlist_param);
void slsi_hotlist_ap_lost_indication(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
void slsi_gscan_hash_remove(struct slsi_dev *sdev, u8 *mac);
void slsi_rx_significant_change_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
int slsi_gscan_alloc_buckets(struct slsi_dev *sdev, struct slsi_gscan *gscan, int num_buckets);
int slsi_vendor_event(struct slsi_dev *sdev, int event_id, const void *data, int len);
int slsi_mib_get_gscan_cap(struct slsi_dev *sdev, struct slsi_nl_gscan_capabilities *cap);
void slsi_rx_rssi_report_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);

static inline bool slsi_is_gscan_id(u16 scan_id)
{
	if ((scan_id >= SLSI_GSCAN_SCAN_ID_START) && (scan_id <= SLSI_GSCAN_SCAN_ID_END))
		return true;

	return false;
}

static inline enum slsi_lls_traffic_ac slsi_fapi_to_android_traffic_q(enum slsi_traffic_q fapi_q)
{
	switch (fapi_q) {
	case SLSI_TRAFFIC_Q_BE:
		return SLSI_LLS_AC_BE;
	case SLSI_TRAFFIC_Q_BK:
		return SLSI_LLS_AC_BK;
	case SLSI_TRAFFIC_Q_VI:
		return SLSI_LLS_AC_VI;
	case SLSI_TRAFFIC_Q_VO:
		return SLSI_LLS_AC_VO;
	default:
		return SLSI_LLS_AC_MAX;
	}
}
#endif
