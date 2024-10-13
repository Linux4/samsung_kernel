#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-misc.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-mlme.h"
#include "kunit-mock-dev.h"
#include "kunit-mock-cm_if.h"
#include "kunit-mock-nl80211_vendor_nan.h"
#include "kunit-mock-log2us.h"
#include "kunit-mock-mib.h"
#include "kunit-mock-scsc_wifilogger.h"
#include "kunit-mock-rx.h"
#include "../nl80211_vendor.c"

#define NLA_DATA(na) ((void *)((char *)(na) + NLA_HDRLEN))
#define NLA_DATA_SIZE(na, size) ((void *)((char *)(na) + NLA_HDRLEN + size))

static void set_wifi_tag_vendor_spec(u8 data[], int *i, int tag_len, u16 tag_id_1, u16 tag_id_2, u16 vtab_value)
{
	// SLSI_WIFI_TAG_VENDOR_SPECIFIC
	data[*i] = 0;
	data[*i + 2] = tag_len;
	*i += 4;

	data[*i + 1] = tag_id_1;	// tag_value = vtag_id
	data[*i] = tag_id_2;		// SLSI_WIFI_TAG_VD_CLUSTER_ID
	// vtag_value
	if (tag_len > 2)
		data[*i + 2] = vtab_value;

	*i += tag_len;
}

static void set_wifi_tag_id_set(u8 data[], int *i, int tag_len, u16 tag_id, int tab_value)
{
	data[*i] = tag_id;
	data[*i + 2] = tag_len;
	*i += 4;
	data[*i] = tab_value;
	*i += tag_len;
}

static void set_wifi_event_test(struct slsi_dev *sdev, struct net_device *dev, int event_id,
				u16 vtag_id_1, u16 vtag_id_2, int tag_len, int tab_value)
{
	struct sk_buff *skb = fapi_alloc(mlme_event_log_ind, MLME_EVENT_LOG_IND, 0, sizeof(u8) * 12);
	u8 data[10];
	int i = 0;

	memset(data, 0, 10);

	event_id = event_id;
	fapi_set_u16(skb, u.mlme_event_log_ind.event, event_id);
	if (vtag_id_1 != 0 && vtag_id_2 != 0) {
		set_wifi_tag_vendor_spec(&data, &i, tag_len, vtag_id_1, vtag_id_2, tab_value);
		fapi_append_data(skb, data, sizeof(data));
	}

	slsi_rx_event_log_indication(sdev, dev, skb);
}

static struct kunit_nlattr *kunit_nla_set_nested_attr(struct nlattr *nla_attr, u16 type, const void *data,
						      int data_type, int data_len, int *len)
{
	struct nlattr *res;

	res = nla_attr;

	while (res->nla_len) {
		res = (struct nlattr *)((char *)res + NLA_ALIGN(res->nla_len));
	};

	if (data_type) {
		res->nla_len = sizeof(char) * data_len + NLA_HDRLEN;
		if (data) {
			memcpy(NLA_DATA(res), data, sizeof(char) * data_len);
		}
	} else {
		res->nla_len = sizeof(data) + NLA_HDRLEN;
		if (data) {
			memcpy(NLA_DATA(res), data, sizeof(data));
		}
	}

	res->nla_type = type;
	*len += NLA_ALIGN(res->nla_len);

	return res;
}

static void test_slsi_vendor_event(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char data[10];

	memset(data, 1, 10);
	sdev->wiphy->n_vendor_events = 10;
	KUNIT_EXPECT_EQ(test, -ENOMEM, slsi_vendor_event(sdev, 12, (void *)data, 10));
	kunit_free_skb();

	KUNIT_EXPECT_EQ(test, 0, slsi_vendor_event(sdev, 1, (void *)data, 10));
	kunit_free_skb();
}

static void test_slsi_vendor_cmd_reply(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	char data[10];

	memset(data, 1, 10);
	KUNIT_EXPECT_EQ(test, -ENOMEM, slsi_vendor_cmd_reply(NULL, NULL, 10));
	kunit_free_skb();

	KUNIT_EXPECT_EQ(test, 1, slsi_vendor_cmd_reply(wiphy, data, 10));
	kunit_free_skb();
}

static void test_slsi_gscan_get_vif(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->netdev[SLSI_NET_INDEX_WLAN] = NULL;
	KUNIT_EXPECT_EQ(test, NULL, (struct netdev_vif *)slsi_gscan_get_vif(sdev));

	sdev->netdev[SLSI_NET_INDEX_WLAN] = dev;
	KUNIT_EXPECT_EQ(test, ndev_vif, (struct netdev_vif *)slsi_gscan_get_vif(sdev));
}

static void test_slsi_number_digits(struct kunit *test)
{
	int num;

	num = 214;
	KUNIT_EXPECT_EQ(test, 3, slsi_number_digits(num));
}

static void test_slsi_gscan_get_capabilities(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	char data[10];

	memset(data, 1, 10);
	kunit_set_gscan_disabled(true);
	KUNIT_EXPECT_EQ(test, -ENOTSUPP, slsi_gscan_get_capabilities(wiphy, NULL, (void *)data, 10));

	kunit_set_gscan_disabled(false);
	KUNIT_EXPECT_EQ(test, 1, slsi_gscan_get_capabilities(wiphy, NULL, (void *)data, 10));
}

static void test_slsi_gscan_put_channels(struct kunit *test)
{
	struct ieee80211_supported_band *chan_data = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band),
								   GFP_KERNEL);
	u32 buf;

	chan_data->channels = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	chan_data->n_channels = 1;
	chan_data->channels[0].flags = (IEEE80211_CHAN_NO_IR | IEEE80211_CHAN_NO_OFDM | IEEE80211_CHAN_RADAR);
	chan_data->channels[0].center_freq = 2412;

	KUNIT_EXPECT_EQ(test, 0, slsi_gscan_put_channels(NULL, false, true, &buf));
	KUNIT_EXPECT_EQ(test, 1, slsi_gscan_put_channels(chan_data, false, true, &buf));
	KUNIT_EXPECT_EQ(test, 0, slsi_gscan_put_channels(chan_data, true, false, &buf));
	KUNIT_EXPECT_EQ(test, 1, slsi_gscan_put_channels(chan_data, false, false, &buf));

	chan_data->channels[0].flags = IEEE80211_CHAN_DISABLED;
	KUNIT_EXPECT_EQ(test, 0, slsi_gscan_put_channels(chan_data, true, false, &buf));
}

static void test_slsi_gscan_get_valid_channel(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr), GFP_KERNEL);
	int len = SLSI_NL_VENDOR_DATA_OVERHEAD - 1;
	u32 data[2] = {0x01, 0x00};

	nla_attr->nla_type = GSCAN_ATTRIBUTE_BASE_PERIOD;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_gscan_get_valid_channel(wiphy, NULL, (void *)nla_attr, len));

	len = SLSI_NL_VENDOR_DATA_OVERHEAD;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_gscan_get_valid_channel(wiphy, NULL, (void *)nla_attr, len));

	nla_attr->nla_type = GSCAN_ATTRIBUTE_BAND;
	KUNIT_EXPECT_EQ(test, 0, slsi_gscan_get_valid_channel(wiphy, NULL, (void *)nla_attr, len));

	nla_attr->nla_len = sizeof(data) + NLA_HDRLEN;
	nla_attr->nla_type = GSCAN_ATTRIBUTE_BAND;
	memcpy(NLA_DATA(nla_attr), data, sizeof(data));
	KUNIT_EXPECT_EQ(test, -ENOTSUPP, slsi_gscan_get_valid_channel(wiphy, NULL, (void *)nla_attr, len));

	wiphy->bands[NL80211_BAND_2GHZ] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band), GFP_KERNEL);
	wiphy->bands[NL80211_BAND_5GHZ] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band), GFP_KERNEL);

	wiphy->bands[NL80211_BAND_2GHZ]->n_channels = 1;
	wiphy->bands[NL80211_BAND_5GHZ]->n_channels = 1;

	wiphy->bands[NL80211_BAND_2GHZ]->channels = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	wiphy->bands[NL80211_BAND_2GHZ]->channels[0].flags = (IEEE80211_CHAN_NO_IR | IEEE80211_CHAN_NO_OFDM |
							      IEEE80211_CHAN_RADAR);
	wiphy->bands[NL80211_BAND_2GHZ]->channels[0].center_freq = 2412;
	wiphy->bands[NL80211_BAND_5GHZ]->channels = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	wiphy->bands[NL80211_BAND_5GHZ]->channels[0].flags = (IEEE80211_CHAN_NO_IR | IEEE80211_CHAN_NO_OFDM |
							      IEEE80211_CHAN_RADAR);
	wiphy->bands[NL80211_BAND_5GHZ]->channels[0].center_freq = 5250;
	KUNIT_EXPECT_EQ(test, 1, slsi_gscan_get_valid_channel(wiphy, NULL, (void *)nla_attr, len));

	data[0] = 0x02;
	memcpy(NLA_DATA(nla_attr), data, sizeof(data));
	KUNIT_EXPECT_EQ(test, 1, slsi_gscan_get_valid_channel(wiphy, NULL, (void *)nla_attr, len));

	data[0] = 0x04;
	memcpy(NLA_DATA(nla_attr), data, sizeof(data));
	KUNIT_EXPECT_EQ(test, 1, slsi_gscan_get_valid_channel(wiphy, NULL, (void *)nla_attr, len));

	data[0] = 0x06;
	memcpy(NLA_DATA(nla_attr), data, sizeof(data));
	KUNIT_EXPECT_EQ(test, 1, slsi_gscan_get_valid_channel(wiphy, NULL, (void *)nla_attr, len));

	data[0] = 0x03;
	memcpy(NLA_DATA(nla_attr), data, sizeof(data));
	KUNIT_EXPECT_EQ(test, 1, slsi_gscan_get_valid_channel(wiphy, NULL, (void *)nla_attr, len));

	data[0] = 0x07;
	memcpy(NLA_DATA(nla_attr), data, sizeof(data));
	KUNIT_EXPECT_EQ(test, 1, slsi_gscan_get_valid_channel(wiphy, NULL, (void *)nla_attr, len));

	data[0] = 0x0A;
	memcpy(NLA_DATA(nla_attr), data, sizeof(data));
	KUNIT_EXPECT_EQ(test, 1, slsi_gscan_get_valid_channel(wiphy, NULL, (void *)nla_attr, len));
}

static void test_slsi_prepare_scan_result(struct kunit *test)
{
	struct sk_buff *skb = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, 0, 50);
	struct ieee80211_mgmt *buf = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt) + sizeof(u8) * 12, GFP_KERNEL);
	struct slsi_gscan_result *res;

	memcpy(&buf->u.beacon.variable[0], "\x00\x06x75\x72\x65\x61\x64\x79", 11);
	buf->u.beacon.capab_info = 0x12;
	buf->u.beacon.beacon_int = -32;
	SLSI_ETHER_COPY(buf->bssid, SLSI_DEFAULT_HW_MAC_ADDR);

	fapi_set_u16(skb, u.mlme_scan_ind.channel_frequency, 4824);
	fapi_set_s16(skb, u.mlme_scan_ind.rssi, -35);

	slsi_skb_cb_get(skb)->data_length = 100;
	memcpy((skb->data) + fapi_get_siglen(skb), buf, sizeof(struct ieee80211_mgmt));

	res = slsi_prepare_scan_result(skb, 2, 3);
	kfree(res);
	res = NULL;
}

static void test_slsi_gscan_hash_add(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_gscan_result *scan_res = kunit_kzalloc(test, sizeof(struct slsi_gscan_result), GFP_KERNEL);

	scan_res->nl_scan_res.bssid[5] = "0x01";

	sdev->gscan_hash_table[1] = kunit_kzalloc(test, sizeof(struct slsi_gscan_result), GFP_KERNEL);

	slsi_gscan_hash_add(sdev, scan_res);
}

static void test_slsi_nl80211_vendor_init_deinit(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	slsi_nl80211_vendor_init(sdev);

	kunit_set_gscan_disabled(true);
	KUNIT_EXPECT_EQ(test, -ENOTSUPP, sdev->wiphy->vendor_commands[0].doit(sdev->wiphy, NULL, NULL, NULL));
	KUNIT_EXPECT_EQ(test, 0, sdev->wiphy->vendor_commands[5].doit(sdev->wiphy, NULL, NULL, NULL));
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, sdev->wiphy->vendor_commands[12].doit(sdev->wiphy, NULL, NULL, NULL));
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, sdev->wiphy->vendor_commands[13].doit(sdev->wiphy, NULL, NULL, NULL));
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, sdev->wiphy->vendor_commands[14].doit(sdev->wiphy, NULL, NULL, NULL));
	KUNIT_EXPECT_EQ(test, 0, sdev->wiphy->vendor_commands[16].doit(sdev->wiphy, NULL, NULL, NULL));
	KUNIT_EXPECT_EQ(test, -EIO, sdev->wiphy->vendor_commands[17].doit(sdev->wiphy, NULL, NULL, NULL));
	KUNIT_EXPECT_EQ(test, 0, sdev->wiphy->vendor_commands[32].doit(sdev->wiphy, NULL, NULL, NULL));

	slsi_nl80211_vendor_deinit(sdev);
}

static void test_slsi_gscan_hash_get(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u8 mac[ETH_ALEN] = {0x0C, 0x0C, 0x1C, 0xAA, 0xB1, 0x00};

	sdev->gscan_hash_table[0] = kunit_kzalloc(test, sizeof(struct slsi_gscan_result), GFP_KERNEL);
	memcpy(sdev->gscan_hash_table[0]->nl_scan_res.bssid, mac, ETH_ALEN);

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, slsi_gscan_hash_get(sdev, mac));

	SLSI_ETHER_COPY(sdev->gscan_hash_table[0]->nl_scan_res.bssid, SLSI_DEFAULT_HW_MAC_ADDR);

	KUNIT_EXPECT_FALSE(test, slsi_gscan_hash_get(sdev, mac));
}

static void test_slsi_gscan_hash_remove(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u8 mac[ETH_ALEN] = {0x0C, 0x0C, 0x1C, 0xAA, 0xB1, 0x00};

	sdev->gscan_hash_table[0] = kzalloc(sizeof(struct slsi_gscan_result), GFP_KERNEL);
	memcpy(sdev->gscan_hash_table[0]->nl_scan_res.bssid, mac, ETH_ALEN);
	sdev->num_gscan_results = 0;
	slsi_gscan_hash_remove(sdev, mac);

	if (sdev->gscan_hash_table[0]) {
		kfree(sdev->gscan_hash_table[0]);
		sdev->gscan_hash_table[0] = NULL;
	}

	sdev->gscan_hash_table[0] = kunit_kzalloc(test, sizeof(struct slsi_gscan_result), GFP_KERNEL);
	sdev->gscan_hash_table[0]->hnext = kunit_kzalloc(test, sizeof(struct slsi_gscan_result), GFP_KERNEL);
	SLSI_ETHER_COPY(sdev->gscan_hash_table[0]->nl_scan_res.bssid, SLSI_DEFAULT_HW_MAC_ADDR);
	sdev->num_gscan_results = 0;
	slsi_gscan_hash_remove(sdev, mac);


	if (sdev->gscan_hash_table[0]->hnext) {
		kunit_kfree(test, sdev->gscan_hash_table[0]->hnext);
		sdev->gscan_hash_table[0]->hnext = NULL;
	}

	if (sdev->gscan_hash_table[0]) {
		kunit_kfree(test, sdev->gscan_hash_table[0]);
		sdev->gscan_hash_table[0] = NULL;
	}

	sdev->gscan_hash_table[0] = kunit_kzalloc(test, sizeof(struct slsi_gscan_result), GFP_KERNEL);
	sdev->gscan_hash_table[0]->hnext = kzalloc(sizeof(struct slsi_gscan_result), GFP_KERNEL);
	SLSI_ETHER_COPY(sdev->gscan_hash_table[0]->nl_scan_res.bssid, SLSI_DEFAULT_HW_MAC_ADDR);
	SLSI_ETHER_COPY(sdev->gscan_hash_table[0]->hnext->nl_scan_res.bssid, mac);
	sdev->num_gscan_results = 0;
	slsi_gscan_hash_remove(sdev, mac);

	if (sdev->gscan_hash_table[0]->hnext) {
		kfree(sdev->gscan_hash_table[0]->hnext);
		sdev->gscan_hash_table[0]->hnext = NULL;
	}

	if (sdev->gscan_hash_table[0]) {
		kunit_kfree(test, sdev->gscan_hash_table[0]);
		sdev->gscan_hash_table[0] = NULL;
	}
}

static void test_slsi_check_scan_result(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u8 mac[ETH_ALEN] = {0x0C, 0x0C, 0x1C, 0xAA, 0xB1, 0x00};
	struct slsi_bucket *bucket = kunit_kzalloc(test, sizeof(struct slsi_bucket), GFP_KERNEL);
	struct slsi_gscan_result *new_scan_res = kunit_kzalloc(test, sizeof(struct slsi_gscan_result), GFP_KERNEL);

	sdev->gscan_hash_table[0] = kunit_kzalloc(test, sizeof(struct slsi_gscan_result), GFP_KERNEL);
	memcpy(sdev->gscan_hash_table[0]->nl_scan_res.bssid, mac, ETH_ALEN);
	memcpy(new_scan_res->nl_scan_res.bssid, mac, ETH_ALEN);

	KUNIT_EXPECT_EQ(test, SLSI_KEEP_SCAN_RESULT, slsi_check_scan_result(sdev, bucket, new_scan_res));

	sdev->buffer_consumed = SLSI_GSCAN_MAX_SCAN_CACHE_SIZE;
	KUNIT_EXPECT_EQ(test, SLSI_DISCARD_SCAN_RESULT, slsi_check_scan_result(sdev, bucket, new_scan_res));
}

static void test_slsi_gscan_handle_scan_result(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb = alloc_skb(100, GFP_KERNEL);
	u16 scan_id = 0xf020;
	bool scan_done = true;
	struct ieee80211_mgmt *buf = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt) + sizeof(u8) * 12, GFP_KERNEL);

	slsi_gscan_handle_scan_result(sdev, dev, skb, scan_id, scan_done);

	scan_id = 0xf012;
	skb = alloc_skb(100, GFP_KERNEL);
	sdev->bucket[2].used = false;
	SLSI_ETHER_COPY(buf->bssid, SLSI_DEFAULT_HW_MAC_ADDR);

	slsi_gscan_handle_scan_result(sdev, dev, skb, scan_id, scan_done);

	skb = alloc_skb(100, GFP_KERNEL);
	sdev->bucket[2].used = true;
	sdev->bucket[2].report_events = SLSI_REPORT_EVENTS_EACH_SCAN;
	sdev->bucket[2].gscan = kunit_kzalloc(test, sizeof(struct slsi_gscan), GFP_KERNEL);
	sdev->bucket[2].gscan->num_scans = 19;
	sdev->bucket[2].gscan->report_threshold_num_scans = 10;
	sdev->buffer_consumed = 20;
	sdev->buffer_threshold = 0;

	slsi_gscan_handle_scan_result(sdev, dev, skb, scan_id, scan_done);

	scan_done = false;
	skb = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, 0, 50);
	memcpy(&buf->u.beacon.variable[0], "\x00\x06x75\x72\x65\x61\x64\x79", 11);
	buf->u.beacon.capab_info = 0x12;
	buf->u.beacon.beacon_int = -32;
	fapi_set_u16(skb, u.mlme_scan_ind.channel_frequency, 4824);
	fapi_set_s16(skb, u.mlme_scan_ind.rssi, -35);
	slsi_skb_cb_get(skb)->data_length = 100;
	memcpy(((struct fapi_signal *)(skb)->data) + fapi_get_siglen(skb), buf, sizeof(struct ieee80211_mgmt));
	sdev->bucket[2].report_events = SLSI_REPORT_EVENTS_FULL_RESULTS;

	slsi_gscan_handle_scan_result(sdev, dev, skb, scan_id, scan_done);
	kunit_free_skb();

	skb = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, 0, 50);
	memcpy(&buf->u.beacon.variable[0], "\x00\x06x75\x72\x65\x61\x64\x79", 11);
	buf->u.beacon.capab_info = 0x12;
	buf->u.beacon.beacon_int = -32;
	fapi_set_u16(skb, u.mlme_scan_ind.channel_frequency, 4824);
	fapi_set_s16(skb, u.mlme_scan_ind.rssi, -35);
	slsi_skb_cb_get(skb)->data_length = 100;
	memcpy(((struct fapi_signal *)(skb)->data) + fapi_get_siglen(skb), buf, sizeof(struct ieee80211_mgmt));
	sdev->bucket[2].report_events = SLSI_REPORT_EVENTS_FULL_RESULTS;
	sdev->wiphy = kunit_kzalloc(test, sizeof(struct wiphy), GFP_KERNEL);
	sdev->wiphy->n_vendor_events = 10;
	slsi_gscan_handle_scan_result(sdev, dev, skb, scan_id, scan_done);
	kunit_free_skb();
}

static void test_slsi_gscan_get_scan_policy(struct kunit *test)
{
	u8 scan_policy = WIFI_BAND_UNSPECIFIED;

	KUNIT_EXPECT_EQ(test,
			FAPI_SCANPOLICY_ANY_RA,
			slsi_gscan_get_scan_policy(scan_policy));

	scan_policy = WIFI_BAND_BG;
	KUNIT_EXPECT_EQ(test,
			FAPI_SCANPOLICY_2_4GHZ,
			slsi_gscan_get_scan_policy(scan_policy));

	scan_policy = WIFI_BAND_A;
	KUNIT_EXPECT_EQ(test,
			FAPI_SCANPOLICY_5GHZ | FAPI_SCANPOLICY_NON_DFS,
			slsi_gscan_get_scan_policy(scan_policy));

	scan_policy = WIFI_BAND_A_DFS;
	KUNIT_EXPECT_EQ(test,
			FAPI_SCANPOLICY_5GHZ | FAPI_SCANPOLICY_DFS,
			slsi_gscan_get_scan_policy(scan_policy));

	scan_policy = WIFI_BAND_A_WITH_DFS;
	KUNIT_EXPECT_EQ(test,
			FAPI_SCANPOLICY_5GHZ | FAPI_SCANPOLICY_NON_DFS | FAPI_SCANPOLICY_DFS,
			slsi_gscan_get_scan_policy(scan_policy));

	scan_policy = WIFI_BAND_ABG;
	KUNIT_EXPECT_EQ(test,
			FAPI_SCANPOLICY_5GHZ | FAPI_SCANPOLICY_NON_DFS | FAPI_SCANPOLICY_2_4GHZ,
			slsi_gscan_get_scan_policy(scan_policy));

	scan_policy = WIFI_BAND_ABG_WITH_DFS;
	KUNIT_EXPECT_EQ(test,
			FAPI_SCANPOLICY_5GHZ | FAPI_SCANPOLICY_NON_DFS | FAPI_SCANPOLICY_DFS | FAPI_SCANPOLICY_2_4GHZ,
			slsi_gscan_get_scan_policy(scan_policy));

	scan_policy = WIFI_BAND_ABG + WIFI_BAND_ABG_WITH_DFS;
	KUNIT_EXPECT_EQ(test, FAPI_SCANPOLICY_ANY_RA, slsi_gscan_get_scan_policy(scan_policy));
}

static void test_slsi_gscan_add_verify_params(struct kunit *test)
{
	struct slsi_nl_gscan_param *nl_gscan_param = kunit_kzalloc(test, sizeof(struct slsi_nl_gscan_param), GFP_KERNEL);

	nl_gscan_param->max_ap_per_scan = -1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_gscan_add_verify_params(nl_gscan_param));

	nl_gscan_param->max_ap_per_scan = 1;
	nl_gscan_param->report_threshold_percent = -1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_gscan_add_verify_params(nl_gscan_param));

	nl_gscan_param->report_threshold_percent = 1;
	nl_gscan_param->num_buckets = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_gscan_add_verify_params(nl_gscan_param));

	nl_gscan_param->num_buckets = 1;
	nl_gscan_param->nl_bucket[0].band = WIFI_BAND_UNSPECIFIED;
	nl_gscan_param->nl_bucket[0].num_channels = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_gscan_add_verify_params(nl_gscan_param));

	nl_gscan_param->nl_bucket[0].band = WIFI_BAND_ABG;
	nl_gscan_param->nl_bucket[1].band = WIFI_BAND_ABG;
	nl_gscan_param->num_buckets = 2;
	nl_gscan_param->nl_bucket[0].num_channels = 3;
	nl_gscan_param->nl_bucket[1].num_channels = 3;
	nl_gscan_param->nl_bucket[1].report_events = 5;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_gscan_add_verify_params(nl_gscan_param));

	nl_gscan_param->nl_bucket[1].report_events = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_gscan_add_verify_params(nl_gscan_param));
}

static void test_slsi_gscan_add_to_list(struct kunit *test)
{
	struct slsi_gscan *sdev_gscan = kunit_kzalloc(test, sizeof(struct slsi_gscan), GFP_KERNEL);
	struct slsi_gscan *gscan = kunit_kzalloc(test, sizeof(struct slsi_gscan), GFP_KERNEL);

	slsi_gscan_add_to_list(sdev_gscan, gscan);
}

static void test_slsi_gscan_alloc_buckets(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_gscan	*gscan = kunit_kzalloc(test, sizeof(struct slsi_gscan), GFP_KERNEL);
	int num_buckets = 2;
	int i = 0;

	for (i = 1; i < SLSI_GSCAN_MAX_BUCKETS; i++)
		sdev->bucket[i].used = true;
	sdev->bucket[0].used = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_gscan_alloc_buckets(sdev, gscan, num_buckets));

	num_buckets = 1;

	for (i = 1; i < SLSI_GSCAN_MAX_BUCKETS; i++)
		sdev->bucket[i].used = true;
	sdev->bucket[0].used = false;

	KUNIT_EXPECT_EQ(test, 0, slsi_gscan_alloc_buckets(sdev, gscan, num_buckets));
}

static void test_slsi_gscan_free_buckets(struct kunit *test)
{
	struct slsi_gscan *gscan = kunit_kzalloc(test, sizeof(struct slsi_gscan), GFP_KERNEL);

	gscan->bucket[0] = kunit_kzalloc(test, sizeof(struct slsi_bucket), GFP_KERNEL);
	gscan->bucket[0]->used = true;
	gscan->bucket[0]->report_events = 12;
	gscan->bucket[0]->scan_id = 0x1234;
	gscan->num_buckets = 1;

	slsi_gscan_free_buckets(gscan);
}

static void test_slsi_gscan_flush_scan_results(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	sdev->gscan_hash_table[0] = kunit_kzalloc(test, sizeof(struct slsi_gscan_result), GFP_KERNEL);
	sdev->gscan_hash_table[0]->hnext = NULL;
	sdev->gscan_hash_table[0]->scan_res_len = 50;
	sdev->num_gscan_results = 2;
	sdev->buffer_consumed = 300;

	slsi_gscan_flush_scan_results(sdev);
}

static void test_slsi_gscan_add(struct kunit *test)
{
	struct wiphy *wiphy = kunit_kzalloc(test, sizeof(struct wiphy), GFP_KERNEL);
	struct wireless_dev *wdev = NULL;
	void *data = 0;
	int len = 10;

	KUNIT_EXPECT_EQ(test, -ENOTSUPP, slsi_gscan_add(wiphy, wdev, data, len));
}

static void test_slsi_gscan_del(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	void *data = 0;
	int len = 10;

	sdev->netdev[SLSI_NET_INDEX_WLAN] = NULL;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_gscan_del(wiphy, wdev, data, len));

	sdev->netdev[SLSI_NET_INDEX_WLAN] = dev;
	sdev->gscan = kzalloc(sizeof(struct slsi_gscan), GFP_KERNEL);
	sdev->gscan->bucket[0] = kunit_kzalloc(test, sizeof(struct slsi_bucket), GFP_KERNEL);
	sdev->gscan->bucket[0]->used = true;
	sdev->gscan->bucket[0]->report_events = 12;
	sdev->gscan->bucket[0]->scan_id = 0x1234;
	sdev->gscan->num_buckets = 1;

	KUNIT_EXPECT_EQ(test, 0, slsi_gscan_del(wiphy, wdev, data, len));
}

static void test_slsi_gscan_get_scan_results(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 20, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	u32 data[2] = {0x01, 0x02};

	nla_attr = kunit_nla_set_nested_attr(nla_attr, GSCAN_ATTRIBUTE_NUM_OF_RESULTS, data, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE, data, 0, 0, &len);

	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	sdev->netdev[SLSI_NET_INDEX_WLAN] = NULL;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_gscan_get_scan_results(wiphy, wdev, nla_attr, len));

	sdev->netdev[SLSI_NET_INDEX_WLAN] = dev;
	sdev->num_gscan_results = 0;

	KUNIT_EXPECT_EQ(test, 0, slsi_gscan_get_scan_results(wiphy, wdev, nla_attr, len));

	sdev->num_gscan_results = 3;
	sdev->device_config.qos_info = 981;

	KUNIT_EXPECT_EQ(test, -ENOMEM, slsi_gscan_get_scan_results(wiphy, wdev, nla_attr, len));

	sdev->device_config.qos_info = 1;
	sdev->gscan_hash_table[0] = kunit_kzalloc(test, sizeof(struct slsi_gscan_result), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 1, slsi_gscan_get_scan_results(wiphy, wdev, nla_attr, len));
}

static void test_slsi_rx_rssi_report_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb = fapi_alloc(mlme_rssi_report_ind, MLME_RSSI_REPORT_IND, 0, 4);

	fapi_set_s16(skb, u.mlme_rssi_report_ind.rssi, -32);
	slsi_rx_rssi_report_ind(sdev, dev, skb);
}

static void test_slsi_set_vendor_ie(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr		*nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 20, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	u32 data[2] = {0x2123123, 0x2123123};
	struct kunit_nlattr *testattr;

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_SCAN_DEFAULT_IES, data, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE, data, 0, 0, &len);

	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	ndev_vif->activated = true;

	KUNIT_EXPECT_EQ(test, 0, slsi_set_vendor_ie(wiphy, wdev, nla_attr, len));

	kfree(sdev->default_scan_ies);
}

static void test_slsi_key_mgmt_set_pmk(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *pmk = "0x123095982350914abd014940";
	int pmklen = 12;

	wdev->iftype = NL80211_IFTYPE_P2P_CLIENT;
	KUNIT_EXPECT_EQ(test, 0, slsi_key_mgmt_set_pmk(wiphy, wdev, pmk, pmklen));

	wdev->iftype = NL80211_IFTYPE_STATION;
	KUNIT_EXPECT_EQ(test, 0, slsi_key_mgmt_set_pmk(wiphy, wdev, pmk, pmklen));
}

static void test_slsi_set_bssid_blacklist(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 40, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	u32 num_bssids = 0x00001;
	u8 *bssids = SLSI_DEFAULT_HW_MAC_ADDR;

	sdev->netdev[SLSI_NET_INDEX_WLAN] = NULL;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_bssid_blacklist(wiphy, wdev, nla_attr, len));

	sdev->netdev[SLSI_NET_INDEX_WLAN] = dev;
	ndev_vif->acl_data_supplicant = kmalloc(sizeof(struct cfg80211_acl_data), GFP_KERNEL);
	ndev_vif->acl_data_hal = kmalloc(sizeof(struct cfg80211_acl_data), GFP_KERNEL);

	nla_attr = kunit_nla_set_nested_attr(nla_attr, GSCAN_ATTRIBUTE_NUM_BSSID, &num_bssids, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, GSCAN_ATTRIBUTE_BLACKLIST_BSSID, bssids, 1, 6, &len);
	nla_attr = kunit_nla_set_nested_attr(nla_attr, GSCAN_ATTRIBUTE_BLACKLIST_FROM_SUPPLICANT, &num_bssids, 0, 0, &len);

	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);
	KUNIT_EXPECT_EQ(test, 0, slsi_set_bssid_blacklist(wiphy, wdev, nla_attr, len));

	len = 0;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, GSCAN_ATTRIBUTE_NUM_BSSID, &num_bssids, 0, 0, &len);

	KUNIT_EXPECT_EQ(test, 0, slsi_set_bssid_blacklist(wiphy, wdev, nla_attr, len));

	nla_attr = kunit_nla_set_nested_attr(nla_attr, GSCAN_ATTRIBUTE_MAX, &num_bssids, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_bssid_blacklist(wiphy, wdev, nla_attr, len));

	kfree(ndev_vif->acl_data_supplicant);
	kfree(ndev_vif->acl_data_hal);
}

static void test_slsi_start_keepalive_offload(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 40, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	u16 ip_pkt_len = 5;
	u8 *ip_pkt  = "0x120938df09123aab9db0";
	u32 period = 100;
	u8 *dst_mac = SLSI_DEFAULT_HW_MAC_ADDR;
	u8 *src_mac = "\x01\x02\xDF\x1E\xB2\x39";
	u8 index = SLSI_MAX_KEEPALIVE_ID - 1;

	sdev->device_config.qos_info = 267;

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_start_keepalive_offload(wiphy, wdev, nla_attr, len));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_start_keepalive_offload(wiphy, wdev, nla_attr, len));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_start_keepalive_offload(wiphy, wdev, nla_attr, len));

	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_start_keepalive_offload(wiphy, wdev, nla_attr, len));

	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = true;

	nla_attr = kunit_nla_set_nested_attr(nla_attr, MKEEP_ALIVE_ATTRIBUTE_IP_PKT_LEN, &ip_pkt_len, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, MKEEP_ALIVE_ATTRIBUTE_IP_PKT, ip_pkt, 0, 0, &len);
	nla_attr = kunit_nla_set_nested_attr(nla_attr, MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC, &period, 0, 0, &len);
	nla_attr = kunit_nla_set_nested_attr(nla_attr, MKEEP_ALIVE_ATTRIBUTE_DST_MAC_ADDR, dst_mac, 1, 6, &len);
	nla_attr = kunit_nla_set_nested_attr(nla_attr, MKEEP_ALIVE_ATTRIBUTE_SRC_MAC_ADDR, src_mac, 1, 6, &len);
	nla_attr = kunit_nla_set_nested_attr(nla_attr, MKEEP_ALIVE_ATTRIBUTE_ID, &index, 0, 0, &len);

	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, 1, slsi_start_keepalive_offload(wiphy, wdev, nla_attr, len));

	sdev->device_config.qos_info = -267;
	KUNIT_EXPECT_EQ(test, 0, slsi_start_keepalive_offload(wiphy, wdev, nla_attr, len));

	nla_attr = kunit_nla_set_nested_attr(nla_attr, MKEEP_ALIVE_ATTRIBUTE_ID + 100, &index, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_start_keepalive_offload(wiphy, wdev, nla_attr, len));
}

static void test_slsi_stop_keepalive_offload(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 40, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	u8 index = SLSI_MAX_KEEPALIVE_ID - 1;

	sdev->device_config.qos_info = 267;

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_stop_keepalive_offload(wiphy, wdev, nla_attr, len));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_stop_keepalive_offload(wiphy, wdev, nla_attr, len));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_stop_keepalive_offload(wiphy, wdev, nla_attr, len));

	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;

	nla_attr = kunit_nla_set_nested_attr(nla_attr, MKEEP_ALIVE_ATTRIBUTE_ID, &index, 0, 0, &len);
	init_len = len;
	KUNIT_EXPECT_EQ(test, 0, slsi_stop_keepalive_offload(wiphy, wdev, nla_attr, len));

	nla_attr = kunit_nla_set_nested_attr(nla_attr, MKEEP_ALIVE_ATTRIBUTE_ID + 1, &index, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_stop_keepalive_offload(wiphy, wdev, nla_attr, len));
}

static void test_epno_hs_set_reset(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, slsi_reset_hs_params(0, 0, 0, 0));
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, slsi_set_hs_params(0, 0, 0, 0));
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, slsi_set_epno_ssid(0, 0, 0, 0));
}

static void test_slsi_set_rssi_monitor(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 40, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	u8 val = 1;
	s8 max_rssi = -40;
	s8 min_rssi = -40;

	sdev->device_config.qos_info = 267;
	sdev->netdev[SLSI_NET_INDEX_WLAN] = NULL;
	KUNIT_EXPECT_EQ(test, -ENODEV, slsi_set_rssi_monitor(wiphy, wdev, nla_attr, len));

	sdev->netdev[SLSI_NET_INDEX_WLAN] = dev;
	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_rssi_monitor(wiphy, wdev, nla_attr, len));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_rssi_monitor(wiphy, wdev, nla_attr, len));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_rssi_monitor(wiphy, wdev, nla_attr, len));

	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_RSSI_MONITOR_ATTRIBUTE_START, &val, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_RSSI_MONITOR_ATTRIBUTE_MIN_RSSI, &min_rssi, 0, 0, &len);
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_RSSI_MONITOR_ATTRIBUTE_MAX_RSSI, &max_rssi, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, 0, slsi_set_rssi_monitor(wiphy, wdev, nla_attr, len));

	min_rssi = 10;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_RSSI_MONITOR_ATTRIBUTE_MIN_RSSI, &min_rssi, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_rssi_monitor(wiphy, wdev, nla_attr, len));
}

static void test_slsi_lls_set_stats(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 40, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	u32 mpdu_size_threshold = 0x1234;
	u32 aggr_stat_gathering = 0x12;

	kunit_set_gscan_disabled(false);
	sdev->device_config.qos_info = 267;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, LLS_ATTRIBUTE_SET_MPDU_SIZE_THRESHOLD,
					     &mpdu_size_threshold, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, LLS_ATTRIBUTE_SET_AGGR_STATISTICS_GATHERING,
					     &aggr_stat_gathering, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);
	KUNIT_EXPECT_EQ(test, 0, slsi_lls_set_stats(wiphy, 0, 0, 0));

	nla_attr = kunit_nla_set_nested_attr(nla_attr, LLS_ATTRIBUTE_MAX, &aggr_stat_gathering, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);
	KUNIT_EXPECT_EQ(test, 0, slsi_lls_set_stats(wiphy, 0, 0, 0));
}

static void test_slsi_lls_clear_stats(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 40, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	u32 stats_clear_req_mask = 0x1234;
	u32 stop_req = 0x12;

	sdev->device_config.qos_info = 267;

	nla_attr = kunit_nla_set_nested_attr(nla_attr, LLS_ATTRIBUTE_CLEAR_STOP_REQUEST_MASK,
					     &stats_clear_req_mask, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, LLS_ATTRIBUTE_CLEAR_STOP_REQUEST, &stop_req, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, 0, slsi_lls_clear_stats(wiphy, wdev, nla_attr, len));

	nla_attr = kunit_nla_set_nested_attr(nla_attr, LLS_ATTRIBUTE_MAX, &stats_clear_req_mask, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, 0, slsi_lls_clear_stats(wiphy, wdev, nla_attr, len));
}

static void test_slsi_lls_get_stats(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_lls_get_stats(wiphy, 0, 0, 0));
}

static void test_slsi_gscan_set_oui(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, slsi_gscan_set_oui(0, 0, 0, 0));
}

static void test_slsi_get_feature_set(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr), GFP_KERNEL);

	sdev->band_5g_supported = true;

	KUNIT_EXPECT_EQ(test, 1, slsi_get_feature_set(wiphy, 0, nla_attr, 10));
}

static void test_slsi_set_country_code(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 40, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	char country_code = "KR";

	sdev->wlan_unsync_vif_state = WLAN_UNSYNC_VIF_ACTIVE;
	sdev->current_tspec_id = 12;

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_NL_ATTRIBUTE_COUNTRY_CODE, &country_code, 0, 0, &len);
	init_len = len;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_country_code(wiphy, wdev, nla_attr, len));

	nla_attr = kunit_nla_set_nested_attr(nla_attr, LLS_ATTRIBUTE_MAX, &country_code, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_country_code(wiphy, wdev, nla_attr, len));
}

static void test_slsi_apf_read_filter(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	sdev->device_config.fw_apf_supported = false;

	KUNIT_EXPECT_EQ(test, -ENOTSUPP, slsi_apf_read_filter(wiphy, 0, 0, 0));

	sdev->device_config.fw_apf_supported = true;

	KUNIT_EXPECT_EQ(test, 1, slsi_apf_read_filter(wiphy, 0, 0, 0));
}

static void test_slsi_apf_get_capabilities(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;

	wdev->netdev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	sdev->device_config.fw_apf_supported = false;

	KUNIT_EXPECT_EQ(test, -ENOTSUPP, slsi_apf_get_capabilities(wiphy, wdev, 0, 0));

	sdev->device_config.fw_apf_supported = true;
	sdev->wiphy = wiphy;

	KUNIT_EXPECT_EQ(test, 1, slsi_apf_get_capabilities(wiphy, wdev, 0, 0));

	sdev->device_config.qos_info = 981;

	KUNIT_EXPECT_EQ(test, -ENOMEM, slsi_apf_get_capabilities(wiphy, wdev, 0, 0));
}

static void test_slsi_apf_set_filter(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 40, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	u32 program_len = 3;
	u32 program = "ABC";

	sdev->device_config.fw_apf_supported = false;

	KUNIT_EXPECT_EQ(test, -ENOTSUPP, slsi_apf_set_filter(wiphy, wdev, nla_attr, len));
}

static void test_slsi_rtt_set_get(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;

	sdev->device_config.fw_apf_supported = false;

	KUNIT_EXPECT_LE(test, 0, slsi_rtt_get_capabilities(wiphy, 0, 0, 0));
	kunit_set_rtt_disabled(true);
	KUNIT_EXPECT_EQ(test, WIFI_HAL_ERROR_NOT_SUPPORTED, slsi_rtt_set_config(wiphy, wdev, 0, 0));
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, slsi_rtt_cancel_config(wiphy, wdev, 0, 0));
}

static void test_slsi_tx_rate_calc(struct kunit *test)
{
	struct sk_buff *nl_skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	int res = 0;
	u16 fw_rate = 0x4001;
	bool tx_rate = false;

	KUNIT_EXPECT_EQ(test, -90, slsi_tx_rate_calc(nl_skb, fw_rate, res, tx_rate));

	fw_rate = 0x8741;
	KUNIT_EXPECT_EQ(test, -90, slsi_tx_rate_calc(nl_skb, fw_rate, res, tx_rate));

	fw_rate = 0x8241;
	KUNIT_EXPECT_EQ(test, -90, slsi_tx_rate_calc(nl_skb, fw_rate, res, tx_rate));

	fw_rate = 0x8220;
	KUNIT_EXPECT_EQ(test, -90, slsi_tx_rate_calc(nl_skb, fw_rate, res, tx_rate));

	fw_rate = 0x8320;
	KUNIT_EXPECT_EQ(test, -90, slsi_tx_rate_calc(nl_skb, fw_rate, res, tx_rate));

	tx_rate = true;
	fw_rate = 0xc220;
	KUNIT_EXPECT_EQ(test, -90, slsi_tx_rate_calc(nl_skb, fw_rate, res, tx_rate));

	fw_rate = 0xc23F;
	KUNIT_EXPECT_EQ(test, -90, slsi_tx_rate_calc(nl_skb, fw_rate, res, tx_rate));
}


static void test_slsi_rx_range_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb = fapi_alloc(mlme_range_ind, MLME_RANGE_IND, 0, 50);

	sdev->wiphy->n_vendor_events = 100;

	fapi_set_u16(skb, u.mlme_range_ind.entries, 1);
	fapi_set_u16(skb, u.mlme_range_ind.rtt_id, 2);
	fapi_set_u32(skb, u.mlme_range_ind.timestamp, 20220721);

	sdev->rtt_id_params[1] = kunit_kzalloc(test, sizeof(struct slsi_rtt_id_params), GFP_KERNEL);
	sdev->rtt_id_params[1]->hal_request_id = 2;

	slsi_rx_range_ind(sdev, dev, skb);
}

static void test_slsi_rx_range_done_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb = fapi_alloc(mlme_range_ind, MLME_RANGE_IND, 0, 50);

	fapi_set_u16(skb, u.mlme_range_ind.rtt_id, 2);

	sdev->rtt_id_params[1] = kzalloc(sizeof(struct slsi_rtt_id_params), GFP_KERNEL);

	slsi_rx_range_done_ind(sdev, dev, skb);
}

static void test_slsi_rtt_remove_peer(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u8 *addr = SLSI_DEFAULT_HW_MAC_ADDR;
	u8 rtt_id_idx = 1;
	u8 count_addr = 1;

	sdev->rtt_id_params[rtt_id_idx] = kzalloc(sizeof(struct slsi_rtt_id_params) + sizeof(u8) * 12, GFP_KERNEL);
	sdev->rtt_id_params[rtt_id_idx]->peer_count = 1;
	sdev->rtt_id_params[rtt_id_idx]->peers[0] = "0x00";

	slsi_rtt_remove_peer(sdev, addr, rtt_id_idx, count_addr);

	sdev->rtt_id_params[rtt_id_idx]->peer_count = 1;
	memset(sdev->rtt_id_params[rtt_id_idx]->peers, 0, ETH_ALEN);

	slsi_rtt_remove_peer(sdev, addr, rtt_id_idx, count_addr);
}

#if IS_ENABLED(CONFIG_IPV6)
static void test_slsi_configure_nd_offload(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 40, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	u8 nd_offload_enabled = 1;
	u32 program = "ABC";

	wdev->netdev = NULL;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_configure_nd_offload(wiphy, wdev, nla_attr, len));

	wdev->netdev = dev;
	ndev_vif->activated = false;

	KUNIT_EXPECT_EQ(test, -EPERM, slsi_configure_nd_offload(wiphy, wdev, nla_attr, len));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = true;

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_NL_ATTRIBUTE_ND_OFFLOAD_VALUE,
					     &nd_offload_enabled, 0, 0, &len);
	init_len = len;

	KUNIT_EXPECT_EQ(test, 0, slsi_configure_nd_offload(wiphy, wdev, nla_attr, len));

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_APF_ATTR_PROGRAM, &program, 0, 0, &len);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_configure_nd_offload(wiphy, wdev, nla_attr, len));
}
#endif

static void test_slsi_get_roaming_capabilities(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 40, GFP_KERNEL);
	int len = 0;

	wdev->netdev = NULL;
	sdev->wiphy = NULL;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_roaming_capabilities(wiphy, wdev, nla_attr, len));

	wdev->netdev = dev;
	KUNIT_EXPECT_EQ(test, -ENOMEM, slsi_get_roaming_capabilities(wiphy, wdev, nla_attr, len));

	sdev->wiphy = wiphy;

	KUNIT_EXPECT_EQ(test, 1, slsi_get_roaming_capabilities(wiphy, wdev, nla_attr, len));
}

static void test_slsi_set_roaming_state(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 40, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	u8 val = 10;

	wdev->netdev = NULL;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_roaming_state(wiphy, wdev, nla_attr, len));

	wdev->netdev = dev;

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_NL_ATTR_ROAM_STATE, &val, 0, 0, &len);
	init_len = len;

	KUNIT_EXPECT_EQ(test, 0, slsi_set_roaming_state(wiphy, wdev, nla_attr, len));

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_NL_ATTR_ROAM_STATE + SLSI_NL_ATTR_ROAM_STATE,
					     &val, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_roaming_state(wiphy, wdev, nla_attr, len));
}

static void test_str_type_check(struct kunit *test)
{
	u32 reason_code = FAPI_REASONCODE_UNKNOWN;

	KUNIT_EXPECT_STREQ(test, "UNKNOWN_REASON",
			   slsi_get_roam_reason_str(SLSI_WIFI_ROAMING_SEARCH_REASON_RESERVED, reason_code));
	KUNIT_EXPECT_STREQ(test, "Anchor Master", slsi_get_nan_role_str(1));
	KUNIT_EXPECT_STREQ(test, "eapol_key_m123", slsi_frame_transmit_failure_message_type(0x0002));
	KUNIT_EXPECT_STREQ(test, "Soft Cached scan", slsi_get_scan_type(FAPI_SCANTYPE_SOFT_CACHED_ROAMING_SCAN));
	KUNIT_EXPECT_STREQ(test, "beacon_table", slsi_get_measure_mode(2));
	KUNIT_EXPECT_STREQ(test, "NAN_DW", slsi_get_nan_schedule_type_str(3));
	KUNIT_EXPECT_STREQ(test, "Power Saving", slsi_get_nan_ulw_reason_str(5));
}

#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
static void test_slsi_handle_nan_rx_event_log_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb = fapi_alloc(mlme_event_log_ind, MLME_EVENT_LOG_IND, 0, 250);
	u16 event_id = FAPI_EVENT_WIFI_EVENT_NAN_AVAILABILITY_UPDATE;
	u8 *tlv_data;
	u8 taglen_size = 4;
	u16 taglen = 1;
	u16 tag_id;
	u8 data[230];
	u8 tmpdata[10];
	int i = 0;

	memset(tmpdata, 0, 10);
	memset(data, 0, 230);
	sdev->wiphy = 0;
	fapi_set_u16(skb, u.mlme_event_log_ind.event, event_id);
	set_wifi_tag_id_set(&data, &i, 1, 22, 1);
	set_wifi_tag_vendor_spec(&data, &i, 8, 0xf0, 0x26, 11);
	set_wifi_tag_vendor_spec(&data, &i, 6, 0xf0, 0x26, 11);
	set_wifi_tag_vendor_spec(&data, &i, 3, 0xf0, 0x27, 9);
	set_wifi_tag_vendor_spec(&data, &i, 3, 0xf0, 0x28, 9);
	set_wifi_tag_vendor_spec(&data, &i, 10, 0xf0, 0x28, 9);
	set_wifi_tag_vendor_spec(&data, &i, 8, 0xf0, 0x2a, 9);
	set_wifi_tag_vendor_spec(&data, &i, 3, 0xf0, 0x2a, 9);
	set_wifi_tag_vendor_spec(&data, &i, 3, 0xf0, 0x29, 9);
	set_wifi_tag_vendor_spec(&data, &i, 3, 0xf0, 0x30, 0);
	set_wifi_tag_vendor_spec(&data, &i, 3, 0xf0, 0x36, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x36, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x31, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x32, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x33, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x35, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x37, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x38, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x39, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x3a, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x3b, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x3c, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x3d, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x3e, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x3f, 0);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x40, 0);
	set_wifi_tag_vendor_spec(&data, &i, 2, 0xff, 0x00, 0);
	set_wifi_tag_vendor_spec(&data, &i, 2, 0xff, 0x00, 9);

	fapi_append_data(skb, data, sizeof(data));

	slsi_handle_nan_rx_event_log_ind(sdev, dev, skb);

	event_id = FAPI_EVENT_WIFI_EVENT_NAN_TRAFFIC_UPDATE;
	fapi_set_u16(skb, u.mlme_event_log_ind.event, event_id);

	slsi_handle_nan_rx_event_log_ind(sdev, dev, skb);

	kfree_skb(skb);

	skb = fapi_alloc(mlme_event_log_ind, MLME_EVENT_LOG_IND, 0, 10);
	event_id = FAPI_EVENT_WIFI_EVENT_NAN_TRAFFIC_UPDATE;
	fapi_set_u16(skb, u.mlme_event_log_ind.event, event_id);

	slsi_handle_nan_rx_event_log_ind(sdev, dev, skb);

	kfree_skb(skb);

	skb = fapi_alloc(mlme_event_log_ind, MLME_EVENT_LOG_IND, 0, 50);

	i = 0;
	event_id = FAPI_EVENT_WIFI_EVENT_NAN_TRAFFIC_UPDATE;
	fapi_set_u16(skb, u.mlme_event_log_ind.event, event_id);
	set_wifi_tag_vendor_spec(&tmpdata, &i, 2, 0xff, 0x00, 0);
	set_wifi_tag_vendor_spec(&tmpdata, &i, 2, 0xff, 0x00, 9);

	fapi_append_data(skb, tmpdata, sizeof(tmpdata));
	slsi_handle_nan_rx_event_log_ind(sdev, dev, skb);

	event_id = FAPI_EVENT_WIFI_EVENT_NAN_ULW_UPDATE;
	fapi_set_u16(skb, u.mlme_event_log_ind.event, event_id);

	slsi_handle_nan_rx_event_log_ind(sdev, dev, skb);

	event_id = FAPI_EVENT_WIFI_EVENT_NAN_TRAFFIC_UPDATE;
	fapi_set_u16(skb, u.mlme_event_log_ind.event, event_id);

	slsi_handle_nan_rx_event_log_ind(sdev, dev, skb);

	event_id = FAPI_EVENT_WIFI_EVENT_FW_NAN_ROLE_TYPE;
	fapi_set_u16(skb, u.mlme_event_log_ind.event, event_id);

	slsi_handle_nan_rx_event_log_ind(sdev, dev, skb);

	set_wifi_tag_vendor_spec(&tmpdata, &i, 500, 0xff, 0x00, 9);

	fapi_append_data(skb, tmpdata, sizeof(tmpdata));
	slsi_handle_nan_rx_event_log_ind(sdev, dev, skb);

	kfree_skb(skb);
}
#endif

static void test_dump_roam_scan_result(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	bool curr = true;
	char *bssid = SLSI_DEFAULT_HW_MAC_ADDR;
	int freq = 2412;
	int rssi = -34;
	short cu = 0x456;
	int score = 54;
	int tp_score = 68;
	bool eligible_value = false;

	dump_roam_scan_result(sdev, dev, &curr, bssid, freq, rssi, cu, score, tp_score, eligible_value);
	dump_roam_scan_result(sdev, dev, &curr, bssid, freq, rssi, cu, score, tp_score, eligible_value);
}

static void test_slsi_rx_event_log_indication(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct sk_buff *skb = fapi_alloc(mlme_event_log_ind, MLME_EVENT_LOG_IND, 0, 450);
	u16 event_id = FAPI_EVENT_WIFI_EVENT_FW_NAN_ROLE_TYPE;
	u8 *tlv_data;
	u8 taglen_size = 4;
	u16 taglen = 1;
	u16 tag_id;
	u8 data[400];
	int i = 0;

	event_id = FAPI_EVENT_WIFI_EVENT_ROAM_SCAN_RESULT;
	fapi_set_u16(skb, u.mlme_event_log_ind.event, event_id);

	skb = fapi_alloc(mlme_event_log_ind, MLME_EVENT_LOG_IND, 0, 450);
	event_id = FAPI_EVENT_WIFI_EVENT_ROAM_SCAN_RESULT;
	fapi_set_u16(skb, u.mlme_event_log_ind.event, event_id);
	memset(data, 0, 400);
	ndev_vif->iftype = NL80211_IFTYPE_STATION;

	set_wifi_tag_id_set(&data, &i, 1, 21, -45);
	set_wifi_tag_id_set(&data, &i, 1, 14, 3);

	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x1a, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x19, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x1b, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x1c, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x0f, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x08, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x12, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x1d, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x1e, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x22, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x23, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x24, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x25, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x21, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x2b, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x2d, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x2c, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x4c, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x4d, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x4b, 11);
	set_wifi_tag_vendor_spec(&data, &i, 2, 0xff, 0x00, 11);
	set_wifi_tag_vendor_spec(&data, &i, 2, 0xff, 0x00, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x41, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x42, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x43, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x44, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x46, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x47, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x14, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x48, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x1f, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x20, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x49, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x4a, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x0b, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x0b, 11);
	set_wifi_tag_vendor_spec(&data, &i, 4, 0xf0, 0x4e, 11);
	set_wifi_tag_id_set(&data, &i, 4, 29, SLSI_WIFI_EAPOL_KEY_TYPE_GTK);
	set_wifi_tag_id_set(&data, &i, 1, 4, 1);
	set_wifi_tag_id_set(&data, &i, 8, 1, SLSI_DEFAULT_HW_MAC_ADDR);
	set_wifi_tag_id_set(&data, &i, 2, 22, 2412);
	set_wifi_tag_id_set(&data, &i, 14, 3, "KUNIT_SSID");

	// SLSI_WIFI_TAG_IE
	data[i] = 12;
	data[i + 2] = 15;
	i += 4;
	data[i] = 0;
	data[i + 1] = 7;
	i += 15;

	// SLSI_WIFI_TAG_IE
	data[i] = 12;
	data[i + 2] = 15;
	i += 4;
	data[i] = 0;
	data[i + 1] = 7;
	data[i + 7] = 0xE2;
	data[i + 8] = 0x2C;
	i += 15;

	fapi_append_data(skb, data, sizeof(data));

	slsi_rx_event_log_indication(sdev, dev, skb);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_EAPOL_FRAME_TRANSMIT_START,
			    0xf0, 0x08, 4, SLSI_WIFI_EAPOL_KEY_TYPE_GTK);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_EAPOL_FRAME_TRANSMIT_START,
			    0xf0, 0x08, 4, SLSI_WIFI_EAPOL_KEY_TYPE_PTK);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_EAPOL_FRAME_RECEIVED,
			    0xf0, 0x08, 4, SLSI_WIFI_EAPOL_KEY_TYPE_GTK);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_EAPOL_FRAME_RECEIVED,
			    0xf0, 0x08, 4, SLSI_WIFI_EAPOL_KEY_TYPE_PTK);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_EAPOL_FRAME_TRANSMIT_STOP,
			    0xf0, 0x0f, 4, 34);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_BTM_FRAME_REQUEST,
			    0xf0, 0x0b, 4, 130);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_BTM_FRAME_RESPONSE,
			    0xf0, 0x1c, 4, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_BTM_FRAME_RESPONSE,
			    0xf0, 0x1c, 4, 10);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_BTM_FRAME_QUERY,
			    0xf0, 0x01, 4, 2);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_ROAM_SEARCH_STARTED,
			    0xf0, 0x19, 4, SLSI_WIFI_ROAMING_SEARCH_REASON_LOW_RSSI);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_AUTH_SENT,
			    0xf0, 0x4d, 4, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_AUTH_SENT,
			    0xf0, 0x4d, 4, 1);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_AUTH_RECEIVED,
			    0xf0, 0x4d, 4, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_AUTH_RECEIVED,
			    0xf0, 0x4d, 4, 1);

	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_ASSOC_STARTED,
			    0xf0, 0x4c, 4, SLSI_MGMT_FRAME_SUBTYPE_ASSOC_REQ);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_ASSOC_STARTED,
			    0xf0, 0x4c, 4, SLSI_MGMT_FRAME_SUBTYPE_REASSOC_REQ);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_ASSOC_COMPLETE,
			    0xf0, 0x4c, 4, SLSI_MGMT_FRAME_SUBTYPE_ASSOC_RESP);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_ASSOC_COMPLETE,
			    0xf0, 0x4c, 4, SLSI_MGMT_FRAME_SUBTYPE_REASSOC_RESP);

	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_DEAUTHENTICATION_RECEIVED,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_DEAUTHENTICATION_SENT,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_DISASSOCIATION_REQUESTED,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_DISASSOCIATION_RECEIVED,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_NR_FRAME_REQUEST,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, 59,
			    0x00, 0x00, 0, 0);

	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_AUTH_TIMEOUT,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_ROAM_AUTH_TIMEOUT,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_CONNECTION_ATTEMPT_ABORTED,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_ROAM_RSSI_THRESHOLD,
			    0xf0, 0x42, 4, -45);

	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_ROAM_SCAN_COMPLETE,
			    0xf0, 0x12, 4, 0x01);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_ROAM_SCAN_RESULT,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_ROAM_SEARCH_STOPPED,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_BEACON_REPORT_REQUEST,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, 73,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_FTM_RANGE_REQUEST,
			    0x00, 0x00, 0, 0);

	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_FRAME_TRANSMIT_FAILURE,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, 72,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_BT_COEX_BT_SCO_START,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_BT_COEX_BT_SCO_STOP,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_BT_COEX_BT_SCAN_START,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_BT_COEX_BT_SCAN_STOP,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_BT_COEX_BT_HID_START,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_BT_COEX_BT_HID_STOP,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_BTM_CANDIDATE,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_FW_NR_FRAME_RESPONSE,
			    0x00, 0x00, 0, 0);
	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_ROAM_SCAN_STARTED,
			    0x00, 0x00, 0, 0);

	skb = fapi_alloc(mlme_event_log_ind, MLME_EVENT_LOG_IND, 0, 250);
	event_id = FAPI_EVENT_WIFI_EVENT_FW_NAN_ROLE_TYPE;
	fapi_set_u16(skb, u.mlme_event_log_ind.event, event_id);

	slsi_rx_event_log_indication(sdev, dev, skb);

	set_wifi_event_test(sdev, dev, FAPI_EVENT_WIFI_EVENT_ROAM_SCAN_STARTED,
			    0xf0, 0xc1, 20, 0);
}

static void test_slsi_get_roam_reason_from_expired_tv(struct kunit *test)
{
	int roam_reason = SLSI_WIFI_ROAMING_SEARCH_REASON_LOW_RSSI;
	int expired_timer_value = SLSI_SOFT_ROAMING_TRIGGER_EVENT_DEFAULT;

	KUNIT_EXPECT_EQ(test, 1, slsi_get_roam_reason_from_expired_tv(roam_reason, expired_timer_value));

	expired_timer_value = SLSI_SOFT_ROAMING_TRIGGER_EVENT_INACTIVITY_TIMER;
	KUNIT_EXPECT_EQ(test,
			8,
			slsi_get_roam_reason_from_expired_tv(roam_reason, expired_timer_value));

	expired_timer_value = SLSI_SOFT_ROAMING_TRIGGER_EVENT_BACKGROUND_RESCAN_TIMER;
	KUNIT_EXPECT_EQ(test,
			9,
			slsi_get_roam_reason_from_expired_tv(roam_reason, expired_timer_value));

	expired_timer_value = -1;
	KUNIT_EXPECT_EQ(test, 0, slsi_get_roam_reason_from_expired_tv(roam_reason, expired_timer_value));
}

#ifdef CONFIG_SCSC_WLAN_ENHANCED_LOGGING
static void test_slsi_on_ring_buffer_data(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct scsc_wifi_ring_buffer_status *buffer_status = kunit_kzalloc(test,
									   sizeof(struct scsc_wifi_ring_buffer_status),
									   GFP_KERNEL);

	slsi_on_ring_buffer_data("kunit_nl80211_vendor", "kunit_logger", 13, buffer_status, sdev);

	sdev->wiphy = kunit_kzalloc(test, sizeof(struct wiphy), GFP_KERNEL);
	sdev->wiphy->n_vendor_events = SLSI_NL80211_LOGGER_RING_EVENT + 2;

	slsi_on_ring_buffer_data("kunit_nl80211_vendor", "kunit_logger", 13, buffer_status, sdev);
}

static void test_slsi_print_channel_list(struct kunit *test)
{
	int channel_list[2] = {9, 11};
	int channel_cnt = 1;
	char *res;

	res = slsi_print_channel_list(channel_list, channel_cnt);
	KUNIT_EXPECT_STREQ(test, "9 ", res);
	kfree(res);
}

static void test_slsi_on_alert(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	int buffer_size = 10;
	int err_code = 3;
	char buffer[10] = "KUNIT_TEST";

	slsi_on_alert(buffer, buffer_size, err_code, sdev);

	sdev->wiphy = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	sdev->wiphy->n_vendor_events = 13;

	slsi_on_alert(buffer, buffer_size, err_code, sdev);
}

static void test_slsi_on_firmware_memory_dump(struct kunit *test)
{
	char buffer[10] = "KUNIT_TEST";
	int buffer_size = 0;

	slsi_on_firmware_memory_dump(buffer, buffer_size, NULL);
}

static void test_slsi_on_driver_memory_dump(struct kunit *test)
{
	char buffer[10] = "KUNIT_TEST";
	int buffer_size = 0;

	slsi_on_driver_memory_dump(buffer, buffer_size, NULL);
}

static void test_slsi_enable_logging(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, slsi_enable_logging(NULL, false));
}

static void test_slsi_start_logging(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 100, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	int verbose_level = 1;
	int flag = 0x1234;
	int max_interval_sec = 500;
	int min_data_size = 10;

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_VERBOSE_LEVEL,
					     &verbose_level, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_RING_NAME,
					     "KUNIT_RING", 1, 12, &len);
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_RING_FLAGS,
					     &flag, 0, 0, &len);
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_LOG_MAX_INTERVAL,
					     &max_interval_sec, 0, 0, &len);
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_LOG_MIN_DATA_SIZE,
					     &min_data_size, 0, 0, &len);

	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);
	sdev->device_config.qos_info = -125;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_start_logging(wiphy, wdev, nla_attr, len));

	sdev->device_config.qos_info = -15;
	KUNIT_EXPECT_EQ(test, 0, slsi_start_logging(wiphy, wdev, nla_attr, len));

	nla_attr = kunit_nla_set_nested_attr(nla_attr, 100, &min_data_size, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_start_logging(wiphy, wdev, nla_attr, len));
}

static void test_slsi_reset_logging(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;

	KUNIT_EXPECT_EQ(test, 0, slsi_reset_logging(wiphy, 0, NULL, 0));
}

static void test_slsi_trigger_fw_mem_dump(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;

	sdev->device_config.qos_info = 981;
	KUNIT_EXPECT_EQ(test, -ENOMEM, slsi_trigger_fw_mem_dump(wiphy, 0, NULL, 0));

	sdev->device_config.qos_info = 1;
	KUNIT_EXPECT_EQ(test, 1, slsi_trigger_fw_mem_dump(wiphy, 0, NULL, 0));
}

static void test_slsi_get_fw_mem_dump(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 100, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	int buf_len = 5;
	char to_addr[5];
	u64 val = to_addr;
	char *buffer = "HELLO";

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_FW_DUMP_LEN, &buf_len, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_FW_DUMP_DATA, &val, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	if (!mem_dump_buffer) {
		mem_dump_buffer = kunit_kzalloc(test, strlen(buffer) - 1, GFP_KERNEL);
		mem_dump_buffer_size = strlen(buffer) - 1;
	}
	KUNIT_EXPECT_EQ(test, 1, slsi_get_fw_mem_dump(wiphy, wdev, nla_attr, len));

	sdev->device_config.qos_info = 981;
	if (!mem_dump_buffer) {
		mem_dump_buffer = kunit_kzalloc(test, strlen(buffer) - 1, GFP_KERNEL);
		mem_dump_buffer_size = strlen(buffer) - 1;
	}

	KUNIT_EXPECT_EQ(test, -ENOMEM, slsi_get_fw_mem_dump(wiphy, wdev, nla_attr, len));

	sdev->device_config.qos_info = 1;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, 100, &buf_len, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_fw_mem_dump(wiphy, wdev, nla_attr, len));
}

static void test_slsi_trigger_driver_mem_dump(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 100, GFP_KERNEL);
	int len = 0;

	sdev->device_config.qos_info = 981;
	KUNIT_EXPECT_EQ(test, -ENOMEM, slsi_trigger_driver_mem_dump(wiphy, wdev, nla_attr, len));

	sdev->device_config.qos_info = 1;
	KUNIT_EXPECT_EQ(test, 1, slsi_trigger_driver_mem_dump(wiphy, wdev, nla_attr, len));
}

static void test_slsi_get_driver_mem_dump(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 100, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	int buf_len = 5;
	char to_addr[5];
	u64 val = to_addr;
	char *buffer = "HELLO";

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_DRIVER_DUMP_LEN,
					     &buf_len, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_DRIVER_DUMP_DATA,
					     &val, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	if (!mem_dump_buffer) {
		mem_dump_buffer = kunit_kzalloc(test, strlen(buffer) - 1, GFP_KERNEL);
		mem_dump_buffer_size = strlen(buffer) - 1;
	}

	KUNIT_EXPECT_EQ(test, 1, slsi_get_driver_mem_dump(wiphy, wdev, nla_attr, len));

	sdev->device_config.qos_info = 981;
	if (!mem_dump_buffer) {
		mem_dump_buffer = kunit_kzalloc(test, strlen(buffer) - 1, GFP_KERNEL);
		mem_dump_buffer_size = strlen(buffer) - 1;
	}

	KUNIT_EXPECT_EQ(test, -ENOMEM, slsi_get_driver_mem_dump(wiphy, wdev, nla_attr, len));

	sdev->device_config.qos_info = 1;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, 100, &buf_len, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_driver_mem_dump(wiphy, wdev, nla_attr, len));
}

static void test_slsi_get_version(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 100, GFP_KERNEL);
	int len = 0;
	int init_len = 0;

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_DRIVER_VERSION,
					     NULL, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_FW_VERSION,
					     NULL, 0, 0, &len);

	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, 1, slsi_get_version(wiphy, wdev, nla_attr, len));
}

static void test_slsi_get_ring_buffers_status(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;

	sdev->device_config.qos_info = 981;
	KUNIT_EXPECT_EQ(test, -ENOMEM, slsi_get_ring_buffers_status(wiphy, NULL, 0, 0));
	kunit_free_skb();

	sdev->device_config.qos_info = 1;
	KUNIT_EXPECT_EQ(test, 1, slsi_get_ring_buffers_status(wiphy, NULL, 0, 0));
	kunit_free_skb();
}

static void test_slsi_get_ring_data(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 100, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	char *name = "KUNIT_NAME";

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_RING_NAME,
					     name, 1, 11, &len);
	init_len = len;

	KUNIT_EXPECT_EQ(test, 0, slsi_get_ring_data(wiphy, wdev, nla_attr, len));

	nla_attr = kunit_nla_set_nested_attr(nla_attr, 8, name, 1, 12, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, 0, slsi_get_ring_data(wiphy, wdev, nla_attr, len));
}

static void test_slsi_get_logger_supported_feature_set(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;

	KUNIT_EXPECT_EQ(test, 1, slsi_get_logger_supported_feature_set(wiphy, NULL, 0, 0));
}

static void test_slsi_get_tx_pkt_fates(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 100, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	int req_cnt = 2;
	char to_addr[2];
	u64 val = to_addr;

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_PKT_FATE_NUM,
					     &req_cnt, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_PKT_FATE_DATA,
					     &val, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);
	sdev->device_config.qos_info = 981;
	KUNIT_EXPECT_EQ(test, -ENOMEM, slsi_get_tx_pkt_fates(wiphy, wdev, nla_attr, len));
	kunit_free_skb();

	sdev->device_config.qos_info = 1;
	KUNIT_EXPECT_EQ(test, 1, slsi_get_tx_pkt_fates(wiphy, wdev, nla_attr, len));
	kunit_free_skb();

	req_cnt = 34;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_PKT_FATE_NUM,
					     &req_cnt, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_tx_pkt_fates(wiphy, wdev, nla_attr, len));
	kunit_free_skb();

	nla_attr = kunit_nla_set_nested_attr(nla_attr, 100, 0, 0, 0, &len);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_tx_pkt_fates(wiphy, wdev, nla_attr, len));
	kunit_free_skb();
}

static void test_slsi_get_rx_pkt_fates(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 100, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	int req_cnt = 2;
	char to_addr[2];
	u64 val = to_addr;

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_PKT_FATE_NUM,
					     &req_cnt, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_PKT_FATE_DATA,
					     &val, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);
	sdev->device_config.qos_info = 981;

	KUNIT_EXPECT_EQ(test, -ENOMEM, slsi_get_rx_pkt_fates(wiphy, wdev, nla_attr, len));

	sdev->device_config.qos_info = 1;

	KUNIT_EXPECT_EQ(test, 1, slsi_get_rx_pkt_fates(wiphy, wdev, nla_attr, len));

	req_cnt = 34;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_PKT_FATE_NUM,
					     &req_cnt, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_rx_pkt_fates(wiphy, wdev, nla_attr, len));

	nla_attr = kunit_nla_set_nested_attr(nla_attr, 100, &req_cnt, 0, 0, &len);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_rx_pkt_fates(wiphy, wdev, nla_attr, len));
}

static void test_slsi_wake_stats_attr_get_cnt_sz(struct kunit *test)
{
	struct slsi_wlan_driver_wake_reason_cnt *wake_reason_count;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 40, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	u32 CMD_CNT = 10;

	wake_reason_count = kunit_kzalloc(test, sizeof(struct slsi_wlan_driver_wake_reason_cnt), GFP_KERNEL);

	nla_attr = kunit_nla_set_nested_attr(nla_attr,
					     SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_CMD_EVENT_WAKE_CNT_SZ,
					     &CMD_CNT, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr,
					     SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_DRIVER_FW_LOCAL_WAKE_CNT_SZ,
					     &CMD_CNT, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, 0, slsi_wake_stats_attr_get_cnt_sz(wake_reason_count, nla_attr, len));

	nla_attr = kunit_nla_set_nested_attr(nla_attr, 111, &CMD_CNT, 0, 0, &len);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_wake_stats_attr_get_cnt_sz(wake_reason_count, nla_attr, len));
}

static void test_slsi_wake_stats_attr_put_skb_buffer(struct kunit *test)
{
#define PUT_SKB_BUFFER_SKB_SIZE 200
	struct sk_buff *skb = nlmsg_new(PUT_SKB_BUFFER_SKB_SIZE, GFP_KERNEL);
	struct slsi_nla_put put_rule[1] = {0};
	u32 put_rule_len = 1;
	u32 tmp_val = 10;
	u8 bin_data[] = "NLA_BINARY TEST DATA";

	put_rule[0].nla_type = NLA_U32;
	put_rule[0].attr = SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_TOTAL_CMD_EVENT_WAKE;
	put_rule[0].value_ptr = &tmp_val;

	KUNIT_EXPECT_EQ(test, 0, slsi_wake_stats_attr_put_skb_buffer(skb, put_rule, put_rule_len));

	put_rule[0].nla_type = NLA_BINARY;
	put_rule[0].attr = SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_CMD_EVENT_WAKE_CNT_PTR;
	put_rule[0].value_ptr = bin_data;
	put_rule[0].value_len = sizeof(bin_data);

	KUNIT_EXPECT_EQ(test, 0, slsi_wake_stats_attr_put_skb_buffer(skb, put_rule, put_rule_len));

	put_rule[0].nla_type = NLA_U16;
	put_rule[0].attr = SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_TOTAL_CMD_EVENT_WAKE;
	put_rule[0].value_ptr = &tmp_val;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_wake_stats_attr_put_skb_buffer(skb, put_rule, put_rule_len));
	kfree_skb(skb);
}

static void test_slsi_get_wake_reason_stats(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 40, GFP_KERNEL);
	int len;
	int CMD_CNT = 10;

	sdev->device_config.qos_info = 981;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ENHANCED_LOGGING_ATTRIBUTE_WAKE_STATS_CMD_EVENT_WAKE_CNT_SZ,
					     &CMD_CNT, 0, 0, &len);
	KUNIT_EXPECT_EQ(test, -ENOMEM, slsi_get_wake_reason_stats(wiphy, wdev, nla_attr, len));
	kunit_free_skb();

	sdev->device_config.qos_info = 1;
	KUNIT_EXPECT_EQ(test, 1, slsi_get_wake_reason_stats(wiphy, wdev, nla_attr, len));
	kunit_free_skb();

	nla_attr = kunit_nla_set_nested_attr(nla_attr, 111, &CMD_CNT, 0, 0, &len);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_wake_reason_stats(wiphy, wdev, nla_attr, len));
	kunit_free_skb();
}
#endif

static void test_slsi_acs_validate_width_hw_mode(struct kunit *test)
{
	struct slsi_acs_request *request = kunit_kzalloc(test, sizeof(struct slsi_acs_request), GFP_KERNEL);

	request->hw_mode = SLSI_ACS_MODE_IEEE80211ANY;
	request->ch_width = 160;

	KUNIT_EXPECT_EQ(test, 0, slsi_acs_validate_width_hw_mode(request));
}

static void test_slsi_acs_init(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 50, GFP_KERNEL);
	int len = 0;
	int init_len = 0;
	u8 hw_mode = SLSI_ACS_MODE_IEEE80211A;
	u16 ch_width = 20;
	u32			freq_list[6] = {2412, 2452, 2462, 2472, 5745, 5520};

	wdev->iftype = NL80211_IFTYPE_STATION;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_acs_init(wiphy, wdev, nla_attr, len));

	wdev->iftype = NL80211_IFTYPE_AP;

	wdev->netdev = NULL;
	KUNIT_EXPECT_EQ(test, -ENODEV, slsi_acs_init(wiphy, wdev, nla_attr, len));

	wdev->netdev = dev;

	len = 0;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ACS_ATTR_MAX + 1, &hw_mode, 0, 0, &len);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_acs_init(wiphy, wdev, nla_attr, len));

	wiphy->bands[NL80211_BAND_2GHZ] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band), GFP_KERNEL);
	wiphy->bands[NL80211_BAND_2GHZ]->n_channels = 2;
	wiphy->bands[NL80211_BAND_2GHZ]->channels = kunit_kzalloc(test, sizeof(struct ieee80211_channel) * 3, GFP_KERNEL);

	wiphy->bands[NL80211_BAND_2GHZ]->channels[0].hw_value = 1;
	wiphy->bands[NL80211_BAND_2GHZ]->channels[0].flags = 0;
	wiphy->bands[NL80211_BAND_2GHZ]->channels[0].band = NL80211_BAND_2GHZ;
	wiphy->bands[NL80211_BAND_2GHZ]->channels[0].center_freq = 2412;
	wiphy->bands[NL80211_BAND_2GHZ]->channels[0].freq_offset = 0;

	wiphy->bands[NL80211_BAND_2GHZ]->channels[1].hw_value = 9;
	wiphy->bands[NL80211_BAND_2GHZ]->channels[1].flags = IEEE80211_CHAN_INDOOR_ONLY;
	wiphy->bands[NL80211_BAND_2GHZ]->channels[1].band = NL80211_BAND_2GHZ;
	wiphy->bands[NL80211_BAND_2GHZ]->channels[1].center_freq = 2452;
	wiphy->bands[NL80211_BAND_2GHZ]->channels[1].freq_offset = 0;

	wiphy->bands[NL80211_BAND_5GHZ] = NULL;

	len = 0;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ACS_ATTR_HW_MODE, &hw_mode, 0, 0, &len);
	init_len = len;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ACS_ATTR_FREQ_LIST, &freq_list, 1, 16, &len);
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ACS_ATTR_CHWIDTH, &ch_width, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	KUNIT_EXPECT_EQ(test, 0, slsi_acs_init(wiphy, wdev, nla_attr, len));

	hw_mode = SLSI_ACS_MODE_IEEE80211G;
	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_ACS_ATTR_HW_MODE, &hw_mode, 0, 0, &len);
	nla_attr = (struct nlattr *)((char *)nla_attr - len + init_len);

	wiphy->bands[NL80211_BAND_2GHZ]->channels[1].hw_value = 9;
	wiphy->bands[NL80211_BAND_2GHZ]->channels[1].flags = 0;
	wiphy->bands[NL80211_BAND_2GHZ]->channels[1].band = NL80211_BAND_2GHZ;
	wiphy->bands[NL80211_BAND_2GHZ]->channels[1].center_freq = 2452;
	wiphy->bands[NL80211_BAND_2GHZ]->channels[1].freq_offset = 0;

	KUNIT_EXPECT_EQ(test, 1, slsi_acs_init(wiphy, wdev, nla_attr, len));
}

static void test_slsi_configure_latency_mode(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct nlattr *nla_attr = kunit_kzalloc(test, sizeof(struct nlattr) * 50, GFP_KERNEL);
	int len = 0;
	u8 val = 12;

	nla_attr = kunit_nla_set_nested_attr(nla_attr, SLSI_NL_ATTRIBUTE_LATENCY_MODE, &val, 0, 0, &len);
	wdev->netdev = NULL;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_configure_latency_mode(wiphy, wdev, nla_attr, len));

	wdev->netdev = dev;

	KUNIT_EXPECT_EQ(test, 0, slsi_configure_latency_mode(wiphy, wdev, nla_attr, len));

	nla_attr = kunit_nla_set_nested_attr(nla_attr, 111, &val, 0, 0, &len);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_configure_latency_mode(wiphy, wdev, nla_attr, len));
}

static void test_slsi_uc_add_ap_channels(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct slsi_usable_channel *buf;
	u32 mem_len = 0;
	u32 cnt = 0;
	u32 max_cnt = 1;

	wiphy->bands[NL80211_BAND_2GHZ] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band), GFP_KERNEL);
	wiphy->bands[NL80211_BAND_2GHZ]->channels = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	wiphy->bands[NL80211_BAND_2GHZ]->n_channels = 1;

	if (wiphy->bands[NL80211_BAND_2GHZ])
		mem_len += wiphy->bands[NL80211_BAND_2GHZ]->n_channels * sizeof(struct slsi_usable_channel);

	if (wiphy->bands[NL80211_BAND_5GHZ])
		mem_len += wiphy->bands[NL80211_BAND_5GHZ]->n_channels * sizeof(struct slsi_usable_channel);

#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	if (wiphy->bands[NL80211_BAND_6GHZ])
		mem_len += wiphy->bands[NL80211_BAND_6GHZ]->n_channels * sizeof(struct slsi_usable_channel);
#endif

	buf = kunit_kzalloc(test, mem_len, GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, max_cnt - cnt, slsi_uc_add_ap_channels(wiphy, NL80211_BAND_2GHZ, buf, cnt, max_cnt));
}

static void test_slsi_get_usable_channels(struct kunit *test)
{
}

static void test_slsi_set_dtim_config(struct kunit *test)
{
}

#ifdef CONFIG_SCSC_WLAN_SAR_SUPPORTED
static void test_slsi_select_tx_power_scenario(struct kunit *test)
{
}

static void  test_slsi_reset_tx_power_scenario(struct kunit *test)
{
}
#endif

static void test_slsi_vendor_delay_wakeup_event(struct kunit *test)
{
}

/*
 * Test fictures
 */
static int nl80211_vendor_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void nl80211_vendor_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
	return;
}

/*
 * KUnit testcase definitions
 */
static struct kunit_case nl80211_vendor_test_cases[] = {
	KUNIT_CASE(test_slsi_vendor_event),
	KUNIT_CASE(test_slsi_vendor_cmd_reply),
	KUNIT_CASE(test_slsi_gscan_get_vif),
	KUNIT_CASE(test_slsi_number_digits),
	KUNIT_CASE(test_slsi_gscan_get_capabilities),
	KUNIT_CASE(test_slsi_gscan_put_channels),
	KUNIT_CASE(test_slsi_gscan_get_valid_channel),
	KUNIT_CASE(test_slsi_prepare_scan_result),
	KUNIT_CASE(test_slsi_gscan_hash_add),
	KUNIT_CASE(test_slsi_nl80211_vendor_init_deinit),
	KUNIT_CASE(test_slsi_gscan_hash_get),
	KUNIT_CASE(test_slsi_gscan_hash_remove),
	KUNIT_CASE(test_slsi_check_scan_result),
	KUNIT_CASE(test_slsi_gscan_handle_scan_result),
	KUNIT_CASE(test_slsi_gscan_get_scan_policy),
	KUNIT_CASE(test_slsi_gscan_add_verify_params),
	KUNIT_CASE(test_slsi_gscan_add_to_list),
	KUNIT_CASE(test_slsi_gscan_alloc_buckets),
	KUNIT_CASE(test_slsi_gscan_free_buckets),
	KUNIT_CASE(test_slsi_gscan_flush_scan_results),
	KUNIT_CASE(test_slsi_gscan_add),
	KUNIT_CASE(test_slsi_gscan_del),
	KUNIT_CASE(test_slsi_gscan_get_scan_results),
	KUNIT_CASE(test_slsi_rx_rssi_report_ind),
	KUNIT_CASE(test_slsi_set_vendor_ie),
	KUNIT_CASE(test_slsi_key_mgmt_set_pmk),
	KUNIT_CASE(test_slsi_set_bssid_blacklist),
	KUNIT_CASE(test_slsi_start_keepalive_offload),
	KUNIT_CASE(test_slsi_stop_keepalive_offload),
	KUNIT_CASE(test_epno_hs_set_reset),
	KUNIT_CASE(test_slsi_set_rssi_monitor),
	KUNIT_CASE(test_slsi_lls_set_stats),
	KUNIT_CASE(test_slsi_lls_clear_stats),
	KUNIT_CASE(test_slsi_lls_get_stats),
	KUNIT_CASE(test_slsi_lls_get_stats),
	KUNIT_CASE(test_slsi_gscan_set_oui),
	KUNIT_CASE(test_slsi_get_feature_set),
	KUNIT_CASE(test_slsi_set_country_code),
	KUNIT_CASE(test_slsi_apf_read_filter),
	KUNIT_CASE(test_slsi_apf_get_capabilities),
	KUNIT_CASE(test_slsi_apf_set_filter),
	KUNIT_CASE(test_slsi_rtt_set_get),
	KUNIT_CASE(test_slsi_tx_rate_calc),
	KUNIT_CASE(test_slsi_rx_range_ind),
	KUNIT_CASE(test_slsi_rx_range_done_ind),
	KUNIT_CASE(test_slsi_rtt_remove_peer),
#if IS_ENABLED(CONFIG_IPV6)
	KUNIT_CASE(test_slsi_configure_nd_offload),
#endif
	KUNIT_CASE(test_slsi_get_roaming_capabilities),
	KUNIT_CASE(test_slsi_set_roaming_state),
	KUNIT_CASE(test_str_type_check),
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
	KUNIT_CASE(test_slsi_handle_nan_rx_event_log_ind),
#endif
	KUNIT_CASE(test_dump_roam_scan_result),
	KUNIT_CASE(test_slsi_get_roam_reason_from_expired_tv),
#ifdef CONFIG_SCSC_WLAN_ENHANCED_LOGGING
	KUNIT_CASE(test_slsi_on_ring_buffer_data),
	KUNIT_CASE(test_slsi_print_channel_list),
	KUNIT_CASE(test_slsi_on_alert),
	KUNIT_CASE(test_slsi_on_firmware_memory_dump),
	KUNIT_CASE(test_slsi_on_driver_memory_dump),
	KUNIT_CASE(test_slsi_enable_logging),
	KUNIT_CASE(test_slsi_start_logging),
	KUNIT_CASE(test_slsi_reset_logging),
	KUNIT_CASE(test_slsi_trigger_fw_mem_dump),
	KUNIT_CASE(test_slsi_get_fw_mem_dump),
	KUNIT_CASE(test_slsi_trigger_driver_mem_dump),
	KUNIT_CASE(test_slsi_get_driver_mem_dump),
	KUNIT_CASE(test_slsi_get_version),
	KUNIT_CASE(test_slsi_get_ring_buffers_status),
	KUNIT_CASE(test_slsi_get_ring_data),
	KUNIT_CASE(test_slsi_get_logger_supported_feature_set),
	KUNIT_CASE(test_slsi_get_tx_pkt_fates),
	KUNIT_CASE(test_slsi_get_rx_pkt_fates),
	KUNIT_CASE(test_slsi_wake_stats_attr_get_cnt_sz),
	KUNIT_CASE(test_slsi_wake_stats_attr_put_skb_buffer),
	KUNIT_CASE(test_slsi_get_wake_reason_stats),
#endif
	KUNIT_CASE(test_slsi_acs_validate_width_hw_mode),
	KUNIT_CASE(test_slsi_acs_init),
	KUNIT_CASE(test_slsi_configure_latency_mode),
	KUNIT_CASE(test_slsi_rx_event_log_indication),
	KUNIT_CASE(test_slsi_uc_add_ap_channels),
	KUNIT_CASE(test_slsi_get_usable_channels),
	KUNIT_CASE(test_slsi_set_dtim_config),
#ifdef CONFIG_SCSC_WLAN_SAR_SUPPORTED
	KUNIT_CASE(test_slsi_select_tx_power_scenario),
	KUNIT_CASE(test_slsi_reset_tx_power_scenario),
#endif
	KUNIT_CASE(test_slsi_vendor_delay_wakeup_event),
	{}
};

static struct kunit_suite nl80211_vendor_test_suite[] = {
	{
		.name = "kunit-nl80211_vendor-test",
		.test_cases = nl80211_vendor_test_cases,
		.init = nl80211_vendor_test_init,
		.exit = nl80211_vendor_test_exit,
	}};

kunit_test_suites(nl80211_vendor_test_suite);
