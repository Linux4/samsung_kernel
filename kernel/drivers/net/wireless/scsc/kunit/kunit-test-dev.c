// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-cfg80211_ops.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-load_manager.h"
#include "kunit-mock-cm_if.h"
#include "kunit-mock-sap_mlme.h"
#include "kunit-mock-sap_ma.h"
#include "kunit-mock-sap_dbg.h"
#include "kunit-mock-sap_test.h"
#include "kunit-mock-scsc_wlan_mmap.h"
#include "kunit-mock-dpd_mmap.h"
#include "kunit-mock-udi.h"
#include "kunit-mock-kic.h"
#include "kunit-mock-procfs.h"
#include "kunit-mock-reg_info.h"
#include "kunit-mock-netif.h"
#include "kunit-mock-nl80211_vendor.h"
#include "kunit-mock-ba.h"
#include "../dev.c"

/* unit test function definition */
static void test_slsi_dev_gscan_supported(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, slsi_dev_gscan_supported());
}

static void test_slsi_dev_rtt_supported(struct kunit *test)
{
	slsi_dev_rtt_supported();
}

static void test_slsi_dev_llslogs_supported(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, slsi_dev_llslogs_supported());
}

static void test_slsi_dev_lls_supported(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, slsi_dev_lls_supported());
}

static void test_slsi_dev_epno_supported(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, slsi_dev_epno_supported());
}

static void test_slsi_dev_vo_vi_block_ack(struct kunit *test)
{
	slsi_dev_vo_vi_block_ack();
}

static void test_slsi_dev_get_scan_result_count(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 200, slsi_dev_get_scan_result_count());
}

static void test_slsi_dev_nan_supported(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	KUNIT_EXPECT_EQ(test, 0, slsi_dev_nan_supported(sdev));
}

#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
static void test_slsi_dev_nan_is_ipv6_link_tlv_include(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, slsi_dev_nan_is_ipv6_link_tlv_include());
}

static void test_slsi_get_nan_max_ndp_instances(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 1, slsi_get_nan_max_ndp_instances());
}

static void test_slsi_get_nan_max_ndi_ifaces(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 1, slsi_get_nan_max_ndi_ifaces());
}

static void test_slsi_get_nan_ndp_delay(struct kunit *test)
{
#ifdef SCSC_SEP_VERSION
	KUNIT_EXPECT_EQ(test, 550, slsi_get_nan_ndp_delay());
#else
	KUNIT_EXPECT_EQ(test, 750, slsi_get_nan_ndp_delay());
#endif
}

static void test_slsi_get_nan_ndp_max_time(struct kunit *test)
{
#ifdef SCSC_SEP_VERSION
	KUNIT_EXPECT_EQ(test, 600, slsi_get_nan_ndp_max_time());
#else
	KUNIT_EXPECT_EQ(test, 800, slsi_get_nan_ndp_max_time());
#endif
}

static void test_slsi_get_nan_mac_random(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, slsi_get_nan_mac_random());
}

#endif

static void test_slsi_dev_6ghz_split_scan_enabled(struct kunit *test)
{
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	slsi_dev_6ghz_split_scan_enabled();
#else
	KUNIT_EXPECT_FALSE(test, slsi_dev_6ghz_split_scan_enabled());
#endif
}

static void test_slsi_dev_6ghz_skip_acs(struct kunit *test)
{
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	slsi_dev_6ghz_skip_acs();
#else
	KUNIT_EXPECT_FALSE(test, slsi_dev_6ghz_skip_acs());
#endif
}

static void test_slsi_dev_inetaddr_changed(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = TEST_DEV_TO_NDEV(dev);
	struct in_ifaddr *ifa = kunit_kzalloc(test, sizeof(struct in_ifaddr), GFP_KERNEL);
	struct in_device *indev = kunit_kzalloc(test, sizeof(struct in_device), GFP_KERNEL);

	indev->dev = dev;
	ifa->ifa_dev = indev;

	sdev->wlan_wl_roam.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	sdev->wlan_wl_roam.ws->active = 1;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->sta.is_wps = true;
	KUNIT_EXPECT_EQ(test, 0, slsi_dev_inetaddr_changed(&sdev->inetaddr_notifier, NETDEV_UP, ifa));

	KUNIT_EXPECT_EQ(test, 0, slsi_dev_inetaddr_changed(&sdev->inetaddr_notifier, NETDEV_DOWN, ifa));
}

#if IS_ENABLED(CONFIG_IPV6)
static void test_slsi_dev_inet6addr_changed(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct inet6_ifaddr *ifa = kunit_kzalloc(test, sizeof(struct inet6_ifaddr), GFP_KERNEL);
	struct inet6_dev *i6_dev = kunit_kzalloc(test, sizeof(struct inet6_dev), GFP_KERNEL);
	struct net_device *i6_ndev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif),
						   GFP_KERNEL);
	unsigned long data = NETDEV_UP;

	i6_ndev->ieee80211_ptr = kunit_kzalloc(test, sizeof(struct wireless_dev), GFP_KERNEL);
	ifa->idev = i6_dev;
	ifa->idev->dev = i6_ndev;

	KUNIT_EXPECT_EQ(test, 0, slsi_dev_inet6addr_changed(&sdev->inet6addr_notifier, data, ifa));
}
#endif

static void test_slsi_dump_system_error_buffer(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	slsi_dump_system_error_buffer(sdev);
}

static void test_slsi_add_log_to_system_error_buffer(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char input_buffer[128] = { 0 };
	slsi_add_log_to_system_error_buffer(sdev, input_buffer);
}

static void test_slsi_sys_error_log_init(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	slsi_sys_error_log_init(sdev);
}

static void test_slsi_dev_attach(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);
	struct scsc_mx *core = NULL;
	struct scsc_service_client *mx_wlan_client = kunit_kzalloc(test, sizeof(struct scsc_service_client), GFP_KERNEL);
	struct net_device *ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);

	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_dev_attach(dev, core, mx_wlan_client));

	test_cfg80211_set_sdev(sdev);
	//to make kunit_mock_slsi_netif_init() failed
	rcu_assign_pointer(sdev->netdev[0], ndev);
	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_dev_attach(dev, core, mx_wlan_client));

	rcu_assign_pointer(sdev->netdev[0], NULL);
	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_dev_attach(dev, core, mx_wlan_client));

	core = (struct scsc_mx *)kunit_kzalloc(test, sizeof(u8), GFP_KERNEL);
	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_dev_attach(dev, core, mx_wlan_client));

	sdev->wiphy = kunit_kzalloc(test, sizeof(struct wiphy), GFP_KERNEL);
#if IS_ENABLED(CONFIG_IPV6)
	sdev->inet6addr_notifier.priority = -1;
	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_dev_attach(dev, core, mx_wlan_client));

	sdev->inet6addr_notifier.priority = 0;
#endif

	sdev->inetaddr_notifier.priority = -1;
	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_dev_attach(dev, core, mx_wlan_client));

	sdev->inetaddr_notifier.priority = 0;
	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_dev_attach(dev, core, mx_wlan_client));

	ndev->netdev_ops = kunit_kzalloc(test, sizeof(struct net_device_ops), GFP_KERNEL);
	rcu_assign_pointer(sdev->netdev[SLSI_NET_INDEX_WLAN], ndev);
	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_dev_attach(dev, core, mx_wlan_client));

	rcu_assign_pointer(sdev->netdev[SLSI_NET_INDEX_AP], ndev);
	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_dev_attach(dev, core, mx_wlan_client));

	rcu_assign_pointer(sdev->netdev[SLSI_NET_INDEX_AP2], ndev);
	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_dev_attach(dev, core, mx_wlan_client));

	rcu_assign_pointer(sdev->netdev[SLSI_NET_INDEX_P2P], ndev);
	KUNIT_EXPECT_PTR_NE(test, NULL, slsi_dev_attach(dev, core, mx_wlan_client));

	slsi_dev_detach(sdev);
	kfree(sdev->device_wq);
}

static void test_slsi_dev_detach(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	bool opt = true;

	sdev->term_udi_users = &opt;
	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	INIT_LIST_HEAD(&sdev->log_clients.log_client_list);

	slsi_dev_detach(sdev);
}

static void __init test_slsi_dev_load(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, slsi_dev_load());
}

static void __exit test_slsi_dev_unload(struct kunit *test)
{
	slsi_dev_unload();
}

/* Test fictures */
static int dev_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
	return 0;
}

static void dev_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case dev_test_cases[] = {
	KUNIT_CASE(test_slsi_dev_gscan_supported),
	KUNIT_CASE(test_slsi_dev_rtt_supported),
	KUNIT_CASE(test_slsi_dev_llslogs_supported),
	KUNIT_CASE(test_slsi_dev_lls_supported),
	KUNIT_CASE(test_slsi_dev_epno_supported),
	KUNIT_CASE(test_slsi_dev_vo_vi_block_ack),
	KUNIT_CASE(test_slsi_dev_get_scan_result_count),
	KUNIT_CASE(test_slsi_dev_nan_supported),
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
	KUNIT_CASE(test_slsi_dev_nan_is_ipv6_link_tlv_include),
	KUNIT_CASE(test_slsi_get_nan_max_ndp_instances),
	KUNIT_CASE(test_slsi_get_nan_max_ndi_ifaces),
	KUNIT_CASE(test_slsi_get_nan_ndp_delay),
	KUNIT_CASE(test_slsi_get_nan_ndp_max_time),
	KUNIT_CASE(test_slsi_get_nan_mac_random),
#endif
	KUNIT_CASE(test_slsi_dev_6ghz_split_scan_enabled),
	KUNIT_CASE(test_slsi_dev_6ghz_skip_acs),
	KUNIT_CASE(test_slsi_dev_inetaddr_changed),
#if IS_ENABLED(CONFIG_IPV6)
	KUNIT_CASE(test_slsi_dev_inet6addr_changed),
#endif
	KUNIT_CASE(test_slsi_dump_system_error_buffer),
	KUNIT_CASE(test_slsi_add_log_to_system_error_buffer),
	KUNIT_CASE(test_slsi_sys_error_log_init),
	KUNIT_CASE(test_slsi_dev_attach),
	KUNIT_CASE(test_slsi_dev_detach),
	KUNIT_CASE(test_slsi_dev_load),
	KUNIT_CASE(test_slsi_dev_unload),
	{}
};

static struct kunit_suite dev_test_suite[] = {
	{
		.name = "dev-test",
		.test_cases = dev_test_cases,
		.init = dev_test_init,
		.exit = dev_test_exit,
	}
};

kunit_test_suites(dev_test_suite);
