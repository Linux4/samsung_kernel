#ifndef __ITM_CMD_H__
#define __ITM_CMD_H__

#include <linux/ieee80211.h>
#include "wlan_common.h"
#include "wlan_fifo.h"

#define ITM_PMKID_LEN     16

enum ITM_HOST_TROUT3_CMD_TYPE {
	HOST_SC2331_CMD = 0,
	SC2331_HOST_RSP,
	HOST_SC2331_PKT,
	HOST_SC2331_WAPI,
};

enum ITM_HOST_TROUT3_CMD_LIST {
	WIFI_CMD_GET_MODE = 1,
	WIFI_CMD_GET_RSSI,
	WIFI_CMD_GET_TXRATE_TXFAILED,
	WIFI_CMD_SET_SCAN,
	WIFI_CMD_SET_AUTH_TYPE,
	WIFI_CMD_SET_WPA_VERSION,
	WIFI_CMD_SET_PAIRWISE_CIPHER,
	WIFI_CMD_SET_GROUP_CIPHER,
	WIFI_CMD_SET_AKM_SUITE,
	WIFI_CMD_SET_CHANNEL,	/*10-0xA*/
	WIFI_CMD_SET_BSSID,
	WIFI_CMD_SET_ESSID,
	WIFI_CMD_KEY_ADD,
	WIFI_CMD_KEY_DEL,
	WIFI_CMD_KEY_SET,
	WIFI_CMD_SET_DISCONNECT,
	WIFI_CMD_SET_RTS_THRESHOLD,
	WIFI_CMD_SET_FRAG_THRESHOLD,
	WIFI_CMD_SET_PMKSA,
	WIFI_CMD_DEL_PMKSA,	/*20--0x14*/
	WIFI_CMD_FLUSH_PMKSA,
	WIFI_CMD_SET_DEV_OPEN,
	WIFI_CMD_SET_DEV_CLOSE,
	WIFI_CMD_SET_PSK,
	WIFI_CMD_START_BEACON,
	WIFI_CMD_SET_WPS_IE,
	WIFI_CMD_TX_MGMT,
	WIFI_CMD_REMAIN_CHAN,
	WIFI_CMD_CANCEL_REMAIN_CHAN,
	WIFI_CMD_P2P_IE,	/*30---0x1e*/
	WIFI_CMD_CHANGE_BEACON,
	WIFI_CMD_REGISTER_FRAME,
	WIFI_CMD_NPI_MSG,
	WIFI_CMD_NPI_GET,
	WIFI_CMD_SET_FT_IE,
	WIFI_CMD_UPDATE_FT_IE,
	WIFI_CMD_ASSERT,
	WIFI_CMD_SLEEP,
	WIFI_CMD_ADD_SOFTAP_BLACKLIST,
	WIFI_CMD_DEL_SOFTAP_BLACKLIST,
	WIFI_CMD_SCAN_NOR_CHANNELS,
	WIFI_CMD_GET_IP,
	WIFI_CMD_REQ_LTE_CONCUR,
	WIFI_CMD_SET_CQM_RSSI,
	WIFI_CMD_MULTICAST_FILTER,
	WIFI_CMD_DISASSOC,
	WIFI_CMD_MAX,

	WIFI_EVENT_CONNECT = 128,
	WIFI_EVENT_DISCONNECT,
	WIFI_EVENT_SCANDONE,
	WIFI_EVENT_MGMT_DEAUTH,
	WIFI_EVENT_MGMT_DISASSOC,
	WIFI_EVENT_REMAIN_ON_CHAN_EXPIRED,
	WIFI_EVENT_NEW_STATION,
	WIFI_EVENT_REPORT_FRAME,
	WIFI_EVENT_CONNECT_AP,
	WIFI_EVENT_SDIO_SEQ_NUM,
	WIFI_EVENT_REPORT_SCAN_FRAME,
	WIFI_EVENT_REPORT_MIC_FAIL,
	WIFI_EVENT_REPORT_CQM_RSSI_LOW,
	WIFI_EVENT_REPORT_CQM_RSSI_HIGH,
	WIFI_EVENT_REPORT_CQM_RSSI_LOSS_BEACON,
	WIFI_EVENT_MLME_TX_STATUS,
	WIFI_EVENT_REPORT_VERSION,
	WIFI_EVENT_MAX,
};

/* The reason code is defined by CP2 */
enum wlan_cmd_disconnect_reason {
	AP_LEAVING = 0xc1,
	AP_DEAUTH = 0xc4,
};

#ifdef CONFIG_MACH_SAMSUNG
enum WLNPI_CMD_TYPE {
	HOST_TO_MARLIN_CMD = 1,
	MARLIN_TO_HOST_REPLY,
};

struct npi_cmd_hdr {
	unsigned char  type;
	unsigned char  subtype;
	unsigned short len;
} __attribute__((packed));
#endif

struct wlan_cmd_add_key {
	unsigned char mac[6];
	unsigned char keyseq[8];
	unsigned char pairwise;
	unsigned char cypher_type;
	unsigned char key_index;
	unsigned char key_len;
	unsigned char value[0];
} __attribute__ ((packed));

struct wlan_cmd_del_key {
	unsigned char key_index;
	unsigned char pairwise;	/* unicase or group */
	unsigned char mac[6];
} __attribute__ ((packed));

struct wlan_cmd_pmkid {
	unsigned char bssid[ETH_ALEN];
	unsigned char pmkid[ITM_PMKID_LEN];
} __attribute__ ((packed));

struct wlan_cmd_cqm_rssi {
	s32 rssih;
	u32 rssil;
} __attribute__ ((packed));

struct wlan_cmd_beacon {
	unsigned char len;
	unsigned char value[0];
} __attribute__ ((packed));

struct wlan_cmd_mac_open {
	unsigned short mode;	/* AP or STATION mode */
	unsigned char mac[6];
} __attribute__ ((packed));

struct wlan_cmd_mac_close {
	unsigned char mode;	/* AP or STATION mode */
} __attribute__ ((packed));

struct wlan_cmd_wps_ie {
	unsigned char type;	/* probe req ie or assoc req ie */
	unsigned char len;	/* max ie len is 255 */
	unsigned char value[0];
} __attribute__ ((packed));

struct wlan_cmd_scan_ssid {
	unsigned char len;
	unsigned char ssid[0];
} __attribute__ ((packed));

struct wlan_cmd_hidden_ssid {
	unsigned int ignore_broadcast_ssid;
	unsigned int ssid_len;
	char ssid[32];
} __attribute__ ((packed));

struct wlan_cmd_set_frag {
	unsigned short frag;
} __attribute__ ((packed));

struct wlan_cmd_set_rts {
	unsigned short threshold;
} __attribute__ ((packed));

struct wlan_cmd_set_key {
	unsigned int key_index;
} __attribute__ ((packed));

struct wlan_cmd_disconnect {
	unsigned short reason_code;
} __attribute__ ((packed));

struct wlan_cmd_set_essid {
	unsigned short len;
	unsigned char essid[0];
} __attribute__ ((packed));

struct wlan_cmd_set_bssid {
	unsigned char addr[6];
} __attribute__ ((packed));

struct wlan_cmd_get_ip {
	unsigned char ip[4];
} __attribute__ ((packed));

struct wlan_cmd_set_channel {
	unsigned int channel;
} __attribute__ ((packed));

struct wlan_cmd_set_psk {
	unsigned short len;
	unsigned char key[0];
} __attribute__ ((packed));

struct wlan_cmd_set_key_management {
	unsigned int key_mgmt;

} __attribute__ ((packed));

struct wlan_cmd_set_cipher {
	unsigned int cipher;
} __attribute__ ((packed));

struct wlan_cmd_set_auth_type {
	unsigned int type;

} __attribute__ ((packed));

struct wlan_cmd_set_wpa_version {

	unsigned int wpa_version;
} __attribute__ ((packed));

struct wlan_cmd_assert_t {
	unsigned int reason_code;
	unsigned int tx_cnt;
	unsigned int rx_cnt;
} __attribute__ ((packed));

struct wlan_cmd_scan {
	/*
	   unsigned char channel_num;
	   unsigned char channel[15];
	 */
	unsigned int len;
	unsigned char ssid[0];
} __attribute__ ((packed));

struct wlan_cmd_get_txrate_txfailed {
	unsigned int rate;
	unsigned int failed;
} __attribute__ ((packed));

struct wlan_cmd_get_device_mode {
	unsigned int mode;
} __attribute__ ((packed));

struct wlan_cmd_rsp_state_code {
	int code;
} __attribute__ ((packed));

struct wlan_cmd_remain_chan_t {
	u8 chan;		/* send channel */
	u8 chan_type;
	u32 duraion;
	u64 cookie;		/* cookie */
} __attribute__ ((packed));

struct wlan_cmd_cancel_remain_chan_t {
	u64 cookie;		/* cookie */
} __attribute__ ((packed));

struct wlan_cmd_mgmt_tx_t {
	u8 chan;		/* send channel */
	u8 dont_wait_for_ack;	/*don't wait for ack*/
	u32 wait;		/* wait time */
	u64 cookie;    /* cookie */
	u32 len;		/* mac length */
	u8 value[0];		/* mac */
} __attribute__ ((packed));

/* wlan_sipc wps ie struct */
struct wlan_cmd_p2p_ie_t {
	u8 type;		/*  assoc req ie */
	u16 len;		/* max ie len is 255 */
	u8 value[0];
} __attribute__ ((packed));

struct wlan_cmd_register_frame_t {
	u16 type;		/*  assoc req ie */
	u8 reg;			/* max ie len is 255 */
} __attribute__ ((packed));

struct wlan_cmd_beacon_t {
	u16 len;
	u8 value[0];
} __attribute__ ((packed));

struct wlan_cmd_ft_ies_params {
	unsigned short md;
	unsigned short ie_len;
	unsigned char ie[0];
} __attribute__ ((packed));

struct wlan_set_regdom_params {
	u32 len;
	u8 value[0];
} __attribute__ ((packed));

struct wlan_event_report_frame_t {
	unsigned char channel;
	unsigned char frame_type;
	unsigned short frame_len;
} __attribute__ ((packed));

struct wlan_report_mgmt_tx_status {
	u64 cookie;     /* cookie */
	u8 ack;     /* status */
	u32 len;    /* frame len */
	u8  buf[0];     /* mgmt frame */
}  __attribute__((packed));

typedef struct {
	unsigned char ops;
	unsigned short channel;
	signed short signal;
	unsigned short frame_len;
} wlan_event_scan_rsp_t;

struct wlan_event_mic_failure {
	u8 key_id;
	u8 is_mcast;
} __attribute__ ((packed));

struct wlan_cmd_disassoc {
	unsigned char mac[6];
	unsigned short reason_code;
} __attribute__ ((packed));

extern int wlan_cmd_send_recv(unsigned char vif_id, unsigned char *pData,
			      int len, int type, int timeout);
extern int wlan_cmd_start_ap(unsigned char vif_id, unsigned char *beacon,
			     unsigned short len);
extern int wlan_cmd_register_frame(unsigned char vif_id,
				   struct wlan_cmd_register_frame_t *data);
extern int wlan_cmd_set_p2p_ie(unsigned char vif_id, u8 type, const u8 *ie,
			       u16 len);
extern int wlan_cmd_set_tx_mgmt(unsigned char vif_id,
				struct ieee80211_channel *channel,
				u8 dont_wait_for_ack, unsigned int wait,
				u64 *cookie, const unsigned char *mac,
				size_t mac_len);
extern int wlan_cmd_remain_chan(unsigned char vif_id,
				struct ieee80211_channel *channel,
				enum nl80211_channel_type channel_type,
				unsigned int duration, u64 *cookie);
extern int wlan_cmd_cancel_remain_chan(unsigned char vif_id, u64 cookie);
extern int wlan_cmd_scan(unsigned char vif_id, const unsigned char *ssid,
			 const unsigned char *channels, int len);
extern int wlan_cmd_set_wpa_version(unsigned char vif_id,
				    unsigned int wpa_version);
extern int wlan_cmd_set_auth_type(unsigned char vif_id, unsigned int type);
extern int wlan_cmd_set_cipher(unsigned char vif_id, unsigned int cipher,
			       unsigned char cmd_id);
extern int wlan_cmd_set_key_management(unsigned char vif_id,
				       unsigned char key_mgmt);
extern int wlan_cmd_set_psk(unsigned char vif_id, const unsigned char *key,
			    unsigned int key_len);
extern int wlan_cmd_set_channel(unsigned char vif_id, unsigned int channel);
extern int wlan_cmd_set_bssid(unsigned char vif_id, const unsigned char *addr);
extern int wlan_cmd_get_ip(unsigned char vif_id, u8 *ip);
extern int wlan_cmd_set_essid(unsigned char vif_id, const unsigned char *essid,
			      int essid_len);
extern int wlan_cmd_pmksa(unsigned char vif_id, const unsigned char *bssid,
			  const unsigned char *pmkid, unsigned char type);
extern int wlan_cmd_disconnect(unsigned char vif_id,
			       unsigned short reason_code);
extern int wlan_cmd_add_key(unsigned char vif_id, const unsigned char *key_data,
			    unsigned char key_len, unsigned char pairwise,
			    unsigned char key_index,
			    const unsigned char *key_seq,
			    unsigned char cypher_type,
			    const unsigned char *pmac);
extern int wlan_cmd_del_key(unsigned char vif_id, unsigned short key_index,
			    const unsigned char *mac_addr);
extern int wlan_cmd_set_key(unsigned char vif_id, unsigned char key_index);
extern int wlan_cmd_set_rts(unsigned char vif_id, unsigned short rts_threshold);
extern int wlan_cmd_set_frag(unsigned char vif_id,
			     unsigned short frag_threshold);
extern int wlan_cmd_set_wps_ie(unsigned char vif_id, unsigned char type,
			       const unsigned char *ie, unsigned char len);
extern int wlan_cmd_mac_open(unsigned char vif_id, unsigned char mode,
			     unsigned char *mac_addr);
extern int wlan_cmd_mac_close(unsigned char vif_id, unsigned char mode);
extern int wlan_cmd_get_rssi(unsigned char vif_id, unsigned char *signal,
			     unsigned char *noise);
extern int wlan_cmd_get_txrate_txfailed(unsigned char vif_id,
					unsigned int *rate,
					unsigned int *failed);
extern int wlan_cmd_get_txrate(unsigned char vif_id, unsigned int *rate);
extern int wlan_rx_rsp_process(const unsigned char vif_id, r_msg_hdr_t *msg);
extern int wlan_rx_event_process(const unsigned char vif_id,
				 unsigned char event, unsigned char *pData,
				 unsigned short len);
extern int wlan_cmd_npi_send_recv(unsigned char *s_buf, unsigned short s_len,
				  unsigned char *r_buf, unsigned short *r_len);
extern int wlan_cmd_init(void);
extern int wlan_cmd_deinit(void);
extern int wlan_cmd_set_ft_ie(unsigned char vif_id, const unsigned char *ies,
			      unsigned short len);
extern int wlan_cmd_update_ft_ies(unsigned char vif_id,
				  struct cfg80211_update_ft_ies_params *ft_ies);
extern int wlan_cmd_assert(unsigned char vif_id, unsigned int reason_code);
#ifdef CONFIG_MACH_SAMSUNG
extern int wlan_cmd_set_cap(unsigned int cap);
extern int wlan_cmd_set_psm_cap(void);
#endif
extern void cfg80211_report_scan_frame(unsigned char vif_id,
				       unsigned char *pData, int len);
extern void cfg80211_report_mic_failure(unsigned char vif_id,
					unsigned char *pdata, int len);
extern int wlan_cmd_sleep(int ops);
extern int wlan_cmd_req_lte_concur(unsigned char vif_id,
				   const unsigned char *val, int len);
extern int wlan_cmd_set_regdom(unsigned char vif_id, unsigned char *regdom,
			       unsigned int len);
extern int wlan_cmd_cmq_rssi(unsigned char vif_id, s32 rssi_thold,
			     u32 rssi_hyst, unsigned char type);
extern int wlan_cmd_disassoc(unsigned char vif_id, const unsigned char *mac_addr,
                             unsigned short reason_code);
#endif
