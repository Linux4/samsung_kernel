#ifndef __WLAN_CFG80211_H__
#define __WLAN_CFG80211_H__

#define WIFI_DIRECT_SUPPORT
#define WLAN_11R_SUPPORT

#define IP_TYPE           0x0800
/* auth type */
#define ITM_AUTH_OPEN	0
#define ITM_AUTH_SHARED	1
/*cipher type*/
#define NONE		0
#define WEP40		1
#define WEP104		2
#define TKIP		3
#define CCMP		4
#define AP_TKIP		5
#define AP_CCMP		6
#define WAPI		7
/*AKM suite*/
#define AKM_SUITE_PSK	    (1)
#define AKM_SUITE_8021X	    (2)
#define AKM_SUITE_FT_8021X  (3)
#define AKM_SUITE_FT_PSK    (4)
#define AKM_SUITE_WAPI_PSK (4)
#define AKM_SUITE_WAPI_CERT (12)

#define P2P_IE_ID                   221
#define WLAN_AKM_SUITE_FT_8021X		0x000FAC03
#define WLAN_AKM_SUITE_FT_PSK		0x000FAC04
#define WLAN_11R_MD_IE_ID   0x36
#define WLAN_11R_FT_IE_ID   0x37
#define P2P_IE_OUI_BYTE0 0x50
#define P2P_IE_OUI_BYTE1 0x6F
#define P2P_IE_OUI_BYTE2 0x9A
#define P2P_IE_OUI_TYPE  0x09
/*FIXME: determine the actual values for the macros below*/
#define SCAN_IE_LEN_MAX			2304
#define MAX_NUM_PMKIDS			4
#define MAX_SITES_FOR_SCAN		12
#define WLAN_MAX_SSID_SIZE		32
#define WLAN_MAX_KEY_INDEX		3
#define ITM_SCAN_TIMER_INTERVAL_MS	8000
/* parise or group key type */
#define GROUP				0
#define PAIRWISE			1
#define HOSTAP_CONF_FILE_NAME "/data/misc/wifi/hostapd.conf"
#define ENG_MAC_ADDR_PATH     "/data/misc/wifi/wifimac.txt"

#define MAX_SCAN_FRAME_BUF_NUM       (30)
#define LTE_CONCUR_REQ               (100)

enum wlan_mode {
	ITM_NONE_MODE,
	ITM_STATION_MODE,
	ITM_AP_MODE,
	ITM_NPI_MODE,
	ITM_P2P_CLIENT_MODE,
	ITM_P2P_GO_MODE,
};

enum WPS_TYPE {
	WPS_REQ_IE = 1,
	WPS_ASSOC_IE,
	P2P_ASSOC_IE,
	P2P_BEACON_IE,
	P2P_PROBERESP_IE,
	P2P_ASSOCRESP_IE,
	P2P_BEACON_IE_HEAD,
	P2P_BEACON_IE_TAIL
};

enum wlan_state {
	ITM_UNKOWN = 0,
	ITM_SCANNING,
	ITM_SCAN_ABORTING,
	ITM_DISCONNECTED,
	ITM_CONNECTING,
	ITM_CONNECTED
};

struct hostap_conf {
	char wpa_psk[128];
	unsigned int len;
};

typedef struct android_wifi_priv_cmd {
	char *buf;
	int used_len;
	int total_len;
} android_wifi_priv_cmd;

typedef struct {
	unsigned char live;
	unsigned char keep;
	unsigned short channel;
	signed short signal;
	unsigned short msa_len;
	unsigned char ssid[33];
	unsigned char bssid[6];
	unsigned char msa[1024];
} buf_scan_frame_t;

typedef struct {
	unsigned int size;
} lte_concur_data_t;

typedef struct {
	u32 n_reg_rules;
	char alpha2[2];
	struct ieee80211_reg_rule reg_rules[];
} wlan_ieee80211_regdomain;

extern void cfg80211_report_connect_result(unsigned char vif_id,
					   unsigned char *pData, int len);
extern void cfg80211_report_disconnect_done(unsigned char vif_id,
					    unsigned char *pData, int len);
extern void cfg80211_report_scan_done(unsigned char vif_id,
				      unsigned char *pData, int len,
				      bool aborted);
extern void cfg80211_report_mgmt_deauth(unsigned char vif_id,
					unsigned char *data,
					unsigned short len);
extern void cfg80211_report_mgmt_disassoc(unsigned char vif_id,
					  unsigned char *data,
					  unsigned short len);
extern void cfg80211_report_remain_on_channel_expired(unsigned char vif_id,
						      unsigned char *data,
						      unsigned short len);
extern void cfg80211_report_station(unsigned char vif_id, unsigned char *data,
				    unsigned short len);
extern void cfg80211_report_frame(unsigned char vif_id, unsigned char *data,
				  unsigned short len);
extern void cfg80211_report_scan_frame(unsigned char vif_id,
				       unsigned char *pData, int len);
extern void cfg80211_report_mic_failure(unsigned char vif_id,
					unsigned char *pdata, int len);
extern int lte_concur_proc_open(struct inode *inode, struct file *filp);
extern int lte_concur_proc_release(struct inode *inode, struct file *filp);
extern ssize_t lte_concur_proc_ioctl(struct file *filp, unsigned int cmd,
				     unsigned long arg);
extern void cfg80211_report_cqm_low(unsigned char vif_id, unsigned char *pdata,
				    int len);
extern void cfg80211_report_cqm_high(unsigned char vif_id, unsigned char *pdata,
				     int len);
extern void cfg80211_report_cqm_beacon_loss(unsigned char vif_id,
					    unsigned char *pdata, int len);
extern void cfg80211_report_mlme_tx_status(unsigned char vif_id, unsigned char *pdata,
					int len);
#endif
