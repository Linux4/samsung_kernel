#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-rx.h"
#include "kunit-mock-mlme.h"
#include "kunit-mock-netif.h"
#include "kunit-mock-ba_replay.h"
#include "kunit-mock-log2us.h"
#include "kunit-mock-scsc_wifilogger_ring_connectivity.h"
#include "kunit-mock-cm_if.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-dev.h"
#include "kunit-mock-debug.h"
#include "kunit-mock-mib.h"
#include "kunit-mock-tdls_manager.h"
#include "kunit-mock-ba.h"
#include "kunit-mock-nl80211_vendor.h"
#include "../cfg80211_ops.c"

#define SLSI_DEFAULT_HW_MAC_ADDR	"\x00\x00\x0F\x11\x22\x33"
#define SLSI_DEFAULT_BSSID		"\x01\x02\xDF\x1E\xB2\x39"
#define SLSI_kUNIT_WLAN_SSID		"\x50\x65\x61\x6b\x54\x68\x70\x75\x74\x73\x5f\x43\x68\x33\x37\x5f\x4d\x6f\x64\x65\x61\x78\x5f\x43\x77\x31\x36\x30"
#define SLSI_KUNIT_P2P_SSID		"\x44\x49\x52\x45\x43\x54\x2d\x43\x48"

static void slsi_get_key_cb(void *cookie, struct key_params *param) {}

static void kunit_slsi_p2p_unsync_vif_delete_work(struct work_struct *work) {}

static void kunit_slsi_p2p_unset_channel_expiry_work(struct work_struct *work) {}

static void kunit_slsi_p2p_roc_duration_expiry_work(struct work_struct *work) {}

static void kunit_slsi_wq_init(struct netdev_vif *ndev_vif, struct kunit *test)
{
	INIT_DELAYED_WORK(&ndev_vif->unsync.del_vif_work, kunit_slsi_p2p_unsync_vif_delete_work);
	INIT_DELAYED_WORK(&ndev_vif->unsync.roc_expiry_work, kunit_slsi_p2p_roc_duration_expiry_work);
	INIT_DELAYED_WORK(&ndev_vif->unsync.unset_channel_expiry_work, kunit_slsi_p2p_unset_channel_expiry_work);
}

static void kunit_slsi_p2p_deinit(struct netdev_vif *ndev_vif)
{
	/* Work should have been cleaned up by now */
	cancel_delayed_work(&ndev_vif->unsync.del_vif_work);
	cancel_delayed_work(&ndev_vif->unsync.roc_expiry_work);cancel_delayed_work(&ndev_vif->unsync.unset_channel_expiry_work);
}

/* TestCase */
static void test_slsi_add_virtual_intf(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct netdev_vif *apndev_vif;
	struct vif_params *params = kunit_kzalloc(test, sizeof(struct vif_params), GFP_KERNEL);
	char *name = "wlan0";
	u8 mac[ETH_ALEN] = {0x0C, 0x0C, 0x1C, 0xAA, 0xB1, 0x1F};

	params->use_4addr = 32;
	SLSI_MUTEX_INIT(sdev->netdev_add_remove_mutex);
	SLSI_MUTEX_INIT(sdev->start_stop_mutex);
	mutex_init(&sdev->netdev_remove_mutex);

	test_netdev_init(test, sdev, SLSI_NET_INDEX_P2PX_SWLAN);

	sdev->netdev_ap = sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN];
	apndev_vif = netdev_priv(sdev->netdev_ap);
	SLSI_MUTEX_INIT(apndev_vif->vif_mutex);
	apndev_vif->activated = true;

	memcpy(params->macaddr, mac, 6);
	sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN]->dev_addr = mac;
	KUNIT_EXPECT_PTR_NE(test,
			    NULL,
			    slsi_add_virtual_intf(wiphy, name, '0', SLSI_NET_INDEX_P2PX_SWLAN, params));

	sdev->netdev_up_count = 2;
	sdev->netdev_ap = sdev->netdev[SLSI_NET_INDEX_WLAN];
	apndev_vif->activated = false;
	sdev->netdev[SLSI_NET_INDEX_WLAN]->ieee80211_ptr = kunit_kzalloc(test, sizeof(struct wireless_dev), GFP_KERNEL);
	KUNIT_EXPECT_PTR_NE(test,
			    NULL,
			    slsi_add_virtual_intf(wiphy, name, '0', SLSI_NET_INDEX_P2PX_SWLAN, params));

	KUNIT_EXPECT_PTR_EQ(test,
			    ERR_PTR(-ENODEV),
			    slsi_add_virtual_intf(wiphy, name, '0', SLSI_NET_INDEX_AP2, params));
}

static void test_slsi_del_virtual_intf(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = kunit_kzalloc(test, sizeof(struct wireless_dev), GFP_KERNEL);
	char *name = "wlan0";
	struct vif_params *params = kunit_kzalloc(test, sizeof(struct vif_params), GFP_KERNEL);
	u8 mac[ETH_ALEN] = {0x0C, 0x0C, 0x1C, 0xAA, 0xB1, 0x1F};

	SLSI_MUTEX_INIT(sdev->netdev_add_remove_mutex);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_del_virtual_intf(wiphy, wdev));

	test_netdev_init(test, sdev, SLSI_NET_INDEX_P2PX_SWLAN);
	sdev->netdev_ap = sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN];
	memcpy(sdev->netdev_ap->name, name, 5);
	wdev->netdev = sdev->netdev_ap;
	KUNIT_EXPECT_EQ(test, 0, slsi_del_virtual_intf(wiphy, wdev));

	test_netdev_init(test, sdev, SLSI_NET_INDEX_P2PX_SWLAN);
	sdev->netdev_p2p = sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN];
	memcpy(sdev->netdev_p2p->name, name, 5);
	wdev->netdev = sdev->netdev_p2p;
	KUNIT_EXPECT_EQ(test, 0, slsi_del_virtual_intf(wiphy, wdev));
}

static void test_slsi_change_virtual_intf(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	struct netdev_vif *ndev_vif;
	struct vif_params *params = kunit_kzalloc(test, sizeof(struct vif_params), GFP_KERNEL);
	struct net_device *ndev;
	u32 *flags = "0x123456";

	test_netdev_init(test, sdev, SLSI_NET_INDEX_P2PX_SWLAN);
	ndev = sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN];
	ndev_vif = netdev_priv(ndev);
	ndev_vif->iftype = SLSI_NET_INDEX_P2PX_SWLAN;
	ndev_vif->activated = true;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_change_virtual_intf(wiphy, ndev, NL80211_IFTYPE_P2P_CLIENT, params));
#else
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_change_virtual_intf(wiphy, ndev, NL80211_IFTYPE_P2P_CLIENT, flags, params));
#endif

	params->use_4addr = 32;
	memcpy(params->macaddr, SLSI_DEFAULT_HW_MAC_ADDR, 6);
	ndev_vif->activated = false;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
	KUNIT_EXPECT_EQ(test, 0, slsi_change_virtual_intf(wiphy, ndev, NL80211_IFTYPE_P2P_CLIENT, params));
#else
	KUNIT_EXPECT_EQ(test, 0, slsi_change_virtual_intf(wiphy, ndev, NL80211_IFTYPE_P2P_CLIENT, flags, params));
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_change_virtual_intf(wiphy, ndev, NL80211_IFTYPE_NAN, params));
#else
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_change_virtual_intf(wiphy, ndev, NL80211_IFTYPE_NAN, flags, params));
#endif
}

static void test_slsi_add_key(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct key_params *params = kunit_kzalloc(test, sizeof(struct key_params), GFP_KERNEL);
	bool pairwise = true;
	const u8 *mac_addr = SLSI_DEFAULT_HW_MAC_ADDR;
	u8 key_index = 0;
	u8 tmp_key[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_add_key(wiphy, ndev, 0, key_index, pairwise, NULL, params));
#elif
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_add_key(wiphy, ndev, key_index, pairwise, NULL, params));
#endif

	ndev_vif->activated = false;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, 0, slsi_add_key(wiphy, ndev, 0, key_index, pairwise, mac_addr, params));
#elif
	KUNIT_EXPECT_EQ(test, 0, slsi_add_key(wiphy, ndev, key_index, pairwise, mac_addr, params));
#endif

	ndev_vif->activated = true;
	key_index = 2;
	params->cipher = WLAN_CIPHER_SUITE_PMK;
	params->key_len = 16;
	params->key = tmp_key;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, 0, slsi_add_key(wiphy, ndev, 0, key_index, pairwise, mac_addr, params));
#elif
	KUNIT_EXPECT_EQ(test, 0, slsi_add_key(wiphy, ndev, key_index, pairwise, mac_addr, params));
#endif

	params->cipher = WLAN_CIPHER_SUITE_BIP_GMAC_256;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.tdls_enabled = true;
	ndev_vif->sta.fils_connection = true;
	ndev_vif->peer_sta_record[SLSI_PEER_INDEX_MIN] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_PEER_INDEX_MIN]->valid = true;
	memcpy(ndev_vif->peer_sta_record[SLSI_PEER_INDEX_MIN]->address, SLSI_DEFAULT_HW_MAC_ADDR, ETH_ALEN);
	key_index = 7;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_add_key(wiphy, ndev, 0, key_index, pairwise, mac_addr, params));
#elif
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_add_key(wiphy, ndev, key_index, pairwise, mac_addr, params));
#endif

	pairwise = false;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_add_key(wiphy, ndev, 0, key_index, pairwise, mac_addr, params));
#elif
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_add_key(wiphy, ndev, key_index, pairwise, mac_addr, params));
#endif

	ndev_vif->sta.gratuitous_arp_needed = true;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_add_key(wiphy, ndev, 0, key_index, pairwise, mac_addr, params));
#elif
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_add_key(wiphy, ndev, key_index, pairwise, mac_addr, params));
#endif

	params->cipher = WLAN_CIPHER_SUITE_WEP104;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = true;
	memcpy(ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->address, SLSI_DEFAULT_HW_MAC_ADDR, ETH_ALEN);
	key_index = 2;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, 0, slsi_add_key(wiphy, ndev, 0, key_index, pairwise, mac_addr, params));
#elif
	KUNIT_EXPECT_EQ(test, 0, slsi_add_key(wiphy, ndev, key_index, pairwise, mac_addr, params));
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, 0, slsi_add_key(wiphy, ndev, 0, key_index, pairwise, mac_addr, params));
#elif
	KUNIT_EXPECT_EQ(test, 0, slsi_add_key(wiphy, ndev, key_index, pairwise, mac_addr, params));
#endif

	pairwise = true;
	params->cipher = WLAN_CIPHER_SUITE_SMS4;
	key_index = 2;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, 0, slsi_add_key(wiphy, ndev, 0, key_index, pairwise, mac_addr, params));
#elif
	KUNIT_EXPECT_EQ(test, 0, slsi_add_key(wiphy, ndev, key_index, pairwise, mac_addr, params));
#endif

	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_GO;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, 0, slsi_add_key(wiphy, ndev, 0, key_index, pairwise, mac_addr, params));
#elif
	KUNIT_EXPECT_EQ(test, 0, slsi_add_key(wiphy, ndev, key_index, pairwise, mac_addr, params));
#endif

	pairwise = false;
	params->cipher = WLAN_CIPHER_SUITE_SMS4;
	ndev->dev_addr = SLSI_DEFAULT_HW_MAC_ADDR;
	key_index = 8;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, 0, slsi_add_key(wiphy, ndev, 0, key_index, pairwise, mac_addr, params));
#elif
	KUNIT_EXPECT_EQ(test, 0, slsi_add_key(wiphy, ndev, key_index, pairwise, mac_addr, params));
#endif

	key_index = 2;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, 0, slsi_del_key(wiphy, ndev, 0, key_index, pairwise, mac_addr));
#elif
	KUNIT_EXPECT_EQ(test, 0, slsi_del_key(wiphy, ndev, key_index, pairwise, mac_addr));
#endif
}

static void test_slsi_channel_switch(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct net_device *ap_dev;
	struct netdev_vif *ap_dev_vif;
	struct cfg80211_csa_settings *params = kunit_kzalloc(test, sizeof(struct cfg80211_csa_settings), GFP_KERNEL);
	struct net_device *wlan_dev;

	SLSI_MUTEX_INIT(ndev_vif->vif_mutex);

	params->chandef.chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	test_netdev_init(test, sdev, SLSI_NET_INDEX_P2PX_SWLAN);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	KUNIT_EXPECT_EQ(test, -EPERM, slsi_channel_switch(wiphy, ndev, params));

	ndev_vif->iftype = NL80211_IFTYPE_AP;
	ndev_vif->chandef_saved.chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);

	params->chandef.chan->center_freq = ndev_vif->chandef_saved.chan->center_freq = 2412;
	params->chandef.width = ndev_vif->chandef_saved.width = NL80211_CHAN_WIDTH_20;
	ndev_vif->iftype = NL80211_IFTYPE_AP;
	KUNIT_EXPECT_EQ(test, -EPERM, slsi_channel_switch(wiphy, ndev, params));

	ndev_vif->iftype = NL80211_IFTYPE_AP;

	params->chandef.chan->center_freq = 5755;
	params->chandef.width = NL80211_CHAN_WIDTH_40;
	slsi_channel_switch(wiphy, ndev, params);
}

static void test_slsi_get_key(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	u8 key_index = 1;
	bool pairwise = false;
	const u8 *mac_addr = SLSI_DEFAULT_HW_MAC_ADDR;
	void *cookie = NULL;

	ndev_vif->activated = false;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_get_key(wiphy, ndev, 0, key_index, pairwise, mac_addr, cookie, slsi_get_key_cb));
#else
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_get_key(wiphy, ndev, key_index, pairwise, mac_addr, cookie, slsi_get_key_cb));
#endif

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_get_key(wiphy, ndev, 0, key_index, pairwise, mac_addr, cookie, slsi_get_key_cb));
#else
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_get_key(wiphy, ndev, key_index, pairwise, mac_addr, cookie, slsi_get_key_cb));
#endif

	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	pairwise = true;
	key_index = 4;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_get_key(wiphy, ndev, 0, key_index, pairwise, mac_addr, cookie, slsi_get_key_cb));
#else
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_get_key(wiphy, ndev, key_index, pairwise, mac_addr, cookie, slsi_get_key_cb));
#endif

	pairwise = false;
	ndev_vif->ap.cipher = WLAN_CIPHER_SUITE_PMK;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, 0,
			slsi_get_key(wiphy, ndev, 0, key_index, pairwise, mac_addr, cookie, slsi_get_key_cb));
#else
	KUNIT_EXPECT_EQ(test, 0,
			slsi_get_key(wiphy, ndev, key_index, pairwise, mac_addr, cookie, slsi_get_key_cb));
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_get_key(wiphy, ndev, 0, 7, pairwise, mac_addr, cookie, slsi_get_key_cb));
#else
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_get_key(wiphy, ndev, 7, pairwise, mac_addr, cookie, slsi_get_key_cb));
#endif
}

static void test_slsi_abort_scan(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = ndev->ieee80211_ptr;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	slsi_abort_scan(wiphy, wdev);

	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = kunit_kzalloc(test, sizeof(struct cfg80211_scan_request),
								 GFP_KERNEL);
	slsi_abort_scan(wiphy, wdev);
}

static void test_slsi_scan(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = ndev->ieee80211_ptr;
	struct cfg80211_scan_request *request = kunit_kzalloc(test, sizeof(struct cfg80211_scan_request), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_scan(wiphy, request));

	request->wdev = wdev;
	SLSI_MUTEX_INIT(ndev_vif->scan_mutex);

	INIT_DELAYED_WORK(&ndev_vif->scan_timeout_work, kunit_slsi_p2p_unsync_vif_delete_work);

	test_netdev_init(test, sdev, SLSI_NET_INDEX_P2P);
	sdev->p2p_state = P2P_ACTION_FRAME_TX_RX;
	ndev_vif->mgmt_tx_data.exp_frame = SLSI_PA_INVALID;
	KUNIT_EXPECT_EQ(test, -EBUSY, slsi_scan(wiphy, request));

	sdev->p2p_state = P2P_IDLE_VIF_ACTIVE;
	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = kunit_kzalloc(test, sizeof(struct cfg80211_scan_request),
								 GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, -EBUSY, slsi_scan(wiphy, request));

	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = NULL;
	ndev_vif->drv_in_p2p_procedure = false;
	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	request->n_channels = 1;
	request->channels[0] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	request->channels[0]->hw_value = 0x01;
	request->channels[1] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	request->channels[1]->hw_value = 0x06;
	request->channels[2] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	request->channels[2]->hw_value = 0x0b;
	request->channels[3] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	request->channels[3]->hw_value = 0x0a;
	request->n_ssids = 1;
	request->ie = "\x00\x12\x44\x49\x52\x45\x43\x54\x2d\x56\x44";
	request->ie_len = 12;
	request->ssids = kunit_kzalloc(test, sizeof(struct cfg80211_ssid), GFP_KERNEL);
	memcpy(request->ssids->ssid, SLSI_KUNIT_P2P_SSID, 9);
	request->ssids->ssid_len = 15;
	sdev->p2p_state = P2P_GROUP_FORMED_GO;
	KUNIT_EXPECT_EQ(test, 0, slsi_scan(wiphy, request));

	request->n_channels = 4;
	sdev->initial_scan = true;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_UNSPECIFIED;
	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = NULL;
	KUNIT_EXPECT_EQ(test, -EIO, slsi_scan(wiphy, request));

	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	request->n_channels = 1;
	sdev->initial_scan = true;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_UNSPECIFIED;
	ndev_vif->unsync.slsi_p2p_continuous_fullscan = true;
	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = NULL;
	KUNIT_EXPECT_EQ(test, 0, slsi_scan(wiphy, request));

	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	request->n_channels = 4;
	sdev->initial_scan = true;
	sdev->p2p_state = P2P_GROUP_FORMED_CLI;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_UNSPECIFIED;
	ndev_vif->unsync.slsi_p2p_continuous_fullscan = true;
	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = NULL;
	KUNIT_EXPECT_EQ(test, -EIO, slsi_scan(wiphy, request));

	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	request->n_channels = 1;
	sdev->p2p_state = P2P_LISTENING;
	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = NULL;
	request->channels[0] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	request->channels[0]->hw_value = 0x01;
	KUNIT_EXPECT_EQ(test, 0, slsi_scan(wiphy, request));

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_UNSPECIFIED;
	sdev->p2p_state = P2P_LISTENING;
	request->n_channels = 0;
	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = NULL;
	sdev->initial_scan = true;
	request->ie = NULL;
	request->ie_len = 14;
	request->ie = "\xdd\x3a\x00\x50\xf2\x04\x10\x4a\x00\x01\x10\x10\x44";
	KUNIT_EXPECT_EQ(test, 0, slsi_scan(wiphy, request));

	request->n_channels = 4;
	KUNIT_EXPECT_EQ(test, -EBUSY, slsi_scan(wiphy, request));

	request->ie = NULL;

	kunit_kfree(test, request);
}

static void test_slsi_sched_scan_start(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct cfg80211_sched_scan_request *request = kunit_kzalloc(test, sizeof(struct cfg80211_sched_scan_request),
								    GFP_KERNEL);

	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_sched_scan_start(wiphy, ndev, request));

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	test_netdev_init(test, sdev, SLSI_NET_INDEX_P2P);
	sdev->p2p_state = P2P_ACTION_FRAME_TX_RX;
	KUNIT_EXPECT_EQ(test, -EBUSY, slsi_sched_scan_start(wiphy, ndev, request));

	sdev->p2p_state = P2P_IDLE_VIF_ACTIVE;
	request->ie = "\x24\x30\x48\x60\x6c\x7f\x0b\x00\x00\x08\x00\x00\x00\x00\x00\x00\x01\x20\xff\x21\x23\x03\x08\x88\x02\x00\x00";
	request->ie_len = 28;
	request->n_channels = 1;
	request->n_ssids = 0;
	ndev_vif->scan[SLSI_SCAN_HW_ID].sched_req = kunit_kzalloc(test, sizeof(struct cfg80211_sched_scan_request),
								  GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, -EBUSY, slsi_sched_scan_start(wiphy, ndev, request));

	ndev_vif->scan[SLSI_SCAN_HW_ID].sched_req = NULL;
	KUNIT_EXPECT_EQ(test, -EIO, slsi_sched_scan_start(wiphy, ndev, request));

	kunit_kfree(test, request);
}

static void test_slsi_sched_scan_stop(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
	KUNIT_EXPECT_EQ(test, 0, slsi_sched_scan_stop(wiphy, ndev, 0));
#else
	KUNIT_EXPECT_EQ(test, 0, slsi_sched_scan_stop(wiphy, ndev));
#endif

	ndev_vif->scan[SLSI_SCAN_SCHED_ID].sched_req = kunit_kzalloc(test, sizeof(struct cfg80211_sched_scan_request),
								     GFP_KERNEL);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0))
	KUNIT_EXPECT_EQ(test, 1, slsi_sched_scan_stop(wiphy, ndev, 0));
#else
	KUNIT_EXPECT_EQ(test, 1, slsi_sched_scan_stop(wiphy, ndev));
#endif
}


static void test_slsi_save_connection_params(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct cfg80211_connect_params *sme = kunit_kzalloc(test, sizeof(struct cfg80211_connect_params),
							    GFP_KERNEL);
	struct ieee80211_channel *channel = kunit_kzalloc(test, sizeof(struct ieee80211_channel),
							  GFP_KERNEL);
	const u8 *bssid = SLSI_DEFAULT_BSSID;
	u32 action_frame_bmap = SLSI_ACTION_FRAME_PUBLIC;
	u32 action_frame_suspend_bmap = SLSI_ACTION_FRAME_PUBLIC;

	slsi_save_connection_params(sdev, ndev, sme, channel, bssid, action_frame_bmap, action_frame_suspend_bmap);

	sme->ie = "\x30\x14\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x08\xcc\x00";
	sme->ie_len = 23;
	sme->ssid = SLSI_kUNIT_WLAN_SSID;
	sme->ssid_len = 16;
	sme->key = "\xc3\x43\xa9\x40\xa4\xdc\x3b\x2e\xa1\x63\x15\xe1\x6b\xfe\x36";
	sme->key_len = 16;
	slsi_save_connection_params(sdev, ndev, sme, channel, bssid, action_frame_bmap, action_frame_suspend_bmap);

	kfree(ndev_vif->sta.sme.ie);
	kfree(ndev_vif->sta.sme.ssid);
	kfree(ndev_vif->sta.sme.key);
	kfree(ndev_vif->sta.connected_bssid);
	kfree(ndev_vif->sta.connected_ssid);
}

static void test_slsi_set_params_on_bss(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct cfg80211_connect_params *sme = kunit_kzalloc(test, sizeof(struct cfg80211_connect_params),
							    GFP_KERNEL);
	struct ieee80211_channel *channel = kunit_kzalloc(test, sizeof(struct ieee80211_channel),
							  GFP_KERNEL);
	const u8 *bssid = SLSI_DEFAULT_BSSID;

	sme->bssid_hint = SLSI_DEFAULT_BSSID;
	sme->channel_hint = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	slsi_set_params_on_bss(sdev, ndev, sme, &channel, &bssid);

	sme->ie = "\x30\x14\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x08\xcc\x00";
	sme->ie_len = 23;
	sme->bssid = SLSI_DEFAULT_BSSID;
	sme->ssid = SLSI_kUNIT_WLAN_SSID;
	sme->ssid_len = 16;
	sme->key = "\xc3\x43\xa9\x40\xa4\xdc\x3b\x2e\xa1\x63\x15\xe1\x6b\xfe\x36";
	sme->key_len = 16;
	slsi_set_params_on_bss(sdev, ndev, sme, &channel, &bssid);
}

static void test_slsi_check_wificonnect_to_connectedGo(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct netdev_vif *p2p_ndev_vif;
	const u8 *bssid = SLSI_DEFAULT_BSSID;

	sdev->p2p_state = P2P_GROUP_FORMED_CLI;
	sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN] = kunit_kzalloc(test, sizeof(struct net_device) +
								sizeof(struct netdev_vif), GFP_KERNEL);
	p2p_ndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN]);
	p2p_ndev_vif->sta.sta_bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss) + sizeof(u8), GFP_KERNEL);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	KUNIT_EXPECT_EQ(test, 0, slsi_check_wificonnect_to_connectedGo(sdev, ndev, bssid));
}

static void test_slsi_set_roam_reassoc(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct cfg80211_connect_params *sme = kunit_kzalloc(test, sizeof(struct cfg80211_connect_params),
							    GFP_KERNEL);
	struct cfg80211_bss_ies *ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies) + sizeof(u8) * 20,
						     GFP_KERNEL);

	ies->len = 2;
	ndev_vif->is_fw_test = false;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_roam_reassoc(ndev, sdev, sme));

	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = true;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_roam_reassoc(ndev, sdev, sme));

	sme->bssid = SLSI_DEFAULT_BSSID;
	sme->bssid_hint = SLSI_DEFAULT_BSSID;
	ndev_vif->activated = false;
	memcpy(ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->address, SLSI_DEFAULT_BSSID, ETH_ALEN);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_roam_reassoc(ndev, sdev, sme));

	ndev_vif->activated = true;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_roam_reassoc(ndev, sdev, sme));

	sme->bssid = SLSI_kUNIT_WLAN_SSID;
	sme->bssid_hint = SLSI_kUNIT_WLAN_SSID;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_roam_reassoc(ndev, sdev, sme));

	memcpy(ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->address, SLSI_DEFAULT_HW_MAC_ADDR, ETH_ALEN);
	ndev_vif->sta.sta_bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss) + sizeof(u8), GFP_KERNEL);
	ndev_vif->sta.sta_bss->ies = ies;
	memset(ies->data, 0, 20);

	sme->ssid = SLSI_kUNIT_WLAN_SSID;
	sme->bssid = SLSI_kUNIT_WLAN_SSID;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_roam_reassoc(ndev, sdev, sme));

	sme->channel = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	sme->channel->center_freq = 0x2412;
	ndev_vif->driver_channel = 1;
	ndev_vif->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	ndev_vif->chan->center_freq = 0x2412;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_roam_reassoc(ndev, sdev, sme));

	sme->ssid = SLSI_kUNIT_WLAN_SSID;
	sme->bssid = 0;
	sme->bssid_hint = SLSI_kUNIT_WLAN_SSID;
	ndev_vif->chan->center_freq = 0x5745;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_roam_reassoc(ndev, sdev, sme));

	sme->bssid = SLSI_kUNIT_WLAN_SSID;
	sme->bssid_hint = 0;
	ndev_vif->mgmt_tx_cookie = 0x1234;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_roam_reassoc(ndev, sdev, sme));

	ndev_vif->mgmt_tx_cookie = 0x1;
	memcpy(ies->data, "\x1C\x05\x1F\x6b\x54\x68\x70", 8);
	ies->len = 8;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_roam_reassoc(ndev, sdev, sme));

	sme->bssid = "\x1F\x1F\x1F\x1F\x1F\x1F";
	sme->bssid_hint = SLSI_kUNIT_WLAN_SSID;
	memcpy(ies->data, "\x53\x4F\x4E\x47\x54\x41", 6);
	ies->len = 6;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_roam_reassoc(ndev, sdev, sme));

}

static void test_slsi_check_valid_netdev_vif_state(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->activated = true;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_check_valid_netdev_vif_state(sdev, ndev));

	ndev_vif->activated = false;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_UNSPECIFIED;
	KUNIT_EXPECT_EQ(test, 0, slsi_check_valid_netdev_vif_state(sdev, ndev));

	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_check_valid_netdev_vif_state(sdev, ndev));
}

static void test_slsi_set_bmap(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct net_device *p2p_dev;
	u32 action_frame_bmap;
	u32 action_frame_suspend_bmap;
	u8 device_address;

	SLSI_MUTEX_INIT(sdev->netdev_add_remove_mutex);
	sdev->device_config.wes_mode = true;

	ndev_vif->iftype = NL80211_IFTYPE_UNSPECIFIED;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_bmap(sdev, ndev, &action_frame_bmap, &action_frame_suspend_bmap,
					       &device_address));

	sdev->netdev[SLSI_NET_INDEX_P2P] = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif),
							 GFP_KERNEL);
	p2p_dev = sdev->netdev[SLSI_NET_INDEX_P2P];
	p2p_dev->dev_addr = SLSI_DEFAULT_HW_MAC_ADDR;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_CLIENT;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_bmap(sdev, ndev, &action_frame_bmap, &action_frame_suspend_bmap,
					       &device_address));

	ndev_vif->iftype = NL80211_IFTYPE_MONITOR;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_set_bmap(sdev, ndev, &action_frame_bmap, &action_frame_suspend_bmap, &device_address));
}

static void test_slsi_set_sta_bss_info(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct netdev_vif *sdev_ndev_vif;
	struct cfg80211_connect_params *sme = kunit_kzalloc(test, sizeof(struct cfg80211_connect_params), GFP_KERNEL);
	u8 *bssid;
	struct ieee80211_channel *channel;
	u16 prev_vif_type = NL80211_IFTYPE_UNSPECIFIED;
	struct slsi_ssid_info *ssid_info = kunit_kzalloc(test, sizeof(struct slsi_ssid_info) * 2, GFP_KERNEL);
	struct slsi_bssid_info *current_result = kunit_kzalloc(test, sizeof(struct slsi_bssid_info) * 2, GFP_KERNEL);

	sdev->tspec_error_code = 15;
	wiphy->interface_modes = NL80211_IFTYPE_P2P_CLIENT;

	sdev->wiphy->bands[NL80211_BAND_5GHZ] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band),
							      GFP_KERNEL);
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->n_channels = 1;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels = kunit_kzalloc(test, sizeof(struct ieee80211_channel),
									GFP_KERNEL);
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].center_freq = 5745;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].freq_offset = 0;

	sdev->netdev[SLSI_NET_INDEX_P2P] = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif),
							 GFP_KERNEL);
	sdev_ndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_P2P]);
	sdev_ndev_vif->sta.sta_bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss) + sizeof(u8), GFP_KERNEL);
	memcpy(sdev_ndev_vif->sta.sta_bss->bssid, SLSI_DEFAULT_BSSID, 6);


	sdev->device_config.supported_band = SLSI_FREQ_BAND_5GHZ;
	sme->privacy = true;
	sme->ssid = SLSI_kUNIT_WLAN_SSID;
	sme->ssid_len = 16;
	sme->bssid = NULL;
	sme->bssid_hint = NULL;

	sdev_ndev_vif->sta.sta_bss->signal = 21;
	channel = NULL;
	bssid = NULL;

	KUNIT_EXPECT_EQ(test, 0, slsi_set_sta_bss_info(wiphy, ndev, sdev, sme, &channel, &bssid, prev_vif_type));

	sdev_ndev_vif->sta.sta_bss->signal = 2;
	INIT_LIST_HEAD(&ndev_vif->sta.ssid_info);
	ssid_info->ssid.ssid_len = 9;
	ndev_vif->sta.ssid_len = 9;
	memcpy(ssid_info->ssid.ssid, SLSI_KUNIT_P2P_SSID, 9);
	memcpy(ndev_vif->sta.ssid, SLSI_KUNIT_P2P_SSID, 9);
	ssid_info->akm_type = SLSI_BSS_SECURED_SAE | SLSI_BSS_SECURED_PSK;
	ndev_vif->sta.akm_type = SLSI_BSS_SECURED_SAE | SLSI_BSS_SECURED_PSK;
	INIT_LIST_HEAD(&ssid_info->bssid_list);
	SLSI_ETHER_COPY(current_result->bssid, SLSI_DEFAULT_BSSID);
	current_result->rssi = -45;
	current_result->freq = 5745;
	current_result->connect_attempted = false;
	list_add_tail(&current_result->list, &ssid_info->bssid_list);
	list_add(&ssid_info->list, &ndev_vif->sta.ssid_info);

	sme->privacy = true;
	sme->bssid = NULL;
	sme->bssid_hint = NULL;
	sme->ssid = SLSI_kUNIT_WLAN_SSID;
	sme->ssid_len = 16;
	channel = NULL;
	bssid = NULL;

	KUNIT_EXPECT_EQ(test, -ENOENT, slsi_set_sta_bss_info(wiphy, ndev, sdev, sme, &channel, &bssid, prev_vif_type));

	sdev_ndev_vif->sta.sta_bss->signal = 4;
	ndev_vif->sta.ssid_len = 9;
	ndev_vif->sta.akm_type = SLSI_BSS_SECURED_SAE | SLSI_BSS_SECURED_PSK;
	sme->privacy = true;
	sme->bssid = NULL;
	sme->bssid_hint = NULL;
	sme->ssid = SLSI_kUNIT_WLAN_SSID;
	sme->ssid_len = 16;
	channel = NULL;
	bssid = NULL;

	KUNIT_EXPECT_EQ(test, 0, slsi_set_sta_bss_info(wiphy, ndev, sdev, sme, &channel, &bssid, prev_vif_type));

	sdev_ndev_vif->sta.sta_bss->signal = 4;
	sme->privacy = true;
	sme->bssid = SLSI_DEFAULT_BSSID;
	sme->bssid_hint = SLSI_DEFAULT_BSSID;
	sme->ssid = SLSI_kUNIT_WLAN_SSID;
	sme->ssid_len = 16;
	channel = NULL;
	bssid = NULL;

	KUNIT_EXPECT_EQ(test, 0, slsi_set_sta_bss_info(wiphy, ndev, sdev, sme, &channel, &bssid, prev_vif_type));
}

static void test_slsi_config_rsn_ie(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct cfg80211_connect_params *sme = kunit_kzalloc(test, sizeof(struct cfg80211_connect_params), GFP_KERNEL);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	sme->crypto.n_akm_suites = 1;
	sme->ie = "\x30\x14\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x08\xcc\x00";
	sme->ie_len = 30;
	ndev_vif->sta.rsn_ie = kmalloc(20, GFP_KERNEL);
	slsi_config_rsn_ie(ndev, sme);
	KUNIT_EXPECT_STREQ(test, ndev_vif->sta.rsn_ie, sme->ie);

	sme->ie = "\x30\x14\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x12\xcc\x00";
	sme->ie_len = 30;
	slsi_config_rsn_ie(ndev, sme);
	KUNIT_EXPECT_STREQ(test, ndev_vif->sta.rsn_ie, sme->ie);

	sme->ie = "\x30\x14\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x22\xcc\x00";
	sme->ie_len = 30;
	slsi_config_rsn_ie(ndev, sme);
	KUNIT_EXPECT_STREQ(test, ndev_vif->sta.rsn_ie, sme->ie);

	kfree(ndev_vif->sta.rsn_ie);
	ndev_vif->sta.rsn_ie = NULL;
}

static void test_slsi_connect(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct cfg80211_connect_params *sme = kunit_kzalloc(test, sizeof(struct cfg80211_connect_params), GFP_KERNEL);
	struct netdev_vif *ndev_p2p_vif;
	struct netdev_vif *sdev_ndev_vif;

	SLSI_MUTEX_INIT(sdev->start_stop_mutex);
	SLSI_MUTEX_INIT(sdev->netdev_add_remove_mutex);
	SLSI_MUTEX_INIT(ndev_vif->vif_mutex);

	test_netdev_init(test, sdev, SLSI_NET_INDEX_P2PX_SWLAN);
	ndev_p2p_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN]);
	ndev_p2p_vif->sta.sta_bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss) + sizeof(u8), GFP_KERNEL);
	ndev_p2p_vif->sta.sta_bss->signal = 21;

	wiphy->interface_modes = NL80211_IFTYPE_P2P_GO;
	sdev->chip_info_mib.chip_version = 1;
	sdev->device_state = SLSI_DEVICE_STATE_STARTING;
	ndev_vif->sta.sta_bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss) + sizeof(u8), GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_connect(wiphy, ndev, sme));

	ndev_vif->acl_data_supplicant = NULL;
	sdev->device_state = SLSI_DEVICE_STATE_STARTED;
	sme->channel = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	sme->channel->center_freq = 5755;

	sme->ie = "\x30\x14\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x08\xcc\x00";
	sme->ie_len = 22;
	sme->bssid = SLSI_DEFAULT_BSSID;
	sme->ssid = "CNT_KUNT_SSID";
	sme->ssid_len = 14;
	sme->key = "\xc3\x43\xa9\x40\xa4\xdc\x3b\x2e\xa1\x93\xc0\x3c\xab\xaa\xe4\x8a\xfd\x86\x9c\x87\xa6\xf5\xe8\xc7\x07\x63\x15\xe1\x6b\xfe\x36\x06";
	sme->key_len = 16;

	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_connect(wiphy, ndev, sme));

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	sdev->p2p_state = P2P_GROUP_FORMED_CLI;

	ndev_vif->iftype = NL80211_IFTYPE_UNSPECIFIED;

	sme->bssid = SLSI_DEFAULT_BSSID;
	sme->ssid = "CNT_KUNT_SSID";
	sme->ssid_len = 14;
	sme->ie = "\x30\x14\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x08\xcc\x00";
	sme->ie_len = 22;
	sme->key = "\xc3\x43\xa9\x40\xa4\xdc\x3b\x2e\xa1\x93\xc0\x3c\xab\xaa\xe4\x8a\xfd\x86\x9c\x87\xa6\xf5\xe8\xc7\x07\x63\x15\xe1\x6b\xfe\x36\x06";
	sme->key_len = 16;
	memcpy(ndev_p2p_vif->sta.sta_bss->bssid, SLSI_DEFAULT_BSSID, 7);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_connect(wiphy, ndev, sme));

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	sdev->p2p_state = P2P_GROUP_FORMED_CLI;
	ndev_p2p_vif->sta.sta_bss->signal = 21;
	ndev_vif->iftype = NL80211_IFTYPE_UNSPECIFIED;

	sme->bssid = SLSI_DEFAULT_BSSID;
	sme->ssid = "CNT_KUNT_SSID";
	sme->ssid_len = 14;
	sme->ie = "\x30\x14\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x08\xcc\x00";
	sme->ie_len = 22;
	sme->key = "\xc3\x43\xa9\x40\xa4\xdc\x3b\x2e\xa1\x93\xc0\x3c\xab\xaa\xe4\x8a\xfd\x86\x9c\x87\xa6\xf5\xe8\xc7\x07\x63\x15\xe1\x6b\xfe\x36\x06";
	sme->key_len = 16;

	memset(ndev_p2p_vif->sta.sta_bss->bssid, 0, 7);
	memcpy(ndev_p2p_vif->sta.sta_bss->bssid, SLSI_DEFAULT_BSSID, 7);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_connect(wiphy, ndev, sme));

	ndev_vif->activated = false;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;

	sme->bssid = 0;
	sme->ssid = "CNT_KUNT_SSID";
	sme->ssid_len = 14;
	sme->ie = "\x30\x14\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x08\xcc\x00";
	sme->ie_len = 22;
	sme->key = "\xc3\x43\xa9\x40\xa4\xdc\x3b\x2e\xa1\x93\xc0\x3c\xab\xaa\xe4\x8a\xfd\x86\x9c\x87\xa6\xf5\xe8\xc7\x07\x63\x15\xe1\x6b\xfe\x36\x06";
	sme->key_len = 16;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_connect(wiphy, ndev, sme));

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->vif_type = FAPI_VIFTYPE_PRECONNECT;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_UNSPECIFIED;
	sdev->p2p_state = P2P_IDLE_VIF_ACTIVE;
	ndev_vif->activated = false;
	ndev->dev_addr = SLSI_DEFAULT_HW_MAC_ADDR;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_CLIENT;
	ndev_vif->probe_req_ies = "0x12";
	sme->crypto.wpa_versions = 2;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_connect(wiphy, ndev, sme));

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->vif_type = NL80211_IFTYPE_MESH_POINT;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_UNSPECIFIED;
	sdev->p2p_state = P2P_GROUP_FORMED_CLI;
	ndev_vif->activated = false;
	ndev->dev_addr = SLSI_DEFAULT_HW_MAC_ADDR;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_CLIENT;
	ndev_vif->probe_req_ies = "0x12";
	sme->crypto.wpa_versions = 3;
	sdev->chip_info_mib.chip_version = 22;
	memcpy(ndev_p2p_vif->sta.sta_bss->bssid, SLSI_DEFAULT_BSSID, 6);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_connect(wiphy, ndev, sme));

	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->iftype = NL80211_IFTYPE_UNSPECIFIED;
	ndev_vif->activated = false;
	sdev->device_state = SLSI_DEVICE_STATE_STARTED;
	sdev->chip_info_mib.chip_version = 22;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_connect(wiphy, ndev, sme));

	ndev_vif->vif_type = FAPI_VIFTYPE_MONITOR;
	memcpy(ndev_p2p_vif->sta.sta_bss->bssid, "CNT_KUNT_SSID", 14);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_connect(wiphy, ndev, sme));

	sme->bssid = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_connect(wiphy, ndev, sme));

	ndev_vif->power_mode = 12;
	ndev_vif->set_power_mode = 43;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_connect(wiphy, ndev, sme));

	kfree(ndev_vif->sta.connected_bssid);
	kfree(ndev_vif->sta.connected_ssid);
	kfree(ndev_vif->sta.sme.ssid);
	kfree(ndev_vif->sta.sme.ie);
	kfree(ndev_vif->sta.sme.key);
	kfree(ndev_vif->sta.rsn_ie);
	sme = NULL;
}

static void test_slsi_disconnect(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	u16 reason_code = FAPI_REASONCODE_UNSPECIFIED_REASON;

	SLSI_MUTEX_INIT(ndev_vif->vif_mutex);
	INIT_WORK(&ndev_vif->set_multicast_filter_work, NULL);
	INIT_WORK(&ndev_vif->update_pkt_filter_work, NULL);
	ndev_vif->activated = false;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	KUNIT_EXPECT_EQ(test, 0, slsi_disconnect(wiphy, ndev, reason_code));

	ndev_vif->activated = true;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer),
									  GFP_KERNEL);
	sdev->device_state = SLSI_DEVICE_STATE_STOPPED;
	ndev->state = __LINK_STATE_DORMANT;
	KUNIT_EXPECT_EQ(test, 0, slsi_disconnect(wiphy, ndev, reason_code));

	ndev_vif->vif_type = FAPI_VIFTYPE_MONITOR;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_disconnect(wiphy, ndev, reason_code));
}

static void test_slsi_set_wiphy_params(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;
	u32 changed = WIPHY_PARAM_FRAG_THRESHOLD | WIPHY_PARAM_RTS_THRESHOLD;

	wiphy->frag_threshold = 100;
	wiphy->rts_threshold = 200;
	sdev->device_state = SLSI_DEVICE_STATE_STARTING;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_wiphy_params(wiphy, changed));
}

static void test_slsi_not_supported_func(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = ndev->ieee80211_ptr;
	struct bss_parameters *params = kunit_kzalloc(test, sizeof(struct bss_parameters), GFP_KERNEL);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, 0, slsi_set_default_key(wiphy, ndev, 0, 1, false, false));
#else
	KUNIT_EXPECT_EQ(test, 0, slsi_set_default_key(wiphy, ndev, 1, false, false));
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0))
	KUNIT_EXPECT_EQ(test, 0, slsi_config_default_mgmt_key(wiphy, ndev, 0, 1));
#else
	KUNIT_EXPECT_EQ(test, 0, slsi_config_default_mgmt_key(wiphy, ndev, 1));
#endif
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, slsi_set_tx_power(wiphy, wdev, 0, 0));
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, slsi_get_tx_power(wiphy, wdev, 0));
	KUNIT_EXPECT_EQ(test, 0, slsi_suspend(wiphy, 0));
	KUNIT_EXPECT_EQ(test, 0, slsi_resume(wiphy));
	KUNIT_EXPECT_EQ(test, 0, slsi_flush_pmksa(wiphy, ndev));

	ndev_vif->iftype = NL80211_IFTYPE_AP;
	params->ap_isolate = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_change_bss(wiphy, ndev, params));
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, slsi_change_beacon(wiphy, ndev, 0));
	slsi_mgmt_frame_register(wiphy, wdev, 0, false);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 41))
	KUNIT_EXPECT_EQ(test, 0, slsi_stop_ap(wiphy, ndev, 0));
#else
	KUNIT_EXPECT_EQ(test, 0, slsi_stop_ap(wiphy, ndev));
#endif
}

static void test_slsi_get_channel(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = ndev->ieee80211_ptr;
	struct cfg80211_chan_def chandef;

	SLSI_MUTEX_INIT(ndev_vif->vif_mutex);
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_UNSPECIFIED;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 41))
	KUNIT_EXPECT_EQ(test, -ENODATA, slsi_get_channel(wiphy, wdev, 0, &chandef));
#else
	KUNIT_EXPECT_EQ(test, -ENODATA, slsi_get_channel(wiphy, wdev, &chandef));
#endif

	ndev_vif->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);

	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->sta.bss_cf = 5745;
	ndev_vif->sta.ch_width = 20;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 41))
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_channel(wiphy, wdev, 0, &chandef));
#else
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_channel(wiphy, wdev, &chandef));
#endif

	ndev_vif->sta.bss_cf = 5755;
	ndev_vif->chan->center_freq = 5765;
	ndev_vif->sta.ch_width = 40;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 41))
	KUNIT_EXPECT_EQ(test, 0, slsi_get_channel(wiphy, wdev, 0, &chandef));
#else
	KUNIT_EXPECT_EQ(test, 0, slsi_get_channel(wiphy, wdev, &chandef));
#endif

	ndev_vif->sta.bss_cf = 5775;
	ndev_vif->chan->center_freq = 5805;
	ndev_vif->sta.ch_width = 80;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 41))
	KUNIT_EXPECT_EQ(test, 0, slsi_get_channel(wiphy, wdev, 0, &chandef));
#else
	KUNIT_EXPECT_EQ(test, 0, slsi_get_channel(wiphy, wdev, &chandef));
#endif

	ndev_vif->sta.bss_cf = 6185;
	ndev_vif->chan->center_freq = 6255;
	ndev_vif->sta.ch_width = 160;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 41))
	KUNIT_EXPECT_EQ(test, 0, slsi_get_channel(wiphy, wdev, 0, &chandef));
#else
	KUNIT_EXPECT_EQ(test, 0, slsi_get_channel(wiphy, wdev, &chandef));
#endif
}

static void test_slsi_del_station(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct station_del_parameters *del_params;
	struct slsi_peer *peer;

	del_params = kunit_kzalloc(test, sizeof(struct station_del_parameters), GFP_KERNEL);
	del_params->mac = "\xFF\xFF\xFF\xFF\xFF\xFF";
	del_params->reason_code = WLAN_REASON_DEAUTH_LEAVING;

	SLSI_MUTEX_INIT(ndev_vif->vif_mutex);

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->ifnum = SLSI_NET_INDEX_AP;
	ndev_vif->ap.p2p_gc_keys_set = 1;

	ndev_vif->peer_sta_record[0] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	peer = ndev_vif->peer_sta_record[0];
	peer->valid = true;
	SLSI_ETHER_COPY(peer->address, "\xA0\x9D\x37\xAA\x22\xAB");
	peer->connected_state = SLSI_STA_CONN_STATE_CONNECTING;
	ndev_vif->ap.wpa_ie_len = 1;
	ndev_vif->ap.cache_wpa_ie = "\x12";
	ndev_vif->ap.wmm_ie_len = 1;
	ndev_vif->ap.cache_wmm_ie = "\x34";

	sdev->device_config.user_suspend_mode = EINVAL;
	KUNIT_EXPECT_EQ(test, 0, slsi_del_station(wiphy, ndev, del_params));

	del_params->mac = "\xA0\x9D\x37\xAA\x22\xAB";
	peer->connected_state = SLSI_STA_CONN_STATE_CONNECTING;
	KUNIT_EXPECT_EQ(test, 0, slsi_del_station(wiphy, ndev, del_params));

	peer->connected_state = SLSI_STA_CONN_STATE_CONNECTED;
	peer->is_wps = true;
	KUNIT_EXPECT_EQ(test, 0, slsi_del_station(wiphy, ndev, del_params));

	peer->is_wps = false;
	sdev->device_config.user_suspend_mode = ERANGE;
	KUNIT_EXPECT_EQ(test, -ERANGE, slsi_del_station(wiphy, ndev, del_params));
}

static void test_slsi_get_station(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	u8 *mac = SLSI_DEFAULT_HW_MAC_ADDR;
	struct slsi_peer *peer;
	struct station_info sinfo;

	SLSI_MUTEX_INIT(ndev_vif->vif_mutex);
	sdev->device_config.user_suspend_mode = 0;
	ndev_vif->sta.tdls_enabled = true;

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_station(wiphy, ndev, mac, &sinfo));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_MONITOR;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_station(wiphy, ndev, mac, &sinfo));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->peer_sta_record[1] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	peer = ndev_vif->peer_sta_record[1];
	peer->valid = true;
	SLSI_ETHER_COPY(peer->address, SLSI_DEFAULT_HW_MAC_ADDR);
	ndev_vif->sta.roam_in_progress = false;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_CLIENT;

	peer->sinfo.tx_packets = 1200;
	peer->sinfo.tx_bytes = 2400;
	peer->sinfo.rx_packets = 3455;
	peer->sinfo.rx_bytes = 23;
	peer->sinfo.tx_retries = 253;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0))
	peer->sinfo.airtime_link_metric = 12;
#endif
	KUNIT_EXPECT_EQ(test, 0, slsi_get_station(wiphy, ndev, mac, &sinfo));
}

static void test_slsi_set_power_mgmt(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;

	SLSI_MUTEX_INIT(ndev_vif->vif_mutex);

	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->activated = true;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_power_mgmt(wiphy, ndev, true, 100));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_power_mgmt(wiphy, ndev, true, 100));
}

static void test_slsi_set_qos_map(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct cfg80211_qos_map *qos_map = kunit_kzalloc(test, sizeof(struct cfg80211_qos_map), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, slsi_set_qos_map(0, ndev, 0));

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_qos_map(0, ndev, qos_map));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_qos_map(0, ndev, qos_map));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_qos_map(0, ndev, qos_map));

	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = true;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_qos_map(0, ndev, qos_map));
}


static void test_slsi_set_monitor_channel(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct cfg80211_chan_def chandef;

	chandef.chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	chandef.chan->center_freq = 5775;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_monitor_channel(wiphy, &chandef));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_monitor_channel(wiphy, &chandef));

	ndev_vif->vif_type = FAPI_VIFTYPE_MONITOR;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_monitor_channel(wiphy, &chandef));
}

static void test_slsi_set_pmksa(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct cfg80211_pmksa *pmksa = kunit_kzalloc(test, sizeof(struct cfg80211_pmksa), GFP_KERNEL);

	ndev_vif->sta.use_set_pmksa = true;
	ndev_vif->sta.rsn_ie = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_pmksa(wiphy, ndev, pmksa));

	ndev_vif->sta.rsn_ie = "\x30\x1a\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x08\xc0\x00\x00\x00\x00\x0f\xac\x06";
	ndev_vif->sta.rsn_ie_len = 26;
	pmksa->pmkid = "\x45\xAB\x7D\x9A\x24\x0A\x99\x12\x23\x84\xA5\xBC\x34\x00\x02\x61";
	ndev_vif->sta.assoc_req_add_info_elem_len = 10;
	ndev_vif->sta.assoc_req_add_info_elem = "";
	KUNIT_EXPECT_EQ(test, 0, slsi_set_pmksa(wiphy, ndev, pmksa));

	ndev_vif->sta.rsn_ie = "\x30\x1a\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x04\x01\x00\x00\x0f\xac\x08\xc0\x00\x00\x01\x45\xAB\x7D\x9A\x24\x0A\x99\x12\x23\x84\xA5\xBC\x34\x00\x02\x61\x00\x0f\xac\x06";
	ndev_vif->sta.rsn_ie_len = 42;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_pmksa(wiphy, ndev, pmksa));
}

static void test_slsi_del_pmksa(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;

	ndev_vif->sta.use_set_pmksa = true;
	ndev_vif->activated = false;

	KUNIT_EXPECT_EQ(test, 0, slsi_del_pmksa(wiphy, ndev, 0));
}

static void test_slsi_remain_on_channel(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = ndev->ieee80211_ptr;
	struct ieee80211_channel *chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	unsigned int duration = 0;
	u64 cookie;

	ndev_vif->activated = true;
	sdev->nan_enabled = false;

	SLSI_MUTEX_INIT(ndev_vif->vif_mutex);
	SLSI_MUTEX_INIT(sdev->start_stop_mutex);

	kunit_slsi_wq_init(ndev_vif, test);

	sdev->device_state = SLSI_DEVICE_STATE_STARTING;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_remain_on_channel(wiphy, wdev, chan, duration, &cookie));

	sdev->device_state = SLSI_DEVICE_STATE_STARTED;
	chan->center_freq = 5755;
	duration = 100;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	sdev->p2p_state = P2P_SCANNING;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_remain_on_channel(wiphy, wdev, chan, duration, &cookie));

	sdev->p2p_state = P2P_GROUP_FORMED_CLI;
	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	KUNIT_EXPECT_EQ(test, 0, slsi_remain_on_channel(wiphy, wdev, chan, duration, &cookie));

	sdev->p2p_state = P2P_ACTION_FRAME_TX_RX;
	KUNIT_EXPECT_EQ(test, 0, slsi_remain_on_channel(wiphy, wdev, chan, duration, &cookie));

	sdev->p2p_state = P2P_IDLE_NO_VIF;
	KUNIT_EXPECT_EQ(test, 0, slsi_remain_on_channel(wiphy, wdev, chan, duration, &cookie));

	sdev->p2p_state = P2P_SCANNING;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_remain_on_channel(wiphy, wdev, chan, duration, &cookie));

	ndev_vif->sta.use_set_pmksa = false;
	ndev_vif->sta.rsn_ie = NULL;

	sdev->p2p_state = P2P_IDLE_VIF_ACTIVE;
	ndev_vif->unsync.ies_changed = true;
	ndev_vif->unsync.probe_rsp_ies = "\x12\x23\x34";
	ndev_vif->unsync.probe_rsp_ies_len = 3;
	ndev_vif->driver_channel = 151;
	chan->hw_value = 149;
	KUNIT_EXPECT_EQ(test, 0, slsi_remain_on_channel(wiphy, wdev, chan, duration, &cookie));

	sdev->p2p_state = P2P_IDLE_VIF_ACTIVE;
	ndev_vif->unsync.ies_changed = true;
	ndev_vif->unsync.probe_rsp_ies = "\x12\x23\x34";
	ndev_vif->unsync.probe_rsp_ies_len = 3;
	chan->hw_value = 196;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_remain_on_channel(wiphy, wdev, chan, duration, &cookie));

	sdev->p2p_state = P2P_IDLE_VIF_ACTIVE;
	ndev_vif->unsync.ies_changed = true;
	ndev_vif->unsync.probe_rsp_ies = NULL;
	ndev_vif->unsync.probe_rsp_ies_len = 548;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_remain_on_channel(wiphy, wdev, chan, duration, &cookie));

	kunit_slsi_p2p_deinit(ndev_vif);
}

static void test_slsi_cancel_remain_on_channel(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = ndev->ieee80211_ptr;
	u64 cookie;

	kunit_slsi_wq_init(ndev_vif, test);

	cookie = 0x24;
	ndev_vif->vif_type = FAPI_VIFTYPE_PRECONNECT;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	sdev->p2p_state = P2P_ACTION_FRAME_TX_RX;
	ndev_vif->ap.p2p_gc_keys_set = 0;
	ndev_vif->unsync.roc_cookie = 0x24;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_cancel_remain_on_channel(wiphy, wdev, cookie));

	ndev_vif->mgmt_tx_data.exp_frame = SLSI_P2P_PA_GO_NEG_RSP;
	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	ndev_vif->drv_in_p2p_procedure = false;
	KUNIT_EXPECT_EQ(test, 0, slsi_cancel_remain_on_channel(wiphy, wdev, cookie));

	kunit_slsi_p2p_deinit(ndev_vif);
}

static void test_slsi_configure_country_code(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	struct cfg80211_ap_settings *settings = kunit_kzalloc(test, sizeof(struct cfg80211_ap_settings), GFP_KERNEL);

	settings->beacon.head = "";
	settings->beacon.head_len = 0;
	settings->beacon.tail = "\x07\x16\x55\x53\x04";
	settings->beacon.tail_len = 5;

	sdev->device_config.domain_info.regdomain = kunit_kzalloc(test, sizeof(struct ieee80211_regdomain) +
								  sizeof(struct ieee80211_reg_rule), GFP_KERNEL);
	memcpy(sdev->device_config.domain_info.regdomain->alpha2, "US\n", 3);
	sdev->wlan_unsync_vif_state = WLAN_UNSYNC_VIF_ACTIVE;

	KUNIT_EXPECT_EQ(test, 0, slsi_configure_country_code(sdev, settings));

	memcpy(sdev->device_config.domain_info.regdomain->alpha2, "KR\n", 3);
	sdev->current_tspec_id = 12;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_configure_country_code(sdev, settings));

	kunit_kfree(test, settings);
}

static void test_slsi_modify_ht_capa_ies(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct cfg80211_ap_settings *settings = kunit_kzalloc(test, sizeof(struct cfg80211_ap_settings), GFP_KERNEL);

	ndev_vif->chandef = kunit_kzalloc(test, sizeof(struct cfg80211_chan_def), GFP_KERNEL);
	ndev_vif->chandef->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	ndev_vif->chandef->width = NL80211_CHAN_WIDTH_20;
	ndev_vif->chandef->chan->hw_value = 10;
	sdev->fw_ht_cap[0] = IEEE80211_HT_CAP_LDPC_CODING | IEEE80211_HT_CAP_SGI_20;
	sdev->fw_ht_cap[1] = IEEE80211_HT_CAP_LDPC_CODING | IEEE80211_HT_CAP_SGI_20;

	KUNIT_EXPECT_EQ(test, 0, slsi_modify_ht_capa_ies(ndev, sdev, settings));

	ndev_vif->chandef->width = NL80211_CHAN_WIDTH_40;
	settings->beacon.tail = "\x3d\x16\x95\x0d\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
	settings->beacon.tail_len = 105;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_modify_ht_capa_ies(ndev, sdev, settings));

	settings->beacon.tail_len = 22;
	KUNIT_EXPECT_EQ(test, 0, slsi_modify_ht_capa_ies(ndev, sdev, settings));
}


static void test_slsi_ap_start_get_indoor_chan(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct cfg80211_ap_settings *settings = kunit_kzalloc(test, sizeof(struct cfg80211_ap_settings), GFP_KERNEL);
	int indoor_channel = 0;

	settings->chandef.chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);

	sdev->wiphy->bands[NL80211_BAND_2GHZ] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band),
							      GFP_KERNEL);
	sdev->wiphy->bands[NL80211_BAND_2GHZ]->n_channels = 0;
	sdev->wiphy->bands[NL80211_BAND_5GHZ] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band),
							      GFP_KERNEL);
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->n_channels = 1;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels = kunit_kzalloc(test, sizeof(struct ieee80211_channel),
									GFP_KERNEL);
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].band = NL80211_BAND_5GHZ;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].center_freq = 5745;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].freq_offset = 0;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].hw_value = 149;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].orig_mag = 0;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].flags = IEEE80211_CHAN_16MHZ;

	settings->chandef.chan = sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels;
	settings->chandef.chan->band = NL80211_BAND_5GHZ;
	settings->chandef.chan->center_freq = 0;
	settings->chandef.chan->flags = IEEE80211_CHAN_INDOOR_ONLY;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_CLIENT;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ap_start_get_indoor_chan(sdev, ndev, settings, false, &indoor_channel));

	settings->chandef.chan->band = NL80211_BAND_5GHZ;
	settings->chandef.chan->center_freq = 5745;
	settings->chandef.chan->flags = IEEE80211_CHAN_INDOOR_ONLY;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ap_start_get_indoor_chan(sdev, ndev, settings, false, &indoor_channel));

	sdev->wiphy = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ap_start_get_indoor_chan(sdev, ndev, settings, false, &indoor_channel));
}

static void test_slsi_ap_start_obss_scan(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->chan = NULL;
	slsi_ap_start_obss_scan(sdev, ndev, ndev_vif);

	sdev->wiphy->bands[NL80211_BAND_2GHZ] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band),
							      GFP_KERNEL);
	sdev->wiphy->bands[NL80211_BAND_2GHZ]->n_channels = 0;

	sdev->wiphy->bands[NL80211_BAND_5GHZ] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band),
							      GFP_KERNEL);
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->n_channels = 1;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels = kunit_kzalloc(test, sizeof(struct ieee80211_channel),
									GFP_KERNEL);
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].band = NL80211_BAND_5GHZ;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].center_freq = 5775;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].freq_offset = 0;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].hw_value = 155;

	ndev_vif->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	ndev_vif->chan->hw_value = 155;
	ndev_vif->chan->center_freq = 5775;

	slsi_ap_start_obss_scan(sdev, ndev, ndev_vif);

	kunit_kfree(test, ndev_vif->chan);
}

static void test_slsi_set_ap_mode(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct cfg80211_ap_settings *settings = kunit_kzalloc(test, sizeof(struct cfg80211_ap_settings), GFP_KERNEL);

	slsi_set_ap_mode(ndev_vif, settings, true);
	KUNIT_EXPECT_EQ(test, SLSI_80211_MODE_11AC, ndev_vif->ap.mode);

	settings->beacon.tail_len = 155;
	settings->beacon.tail = "\x01\x08\x8c\x12\x98\x24\xb0\x48\x60\x6c";
	slsi_set_ap_mode(ndev_vif, settings, false);
	KUNIT_EXPECT_EQ(test, SLSI_80211_MODE_11G, ndev_vif->ap.mode);

	settings->beacon.tail_len = 26;
	settings->beacon.tail = "\x2d\x1a\xac\x19\x1b\xff\xff\xff\x00\x00\x00\x00\x00";
	slsi_set_ap_mode(ndev_vif, settings, false);
	KUNIT_EXPECT_EQ(test, SLSI_80211_MODE_11N, ndev_vif->ap.mode);

	kunit_kfree(test, settings);
}

static void test_slsi_store_settings_for_recovery(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct cfg80211_ap_settings *settings = kunit_kzalloc(test, sizeof(struct cfg80211_ap_settings), GFP_KERNEL);

	ndev_vif->backup_settings = *settings;
	slsi_store_settings_for_recovery(settings, ndev_vif);

	settings->chandef.chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	settings->beacon.head_len = 0;
	settings->beacon.tail_len = 0;
	settings->beacon.beacon_ies_len = 0;
	settings->beacon.proberesp_ies_len = 0;
	settings->beacon.assocresp_ies_len = 0;
	settings->beacon.probe_resp_len = 0;
	settings->ssid_len = 0;

	settings->ht_cap = kunit_kzalloc(test, sizeof(struct ieee80211_ht_cap), GFP_KERNEL);
	settings->vht_cap = kunit_kzalloc(test, sizeof(struct ieee80211_vht_cap), GFP_KERNEL);

	slsi_store_settings_for_recovery(settings, ndev_vif);

	kfree(ndev_vif->backup_settings.ht_cap);
	kfree(ndev_vif->backup_settings.vht_cap);
	kunit_kfree(test, settings);
}

static void test_slsi_ap_start_validate(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct cfg80211_ap_settings *settings = kunit_kzalloc(test, sizeof(struct cfg80211_ap_settings), GFP_KERNEL);

	settings->ssid = SLSI_kUNIT_WLAN_SSID;
	settings->ssid_len = 0;
	settings->beacon.head = "test";
	settings->beacon.head_len = 0;
	settings->beacon_interval = 0;

	settings->chandef.width = NL80211_CHAN_WIDTH_40;
	settings->chandef.chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	settings->chandef.chan->band = NL80211_BAND_2GHZ;
	sdev->fw_SoftAp_2g_40mhz_enabled = false;

	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ap_start_validate(ndev, sdev, settings));

	ndev_vif->ifnum = SLSI_NET_INDEX_P2PX_SWLAN;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_GO;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ap_start_validate(ndev, sdev, settings));

	settings->ssid_len = 26;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ap_start_validate(ndev, sdev, settings));

	settings->beacon.head_len = 4;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ap_start_validate(ndev, sdev, settings));

	settings->beacon_interval = 100;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ap_start_validate(ndev, sdev, settings));

	settings->chandef.chan->band = NL80211_BAND_5GHZ;
	KUNIT_EXPECT_EQ(test, 0, slsi_ap_start_validate(ndev, sdev, settings));
}

static void test_slsi_get_max_bw_mhz(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	u16 prim_chan_cf = 5775;
	struct ieee80211_regdomain *regd;

	sdev->device_config.domain_info.regdomain = kunit_kzalloc(test, sizeof(struct ieee80211_regdomain) +
								  sizeof(struct ieee80211_reg_rule) * 2, GFP_KERNEL);
	regd = sdev->device_config.domain_info.regdomain;

	regd->n_reg_rules = 2;
	regd->reg_rules[0].freq_range.start_freq_khz = 5765000;
	regd->reg_rules[0].freq_range.end_freq_khz = 5785000;
	regd->reg_rules[0].flags = NL80211_RRF_AUTO_BW;
	regd->reg_rules[0].freq_range.max_bandwidth_khz = 5895000;

	regd->reg_rules[1].freq_range.start_freq_khz = 5785000;
	regd->reg_rules[1].freq_range.end_freq_khz = 5805000;
	regd->reg_rules[1].flags = NL80211_RRF_AUTO_BW;
	KUNIT_EXPECT_EQ(test, 40, slsi_get_max_bw_mhz(sdev, prim_chan_cf));

	regd->n_reg_rules = 2;
	regd->reg_rules[0].freq_range.start_freq_khz = 5775000;
	regd->reg_rules[0].freq_range.end_freq_khz = 5765000;

	regd->reg_rules[1].freq_range.start_freq_khz = 5765000;
	regd->reg_rules[1].freq_range.end_freq_khz = 5855000;
	KUNIT_EXPECT_EQ(test, 80, slsi_get_max_bw_mhz(sdev, prim_chan_cf));

	regd->n_reg_rules = 1;
	regd->reg_rules[0].freq_range.start_freq_khz = 5765000;
	regd->reg_rules[0].freq_range.end_freq_khz = 5785000;
	KUNIT_EXPECT_EQ(test, 5895, slsi_get_max_bw_mhz(sdev, prim_chan_cf));

	regd->n_reg_rules = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_get_max_bw_mhz(sdev, prim_chan_cf));

	sdev->device_config.domain_info.regdomain = NULL;
	KUNIT_EXPECT_EQ(test, 0, slsi_get_max_bw_mhz(sdev, prim_chan_cf));
}


static void test_slsi_stop_p2p_group_iface_on_ap_start(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct netdev_vif *ndev_p2p_vif;

	slsi_stop_p2p_group_iface_on_ap_start(sdev);

	test_netdev_init(test, sdev, SLSI_NET_INDEX_P2PX_SWLAN);
	ndev_p2p_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN]);
	sdev->netdev_ap = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);

	slsi_stop_p2p_group_iface_on_ap_start(sdev);
}

static void test_slsi_mgmt_tx_cancel_wait(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = ndev->ieee80211_ptr;
	u64 cookie = 0x1234;

	kunit_slsi_wq_init(ndev_vif, test);

	ndev_vif->activated = false;
	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	ndev_vif->vif_type = FAPI_VIFTYPE_UNSYNCHRONISED;
	sdev->p2p_state = P2P_ACTION_FRAME_TX_RX;
	ndev_vif->mgmt_tx_data.cookie = cookie;
	sdev->p2p_group_exp_frame = SLSI_P2P_PA_GO_NEG_REQ;
	sdev->wlan_unsync_vif_state = WLAN_UNSYNC_VIF_TX;
	KUNIT_EXPECT_EQ(test, 0, slsi_mgmt_tx_cancel_wait(wiphy, wdev, cookie));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->ifnum = SLSI_NET_INDEX_P2PX_SWLAN;
	sdev->p2p_state = P2P_GROUP_FORMED_GO;
	sdev->p2p_group_exp_frame = SLSI_P2P_PA_GO_NEG_REQ;
	KUNIT_EXPECT_EQ(test, 0, slsi_mgmt_tx_cancel_wait(wiphy, wdev, cookie));

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	sdev->wlan_unsync_vif_state = WLAN_UNSYNC_VIF_TX;
	ndev_vif->mgmt_tx_data.cookie = cookie;
	ndev_vif->mgmt_tx_data.exp_frame = SLSI_P2P_PA_GO_NEG_REQ;
	sdev->p2p_group_exp_frame = SLSI_PA_INVALID;
	KUNIT_EXPECT_EQ(test, 0, slsi_mgmt_tx_cancel_wait(wiphy, wdev, cookie));

	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_P2PX_SWLAN;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->p2p_state = P2P_ACTION_FRAME_TX_RX;
	ndev_vif->mgmt_tx_data.exp_frame = SLSI_P2P_PA_GO_NEG_REQ;
	KUNIT_EXPECT_EQ(test, 0, slsi_mgmt_tx_cancel_wait(wiphy, wdev, cookie));

	kunit_slsi_p2p_deinit(ndev_vif);
}

static void test_slsi_wlan_mgmt_tx(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct ieee80211_channel *chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	struct ieee80211_mgmt *buf;
	u64 cookie;
	size_t len = 0;
	// action[0] : SLSI_PA_GAS_INITIAL_REQ == 10
	// exp_peer_frame : SLSI_PA_INVALID
	u8 wlan_pa_frame[8] = { 0x0A, 0x50, 0x6f, 0x9a, 0x09, 0x00, 0x11, 0x32};

	INIT_DELAYED_WORK(&ndev_vif->unsync.hs2_del_vif_work, kunit_slsi_p2p_roc_duration_expiry_work);

	ndev_vif->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	ndev_vif->chan->hw_value = 149;
	chan->hw_value = 149;

	atomic_set(&sdev->tx_host_tag[0], ((1 << 2) | 5));

	buf = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt) + sizeof(u8) * 10, GFP_KERNEL);
	// management 00 action frame D0 Action frame
	buf->frame_control = 0x00D0;
	memcpy(&buf->u.action.u, wlan_pa_frame, 8);
	ndev_vif->activated = false;
	sdev->device_config.qos_info = 267;
	KUNIT_EXPECT_EQ(test, 0, slsi_wlan_mgmt_tx(sdev, ndev, chan, 100, buf, len, true, &cookie));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_PRECONNECT;
	ndev_vif->driver_channel = 135;
	KUNIT_EXPECT_EQ(test, 0, slsi_wlan_mgmt_tx(sdev, ndev, chan, 100, buf, len, true, &cookie));

	wlan_pa_frame[0] = 0x09;
	// SLSI_P2P_PA_GO_NEG_REQ - subtype
	wlan_pa_frame[5] = SLSI_P2P_PA_GO_NEG_REQ;
	// exp_peer_frame = SLSI_P2P_PA_GO_NEG_RSP
	memcpy(&buf->u.action.u, wlan_pa_frame, 8);
	ndev_vif->mgmt_tx_gas_frame = true;
	KUNIT_EXPECT_EQ(test, 0, slsi_wlan_mgmt_tx(sdev, ndev, chan, 100, buf, len, true, &cookie));

	ndev_vif->driver_channel = 149;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	KUNIT_EXPECT_EQ(test, 0, slsi_wlan_mgmt_tx(sdev, ndev, chan, 100, buf, len, true, &cookie));

	ndev_vif->driver_channel = 1;
	ndev_vif->chan->hw_value = 0;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	KUNIT_EXPECT_EQ(test, 0, slsi_wlan_mgmt_tx(sdev, ndev, chan, 100, buf, len, true, &cookie));

	ndev_vif->activated = false;
	wlan_pa_frame[0] = 0x09;
	// SLSI_P2P_PA_GO_NEG_REQ - subtype
	wlan_pa_frame[5] = SLSI_P2P_PA_GO_NEG_REQ;
	// exp_peer_frame = SLSI_P2P_PA_GO_NEG_RSP
	memcpy(&buf->u.action.u, wlan_pa_frame, 8);
	ndev_vif->mgmt_tx_gas_frame = true;
	KUNIT_EXPECT_EQ(test, 0, slsi_wlan_mgmt_tx(sdev, ndev, chan, 100, buf, len, true, &cookie));

	ndev_vif->activated = true;
	// management 00 action frame D0 Action frame
	buf->frame_control = 0x00B0;
	memcpy(&buf->u.action.u, wlan_pa_frame, 8);
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	KUNIT_EXPECT_EQ(test, 0, slsi_wlan_mgmt_tx(sdev, ndev, chan, 100, buf, len, true, &cookie));

	kfree(ndev_vif->mgmt_tx_data.buf);
}

static void test_slsi_mgmt_tx(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct wireless_dev *wdev = ndev->ieee80211_ptr;
	struct cfg80211_mgmt_tx_params *params;
	struct net_device *p2pdev;
	struct netdev_vif *p2pdev_vif;
	u64 cookie;
	struct ieee80211_mgmt *mgmt = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt) + sizeof(u8) * 10, GFP_KERNEL);
	// action[0] : SLSI_PA_GAS_INITIAL_REQ == 10
	// exp_peer_frame : SLSI_PA_INVALID
	u8 wlan_pa_frame[8] = { 0x0A, 0x50, 0x6f, 0x9a, 0x09, 0x00, 0x11, 0x32};

	test_netdev_init(test, sdev, SLSI_NET_INDEX_P2PX_SWLAN);
	p2pdev = sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN];
	p2pdev_vif = netdev_priv(p2pdev);
	p2pdev_vif->iftype = NL80211_IFTYPE_P2P_GO;

	INIT_DELAYED_WORK(&ndev_vif->unsync.hs2_del_vif_work, kunit_slsi_p2p_roc_duration_expiry_work);

	params = kunit_kzalloc(test, sizeof(struct cfg80211_mgmt_tx_params), GFP_KERNEL);
	params->dont_wait_for_ack = false;
	params->buf = kunit_kzalloc(test, sizeof(*mgmt), GFP_KERNEL);
	params->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	params->offchan = false;
	params->no_cck = false;
	params->wait = false;
	params->len = 5;

	params->dont_wait_for_ack = false;
	sdev->device_config.qos_info = 267;

	ndev_vif->activated = true;
	sdev->device_state = SLSI_DEVICE_STATE_STARTING;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mgmt_tx(wiphy, wdev, params, &cookie));

	sdev->device_state = SLSI_DEVICE_STATE_STARTED;
	// management 00 action frame D0 Action frame X
	mgmt->frame_control = 0x00B4;
	memcpy(&mgmt->u.action.u, wlan_pa_frame, 8);
	params->buf = mgmt;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mgmt_tx(wiphy, wdev, params, &cookie));

	// management 00 action frame D0 Action frame
	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	ndev_vif->iftype = NL80211_IFTYPE_AP;
	sdev->p2p_state = P2P_GROUP_FORMED_CLI;
	sdev->p2p_group_exp_frame = SLSI_P2P_PA_GO_NEG_REQ;
	params->chan->hw_value = 1;
	mgmt->frame_control = 0x00B0;
	wlan_pa_frame[0] = 0x09;
	// SLSI_P2P_PA_GO_NEG_REQ - subtype
	wlan_pa_frame[5] = SLSI_P2P_PA_GO_NEG_REQ;
	// exp_peer_frame = SLSI_P2P_PA_GO_NEG_RSP
	memcpy(&mgmt->u.action.u, wlan_pa_frame, 8);
	params->buf = NULL;
	params->buf = kunit_kzalloc(test, sizeof(*mgmt), GFP_KERNEL);
	params->buf = mgmt;
	KUNIT_EXPECT_EQ(test, 0, slsi_mgmt_tx(wiphy, wdev, params, &cookie));

	mgmt->frame_control = 0x0050;
	KUNIT_EXPECT_EQ(test, 0, slsi_mgmt_tx(wiphy, wdev, params, &cookie));
	// management 00 action frame D0 Action frame

	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	sdev->p2p_state = P2P_IDLE_NO_VIF;
	p2pdev_vif->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	p2pdev_vif->chan->hw_value = 10;
	params->chan->hw_value = 1;
	ndev_vif->activated = true;
	mgmt->frame_control = 0x00D0;
	wlan_pa_frame[0] = 0x09;
	// SLSI_P2P_PA_GO_NEG_REQ - subtype
	wlan_pa_frame[5] = SLSI_P2P_PA_GO_NEG_REQ;
	// exp_peer_frame = SLSI_P2P_PA_GO_NEG_RSP
	memcpy(&mgmt->u.action.u, wlan_pa_frame, 8);
	params->buf = NULL;
	params->buf = kunit_kzalloc(test, sizeof(*mgmt), GFP_KERNEL);
	params->buf = mgmt;
	KUNIT_EXPECT_EQ(test, 0, slsi_mgmt_tx(wiphy, wdev, params, &cookie));

	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	sdev->p2p_state = P2P_GROUP_FORMED_CLI;
	p2pdev_vif->chan->hw_value = 10;
	params->chan->hw_value = 1;

	mgmt->frame_control = 0x00D0;
	wlan_pa_frame[0] = 0x09;
	// SLSI_P2P_PA_GO_NEG_REQ - subtype
	wlan_pa_frame[5] = SLSI_P2P_PA_GO_NEG_REQ;
	// exp_peer_frame = SLSI_P2P_PA_GO_NEG_RSP
	memcpy(&mgmt->u.action.u, wlan_pa_frame, 8);
	params->buf = NULL;
	params->buf = kunit_kzalloc(test, sizeof(*mgmt), GFP_KERNEL);
	params->buf = mgmt;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mgmt_tx(wiphy, wdev, params, &cookie));

	params->chan->hw_value = 10;
	ndev_vif->ifnum = SLSI_NET_INDEX_P2PX_SWLAN;
	ndev_vif->chan->hw_value = 10;
	sdev->p2p_state = P2P_IDLE_NO_VIF;
	params->len = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_mgmt_tx(wiphy, wdev, params, &cookie));

	sdev->device_config.qos_info = -267;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mgmt_tx(wiphy, wdev, params, &cookie));

	kfree(p2pdev_vif->mgmt_tx_data.buf);
}

static void test_slsi_slsi_p2p_group_mgmt_tx(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct netdev_vif *p2pdev_vif;
	struct ieee80211_channel *chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	struct ieee80211_mgmt *mgmt = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt) + sizeof(u8) * 8, GFP_KERNEL);
	u64 cookie;
	// action[0] : SLSI_PA_GAS_INITIAL_REQ == 10
	// exp_peer_frame : SLSI_PA_INVALID
	u8 p2p_pa_frame[6] = { 0x09, 0x50, 0x6f, 0x9a, 0x09, 0x00 };

	mgmt->frame_control = 0x00D0;
	p2p_pa_frame[0] = 0x09;
	// SLSI_P2P_PA_GO_NEG_REQ - subtype
	p2p_pa_frame[5] = SLSI_P2P_PA_GO_NEG_REQ;
	// exp_peer_frame = SLSI_P2P_PA_GO_NEG_RSP
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 8);

	sdev->device_config.qos_info = 267;
	sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN] = kunit_kzalloc(test, sizeof(struct net_device) +
								sizeof(struct netdev_vif), GFP_KERNEL);
	p2pdev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN]);
	p2pdev_vif->chan = NULL;
	sdev->p2p_group_exp_frame = SLSI_PA_INVALID;

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_p2p_group_mgmt_tx(mgmt, wiphy, ndev, chan, 100, 0, 0, false, &cookie));

	p2pdev_vif->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	p2pdev_vif->chan->hw_value = 1;
	p2pdev_vif->activated = true;
	p2pdev_vif->ifnum = SLSI_NET_INDEX_P2PX_SWLAN;
	p2pdev_vif->iftype = NL80211_IFTYPE_P2P_CLIENT;
	chan->hw_value = 1;

	KUNIT_EXPECT_EQ(test, 0,
			slsi_p2p_group_mgmt_tx(mgmt, wiphy, ndev, chan, 100, 0, 0, false, &cookie));
	kfree(p2pdev_vif->mgmt_tx_data.buf);
}

static void test_slsi_p2p_mgmt_tx(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct ieee80211_mgmt *mgmt = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt) + sizeof(u8) * 10, GFP_KERNEL);
	struct ieee80211_channel *chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	u64 cookie;

	// action[0] : SLSI_PA_GAS_INITIAL_REQ == 10
	// exp_peer_frame : SLSI_PA_INVALID
	u8 p2p_pa_frame[6] = { 0x09, 0x50, 0x6f, 0x9a, 0x09, 0x00 };

	ndev_vif->sdev = 0;
	kunit_slsi_wq_init(ndev_vif, test);

	mgmt->frame_control = 0x00D0;
	p2p_pa_frame[0] = 0x09;
	// SLSI_P2P_PA_GO_NEG_REQ - subtype
	p2p_pa_frame[5] = SLSI_PA_INVALID;
	// exp_peer_frame = SLSI_P2P_PA_GO_NEG_RSP
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 6);

	sdev->device_config.qos_info = 267;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_p2p_mgmt_tx(mgmt, wiphy, ndev, ndev_vif, chan, 100, 0, 0, false, &cookie));

	sdev->p2p_state = P2P_GROUP_FORMED_CLI;
	chan->hw_value = 11;
	ndev_vif->driver_channel = 11;
	ndev_vif->mgmt_tx_data.exp_frame = SLSI_P2P_PA_GO_NEG_CFM;
	p2p_pa_frame[5] = SLSI_P2P_PA_GO_NEG_RSP;
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 6);

	KUNIT_EXPECT_EQ(test, 0,
			slsi_p2p_mgmt_tx(mgmt, wiphy, ndev, ndev_vif, chan, 100, 0, 0, false, &cookie));

	chan->hw_value = 11;
	ndev_vif->driver_channel = 9;
	ndev_vif->mgmt_tx_data.exp_frame = SLSI_P2P_PA_GO_NEG_CFM;
	p2p_pa_frame[5] = SLSI_P2P_PA_INV_REQ;
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 6);
	ndev_vif->unsync.probe_rsp_ies_len = 3;
	ndev_vif->unsync.probe_rsp_ies = kmalloc(20, GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0,
			slsi_p2p_mgmt_tx(mgmt, wiphy, ndev, ndev_vif, chan, 100, 0, 0, false, &cookie));

	p2p_pa_frame[5] = SLSI_P2P_PA_GO_NEG_CFM;
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 6);
	ndev_vif->mgmt_tx_data.host_tag = 0;

	KUNIT_EXPECT_EQ(test, 0,
			slsi_p2p_mgmt_tx(mgmt, wiphy, ndev, ndev_vif, chan, 100, 0, 0, false, &cookie));

	p2p_pa_frame[5] = SLSI_P2P_PA_DEV_DISC_REQ;
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 6);
	ndev_vif->mgmt_tx_data.host_tag = 0;

	KUNIT_EXPECT_EQ(test, 0,
			slsi_p2p_mgmt_tx(mgmt, wiphy, ndev, ndev_vif, chan, 100, 0, 0, false, &cookie));

	p2p_pa_frame[5] = SLSI_P2P_PA_DEV_DISC_RSP;
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 6);
	ndev_vif->mgmt_tx_data.host_tag = 0;

	KUNIT_EXPECT_EQ(test, 0,
			slsi_p2p_mgmt_tx(mgmt, wiphy, ndev, ndev_vif, chan, 100, 0, 0, false, &cookie));

	p2p_pa_frame[5] = SLSI_P2P_PA_PROV_DISC_REQ;
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 6);
	ndev_vif->mgmt_tx_data.host_tag = 0;

	KUNIT_EXPECT_EQ(test, 0,
			slsi_p2p_mgmt_tx(mgmt, wiphy, ndev, ndev_vif, chan, 100, 0, 0, false, &cookie));

	p2p_pa_frame[5] = SLSI_P2P_PA_PROV_DISC_RSP;
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 6);
	ndev_vif->mgmt_tx_data.host_tag = 0;

	KUNIT_EXPECT_EQ(test, 0,
			slsi_p2p_mgmt_tx(mgmt, wiphy, ndev, ndev_vif, chan, 100, 0, 0, false, &cookie));

	p2p_pa_frame[5] = SLSI_PA_GAS_INITIAL_REQ_SUBTYPE;
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 6);
	ndev_vif->mgmt_tx_data.host_tag = 0;

	KUNIT_EXPECT_EQ(test, 0,
			slsi_p2p_mgmt_tx(mgmt, wiphy, ndev, ndev_vif, chan, 100, 0, 0, false, &cookie));

	p2p_pa_frame[5] = SLSI_PA_GAS_INITIAL_RSP_SUBTYPE;
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 6);
	ndev_vif->mgmt_tx_data.host_tag = 0;

	KUNIT_EXPECT_EQ(test, 0,
			slsi_p2p_mgmt_tx(mgmt, wiphy, ndev, ndev_vif, chan, 100, 0, 0, false, &cookie));

	p2p_pa_frame[5] = SLSI_PA_GAS_COMEBACK_REQ_SUBTYPE;
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 6);
	ndev_vif->mgmt_tx_data.host_tag = 0;

	KUNIT_EXPECT_EQ(test, 0,
			slsi_p2p_mgmt_tx(mgmt, wiphy, ndev, ndev_vif, chan, 100, 0, 0, false, &cookie));

	p2p_pa_frame[5] = SLSI_PA_GAS_COMEBACK_RSP_SUBTYPE;
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 6);
	ndev_vif->mgmt_tx_data.host_tag = 0;

	KUNIT_EXPECT_EQ(test, 0,
			slsi_p2p_mgmt_tx(mgmt, wiphy, ndev, ndev_vif, chan, 100, 0, 0, false, &cookie));

	p2p_pa_frame[5] = SLSI_PA_GAS_INITIAL_REQ_SUBTYPE + 10;
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 6);
	ndev_vif->mgmt_tx_data.host_tag = 0;

	KUNIT_EXPECT_EQ(test, 0,
			slsi_p2p_mgmt_tx(mgmt, wiphy, ndev, ndev_vif, chan, 100, 0, 0, false, &cookie));

	mgmt->frame_control = 0x00BC;
	p2p_pa_frame[0] = 0x09;
	// SLSI_P2P_PA_GO_NEG_REQ - subtype
	p2p_pa_frame[5] = SLSI_PA_INVALID;
	// exp_peer_frame = SLSI_P2P_PA_GO_NEG_RSP
	memcpy(&mgmt->u.action.u, p2p_pa_frame, 6);

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_p2p_mgmt_tx(mgmt, wiphy, ndev, ndev_vif, chan, 100, 0, 0, false, &cookie));

	kunit_slsi_p2p_deinit(ndev_vif);

	kfree(ndev_vif->mgmt_tx_data.buf);
	kfree(ndev_vif->unsync.probe_rsp_ies);
}

static void test_slsi_set_txq_params(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct ieee80211_txq_params *params = kunit_kzalloc(test, sizeof(struct ieee80211_txq_params), GFP_KERNEL);

	params->ac = 3;
	params->aifs = 2;
	params->cwmin = 3;
	params->cwmax = 4;
	params->txop = 5;
	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;

	ndev_vif->activated = true;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_txq_params(wiphy, ndev, params));
	KUNIT_EXPECT_EQ(test, 3, slsi_get_ecw(10));
}

static void test_slsi_synchronised_response(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct cfg80211_external_auth_params *params;

	params = kunit_kzalloc(test, sizeof(struct cfg80211_external_auth_params), GFP_KERNEL);

	ndev_vif->sta.wpa3_sae_reconnection = true;
	SLSI_ETHER_COPY(params->bssid, SLSI_DEFAULT_HW_MAC_ADDR);
	SLSI_ETHER_COPY(ndev_vif->sta.bssid, SLSI_DEFAULT_HW_MAC_ADDR);

	KUNIT_EXPECT_EQ(test, 0, slsi_synchronised_response(wiphy, ndev, params));

	SLSI_ETHER_COPY(params->bssid, SLSI_DEFAULT_HW_MAC_ADDR);
	SLSI_ETHER_COPY(ndev_vif->sta.bssid, SLSI_DEFAULT_BSSID);

	ndev_vif->sta.wpa3_sae_reconnection = true;
	KUNIT_EXPECT_EQ(test, 1, slsi_synchronised_response(wiphy, ndev, params));
}

static void test_slsi_set_mac_acl(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct cfg80211_acl_data *params = kunit_kzalloc(test, sizeof(struct cfg80211_acl_data) +
							 sizeof(struct mac_address) * 255, GFP_KERNEL);

	params->acl_policy = NL80211_ACL_POLICY_DENY_UNLESS_LISTED;
	params->n_acl_entries = 54;
	SLSI_ETHER_COPY(params->mac_addrs[0].addr, SLSI_DEFAULT_HW_MAC_ADDR);

	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_mac_acl(wiphy, ndev, (const struct cfg80211_acl_data *)params));

	ndev_vif->ifnum = SLSI_NET_INDEX_P2PX_SWLAN;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_mac_acl(wiphy, ndev, (const struct cfg80211_acl_data *)params));

	params->n_acl_entries = -54;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_mac_acl(wiphy, ndev, (const struct cfg80211_acl_data *)params));
}

static void test_slsi_cfg80211_new(struct kunit *test)
{
	struct net_device *ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	struct slsi_dev *sdev;

	sdev = slsi_cfg80211_new(ndev);
	KUNIT_EXPECT_EQ(test, 0, slsi_cfg80211_register(sdev));

	sdev->band_5g_supported = true;
	sdev->fw_ht_enabled = true;
	slsi_cfg80211_update_wiphy(sdev);

	sdev->band_5g_supported = false;
	sdev->fw_ht_enabled = false;
	slsi_cfg80211_update_wiphy(sdev);

	sdev->fw_vht_enabled = true;
	slsi_cfg80211_update_wiphy(sdev);

	slsi_cfg80211_unregister(sdev);
	slsi_cfg80211_free(sdev);
}

static void test_slsi_update_ft_ies(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct cfg80211_update_ft_ies_params *ftie;
	u8 *ie = "\xdd\x05\x00\x16\x32\x80\x00\xdd\x04\x00";

	ftie = kunit_kzalloc(test, sizeof(struct cfg80211_update_ft_ies_params) + sizeof(u8) * 20, GFP_KERNEL);
	ftie->ie_len = 10;
	ftie->ie = ie;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;

	ndev_vif->sta.assoc_req_add_info_elem = "\xdd\x05\x00\x16\x32\x80\x00\xdd\x04\x00\x00\xf0\x22";
	ndev_vif->sta.assoc_req_add_info_elem_len = 13;

	KUNIT_EXPECT_EQ(test, 0, slsi_update_ft_ies(wiphy, ndev, ftie));
}

static void test_slsi_ap_chandef_vht_ht(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct ieee80211_regdomain *regd = kunit_kzalloc(test, sizeof(struct ieee80211_regdomain) +
							 sizeof(struct ieee80211_reg_rule) * 10, GFP_KERNEL);
#ifdef CONFIG_SCSC_WLAN_WIFI_SHARING
	int wifi_sharing_channel_switched;
#endif

	ndev_vif->chandef = kunit_kzalloc(test, sizeof(struct cfg80211_chan_def), GFP_KERNEL);
	ndev_vif->chandef->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	ndev_vif->chandef->chan->center_freq = 5570;
	ndev_vif->chandef->chan->freq_offset = 0;
	ndev_vif->chandef->chan->hw_value = 114;

	sdev->device_config.domain_info.regdomain = regd;
	regd->n_reg_rules = 1;
	regd->dfs_region = 0;
	regd->reg_rules[0].freq_range.end_freq_khz = 5650000;
	regd->reg_rules[0].freq_range.start_freq_khz = 5490000;
	regd->reg_rules[0].freq_range.max_bandwidth_khz = 1600000;

	sdev->fw_vht_enabled = true;
	sdev->allow_switch_160_mhz = true;
	sdev->fw_vht_cap[0] = 4;
	sdev->fw_vht_cap[1] = 0;
	sdev->fw_vht_cap[2] = 0;
	sdev->fw_vht_cap[3] = 0;
	KUNIT_EXPECT_TRUE(test, slsi_ap_chandef_vht_ht(ndev, sdev));

	regd->reg_rules[0].freq_range.end_freq_khz = 5330000;
	regd->reg_rules[0].freq_range.start_freq_khz = 5170000;
	regd->reg_rules[0].freq_range.max_bandwidth_khz = 160000;

	ndev_vif->chandef->chan->center_freq = 5250;
	ndev_vif->chandef->chan->freq_offset = 0;
	ndev_vif->chandef->chan->hw_value = 50;
	KUNIT_EXPECT_TRUE(test, slsi_ap_chandef_vht_ht(ndev, sdev));

	regd->reg_rules[0].freq_range.start_freq_khz = 5735000;
	regd->reg_rules[0].freq_range.end_freq_khz = 5815000;
	regd->reg_rules[0].freq_range.max_bandwidth_khz = 80000;

	ndev_vif->chandef->chan->hw_value = 155;
	ndev_vif->chandef->chan->center_freq = 5775;

	sdev->allow_switch_160_mhz = false;
	sdev->allow_switch_80_mhz = true;
	sdev->fw_vht_cap[0] = 3;
	sdev->fw_vht_cap[1] = 0;
	sdev->fw_vht_cap[2] = 0;
	sdev->fw_vht_cap[3] = 0;
	KUNIT_EXPECT_TRUE(test, slsi_ap_chandef_vht_ht(ndev, sdev));

	regd->reg_rules[0].freq_range.start_freq_khz = 5170000;
	regd->reg_rules[0].freq_range.end_freq_khz = 5250000;
	regd->reg_rules[0].freq_range.max_bandwidth_khz = 80000;

	ndev_vif->chandef->chan->hw_value = 42;
	ndev_vif->chandef->chan->center_freq = 5210;
	KUNIT_EXPECT_TRUE(test, slsi_ap_chandef_vht_ht(ndev, sdev));

	sdev->fw_vht_enabled = false;
	sdev->allow_switch_80_mhz = false;
	sdev->allow_switch_40_mhz = true;
	sdev->fw_ht_enabled = true;
	sdev->fw_ht_cap[0] = IEEE80211_HT_CAP_SUP_WIDTH_20_40;

	regd->reg_rules[0].freq_range.start_freq_khz = 5230000;
	regd->reg_rules[0].freq_range.end_freq_khz = 5250000;
	regd->reg_rules[0].freq_range.max_bandwidth_khz = 40000;

	ndev_vif->chandef->chan->hw_value = 48;
	ndev_vif->chandef->chan->center_freq = 5240;
	KUNIT_EXPECT_FALSE(test, slsi_ap_chandef_vht_ht(ndev, sdev));
}

static void test_slsi_start_ap(struct kunit *test)
{
	struct net_device *ndev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(ndev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct wiphy *wiphy = sdev->wiphy;
	struct cfg80211_ap_settings *settings;
	struct netdev_vif *wlan_vif;
	struct ieee80211_mgmt *mgmt;

	settings = kunit_kzalloc(test, sizeof(struct cfg80211_ap_settings), GFP_KERNEL);
	settings->chandef.chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	settings->chandef.chan->center_freq = 5745;
	settings->chandef.width = 40;
	settings->chandef.chan->band = NL80211_BAND_5GHZ;
	settings->beacon.head = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt) + sizeof(u8) * 2, GFP_KERNEL);
	mgmt = settings->beacon.head;
	settings->beacon.head_len = 21;
	memset(&mgmt->u.beacon.variable, 0, 21);
	memcpy(&mgmt->u.beacon.variable, "\x00\x0c\x43\x54\x35\x35\x30\x38\x5f\x31\x58\x5f\x46\x54", 15);

	settings->beacon.tail_len = 27;
	settings->beacon.tail = "\x00\x0c\x75\x72\x65\x61\x64\x79\x74\x68\x69\x6e\x67\x73\x01\x05\x98\xb0\x48\x60\x6c\x05\x04\x00\x03\x00";

	settings->beacon.head_len = 0;
	settings->beacon.beacon_ies_len = 0;
	settings->beacon.proberesp_ies_len = 0;
	settings->beacon.assocresp_ies_len = 0;
	settings->beacon.probe_resp_len = 0;
	settings->ht_cap = NULL;
	settings->vht_cap = NULL;

	ndev_vif->chandef = kunit_kzalloc(test, sizeof(struct cfg80211_chan_def), GFP_KERNEL);
	ndev_vif->chandef->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);

	ndev_vif->ifnum = SLSI_NET_INDEX_P2PX_SWLAN;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_GO;
	ndev_vif->chandef->width = NL80211_CHAN_WIDTH_20;
	ndev_vif->chandef->chan->hw_value = 10;
	sdev->fw_ht_cap[0] = IEEE80211_HT_CAP_LDPC_CODING | IEEE80211_HT_CAP_SGI_20;
	sdev->fw_ht_cap[1] = IEEE80211_HT_CAP_LDPC_CODING | IEEE80211_HT_CAP_SGI_20;
	settings->ssid = "KUNIT_SSID";
	settings->ssid_len = 11;
	settings->beacon_interval = 100;

	test_netdev_init(test, sdev, SLSI_NET_INDEX_P2P);
	sdev->netdev_ap = sdev->netdev[SLSI_NET_INDEX_P2P];
	sdev->netdev[SLSI_NET_INDEX_P2P]->dev_addr = SLSI_DEFAULT_HW_MAC_ADDR;
	sdev->device_config.domain_info.regdomain = kunit_kzalloc(test, sizeof(struct ieee80211_regdomain) +
								  sizeof(struct ieee80211_reg_rule), GFP_KERNEL);
	memcpy(sdev->device_config.domain_info.regdomain->alpha2, "KR", 2);
	sdev->wlan_unsync_vif_state = WLAN_UNSYNC_VIF_ACTIVE;
	sdev->device_state = SLSI_DEVICE_STATE_STARTING;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_start_ap(wiphy, ndev, settings));

	settings->chandef.chan->center_freq = 2437;
	settings->chandef.width = 20;
	settings->chandef.chan->band = NL80211_BAND_2GHZ;
	sdev->device_state = SLSI_DEVICE_STATE_STARTED;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_start_ap(wiphy, ndev, settings));

	ndev_vif->iftype = NL80211_IFTYPE_P2P_GO;
	settings->beacon.head = "\x32";
	settings->beacon.head_len = 2;
	settings->beacon.beacon_ies = "\x32";
	settings->beacon.beacon_ies_len = 2;
	ndev_vif->chandef->width = NL80211_CHAN_WIDTH_20;
	ndev_vif->ap.add_info_ies_len = 548;
	sdev->device_config.supported_roam_band = 522;
	ndev_vif->activated = false;
	settings->chandef.chan->band = NL80211_BAND_5GHZ;
	settings->beacon.proberesp_ies_len = 0;
	settings->beacon.assocresp_ies_len = 0;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_start_ap(wiphy, ndev, settings));

	ndev_vif->iftype = NL80211_IFTYPE_P2P_GO;
	settings->beacon.head = "\x32";
	settings->beacon.head_len = 2;
	settings->beacon.beacon_ies = "\x32";
	settings->beacon.beacon_ies_len = 2;
	ndev_vif->chandef->width = NL80211_CHAN_WIDTH_20;
	ndev_vif->ap.add_info_ies_len = 548;
	sdev->device_config.supported_roam_band = 0;
	ndev_vif->activated = false;
	settings->chandef.chan->band = NL80211_BAND_2GHZ;
	settings->beacon.proberesp_ies_len = 0;
	settings->beacon.assocresp_ies_len = 0;

	KUNIT_EXPECT_EQ(test, 0, slsi_start_ap(wiphy, ndev, settings));

	ndev_vif->iftype = NL80211_IFTYPE_AP;
	settings->beacon.head = "\x32";
	settings->beacon.head_len = 2;
	settings->beacon.beacon_ies = "\x32";
	settings->beacon.beacon_ies_len = 2;
	ndev_vif->chandef->width = NL80211_CHAN_WIDTH_20;
	ndev_vif->ap.add_info_ies_len = 548;
	sdev->device_config.supported_roam_band = 522;
	ndev_vif->activated = false;
	settings->chandef.chan->band = NL80211_BAND_6GHZ;
	settings->beacon.proberesp_ies_len = 0;
	settings->beacon.assocresp_ies_len = 0;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_start_ap(wiphy, ndev, settings));

	settings->chandef.chan->center_freq = 6055;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_start_ap(wiphy, ndev, settings));

	kfree(ndev_vif->ap.cache_wmm_ie);
	kfree(ndev_vif->backup_settings.chandef.chan);
	kfree(ndev_vif->backup_settings.beacon.head);
	kfree(ndev_vif->backup_settings.beacon.tail);
	kfree(ndev_vif->backup_settings.beacon.beacon_ies);
	kfree(ndev_vif->backup_settings.ssid);
	kunit_kfree(test, settings);
}

static void test_slsi_update_connect_params(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct wiphy *wiphy = sdev->wiphy;

	KUNIT_EXPECT_EQ(test, 0, slsi_update_connect_params(wiphy, sdev, 0, 0));
}


/*
 * Test fictures
 */
static int cfg80211_ops_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void cfg80211_ops_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
	return;
}

/*
 * KUnit testcase definitions
 */
static struct kunit_case cfg80211_ops_test_cases[] = {
	KUNIT_CASE(test_slsi_add_virtual_intf),
	KUNIT_CASE(test_slsi_del_virtual_intf),
	KUNIT_CASE(test_slsi_change_virtual_intf),
	KUNIT_CASE(test_slsi_add_key),
	KUNIT_CASE(test_slsi_channel_switch),
	KUNIT_CASE(test_slsi_get_key),
	KUNIT_CASE(test_slsi_abort_scan),
	KUNIT_CASE(test_slsi_scan),
	KUNIT_CASE(test_slsi_sched_scan_start),
	KUNIT_CASE(test_slsi_sched_scan_stop),
	KUNIT_CASE(test_slsi_save_connection_params),
	KUNIT_CASE(test_slsi_set_params_on_bss),
	KUNIT_CASE(test_slsi_check_wificonnect_to_connectedGo),
	KUNIT_CASE(test_slsi_set_roam_reassoc),
	KUNIT_CASE(test_slsi_check_valid_netdev_vif_state),
	KUNIT_CASE(test_slsi_set_bmap),
	KUNIT_CASE(test_slsi_set_sta_bss_info),
	KUNIT_CASE(test_slsi_config_rsn_ie),
	KUNIT_CASE(test_slsi_connect),
	KUNIT_CASE(test_slsi_disconnect),
	KUNIT_CASE(test_slsi_set_wiphy_params),
	KUNIT_CASE(test_slsi_not_supported_func),
	KUNIT_CASE(test_slsi_get_channel),
	KUNIT_CASE(test_slsi_del_station),
	KUNIT_CASE(test_slsi_get_station),
	KUNIT_CASE(test_slsi_set_power_mgmt),
	KUNIT_CASE(test_slsi_set_qos_map),
	KUNIT_CASE(test_slsi_set_monitor_channel),
	KUNIT_CASE(test_slsi_set_pmksa),
	KUNIT_CASE(test_slsi_del_pmksa),
	KUNIT_CASE(test_slsi_remain_on_channel),
	KUNIT_CASE(test_slsi_cancel_remain_on_channel),
	KUNIT_CASE(test_slsi_store_settings_for_recovery),
	KUNIT_CASE(test_slsi_get_max_bw_mhz),
	KUNIT_CASE(test_slsi_ap_start_validate),
	KUNIT_CASE(test_slsi_configure_country_code),
	KUNIT_CASE(test_slsi_ap_start_get_indoor_chan),
	KUNIT_CASE(test_slsi_modify_ht_capa_ies),
	KUNIT_CASE(test_slsi_ap_start_obss_scan),
	KUNIT_CASE(test_slsi_set_ap_mode),
	KUNIT_CASE(test_slsi_stop_p2p_group_iface_on_ap_start),
	KUNIT_CASE(test_slsi_mgmt_tx_cancel_wait),
	KUNIT_CASE(test_slsi_wlan_mgmt_tx),
	KUNIT_CASE(test_slsi_mgmt_tx),
	KUNIT_CASE(test_slsi_slsi_p2p_group_mgmt_tx),
	KUNIT_CASE(test_slsi_p2p_mgmt_tx),
	KUNIT_CASE(test_slsi_set_txq_params),
	KUNIT_CASE(test_slsi_synchronised_response),
	KUNIT_CASE(test_slsi_cfg80211_new),
	KUNIT_CASE(test_slsi_set_mac_acl),
	KUNIT_CASE(test_slsi_update_ft_ies),
	KUNIT_CASE(test_slsi_start_ap),
	KUNIT_CASE(test_slsi_ap_chandef_vht_ht),
	KUNIT_CASE(test_slsi_update_connect_params),
	{}
};

static struct kunit_suite cfg80211_ops_test_suite[] = {
	{
		.name = "kunit-cfg80211-ops-test",
		.test_cases = cfg80211_ops_test_cases,
		.init = cfg80211_ops_test_init,
		.exit = cfg80211_ops_test_exit,
	}
};

kunit_test_suites(cfg80211_ops_test_suite);
