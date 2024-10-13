// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>
#include <linux/ieee80211.h>

#include "kunit-common.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-dev.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-mlme.h"
#include "kunit-mock-ba.h"
#include "kunit-mock-tdls_manager.h"
#include "kunit-mock-log2us.h"
#include "kunit-mock-txbp.h"
#include "kunit-mock-misc.h"
#include "kunit-mock-scsc_wifi_fcq.h"
#include "kunit-mock-cac.h"
#include "../rx.c"


static struct slsi_peer *test_peer_alloc(struct kunit *test)
{
	struct slsi_peer *peer;
	int i = 0;

	peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, peer);

	for (i = 0; i < NUM_BA_SESSIONS_PER_PEER; i++) {
		peer->ba_session_rx[i] = kunit_kzalloc(test, sizeof(struct slsi_ba_session_rx), GFP_KERNEL);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, peer->ba_session_rx[i]);
	}

	return peer;
}

static void kunit_set_skb(struct sk_buff **skb, u8 *data)
{
	*skb = kzalloc(sizeof(struct sk_buff), GFP_KERNEL);
	(*skb)->data = data;
	((struct slsi_skb_cb *)(*skb)->cb)->sig_length = sizeof(struct fapi_signal);
	((struct slsi_skb_cb *)(*skb)->cb)->data_length = sizeof(struct fapi_signal) + 30;
}

static void test_slsi_freq_to_band(struct kunit *test)
{
	u32 freq = 5100;

	KUNIT_EXPECT_EQ(test, SLSI_FREQ_BAND_2GHZ, slsi_freq_to_band(freq));

	freq = SLSI_5GHZ_MIN_FREQ;
	KUNIT_EXPECT_EQ(test, SLSI_FREQ_BAND_5GHZ, slsi_freq_to_band(freq));

	freq = 5830;
	KUNIT_EXPECT_EQ(test, SLSI_FREQ_BAND_6GHZ, slsi_freq_to_band(freq));
}

static void test_slsi_find_scan_channel(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb;
	struct ieee80211_mgmt *mgmt;
	size_t new_len;

	sdev->wiphy->bands[0] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band), GFP_KERNEL);
	sdev->wiphy->bands[0]->n_channels = 1;
	sdev->wiphy->bands[0]->channels = kunit_kzalloc(test, sizeof(struct ieee80211_channel) * 1, GFP_KERNEL);
	sdev->wiphy->bands[0]->channels[0].center_freq = NL80211_BAND_2GHZ;

	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, sizeof(unsigned char) * 52, GFP_KERNEL);
	skb->data[ETH_ALEN * 2] = 0;
	skb->data[(ETH_ALEN * 2) + 1] = 9;
	skb->len = 24;
	skb->mac_header = 0;
	skb->head = kunit_kzalloc(test, sizeof(struct ethhdr), GFP_KERNEL);

	mgmt = fapi_get_mgmt(skb);
	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
	memcpy(&mgmt->u.beacon.variable, "\x00\x0c\x0e\x54\x35\x35\x30\x38\x5f\x31\x58\x5f\x46\x54", 15);
	memset(mgmt->bssid, 0x0, sizeof(u8) * ETH_ALEN);
	memcpy(sdev->ssid_map[0].bssid, mgmt->bssid, sizeof(u8) * ETH_ALEN);
	sdev->ssid_map[0].band = SLSI_FREQ_BAND_2GHZ;

	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_find_scan_channel(sdev, mgmt, sizeof(struct ieee80211_mgmt), 5100));
}

static void test_slsi_rx_scan_update_ssid(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct ieee80211_mgmt *mgmt;
	struct ieee80211_mgmt *new_mgmt;
	size_t new_len;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, sizeof(unsigned char) * 52, GFP_KERNEL);

	mgmt = fapi_get_mgmt(skb);
	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
	memcpy(&mgmt->u.beacon.variable, "\x00\x00\x0e\x54\x35\x35\x30\x38\x5f\x31\x58\x5f\x46\x54", 15);
	memset(mgmt->bssid, 0x0, sizeof(u8) * ETH_ALEN);
	memcpy(sdev->ssid_map[0].bssid, mgmt->bssid, sizeof(u8) * ETH_ALEN);
	sdev->ssid_map[0].band = SLSI_FREQ_BAND_2GHZ;

	new_mgmt = slsi_rx_scan_update_ssid(sdev, dev, mgmt, sizeof(struct ieee80211_mgmt), &new_len, 0);
	KUNIT_EXPECT_PTR_NE(test, NULL, new_mgmt);
	kfree(new_mgmt);
}

static void test_slsi_rx_scan_pass_to_cfg80211(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct fapi_signal *fapi;
	struct ieee80211_mgmt *mgmt;
	struct slsi_skb_cb *skb_cb;
	size_t new_len;

	sdev->wiphy->bands[0] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band), GFP_KERNEL);
	sdev->wiphy->bands[0]->n_channels = 1;
	sdev->wiphy->bands[0]->channels = kunit_kzalloc(test, sizeof(struct ieee80211_channel) * 1, GFP_KERNEL);
	sdev->wiphy->bands[0]->channels[0].center_freq = NL80211_BAND_2GHZ;
	sdev->wiphy->bands[0]->channels[0].freq_offset = 2484;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, sizeof(unsigned char) * 52, GFP_KERNEL);
	skb_cb = (struct slsi_skb_cb *)skb->cb;
	skb_cb->data_length = 50;
	skb_cb->sig_length = 10;

	mgmt = fapi_get_mgmt(skb);
	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
	memcpy(&mgmt->u.beacon.variable, "\x00\x0c\x0e\x54\x35\x35\x30\x38\x5f\x31\x58\x5f\x46\x54", 15);
	memset(mgmt->bssid, 0x0, sizeof(u8) * ETH_ALEN);
	memcpy(sdev->ssid_map[0].bssid, mgmt->bssid, sizeof(u8) * ETH_ALEN);
	sdev->ssid_map[0].band = SLSI_FREQ_BAND_2GHZ;

	fapi = (struct fapi_signal *)skb->data;
	fapi_set_u16(skb, id, MLME_SYNCHRONISED_IND);
	fapi_set_low16_u32(skb, u.mlme_synchronised_ind.spare_1, 0x00f0);
	fapi_set_low16_u32(skb, u.mlme_synchronised_ind.spare_2, 0xffff);
	fapi->u.mlme_synchronised_ind.rssi = 0xf0;
	fapi->u.mlme_scan_ind.channel_frequency = 10200;
	KUNIT_EXPECT_PTR_NE(test, NULL, slsi_rx_scan_pass_to_cfg80211(sdev, dev, skb, false));

	fapi_set_low16_u32(skb, u.mlme_synchronised_ind.spare_2, 0x0350);
	KUNIT_EXPECT_PTR_NE(test, NULL, slsi_rx_scan_pass_to_cfg80211(sdev, dev, skb, false));

	fapi_set_u16(skb, id, MLME_BEACON_REPORTING_EVENT_IND);
	KUNIT_EXPECT_PTR_NE(test, NULL, slsi_rx_scan_pass_to_cfg80211(sdev, dev, skb, false));
}

static void test_slsi_populate_bssid_info(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct ieee80211_mgmt *mgmt;
	struct slsi_skb_cb *skb_cb;
	struct list_head bssid_list = LIST_HEAD_INIT(bssid_list);
	struct slsi_bssid_info *new_bssid_info;
	struct slsi_bssid_info *temp_bssid_info;
	struct list_head *pos;

	new_bssid_info = kunit_kzalloc(test, sizeof(struct slsi_bssid_info), GFP_KERNEL);

	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, sizeof(unsigned char) * 52, GFP_KERNEL);
	skb_cb = (struct slsi_skb_cb *)skb->cb;
	skb_cb->data_length = 20;
	skb_cb->sig_length = 10;

	mgmt = fapi_get_mgmt(skb);
	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
	memcpy(mgmt->bssid, "\x00\x0c\x0e\x54\x35\x35", 6);

	fapi_set_u16(skb, id, MLME_SYNCHRONISED_IND);
	fapi_set_low16_u32(skb, u.mlme_scan_ind.rssi, 0x00f0);
	fapi_set_low16_u32(skb, u.mlme_scan_ind.channel_frequency, 0xffff);

	memcpy(new_bssid_info->bssid, mgmt->bssid, ETH_ALEN);
	list_add(&new_bssid_info->list, &bssid_list);
	KUNIT_EXPECT_EQ(test, 0, slsi_populate_bssid_info(sdev, ndev_vif, skb, &bssid_list));

	memcpy(mgmt->bssid, "\x00\x00\x00\x00\x00\x00", 6);
	KUNIT_EXPECT_EQ(test, 0, slsi_populate_bssid_info(sdev, ndev_vif, skb, &bssid_list));
	list_for_each(pos, &bssid_list) {
		temp_bssid_info = list_entry(pos, struct slsi_bssid_info, list);
		if (SLSI_ETHER_EQUAL(temp_bssid_info->bssid, mgmt->bssid)) {
			list_del(&temp_bssid_info->list);
			kfree(temp_bssid_info);
			break;
		}
	}
}

static void test_slsi_gen_new_bssid(struct kunit *test)
{
	u8 bssid[ETH_ALEN];
	u8 max_bssid = 127;
	u8 mbssid_index = ETH_ALEN;
	u8 new_bssid[ETH_ALEN];

	memcpy(bssid, "\x00\x0c\x0e\x54\x35\x35", ETH_ALEN);
	slsi_gen_new_bssid(bssid, max_bssid, mbssid_index, new_bssid);
}

static void test_slsi_mbssid_to_ssid_list(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct ieee80211_mgmt *mgmt;
	struct slsi_skb_cb *skb_cb;
	struct slsi_ssid_info *new_ssid_info;
	struct slsi_bssid_info *new_bssid_info;
	struct list_head *pos;
	struct slsi_bssid_info *temp_bssid_info;
	struct slsi_ssid_info *temp_ssid_info;
	int ssid_len = 10;
	u8 scan_ssid[IEEE80211_MAX_SSID_LEN] = "kunit_ssid";
	u8 bssid[ETH_ALEN] = "\x00\x0c\x0e\x54\x35\x35";
	u8 akm_type = 0xff;
	int freq = 0;
	int rssi = 0;

	new_ssid_info = kunit_kzalloc(test, sizeof(struct slsi_ssid_info), GFP_KERNEL);
	new_bssid_info = kunit_kzalloc(test, sizeof(struct slsi_bssid_info), GFP_KERNEL);

	INIT_LIST_HEAD(&ndev_vif->sta.ssid_info);
	INIT_LIST_HEAD(&new_ssid_info->bssid_list);

	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, sizeof(unsigned char) * 52, GFP_KERNEL);
	skb_cb = (struct slsi_skb_cb *)skb->cb;
	skb_cb->data_length = 20;
	skb_cb->sig_length = 10;

	new_ssid_info->ssid.ssid_len = ssid_len;
	memcpy(new_ssid_info->ssid.ssid, scan_ssid, strlen(scan_ssid));
	new_ssid_info->akm_type = 0xff;
	list_add(&new_ssid_info->list, &ndev_vif->sta.ssid_info);

	memcpy(new_bssid_info->bssid, bssid, ETH_ALEN);
	list_add(&new_bssid_info->list, &new_ssid_info->bssid_list);
	KUNIT_EXPECT_EQ(test, 0, slsi_mbssid_to_ssid_list(sdev, ndev_vif, scan_ssid, ssid_len,
								bssid, freq, rssi, akm_type));
	list_del(&new_bssid_info->list);

	memcpy(new_bssid_info->bssid, "\x00\x00\x00\x00\x00\x00", ETH_ALEN);
	list_add(&new_bssid_info->list, &new_ssid_info->bssid_list);
	KUNIT_EXPECT_EQ(test, 0, slsi_mbssid_to_ssid_list(sdev, ndev_vif, scan_ssid, ssid_len,
								bssid, freq, rssi, akm_type));
	list_for_each(pos, &new_ssid_info->bssid_list) {
		struct slsi_bssid_info *temp_bssid_info = list_entry(pos, struct slsi_bssid_info, list);

		if (SLSI_ETHER_EQUAL(temp_bssid_info->bssid, bssid)) {
			list_del(&temp_bssid_info->list);
			kfree(temp_bssid_info);
			break;
		}
	}

	new_ssid_info->ssid.ssid_len = 8;
	KUNIT_EXPECT_EQ(test, 0, slsi_mbssid_to_ssid_list(sdev, ndev_vif, scan_ssid, ssid_len,
								bssid, freq, rssi, akm_type));
	list_for_each(pos, &ndev_vif->sta.ssid_info) {
		struct slsi_ssid_info *temp_ssid_info = list_entry(pos, struct slsi_ssid_info, list);
		struct list_head *pos_bssid;
		bool flag = false;

		list_for_each(pos_bssid, &temp_ssid_info->bssid_list) {
			struct slsi_bssid_info *temp_bssid_info = list_entry(pos_bssid, struct slsi_bssid_info, list);

			if (SLSI_ETHER_EQUAL(temp_bssid_info->bssid, bssid)) {
				flag = true;
				list_del(&temp_bssid_info->list);
				kfree(temp_bssid_info);
				break;
			}
		}
		if (flag) {
			list_del(&temp_ssid_info->list);
			kfree(temp_ssid_info);
			break;
		}
	}
}

static void test_slsi_extract_mbssids(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct ieee80211_mgmt *mgmt;
	struct slsi_skb_cb *skb_cb;
	struct slsi_ssid_info *new_ssid_info;
	struct slsi_bssid_info bssid_info;
	struct slsi_ssid_info *ssid_info;

	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, sizeof(unsigned char) * 56, GFP_KERNEL);
	skb_cb = (struct slsi_skb_cb *)skb->cb;
	skb_cb->data_length = 100;
	skb_cb->sig_length = 10;

	mgmt = fapi_get_mgmt(skb);
	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);

	memcpy(&mgmt->u.beacon.variable, "\x00\x0c\x0e\x54\x35\x35\x30\x38\x5f\x31\x58\x5f\x46\x54", 15);
	((struct element *)(mgmt->u.beacon.variable))->id = WLAN_EID_MULTIPLE_BSSID;
	((struct element *)(mgmt->u.beacon.variable))->datalen = 16;
	((struct element *)(mgmt->u.beacon.variable + 1))->datalen = 5;
	((struct element *)(((struct element *)(mgmt->u.beacon.variable))->data + 1))->datalen = 5;
	((struct element *)(((struct element *)(mgmt->u.beacon.variable))->data + 1))->id = 0;
	((struct element *)(((struct element *)(mgmt->u.beacon.variable))->data + 1))->data[0] = WLAN_EID_NON_TX_BSSID_CAP;
	((struct element *)(((struct element *)(mgmt->u.beacon.variable))->data + 1))->data[1] = 2;

	new_ssid_info = kunit_kzalloc(test, sizeof(struct list_head), GFP_KERNEL);
	INIT_LIST_HEAD(&ndev_vif->sta.ssid_info);
	list_add(&new_ssid_info->list, &ndev_vif->sta.ssid_info);

	KUNIT_EXPECT_EQ(test, 0, slsi_extract_mbssids(sdev, ndev_vif, mgmt, skb, 1));
}

static void test_slsi_remove_assoc_disallowed_bssid(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_ssid_info *new_ssid_info;
	struct slsi_bssid_info *new_bssid_info;
	struct slsi_ssid_info *ssid_info_ptr;
	struct list_head *pos;

	new_ssid_info = kunit_kzalloc(test, sizeof(struct slsi_ssid_info), GFP_KERNEL);
	new_bssid_info = kunit_kzalloc(test, sizeof(struct slsi_bssid_info), GFP_KERNEL);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->scan[0].scan_results = kunit_kzalloc(test, sizeof(struct slsi_scan_result), GFP_KERNEL);
	ndev_vif->scan[0].scan_results->ssid_length = IEEE80211_MAX_SSID_LEN;
	ndev_vif->scan[0].scan_results->akm_type = 1;

	INIT_LIST_HEAD(&ndev_vif->sta.ssid_info);
	list_add(&new_ssid_info->list, &ndev_vif->sta.ssid_info);

	ssid_info_ptr = list_entry((&ndev_vif->sta.ssid_info)->next, struct slsi_ssid_info, list);
	ssid_info_ptr->ssid.ssid_len = IEEE80211_MAX_SSID_LEN;
	ssid_info_ptr->akm_type = 1;

	memset(ssid_info_ptr->ssid.ssid, 0x0, IEEE80211_MAX_SSID_LEN);
	memset(ndev_vif->scan[0].scan_results->ssid, 0x0, IEEE80211_MAX_SSID_LEN);

	INIT_LIST_HEAD(&new_ssid_info->bssid_list);
	list_add(&new_bssid_info->list, &new_ssid_info->bssid_list);
	memset(ndev_vif->scan[0].scan_results->bssid, 0x1, sizeof(char) * ETH_ALEN);
	memset(new_bssid_info->bssid, 0x0, sizeof(char) * ETH_ALEN);

	slsi_remove_assoc_disallowed_bssid(sdev, ndev_vif, ndev_vif->scan[0].scan_results);
}

static void test_slsi_reject_ap_for_scan_info(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct ieee80211_mgmt *mgmt;
	struct slsi_skb_cb *skb_cb;
	struct slsi_ssid_info *new_ssid_info;
	struct slsi_bssid_info *new_bssid_info;
	struct slsi_ssid_info *ssid_info_ptr;
	size_t mgmt_len;

	new_ssid_info = kunit_kzalloc(test, sizeof(struct slsi_ssid_info), GFP_KERNEL);
	new_bssid_info = kunit_kzalloc(test, sizeof(struct slsi_bssid_info), GFP_KERNEL);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->scan[0].scan_results = kunit_kzalloc(test, sizeof(struct slsi_scan_result), GFP_KERNEL);
	ndev_vif->scan[0].scan_results->ssid_length = IEEE80211_MAX_SSID_LEN;
	ndev_vif->scan[0].scan_results->akm_type = 1;
	mgmt_len = 100;

	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, sizeof(unsigned char) * 52, GFP_KERNEL);
	skb_cb = (struct slsi_skb_cb *)skb->cb;
	skb_cb->data_length = 100;
	skb_cb->sig_length = 10;

	mgmt = fapi_get_mgmt(skb);
	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);

	memcpy(&mgmt->u.beacon.variable, "\x00\x0c\x0e\x54\x35\x35\x30\x38\x5f\x31\x58\x5f\x46\x54", 15);

	INIT_LIST_HEAD(&ndev_vif->sta.ssid_info);
	list_add(&new_ssid_info->list, &ndev_vif->sta.ssid_info);

	ssid_info_ptr = list_entry((&ndev_vif->sta.ssid_info)->next, struct slsi_ssid_info, list);
	ssid_info_ptr->ssid.ssid_len = IEEE80211_MAX_SSID_LEN;
	ssid_info_ptr->akm_type = 1;

	memset(ssid_info_ptr->ssid.ssid, 0x0, IEEE80211_MAX_SSID_LEN);
	memset(ndev_vif->scan[0].scan_results->ssid, 0x0, IEEE80211_MAX_SSID_LEN);

	INIT_LIST_HEAD(&new_ssid_info->bssid_list);
	list_add(&new_bssid_info->list, &new_ssid_info->bssid_list);
	memset(ndev_vif->scan[0].scan_results->bssid, 0x1, sizeof(char) * ETH_ALEN);
	memset(new_bssid_info->bssid, 0x0, sizeof(char) * ETH_ALEN);

	KUNIT_EXPECT_EQ(test, 1, slsi_reject_ap_for_scan_info(sdev, ndev_vif, mgmt, mgmt_len,
							      ndev_vif->scan[0].scan_results));
}

struct scan_list_test_data {
	int rssi;
	u16 ch_freq;
	int band;
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 bssid[ETH_ALEN];
	u16 stype;
};

static const struct scan_list_test_data scan_list_tests[] =  {
	{ .rssi = -50, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_2G",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x1}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -40, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_2G",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x1}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -60, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_2G",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x1}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -50, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "kunit_test_ssid_5G_probe",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x1}, .stype = IEEE80211_STYPE_PROBE_RESP },
	{ .rssi = -80, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "kunit_test_ssid_5G_probe_#2",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_PROBE_RESP },
	{ .rssi = -20, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_2G",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x1}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -60, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "kunit_test_ssid_5G",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -60, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -20, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_2G_#2",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -20, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_2G_#3",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x3}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -90, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_2G_#4",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x4}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -20, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_2G_#4",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x4}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -80, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_2G_#4",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x4}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -40, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "kunit_test_ssid_5G_probe",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x1}, .stype = IEEE80211_STYPE_PROBE_RESP },
	{ .rssi = -50, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x5}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -30, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -80, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -5, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_2G",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x1}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -90, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "kunit_test_ssid_5G_probe_#2",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_PROBE_RESP },
	{ .rssi = -90, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "kunit_test_ssid_5G_probe_#2",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_PROBE_RESP },
	{ .rssi = -105, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_5G_probe_#2",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_PROBE_RESP },
	{ .rssi = -110, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_5G_probe_#2",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -100, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_5G_probe_#2",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_PROBE_RESP },
};

static const struct scan_list_test_data scan_list_tests_results[] =  {
	{ .rssi = -5, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_2G",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x1}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -20, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_2G_#2",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -20, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_2G_#3",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x3}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -20, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_2G_#4",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x4}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -30, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -40, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "kunit_test_ssid_5G_probe",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x1}, .stype = IEEE80211_STYPE_PROBE_RESP },
	{ .rssi = -50, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x5}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -60, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "kunit_test_ssid_5G",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_BEACON },
	{ .rssi = -80, .ch_freq = 5180, .band = SLSI_FREQ_BAND_5GHZ, .ssid = "kunit_test_ssid_5G_probe_#2",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_PROBE_RESP },
	{ .rssi = -100, .ch_freq = 2412, .band = SLSI_FREQ_BAND_2GHZ, .ssid = "kunit_test_ssid_5G_probe_#2",
	  .bssid = {0x0, 0x0, 0x0, 0x0, 0x0, 0x2}, .stype = IEEE80211_STYPE_PROBE_RESP },
};

static struct sk_buff *test_make_scan_result_data(struct kunit *test, struct scan_list_test_data *scan_param)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff *req;
	struct ieee80211_mgmt *mgmt = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt) + 128, GFP_KERNEL);
	u8 *pos;

	req = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, ndev_vif->ifnum, 512);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, req);

	fapi_set_u16(req, u.mlme_scan_ind.channel_frequency, scan_param->ch_freq * 2);
	fapi_set_u16(req, u.mlme_scan_ind.rssi, scan_param->rssi);
	fapi_set_u16(req, u.mlme_scan_ind.scan_id, 0);

	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | scan_param->stype);
	memcpy(mgmt->bssid, scan_param->bssid, ETH_ALEN);

	pos = mgmt->u.beacon.variable;
	*pos++ = WLAN_EID_SSID;
	*pos++ = strlen(scan_param->ssid);
	memcpy(pos, scan_param->ssid, strlen(scan_param->ssid));

	fapi_append_data(req, (u8 *)mgmt, sizeof(*mgmt) + 2 + strlen(scan_param->ssid));
	return req;
}

static void test_slsi_add_to_scan_list(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct ieee80211_mgmt *mgmt;
	struct slsi_scan_result *scan_results, *prev = NULL;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(scan_list_tests); i++) {
		skb = test_make_scan_result_data(test, &scan_list_tests[i]);
		mgmt = fapi_get_mgmt(skb);

		KUNIT_EXPECT_EQ(test, 0,
				slsi_add_to_scan_list(sdev, ndev_vif, skb, &mgmt->u.beacon.variable, SLSI_SCAN_HW_ID));
	}

	scan_results = ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results;
	for (i = 0; i < ARRAY_SIZE(scan_list_tests_results); i++) {
		KUNIT_EXPECT_PTR_NE(test, NULL, scan_results);

		KUNIT_EXPECT_EQ(test, scan_list_tests_results[i].rssi, scan_results->rssi);
		KUNIT_EXPECT_EQ(test, scan_list_tests_results[i].band, scan_results->band);
		KUNIT_EXPECT_TRUE(test, !strcmp(scan_list_tests_results[i].ssid, scan_results->ssid));
		KUNIT_EXPECT_TRUE(test, !memcmp(scan_list_tests_results[i].bssid, scan_results->bssid, ETH_ALEN));
		if (scan_list_tests_results[i].stype == IEEE80211_STYPE_BEACON)
			KUNIT_EXPECT_PTR_NE(test, NULL, scan_results->beacon);
		if (scan_list_tests_results[i].stype == IEEE80211_STYPE_PROBE_RESP)
			KUNIT_EXPECT_PTR_NE(test, NULL, scan_results->probe_resp);

		kfree_skb(scan_results->beacon);
		kfree_skb(scan_results->probe_resp);
		prev = scan_results;
		scan_results = scan_results->next;
		kfree(prev);
	}
	KUNIT_EXPECT_PTR_EQ(test, NULL, scan_results);
}

static void test_slsi_populate_ssid_info(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct ieee80211_mgmt *mgmt;
	struct slsi_skb_cb *skb_cb;
	struct slsi_ssid_info *new_ssid_info;
	struct slsi_bssid_info *new_bssid_info;
	struct slsi_ssid_info *ssid_info_ptr;
	struct list_head bssid_list = LIST_HEAD_INIT(bssid_list);
	struct list_head *pos;
	struct slsi_bssid_info *temp_bssid_info;
	size_t mgmt_len;

	new_ssid_info = kunit_kzalloc(test, sizeof(struct slsi_ssid_info), GFP_KERNEL);
	new_bssid_info = kunit_kzalloc(test, sizeof(struct slsi_bssid_info), GFP_KERNEL);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->scan[0].scan_results = kunit_kzalloc(test, sizeof(struct slsi_scan_result), GFP_KERNEL);
	ndev_vif->scan[0].scan_results->ssid_length = IEEE80211_MAX_SSID_LEN;
	ndev_vif->scan[0].scan_results->akm_type = 1;

	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, sizeof(unsigned char) * 52, GFP_KERNEL);
	skb_cb = (struct slsi_skb_cb *)skb->cb;
	skb_cb->data_length = 110;
	skb_cb->sig_length = 10;

	ndev_vif->scan[0].scan_results->beacon = skb;
	strcpy((ndev_vif->scan[0].scan_results)->ssid, "kunit_ssid");
	(ndev_vif->scan[0].scan_results)->ssid_length = strlen((ndev_vif->scan[0].scan_results)->ssid);

	mgmt = fapi_get_mgmt(skb);
	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);

	memcpy(&mgmt->u.beacon.variable, "\x00\x00\x00\x00\x35\x35\x30\x38\x5f\x31\x58\x5f\x46\x54", 15);
	memcpy(mgmt->bssid, "\x00\x0c\x0e\x54\x35\x35", 6);

	INIT_LIST_HEAD(&ndev_vif->sta.ssid_info);
	list_add(&new_ssid_info->list, &ndev_vif->sta.ssid_info);

	ssid_info_ptr = list_entry((&ndev_vif->sta.ssid_info)->next, struct slsi_ssid_info, list);
	ssid_info_ptr->ssid.ssid_len = IEEE80211_MAX_SSID_LEN;
	ssid_info_ptr->akm_type = 1;

	memset(ssid_info_ptr->ssid.ssid, 0x0, IEEE80211_MAX_SSID_LEN);
	memset(ndev_vif->scan[0].scan_results->ssid, 0x0, IEEE80211_MAX_SSID_LEN);

	INIT_LIST_HEAD(&new_ssid_info->bssid_list);
	list_add(&new_bssid_info->list, &new_ssid_info->bssid_list);
	memset(ndev_vif->scan[0].scan_results->bssid, 0x1, sizeof(char) * ETH_ALEN);
	memset(new_bssid_info->bssid, 0x0, sizeof(char) * ETH_ALEN);

	memcpy(mgmt->bssid, "\x00\x0c\x0e\x54\x35\x35", 6);

	fapi_set_u16(skb, id, MLME_SYNCHRONISED_IND);
	fapi_set_low16_u32(skb, u.mlme_scan_ind.rssi, 0x00f0);
	fapi_set_low16_u32(skb, u.mlme_scan_ind.channel_frequency, 0xffff);

	KUNIT_EXPECT_EQ(test, 0, slsi_populate_ssid_info(sdev, ndev_vif, 0));

	list_for_each(pos, &new_ssid_info->bssid_list) {
		temp_bssid_info = list_entry(pos, struct slsi_bssid_info, list);
		if (SLSI_ETHER_EQUAL(temp_bssid_info->bssid, mgmt->bssid)) {
			list_del(&temp_bssid_info->list);
			kfree(temp_bssid_info);
			break;
		}
	}
}

static void test_slsi_rx_scan_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct ieee80211_mgmt *mgmt = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt) + 128, GFP_KERNEL);

	skb = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, ndev_vif->ifnum, 512);

	ndev_vif->scan[0].scan_results = kunit_kzalloc(test, sizeof(struct slsi_scan_result), GFP_KERNEL);
	ndev_vif->scan[0].scan_req = kunit_kzalloc(test, sizeof(struct cfg80211_scan_request), GFP_KERNEL);
	SLSI_MUTEX_INIT(ndev_vif->scan_result_mutex);

	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
	memcpy(&mgmt->u.probe_resp.variable, "\x01\x08\x03\x04\x35\x35\x30\x38\x5f\x31\x02\x5f\x46\x54", 15);
	fapi_append_data(skb, (u8 *)mgmt, sizeof(struct ieee80211_mgmt) + 15);

	fapi_set_u16(skb, u.mlme_scan_ind.rssi, 10);
	fapi_set_u16(skb, u.mlme_scan_ind.channel_frequency, 5150);
	fapi_set_u16(skb, u.mlme_scan_ind.scan_id, SLSI_GSCAN_SCAN_ID_START);
	slsi_rx_scan_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, ndev_vif->ifnum, 512);
	fapi_set_u16(skb, u.mlme_scan_ind.scan_id, SLSI_NL80211_APF_SUBCMD_RANGE_START);
	sdev->p2p_certif = true;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_CLIENT;
	ndev_vif->ifnum = 0x0016;
	slsi_rx_scan_ind(sdev, dev, skb);
}

static void test_slsi_rx_rcl_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	skb = fapi_alloc(mlme_roaming_channel_list_ind, MLME_ROAMING_CHANNEL_LIST_IND, ndev_vif->ifnum, 0);

	sdev->p2p_certif = true;

	fapi_append_data(skb, (u8 *)"\x01\x08\x03\x04\x35\x35\x30\x68\x13\x31\x02\x5f\x46\x54", 15);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_CLIENT;
	ndev_vif->sta.last_connected_bss.ssid[0] = 'a';
	ndev_vif->sta.last_connected_bss.ssid_len = 1;

	ndev_vif->scan[0].scan_results = kunit_kzalloc(test, sizeof(struct slsi_scan_result), GFP_KERNEL);

	slsi_rx_rcl_ind(sdev, dev, skb);
}

static void test_slsi_rx_start_detect_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	skb = fapi_alloc(mlme_start_detect_ind, MLME_START_DETECT_IND, ndev_vif->ifnum, 0);
	fapi_set_s16(skb, u.mlme_start_detect_ind.result, 10);

	slsi_rx_start_detect_ind(sdev, dev, skb);
}

static void test_slsi_scan_update_ssid_map(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct slsi_skb_cb *skb_cb;
	struct ieee80211_mgmt *mgmt;
	struct cfg80211_bss_ies *ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies) + sizeof(u8) * 20,
						     GFP_KERNEL);
	u16 scan_id = 0;

	skb = kmalloc(sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, sizeof(unsigned char) * 52, GFP_KERNEL);
	skb_cb = (struct slsi_skb_cb *)skb->cb;
	skb_cb->data_length = 110;
	skb_cb->sig_length = 10;

	sdev->ssid_map[0].ssid_len = 1;

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.sta_bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	ndev_vif->sta.sta_bss->ies = ies;
	ndev_vif->sta.sta_bss->channel = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	ndev_vif->sta.sta_bss->channel->center_freq = SLSI_5GHZ_MIN_FREQ;
	memset(ies->data, 0, 20);
	ndev_vif->scan[0].scan_results = kunit_kzalloc(test, sizeof(struct slsi_scan_result), GFP_KERNEL);
	ndev_vif->scan[0].scan_req = kunit_kzalloc(test, sizeof(struct cfg80211_scan_request), GFP_KERNEL);
	SLSI_MUTEX_INIT(ndev_vif->scan_result_mutex);

	ndev_vif->scan[0].scan_results->probe_resp = skb;
	ndev_vif->scan[0].scan_results->hidden = 1;
	ndev_vif->scan[0].scan_results->next = 0;

	mgmt = fapi_get_mgmt(ndev_vif->scan[0].scan_results->probe_resp);
	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
	memset(sdev->ssid_map[0].bssid, 0x0, sizeof(u8) * ETH_ALEN);
	memcpy(ndev_vif->scan[0].scan_results->bssid, sdev->ssid_map[0].bssid, sizeof(u8) * ETH_ALEN);
	sdev->ssid_map[0].band = 1;
	ndev_vif->scan[0].scan_results->band = 1;

	sdev->ssid_map[0].band = SLSI_5GHZ_MIN_FREQ;
	memcpy(&mgmt->u.beacon.variable, "\x00\x00\x00\x00\x35\x35\x30\x38\x5f\x31\x58\x5f\x46\x54", 15);
	slsi_scan_update_ssid_map(sdev, dev, scan_id);

	ndev_vif->scan[0].scan_results->band = 0;
	slsi_scan_update_ssid_map(sdev, dev, scan_id);
}

static void test_slsi_set_2g_auto_channel(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_acs_selected_channels acs_selected_channels;
	struct slsi_acs_chan_info *ch_info;
	int i;

	ndev_vif->scan[0].acs_request = kunit_kzalloc(test, sizeof(struct slsi_acs_request), GFP_KERNEL);
	ndev_vif->scan[0].acs_request->ch_width = 40;
	ndev_vif->scan[0].acs_request->hw_mode = 5;

	ch_info = kunit_kzalloc(test, sizeof(struct slsi_acs_chan_info) * SLSI_NUM_2P4GHZ_CHANNELS, GFP_KERNEL);
	for (i = 0; i < SLSI_NUM_2P4GHZ_CHANNELS; i++) {
		ch_info[i].chan = 1;
		ch_info[i].total_chan_utilization = 10;
		ch_info[i].num_bss_load_ap = 2;
		ch_info[i].num_ap = 4;
		ch_info[i].rssi_factor = 10;
		ch_info[i].adj_rssi_factor = 10;
	}

	KUNIT_EXPECT_EQ(test, 0, slsi_set_2g_auto_channel(sdev, ndev_vif, &acs_selected_channels, ch_info));

	ndev_vif->scan[0].acs_request->ch_width = 20;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_2g_auto_channel(sdev, ndev_vif, &acs_selected_channels, ch_info));
}

static void test_slsi_is_40mhz(struct kunit *test)
{
	u8 pri_channel = 36;
	u8 sec_channel = 40;

	KUNIT_EXPECT_EQ(test, 1, slsi_is_40mhz(pri_channel, sec_channel, false));

	sec_channel = 42;
	KUNIT_EXPECT_EQ(test, 0, slsi_is_40mhz(pri_channel, sec_channel, false));

	pri_channel = 160;
	KUNIT_EXPECT_EQ(test, 0, slsi_is_40mhz(pri_channel, sec_channel, false));
}

static void test_slsi_is_80mhz(struct kunit *test)
{
	u8 pri_channel = 36;
	u8 last_channel = 48;

	KUNIT_EXPECT_EQ(test, 1, slsi_is_80mhz(pri_channel, last_channel, false));

	last_channel = 50;
	KUNIT_EXPECT_EQ(test, 0, slsi_is_80mhz(pri_channel, last_channel, false));

	pri_channel = 160;
	KUNIT_EXPECT_EQ(test, 0, slsi_is_80mhz(pri_channel, last_channel, false));
}

static void test_slsi_set_5g_auto_channel(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_acs_selected_channels acs_selected_channels;
	struct slsi_acs_chan_info *ch_info;
	int i;

	ndev_vif->scan[0].acs_request = kunit_kzalloc(test, sizeof(struct slsi_acs_request), GFP_KERNEL);
	ndev_vif->scan[0].acs_request->ch_width = 80;
	ndev_vif->scan[0].acs_request->hw_mode = 5;

	ch_info = kunit_kzalloc(test, sizeof(struct slsi_acs_chan_info) * SLSI_NUM_5GHZ_CHANNELS, GFP_KERNEL);
	for (i = 0; i < SLSI_NUM_5GHZ_CHANNELS; i++) {
		ch_info[i].chan = 1;
		ch_info[i].total_chan_utilization = 10;
		ch_info[i].num_bss_load_ap = 2;
		ch_info[i].num_ap = 4;
		ch_info[i].rssi_factor = 10;
		ch_info[i].adj_rssi_factor = 10;
	}

	KUNIT_EXPECT_EQ(test, 0, slsi_set_5g_auto_channel(sdev, ndev_vif, &acs_selected_channels, ch_info));

	for (i = 0; i < SLSI_NUM_5GHZ_CHANNELS; i++)
		ch_info[i].num_bss_load_ap = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_5g_auto_channel(sdev, ndev_vif, &acs_selected_channels, ch_info));

	ch_info[0].chan = 36;
	ch_info[3].chan = 48;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_5g_auto_channel(sdev, ndev_vif, &acs_selected_channels, ch_info));

	ch_info[0].chan = 36;
	ch_info[1].chan = 40;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_5g_auto_channel(sdev, ndev_vif, &acs_selected_channels, ch_info));

	ndev_vif->scan[0].acs_request->ch_width = 40;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_5g_auto_channel(sdev, ndev_vif, &acs_selected_channels, ch_info));

	ndev_vif->scan[0].acs_request->ch_width = 20;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_5g_auto_channel(sdev, ndev_vif, &acs_selected_channels, ch_info));
}

static struct ieee80211_rate dummy_rates[] = {
	{.bitrate = 10, .hw_value = 2, },
};

static struct ieee80211_channel dummy_ch_2ghz[] = {
	{.center_freq = 2412, .hw_value = 1, },
};

static struct ieee80211_supported_band dummy_band_2ghz = {
	.channels   = dummy_ch_2ghz,
	.band       = NL80211_BAND_2GHZ,
	.n_channels = ARRAY_SIZE(dummy_ch_2ghz),
	.bitrates   = dummy_rates,
	.n_bitrates = ARRAY_SIZE(dummy_rates),
};

static struct ieee80211_channel dummy_ch_6ghz[] = {
	{.center_freq = 6115, .hw_value = 33, .band = NL80211_BAND_6GHZ},
	{.center_freq = 6135, .hw_value = 37, .band = NL80211_BAND_6GHZ},
	{.center_freq = 6155, .hw_value = 41, .band = NL80211_BAND_6GHZ},
	{.center_freq = 6175, .hw_value = 45, .band = NL80211_BAND_6GHZ},
};

static struct ieee80211_supported_band dummy_band_6ghz = {
	.channels   = dummy_ch_6ghz,
	.band       = NL80211_BAND_6GHZ,
	.n_channels = ARRAY_SIZE(dummy_ch_6ghz),
	.bitrates   = dummy_rates,
	.n_bitrates = ARRAY_SIZE(dummy_rates),
};

static void test_slsi_set_band_any_auto_channel(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_acs_selected_channels acs_selected_channels;
	struct slsi_acs_chan_info *ch_info;
	int i = 0;

#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	sdev->wiphy->bands[NL80211_BAND_6GHZ] = &dummy_band_6ghz;
#endif

	ndev_vif->scan[0].acs_request = kunit_kzalloc(test, sizeof(struct slsi_acs_request), GFP_KERNEL);
	ndev_vif->scan[0].acs_request->ch_width = 80;
	ndev_vif->scan[0].acs_request->hw_mode = SLSI_ACS_MODE_IEEE80211ANY;

	ch_info = kunit_kzalloc(test, sizeof(struct slsi_acs_chan_info) * SLSI_MAX_CHAN_VALUE_ACS, GFP_KERNEL);
	for (i = 0; i < SLSI_NUM_2P4GHZ_CHANNELS + SLSI_NUM_5GHZ_CHANNELS; i++) {
		ch_info[i].chan = 20;
		ch_info[i].total_chan_utilization = 10;
		ch_info[i].num_bss_load_ap = 2;
		ch_info[i].num_ap = 20;
		ch_info[i].rssi_factor = 10;
		ch_info[i].adj_rssi_factor = 10;
	}
	KUNIT_EXPECT_EQ(test, 0, slsi_set_band_any_auto_channel(sdev, ndev_vif, &acs_selected_channels, ch_info));

#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	sdev->supported_6g_160mhz = false;
	for (i = 0; i < 4; i++) {
		ch_info[SLSI_NUM_2P4GHZ_CHANNELS + SLSI_NUM_5GHZ_CHANNELS + i].chan = dummy_ch_6ghz[i].hw_value;
		ch_info[SLSI_NUM_2P4GHZ_CHANNELS + SLSI_NUM_5GHZ_CHANNELS + i].total_chan_utilization = 10;
		ch_info[SLSI_NUM_2P4GHZ_CHANNELS + SLSI_NUM_5GHZ_CHANNELS + i].num_bss_load_ap = 2;
		ch_info[SLSI_NUM_2P4GHZ_CHANNELS + SLSI_NUM_5GHZ_CHANNELS + i].num_ap = 5;
		ch_info[SLSI_NUM_2P4GHZ_CHANNELS + SLSI_NUM_5GHZ_CHANNELS + i].rssi_factor = 10;
		ch_info[SLSI_NUM_2P4GHZ_CHANNELS + SLSI_NUM_5GHZ_CHANNELS + i].adj_rssi_factor = 10;
	}
	KUNIT_EXPECT_EQ(test, 0, slsi_set_band_any_auto_channel(sdev, ndev_vif, &acs_selected_channels, ch_info));
	KUNIT_EXPECT_EQ(test, 80, acs_selected_channels.ch_width);
	KUNIT_EXPECT_EQ(test, 37, acs_selected_channels.pri_channel);
	KUNIT_EXPECT_EQ(test, 45, acs_selected_channels.sec_channel);
	KUNIT_EXPECT_EQ(test, NL80211_BAND_6GHZ, acs_selected_channels.band);
	KUNIT_EXPECT_EQ(test, SLSI_ACS_MODE_IEEE80211A, acs_selected_channels.hw_mode);
#endif
}

static void test_slsi_acs_get_rssi_factor(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	int rssi;
	u8 ch_util;

	rssi = -20;
	ch_util = 10;
	KUNIT_EXPECT_NE(test, 0, slsi_acs_get_rssi_factor(sdev, rssi, ch_util));

	rssi = 20;
	KUNIT_EXPECT_NE(test, 0, slsi_acs_get_rssi_factor(sdev, rssi, ch_util));
}

static void test_slsi_acs_scan_results(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct slsi_acs_chan_info *ch_info;
	u8 *pos = NULL;
	struct ieee80211_mgmt *mgmt = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt) + 128, GFP_KERNEL);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	skb = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, ndev_vif->ifnum, 512);
	fapi_set_u16(skb, u.mlme_scan_ind.channel_frequency, dummy_ch_2ghz[0].center_freq * 2);
	fapi_set_s16(skb, u.mlme_scan_ind.rssi, -30);
	fapi_set_u16(skb, u.mlme_scan_ind.scan_id, SLSI_SCAN_HW_ID);

	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
	memset(mgmt->bssid, 0x0, sizeof(u8) * ETH_ALEN);

	pos = mgmt->u.beacon.variable;
	*pos++ = WLAN_EID_DS_PARAMS;
	*pos++ = 1;
	*pos++ = dummy_ch_2ghz[0].hw_value;

	fapi_append_data(skb, (u8 *)mgmt, sizeof(*mgmt) + (pos - mgmt->u.beacon.variable));

	ndev_vif->scan[0].scan_results = kunit_kzalloc(test, sizeof(struct slsi_scan_result), GFP_KERNEL);
	ndev_vif->scan[0].scan_results->hidden = 76;
	ndev_vif->scan[0].scan_results->beacon = skb;

	ndev_vif->scan[0].acs_request = kunit_kzalloc(test, sizeof(struct slsi_acs_request), GFP_KERNEL);
	ndev_vif->scan[0].acs_request->acs_chan_info[0].chan = dummy_ch_2ghz[0].hw_value;
	ndev_vif->scan[0].acs_request->acs_chan_info[0].num_ap = 0;

	sdev->wiphy->bands[NL80211_BAND_2GHZ] = &dummy_band_2ghz;

	ch_info = slsi_acs_scan_results(sdev, ndev_vif, SLSI_SCAN_HW_ID);
	KUNIT_EXPECT_NE(test, NULL, ch_info);
	KUNIT_EXPECT_EQ(test, 1, ch_info[0].chan);
	KUNIT_EXPECT_EQ(test, 1, ch_info[0].num_ap);
}

static void test_slsi_acs_scan_complete(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	skb = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, ndev_vif->ifnum, 0);
	ndev_vif->scan[0].scan_results = kunit_kzalloc(test, sizeof(struct slsi_scan_result), GFP_KERNEL);
	ndev_vif->scan[0].scan_results->hidden = 76;
	ndev_vif->scan[0].scan_results->beacon = skb;

	ndev_vif->scan[0].acs_request = kzalloc(sizeof(*ndev_vif->scan[0].acs_request), GFP_KERNEL);
	ndev_vif->scan[0].acs_request->acs_chan_info[0].chan = 1;
	ndev_vif->scan[0].acs_request->acs_chan_info[0].num_ap = 0;
	slsi_acs_scan_complete(sdev, dev, SLSI_SCAN_HW_ID);
	KUNIT_EXPECT_PTR_EQ(test, NULL, ndev_vif->scan[0].acs_request);

	ndev_vif->scan[0].acs_request = kzalloc(sizeof(*ndev_vif->scan[0].acs_request), GFP_KERNEL);
	ndev_vif->scan[0].acs_request->hw_mode = SLSI_ACS_MODE_IEEE80211A;
	slsi_acs_scan_complete(sdev, dev, SLSI_SCAN_HW_ID);
	KUNIT_EXPECT_PTR_EQ(test, NULL, ndev_vif->scan[0].acs_request);

	ndev_vif->scan[0].acs_request = kzalloc(sizeof(*ndev_vif->scan[0].acs_request), GFP_KERNEL);
	ndev_vif->scan[0].acs_request->hw_mode = SLSI_ACS_MODE_IEEE80211ANY;
	slsi_acs_scan_complete(sdev, dev, SLSI_SCAN_HW_ID);
	KUNIT_EXPECT_PTR_EQ(test, NULL, ndev_vif->scan[0].acs_request);

	kfree_skb(skb);
}

static void test_slsi_rx_scan_done_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct slsi_roaming_network_map_entry *network_map;
	struct list_head *pos;
	struct cfg80211_bss_ies *ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies) + sizeof(u8) * 20,
						     GFP_KERNEL);
	int len = 0;

	skb = fapi_alloc(mlme_scan_done_ind, MLME_SCAN_DONE_IND, ndev_vif->ifnum, 0);

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->iftype = NL80211_IFTYPE_WDS;
	ndev_vif->scan[0].acs_request = 0;

	ndev_vif->scan[0].acs_request = kmalloc(sizeof(struct slsi_acs_request), GFP_KERNEL);
	ndev_vif->scan[0].acs_request->hw_mode = SLSI_ACS_MODE_IEEE80211A;

	fapi_set_u16(skb, u.mlme_scan_done_ind.scan_id, 0);

	ndev_vif->sta.sta_bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	ndev_vif->sta.sta_bss->ies = ies;
	ies->len = 10;
	memset(ies->data, 0, 20);

	slsi_rx_scan_done_ind(sdev, dev, skb);

	list_for_each(pos, &ndev_vif->sta.network_map) {
		network_map = list_entry(pos, struct slsi_roaming_network_map_entry, list);
		kfree(network_map);
	}
}

static void test_slsi_scan_complete(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct cfg80211_scan_request *scan_req = kunit_kzalloc(test, sizeof(struct cfg80211_scan_request), GFP_KERNEL);
	struct cfg80211_sched_scan_request *sched_req = kunit_kzalloc(test, sizeof(struct cfg80211_sched_scan_request),
								      GFP_KERNEL);
	struct slsi_ssid_info *ssid_info = kzalloc(sizeof(struct slsi_ssid_info), GFP_KERNEL);
	struct slsi_bssid_info *bssid_info = kzalloc(sizeof(struct slsi_bssid_info), GFP_KERNEL);
	struct slsi_bssid_blacklist_info *blacklist_info = kzalloc(sizeof(struct slsi_bssid_blacklist_info),
								   GFP_KERNEL);
	struct slsi_scan_result	*scan_result = kunit_kzalloc(test, sizeof(struct slsi_scan_result), GFP_KERNEL);
	struct sk_buff *skb = NULL;
	struct ieee80211_mgmt *mgmt = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt) + 300, GFP_KERNEL);
	u16 scan_id = 0;
	bool aborted = 0;
	bool flush_scan_results = 1;

	scan_id = SLSI_SCAN_MAX;
	slsi_scan_complete(sdev, dev, scan_id, aborted, flush_scan_results);

	scan_id = SLSI_SCAN_HW_ID;
	slsi_scan_complete(sdev, dev, scan_id, aborted, flush_scan_results);

	scan_id = SLSI_SCAN_SCHED_ID;
	slsi_scan_complete(sdev, dev, scan_id, aborted, flush_scan_results);

	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = scan_req;
	ndev_vif->scan[SLSI_SCAN_SCHED_ID].sched_req = sched_req;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	ndev_vif->sta.ssid_info.next = &ssid_info->list;
	ndev_vif->sta.ssid_info.prev = &ndev_vif->sta.ssid_info;
	ssid_info->list.next = &ndev_vif->sta.ssid_info;
	ssid_info->list.prev = &ndev_vif->sta.ssid_info;

	ssid_info->bssid_list.next = &bssid_info->list;
	ssid_info->bssid_list.prev = &ssid_info->bssid_list;
	bssid_info->list.next = &ssid_info->bssid_list;
	bssid_info->list.prev = &ssid_info->bssid_list;

	ndev_vif->sta.blacklist_head.next = &blacklist_info->list;
	ndev_vif->sta.blacklist_head.prev = &ndev_vif->sta.blacklist_head;
	blacklist_info->list.next = &ndev_vif->sta.blacklist_head;
	blacklist_info->list.prev = &ndev_vif->sta.blacklist_head;
	slsi_scan_complete(sdev, dev, scan_id, aborted, flush_scan_results);

	skb = fapi_alloc(mlme_scan_done_ind, MLME_SCAN_DONE_IND, ndev_vif->ifnum, 512);
	fapi_append_data(skb, (u8 *)mgmt, sizeof(struct ieee80211_mgmt) + 300);

	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results = scan_result;
	ndev_vif->scan[SLSI_SCAN_SCHED_ID].scan_results = scan_result;
	ndev_vif->sta.blacklist_head.next = &ndev_vif->sta.blacklist_head;
	ndev_vif->sta.blacklist_head.prev = &ndev_vif->sta.blacklist_head;
	scan_result->hidden = 76;
	scan_result->beacon = skb;
	flush_scan_results = 0;
	slsi_scan_complete(sdev, dev, scan_id, aborted, flush_scan_results);

	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	scan_id = SLSI_SCAN_HW_ID;
	scan_result->hidden = 76;
	scan_result->beacon = NULL;
	slsi_scan_complete(sdev, dev, scan_id, aborted, flush_scan_results);

	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = scan_req;
	ndev_vif->activated = 1;
	slsi_scan_complete(sdev, dev, scan_id, aborted, flush_scan_results);
}

static void test_slsi_rx_channel_switched_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	sdev->wiphy->bands[0] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band), GFP_KERNEL);
	sdev->wiphy->bands[0]->n_channels = 1;
	sdev->wiphy->bands[0]->channels = kunit_kzalloc(test, sizeof(struct ieee80211_channel) * 1, GFP_KERNEL);
	sdev->wiphy->bands[0]->channels[0].center_freq = 2422;
	sdev->wiphy->bands[0]->channels[0].hw_value = 2;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	skb = fapi_alloc(mlme_channel_switched_ind, MLME_CHANNEL_SWITCHED_IND, ndev_vif->ifnum, 0);

	ndev_vif->activated = false;

	slsi_rx_channel_switched_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_channel_switched_ind, MLME_CHANNEL_SWITCHED_IND, ndev_vif->ifnum, 0);
	ndev_vif->activated = true;
	fapi_set_u16(skb, u.mlme_channel_switched_ind.channel_information, 0x0f);
	fapi_set_u16(skb, u.mlme_channel_switched_ind.channel_frequency, 4844);

	slsi_rx_channel_switched_ind(sdev, dev, skb);
}

static void test_slsi_rx_blockack_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb;
	struct slsi_skb_cb *skb_cb;
	struct netdev_vif *ndev_vif;
	struct ieee80211_mgmt *mgmt;

	sdev->wiphy->bands[0] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band), GFP_KERNEL);
	sdev->wiphy->bands[0]->n_channels = 1;
	sdev->wiphy->bands[0]->channels = kunit_kzalloc(test, sizeof(struct ieee80211_channel) * 1, GFP_KERNEL);
	sdev->wiphy->bands[0]->channels[0].center_freq = 2422;
	sdev->wiphy->bands[0]->channels[0].hw_value = 2;

	skb = fapi_alloc(mlme_blacklisted_ind, MLME_BLACKLISTED_IND, 0, 1);

	ndev_vif = netdev_priv(dev);
	ndev_vif->sdev = sdev;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->activated = false;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = test_peer_alloc(test);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = 1;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->connected_state = SLSI_STA_CONN_STATE_CONNECTING;
	memcpy(ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->address, SLSI_DEFAULT_HW_MAC_ADDR, ETH_ALEN);
	fapi_set_memcpy(skb, u.mlme_blockack_action_ind.peer_sta_address,
			ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->address);
	slsi_rx_blockack_ind(sdev, dev, skb);

	ndev_vif->activated = true;
	skb = fapi_alloc(mlme_blockack_action_ind, MLME_BLOCKACK_ACTION_IND, 0, 1);
	fapi_set_u16(skb, u.ma_blockackreq_ind.timestamp, MA_BLOCKACKREQ_IND);
	slsi_rx_blockack_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_blockack_action_ind, MLME_BLOCKACK_ACTION_IND, 0, 1);
	mgmt = fapi_get_mgmt(skb);
	mgmt->u.action.category = WLAN_CATEGORY_BACK;
	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_ACTION);
	mgmt->u.action.u.addba_req.action_code = WLAN_ACTION_ADDBA_REQ;
	slsi_rx_blockack_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_blockack_action_ind, MLME_BLOCKACK_ACTION_IND, 0, 1);
	mgmt->u.action.u.addba_req.action_code = WLAN_ACTION_DELBA;
	slsi_rx_blockack_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_blockack_action_ind, MLME_BLOCKACK_ACTION_IND, 0, 1);
	mgmt->u.action.u.addba_req.action_code = WLAN_ACTION_ADDBA_RESP;
	slsi_rx_blockack_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_blockack_action_ind, MLME_BLOCKACK_ACTION_IND, 0, 1);
	mgmt->u.action.category = WLAN_CATEGORY_PUBLIC;
	slsi_rx_blockack_ind(sdev, dev, skb);
}

static void test_slsi_send_roam_vendor_event(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u8 bssid[ETH_ALEN] = "\x00\x00\x00\x00\x00\x00";
	u8 *req_ie = kunit_kzalloc(test, sizeof(char) * 15, GFP_KERNEL);
	u32 req_ie_len = 10;
	u8 *resp_ie = kunit_kzalloc(test, sizeof(char) * 15, GFP_KERNEL);
	u32 resp_ie_len = 10;
	u8 *beacon_ie = NULL;
	u32 beacon_ie_len = 0;
	bool authorized = true;

	memcpy(req_ie, "\x00\x0c\x0e\x54\x35\x35\x30\x38\x5f\x31\x58\x5f\x46\x54", 15);
	memcpy(resp_ie, "\x00\x0c\x0e\x54\x35\x35\x30\x38\x5f\x31\x58\x5f\x46\x54", 15);

	sdev->wiphy->n_vendor_events = 20;
	slsi_send_roam_vendor_event(sdev, bssid, req_ie, req_ie_len, resp_ie, resp_ie_len,
				    beacon_ie, beacon_ie_len, authorized);
}

static void test_slsi_rx_roamed_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = NULL;
	struct ieee80211_mgmt *mgmt = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt), GFP_KERNEL);
	struct cfg80211_bss_ies *ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies) + sizeof(u8) * 20,
						     GFP_KERNEL);

	sdev->wiphy->interface_modes = NL80211_IFTYPE_P2P_CLIENT;

	sdev->netdev[SLSI_NET_INDEX_P2P] = dev;
	skb = fapi_alloc(mlme_roamed_ind, MLME_ROAMED_IND, ndev_vif->ifnum, 512);
	fapi_append_data(skb, (u8 *)mgmt, sizeof(struct ieee80211_mgmt));

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;

	ndev_vif->sta.sta_bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	ndev_vif->sta.sta_bss->signal = 21;
	ndev_vif->sta.sta_bss->ies = ies;
	ndev_vif->sta.roam_mlme_procedure_started_ind = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = test_peer_alloc(test);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = 1;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->assoc_ie =
			kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->assoc_ie->data =
			kunit_kzalloc(test, sizeof(unsigned char)*16, GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->assoc_ie->len = 20;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->assoc_resp_ie =
			kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->assoc_resp_ie->data =
			kunit_kzalloc(test, sizeof(unsigned char)*16, GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->assoc_resp_ie->len = 20;
	memcpy(ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->address,
	       "\x00\x00\x00\x00\x00\x00", 6);
	memcpy(ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->assoc_ie->data,
	       "\x00\x0c\x0e\x54\x35\x35\x30\x38\x5f\x31\x58\x5f\x46\x54", 15);
	memcpy(ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->assoc_resp_ie->data,
	       "\x00\x0c\x0e\x54\x35\x35\x30\x38\x5f\x31\x58\x5f\x46\x54", 15);
	slsi_rx_roamed_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_roamed_ind, MLME_ROAMED_IND, ndev_vif->ifnum, 512);
	fapi_append_data(skb, (u8 *)mgmt, sizeof(struct ieee80211_mgmt));
	slsi_rx_roamed_ind(sdev, dev, skb);
}

static void test_slsi_rx_roam_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	skb = fapi_alloc(mlme_roam_ind, MLME_ROAM_IND, ndev_vif->ifnum, 0);

	ndev_vif->activated = false;

	slsi_rx_roam_ind(sdev, dev, skb);
}

static void test_slsi_tdls_event_discovered(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	skb = fapi_alloc(mlme_tdls_peer_ind, FAPI_TDLSEVENT_DISCOVERED, ndev_vif->ifnum, 0);

	slsi_tdls_event_discovered(sdev, dev, skb);
}

static void test_slsi_tdls_event_connected(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	skb = fapi_alloc(mlme_tdls_peer_ind, FAPI_TDLSEVENT_CONNECTED, ndev_vif->ifnum, 0);

	slsi_tdls_event_connected(sdev, dev, skb);
}

static void test_slsi_tdls_event_disconnected(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	skb = fapi_alloc(mlme_tdls_peer_ind, FAPI_TDLSEVENT_DISCONNECTED, ndev_vif->ifnum, 0);

	slsi_tdls_event_disconnected(sdev, dev, skb);
}


static void test_slsi_tdls_peer_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, ndev_vif->ifnum, 0);

	ndev_vif->activated = false;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.tdls_enabled = 1;
	slsi_tdls_peer_ind(sdev, dev, skb);

	ndev_vif->activated = true;

	skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, ndev_vif->ifnum, 0);
	fapi_set_u16(skb, u.mlme_tdls_peer_ind.tdls_event, FAPI_TDLSEVENT_CONNECTED);
	slsi_tdls_peer_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, ndev_vif->ifnum, 0);
	fapi_set_u16(skb, u.mlme_tdls_peer_ind.tdls_event, FAPI_TDLSEVENT_DISCONNECTED);
	slsi_tdls_peer_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, ndev_vif->ifnum, 0);
	fapi_set_u16(skb, u.mlme_tdls_peer_ind.tdls_event, FAPI_TDLSEVENT_DISCOVERED);
	slsi_tdls_peer_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, ndev_vif->ifnum, 0);
	fapi_set_u16(skb, u.mlme_tdls_peer_ind.tdls_event, FAPI_TDLSEVENT_DISCOVERED + 1);
	slsi_tdls_peer_ind(sdev, dev, skb);
}

static void test_slsi_rx_buffered_frames(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct slsi_skb_cb *skb_cb;

	skb = fapi_alloc(mlme_scan_ind, FAPI_REASONCODE_START, ndev_vif->ifnum, 0);
	fapi_set_u16(skb, id, MA_BLOCKACKREQ_IND);

	ndev_vif->activated = false;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.tdls_enabled = 1;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = test_peer_alloc(test);

	skb_queue_head_init(&ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->buffered_frames);
	__skb_queue_tail(&ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->buffered_frames, skb);
	slsi_rx_buffered_frames(sdev, dev, ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET], NUM_BA_SESSIONS_PER_PEER);

	skb = fapi_alloc(mlme_scan_ind, FAPI_REASONCODE_START, ndev_vif->ifnum, 0);
	fapi_set_u16(skb, id, MA_UNITDATA_IND);
	__skb_queue_tail(&ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->buffered_frames, skb);
	slsi_rx_buffered_frames(sdev, dev, ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET], NUM_BA_SESSIONS_PER_PEER);
}

static void test_slsi_rx_synchronised_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct slsi_skb_cb *skb_cb;

	skb = alloc_skb(100, GFP_KERNEL);

	ndev_vif->activated = false;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.tdls_enabled = 1;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = test_peer_alloc(test);
	ndev_vif->wlan_wl_sae.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	ndev_vif->wlan_wl_sae.ws->active = false;
	slsi_rx_synchronised_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_synchronised_ind, MLME_SYNCHRONISED_IND, 0, 1);
	ndev_vif->activated = true;
	fapi_set_memcpy(skb, u.mlme_synchronised_ind.bssid, "\x00\x00\x00\x00\x00\x00");
	fapi_set_high16_u32(skb, u.mlme_synchronised_ind.spare_1, 0x00FF);
	slsi_rx_synchronised_ind(sdev, dev, skb);

	fapi_set_high16_u32(skb, u.mlme_synchronised_ind.spare_1, 0x0);
	ndev_vif->sta.crypto.wpa_versions = 3;
	slsi_rx_synchronised_ind(sdev, dev, skb);
	kfree_skb(skb);
}

static void test_slsi_add_blacklist_info(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_bssid_blacklist_info *blacklist_info;
	struct slsi_bssid_blacklist_info *temp_blacklist_info;
	struct list_head *blacklist_pos;
	u8 addr[ETH_ALEN] = "\x00\x00\x00\x00\x00\x00";
	u32 retention_time = 1;

	blacklist_info = kunit_kzalloc(test, sizeof(struct slsi_bssid_blacklist_info), GFP_KERNEL);

	INIT_LIST_HEAD(&ndev_vif->acl_data_fw_list);
	slsi_add_blacklist_info(sdev, dev, ndev_vif, addr, retention_time);

	list_for_each(blacklist_pos, &ndev_vif->acl_data_fw_list) {
		temp_blacklist_info = list_entry(blacklist_pos, struct slsi_bssid_blacklist_info, list);
		if (SLSI_ETHER_EQUAL(temp_blacklist_info->bssid, addr)) {
			list_del(&temp_blacklist_info->list);
			kfree(temp_blacklist_info);
			break;
		}
	}

	list_add(&blacklist_info->list, &ndev_vif->acl_data_fw_list);
	memcpy(blacklist_info->bssid, "\x00\x00\x00\x00\x00\x00", ETH_ALEN);
	slsi_add_blacklist_info(sdev, dev, ndev_vif, addr, retention_time);
}

static void test_slsi_set_acl(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_bssid_blacklist_info *blacklist_info_1;
	struct slsi_bssid_blacklist_info *blacklist_info_2;

	blacklist_info_1 = kunit_kzalloc(test, sizeof(struct slsi_bssid_blacklist_info), GFP_KERNEL);
	blacklist_info_2 = kunit_kzalloc(test, sizeof(struct slsi_bssid_blacklist_info), GFP_KERNEL);

	INIT_LIST_HEAD(&ndev_vif->acl_data_fw_list);
	INIT_LIST_HEAD(&ndev_vif->acl_data_ioctl_list);
	list_add(&blacklist_info_1->list, &ndev_vif->acl_data_fw_list);
	list_add(&blacklist_info_2->list, &ndev_vif->acl_data_ioctl_list);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->acl_data_supplicant = kunit_kzalloc(test, sizeof(struct cfg80211_acl_data), GFP_KERNEL);
	ndev_vif->acl_data_hal = kunit_kzalloc(test, sizeof(struct cfg80211_acl_data), GFP_KERNEL);
	ndev_vif->acl_data_supplicant->n_acl_entries = 1;
	ndev_vif->acl_data_hal->n_acl_entries = 1;

	KUNIT_EXPECT_EQ(test, 0, slsi_set_acl(sdev, dev));
}

static void test_slsi_rx_blacklisted_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct slsi_skb_cb *skb_cb;
	struct slsi_bssid_blacklist_info *blacklist_info;

	skb = fapi_alloc(mlme_blacklisted_ind, MLME_BLACKLISTED_IND, 0, 1);
	blacklist_info = kunit_kzalloc(test, sizeof(struct slsi_bssid_blacklist_info), GFP_KERNEL);

	INIT_LIST_HEAD(&ndev_vif->acl_data_fw_list);
	list_add(&blacklist_info->list, &ndev_vif->acl_data_fw_list);
	memcpy(blacklist_info->bssid, "\x00\x00\x00\x00\x00\x00", ETH_ALEN);
	fapi_set_memcpy(skb, u.mlme_blacklisted_ind.bssid, "\x00\x00\x00\x00\x00\x00");
	fapi_set_u32(skb, u.mlme_blacklisted_ind.reassociation_retry_delay, 1);

	slsi_rx_blacklisted_ind(sdev, dev, skb);
}

static void test_slsi_rx_connect_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct slsi_peer *peer = NULL;
	struct cfg80211_bss *sta_bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	struct cfg80211_bss_ies *ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies) + 200, GFP_KERNEL);
	struct sk_buff *skb;
	struct sk_buff *assoc_ie = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u8 *data = kunit_kzalloc(test, sizeof(struct fapi_signal) + sizeof(struct ieee80211_mgmt) + 300, GFP_KERNEL);
	u8 *assoc_data = kunit_kzalloc(test, sizeof(struct fapi_signal) + sizeof(struct ieee80211_mgmt) + 300,
				       GFP_KERNEL);
	struct ieee80211_mgmt *mgmt = data + sizeof(struct fapi_signal);
	struct slsi_roaming_network_map_entry *network_map = kunit_kzalloc(test,
									   sizeof(struct slsi_roaming_network_map_entry),
									   GFP_KERNEL);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	slsi_rx_connect_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	ndev_vif->activated = 1;
	slsi_rx_connect_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	slsi_rx_connect_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	peer = test_peer_alloc(test);
	peer->valid = 1;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = peer;
	fapi_set_u16(skb, u.mlme_connect_ind.result_code, FAPI_RESULTCODE_AUTH_TX_FAIL);
	ndev_vif->sta.crypto.wpa_versions = 3;
	ndev_vif->sta.wpa3_auth_state = SLSI_WPA3_AUTHENTICATING;
	ndev_vif->sta.drv_bss_selection = 1;
	slsi_rx_connect_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	peer->valid = 0;
	ndev_vif->sta.drv_bss_selection = 0;
	fapi_set_u16(skb, u.mlme_connect_ind.result_code, FAPI_RESULTCODE_AUTH_TIMEOUT);
	slsi_rx_connect_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_connect_ind.result_code, FAPI_RESULTCODE_ASSOC_TIMEOUT);
	slsi_rx_connect_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_connect_ind.result_code, FAPI_RESULTCODE_PROBE_TIMEOUT);
	slsi_rx_connect_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_connect_ind.result_code, FAPI_RESULTCODE_AUTH_FAILED_CODE + 1);
	slsi_rx_connect_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_connect_ind.result_code, FAPI_RESULTCODE_ASSOC_FAILED_CODE + 1);
	slsi_rx_connect_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	mgmt->frame_control = cpu_to_le16(0x0010);
	slsi_rx_connect_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_connect_ind.result_code, 0x8300);
	slsi_rx_connect_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_connect_ind.result_code, FAPI_RESULTCODE_SUCCESS);
	peer->valid = 1;
	sdev->wiphy = wiphy;
	peer->capabilities = (1 << 4);
	ndev_vif->iftype = NL80211_IFTYPE_P2P_CLIENT;
	slsi_rx_connect_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	peer->assoc_ie = assoc_ie;
	peer->assoc_resp_ie = assoc_ie;
	assoc_ie->data = assoc_data;
	((struct slsi_skb_cb *)assoc_ie->cb)->sig_length = sizeof(struct fapi_signal);
	((struct slsi_skb_cb *)assoc_ie->cb)->data_length = sizeof(struct fapi_signal) + 300;
	ndev_vif->sta.mlme_scan_ind_skb = NULL;
	ndev_vif->sta.sta_bss = sta_bss;
	sta_bss->ies = ies;
	((struct element *)ies->data)->datalen = 3;
	((struct element *)ies->data)->data[1] = BIT(1) | BIT(2) | BIT(3);
	ndev_vif->sta.roam_on_disconnect = 1;
	slsi_rx_connect_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	assoc_ie->len = -1;
	ndev_vif->ipaddress = 1;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_CLIENT + 1;
	dev->dev_addr = network_map;
	sdev->band_6g_supported = 1;
	slsi_rx_connect_ind(sdev, dev, skb);
}

static void test_slsi_rx_disconnected_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct ieee80211_channel *chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	u8 *data = kunit_kzalloc(test, sizeof(struct fapi_signal) + sizeof(struct ieee80211_mgmt) + 300, GFP_KERNEL);
	struct ieee80211_mgmt *mgmt = data + sizeof(struct fapi_signal);

	skb = fapi_alloc(mlme_disconnected_ind, MLME_DISCONNECTED_IND, 0, 1);
	slsi_rx_disconnected_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_disconnected_ind, MLME_DISCONNECTED_IND, 0, 1);
	ndev_vif->activated = 1;
	fapi_set_u16(skb, u.mlme_disconnected_ind.reason_code, 0);
	mgmt->frame_control = cpu_to_le16(0x00c0);
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	slsi_rx_disconnected_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_disconnected_ind, MLME_DISCONNECTED_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_disconnected_ind.reason_code, 0x8100);
	mgmt->frame_control = cpu_to_le16(0x00a0);
	slsi_rx_disconnected_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_disconnected_ind, MLME_DISCONNECTED_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_disconnected_ind.reason_code, 0x8200);
	mgmt->frame_control = cpu_to_le16(0);
	slsi_rx_disconnected_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_disconnected_ind, MLME_DISCONNECTED_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_disconnected_ind.reason_code, FAPI_REASONCODE_HOTSPOT_MAX_CLIENT_REACHED);
	slsi_rx_disconnected_ind(sdev, dev, skb);
}

static void test_slsi_rx_p2p_device_discovered_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct ieee80211_channel *chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	u8 *data = kunit_kzalloc(test, sizeof(struct fapi_signal) + sizeof(struct ieee80211_mgmt) + 300, GFP_KERNEL);
	struct ieee80211_mgmt *mgmt = data + sizeof(struct fapi_signal);

	skb->data = data;
	((struct slsi_skb_cb *)skb->cb)->sig_length = sizeof(struct fapi_signal);
	((struct slsi_skb_cb *)skb->cb)->data_length = sizeof(struct fapi_signal) + 300;
	ndev_vif->chan = chan;
	mgmt->frame_control = cpu_to_le16(0x0040);
	slsi_rx_p2p_device_discovered_ind(sdev, dev, skb);

	mgmt->frame_control = cpu_to_le16(0x00f0);
	slsi_rx_p2p_device_discovered_ind(sdev, dev, skb);

	mgmt->frame_control = cpu_to_le16(0x000c);
	slsi_rx_p2p_device_discovered_ind(sdev, dev, skb);
}

static void test_slsi_rx_connected_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = test_peer_alloc(test);

	skb = fapi_alloc(mlme_connected_ind, MLME_CONNECTED_IND, 0, 1);
	slsi_rx_connected_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connected_ind, MLME_CONNECTED_IND, 0, 1);
	ndev_vif->activated = 1;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	slsi_rx_connected_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connected_ind, MLME_CONNECTED_IND, 0, 1);
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	slsi_rx_connected_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connected_ind, MLME_CONNECTED_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_connected_ind.flow_id, 1 << 8);
	slsi_rx_connected_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connected_ind, MLME_CONNECTED_IND, 0, 1);

	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = 1;
	ndev_vif->ap.privacy = 1;
	skb_queue_head_init(&ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->buffered_frames);
	__skb_queue_tail(&ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->buffered_frames, skb);
	slsi_rx_connected_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connected_ind, MLME_CONNECTED_IND, 0, 1);
	ndev_vif->ap.privacy = 0;
	slsi_rx_connected_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_connected_ind, MLME_CONNECTED_IND, 0, 1);
	ndev_vif->vif_type = 1;
	slsi_rx_connected_ind(sdev, dev, skb);
}

static void test_slsi_rx_reassoc_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	struct slsi_peer *peer = NULL;
	struct sk_buff *skb;
	struct sk_buff *assoc_ie = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u8 *data = kunit_kzalloc(test, sizeof(struct fapi_signal) + sizeof(struct ieee80211_mgmt) + 300, GFP_KERNEL);

	skb = fapi_alloc(mlme_reassociate_ind, MLME_REASSOCIATE_IND, 0, 1);
	slsi_rx_reassoc_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_reassociate_ind, MLME_REASSOCIATE_IND, 0, 1);
	ndev_vif->activated = 1;
	slsi_rx_reassoc_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_reassociate_ind, MLME_REASSOCIATE_IND, 0, 1);
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	slsi_rx_reassoc_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_reassociate_ind, MLME_REASSOCIATE_IND, 0, 1);
	peer = test_peer_alloc(test);
	ndev_vif->peer_sta_record[0] = peer;
	peer->valid = 1;
	fapi_set_u16(skb, u.mlme_reassociate_ind.result_code, FAPI_RESULTCODE_SUCCESS + 1);
	sdev->device_config.user_suspend_mode = EINVAL;
	slsi_rx_reassoc_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_reassociate_ind, MLME_REASSOCIATE_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_reassociate_ind.result_code, FAPI_RESULTCODE_SUCCESS);
	peer->assoc_ie = assoc_ie;
	peer->assoc_resp_ie = NULL;
	peer->capabilities = WLAN_CAPABILITY_PRIVACY;
	assoc_ie->data = data;
	assoc_ie->len = 1;
	slsi_rx_reassoc_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_reassociate_ind, MLME_REASSOCIATE_IND, 0, 1);
	peer->assoc_resp_ie = assoc_ie;
	assoc_ie->data = NULL;
	assoc_ie->len = -1;
	slsi_rx_reassoc_ind(sdev, dev, skb);
}

static void test_slsi_rx_ma_to_mlme_delba_req(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	skb = fapi_alloc(mlme_delba_req, SAP_DRV_MA_TO_MLME_DELBA_REQ, ndev_vif->ifnum,
			 sizeof(struct sap_drv_ma_to_mlme_delba_req));
	ndev_vif->activated = false;
	slsi_rx_ma_to_mlme_delba_req(sdev, dev, skb);

	skb = fapi_alloc(mlme_delba_req, SAP_DRV_MA_TO_MLME_DELBA_REQ, ndev_vif->ifnum,
			 sizeof(struct sap_drv_ma_to_mlme_delba_req));
	ndev_vif->activated = true;
	slsi_rx_ma_to_mlme_delba_req(sdev, dev, skb);
}

static void test_slsi_connect_result_code(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u16 fw_result_code;
	int status;
	enum nl80211_timeout_reason timeout_reason;

	slsi_connect_result_code(ndev_vif, FAPI_RESULTCODE_AUTH_NO_ACK, &status, &timeout_reason);

	ndev_vif->sta.crypto.wpa_versions = 3;
	slsi_connect_result_code(ndev_vif, FAPI_RESULTCODE_AUTH_NO_ACK, &status, &timeout_reason);

	slsi_connect_result_code(ndev_vif, FAPI_RESULTCODE_AUTH_TX_FAIL, &status, &timeout_reason);

	ndev_vif->sta.crypto.wpa_versions = 0;
	slsi_connect_result_code(ndev_vif, FAPI_RESULTCODE_AUTH_TX_FAIL, &status, &timeout_reason);

	slsi_connect_result_code(ndev_vif, FAPI_RESULTCODE_ASSOC_TIMEOUT, &status, &timeout_reason);

	slsi_connect_result_code(ndev_vif, FAPI_RESULTCODE_ASSOC_ABORT, &status, &timeout_reason);

	slsi_connect_result_code(ndev_vif, FAPI_RESULTCODE_ASSOC_NO_ACK, &status, &timeout_reason);

	slsi_connect_result_code(ndev_vif, FAPI_RESULTCODE_ASSOC_TX_FAIL, &status, &timeout_reason);
}

static void test_slsi_rx_procedure_started_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_peer *peer = NULL;
	struct sk_buff *skb;
	struct sk_buff *assoc_ie = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u8 *data = kunit_kzalloc(test, sizeof(struct fapi_signal) + sizeof(struct ieee80211_mgmt) + 300, GFP_KERNEL);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 1);
	slsi_rx_procedure_started_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 1);
	ndev_vif->activated = 1;
	fapi_set_u16(skb, u.mlme_procedure_started_ind.procedure_type, FAPI_PROCEDURETYPE_CONNECTION_STARTED);
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->peer_sta_records = SLSI_AP_PEER_CONNECTIONS_MAX;
	slsi_rx_procedure_started_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 1);
	ndev_vif->peer_sta_records = 0;
	slsi_rx_procedure_started_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_procedure_started_ind.flow_id, 1 << 8);
	slsi_rx_procedure_started_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 1);
	peer = test_peer_alloc(test);
	ndev_vif->peer_sta_record[0] = peer;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_GO;
	peer->assoc_ie = assoc_ie;
	peer->assoc_ie->data = data;
	peer->assoc_ie->len = 0;
	slsi_rx_procedure_started_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 1);
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->peer_sta_record[0] = NULL;
	slsi_rx_procedure_started_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 1);
	ndev_vif->peer_sta_record[0] = peer;
	slsi_rx_procedure_started_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 1);
	ndev_vif->vif_type = 0;
	slsi_rx_procedure_started_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 1);
	ndev_vif->iftype = 0;
	fapi_set_u16(skb, u.mlme_procedure_started_ind.procedure_type, FAPI_PROCEDURETYPE_DEVICE_DISCOVERED);
	slsi_rx_procedure_started_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 1);
	ndev_vif->iftype = NL80211_IFTYPE_P2P_GO;
	slsi_rx_procedure_started_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 1);
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	fapi_set_u16(skb, u.mlme_procedure_started_ind.procedure_type, FAPI_PROCEDURETYPE_ROAMING_STARTED);
	slsi_rx_procedure_started_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_procedure_started_ind.procedure_type, 0);
	slsi_rx_procedure_started_ind(sdev, dev, skb);
}

static void test_slsi_rx_frame_transmission_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_peer *peer = NULL;
	struct cfg80211_bss *sta_bss = kunit_kzalloc(test, sizeof(*sta_bss), GFP_KERNEL);
	struct ieee80211_mgmt *mgmt = kzalloc(sizeof(*mgmt), GFP_KERNEL);
	struct sk_buff *skb;
	u8 dev_addr[ETH_ALEN] = {0};

	skb = fapi_alloc(mlme_frame_transmission_ind, MLME_FRAME_TRANSMISSION_IND, 0, 1);
	slsi_rx_frame_transmission_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_frame_transmission_ind, MLME_FRAME_TRANSMISSION_IND, 0, 1);
	peer = test_peer_alloc(test);
	peer->valid = 1;
	ndev_vif->activated = 1;
	ndev_vif->sta.sta_bss = sta_bss;
	ndev_vif->peer_sta_record[0] = peer;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->sta.resp_id = MLME_ROAMED_RES;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	sdev->wlan_unsync_vif_state = WLAN_UNSYNC_VIF_TX;
	fapi_set_u16(skb, u.mlme_frame_transmission_ind.transmission_status, 1);
	slsi_rx_frame_transmission_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_frame_transmission_ind, MLME_FRAME_TRANSMISSION_IND, 0, 1);
	ndev_vif->sta.sta_bss = NULL;
	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	ndev_vif->sta.resp_id = MLME_CONNECT_RES;
	ndev_vif->mgmt_tx_data.exp_frame = SLSI_PA_INVALID;
	slsi_rx_frame_transmission_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_frame_transmission_ind, MLME_FRAME_TRANSMISSION_IND, 0, 1);
	mgmt->sa[0] = 1;
	dev->dev_addr = dev_addr;
	ndev_vif->mgmt_tx_data.buf = mgmt;
	ndev_vif->ifnum = SLSI_NET_INDEX_P2PX_SWLAN;
	ndev_vif->sta.resp_id = MLME_REASSOCIATE_RES;
	sdev->netdev[SLSI_NET_INDEX_P2P] = dev;
	slsi_rx_frame_transmission_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_frame_transmission_ind, MLME_FRAME_TRANSMISSION_IND, 0, 1);
	ndev_vif->mgmt_tx_data.host_tag = 0xf;
	fapi_set_u16(skb, u.mlme_frame_transmission_ind.transmission_status, 2);
	slsi_rx_frame_transmission_ind(sdev, dev, skb);
}

static void test_slsi_rx_received_frame_logging(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u8 *data = kunit_kzalloc(test, sizeof(struct fapi_signal) + sizeof(struct ieee80211_mgmt) + 500, GFP_KERNEL);
	u8 *eapol = data + sizeof(struct ethhdr);
	u8 *eap = data + sizeof(struct ethhdr);

	ndev_vif->sdev = sdev;
	skb->len = 285;
	skb->data = data;
	skb->protocol = htons(ETH_P_PAE);
	((struct slsi_skb_cb *)skb->cb)->sig_length = sizeof(struct fapi_signal);
	((struct slsi_skb_cb *)skb->cb)->data_length = sizeof(struct fapi_signal) + 300;
	eapol[SLSI_EAPOL_IEEE8021X_TYPE_POS] = SLSI_IEEE8021X_TYPE_EAPOL_KEY;
	eapol[SLSI_EAPOL_TYPE_POS] = SLSI_EAPOL_TYPE_RSN_KEY;
	eapol[SLSI_EAPOL_KEY_INFO_HIGHER_BYTE_POS] = SLSI_EAPOL_KEY_INFO_MIC_BIT_IN_HIGHER_BYTE;
	eapol[SLSI_EAPOL_KEY_INFO_LOWER_BYTE_POS] = 0xf;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	eapol[SLSI_EAPOL_TYPE_POS] = 0;
	eapol[SLSI_EAPOL_KEY_INFO_HIGHER_BYTE_POS] = 0;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	eapol[SLSI_EAPOL_KEY_INFO_HIGHER_BYTE_POS] = 0xf;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	eapol[SLSI_EAPOL_KEY_INFO_HIGHER_BYTE_POS] = SLSI_EAPOL_KEY_INFO_MIC_BIT_IN_HIGHER_BYTE;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	eapol[SLSI_EAPOL_KEY_INFO_HIGHER_BYTE_POS] = SLSI_EAPOL_KEY_INFO_SECURE_BIT_IN_HIGHER_BYTE;
	eapol[SLSI_EAPOL_KEY_INFO_LOWER_BYTE_POS] = SLSI_EAPOL_KEY_INFO_ACK_BIT_IN_LOWER_BYTE;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	eapol[SLSI_EAPOL_KEY_INFO_HIGHER_BYTE_POS] = 0;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	skb->len = 98;
	eap[SLSI_EAPOL_IEEE8021X_TYPE_POS] = SLSI_IEEE8021X_TYPE_EAP_PACKET;
	eap[SLSI_EAP_CODE_POS] = SLSI_EAP_PACKET_REQUEST;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	eap[SLSI_EAP_CODE_POS] = SLSI_EAP_PACKET_RESPONSE;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	eap[SLSI_EAP_CODE_POS] = SLSI_EAP_PACKET_SUCCESS;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	eap[SLSI_EAP_CODE_POS] = SLSI_EAP_PACKET_FAILURE;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	skb->protocol = htons(ETH_P_IP);
	skb->len = 285;
	skb->data[284] = SLSI_DHCP_MESSAGE_TYPE_DISCOVER;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	skb->data[284] = SLSI_DHCP_MESSAGE_TYPE_OFFER;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	skb->data[284] = SLSI_DHCP_MESSAGE_TYPE_REQUEST;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	skb->data[284] = SLSI_DHCP_MESSAGE_TYPE_DECLINE;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	skb->data[284] = SLSI_DHCP_MESSAGE_TYPE_ACK;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	skb->data[284] = SLSI_DHCP_MESSAGE_TYPE_NAK;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	skb->data[284] = SLSI_DHCP_MESSAGE_TYPE_RELEASE;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	skb->data[284] = SLSI_DHCP_MESSAGE_TYPE_INFORM;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	skb->data[284] = SLSI_DHCP_MESSAGE_TYPE_FORCERENEW;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	skb->data[284] = SLSI_DHCP_MESSAGE_TYPE_INVALID;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);

	skb->data[284] = 0;
	slsi_rx_received_frame_logging(dev, skb, NULL, 0);
}

static void test_slsi_rx_received_frame_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;
	struct slsi_peer *peer = NULL;

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, 0, 1);
	((struct slsi_skb_cb *)skb->cb)->data_length = sizeof(struct fapi_signal);
	slsi_rx_received_frame_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, 0, 1);
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	slsi_rx_received_frame_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, 0, 1);
	ndev_vif->vif_type = 0;
	slsi_rx_received_frame_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, 0, 1);
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	slsi_rx_received_frame_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, 0, 1);
	slsi_rx_received_frame_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, 0, 1);
	sdev->wlan_unsync_vif_state = WLAN_UNSYNC_VIF_TX;
	ndev_vif->mgmt_tx_data.exp_frame = SLSI_P2P_PA_GO_NEG_CFM;
	slsi_rx_received_frame_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, 0, 1);
	sdev->p2p_state = P2P_LISTENING;
	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	ndev_vif->vif_type = FAPI_VIFTYPE_DISCOVERY;
	ndev_vif->mgmt_tx_data.exp_frame = SLSI_P2P_PA_GO_NEG_CFM;
	ndev_vif->mgmt_tx_data.host_tag = 1;
	slsi_rx_received_frame_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, 0, 1);
	ndev_vif->mgmt_tx_data.exp_frame = SLSI_PA_INVALID;
	slsi_rx_received_frame_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, 0, 1);
	sdev->p2p_group_exp_frame = 1;
	ndev_vif->mgmt_tx_data.host_tag = 1;
	slsi_rx_received_frame_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, 0, 1);
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	fapi_set_u16(skb, u.mlme_received_frame_ind.data_unit_descriptor, FAPI_DATAUNITDESCRIPTOR_IEEE802_3_FRAME);
	slsi_rx_received_frame_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, 0, 1);
	peer = test_peer_alloc(test);
	peer->valid = 1;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = peer;
	fapi_set_u16(skb, u.mlme_received_frame_ind.data_unit_descriptor, FAPI_DATAUNITDESCRIPTOR_IEEE802_3_FRAME);
	slsi_rx_received_frame_ind(sdev, dev, skb);

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, 0, 1);
	skb->skb_iif = NET_RX_DROP;
	fapi_set_u16(skb, u.mlme_received_frame_ind.data_unit_descriptor, FAPI_DATAUNITDESCRIPTOR_IEEE802_3_FRAME);
	slsi_rx_received_frame_ind(sdev, dev, skb);
}

static void test_slsi_rx_mic_failure_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	slsi_rx_mic_failure_ind(sdev, dev, NULL);

	skb = fapi_alloc(mlme_mic_failure_ind, MLME_MIC_FAILURE_IND, 0, 1);
	ndev_vif->activated = 1;
	slsi_rx_mic_failure_ind(sdev, dev, skb);
}

static void test_slsi_rx_listen_end_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	slsi_rx_listen_end_ind(dev, NULL);

	ndev_vif->activated = 1;
	slsi_rx_listen_end_ind(dev, NULL);
}

static void test_slsi_rx_blocking_signals(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = NULL;

	skb = fapi_alloc(mlme_del_vif_cfm, MLME_DEL_VIF_CFM, cpu_to_le16(SLSI_NET_INDEX_DETECT), 0);
	ndev_vif->sig_wait.cfm_id = MLME_DEL_VIF_CFM;
	ndev_vif->sig_wait.cfm = alloc_skb(100, GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_blocking_signals(sdev, skb));
	KUNIT_EXPECT_PTR_EQ(test, ndev_vif->sig_wait.cfm, skb);
	kfree_skb(skb);

	skb = fapi_alloc(debug_spare_signal_1_req, DEBUG_SPARE_SIGNAL_1_IND, 1, 0);
	ndev_vif->sig_wait.cfm_id = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_rx_blocking_signals(sdev, skb));
	kfree_skb(skb);

	skb = fapi_alloc(debug_spare_signal_1_req, DEBUG_SPARE_SIGNAL_1_IND, 1, 0);
	ndev_vif->sig_wait.ind_id = DEBUG_SPARE_SIGNAL_1_IND;
	ndev_vif->sig_wait.ind = alloc_skb(100, GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_blocking_signals(sdev, skb));
	KUNIT_EXPECT_PTR_EQ(test, ndev_vif->sig_wait.ind, skb);
	kfree_skb(skb);

	skb = fapi_alloc(debug_spare_signal_1_req, DEBUG_SPARE_SIGNAL_1_IND, 1, 0);
	ndev_vif->sig_wait.ind_id = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_rx_blocking_signals(sdev, skb));
	kfree_skb(skb);
}

static void test_slsi_rx_twt_setup_info_event(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	skb = fapi_alloc(mlme_twt_setup_ind, MLME_TWT_SETUP_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_twt_setup_ind.result_code, FAPI_RESULTCODE_SUCCESS);
	slsi_rx_twt_setup_info_event(sdev, dev, skb);

	skb = fapi_alloc(mlme_twt_setup_ind, MLME_TWT_SETUP_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_twt_setup_ind.result_code, FAPI_RESULTCODE_TWT_SETUP_REJECTED);
	slsi_rx_twt_setup_info_event(sdev, dev, skb);

	skb = fapi_alloc(mlme_twt_setup_ind, MLME_TWT_SETUP_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_twt_setup_ind.result_code, FAPI_RESULTCODE_TWT_SETUP_TIMEOUT);
	slsi_rx_twt_setup_info_event(sdev, dev, skb);

	skb = fapi_alloc(mlme_twt_setup_ind, MLME_TWT_SETUP_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_twt_setup_ind.result_code, FAPI_RESULTCODE_TWT_SETUP_INVALID_IE);
	slsi_rx_twt_setup_info_event(sdev, dev, skb);

	skb = fapi_alloc(mlme_twt_setup_ind, MLME_TWT_SETUP_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_twt_setup_ind.result_code, FAPI_RESULTCODE_TWT_SETUP_PARAMS_VALUE_REJECTED);
	slsi_rx_twt_setup_info_event(sdev, dev, skb);

	skb = fapi_alloc(mlme_twt_setup_ind, MLME_TWT_SETUP_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_twt_setup_ind.result_code, 0xa005);
	slsi_rx_twt_setup_info_event(sdev, dev, skb);
}

static void test_slsi_rx_twt_teardown_indication(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	skb = fapi_alloc(mlme_twt_teardown_ind, MLME_TWT_TEARDOWN_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_twt_teardown_ind.reason_code, FAPI_REASONCODE_HOST_INITIATED);
	slsi_rx_twt_teardown_indication(sdev, dev, skb);

	skb = fapi_alloc(mlme_twt_teardown_ind, MLME_TWT_TEARDOWN_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_twt_teardown_ind.reason_code, FAPI_REASONCODE_PEER_INITIATED);
	slsi_rx_twt_teardown_indication(sdev, dev, skb);

	skb = fapi_alloc(mlme_twt_teardown_ind, MLME_TWT_TEARDOWN_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_twt_teardown_ind.reason_code, FAPI_REASONCODE_CONCURRENT_OPERATION_SAME_BAND);
	slsi_rx_twt_teardown_indication(sdev, dev, skb);

	skb = fapi_alloc(mlme_twt_teardown_ind, MLME_TWT_TEARDOWN_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_twt_teardown_ind.reason_code, FAPI_REASONCODE_CONCURRENT_OPERATION_DIFFERENT_BAND);
	slsi_rx_twt_teardown_indication(sdev, dev, skb);

	skb = fapi_alloc(mlme_twt_teardown_ind, MLME_TWT_TEARDOWN_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_twt_teardown_ind.reason_code, FAPI_REASONCODE_ROAMING_OR_ECSA);
	slsi_rx_twt_teardown_indication(sdev, dev, skb);

	skb = fapi_alloc(mlme_twt_teardown_ind, MLME_TWT_TEARDOWN_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_twt_teardown_ind.reason_code, FAPI_REASONCODE_BT_COEX);
	slsi_rx_twt_teardown_indication(sdev, dev, skb);

	skb = fapi_alloc(mlme_twt_teardown_ind, MLME_TWT_TEARDOWN_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_twt_teardown_ind.reason_code, FAPI_REASONCODE_TIMEOUT);
	slsi_rx_twt_teardown_indication(sdev, dev, skb);

	skb = fapi_alloc(mlme_twt_teardown_ind, MLME_TWT_TEARDOWN_IND, 0, 1);
	fapi_set_u16(skb, u.mlme_twt_teardown_ind.reason_code, 0);
	slsi_rx_twt_teardown_indication(sdev, dev, skb);
}

static void test_slsi_rx_twt_notification_indication(struct kunit *test)
{
	slsi_rx_twt_notification_indication(NULL, NULL, NULL);
}

static int rx_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void rx_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case rx_test_cases[] = {

	KUNIT_CASE(test_slsi_freq_to_band),
	KUNIT_CASE(test_slsi_find_scan_channel),
	KUNIT_CASE(test_slsi_rx_scan_update_ssid),
	KUNIT_CASE(test_slsi_rx_scan_pass_to_cfg80211),
	KUNIT_CASE(test_slsi_populate_bssid_info),
	KUNIT_CASE(test_slsi_gen_new_bssid),
	KUNIT_CASE(test_slsi_mbssid_to_ssid_list),
	KUNIT_CASE(test_slsi_extract_mbssids),
	KUNIT_CASE(test_slsi_remove_assoc_disallowed_bssid),
	KUNIT_CASE(test_slsi_reject_ap_for_scan_info),
	KUNIT_CASE(test_slsi_add_to_scan_list),
	KUNIT_CASE(test_slsi_populate_ssid_info),
	KUNIT_CASE(test_slsi_rx_scan_ind),
	KUNIT_CASE(test_slsi_rx_rcl_ind),
	KUNIT_CASE(test_slsi_rx_start_detect_ind),
	KUNIT_CASE(test_slsi_scan_update_ssid_map),
	KUNIT_CASE(test_slsi_scan_complete),
	KUNIT_CASE(test_slsi_set_2g_auto_channel),
	KUNIT_CASE(test_slsi_is_40mhz),
	KUNIT_CASE(test_slsi_is_80mhz),
	KUNIT_CASE(test_slsi_set_5g_auto_channel),
	KUNIT_CASE(test_slsi_set_band_any_auto_channel),
	KUNIT_CASE(test_slsi_acs_get_rssi_factor),
	KUNIT_CASE(test_slsi_acs_scan_results),
	KUNIT_CASE(test_slsi_acs_scan_complete),
	KUNIT_CASE(test_slsi_rx_scan_done_ind),
	KUNIT_CASE(test_slsi_rx_channel_switched_ind),
	KUNIT_CASE(test_slsi_rx_blockack_ind),
	KUNIT_CASE(test_slsi_rx_ma_to_mlme_delba_req),
	KUNIT_CASE(test_slsi_send_roam_vendor_event),
	KUNIT_CASE(test_slsi_rx_roamed_ind),
	KUNIT_CASE(test_slsi_rx_roam_ind),
	KUNIT_CASE(test_slsi_tdls_event_discovered),
	KUNIT_CASE(test_slsi_tdls_event_connected),
	KUNIT_CASE(test_slsi_tdls_event_disconnected),
	KUNIT_CASE(test_slsi_tdls_peer_ind),
	KUNIT_CASE(test_slsi_rx_buffered_frames),
	KUNIT_CASE(test_slsi_rx_synchronised_ind),
	KUNIT_CASE(test_slsi_add_blacklist_info),
	KUNIT_CASE(test_slsi_set_acl),
	KUNIT_CASE(test_slsi_rx_blacklisted_ind),
	KUNIT_CASE(test_slsi_rx_connected_ind),
	KUNIT_CASE(test_slsi_rx_reassoc_ind),
	KUNIT_CASE(test_slsi_connect_result_code),
	KUNIT_CASE(test_slsi_rx_connect_ind),
	KUNIT_CASE(test_slsi_rx_disconnected_ind),
	KUNIT_CASE(test_slsi_rx_p2p_device_discovered_ind),
	KUNIT_CASE(test_slsi_rx_procedure_started_ind),
	KUNIT_CASE(test_slsi_rx_frame_transmission_ind),
	KUNIT_CASE(test_slsi_rx_received_frame_logging),
	KUNIT_CASE(test_slsi_rx_received_frame_ind),
	KUNIT_CASE(test_slsi_rx_mic_failure_ind),
	KUNIT_CASE(test_slsi_rx_listen_end_ind),
	KUNIT_CASE(test_slsi_rx_blocking_signals),
	KUNIT_CASE(test_slsi_rx_twt_setup_info_event),
	KUNIT_CASE(test_slsi_rx_twt_teardown_indication),
	KUNIT_CASE(test_slsi_rx_twt_notification_indication),
	{}
};

static struct kunit_suite rx_test_suite[] = {
	{
		.name = "kunit-rx-test",
		.test_cases = rx_test_cases,
		.init = rx_test_init,
		.exit = rx_test_exit,
	}
};

kunit_test_suites(rx_test_suite);
