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

#ifndef __SPRDWL_H__
#define __SPRDWL_H__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/ieee80211.h>
#include <linux/etherdevice.h>
#include <net/cfg80211.h>
#include <linux/inetdevice.h>
#include <linux/wireless.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <net/if_inet6.h>
#include <net/addrconf.h>
#include <linux/dcache.h>

#include "cfg80211.h"
#include "cmdevt.h"
#include "intf.h"
#include "vendor.h"

#define SPRDWL_DRIVER_VERSION	"2.0"

#define SPRDWL_UNALIAGN		1
#ifdef SPRDWL_UNALIAGN
#define SPRDWL_PUT_LE16(val, addr)	put_unaligned_le16((val), (&addr))
#define SPRDWL_PUT_LE32(val, addr)	put_unaligned_le32((val), (&addr))
#define SPRDWL_GET_LE16(addr)		get_unaligned_le16(&addr)
#define SPRDWL_GET_LE32(addr)		get_unaligned_le32(&addr)
#define SPRDWL_GET_LE64(addr)		get_unaligned_le64(&addr)
#else
#define SPRDWL_PUT_LE16(val, addr)	cpu_to_le16((val), (addr))
#define SPRDWL_PUT_LE32(val, addr)	cpu_to_le32((val), (addr))
#define SPRDWL_GET_LE16(addr)		le16_to_cpu((addr))
#define SPRDWL_GET_LE32(addr)		le32_to_cpu((addr))
#endif

/* the max length between data_head and net data */
#define SPRDWL_SKB_HEAD_RESERV_LEN	16
#define SPRDWL_COUNTRY_CODE_LEN		2

struct sprdwl_mc_filter {
	bool mc_change;
	u8 subtype;
	u8 mac_num;
	u8 mac_addr[0];
};

struct android_wifi_priv_cmd {
	char *buf;
	int used_len;
	int total_len;
};

struct sprdwl_vif {
	struct net_device *ndev;	/* Linux net device */
	struct wireless_dev wdev;	/* Linux wireless device */
	struct sprdwl_priv *priv;

	char name[IFNAMSIZ];
	enum sprdwl_mode mode;
	struct list_head vif_node;	/* node for virtual interface list */
	int ref;

	struct kobject sprdwl_power_obj;
	bool reduce_power;

	/* multicast filter stuff */
	struct sprdwl_mc_filter *mc_filter;

	/* common stuff */
	enum sm_state sm_state;
	unsigned char mac[ETH_ALEN];
	int ssid_len;
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 bssid[ETH_ALEN];
	unsigned char beacon_loss;
	enum nl80211_cqm_rssi_threshold_event cqm;

	/* encryption stuff */
	u8 prwise_crypto;
	u8 grp_crypto;
	u8 key_index[2];
	u8 key[2][4][WLAN_MAX_KEY_LEN];
	u8 key_len[2][4];
	u8 key_txrsc[2][WLAN_MAX_KEY_LEN];
	unsigned long mgmt_reg;

	/* P2P stuff */
	struct ieee80211_channel listen_channel;
	u64 listen_cookie;
};

enum sprdwl_hw_type {
	SPRDWL_HW_SDIO,
	SPRDWL_HW_SIPC,
	SPRDWL_HW_SDIO_BA
};

struct sprdwl_priv {
	struct wiphy *wiphy;
	/* virtual interface list */
	spinlock_t list_lock;
	struct list_head vif_list;

	/* necessary info from fw */
	u32 chip_model;
	u32 chip_ver;
	u32 fw_ver;
	u32 fw_std;
	u32 fw_capa;
	u8 max_ap_assoc_sta;
	u8 max_acl_mac_addrs;
	u8 max_mc_mac_addrs;
	u8 wnm_ft_support;

#define SPRDWL_AP_FLOW_CTR  (1)
	unsigned long flags;
	unsigned short skb_head_len;
	enum sprdwl_hw_type hw_type;
	void *hw_priv;
	struct sprdwl_if_ops *if_ops;

	/* scan */
	spinlock_t scan_lock;
	struct sprdwl_vif *scan_vif;
	struct cfg80211_scan_request *scan_request;
	struct timer_list scan_timer;

	/* schedule scan */
	spinlock_t sched_scan_lock;
	struct sprdwl_vif *sched_scan_vif;
	struct cfg80211_sched_scan_request *sched_scan_request;

	/*gscan*/
	u32 gscan_buckets_num;
	struct sprdwl_gscan_cached_results *gscan_res;
	/* default MAC addr*/
	unsigned char default_mac[ETH_ALEN];
#define SPRDWL_INTF_CLOSE	(0)
#define SPRDWL_INTF_OPEN	(1)
	unsigned char fw_stat[SPRDWL_MODE_MAX];

	/* delayed work */
	spinlock_t work_lock;
	struct work_struct work;
	struct list_head work_list;

	struct dentry *debugfs;
};

struct eap_hdr {
	u8 version;
#define EAP_PACKET_TYPE		(0)
	u8 type;
	u16 len;
#define EAP_FAILURE_CODE	(4)
	u8 code;
	u8 id;
};

extern struct sprdwl_priv *g_sprdwl_priv;

void sprdwl_netif_rx(struct sk_buff *skb, struct net_device *ndev);
void sprdwl_stop_net(struct sprdwl_vif *vif);
void sprdwl_net_flowcontrl(struct sprdwl_priv *priv,
			   enum sprdwl_mode mode, bool state);

struct wireless_dev *sprdwl_add_iface(struct sprdwl_priv *priv,
				      const char *name,
				      enum nl80211_iftype type, u8 *addr);
int sprdwl_del_iface(struct sprdwl_priv *priv, struct sprdwl_vif *vif);
struct sprdwl_priv *sprdwl_core_create(enum sprdwl_hw_type type,
				       struct sprdwl_if_ops *ops);
void sprdwl_core_free(struct sprdwl_priv *priv);
int sprdwl_core_init(struct device *dev, struct sprdwl_priv *priv);
int sprdwl_core_deinit(struct sprdwl_priv *priv);
#endif /* __SPRDWL_H__ */
