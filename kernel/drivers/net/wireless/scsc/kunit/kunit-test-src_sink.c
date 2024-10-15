// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-mlme.h"
#include "kunit-mock-mgt.h"
#include "../src_sink.c"

static void test_slsi_src_sink_fake_sta_start(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);

	ndev_vif->activated = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_fake_sta_start(sdev, dev));

	ndev_vif->activated = 0;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_DISCONNECTING;
	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_src_sink_fake_sta_start(sdev, dev));

	sdev->device_config.user_suspend_mode = EINVAL;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_src_sink_fake_sta_start(sdev, dev));

	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, -EFAULT, slsi_src_sink_fake_sta_start(sdev, dev));

	ndev_vif->activated = 0;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = peer;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_fake_sta_start(sdev, dev));
}

static void test_slsi_src_sink_fake_sta_stop(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);

	ndev_vif->activated = 0;
	slsi_src_sink_fake_sta_stop(sdev, dev);

	ndev_vif->activated = 1;
	sdev->device_config.user_suspend_mode = EINVAL;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = peer;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = 1;
	slsi_src_sink_fake_sta_stop(sdev, dev);
}

static void test_slsi_src_sink_loopback_start(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 queue_idx = 0;

	ndev_vif->activated = 1;

	dev = test_netdev_init(test, sdev, SLSI_NET_INDEX_P2P);
	ndev_vif = netdev_priv(dev);
	ndev_vif->activated = 1;

	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_loopback_start(sdev));

	while (queue_idx < SLSI_ADHOC_PEER_CONNECTIONS_MAX)
		if (ndev_vif->peer_sta_record[queue_idx])
			kfree(ndev_vif->peer_sta_record[queue_idx++]);
}

static void test_slsi_src_sink_loopback_stop(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	u8 queue_idx;

	slsi_src_sink_loopback_stop(sdev);

	ndev_vif->activated = 0;

	dev = test_netdev_init(test, sdev, SLSI_NET_INDEX_P2P);
	ndev_vif = netdev_priv(dev);
	ndev_vif->activated = 0;

	for (queue_idx = 0; queue_idx < SLSI_ADHOC_PEER_CONNECTIONS_MAX; queue_idx++)
		ndev_vif->peer_sta_record[queue_idx] = kzalloc(sizeof(*ndev_vif->peer_sta_record[queue_idx]),
						       GFP_KERNEL);

	slsi_src_sink_loopback_stop(sdev);
}

static void test_slsi_src_sink_cdev_ioctl_cfg(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct unifiio_src_sink_arg_t *src_sink_arg = kunit_kzalloc(test,
						      sizeof(struct unifiio_src_sink_arg_t), GFP_KERNEL);
	struct wireless_dev *ieee80211_ptr = kunit_kzalloc(test, sizeof(struct wireless_dev), GFP_KERNEL);
	struct slsi_scan_result *scan_result = kunit_kzalloc(test, sizeof(struct slsi_scan_result), GFP_KERNEL);
	u8 addr[ETH_ALEN];
	u8 queue_idx;

	src_sink_arg->common.vif = SLSI_NET_INDEX_WLAN;

	sdev->netdev[SLSI_NET_INDEX_WLAN] = NULL;
	KUNIT_EXPECT_EQ(test, -ENODEV, slsi_src_sink_cdev_ioctl_cfg(sdev, (unsigned long)src_sink_arg));

	sdev->netdev[SLSI_NET_INDEX_WLAN] = dev;
	sdev->netdev[SLSI_NET_INDEX_P2P] = dev;
	ndev_vif->activated = 1;

	src_sink_arg->common.action = SRC_SINK_ACTION_SINK_START;
	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_cdev_ioctl_cfg(sdev, (unsigned long)src_sink_arg));

	src_sink_arg->common.action = SRC_SINK_ACTION_SINK_STOP;
	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_cdev_ioctl_cfg(sdev, (unsigned long)src_sink_arg));

	src_sink_arg->common.action = SRC_SINK_ACTION_GEN_START;
	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_cdev_ioctl_cfg(sdev, (unsigned long)src_sink_arg));

	src_sink_arg->common.action = SRC_SINK_ACTION_GEN_STOP;
	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_cdev_ioctl_cfg(sdev, (unsigned long)src_sink_arg));

	src_sink_arg->common.action = SRC_SINK_ACTION_LOOPBACK_START;
	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_cdev_ioctl_cfg(sdev, (unsigned long)src_sink_arg));

	src_sink_arg->common.action = SRC_SINK_ACTION_LOOPBACK_STOP;
	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_cdev_ioctl_cfg(sdev, (unsigned long)src_sink_arg));

	src_sink_arg->common.action = SRC_SINK_ACTION_SINK_REPORT;
	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_cdev_ioctl_cfg(sdev, (unsigned long)src_sink_arg));

	src_sink_arg->common.action = SRC_SINK_ACTION_GEN_REPORT;
	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_cdev_ioctl_cfg(sdev, (unsigned long)src_sink_arg));

	src_sink_arg->common.action = SRC_SINK_ACTION_SINK_REPORT;
	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_cdev_ioctl_cfg(sdev, (unsigned long)src_sink_arg));

	src_sink_arg->common.action = SRC_SINK_ACTION_GEN_REPORT;
	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_cdev_ioctl_cfg(sdev, (unsigned long)src_sink_arg));

	src_sink_arg->common.action = SRC_SINK_ACTION_SINK_REPORT_CACHED;
	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_cdev_ioctl_cfg(sdev, (unsigned long)src_sink_arg));

	src_sink_arg->common.action = SRC_SINK_ACTION_GEN_REPORT_CACHED;
	KUNIT_EXPECT_EQ(test, 0, slsi_src_sink_cdev_ioctl_cfg(sdev, (unsigned long)src_sink_arg));

	src_sink_arg->common.action = SRC_SINK_ACTION_NONE;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_src_sink_cdev_ioctl_cfg(sdev, (unsigned long)src_sink_arg));
}

static void test_slsi_rx_sink_report(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb;

	skb = alloc_skb(sizeof(u8)*100, GFP_KERNEL);

	slsi_rx_sink_report(sdev, dev, skb);
}

static void test_slsi_rx_gen_report(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb;

	skb = alloc_skb(sizeof(u8)*100, GFP_KERNEL);

	slsi_rx_gen_report(sdev, dev, skb);
}

static int src_sink_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void src_sink_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case src_sink_test_cases[] = {
	KUNIT_CASE(test_slsi_src_sink_fake_sta_start),
	KUNIT_CASE(test_slsi_src_sink_fake_sta_stop),
	KUNIT_CASE(test_slsi_src_sink_loopback_start),
	KUNIT_CASE(test_slsi_src_sink_loopback_stop),
	KUNIT_CASE(test_slsi_src_sink_cdev_ioctl_cfg),
	KUNIT_CASE(test_slsi_rx_sink_report),
	KUNIT_CASE(test_slsi_rx_gen_report),
	{}
};

static struct kunit_suite src_sink_test_suite[] = {
	{
		.name = "kunit-src_sink-test",
		.test_cases = src_sink_test_cases,
		.init = src_sink_test_init,
		.exit = src_sink_test_exit,
	}
};

kunit_test_suites(src_sink_test_suite);
