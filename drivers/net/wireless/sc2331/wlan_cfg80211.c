/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 *
 * Authors:<jinglong.chen@spreadtrum.com>
 * Owner:
 *      jinglong.chen
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

#include "wlan_common.h"
#include "wlan_cfg80211.h"
#include "wlan_cmd.h"

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

static struct ieee80211_rate itm_rates[] = {
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

#define ITM_G_RATE_NUM	28
#define itm_g_rates		(itm_rates)
#define ITM_A_RATE_NUM	24
#define itm_a_rates		(itm_rates + 4)

#define itm_g_htcap (IEEE80211_HT_CAP_SUP_WIDTH_20_40 | \
			IEEE80211_HT_CAP_SGI_20		 | \
			IEEE80211_HT_CAP_SGI_40)

static struct ieee80211_channel itm_2ghz_channels[] = {
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

/*static struct ieee80211_channel itm_5ghz_channels[] =
{
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
};*/

static struct ieee80211_supported_band itm_band_2ghz = {
	.n_channels = ARRAY_SIZE(itm_2ghz_channels),
	.channels = itm_2ghz_channels,
	.n_bitrates = ITM_G_RATE_NUM,
	.bitrates = itm_g_rates,
	.ht_cap.cap = itm_g_htcap,
	.ht_cap.ht_supported = true,
};

/*static struct ieee80211_supported_band itm_band_5ghz = {
	.n_channels = ARRAY_SIZE(itm_5ghz_channels),
	.channels = itm_5ghz_channels,
	.n_bitrates = ITM_A_RATE_NUM,
	.bitrates = itm_a_rates,
	.ht_cap.cap = itm_g_htcap,
	.ht_cap.ht_supported = true,
};*/

static const u32 itm_cipher_suites[] = {
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,
	WLAN_CIPHER_SUITE_SMS4,
#ifdef BSS_ACCESS_POINT_MODE
	WLAN_CIPHER_SUITE_ITM_CCMP,
	WLAN_CIPHER_SUITE_ITM_TKIP,
#endif
};

/* Supported mgmt frame types to be advertised to cfg80211 */
static const struct ieee80211_txrx_stypes
 itm_mgmt_stypes[NUM_NL80211_IFTYPES] = {
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
/* Supported mgmt frame types for p2p*/
	[NL80211_IFTYPE_ADHOC] = {
				  .tx = 0xffff,
				  .rx = BIT(IEEE80211_STYPE_ACTION >> 4)
				  },
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
};

#define WLAN_EID_VENDOR_SPECIFIC 221

void get_ssid(unsigned char *data, unsigned char *ssid)
{
	unsigned char len = 0;
	unsigned char i = 0;
	unsigned char j = 0;

	len = data[37];
	j = 38;

	if (len >= 33)
		len = 0;

	for (i = 0; i < len; i++, j++)
		ssid[i] = data[j];
	ssid[len] = '\0';
}

void get_bssid(unsigned char *data, unsigned char *bssid)
{
	if (1 == ((data[1] & 0x02) >> 1))
		memcpy(bssid, data + 10, 6);
	else if ((data[1] & 0x01) == 1)
		memcpy(bssid, data + 4, 6);
	else
		memcpy(bssid, data + 16, 6);
	return;
}

#ifdef WIFI_DIRECT_SUPPORT

struct ieee80211_channel global_channel;
u64 global_cookie;

static int get_file_size(struct file *f)
{
	int error = -EBADF;
	struct kstat stat;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
	error = vfs_getattr(&f->f_path, &stat);
#else
	error = vfs_getattr(f->f_path.mnt, f->f_path.dentry, &stat);
#endif
	if (error == 0) {
		return stat.size;
	} else {
		pr_err("get conf file stat error\n");
		return error;
	}
}

#define P2P_MODE_PATH "/data/misc/wifi/fwpath"
int itm_get_p2p_mode_from_file(void)
{
	struct file *fp = 0;
	mm_segment_t fs;
	int size = 0;
	loff_t pos = 0;
	u8 *buf;
	int ret = false;
	fp = filp_open(P2P_MODE_PATH, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("open %s file error\n", P2P_MODE_PATH);
		goto end;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	size = get_file_size(fp);
	if (size <= 0) {
		pr_err("load file:%s error\n", P2P_MODE_PATH);
		goto error;
	}
	buf = kzalloc(size + 1, GFP_KERNEL);
	vfs_read(fp, buf, size, &pos);
	if (strcmp(buf, "p2p_mode") == 0)
		ret = true;
	kfree(buf);
error:
	filp_close(fp, NULL);
	set_fs(fs);
end:
	return ret;
}

static bool itm_find_p2p_ie(const u8 *ie, size_t ie_len, u8 *p2p_ie,
			    size_t *p2p_ie_len)
{
	bool flags = false;
	u16 index = 0;
/*Find out P2P IE.*/

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

static int wlan_cfg80211_remain_on_channel(struct wiphy *wiphy,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
					   struct wireless_dev *dev,
#else
					   struct net_device *dev,
#endif
					   struct ieee80211_channel
					   *channel,
					   unsigned int duration, u64 *cookie)
{
	wlan_vif_t *vif;
	unsigned char vif_id;
	int ret;

	enum nl80211_channel_type channel_type = 0;
	vif = ndev_to_vif(dev->netdev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	memcpy(&global_channel, channel, sizeof(struct ieee80211_channel));
	global_cookie = *cookie;

	/* send remain chan */
	ret =
	    wlan_cmd_remain_chan(vif_id, channel, channel_type, duration,
				 cookie);
	if (OK != ret)
		return -1;

	/* report remain chan */
	cfg80211_ready_on_channel(dev, *cookie, channel, duration, GFP_KERNEL);
	return 0;
}

static int wlan_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
						  struct wireless_dev *dev,
#else
						  struct net_device *dev,
#endif
						  u64 cookie)
{
	int ret;
	wlan_vif_t *vif;
	unsigned char vif_id;
	vif = ndev_to_vif(dev->netdev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	ret = wlan_cmd_cancel_remain_chan(vif_id, cookie);
	if (OK != ret)
		return ERROR;
	return OK;
}

static int wlan_cfg80211_del_station(struct wiphy *wiphy,
				     struct net_device *ndev, u8 *mac)
{
	wlan_vif_t *vif;
	unsigned char vif_id;
	vif = ndev_to_vif(ndev);
	vif_id = vif->id;

	if (!mac) {
		wiphy_dbg(wiphy, "Ignore NULL MAC address!\n");
		goto out;
	}

	wiphy_info(wiphy, "%s %pM\n", __func__, mac);
	wlan_cmd_disassoc(vif_id, mac, WLAN_REASON_DEAUTH_LEAVING);
out:
	return 0;
}

static void register_frame_work_fun(struct work_struct *work)
{
	unsigned char vif_id;
	struct wlan_cmd_register_frame_t data;
	register_frame_param_t *param =
	    container_of(work, register_frame_param_t, work);
	data.type = param->frame_type;
	data.reg = param->reg ? 1 : 0;
	param->frame_type = 0xffff;
	param->reg = 0;
	vif_id = ((wlan_vif_t *) (param->vif))->id;
	wlan_cmd_register_frame(vif_id, &data);
	return;
}

void init_register_frame_param(wlan_vif_t *vif)
{
	register_frame_param_t *param;
	param = &(vif->cfg80211.register_frame);
	param->frame_type = 0xffff;
	param->reg = 0;
	param->vif = (void *)vif;
	INIT_WORK(&(param->work), register_frame_work_fun);
}

static int register_frame(wlan_vif_t *vif, unsigned short frame_type, bool reg)
{
	vif->cfg80211.register_frame.frame_type = frame_type;
	vif->cfg80211.register_frame.reg = reg;
	schedule_work(&(vif->cfg80211.register_frame.work));
	return 0;
}

void cfg80211_report_remain_on_channel_expired(unsigned char vif_id,
					       unsigned char *data,
					       unsigned short len)
{
	wlan_vif_t *vif;
	vif = id_to_vif(vif_id);
	cfg80211_remain_on_channel_expired(&(vif->wdev), global_cookie,
					   &global_channel, GFP_KERNEL);
	return;
}

static void send_deauth_work_func(struct work_struct *work)
{
	wlan_vif_t *vif;
	struct deauth_info *info;

	info = container_of(work, struct deauth_info, work);
	vif = container_of(info, wlan_vif_t, deauth_info);
	cfg80211_send_deauth(vif->ndev, info->mac, info->len);

	return;
}

void init_send_deauth_work(wlan_vif_t *vif)
{
	struct deauth_info *info;
	info = &vif->deauth_info;
	memset(info, 0, sizeof(*info));
	INIT_WORK(&info->work, send_deauth_work_func);
}

void cfg80211_report_mgmt_deauth(unsigned char vif_id, unsigned char *data,
				 unsigned short len)
{
	wlan_vif_t *vif = id_to_vif(vif_id);
	memcpy(&vif->deauth_info.len, data, 2);
	if (vif->deauth_info.len > sizeof(vif->deauth_info.mac)) {
		ASSERT("%s len:%d > max:%d\n", __func__,
		       vif->deauth_info.len, sizeof(vif->deauth_info.mac));
		return;
	}

	memcpy(vif->deauth_info.mac, data + 2, vif->deauth_info.len);
	schedule_work(&vif->deauth_info.work);

	return;
}

void cfg80211_report_mgmt_disassoc(unsigned char vif_id, unsigned char *data,
				   unsigned short len)
{
	u8 *mac_ptr, *index;
	u16 mac_len;
	wlan_vif_t *vif = id_to_vif(vif_id);

	index = data;
	memcpy(&mac_len, index, 2);
	index += 2;
	mac_ptr = index;
	cfg80211_send_disassoc(vif->ndev, mac_ptr, mac_len);
}

void cfg80211_report_station(unsigned char vif_id, unsigned char *data,
			     unsigned short len)
{
	u8 connect_ap;
	u8 *req_ptr, *index;
	u16 req_len, mac_len;
	u8 *sta_mac;
	u32 event_len;
	int left;
	struct station_info sinfo;
	wlan_vif_t *vif = id_to_vif(vif_id);
	struct wiphy *wiphy = vif->wdev.wiphy;

	event_len = len;
	index = data;

	left = event_len;

	/* The first byte of event data is connection */
	memcpy(&connect_ap, index, 1);
	index++;
	left--;

	/* The second  byte of event data is mac */
	memcpy(&mac_len, index, 2);
	index += 2;
	left -= 2;

	if (mac_len != 6) {
		printkd("channel len %d not equal 6 bytes\n", mac_len);
		return;
	}

	sta_mac = index;
	index += mac_len;
	left -= mac_len;

	if (!left) {
		printkd("There is no associa req frame!\n");
		return;
	}

	/* The third event data is associate request */
	memcpy(&req_len, index, 2);
	index += 2;
	left -= 2;

	req_ptr = index;
	left -= req_len;

	memset(&sinfo, 0, sizeof(struct station_info));
	sinfo.assoc_req_ies = req_ptr;
	sinfo.assoc_req_ies_len = req_len;
	sinfo.filled = STATION_INFO_ASSOC_REQ_IES;

	if (connect_ap) {
		cfg80211_new_sta(vif->ndev, sta_mac, &sinfo, GFP_KERNEL);
		wiphy_info(wiphy, "New station (" MACSTR ") connected\n",
			   MAC2STR(sta_mac));
	} else {
		cfg80211_del_sta(vif->ndev, sta_mac, GFP_KERNEL);
		wiphy_info(wiphy, "A station (" MACSTR ") disconnected\n",
			   MAC2STR(sta_mac));
	}
}

void cfg80211_report_frame(unsigned char vif_id, unsigned char *data,
			   unsigned short len)
{
	unsigned short mac_len;
	unsigned char *mac_ptr = NULL;
	unsigned char channel = 0, type = 0;
	int freq;
	struct wlan_event_report_frame_t *report_frame = NULL;
	wlan_vif_t *vif = id_to_vif(vif_id);

	report_frame = (struct wlan_event_report_frame_t *)data;
	channel = report_frame->channel;
	type = report_frame->frame_type;
	freq = ieee80211_channel_to_frequency(channel, IEEE80211_BAND_2GHZ);
	mac_ptr = (unsigned char *)(report_frame + 1);
	mac_len = report_frame->frame_len;
	printkd("%s, frame_len:%d\n", __func__, mac_len);
	cfg80211_rx_mgmt(&(vif->wdev), freq, 0, mac_ptr, mac_len, GFP_KERNEL);
}

#endif /*WIFI_DIRECT_SUPPORT */

static bool itm_is_wps_ie(const unsigned char *pos)
{
	return (pos[0] == WLAN_EID_VENDOR_SPECIFIC &&
		pos[1] >= 4 &&
		pos[2] == 0x00 && pos[3] == 0x50 && pos[4] == 0xf2 &&
		pos[5] == 0x04);
}

static bool itm_find_wpsie(const unsigned char *ies, size_t ies_len,
			   unsigned char *buf, size_t *wps_len)
{
	const unsigned char *pos;
	size_t len = 0;
	bool flags = false;

	/*
	 * Filter out RSN/WPA IE(s)
	 */
	if (ies && ies_len) {
		pos = ies;

		while (pos + 1 < ies + ies_len) {
			if (pos + 2 + pos[1] > ies + ies_len)
				break;

			if (itm_is_wps_ie(pos)) {
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

static bool itm_find_ft_ie(const unsigned char *ies, size_t ies_len,
			   unsigned char *buf, size_t *ie_len)
{
	const unsigned char *pos;
	size_t len = 0;
	bool flags = false;
	if (ies && ies_len) {
		pos = ies;
		while (pos + 1 < ies + ies_len) {
			if (pos + 2 + pos[1] > ies + ies_len)
				break;
			if ((WLAN_11R_FT_IE_ID == pos[0])
			    || (WLAN_11R_MD_IE_ID == pos[0])) {
				memcpy(buf + len, pos, 2 + pos[1]);
				len += 2 + pos[1];
				flags = true;
			}

			pos += 2 + pos[1];
		}
	}
	*ie_len = len;
	return flags;
}

static int itm_wlan_add_cipher_key(wlan_vif_t *vif, bool pairwise,
				   unsigned char key_index, unsigned int cipher,
				   const unsigned char *key_seq,
				   const unsigned char *macaddr)
{
	unsigned char pn_key[16] = { 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36,
		0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36, 0x5c, 0x36
	};
	int ret;
	unsigned char vif_id;
	vif_id = vif->id;
	printkd("%s()\n", __func__);
	if (vif->cfg80211.key_len[pairwise][0]
	    || vif->cfg80211.key_len[pairwise][1]
	    || vif->cfg80211.key_len[pairwise][2]
	    || vif->cfg80211.key_len[pairwise][3]) {
		/* Only set wep keys if we have at least one of them.
		   pairwise: 0:GTK 1:PTK */
		switch (cipher) {
		case WLAN_CIPHER_SUITE_WEP40:
			vif->cfg80211.cipher_type = WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			vif->cfg80211.cipher_type = WEP104;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			vif->cfg80211.cipher_type = TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			vif->cfg80211.cipher_type = CCMP;
			break;
		case WLAN_CIPHER_SUITE_SMS4:
			vif->cfg80211.cipher_type = WAPI;
			break;
		default:
			printkd("Invalid cipher select: %d\n",
				vif->cfg80211.cipher_type);
			return -EINVAL;
		}
		memcpy(vif->cfg80211.key_txrsc[pairwise], pn_key,
		       sizeof(pn_key));
		ret =
		    wlan_cmd_add_key(vif_id,
				     vif->cfg80211.key[pairwise][key_index],
				     vif->cfg80211.key_len[pairwise][key_index],
				     pairwise, key_index, key_seq,
				     vif->cfg80211.cipher_type, macaddr);
		if (ret < 0) {
			printkd("wlan_cmd_add_key failed %d\n", ret);
			return ret;
		}
	}
	return 0;
}

u8 sprdwl_find_ssid_count(wlan_vif_t *vif)
{
	buf_scan_frame_t *scan_buf = NULL;
	int i;
	int count = 0;
	struct wiphy *wiphy = vif->wdev.wiphy;

	if (!vif->cfg80211.scan_frame_array)
		return 0;

	for (i = 0; i < MAX_SCAN_FRAME_BUF_NUM; i++) {
		scan_buf = (buf_scan_frame_t *) (vif->cfg80211.scan_frame_array
						 +
						 i * sizeof(buf_scan_frame_t));

		if (0xff != scan_buf->live)
			continue;

		if (0 == scan_buf->ssid[0])
			continue;

		if (0 !=
		    memcmp(vif->cfg80211.ssid, scan_buf->ssid,
			   vif->cfg80211.ssid_len))
			continue;

		count++;
	}

	wiphy_info(wiphy, "the same ssid num %d with current connect ssid",
		   count);

	return count;
}

static int wlan_cfg80211_scan(struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
			      struct net_device *dev,
#endif
			      struct cfg80211_scan_request *request)
{
	wlan_vif_t *vif;
	unsigned char vif_id;
	struct wireless_dev *wdev;
	struct cfg80211_ssid *ssids;
	struct wlan_cmd_scan_ssid *scan_ssids;
	int scan_ssids_len = 0;
	unsigned char *data = NULL;
	unsigned int i, n, j;
	int ret;
	unsigned char channels[16] = { 0 };

	ssids = request->ssids;
	wdev = request->wdev;
	vif = ndev_to_vif(wdev->netdev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	if (vif->cfg80211.scan_request) {
		printkd("Already scanning\n");
		return -EAGAIN;
	}
	/* check we are client side */
	switch (wdev->iftype) {
	case NL80211_IFTYPE_AP:
		break;
		/*      case NL80211_IFTYPE_P2P_CLIENT:
	case NL80211_IFTYPE_P2P_GO:
		 */
	case NL80211_IFTYPE_STATION:
		break;
	case NL80211_IFTYPE_P2P_CLIENT:
	case NL80211_IFTYPE_P2P_GO:
	case NL80211_IFTYPE_P2P_DEVICE:
		break;
	default:
		{
			printkd("%s(), end\n", __func__);
			return -EOPNOTSUPP;
		}
	}

	/* set wps ie */
	if (request->ie_len > 0) {
		if (request->ie_len > 255) {
			printkd("%s invalid ie len(%d)\n", __func__,
				request->ie_len);
			return -EOPNOTSUPP;
		}
		ret =
		    wlan_cmd_set_wps_ie(vif_id, WPS_REQ_IE, request->ie,
					request->ie_len);
		if (ret) {
			printkd("wlan_cmd_set_wps_ie failed with ret %d\n",
				ret);
			printkd("%s(), end\n", __func__);
			return ret;
		}
	} else {
		printkd("%s request->ie_len is 0\n", __func__);
	}
	n = min(request->n_ssids, 9);
	if (n) {
		data = kzalloc(512, GFP_KERNEL);
		if (!data) {
			printkd("%s failed to alloc for combo ssid\n",
				__func__);
			return -2;
		}
		scan_ssids = (struct wlan_cmd_scan_ssid *)data;
		for (i = 0; i < n; i++) {
			if (!ssids[i].ssid_len)
				continue;
			scan_ssids->len = ssids[i].ssid_len;
			memcpy(scan_ssids->ssid, ssids[i].ssid,
			       ssids[i].ssid_len);
			scan_ssids_len += (ssids[i].ssid_len
					   + sizeof(scan_ssids->len));
			scan_ssids = (struct wlan_cmd_scan_ssid *)
			    (data + scan_ssids_len);
		}
	}

	n = min(request->n_channels, 14);
	if (n > 15)
		n = 15;
	for (i = 0, j = 0; i < n; i++) {
		int ch = request->channels[i]->hw_value;
		if (ch == 0) {
			printkd("Scan requested for unknown frequency %dMhz\n",
				request->channels[i]->center_freq);
			continue;
		}
		channels[j + 1] = ch;
		j++;
	}
	channels[0] = j;

	ret = wlan_cmd_scan(vif_id, data, channels, scan_ssids_len);
	if (ret) {
		printkd("wlan_cmd_scan failed with ret %d\n", ret);
		kfree(data);
		return ret;
	}

	if (vif->cfg80211.scan_done_lock.link.next == LIST_POISON1 ||
	    vif->cfg80211.scan_done_lock.link.prev == LIST_POISON2)
		wake_lock(&vif->cfg80211.scan_done_lock);
	/* Arm scan timeout timer */
	mod_timer(&vif->cfg80211.scan_timeout,
		  jiffies + ITM_SCAN_TIMER_INTERVAL_MS * HZ / 1000);
	vif->cfg80211.scan_request = request;
	kfree(data);
	printkd("%s(), ok!\n", __func__);
	return 0;
}

static int wlan_cfg80211_connect(struct wiphy *wiphy, struct net_device *ndev,
				 struct cfg80211_connect_params *sme)
{
	wlan_vif_t *vif;
	unsigned char vif_id;
	struct wireless_dev *wdev;
	int ret;
	unsigned int cipher = 0;
	unsigned char key_mgmt = 0;
	int is_wep = (sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP40)
	    || (sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP104);
	bool is_wapi = false;
	int auth_type = 0;
	unsigned char *buf = NULL;
	size_t wps_len = 0;
	unsigned short p2p_len = 0;
	size_t ftie_len = 0;
	vif = ndev_to_vif(ndev);
	vif_id = vif->id;
	wdev = &(vif->wdev);
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	printkd("%s(), Begin connect: %s\n", __func__, sme->ssid);

	/* To avoid confused wapi frame */
	vif->cfg80211.cipher_type = NONE;
	/* Get request status, type, bss, ie and so on */
	/* Set appending ie */
	/* Set wps ie */
	if (sme->ie_len > 0) {
		if (sme->ie_len > 255) {
			printkd("%s invalid sme->len(%d)\n", __func__,
				sme->ie_len);
			return -EOPNOTSUPP;
		}
		buf = kmalloc(sme->ie_len, GFP_KERNEL);
		if (NULL == buf) {
			printkd("%s(), end\n", __func__);
			return -ENOMEM;
		}
		if (itm_find_wpsie(sme->ie, sme->ie_len, buf, &wps_len) == true) {
			ret =
			    wlan_cmd_set_wps_ie(vif_id, WPS_ASSOC_IE, buf,
						wps_len);
			if (ret) {
				printkd
				    ("wlan_cmd_set_wps_ie failed with ret %d\n",
				     ret);
				return ret;
			}
		}
	}
#ifdef WIFI_DIRECT_SUPPORT
	if (itm_find_p2p_ie(sme->ie, sme->ie_len, buf, &p2p_len) == true) {
		ret = wlan_cmd_set_p2p_ie(vif_id, P2P_ASSOC_IE, buf, p2p_len);
		if (ret) {
			printkd("wlan_cmd_set_p2p_ie failed with ret %d\n",
				ret);
			return ret;
		}
	}
#endif /*WIFI_DIRECT_SUPPORT */
#ifdef WLAN_11R_SUPPORT
	if (itm_find_ft_ie(sme->ie, sme->ie_len, buf, &ftie_len)) {
		ret = wlan_cmd_set_ft_ie(vif_id, buf, ftie_len);
		if (ret) {
			printkd("wlan_cmd_set_ft_ie failed with ret %d\n", ret);
		}
	}
#endif
	/* Set WPA version */
	printkd("Set wpa_versions %#x\n", sme->crypto.wpa_versions);
	ret = wlan_cmd_set_wpa_version(vif_id, sme->crypto.wpa_versions);
	if (ret < 0) {
		printkd("wlan_cmd_set_wpa_version failed with ret %d\n", ret);
		printkd("%s(), end\n", __func__);
		return ret;
	}

	/* Set Auth type */
	printkd("Set auth_type %#x\n", sme->auth_type);
	/* Set the authorisation */
	if ((sme->auth_type == NL80211_AUTHTYPE_OPEN_SYSTEM) ||
	    ((sme->auth_type == NL80211_AUTHTYPE_AUTOMATIC) && !is_wep))
		auth_type = ITM_AUTH_OPEN;
	else if ((sme->auth_type == NL80211_AUTHTYPE_SHARED_KEY) ||
		 ((sme->auth_type == NL80211_AUTHTYPE_AUTOMATIC) && is_wep))
		auth_type = ITM_AUTH_SHARED;
	ret = wlan_cmd_set_auth_type(vif_id, auth_type);
	if (ret < 0) {
		printkd("wlan_cmd_set_auth_type failed with ret %d\n", ret);
		return ret;
	}
	/* Set cipher - pairewise and group */
	printkd("n_ciphers_pairwise %d\n", sme->crypto.n_ciphers_pairwise);
	if (sme->crypto.n_ciphers_pairwise) {
		switch (sme->crypto.ciphers_pairwise[0]) {
		case WLAN_CIPHER_SUITE_WEP40:
			cipher = WEP40;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			cipher = WEP104;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			cipher = TKIP;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			cipher = CCMP;
			break;
			/* WAPI cipher is not processed by CP2 */
		case WLAN_CIPHER_SUITE_SMS4:
			cipher = WAPI;
			is_wapi = true;
			break;
		default:
			printkd("Unicast cipher suite 0x%x is not supported\n",
				sme->crypto.ciphers_pairwise[0]);
			printkd("%s(), end\n", __func__);
			return -ENOTSUPP;
		}

		if (is_wapi != true) {
			ret =
			    wlan_cmd_set_cipher(vif_id, cipher,
						WIFI_CMD_SET_PAIRWISE_CIPHER);
			if (ret < 0) {
				printkd
				    ("set_cipher_cmd pairwise failed with ret %d\n",
				     ret);
				printkd("%s(), end\n", __func__);
				return ret;
			}
		}
	} else {
		/*No pairewise cipher */
		printkd("No pairewise cipher\n");
	}

	/* Set group cipher */
	switch (sme->crypto.cipher_group) {
	case NONE:
		cipher = NONE;
		break;
	case WLAN_CIPHER_SUITE_WEP40:
		cipher = WEP40;
		break;
	case WLAN_CIPHER_SUITE_WEP104:
		cipher = WEP104;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		cipher = TKIP;
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		cipher = CCMP;
		break;
	/* WAPI cipher is not processed by CP2 */
	case WLAN_CIPHER_SUITE_SMS4:
		cipher = WAPI;
		is_wapi = true;
		break;
	default:
		printkd("Group cipher suite 0x%x is not supported\n",
			sme->crypto.cipher_group);
		printkd("%s(), end\n", __func__);
		return -ENOTSUPP;
	}

	if (is_wapi != true) {
		ret =
		    wlan_cmd_set_cipher(vif_id, cipher,
					WIFI_CMD_SET_GROUP_CIPHER);
		if (ret < 0) {
			printkd("set_cipher_cmd group failed with ret %d\n",
				ret);
			printkd("%s(), end\n", __func__);
			return ret;
		}
	}

	/* FIXME */
	/* Set Auth type again because of CP2 process's differece */
	printkd("Set auth_type %#x\n", sme->auth_type);
	/* Set the authorisation */
	if ((sme->auth_type == NL80211_AUTHTYPE_OPEN_SYSTEM) ||
	    ((sme->auth_type == NL80211_AUTHTYPE_AUTOMATIC) && !is_wep))
		auth_type = ITM_AUTH_OPEN;
	else if ((sme->auth_type == NL80211_AUTHTYPE_SHARED_KEY) ||
		 ((sme->auth_type == NL80211_AUTHTYPE_AUTOMATIC) && is_wep))
		auth_type = ITM_AUTH_SHARED;
	ret = wlan_cmd_set_auth_type(vif_id, auth_type);
	if (ret < 0) {
		printkd("wlan_cmd_set_auth_type failed with ret %d\n", ret);
		printkd("%s(), end\n", __func__);
		return ret;
	}

	/* Set auth key management (akm) */
	printkd("akm_suites %#x\n", sme->crypto.n_akm_suites);
	if (sme->crypto.n_akm_suites) {
		if (sme->crypto.akm_suites[0] == WLAN_AKM_SUITE_PSK)
			key_mgmt = AKM_SUITE_PSK;
		else if (WLAN_AKM_SUITE_FT_PSK == sme->crypto.akm_suites[0])
			key_mgmt = AKM_SUITE_FT_PSK;
		else if (sme->crypto.akm_suites[0] == WLAN_AKM_SUITE_8021X)
			key_mgmt = AKM_SUITE_8021X;
		/* WAPI akm is not processed by CP2 */
		else if (sme->crypto.akm_suites[0] == WLAN_AKM_SUITE_WAPI_CERT)
			key_mgmt = AKM_SUITE_WAPI_CERT;
		else if (sme->crypto.akm_suites[0] == WLAN_AKM_SUITE_WAPI_PSK)
			key_mgmt = AKM_SUITE_WAPI_PSK;
		else if (WLAN_AKM_SUITE_FT_8021X == sme->crypto.akm_suites[0])
			key_mgmt = AKM_SUITE_FT_8021X;
		else {
		}
		ret = wlan_cmd_set_key_management(vif_id, key_mgmt);
		if (ret < 0) {
			printkd("wlan_cmd_set_key_management failed %d\n", ret);
			printkd("%s(), end\n", __func__);
			return ret;
		}
	}

	/* Set PSK */
	if (sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP40 ||
	    sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP104 ||
	    sme->crypto.ciphers_pairwise[0] == WLAN_CIPHER_SUITE_WEP40 ||
	    sme->crypto.ciphers_pairwise[0] == WLAN_CIPHER_SUITE_WEP104) {
		printkd("Don't need to set PSK since driver is using WEP\n");
		vif->cfg80211.key_index[GROUP] = sme->key_idx;
		vif->cfg80211.key_len[GROUP][sme->key_idx] = sme->key_len;
		memcpy(vif->cfg80211.key[GROUP][sme->key_idx], sme->key,
		       sme->key_len);
		ret =
		    itm_wlan_add_cipher_key(vif, 0, sme->key_idx,
					    sme->crypto.ciphers_pairwise[0],
					    NULL, NULL);
		if (ret < 0) {
			printkd("itm_wlan_add_key failed %d\n", ret);
			printkd("%s(), end\n", __func__);
			return ret;
		}
	} else {
		unsigned char psk[32];
		int key_len = 0;
		if (wdev->iftype == NL80211_IFTYPE_AP) {
			ret = hostap_conf_load(HOSTAP_CONF_FILE_NAME, psk);
			if (ret) {
				printkd("load hostap failed with ret %d\n",
					ret);
				printkd("%s(), end\n", __func__);
				return ret;
			}
			key_len = sizeof(psk);
		} else {
			if (sme->key_len > 32) {
				printkd("Invalid key len (%d)\n", sme->key_len);
				printkd("%s(), end\n", __func__);
				return -EINVAL;
			}
			memcpy(psk, sme->key, sme->key_len);
			key_len = sme->key_len;
		}
		ret = wlan_cmd_set_psk(vif_id, psk, key_len);
		if (ret < 0) {
			printkd("set_psk_cmd failed with ret %d\n", ret);
			printkd("%s(), end\n", __func__);
			return ret;
		}
	}

	/* Auth RX unencrypted EAPOL is not implemented, do nothing */
	/* Set channel */
	if (sme->channel != NULL) {
		printkd("Settting channel to %d\n",
			ieee80211_frequency_to_channel(sme->channel->
						       center_freq));
		ret =
		    wlan_cmd_set_channel(vif_id,
					 ieee80211_frequency_to_channel(sme->
									channel->
									center_freq));
		if (ret < 0) {
			printkd("wlan_cmd_set_channel failed with ret %d\n",
				ret);
			printkd("%s(), end\n", __func__);
			return ret;
		}
	} else {
		printkd("Channel is not specified\n");
	}

	/* Set BSSID */
	if (sme->bssid != NULL) {
		ret = wlan_cmd_set_bssid(vif_id, sme->bssid);
		if (ret < 0) {
			printkd("wlan_cmd_set_bssid failed with ret %d\n", ret);
			printkd("%s(), end\n", __func__);
			return ret;
		}
		memcpy(vif->cfg80211.bssid, sme->bssid, 6);
	} else {
		printkd("BSSID is not specified\n");
	}

	/* Special process for WEP(WEP key must be set before itm_set_essid) */
	if (sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP40 ||
	    sme->crypto.cipher_group == WLAN_CIPHER_SUITE_WEP104) {
		printkd("Setting WEP group cipher\n");
		if (sme->key_len <= 0) {
			printkd("No key is specified\n");
		} else {
			if (sme->key_len != WLAN_KEY_LEN_WEP104 &&
			    sme->key_len != WLAN_KEY_LEN_WEP40) {
				printkd("Invalid key length for WEP\n");
				printkd("%s(), end\n", __func__);
				return -EINVAL;
			}

			wlan_cmd_set_key(vif_id, sme->key_idx);
		}
	}
	/* Set ESSID */
	if (sme->ssid != NULL) {
		printkd("sme->ssid:%s\n", sme->ssid);
		ret = wlan_cmd_set_essid(vif_id, sme->ssid, (int)sme->ssid_len);
		if (ret < 0) {
			printkd("wlan_cmd_set_essid failed with ret %d\n", ret);
			printkd("%s(), end\n", __func__);
			return ret;
		}
		memcpy(vif->cfg80211.ssid, sme->ssid, sme->ssid_len);
		vif->cfg80211.ssid_len = sme->ssid_len;
	}
	vif->cfg80211.connect_status = ITM_CONNECTING;
	printkd("%s(), ok\n", __func__);
	return ret;
}

static int wlan_cfg80211_disconnect(struct wiphy *wiphy,
				    struct net_device *ndev,
				    unsigned short reason_code)
{
	wlan_vif_t *vif;
	unsigned char vif_id;
	struct cfg80211_bss *bss = NULL;
	bool found = false;
	int ret;
	vif = ndev_to_vif(ndev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	printkd("Begin disconnect: %s\n", vif->cfg80211.ssid);

	ret = wlan_cmd_disconnect(vif_id, reason_code);
	if (ret < 0) {
		printkd("swifi_disconnect_cmd failed with ret %d\n", ret);
	}
	memset(vif->cfg80211.ssid, 0, sizeof(vif->cfg80211.ssid));
	return ret;
}

static int wlan_cfg80211_add_key(struct wiphy *wiphy,
				 struct net_device *netdev, u8 idx,
				 bool pairwise, const u8 *mac_addr,
				 struct key_params *params)
{
	wlan_vif_t *vif;
	unsigned char vif_id;
	int ret;
	unsigned char key[32];
	vif = ndev_to_vif(netdev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);

	vif->cfg80211.key_index[pairwise] = idx;
	vif->cfg80211.key_len[pairwise][idx] = params->key_len;
	memcpy(vif->cfg80211.key[pairwise][idx], params->key, params->key_len);
	ret =
	    itm_wlan_add_cipher_key(vif, pairwise, idx, params->cipher,
				    params->seq, mac_addr);
	if (ret < 0) {
		printkd("%s failed to add cipher key!\n", __func__);
		return ret;
	}

	return 0;
}

static int wlan_cfg80211_del_key(struct wiphy *wiphy,
				 struct net_device *ndev,
				 unsigned char key_index, bool pairwise,
				 const unsigned char *mac_addr)
{
	wlan_vif_t *vif;
	unsigned char vif_id;

	vif = ndev_to_vif(ndev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	if (key_index > WLAN_MAX_KEY_INDEX) {
		printkd("key index %d out of bounds\n", key_index);
		return -ENOENT;
	}
	if (!vif->cfg80211.key_len[pairwise][key_index]) {
		printkd("index %d is empty\n", key_index);
		return 0;
	}
	vif->cfg80211.key_len[pairwise][key_index] = 0;
	vif->cfg80211.cipher_type = NONE;

	return wlan_cmd_del_key(vif_id, key_index, mac_addr);
}

static int wlan_cfg80211_set_default_key(struct wiphy *wiphy,
					 struct net_device *ndev,
					 unsigned char key_index, bool unicast,
					 bool multicast)
{
	int ret;
	wlan_vif_t *vif;
	unsigned char vif_id;
	vif = ndev_to_vif(ndev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	if (key_index < 0 || key_index > 3) {
		printkd("Invalid key index %d\n", key_index);
		return -EINVAL;
	}
	ret = wlan_cmd_set_key(vif_id, key_index);
	if (ret < 0) {
		printkd("wlan_cmd_set_key failed\n");
		return ret;
	}

	return 0;
}

static int wlan_cfg80211_set_wiphy_params(struct wiphy *wiphy,
					  unsigned int changed)
{
	int ret;
	wlan_vif_t *vif;
	unsigned char vif_id;
	vif_id = NETIF_0_ID;
	vif = id_to_vif(vif_id);
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	if (changed & WIPHY_PARAM_RTS_THRESHOLD) {
		ret = wlan_cmd_set_rts(vif_id, wiphy->rts_threshold);
		if (ret != 0) {
			printkd("wlan_cmd_set_rts failed\n");
			return -EIO;
		}
	}

	if (changed & WIPHY_PARAM_FRAG_THRESHOLD) {
		ret = wlan_cmd_set_frag(vif_id, wiphy->frag_threshold);
		if (ret != 0) {
			printkd("wlan_cmd_set_frag failed\n");
			return -EIO;
		}
	}
	return 0;
}

static int wlan_cfg80211_get_station(struct wiphy *wiphy,
				     struct net_device *dev, unsigned char *mac,
				     struct station_info *sinfo)
{
	unsigned char signal, noise;
	int rate, ret, i, failed;
	wlan_vif_t *vif;
	unsigned char vif_id;
	static int cfg80211_get_station_time;
	static char cfg80211_get_station_signal;
	static int cfg80211_get_station_txrate;
	static int cfg80211_get_station_txfailed;

	vif = ndev_to_vif(dev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	if (NULL == sinfo) {
		printke("[%s][sinfo null]\n", __func__);
		return -EAGAIN;
	}

	sinfo->filled |=
	    STATION_INFO_TX_BYTES | STATION_INFO_TX_PACKETS |
	    STATION_INFO_RX_BYTES | STATION_INFO_RX_PACKETS;
	sinfo->tx_bytes = vif->ndev->stats.tx_bytes;
	sinfo->tx_packets = vif->ndev->stats.tx_packets;
	sinfo->rx_bytes = vif->ndev->stats.rx_bytes;
	sinfo->rx_packets = vif->ndev->stats.rx_packets;

	if (0 != cfg80211_get_station_time) {
		sinfo->signal = cfg80211_get_station_signal;
		sinfo->filled |= STATION_INFO_SIGNAL;
		sinfo->txrate.legacy = cfg80211_get_station_txrate;
		sinfo->filled |= STATION_INFO_TX_BITRATE;
		sinfo->tx_failed = cfg80211_get_station_txfailed;
		sinfo->filled |= STATION_INFO_TX_FAILED;
		if (2 == cfg80211_get_station_time) {
			cfg80211_get_station_time = 0;
		} else {
			cfg80211_get_station_time++;
		}
		return 0;
	}

	ret = wlan_cmd_get_rssi(vif_id, &signal, &noise);
	if (OK == ret) {
		sinfo->signal = signal;
		sinfo->filled |= STATION_INFO_SIGNAL;
	} else {
		printkd("wlan_cmd_get_rssi error!\n");
		return -EIO;
	}

	ret = wlan_cmd_get_txrate_txfailed(vif_id, &rate, &failed);
	if (OK == ret) {
		sinfo->tx_failed = failed;
		sinfo->filled |=
		    STATION_INFO_TX_BITRATE | STATION_INFO_TX_FAILED;
	} else {
		printkd("wlan_cmd_get_txrate_txfailed error!\n");
		return -EIO;
	}

	if (!(rate & 0x7f)) {
		sinfo->txrate.legacy = 10;
	} else {
		for (i = 0; i < ARRAY_SIZE(itm_rates); i++) {
			if (rate == itm_rates[i].hw_value) {
				sinfo->txrate.legacy = itm_rates[i].bitrate;
				if (rate & 0x80)
					sinfo->txrate.mcs =
					    itm_rates[i].hw_value;
				break;
			}
		}
		if (i >= ARRAY_SIZE(itm_rates))
			sinfo->txrate.legacy = 10;
	}
	cfg80211_get_station_signal = sinfo->signal;
	cfg80211_get_station_txrate = sinfo->txrate.legacy;
	cfg80211_get_station_txfailed = sinfo->tx_failed;
	cfg80211_get_station_time++;

	return 0;
}

static int wlan_cfg80211_set_pmksa(struct wiphy *wiphy,
				   struct net_device *netdev,
				   struct cfg80211_pmksa *pmksa)
{
	int ret;
	wlan_vif_t *vif;
	unsigned char vif_id;
	vif = ndev_to_vif(netdev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	ret =
	    wlan_cmd_pmksa(vif_id, pmksa->bssid, pmksa->pmkid,
			   WIFI_CMD_SET_PMKSA);
	return ret;
}

static int wlan_cfg80211_del_pmksa(struct wiphy *wiphy,
				   struct net_device *netdev,
				   struct cfg80211_pmksa *pmksa)
{
	int ret;
	wlan_vif_t *vif;
	unsigned char vif_id;
	vif = ndev_to_vif(netdev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	ret =
	    wlan_cmd_pmksa(vif_id, pmksa->bssid, pmksa->pmkid,
			   WIFI_CMD_DEL_PMKSA);
	return ret;
}

static int wlan_cfg80211_flush_pmksa(struct wiphy *wiphy,
				     struct net_device *netdev)
{
	int ret;
	wlan_vif_t *vif;
	unsigned char vif_id;
	vif = ndev_to_vif(netdev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	ret =
	    wlan_cmd_pmksa(vif_id, vif->cfg80211.bssid, NULL,
			   WIFI_CMD_FLUSH_PMKSA);
	return ret;
}

void cfg80211_report_connect_result(unsigned char vif_id, unsigned char *pData,
				    int len)
{
	unsigned char *req_ie_ptr, *resp_ie_ptr, *bssid_ptr, *pos, *value_ptr;
	unsigned char status_code;
	unsigned short bssid_len;
	unsigned char req_ie_len;
	unsigned short resp_ie_len;
	unsigned int event_len;
	int left;
	unsigned char reassociate_rsp_flag = 0;

	wlan_vif_t *vif = id_to_vif(vif_id);
	printkd("%s(), enter\n", __func__);
	event_len = len;
	/* status_len 2 + status_code 1 = 3 bytes */
	if (event_len < 3) {
		printkd("filled event len(%d) is not a valid len\n", event_len);
		goto out;
	}
	pos = kmalloc(event_len, GFP_KERNEL);
	if (pos == NULL) {
		printkd("[%s][%d][%d]\n", __func__, __LINE__, event_len);
		if (event_len > 16384)
			BUG_ON(1);
		goto out;
	}
	/* The first byte of event data is status and len */
	memcpy(pos, pData, event_len);
	/* msg byte format
	 * byte [0] the length for status_code,here is 1
	 * byte [1] reassociate_rsp_flag
	 * byte [2] status_code
	 * ..... other data
	 */
	if (1 != *pos) {
		ASSERT("msg first byte err:%d != 1", (int)*pos);
		kfree(pos);
		goto out;
	}
	reassociate_rsp_flag = *(pos + 1);
	status_code = *(pos + 2);

	/* FIXME later the status code should be reported by CP2 */
	if (status_code != 0) {
		printkd("%s, Connect is failled (%d)\n", __func__, status_code);
		kfree(pos);
		goto out;
	}

	value_ptr = pos + 3;
	left = event_len - 3;
	/* BSSID is 6 + len is 2 = 8 */
	if (left < 8) {
		printkd("%s(), Do not have a vaild bssid\n", __func__);
		kfree(pos);
		goto out;
	}
	memcpy(&bssid_len, value_ptr, 2);
	left -= 2;
	bssid_ptr = value_ptr + 2;
	left -= bssid_len;

	if (!left) {
		printkd("%s(), There is no req_ie frame!\n", __func__);
		kfree(pos);
		goto out;
	}
	req_ie_len = *(unsigned char *)(bssid_ptr + bssid_len);
	left -= 1;
	req_ie_ptr = bssid_ptr + bssid_len + 1;
	left -= req_ie_len;
	if (!left) {
		printkd("%s(), There is no resp_ie frame!\n", __func__);
		kfree(pos);
		goto out;
	}
	resp_ie_len =
	    *(unsigned char *)(req_ie_ptr + req_ie_len) +
	    *(unsigned char *)(req_ie_ptr + req_ie_len + 1);
	resp_ie_ptr = req_ie_ptr + req_ie_len + 2;

	if ((vif->cfg80211.connect_status == ITM_CONNECTING)
	    || (1 == reassociate_rsp_flag)) {
		/* inform connect result to cfg80211 */
		vif->cfg80211.connect_status = ITM_CONNECTED;
		if (1 == reassociate_rsp_flag) {
			vif->wdev.sme_state = CFG80211_SME_CONNECTING;
		}
		cfg80211_connect_result(vif->ndev, bssid_ptr, req_ie_ptr,
					req_ie_len, resp_ie_ptr, resp_ie_len,
					WLAN_STATUS_SUCCESS, GFP_KERNEL);
		kfree(pos);
		if (!netif_carrier_ok(vif->ndev)) {
			printkd("%s(), netif_carrier_on, ssid:%s\n", __func__,
				vif->cfg80211.ssid);
			netif_carrier_on(vif->ndev);
			netif_wake_queue(vif->ndev);
		}
	}
	printkd("%s(), ok\n", __func__);
	return;
out:
	if (vif->cfg80211.scan_request
	    && (atomic_add_unless(&vif->cfg80211.scan_status, 1, 1) == 1)) {
		del_timer_sync(&vif->cfg80211.scan_timeout);
		cfg80211_scan_done(vif->cfg80211.scan_request, true);
		vif->cfg80211.scan_request = NULL;
		if (vif->cfg80211.scan_done_lock.link.next != LIST_POISON1 &&
		    vif->cfg80211.scan_done_lock.link.prev != LIST_POISON2)
			wake_unlock(&vif->cfg80211.scan_done_lock);
		atomic_dec(&vif->cfg80211.scan_status);
	}
	if (vif->cfg80211.connect_status == ITM_CONNECTING) {
		cfg80211_connect_result(vif->ndev, vif->cfg80211.bssid, NULL, 0,
					NULL, 0,
					WLAN_STATUS_UNSPECIFIED_FAILURE,
					GFP_KERNEL);
	} else if (vif->cfg80211.connect_status == ITM_CONNECTED) {
		cfg80211_disconnected(vif->ndev, status_code, NULL, 0,
				      GFP_KERNEL);
	}

	printkd("%s(), err\n", __func__);
	return;
}

void cfg80211_report_disconnect_done(unsigned char vif_id, unsigned char *pData,
				     int len)
{
	struct cfg80211_bss *bss = NULL;
	unsigned short reason_code = 0;
	bool found = false;
	wlan_vif_t *vif = id_to_vif(vif_id);
	printkd("%s()\n", __func__);
	/* This should filled if disconnect reason is not only one */
	memcpy(&reason_code, pData, 2);
	if (vif->cfg80211.scan_request
	    && (atomic_add_unless(&vif->cfg80211.scan_status, 1, 1) == 1)) {
		del_timer_sync(&vif->cfg80211.scan_timeout);
		cfg80211_scan_done(vif->cfg80211.scan_request, true);
		vif->cfg80211.scan_request = NULL;
		if (vif->cfg80211.scan_done_lock.link.next != LIST_POISON1
		    && vif->cfg80211.scan_done_lock.link.prev != LIST_POISON2)
			wake_unlock(&vif->cfg80211.scan_done_lock);
		atomic_dec(&vif->cfg80211.scan_status);
	}
	if (vif->cfg80211.connect_status == ITM_CONNECTING) {
		cfg80211_connect_result(vif->ndev,
					vif->cfg80211.bssid, NULL, 0,
					NULL, 0,
					WLAN_STATUS_UNSPECIFIED_FAILURE,
					GFP_KERNEL);
	} else if (vif->cfg80211.connect_status == ITM_CONNECTED) {
		if (reason_code == AP_LEAVING	/*||
						   reason_code == AP_DEAUTH */) {
			do {
				bss = cfg80211_get_bss(vif->wdev.wiphy, NULL,
						       vif->cfg80211.bssid,
						       vif->cfg80211.ssid,
						       vif->cfg80211.ssid_len,
						       WLAN_CAPABILITY_ESS,
						       WLAN_CAPABILITY_ESS);
				if (bss) {
					cfg80211_unlink_bss(vif->wdev.wiphy,
							    bss);
					found = true;
				} else {
					found = false;
				}
			}
			while (found)
				;
		}
		cfg80211_disconnected(vif->ndev, reason_code,
				      NULL, 0, GFP_KERNEL);
	}

	vif->cfg80211.connect_status = ITM_DISCONNECTED;
	if (netif_carrier_ok(vif->ndev)) {
		printkd("netif_carrier_off\n");
		netif_carrier_off(vif->ndev);
		netif_stop_queue(vif->ndev);
	}
	return;
}

static void wlan_scan_timeout(unsigned long data)
{
	wlan_vif_t *vif = (wlan_vif_t *) data;

	printkd("%s()\n", __func__);
	if (vif->cfg80211.scan_request
	    && (atomic_add_unless(&vif->cfg80211.scan_status, 1, 1) == 1)) {
		printkd("scan timer expired!\n");
		cfg80211_scan_done(vif->cfg80211.scan_request, true);
		vif->cfg80211.scan_request = NULL;
		if (vif->cfg80211.scan_done_lock.link.next != LIST_POISON1
		    && vif->cfg80211.scan_done_lock.link.prev != LIST_POISON2)
			wake_unlock(&vif->cfg80211.scan_done_lock);
		atomic_dec(&vif->cfg80211.scan_status);
		printkd("%s()  end\n", __func__);

		return;
	}

	printkd("%s()  end, wrong scan timer expired!\n", __func__);
	return;
}

void cfg80211_report_scan_done(unsigned char vif_id, unsigned char *pData,
			       int len, bool aborted)
{
	struct ieee80211_mgmt *mgmt;
	struct ieee80211_channel *channel;
	struct ieee80211_supported_band *band;
	struct wiphy *wiphy;
	struct cfg80211_bss *itm_bss = NULL;
	unsigned char ssid[33] = { 0 };
	unsigned char bssid[6] = { 0 };
	unsigned int mgmt_len = 0;
	unsigned short channel_num, channel_len;
	unsigned short rssi_len;
	short rssi;
	int signal;
	int freq;
	unsigned int left = len;
	wlan_vif_t *vif = id_to_vif(vif_id);
	const unsigned char *pos = pData;

	wiphy = vif->wdev.wiphy;
	printkd("%s()\n", __func__);
	if (atomic_add_unless(&vif->cfg80211.scan_status, 1, 1) == 0) {
		printkd("scan event is aborted\n");
		return;
	}

	if (!vif->cfg80211.scan_request) {
		printkd("vif->cfg80211.scan_request is null\n");
		atomic_dec(&vif->cfg80211.scan_status);
		return;
	}

	if (left < 10 || aborted) {
		printkd("filled event len(%d) is not a valid len\n", len);
		goto out;
	}

	mgmt = kmalloc(left, GFP_KERNEL);
	if (mgmt == NULL) {
		printkd("[%s][%d]\n", __func__, left);
		goto out;
	}
	while (left >= 10) {
		/* must use memcpy to protect unaligned */
		/* The formate of frame is len(two bytes) + data */
		memcpy(&channel_len, pos, 2);
		pos += 2;
		left -= 2;
		if (channel_len > 2) {
			ASSERT("channel_len %u > 2\n", channel_len);
			kfree(mgmt);
			goto out;
		}
		memcpy(&channel_num, pos, channel_len);
		pos += channel_len;
		left -= channel_len;
		/* The second two value of frame is rssi */
		memcpy(&rssi_len, pos, 2);
		pos += 2;
		left -= 2;
		memcpy(&rssi, pos, rssi_len);
		pos += rssi_len;
		left -= rssi_len;
		/* The third two value of frame is following data len */
		memcpy(&mgmt_len, pos, 2);
		pos += 2;
		left -= 2;

		if (mgmt_len > left) {
			printkd("mgmt_len(0x%08x) > left(0x%08x)!\n", mgmt_len,
				left);
			kfree(mgmt);
			goto out;
		}

		/* The following is real data */
		memcpy(mgmt, pos, mgmt_len);
		left -= mgmt_len;
		pos += mgmt_len;

		/* FIXME Now only support 2GHZ */
		band = wiphy->bands[IEEE80211_BAND_2GHZ];
		freq = ieee80211_channel_to_frequency(channel_num, band->band);
		channel = ieee80211_get_channel(wiphy, freq);
		if (!channel) {
			printkd("freq is %d\n", freq);
			continue;
		}
		signal = rssi;
		/*printkd("[report][%s][%d][%d]\n",ssid, channel_num, signal);*/
		itm_bss =
		    cfg80211_inform_bss_frame(wiphy, channel, mgmt,
					      le16_to_cpu(mgmt_len), signal,
					      GFP_KERNEL);

		if (unlikely(!itm_bss))
			printkd("cfg80211_inform_bss_frame error\n");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
		cfg80211_put_bss(wiphy, itm_bss);
#else
		cfg80211_put_bss(itm_bss);
#endif
	}

	if (left) {
		kfree(mgmt);
		goto out;
	}

	kfree(mgmt);
	del_timer_sync(&vif->cfg80211.scan_timeout);
	cfg80211_scan_done(vif->cfg80211.scan_request, aborted);
	vif->cfg80211.scan_request = NULL;
	if (vif->cfg80211.scan_done_lock.link.next != LIST_POISON1
	    && vif->cfg80211.scan_done_lock.link.prev != LIST_POISON2)
		wake_unlock(&vif->cfg80211.scan_done_lock);
	atomic_dec(&vif->cfg80211.scan_status);

	printkd("%s(), ok!\n", __func__);
	return;

out:
	del_timer_sync(&vif->cfg80211.scan_timeout);
	cfg80211_scan_done(vif->cfg80211.scan_request, true);
	vif->cfg80211.scan_request = NULL;
	if (vif->cfg80211.scan_done_lock.link.next != LIST_POISON1 &&
	    vif->cfg80211.scan_done_lock.link.prev != LIST_POISON2)
		wake_unlock(&vif->cfg80211.scan_done_lock);
	atomic_dec(&vif->cfg80211.scan_status);

	printkd("%s(), err\n", __func__);
	return;
}

void cfg80211_report_scan_frame(unsigned char vif_id, unsigned char *pData,
				int len)
{
	struct cfg80211_bss *bss = NULL;
	int i, report_null;
	wlan_vif_t *vif;
	buf_scan_frame_t *scan_buf;
	wlan_event_scan_rsp_t *event;
	unsigned char *msa;
	unsigned char ssid[33] = { 0 };
	unsigned char bssid[6] = { 0 };

	struct ieee80211_channel *channel = NULL;
	struct ieee80211_supported_band *band = NULL;
	struct wiphy *wiphy = NULL;
	struct cfg80211_bss *itm_bss = NULL;
	struct ieee80211_mgmt *mgmt = NULL;
	u8 *ie;
	size_t ielen;
	u16 capability, beacon_interval;
	int freq, signal;
	u64 tsf;

	vif = id_to_vif(vif_id);
	wiphy = vif->wdev.wiphy;
	event = (wlan_event_scan_rsp_t *) (pData);
	if (((event->frame_len + sizeof(wlan_event_scan_rsp_t)) > len)
	    || (event->ops > 2)) {
		printkd("[%s %d][line %d err]\n", __func__, vif_id, __LINE__);
		return;
	}
	if (0 == event->ops) {
		if (event->frame_len < 37) {
			printkd("[%s %d][line %d err]\n", __func__, vif_id,
				__LINE__);
			return;
		}
		msa = (unsigned char *)(event + 1);
		get_ssid(msa, ssid);
		get_bssid(msa, bssid);
		if (0 == strlen(ssid)) {
			printkd("[%s %d][line %d err]\n", __func__, vif_id,
				__LINE__);
			return;
		}
		if (1024 < event->frame_len) {
			printkd("[%s %d][line %d err]\n", __func__, vif_id,
				__LINE__);
			return;
		}
		for (i = 0; i < MAX_SCAN_FRAME_BUF_NUM; i++) {
			scan_buf =
			    (buf_scan_frame_t *) (vif->cfg80211.
						  scan_frame_array +
						  i * sizeof(buf_scan_frame_t));
			if (0xff != scan_buf->live)
				continue;
			if (0 != memcmp(bssid, scan_buf->bssid, 6))
				continue;
			strcpy(scan_buf->ssid, ssid);
			memcpy(scan_buf->msa, msa, event->frame_len);
			scan_buf->msa_len = event->frame_len;
			scan_buf->channel = event->channel;
			scan_buf->signal = event->signal;
			scan_buf->live = 0xff;
			scan_buf->keep = 1;
			return;
		}
		for (i = 0; i < MAX_SCAN_FRAME_BUF_NUM; i++) {
			scan_buf =
			    (buf_scan_frame_t *) (vif->cfg80211.
						  scan_frame_array +
						  i * sizeof(buf_scan_frame_t));
			if (0xff == scan_buf->live)
				continue;
			memcpy(scan_buf->bssid, bssid, 6);
			strcpy(scan_buf->ssid, ssid);
			memcpy(scan_buf->msa, msa, event->frame_len);
			scan_buf->msa_len = event->frame_len;
			scan_buf->channel = event->channel;
			scan_buf->signal = event->signal;
			scan_buf->keep = 1;
			scan_buf->live = 0xff;
			printkd("[netif:%d find_ssid][%s][%d][%d]\n", vif_id,
				scan_buf->ssid, scan_buf->channel,
				scan_buf->signal);
			return;
		}
		return;
	}
	if (1 != event->ops) {
		printkd("[%s %d][line %d err]\n", __func__, vif_id, __LINE__);
		return;
	}
	if (!vif->cfg80211.scan_request) {
		printkd("[%s %d][line %d err]\n", __func__, vif_id, __LINE__);
		atomic_dec(&vif->cfg80211.scan_status);
		return;
	}
	if (atomic_add_unless(&vif->cfg80211.scan_status, 1, 1) == 0) {
		printkd("[%s %d][line %d err]\n", __func__, vif_id, __LINE__);
		return;
	}
	if (!vif->cfg80211.scan_request) {
		printkd("[%s %d][line %d err]\n", __func__, vif_id, __LINE__);
		atomic_dec(&vif->cfg80211.scan_status);
		return;
	}
	report_null = -1;
	band = wiphy->bands[IEEE80211_BAND_2GHZ];
	for (i = 0; i < MAX_SCAN_FRAME_BUF_NUM; i++) {
		scan_buf =
		    (buf_scan_frame_t *) (vif->cfg80211.scan_frame_array +
					  i * sizeof(buf_scan_frame_t));
		if (0xff != scan_buf->live)
			continue;
		if (0 == scan_buf->keep) {
			printkd("[netif:%d leave_ssid][%s][%d][%d]\n", vif_id,
				scan_buf->ssid, event->channel, event->signal);
			memset((char *)scan_buf, 0, sizeof(buf_scan_frame_t));
			continue;
		}
		scan_buf->keep--;
		freq =
		    ieee80211_channel_to_frequency(scan_buf->channel,
						   band->band);
		channel = ieee80211_get_channel(wiphy, freq);
		if (!channel) {
			printkd("[%s %d][line %d err]\n", __func__, vif_id,
				__LINE__);
			continue;
		}
		mgmt = (struct ieee80211_mgmt *)(&scan_buf->msa[0]);
		tsf = le64_to_cpu(mgmt->u.probe_resp.timestamp);
		capability = le16_to_cpu(mgmt->u.probe_resp.capab_info);
		beacon_interval = le16_to_cpu(mgmt->u.probe_resp.beacon_int);
		ie = mgmt->u.probe_resp.variable;
		ielen =
		    le16_to_cpu(scan_buf->msa_len) -
		    offsetof(struct ieee80211_mgmt, u.probe_resp.variable);
		signal = scan_buf->signal;
		signal = signal * 100;
		wiphy_info(wiphy, "   %s, " MACSTR ", channel %2u, signal %d\n",
			   ieee80211_is_probe_resp(mgmt->frame_control)
			   ? "proberesp" : "beacon   ",
			   MAC2STR(mgmt->bssid), scan_buf->channel,
			   scan_buf->signal);
		itm_bss =
		    cfg80211_inform_bss(wiphy, channel, mgmt->bssid, tsf,
					capability, beacon_interval, ie, ielen,
					signal, GFP_KERNEL);
		if (unlikely(!itm_bss))
			printkd("[%s %d][line %d err]\n", __func__, vif_id,
				__LINE__);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
		cfg80211_put_bss(wiphy, itm_bss);
#else
		cfg80211_put_bss(itm_bss);
#endif
		report_null = 0;
	}

	if (vif->beacon_loss) {
		bss = cfg80211_get_bss(wiphy, NULL,
				       vif->cfg80211.bssid,
				       vif->cfg80211.ssid,
				       vif->cfg80211.ssid_len,
				       WLAN_CAPABILITY_ESS,
				       WLAN_CAPABILITY_ESS);
		if (bss) {
			cfg80211_unlink_bss(wiphy, bss);
			wiphy_info(wiphy, "find beacon loss event " MACSTR "",
				   MAC2STR(bss->bssid));
			vif->beacon_loss = 0;
		}
	}

	if (-1 == report_null) {
		printkd("[%s %d][report-ssid][NULL]\n", __func__, vif_id);
	}
	del_timer_sync(&vif->cfg80211.scan_timeout);
	cfg80211_scan_done(vif->cfg80211.scan_request, false);
	vif->cfg80211.scan_request = NULL;
	if (vif->cfg80211.scan_done_lock.link.next != LIST_POISON1
	    && vif->cfg80211.scan_done_lock.link.prev != LIST_POISON2)
		wake_unlock(&vif->cfg80211.scan_done_lock);
	atomic_dec(&vif->cfg80211.scan_status);
	/*memset(vif->cfg80211.scan_frame_array, 0,*/
	/*MAX_SCAN_FRAME_BUF_NUM * sizeof(buf_scan_frame_t));*/
	printkd("[%s %d] ok!\n", __func__, vif_id);
	return;
}

void cfg80211_report_mic_failure(unsigned char vif_id,
				 unsigned char *pdata, int len)
{
	struct wlan_event_mic_failure *mic_failure;
	wlan_vif_t *vif;

	mic_failure = (struct wlan_event_mic_failure *)pdata;
	vif = id_to_vif(vif_id);
	if (vif) {
		/* debug info,Pls remove it in the future */
		printkd
		    ("[%s %d] is_mcast:0x%x key_id: 0x%x bssid: %x %x %x %x %x %x\n",
		     __func__, vif_id, mic_failure->is_mcast,
		     mic_failure->key_id, vif->cfg80211.bssid[0],
		     vif->cfg80211.bssid[1], vif->cfg80211.bssid[2],
		     vif->cfg80211.bssid[3], vif->cfg80211.bssid[4],
		     vif->cfg80211.bssid[5]);
		cfg80211_michael_mic_failure(vif->ndev, vif->cfg80211.bssid,
					     (mic_failure->
					      is_mcast ? NL80211_KEYTYPE_GROUP :
					      NL80211_KEYTYPE_PAIRWISE),
					     mic_failure->key_id, NULL,
					     GFP_KERNEL);
	}
}

void cfg80211_report_cqm_low(unsigned char vif_id,
			     unsigned char *pdata, int len)
{
	wlan_vif_t *vif = id_to_vif(vif_id);
	struct wiphy *wiphy = vif->wdev.wiphy;
	wiphy_info(wiphy, "Recv NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW");

	if (vif) {
		if (sprdwl_find_ssid_count(vif) >= 2) {
			cfg80211_cqm_rssi_notify(vif->ndev,
						 NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW,
						 GFP_KERNEL);
		}
	}
}

void cfg80211_report_cqm_high(unsigned char vif_id,
			      unsigned char *pdata, int len)
{
	wlan_vif_t *vif = id_to_vif(vif_id);
	struct wiphy *wiphy = vif->wdev.wiphy;
	wiphy_info(wiphy, "Recv  NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH");

	if (vif) {
		if (sprdwl_find_ssid_count(vif) >= 2) {
			cfg80211_cqm_rssi_notify(vif->ndev,
						 NL80211_CQM_RSSI_THRESHOLD_EVENT_HIGH,
						 GFP_KERNEL);
		}
	}
}

void cfg80211_report_cqm_beacon_loss(unsigned char vif_id,
				     unsigned char *pdata, int len)
{
	wlan_vif_t *vif = id_to_vif(vif_id);
	struct wiphy *wiphy = vif->wdev.wiphy;
	wiphy_info(wiphy, "Recv NL80211_CQM_RSSI_BEACON_LOSS_EVENT");

	if (vif) {
		vif->beacon_loss = 1;
		/*
		   TODO wpa_supplicant not support the event ,
		   so we workaround this issue
		 */
		cfg80211_cqm_rssi_notify(vif->ndev,
					 NL80211_CQM_RSSI_THRESHOLD_EVENT_LOW,
					 GFP_KERNEL);
	}
}

void cfg80211_report_mlme_tx_status(unsigned char vif_id,
				unsigned char *pdata, int len)
{
	struct wlan_report_mgmt_tx_status *tx_status = NULL;
	wlan_vif_t *vif;

	vif = id_to_vif(vif_id);
	tx_status = (struct wlan_report_mgmt_tx_status *)pdata;
	printkd("[%s]: index: %lld\n", __func__,tx_status->cookie);
	printkd("data len is %d\n", len);
	hex_dump("receive is:", strlen("receive is:"), pdata, len);
	cfg80211_mgmt_tx_status(&vif->wdev, tx_status->cookie, tx_status->buf,
				tx_status->len, tx_status->ack, GFP_KERNEL);
	printkd("cfg80211_mgmt_tx_status end\n");
}

static int wlan_cfg80211_mgmt_tx(struct wiphy *wiphy,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
				 struct wireless_dev *wdev,
#else
				 struct net_device *ndev,
#endif
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)) || \
	defined(COMPAT_KERNEL_RELEASE))
				 struct ieee80211_channel *chan, bool offchan,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
				 enum nl80211_channel_type channel_type,
				 bool channel_type_valid,
#endif
				 unsigned int wait,
#else				/*(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38)) */
				 struct ieee80211_channel *chan,
				 enum nl80211_channel_type channel_type,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)) || \
	defined(COMPAT_KERNEL_RELEASE)
				 bool channel_type_valid,
#endif
#endif				/*(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38)) */
				 const u8 *buf, size_t len,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0))
				 bool no_cck,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0))
				 bool dont_wait_for_ack,
#endif
				 u64 *cookie)
{
	int ret = -1;
	wlan_vif_t *vif;
	unsigned char vif_id;
	static u64 mgmt_index = 0;

	mgmt_index++;
	vif = ndev_to_vif(wdev->netdev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	printkd("[%s], index: %lld, cookie: %lld\n", __func__, mgmt_index, *cookie);
	*cookie = mgmt_index;
	if (len > 0) {
		ret = wlan_cmd_set_tx_mgmt(vif_id, chan, dont_wait_for_ack, wait, cookie, buf, len);
		if (ret) {
			if(dont_wait_for_ack == false)
				cfg80211_mgmt_tx_status(wdev, *cookie, buf, len, 0,GFP_KERNEL);
			printkd("[%s] Failed to set tx mgmt!\n", __func__);
			return ret;
		}
	}
	return ret;
}

static void wlan_cfg80211_mgmt_frame_register(struct wiphy *wiphy,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
					      struct wireless_dev *wdev,
#else
					      struct net_device *ndev,
#endif
					      u16 frame_type, bool reg)
{
	wlan_vif_t *vif;
	unsigned char vif_id;
	vif = ndev_to_vif(wdev->netdev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return;
	if (NETIF_0_ID == vif_id)
		return;
	printkd("[%s][%d]\n", __func__, vif_id);
	register_frame(vif, frame_type, reg);
	return;
}

static int wlan_change_beacon(wlan_vif_t *vif,
			      struct cfg80211_beacon_data *beacon)
{
	u16 ie_len;
	u8 *ie_ptr;
	int ret = 0;
	unsigned char vif_id = vif->id;

	printkd("%s enter\n", __func__);
#ifdef WIFI_DIRECT_SUPPORT
	/* send beacon extra ies */
	if (beacon->head != NULL) {
		ie_len = beacon->head_len;
		ie_ptr = kmalloc(ie_len, GFP_KERNEL);
		if (ie_ptr == NULL) {
			printkd("[%s][%d][%d]\n", __func__, __LINE__, ie_ptr);
			return -EINVAL;
		}
		memcpy(ie_ptr, beacon->head, ie_len);
		printkd("begin send beacon head ies\n");

		ret =
		    wlan_cmd_set_p2p_ie(vif_id, P2P_BEACON_IE_HEAD, ie_ptr,
					ie_len);
		if (ret) {
			printkd
			    ("itm_wlan_set_p2p_ie beacon_ies head failed with ret %d\n",
			     ret);
		} else {
			printkd("send beacon head ies successfully\n");
		}

		kfree(ie_ptr);
	}

	/* send beacon extra ies */
	if (beacon->tail != NULL) {
		ie_len = beacon->tail_len;

		ie_ptr = kmalloc(ie_len, GFP_KERNEL);
		if (ie_ptr == NULL) {
			printkd("[%s][%d][%d]\n", __func__, __LINE__, ie_ptr);
			return -EINVAL;
		}
		memcpy(ie_ptr, beacon->tail, ie_len);
		printkd("begin send beacon tail ies\n");

		ret = wlan_cmd_set_p2p_ie(vif_id,
					  P2P_BEACON_IE_TAIL, ie_ptr, ie_len);
		if (ret) {
			printkd
			    ("wlan_cmd_set_p2p_ie beacon_ies tail failed with ret %d\n",
			     ret);
		} else {
			printkd("send beacon tail ies successfully\n");
		}

		kfree(ie_ptr);
	}

	/* send probe response ies */

	/* send beacon extra ies */
	if (beacon->beacon_ies != NULL) {
		ie_len = beacon->beacon_ies_len;

		ie_ptr = kmalloc(ie_len, GFP_KERNEL);
		if (ie_ptr == NULL) {
			printkd("[%s][%d][%d]\n", __func__, __LINE__, ie_ptr);
			return -EINVAL;
		}
		memcpy(ie_ptr, beacon->beacon_ies, ie_len);
		printkd("begin send beacon extra ies\n");

		ret =
		    wlan_cmd_set_p2p_ie(vif_id, P2P_BEACON_IE, ie_ptr, ie_len);
		if (ret) {
			printkd
			    ("wlan_cmd_set_p2p_ie beacon_ies failed with ret %d\n",
			     ret);
		} else {
			printkd("send beacon extra ies successfully\n");
		}

		kfree(ie_ptr);
	}

	/* send probe response ies */

	if (beacon->proberesp_ies != NULL) {
		printkd("%s line:%d\n", __func__, __LINE__);
		ie_len = beacon->proberesp_ies_len;

		ie_ptr = kmalloc(ie_len, GFP_KERNEL);
		if (ie_ptr == NULL) {
			printkd("[%s][%d][%d]\n", __func__, __LINE__, ie_ptr);
			return -EINVAL;
		}
		memcpy(ie_ptr, beacon->proberesp_ies, ie_len);
		printkd("begin send probe response extra ies\n");

		ret =
		    wlan_cmd_set_p2p_ie(vif_id, P2P_PROBERESP_IE, ie_ptr,
					ie_len);
		if (ret) {
			printkd
			    ("wlan_cmd_set_p2p_ie proberesp_ies failed with ret %d\n",
			     ret);
		} else {
			printkd("send probe response ies successfully\n");
		}

		kfree(ie_ptr);
	}

	/* send associate response ies */

	if (beacon->assocresp_ies != NULL) {
		printkd("%s line:%d\n", __func__, __LINE__);
		ie_len = beacon->assocresp_ies_len;

		ie_ptr = kmalloc(ie_len, GFP_KERNEL);
		if (ie_ptr == NULL) {
			printkd("[%s][%d][%d]\n", __func__, __LINE__, ie_ptr);
			return -EINVAL;
		}
		memcpy(ie_ptr, beacon->assocresp_ies, ie_len);
		printkd("begin send associate response extra ies\n");

		ret =
		    wlan_cmd_set_p2p_ie(vif_id, P2P_ASSOCRESP_IE, ie_ptr,
					ie_len);
		if (ret) {
			printkd
			    ("wlan_cmd_set_p2p_ie assocresp_ies failed with ret %d\n",
			     ret);
		} else {
			printkd("send associate response ies successfully\n");
		}

		kfree(ie_ptr);
	}
#endif /*WIFI_DIRECT_SUPPORT */
	return ret;
}

static int itm_wlan_start_ap(wlan_vif_t *vif,
			     struct cfg80211_beacon_data *beacon)
{
	struct ieee80211_mgmt *mgmt;
	u16 mgmt_len;
	int ret;
	unsigned char vif_id = vif->id;
	printkd("%s enter\n", __func__);
	wlan_change_beacon(vif, beacon);
	if (beacon->head == NULL) {
		printke("%s line:%d err\n", __func__, __LINE__);
		return -EINVAL;
	}
	mgmt_len = beacon->head_len;
	if (beacon->tail)
		mgmt_len += beacon->tail_len;

	mgmt = kmalloc(mgmt_len, GFP_KERNEL);
	if (mgmt == NULL) {
		printkd("[%s][%d][%d]\n", __func__, __LINE__, mgmt);
		return -EINVAL;
	}
	memcpy((u8 *) mgmt, beacon->head, beacon->head_len);
	if (beacon->tail)
		memcpy((u8 *) mgmt + beacon->head_len, beacon->tail,
		       beacon->tail_len);

	ret = wlan_cmd_start_ap(vif_id, (unsigned char *)mgmt, mgmt_len);
	kfree(mgmt);
	if (!netif_carrier_ok(vif->ndev)) {
		printkd("%s(), netif_carrier_on, ssid:%s\n", __func__,
			vif->cfg80211.ssid);
		netif_carrier_on(vif->ndev);
		netif_wake_queue(vif->ndev);
	}
	if (ret != 0) {
		printke("%s line:%d err\n", __func__, __LINE__);
	}
	return ret;
}

static int wlan_cfg80211_start_ap(struct wiphy *wiphy, struct net_device *ndev,
				  struct cfg80211_ap_settings *info)
{
	wlan_vif_t *vif;
	unsigned char vif_id;
	vif = ndev_to_vif(ndev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	if (info->ssid == NULL) {
		printkd("%s line:%d\n", __func__, __LINE__);
		return -EINVAL;
	}
	printkd("[cfg80211] \t ==>>>%s\n", __func__);
	memcpy(vif->cfg80211.ssid, info->ssid, info->ssid_len);
	vif->cfg80211.ssid_len = info->ssid_len;
	return itm_wlan_start_ap(vif, &info->beacon);
}

static int wlan_cfg80211_stop_ap(struct wiphy *wiphy, struct net_device *ndev)
{
	int ret;
	wlan_vif_t *vif;
	unsigned char vif_id;
	vif = ndev_to_vif(ndev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	ret = wlan_cmd_mac_close(vif_id, vif->mode);
	return ret;
}

static int wlan_cfg80211_change_beacon(struct wiphy *wiphy,
				       struct net_device *ndev,
				       struct cfg80211_beacon_data *beacon)
{
	wlan_vif_t *vif;
	unsigned char vif_id;
	vif = ndev_to_vif(ndev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
#ifdef WIFI_DIRECT_SUPPORT
	return wlan_change_beacon(vif, beacon);
#else
	return itm_wlan_start_ap(vif, beacon);
#endif

}

static int itm_wlan_change_mode(wlan_vif_t *vif, enum nl80211_iftype type)
{
	int mode;
	int ret;
	unsigned char vif_id = vif->id;
	switch (type) {
	case NL80211_IFTYPE_STATION:
		if (NETIF_0_ID == vif->id)
			mode = ITM_STATION_MODE;
		else
			mode = ITM_P2P_CLIENT_MODE;
		break;
	case NL80211_IFTYPE_AP:
		if (NETIF_0_ID == vif->id)
			mode = ITM_AP_MODE;
		else
			mode = ITM_P2P_GO_MODE;
		break;
	case NL80211_IFTYPE_P2P_CLIENT:
		mode = ITM_P2P_CLIENT_MODE;
		break;
	case NL80211_IFTYPE_P2P_GO:
		mode = ITM_P2P_GO_MODE;
		break;
	default:
		printkd("invalid interface type %u\n", type);
		return -EOPNOTSUPP;
	}
	vif->wdev.iftype = type;
	if (mode == vif->mode) {
		printkd("not need change mode\n");
		return 0;
	}
	vif->wdev.iftype = type;
	vif->mode = mode;
	printkd("[%s][%d][%d]\n", __func__, vif_id, mode);
	ret = wlan_cmd_mac_open(vif_id, mode, vif->ndev->dev_addr);
	if (OK != ret)
		return -EIO;
	vif->wdev.iftype = type;
	vif->mode = mode;
	return OK;
}

static int wlan_cfg80211_change_iface(struct wiphy *wiphy,
				      struct net_device *ndev,
				      enum nl80211_iftype type,
				      unsigned int *flags,
				      struct vif_params *params)
{
	wlan_vif_t *vif;
	unsigned char vif_id;
	vif = ndev_to_vif(ndev);
	vif_id = vif->id;
#ifdef WIFI_DIRECT_SUPPORT
	if (NETIF_1_ID == vif_id) {
		vif->cfg80211.p2p_mode = itm_get_p2p_mode_from_file();
		printkd("[%s][%d][%d]\n", __func__, vif_id,
			(vif->cfg80211.p2p_mode ? 1 : 0));
	}
#endif /* WIFI_DIRECT_SUPPORT */
	return itm_wlan_change_mode(vif, type);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
static int wlan_cfg80211_set_channel(struct wiphy *wiphy,
				     struct net_device *ndev,
				     struct ieee80211_channel *channel)
#else
static int wlan_cfg80211_set_channel(struct wiphy *wiphy,
				     struct net_device *ndev,
				     struct ieee80211_channel *channel,
				     enum nl80211_channel_type channel_type)
#endif
{
	int ret = -ENOTSUPP;
	wlan_vif_t *vif;
	unsigned char vif_id;
	vif = ndev_to_vif(ndev);
	vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	printkd("[%s][%d] enter\n", __func__, vif_id);
	/*
	 * FIXME: To be handled properly when monitor mode is supported.
	 */
	ret =
	    wlan_cmd_set_channel(vif_id,
				 ieee80211_frequency_to_channel(channel->
								center_freq));
	if (ret < 0) {
		printkd("wlan_cmd_set_channel failed with ret %d\n", ret);
		return ret;
	}

	return 0;
}

int wlan_cfg80211_update_ft_ies(struct wiphy *wiphy, struct net_device *ndev,
				struct cfg80211_update_ft_ies_params *ftie)
{
	wlan_vif_t *vif;
	unsigned char vif_id;
	vif = ndev_to_vif(ndev);
	vif_id = vif->id;
	printkd("%s enter\n", __func__);
	return wlan_cmd_update_ft_ies(vif_id, ftie);
}

static void wlan_cfg80211_reg_notify(struct wiphy *wiphy,
				     struct regulatory_request *request)
{
	struct ieee80211_supported_band *sband;
	struct ieee80211_channel *chan;
	const struct ieee80211_freq_range *freq_range;
	const struct ieee80211_reg_rule *reg_rule;
	wlan_ieee80211_regdomain *rd = NULL;
	u32 band, channel, i;
	u32 last_start_freq;
	u32 n_rules = 0, rd_size;
	static int num;

	wiphy_info(wiphy, "%s %c%c initiator %d hint_type %d\n", __func__,
		   request->alpha2[0], request->alpha2[1], request->initiator,
		   request->user_reg_hint_type);

	if ((num != 1) && (num != 2)) {
		num++;
		return;
	}

	/* Figure out the actual rule number */
	for (band = 0; band < IEEE80211_NUM_BANDS; band++) {
		sband = wiphy->bands[band];
		if (!sband)
			continue;

		last_start_freq = 0;
		for (channel = 0; channel < sband->n_channels; channel++) {
			chan = &sband->channels[channel];
			if (chan->flags & IEEE80211_CHAN_PASSIVE_SCAN)
			{
				chan->flags &= ~IEEE80211_CHAN_PASSIVE_SCAN;
			}

			if (chan->flags & IEEE80211_CHAN_NO_IBSS)
			{
				chan->flags &= ~IEEE80211_CHAN_NO_IBSS;
			}
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
	rd_size = sizeof(wlan_ieee80211_regdomain) +
	    n_rules * sizeof(struct ieee80211_reg_rule);

	rd = kzalloc(rd_size, GFP_KERNEL);
	if (!rd) {
		wiphy_err(wiphy,
			  "Failed to allocate itm_ieee80211_regdomain\n");
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
			if (last_start_freq != freq_range->start_freq_khz
			    && i < n_rules) {
				last_start_freq = freq_range->start_freq_khz;
				memcpy(&rd->reg_rules[i], reg_rule,
				       sizeof(struct ieee80211_reg_rule));
				i++;
				wiphy_dbg(wiphy,
					  "%s %d KHz - %d KHz @ %d KHz flags %#x\n",
					  __func__, freq_range->start_freq_khz,
					  freq_range->end_freq_khz,
					  freq_range->max_bandwidth_khz,
					  reg_rule->flags);
			}
		}
	}
	if (wlan_cmd_set_regdom(0, (u8 *) rd, rd_size))
		wiphy_err(wiphy, "%s failed to set regdomain!\n", __func__);
	kfree(rd);
	num++;
}

int lte_concur_proc_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int lte_concur_proc_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t lte_concur_proc_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	int ret = 0;
	lte_concur_data_t *val;
	unsigned char buff[100];
	int len;
	switch (cmd) {
	case LTE_CONCUR_REQ:
		if (copy_from_user
		    (buff, (unsigned char *)arg,
		     ((lte_concur_data_t *) arg)->size +
		     sizeof(lte_concur_data_t))) {
			return -EFAULT;
		}

		val = (lte_concur_data_t *) buff;
		len = val->size;
		ret =
		    wlan_cmd_req_lte_concur(0,
					    (unsigned char *)val +
					    sizeof(lte_concur_data_t), len);
		if (ret < 0) {
			printkd("wlan_cmd_req_lte_concur failed with ret %d\n",
				ret);
			return ret;
		}
		break;
	default:
		break;
	}
	return 0;
}

/*
 * CFG802.11 operation handler for connection quality monitoring.
 *
 * This function subscribes/unsubscribes HIGH_RSSI and LOW_RSSI
 * events to FW.
 */
int wlan_cfg80211_set_cqm_rssi_config(struct wiphy *wiphy,
				      struct net_device *ndev,
				      s32 rssi_thold, u32 rssi_hyst)
{
	int ret = -ENOTSUPP;
	wlan_vif_t *vif = ndev_to_vif(ndev);
	unsigned char vif_id = vif->id;
	if (ITM_NONE_MODE == vif->mode)
		return -EAGAIN;
	wiphy_info(wiphy, "[%s][%d] rssi_thold %d rssi_hyst %d",
		   __func__, vif_id, rssi_thold, rssi_hyst);
	if (rssi_thold && rssi_hyst)
		ret = wlan_cmd_cmq_rssi(vif_id, rssi_thold, rssi_hyst,
					WIFI_CMD_SET_CQM_RSSI);
	return ret;
}

static struct cfg80211_ops wlan_cfg80211_ops = {
	.scan = wlan_cfg80211_scan,
	.connect = wlan_cfg80211_connect,
	.disconnect = wlan_cfg80211_disconnect,
	.add_key = wlan_cfg80211_add_key,
	.del_key = wlan_cfg80211_del_key,
	.set_default_key = wlan_cfg80211_set_default_key,
	.set_wiphy_params = wlan_cfg80211_set_wiphy_params,
	.get_station = wlan_cfg80211_get_station,
	.set_pmksa = wlan_cfg80211_set_pmksa,
	.del_pmksa = wlan_cfg80211_del_pmksa,
	.flush_pmksa = wlan_cfg80211_flush_pmksa,
	.start_ap = wlan_cfg80211_start_ap,
	.change_beacon = wlan_cfg80211_change_beacon,
	.stop_ap = wlan_cfg80211_stop_ap,
	.mgmt_tx = wlan_cfg80211_mgmt_tx,
	.mgmt_frame_register = wlan_cfg80211_mgmt_frame_register,
	.change_virtual_intf = wlan_cfg80211_change_iface,
	.update_ft_ies = wlan_cfg80211_update_ft_ies,
	.set_cqm_rssi_config = wlan_cfg80211_set_cqm_rssi_config,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
	.libertas_set_mesh_channel = wlan_cfg80211_set_channel,
#else
	.set_channel = wlan_cfg80211_set_channel,
#endif

#ifdef WIFI_DIRECT_SUPPORT
	.remain_on_channel = wlan_cfg80211_remain_on_channel,
	.cancel_remain_on_channel = wlan_cfg80211_cancel_remain_on_channel,
	.del_station = wlan_cfg80211_del_station,
#endif
};

/*Init wiphy parameters*/
static void init_wiphy_parameters(struct wiphy *wiphy)
{
	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
	wiphy->mgmt_stypes = itm_mgmt_stypes;

	wiphy->max_scan_ssids = MAX_SITES_FOR_SCAN;
	wiphy->max_scan_ie_len = SCAN_IE_LEN_MAX;
	wiphy->max_num_pmkids = MAX_NUM_PMKIDS;

	wiphy->interface_modes =
	    BIT(NL80211_IFTYPE_ADHOC) | BIT(NL80211_IFTYPE_STATION) |
	    BIT(NL80211_IFTYPE_AP);

	wiphy->interface_modes |=
	    BIT(NL80211_IFTYPE_P2P_CLIENT) | BIT(NL80211_IFTYPE_P2P_GO) |
	    BIT(NL80211_IFTYPE_P2P_DEVICE);
	wiphy->max_remain_on_channel_duration = 5000;
	wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;
	/* set AP SME flag, also needed by STA mode? */
	wiphy->flags |= WIPHY_FLAG_HAVE_AP_SME;
	wiphy->ap_sme_capa = 1;

	wiphy->software_iftypes =
	    BIT(NL80211_IFTYPE_ADHOC) | BIT(NL80211_IFTYPE_STATION) |
	    BIT(NL80211_IFTYPE_AP) | BIT(NL80211_IFTYPE_P2P_CLIENT) |
	    BIT(NL80211_IFTYPE_P2P_GO) | BIT(NL80211_IFTYPE_P2P_DEVICE);
	/*Attach cipher suites */
	wiphy->cipher_suites = itm_cipher_suites;
	wiphy->n_cipher_suites = ARRAY_SIZE(itm_cipher_suites);
	/*Attach bands */
	wiphy->bands[IEEE80211_BAND_2GHZ] = &itm_band_2ghz;
	/*wiphy->bands[IEEE80211_BAND_5GHZ] = &itm_band_5ghz;*/

	/*Default not in powersave state */
	wiphy->flags &= ~WIPHY_FLAG_PS_ON_BY_DEFAULT;
#if defined(CONFIG_PM) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
	/*Set WoWLAN flags */
	wiphy->wowlan.flags = WIPHY_WOWLAN_ANY | WIPHY_WOWLAN_DISCONNECT;
#endif
	wiphy->reg_notifier = wlan_cfg80211_reg_notify;
}

int wlan_wiphy_new(wlan_info_t *wlan)
{
	int ret;
	printke("%s enter\n", __func__);
	wlan->wiphy = wiphy_new(&wlan_cfg80211_ops, 0);
	if (wlan->wiphy == NULL) {
		ASSERT();
		return ERROR;
	}
	*(struct wlan_info_t **)wiphy_priv(wlan->wiphy) = wlan;
	set_wiphy_dev(wlan->wiphy, wlan->dev);
	init_wiphy_parameters(wlan->wiphy);
	ret = wiphy_register(wlan->wiphy);
	if (ret < 0) {
		printke("%s err:%d\n", __func__, ret);
		ASSERT();
		goto out_free_wiphy;
	}
	return OK;
out_free_wiphy:
	wiphy_free(wlan->wiphy);
	return ERROR;
}

int wlan_wiphy_free(wlan_info_t *wlan)
{
	int i;
	wlan_vif_t *vif;
	if (NULL == wlan)
		return ERROR;
	for (i = 0; i < 2; i++) {
		vif = &(g_wlan.netif[i]);
		if (vif->cfg80211.scan_request
		    && (atomic_add_unless(&vif->cfg80211.scan_status, 1, 1) ==
			1)) {
			if (vif->cfg80211.scan_request->wiphy !=
			    vif->wdev.wiphy) {
				printkd
				    ("Scan request is from a wrong wiphy device\n");
			} else {
				/*If there's a pending scan request,abort it */
				cfg80211_scan_done(vif->cfg80211.scan_request,
						   1);
			}
			vif->cfg80211.scan_request = NULL;
			if (vif->cfg80211.scan_done_lock.link.next !=
			    LIST_POISON1
			    && vif->cfg80211.scan_done_lock.link.prev !=
			    LIST_POISON2)
				wake_unlock(&vif->cfg80211.scan_done_lock);
			atomic_dec(&vif->cfg80211.scan_status);
		}
		wake_lock_destroy(&vif->cfg80211.scan_done_lock);
		del_timer_sync(&(vif->cfg80211.scan_timeout));
		vfree(vif->cfg80211.scan_frame_array);
		vif->cfg80211.scan_frame_array = NULL;
	}
	wiphy_unregister(wlan->wiphy);
	wiphy_free(wlan->wiphy);
	wlan->wiphy = NULL;
	return OK;
}

int mac_addr_cfg(wlan_vif_t *vif, unsigned char vif_id)
{
	struct file *fp = 0;
	mm_segment_t fs;
	loff_t *pos;
	bool no_file = false;
	unsigned char file_data[64] = { 0 };
	unsigned char mac_addr[18] = { 0 };
	unsigned char *tmp_p = NULL;
	fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(ENG_MAC_ADDR_PATH, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		random_ether_addr(vif->ndev->dev_addr);
		fp = filp_open(ENG_MAC_ADDR_PATH, O_CREAT | O_RDWR, 0666);
		if (IS_ERR(fp)) {
			printke("%s %d err\n", __func__, __LINE__);
			goto EXIT;
		}
		no_file = true;
	}
	pos = &(fp->f_pos);
	if (false == no_file) {
		tmp_p = file_data;
		vfs_read(fp, file_data, sizeof(file_data), pos);
		memcpy(mac_addr, tmp_p, 18);
		sscanf(mac_addr, "%02x:%02x:%02x:%02x:%02x:%02x",
		       (unsigned int *)&(vif->ndev->dev_addr[0]),
		       (unsigned int *)&(vif->ndev->dev_addr[1]),
		       (unsigned int *)&(vif->ndev->dev_addr[2]),
		       (unsigned int *)&(vif->ndev->dev_addr[3]),
		       (unsigned int *)&(vif->ndev->dev_addr[4]),
		       (unsigned int *)&(vif->ndev->dev_addr[5]));
	} else {
		sprintf(mac_addr, "%02x:%02x:%02x:%02x:%02x:%02x",
			vif->ndev->dev_addr[0],
			vif->ndev->dev_addr[1],
			vif->ndev->dev_addr[2],
			vif->ndev->dev_addr[3],
			vif->ndev->dev_addr[4], vif->ndev->dev_addr[4]);
		vfs_write(fp, mac_addr, 18, pos);
		printke("[%s write addr:%s]\n", __func__, mac_addr);
	}
	vif->ndev->dev_addr[5] = vif->ndev->dev_addr[5] + vif_id;

EXIT:
	if (!(IS_ERR(fp))) {
		filp_close(fp, NULL);
	}
	set_fs(fs);
	return OK;
}

int wlan_vif_init(wlan_vif_t *vif, int type, const char *name, void *ops)
{
	int ret;
	void **net_priv;
	struct net_device *ndev;
	unsigned char str[64] = { 0 };
	ndev = alloc_netdev(4, name, ether_setup);
	if (!ndev) {
		ASSERT();
		return ERROR;
	}
	vif->ndev = ndev;
	vif->beacon_loss = 0;
	net_priv = netdev_priv(ndev);         
	*net_priv = vif;
	ndev->netdev_ops = ops;
	ndev->watchdog_timeo = 1 * HZ;
	ndev->ieee80211_ptr = &(vif->wdev);
	vif->wdev.iftype = type;
	init_register_frame_param(vif);
	init_send_deauth_work(vif);
	vif->wdev.wiphy = g_wlan.wiphy;
	SET_NETDEV_DEV(ndev, wiphy_dev(vif->wdev.wiphy));
	vif->wdev.netdev = ndev;
	init_timer(&(vif->cfg80211.scan_timeout));
	vif->cfg80211.scan_timeout.data = (unsigned long)vif;
	vif->cfg80211.scan_timeout.function = wlan_scan_timeout;
	vif->cfg80211.scan_request = NULL;
	vif->cfg80211.scan_frame_array =
	    vmalloc(MAX_SCAN_FRAME_BUF_NUM * sizeof(buf_scan_frame_t));
	memset(vif->cfg80211.scan_frame_array, 0,
	       MAX_SCAN_FRAME_BUF_NUM * sizeof(buf_scan_frame_t));
	atomic_set(&vif->cfg80211.scan_status, 0);
	vif->cfg80211.connect_status = ITM_DISCONNECTED;
	memset(vif->cfg80211.bssid, 0, sizeof(vif->cfg80211.bssid));
	vif->mode = ITM_NONE_MODE;
	wake_lock_init(&(vif->cfg80211.scan_done_lock), WAKE_LOCK_SUSPEND,
		       "scan_lock");
	mac_addr_cfg(vif, vif->id);
	ret = register_netdev(vif->ndev);
	if (ret < 0) {
		printkd("[%s][register_netdev err:%d]\n", __func__, ret);
		return ERROR;
	}
	sprintf(str, "[%s][%d][%s][0x%p][addr]:", __func__, vif->id,
		vif->ndev->name, vif->ndev);
	hex_dump(str, strlen(str), (unsigned char *)(&(vif->ndev->dev_addr[0])),
		 6);
	return OK;
}

int wlan_vif_free(wlan_vif_t *vif)
{
	if (NULL == vif->ndev)
		return ERROR;
	printkd("[unregister_netdev][%s][0x%p]\n", __func__, vif->ndev->name);
	cancel_work_sync(&vif->cfg80211.register_frame.work);
	cancel_work_sync(&vif->deauth_info.work);
	unregister_netdev(vif->ndev);
	printkd("[free_netdev][%s][0x%p]\n", __func__, vif->ndev->name);
	free_netdev(vif->ndev);
	printkd("%s(), ok\n", __func__);
	return OK;
}

wlan_vif_t *id_to_vif(unsigned char id)
{
	if ((NETIF_0_ID != id) && (NETIF_1_ID != id))
		return NULL;
	return &(g_wlan.netif[id]);
}

