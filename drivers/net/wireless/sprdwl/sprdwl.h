/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Authors:
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
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

#ifndef __SPRDWL_H__
#define __SPRDWL_H__

#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#ifdef CONFIG_SPRDWL_ENHANCED_PM
#include <linux/earlysuspend.h>
#endif

#define TX_FLOW_LOW	5
#define TX_FLOW_HIGH	10
#define TX_SBLOCK_NUM   64

#ifdef CONFIG_SPRDWL_FW_ZEROCOPY
#define FW_ZEROCOPY	0x80
#endif

enum cp2_status {
	SPRDWL_NOT_READY,
	SPRDWL_READY
};

enum sm_state {
	SPRDWL_UNKOWN = 0,
	SPRDWL_SCANNING,
	SPRDWL_SCAN_ABORTING,
	SPRDWL_DISCONNECTED,
	SPRDWL_CONNECTING,
	SPRDWL_CONNECTED
};

enum wlan_mode {
	SPRDWL_NONE_MODE,
	SPRDWL_STATION_MODE,
	SPRDWL_AP_MODE,
	SPRDWL_NPI_MODE,
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	SPRDWL_P2P_CLIENT_MODE,
	SPRDWL_P2P_GO_MODE,
#endif				/* CONFIG_SPRDWL_WIFI_DIRECT */
};

/* parise or group key type */
#define GROUP				0
#define PAIRWISE			1

struct sprdwl_vif {
	struct list_head list;
	struct net_device *ndev;	/* Linux net device */
	struct sprdwl_priv *priv;
	struct wireless_dev wdev;	/* Linux wireless device */
	struct napi_struct napi;
#ifdef CONFIG_SPRDWL_ENHANCED_PM
	struct early_suspend early_suspend;
#endif
	/* cfg80211 stuff */
	enum sm_state sm_state;
	spinlock_t scan_lock;
	struct cfg80211_scan_request *scan_req;
	struct timer_list scan_timer;	/* Timer for catch scan event timeout */

	u8 prwise_crypto;
	u8 grp_crypto;
	int ssid_len;
	u8 ssid[IEEE80211_MAX_SSID_LEN + 1];
	u8 bssid[ETH_ALEN];
	/* encryption stuff */
	u8 key_index[2];
	u8 key[2][4][WLAN_MAX_KEY_LEN];
	u8 key_len[2][4];
	u8 key_txrsc[2][WLAN_MAX_KEY_LEN];
	/* P2P stuff */
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	bool reg;
	u16 frame_type;
	struct work_struct work;
	struct ieee80211_channel listen_channel;
	u64 listen_cookie;
#endif	/* CONFIG_SPRDWL_WIFI_DIRECT */
};

struct sprdwl_priv {
	struct wiphy *wiphy;
	struct list_head vif_list;
	spinlock_t list_lock;
	struct sprdwl_vif *cur_vif;
	struct sprdwl_vif *wlan_vif;
	struct sprdwl_vif *p2p_vif;
	struct sprdwl_vif *scan_vif;

	atomic_t stopped;	/* sblock indicator */
	int txrcnt;		/* tx resend count */
	atomic_t tx_free;	/* tx flow control */

	struct wlan_sipc *sipc;	/* hook of sipc command ops */
	enum cp2_status cp2_status;
	enum wlan_mode mode;

	/*FIXME concurrency issue*/
	/*one netdev is allowed at the same time*/
	struct mutex global_lock;
};

extern struct net_device *sprdwl_register_netdev(struct sprdwl_priv *priv,
						 const char *name,
						 enum nl80211_iftype type,
						 u8 *addr);
extern void sprdwl_unregister_netdev(struct net_device *ndev);
extern struct sprdwl_vif *sprdwl_get_report_vif(struct sprdwl_priv *priv);
#endif/*__SPRDWL_H__*/
