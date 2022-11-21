/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * Filename : cfg80211.c
 * Abstract : This file is a implementation for cfg80211 subsystem
 *
 * Authors:
 * Leon Liu <leon.liu@spreadtrum.com>
 * Wenjie.Zhang <Wenjie.Zhang@spreadtrum.com>
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
 * Jingxiang Li <Jingxiang.Li@spreadtrum.com>
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

#include <linux/spinlock.h>
#include <linux/ieee80211.h>
#include <linux/rtnetlink.h>
#include <net/cfg80211.h>

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/path.h>
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */

#include "sipc.h"
#include "sprdwl.h"
#include "cfg80211.h"

/*----------CFG80211 macros and variables----------*/
#define RATETAB_ENT(_rate, _rateid, _flags)				\
{									\
	.bitrate	= (_rate),					\
	.hw_value	= (_rateid),					\
	.flags		= (_flags),					\
}

#define CHAN2G(_channel, _freq, _flags) {				\
	.band			= IEEE80211_BAND_2GHZ,			\
	.center_freq		= (_freq),				\
	.hw_value		= (_channel),				\
	.flags			= (_flags),				\
	.max_antenna_gain	= 0,					\
	.max_power		= 30,					\
}

#define CHAN5G(_channel, _flags) {					\
	.band			= IEEE80211_BAND_5GHZ,			\
	.center_freq		= 5000 + (5 * (_channel)),		\
	.hw_value		= (_channel),				\
	.flags			= (_flags),				\
	.max_antenna_gain	= 0,					\
	.max_power		= 30,					\
}

static struct ieee80211_rate sprdwl_rates[] = {
	RATETAB_ENT(10, 0x1, 0),
	RATETAB_ENT(20, 0x2, 0),
	RATETAB_ENT(55, 0x5, 0),
	RATETAB_ENT(110, 0xb, 0),
	RATETAB_ENT(60, 0x6, 0),
	RATETAB_ENT(90, 0x9, 0),
	RATETAB_ENT(120, 0xc, 0),
	RATETAB_ENT(180, 0x12, 0),
	RATETAB_ENT(240, 0x18, 0),
	RATETAB_ENT(360, 0x24, 0),
	RATETAB_ENT(480, 0x30, 0),
	RATETAB_ENT(540, 0x36, 0),

	RATETAB_ENT(65, 0x80, 0),
	RATETAB_ENT(130, 0x81, 0),
	RATETAB_ENT(195, 0x82, 0),
	RATETAB_ENT(260, 0x83, 0),
	RATETAB_ENT(390, 0x84, 0),
	RATETAB_ENT(520, 0x85, 0),
	RATETAB_ENT(585, 0x86, 0),
	RATETAB_ENT(650, 0x87, 0),
	RATETAB_ENT(130, 0x88, 0),
	RATETAB_ENT(260, 0x89, 0),
	RATETAB_ENT(390, 0x8a, 0),
	RATETAB_ENT(520, 0x8b, 0),
	RATETAB_ENT(780, 0x8c, 0),
	RATETAB_ENT(1040, 0x8d, 0),
	RATETAB_ENT(1170, 0x8e, 0),
	RATETAB_ENT(1300, 0x8f, 0),
};

#define SPRDWL_G_RATE_NUM	28
#define sprdwl_g_rates		(sprdwl_rates)
#define SPRDWL_A_RATE_NUM	24
#define sprdwl_a_rates		(sprdwl_rates + 4)

#define sprdwl_g_htcap (IEEE80211_HT_CAP_SUP_WIDTH_20_40 | \
			IEEE80211_HT_CAP_SGI_20		 | \
			IEEE80211_HT_CAP_SGI_40)

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
#define P2P_IE_ID 221
#define P2P_IE_OUI_BYTE0 0x50
#define P2P_IE_OUI_BYTE1 0x6F
#define P2P_IE_OUI_BYTE2 0x9A
#define P2P_IE_OUI_TYPE 0x09
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */

static struct ieee80211_channel sprdwl_2ghz_channels[] = {
	CHAN2G(1, 2412, 0),
	CHAN2G(2, 2417, 0),
	CHAN2G(3, 2422, 0),
	CHAN2G(4, 2427, 0),
	CHAN2G(5, 2432, 0),
	CHAN2G(6, 2437, 0),
	CHAN2G(7, 2442, 0),
	CHAN2G(8, 2447, 0),
	CHAN2G(9, 2452, 0),
	CHAN2G(10, 2457, 0),
	CHAN2G(11, 2462, 0),
	CHAN2G(12, 2467, 0),
	CHAN2G(13, 2472, 0),
	CHAN2G(14, 2484, 0),
};

/*static struct ieee80211_channel sprdwl_5ghz_channels[] = {
	CHAN5G(34, 0), CHAN5G(36, 0),
	CHAN5G(38, 0), CHAN5G(40, 0),
	CHAN5G(42, 0), CHAN5G(44, 0),
	CHAN5G(46, 0), CHAN5G(48, 0),
	CHAN5G(52, 0), CHAN5G(56, 0),
	CHAN5G(60, 0), CHAN5G(64, 0),
	CHAN5G(100, 0), CHAN5G(104, 0),
	CHAN5G(108, 0), CHAN5G(112, 0),
	CHAN5G(116, 0), CHAN5G(120, 0),
	CHAN5G(124, 0), CHAN5G(128, 0),
	CHAN5G(132, 0), CHAN5G(136, 0),
	CHAN5G(140, 0), CHAN5G(149, 0),
	CHAN5G(153, 0), CHAN5G(157, 0),
	CHAN5G(161, 0), CHAN5G(165, 0),
	CHAN5G(184, 0), CHAN5G(188, 0),
	CHAN5G(192, 0), CHAN5G(196, 0),
	CHAN5G(200, 0), CHAN5G(204, 0),
	CHAN5G(208, 0), CHAN5G(212, 0),
	CHAN5G(216, 0),
};
*/
static struct ieee80211_supported_band sprdwl_band_2ghz = {
	.n_channels = ARRAY_SIZE(sprdwl_2ghz_channels),
	.channels = sprdwl_2ghz_channels,
	.n_bitrates = SPRDWL_G_RATE_NUM,
	.bitrates = sprdwl_g_rates,
	.ht_cap.cap = sprdwl_g_htcap,
	.ht_cap.ht_supported = true,
};

/*static struct ieee80211_supported_band sprdwl_band_5ghz = {
	.n_channels = ARRAY_SIZE(sprdwl_5ghz_channels),
	.channels = sprdwl_5ghz_channels,
	.n_bitrates = SPRDWL_A_RATE_NUM,
	.bitrates = sprdwl_a_rates,
	.ht_cap.cap = sprdwl_g_htcap,
	.ht_cap.ht_supported = true,
};
*/
static const u32 sprdwl_cipher_suites[] = {
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,
	WLAN_CIPHER_SUITE_SMS4,
};

/* Supported mgmt frame types to be advertised to cfg80211 */
static const struct ieee80211_txrx_stypes
sprdwl_mgmt_stypes[NUM_NL80211_IFTYPES] = {
#ifndef CONFIG_SPRDWL_WIFI_DIRECT
	[NL80211_IFTYPE_STATION] = {
				    .tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
				    BIT(IEEE80211_STYPE_PROBE_RESP >> 4),
				    .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
				    BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
				    },
	[NL80211_IFTYPE_AP] = {
			       .tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
			       BIT(IEEE80211_STYPE_PROBE_RESP >> 4),
			       .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
			       BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
			       },
	[NL80211_IFTYPE_P2P_CLIENT] = {
				       .tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
				       BIT(IEEE80211_STYPE_PROBE_RESP >> 4),
				       .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
				       BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
				       },
	[NL80211_IFTYPE_P2P_GO] = {
				   .tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
				   BIT(IEEE80211_STYPE_PROBE_RESP >> 4),
				   .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
				   BIT(IEEE80211_STYPE_PROBE_REQ >> 4),
				   },
#else
/* Supported mgmt frame types for p2p*/
	[NL80211_IFTYPE_STATION] = {
				    .tx = 0xffff,
				    .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
				    BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
				    },
	[NL80211_IFTYPE_AP] = {
			       .tx = 0xffff,
			       .rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
			       BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
			       BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
			       BIT(IEEE80211_STYPE_DISASSOC >> 4) |
			       BIT(IEEE80211_STYPE_AUTH >> 4) |
			       BIT(IEEE80211_STYPE_DEAUTH >> 4) |
			       BIT(IEEE80211_STYPE_ACTION >> 4)
			       },
	[NL80211_IFTYPE_AP_VLAN] = {
				    /* copy AP */
				    .tx = 0xffff,
				    .rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
				    BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
				    BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
				    BIT(IEEE80211_STYPE_DISASSOC >> 4) |
				    BIT(IEEE80211_STYPE_AUTH >> 4) |
				    BIT(IEEE80211_STYPE_DEAUTH >> 4) |
				    BIT(IEEE80211_STYPE_ACTION >> 4)
				    },
	[NL80211_IFTYPE_P2P_CLIENT] = {
				       .tx = 0xffff,
				       .rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
				       BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
				       },
	[NL80211_IFTYPE_P2P_GO] = {
				   .tx = 0xffff,
				   .rx = BIT(IEEE80211_STYPE_ASSOC_REQ >> 4) |
				   BIT(IEEE80211_STYPE_REASSOC_REQ >> 4) |
				   BIT(IEEE80211_STYPE_PROBE_REQ >> 4) |
				   BIT(IEEE80211_STYPE_DISASSOC >> 4) |
				   BIT(IEEE80211_STYPE_AUTH >> 4) |
				   BIT(IEEE80211_STYPE_DEAUTH >> 4) |
				   BIT(IEEE80211_STYPE_ACTION >> 4)
				   },
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */
};

#ifdef CONFIG_SPRDWL_WIFI_DIRECT

static char type_name[16][32] = {
	"ASSO REQ",
	"ASSO RESP",
	"REASSO REQ",
	"REASSO RESP",
	"PROBE REQ",
	"PROBE RESP",
	"TIMING ADV",
	"RESERVED",
	"BEACON",
	"ATIM",
	"DISASSO",
	"AUTH",
	"DEAUTH",
	"ACTION",
	"ACTION NO ACK",
	"RESERVED"
};

static char pub_action_name[][32] = {
	"GO Negotiation Req",
	"GO Negotiation Resp",
	"GO Negotiation Conf",
	"P2P Invitation Req",
	"P2P Invitation Resp",
	"Device Discovery Req",
	"Device Discovery Resp",
	"Provision Discovery Req",
	"Provision Discovery Resp",
	"Reserved"
};

static char p2p_action_name[][32] = {
	"Notice of Absence",
	"P2P Precence Req",
	"P2P Precence Resp",
	"GO Discoverability Req",
	"Reserved"
};

#define MAC_LEN			(24)
#define ADDR1_OFFSET		(4)
#define ADDR2_OFFSET		(10)
#define ACTION_TYPE		(13)
#define ACTION_SUBTYPE_OFFSET	(30)
#define PUB_ACTION		(0x4)
#define P2P_ACTION		(0x7f)

static void cfg80211_dump_frame_prot_info(struct wiphy *wiphy,
					  int send, int freq,
					  const unsigned char *buf, int len)
{
	char print_buf[1024];
	int buf_idx = 0;
	int type = ((*buf) & IEEE80211_FCTL_FTYPE) >> 2;
	int subtype = ((*buf) & IEEE80211_FCTL_STYPE) >> 4;
	int action, action_subtype;

	buf_idx += sprintf(print_buf + buf_idx, "[cfg80211] ");

	if (send)
		buf_idx += sprintf(print_buf + buf_idx, "SEND: ");
	else
		buf_idx += sprintf(print_buf + buf_idx, "RECV: ");

	if (type == IEEE80211_FTYPE_MGMT) {
		buf_idx += sprintf(print_buf + buf_idx, "%dMHz, %s, ",
				   freq, type_name[subtype]);
	} else {
		buf_idx += sprintf(print_buf + buf_idx,
				   "%dMHz, not mgmt frame, type=%d, ",
				   freq, type);
	}

	if (subtype == ACTION_TYPE) {
		action = *(buf + MAC_LEN);
		action_subtype = *(buf + ACTION_SUBTYPE_OFFSET);
		if (action == PUB_ACTION)
			buf_idx += sprintf(print_buf + buf_idx, "PUB:%s ",
					   pub_action_name[action_subtype]);
		else if (action == P2P_ACTION)
			buf_idx += sprintf(print_buf + buf_idx, "P2P:%s ",
					   p2p_action_name[action_subtype]);
		else
			buf_idx += sprintf(print_buf + buf_idx,
					   "Unknown ACTION(0x%x)", action);
	}

	wiphy_info(wiphy, "%s %pM %pM\n", print_buf, &buf[4], &buf[10]);
}

#endif /*CONFIG_SPRDWL_WIFI_DIRECT */

static inline bool sprdwl_is_ready(struct sprdwl_priv *priv)
{
	if (priv->cp2_status == SPRDWL_READY)
		return true;

	wiphy_err(priv->wiphy, "firmware not ready!\n");
	return false;
}

#define WLAN_EID_VENDOR_SPECIFIC 221
static bool sprdwl_is_wps_ie(const u8 *pos)
{
	return (pos[0] == WLAN_EID_VENDOR_SPECIFIC &&
		pos[1] >= 4 &&
		pos[2] == 0x00 && pos[3] == 0x50 && pos[4] == 0xf2 &&
		pos[5] == 0x04);
}

static bool sprdwl_find_wpsie(const u8 *ies, size_t ies_len,
			      u8 *buf, size_t *wps_len)
{
	const u8 *pos;
	size_t len = 0;
	bool flags = false;

	/* Filter out RSN/WPA IE(s) */
	if (ies && ies_len) {
		pos = ies;
		while (pos + 1 < ies + ies_len) {
			if (pos + 2 + pos[1] > ies + ies_len)
				break;

			if (sprdwl_is_wps_ie(pos)) {
				memcpy(buf + len, pos, 2 + pos[1]);
				len += 2 + pos[1];
				flags = true;
			}

			pos += 2 + pos[1];
		}
	}

	*wps_len = len;
	return flags;
}

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
static bool sprdwl_find_p2p_ie(const u8 *ie, size_t ie_len, u8 *p2p_ie,
			       size_t *p2p_ie_len)
{
	bool flags = false;
	u16 index = 0;

	/*Find out P2P IE*/
	if (NULL == ie || ie_len <= 0 || NULL == p2p_ie)
		return flags;

	while (index < ie_len) {
		if (P2P_IE_ID == ie[index]) {
			*p2p_ie_len = ie[index + 1];
			if (ie_len >= *p2p_ie_len &&
			    P2P_IE_OUI_BYTE0 == ie[index + 2] &&
			    P2P_IE_OUI_BYTE1 == ie[index + 3] &&
			    P2P_IE_OUI_BYTE2 == ie[index + 4] &&
			    P2P_IE_OUI_TYPE == ie[index + 5]) {
				memcpy(p2p_ie, ie + index, *p2p_ie_len + 2);
				*p2p_ie_len += 2;
				return true;
			}
		}
		index++;
	}

	return false;
}
#endif /*CONFIG_SPRDWL_WIFI_DIRECT */

/* pairwise: 0: pairwise 1: group */
static inline int sprdwl_set_cipher(struct sprdwl_vif *vif, bool pairwise,
				    u32 cipher)
{
	struct sprdwl_priv *priv = vif->priv;
	u8 *cipher_ptr = pairwise ? &vif->prwise_crypto : &vif->grp_crypto;

	switch (cipher) {
	case 0:
		/* our own hack to use value 0 as no crypto used */
		*cipher_ptr = SPRDWL_CIPHER_NONE;
		break;
	case WLAN_CIPHER_SUITE_WEP40:
		*cipher_ptr = SPRDWL_CIPHER_WEP40;
		break;
	case WLAN_CIPHER_SUITE_WEP104:
		*cipher_ptr = SPRDWL_CIPHER_WEP104;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		*cipher_ptr = SPRDWL_CIPHER_TKIP;
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		*cipher_ptr = SPRDWL_CIPHER_CCMP;
		break;
		/* WAPI cipher is not processed by CP2 */
	case WLAN_CIPHER_SUITE_SMS4:
		*cipher_ptr = SPRDWL_CIPHER_WAPI;
		break;
	default:
		wiphy_err(priv->wiphy,
			  "%s cipher 0x%x not supported\n", __func__, cipher);
		return -ENOTSUPP;
	}

	return 0;
}

static int sprdwl_add_key(struct sprdwl_vif *vif, bool pairwise,
			  u8 key_index, u32 cipher, const u8 *key_seq,
			  const u8 *macaddr)
{
	struct sprdwl_priv *priv = vif->priv;
	u8 *cipher_ptr = pairwise ? &vif->prwise_crypto : &vif->grp_crypto;
	u8 pn_key[16] = { 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36,
		0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36
	};
	int ret;

	if (vif->key_len[pairwise][0] || vif->key_len[pairwise][1] ||
	    vif->key_len[pairwise][2] || vif->key_len[pairwise][3]) {
		sprdwl_set_cipher(vif, pairwise, cipher);
		if (vif->prwise_crypto == SPRDWL_CIPHER_WAPI)
			memcpy(vif->key_txrsc[pairwise], pn_key,
			       sizeof(pn_key));
		ret = sprdwl_add_key_cmd(priv->sipc,
					 vif->key[pairwise][key_index],
					 vif->key_len[pairwise][key_index],
					 pairwise, key_index, key_seq,
					 *cipher_ptr, macaddr);
		if (ret < 0) {
			wiphy_err(priv->wiphy,
				  "%s error(%d)!\n", __func__, ret);
			return ret;
		}
	}
	return 0;
}

int sprdwl_open_mac(struct sprdwl_vif *vif, enum nl80211_iftype type)
{
	struct sprdwl_priv *priv = vif->priv;
	enum wlan_mode mode;

	wiphy_info(priv->wiphy, "%s old type:%d new type:%d\n",
		   __func__, vif->wdev.iftype, type);

	switch (type) {
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_P2P_CLIENT:
		mode = SPRDWL_STATION_MODE;
		break;
	case NL80211_IFTYPE_AP:
	case NL80211_IFTYPE_P2P_GO:
		mode = SPRDWL_AP_MODE;
		break;
	default:
		wiphy_err(priv->wiphy, "%s invalid interface type: %u\n",
			  __func__, vif->wdev.iftype);
		mutex_unlock(&priv->global_lock);
		return -EOPNOTSUPP;
	}

	if (sprdwl_mac_open_cmd(priv->sipc, mode, vif->ndev->dev_addr)) {
		wiphy_err(priv->wiphy, "%s Failed to open mac!\n", __func__);
		return -EIO;
	}
	priv->mode = mode;
	wiphy_info(priv->wiphy, "occupied by %s (type %d->%d, mode %d)\n",
		   vif->ndev->name, vif->wdev.iftype, type, priv->mode);

	return 0;
}

int sprdwl_close_mac(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;

	if (priv->mode == SPRDWL_NONE_MODE) {
		wiphy_err(priv->wiphy, "%s invalid operation mode: %d\n",
			  __func__, priv->mode);
		return -EINVAL;
	}

	if (sprdwl_mac_close_cmd(priv->sipc, priv->mode)) {
		wiphy_err(priv->wiphy, "%s Failed to close mac!\n", __func__);
		return -EIO;
	}
	wiphy_info(priv->wiphy, "released by %s (type %d, mode %d)\n",
		   vif->ndev->name, vif->wdev.iftype, priv->mode);
	priv->mode = SPRDWL_NONE_MODE;

	return 0;
}

static inline bool sprdwl_is_valid_iftype(enum nl80211_iftype type)
{
	bool ret;

	switch (type) {
	case NL80211_IFTYPE_STATION:
	case NL80211_IFTYPE_AP:
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	case NL80211_IFTYPE_P2P_DEVICE:
	case NL80211_IFTYPE_P2P_CLIENT:
	case NL80211_IFTYPE_P2P_GO:
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	return ret;
}

static struct wireless_dev *sprdwl_cfg80211_add_iface(struct wiphy *wiphy,
						      const char *name,
						      enum nl80211_iftype type,
						      u32 *flags,
						      struct vif_params *params)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif;
	struct net_device *ndev;

	if (!sprdwl_is_valid_iftype(type)) {
		wiphy_err(wiphy, "%s invalid interface type: %u\n",
			  __func__, type);
		return ERR_PTR(-EINVAL);
	}

	ndev = sprdwl_register_netdev(priv, name, type, params->macaddr);
	if (!ndev)
		return ERR_PTR(-ENOMEM);
	vif = netdev_priv(ndev);

	return &vif->wdev;
}

static int sprdwl_cfg80211_del_iface(struct wiphy *wiphy,
				     struct wireless_dev *wdev)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = netdev_priv(wdev->netdev);

	spin_lock_bh(&priv->list_lock);
	list_del(&vif->list);
	spin_unlock_bh(&priv->list_lock);

	sprdwl_unregister_netdev(vif->ndev);

	return 0;
}

static int sprdwl_cfg80211_change_iface(struct wiphy *wiphy,
					struct net_device *ndev,
					enum nl80211_iftype type, u32 *flags,
					struct vif_params *params)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = netdev_priv(ndev);
	int ret;
	enum nl80211_iftype old_type;

	netdev_info(ndev, "%s type %d\n", __func__, type);

	if (!sprdwl_is_ready(priv))
		return -EIO;

	if (!sprdwl_is_valid_iftype(type)) {
		wiphy_err(wiphy, "%s invalid interface type: %u\n",
			  __func__, type);
		return -EOPNOTSUPP;
	}

	old_type = vif->wdev.iftype;
	vif->wdev.iftype = type;
	if (vif == priv->p2p_vif &&
	    (old_type == NL80211_IFTYPE_P2P_DEVICE ||
	    (old_type == NL80211_IFTYPE_STATION &&
	     type == NL80211_IFTYPE_P2P_CLIENT) ||
	    (old_type == NL80211_IFTYPE_P2P_CLIENT &&
	     type == NL80211_IFTYPE_STATION))) {
		wiphy_debug(wiphy, "%s not change type\n", __func__);
		return 0;
	}


	if (!sprdwl_close_mac(vif)) {
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
		if (vif == priv->p2p_vif && type == NL80211_IFTYPE_P2P_GO) {
			ret = sprdwl_set_p2p_enable_cmd(priv->sipc, 1);
			if (ret) {
				wiphy_err(wiphy, "%s Failed to set p2p_enable!\n",
					  __func__);
				return ret;
			}
			netdev_info(ndev, "%s set p2penable ok\n", __func__);
		}
#endif
		sprdwl_open_mac(vif, type);
	}

	return 0;
}

/* supposing sipc cfg80211 privite struct is sprdwl_priv. it should
 * be modified later
 */
static int sprdwl_cfg80211_scan(struct wiphy *wiphy,
				struct cfg80211_scan_request *request)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif =
	    container_of(request->wdev, struct sprdwl_vif, wdev);
	struct cfg80211_ssid *ssids = request->ssids;
	struct wlan_sipc_scan_ssid *scan_ssids;
	u8 *sipc_data = NULL;
	unsigned int i, n, scan_ssids_len = 0;
	int ret;
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	struct wlan_sipc_scan_channels scan_channels;
	static u32 scan_channels_count;
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */

	wiphy_info(wiphy, "%s: scanvif:%s\n", __func__,
		   vif == priv->wlan_vif ? "wlan" : "p2p");

	if (!sprdwl_is_ready(priv))
		return -EIO;

	if (!sprdwl_is_valid_iftype(request->wdev->iftype)) {
		wiphy_err(wiphy, "%s invalid interface type: %u\n",
			  __func__, request->wdev->iftype);
		return -EOPNOTSUPP;
	}

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	ret = sprdwl_set_p2p_enable_cmd(priv->sipc, vif == priv->p2p_vif);
	if (ret) {
		wiphy_err(wiphy, "%s Failed to set p2p_enable!\n",
			  __func__);
		return ret;
	}
#endif
	/* set wps ie */
	if (request->ie_len > 0) {
		if (request->ie_len > 255) {
			wiphy_err(wiphy, "%s invalid len: %zu\n", __func__,
				  request->ie_len);
			return -EOPNOTSUPP;
		}
		ret = sprdwl_set_wps_ie_cmd(priv->sipc, WPS_REQ_IE,
				request->ie, request->ie_len);
		if (ret) {
			wiphy_err(wiphy, "%s Failed to set WPS IE!\n",
				  __func__);
			return ret;
		}
	}
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	/* set scan channel */
	if (vif == priv->p2p_vif) {
		scan_channels.channel_num =
		    (request->n_channels > 4) ? 0 : request->n_channels;
		for (i = 0; i < scan_channels.channel_num; i++) {
			scan_channels.channel_freq[i] =
			     request->channels[i]->center_freq;
		}
		scan_channels_count++;
		scan_channels.random_count = scan_channels_count;
		wiphy_info(wiphy,
			   "%s:channel_num=%d[%d, %d, %d, %d] random_count=0x%x\n",
			   __func__, scan_channels.channel_num,
			   scan_channels.channel_freq[0],
			   scan_channels.channel_freq[1],
			   scan_channels.channel_freq[2],
			   scan_channels.channel_freq[3],
			   scan_channels.random_count);
		ret = sprdwl_set_scan_chan_cmd(priv->sipc,
				(u8 *)&scan_channels,
				sizeof(struct wlan_sipc_scan_channels));
		if (ret) {
			wiphy_err(wiphy, "%s Failed to set scan channels!\n",
				  __func__);
			return ret;
		}
	}
#endif

	n = min(request->n_ssids, 9);
	if (n) {
		sipc_data = kzalloc(512, GFP_ATOMIC);
		if (!sipc_data) {
			wiphy_err(wiphy,
				  "%s Failed to alloc combo ssid!\n", __func__);
			return -2;
		}
		scan_ssids = (struct wlan_sipc_scan_ssid *)sipc_data;
		for (i = 0; i < n; i++) {
			if (!ssids[i].ssid_len)
				continue;
			scan_ssids->len = ssids[i].ssid_len;
			memcpy(scan_ssids->ssid, ssids[i].ssid,
			       ssids[i].ssid_len);
			scan_ssids_len += (ssids[i].ssid_len
					   + sizeof(scan_ssids->len));
			scan_ssids = (struct wlan_sipc_scan_ssid *)
			    (sipc_data + scan_ssids_len);
		}
	} else {
		wiphy_err(wiphy, "%s err param n_ssids is 0\n", __func__);
		return -EINVAL;
	}

	if (priv->scan_vif)
		wiphy_debug(wiphy, "%s ----- scan mutex OK?\n", __func__);
	spin_lock_bh(&vif->scan_lock);
	vif->scan_req = request;
	priv->scan_vif = vif;
	spin_unlock_bh(&vif->scan_lock);
	ret = sprdwl_scan_cmd(priv->sipc, sipc_data, scan_ssids_len);
	kfree(sipc_data);
	if (ret) {
		wiphy_err(wiphy, "%s error(%d)!\n", __func__, ret);
		spin_lock_bh(&vif->scan_lock);
		if (vif->scan_req) {
			cfg80211_scan_done(vif->scan_req, true);
			vif->scan_req = NULL;
			priv->scan_vif = NULL;
		}
		spin_unlock_bh(&vif->scan_lock);
		return ret;
	}

	/* Arm scan timeout timer */
	mod_timer(&vif->scan_timer,
		  jiffies + SPRDWL_SCAN_TIMER_INTERVAL_MS * HZ / 1000);

	return 0;
}

static int sprdwl_cfg80211_connect(struct wiphy *wiphy, struct net_device *ndev,
				   struct cfg80211_connect_params *sme)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = netdev_priv(ndev);
	u8 key_mgmt = 0;
	int is_wep = (sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP40) ||
	    (sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP104);
	int auth_type = 0;
	u8 *buf = NULL;
	size_t wps_len = 0;
	int ret;
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	size_t p2p_len = 0;
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */

	wiphy_info(wiphy, "%s\n", __func__);

	if (!sprdwl_is_ready(priv))
		return -EIO;

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	ret = sprdwl_set_p2p_enable_cmd(priv->sipc, vif == priv->p2p_vif);
	if (ret) {
		wiphy_err(wiphy, "%s Failed to set p2p_enable!\n", __func__);
		return ret;
	}
#endif
	priv->cur_vif = vif;
	/* To avoid confused wapi frame */
	vif->prwise_crypto = SPRDWL_CIPHER_NONE;
	vif->grp_crypto = SPRDWL_CIPHER_NONE;
	/* Get request status, type, bss, ie and so on */
	/* Set appending ie */
	/* Set wps ie */
	if (sme->ie_len > 0) {
		if (sme->ie_len > 255) {
			wiphy_err(wiphy, "%s invalid len: %zu\n", __func__,
				  sme->ie_len);
			return -EOPNOTSUPP;
		}
		buf = kmalloc(sme->ie_len, GFP_KERNEL);
		if (buf == NULL)
			return -ENOMEM;

		if (sprdwl_find_wpsie(sme->ie, sme->ie_len, buf, &wps_len)) {
			ret = sprdwl_set_wps_ie_cmd(priv->sipc,
						    WPS_ASSOC_IE, buf, wps_len);
			if (ret) {
				wiphy_err(wiphy, "%s Failed to set wpas ie!\n",
					  __func__);
				kfree(buf);
				return ret;
			}
		}
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
		if (sprdwl_find_p2p_ie(sme->ie, sme->ie_len, buf, &p2p_len)) {
			ret = sprdwl_set_p2p_ie_cmd(priv->sipc,
						    P2P_ASSOC_IE, buf, p2p_len);
			if (ret) {
				wiphy_err(wiphy, "%s Failed to set p2p ie!\n",
					  __func__);
				kfree(buf);
				return ret;
			}
		}
#endif /*CONFIG_SPRDWL_WIFI_DIRECT */

		kfree(buf);
	}

	/* Set WPA version */
	if (sme->crypto.wpa_versions) {
		wiphy_info(wiphy, "wpa versions %#x\n",
			   sme->crypto.wpa_versions);
		ret = sprdwl_set_wpa_version_cmd(priv->sipc,
						 sme->crypto.wpa_versions);
		if (ret < 0) {
			wiphy_err(wiphy, "%s Failed to set wpa version!\n",
				  __func__);
			return ret;
		}
	} else {
		wiphy_dbg(wiphy, "No wpa versions\n");
	}

	/* Set Auth type */
	wiphy_info(wiphy, "auth type %#x\n", sme->auth_type);
	/* Set the authorisation */
	if ((sme->auth_type == NL80211_AUTHTYPE_OPEN_SYSTEM) ||
	    ((sme->auth_type == NL80211_AUTHTYPE_AUTOMATIC) && !is_wep))
		auth_type = SPRDWL_AUTH_OPEN;
	else if ((sme->auth_type == NL80211_AUTHTYPE_SHARED_KEY) ||
		 ((sme->auth_type == NL80211_AUTHTYPE_AUTOMATIC) && is_wep))
		auth_type = SPRDWL_AUTH_SHARED;
	ret = sprdwl_set_auth_type_cmd(priv->sipc, auth_type);
	if (ret < 0) {
		wiphy_err(wiphy, "%s Failed to set auth type!\n", __func__);
		return ret;
	}
	/* Set pairewise cipher */
	if (sme->crypto.n_ciphers_pairwise) {
		sprdwl_set_cipher(vif, true, sme->crypto.ciphers_pairwise[0]);
		if (vif->prwise_crypto != SPRDWL_CIPHER_WAPI) {
			wiphy_info(wiphy, "pairwise cipher %#x\n",
				   sme->crypto.ciphers_pairwise[0]);
			ret =
			    sprdwl_set_cipher_cmd(priv->sipc,
						  vif->prwise_crypto,
						  WIFI_CMD_PAIRWISE_CIPHER);
			if (ret < 0) {
				wiphy_err(wiphy,
					  "%s Failed to set pairwise cipher!\n",
					  __func__);
				return ret;
			}
		}
	} else {
		wiphy_dbg(wiphy, "No pairewise cipher\n");
	}

	/* Set group cipher */
	sprdwl_set_cipher(vif, false, sme->crypto.cipher_group);
	if (vif->prwise_crypto != SPRDWL_CIPHER_WAPI) {
		wiphy_info(wiphy, "group cipher %#x\n",
			   sme->crypto.cipher_group);
		ret = sprdwl_set_cipher_cmd(priv->sipc, vif->grp_crypto,
					    WIFI_CMD_GROUP_CIPHER);
		if (ret < 0) {
			wiphy_err(wiphy, "%s Failed to set group cipher!\n",
				  __func__);
			return ret;
		}
	}

	/* FIXME */
	/* Set Auth type again because of CP2 process's differece */
	/* Set the authorisation */
	if ((sme->auth_type == NL80211_AUTHTYPE_OPEN_SYSTEM) ||
	    ((sme->auth_type == NL80211_AUTHTYPE_AUTOMATIC) && !is_wep))
		auth_type = SPRDWL_AUTH_OPEN;
	else if ((sme->auth_type == NL80211_AUTHTYPE_SHARED_KEY) ||
		 ((sme->auth_type == NL80211_AUTHTYPE_AUTOMATIC) && is_wep))
		auth_type = SPRDWL_AUTH_SHARED;
	ret = sprdwl_set_auth_type_cmd(priv->sipc, auth_type);
	if (ret < 0) {
		wiphy_err(wiphy, "%s Failed to set auth type!\n", __func__);
		return ret;
	}

	/* Set auth key management (akm) */
	if (sme->crypto.n_akm_suites) {
		if (sme->crypto.akm_suites[0] == WLAN_AKM_SUITE_PSK)
			key_mgmt = AKM_SUITE_PSK;
		else if (sme->crypto.akm_suites[0] == WLAN_AKM_SUITE_8021X)
			key_mgmt = AKM_SUITE_8021X;
		/* WAPI akm is not processed by CP2 */
		else if (sme->crypto.akm_suites[0] == WLAN_AKM_SUITE_WAPI_CERT)
			key_mgmt = AKM_SUITE_WAPI;
		else if (sme->crypto.akm_suites[0] == WLAN_AKM_SUITE_WAPI_PSK)
			key_mgmt = AKM_SUITE_WAPI;

		wiphy_info(wiphy, "akm suites %#x\n",
			   sme->crypto.akm_suites[0]);
		ret = sprdwl_set_key_management_cmd(priv->sipc, key_mgmt);
		if (ret < 0) {
			wiphy_err(wiphy, "%s Failed to set key management!\n",
				  __func__);
			return ret;
		}
	} else {
		wiphy_dbg(wiphy, "No akm suites\n");
	}

	/* Set PSK */
	if (sme->key_len) {
		if (sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP40 ||
		    sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP104 ||
		    sme->crypto.ciphers_pairwise[0] == WLAN_CIPHER_SUITE_WEP40
		    || sme->crypto.ciphers_pairwise[0] ==
		    WLAN_CIPHER_SUITE_WEP104) {
			vif->key_index[GROUP] = sme->key_idx;
			vif->key_len[GROUP][sme->key_idx] = sme->key_len;
			memcpy(vif->key[GROUP][sme->key_idx], sme->key,
			       sme->key_len);
			ret =
			    sprdwl_add_key(vif, 0, sme->key_idx,
					   sme->crypto.ciphers_pairwise[0],
					   NULL, NULL);
			if (ret < 0) {
				wiphy_err(wiphy,
					  "%s Failed to add cipher key!\n",
					  __func__);
				return ret;
			}
		} else {
			if (sme->key_len > 32) {
				wiphy_err(wiphy,
					  "%s invalid key len: %d\n", __func__,
					  sme->key_len);
				return -EINVAL;
			}
			wiphy_info(wiphy, "psk %s\n", sme->key);
			ret = sprdwl_set_psk_cmd(priv->sipc, sme->key,
						 sme->key_len);
			if (ret < 0) {
				wiphy_err(wiphy, "%s Failed to set psk!\n",
					  __func__);
				return ret;
			}
		}
	}

	/* Auth RX unencrypted EAPOL is not implemented, do nothing */
	/* Set channel */
	if (sme->channel != NULL) {
		wiphy_info(wiphy, "channel %d\n",
			   ieee80211_frequency_to_channel(sme->channel->
							  center_freq));
		ret =
		    sprdwl_set_channel_cmd(priv->sipc,
					   ieee80211_frequency_to_channel
					   (sme->channel->center_freq));
		if (ret < 0) {
			wiphy_err(wiphy,
				  "%s Failed to set channel!\n", __func__);
			return ret;
		}
	} else {
		wiphy_dbg(wiphy, "Channel not specified\n");
	}

	/* Set BSSID */
	if (sme->bssid != NULL) {
		ret = sprdwl_set_bssid_cmd(priv->sipc, sme->bssid);
		if (ret < 0) {
			wiphy_err(wiphy, "%s Failed to set bssid!\n", __func__);
			return ret;
		}
		memcpy(vif->bssid, sme->bssid, sizeof(vif->bssid));
	} else {
		wiphy_dbg(wiphy, "BSSID not specified\n");
	}

	/* Special process for WEP(WEP key must be set before essid) */
	if (sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP40 ||
	    sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP104) {
		wiphy_info(wiphy, "%s WEP cipher_group\n", __func__);

		if (sme->key_len <= 0) {
			wiphy_dbg(wiphy, "No key is specified\n");
		} else {
			if (sme->key_len != WLAN_KEY_LEN_WEP104 &&
			    sme->key_len != WLAN_KEY_LEN_WEP40) {
				wiphy_err(wiphy, "%s invalid WEP key length!\n",
					  __func__);
				return -EINVAL;
			}

			sprdwl_set_key_cmd(priv->sipc, sme->key_idx);
		}
	}

	/* Set ESSID */
	if (sme->ssid != NULL) {
		ret = sprdwl_set_essid_cmd(priv->sipc, sme->ssid,
					   (int)sme->ssid_len);
		if (ret < 0) {
			wiphy_err(wiphy, "%s Failed to set ssid!\n", __func__);
			return ret;
		}
		memcpy(vif->ssid, sme->ssid, sme->ssid_len);
		if (sme->ssid_len <= IEEE80211_MAX_SSID_LEN)
			vif->ssid[sme->ssid_len] = 0;
		vif->ssid_len = sme->ssid_len;
		wiphy_info(wiphy, "%s %s (%pM)\n", __func__,
			   vif->ssid, sme->bssid);
	}

	vif->sm_state = SPRDWL_CONNECTING;
	return ret;
}

static int sprdwl_cfg80211_disconnect(struct wiphy *wiphy,
				      struct net_device *ndev, u16 reason_code)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = netdev_priv(ndev);
	int ret;
	enum sm_state old = 0;

	wiphy_info(wiphy, "%s %s (%pM)\n", __func__, vif->ssid,
		   vif->bssid);

	if (!sprdwl_is_ready(priv))
		return -EIO;

	old = vif->sm_state;
	vif->sm_state = SPRDWL_DISCONNECTED;
	ret = sprdwl_disconnect_cmd(priv->sipc, reason_code);
	if (ret < 0) {
		wiphy_err(wiphy, "%s Failed disconnect!\n", __func__);
		vif->sm_state = old;
	}

	return ret;
}

static int sprdwl_cfg80211_add_key(struct wiphy *wiphy, struct net_device *ndev,
				   u8 key_index, bool pairwise,
				   const u8 *mac_addr,
				   struct key_params *params)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = netdev_priv(ndev);
	int ret;

	wiphy_info(wiphy, "%s key_index=%d, pairwise=%d\n",
		   __func__, key_index, pairwise);

	if (!sprdwl_is_ready(priv))
		return -EIO;

	vif->key_index[pairwise] = key_index;
	vif->key_len[pairwise][key_index] = params->key_len;
	memcpy(vif->key[pairwise][key_index], params->key,
	       params->key_len);
	ret = sprdwl_add_key(vif, pairwise, key_index, params->cipher,
			     params->seq, mac_addr);
	if (ret < 0) {
		wiphy_err(wiphy,
			  "%s Failed to add cipher key!\n", __func__);
		return ret;
	}

	return 0;
}

static int sprdwl_cfg80211_del_key(struct wiphy *wiphy, struct net_device *ndev,
				   u8 key_index, bool pairwise,
				   const u8 *mac_addr)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = netdev_priv(ndev);

	wiphy_info(wiphy, "%s key_index=%d, pairwise=%d\n",
		   __func__, key_index, pairwise);

	if (!sprdwl_is_ready(priv))
		return -EIO;

	if (key_index > WLAN_MAX_KEY_INDEX) {
		wiphy_err(wiphy, "%s key index %d out of bounds!\n", __func__,
			  key_index);
		return -ENOENT;
	}

	if (!vif->key_len[pairwise][key_index]) {
		wiphy_err(wiphy, "%s key index %d is empty!\n", __func__,
			  key_index);
		return 0;
	}

	vif->key_len[pairwise][key_index] = 0;
	vif->prwise_crypto = SPRDWL_CIPHER_NONE;
	vif->grp_crypto = SPRDWL_CIPHER_NONE;

	return sprdwl_del_key_cmd(priv->sipc, key_index, mac_addr);
}

static int sprdwl_cfg80211_set_default_key(struct wiphy *wiphy,
					   struct net_device *ndev,
					   u8 key_index, bool unicast,
					   bool multicast)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	int ret;

	if (!sprdwl_is_ready(priv))
		return -EIO;

	if (key_index > 3) {
		wiphy_err(wiphy, "%s invalid key index: %d\n", __func__,
			  key_index);
		return -EINVAL;
	}

	ret = sprdwl_set_key_cmd(priv->sipc, key_index);
	if (ret < 0) {
		wiphy_err(wiphy, "%s Failed to set key!\n", __func__);
		return ret;
	}

	return 0;
}

static int sprdwl_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	int ret;

	if (!sprdwl_is_ready(priv))
		return -EIO;

	if (changed & WIPHY_PARAM_RTS_THRESHOLD) {
		ret = sprdwl_set_rts_cmd(priv->sipc, wiphy->rts_threshold);
		if (ret != 0) {
			wiphy_err(wiphy, "%s Failed to set rts!\n", __func__);
			return -EIO;
		}
	}

	if (changed & WIPHY_PARAM_FRAG_THRESHOLD) {
		ret = sprdwl_set_frag_cmd(priv->sipc, wiphy->frag_threshold);
		if (ret != 0) {
			wiphy_err(wiphy, "%s Failed to set frag!\n", __func__);
			return -EIO;
		}
	}
	return 0;
}

static int sprdwl_cfg80211_get_station(struct wiphy *wiphy,
				       struct net_device *ndev, u8 *mac,
				       struct station_info *sinfo)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	s8 signal, noise;
	s32 rate;
	int ret;
	size_t i;

	if (!sprdwl_is_ready(priv))
		return -EIO;

	sinfo->filled |= STATION_INFO_TX_BYTES |
			 STATION_INFO_TX_PACKETS |
			 STATION_INFO_RX_BYTES |
			 STATION_INFO_RX_PACKETS;
	sinfo->tx_bytes = ndev->stats.tx_bytes;
	sinfo->tx_packets = ndev->stats.tx_packets;
	sinfo->rx_bytes = ndev->stats.rx_bytes;
	sinfo->rx_packets = ndev->stats.rx_packets;

	/* Get current RSSI */
	ret = sprdwl_get_rssi_cmd(priv->sipc, &signal, &noise);
	if (ret == 0) {
		sinfo->signal = signal;
		sinfo->filled |= STATION_INFO_SIGNAL;
	} else {
		wiphy_err(wiphy, "%s Failed to get RSSI!\n", __func__);
		return -EIO;
	}

	ret = sprdwl_get_txrate_cmd(priv->sipc, &rate);
	if (ret == 0) {
		sinfo->filled |= STATION_INFO_TX_BITRATE;
	} else {
		wiphy_err(wiphy, "%s Failed to get txrate!\n", __func__);
		return -EIO;
	}

	/* Convert got rate from hw_value to NL80211 value */
	if (!(rate & 0x7f)) {
		wiphy_info(wiphy, "%s rate %d\n", __func__, (rate & 0x7f));
		sinfo->txrate.legacy = 10;
	} else {
		for (i = 0; i < ARRAY_SIZE(sprdwl_rates); i++) {
			if (rate == sprdwl_rates[i].hw_value) {
				sinfo->txrate.legacy = sprdwl_rates[i].bitrate;
				if (rate & 0x80)
					sinfo->txrate.mcs =
					    sprdwl_rates[i].hw_value;
				break;
			}
		}

		if (i >= ARRAY_SIZE(sprdwl_rates))
			sinfo->txrate.legacy = 10;
	}

	wiphy_info(wiphy, "%s signal %d txrate %d\n", __func__, sinfo->signal,
		   sinfo->txrate.legacy);

	return 0;
}

static int sprdwl_cfg80211_set_pmksa(struct wiphy *wiphy,
				     struct net_device *ndev,
				     struct cfg80211_pmksa *pmksa)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	int ret;

	wiphy_info(wiphy, "%s\n", __func__);

	if (!sprdwl_is_ready(priv))
		return -EIO;

	ret = sprdwl_pmksa_cmd(priv->sipc, pmksa->bssid,
			       pmksa->pmkid, CMD_TYPE_SET);

	return ret;
}

static int sprdwl_cfg80211_del_pmksa(struct wiphy *wiphy,
				     struct net_device *ndev,
				     struct cfg80211_pmksa *pmksa)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	int ret;

	wiphy_info(wiphy, "%s\n", __func__);

	if (!sprdwl_is_ready(priv))
		return -EIO;

	ret = sprdwl_pmksa_cmd(priv->sipc, pmksa->bssid,
			       pmksa->pmkid, CMD_TYPE_DEL);

	return ret;
}

static int sprdwl_cfg80211_flush_pmksa(struct wiphy *wiphy,
				       struct net_device *ndev)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = netdev_priv(ndev);
	int ret;

	wiphy_info(wiphy, "%s\n", __func__);

	if (!sprdwl_is_ready(priv))
		return -EIO;

	ret = sprdwl_pmksa_cmd(priv->sipc, vif->bssid, NULL, CMD_TYPE_FLUSH);

	return ret;
}

void sprdwl_scan_timeout(unsigned long data)
{
	struct sprdwl_vif *vif = (struct sprdwl_vif *)data;
	struct sprdwl_priv *priv = vif->priv;

	spin_lock_bh(&vif->scan_lock);
	if (vif->scan_req) {
		wiphy_err(priv->wiphy, "%s\n", __func__);
		cfg80211_scan_done(vif->scan_req, true);
		vif->scan_req = NULL;
		priv->scan_vif = NULL;
	}
	spin_unlock_bh(&vif->scan_lock);

	return;
}

/*
 * flag: 0 for scan_req
 *       1 for WIFI_EVENT_INTERNAL_BSS_INFO,update bss info when connecting
 */
void sprdwl_event_scan_results(struct sprdwl_priv *priv, int flag)
{
	struct sprdwl_vif *vif;
	struct wiphy *wiphy = priv->wiphy;
	struct ieee80211_supported_band *band;
	struct ieee80211_channel *channel;
	struct ieee80211_mgmt *mgmt = NULL;
	struct cfg80211_bss *bss = NULL;
	u32 i = 0, left = priv->sipc->event_len;
	u8 *pos = priv->sipc->event_buf->u.event.variable, *ie;
	u16 channel_num, channel_len, mgmt_len, capability, beacon_interval;
	s16 rssi, rssi_len;
	u32 freq;
	s32 signal;
	u64 tsf;
	size_t ielen;
	bool aborted = false;

	wiphy_info(wiphy, "%s flag:%d\n", __func__, flag);
	if (flag == 0) {
		vif = priv->scan_vif;
		if (!vif) {
			wiphy_debug(wiphy, "%s scan timeout?\n", __func__);
			return;
		}
	}

	if (left < 10) {
		wiphy_err(wiphy, "%s invalid event len: %d\n", __func__, left);
		/* aborted = true; */
		goto out;
	}

	while (left >= 10) {
		/* must use memcpy to protect unaligned */
		/* The formate of frame is len(two bytes) + data */
		memcpy(&channel_len, pos, sizeof(channel_len));
		pos += sizeof(channel_len);
		left -= sizeof(channel_len);
		memcpy(&channel_num, pos, channel_len);
		pos += channel_len;
		left -= channel_len;
		/* FIXME only support 2GHZ */
		band = wiphy->bands[IEEE80211_BAND_2GHZ];
		freq = ieee80211_channel_to_frequency(channel_num, band->band);
		channel = ieee80211_get_channel(wiphy, freq);
		if (!channel) {
			wiphy_err(wiphy, "%s invalid freq!\n", __func__);
			continue;
		}

		/* The second two value of frame is rssi */
		memcpy(&rssi_len, pos, sizeof(rssi_len));
		pos += sizeof(rssi_len);
		left -= sizeof(rssi_len);
		memcpy(&rssi, pos, rssi_len);
		pos += rssi_len;
		left -= rssi_len;
		signal = rssi * 100;

		/* The third two value of frame is following data len */
		memcpy(&mgmt_len, pos, sizeof(mgmt_len));
		pos += sizeof(mgmt_len);
		left -= sizeof(mgmt_len);
		if (mgmt_len > left) {
			wiphy_err(wiphy,
				  "%s mgmt_len(0x%08x) > left(0x%08x)!\n",
				  __func__, mgmt_len, left);
			aborted = true;
			goto out;
		}
		/* The following is real data */
		mgmt = (struct ieee80211_mgmt *)pos;
		pos += mgmt_len;
		left -= mgmt_len;
		ie = mgmt->u.probe_resp.variable;
		ielen = mgmt_len - offsetof(struct ieee80211_mgmt,
					    u.probe_resp.variable);
		tsf = le64_to_cpu(mgmt->u.probe_resp.timestamp);
		beacon_interval = le16_to_cpu(mgmt->u.probe_resp.beacon_int);
		capability = le16_to_cpu(mgmt->u.probe_resp.capab_info);
		if (flag == 0)
			wiphy_dbg(wiphy, "   %s, %pM, channel %2u, signal %d\n",
				  ieee80211_is_probe_resp(mgmt->frame_control)
				  ? "proberesp" : "beacon   ",
				  mgmt->bssid, channel_num, rssi);

		bss = cfg80211_inform_bss(wiphy, channel, mgmt->bssid,
					  tsf, capability, beacon_interval, ie,
					  ielen, signal, GFP_KERNEL);

		if (unlikely(!bss))
			wiphy_err(wiphy,
				  "%s Failed to inform bss frame!\n", __func__);
		else
			cfg80211_put_bss(wiphy, bss);
		i++;
	}

	if (left) {
		wiphy_err(wiphy, "%s invalid event left len: %d\n",
			  __func__, left);
		aborted = true;
		goto out;
	}

	if (flag == 0 && vif->beacon_loss) {
		bss = cfg80211_get_bss(wiphy, NULL,
				       vif->bssid, vif->ssid, vif->ssid_len,
				       WLAN_CAPABILITY_ESS,
				       WLAN_CAPABILITY_ESS);
		if (bss) {
			cfg80211_unlink_bss(wiphy, bss);
			wiphy_info(wiphy, "beacon loss: %pM", bss->bssid);
			vif->beacon_loss = 0;
		}
	}

	if (flag == 0)
		wiphy_info(wiphy, "%s got %d BSSes flag:%d\n",
			   __func__, i, flag);

out:
	if (flag == 0) {
		if (timer_pending(&vif->scan_timer))
			del_timer_sync(&vif->scan_timer);
		spin_lock_bh(&vif->scan_lock);
		if (vif->scan_req) {
			cfg80211_scan_done(vif->scan_req, aborted);
			vif->scan_req = NULL;
			priv->scan_vif = NULL;
		}
		spin_unlock_bh(&vif->scan_lock);
	}

	return;
}

void sprdwl_event_connect_result(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	u8 *req_ie_ptr, *resp_ie_ptr, *bssid_ptr, *pos, *value_ptr;
	u8 status_code = 0, req_ie_len, resp_ie_len;
	u16 bssid_len, status_len;
	u32 event_len;
	int left;

	if (!vif) {
		pr_err("%s: unlikly vif\n", __func__);
		return;
	}

	event_len = priv->sipc->event_len;
	/* status_len 2 + status_code 1 = 3 bytes */
	if (event_len < 3) {
		wiphy_err(priv->wiphy,
			  "%s invalid event len: %d\n", __func__, event_len);
		goto out;
	}
	pos = kmalloc(event_len, GFP_ATOMIC);
	if (pos == NULL)
		goto out;

	/* The first byte of event data is status and len */
	memcpy(pos, priv->sipc->event_buf->u.event.variable, event_len);
	memcpy(&status_len, pos, sizeof(status_len));
	if (status_len != 1) {
		wiphy_err(priv->wiphy,
			  "%s invalid status len: %d\n", __func__, status_len);
		goto freepos;
	}
	memcpy(&status_code, pos + sizeof(status_len), status_len);
	/* FIXME later the status code should be reported by CP2 */
	if (status_code != 0) {
		wiphy_err(priv->wiphy,
			  "%s Failed to connect(%d)!\n", __func__, status_code);
		goto freepos;
	}

	value_ptr = pos + sizeof(status_len) + status_len;
	left = event_len - sizeof(status_len) - status_len;
	/* BSSID is 6 + len is 2 = 8 */
	if (left < 8) {
		wiphy_err(priv->wiphy, "%s invaild bssid!\n", __func__);
		goto freepos;
	}
	memcpy(&bssid_len, value_ptr, sizeof(bssid_len));
	left -= sizeof(bssid_len);
	bssid_ptr = value_ptr + sizeof(bssid_len);
	left -= bssid_len;

	if (!left) {
		wiphy_err(priv->wiphy, "%s no req_ie frame!\n", __func__);
		goto freepos;
	}
	req_ie_len = *(u8 *)(bssid_ptr + bssid_len);
	left -= sizeof(req_ie_len);
	req_ie_ptr = bssid_ptr + bssid_len + sizeof(req_ie_len);
	left -= req_ie_len;
	if (!left) {
		wiphy_err(priv->wiphy, "%s no resp_ie frame!\n", __func__);
		goto freepos;
	}
	resp_ie_len = *(u8 *)(req_ie_ptr + req_ie_len);
	resp_ie_ptr = req_ie_ptr + req_ie_len + sizeof(resp_ie_ptr);

	if (vif->sm_state == SPRDWL_CONNECTING) {
		/* inform connect result to cfg80211 */
		vif->sm_state = SPRDWL_CONNECTED;
		cfg80211_connect_result(vif->ndev,
					bssid_ptr, req_ie_ptr, req_ie_len,
					resp_ie_ptr, resp_ie_len,
					WLAN_STATUS_SUCCESS, GFP_KERNEL);
		wiphy_info(priv->wiphy, "%s %s successfully!\n", __func__,
			   vif->ssid);

		if (!netif_carrier_ok(vif->ndev)) {
			netif_carrier_on(vif->ndev);
			netif_wake_queue(vif->ndev);
		}
	} else {
		wiphy_err(priv->wiphy,
			  "%s wrong previous connect status!\n", __func__);
		goto freepos;
	}

	kfree(pos);
	return;

freepos:
	kfree(pos);
out:
	if (vif->sm_state == SPRDWL_CONNECTING) {
		cfg80211_connect_result(vif->ndev, vif->bssid, NULL, 0, NULL,
					0, WLAN_STATUS_UNSPECIFIED_FAILURE,
					GFP_KERNEL);
		wiphy_info(priv->wiphy, "%s %s failed!\n", __func__, vif->ssid);
	} else if (vif->sm_state == SPRDWL_CONNECTED) {
		cfg80211_disconnected(vif->ndev, status_code,
				      NULL, 0, GFP_KERNEL);
	}
	memset(vif->ssid, 0, sizeof(vif->ssid));
	return;
}

void sprdwl_event_disconnect(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	u16 reason_code = 0, status_code = 0;
	u8 *pos;

	if (!vif) {
		pr_err("%s: unlikly vif\n", __func__);
		return;
	}

	pos = priv->sipc->event_buf->u.event.variable;
	if (pos)
		memcpy(&reason_code, pos, sizeof(reason_code));
	pos += 2;
	if (pos)
		memcpy(&status_code, pos, sizeof(status_code));

	if (timer_pending(&vif->scan_timer))
		del_timer_sync(&vif->scan_timer);
	spin_lock_bh(&vif->scan_lock);
	if (vif->scan_req) {
		cfg80211_scan_done(vif->scan_req, true);
		vif->scan_req = NULL;
		priv->scan_vif = NULL;
	}
	spin_unlock_bh(&vif->scan_lock);

	if (vif->sm_state == SPRDWL_CONNECTING) {
		cfg80211_connect_result(vif->ndev, vif->bssid, NULL, 0, NULL,
					0, WLAN_STATUS_UNSPECIFIED_FAILURE,
					GFP_KERNEL);
	} else if (vif->sm_state == SPRDWL_CONNECTED) {
		cfg80211_disconnected(vif->ndev, reason_code,
				      NULL, 0, GFP_KERNEL);
		wiphy_info(priv->wiphy,
			   "%s %s, reason_code %d, status_code %d\n", __func__,
			   vif->ssid, reason_code, status_code);
	}

	vif->sm_state = SPRDWL_DISCONNECTED;
	memset(vif->ssid, 0, sizeof(vif->ssid));
	if (netif_carrier_ok(vif->ndev)) {
		netif_carrier_off(vif->ndev);
		netif_stop_queue(vif->ndev);
	}
	return;
}

void sprdwl_event_report_mic_failure(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	struct wlan_sipc_mic_failure *mic_failure;
	u8 *pos;

	if (!vif) {
		pr_err("%s: unlikly vif\n", __func__);
		return;
	}

	pos = priv->sipc->event_buf->u.event.variable;
	mic_failure = (struct wlan_sipc_mic_failure *)pos;
	/* debug info,Pls remove it in the future */
	wiphy_info(priv->wiphy,
		   "%s is_mcast:0x%x key_id: 0x%x bssid: %pM\n",
		   __func__, mic_failure->is_mcast, mic_failure->key_id,
		   vif->bssid);

	cfg80211_michael_mic_failure(vif->ndev, vif->bssid,
				     (mic_failure->
				      is_mcast ? NL80211_KEYTYPE_GROUP :
				      NL80211_KEYTYPE_PAIRWISE),
				     mic_failure->key_id, NULL, GFP_KERNEL);
}

void sprdwl_event_report_cqm(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	wiphy_info(priv->wiphy, "%s %d\n", __func__, vif->cqm);

	cfg80211_cqm_rssi_notify(vif->ndev, vif->cqm, GFP_KERNEL);
}

void sprdwl_event_ready(struct sprdwl_priv *priv)
{
	priv->cp2_status = SPRDWL_READY;
}

void sprdwl_event_tx_busy(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	u8 busy_flag = 0;

	if (!vif) {
		pr_err("%s: unlikly vif\n", __func__);
		return;
	}

	/* busy_flag is 1 mean stop tx otherwise wake tx */
	memcpy(&busy_flag, priv->sipc->event_buf->u.event.variable,
	       sizeof(busy_flag));
	if (busy_flag) {
		atomic_set(&priv->stopped, 1);
		netif_stop_queue(vif->ndev);
		wiphy_dbg(priv->wiphy, "tx busy event, stop queue\n");
	} else {
		atomic_set(&priv->stopped, 0);
		netif_wake_queue(vif->ndev);
		wiphy_dbg(priv->wiphy, "tx ok event, wake up queue\n");
	};

	return;
}

void sprdwl_event_softap(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	struct wlan_softap_event *event;
	struct station_info sinfo;

	if (!vif) {
		pr_err("%s: unlikly vif\n", __func__);
		return;
	}

	event = (struct wlan_softap_event *)
		priv->sipc->event_buf->u.event.variable;
	memset(&sinfo, 0, sizeof(sinfo));
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	if (event->req_ie_len > 0) {
		sinfo.assoc_req_ies = event->ie;
		sinfo.assoc_req_ies_len = event->req_ie_len;
		sinfo.filled |= STATION_INFO_ASSOC_REQ_IES;
	}
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */

	if (event->connected) {
		cfg80211_new_sta(vif->ndev, (u8 const *)&event->mac, &sinfo,
				 GFP_KERNEL);
		wiphy_dbg(priv->wiphy, "New station (%pM) connected\n",
			  event->mac);
	} else {
		cfg80211_del_sta(vif->ndev, (u8 const *)&event->mac,
				 GFP_ATOMIC);
		wiphy_dbg(priv->wiphy, "A station (%pM) disconnected\n",
			  event->mac);
	}
}

static int sprdwl_change_beacon(struct sprdwl_vif *vif,
				struct cfg80211_beacon_data *beacon)
{
	struct sprdwl_priv *priv = vif->priv;
	int ret = 0;

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	u16 ie_len;
	u8 *ie_ptr;

	if (vif != priv->p2p_vif)
		return ret;
	/* send beacon extra ies */
	if (beacon->head != NULL) {
		ie_len = beacon->head_len;

		ie_ptr = kmalloc(ie_len, GFP_ATOMIC);
		if (ie_ptr == NULL)
			return -EINVAL;

		memcpy(ie_ptr, beacon->head, ie_len);
		wiphy_dbg(priv->wiphy, "begin send beacon head ies\n");

		ret = sprdwl_set_p2p_ie_cmd(priv->sipc,
					    P2P_BEACON_IE_HEAD, ie_ptr, ie_len);
		if (ret) {
			wiphy_err(priv->wiphy,
				  "%s Failed to set p2p_ie of beacon_ies head!\n",
				  __func__);
		} else {
			wiphy_dbg(priv->wiphy,
				  "send beacon head ies successfully\n");
		}

		kfree(ie_ptr);
	}

	/* send beacon extra ies */
	if (beacon->tail != NULL) {
		ie_len = beacon->tail_len;

		ie_ptr = kmalloc(ie_len, GFP_ATOMIC);
		if (ie_ptr == NULL)
			return -EINVAL;

		memcpy(ie_ptr, beacon->tail, ie_len);
		wiphy_dbg(priv->wiphy, "begin send beacon tail ies\n");

		ret = sprdwl_set_p2p_ie_cmd(priv->sipc,
					    P2P_BEACON_IE_TAIL, ie_ptr, ie_len);
		if (ret) {
			wiphy_err(priv->wiphy,
				  "%s Failed to set p2p_ie of beacon_ies tail!\n",
				  __func__);
		} else {
			wiphy_dbg(priv->wiphy,
				  "send beacon tail ies successfully\n");
		}

		kfree(ie_ptr);
	}

	/* send probe response ies */

	/* send beacon extra ies */
	if (beacon->beacon_ies != NULL) {
		ie_len = beacon->beacon_ies_len;

		ie_ptr = kmalloc(ie_len, GFP_ATOMIC);
		if (ie_ptr == NULL)
			return -EINVAL;

		memcpy(ie_ptr, beacon->beacon_ies, ie_len);
		wiphy_dbg(priv->wiphy, "begin send beacon extra ies\n");

		ret = sprdwl_set_p2p_ie_cmd(priv->sipc,
					    P2P_BEACON_IE, ie_ptr, ie_len);
		if (ret) {
			wiphy_err(priv->wiphy,
				  "%s Failed to set p2p_ie of beacon_ies!\n",
				  __func__);
		} else {
			wiphy_dbg(priv->wiphy,
				  "send beacon extra ies successfully\n");
		}

		kfree(ie_ptr);
	}

	/* send probe response ies */

	if (beacon->proberesp_ies != NULL) {
		ie_len = beacon->proberesp_ies_len;

		ie_ptr = kmalloc(ie_len, GFP_ATOMIC);
		if (ie_ptr == NULL)
			return -EINVAL;

		memcpy(ie_ptr, beacon->proberesp_ies, ie_len);
		wiphy_dbg(priv->wiphy, "begin send probe response extra ies\n");

		ret = sprdwl_set_p2p_ie_cmd(priv->sipc,
					    P2P_PROBERESP_IE, ie_ptr, ie_len);
		if (ret) {
			wiphy_err(priv->wiphy,
				  "%s Failed to set p2p_ie of proberesp_ies!\n",
				  __func__);
		} else {
			wiphy_dbg(priv->wiphy,
				  "send probe response ies successfully\n");
		}

		kfree(ie_ptr);
	}

	/* send associate response ies */

	if (beacon->assocresp_ies != NULL) {
		ie_len = beacon->assocresp_ies_len;

		ie_ptr = kmalloc(ie_len, GFP_ATOMIC);
		if (ie_ptr == NULL)
			return -EINVAL;

		memcpy(ie_ptr, beacon->assocresp_ies, ie_len);
		wiphy_dbg(priv->wiphy,
			  "begin send associate response extra ies\n");

		ret = sprdwl_set_p2p_ie_cmd(priv->sipc,
					    P2P_ASSOCRESP_IE, ie_ptr, ie_len);
		if (ret) {
			wiphy_err(priv->wiphy,
				  "%s Failed to set p2p_ie of assocresp_ies!\n",
				  __func__);
		} else {
			wiphy_dbg(priv->wiphy,
				  "send associate response ies successfully\n");
		}

		kfree(ie_ptr);
	}
#endif /*CONFIG_SPRDWL_WIFI_DIRECT */
	return ret;
}

static int sprdwl_cfg80211_start_ap(struct wiphy *wiphy,
				    struct net_device *ndev,
				    struct cfg80211_ap_settings *info)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = netdev_priv(ndev);
	struct cfg80211_beacon_data *beacon = &info->beacon;
	struct ieee80211_mgmt *mgmt;
	u16 mgmt_len;
	int ret;

	wiphy_info(wiphy, "%s\n", __func__);

	if (!sprdwl_is_ready(priv))
		return -EIO;

	if (info->ssid == NULL) {
		wiphy_err(wiphy, "%s null ssid!\n", __func__);
		return -EINVAL;
	}

	memcpy(vif->ssid, info->ssid, info->ssid_len);
	if (info->ssid_len <= IEEE80211_MAX_SSID_LEN)
		vif->ssid[info->ssid_len] = 0;
	vif->ssid_len = info->ssid_len;

	ret = sprdwl_set_ap_sme_cmd(priv->sipc, 0);
	if (ret) {
		wiphy_err(wiphy, "%s Failed to set ap sme cmd!\n", __func__);
		return ret;
	}
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	if (!netif_carrier_ok(ndev)) {
		netif_carrier_on(ndev);
		netif_wake_queue(ndev);
	}
	ret = sprdwl_set_p2p_enable_cmd(priv->sipc, vif == priv->p2p_vif);
	if (ret) {
		wiphy_err(wiphy, "%s Failed to set p2p_enable!\n", __func__);
		return ret;
	}
#endif

	/* TODO:
	 * info->interval
	 * info->dtim_period
	 */
	sprdwl_change_beacon(vif, beacon);

	if (beacon->head == NULL)
		return -EINVAL;

	mgmt_len = beacon->head_len;
	if (beacon->tail)
		mgmt_len += beacon->tail_len;

	mgmt = kmalloc(mgmt_len, GFP_ATOMIC);
	if (mgmt == NULL)
		return -EINVAL;

	memcpy((u8 *)mgmt, beacon->head, beacon->head_len);
	if (beacon->tail)
		memcpy((u8 *)mgmt + beacon->head_len,
		       beacon->tail, beacon->tail_len);

	/* auth type */
	/* akm suites */
	/* ciphers_pairwise */
	/* ciphers_group */
	/* ssid */
	ret = sprdwl_start_ap_cmd(priv->sipc, (u8 *)mgmt, mgmt_len);
	kfree(mgmt);
	if (ret != 0)
		wiphy_err(wiphy, "%s Failed to start AP!\n", __func__);

	return ret;
}

static int sprdwl_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *ndev)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);

	wiphy_info(wiphy, "%s\n", __func__);

	if (!sprdwl_is_ready(priv))
		return -EIO;

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	if (netif_carrier_ok(ndev)) {
		netif_carrier_off(ndev);
		netif_stop_queue(ndev);
	}
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */

	return 0;
}

static int sprdwl_cfg80211_change_beacon(struct wiphy *wiphy,
					 struct net_device *ndev,
					 struct cfg80211_beacon_data *beacon)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = netdev_priv(ndev);

	if (!sprdwl_is_ready(priv))
		return -EIO;

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	wiphy_info(wiphy, "%s\n", __func__);
	return sprdwl_change_beacon(vif, beacon);
#endif /* CONFIG_SPRDWL_WIFI_DIRECT */
	return 0;
}

static int sprdwl_cfg80211_del_station(struct wiphy *wiphy,
				       struct net_device *ndev, u8 *mac)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);

	if (!sprdwl_is_ready(priv))
		return -EIO;

	if (!mac) {
		wiphy_dbg(wiphy, "Ignore NULL MAC address!\n");
		goto out;
	}

	wiphy_info(wiphy, "%s %pM\n", __func__, mac);
	sprdwl_disassoc_cmd(priv->sipc, mac, WLAN_REASON_DEAUTH_LEAVING);
out:
	return 0;
}

static int sprdwl_cfg80211_set_channel(struct wiphy *wiphy,
				       struct net_device *ndev,
				       struct ieee80211_channel *channel)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	int ret = -ENOTSUPP;

	if (!sprdwl_is_ready(priv))
		return -EIO;

	/*FIXME: To be handled properly when monitor mode is supported*/
	ret =
	    sprdwl_set_channel_cmd(priv->sipc,
				   ieee80211_frequency_to_channel
				   (channel->center_freq));
	if (ret < 0) {
		wiphy_err(wiphy, "%s Failed to set channel!\n", __func__);
		return ret;
	}

	return 0;
}

/*static int sprdwl_cfg80211_set_power_mgmt(struct wiphy *wiphy,
					  struct net_device *dev,
					  bool pmgmt, int timeout)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	int ret;

	if (!sprdwl_is_ready(priv))
		return -EIO;

	if (pmgmt) {
		ret = sprdwl_pm_enter_ps_cmd(priv->sipc);
		if (ret < 0) {
			wiphy_err(wiphy,
				"sprdwl_pm_enter_ps_cmd failed(%d)\n", ret);
			return ret;
		}
	} else {
		ret = sprdwl_pm_exit_ps_cmd(priv->sipc);
		if (ret < 0) {
			wiphy_err(wiphy,
				"sprdwl_pm_exit_ps_cmd failed(%d)\n", ret);
			return ret;
		}
	}

	return 0;
}
*/
static int sprdwl_cfg80211_mgmt_tx(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   struct ieee80211_channel *chan,
				   bool offchan, unsigned int wait,
				   const u8 *buf, size_t len, bool no_cck,
				   bool dont_wait_for_ack, u64 *cookie)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	int ret = 0;
	static u64 mgmt_index;

	mgmt_index++;
	wiphy_info(wiphy, "%s, index: %lld, cookie: %lld\n", __func__,
		   mgmt_index, *cookie);
	*cookie = mgmt_index;

	if (!sprdwl_is_ready(priv))
		return -EIO;
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	cfg80211_dump_frame_prot_info(wiphy, 1, chan->center_freq, buf, len);

	/* send tx mgmt */
	if (len > 0) {
		ret = sprdwl_set_tx_mgmt_cmd(priv->sipc,
					     chan, dont_wait_for_ack,
					     wait, cookie, buf, len);
		if (ret) {
			if (dont_wait_for_ack == false)
				cfg80211_mgmt_tx_status(wdev, *cookie, buf, len,
							0, GFP_KERNEL);

			wiphy_err(wiphy,
				  "%s Failed to set tx mgmt!\n", __func__);
			return ret;
		}
	}
#endif /*CONFIG_SPRDWL_WIFI_DIRECT */

	return ret;
}

static void sprdwl_cfg80211_mgmt_frame_register(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						u16 frame_type, bool reg)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	struct sprdwl_vif *vif = netdev_priv(wdev->netdev);
#endif

	if (!sprdwl_is_ready(priv))
		return;

#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	/*wiphy_dbg(wiphy, "%s frame_type %d, reg %d\n", __func__,
		  frame_type, reg);
	*/

	vif->frame_type = frame_type;
	vif->reg = reg;
	schedule_work(&vif->work);
#endif
}

#ifdef CONFIG_SPRDWL_WIFI_DIRECT

static int sprdwl_cfg80211_remain_on_channel(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     struct ieee80211_channel
					     *channel, unsigned int duration,
					     u64 *cookie)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = netdev_priv(wdev->netdev);
	enum nl80211_channel_type channel_type = 0;
	int ret;

	wiphy_info(wiphy, "%s %d for %dms. cookie=0x%llx\n",
		   __func__, channel->center_freq, duration, *cookie);
	if (!sprdwl_is_ready(priv))
		return -EIO;

	memcpy(&vif->listen_channel, channel, sizeof(struct ieee80211_channel));
	vif->listen_cookie = *cookie;

	/* send remain chan */

	ret = sprdwl_remain_chan_cmd(priv->sipc, channel,
				     channel_type, duration, cookie);
	if (ret) {
		wiphy_err(wiphy, "%s Failed to remain chan!\n", __func__);
		return ret;
	}
	/* report remain chan */
	cfg80211_ready_on_channel(wdev, *cookie, channel, duration, GFP_KERNEL);
	return 0;
}

static int sprdwl_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
						    struct wireless_dev *wdev,
						    u64 cookie)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	int ret;

	wiphy_info(wiphy, "%s cookie = 0x%llx.\n", __func__, cookie);

	if (!sprdwl_is_ready(priv))
		return -EIO;

	ret = sprdwl_cancel_remain_chan_cmd(priv->sipc, cookie);
	if (ret) {
		wiphy_err(wiphy,
			  "%s Failed to cancel remain chan!\n", __func__);
		return ret;
	}

	return 0;
}

void sprdwl_event_mgmt_deauth(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	u8 *mac_ptr, *index;
	u16 mac_len;

	index = priv->sipc->event_buf->u.event.variable;
	memcpy(&mac_len, index, sizeof(mac_len));
	index += sizeof(mac_len);
	mac_ptr = index;

	cfg80211_dump_frame_prot_info(wiphy, 0, 0, mac_ptr, mac_len);

	cfg80211_send_deauth(vif->ndev, mac_ptr, mac_len);
}

void sprdwl_event_mgmt_disassoc(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	u8 *mac_ptr, *index;
	u16 mac_len;

	index = priv->sipc->event_buf->u.event.variable;
	memcpy(&mac_len, index, sizeof(mac_len));
	index += sizeof(mac_len);
	mac_ptr = index;

	cfg80211_dump_frame_prot_info(wiphy, 0, 0, mac_ptr, mac_len);

	cfg80211_send_disassoc(vif->ndev, mac_ptr, mac_len);
}

void sprdwl_event_remain_on_channel_expired(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	u8 *index = NULL;
	u8 chan_type;
	u16 type_len;
	u32 event_len;
	int left;

	wiphy_info(wiphy, "%s\n", __func__);

	event_len = priv->sipc->event_len;

	index = priv->sipc->event_buf->u.event.variable;
	left = event_len;

	/* The first byte of event data is cookie */
	memcpy(&type_len, index, sizeof(type_len));
	index += sizeof(type_len);
	left -= sizeof(type_len);

	if (type_len > 1) {
		wiphy_err(wiphy, "%s wrong type len!\n", __func__);
		return;
	}

	memcpy(&chan_type, index, type_len);
	index += type_len;
	left -= type_len;

	cfg80211_remain_on_channel_expired(&vif->wdev, vif->listen_cookie,
					   &vif->listen_channel, GFP_KERNEL);
}

void sprdwl_event_report_frame(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	u16 mac_len;
	u8 *mac_ptr = NULL;
	u8 channel = 0, type = 0;
	int freq;

	struct wlan_sipc_event_report_frame *report_frame = NULL;

	report_frame =
	    (struct wlan_sipc_event_report_frame *)priv->sipc->
	    event_buf->u.event.variable;
	channel = report_frame->channel;
	type = report_frame->frame_type;
	freq = ieee80211_channel_to_frequency(channel, IEEE80211_BAND_2GHZ);
	mac_ptr = (u8 *)(report_frame + 1);
	mac_len = report_frame->frame_len;

	cfg80211_dump_frame_prot_info(wiphy, 0, freq, mac_ptr, mac_len);

	cfg80211_rx_mgmt(&vif->wdev, freq, 0, mac_ptr, mac_len - FCS_LEN,
			 GFP_ATOMIC);
}

void sprdwl_event_mlme_tx_status(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct wlan_sipc_event_mgmt_tx_status *tx_status = NULL;

	tx_status = (struct wlan_sipc_event_mgmt_tx_status *)priv->
		sipc->event_buf->u.event.variable;

	wiphy_info(wiphy, "%s index: %lld\n", __func__, tx_status->cookie);

	cfg80211_mgmt_tx_status(&vif->wdev, tx_status->cookie, tx_status->buf,
				tx_status->len, tx_status->ack, GFP_KERNEL);
}

#endif /*CONFIG_SPRDWL_WIFI_DIRECT */

/* CFG802.11 operation handler for connection quality monitoring.
 *
 * This function subscribes/unsubscribes HIGH_RSSI and LOW_RSSI
 * events to FW.
 */
int sprdwl_cfg80211_set_cqm_rssi_config(struct wiphy *wiphy,
					struct net_device *ndev,
					s32 rssi_thold, u32 rssi_hyst)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	int ret;

	wiphy_info(wiphy, "%s rssi_thold %d rssi_hyst %d",
		   __func__, rssi_thold, rssi_hyst);

	ret = sprdwl_set_cqm_rssi(priv->sipc, rssi_thold, rssi_hyst);

	return ret;
}

static struct cfg80211_ops sprdwl_cfg80211_ops = {
	.add_virtual_intf = sprdwl_cfg80211_add_iface,
	.del_virtual_intf = sprdwl_cfg80211_del_iface,
	.change_virtual_intf = sprdwl_cfg80211_change_iface,
	.scan = sprdwl_cfg80211_scan,
	.connect = sprdwl_cfg80211_connect,
	.disconnect = sprdwl_cfg80211_disconnect,
	.add_key = sprdwl_cfg80211_add_key,
	.del_key = sprdwl_cfg80211_del_key,
	.set_default_key = sprdwl_cfg80211_set_default_key,
	.set_wiphy_params = sprdwl_cfg80211_set_wiphy_params,
	.get_station = sprdwl_cfg80211_get_station,
	.set_pmksa = sprdwl_cfg80211_set_pmksa,
	.del_pmksa = sprdwl_cfg80211_del_pmksa,
	.flush_pmksa = sprdwl_cfg80211_flush_pmksa,
	.set_cqm_rssi_config = sprdwl_cfg80211_set_cqm_rssi_config,
	/* AP mode */
	.start_ap = sprdwl_cfg80211_start_ap,
	.change_beacon = sprdwl_cfg80211_change_beacon,
	.stop_ap = sprdwl_cfg80211_stop_ap,
	.del_station = sprdwl_cfg80211_del_station,
	.libertas_set_mesh_channel = sprdwl_cfg80211_set_channel,
	.mgmt_tx = sprdwl_cfg80211_mgmt_tx,
	.mgmt_frame_register = sprdwl_cfg80211_mgmt_frame_register,
#ifdef CONFIG_SPRDWL_WIFI_DIRECT
	.remain_on_channel = sprdwl_cfg80211_remain_on_channel,
	.cancel_remain_on_channel = sprdwl_cfg80211_cancel_remain_on_channel,
#endif /*CONFIG_SPRDWL_WIFI_DIRECT */
};

static void sprdwl_reg_notify(struct wiphy *wiphy,
			      struct regulatory_request *request)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *chan;
	const struct ieee80211_freq_range *freq_range;
	const struct ieee80211_reg_rule *reg_rule;
	struct sprdwl_ieee80211_regdomain *rd = NULL;
	u32 band, channel, i;
	u32 last_start_freq;
	u32 n_rules = 0, rd_size;

	wiphy_info(wiphy, "%s %c%c initiator %d hint_type %d\n", __func__,
		   request->alpha2[0], request->alpha2[1],
		   request->initiator, request->user_reg_hint_type);

	/* Figure out the actual rule number */
	for (band = 0; band < IEEE80211_NUM_BANDS; band++) {
		sband = wiphy->bands[band];
		if (!sband)
			continue;

		last_start_freq = 0;
		for (channel = 0; channel < sband->n_channels; channel++) {
			chan = &sband->channels[channel];

			reg_rule =
			    freq_reg_info(wiphy, MHZ_TO_KHZ(chan->center_freq));
			if (IS_ERR(reg_rule))
				continue;

			freq_range = &reg_rule->freq_range;
			if (last_start_freq != freq_range->start_freq_khz) {
				last_start_freq = freq_range->start_freq_khz;
				n_rules++;
			}
		}
	}

	rd_size = sizeof(struct sprdwl_ieee80211_regdomain) +
	    n_rules * sizeof(struct ieee80211_reg_rule);

	rd = kzalloc(rd_size, GFP_KERNEL);
	if (!rd) {
		wiphy_err(wiphy,
			  "%s Failed to allocate sprdwl_ieee80211_regdomain!\n",
			  __func__);
		return;
	}

	/* Fill regulatory domain */
	rd->n_reg_rules = n_rules;
	memcpy(rd->alpha2, request->alpha2, ARRAY_SIZE(rd->alpha2));
	for (band = 0; band < IEEE80211_NUM_BANDS; band++) {
		sband = wiphy->bands[band];
		if (!sband)
			continue;

		last_start_freq = 0;
		for (channel = i = 0; channel < sband->n_channels; channel++) {
			chan = &sband->channels[channel];

			if (chan->flags & IEEE80211_CHAN_DISABLED)
				continue;

			reg_rule =
			    freq_reg_info(wiphy, MHZ_TO_KHZ(chan->center_freq));
			if (IS_ERR(reg_rule))
				continue;

			freq_range = &reg_rule->freq_range;
			if (last_start_freq != freq_range->start_freq_khz &&
			    i < n_rules) {
				last_start_freq = freq_range->start_freq_khz;
				memcpy(&rd->reg_rules[i], reg_rule,
				       sizeof(struct ieee80211_reg_rule));
				i++;
				wiphy_dbg(wiphy,
					  "   %d KHz - %d KHz @ %d KHz flags %#x\n",
					  freq_range->start_freq_khz,
					  freq_range->end_freq_khz,
					  freq_range->max_bandwidth_khz,
					  reg_rule->flags);
			}
		}
	}

	if (sprdwl_set_regdom_cmd(priv->sipc, (u8 *)rd, rd_size))
		wiphy_err(wiphy, "%s Failed to set regdomain!\n", __func__);
	kfree(rd);
}

static const struct ieee80211_iface_limit sprdwl_iface_limits[] = {
	{
		.max = 2,
		.types = BIT(NL80211_IFTYPE_STATION) |
			 BIT(NL80211_IFTYPE_AP)
	},
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_P2P_CLIENT) |
			 BIT(NL80211_IFTYPE_P2P_GO)
	},
	{
		.max = 1,
		.types = BIT(NL80211_IFTYPE_P2P_DEVICE)
	}
};

static const struct ieee80211_iface_combination sprdwl_iface_combos[] = {
	{
		 .max_interfaces = 3,
		 .num_different_channels = 2,
		 .n_limits = ARRAY_SIZE(sprdwl_iface_limits),
		 .limits = sprdwl_iface_limits
	}
};

/*Init wiphy parameters*/
static void sprdwl_setup_wiphy(struct wiphy *wiphy)
{
	wiphy->mgmt_stypes = sprdwl_mgmt_stypes;
	wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION)
	    | BIT(NL80211_IFTYPE_AP);

	/*Default not in powersave state */
	wiphy->flags &= ~WIPHY_FLAG_PS_ON_BY_DEFAULT;
	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;

	wiphy->max_scan_ssids = MAX_SITES_FOR_SCAN;
	wiphy->max_scan_ie_len = SCAN_IE_LEN_MAX;
	wiphy->max_num_pmkids = MAX_NUM_PMKIDS;

	/*Attach cipher suites */
	wiphy->n_cipher_suites = ARRAY_SIZE(sprdwl_cipher_suites);
	wiphy->cipher_suites = sprdwl_cipher_suites;
#ifdef CONFIG_PM
	/*Set WoWLAN flags */
	wiphy->wowlan.flags = WIPHY_WOWLAN_ANY | WIPHY_WOWLAN_DISCONNECT;
#endif
	/*Attach bands */
	wiphy->bands[IEEE80211_BAND_2GHZ] = &sprdwl_band_2ghz;
	/*wiphy->bands[IEEE80211_BAND_5GHZ] = &sprdwl_band_5ghz;*/

	wiphy->reg_notifier = sprdwl_reg_notify;

	wiphy->interface_modes |= BIT(NL80211_IFTYPE_P2P_DEVICE) |
	    BIT(NL80211_IFTYPE_P2P_CLIENT) | BIT(NL80211_IFTYPE_P2P_GO);
	wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;
	/* set AP SME flag, also needed by STA mode? */
	wiphy->flags |= WIPHY_FLAG_HAVE_AP_SME;
	wiphy->ap_sme_capa = 1;
	wiphy->max_remain_on_channel_duration = 5000;

	wiphy->iface_combinations = sprdwl_iface_combos;
	wiphy->n_iface_combinations = ARRAY_SIZE(sprdwl_iface_combos);
}

struct wiphy *sprdwl_register_wiphy(struct device *dev)
{
	struct wiphy *wiphy;
	struct sprdwl_priv *priv;
	int ret;

	/*Allocate wiphy */
	wiphy = wiphy_new(&sprdwl_cfg80211_ops, sizeof(struct sprdwl_priv));
	if (wiphy == NULL) {
		pr_err("Failed to allocate wiphy!\n");
		goto err;
	}

	priv = wiphy_priv(wiphy);
	priv->wiphy = wiphy;
	set_wiphy_dev(wiphy, dev);

	/*Init wiphy parameters */
	sprdwl_setup_wiphy(wiphy);

	/*Init wiphy_priv */
	spin_lock_init(&priv->list_lock);
	INIT_LIST_HEAD(&priv->vif_list);
	/* FIXME it will be modify when cp2 code ready */
	priv->cp2_status = SPRDWL_READY;
	atomic_set(&priv->stopped, 0);
	atomic_set(&priv->tx_free, TX_SBLOCK_NUM);
	mutex_init(&priv->global_lock);

	ret = sprdwl_alloc_sipc_buf(priv);
	if (ret) {
		pr_err("%s Failed to allocate sipc buf!\n", __func__);
		goto err_alloc_sipc;
	}

	/*register wiphy */
	ret = wiphy_register(wiphy);
	if (ret < 0) {
		wiphy_err(wiphy, "Failed to regitster wiphy(%d)!\n", ret);
		goto err_wiphy_register;
	}
	return wiphy;

err_wiphy_register:
	sprdwl_free_sipc_buf(priv);
err_alloc_sipc:
	wiphy_free(wiphy);
err:
	return NULL;
}

void sprdwl_unregister_wiphy(struct wiphy *wiphy)
{
	if (wiphy == NULL)
		return;

	wiphy_unregister(wiphy);
	sprdwl_free_sipc_buf(wiphy_priv(wiphy));
	wiphy_free(wiphy);
}
