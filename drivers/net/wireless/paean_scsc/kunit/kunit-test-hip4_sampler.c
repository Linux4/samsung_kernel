// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-kernel.h"
#include "kunit-mock-traffic_monitor.h"
#include "../hip4_sampler.c"


/* Test function */
/* hip4_sampler.c */
static void test_hip4_sampler_update_record_filter(struct kunit *test)
{
#ifdef CONFIG_SCSC_WLAN_HIP5
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP5_MIF_Q_FH_CTRL));
#else
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_MIF_Q_FH_CTRL));
#endif
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_SAMPLER_QREF));
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_SAMPLER_SIGNAL_CTRLTX));
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_SAMPLER_THROUG));
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_SAMPLER_STOP_Q));
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_SAMPLER_MBULK));
	hip4_sampler_update_record_filter(HIP4_SAMPLER_QFULL);
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_SAMPLER_MFULL));
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_SAMPLER_INT));
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_SAMPLER_RESET));
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_SAMPLER_PEER));
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_SAMPLER_BOT_RX));
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_SAMPLER_BOT_QMOD_START));
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_SAMPLER_PKT_TX));
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_SAMPLER_SUSPEND));
	KUNIT_EXPECT_TRUE(test, hip4_sampler_update_record_filter(HIP4_SAMPLER_TCP_SYN));
}

static void test_hip4_sampler_update_record(struct kunit *test)
{
	struct hip4_sampler_dev *hip4_dev = kunit_kzalloc(test, sizeof(struct hip4_sampler_dev), GFP_KERNEL);
	unsigned char v2 = 0x26;

	hip4_sampler.init = 1;
	hip4_sampler_update_record(hip4_dev, v2, 0, 0, 0, 0);

	hip4_sampler.devs[0].filp = 1;
	hip4_dev->filp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	hip4_sampler_update_record(hip4_dev, v2, 0, 0, 0, 0);
}

static void test_hip4_sampler_store_param(struct kunit *test)
{
	hip4_sampler_store_param();
}

static void test_hip4_sampler_restore_param(struct kunit *test)
{
	hip4_sampler_restore_param();
}

static void test_hip4_sampler_dynamic_switcher(struct kunit *test)
{
	hip4_sampler_dynamic_switcher(300000000);
	hip4_sampler_dynamic_switcher(300000000);
}

static void test_hip4_sampler_tput_monitor(struct kunit *test)
{
	struct hip4_sampler_dev *hip4_dev = kunit_kzalloc(test, sizeof(struct hip4_sampler_dev), GFP_KERNEL);

	hip4_dev->filp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	/* tput_tx < 1000 && tput_rx < 1000 */
	hip4_sampler_tput_monitor(hip4_dev, 0, 500, 700);
	hip4_sampler_tput_monitor(hip4_dev, 0, 700, 500);

	/* 1000 <= tput_tx < 1000 * 1000 && 1000 <= tput_rx < 1000 * 1000 */
	hip4_sampler_tput_monitor(hip4_dev, 0, 1500, 1700);
	hip4_sampler_tput_monitor(hip4_dev, 0, 1700, 1500);

	/* 1000 * 1000 < tput_tx && 1000 * 1000 <= tput_rx */
	hip4_sampler_tput_monitor(hip4_dev, 0, 1000 * 1000, 1000 * 1001);
	hip4_sampler_tput_monitor(hip4_dev, 0, 1000 * 1001, 1000 * 1000);
}

static void test_hip4_sampler_tcp_decode(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct ethhdr *eth_hdr;
	struct iphdr *ip_hdr;
	struct tcphdr *tcp_hdr;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u8 *ptr = kunit_kzalloc(test, sizeof(struct iphdr) + sizeof(struct ethhdr), GFP_KERNEL);

	sdev->minor_prof = 0;
	//pointer array
	sdev->netdev[0] = dev;

	eth_hdr = (struct ethhdr *)(ptr);
	eth_hdr->h_proto = 0x0008;

	ip_hdr = (struct iphdr *)(ptr + ETH_HLEN);
	ip_hdr->protocol = 6;

	tcp_hdr = (struct tcphdr *)(ptr + ETH_HLEN + 20);
	tcp_hdr->source = 0;
	tcp_hdr->window = 1;
	tcp_hdr->ack_seq = 1;
	tcp_hdr->ack = 1;
	tcp_hdr->syn = 1;
	tcp_hdr->doff = 10;
	tcp_hdr->dest = 0;

	ndev_vif->ack_suppression[0].dport = 0;
	ndev_vif->ack_suppression[0].sport = 0;
	ndev_vif->ack_suppression[0].rx_window_scale = 1;
	ndev_vif->ack_suppression[0].stream_id = 1;

	hip4_sampler_tcp_decode(sdev, dev, ptr, true);
}

static void test_hip4_sampler_open(struct kunit *test)
{
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);
	struct file *filp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	struct hip4_sampler_dev *hip4_dev = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);

	inode->i_cdev = &hip4_dev->cdev;

	hip4_sampler_open(inode, filp);
}

static void test_hip4_sampler_read(struct kunit *test)
{
	struct file *filp = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct hip4_sampler_dev *hip4_dev = kunit_kzalloc(test, sizeof(struct hip4_sampler_dev), GFP_KERNEL);
	char __user *buf = NULL;
	size_t len = 0;
	loff_t *offset = NULL;

	filp->private_data = hip4_dev;
	hip4_dev->type = OFFLINE;
	hip4_sampler_read(filp, buf, len, offset);

	hip4_dev->type = STREAMING;
	hip4_dev->error = NO_ERROR;
	hip4_sampler_read(filp, buf, len, offset);
}

static void test_hip4_sampler_poll(struct kunit *test)
{
	struct file *filp = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct hip4_sampler_dev *hip4_dev = kunit_kzalloc(test, sizeof(struct hip4_sampler_dev), GFP_KERNEL);
	poll_table *wait = NULL;

	filp->private_data = hip4_dev;

	hip4_sampler_poll(filp, wait);
}

static void test_hip4_sampler_release(struct kunit *test)
{
	struct file *filp = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct hip4_sampler_dev *hip4_dev = kunit_kzalloc(test, sizeof(struct hip4_sampler_dev), GFP_KERNEL);
	struct inode *inode = kunit_kzalloc(test, sizeof(struct inode), GFP_KERNEL);

	hip4_dev->filp = kunit_kzalloc(test, sizeof(struct file), GFP_KERNEL);
	inode->i_cdev = &hip4_dev->cdev;
	filp->private_data = hip4_dev;

	hip4_sampler_release(inode, filp);
}

static void test_hip4_sampler_register_hip(struct kunit *test)
{
	hip4_sampler.devs[1].mx = 0;

	KUNIT_EXPECT_EQ(test, 1, hip4_sampler_register_hip(0));
}

static void test_hip4_sampler_create(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct scsc_mx *mx = 0;
	hip4_sampler_create(sdev, mx);
	kfree(hip4_sampler.devs[0].dev);
	kfree(hip4_sampler.devs[1].dev);
}
static void test_hip4_sampler_destroy(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct hip_priv *priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	struct scsc_mx *mx = 0;


	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	hip4_sampler.devs[1].cdev.dev = 1;
	hip4_sampler.devs[1].mx = 1;

	hip4_sampler_destroy(sdev, mx);
}

static void test_hip4_collect_init(struct kunit *test)
{
	struct scsc_log_collector_client *collect_client = NULL;

	KUNIT_EXPECT_EQ(test, 0, hip4_collect_init(collect_client));
}

static void test_hip4_collect_end(struct kunit *test)
{
	struct scsc_log_collector_client *collect_client = NULL;

	KUNIT_EXPECT_EQ(test, 0, hip4_collect_end(collect_client));
}

static void test_hip4_collect(struct kunit *test)
{
	struct scsc_log_collector_client *collect_client;

	collect_client = kunit_kzalloc(test, sizeof(struct scsc_log_collector_client), GFP_KERNEL);
	collect_client->prv = 0;
	hip4_sampler.devs[SCSC_HIP4_DEBUG_INTERFACES-1].mx = 0;
	hip4_sampler.devs[SCSC_HIP4_DEBUG_INTERFACES-1].type = OFFLINE;
	KUNIT_EXPECT_NE(test, 0, hip4_collect(collect_client, 1));
}

/* Test features*/
static int hip4_sampler_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void hip4_sampler_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case hip4_sampler_test_cases[] = {
	/* hip4_sampler.c */
	KUNIT_CASE(test_hip4_sampler_update_record_filter),
	KUNIT_CASE(test_hip4_sampler_update_record),
	KUNIT_CASE(test_hip4_sampler_store_param),
	KUNIT_CASE(test_hip4_sampler_restore_param),
	KUNIT_CASE(test_hip4_sampler_dynamic_switcher),
	KUNIT_CASE(test_hip4_sampler_tput_monitor),
	KUNIT_CASE(test_hip4_sampler_tcp_decode),
	KUNIT_CASE(test_hip4_sampler_open),
	KUNIT_CASE(test_hip4_sampler_read),
	KUNIT_CASE(test_hip4_sampler_poll),
	KUNIT_CASE(test_hip4_sampler_release),
	KUNIT_CASE(test_hip4_sampler_register_hip),
	KUNIT_CASE(test_hip4_sampler_create),
	KUNIT_CASE(test_hip4_sampler_destroy),
	KUNIT_CASE(test_hip4_collect_init),
	KUNIT_CASE(test_hip4_collect_end),
	KUNIT_CASE(test_hip4_collect),
	{}
};

static struct kunit_suite hip4_sampler_test_suite[] = {
	{
		.name = "kunit-hip4-sampler-test",
		.test_cases = hip4_sampler_test_cases,
		.init = hip4_sampler_test_init,
		.exit = hip4_sampler_test_exit,
	}
};

kunit_test_suites(hip4_sampler_test_suite);
