/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : sipc.h
 * Abstract : This file is a general definition for SIPC command/event
 *
 * Authors	:
 * Leon Liu <leon.liu@spreadtrum.com>
 * Wenjie.Zhang <Wenjie.Zhang@spreadtrum.com>
 * danny.deng  <danny.deng@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SIPC_H__
#define __SIPC_H__

#include <linux/ieee80211.h>
#include <linux/slab.h>
#include "sprdwl.h"
#include "wifi_nvm_data.h"

/* default command wait response time in msecs */
#define DEFAULT_WAIT_SBUF_TIME		2000

/*MSG TAG*/
#define MSG_TYPE_MASK		0x0000FFFF
#define MSG_LEN_MASK		0xFFFF0000
#define MSG_TYPE_OFFSET		0
#define MSG_LEN_OFFSET		16
/*MSG TYPES*/
#define SPRDWL_MSG_CMD		0x0
#define SPRDWL_MSG_CMD_RSP		0x1
#define SPRDWL_MSG_EVENT		0x2
#define SPRDWL_MSG_MAX		(SPRDWL_MSG_EVENT + 1)
#define SPRDWL_MAX_MSG_LEN		65535
/*help function*/
#define SPRDWL_MSG(type, len)		((type << MSG_TYPE_OFFSET)	\
					| (len << MSG_LEN_OFFSET))
#define SPRDWL_MSG_GET_TYPE(msg)	(((msg) & MSG_TYPE_MASK)	\
					>> MSG_TYPE_OFFSET)
#define SPRDWL_MSG_GET_LEN(msg)	(((msg) & MSG_LEN_MASK)	\
					>> MSG_LEN_OFFSET)
#define SPRDWL_MSG_HDR_SIZE		4

/*CMD TAG*/
#define CMD_TAG_TYPE_MASK		0xF000
#define CMD_TAG_DIR_MASK		0x800
#define CMD_TAG_LEVEL_MASK		0x700
#define CMD_TAG_ID_MASK			0xFF
#define CMD_TAG_TYPE_OFFSET		12
#define CMD_TAG_DIR_OFFSET		11
#define CMD_TAG_LEVEL_OFFSET		8
#define CMD_TAG_ID_OFFSET		0
 /*CMD TYPES */
#define CMD_TYPE_GET			0x0
#define CMD_TYPE_SET			0x1
#define CMD_TYPE_ADD			0x2
#define CMD_TYPE_DEL			0x3
#define CMD_TYPE_FLUSH			0x4
#define CMD_TYPE_MAX			(CMD_TYPE_FLUSH + 1)
/*CMD DIRECTION*/
#define CMD_DIR_AP2CP			0
#define CMD_DIR_CP2AP			1
/*CMD LEVEL*/
#define CMD_LEVEL_DEBUG			0x0
#define CMD_LEVEL_NORMAL		0x1
#define CMD_LEVEL_MAX			(CMD_LEVEL_NORMAL + 1)
/*help function*/
#define SPRDWL_CMD_TAG(type, dir, level, id)((type << CMD_TAG_TYPE_OFFSET)\
					| (dir << CMD_TAG_DIR_OFFSET)	 \
					| (level << CMD_TAG_LEVEL_OFFSET)\
					| (id << CMD_TAG_ID_OFFSET))
#define SPRDWL_CMD_TAG_GET_TYPE(cmd)	(((cmd) & CMD_TAG_TYPE_MASK) \
					>> CMD_TAG_TYPE_OFFSET)
#define SPRDWL_CMD_TAG_GET_DIR(cmd)	(((cmd) & CMD_TAG_DIR_MASK)	 \
					>> CMD_TAG_DIR_OFFSET)
#define SPRDWL_CMD_TAG_GET_LEVEL(cmd)	(((cmd) & CMD_TAG_LEVEL_MASK)\
					>> CMD_TAG_LEVEL_OFFSET)
#define SPRDWL_CMD_TAG_GET_ID(cmd)	(((cmd) & CMD_TAG_ID_MASK)	 \
					>> CMD_TAG_ID_OFFSET)
#define	SPRDWL_CMD_HDR_SIZE			(SPRDWL_MSG_HDR_SIZE + 4)
#define	SPRDWL_CMD_RESP_HDR_SIZE		(SPRDWL_MSG_HDR_SIZE + 8)

/*EVENT TAG*/
#define EVENT_TAG_TYPE_MASK			0xF000
#define EVENT_TAG_ID_MASK			0xFFF
#define EVENT_TAG_TYPE_OFFSET			12
#define EVENT_TAG_ID_OFFSET			0
/*EVENT TYPES*/
#define EVENT_TYPE_NORMAL			0x0
#define EVENT_TYPE_SYSERR			0x1
#define EVENT_TYPE_MAX				(EVENT_TYPE_SYSERR + 1)
/*help function*/
#define SPRDWL_EVENT_TAG(type, id)	((type << EVENT_TAG_TYPE_OFFSET) \
					| (id << EVENT_TAG_ID_OFFSET))
#define SPRDWL_EVENT_TAG_GET_TYPE(event)(((event) & EVENT_TAG_TYPE_MASK) \
					>> EVENT_TAG_TYPE_OFFSET)
#define SPRDWL_EVENT_TAG_GET_ID(event)(((event) & EVENT_TAG_ID_MASK) \
					>> EVENT_TAG_ID_OFFSET)
#define SPRDWL_EVENT_HDR_SIZE			(SPRDWL_MSG_HDR_SIZE + 6)

/*device mode*/
#define DEVICE_MODE_STA				0
#define DEVICE_MODE_ACCESS_POINT		1
#define DEVICE_MODE_P2P_CLIENT			2
#define DEVICE_MODE_P2P_GO			3

/* auth type */
#define SPRDWL_AUTH_OPEN	0
#define SPRDWL_AUTH_SHARED	1

/*cipher type*/
#define SPRDWL_CIPHER_NONE		0
#define SPRDWL_CIPHER_WEP40		1
#define SPRDWL_CIPHER_WEP104		2
#define SPRDWL_CIPHER_TKIP		3
#define SPRDWL_CIPHER_CCMP		4
#define SPRDWL_CIPHER_AP_TKIP		5
#define SPRDWL_CIPHER_AP_CCMP		6
#define SPRDWL_CIPHER_WAPI		7

/*AKM suite*/
#define AKM_SUITE_PSK	1
#define AKM_SUITE_8021X	2
#define AKM_SUITE_WAPI	3

/*PMKID max len*/
#define SPRDWL_PMKID_LEN		16

/* WPS type */
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

#define ETH_ALEN		6

enum wlan_sipc_status {
	WLAN_SIPC_STATUS_OK = 0,
	WLAN_SIPC_STATUS_ERR = -1,
};

/* The reason code is defined by CP2 */
enum wlan_sipc_disconnect_reason {
	AP_LEAVING = 0xc1,
	AP_DEAUTH = 0xc4,
};

/* SPRDWL_WLAN SIPC DATA*/
struct wlan_sipc_data {
	u32 msg_hdr;
	union {
		struct {
			u16 cmd_tag;
			u16 seq_no;
			/* possibly followed by cmd data */
			u8 variable[0];
		} __packed cmd;
		struct {
			u16 cmd_tag;
			u16 seq_no;
			u16 status_code;
			u16 resv;
			/* possibly followed by cmd resp data */
			u8 variable[0];
		} __packed cmd_resp;
		struct {
			u16 event_tag;
			u16 seq_no;
			u16 resv;
			/* possibly followed by event text */
			u8 variable[0];
		} __packed event;
	} u;
} __packed;

/* wlan_sipc add key value struct*/
struct wlan_sipc_add_key {
	u8 mac[6];
	u8 keyseq[8];
	u8 pairwise;
	u8 cypher_type;
	u8 key_index;
	u8 key_len;
	u8 value[0];
} __packed;

/*wlan_sipc del key value struct */
struct wlan_sipc_del_key {
	u8 key_index;
	u8 pairwise;		/* unicase or group */
	u8 mac[6];
} __packed;

/* wlan_sipc pmkid set/del/flush struct */
struct wlan_sipc_pmkid {
	u8 bssid[ETH_ALEN];
	u8 pmkid[SPRDWL_PMKID_LEN];
} __packed;

/* wlan_sipc beacon struct */
struct wlan_sipc_beacon {
	u8 len;
	u8 value[0];
} __packed;

/* wlan_sipc mac open struct */
struct wlan_sipc_mac_open {
	u8 mode;		/* AP or STATION mode */
	u8 mac[6];
	struct wifi_nvm_data nvm_data;
	u8 ap_timestamp[8]; /* AP timestamp |seconds| microseconds| */
} __packed;

/* wlan_sipc wps ie struct */
struct wlan_sipc_wps_ie {
	u8 type;		/* probe req ie or assoc req ie */
	u8 len;			/* max ie len is 255 */
	u8 value[0];
} __packed;

/* combo scan data */
struct wlan_sipc_scan_ssid {
	u8 len;
	u8 ssid[0];
} __packed;

/* wlan_sipc regdom struct */
struct wlan_sipc_regdom {
	u32 len;
	u8 value[0];
} __packed;

/* sblock data */
struct wlan_sblock_recv_data {
	u8 is_encrypted;
	union {
		u8 resv[13];
		struct {
			u8 resv[13];
		} __packed nomal;
		struct {
			u16 header_len;
			/* followed by reseved data */
			u8 resv[11];
		} __packed encrypt;
	} u1;
	union {
		struct {
			u8 eth_header[14];
			/* followed by nomal data */
			u8 variable[0];
		} __packed nomal;
		struct {
			struct ieee80211_hdr_3addr mac_header;
			/* followed by nomal data */
			u8 variable[0];
		} __packed encrypt;
	} u2;
} __packed;

struct wlan_softap_event {
	u8 connected;
	u8 mac[6];
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	u16 req_ie_len;
	u8 ie[0];
#endif	/* CONFIG_SPRDWL_WIFI_DIRECT */
} __packed;

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
struct wlan_sipc_scan_channels {
	u8 channel_num;
	/* set to 4 for p2p social scan */
	u16 channel_freq[4];
	u8 reserved[3];
	/* sync logs between kernel and cp */
	u32 random_count;
} __packed;

/* wlan_sipc management tx struct */
struct wlan_sipc_mgmt_tx {
	u8 chan;		/* send channel */
	u32 wait;		/* wait time */
	u32 len;		/* mac length */
	u8 value[1];		/* mac */
} __packed;

struct wlan_sipc_remain_chan {
	u8 chan;		/* send channel */
	u8 chan_type;
	u32 duraion;
	u64 cookie;		/* cookie */
} __packed;

/* wlan_sipc wps ie struct */
struct wlan_sipc_p2p_ie {
	u8 type;		/*  assoc req ie */
	u8 len;			/* max ie len is 255 */
	u8 value[1];
} __packed;

/* wlan_sipc wps ie struct */
struct wlan_sipc_register_frame {
	u16 type;		/*  assoc req ie */
	u8 reg;			/* max ie len is 255 */
} __packed;

/* wlan_sipc wps ie struct */
struct wlan_sipc_event_report_frame {
	u8 channel;
	u8 frame_type;
	u16 frame_len;
} __packed;

struct wlan_sipc_cancel_remain_chan {
	u64 cookie;		/* cookie */
} __packed;
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */

/*FIXME*/
#define WLAN_CP_ID			SIPC_ID_WCN
#define WLAN_SBUF_ID		7
#define WLAN_SBUF_CH		4
#define WLAN_SBUF_NUM		64
/* WLAN_SBUF_SIZE should < 4k(sprd_spipe_wcn_pdata define 4k) */
#define WLAN_SBUF_SIZE		(ETH_HLEN + ETH_DATA_LEN + NET_IP_ALIGN)
#define WLAN_SBLOCK_CH		SMSG_CH_DATA0
#define WLAN_SBLOCK_NUM		64
#define WLAN_SBLOCK_RX_NUM	128
#define WLAN_SBLOCK_SIZE	1664
#define WLAN_EVENT_SBLOCK_CH	SMSG_CH_DATA1
#define WLAN_EVENT_SBLOCK_NUM	1
#define WLAN_EVENT_SBLOCK_SIZE	(10 * 1024)	/* FIXME */
#define ERR_AP_ZEROCOPY_CP_NOT	0xfd
#define ERR_CP_ZEROCOPY_AP_NOT	0xfe
/*COMMAND IDs*/
enum wlan_sipc_cmd_id {
	WIFI_CMD_DEV_MODE,
	WIFI_CMD_SCAN,
	WIFI_CMD_AUTH_TYPE,
	WIFI_CMD_WPA_VERSION,
	WIFI_CMD_PAIRWISE_CIPHER,
	WIFI_CMD_GROUP_CIPHER,
	WIFI_CMD_AKM_SUITE,
	WIFI_CMD_CHANNEL,
	WIFI_CMD_BSSID,
	WIFI_CMD_ESSID,
	WIFI_CMD_KEY,
	WIFI_CMD_KEY_ID,
	WIFI_CMD_CONNECT,
	WIFI_CMD_DISCONNECT,
	WIFI_CMD_RTS_THRESHOLD,
	WIFI_CMD_FRAG_THRESHOLD,
	WIFI_CMD_RSSI,
	WIFI_CMD_TXRATE,
	WIFI_CMD_PMKSA,
	WIFI_CMD_MAC_ADDR,

	WIFI_CMD_DEV_OPEN,
	WIFI_CMD_DEV_CLOSE,
	WIFI_CMD_MAC_STATUS,
	WIFI_CMD_PM_ENTER_PS,
	WIFI_CMD_PM_EXIT_PS,
	WIFI_CMD_PM_SUSPEND,
	WIFI_CMD_PM_RESUME,
	WIFI_CMD_PSK,
	WIFI_CMD_START_AP,
	WIFI_CMD_NPI,
	WIFI_CMD_WPS_IE,
	WIFI_CMD_LINK_STATUS,
	WIFI_CMD_PM_EARLY_SUSPEND,
	WIFI_CMD_PM_LATER_RESUME,
	WIFI_CMD_BLACKLIST,

	WIFI_CMD_REGDOM,
	/*for Wi-Fi direct*/
	WIFI_CMD_TX_MGMT,
	WIFI_CMD_REMAIN_CHAN,
	WIFI_CMD_CANCEL_REMAIN_CHAN,
	WIFI_CMD_P2P_IE,
	WIFI_CMD_CHANGE_BEACON,
	WIFI_CMD_REGISTER_FRAME,
	WIFI_CMD_SCAN_CHANNELS,

	WIFI_CMD_MAX
};

/* EVENT IDs */
enum wlan_sipc_event_id {
	WIFI_EVENT_CONNECT,
	WIFI_EVENT_DISCONNECT,

	WIFI_EVENT_SCANDONE,

	WIFI_EVENT_TX_BUSY,
	WIFI_EVENT_READY,

	WIFI_EVENT_SOFTAP,
	/*for Wi-Fi direct*/
	WIFI_EVENT_MGMT_DEAUTH,
	WIFI_EVENT_MGMT_DISASSOC,
	WIFI_EVENT_REMAIN_ON_CHAN_EXPIRED,
	WIFI_EVENT_REPORT_FRAME,

	WIFI_EVENT_MAX
};

/* wlan_sipc sipc privite data struct for cmd and event*/
struct wlan_sipc {
	/* reserved for future sipc protection */
	spinlock_t lock;
	/* mutex for protecting atomic command send/recv */
	struct mutex cmd_lock;
	/* mutex for protecting not sleep when command send/recv */
	struct mutex pm_lock;

	struct wlan_sipc_data *send_buf;
	size_t wlan_sipc_send_len;

	struct wlan_sipc_data *recv_buf;
	size_t wlan_sipc_recv_len;

	struct wlan_sipc_data *event_buf;
	size_t wlan_sipc_event_len;

	u16 seq_no;
};

/* static inline helper ops*/
static inline bool is_wlan_sipc_cmd_id_valid(enum wlan_sipc_cmd_id id)
{
	return ((id >= 0) && (id < WIFI_CMD_MAX));
}

/* global definition used for cfg80211 ops*/
extern int wlan_sipc_cmd_send(struct wlan_sipc *wlan_sipc, u16 len,
			      u8 type, u8 id);
extern int wlan_sipc_cmd_receive(struct wlan_sipc *wlan_sipc, u16 len, u8 id);
extern int sprdwl_cmd_send_recv(struct wlan_sipc *wlan_sipc, u8 type, u8 id);
extern int sprdwl_scan_cmd(struct wlan_sipc *wlan_sipc, const u8 *ssid,
			   int len);
extern int sprdwl_set_wpa_version_cmd(struct wlan_sipc *wlan_sipc,
				      u32 wpa_version);
extern int sprdwl_set_auth_type_cmd(struct wlan_sipc *wlan_sipc, u32 type);
extern int sprdwl_set_cipher_cmd(struct wlan_sipc *wlan_sipc,
				 u32 cipher, u8 cmd_id);
extern int sprdwl_set_key_management_cmd(struct wlan_sipc *wlan_sipc,
					 u8 key_mgmt);
extern int sprdwl_get_device_mode_cmd(struct wlan_sipc *wlan_sipc);
extern int sprdwl_pmksa_cmd(struct wlan_sipc *wlan_sipc,
			    const u8 *bssid, const u8 *pmkid, u8 type);
extern int sprdwl_set_psk_cmd(struct wlan_sipc *wlan_sipc,
			      const u8 *key, u32 key_len);
extern int sprdwl_set_channel_cmd(struct wlan_sipc *wlan_sipc, u32 channel);
extern int sprdwl_set_bssid_cmd(struct wlan_sipc *wlan_sipc, const u8 *addr);
extern int sprdwl_set_essid_cmd(struct wlan_sipc *wlan_sipc,
				const u8 *essid, int essid_len);
extern int sprdwl_disconnect_cmd(struct wlan_sipc *wlan_sipc, u16 reason_code);
extern int sprdwl_add_key_cmd(struct wlan_sipc *wlan_sipc, const u8 *key_data,
			      u8 key_len, u8 pairwise, u8 key_index,
			      const u8 *key_seq, u8 cypher_type,
			      const u8 *pmac);
extern int sprdwl_del_key_cmd(struct wlan_sipc *wlan_sipc,
			      u16 key_index, const u8 *mac_addr);
extern int sprdwl_set_key_cmd(struct wlan_sipc *wlan_sipc, u8 key_index);
extern int sprdwl_set_rts_cmd(struct wlan_sipc *wlan_sipc, u16 rts_threshold);
extern int sprdwl_set_frag_cmd(struct wlan_sipc *wlan_sipc, u16 frag_threshold);
extern int sprdwl_get_rssi_cmd(struct wlan_sipc *wlan_sipc,
			       s8 *signal, s8 *noise);
extern int sprdwl_get_txrate_cmd(struct wlan_sipc *wlan_sipc, s32 *rate);
extern int sprdwl_start_ap_cmd(struct wlan_sipc *wlan_sipc,
			       u8 *beacon, u16 len);
extern int sprdwl_set_wps_ie_cmd(struct wlan_sipc *wlan_sipc,
				 u8 type, const u8 *ie, u8 len);
extern int sprdwl_set_blacklist_cmd(struct wlan_sipc *wlan_sipc,
				    u8 *addr, u8 flag);
extern int sprdwl_get_ip_cmd(struct sprdwl_priv *sprdwl_priv, u8 *ip);
extern int sprdwl_pm_enter_ps_cmd(struct sprdwl_priv *sprdwl_priv);
extern int sprdwl_pm_exit_ps_cmd(struct wlan_sipc *wlan_sipc);

extern int sprdwl_pm_early_suspend_cmd(struct wlan_sipc *wlan_sipc);
extern int sprdwl_pm_later_resume_cmd(struct wlan_sipc *wlan_sipc);
extern int sprdwl_set_regdom_cmd(struct wlan_sipc *wlan_sipc, u8 *regdom,
				 u16 len);
extern void sprdwl_get_ap_time(u8 *ts);
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
extern int sprdwl_set_scan_chan_cmd(struct wlan_sipc *wlan_sipc,
				    u8 *channel_info, int len);
extern int sprdwl_set_p2p_ie_cmd(struct wlan_sipc *wlan_sipc, u8 type,
				 const u8 *ie, u8 len);
extern int sprdwl_set_tx_mgmt_cmd(struct wlan_sipc *wlan_sipc,
				  struct ieee80211_channel *channel,
				  unsigned int wait, const u8 *mac,
				  size_t mac_len);
extern int sprdwl_remain_chan_cmd(struct wlan_sipc *wlan_sipc,
				  struct ieee80211_channel *channel,
				  enum nl80211_channel_type channel_type,
				  unsigned int duration, u64 *cookie);
extern int sprdwl_cancel_remain_chan_cmd(struct wlan_sipc *wlan_sipc,
					 u64 cookie);
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */
extern int sprdwl_mac_open_cmd(struct wlan_sipc *wlan_sipc,
			       u8 mode, u8 *mac_addr);
extern int sprdwl_mac_close_cmd(struct wlan_sipc *wlan_sipc, u8 mode);

extern int sprdwl_sipc_alloc(struct sprdwl_priv *sprdwl_priv);
extern void sprdwl_sipc_free(struct sprdwl_priv *sprdwl_priv);

extern void sprdwl_nvm_init(void);
extern struct wifi_nvm_data *get_gwifi_nvm_data(void);
#ifdef CONFIG_OF
extern void sprdwl_sipc_sblock_deinit(int sblock_ch);
extern int sprdwl_sipc_sblock_init(int sblock_ch,
				     void (*handler) (int event, void *data),
				     void *data);
extern void wlan_sipc_sblock_handler(int event, void *data);
#endif
#endif/*__SIPC_H__*/
