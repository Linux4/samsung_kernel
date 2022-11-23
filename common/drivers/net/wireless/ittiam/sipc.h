/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : sipc.h
 * Abstract : This file is a general definition for sipc command/event type
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

#ifndef __ITM_SIPC_CMD_H__
#define __ITM_SIPC_CMD_H__

#include <linux/slab.h>
#include "sipc_types.h"
#include "ittiam.h"

/* default command wait response time in msecs */
#define DEFAULT_WAIT_SBUF_TIME		2000

/*FIXME*/
#define WLAN_CP_ID			SIPC_ID_WCN
#define WLAN_SBUF_ID		7
#define WLAN_SBUF_CH		4
#define WLAN_SBUF_NUM		64
#define WLAN_SBUF_SIZE		(ETH_HLEN + ETH_DATA_LEN + NET_IP_ALIGN)
#define WLAN_SBLOCK_CH		7 /* FIXME This should be 7 */
#define WLAN_SBLOCK_NUM		64
#define WLAN_SBLOCK_SIZE	(ETH_HLEN + ETH_DATA_LEN + NET_IP_ALIGN + 84)
#define WLAN_EVENT_SBLOCK_CH	8  /* FIXME This should be 7  1 */
#define WLAN_EVENT_SBLOCK_NUM	1
#define WLAN_EVENT_SBLOCK_SIZE	(10 * 1024)	/* FIXME */

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
	WIFI_CMD_BEACON,
	WIFI_CMD_NPI,
	WIFI_CMD_WPS_IE,
	WIFI_CMD_LINK_STATUS,
	WIFI_CMD_PM_EARLY_SUSPEND,
	WIFI_CMD_PM_LATER_RESUME,
	WIFI_CMD_BLACKLIST,
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

static inline struct wlan_sipc_data *itm_wlan_get_new_buf(u32 size)
{
	struct wlan_sipc_data *wlan_sipc_data;

	wlan_sipc_data = kzalloc(size, GFP_KERNEL);
	if (!wlan_sipc_data)
		return NULL;

	return wlan_sipc_data;
}

/* global definition used for cfg80211 ops*/
extern int wlan_sipc_cmd_send(struct wlan_sipc *wlan_sipc, u16 len,
				u8 type, u8 id);
extern int wlan_sipc_cmd_receive(struct wlan_sipc *wlan_sipc,
				u16 len, u8 id);
extern int itm_wlan_cmd_send_recv(struct wlan_sipc *wlan_sipc,
				u8 type, u8 id);
extern int itm_wlan_scan_cmd(struct wlan_sipc *wlan_sipc, const u8 *ssid,
				int len);
extern int itm_wlan_set_wpa_version_cmd(struct wlan_sipc *wlan_sipc,
				u32 wpa_version);
extern int itm_wlan_set_auth_type_cmd(struct wlan_sipc *wlan_sipc,
				u32 type);
extern int itm_wlan_set_cipher_cmd(struct wlan_sipc *wlan_sipc,
				u32 cipher, u8 cmd_id);
extern int itm_wlan_set_key_management_cmd(struct wlan_sipc *wlan_sipc,
				u8 key_mgmt);
extern int itm_wlan_get_device_mode_cmd(struct wlan_sipc *wlan_sipc);
extern int itm_wlan_set_psk_cmd(struct wlan_sipc *wlan_sipc,
				const u8 *key, u32 key_len);
extern int itm_wlan_set_channel_cmd(struct wlan_sipc *wlan_sipc,
				u32 channel);
extern int itm_wlan_set_bssid_cmd(struct wlan_sipc *wlan_sipc,
				const u8 *addr);
extern int itm_wlan_set_essid_cmd(struct wlan_sipc *wlan_sipc,
				const u8 *essid, int essid_len);
extern int itm_wlan_disconnect_cmd(struct wlan_sipc *wlan_sipc,
				u16 reason_code);
extern int itm_wlan_add_key_cmd(struct wlan_sipc *wlan_sipc,
				const u8 *key_data,
				u8 key_len, u8 pairwise, u8 key_index,
				const u8 *key_seq, u8 cypher_type,
				const u8 *pmac);
extern int itm_wlan_set_key_cmd(struct wlan_sipc *wlan_sipc, u8 key_index);
extern int itm_wlan_del_key_cmd(struct wlan_sipc *wlan_sipc,
				u16 key_index, const u8 *mac_addr);
extern int itm_wlan_set_rts_cmd(struct wlan_sipc *wlan_sipc,
				u16 rts_threshold);
extern int itm_wlan_set_frag_cmd(struct wlan_sipc *wlan_sipc,
				u16 frag_threshold);
extern int itm_wlan_get_rssi_cmd(struct wlan_sipc *wlan_sipc,
				s8 *signal, s8 *noise);
extern int itm_wlan_get_txrate_cmd(struct wlan_sipc *wlan_sipc,
				s32 *rate);
extern int itm_wlan_pmksa_cmd(struct wlan_sipc *wlan_sipc,
				const u8 *bssid, const u8 *pmkid, u8 type);
extern int itm_wlan_set_beacon_cmd(struct wlan_sipc *wlan_sipc,
				u8 *beacon, u16 len);
extern int itm_wlan_set_wps_ie_cmd(struct wlan_sipc *wlan_sipc,
				u8 type, const u8 *ie, u8 len);
extern int itm_wlan_set_blacklist_cmd(struct wlan_sipc *wlan_sipc,
				u8 *addr, u8 flag);
extern int itm_wlan_mac_open_cmd(struct wlan_sipc *wlan_sipc,
				u8 mode, u8 *mac_addr);
extern int itm_wlan_mac_close_cmd(struct wlan_sipc *wlan_sipc, u8 mode);

extern int itm_wlan_sipc_alloc(struct itm_priv *itm_priv);
extern void itm_wlan_sipc_free(struct itm_priv *itm_priv);

extern int itm_wlan_get_ip_cmd(struct itm_priv *itm_priv, u8 *ip);
extern int itm_wlan_pm_enter_ps_cmd(struct itm_priv *itm_priv);
extern int itm_wlan_pm_exit_ps_cmd(struct wlan_sipc *wlan_sipc);

extern int itm_wlan_pm_early_suspend_cmd(struct wlan_sipc *wlan_sipc);
extern int itm_wlan_pm_later_resume_cmd(struct wlan_sipc *wlan_sipc);
#endif/*__ITM_SIPC_CMD_H__*/
