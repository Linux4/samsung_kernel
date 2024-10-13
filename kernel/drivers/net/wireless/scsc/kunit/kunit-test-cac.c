// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-mlme.h"
#include "../cac.c"

static void test_get_netdev_for_station(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_PTR_EQ(test, NULL, get_netdev_for_station(sdev));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	KUNIT_EXPECT_PTR_EQ(test, dev, (struct net_device *)get_netdev_for_station(sdev));
}

static void test_cac_create_tspec(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	tspec_list_next_id = 8;
	KUNIT_EXPECT_EQ(test, -1, cac_create_tspec(sdev, NULL));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->activated = 0;
	KUNIT_EXPECT_EQ(test, -1, cac_create_tspec(sdev, NULL));

	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;
	KUNIT_EXPECT_EQ(test, 0, cac_create_tspec(sdev, ""));
	KUNIT_EXPECT_EQ(test, 1, cac_create_tspec(sdev, ""));
	KUNIT_EXPECT_EQ(test, 2, cac_create_tspec(sdev, ""));
	KUNIT_EXPECT_EQ(test, 3, cac_create_tspec(sdev, ""));
	KUNIT_EXPECT_EQ(test, 4, cac_create_tspec(sdev, ""));
	KUNIT_EXPECT_EQ(test, 5, cac_create_tspec(sdev, ""));
	KUNIT_EXPECT_EQ(test, 6, cac_create_tspec(sdev, ""));
	KUNIT_EXPECT_EQ(test, 7, cac_create_tspec(sdev, ""));
	KUNIT_EXPECT_EQ(test, 0, cac_create_tspec(sdev, ""));
	KUNIT_EXPECT_EQ(test, -1, cac_create_tspec(sdev, "-1"));

	KUNIT_EXPECT_EQ(test, 1, cac_create_tspec(sdev, NULL));
	KUNIT_EXPECT_EQ(test, 2, cac_create_tspec(sdev, NULL));
	KUNIT_EXPECT_EQ(test, 3, cac_create_tspec(sdev, NULL));
	KUNIT_EXPECT_EQ(test, 4, cac_create_tspec(sdev, NULL));
	KUNIT_EXPECT_EQ(test, 5, cac_create_tspec(sdev, NULL));
	KUNIT_EXPECT_EQ(test, 6, cac_create_tspec(sdev, NULL));
	KUNIT_EXPECT_EQ(test, 7, cac_create_tspec(sdev, NULL));
	KUNIT_EXPECT_EQ(test, 0, cac_create_tspec(sdev, NULL));
	KUNIT_EXPECT_EQ(test, 1, cac_create_tspec(sdev, NULL));

	cac_delete_tspec(sdev, 0);
	cac_delete_tspec(sdev, 1);
	cac_delete_tspec(sdev, 2);
	cac_delete_tspec(sdev, 3);
	cac_delete_tspec(sdev, 4);
	cac_delete_tspec(sdev, 5);
	cac_delete_tspec(sdev, 6);
	cac_delete_tspec(sdev, 7);
	cac_delete_tspec(sdev, 0);
	cac_delete_tspec(sdev, 1);
	cac_delete_tspec(sdev, 2);
	cac_delete_tspec(sdev, 3);
	cac_delete_tspec(sdev, 4);
	cac_delete_tspec(sdev, 5);
	cac_delete_tspec(sdev, 6);
	cac_delete_tspec(sdev, 7);
	cac_delete_tspec(sdev, 0);
	cac_delete_tspec(sdev, 1);
}

static void test_cac_delete_tspec(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct cac_tspec *itr;
	int id;

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	id = cac_create_tspec(sdev, NULL);
	itr = find_tspec_entry(id, 0);
	itr->accepted = 1;

	KUNIT_EXPECT_EQ(test, 0, cac_delete_tspec(sdev_nodev, id));
	KUNIT_EXPECT_EQ(test, -1, cac_delete_tspec(sdev_nodev, id+1));
}

static void test_cac_query_tspec_field(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct cac_tspec *entry;
	const char *field_invalid = "invalid";
	const char *field_size1 = "traffic_type";
	const char *field_size2 = "nominal_msdu_size";
	const char *field_size3 = "min_service_interval";
	u32 value = 1;

	entry = kunit_kzalloc(test, sizeof(struct cac_tspec), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, -1, cac_query_tspec_field(sdev, entry, field_invalid, NULL));
	KUNIT_EXPECT_EQ(test, -1, cac_query_tspec_field(sdev, entry, field_invalid, &value));
	KUNIT_EXPECT_EQ(test, 0, cac_query_tspec_field(sdev, entry, field_size1, &value));
	KUNIT_EXPECT_EQ(test, 0, cac_query_tspec_field(sdev, entry, field_size2, &value));
	KUNIT_EXPECT_EQ(test, 0, cac_query_tspec_field(sdev, entry, field_size3, &value));
}

static void test_add_ebw_ie(struct kunit *test)
{
	u8 buf1[7] = {0};
	u8 buf2[9] = {0};

	KUNIT_EXPECT_EQ(test, -1, add_ebw_ie(buf1, sizeof(buf1), 0));
	KUNIT_EXPECT_EQ(test, 8, add_ebw_ie(buf2, sizeof(buf2), 0));
}

static void test_add_tsrs_ie(struct kunit *test)
{
	u8 buf[9] = {0};
	u8 rates[CCX_MAX_NUM_RATES] = {0,};
	size_t num_rates = 1;

	KUNIT_EXPECT_EQ(test, -1, add_tsrs_ie(NULL, 9, 0, rates, num_rates));
	KUNIT_EXPECT_EQ(test, 7 + num_rates, add_tsrs_ie(buf, 9, 0, rates, num_rates));
}

static void test_bss_get_ie(struct kunit *test)
{
	struct cfg80211_bss *bss;
	struct cfg80211_bss_ies *ies;

	bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies), GFP_KERNEL);
	ies->len = 1;
	bss->ies = ies;

	KUNIT_EXPECT_PTR_EQ(test, (u8 *)ies, (u8 *)bss_get_ie(bss, 0));
	KUNIT_EXPECT_PTR_EQ(test, NULL, (u8 *)bss_get_ie(bss, 1));
}

static void test_bss_get_bit_rates(struct kunit *test)
{
	struct cfg80211_bss *bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	struct cfg80211_bss_ies *ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies), GFP_KERNEL);
	u8 *rates;

	ies->len = 5;
	((u8 *)ies)[0] = WLAN_EID_SUPP_RATES;
	((u8 *)ies)[1] = 0;
	((u8 *)ies)[2] = WLAN_EID_EXT_SUPP_RATES;
	((u8 *)ies)[3] = 3;
	bss->ies = ies;

	KUNIT_EXPECT_EQ(test, 3, bss_get_bit_rates(bss, &rates));
	kfree(rates);
}

static void test_cac_send_addts(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct cfg80211_bss *bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	struct cfg80211_bss_ies *ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies), GFP_KERNEL);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	int id;
	u8 addr[ETH_ALEN];

	dev->dev_addr = addr;
	ndev_vif->activated = 1;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;

	KUNIT_EXPECT_EQ(test, -1, cac_send_addts(sdev, -1, 1));

	id = cac_create_tspec(sdev, NULL);
	sdev->netdev[SLSI_NET_INDEX_WLAN] = 0;
	KUNIT_EXPECT_EQ(test, -1, cac_send_addts(sdev, id, 1));

	sdev->netdev[SLSI_NET_INDEX_WLAN] = dev;
	ndev_vif->sta.sta_bss = NULL;
	KUNIT_EXPECT_EQ(test, -1, cac_send_addts(sdev, id, 1));

	ndev_vif->sta.sta_bss = bss;
	bss->ies = ies;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_UNSPECIFIED;
	KUNIT_EXPECT_EQ(test, (u8)(-1), cac_send_addts(sdev, id, 1));

	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, (u8)(-1), cac_send_addts(sdev, id, 1));

	ccx_status = BSS_CCX_ENABLED;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = peer;
	peer->valid = 1;
	peer->uapsd = 1;
	KUNIT_EXPECT_EQ(test, 0, cac_send_addts(sdev, id, 1));

	ies->len = 5;
	((u8 *)ies)[0] = WLAN_EID_SUPP_RATES;
	((u8 *)ies)[1] = 0;
	((u8 *)ies)[2] = WLAN_EID_EXT_SUPP_RATES;
	((u8 *)ies)[3] = 3;
	((u8 *)ies)[4] = 10;
	((u8 *)ies)[5] = 10;
	((u8 *)ies)[6] = 10;
	sdev->device_config.qos_info = -267;
	KUNIT_EXPECT_EQ(test, (u8)(-1), cac_send_addts(sdev, id, 1));

	sdev->device_config.qos_info = 267;
	KUNIT_EXPECT_EQ(test, 0, cac_send_addts(sdev, id, 1));
	ccx_status = BSS_CCX_DISABLED;
	cac_delete_tspec(sdev_nodev, id);
}

static void test_cac_send_delts(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct cfg80211_bss_ies *ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies), GFP_KERNEL);
	struct cfg80211_bss *bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	struct sk_buff *cfm = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u8 addr[ETH_ALEN];
	int id;
	struct cac_tspec *itr;

	dev->dev_addr = addr;
	KUNIT_EXPECT_EQ(test, -1, cac_send_delts(sdev, 3));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	id = cac_create_tspec(sdev, NULL);
	itr = find_tspec_entry(id, 0);
	itr->accepted = 1;
	sdev->netdev[SLSI_NET_INDEX_WLAN] = NULL;
	KUNIT_EXPECT_EQ(test, -1, cac_send_delts(sdev, id));

	sdev->netdev[SLSI_NET_INDEX_WLAN] = dev;
	KUNIT_EXPECT_EQ(test, -1, cac_send_delts(sdev, id));

	ndev_vif->sta.sta_bss = bss;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_UNSPECIFIED;
	KUNIT_EXPECT_EQ(test, (u8)(-1), cac_send_delts(sdev, id));

	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, (u8)(-1), cac_send_delts(sdev, id));

	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = peer;
	peer->valid = 1;
	sdev->device_config.qos_info = -267;
	KUNIT_EXPECT_EQ(test, (u8)(-1), cac_send_delts(sdev, id));

	sdev->device_config.qos_info = 267;
	sdev->sig_wait.cfm = NULL;
	KUNIT_EXPECT_EQ(test, (u8)(-1), cac_send_delts(sdev, id));

	ccx_status = BSS_CCX_ENABLED;
	previous_msdu_lifetime = BSS_CCX_ENABLED;

	itr->accepted = 1;
	sdev->sig_wait.cfm = cfm;
	KUNIT_EXPECT_EQ(test, 0, cac_send_delts(sdev, id));

	ccx_status = BSS_CCX_DISABLED;
	previous_msdu_lifetime = MAX_TRANSMIT_MSDU_LIFETIME_NOT_VALID;
	cac_delete_tspec(sdev_nodev, id);
}

static void test_cac_delete_tspec_by_state(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	int id1;
	int id2;
	int accepted;

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	id1 = cac_create_tspec(sdev, NULL);
	id2 = cac_create_tspec(sdev, NULL);

	KUNIT_EXPECT_EQ(test, -1, cac_delete_tspec_by_state(sdev, 5000, 0));
	KUNIT_EXPECT_EQ(test, 0, cac_delete_tspec_by_state(sdev_nodev, id1, 0));
	KUNIT_EXPECT_EQ(test, 0, cac_delete_tspec_by_state(sdev_nodev, id2, 0));
}

static void test_cac_config_tspec(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	const char *field_invalid = "invalid";
	const char *field_info_psb = "psb";
	const char *field_info_field = "traffic_type";
	const char *field_non_info_field_size2 = "nominal_msdu_size";
	const char *field_non_info_field_size4 = "min_service_interval";
	int id = 0;
	u32 value = 0;

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	KUNIT_EXPECT_EQ(test, -1, cac_config_tspec(sdev, id, NULL, value));
	KUNIT_EXPECT_EQ(test, -1, cac_config_tspec(sdev, -1, field_invalid, 0));

	id = cac_create_tspec(sdev, NULL);

	KUNIT_EXPECT_EQ(test, -1, cac_config_tspec(sdev, id, field_invalid, 0));
	KUNIT_EXPECT_EQ(test, 0, cac_config_tspec(sdev, id, field_info_psb, 0));
	KUNIT_EXPECT_EQ(test, 0, cac_config_tspec(sdev, id, field_info_psb, 5));
	KUNIT_EXPECT_EQ(test, -1, cac_config_tspec(sdev, id, field_info_field, 2));
	KUNIT_EXPECT_EQ(test, -1, cac_config_tspec(sdev, id, field_non_info_field_size2, 1<<(17)));
	KUNIT_EXPECT_EQ(test, 0, cac_config_tspec(sdev, id, field_non_info_field_size2, 0));
	KUNIT_EXPECT_EQ(test, 0, cac_config_tspec(sdev, id, field_non_info_field_size4, 0));

	cac_delete_tspec(sdev_nodev, id);
}

static void test_cac_ctrl_create_tspec(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	int id;

	KUNIT_EXPECT_EQ(test, -1, cac_ctrl_create_tspec(sdev, NULL));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	KUNIT_EXPECT_NE(test, -1, id = cac_ctrl_create_tspec(sdev, NULL));

	cac_delete_tspec(sdev_nodev, id);
}

static void test_cac_ctrl_delete_tspec(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	char *args = "2";
	char *args_minus = "-1";
	int id;

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	KUNIT_EXPECT_EQ(test, -1, cac_ctrl_delete_tspec(sdev, args_minus));
	KUNIT_EXPECT_EQ(test, -1, cac_ctrl_delete_tspec(sdev, args));

	id = cac_create_tspec(sdev, args);

	KUNIT_EXPECT_EQ(test, 0, cac_ctrl_delete_tspec(sdev, args));
}

static void test_cac_ctrl_config_tspec(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	char *args_empty = "";
	char *args_no_field = " ";
	char *args_id_minus = "-1 traffic_type 0";
	char *args_val_minus = "0 traffic_type -1";
	char *args_ok = "0 traffic_type 0";
	char *args = "0";
	int id;

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	id = cac_create_tspec(sdev, args);

	KUNIT_EXPECT_EQ(test, -1, cac_ctrl_config_tspec(sdev, args_empty));
	KUNIT_EXPECT_EQ(test, -1, cac_ctrl_config_tspec(sdev, args_no_field));
	KUNIT_EXPECT_EQ(test, -1, cac_ctrl_config_tspec(sdev, args_id_minus));
	KUNIT_EXPECT_EQ(test, -1, cac_ctrl_config_tspec(sdev, args_val_minus));
	KUNIT_EXPECT_EQ(test, 0, cac_ctrl_config_tspec(sdev, args_ok));

	cac_delete_tspec(sdev_nodev, id);
}

static void test_cac_ctrl_send_addts(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct cfg80211_bss *bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	struct cfg80211_bss_ies *ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies), GFP_KERNEL);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	int id;
	const char *args = "0";
	const char *args_id_minus = "-1 ebw 0";
	const char *args_ok = "0 ebw 0";
	u8 addr[ETH_ALEN];

	dev->dev_addr = addr;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	id = cac_create_tspec(sdev, args);

	KUNIT_EXPECT_EQ(test, -1, cac_ctrl_send_addts(sdev, args_id_minus));

	ndev_vif->sta.sta_bss = bss;
	bss->ies = ies;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = peer;
	peer->valid = 1;
	peer->uapsd = 1;
	KUNIT_EXPECT_EQ(test, 0, cac_ctrl_send_addts(sdev, args_ok));

	cac_delete_tspec(sdev_nodev, id);
}

static void test_cac_ctrl_send_delts(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct cfg80211_bss *bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	struct cfg80211_bss_ies *ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies), GFP_KERNEL);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	struct cac_tspec *itr;
	int id;
	u8 addr[ETH_ALEN];
	const char *args = "0";
	const char *args_minus = "-1";

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	id = cac_create_tspec(sdev, args);
	itr = find_tspec_entry(id, 0);
	itr->accepted = 1;

	KUNIT_EXPECT_EQ(test, -1, cac_ctrl_send_delts(sdev, args_minus));

	dev->dev_addr = addr;
	ndev_vif->sta.sta_bss = bss;
	ndev_vif->sta.sta_bss->ies = ies;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = peer;
	peer->valid = 1;
	sdev->device_config.qos_info = 267;

	KUNIT_EXPECT_EQ(test, 0, cac_ctrl_send_delts(sdev, args));

	cac_delete_tspec(sdev_nodev, id);
}

static void test_cac_process_delts_req(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct cfg80211_bss *bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	struct cfg80211_bss_ies *ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies), GFP_KERNEL);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	struct sk_buff *cfm = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct action_delts_req *req = kunit_kzalloc(test, sizeof(struct action_delts_req), GFP_KERNEL);
	struct cac_tspec *itr;
	int id;
	const char *args = "0";
	u8 addr[ETH_ALEN];

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	ndev_vif->sta.sta_bss = NULL;
	cac_process_delts_req(sdev, dev, req);

	ndev_vif->sta.sta_bss = bss;
	ndev_vif->sta.sta_bss->ies = ies;
	cac_process_delts_req(sdev, dev, req);

	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = peer;
	peer->valid = 1;
	CAC_PUT_LE24(req->tspec.ts_info, 0);
	cac_process_delts_req(sdev, dev, req);

	id = cac_create_tspec(sdev, args);
	itr = find_tspec_entry(id, 0);
	itr->accepted = 1;
	cac_process_delts_req(sdev, dev, req);

	sdev->sig_wait.cfm = cfm;
	cac_process_delts_req(sdev, dev, req);

	cac_delete_tspec(sdev_nodev, id);
}

static void test_cac_find_edca_ie(struct kunit *test)
{
	const u8 *ie = "find_edca_ie";
	size_t ie_len = 9;
	u8 tsid;
	u16 lifetime;

	KUNIT_EXPECT_EQ(test, -1, cac_find_edca_ie(ie, ie_len, &tsid, NULL));
	KUNIT_EXPECT_EQ(test, 0, cac_find_edca_ie(ie, ie_len, &tsid, &lifetime));
}

static void set_tspec(struct slsi_dev *sdev, struct slsi_peer *peer,
			struct cac_tspec *itr, int ts_info, int establish, bool create)
{
	if (create) {
		int id = cac_create_tspec(sdev, NULL);

		itr = find_tspec_entry(id, 0);
	}
	CAC_PUT_LE24(itr->tspec.ts_info, ts_info << 11);
	peer->tspec_established = 1 << establish;
}

static void test_cac_process_addts_rsp(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct cfg80211_bss *bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	struct cfg80211_bss_ies *ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies), GFP_KERNEL);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	struct sk_buff *cfm = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct action_delts_req *req = kunit_kzalloc(test, sizeof(struct action_delts_req), GFP_KERNEL);
	struct ieee80211_mgmt *data = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt), GFP_KERNEL);
	struct action_addts_rsp *rsp = (struct action_addts_rsp *)&data->u.action;
	struct cac_tspec *itr;
	int id;
	const char *args = "0";
	u8 addr[ETH_ALEN];
	u8 *ie = "12345";
	int ie_len = 0;

	sdev->wlan_service_on = 1;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	id = cac_create_tspec(sdev, NULL);
	itr = find_tspec_entry(id, 0);
	itr->dialog_token = 0;

	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);

	ndev_vif->sta.sta_bss = bss;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);

	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = peer;
	peer->valid = 1;
	rsp->hdr.dialog_token = 1;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);

	rsp->hdr.dialog_token = 0;
	rsp->hdr.status_code = ADDTS_STATUS_INVALID_PARAM;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);

	rsp->hdr.status_code = ADDTS_STATUS_ACCEPTED;
	id = cac_create_tspec(sdev, NULL);
	itr = find_tspec_entry(id, 0);
	itr->dialog_token = 0;

	CAC_PUT_LE24(itr->tspec.ts_info, FAPI_PRIORITY_QOS_UP1 << 11);
	peer->tspec_established = 1 << FAPI_PRIORITY_QOS_UP1;
	itr->accepted = 0;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);

	CAC_PUT_LE24(itr->tspec.ts_info, FAPI_PRIORITY_QOS_UP2 << 11);
	peer->tspec_established = 1 << FAPI_PRIORITY_QOS_UP2;
	itr->accepted = 0;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);

	peer->tspec_established = 1 << FAPI_PRIORITY_QOS_UP3;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);


	CAC_PUT_LE24(itr->tspec.ts_info, FAPI_PRIORITY_QOS_UP0 << 11);
	peer->tspec_established = 1;
	itr->accepted = 0;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);

	CAC_PUT_LE24(itr->tspec.ts_info, FAPI_PRIORITY_QOS_UP3 << 11);
	peer->tspec_established = 1 << FAPI_PRIORITY_QOS_UP3;
	itr->accepted = 0;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);

	peer->tspec_established = 1 << FAPI_PRIORITY_QOS_UP1;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);


	CAC_PUT_LE24(itr->tspec.ts_info, FAPI_PRIORITY_QOS_UP4 << 11);
	peer->tspec_established = 1 << FAPI_PRIORITY_QOS_UP4;
	itr->accepted = 0;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);

	CAC_PUT_LE24(itr->tspec.ts_info, FAPI_PRIORITY_QOS_UP5 << 11);
	peer->tspec_established = 1 << FAPI_PRIORITY_QOS_UP5;
	itr->accepted = 0;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);

	peer->tspec_established = 1 << FAPI_PRIORITY_QOS_UP1;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);


	CAC_PUT_LE24(itr->tspec.ts_info, FAPI_PRIORITY_QOS_UP6 << 11);
	peer->tspec_established = 1 << FAPI_PRIORITY_QOS_UP6;
	itr->accepted = 0;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);

	CAC_PUT_LE24(itr->tspec.ts_info, FAPI_PRIORITY_QOS_UP7 << 11);
	peer->tspec_established = 1 << FAPI_PRIORITY_QOS_UP7;
	itr->accepted = 0;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);

	peer->tspec_established = 1 << FAPI_PRIORITY_QOS_UP1;
	itr->accepted = 0;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);

	CAC_PUT_LE24(itr->tspec.ts_info, FAPI_PRIORITY_QOS_UP7 << 11);
	peer->tspec_established = 1 << FAPI_PRIORITY_QOS_UP7;
	itr->accepted = 1;
	cac_process_addts_rsp(sdev, dev, rsp, ie, ie_len);

	cac_delete_tspec(sdev_nodev, id);
}

static void test_cac_rx_wmm_action(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct ieee80211_mgmt *data = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt), GFP_KERNEL);
	size_t len = 1;

	data->u.action.u.wme_action.action_code = WMM_ACTION_CODE_ADDTS_RESP;
	cac_rx_wmm_action(sdev, dev, data, len);

	data->u.action.u.wme_action.action_code = WMM_ACTION_CODE_DELTS;
	cac_rx_wmm_action(sdev, dev, data, len);
}

static void test_cac_get_active_tspecs(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct cac_activated_tspec *tspecs = kunit_kzalloc(test, sizeof(struct cac_activated_tspec), GFP_KERNEL);
	struct cac_tspec *itr;
	int id;

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	id = cac_create_tspec(sdev, NULL);
	itr = find_tspec_entry(id, 0);
	itr->accepted = 1;

	cac_get_active_tspecs(&tspecs);

	kfree(tspecs);
	cac_delete_tspec(sdev_nodev, id);
}

static void test_cac_delete_tspec_list(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	cac_create_tspec(sdev, NULL);

	cac_delete_tspec_list(sdev);
}

static void test_cac_deactivate_tspecs(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	int id;

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	id = cac_create_tspec(sdev, NULL);

	cac_deactivate_tspecs(sdev);

	cac_delete_tspec(sdev_nodev, id);
}

static void test_cac_get_rde_tspec_ie(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u8 assoc_rsp_ie[] = {1, 10, 5, 10, 0, 0, WMM_OUI_SUBTYPE_TSPEC_ELEMENT};
	int assoc_rsp_ie_len = 5;
	const u8 *tspec_ie_arr[10];

	CAC_PUT_LE16(&assoc_rsp_ie[4], 0);
	KUNIT_EXPECT_EQ(test, 1, cac_get_rde_tspec_ie(sdev, assoc_rsp_ie, assoc_rsp_ie_len, tspec_ie_arr));
}

static void test_cac_update_roam_traffic_params(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_dev *sdev_nodev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct cfg80211_bss *bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	struct wmm_tspec_element assoc_rsp_tspec;
	u8 *assoc_rsp_ie = &assoc_rsp_tspec;
	const char *args = "0";
	int id;

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->activated = 1;

	cac_update_roam_traffic_params(sdev, dev);

	ndev_vif->sta.sta_bss = bss;
	ndev_vif->peer_sta_record[0] = peer;
	peer->valid = 1;

	id = cac_create_tspec(sdev, args);

	peer->assoc_resp_ie = NULL;
	cac_update_roam_traffic_params(sdev, dev);

	peer->assoc_resp_ie = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	peer->assoc_resp_ie->data = assoc_rsp_ie;
	peer->assoc_resp_ie->len = 5;
	CAC_PUT_LE16(&assoc_rsp_ie[4], 0);
	assoc_rsp_ie[0] = 1;
	assoc_rsp_ie[1] = 10;
	assoc_rsp_ie[2] = 5;
	assoc_rsp_ie[3] = 10;
	assoc_rsp_ie[4] = 0;
	assoc_rsp_ie[5] = 0;
	assoc_rsp_ie[6] = WMM_OUI_SUBTYPE_TSPEC_ELEMENT;
	assoc_rsp_tspec.ts_info[0] = 0;

	cac_update_roam_traffic_params(sdev, dev);
	cac_delete_tspec(sdev_nodev, id);
}

static int cac_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void cac_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case cac_test_cases[] = {
	KUNIT_CASE(test_get_netdev_for_station),
	KUNIT_CASE(test_cac_create_tspec),
	KUNIT_CASE(test_cac_delete_tspec),
	KUNIT_CASE(test_cac_query_tspec_field),
	KUNIT_CASE(test_add_ebw_ie),
	KUNIT_CASE(test_add_tsrs_ie),
	KUNIT_CASE(test_bss_get_ie),
	KUNIT_CASE(test_bss_get_bit_rates),
	KUNIT_CASE(test_cac_send_addts),
	KUNIT_CASE(test_cac_send_delts),
	KUNIT_CASE(test_cac_delete_tspec_by_state),
	KUNIT_CASE(test_cac_config_tspec),
	KUNIT_CASE(test_cac_ctrl_create_tspec),
	KUNIT_CASE(test_cac_ctrl_delete_tspec),
	KUNIT_CASE(test_cac_ctrl_config_tspec),
	KUNIT_CASE(test_cac_ctrl_send_addts),
	KUNIT_CASE(test_cac_ctrl_send_delts),
	KUNIT_CASE(test_cac_process_delts_req),
	KUNIT_CASE(test_cac_find_edca_ie),
	KUNIT_CASE(test_cac_process_addts_rsp),
	KUNIT_CASE(test_cac_rx_wmm_action),
	KUNIT_CASE(test_cac_get_active_tspecs),
	KUNIT_CASE(test_cac_delete_tspec_list),
	KUNIT_CASE(test_cac_deactivate_tspecs),
	KUNIT_CASE(test_cac_get_rde_tspec_ie),
	KUNIT_CASE(test_cac_update_roam_traffic_params),
	{}
};

static struct kunit_suite cac_test_suite[] = {
	{
		.name = "kunit-cac-test",
		.test_cases = cac_test_cases,
		.init = cac_test_init,
		.exit = cac_test_exit,
	}
};

kunit_test_suites(cac_test_suite);
