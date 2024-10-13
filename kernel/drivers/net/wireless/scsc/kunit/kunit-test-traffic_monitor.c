// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-mlme.h"
#include "../traffic_monitor.c"


void test_traffic_mon_client_cb(void *client_ctx, u32 state, u32 tput_tx, u32 tput_rx)
{
	printk("%s called.\n", __func__);
}

/* unit test function definition*/
static void test_traffic_mon_invoke_client_callback(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_traffic_mon_client_entry *traffic_mon_client = kunit_kzalloc(test,
										 sizeof(struct slsi_traffic_mon_client_entry),
										 GFP_KERNEL);

	traffic_mon_client->traffic_mon_client_cb = test_traffic_mon_client_cb;

	spin_lock_bh(&sdev->traffic_mon_clients.lock);
	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	list_add_tail(&traffic_mon_client->q, &sdev->traffic_mon_clients.client_list);
	spin_unlock_bh(&sdev->traffic_mon_clients.lock);

	traffic_mon_invoke_client_callback(sdev, 0, 0, true);

	traffic_mon_client->mode = TRAFFIC_MON_CLIENT_MODE_PERIODIC;
	traffic_mon_invoke_client_callback(sdev, 0, 0, false);

	traffic_mon_client->mode = TRAFFIC_MON_CLIENT_MODE_EVENTS;
	traffic_mon_client->high_tput = 1;
	traffic_mon_client->state = TRAFFIC_MON_CLIENT_STATE_LOW;
	traffic_mon_client->hysteresis = SLSI_TRAFFIC_MON_HYSTERESIS_LOW;
	traffic_mon_invoke_client_callback(sdev, 1, 1, false);

	traffic_mon_client->mode = TRAFFIC_MON_CLIENT_MODE_EVENTS;
	traffic_mon_client->high_tput = 1;
	traffic_mon_client->state = TRAFFIC_MON_CLIENT_STATE_HIGH;
	traffic_mon_client->hysteresis = SLSI_TRAFFIC_MON_HYSTERESIS_LOW;
	traffic_mon_invoke_client_callback(sdev, 1, 1, false);

	traffic_mon_client->high_tput = 0;
	traffic_mon_client->mid_tput = 1;
	traffic_mon_client->state = TRAFFIC_MON_CLIENT_STATE_HIGH;
	traffic_mon_client->hysteresis = SLSI_TRAFFIC_MON_HYSTERESIS_HIGH + SLSI_TRAFFIC_MON_HYSTERESIS_LOW;
	traffic_mon_invoke_client_callback(sdev, 1, 1, false);

	traffic_mon_client->mid_tput = 0;
	traffic_mon_client->state = TRAFFIC_MON_CLIENT_STATE_HIGH;
	traffic_mon_client->hysteresis = SLSI_TRAFFIC_MON_HYSTERESIS_HIGH + SLSI_TRAFFIC_MON_HYSTERESIS_LOW;
	traffic_mon_invoke_client_callback(sdev, 1, 1, false);
}

#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
static void test_traffic_mon_timer(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct netdev_vif *ndev_vif;
	int i;

	for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++) {
		sdev->netdev[i] = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
		ndev_vif = netdev_priv(sdev->netdev[i]);
		ndev_vif->report_time = 1000;
	}

	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	traffic_mon_timer(&sdev->traffic_mon_clients.timer);
}
#else
static void test_traffic_mon_timer(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	traffic_mon_timer((unsigned long)sdev);
}
#endif

static void test_slsi_traffic_mon_event_rx(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	skb->len = 0;
	slsi_traffic_mon_event_rx(NULL, dev, skb);

	skb->len = 40;
	slsi_traffic_mon_event_rx(NULL, dev, skb);
}

static void test_slsi_traffic_mon_event_tx(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	slsi_traffic_mon_event_tx(NULL, dev, 60);

	slsi_traffic_mon_event_tx(NULL, dev, 40);
}


static void test_slsi_traffic_mon_override(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_traffic_mon_client_entry *traffic_mon_client = kunit_kzalloc(test,
										 sizeof(struct slsi_traffic_mon_client_entry),
										 GFP_KERNEL);

	spin_lock_bh(&sdev->traffic_mon_clients.lock);
	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	list_add_tail(&traffic_mon_client->q, &sdev->traffic_mon_clients.client_list);
	spin_unlock_bh(&sdev->traffic_mon_clients.lock);

	slsi_traffic_mon_override(sdev);
}

static void test_slsi_traffic_mon_is_running(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_traffic_mon_client_entry *traffic_mon_client = kunit_kzalloc(test,
										 sizeof(struct slsi_traffic_mon_client_entry),
										 GFP_KERNEL);

	spin_lock_bh(&sdev->traffic_mon_clients.lock);
	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	list_add_tail(&traffic_mon_client->q, &sdev->traffic_mon_clients.client_list);
	spin_unlock_bh(&sdev->traffic_mon_clients.lock);

	KUNIT_EXPECT_EQ(test, 1, slsi_traffic_mon_is_running(sdev));
}

static void test_slsi_traffic_mon_client_register(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	int i, dir = TRAFFIC_MON_DIR_RX;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_traffic_mon_client_register(sdev, NULL, TRAFFIC_MON_CLIENT_MODE_EVENTS,
						0, 0, dir, NULL));

	for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++)
		sdev->netdev[i] = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);

	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	KUNIT_EXPECT_EQ(test, 0, slsi_traffic_mon_client_register(sdev, sdev, TRAFFIC_MON_CLIENT_MODE_EVENTS,
						0, 0, dir, NULL));

	slsi_traffic_mon_client_unregister(sdev, sdev);
}

static void test_slsi_traffic_mon_client_unregister(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_traffic_mon_client_entry *traffic_mon_client = kzalloc(sizeof(struct slsi_traffic_mon_client_entry),
									   GFP_KERNEL);

	int i;
	for (i = 1; i <= CONFIG_SCSC_WLAN_MAX_INTERFACES; i++)
		sdev->netdev[i] = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);

	spin_lock_bh(&sdev->traffic_mon_clients.lock);
	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	list_add_tail(&traffic_mon_client->q, &sdev->traffic_mon_clients.client_list);
	spin_unlock_bh(&sdev->traffic_mon_clients.lock);

	slsi_traffic_mon_client_unregister(sdev, traffic_mon_client->client_ctx);
	slsi_traffic_mon_client_unregister(sdev, NULL);
}

static void test_slsi_traffic_mon_clients_init(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	slsi_traffic_mon_clients_init(NULL);
	slsi_traffic_mon_clients_init(sdev);
}

static void test_slsi_traffic_mon_clients_deinit(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_traffic_mon_client_entry *traffic_mon_client = kzalloc(sizeof(struct slsi_traffic_mon_client_entry),
									   GFP_KERNEL);

	slsi_traffic_mon_clients_deinit(NULL);

	spin_lock_bh(&sdev->traffic_mon_clients.lock);
	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	list_add_tail(&traffic_mon_client->q, &sdev->traffic_mon_clients.client_list);
	spin_unlock_bh(&sdev->traffic_mon_clients.lock);

	slsi_traffic_mon_clients_deinit(sdev);
}

#ifdef CONFIG_SCSC_WLAN_TPUT_MONITOR
static void test_traffic_mon_get_hostio_usage(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *dev = TEST_TO_DEV(test);
	char buf[512] = {'\0',};

	KUNIT_EXPECT_LT(test, 0, traffic_mon_get_hostio_usage(sdev, dev, buf));
}

#ifdef CONFIG_SCSC_WLAN_TX_API
static void test_traffic_mon_get_tx_usage(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tx_netdev_data *tx_priv;
	char buf[512] = {'\0',};

	ndev_vif->tx_netdev_data = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);
	tx_priv = ndev_vif->tx_netdev_data;
	KUNIT_EXPECT_LT(test, 0, traffic_mon_get_tx_usage(ndev_vif, buf));

	tx_priv->qstat[0].last_cumulated_stop = ktime_get();
	tx_priv->qstat[0].cumulated_stop = tx_priv->qstat[0].last_cumulated_stop + 1;
	KUNIT_EXPECT_LT(test, 0, traffic_mon_get_tx_usage(ndev_vif, buf));
}
#endif

static void test_slsi_traffic_mon_work(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	tm_data.sdev = TEST_TO_SDEV(test);
	tm_data.tm_enable = true;
	slsi_traffic_mon_work(NULL);

	tm_logger_enable = (TM_LOG_HIO | TM_LOG_TX);
	slsi_traffic_mon_work(NULL);

	ndev_vif->activated = true;
	slsi_traffic_mon_work(NULL);
}

static void test_slsi_traffic_mon_logger_cb(struct kunit *test)
{
	struct tm_logger *tm = kunit_kzalloc(test, sizeof(struct tm_logger), GFP_KERNEL);

	tm->sdev = TEST_TO_SDEV(test);
	tm_logger_enable = TM_LOG_TX;

	slsi_traffic_mon_logger_cb(tm, TRAFFIC_MON_CLIENT_STATE_HIGH, 0, 0);
	slsi_traffic_mon_logger_cb(tm, TRAFFIC_MON_CLIENT_STATE_LOW, 0, 0);
}

static void test_slsi_traffic_mon_init(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	slsi_traffic_mon_init(sdev);
}

static void test_slsi_traffic_mon_deinit(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	slsi_traffic_mon_deinit(sdev);
}
#endif

/* Test fictures */
static int traffic_monitor_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void traffic_monitor_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case traffic_monitor_test_cases[] = {
	KUNIT_CASE(test_traffic_mon_invoke_client_callback),
#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	KUNIT_CASE(test_traffic_mon_timer),
#else
	KUNIT_CASE(test_traffic_mon_timer),
#endif
	KUNIT_CASE(test_slsi_traffic_mon_event_rx),
	KUNIT_CASE(test_slsi_traffic_mon_event_tx),
	KUNIT_CASE(test_slsi_traffic_mon_override),
	KUNIT_CASE(test_slsi_traffic_mon_is_running),
	KUNIT_CASE(test_slsi_traffic_mon_client_register),
	KUNIT_CASE(test_slsi_traffic_mon_client_unregister),
	KUNIT_CASE(test_slsi_traffic_mon_clients_init),
	KUNIT_CASE(test_slsi_traffic_mon_clients_deinit),
#ifdef CONFIG_SCSC_WLAN_TPUT_MONITOR
	KUNIT_CASE(test_traffic_mon_get_hostio_usage),
#ifdef CONFIG_SCSC_WLAN_TX_API
	KUNIT_CASE(test_traffic_mon_get_tx_usage),
#endif
	KUNIT_CASE(test_slsi_traffic_mon_work),
	KUNIT_CASE(test_slsi_traffic_mon_logger_cb),
	KUNIT_CASE(test_slsi_traffic_mon_init),
	KUNIT_CASE(test_slsi_traffic_mon_deinit),
#endif
	{}
};

static struct kunit_suite traffic_monitor_test_suite[] = {
	{
		.name = "traffic_monitor-test",
		.test_cases = traffic_monitor_test_cases,
		.init = traffic_monitor_test_init,
		.exit = traffic_monitor_test_exit,
	}
};

kunit_test_suites(traffic_monitor_test_suite);
