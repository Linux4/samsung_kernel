#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-scsc_wifi_fcq.h"
#include "kunit-mock-nl80211_vendor.h"
#include "kunit-mock-nl80211_vendor_nan.h"
#include "kunit-mock-txbp.h"
#include "kunit-mock-mlme.h"
#include "kunit-mock-rx.h"
#include "../sap_mlme.c"

#define SLSI_DEFAULT_HW_MAC_ADDR	"\x00\x00\x0F\x11\x22\x33"

static void test_sap_mlme_init(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_init());
}

static void test_sap_mlme_deinit(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_deinit());
}

static void test_slsi_get_fapi_version_string(struct kunit *test)
{
	char info[100];

	slsi_get_fapi_version_string(info);
}

static void test_sap_mlme_notifier(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct netdev_vif *ndev_vif;
	unsigned long event = SCSC_WIFI_STOP;

	sdev->netdev[SLSI_NET_INDEX_AP] = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif),
							GFP_KERNEL);
	ndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_AP]);
	atomic_set(&sdev->cm_if.reset_level, SCSC_WIFI_CM_IF_STATE_STOPPED);
#if !defined(CONFIG_SCSC_WLAN_TX_API) && defined(CONFIG_SCSC_WLAN_ARP_FLOW_CONTROL)
	atomic_set(&ndev_vif->arp_tx_count, 1);
	atomic_set(&sdev->ctrl_pause_state, 1);
#endif
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->ifnum = SLSI_NET_INDEX_AP;

	KUNIT_EXPECT_EQ(test, 0, sap_mlme_notifier(sdev, event));

	event = SCSC_WIFI_FAILURE_RESET;
	atomic_set(&sdev->cm_if.reset_level, SCSC_WIFI_CM_IF_STATE_STOPPED);
	sdev->require_service_close = true;

	KUNIT_EXPECT_EQ(test, 0, sap_mlme_notifier(sdev, event));

	event = SCSC_WIFI_SUSPEND;
	ndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_WLAN]);
	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->power_mode = FAPI_POWERMANAGEMENTMODE_ACTIVE_MODE;
	sdev->device_config.user_suspend_mode = false;
	sdev->device_config.host_state |= SLSI_HOSTSTATE_LCD_ACTIVE;

	KUNIT_EXPECT_EQ(test, 0, sap_mlme_notifier(sdev, event));

	event = SCSC_WIFI_RESUME;
#if defined(CONFIG_SLSI_WLAN_STA_FWD_BEACON) && (defined(SCSC_SEP_VERSION) && SCSC_SEP_VERSION >= 10)
	ndev_vif->is_wips_running = true;
#endif
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_notifier(sdev, event));

	event = SCSC_WIFI_SUBSYSTEM_RESET;
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_notifier(sdev, event));

	event = SCSC_WIFI_CHIP_READY;
	atomic_set(&sdev->cm_if.reset_level, SCSC_WIFI_CM_IF_STATE_STOPPED);
	sdev->netdev_up_count = 1;

	KUNIT_EXPECT_EQ(test, 0, sap_mlme_notifier(sdev, event));

	KUNIT_EXPECT_EQ(test, -EIO, sap_mlme_notifier(sdev, SCSC_WIFI_CHIP_READY + 10));
}

static void test_sap_mlme_version_supported(struct kunit *test)
{
	u16 version = 0x1234;

	KUNIT_EXPECT_EQ(test, -EINVAL, sap_mlme_version_supported(version));

	version = FAPI_CONTROL_SAP_VERSION;

	KUNIT_EXPECT_EQ(test, 0, sap_mlme_version_supported(version));
}

static void test_slsi_rx_netdev_mlme(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb;

	skb = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_scan_done_ind, MLME_SCAN_DONE_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_connect_ind, MLME_CONNECT_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_connected_ind, MLME_CONNECTED_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_disconnected_ind, MLME_DISCONNECTED_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_procedure_started_ind, MLME_PROCEDURE_STARTED_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_frame_transmission_ind, MLME_FRAME_TRANSMISSION_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_blockack_action_ind, MLME_BLOCKACK_ACTION_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_roamed_ind, MLME_ROAMED_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_roam_ind, MLME_ROAM_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_mic_failure_ind, MLME_MIC_FAILURE_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_reassociate_ind, MLME_REASSOCIATE_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_listen_end_ind, MLME_LISTEN_END_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_channel_switched_ind, MLME_CHANNEL_SWITCHED_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_ac_priority_update_ind, MLME_AC_PRIORITY_UPDATE_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_rssi_report_ind, MLME_RSSI_REPORT_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_range_ind, MLME_RANGE_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_range_done_ind, MLME_RANGE_DONE_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_event_log_ind, MLME_EVENT_LOG_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_synchronised_ind, MLME_SYNCHRONISED_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_beacon_reporting_event_ind, MLME_BEACON_REPORTING_EVENT_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_blacklisted_ind, MLME_BLACKLISTED_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_roaming_channel_list_ind, MLME_ROAMING_CHANNEL_LIST_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_send_frame_cfm, MLME_SEND_FRAME_CFM, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_start_detect_ind, MLME_START_DETECT_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_twt_setup_ind, MLME_TWT_SETUP_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_twt_teardown_ind, MLME_TWT_TEARDOWN_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_twt_notify_ind, MLME_TWT_NOTIFY_IND, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_spare_signal_3_req, MLME_SPARE_SIGNAL_3_REQ, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));

	skb = fapi_alloc(mlme_spare_signal_3_req, 0xA001, 0, 1);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_netdev_mlme(sdev, dev, skb));
}

static void test_slsi_rx_netdev_mlme_work(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_skb_work *work = kunit_kzalloc(test, sizeof(struct slsi_skb_work), GFP_KERNEL);
	struct sk_buff *skb = fapi_alloc(mlme_twt_notify_ind, MLME_TWT_NOTIFY_IND, 0, 1);

	slsi_skb_work_init(sdev, dev, work, "kunit_wlan_driver", slsi_rx_netdev_mlme_work);

	slsi_skb_work_enqueue(work, skb);

	slsi_rx_netdev_mlme_work(&work->work);

	slsi_skb_work_deinit(work);
}

static void test_slsi_rx_enqueue_netdev_mlme(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb = fapi_alloc(mlme_twt_notify_ind, MLME_TWT_NOTIFY_IND, 0, 1);
	u16 vif = SLSI_NET_INDEX_WLAN;
	struct netdev_vif *ndev_vif;

	KUNIT_EXPECT_EQ(test, 0, slsi_rx_enqueue_netdev_mlme(sdev, skb, vif));

	skb = fapi_alloc(mlme_twt_notify_ind, MLME_TWT_NOTIFY_IND, 0, 1);
	ndev_vif = netdev_priv(sdev->netdev[vif]);
	ndev_vif->is_fw_test = false;

	KUNIT_EXPECT_EQ(test, 0, slsi_rx_enqueue_netdev_mlme(sdev, skb, vif));

	skb = fapi_alloc(mlme_twt_notify_ind, MLME_TWT_NOTIFY_IND, 0, 1);
	ndev_vif->is_fw_test = true;

	KUNIT_EXPECT_EQ(test, 0, slsi_rx_enqueue_netdev_mlme(sdev, skb, vif));
}

static void test_slsi_rx_action_enqueue_netdev_mlme(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = alloc_skb(100, GFP_KERNEL);
	struct ieee80211_mgmt *mgmt = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt), GFP_KERNEL);
	struct net_device *p2p_dev;
	struct netdev_vif *ndev_p2p_vif;
	u16 vif = SLSI_NET_INDEX_WLAN;

	sdev->netdev[vif] = NULL;
	KUNIT_EXPECT_EQ(test, -ENODEV, slsi_rx_action_enqueue_netdev_mlme(sdev, skb, vif));

	sdev->netdev[vif] = dev;
	ndev_vif->is_fw_test = true;
	dev->dev_addr = "\x01\x02\xDF\x1E\xB2\x39";
	SLSI_ETHER_COPY(mgmt->da, SLSI_DEFAULT_HW_MAC_ADDR);
	memcpy(skb->data, mgmt, 20);

	KUNIT_EXPECT_EQ(test, 0, slsi_rx_action_enqueue_netdev_mlme(sdev, skb, vif));

	ndev_vif->is_fw_test = false;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_GO;

	KUNIT_EXPECT_EQ(test, -ENODEV, slsi_rx_action_enqueue_netdev_mlme(sdev, skb, vif));

	p2p_dev = test_netdev_init(test, sdev, SLSI_NET_INDEX_P2P);
	ndev_p2p_vif = netdev_priv(p2p_dev);
	ndev_p2p_vif->is_fw_test = true;
	p2p_dev->dev_addr = SLSI_DEFAULT_HW_MAC_ADDR;

	KUNIT_EXPECT_EQ(test, 0, slsi_rx_action_enqueue_netdev_mlme(sdev, skb, vif));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	skb = alloc_skb(100, GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, slsi_rx_action_enqueue_netdev_mlme(sdev, skb, vif));
}

static void test_sap_mlme_rx_handler(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb;

	sdev->netdev[SLSI_NET_INDEX_WLAN]  = NULL;

	sdev->device_config.qos_info = 234;

	skb = fapi_alloc(mlme_scan_done_ind, MLME_SCAN_DONE_IND, 0, 10);
	fapi_set_u16(skb, u.mlme_scan_done_ind.scan_id, SLSI_GSCAN_SCAN_ID_END);
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_scan_done_ind, MLME_SCAN_DONE_IND, 0, 10);
	fapi_set_u16(skb, u.mlme_scan_done_ind.scan_id, 0x0010);
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, 0, 10);
	fapi_set_u16(skb, u.mlme_scan_ind.scan_id, SLSI_GSCAN_SCAN_ID_END);
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, 0, 10);
	fapi_set_u16(skb, u.mlme_scan_ind.scan_id, 0x0010);
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_range_done_ind, MLME_RANGE_DONE_IND, 0, 10);
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_event_log_ind, MLME_EVENT_LOG_IND, 0, 10);
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_start_detect_ind, MLME_START_DETECT_IND, 0, 10);
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, 0, 10);
	KUNIT_EXPECT_EQ(test, -EINVAL, sap_mlme_rx_handler(sdev, skb));

	kfree_skb(skb);

	skb = fapi_alloc(mlme_roamed_ind, MLME_ROAMED_IND, 0, 10);
	KUNIT_EXPECT_EQ(test, -EINVAL, sap_mlme_rx_handler(sdev, skb));

	kfree_skb(skb);

	skb = fapi_alloc(mlme_roamed_ind, MLME_ROAMED_IND, SLSI_NET_INDEX_WLAN, 10);
	KUNIT_EXPECT_EQ(test, -ENODEV, sap_mlme_rx_handler(sdev, skb));

	kfree_skb(skb);

	skb = fapi_alloc(mlme_roamed_ind, MLME_ROAMED_IND, SLSI_NET_INDEX_WLAN, 10);
	sdev->netdev[SLSI_NET_INDEX_WLAN] = dev;
	atomic_set(&ndev_vif->sta.drop_roamed_ind, true);
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_roamed_ind, MLME_ROAMED_IND, SLSI_NET_INDEX_WLAN, 10);
	atomic_set(&ndev_vif->sta.drop_roamed_ind, false);

	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_twt_notify_ind, MLME_TWT_NOTIFY_IND, 0, 10);
	KUNIT_EXPECT_EQ(test, -EINVAL, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_received_frame_ind, MLME_RECEIVED_FRAME_IND, SLSI_NET_INDEX_WLAN, 10);
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, SLSI_NET_INDEX_WLAN, 10);
	fapi_set_u16(skb, u.mlme_scan_ind.scan_id, SLSI_GSCAN_SCAN_ID_END);
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_range_done_ind, MLME_RANGE_DONE_IND, SLSI_NET_INDEX_WLAN, 10);
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_event_log_ind, MLME_EVENT_LOG_IND, SLSI_NET_INDEX_WLAN, 10);
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_twt_notify_ind, MLME_TWT_NOTIFY_IND, SLSI_NET_INDEX_WLAN, 10);
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_send_frame_cfm, MLME_SEND_FRAME_CFM, SLSI_NET_INDEX_WLAN, 10);
	KUNIT_EXPECT_EQ(test, 0, sap_mlme_rx_handler(sdev, skb));

	skb = fapi_alloc(mlme_send_frame_req, MLME_SEND_FRAME_REQ, SLSI_NET_INDEX_WLAN, 10);
	KUNIT_EXPECT_EQ(test, -EINVAL, sap_mlme_rx_handler(sdev, skb));

	kfree_skb(skb);
}

/*
 * Test fictures
 */
static int sap_mlme_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void sap_mlme_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
	return;
}

/*
 * KUnit testcase definitions
 */
static struct kunit_case sap_mlme_test_cases[] = {
	KUNIT_CASE(test_sap_mlme_init),
	KUNIT_CASE(test_sap_mlme_deinit),
	KUNIT_CASE(test_slsi_get_fapi_version_string),
	KUNIT_CASE(test_sap_mlme_notifier),
	KUNIT_CASE(test_sap_mlme_version_supported),
	KUNIT_CASE(test_slsi_rx_netdev_mlme),
	KUNIT_CASE(test_slsi_rx_netdev_mlme_work),
	KUNIT_CASE(test_slsi_rx_enqueue_netdev_mlme),
	KUNIT_CASE(test_slsi_rx_action_enqueue_netdev_mlme),
	KUNIT_CASE(test_sap_mlme_rx_handler),
	{}
};

static struct kunit_suite sap_mlme_test_suite[] = {
	{
		.name = "kunit-sap-mlme-test",
		.test_cases = sap_mlme_test_cases,
		.init = sap_mlme_test_init,
		.exit = sap_mlme_test_exit,
	}};

kunit_test_suites(sap_mlme_test_suite);
