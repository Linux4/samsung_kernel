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
#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_SPRDWL_ENHANCED_PM)
#include <linux/earlysuspend.h>
#endif

#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define STR2MAC(a) (unsigned int *)&(a)[0], (unsigned int *)&(a)[1], \
	(unsigned int *)&(a)[2], (unsigned int *)&(a)[3], \
	(unsigned int *)&(a)[4], (unsigned int *)&(a)[5]

#define TX_FLOW_LOW	5
#define TX_FLOW_HIGH	10
#define TX_SBLOCK_NUM   64

#ifdef CONFIG_SPRDWL_FW_ZEROCOPY
#define FW_ZEROCOPY	0x80
#endif

struct sprdwl_priv {
	struct net_device *ndev;	/* Linux net device */
	struct wireless_dev *wdev;	/* Linux wireless device */
	struct napi_struct napi;
	bool pm_status;
#if defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_SPRDWL_ENHANCED_PM)
	struct early_suspend early_suspend;
#endif

	atomic_t stopped;	/* sblock indicator */
	int txrcnt;		/* tx resend count */
	int tx_free;		/* tx flow control */
	struct wlan_sipc *wlan_sipc;	/* hook of sipc command ops */

	int cp2_status;

	/* CFG80211 */
	struct cfg80211_scan_request *scan_request;
	spinlock_t scan_lock;
	struct timer_list scan_timeout;	/* Timer for catch scan event timeout */
	int connect_status;
	int mode;
	int ssid_len;
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 bssid[ETH_ALEN];

	/* Encryption stuff */
	u8 cipher_type;
	u8 key_index[2];
	u8 key[2][4][WLAN_MAX_KEY_LEN];
	u8 key_len[2][4];
	u8 key_txrsc[2][WLAN_MAX_KEY_LEN];
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	struct work_struct work;
	u16 frame_type;
	bool reg;
	int p2p_mode;
	struct ieee80211_channel listen_channel;
	u64 listen_cookie;
#endif	/* CONFIG_SPRDWL_WIFI_DIRECT */
};

#endif/*__SPRDWL_H__*/
