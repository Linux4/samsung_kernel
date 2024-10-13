/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : cfg80211.h
 * Abstract : This file is a definition for cfg80211 subsystem
 *
 * Authors	:
 * Leon Liu <leon.liu@spreadtrum.com>
 * Wenjie.Zhang <Wenjie.Zhang@spreadtrum.com>
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

#ifndef __CFG80211_H__
#define __CFG80211_H__

#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
#include <linux/workqueue.h>
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */
#include <net/cfg80211.h>

#include "sprdwl.h"

/*FIXME: determine the actual values for the macros below*/
#define SCAN_IE_LEN_MAX			2304
#define MAX_NUM_PMKIDS			4
#define MAX_SITES_FOR_SCAN		12
#define WLAN_MAX_SSID_SIZE		32
#define WLAN_MAX_KEY_INDEX		3
#define SPRDWL_SCAN_TIMER_INTERVAL_MS	8000

/* parise or group key type */
#define GROUP				0
#define PAIRWISE			1

enum ANDROID_WIFI_CMD {
	ANDROID_WIFI_CMD_START,
	ANDROID_WIFI_CMD_STOP,
	ANDROID_WIFI_CMD_SCAN_ACTIVE,
	ANDROID_WIFI_CMD_SCAN_PASSIVE,
	ANDROID_WIFI_CMD_RSSI,
	ANDROID_WIFI_CMD_LINKSPEED,
	ANDROID_WIFI_CMD_RXFILTER_START,
	ANDROID_WIFI_CMD_RXFILTER_STOP,
	ANDROID_WIFI_CMD_RXFILTER_ADD,
	ANDROID_WIFI_CMD_RXFILTER_REMOVE,
	ANDROID_WIFI_CMD_BTCOEXSCAN_START,
	ANDROID_WIFI_CMD_BTCOEXSCAN_STOP,
	ANDROID_WIFI_CMD_BTCOEXMODE,
	ANDROID_WIFI_CMD_SETSUSPENDOPT,
	ANDROID_WIFI_CMD_P2P_DEV_ADDR,
	ANDROID_WIFI_CMD_SETFWPATH,
	ANDROID_WIFI_CMD_SETBAND,
	ANDROID_WIFI_CMD_GETBAND,
	ANDROID_WIFI_CMD_COUNTRY,
	ANDROID_WIFI_CMD_P2P_SET_NOA,
	ANDROID_WIFI_CMD_P2P_GET_NOA,
	ANDROID_WIFI_CMD_P2P_SET_PS,
	ANDROID_WIFI_CMD_SET_AP_WPS_P2P_IE,
#ifdef PNO_SUPPORT
	ANDROID_WIFI_CMD_PNOSSIDCLR_SET,
	ANDROID_WIFI_CMD_PNOSETUP_SET,
	ANDROID_WIFI_CMD_PNOENABLE_SET,
	ANDROID_WIFI_CMD_PNODEBUG_SET,
#endif
	ANDROID_WIFI_CMD_MACADDR,
	ANDROID_WIFI_CMD_BLOCK,
	ANDROID_WIFI_CMD_WFD_ENABLE,
	ANDROID_WIFI_CMD_WFD_DISABLE,
	ANDROID_WIFI_CMD_WFD_SET_TCPPORT,
	ANDROID_WIFI_CMD_WFD_SET_MAX_TPUT,
	ANDROID_WIFI_CMD_WFD_SET_DEVTYPE,
	ANDROID_WIFI_CMD_MAX
};

enum cp2_state {
	SPRDWL_NOT_READY,
	SPRDWL_READY
};

enum wlan_state {
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

#define HOSTAP_CONF_FILE_NAME "/data/misc/wifi/hostapd.conf"

struct hostap_conf {
	char wpa_psk[128];
	unsigned int len;
};

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
void init_register_frame_param(struct sprdwl_priv *priv);
void sprdwl_event_remain_on_channel_expired(struct sprdwl_priv *priv);
void sprdwl_event_mgmt_deauth(struct sprdwl_priv *priv);
void sprdwl_event_mgmt_disassoc(struct sprdwl_priv *priv);
void sprdwl_event_report_frame(struct sprdwl_priv *priv);
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */

int hostap_conf_load(char *filename, u8 *key_val);

struct android_wifi_priv_cmd {
#ifdef CONFIG_COMPAT
	compat_uptr_t buf;
#else
	char *buf;
#endif
	int used_len;
	int total_len;
};

struct sprdwl_ieee80211_regdomain {
	u32 n_reg_rules;
	char alpha2[2];
	struct ieee80211_reg_rule reg_rules[];
};

void sprdwl_event_scan_results(struct sprdwl_priv *priv, bool aborted);
void sprdwl_event_connect_result(struct sprdwl_priv *priv);
void sprdwl_event_disconnect(struct sprdwl_priv *priv);
void sprdwl_event_ready(struct sprdwl_priv *priv);
void sprdwl_event_tx_busy(struct sprdwl_priv *priv);
void sprdwl_event_softap(struct sprdwl_priv *priv);
int sprdwl_register_wdev(struct sprdwl_priv *priv, struct device *dev);
void sprdwl_unregister_wdev(struct sprdwl_priv *priv);

#endif /* __CFG80211_H__ */
