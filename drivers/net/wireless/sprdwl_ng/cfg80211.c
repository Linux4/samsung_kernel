/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Abstract : This file is an implementation for cfg80211 subsystem
 *
 * Authors:
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
 * Jingxiang Li <Jingxiang.li@spreadtrum.com>
 * Dong Xiang <dong.xiang@spreadtrum.com>
 * Huiquan Zhou <huiquan.zhou@spreadtrum.com>
 * Baolei Yuan <baolei.yuan@spreadtrum.com>
 * Xianwei Zhao <xianwei.zhao@spreadtrum.com>
 * Gui Zhu <gui.zhu@spreadtrum.com>
 * Andy He <andy.he@spreadtrum.com>
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

#include "sprdwl.h"
#include "cfg80211.h"
#include "cmdevt.h"
#include "work.h"

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

static struct ieee80211_supported_band sprdwl_band_2ghz = {
	.n_channels = ARRAY_SIZE(sprdwl_2ghz_channels),
	.channels = sprdwl_2ghz_channels,
	.n_bitrates = SPRDWL_G_RATE_NUM,
	.bitrates = sprdwl_g_rates,
	.ht_cap.cap = sprdwl_g_htcap,
	.ht_cap.ht_supported = true,
};

static struct ieee80211_channel sprdwl_5ghz_channels[] = {
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

static struct ieee80211_supported_band sprdwl_band_5ghz = {
	.n_channels = ARRAY_SIZE(sprdwl_5ghz_channels),
	.channels = sprdwl_5ghz_channels,
	.n_bitrates = SPRDWL_A_RATE_NUM,
	.bitrates = sprdwl_a_rates,
	.ht_cap.cap = sprdwl_g_htcap,
	.ht_cap.ht_supported = true,
};

static const u32 sprdwl_cipher_suites[] = {
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,
	WLAN_CIPHER_SUITE_SMS4,
	/* required by ieee802.11w */
	WLAN_CIPHER_SUITE_AES_CMAC,
	WLAN_CIPHER_SUITE_PMK,
};

/* Supported mgmt frame types to be advertised to cfg80211 */
static const struct ieee80211_txrx_stypes
sprdwl_mgmt_stypes[NUM_NL80211_IFTYPES] = {
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
	[NL80211_IFTYPE_P2P_DEVICE] = {
		.tx = 0xffff,
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		      BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
	},
};

static const struct ieee80211_iface_limit sprdwl_iface_limits[] = {
	{
		.max = 1,
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

static void sprdwl_cancel_scan(struct sprdwl_vif *vif);
static void sprdwl_cancel_sched_scan(struct sprdwl_vif *vif);
/* Interface related stuff*/
inline void sprdwl_put_vif(struct sprdwl_vif *vif)
{
	if (vif) {
		spin_lock_bh(&vif->priv->list_lock);
		vif->ref--;
		spin_unlock_bh(&vif->priv->list_lock);
	}
}

inline struct sprdwl_vif *mode_to_vif(struct sprdwl_priv *priv, u8 vif_mode)
{
	struct sprdwl_vif *vif, *found = NULL;

	spin_lock_bh(&priv->list_lock);
	list_for_each_entry(vif, &priv->vif_list, vif_node) {
		if (vif->mode == vif_mode) {
			vif->ref++;
			found = vif;
			break;
		}
	}
	spin_unlock_bh(&priv->list_lock);

	return found;
}

static inline enum sprdwl_mode type_to_mode(enum nl80211_iftype type)
{
	enum sprdwl_mode mode;

	switch (type) {
	case NL80211_IFTYPE_STATION:
		mode = SPRDWL_MODE_STATION;
		break;
	case NL80211_IFTYPE_AP:
		mode = SPRDWL_MODE_AP;
		break;
	case NL80211_IFTYPE_P2P_GO:
		mode = SPRDWL_MODE_P2P_GO;
		break;
	case NL80211_IFTYPE_P2P_CLIENT:
		mode = SPRDWL_MODE_P2P_CLIENT;
		break;
	case NL80211_IFTYPE_P2P_DEVICE:
		mode = SPRDWL_MODE_P2P_DEVICE;
		break;
	default:
		mode = SPRDWL_MODE_NONE;
		break;
	}

	return mode;
}

int sprdwl_init_fw(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	enum nl80211_iftype type = vif->wdev.iftype;
	enum sprdwl_mode mode;
	u8 *mac;

	netdev_info(vif->ndev, "%s type %d, mode %d\n", __func__, type,
		    vif->mode);

	if (vif->mode != SPRDWL_MODE_NONE) {
		netdev_err(vif->ndev, "%s already in use: mode %d\n",
			   __func__, vif->mode);
		return -EBUSY;
	}

	mode = type_to_mode(type);
	if ((mode <= SPRDWL_MODE_NONE) || (mode >= SPRDWL_MODE_MAX)) {
		netdev_err(vif->ndev, "%s unsupported interface type: %d\n",
			   __func__, type);
		return -EINVAL;
	}

	if (priv->fw_stat[mode] == SPRDWL_INTF_OPEN) {
		netdev_err(vif->ndev, "%s mode %d already opened\n",
			   __func__, mode);
		return 0;
	}

	vif->mode = mode;
	if (!vif->ndev)
		mac = vif->wdev.address;
	else
		mac = vif->ndev->dev_addr;

	if (sprdwl_open_fw(priv, vif->mode, vif->mode, mac)) {
		netdev_err(vif->ndev, "%s failed!\n", __func__);
		vif->mode = SPRDWL_MODE_NONE;
		return -EIO;
	}
	priv->fw_stat[vif->mode] = SPRDWL_INTF_OPEN;

	return 0;
}

int sprdwl_uninit_fw(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;

	if ((vif->mode <= SPRDWL_MODE_NONE) || (vif->mode >= SPRDWL_MODE_MAX)) {
		netdev_err(vif->ndev, "%s invalid operation mode: %d\n",
			   __func__, vif->mode);
		return -EINVAL;
	}

	if (priv->fw_stat[vif->mode] == SPRDWL_INTF_CLOSE) {
		netdev_err(vif->ndev, "%s mode %d already closed\n",
			   __func__, vif->mode);
		return -EBUSY;
	}

	if (sprdwl_close_fw(priv, vif->mode, vif->mode)) {
		netdev_err(vif->ndev, "%s failed!\n", __func__);
		return -EIO;
	}

	priv->fw_stat[vif->mode] = SPRDWL_INTF_CLOSE;

	vif->mode = SPRDWL_MODE_NONE;
	netdev_info(vif->ndev, "%s type %d, mode %d\n", __func__,
		    vif->wdev.iftype, vif->mode);

	return 0;
}

static inline int sprdwl_is_valid_iftype(struct wiphy *wiphy,
					 enum nl80211_iftype type)
{
	return wiphy->interface_modes & BIT(type);
}

static struct wireless_dev *sprdwl_cfg80211_add_iface(struct wiphy *wiphy,
						      const char *name,
						      enum nl80211_iftype type,
						      u32 *flags,
						      struct vif_params *params)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);

	if (!sprdwl_is_valid_iftype(wiphy, type)) {
		wiphy_err(wiphy, "%s unsupported interface type: %u\n",
			  __func__, type);
		return ERR_PTR(-EINVAL);
	}

	return sprdwl_add_iface(priv, name, type, params->macaddr);
}

static int sprdwl_cfg80211_del_iface(struct wiphy *wiphy,
				     struct wireless_dev *wdev)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = container_of(wdev, struct sprdwl_vif, wdev);

	spin_lock_bh(&priv->list_lock);
	list_del(&vif->vif_node);
	spin_unlock_bh(&priv->list_lock);

	return sprdwl_del_iface(priv, vif);
}

static int sprdwl_cfg80211_change_iface(struct wiphy *wiphy,
					struct net_device *ndev,
					enum nl80211_iftype type, u32 *flags,
					struct vif_params *params)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	enum nl80211_iftype old_type = vif->wdev.iftype;
	int ret;

	netdev_info(ndev, "%s type %d -> %d\n", __func__, old_type, type);

	if (!sprdwl_is_valid_iftype(wiphy, type)) {
		wiphy_err(wiphy, "%s unsupported interface type: %u\n",
			  __func__, type);
		return -EOPNOTSUPP;
	}

	ret = sprdwl_uninit_fw(vif);
	if (!ret) {
		vif->wdev.iftype = type;
		ret = sprdwl_init_fw(vif);
		if (ret)
			vif->wdev.iftype = old_type;
	}

	return ret;
}

static inline u8 sprdwl_parse_akm(u32 akm)
{
	u8 ret;

	switch (akm) {
	case WLAN_AKM_SUITE_PSK:
		ret = SPRDWL_AKM_SUITE_PSK;
		break;
	case WLAN_AKM_SUITE_8021X:
		ret = SPRDWL_AKM_SUITE_8021X;
		break;
	case WLAN_AKM_SUITE_FT_PSK:
		ret = SPRDWL_AKM_SUITE_FT_PSK;
		break;
	case WLAN_AKM_SUITE_FT_8021X:
		ret = SPRDWL_AKM_SUITE_FT_8021X;
		break;
	case WLAN_AKM_SUITE_WAPI_PSK:
		ret = SPRDWL_AKM_SUITE_WAPI_PSK;
		break;
	case WLAN_AKM_SUITE_WAPI_CERT:
		ret = SPRDWL_AKM_SUITE_WAPI_CERT;
		break;
	case WLAN_AKM_SUITE_PSK_SHA256:
		ret = SPRDWL_AKM_SUITE_PSK_SHA256;
		break;
	case WLAN_AKM_SUITE_8021X_SHA256:
		ret = SPRDWL_AKM_SUITE_8021X_SHA256;
		break;
	default:
		ret = SPRDWL_AKM_SUITE_NONE;
		break;
	}

	return ret;
}

/* Encryption related stuff */
static inline u8 sprdwl_parse_cipher(u32 cipher)
{
	u8 ret;

	switch (cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
		ret = SPRDWL_CIPHER_WEP40;
		break;
	case WLAN_CIPHER_SUITE_WEP104:
		ret = SPRDWL_CIPHER_WEP104;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		ret = SPRDWL_CIPHER_TKIP;
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		ret = SPRDWL_CIPHER_CCMP;
		break;
		/* WAPI cipher is not processed by firmware */
	case WLAN_CIPHER_SUITE_SMS4:
		ret = SPRDWL_CIPHER_WAPI;
		break;
	case WLAN_CIPHER_SUITE_AES_CMAC:
		ret = SPRDWL_CIPHER_AES_CMAC;
		break;
	default:
		ret = SPRDWL_CIPHER_NONE;
		break;
	}

	return ret;
}

static int sprdwl_add_cipher_key(struct sprdwl_vif *vif, bool pairwise,
				 u8 key_index, u32 cipher, const u8 *key_seq,
				 const u8 *mac_addr)
{
	u8 *cipher_ptr = pairwise ? &vif->prwise_crypto : &vif->grp_crypto;
	u8 pn_key[16] = { 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36,
		0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36
	};
	int ret = 0;

	netdev_info(vif->ndev, "%s %s key_index %d\n", __func__,
		    pairwise ? "pairwise" : "group", key_index);

	if (vif->key_len[pairwise][0] || vif->key_len[pairwise][1] ||
	    vif->key_len[pairwise][2] || vif->key_len[pairwise][3]) {
		*cipher_ptr = vif->prwise_crypto = sprdwl_parse_cipher(cipher);

		if (vif->prwise_crypto == SPRDWL_CIPHER_WAPI)
			memcpy(vif->key_txrsc[pairwise], pn_key,
			       sizeof(pn_key));
		ret = sprdwl_add_key(vif->priv, vif->mode,
				     vif->key[pairwise][key_index],
				     vif->key_len[pairwise][key_index],
				     pairwise, key_index, key_seq,
				     *cipher_ptr, mac_addr);
	}

	return ret;
}

static int sprdwl_cfg80211_add_key(struct wiphy *wiphy, struct net_device *ndev,
				   u8 key_index, bool pairwise,
				   const u8 *mac_addr,
				   struct key_params *params)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	vif->key_index[pairwise] = key_index;
	vif->key_len[pairwise][key_index] = params->key_len;
	memcpy(vif->key[pairwise][key_index], params->key, params->key_len);

	/* PMK is for Roaming offload */
	if (params->cipher == WLAN_CIPHER_SUITE_PMK)
		return sprdwl_set_roam_offload(vif->priv, vif->mode,
					       SPRDWL_ROAM_OFFLOAD_SET_PMK,
					       params->key, params->key_len);
	else
		return sprdwl_add_cipher_key(vif, pairwise, key_index,
					     params->cipher, params->seq,
					     mac_addr);
}

static int sprdwl_cfg80211_del_key(struct wiphy *wiphy, struct net_device *ndev,
				   u8 key_index, bool pairwise,
				   const u8 *mac_addr)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	netdev_info(ndev, "%s key_index=%d, pairwise=%d\n",
		    __func__, key_index, pairwise);

	if (key_index > SPRDWL_MAX_KEY_INDEX) {
		netdev_err(ndev, "%s key index %d out of bounds!\n", __func__,
			   key_index);
		return -ENOENT;
	}

	if (!vif->key_len[pairwise][key_index]) {
		netdev_err(ndev, "%s key index %d is empty!\n", __func__,
			   key_index);
		return 0;
	}

	vif->key_len[pairwise][key_index] = 0;
	vif->prwise_crypto = SPRDWL_CIPHER_NONE;
	vif->grp_crypto = SPRDWL_CIPHER_NONE;

	return sprdwl_del_key(vif->priv, vif->mode, key_index, mac_addr);
}

static int sprdwl_cfg80211_set_default_key(struct wiphy *wiphy,
					   struct net_device *ndev,
					   u8 key_index, bool unicast,
					   bool multicast)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	if (key_index > 3) {
		netdev_err(ndev, "%s invalid key index: %d\n", __func__,
			   key_index);
		return -EINVAL;
	}

	return sprdwl_set_def_key(vif->priv, vif->mode, key_index);
}

/* SoftAP related stuff */
static int sprdwl_change_beacon(struct sprdwl_vif *vif,
				struct cfg80211_beacon_data *beacon)
{
	int ret = 0;

	if (!beacon)
		return -EINVAL;

	if (beacon->beacon_ies_len) {
		netdev_dbg(vif->ndev, "set beacon extra IE\n");
		ret = sprdwl_set_ie(vif->priv, vif->mode, SPRDWL_IE_BEACON,
				    beacon->beacon_ies, beacon->beacon_ies_len);
	}

	if (beacon->proberesp_ies_len) {
		netdev_dbg(vif->ndev, "set probe response extra IE\n");
		ret = sprdwl_set_ie(vif->priv, vif->mode, SPRDWL_IE_PROBE_RESP,
				    beacon->proberesp_ies,
				    beacon->proberesp_ies_len);
	}

	if (beacon->assocresp_ies_len) {
		netdev_dbg(vif->ndev, "set associate response extra IE\n");
		ret = sprdwl_set_ie(vif->priv, vif->mode, SPRDWL_IE_ASSOC_RESP,
				    beacon->assocresp_ies,
				    beacon->assocresp_ies_len);
	}

	if (ret)
		netdev_err(vif->ndev, "%s failed\n", __func__);

	return ret;
}

static int sprdwl_cfg80211_start_ap(struct wiphy *wiphy,
				    struct net_device *ndev,
				    struct cfg80211_ap_settings *settings)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	struct cfg80211_beacon_data *beacon = &settings->beacon;
	struct ieee80211_mgmt *mgmt;
	u16 mgmt_len, index = 0;
	u8 *data = NULL;
	int ret;

	netdev_info(ndev, "%s\n", __func__);

	if (!settings->ssid) {
		netdev_err(ndev, "%s invalid SSID!\n", __func__);
		return -EINVAL;
	}
	strncpy(vif->ssid, settings->ssid, settings->ssid_len);
	vif->ssid_len = settings->ssid_len;

	sprdwl_change_beacon(vif, beacon);

	if (!beacon->head)
		return -EINVAL;

	mgmt_len = beacon->head_len;
	/* add 1 byte for hidden ssid flag */
	mgmt_len += 1;

	if (beacon->tail)
		mgmt_len += beacon->tail_len;

	mgmt = kmalloc(mgmt_len, GFP_KERNEL);
	if (!mgmt)
		return -ENOMEM;
	data = (u8 *)mgmt;

#define SSID_LEN_OFFSET		(37)
	memcpy(data, beacon->head, SSID_LEN_OFFSET);
	index += SSID_LEN_OFFSET;
	/* modify ssid_len */
	*(data + index) = (u8)(settings->ssid_len + 1);
	index += 1;
	/* copy ssid */
	strncpy(data + index, settings->ssid, settings->ssid_len);
	index += settings->ssid_len;
	/* set hidden ssid flag */
	*(data + index) = (u8)settings->hidden_ssid;
	index += 1;

	/* cope left settings */
	memcpy(data + index, beacon->head + index - 1,
	       beacon->head_len + 1 - index);

	if (beacon->tail)
		memcpy(data + beacon->head_len + 1,
		       beacon->tail, beacon->tail_len);

	ret = sprdwl_start_ap(vif->priv, vif->mode, (unsigned char *)mgmt,
			      mgmt_len);
	kfree(mgmt);
	if (ret)
		netdev_err(ndev, "%s failed to start AP!\n", __func__);

	return ret;
}

static int sprdwl_cfg80211_change_beacon(struct wiphy *wiphy,
					 struct net_device *ndev,
					 struct cfg80211_beacon_data *info)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	netdev_info(ndev, "%s\n", __func__);

	return sprdwl_change_beacon(vif, info);
}

static int sprdwl_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *ndev)
{
	netdev_info(ndev, "%s\n", __func__);

	return 0;
}

static int sprdwl_cfg80211_add_station(struct wiphy *wiphy,
				       struct net_device *ndev, u8 *mac,
				       struct station_parameters *params)
{
	return 0;
}

static int sprdwl_cfg80211_del_station(struct wiphy *wiphy,
				       struct net_device *ndev, u8 *mac)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	struct sprdwl_work *work;

	if (!mac) {
		netdev_dbg(ndev, "ignore NULL MAC address!\n");
		goto out;
	}
	netdev_info(ndev, "%s %pM\n", __func__, mac);

	work = sprdwl_alloc_work(ETH_ALEN);
	if (!work)
		return -ENOMEM;

	work->vif = vif;
	work->id = SPRDWL_WORK_DEL_STA;
	memcpy(work->data, mac, ETH_ALEN);

	sprdwl_queue_work(vif->priv, work);

out:
	return 0;
}

static int sprdwl_cfg80211_change_station(struct wiphy *wiphy,
					  struct net_device *ndev, u8 *mac,
					  struct station_parameters *params)
{
	return 0;
}

static int sprdwl_cfg80211_get_station(struct wiphy *wiphy,
				       struct net_device *ndev, u8 *mac,
				       struct station_info *sinfo)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	s8 signal, noise;
	u8 rate;
	s32 failed, i;
	int ret;

	sinfo->filled |= STATION_INFO_TX_BYTES |
			 STATION_INFO_TX_PACKETS |
			 STATION_INFO_RX_BYTES |
			 STATION_INFO_RX_PACKETS;
	sinfo->tx_bytes = ndev->stats.tx_bytes;
	sinfo->tx_packets = ndev->stats.tx_packets;
	sinfo->rx_bytes = ndev->stats.rx_bytes;
	sinfo->rx_packets = ndev->stats.rx_packets;

	/* Get current RSSI */
	ret = sprdwl_get_station(vif->priv, vif->mode,
				 &signal, &noise, &rate, &failed);
	if (ret)
		goto out;
	sinfo->signal = signal;
	sinfo->filled |= STATION_INFO_SIGNAL;

	sinfo->tx_failed = failed;
	sinfo->filled |= STATION_INFO_TX_BITRATE | STATION_INFO_TX_FAILED;

	/* Convert got rate from hw_value to NL80211 value */
	if (!(rate & 0x7f)) {
		netdev_info(ndev, "%s rate %d\n", __func__, (rate & 0x7f));
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

	netdev_info(ndev, "%s signal %d txrate %d\n", __func__, sinfo->signal,
		    sinfo->txrate.legacy);
out:
	return ret;
}

static int sprdwl_cfg80211_set_channel(struct wiphy *wiphy,
				       struct net_device *ndev,
				       struct ieee80211_channel *chan)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	return sprdwl_set_channel(vif->priv, vif->mode,
				  ieee80211_frequency_to_channel
				  (chan->center_freq));
}

void sprdwl_report_softap(struct sprdwl_vif *vif, u8 is_connect, u8 *addr,
			  u8 *req_ie, u16 req_ie_len)
{
	struct station_info sinfo;

	if (!addr)
		return;

	memset(&sinfo, 0, sizeof(sinfo));
	if (req_ie_len > 0) {
		sinfo.assoc_req_ies = req_ie;
		sinfo.assoc_req_ies_len = req_ie_len;
		sinfo.filled |= STATION_INFO_ASSOC_REQ_IES;
	}

	if (is_connect) {
		cfg80211_new_sta(vif->ndev, addr, &sinfo, GFP_KERNEL);
		netdev_info(vif->ndev, "New station (%pM) connected\n", addr);
	} else {
		cfg80211_del_sta(vif->ndev, addr, GFP_KERNEL);
		netdev_info(vif->ndev, "The station (%pM) disconnected\n",
			    addr);
	}
}

/* Station related stuff */
static int sprdwl_cfg80211_scan(struct wiphy *wiphy,
				struct cfg80211_scan_request *request)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif =
	    container_of(request->wdev, struct sprdwl_vif, wdev);
	struct cfg80211_ssid *ssids = request->ssids;
	struct sprdwl_scan_ssid *scan_ssids;
	u8 *ssids_ptr = NULL;
	int scan_ssids_len = 0;
	u32 channels = 0;
	unsigned int i, n;
	int ret;

	netdev_info(vif->ndev, "%s\n", __func__);

	if (!sprdwl_is_valid_iftype(wiphy, request->wdev->iftype)) {
		wiphy_err(wiphy, "%s unsupported interface type: %u\n",
			  __func__, request->wdev->iftype);
		ret = -EOPNOTSUPP;
		goto err;
	}

	/* set WPS ie */
	if (request->ie_len > 0) {
		if (request->ie_len > 255) {
			netdev_err(vif->ndev, "%s invalid len: %zu\n", __func__,
				   request->ie_len);
			ret = -EOPNOTSUPP;
			goto err;
		}

		ret = sprdwl_set_ie(priv, vif->mode, SPRDWL_IE_PROBE_REQ,
				    request->ie, request->ie_len);
		if (ret)
			goto err;
	}

	for (i = 0; i < request->n_channels; i++)
		channels |= (1 << (request->channels[i]->hw_value - 1));

	n = min(request->n_ssids, 9);
	if (n) {
		ssids_ptr = kzalloc(512, GFP_KERNEL);
		if (!ssids_ptr) {
			netdev_err(vif->ndev,
				   "%s failed to alloc scan ssids!\n",
				   __func__);
			ret = -ENOMEM;
			goto err;
		}

		scan_ssids = (struct sprdwl_scan_ssid *)ssids_ptr;
		for (i = 0; i < n; i++) {
			if (!ssids[i].ssid_len)
				continue;
			scan_ssids->len = ssids[i].ssid_len;
			strncpy(scan_ssids->ssid, ssids[i].ssid,
				ssids[i].ssid_len);
			scan_ssids_len += (ssids[i].ssid_len
					   + sizeof(scan_ssids->len));
			scan_ssids = (struct sprdwl_scan_ssid *)
			    (ssids_ptr + scan_ssids_len);
		}
	} else {
		netdev_err(vif->ndev, "%s n_ssids is 0\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	spin_lock_bh(&priv->scan_lock);
	priv->scan_request = request;
	priv->scan_vif = vif;
	spin_unlock_bh(&priv->scan_lock);
	mod_timer(&priv->scan_timer,
		  jiffies + SPRDWL_SCAN_TIMEOUT_MS * HZ / 1000);

	ret = sprdwl_scan(vif->priv, vif->mode, channels,
			  scan_ssids_len, ssids_ptr);
	kfree(ssids_ptr);
	if (ret) {
		sprdwl_cancel_scan(vif);
		goto err;
	}

	return 0;
err:
	netdev_err(vif->ndev, "%s failed (%d)\n", __func__, ret);
	return ret;
}

static int sprdwl_cfg80211_sched_scan_start(struct wiphy *wiphy,
					    struct net_device *ndev,
					    struct cfg80211_sched_scan_request
					    *request)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_sched_scan_buf *sscan_buf = NULL;
	struct sprdwl_vif *vif = NULL;
	struct cfg80211_ssid *ssid_tmp = NULL;
	struct cfg80211_match_set *match_ssid_tmp = NULL;
	int ret = 0;
	int i = 0, j = 0;

	if (!ndev) {
		netdev_err(ndev, "%s NULL ndev\n", __func__);
		return ret;
	}
	vif = netdev_priv(ndev);

	if (vif->priv->sched_scan_request) {
		netdev_err(ndev, "%s  schedule scan is running\n", __func__);
		return 0;
	}
	sscan_buf = kzalloc(sizeof(*sscan_buf), GFP_KERNEL);
	if (!sscan_buf)
		return -ENOMEM;
	sscan_buf->interval = request->interval;
	sscan_buf->flags = request->flags;

	if (request->rssi_thold <= NL80211_SCAN_RSSI_THOLD_OFF)
		sscan_buf->rssi_thold = 0;
	else if (request->rssi_thold < -127)
		sscan_buf->rssi_thold = -127;
	else
		sscan_buf->rssi_thold = request->rssi_thold;

	for (i = 0, j = 0; i < request->n_channels; i++) {
		int ch = request->channels[i]->hw_value;

		if (ch == 0) {
			netdev_info(ndev, "%s  unknown frequency %dMhz\n",
				    __func__,
				    request->channels[i]->center_freq);
			continue;
		}

		netdev_info(ndev, "%s: channel is %d\n", __func__, ch);
		sscan_buf->channel[j + 1] = ch;
		j++;
	}
	sscan_buf->channel[0] = j;

	if (request->ssids && request->n_ssids > 0) {
		sscan_buf->n_ssids = request->n_ssids;

		for (i = 0; i < request->n_ssids; i++) {
			ssid_tmp = request->ssids + i;
			sscan_buf->ssid[i] = ssid_tmp->ssid;
		}
	}

	if (request->match_sets && request->n_match_sets > 0) {
		sscan_buf->n_match_ssids = request->n_match_sets;

		for (i = 0; i < request->n_match_sets; i++) {
			match_ssid_tmp = request->match_sets + i;
			sscan_buf->mssid[i] = match_ssid_tmp->ssid.ssid;
		}
	}
	sscan_buf->ie_len = request->ie_len;
	sscan_buf->ie = request->ie;

	spin_lock_bh(&priv->sched_scan_lock);
	vif->priv->sched_scan_request = request;
	vif->priv->sched_scan_vif = vif;
	spin_unlock_bh(&priv->sched_scan_lock);

	ret = sprdwl_sched_scan_start(priv, vif->mode, sscan_buf);
	if (ret)
		sprdwl_cancel_sched_scan(vif);

	kfree(sscan_buf);
	return ret;
}

static int sprdwl_cfg80211_sched_scan_stop(struct wiphy *wiphy,
					   struct net_device *ndev)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = NULL;
	int ret = 0;

	if (!ndev) {
		netdev_err(ndev, "%s NULL ndev\n", __func__);
		return ret;
	}
	vif = netdev_priv(ndev);
	ret = sprdwl_sched_scan_stop(priv, vif->mode);
	if (!ret) {
		spin_lock_bh(&priv->sched_scan_lock);
		vif->priv->sched_scan_request = NULL;
		vif->priv->sched_scan_vif = NULL;
		spin_unlock_bh(&priv->sched_scan_lock);
	} else {
		netdev_err(ndev, "%s  scan stop failed\n", __func__);
	}
	return ret;
}

static int sprdwl_cfg80211_connect(struct wiphy *wiphy, struct net_device *ndev,
				   struct cfg80211_connect_params *sme)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	struct sprdwl_cmd_connect con;
	int is_wep = (sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP40) ||
	    (sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP104);
	int ret;

	memset(&con, 0, sizeof(con));

	/* Set wps ie */
	if (sme->ie_len > 0) {
		netdev_info(ndev, "set assoc req ie, len %zx\n", sme->ie_len);
		ret = sprdwl_set_ie(vif->priv, vif->mode, SPRDWL_IE_ASSOC_REQ,
				    sme->ie, sme->ie_len);

		if (ret)
			goto err;
	}

	netdev_info(ndev, "wpa versions %#x\n", sme->crypto.wpa_versions);
	con.wpa_versions = sme->crypto.wpa_versions;
	netdev_info(ndev, "management frame protection %#x\n", sme->mfp);
	con.mfp_enable = sme->mfp;

	netdev_info(ndev, "auth type %#x\n", sme->auth_type);
	if ((sme->auth_type == NL80211_AUTHTYPE_OPEN_SYSTEM) ||
	    ((sme->auth_type == NL80211_AUTHTYPE_AUTOMATIC) && !is_wep))
		con.auth_type = SPRDWL_AUTH_OPEN;
	else if ((sme->auth_type == NL80211_AUTHTYPE_SHARED_KEY) ||
		 ((sme->auth_type == NL80211_AUTHTYPE_AUTOMATIC) && is_wep))
		con.auth_type = SPRDWL_AUTH_SHARED;

	/* Set pairewise cipher */
	if (sme->crypto.n_ciphers_pairwise) {
		vif->prwise_crypto =
		    sprdwl_parse_cipher(sme->crypto.ciphers_pairwise[0]);

		if (vif->prwise_crypto != SPRDWL_CIPHER_WAPI) {
			netdev_info(ndev, "pairwise cipher %#x\n",
				    sme->crypto.ciphers_pairwise[0]);
			con.pairwise_cipher = vif->prwise_crypto;
			con.pairwise_cipher |= SPRDWL_VALID_CONFIG;
		}
	} else {
		netdev_dbg(ndev, "No pairewise cipher specified!\n");
		vif->prwise_crypto = SPRDWL_CIPHER_NONE;
	}

	/* Set group cipher */
	vif->grp_crypto = sprdwl_parse_cipher(sme->crypto.cipher_group);
	if (vif->grp_crypto != SPRDWL_CIPHER_WAPI) {
		netdev_info(ndev, "group cipher %#x\n",
			    sme->crypto.cipher_group);
		con.group_cipher = vif->grp_crypto;
		con.group_cipher |= SPRDWL_VALID_CONFIG;
	}

	/* Set auth key management (akm) */
	if (sme->crypto.n_akm_suites) {
		netdev_info(ndev, "akm suites %#x\n",
			    sme->crypto.akm_suites[0]);
		con.key_mgmt = sprdwl_parse_akm(sme->crypto.akm_suites[0]);
		con.key_mgmt |= SPRDWL_VALID_CONFIG;
	} else {
		netdev_dbg(ndev, "No akm suites specified!\n");
	}

	/* Set PSK */
	if (sme->key_len) {
		if (sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP40 ||
		    sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP104 ||
		    sme->crypto.ciphers_pairwise[0] ==
		    WLAN_CIPHER_SUITE_WEP40 ||
		    sme->crypto.ciphers_pairwise[0] ==
		    WLAN_CIPHER_SUITE_WEP104) {
			vif->key_index[SPRDWL_GROUP] = sme->key_idx;
			vif->key_len[SPRDWL_GROUP][sme->key_idx] = sme->key_len;
			memcpy(vif->key[SPRDWL_GROUP][sme->key_idx], sme->key,
			       sme->key_len);
			ret =
			    sprdwl_add_cipher_key(vif, 0, sme->key_idx,
						  sme->crypto.
						  ciphers_pairwise[0],
						  NULL, NULL);
			if (ret)
				goto err;
		} else if (sme->key_len > WLAN_MAX_KEY_LEN) {
			netdev_err(ndev, "%s invalid key len: %d\n", __func__,
				   sme->key_len);
			ret = -EINVAL;
			goto err;
		} else {
			netdev_info(ndev, "PSK %s\n", sme->key);
			con.psk_len = sme->key_len;
			memcpy(con.psk, sme->key, sme->key_len);
		}
	}

	/* Auth RX unencrypted EAPOL is not implemented, do nothing */
	/* Set channel */
	if (!sme->channel) {
		netdev_dbg(ndev, "No channel specified!\n");
	} else {
		con.channel =
		    ieee80211_frequency_to_channel(sme->channel->center_freq);
		netdev_info(ndev, "channel %d\n", con.channel);
	}

	/* Set BSSID */
	if (!sme->bssid) {
		netdev_dbg(ndev, "No BSSID specified!\n");
	} else {
		memcpy(con.bssid, sme->bssid, sizeof(con.bssid));
		memcpy(vif->bssid, sme->bssid, sizeof(vif->bssid));
	}

	/* Special process for WEP(WEP key must be set before essid) */
	if (sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP40 ||
	    sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP104) {
		netdev_info(ndev, "%s WEP cipher_group\n", __func__);

		if (sme->key_len <= 0) {
			netdev_dbg(ndev, "No key specified!\n");
		} else {
			if (sme->key_len != WLAN_KEY_LEN_WEP104 &&
			    sme->key_len != WLAN_KEY_LEN_WEP40) {
				netdev_err(ndev, "%s invalid WEP key length!\n",
					   __func__);
				ret = -EINVAL;
				goto err;
			}

			sprdwl_set_def_key(vif->priv, vif->mode, sme->key_idx);
			if (ret)
				goto err;
		}
	}

	/* Set ESSID */
	if (!sme->ssid) {
		netdev_dbg(ndev, "No SSID specified!\n");
	} else {
		strncpy(con.ssid, sme->ssid, sme->ssid_len);
		con.ssid_len = sme->ssid_len;
		ret = sprdwl_connect(vif->priv, vif->mode, &con);
		if (ret)
			goto err;
		strncpy(vif->ssid, sme->ssid, sme->ssid_len);
		vif->ssid_len = sme->ssid_len;
		netdev_info(ndev, "%s %s\n", __func__, vif->ssid);
	}

	vif->sm_state = SPRDWL_CONNECTING;
	return 0;
err:
	netdev_err(ndev, "%s failed\n", __func__);
	return ret;
}

static int sprdwl_cfg80211_disconnect(struct wiphy *wiphy,
				      struct net_device *ndev, u16 reason_code)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	enum sm_state old_state = vif->sm_state;
	int ret;

	netdev_info(ndev, "%s %s reason: %d\n", __func__, vif->ssid,
		    reason_code);

	vif->sm_state = SPRDWL_DISCONNECTING;
	ret = sprdwl_disconnect(vif->priv, vif->mode, reason_code);
	if (ret)
		vif->sm_state = old_state;

	return ret;
}

static int sprdwl_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	u16 rts = 0, frag = 0;

	if (changed & WIPHY_PARAM_RTS_THRESHOLD)
		rts = wiphy->rts_threshold;

	if (changed & WIPHY_PARAM_FRAG_THRESHOLD)
		frag = wiphy->frag_threshold;

	return sprdwl_set_param(priv, rts, frag);
}

static int sprdwl_cfg80211_set_pmksa(struct wiphy *wiphy,
				     struct net_device *ndev,
				     struct cfg80211_pmksa *pmksa)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	netdev_info(ndev, "%s\n", __func__);

	return sprdwl_pmksa(vif->priv, vif->mode, pmksa->bssid,
			    pmksa->pmkid, SPRDWL_SUBCMD_SET);
}

static int sprdwl_cfg80211_del_pmksa(struct wiphy *wiphy,
				     struct net_device *ndev,
				     struct cfg80211_pmksa *pmksa)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	netdev_info(ndev, "%s\n", __func__);

	return sprdwl_pmksa(vif->priv, vif->mode, pmksa->bssid,
			    pmksa->pmkid, SPRDWL_SUBCMD_DEL);
}

static int sprdwl_cfg80211_flush_pmksa(struct wiphy *wiphy,
				       struct net_device *ndev)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	netdev_info(ndev, "%s\n", __func__);

	return sprdwl_pmksa(vif->priv, vif->mode, vif->bssid, NULL,
			    SPRDWL_SUBCMD_FLUSH);
}

void sprdwl_scan_timeout(unsigned long data)
{
	struct sprdwl_priv *priv = (struct sprdwl_priv *)data;

	netdev_info(priv->scan_vif->ndev, "%s\n", __func__);

	spin_lock_bh(&priv->scan_lock);
	if (priv->scan_request) {
		cfg80211_scan_done(priv->scan_request, true);
		priv->scan_vif = NULL;
		priv->scan_request = NULL;
	}
	spin_unlock_bh(&priv->scan_lock);
}

static void sprdwl_cancel_scan(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;

	if (priv->scan_vif && priv->scan_vif == vif) {
		if (timer_pending(&priv->scan_timer))
			del_timer_sync(&priv->scan_timer);

		spin_lock_bh(&priv->scan_lock);
		if (priv->scan_request) {
			priv->scan_request = NULL;
			priv->scan_vif = NULL;
		}
		spin_unlock_bh(&priv->scan_lock);
	}
}

void sprdwl_scan_done(struct sprdwl_vif *vif, bool abort)
{
	struct sprdwl_priv *priv = vif->priv;

	if (priv->scan_vif && priv->scan_vif == vif) {
		if (timer_pending(&priv->scan_timer))
			del_timer_sync(&priv->scan_timer);

		spin_lock_bh(&priv->scan_lock);
		if (priv->scan_request) {
			cfg80211_scan_done(priv->scan_request, abort);
			priv->scan_request = NULL;
			priv->scan_vif = NULL;
		}
		spin_unlock_bh(&priv->scan_lock);
	}
}

static void sprdwl_cancel_sched_scan(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;

	if (priv->sched_scan_vif && priv->sched_scan_vif == vif) {
		spin_lock_bh(&priv->sched_scan_lock);
		if (priv->sched_scan_request) {
			priv->sched_scan_request = NULL;
			priv->sched_scan_vif = NULL;
		}
		spin_unlock_bh(&priv->sched_scan_lock);
	}
}

void sprdwl_sched_scan_done(struct sprdwl_vif *vif, bool abort)
{
	struct sprdwl_priv *priv = vif->priv;

	if (priv->sched_scan_vif && priv->sched_scan_vif == vif) {
		spin_lock_bh(&priv->sched_scan_lock);
		if (priv->sched_scan_request) {
			cfg80211_sched_scan_results(vif->wdev.wiphy);
			netdev_info(priv->sched_scan_vif->ndev,
				    "%s report result\n", __func__);
			priv->sched_scan_request = NULL;
			priv->sched_scan_vif = NULL;
		}
		spin_unlock_bh(&priv->sched_scan_lock);
	}
}

void sprdwl_report_scan_result(struct sprdwl_vif *vif, u16 chan, s16 rssi,
			       u8 *frame, u16 len)
{
	struct sprdwl_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)frame;
	struct ieee80211_supported_band *band;
	struct ieee80211_channel *channel;
	struct cfg80211_bss *bss;
	u16 capability, beacon_interval;
	u32 freq;
	s32 signal;
	u64 tsf;
	u8 *ie;
	size_t ielen;

	if (!priv->scan_request && !priv->sched_scan_request) {
		netdev_err(vif->ndev, "%s Unexpected event\n", __func__);
		return;
	}

	band = wiphy->bands[IEEE80211_BAND_2GHZ];
	freq = ieee80211_channel_to_frequency(chan, band->band);
	channel = ieee80211_get_channel(wiphy, freq);
	if (!channel) {
		netdev_err(vif->ndev, "%s invalid freq!\n", __func__);
		return;
	}

	signal = rssi * 100;

	if (!mgmt) {
		netdev_err(vif->ndev, "%s NULL frame!\n", __func__);
		return;
	}
	ie = mgmt->u.probe_resp.variable;
	ielen = len - offsetof(struct ieee80211_mgmt, u.probe_resp.variable);
	tsf = le64_to_cpu(mgmt->u.probe_resp.timestamp);
	beacon_interval = le16_to_cpu(mgmt->u.probe_resp.beacon_int);
	capability = le16_to_cpu(mgmt->u.probe_resp.capab_info);
	netdev_dbg(vif->ndev, "   %s, %pM, channel %2u, signal %d\n",
		   ieee80211_is_probe_resp(mgmt->frame_control)
		   ? "proberesp" : "beacon   ", mgmt->bssid, chan, rssi);

	bss = cfg80211_inform_bss(wiphy, channel, mgmt->bssid,
				  tsf, capability, beacon_interval, ie,
				  ielen, signal, GFP_KERNEL);

	if (unlikely(!bss))
		netdev_err(vif->ndev,
			   "%s failed to inform bss frame!\n", __func__);
	cfg80211_put_bss(wiphy, bss);

	if (vif->beacon_loss) {
		bss = cfg80211_get_bss(wiphy, NULL, vif->bssid,
				       vif->ssid, vif->ssid_len,
				       WLAN_CAPABILITY_ESS,
				       WLAN_CAPABILITY_ESS);
		if (bss) {
			cfg80211_unlink_bss(wiphy, bss);
			netdev_info(vif->ndev,
				    "unlink %pM due to beacon loss\n",
				    bss->bssid);
			vif->beacon_loss = 0;
		}
	}
}

void sprdwl_report_connection(struct sprdwl_vif *vif, u8 *bssid,
			      u8 channel_num, s8 signal,
			      u8 *beacon_ie, u16 beacon_ie_len,
			      u8 *req_ie, u16 req_ie_len, u8 *resp_ie,
			      u16 resp_ie_len, u8 status_code)
{
	struct sprdwl_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct ieee80211_supported_band *band;
	struct ieee80211_channel *channel;
	struct ieee80211_mgmt *mgmt;
	struct cfg80211_bss *bss = NULL;
	u16 capability, beacon_interval;
	u32 freq;
	u64 tsf;
	u8 *ie;
	size_t ielen;

	if (vif->sm_state != SPRDWL_CONNECTING &&
	    vif->sm_state != SPRDWL_CONNECTED) {
		netdev_err(vif->ndev, "%s Unexpected event!\n", __func__);
		return;
	}
	if (status_code != SPRDWL_CONNECT_SUCCESS &&
	    status_code != SPRDWL_ROAM_SUCCESS)
		goto err;
	if (!bssid) {
		netdev_err(vif->ndev, "%s NULL BSSID!\n", __func__);
		goto err;
	}
	if (!req_ie_len) {
		netdev_err(vif->ndev, "%s No associate REQ IE!\n", __func__);
		goto err;
	}
	if (!resp_ie_len) {
		netdev_err(vif->ndev, "%s No associate RESP IE!\n", __func__);
		goto err;
	}

	if (beacon_ie_len) {
		band = wiphy->bands[IEEE80211_BAND_2GHZ];
		freq = ieee80211_channel_to_frequency(channel_num, band->band);
		channel = ieee80211_get_channel(wiphy, freq);
		if (!channel) {
			netdev_err(vif->ndev, "%s invalid freq!\n", __func__);
			goto err;
		}
		mgmt = (struct ieee80211_mgmt *)beacon_ie;
		netdev_info(vif->ndev, "%s update BSS %s\n", __func__,
			    vif->ssid);
		if (!mgmt) {
			netdev_err(vif->ndev, "%s NULL frame!\n", __func__);
			goto err;
		}
		if (!ether_addr_equal(bssid, mgmt->bssid))
			netdev_warn(vif->ndev, "%s Invalid Beacon!\n",
				    __func__);
		ie = mgmt->u.probe_resp.variable;
		ielen = beacon_ie_len - offsetof(struct ieee80211_mgmt,
						 u.probe_resp.variable);
		tsf = le64_to_cpu(mgmt->u.probe_resp.timestamp);
		beacon_interval = le16_to_cpu(mgmt->u.probe_resp.beacon_int);
		capability = le16_to_cpu(mgmt->u.probe_resp.capab_info);
		netdev_dbg(vif->ndev, "%s, %pM, signal: %d\n",
			   ieee80211_is_probe_resp(mgmt->frame_control)
			   ? "proberesp" : "beacon", mgmt->bssid, signal);

		bss = cfg80211_inform_bss(wiphy, channel, mgmt->bssid,
					  tsf, capability, beacon_interval, ie,
					  ielen, signal, GFP_KERNEL);
		if (unlikely(!bss))
			netdev_err(vif->ndev,
				   "%s failed to inform bss frame!\n",
				   __func__);
	} else {
		netdev_warn(vif->ndev, "%s No Beason IE!\n", __func__);
	}

	if (vif->sm_state == SPRDWL_CONNECTING &&
	    status_code == SPRDWL_CONNECT_SUCCESS)
		cfg80211_connect_result(vif->ndev,
					bssid, req_ie, req_ie_len,
					resp_ie, resp_ie_len,
					WLAN_STATUS_SUCCESS, GFP_KERNEL);
	else if (vif->sm_state == SPRDWL_CONNECTED &&
		 status_code == SPRDWL_ROAM_SUCCESS)
		cfg80211_roamed_bss(vif->ndev, bss, req_ie, req_ie_len,
				    resp_ie, resp_ie_len, GFP_KERNEL);
	else {
		netdev_err(vif->ndev, "%s sm_state (%d), status code (%d)!\n",
			   __func__, vif->sm_state, status_code);
		goto err;
	}

	if (!netif_carrier_ok(vif->ndev)) {
		netif_carrier_on(vif->ndev);
		netif_wake_queue(vif->ndev);
	}
	vif->sm_state = SPRDWL_CONNECTED;
	memcpy(vif->bssid, bssid, sizeof(vif->bssid));
	netdev_info(vif->ndev, "%s %s to %s (%pM)\n", __func__,
		    status_code == SPRDWL_CONNECT_SUCCESS ? "connect" : "roam",
		    vif->ssid, vif->bssid);
	return;
err:
	if (vif->sm_state == SPRDWL_CONNECTING)
		cfg80211_connect_result(vif->ndev, vif->bssid, NULL, 0, NULL, 0,
					WLAN_STATUS_UNSPECIFIED_FAILURE,
					GFP_KERNEL);
	else if (vif->sm_state == SPRDWL_CONNECTED)
		cfg80211_disconnected(vif->ndev, status_code, NULL, 0,
				      GFP_KERNEL);
	netdev_err(vif->ndev, "%s %s failed (%d)!\n", __func__, vif->ssid,
		   status_code);
	memset(vif->bssid, 0, sizeof(vif->bssid));
	memset(vif->ssid, 0, sizeof(vif->ssid));
}

#define AP_LEAVING	0xc1
#define AP_DEAUTH	0xc4
void sprdwl_report_disconnection(struct sprdwl_vif *vif, u16 reason_code)
{
	if (vif->sm_state == SPRDWL_CONNECTING) {
		cfg80211_connect_result(vif->ndev, vif->bssid, NULL, 0, NULL, 0,
					WLAN_STATUS_UNSPECIFIED_FAILURE,
					GFP_KERNEL);
	} else if (vif->sm_state == SPRDWL_CONNECTED) {
		if (reason_code == AP_LEAVING) {
			struct cfg80211_bss *bss = NULL;

			bss = cfg80211_get_bss(vif->wdev.wiphy, NULL,
					       vif->bssid, vif->ssid,
					       vif->ssid_len,
					       WLAN_CAPABILITY_ESS,
					       WLAN_CAPABILITY_ESS);
			if (bss)
				cfg80211_unlink_bss(vif->wdev.wiphy, bss);
		}
		cfg80211_disconnected(vif->ndev, reason_code,
				      NULL, 0, GFP_KERNEL);
		netdev_info(vif->ndev,
			    "%s %s, reason_code %d\n", __func__,
			    vif->ssid, reason_code);
	} else if (vif->sm_state != SPRDWL_DISCONNECTING) {
		netdev_err(vif->ndev, "%s Unexpected event!\n", __func__);
		return;
	}

	vif->sm_state = SPRDWL_DISCONNECTED;
	memset(vif->bssid, 0, sizeof(vif->bssid));
	memset(vif->ssid, 0, sizeof(vif->ssid));

	if (netif_carrier_ok(vif->ndev)) {
		netif_carrier_off(vif->ndev);
		netif_stop_queue(vif->ndev);
	}
}

void sprdwl_report_mic_failure(struct sprdwl_vif *vif, u8 is_mcast, u8 key_id)
{
	netdev_info(vif->ndev,
		    "%s is_mcast:0x%x key_id: 0x%x bssid: %pM\n",
		    __func__, is_mcast, key_id, vif->bssid);

	cfg80211_michael_mic_failure(vif->ndev, vif->bssid,
				     (is_mcast ? NL80211_KEYTYPE_GROUP :
				      NL80211_KEYTYPE_PAIRWISE),
				     key_id, NULL, GFP_KERNEL);
}

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

#define	PRINT_BUF_LEN		(1 << 10)
static char print_buf[PRINT_BUF_LEN];
void sprdwl_cfg80211_dump_frame_prot_info(int send, int freq,
					  const unsigned char *buf, int len)
{
	int idx = 0;
	int type = ((*buf) & IEEE80211_FCTL_FTYPE) >> 2;
	int subtype = ((*buf) & IEEE80211_FCTL_STYPE) >> 4;
	int action, action_subtype;
	char *p = print_buf;

	idx += sprintf(p + idx, "[cfg80211] ");

	if (send)
		idx += sprintf(p + idx, "SEND: ");
	else
		idx += sprintf(p + idx, "RECV: ");

	if (type == IEEE80211_FTYPE_MGMT) {
		idx += sprintf(p + idx, "%dMHz, %s, ",
			       freq, type_name[subtype]);
	} else {
		idx += sprintf(p + idx,
			       "%dMHz, not mgmt frame, type=%d, ", freq, type);
	}

	if (subtype == ACTION_TYPE) {
		action = *(buf + MAC_LEN);
		action_subtype = *(buf + ACTION_SUBTYPE_OFFSET);
		if (action == PUB_ACTION)
			idx += sprintf(p + idx, "PUB:%s ",
				       pub_action_name[action_subtype]);
		else if (action == P2P_ACTION)
			idx += sprintf(p + idx, "P2P:%s ",
				       p2p_action_name[action_subtype]);
		else
			idx += sprintf(p + idx, "Unknown ACTION(0x%x)", action);
	}
	p[idx] = '\0';

	pr_info("%s %pM %pM\n", p, &buf[4], &buf[10]);
}

/* P2P related stuff */
static int sprdwl_cfg80211_remain_on_channel(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     struct ieee80211_channel *chan,
					     unsigned int duration, u64 *cookie)
{
	struct sprdwl_vif *vif = container_of(wdev, struct sprdwl_vif, wdev);
	enum nl80211_channel_type channel_type = 0;
	static u64 remain_index;
	int ret;

	*cookie = vif->listen_cookie = ++remain_index;
	netdev_info(wdev->netdev, "%s %d for %dms, cookie 0x%lld\n",
		    __func__, chan->center_freq, duration, *cookie);
	memcpy(&vif->listen_channel, chan, sizeof(struct ieee80211_channel));

	ret = sprdwl_remain_chan(vif->priv, vif->mode, chan,
				 channel_type, duration, cookie);
	if (ret)
		return ret;

	cfg80211_ready_on_channel(wdev, *cookie, chan, duration, GFP_KERNEL);

	return 0;
}

static int sprdwl_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
						    struct wireless_dev *wdev,
						    u64 cookie)
{
	struct sprdwl_vif *vif = container_of(wdev, struct sprdwl_vif, wdev);

	netdev_info(wdev->netdev, "%s cookie 0x%lld\n", __func__, cookie);

	return sprdwl_cancel_remain_chan(vif->priv, vif->mode, cookie);
}

static int sprdwl_cfg80211_mgmt_tx(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   struct ieee80211_channel *chan,
				   bool offchan, unsigned int wait,
				   const u8 *buf, size_t len, bool no_cck,
				   bool dont_wait_for_ack, u64 *cookie)
{
	struct sprdwl_vif *vif = container_of(wdev, struct sprdwl_vif, wdev);
	static u64 mgmt_index;
	int ret = 0;

	*cookie = ++mgmt_index;
	netdev_info(wdev->netdev, "%s cookie %lld\n", __func__, *cookie);

	sprdwl_cfg80211_dump_frame_prot_info(1, chan->center_freq, buf, len);
	/* send tx mgmt */
	if (len > 0) {
		ret = sprdwl_tx_mgmt(vif->priv, vif->mode,
				     ieee80211_frequency_to_channel
				     (chan->center_freq), dont_wait_for_ack,
				     wait, cookie, buf, len);
		if (ret)
			if (!dont_wait_for_ack)
				cfg80211_mgmt_tx_status(wdev, *cookie, buf, len,
							0, GFP_KERNEL);
	}

	return ret;
}

static void sprdwl_cfg80211_mgmt_frame_register(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						u16 frame_type, bool reg)
{
	struct sprdwl_vif *vif = container_of(wdev, struct sprdwl_vif, wdev);
	struct sprdwl_work *misc_work;
	struct sprdwl_reg_mgmt *reg_mgmt;
	u16 mgmt_type;

	if (vif->mode == SPRDWL_MODE_NONE)
		return;

	mgmt_type = (frame_type & IEEE80211_FCTL_STYPE) >> 4;
	if ((reg && test_and_set_bit(mgmt_type, &vif->mgmt_reg)) ||
	    (!reg && !test_and_clear_bit(mgmt_type, &vif->mgmt_reg))) {
		netdev_info(wdev->netdev, "%s  mgmt %d has %sreg\n", __func__,
			    frame_type, reg ? "" : "un");
		return;
	}

	netdev_dbg(wdev->netdev, "frame_type %d, reg %d\n", frame_type, reg);

	misc_work = sprdwl_alloc_work(sizeof(*reg_mgmt));
	if (!misc_work) {
		netdev_err(wdev->netdev, "%s out of memory\n", __func__);
		return;
	}

	misc_work->vif = vif;
	misc_work->id = SPRDWL_WORK_REG_MGMT;

	reg_mgmt = (struct sprdwl_reg_mgmt *)misc_work->data;
	reg_mgmt->type = frame_type;
	reg_mgmt->reg = reg;

	sprdwl_queue_work(vif->priv, misc_work);
}

void sprdwl_report_remain_on_channel_expired(struct sprdwl_vif *vif)
{
	netdev_info(vif->ndev, "%s\n", __func__);

	cfg80211_remain_on_channel_expired(&vif->wdev, vif->listen_cookie,
					   &vif->listen_channel, GFP_KERNEL);
}

void sprdwl_report_mgmt_tx_status(struct sprdwl_vif *vif, u64 cookie,
				  const u8 *buf, u32 len, u8 ack)
{
	netdev_info(vif->ndev, "%s cookie %lld\n", __func__, cookie);

	cfg80211_mgmt_tx_status(&vif->wdev, cookie, buf, len, ack, GFP_KERNEL);
}

void sprdwl_report_rx_mgmt(struct sprdwl_vif *vif, u8 chan, const u8 *buf,
			   size_t len)
{
	bool ret;
	int freq = ieee80211_channel_to_frequency(chan, IEEE80211_BAND_2GHZ);

	ret = cfg80211_rx_mgmt(&vif->wdev, freq, 0, buf, len, GFP_ATOMIC);
	if (!ret)
		netdev_err(vif->ndev, "%s unregistered frame!", __func__);
}

void sprdwl_report_mgmt_deauth(struct sprdwl_vif *vif, const u8 *buf,
			       size_t len)
{
	struct sprdwl_work *misc_work;

	misc_work = sprdwl_alloc_work(len);
	if (!misc_work) {
		netdev_err(vif->ndev, "%s out of memory", __func__);
		return;
	}

	misc_work->vif = vif;
	misc_work->id = SPRDWL_WORK_DEAUTH;
	memcpy(misc_work->data, buf, len);

	sprdwl_queue_work(vif->priv, misc_work);
}

void sprdwl_report_mgmt_disassoc(struct sprdwl_vif *vif, const u8 *buf,
				 size_t len)
{
	struct sprdwl_work *misc_work;

	misc_work = sprdwl_alloc_work(len);
	if (!misc_work) {
		netdev_err(vif->ndev, "%s out of memory", __func__);
		return;
	}

	misc_work->vif = vif;
	misc_work->id = SPRDWL_WORK_DISASSOC;
	memcpy(misc_work->data, buf, len);

	sprdwl_queue_work(vif->priv, misc_work);
}

static int sprdwl_cfg80211_start_p2p_device(struct wiphy *wiphy,
					    struct wireless_dev *wdev)
{
	struct sprdwl_vif *vif = container_of(wdev, struct sprdwl_vif, wdev);

	netdev_info(vif->ndev, "%s\n", __func__);

	return sprdwl_init_fw(vif);
}

static void sprdwl_cfg80211_stop_p2p_device(struct wiphy *wiphy,
					    struct wireless_dev *wdev)
{
	struct sprdwl_vif *vif = container_of(wdev, struct sprdwl_vif, wdev);

	netdev_info(vif->ndev, "%s\n", __func__);

	sprdwl_uninit_fw(vif);
}

static int sprdwl_cfg80211_tdls_mgmt(struct wiphy *wiphy,
				     struct net_device *ndev, u8 *peer,
				     u8 action_code, u8 dialog_token,
				     u16 status_code, const u8 *buf, size_t len)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	struct sk_buff *tdls_skb;
	struct sprdwl_cmd_tdls_mgmt *p;
	u16 datalen, ielen;
	u32 end = 0x1a2b3c4d;

	netdev_info(ndev, "%s action_code=%d(%pM)\n", __func__,
		    action_code, peer);

	datalen = sizeof(*p) + len + sizeof(end);
	ielen = len + sizeof(end);
	tdls_skb = dev_alloc_skb(datalen + NET_IP_ALIGN);
	if (!tdls_skb) {
		wiphy_err(wiphy, "dev_alloc_skb failed\n");
		return -ENOMEM;
	}
	skb_reserve(tdls_skb, NET_IP_ALIGN);
	p = (struct sprdwl_cmd_tdls_mgmt *)skb_put(tdls_skb,
			offsetof(struct sprdwl_cmd_tdls_mgmt, u));

	memcpy(p->da, peer, ETH_ALEN);
	memcpy(p->sa, vif->ndev->dev_addr, ETH_ALEN);
	p->ether_type = cpu_to_be16(ETH_P_TDLS);
	p->payloadtype = WLAN_TDLS_SNAP_RFTYPE;
	switch (action_code) {
	case WLAN_TDLS_SETUP_REQUEST:
		p->category = WLAN_CATEGORY_TDLS;
		p->action_code = WLAN_TDLS_SETUP_REQUEST;
		p = (struct sprdwl_cmd_tdls_mgmt *)skb_put(tdls_skb,
			(sizeof(p->u.setup_req) + ielen));
		memcpy(p, &dialog_token, 1);
		memcpy((u8 *)p + 1, buf, len);
		memcpy((u8 *)p + 1 + len, &end, sizeof(end));
		break;
	case WLAN_TDLS_SETUP_RESPONSE:
		p->category = WLAN_CATEGORY_TDLS;
		p->action_code = WLAN_TDLS_SETUP_RESPONSE;
		p = (struct sprdwl_cmd_tdls_mgmt *)skb_put(tdls_skb,
			(sizeof(p->u.setup_resp) + ielen));
		memcpy(p, &status_code, 2);
		memcpy((u8 *)p + 2, &dialog_token, 1);
		memcpy((u8 *)p + 3, buf, len);
		memcpy((u8 *)p + 3 + len, &end, sizeof(end));
		break;
	case WLAN_TDLS_SETUP_CONFIRM:
		p->category = WLAN_CATEGORY_TDLS;
		p->action_code = WLAN_TDLS_SETUP_CONFIRM;
		p = (struct sprdwl_cmd_tdls_mgmt *)skb_put(tdls_skb,
			(sizeof(p->u.setup_cfm) + ielen));
		memcpy(p, &status_code, 2);
		memcpy((u8 *)p + 2, &dialog_token, 1);
		memcpy((u8 *)p + 3, buf, len);
		memcpy((u8 *)p + 3 + len, &end, sizeof(end));
		break;
	case WLAN_TDLS_TEARDOWN:
		p->category = WLAN_CATEGORY_TDLS;
		p->action_code = WLAN_TDLS_TEARDOWN;
		p = (struct sprdwl_cmd_tdls_mgmt *)skb_put(tdls_skb,
			(sizeof(p->u.teardown) + ielen));
		memcpy(p, &status_code, 2);
		memcpy((u8 *)p + 2, buf, len);
		memcpy((u8 *)p + 2 + len, &end, sizeof(end));
		break;
	case SPRDWL_TDLS_DISCOVERY_RESPONSE:
		p->category = WLAN_CATEGORY_TDLS;
		p->action_code = SPRDWL_TDLS_DISCOVERY_RESPONSE;
		p = (struct sprdwl_cmd_tdls_mgmt *)skb_put(tdls_skb,
			(sizeof(p->u.discover_resp) + ielen));
		memcpy(p, &dialog_token, 1);
		memcpy((u8 *)p + 1, buf, len);
		memcpy((u8 *)p + 1 + len, &end, sizeof(end));
		break;
	}

	return sprdwl_tdls_mgmt(vif, tdls_skb);
}

static int sprdwl_cfg80211_tdls_oper(struct wiphy *wiphy,
				     struct net_device *ndev, u8 *peer,
				     enum nl80211_tdls_operation oper)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	netdev_info(ndev, "%s oper=%d\n", __func__, oper);

	if (oper == NL80211_TDLS_ENABLE_LINK)
		oper = SPRDWL_TDLS_ENABLE_LINK;
	else if (oper == NL80211_TDLS_DISABLE_LINK)
		oper = SPRDWL_TDLS_DISABLE_LINK;
	else
		netdev_err(ndev, "unsupported this TDLS oper\n");

	return sprdwl_tdls_oper(vif->priv, vif->mode, peer, oper);
}

static int sprdwl_cfg80211_tdls_chan_switch(struct wiphy *wiphy,
					    struct net_device *ndev,
					    const u8 *addr, u8 oper_class,
					    struct cfg80211_chan_def *chandef)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	u8 chan, band;

	chan = chandef->chan->hw_value;
	band = chandef->chan->band;

	netdev_info(ndev, "%s: chan=%u, band=%u\n", __func__, chan, band);
	return sprdwl_start_tdls_channel_switch(vif->priv, vif->mode, addr,
						chan, 0, band);
}

static void sprdwl_cfg80211_tdls_cancel_chan_switch(struct wiphy *wiphy,
						    struct net_device *ndev,
						    const u8 *addr)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	netdev_info(ndev, "%s\n", __func__);
	sprdwl_cancel_tdls_channel_switch(vif->priv, vif->mode, addr);
}

void sprdwl_report_tdls(struct sprdwl_vif *vif, const u8 *peer,
			u8 oper, u16 reason_code)
{
	netdev_info(vif->ndev, "%s A station (%pM)found\n", __func__, peer);

	cfg80211_tdls_oper_request(vif->ndev, peer, oper,
				   reason_code, GFP_KERNEL);
}

/* Roaming related stuff */
int sprdwl_cfg80211_set_cqm_rssi_config(struct wiphy *wiphy,
					struct net_device *ndev,
					s32 rssi_thold, u32 rssi_hyst)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	netdev_info(ndev, "%s rssi_thold %d rssi_hyst %d",
		    __func__, rssi_thold, rssi_hyst);

	return sprdwl_set_cqm_rssi(vif->priv, vif->mode, rssi_thold, rssi_hyst);
}

void sprdwl_report_cqm(struct sprdwl_vif *vif, u8 rssi_event)
{
	netdev_info(vif->ndev, "%s %d\n", __func__, vif->cqm);

	cfg80211_cqm_rssi_notify(vif->ndev, rssi_event, GFP_KERNEL);
}

int sprdwl_cfg80211_update_ft_ies(struct wiphy *wiphy, struct net_device *ndev,
				  struct cfg80211_update_ft_ies_params *ftie)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	netdev_info(ndev, "%s\n", __func__);

	return sprdwl_set_roam_offload(vif->priv, vif->mode,
				       SPRDWL_ROAM_OFFLOAD_SET_FTIE,
				       ftie->ie, ftie->ie_len);
}

static int sprdwl_cfg80211_set_qos_map(struct wiphy *wiphy,
				       struct net_device *ndev,
				       struct cfg80211_qos_map *qos_map)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	netdev_info(ndev, "%s\n", __func__);

	return sprdwl_set_qos_map(vif->priv, vif->mode, (void *)qos_map);
}

static int sprdwl_cfg80211_add_tx_ts(struct wiphy *wiphy,
				     struct net_device *ndev,
				     u8 tsid, const u8 *peer,
				     u8 user_prio, u16 admitted_time)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	netdev_info(ndev, "%s\n", __func__);

	return sprdwl_add_tx_ts(vif->priv, vif->mode, tsid, peer,
				user_prio, admitted_time);
}

static int sprdwl_cfg80211_del_tx_ts(struct wiphy *wiphy,
				     struct net_device *ndev,
				     u8 tsid, const u8 *peer)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	netdev_info(ndev, "%s\n", __func__);

	return sprdwl_del_tx_ts(vif->priv, vif->mode, tsid, peer);
}

static int sprdwl_cfg80211_set_mac_acl(struct wiphy *wiphy,
				       struct net_device *ndev,
				       const struct cfg80211_acl_data *acl)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);
	int index, num;
	int mode = SPRDWL_ACL_MODE_DISABLE;
	unsigned char *mac_addr = NULL;

	if (!acl || !acl->n_acl_entries || !acl->mac_addrs) {
		netdev_err(ndev, "%s no ACL data\n", __func__);
		return 0;
	}

	if (acl->acl_policy == NL80211_ACL_POLICY_DENY_UNLESS_LISTED) {
		mode = SPRDWL_ACL_MODE_WHITELIST;
	} else if (acl->acl_policy == NL80211_ACL_POLICY_ACCEPT_UNLESS_LISTED) {
		mode = SPRDWL_ACL_MODE_BLACKLIST;
	} else {
		netdev_err(ndev, "%s invalid ACL mode\n", __func__);
		return -EINVAL;
	}

	num = acl->n_acl_entries;
	netdev_info(ndev, "%s ACL MAC num:%d\n", __func__, num);
	if (num < 0 || num > vif->priv->max_acl_mac_addrs)
		return -EINVAL;

	mac_addr = kzalloc(num * ETH_ALEN, GFP_KERNEL);
	if (IS_ERR(mac_addr))
		return -ENOMEM;

	for (index = 0; index < num; index++) {
		netdev_info(ndev, "%s  MAC: %pM\n", __func__,
			    &acl->mac_addrs[index]);
		memcpy(mac_addr + index * ETH_ALEN,
		       &acl->mac_addrs[index], ETH_ALEN);
	}

	if (mode == SPRDWL_ACL_MODE_WHITELIST)
		return sprdwl_set_whitelist(vif->priv, vif->mode,
					    SPRDWL_SUBCMD_ENABLE,
					    num, mac_addr);
	else
		return sprdwl_set_blacklist(vif->priv, vif->mode,
					    SPRDWL_SUBCMD_ADD, num, mac_addr);
}

int sprdwl_cfg80211_set_power_mgmt(struct wiphy *wiphy, struct net_device *ndev,
				   bool enabled, int timeout)
{
	struct sprdwl_vif *vif = netdev_priv(ndev);

	netdev_info(ndev, "%s power save status:%d\n", __func__, enabled);
	return sprdwl_power_save(vif->priv, vif->mode,
				 SPRDWL_SET_PS_STATE, enabled);
}

static struct cfg80211_ops sprdwl_cfg80211_ops = {
	.add_virtual_intf = sprdwl_cfg80211_add_iface,
	.del_virtual_intf = sprdwl_cfg80211_del_iface,
	.change_virtual_intf = sprdwl_cfg80211_change_iface,
	.add_key = sprdwl_cfg80211_add_key,
	.del_key = sprdwl_cfg80211_del_key,
	.set_default_key = sprdwl_cfg80211_set_default_key,
	.start_ap = sprdwl_cfg80211_start_ap,
	.change_beacon = sprdwl_cfg80211_change_beacon,
	.stop_ap = sprdwl_cfg80211_stop_ap,
	.add_station = sprdwl_cfg80211_add_station,
	.del_station = sprdwl_cfg80211_del_station,
	.change_station = sprdwl_cfg80211_change_station,
	.get_station = sprdwl_cfg80211_get_station,
	.libertas_set_mesh_channel = sprdwl_cfg80211_set_channel,
	.scan = sprdwl_cfg80211_scan,
	.connect = sprdwl_cfg80211_connect,
	.disconnect = sprdwl_cfg80211_disconnect,
	.set_wiphy_params = sprdwl_cfg80211_set_wiphy_params,
	.set_pmksa = sprdwl_cfg80211_set_pmksa,
	.del_pmksa = sprdwl_cfg80211_del_pmksa,
	.flush_pmksa = sprdwl_cfg80211_flush_pmksa,
	.remain_on_channel = sprdwl_cfg80211_remain_on_channel,
	.cancel_remain_on_channel = sprdwl_cfg80211_cancel_remain_on_channel,
	.mgmt_tx = sprdwl_cfg80211_mgmt_tx,
	.mgmt_frame_register = sprdwl_cfg80211_mgmt_frame_register,
	.set_power_mgmt = sprdwl_cfg80211_set_power_mgmt,
	.set_cqm_rssi_config = sprdwl_cfg80211_set_cqm_rssi_config,
	.sched_scan_start = sprdwl_cfg80211_sched_scan_start,
	.sched_scan_stop = sprdwl_cfg80211_sched_scan_stop,
	.tdls_mgmt = sprdwl_cfg80211_tdls_mgmt,
	.tdls_oper = sprdwl_cfg80211_tdls_oper,
	.start_p2p_device = sprdwl_cfg80211_start_p2p_device,
	.stop_p2p_device = sprdwl_cfg80211_stop_p2p_device,
	.set_mac_acl = sprdwl_cfg80211_set_mac_acl,
	.update_ft_ies = sprdwl_cfg80211_update_ft_ies,
	.set_qos_map = sprdwl_cfg80211_set_qos_map,
	.add_tx_ts = sprdwl_cfg80211_add_tx_ts,
	.del_tx_ts = sprdwl_cfg80211_del_tx_ts,
	.tdls_channel_switch = sprdwl_cfg80211_tdls_chan_switch,
	.tdls_cancel_channel_switch = sprdwl_cfg80211_tdls_cancel_chan_switch,
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
			  "%s failed to alloc sprdwl_ieee80211_regdomain!\n",
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

	if (sprdwl_set_regdom(priv, (u8 *)rd, rd_size))
		wiphy_err(wiphy, "%s failed to set regdomain!\n", __func__);

	kfree(rd);
}

void sprdwl_setup_wiphy(struct wiphy *wiphy, struct sprdwl_priv *priv)
{
	wiphy->mgmt_stypes = sprdwl_mgmt_stypes;
	wiphy->interface_modes =
	    BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP) |
	    BIT(NL80211_IFTYPE_P2P_GO) | BIT(NL80211_IFTYPE_P2P_CLIENT) |
	    BIT(NL80211_IFTYPE_P2P_DEVICE);

	wiphy->flags &= ~WIPHY_FLAG_PS_ON_BY_DEFAULT;
	wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;
	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
	wiphy->max_scan_ssids = SPRDWL_MAX_SCAN_SSIDS;
	wiphy->max_scan_ie_len = SPRDWL_MAX_SCAN_IE_LEN;
	wiphy->cipher_suites = sprdwl_cipher_suites;
	wiphy->n_cipher_suites = ARRAY_SIZE(sprdwl_cipher_suites);
	wiphy->bands[IEEE80211_BAND_2GHZ] = &sprdwl_band_2ghz;
	/*wiphy->max_ap_assoc_sta = priv->max_ap_assoc_sta;*/
#ifdef CONFIG_PM
	/*Set WoWLAN flags */
	wiphy->wowlan.flags = WIPHY_WOWLAN_ANY | WIPHY_WOWLAN_DISCONNECT;
#endif
	wiphy->max_remain_on_channel_duration = 5000;
	wiphy->max_num_pmkids = SPRDWL_MAX_NUM_PMKIDS;

	if (priv->fw_std & SPRDWL_STD_11D) {
		pr_info("\tIEEE802.11d supported\n");
		wiphy->reg_notifier = sprdwl_reg_notify;
	}

	if (priv->fw_std & SPRDWL_STD_11E) {
		pr_info("\tIEEE802.11e supported\n");
		wiphy->features |= NL80211_FEATURE_SUPPORTS_WMM_ADMISSION;
	}

	if (priv->fw_std & SPRDWL_STD_11K)
		pr_info("\tIEEE802.11k supported\n");

	if (priv->fw_std & SPRDWL_STD_11R)
		pr_info("\tIEEE802.11r supported\n");

	if (priv->fw_std & SPRDWL_STD_11U)
		pr_info("\tIEEE802.11u supported\n");

	if (priv->fw_std & SPRDWL_STD_11V)
		pr_info("\tIEEE802.11v supported\n");

	if (priv->fw_std & SPRDWL_STD_11W)
		pr_info("\tIEEE802.11w supported\n");

	if (priv->fw_capa & SPRDWL_CAPA_5G) {
		pr_info("\tDual band supported\n");
		wiphy->bands[IEEE80211_BAND_5GHZ] = &sprdwl_band_5ghz;
	}

	if (priv->fw_capa & SPRDWL_CAPA_MCC) {
		pr_info("\tMCC supported\n");
		wiphy->n_iface_combinations = ARRAY_SIZE(sprdwl_iface_combos);
		wiphy->iface_combinations = sprdwl_iface_combos;
	} else {
		pr_info("\tSCC supported\n");
		wiphy->software_iftypes =
		    BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_AP) |
		    BIT(NL80211_IFTYPE_P2P_CLIENT) |
		    BIT(NL80211_IFTYPE_P2P_GO) |
		    BIT(NL80211_IFTYPE_P2P_DEVICE);
	}

	if (priv->fw_capa & SPRDWL_CAPA_ACL) {
		pr_info("\tACL supported (%d)\n", priv->max_acl_mac_addrs);
		wiphy->max_acl_mac_addrs = priv->max_acl_mac_addrs;
	}

	if (priv->fw_capa & SPRDWL_CAPA_AP_SME) {
		pr_info("\tAP SME enabled\n");
		wiphy->flags |= WIPHY_FLAG_HAVE_AP_SME;
		wiphy->ap_sme_capa = 1;
	}

	if (priv->fw_capa & SPRDWL_CAPA_PMK_OKC_OFFLOAD &&
	    priv->fw_capa & SPRDWL_CAPA_11R_ROAM_OFFLOAD) {
		pr_info("\tRoaming offload supported\n");
		wiphy->flags |= WIPHY_FLAG_SUPPORTS_FW_ROAM;
	}

	if (priv->fw_capa & SPRDWL_CAPA_SCHED_SCAN) {
		pr_info("\tScheduled scan supported\n");
		wiphy->flags |= WIPHY_FLAG_SUPPORTS_SCHED_SCAN;
		wiphy->max_sched_scan_ssids = SPRDWL_MAX_PFN_LIST_COUNT;
		wiphy->max_match_sets = SPRDWL_MAX_PFN_LIST_COUNT;
		wiphy->max_sched_scan_ie_len = SPRDWL_MAX_SCAN_IE_LEN;
	}

	if (priv->fw_capa & SPRDWL_CAPA_TDLS) {
		pr_info("\tTDLS supported\n");
		wiphy->flags |= WIPHY_FLAG_SUPPORTS_TDLS;
		wiphy->flags |= WIPHY_FLAG_TDLS_EXTERNAL_SETUP;
		wiphy->features |= NL80211_FEATURE_TDLS_CHANNEL_SWITCH;
	}
}

static void sprdwl_check_intf_ops(struct sprdwl_if_ops *ops)
{
	WARN_ON(!ops->get_msg_buf);
	WARN_ON(!ops->free_msg_buf);
	WARN_ON(!ops->tx);
	WARN_ON(!ops->force_exit);
	WARN_ON(!ops->is_exit);
}

struct sprdwl_priv *sprdwl_core_create(enum sprdwl_hw_type type,
				       struct sprdwl_if_ops *ops)
{
	struct wiphy *wiphy;
	struct sprdwl_priv *priv;

	sprdwl_check_intf_ops(ops);
	sprdwl_cmd_init();

	wiphy = wiphy_new(&sprdwl_cfg80211_ops, sizeof(*priv));
	if (!wiphy) {
		pr_err("failed to allocate wiphy!\n");
		return NULL;
	}
	priv = wiphy_priv(wiphy);
	priv->wiphy = wiphy;
	g_sprdwl_priv = priv;
	priv->hw_type = type;

	priv->skb_head_len = sizeof(struct sprdwl_data_hdr) + NET_IP_ALIGN +
	    SPRDWL_SKB_HEAD_RESERV_LEN;

	priv->if_ops = ops;
	setup_timer(&priv->scan_timer, sprdwl_scan_timeout,
		    (unsigned long)priv);
	spin_lock_init(&priv->scan_lock);
	spin_lock_init(&priv->sched_scan_lock);
	spin_lock_init(&priv->list_lock);
	INIT_LIST_HEAD(&priv->vif_list);
	sprdwl_init_work(priv);

	return priv;
}

void sprdwl_core_free(struct sprdwl_priv *priv)
{
	sprdwl_deinit_work(priv);
	sprdwl_cmd_deinit();

	if (priv) {
		struct wiphy *wiphy = priv->wiphy;

		if (wiphy)
			wiphy_free(wiphy);
		g_sprdwl_priv = NULL;
	}
}
