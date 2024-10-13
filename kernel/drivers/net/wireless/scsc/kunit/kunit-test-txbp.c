// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>
#include <net/sch_generic.h>

#include "kunit-common.h"
#ifdef CONFIG_SCSC_WLAN_HIP5
#include "kunit-mock-hip5.h"
#else
#include "kunit-mock-hip4.h"
#endif
#include "kunit-mock-load_manager.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-misc.h"
#include "kunit-mock-netif.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-tx.h"
#include "kunit-mock-tdls_manager.h"
#include "kunit-mock-traffic_monitor.h"
#include "kunit-mock-dev.h"
#include "../txbp.h"
#include "../txbp.c"

#if defined(CONFIG_SCSC_PCIE_CHIP)
static void test_slsi_tx_stop_all_queues(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);

	slsi_tx_stop_all_queues(dev);

	ndev_vif->tx_netdev_data = tx_priv;
	slsi_tx_stop_all_queues(dev);
}

static void test_slsi_tx_wake_all_queues(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);

	slsi_tx_wake_all_queues(dev);

	ndev_vif->tx_netdev_data = tx_priv;
	slsi_tx_wake_all_queues(dev);
}
#endif

static void test_slsi_tx_timeout(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	int qidx;

	slsi_tx_timeout(dev);

	dev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue)*SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);
	for (qidx = 0; qidx < SLSI_TX_TOTAL_QUEUES; qidx++) {
		dev->_tx[qidx].qdisc = kunit_kzalloc(test, sizeof(struct Qdisc), GFP_KERNEL);
		dev->_tx[qidx].qdisc->ops = kunit_kzalloc(test, sizeof(struct Qdisc_ops), GFP_KERNEL);
	}

	txbp_priv.vif_list.next = NULL;
	slsi_tx_timeout(dev);
}

static void test_slsi_tx_get_cod(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);
	int gcod = 0;
	int netdev_cod = 0;
	int ac_cod[AC_CATEGORIES];
	int q;

	slsi_tx_get_cod(sdev, tx_priv, &gcod, &netdev_cod, ac_cod);
	KUNIT_EXPECT_EQ(test, txbp_priv.cod, gcod);
	KUNIT_EXPECT_EQ(test, tx_priv->netdev_cod, netdev_cod);
	for (q = SLSI_TRAFFIC_Q_BE; q <= SLSI_TRAFFIC_Q_VO; q++)
		KUNIT_EXPECT_EQ(test, tx_priv->ac_cod[q], ac_cod[q]);
}

static void test_slsi_txbp_has_resource(struct kunit *test)
{
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);
	int qidx;

	tx_priv->tx.ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	tx_priv->tx.ndev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue)*SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);
	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++)
		tx_priv->tx.ndev->_tx[qidx].qdisc = kunit_kzalloc(test, sizeof(struct Qdisc), GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, SLSI_HIP_TX_DATA_SLOTS_NUM - SLSI_TX_BP_MBULK_GUARD, slsi_txbp_has_resource(tx_priv));

	tx_priv->ac_cod[0] = 1;
	KUNIT_EXPECT_EQ(test, SLSI_HIP_TX_DATA_SLOTS_NUM - SLSI_TX_BP_MBULK_GUARD, slsi_txbp_has_resource(tx_priv));
}

static void test_slsi_txbp_get_max_qlen(struct kunit *test)
{
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);
	int qidx;

	tx_priv->tx.ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	tx_priv->tx.ndev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue)*SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);
	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++)
		tx_priv->tx.ndev->_tx[qidx].qdisc = kunit_kzalloc(test, sizeof(struct Qdisc), GFP_KERNEL);

	bp_table[4] = 1;
	bp_control_policy = BP_POLICY_MAX_WEIGHT;
	bp_table_select = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_txbp_get_max_qlen(tx_priv, 0));

	bp_control_policy = BP_POLICY_MAX_WEIGHT;
	bp_table_select = 2;
	KUNIT_EXPECT_EQ(test, 0, slsi_txbp_get_max_qlen(tx_priv, 0));

	bp_control_policy = BP_POLICY_MAX_WEIGHT;
	bp_table_select = 3;
	KUNIT_EXPECT_EQ(test, 0, slsi_txbp_get_max_qlen(tx_priv, 0));

	bp_control_policy = BP_POLICY_PLAIN_FLOW_CONTROL_1;
	KUNIT_EXPECT_EQ(test, 0, slsi_txbp_get_max_qlen(tx_priv, 0));

	bp_control_policy = 3;
	KUNIT_EXPECT_EQ(test, 0, slsi_txbp_get_max_qlen(tx_priv, 0));
}

static void test_max_weight_sort(struct kunit *test)
{
	struct max_weight_element a_element;
	struct max_weight_element b_element;

	a_element.weight = 0;
	b_element.weight = 0;
	KUNIT_EXPECT_EQ(test, 1, max_weight_sort(NULL, &a_element.list, &b_element.list));

	a_element.weight = 1;
	KUNIT_EXPECT_EQ(test, -1, max_weight_sort(NULL, &a_element.list, &b_element.list));
}

static void test_slsi_txbp_budget_distribution(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct tx_struct *tx = kunit_kzalloc(test, sizeof(struct tx_struct), GFP_KERNEL);
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);
	int available_budget = SLSI_HIP_TX_DATA_SLOTS_NUM / 2;
	int budget[AC_CATEGORIES];
	int qidx;

	tx->sdev = sdev;
	tx->ndev = dev;
	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++)
		tx->assigned_q[qidx] = kunit_kzalloc(test, sizeof(struct sk_buff_head), GFP_KERNEL);

	ndev_vif->tx_netdev_data = tx_priv;
	tx_priv->tx.ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	tx_priv->tx.ndev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue)*SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);
	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++)
		tx_priv->tx.ndev->_tx[qidx].qdisc = kunit_kzalloc(test, sizeof(struct Qdisc), GFP_KERNEL);

	bp_table[4] = 1;
	bp_control_policy = BP_POLICY_MAX_WEIGHT;
	bp_table_select = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_txbp_budget_distribution(tx, available_budget, budget));

	tx->assigned_q[0]->qlen = 1024;
	tx_priv->ac_presence = 1;
	KUNIT_EXPECT_EQ(test, available_budget, slsi_txbp_budget_distribution(tx, available_budget, budget));

	bp_virtual_pressure[0] = 1;
	KUNIT_EXPECT_EQ(test, available_budget, slsi_txbp_budget_distribution(tx, available_budget, budget));

	bp_control_policy = BP_POLICY_PLAIN_FLOW_CONTROL_1;
	KUNIT_EXPECT_EQ(test, available_budget, slsi_txbp_budget_distribution(tx, available_budget, budget));
	tx->assigned_q[0]->qlen = 500;
	tx_priv->ac_presence = 1;
	KUNIT_EXPECT_EQ(test, available_budget, slsi_txbp_budget_distribution(tx, available_budget, budget));

	tx->assigned_q[0]->qlen = 0;
	tx->assigned_q[1]->qlen = 500;
	tx_priv->ac_presence = 2;
	KUNIT_EXPECT_EQ(test, available_budget, slsi_txbp_budget_distribution(tx, available_budget, budget));

	tx->assigned_q[1]->qlen = 0;
	tx->assigned_q[2]->qlen = 500;
	tx_priv->ac_presence = 4;
	KUNIT_EXPECT_EQ(test, available_budget, slsi_txbp_budget_distribution(tx, available_budget, budget));

	tx->assigned_q[2]->qlen = 0;
	tx->assigned_q[3]->qlen = 500;
	tx_priv->ac_presence = 8;
	KUNIT_EXPECT_EQ(test, available_budget, slsi_txbp_budget_distribution(tx, available_budget, budget));

	tx->assigned_q[3]->qlen = 0;
	tx->assigned_q[0]->qlen = 1024;
	tx_priv->ac_presence = 1;
	bp_control_policy = BP_POLICY_PLAIN_FLOW_CONTROL_2;
	KUNIT_EXPECT_EQ(test, available_budget, slsi_txbp_budget_distribution(tx, available_budget, budget));

	tx_priv->ac_presence = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_txbp_budget_distribution(tx, available_budget, budget));

	bp_control_policy = 3;
	KUNIT_EXPECT_EQ(test, 0, slsi_txbp_budget_distribution(tx, available_budget, budget));

	available_budget = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_txbp_budget_distribution(tx, available_budget, budget));
	bp_virtual_pressure[0] = 0;
}

static void test_slsi_txbp_napi(struct kunit *test)
{
	struct napi_cpu_info *napi_cpu_info = kunit_kzalloc(test, sizeof(struct napi_cpu_info), GFP_KERNEL);
	struct bh_struct *bh_s = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	struct tx_struct *tx = kunit_kzalloc(test, sizeof(struct tx_struct), GFP_KERNEL);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);
	int qidx;
	struct sk_buff *skb;
	int prev;

	napi_cpu_info->priv = kunit_kzalloc(test, sizeof(struct napi_priv), GFP_KERNEL);
	napi_cpu_info->priv->bh = bh_s;
	bh_s->sdev = sdev;
	bh_s->tx = tx;
	tx->ndev = dev;
	tx->sdev = sdev;
	ndev_vif->tx_netdev_data = tx_priv;
	ndev_vif->activated = 1;

	tx_priv->tx.ndev = dev;
	tx_priv->tx.ndev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue) * SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);
	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++) {
		tx_priv->tx.ndev->_tx[qidx].qdisc = kunit_kzalloc(test, sizeof(struct Qdisc), GFP_KERNEL);
		tx->assigned_q[qidx] = &tx_priv->q;
	}

	bp_table[4] = 1;
	bp_control_policy = BP_POLICY_MAX_WEIGHT;
	bp_table_select = 1;

	tx_priv->ac_presence = 1;
	tx_priv->ac_presence_update = 1;

	skb = alloc_skb(sizeof(struct fapi_signal), GFP_KERNEL);
	skb->next = tx->assigned_q[0];
	skb->prev = tx->assigned_q[0];
	fapi_set_u16(skb, id, FAPI_SAP_TYPE_MA);
	fapi_set_u16(skb, u.ma_unitdata_req.vif, 1);
	slsi_skb_cb_get(skb)->peer_idx = 2;

	tx->assigned_q[0]->qlen = 1;
	tx->assigned_q[0]->next = skb;
	tx->assigned_q[0]->prev = tx->assigned_q[0];

	ndev_vif->peer_sta_record[1] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[1]->valid = 1;

	bp_mod += 5;
	prev = txbp_priv.cod;
	txbp_priv.cod = SLSI_HIP_TX_DATA_SLOTS_NUM - SLSI_TX_BP_MBULK_GUARD;

	bh_s->cpu_affinity = kunit_kzalloc(test, sizeof(struct cpu_affinity_ctrl_info), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 1, slsi_txbp_napi(&napi_cpu_info->napi_instance, 1024));
	bp_mod -= 5;
	txbp_priv.cod = prev;
	kfree_skb(skb);
}

#ifndef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
static void test_slsi_txbp_trigger_tx(struct kunit *test)
{
}
#endif

static void test_slsi_dev_attach_post(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	slsi_dev_attach_post(sdev);
}

static void test_slsi_dev_detach_post(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	slsi_dev_detach_post(sdev);
}

static void test_slsi_tx_get_number_of_queues(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, SLSI_TX_TOTAL_QUEUES, slsi_tx_get_number_of_queues());
}

static void test_slsi_tx_setup_net_device(struct kunit *test)
{
	slsi_tx_setup_net_device(NULL);
}

static void test_slsi_peer_add_post(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, true == slsi_peer_add_post(NULL, NULL, NULL, NULL));
}

static void test_slsi_peer_remove_post(struct kunit *test)
{
	slsi_peer_remove_post(NULL, NULL, NULL, NULL);
}

static void test_slsi_vif_activated_post(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);

	ndev_vif->tx_netdev_data = tx_priv;
	KUNIT_EXPECT_TRUE(test, true == slsi_vif_activated_post(sdev, dev, ndev_vif));

	ndev_vif->tx_netdev_data = NULL;
	dev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue)*SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);

	KUNIT_EXPECT_TRUE(test, true == slsi_vif_activated_post(sdev, dev, ndev_vif));

	kfree(((struct tx_netdev_data *)ndev_vif->tx_netdev_data)->bh_tx);
	kfree(ndev_vif->tx_netdev_data);
}

static void test_slsi_vif_deactivated_post(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct tx_netdev_data *tx_priv = kzalloc(sizeof(struct tx_netdev_data), GFP_KERNEL);
	struct bh_struct *bh_tx = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);

	tx_priv->list.prev = kunit_kzalloc(test, sizeof(struct list_head), GFP_KERNEL);
	tx_priv->list.next = kunit_kzalloc(test, sizeof(struct list_head), GFP_KERNEL);
	ndev_vif->tx_netdev_data = tx_priv;
	slsi_vif_deactivated_post(sdev, dev, ndev_vif);
}

static void test_slsi_tx_ps_port_control(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);

	slsi_tx_ps_port_control(sdev, dev, peer, SLSI_STA_CONN_STATE_DISCONNECTED);
	slsi_tx_ps_port_control(sdev, dev, peer, SLSI_STA_CONN_STATE_DOING_KEY_CONFIG);
	slsi_tx_ps_port_control(sdev, dev, peer, SLSI_STA_CONN_STATE_CONNECTED);
	slsi_tx_ps_port_control(sdev, dev, peer, SLSI_STA_CONN_STATE_CONNECTING);
}

static void test_slsi_tx_tdls_update(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, true, slsi_tx_tdls_update(NULL, NULL, NULL, NULL, 0));
}

static void test_slsi_txbp_set_priority(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	struct sk_buff *skb = alloc_skb(sizeof(struct ethhdr) + 500, GFP_KERNEL);
	enum priority_set_by {
		PRIORITY_SET_BY_TOS = 0,
		PRIORITY_SET_BY_PEER
	};

	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->is_available = 1;
	skb->mac_header = 0;
	eth_hdr(skb)->h_dest[5] = 1;
	eth_hdr(skb)->h_dest[0] = 1;
	slsi_txbp_set_priority(sdev, dev, peer, skb);
	KUNIT_EXPECT_EQ(test, PRIORITY_SET_BY_TOS, skb->priority);

	eth_hdr(skb)->h_dest[5] = 0;
	eth_hdr(skb)->h_dest[0] = 0;
	peer->qos_enabled = 1;
	slsi_txbp_set_priority(sdev, dev, peer, skb);
	KUNIT_EXPECT_EQ(test, PRIORITY_SET_BY_TOS, skb->priority);

	peer->qos_map_set = 1;
	slsi_txbp_set_priority(sdev, dev, peer, skb);
	KUNIT_EXPECT_EQ(test, PRIORITY_SET_BY_PEER, skb->priority);

	peer->qos_enabled = 0;
	slsi_txbp_set_priority(sdev, dev, peer, skb);
	KUNIT_EXPECT_EQ(test, FAPI_PRIORITY_QOS_UP0, skb->priority);

	slsi_txbp_set_priority(sdev, dev, NULL, skb);
	KUNIT_EXPECT_EQ(test, FAPI_PRIORITY_QOS_UP0, skb->priority);
	kfree_skb(skb);
}

static u16 call_slsi_tx_select_queue(struct net_device *dev, struct sk_buff *skb, struct net_device *sb_dev)
{
#if (KERNEL_VERSION(5, 2, 0) <= LINUX_VERSION_CODE)
	return slsi_tx_select_queue(dev, skb, sb_dev);
#endif
}

static void test_slsi_tx_select_queue(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	struct sk_buff *skb = alloc_skb(sizeof(struct ethhdr) + 500, GFP_KERNEL);

	ndev_vif->sdev = sdev;
	skb->mac_header = 0;
	eth_hdr(skb)->h_proto = cpu_to_be16(ETH_P_PAE);
	KUNIT_EXPECT_EQ(test, SLSI_TX_PRIORITY_Q_INDEX, call_slsi_tx_select_queue(dev, skb, NULL));

	eth_hdr(skb)->h_proto = cpu_to_be16(ETH_P_ARP);
	KUNIT_EXPECT_EQ(test, SLSI_TX_ARP_Q_INDEX, call_slsi_tx_select_queue(dev, skb, NULL));

	eth_hdr(skb)->h_proto = cpu_to_be16(ETH_P_IP);
	KUNIT_EXPECT_EQ(test, 0, call_slsi_tx_select_queue(dev, skb, NULL));
	kfree_skb(skb);
}

static void test_slsi_tx_get_packet_type(struct kunit *test)
{
	struct sk_buff *skb = alloc_skb(sizeof(struct ethhdr) + 500, GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, SLSI_DATA_PKT, slsi_tx_get_packet_type(skb));

	skb->queue_mapping = SLSI_TX_DATA_QUEUE_NUM;
	KUNIT_EXPECT_EQ(test, SLSI_CTRL_PKT, slsi_tx_get_packet_type(skb));
	kfree_skb(skb);
}

static void test_slsi_tx_transmit_ctrl(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = alloc_skb(sizeof(struct ethhdr) + 500, GFP_KERNEL);
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);

	ndev_vif->sdev = sdev;
	skb->mac_header = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_tx_transmit_ctrl(sdev, dev, skb));

	eth_hdr(skb)->h_proto = cpu_to_be16(ETH_P_PAE);
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_transmit_ctrl(sdev, dev, skb));

	eth_hdr(skb)->h_proto = cpu_to_be16(ETH_P_ARP);
	KUNIT_EXPECT_EQ(test, -ENODEV, slsi_tx_transmit_ctrl(sdev, dev, skb));
	ndev_vif->tx_netdev_data = tx_priv;
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_transmit_ctrl(sdev, dev, skb));

	eth_hdr(skb)->h_proto = cpu_to_be16(ETH_P_IP);
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_transmit_ctrl(sdev, dev, skb));
	kfree_skb(skb);
}

#if defined(CONFIG_SCSC_PCIE_CHIP)
static void test_slsi_tx_ctrl_claim_complete_cb(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int qidx;

	dev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue) * SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);
	for (qidx = 0; qidx < SLSI_TX_TOTAL_QUEUES; qidx++)
		dev->_tx[qidx].qdisc = kunit_kzalloc(test, sizeof(struct Qdisc), GFP_KERNEL);

	slsi_tx_ctrl_claim_complete_cb(NULL, (void *)dev);
	KUNIT_EXPECT_EQ(test, ndev_vif->mx_claim_tx_ctrl, false);

	dev->_tx[SLSI_TX_PRIORITY_Q_INDEX].qdisc->q.qlen = 1;
	slsi_tx_ctrl_claim_complete_cb(NULL, (void *)dev);
	KUNIT_EXPECT_EQ(test, ndev_vif->mx_claim_tx_ctrl, true);
}
#endif

static void test_slsi_tx_mlme_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = alloc_skb(sizeof(struct ethhdr) + 500, GFP_KERNEL);
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);

	ndev_vif->sdev = sdev;
	skb->mac_header = 0;
	ndev_vif->tx_netdev_data = tx_priv;
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_mlme_ind(sdev, dev, skb));
	kfree_skb(skb);
}

static void test_slsi_tx_mlme_cfm(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = alloc_skb(sizeof(struct ethhdr) + 500, GFP_KERNEL);
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);

	ndev_vif->sdev = sdev;
	skb->mac_header = 0;
	ndev_vif->tx_netdev_data = tx_priv;
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_mlme_cfm(sdev, dev, skb));
	kfree_skb(skb);
}

static void test_slsi_txbp_schedule_napi(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);
	struct tx_struct *tx_info;
	int qidx;

	ndev_vif->tx_netdev_data = tx_priv;
	tx_info = &tx_priv->tx;
	tx_priv->tx.ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	tx_priv->tx.ndev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue)*SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);
	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++)
		tx_priv->tx.ndev->_tx[qidx].qdisc = kunit_kzalloc(test, sizeof(struct Qdisc), GFP_KERNEL);
#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	tx_info->lbm_bh_state = 1;
#else
	tx_info->target_cpu = smp_processor_id();
#endif
	slsi_txbp_schedule_napi(dev, tx_priv);

	test_and_set_bit(SLSI_TX_NAPI_ENABLED, &tx_info->napi_state);
	slsi_txbp_schedule_napi(dev, tx_priv);
}

#if defined(CONFIG_SCSC_PCIE_CHIP)
static void test_slsi_tx_mx_service_claim_complete_cb(struct kunit *test)
{
	struct tx_struct *tx = kunit_kzalloc(test, sizeof(struct tx_struct), GFP_KERNEL);
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);
	struct tx_struct *tx_info;
	int qidx;

	ndev_vif->tx_netdev_data = tx_priv;
	tx_info = &tx_priv->tx;
	tx_priv->tx.ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	tx_priv->tx.ndev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue) * SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);
	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++)
		tx_priv->tx.ndev->_tx[qidx].qdisc = kunit_kzalloc(test, sizeof(struct Qdisc), GFP_KERNEL);
#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	tx_info->lbm_bh_state = 1;
#else
	tx_info->target_cpu = smp_processor_id();
#endif
	test_and_set_bit(SLSI_TX_NAPI_ENABLED, &tx_info->napi_state);
	tx->ndev = dev;
	ndev_vif->tx_netdev_data = tx_priv;
	slsi_tx_mx_service_claim_complete_cb(NULL, (void *)tx);
	KUNIT_EXPECT_TRUE(test, test_bit(SLSI_TX_LBM_RUNNING, &tx_info->lbm_bh_state));
}
#endif

static void test_slsi_txbp_enqueue(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);
	struct sk_buff *skb = alloc_skb(sizeof(struct ethhdr) + 500, GFP_KERNEL);
	int qidx;

	ndev_vif->tx_netdev_data = tx_priv;
	tx_priv->tx.ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	tx_priv->tx.ndev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue) * SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);
	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++)
		tx_priv->tx.ndev->_tx[qidx].qdisc = kunit_kzalloc(test, sizeof(struct Qdisc), GFP_KERNEL);
	tx_priv->q[0].qlen = 0;

#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	tx_priv->bh_tx = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	tx_priv->bh_tx->cpu_affinity = kunit_kzalloc(test, sizeof(struct cpu_affinity_ctrl_info), GFP_KERNEL);
	tx_priv->bh_tx->cpu_affinity->state = TRAFFIC_MON_CLIENT_STATE_HIGH;
#endif

	bp_table[4] = 1;
	bp_control_policy = BP_POLICY_PLAIN_FLOW_CONTROL_2 + 1;
	bp_table_select = 1;
	slsi_txbp_enqueue(dev, ndev_vif, skb);
	kfree_skb(skb);
}

static void test_slsi_tx_transmit_ap_multicast(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);
	struct sk_buff *skb = alloc_skb(sizeof(struct ethhdr) + 1024, GFP_KERNEL);
	int qidx;

	skb->mac_header = 0;
	ndev_vif->tx_netdev_data = tx_priv;
	skb_reserve(skb, fapi_sig_size(ma_unitdata_req) + 50);
	tx_priv->tx.ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	tx_priv->tx.ndev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue)*SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);
	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++)
		tx_priv->tx.ndev->_tx[qidx].qdisc = kunit_kzalloc(test, sizeof(struct Qdisc), GFP_KERNEL);
	tx_priv->q[0].qlen = 513;

#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	tx_priv->bh_tx = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	tx_priv->bh_tx->cpu_affinity = kunit_kzalloc(test, sizeof(struct cpu_affinity_ctrl_info), GFP_KERNEL);
#endif

	KUNIT_EXPECT_EQ(test, NETDEV_TX_OK, slsi_tx_transmit_ap_multicast(sdev, dev, skb));
	kfree_skb(skb);
}

#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
static void test_slsi_tx_transmit_nan_multicast(struct kunit *test)
{
}
#endif

static void test_slsi_tx_transmit_normal(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);
	struct sk_buff *skb = alloc_skb(sizeof(struct ethhdr) + 500, GFP_KERNEL);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	int qidx;

	skb->mac_header = 0;
	ndev_vif->tx_netdev_data = tx_priv;
	tx_priv->tx.ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	tx_priv->tx.ndev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue)*SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);
	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++)
		tx_priv->tx.ndev->_tx[qidx].qdisc = kunit_kzalloc(test, sizeof(struct Qdisc), GFP_KERNEL);

#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	tx_priv->bh_tx = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	tx_priv->bh_tx->cpu_affinity = kunit_kzalloc(test, sizeof(struct cpu_affinity_ctrl_info), GFP_KERNEL);
#endif

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_tx_transmit_normal(sdev, dev, skb));

	ndev_vif->peer_sta_record[1] = peer;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_tx_transmit_normal(sdev, dev, skb));

	peer->authorized = 1;
	peer->qos_enabled = 1;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.tdls_enabled = 1;
	memcpy(ndev_vif->peer_sta_record[1]->address, eth_hdr(skb)->h_dest, ETH_ALEN);
	ndev_vif->peer_sta_record[1]->valid = true;
	peer->aid = SLSI_TDLS_PEER_INDEX_MIN;
	skb_reserve(skb, fapi_sig_size(ma_unitdata_req) + 50);
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_transmit_normal(sdev, dev, skb));
	kfree_skb(skb);
	skb = alloc_skb(sizeof(struct ethhdr) + 500, GFP_KERNEL);
	skb_reserve(skb, fapi_sig_size(ma_unitdata_req) + 50);

	ndev_vif->sta.tdls_enabled = 0;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = peer;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = true;
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_transmit_normal(sdev, dev, skb));
	kfree_skb(skb);
	skb = alloc_skb(sizeof(struct ethhdr) + 500, GFP_KERNEL);
	skb_reserve(skb, fapi_sig_size(ma_unitdata_req) + 50);

	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	memcpy(ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->address, eth_hdr(skb)->h_dest, ETH_ALEN);
	peer->qos_enabled = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_transmit_normal(sdev, dev, skb));
	kfree_skb(skb);
}

static void test_slsi_tx_transmit_data(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);
	struct sk_buff *skb = alloc_skb(sizeof(struct ethhdr) + 500, GFP_KERNEL);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	int qidx;

	skb->mac_header = 0;
	ndev_vif->activated = 1;
	eth_hdr(skb)->h_dest[0] = 1;
	KUNIT_EXPECT_EQ(test, -ENODEV, slsi_tx_transmit_data(sdev, dev, skb));

	ndev_vif->tx_netdev_data = tx_priv;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	tx_priv->tx.ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	tx_priv->tx.ndev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue)*SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);
	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++)
		tx_priv->tx.ndev->_tx[qidx].qdisc = kunit_kzalloc(test, sizeof(struct Qdisc), GFP_KERNEL);

#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	tx_priv->bh_tx = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	tx_priv->bh_tx->cpu_affinity = kunit_kzalloc(test, sizeof(struct cpu_affinity_ctrl_info), GFP_KERNEL);
#endif
	skb_reserve(skb, fapi_sig_size(ma_unitdata_req) + 50);

	KUNIT_EXPECT_EQ(test, 0, slsi_tx_transmit_data(sdev, dev, skb));
	kfree_skb(skb);
	skb = alloc_skb(sizeof(struct ethhdr) + 500, GFP_KERNEL);
	skb_reserve(skb, fapi_sig_size(ma_unitdata_req) + 50);

	eth_hdr(skb)->h_dest[0] = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_tx_transmit_data(sdev, dev, skb));
	kfree_skb(skb);
}

static void test_slsi_tx_done(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);
	int qidx;

	tx_priv->tx.ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	tx_priv->tx.ndev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue)*SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);
	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++)
		tx_priv->tx.ndev->_tx[qidx].qdisc = kunit_kzalloc(test, sizeof(struct Qdisc), GFP_KERNEL);

	txbp_priv.vif_list.next = &tx_priv->list;
	txbp_priv.vif_list.prev = &txbp_priv.vif_list;
	tx_priv->list.next = &txbp_priv.vif_list;
	tx_priv->list.prev = &txbp_priv.vif_list;

	KUNIT_EXPECT_EQ(test, 0, slsi_tx_done(sdev, 0, 0));

	sdev->netdev[1] = dev;
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_done(sdev, 1, 0));

	ndev_vif->tx_netdev_data = tx_priv;
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_done(sdev, 1, 0));
}

static void test_slsi_tx_transmit_lower(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct tx_netdev_data *tx_priv = kunit_kzalloc(test, sizeof(struct tx_netdev_data), GFP_KERNEL);
	struct sk_buff *skb = alloc_skb(sizeof(struct ethhdr) + 500, GFP_KERNEL);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	int qidx;

	skb->mac_header = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_tx_transmit_lower(sdev, skb));

	sdev->netdev[0] = dev;
	eth_hdr(skb)->h_dest[0] = 1;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->tx_netdev_data = tx_priv;
	tx_priv->tx.ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	tx_priv->tx.ndev->_tx = kunit_kzalloc(test, sizeof(struct netdev_queue)*SLSI_TX_TOTAL_QUEUES, GFP_KERNEL);
	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++)
		tx_priv->tx.ndev->_tx[qidx].qdisc = kunit_kzalloc(test, sizeof(struct Qdisc), GFP_KERNEL);

#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	tx_priv->bh_tx = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	tx_priv->bh_tx->cpu_affinity = kunit_kzalloc(test, sizeof(struct cpu_affinity_ctrl_info), GFP_KERNEL);
#endif

	KUNIT_EXPECT_EQ(test, NETDEV_TX_OK, slsi_tx_transmit_lower(sdev, skb));

	eth_hdr(skb)->h_dest[0] = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_tx_transmit_lower(sdev, skb));

	fapi_set_u16(skb, u.ma_unitdata_req.vif, 0);
	fapi_set_u16(skb, id, MA_UNITDATA_REQ);
	sdev->netdev[0] = dev;

	ndev_vif->peer_sta_record[1] = peer;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.tdls_enabled = 1;
	memcpy(ndev_vif->peer_sta_record[1]->address, eth_hdr(skb)->h_dest, ETH_ALEN);
	ndev_vif->peer_sta_record[1]->valid = true;
	KUNIT_EXPECT_EQ(test, NETDEV_TX_OK, slsi_tx_transmit_lower(sdev, skb));

	fapi_set_u16(skb, u.ma_unitdata_req.flow_id, ((FAPI_PRIORITY_CONTENTION >> 8) & 0xff));
	KUNIT_EXPECT_EQ(test, NETDEV_TX_OK, slsi_tx_transmit_lower(sdev, skb));
	kfree_skb(skb);
}

static int txbp_test_init(struct kunit *test)
{
	test_dev_init(test);
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	txbp_priv.cod = 0;
	txbp_priv.vif_cnt = 1;
	txbp_priv.vif_list.next = &txbp_priv.vif_list;
	txbp_priv.vif_list.prev = &txbp_priv.vif_list;
	bp_log = 0xFFF;
	return 0;
}

static void txbp_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case txbp_test_cases[] = {
#if defined(CONFIG_SCSC_PCIE_CHIP)
	KUNIT_CASE(test_slsi_tx_stop_all_queues),
	KUNIT_CASE(test_slsi_tx_wake_all_queues),
#endif
	KUNIT_CASE(test_slsi_tx_timeout),
	KUNIT_CASE(test_slsi_tx_get_cod),
	KUNIT_CASE(test_slsi_txbp_has_resource),
	KUNIT_CASE(test_slsi_txbp_get_max_qlen),
	KUNIT_CASE(test_max_weight_sort),
	KUNIT_CASE(test_slsi_txbp_budget_distribution),
	KUNIT_CASE(test_slsi_txbp_napi),
#ifndef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	KUNIT_CASE(test_slsi_txbp_trigger_tx),
#endif
	KUNIT_CASE(test_slsi_dev_attach_post),
	KUNIT_CASE(test_slsi_dev_detach_post),
	KUNIT_CASE(test_slsi_tx_get_number_of_queues),
	KUNIT_CASE(test_slsi_tx_setup_net_device),
	KUNIT_CASE(test_slsi_peer_add_post),
	KUNIT_CASE(test_slsi_peer_remove_post),
	KUNIT_CASE(test_slsi_vif_activated_post),
	KUNIT_CASE(test_slsi_vif_deactivated_post),
	KUNIT_CASE(test_slsi_tx_ps_port_control),
	KUNIT_CASE(test_slsi_tx_tdls_update),
	KUNIT_CASE(test_slsi_txbp_set_priority),
	KUNIT_CASE(test_slsi_tx_select_queue),
	KUNIT_CASE(test_slsi_tx_get_packet_type),
	KUNIT_CASE(test_slsi_tx_transmit_ctrl),
#if defined(CONFIG_SCSC_PCIE_CHIP)
	KUNIT_CASE(test_slsi_tx_ctrl_claim_complete_cb),
#endif
	KUNIT_CASE(test_slsi_tx_mlme_ind),
	KUNIT_CASE(test_slsi_tx_mlme_cfm),
	KUNIT_CASE(test_slsi_txbp_schedule_napi),
#if defined(CONFIG_SCSC_PCIE_CHIP)
	KUNIT_CASE(test_slsi_tx_mx_service_claim_complete_cb),
#endif
	KUNIT_CASE(test_slsi_txbp_enqueue),
	KUNIT_CASE(test_slsi_tx_transmit_ap_multicast),
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
	KUNIT_CASE(test_slsi_tx_transmit_nan_multicast),
#endif
	KUNIT_CASE(test_slsi_tx_transmit_normal),
	KUNIT_CASE(test_slsi_tx_transmit_data),
	KUNIT_CASE(test_slsi_tx_done),
	KUNIT_CASE(test_slsi_tx_transmit_lower),
	{}
};

static struct kunit_suite txbp_test_suite[] = {
	{
		.name = "kunit-txbp-test",
		.test_cases = txbp_test_cases,
		.init = txbp_test_init,
		.exit = txbp_test_exit,
	}
};

kunit_test_suites(txbp_test_suite);
