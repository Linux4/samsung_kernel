// SPDX-License-Identifier: <SPDX License Expression>
/*****************************************************************************
 *
 * Copyright (c) 2021 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/netdevice.h>
#include <linux/smp.h>
#include <linux/list_sort.h>
#include <net/sch_generic.h>
#include <scsc/scsc_warn.h>

#include "tx_api.h"
#include "netif.h"
#include "hip.h"
#include "mgt.h"
#include "tx.h"
#include "tdls_manager.h"
#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
#include "load_manager.h"
#endif

#define SLSI_HOST_TAG_ARP_MASK         BIT(15)

#define LOG_FILTER_Q_STATUS            (0x1)
#define LOG_FILTER_RESOURCE            (0x2)
#define LOG_FILTER_AC_PRESENCE         (0x4)
#define LOG_FILTER_TX_FAILURE          (0x8)
#define LOG_FILTER_TX_NAPI             (0x10)
#define LOG_FILTER_SHOW_COD            (0x20)

#define TXBP_NAPI_CPU_NORM 0
#define TXBP_NAPI_CPU_PERF 5

static int bp_queue_length = NAPI_POLL_WEIGHT;
module_param(bp_queue_length, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bp_queue_length, "BP queue length");

static int bp_queue_scaling_factor = SLSI_HIP_TX_DATA_SLOTS_NUM / NAPI_POLL_WEIGHT;
module_param(bp_queue_scaling_factor, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bp_queue_scaling_factor,
		 "BP queue Scaling factor. Actual queue length is (bp_queue_length * bp_queue_scaling_factor).");

static int bp_mod = SLSI_HIP_TX_DATA_SLOTS_NUM;
module_param(bp_mod, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bp_mod, "Maximum number of outstanding packet");

static int bp_ac_presence_check_interval = 100;
module_param(bp_ac_presence_check_interval, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bp_ac_presence_check_interval, "AC presence check interval");

static int bp_trigger_min = NAPI_POLL_WEIGHT;
module_param(bp_trigger_min, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bp_trigger_min, "Minimum number of free-mbulk to schedule transmission poll");

static int bp_napi_budget = NAPI_POLL_WEIGHT;
module_param(bp_napi_budget, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bp_napi_budget, "Maximum number of packet transmission in a single transmission poll");

#define BP_POLICY_MAX_WEIGHT           (0)
#define BP_POLICY_PLAIN_FLOW_CONTROL_1 (1)
#define BP_POLICY_PLAIN_FLOW_CONTROL_2 (2)
static int bp_control_policy = BP_POLICY_MAX_WEIGHT;
module_param(bp_control_policy, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bp_control_policy, "BP control policy");

static int bp_virtual_pressure[4] = {0};
static int bp_virtual_pressure_count = 4;
module_param_array(bp_virtual_pressure, int, &bp_virtual_pressure_count, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bp_virtual_pressure, "bp virtual pressure");

static int bp_table_select = 1;
module_param(bp_table_select, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bp_table_select, "bp resource table select. Without VO_BA:1, With VO_BA:2, manual:0");

static int bp_table[5] = {0};
static int bp_table_cnt = 5;
module_param_array(bp_table, int, &bp_table_cnt, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bp_table, "bp resource table configuration");

static int bp_log = LOG_FILTER_TX_FAILURE | LOG_FILTER_SHOW_COD;
module_param(bp_log, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bp_log, "bp log");

static int bp_fixed_cpu;
module_param(bp_fixed_cpu, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bp_fixed_cpu, "bp use dedicated cpu for napi");

static int bp_q_wake = 2;
module_param(bp_q_wake, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(bp_q_wake, "bp queue wake denominator");

/**
 * Resource allocation strategy.
 * Indexd by ac_presence.
 * This table could be tunned proportional values with repect to each AC
 */
static u8 maxweight_resource_table[16][5] = {
     /* BE   BK  VI  VO  SUM */
	{0,  0,  0,  0,  64},
	{64, 0,  0,  0,  64},
	{0,  64, 0,  0,  64},
	{39, 25, 0,  0,  64},
	{0,  0,  64, 0,  64},
	{16, 0,  48, 0,  64},
	{0,  25, 39, 0,  64},
	{24, 16, 24, 0,  64},
	{0,  0,  0,  64, 64},
	{48, 0,  0,  16, 64},
	{0,  57, 0,  7,  64},
	{37, 24, 0,  3,  64},
	{0,  0,  48, 16, 64},
	{31, 0,  30, 3,  64},
	{0,  24, 37, 3,  64},
	{24, 15, 23, 2,  64}
};

static u8 maxweight_resource_table_vo_ba[16][5] = {
     /* BE   BK  VI  VO  SUM */
	{0,  0,  0,  0,  64},
	{64, 0,  0,  0,  64},
	{0,  64, 0,  0,  64},
	{39, 25, 0,  0,  64},
	{0,  0,  64, 0,  64},
	{32, 0,  32, 0,  64},
	{0,  25, 39, 0,  64},
	{24, 16, 24, 0,  64},
	{0,  0,  0,  64, 64},
	{48, 0,  0,  16, 64},
	{0,  4,  0,  60, 64},
	{20, 20, 0,  24, 64},
	{0,  0,  12, 52, 64},
	{28, 0,  26, 10, 64},
	{0,  20, 20, 24, 64},
	{22, 16, 22, 4,  64}
};

static u8 plain_flowcontrol_resource_table[16][5] = {
     /* BE   BK  VI  VO  SUM */
	{0,  0,  0,  0,  64},
	{64, 0,  0,  0,  64},
	{0,  64, 0,  0,  64},
	{32, 32, 0,  0,  64},
	{0,  0,  64, 0,  64},
	{32, 0,  32, 0,  64},
	{0,  32, 32, 0,  64},
	{21, 22, 21, 0,  64},
	{0,  0,  0,  64, 64},
	{32, 0,  0,  32, 64},
	{0,  32, 0,  32, 64},
	{21, 21, 0,  22, 64},
	{0,  0,  32, 32, 64},
	{21, 0,  21, 22, 64},
	{0,  21, 21, 22, 64},
	{16, 16, 16, 16, 64}
};

static struct txbp_data txbp_priv;

struct max_weight_element {
	struct list_head list;
	u8 ac;
	int service;
	int weight;
};

#if defined(CONFIG_SCSC_PCIE_CHIP)
void slsi_tx_stop_all_queues(struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tx_netdev_data *tx_priv = NULL;
	u8 q_num = 0;


	tx_priv = (struct tx_netdev_data *)ndev_vif->tx_netdev_data;
	if (!tx_priv) {
		SLSI_WARN_NODEV("invalid Tx priv\n");
		return;
	}

	SLSI_NET_INFO(dev, "\n");
	for (q_num = 0; q_num < SLSI_TX_TOTAL_QUEUES; q_num++) {
		netif_stop_subqueue(dev, q_num);
	}
}

void slsi_tx_wake_all_queues(struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tx_netdev_data *tx_priv = NULL;
	u8 q_num = 0;

	tx_priv = (struct tx_netdev_data *)ndev_vif->tx_netdev_data;
	if (!tx_priv) {
		SLSI_WARN_NODEV("invalid Tx priv\n");
		return;
	}

	SLSI_NET_INFO(dev, "\n");
	for (q_num = 0; q_num < SLSI_TX_TOTAL_QUEUES; q_num++) {
		netif_wake_subqueue(dev, q_num);
	}
}
#endif

void slsi_txbp_suspend(struct slsi_dev *sdev)
{
#ifndef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	struct tx_netdev_data *tx_priv;
	struct tx_struct *tx_info;
	struct netdev_vif *ndev_vif;

	SLSI_MUTEX_LOCK(sdev->start_stop_mutex);
	list_for_each_entry(tx_priv, &txbp_priv.vif_list, list) {
		tx_info = &tx_priv->tx;

		if (test_and_clear_bit(SLSI_TX_NAPI_ENABLED, &tx_info->napi_state))
			napi_disable(&tx_info->napi);
	}
#else
	SLSI_MUTEX_LOCK(sdev->start_stop_mutex);
	slsi_lbm_disable_tx_napi();
#endif
	SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
	return;
}

void slsi_txbp_resume(struct slsi_dev *sdev)
{
#ifndef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	struct tx_netdev_data *tx_priv;
	struct tx_struct *tx_info;
	struct netdev_vif *ndev_vif;

	SLSI_MUTEX_LOCK(sdev->start_stop_mutex);
	list_for_each_entry(tx_priv, &txbp_priv.vif_list, list) {
		tx_info = &tx_priv->tx;

		if (!test_and_set_bit(SLSI_TX_NAPI_ENABLED, &tx_info->napi_state))
			napi_enable(&tx_info->napi);
	}
#else
	SLSI_MUTEX_LOCK(sdev->start_stop_mutex);
	slsi_lbm_enable_tx_napi();
#endif
	SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
	return;
}

void slsi_tx_timeout(struct net_device *dev)
{
	struct netdev_queue *txq;
	struct Qdisc *qdisc;
	int qidx;

	read_lock(&txbp_priv.vif_lock);
	if (list_empty(&txbp_priv.vif_list)) {
		read_unlock(&txbp_priv.vif_lock);
		return;
	}
	read_unlock(&txbp_priv.vif_lock);

	SLSI_WARN_NODEV("net_device %s\n", dev->name);
	for (qidx = 0; qidx < SLSI_TX_TOTAL_QUEUES; qidx++) {
		txq = &dev->_tx[qidx];
		qdisc = txq->qdisc;
		SLSI_NET_INFO(dev, "q[%d]: devq state 0x%lx timeo %lu\n", qidx, txq->state, txq->trans_timeout);
		SLSI_NET_INFO(dev, "q[%d]: qdisc name %s state 0x%lx flag 0x%x qlen %u\n",
			      qidx, qdisc->ops->id, qdisc->state, qdisc->flags, qdisc->q.qlen);
	}
}

static void show_cod(struct tx_netdev_data *tx_priv)
{
	if (bp_log & LOG_FILTER_SHOW_COD) {
		SLSI_NET_DBG3(tx_priv->tx.ndev, SLSI_TX, "BP: G-COD: %u NETDEV-COD: %u AC-COD: [BE:%u BK:%u VI:%u VO:%u]\n",
			      txbp_priv.cod, tx_priv->netdev_cod, tx_priv->ac_cod[SLSI_TRAFFIC_Q_BE],
			      tx_priv->ac_cod[SLSI_TRAFFIC_Q_BK], tx_priv->ac_cod[SLSI_TRAFFIC_Q_VI],
			      tx_priv->ac_cod[SLSI_TRAFFIC_Q_VO]);
	}
}

int slsi_tx_get_cod(struct slsi_dev *sdev, void *tx_priv, int *gcod, int *netdev_cod, int *ac_cod)
{
	if (WARN_ON(!sdev) || WARN_ON(!tx_priv) || WARN_ON(!gcod) || WARN_ON(!netdev_cod) || WARN_ON(!ac_cod))
		return -EINVAL;

	read_lock_bh(&txbp_priv.cod_lock);

	*gcod = txbp_priv.cod;
	*netdev_cod = ((struct tx_netdev_data *)tx_priv)->netdev_cod;
	ac_cod[SLSI_TRAFFIC_Q_BE] = ((struct tx_netdev_data *)tx_priv)->ac_cod[SLSI_TRAFFIC_Q_BE];
	ac_cod[SLSI_TRAFFIC_Q_BK] = ((struct tx_netdev_data *)tx_priv)->ac_cod[SLSI_TRAFFIC_Q_BK];
	ac_cod[SLSI_TRAFFIC_Q_VI] = ((struct tx_netdev_data *)tx_priv)->ac_cod[SLSI_TRAFFIC_Q_VI];
	ac_cod[SLSI_TRAFFIC_Q_VO] = ((struct tx_netdev_data *)tx_priv)->ac_cod[SLSI_TRAFFIC_Q_VO];

	read_unlock_bh(&txbp_priv.cod_lock);

	return 0;
}

int slsi_tx_get_gcod(void)
{
	int gcod;

	read_lock_bh(&txbp_priv.cod_lock);
	gcod = txbp_priv.cod;
	read_unlock_bh(&txbp_priv.cod_lock);

	return gcod;
}

static __always_inline u8 slsi_txbp_check_ac_presence(struct tx_netdev_data *tx_priv)
{
	u8 ac_presence = 0;
	u8 qidx = 0;

	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++) {
		if (READ_ONCE(tx_priv->q[qidx].qlen) || qdisc_qlen(tx_priv->tx.ndev->_tx[qidx].qdisc) ||
		    tx_priv->ac_cod[qidx])
			ac_presence |= 0x1 << qidx;
	}

	ac_presence |= (tx_priv->ac_presence | tx_priv->ac_presence_update);
	if (bp_log & LOG_FILTER_AC_PRESENCE)
		SLSI_NET_INFO(tx_priv->tx.ndev, "BP: AC Presence [BE:%hhu BK:%hhu VI:%hhu VO:%hhu]\n",
			      (ac_presence & (0x1 << SLSI_TRAFFIC_Q_BE) ? 1 : 0),
			      (ac_presence & (0x1 << SLSI_TRAFFIC_Q_BK) ? 1 : 0),
			      (ac_presence & (0x1 << SLSI_TRAFFIC_Q_VI) ? 1 : 0),
			      (ac_presence & (0x1 << SLSI_TRAFFIC_Q_VO) ? 1 : 0));
	return ac_presence;
}

static __always_inline int slsi_txbp_has_resource(struct tx_netdev_data *tx_priv)
{
	struct tx_struct *tx_info;
	int mod;
	int cod;
	int trigger_min;
	int ret = 0;
	u8 ac_presence;

	if (!tx_priv) {
		SLSI_WARN_NODEV("BP: tx_priv is NULL\n");
		return ret;
	}

	tx_info = &tx_priv->tx;
	if (!tx_info) {
		SLSI_WARN_NODEV("BP: tx_info is NULL\n");
		return ret;
	}

	ac_presence = slsi_txbp_check_ac_presence(tx_priv);
	if ((ac_presence == (0x1 << SLSI_TRAFFIC_Q_BE)) || (ac_presence == (0x1 << SLSI_TRAFFIC_Q_BK)) ||
	    (ac_presence == (0x1 << SLSI_TRAFFIC_Q_VI)) || (ac_presence == (0x1 << SLSI_TRAFFIC_Q_VO)))
		trigger_min = 5;
	else
		trigger_min = bp_trigger_min;

	read_lock(&txbp_priv.cod_lock);
	/* Global domain */
	mod = bp_mod - SLSI_TX_BP_MBULK_GUARD;
	cod = txbp_priv.cod;
	if (mod - cod < trigger_min)
		goto end;

	if (!txbp_priv.vif_cnt)
		goto end;
	/* Netdev domain */
	mod /= txbp_priv.vif_cnt;
	cod = tx_priv->netdev_cod;
	if (mod - cod < trigger_min)
		goto end;

	ret = mod - cod;
	if (ret < 0) {
		ret = 0;
		goto end;
	}

	if (bp_log & LOG_FILTER_RESOURCE)
		SLSI_NET_INFO(tx_info->ndev, "BP: resource available %d\n", ret);
end:
	read_unlock(&txbp_priv.cod_lock);
	return ret;
}

static __always_inline u32 slsi_txbp_get_max_qlen(struct tx_netdev_data *tx_priv, u8 ac)
{
	u8 ac_presence = slsi_txbp_check_ac_presence(tx_priv);
	u32 ndev_mod;
	u32 scaled_q_length = 0;
	u32 max_q_length = 0;

	if (!txbp_priv.vif_cnt) {
		SLSI_WARN_NODEV("BP: vif count 0\n");
		return max_q_length;
	}
	ndev_mod = bp_mod / txbp_priv.vif_cnt;

	switch (bp_control_policy) {
	case BP_POLICY_MAX_WEIGHT:
		if (bp_table_select == 1) {
			scaled_q_length = bp_queue_length * maxweight_resource_table[ac_presence][ac] *
				bp_queue_scaling_factor / maxweight_resource_table[ac_presence][4];
			max_q_length = ndev_mod * maxweight_resource_table[ac_presence][ac] /
				maxweight_resource_table[ac_presence][4];
		} else if (bp_table_select == 2) {
			scaled_q_length = bp_queue_length * maxweight_resource_table_vo_ba[ac_presence][ac] *
				bp_queue_scaling_factor / maxweight_resource_table_vo_ba[ac_presence][4];
			max_q_length = ndev_mod * maxweight_resource_table_vo_ba[ac_presence][ac] /
				maxweight_resource_table_vo_ba[ac_presence][4];
		} else {
			scaled_q_length = bp_queue_length * bp_table[ac] * bp_queue_scaling_factor / bp_table[4];
			max_q_length = ndev_mod * bp_table[ac] / bp_table[4];
		}
		break;
	case BP_POLICY_PLAIN_FLOW_CONTROL_1:
	case BP_POLICY_PLAIN_FLOW_CONTROL_2:
		scaled_q_length = bp_queue_length * plain_flowcontrol_resource_table[ac_presence][ac] *
			bp_queue_scaling_factor / plain_flowcontrol_resource_table[ac_presence][4];
		max_q_length = ndev_mod * plain_flowcontrol_resource_table[ac_presence][ac] /
			plain_flowcontrol_resource_table[ac_presence][4];
		break;
	default:
		SLSI_WARN_NODEV("BP: unknown bp control policy %d\n", bp_control_policy);
		break;
	}

	return (scaled_q_length > max_q_length ? max_q_length : scaled_q_length);
}

/**
 * max_weight_sort must return a negative value if @a
 * should sort before @b, and a positive value if @a should sort after
 * @b.
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 13, 0))
static int max_weight_sort(void *priv, const struct list_head *a, const struct list_head *b)
{
#else
static int max_weight_sort(void *priv, struct list_head *a, struct list_head *b)
{
#endif
	struct max_weight_element *a_element = container_of(a, struct max_weight_element, list);
	struct max_weight_element *b_element = container_of(b, struct max_weight_element, list);

	if (a_element->weight > b_element->weight)
		return -1;
	return 1;
}

static __always_inline int slsi_txbp_budget_distribution(struct tx_struct *tx, int available_budget, int *budget)
{
	struct netdev_vif *ndev_vif = netdev_priv(tx->ndev);
	struct tx_netdev_data *tx_priv = (struct tx_netdev_data *)ndev_vif->tx_netdev_data;
	u8 qidx = 0;
	u8 ac_cnt = 0;
	u8 ac;
	u32 ac_queue[AC_CATEGORIES] = {0};
	u32 max_ac_queue_len[AC_CATEGORIES];
	u32 ac_qlen;
	int total_allocated = 0;
	int available_mbulk;

	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++) {
		if (tx->assigned_q[qidx])
			ac_queue[qidx] += READ_ONCE(tx->assigned_q[qidx]->qlen);
	}

	ac_qlen = ac_queue[SLSI_TRAFFIC_Q_BE] + ac_queue[SLSI_TRAFFIC_Q_BK] + ac_queue[SLSI_TRAFFIC_Q_VI] +
		ac_queue[SLSI_TRAFFIC_Q_VO];

	if (!ac_qlen) {
		if (bp_log & LOG_FILTER_RESOURCE) {
			SLSI_NET_INFO(tx->ndev, "BP: All assigned queues are empty\n");
			for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++)
				SLSI_NET_INFO(tx->ndev, "BP: Actual queue length %u = %u\n",
					      qidx, READ_ONCE(tx_priv->q[qidx].qlen));
		}
		return 0;
	}

	for (ac = SLSI_TRAFFIC_Q_BE; ac < AC_CATEGORIES; ac++) {
		ac_qlen = slsi_txbp_get_max_qlen(tx_priv, ac);
		max_ac_queue_len[ac] = ac_qlen;

		if (ac_qlen > 0)
			ac_cnt++;

		if (ac_qlen < ac_queue[ac])
			ac_queue[ac] = ac_qlen;
	}

	available_mbulk = slsi_txbp_has_resource(tx_priv);
	available_budget = (available_mbulk > available_budget ? available_budget : available_mbulk);

	switch (bp_control_policy) {
	case BP_POLICY_MAX_WEIGHT:
	{
		/* Max weight calculation. */
		struct list_head sorted_list;
		struct max_weight_element elements[AC_CATEGORIES];
		struct max_weight_element *element;
		bool use_custom_virtual_pressure;

		INIT_LIST_HEAD(&sorted_list);
		use_custom_virtual_pressure = bp_virtual_pressure[0] |  bp_virtual_pressure[1] |
			bp_virtual_pressure[2] | bp_virtual_pressure[3] ? true : false;
		read_lock(&txbp_priv.cod_lock);
		for (ac = SLSI_TRAFFIC_Q_BE; ac < AC_CATEGORIES; ac++) {
			elements[ac].ac = ac;
			elements[ac].service = (ac_queue[ac] > available_budget ? available_budget : ac_queue[ac]);
			if (use_custom_virtual_pressure)
				ac_queue[ac] += bp_virtual_pressure[ac];
			else
				ac_queue[ac] += max_ac_queue_len[ac] / ac_cnt;
			elements[ac].weight = elements[ac].service * (ac_queue[ac] - tx_priv->ac_cod[ac]);

			if (elements[ac].weight > 0)
				list_add(&elements[ac].list, &sorted_list);
		}
		read_unlock(&txbp_priv.cod_lock);
		list_sort(NULL, &sorted_list, max_weight_sort);

		budget[SLSI_TRAFFIC_Q_BE] = 0;
		budget[SLSI_TRAFFIC_Q_BK] = 0;
		budget[SLSI_TRAFFIC_Q_VI] = 0;
		budget[SLSI_TRAFFIC_Q_VO] = 0;

		list_for_each_entry(element, &sorted_list, list) {
			budget[element->ac] = (element->service <= available_budget - total_allocated ?
					element->service : available_budget - total_allocated);
			total_allocated += budget[element->ac];
			if (total_allocated >= available_budget)
				break;
		}
		break;
	}

	case BP_POLICY_PLAIN_FLOW_CONTROL_1:
	{
		struct list_head sorted_list;
		struct max_weight_element elements[AC_CATEGORIES];
		struct max_weight_element *element;

		INIT_LIST_HEAD(&sorted_list);
		read_lock(&txbp_priv.cod_lock);
		for (ac = SLSI_TRAFFIC_Q_BE; ac < AC_CATEGORIES; ac++) {
			elements[ac].ac = ac;
			elements[ac].service = (ac_queue[ac] > available_budget ? available_budget : ac_queue[ac]);
			elements[ac].weight = max_ac_queue_len[ac] - tx_priv->ac_cod[ac];

			if (elements[ac].weight > 0)
				list_add(&elements[ac].list, &sorted_list);
		}
		read_unlock(&txbp_priv.cod_lock);
		list_sort(NULL, &sorted_list, max_weight_sort);

		budget[SLSI_TRAFFIC_Q_BE] = 0;
		budget[SLSI_TRAFFIC_Q_BK] = 0;
		budget[SLSI_TRAFFIC_Q_VI] = 0;
		budget[SLSI_TRAFFIC_Q_VO] = 0;

		list_for_each_entry(element, &sorted_list, list) {
			budget[element->ac] = (element->service <= available_budget - total_allocated ?
					element->service : available_budget - total_allocated);
			total_allocated += budget[element->ac];
			if (total_allocated >= available_budget)
				break;
		}
		break;
	}

	case BP_POLICY_PLAIN_FLOW_CONTROL_2:
	{
		struct max_weight_element elements[AC_CATEGORIES];
		int weight_sum = 0;

		read_lock(&txbp_priv.cod_lock);
		for (ac = SLSI_TRAFFIC_Q_BE; ac < AC_CATEGORIES; ac++) {
			elements[ac].ac = ac;
			elements[ac].service = (ac_queue[ac] > available_budget ? available_budget : ac_queue[ac]);
			elements[ac].weight = max_ac_queue_len[ac] - tx_priv->ac_cod[ac];

			if (elements[ac].weight > 0)
				weight_sum += elements[ac].weight;
		}
		read_unlock(&txbp_priv.cod_lock);

		if (!weight_sum)
			return 0;

		budget[SLSI_TRAFFIC_Q_BE] = 0;
		budget[SLSI_TRAFFIC_Q_BK] = 0;
		budget[SLSI_TRAFFIC_Q_VI] = 0;
		budget[SLSI_TRAFFIC_Q_VO] = 0;

		for (ac = SLSI_TRAFFIC_Q_BE; ac < AC_CATEGORIES; ac++) {
			if (elements[ac].weight > 0) {
				budget[ac] = available_budget * elements[ac].weight / weight_sum;
				total_allocated += budget[ac];
			}
		}
		break;
	}

	default:
		SLSI_NET_WARN(tx->ndev, "BP: unknown bp control policy %d\n", bp_control_policy);
		break;
	}

	if (total_allocated == 0)
		return 0;

	if (available_budget > total_allocated) {
		if (budget[SLSI_TRAFFIC_Q_VO])
			budget[SLSI_TRAFFIC_Q_VO] += (available_budget - total_allocated);
		else if (budget[SLSI_TRAFFIC_Q_BK])
			budget[SLSI_TRAFFIC_Q_BK] += (available_budget - total_allocated);
		else if (budget[SLSI_TRAFFIC_Q_VI])
			budget[SLSI_TRAFFIC_Q_VI] += (available_budget - total_allocated);
		else if (budget[SLSI_TRAFFIC_Q_BE])
			budget[SLSI_TRAFFIC_Q_BE] += (available_budget - total_allocated);
	}

	if (bp_log & LOG_FILTER_RESOURCE)
		SLSI_NET_INFO(tx->ndev, "BP: Available resources %d [BE:%hhu BK:%hhu VI:%hhu VO:%hhu]\n",
			      available_budget, budget[SLSI_TRAFFIC_Q_BE], budget[SLSI_TRAFFIC_Q_BK],
			      budget[SLSI_TRAFFIC_Q_VI], budget[SLSI_TRAFFIC_Q_VO]);
	return available_budget;
}

static __always_inline struct sk_buff_head *slsi_txbp_select_queue(struct tx_struct *tx, u8 ac)
{
	struct sk_buff_head *q = NULL;

	if (tx->assigned_q[ac] && !skb_queue_empty(tx->assigned_q[ac]))
		q = tx->assigned_q[ac];

	return q;
}

static __always_inline void slsi_txbp_update_presence(struct tx_netdev_data *tx_priv)
{
	if (jiffies_to_msecs(jiffies - tx_priv->last_presence_check_time) > bp_ac_presence_check_interval) {
		tx_priv->last_presence_check_time = jiffies;
		tx_priv->ac_presence = tx_priv->ac_presence_update;
		tx_priv->ac_presence_update = 0;
	}
}

#ifdef CONFIG_SCSC_WLAN_SAP_POWER_SAVE
static __always_inline void slsi_txbp_check_q_status(struct net_device *dev, struct tx_netdev_data *tx_priv,
						     int qidx, int softap_suspended)
#else
static __always_inline void slsi_txbp_check_q_status(struct net_device *dev, struct tx_netdev_data *tx_priv, int qidx)
#endif
{
	struct txq_stats *stat;
	u32 max_q_len;
	ktime_t now, stop;

#ifdef CONFIG_SCSC_WLAN_SAP_POWER_SAVE
	if (__netif_subqueue_stopped(dev, qidx) && !softap_suspended) {
#else
	if (__netif_subqueue_stopped(dev, qidx)) {
#endif
		max_q_len = slsi_txbp_get_max_qlen(tx_priv, qidx);
		if (READ_ONCE(tx_priv->q[qidx].qlen) < (max_q_len / bp_q_wake) || !READ_ONCE(tx_priv->q[qidx].qlen)) {
			if (bp_log & LOG_FILTER_Q_STATUS)
				SLSI_NET_INFO(dev, "BP: Queue Start[%hu]=%u Limit=%u\n",
					      qidx, READ_ONCE(tx_priv->q[qidx].qlen), max_q_len);

			stat = &tx_priv->qstat[qidx];
			now = ktime_get();
			stop = ktime_sub(now, stat->stop);
			stat->cumulated_stop = ktime_add(stop, stat->cumulated_stop);
			stat->wake = now;
			netif_wake_subqueue(dev, qidx);
		}
	}
}

static int slsi_txbp_napi(struct napi_struct *napi, int budget)
{
#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	struct napi_cpu_info  *napi_cpu_info = container_of(napi, struct napi_cpu_info, napi_instance);
	struct bh_struct      *bh_s = napi_cpu_info->priv->bh;
	struct slsi_dev       *sdev = bh_s->sdev;
	struct tx_struct      *tx = bh_s->tx;
#else
	struct tx_struct      *tx = container_of(napi, struct tx_struct, napi);
	struct slsi_dev       *sdev = tx->sdev;
#endif
	struct netdev_vif     *ndev_vif = netdev_priv(tx->ndev);
	struct tx_netdev_data *tx_priv = NULL;
	int work_done = 0;
	int ac;
	int ac_budget_limit[AC_CATEGORIES] = {0};
	int ac_service[AC_CATEGORIES] = {0};
	int ret;
	int cod = 0;
	struct netdev_vif     *ndev_vif_peer;

	read_lock_bh(&txbp_priv.vif_lock);
	tx_priv = (struct tx_netdev_data *)ndev_vif->tx_netdev_data;
	if (!ndev_vif->activated || !tx_priv)
		goto tx_done;

#if defined(CONFIG_SCSC_PCIE_CHIP)
	spin_lock_bh(&tx->mx_claim_lock);
	/* PCIe is not claimed. */
	if (!(tx->mx_claim & MX_CLAIM_STATE_CLAIMED)) {
		/* Make sure deferred claim is called only once. */
		if (!(tx->mx_claim & MX_CLAIM_STATE_DEFERRED)) {
			if (scsc_mx_service_claim_deferred(sdev->service, slsi_tx_mx_service_claim_complete_cb, (void *)tx, WLAN_TX_DATA)) {
				slsi_tx_stop_all_queues(tx->ndev);
				tx->mx_claim |= MX_CLAIM_STATE_DEFERRED;
				spin_unlock_bh(&tx->mx_claim_lock);
				goto tx_done;
			} else {
				tx->mx_claim |= MX_CLAIM_STATE_CLAIMED;
			}
		} else {
			spin_unlock_bh(&tx->mx_claim_lock);
			goto tx_done;
		}
	}
	spin_unlock_bh(&tx->mx_claim_lock);
#endif

#if defined(CONFIG_SCSC_WLAN_TAS)
	slsi_mlme_tas_deferred_tx_sar_limit(sdev);
#endif
	slsi_txbp_update_presence(tx_priv);

	if (!slsi_txbp_has_resource(tx_priv))
		goto tx_done_check_q;

	if (!slsi_txbp_budget_distribution(tx, budget, (int *)ac_budget_limit))
		goto tx_done_check_q;

#ifndef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	if (tx->target_cpu != smp_processor_id() && cpu_online(tx->target_cpu))
		goto tx_done_check_q;
#endif

	if (ndev_vif->iftype == NL80211_IFTYPE_AP_VLAN)
		ndev_vif_peer = netdev_priv(ndev_vif->netdev_ap);
	else
		ndev_vif_peer = ndev_vif;

	for (ac = SLSI_TRAFFIC_Q_VO; ac > -1; ac--) {
		struct sk_buff_head *q;
		struct sk_buff *skb;
		struct slsi_peer *peer;
		u16 vif_index;
		u16 peer_index;

		while (ac_budget_limit[ac]) {
			q = slsi_txbp_select_queue(tx, ac);
			if (!q)
				break;

			skb = skb_dequeue(q);

			vif_index = fapi_get_u16(skb, u.ma_unitdata_req.vif);
			peer_index = slsi_skb_cb_get(skb)->peer_idx;
			if (peer_index >= SLSI_PEER_INDEX_MIN && peer_index <= SLSI_PEER_INDEX_MAX) {
				peer = ndev_vif_peer->peer_sta_record[MAP_AID_TO_QS(peer_index)];
				if (!peer || !peer->valid) {
					SLSI_DBG1(sdev, SLSI_TX, "Discard skb with invalid peer. peer_index:%d\n", peer_index);
					kfree_skb(skb);
					continue;
				}
			}

			ret = slsi_hip_transmit_frame(&sdev->hip, skb, false, vif_index, peer_index,
						       slsi_frame_priority_to_ac_queue(skb->priority));
			if (ret != NETDEV_TX_OK) {
				skb_queue_head(q, skb);
				SLSI_DBG1(sdev, SLSI_TX, "BP: Tx failed with ret:%d\n", ret);
				show_cod(tx_priv);
				if (bp_log & LOG_FILTER_TX_FAILURE)
					SLSI_NET_INFO(tx->ndev, "BP: Tx failed remaining budget %u\n", ac_budget_limit[ac]);
				goto tx_done_check_q;
			} else {
				work_done++;
				ac_budget_limit[ac]--;
				ac_service[ac]++;

				write_lock(&txbp_priv.cod_lock);
				txbp_priv.cod++;
				cod = txbp_priv.cod;
				tx_priv->netdev_cod++;
				tx_priv->ac_cod[ac]++;
				write_unlock(&txbp_priv.cod_lock);
			}

			if (bp_mod - cod < SLSI_TX_BP_MBULK_GUARD)
				goto tx_done_check_q;
		}
	}
tx_done_check_q:
	for (ac = SLSI_TRAFFIC_Q_VO; ac > -1; ac--)
#ifdef CONFIG_SCSC_WLAN_SAP_POWER_SAVE
		slsi_txbp_check_q_status(tx->ndev, tx_priv, ac, ndev_vif->softap_suspend_mode);
#else
		slsi_txbp_check_q_status(tx->ndev, tx_priv, ac);
#endif
tx_done:
	if (work_done)
		slsi_hip_from_host_intr_set(sdev->service, &sdev->hip);

	if (work_done < budget) {
#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
		clear_bit(SLSI_TX_LBM_RUNNING, &tx->lbm_bh_state);
#else
		clear_bit(SLSI_TX_NAPI_POLLING, &tx->napi_state);
#endif
#if defined(CONFIG_SCSC_PCIE_CHIP)
		spin_lock_bh(&tx->mx_claim_lock);
		if ((tx->mx_claim & MX_CLAIM_STATE_CLAIMED) && (bh_s->cpu_affinity->state == TRAFFIC_MON_CLIENT_STATE_LOW)) {
			tx->mx_claim &= ~MX_CLAIM_STATE_CLAIMED;
			scsc_mx_service_release(WLAN_TX_DATA);
		}
		spin_unlock_bh(&tx->mx_claim_lock);
#endif
		napi_complete(napi);
		if (tx_priv)
			show_cod(tx_priv);
	}

	if (bp_log & LOG_FILTER_TX_NAPI)
		SLSI_NET_INFO(tx->ndev, "BP: work_done %d budget %d\n", work_done, budget);

	read_unlock_bh(&txbp_priv.vif_lock);
	return work_done;
}

#ifndef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
static __always_inline void slsi_txbp_trigger_tx(void *data)
{
	struct tx_struct *tx = (struct tx_struct *)data;

	napi_schedule(&tx->napi);
}
#endif

void slsi_hip_setup_post(struct slsi_dev *sdev, u16 tx_slots)
{
	bp_mod = tx_slots;
	bp_queue_scaling_factor = tx_slots / NAPI_POLL_WEIGHT;
}

void slsi_dev_attach_post(struct slsi_dev *sdev)
{
	rwlock_init(&txbp_priv.cod_lock);
	rwlock_init(&txbp_priv.vif_lock);
	INIT_LIST_HEAD(&txbp_priv.vif_list);
	txbp_priv.vif_cnt = 0;
	txbp_priv.peers = 0;
#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	bp_fixed_cpu = -1;
#else
	bp_fixed_cpu = TXBP_NAPI_CPU_NORM;
#endif
}

void slsi_dev_detach_post(struct slsi_dev *sdev)
{
}

int slsi_tx_get_number_of_queues(void)
{
	/* data queue + priority and arp queues */
	return SLSI_TX_TOTAL_QUEUES;
}

__always_inline void slsi_tx_setup_net_device(struct net_device *dev)
{
}

__always_inline bool slsi_peer_add_post(struct slsi_dev *sdev, struct net_device *dev, struct netdev_vif *ndev_vif,
					struct slsi_peer *peer)
{
	txbp_priv.peers++;
	if (txbp_priv.peers == 1) {
		write_lock(&txbp_priv.cod_lock);
		txbp_priv.cod = 0;
		write_unlock(&txbp_priv.cod_lock);
	}
	return true;
}

__always_inline void slsi_peer_remove_post(struct slsi_dev *sdev, struct net_device *dev, struct netdev_vif *ndev_vif,
					   struct slsi_peer *peer)
{
	txbp_priv.peers--;
}

__always_inline void slsi_txbp_vif_budget_redistribute(struct net_device *dev, bool is_activated)
{
	struct tx_netdev_data *tx_priv = NULL;
	int mod;

	if (!txbp_priv.vif_cnt)
		return;

	mod = bp_mod / txbp_priv.vif_cnt;
	list_for_each_entry(tx_priv, &txbp_priv.vif_list, list) {
		tx_priv->netdev_mod = mod;
	}
}

__always_inline bool slsi_vif_activated_post(struct slsi_dev *sdev, struct net_device *dev, struct netdev_vif *ndev_vif)
{
	struct tx_netdev_data *tx_priv;
	struct tx_struct *tx_info;
	u8 ac;

#if defined(CONFIG_SCSC_PCIE_CHIP)
	spin_lock_init(&ndev_vif->mx_claim_tx_ctrl_lock);
#endif
	tx_priv = (struct tx_netdev_data *)ndev_vif->tx_netdev_data;
	if (tx_priv)
		return true;

	/**
	 * initialize control queues
	 * ARP Q uses byte queue limit flow control
	 */
	netdev_tx_reset_queue(&dev->_tx[SLSI_TX_ARP_Q_INDEX]);

	tx_priv = kzalloc(sizeof(*tx_priv), GFP_ATOMIC);
	if (!tx_priv)
		return false;

	tx_info = &tx_priv->tx;
#if defined(CONFIG_SCSC_PCIE_CHIP)
	spin_lock_init(&tx_info->mx_claim_lock);
	tx_info->mx_claim = 0;
#endif

#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	tx_priv->bh_tx = slsi_lbm_register_napi(sdev, slsi_txbp_napi, -1, NP_TX_0);
	if (tx_priv->bh_tx) {
		tx_priv->bh_tx->tx = tx_info;
		slsi_lbm_setup(tx_priv->bh_tx);
		slsi_lbm_register_cpu_affinity_control(tx_priv->bh_tx, NP_TX_0);
		tx_priv->dedicated_napi = slsi_lbm_get_napi_cpu(NP_TX_0, TRAFFIC_MON_CLIENT_STATE_HIGH);
	} else {
		SLSI_NET_ERR(dev, "BP: slsi_lbm_register_napi failed\n");
		kfree(tx_priv);
		return false;
	}
#else
	init_dummy_netdev(&tx_priv->napi_netdev);
	SLSI_NET_INFO(dev, "BP: tx poll budget %u\n", bp_napi_budget);
	netif_tx_napi_add(&tx_priv->napi_netdev, &tx_info->napi, slsi_txbp_napi, bp_napi_budget);
	if (!test_and_set_bit(SLSI_TX_NAPI_ENABLED, &tx_info->napi_state))
		napi_enable(&tx_info->napi);

	tx_info->csd.func = slsi_txbp_trigger_tx;
	tx_info->csd.info = tx_info;
	tx_info->target_cpu = TXBP_NAPI_CPU_NORM;
#endif
	tx_info->sdev = sdev;
	tx_info->ndev = dev;

	tx_priv->ac_presence = 0;
	tx_priv->ac_presence_update = 0;
	tx_priv->last_presence_check_time = jiffies;

	for (ac = 0; ac < AC_CATEGORIES; ac++) {
		skb_queue_head_init(&tx_priv->q[ac]);
		tx_info->assigned_q[ac] = &tx_priv->q[ac];
	}

	tx_priv->netdev_cod = 0;
	for (ac = 0; ac < AC_CATEGORIES; ac++) {
		tx_priv->ac_cod[ac] = 0;
		tx_priv->ac_completed[ac] = 0;
	}

	write_lock_bh(&txbp_priv.vif_lock);
	ndev_vif->tx_netdev_data = tx_priv;
	txbp_priv.vif_cnt++;
	list_add(&tx_priv->list, &txbp_priv.vif_list);
	slsi_txbp_vif_budget_redistribute(dev, true);
	write_unlock_bh(&txbp_priv.vif_lock);
	spin_lock_init(&tx_priv->arp_lock);
	SLSI_NET_INFO(dev, "BP: active interfaces(%s) vif_cnt %u\n", dev->name, txbp_priv.vif_cnt);

	return true;
}

__always_inline bool slsi_vif_deactivated_post(struct slsi_dev *sdev, struct net_device *dev,
					       struct netdev_vif *ndev_vif)
{
	struct tx_netdev_data *tx_priv;
	struct tx_struct *tx_info;
	u8 qidx;

	tx_priv = (struct tx_netdev_data *)ndev_vif->tx_netdev_data;
	if (!tx_priv)
		return false;

	write_lock_bh(&txbp_priv.vif_lock);
	ndev_vif->tx_netdev_data = NULL;
	txbp_priv.vif_cnt--;
	list_del(&tx_priv->list);
	slsi_txbp_vif_budget_redistribute(dev, false);
	write_unlock_bh(&txbp_priv.vif_lock);

	tx_info = &tx_priv->tx;
#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	slsi_lbm_unregister_bh(tx_priv->bh_tx);
	tx_priv->bh_tx = NULL;
#else
	if (test_and_clear_bit(SLSI_TX_NAPI_ENABLED, &tx_info->napi_state))
		napi_disable(&tx_info->napi);
	netif_napi_del(&tx_info->napi);
#endif

	for (qidx = 0; qidx < SLSI_TX_DATA_QUEUE_NUM; qidx++)
		skb_queue_purge(&tx_priv->q[qidx]);

#if defined(CONFIG_SCSC_PCIE_CHIP)
	spin_lock_bh(&tx_info->mx_claim_lock);
	if (tx_info->mx_claim & (MX_CLAIM_STATE_DEFERRED | MX_CLAIM_STATE_CLAIMED)) {
		scsc_mx_service_release(WLAN_TX_DATA);
		tx_info->mx_claim = 0;
	}
	spin_unlock_bh(&tx_info->mx_claim_lock);

	spin_lock_bh(&ndev_vif->mx_claim_tx_ctrl_lock);
	if (ndev_vif->mx_claim_tx_ctrl) {
		ndev_vif->mx_claim_tx_ctrl = false;
		scsc_mx_service_release(WLAN_TX_CTRL);
	}
	spin_unlock_bh(&ndev_vif->mx_claim_tx_ctrl_lock);
#endif

	kfree(tx_priv);
	SLSI_NET_INFO(dev, "BP: deactive interfaces(%s) vif_cnt %u\n", dev->name, txbp_priv.vif_cnt);

	return true;
}

__always_inline int slsi_tx_ps_port_control(struct slsi_dev *sdev, struct net_device *dev, struct slsi_peer *peer,
					    enum slsi_sta_conn_state s)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	SLSI_UNUSED_PARAMETER(sdev);
	WLBT_WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->vif_mutex));
	switch (s) {
	case SLSI_STA_CONN_STATE_DISCONNECTED:
		SLSI_NET_DBG1(dev, SLSI_TX, "STA disconnected\n");
		peer->authorized = false;
		return 0;

	case SLSI_STA_CONN_STATE_DOING_KEY_CONFIG:
		SLSI_NET_DBG1(dev, SLSI_TX, "STA doing KEY config\n");
		peer->authorized = false;
		return 0;

	case SLSI_STA_CONN_STATE_CONNECTED:
		SLSI_NET_DBG1(dev, SLSI_TX, "STA connected\n");
		peer->authorized = true;
		return 0;

	default:
		SLSI_NET_DBG1(dev, SLSI_TX, "STA state %d\n", s);
		peer->authorized = false;
		return 0;
	}
	return 0;
}

__always_inline bool slsi_tx_tdls_update(struct slsi_dev *sdev, struct net_device *dev, struct slsi_peer *sta_peer,
					 struct slsi_peer *tdls_peer, bool connection)
{
	return true;
}

static __always_inline void slsi_txbp_set_priority(struct slsi_dev *sdev, struct net_device *dev,
						   struct slsi_peer *peer, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int proto;

	if (!ndev_vif->is_available) {
		skb->priority = FAPI_PRIORITY_QOS_UP0;
		return;
	}

	proto = be16_to_cpu(eth_hdr(skb)->h_proto);
	/**
	 * Multicast / broadcast over AP interface
	 */
	if (ndev_vif->vif_type == FAPI_VIFTYPE_AP)
		/* MULTICAST/BROADCAST Queue is only used for AP */
		if (is_multicast_ether_addr(eth_hdr(skb)->h_dest)) {
			SLSI_NET_DBG3(dev, SLSI_TX, "Multicast AC queue will be selected\n");
#ifdef CONFIG_SCSC_USE_WMM_TOS
			skb->priority = slsi_get_priority_from_tos(skb->data + ETH_HLEN, proto);
#else
			skb->priority = slsi_get_priority_from_tos_dscp(skb->data + ETH_HLEN, proto);
#endif
			return;
		}
	/**
	 * Normal packets
	 */
	if (peer) {
		if (peer->qos_enabled) {
			if (peer->qos_map_set) {
				skb->priority = cfg80211_classify8021d(skb, &peer->qos_map);
			} else {
#ifdef CONFIG_SCSC_USE_WMM_TOS
				skb->priority = slsi_get_priority_from_tos(skb->data + ETH_HLEN, proto);
#else
				skb->priority = slsi_get_priority_from_tos_dscp(skb->data + ETH_HLEN, proto);
#endif
			}
			slsi_netif_set_tid_change_tid(dev, skb);
		} else {
			skb->priority = FAPI_PRIORITY_QOS_UP0;
		}
		/* Downgrade the priority if acm bit is set and tspec is not established */
		slsi_net_downgrade_pri(dev, peer, skb);
	} else {
		skb->priority = FAPI_PRIORITY_QOS_UP0;
	}
}

#if (KERNEL_VERSION(5, 2, 0) <= LINUX_VERSION_CODE)
u16 slsi_tx_select_queue(struct net_device *dev, struct sk_buff *skb, struct net_device *sb_dev)
#elif (KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE)
u16 slsi_tx_select_queue(struct net_device *dev, struct sk_buff *skb, struct net_device *sb_dev,
			 select_queue_fallback_t fallback)
#else
u16 slsi_tx_select_queue(struct net_device *dev, struct sk_buff *skb, void *accel_priv,
			 select_queue_fallback_t fallback)
#endif
{
	u16 proto;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_peer *peer = NULL;
#if (KERNEL_VERSION(4, 19, 0) > LINUX_VERSION_CODE)
	SLSI_UNUSED_PARAMETER(accel_priv);
#endif
#if (KERNEL_VERSION(5, 2, 0) > LINUX_VERSION_CODE)
	SLSI_UNUSED_PARAMETER(fallback);
#endif

	/* Defensive check for uninitialized mac header */
	if (!skb_mac_header_was_set(skb))
		skb_reset_mac_header(skb);

	proto = be16_to_cpu(eth_hdr(skb)->h_proto);

	switch (proto) {
	case ETH_P_PAE:
	case ETH_P_WAI:
		SLSI_NET_DBG3(dev, SLSI_TX, "EAP packet queue(priority) Selected\n");
		return SLSI_TX_PRIORITY_Q_INDEX;
	case ETH_P_ARP:
		SLSI_NET_DBG3(dev, SLSI_TX, "ARP packet queue Selected\n");
		return SLSI_TX_ARP_Q_INDEX;
	case ETH_P_IP:
		if (slsi_is_dhcp_packet(skb->data) == SLSI_TX_IS_NOT_DHCP)
			break;
		SLSI_NET_DBG3(dev, SLSI_TX, "DHCP packet queue(priority) Selected\n");
		return SLSI_TX_PRIORITY_Q_INDEX;
	default:
		break;
	}
	slsi_spinlock_lock(&ndev_vif->peer_lock);
	peer = slsi_get_peer_from_mac(sdev, dev, eth_hdr(skb)->h_dest);
	slsi_txbp_set_priority(sdev, dev, peer, skb);
	slsi_spinlock_unlock(&ndev_vif->peer_lock);

	return slsi_frame_priority_to_ac_queue(skb->priority);
}

__always_inline enum slsi_tx_packet_t slsi_tx_get_packet_type(struct sk_buff *skb)
{
	if (skb->queue_mapping < SLSI_TX_DATA_QUEUE_NUM)
		return SLSI_DATA_PKT;
	return SLSI_CTRL_PKT;
}

__always_inline int __slsi_tx_transmit_ctrl(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	int proto = be16_to_cpu(eth_hdr(skb)->h_proto);
	int ret;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tx_netdev_data *tx_priv = NULL;

	switch (proto) {
	default:
		/* Only EAP packets and IP frames with DHCP are stored in SLSI_NETIF_Q_PRIORITY */
		SLSI_NET_ERR(dev, "Bad h_proto=0x%x SLSI_NETIF_Q_PRIORITY\n", proto);
		return -EINVAL;
	case ETH_P_PAE:
	case ETH_P_WAI:
		SLSI_NET_DBG2(dev, SLSI_MLME, "tx EAP pkt from SLSI_NETIF_Q_PRIORITY\n");
		ret = slsi_tx_eapol(sdev, dev, skb);
		break;
	case ETH_P_ARP:
		SLSI_NET_DBG2(dev, SLSI_MLME, "tx ARP pkt from SLSI_NETIF_Q_ARP\n");
		read_lock(&txbp_priv.vif_lock);
		tx_priv = ndev_vif->tx_netdev_data;
		if (!tx_priv) {
			read_unlock(&txbp_priv.vif_lock);
			return -ENODEV;
		}

		spin_lock(&tx_priv->arp_lock);
		ret = slsi_tx_arp(sdev, dev, skb);
		if (ret == NETDEV_TX_OK && !ndev_vif->is_fw_test)
			netdev_tx_sent_queue(&dev->_tx[SLSI_TX_ARP_Q_INDEX], 1);
		spin_unlock(&tx_priv->arp_lock);
		read_unlock(&txbp_priv.vif_lock);
		break;
	case ETH_P_IP:
		SLSI_NET_DBG2(dev, SLSI_MLME, "tx DHCP pkt from SLSI_NETIF_Q_PRIORITY\n");
		ret = slsi_tx_dhcp(sdev, dev, skb);
		break;
	}
	return ret;
}

#if defined(CONFIG_SCSC_PCIE_CHIP)
int slsi_tx_ctrl_claim_complete_cb(void *service, void *data)
{
	struct net_device    *dev = (struct net_device *)data;
	struct netdev_vif    *ndev_vif = netdev_priv(dev);
	struct netdev_queue  *txq;
	int                  qidx;
	bool                 is_empty = true;

	SLSI_UNUSED_PARAMETER(service);

	spin_lock_bh(&ndev_vif->mx_claim_tx_ctrl_lock);
	ndev_vif->mx_claim_tx_ctrl = true;
	slsi_tx_wake_all_queues(dev);
	spin_unlock_bh(&ndev_vif->mx_claim_tx_ctrl_lock);

	for (qidx = SLSI_TX_PRIORITY_Q_INDEX; qidx < SLSI_TX_TOTAL_QUEUES; qidx++) {
		txq = &dev->_tx[qidx];

		if (txq->qdisc->q.qlen) {
			is_empty = false;
			break;
		}
	}

	if (is_empty) {
		spin_lock_bh(&ndev_vif->mx_claim_tx_ctrl_lock);
		if (ndev_vif->mx_claim_tx_ctrl) {
			ndev_vif->mx_claim_tx_ctrl = false;
			scsc_mx_service_release(WLAN_TX_CTRL);
		}
		spin_unlock_bh(&ndev_vif->mx_claim_tx_ctrl_lock);
	}

	return 0;
}
#endif

__always_inline int slsi_tx_transmit_ctrl(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	int ret;

#if defined(CONFIG_SCSC_PCIE_CHIP)
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	spin_lock_bh(&ndev_vif->mx_claim_tx_ctrl_lock);

	if (!ndev_vif->mx_claim_tx_ctrl) {
		if (scsc_mx_service_claim_deferred(sdev->service, slsi_tx_ctrl_claim_complete_cb, (void *)dev, WLAN_TX_CTRL)) {
			slsi_tx_stop_all_queues(dev);
			spin_unlock_bh(&ndev_vif->mx_claim_tx_ctrl_lock);
			return -ENOSPC;
		}
		ndev_vif->mx_claim_tx_ctrl = true;
	}
	spin_unlock_bh(&ndev_vif->mx_claim_tx_ctrl_lock);
#endif
#if defined(CONFIG_SCSC_WLAN_TAS)
	slsi_mlme_tas_deferred_tx_sar_limit(sdev);
#endif
	ret = __slsi_tx_transmit_ctrl(sdev, dev, skb);

#if defined(CONFIG_SCSC_PCIE_CHIP)
	spin_lock_bh(&ndev_vif->mx_claim_tx_ctrl_lock);
	if (ndev_vif->mx_claim_tx_ctrl) {
		ndev_vif->mx_claim_tx_ctrl = false;
		scsc_mx_service_release(WLAN_TX_CTRL);
	}
	spin_unlock_bh(&ndev_vif->mx_claim_tx_ctrl_lock);
#endif
	return ret;
}

static __always_inline struct net_device *
slsi_tx_mlme_get_vlan_netdev(struct slsi_dev *sdev,
			     struct net_device *dev,
			     u16 host_tag)
{
	char tmp_name[IFNAMSIZ], *vlan_if_name, key_index[5];
	struct net_device *dev_to_send = NULL;

	strcpy(tmp_name, dev->name);
	sprintf(key_index, ".%d",
		(host_tag & SLSI_HOST_TAG_VLAN_ID_MASK) >> 13);
	vlan_if_name = strcat(tmp_name, key_index);
	dev_to_send = slsi_get_netdev_by_ifname(sdev, vlan_if_name);

	return dev_to_send;
}

__always_inline int slsi_tx_mlme_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	u16 host_tag = fapi_get_u16(skb, u.mlme_frame_transmission_ind.host_tag);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tx_netdev_data *tx_priv = NULL;
	struct net_device *dev_to_send = dev;

	if (ndev_vif->is_fw_test)
		return 0;

	if (!(host_tag & SLSI_HOST_TAG_ARP_MASK))
		return 0;

	if (host_tag & SLSI_HOST_TAG_VLAN_ID_MASK) {
		dev_to_send = slsi_tx_mlme_get_vlan_netdev(sdev, dev, host_tag);
		if (!dev_to_send)
			return -ENODEV;
	}

	read_lock_bh(&txbp_priv.vif_lock);
	tx_priv = ndev_vif->tx_netdev_data;
	if (!tx_priv) {
		read_unlock_bh(&txbp_priv.vif_lock);
		return -ENODEV;
	}

	spin_lock_bh(&tx_priv->arp_lock);
	netdev_tx_completed_queue(&dev_to_send->_tx[SLSI_TX_ARP_Q_INDEX], 1, 1);
	spin_unlock_bh(&tx_priv->arp_lock);
	read_unlock_bh(&txbp_priv.vif_lock);

	return 0;
}

__always_inline int slsi_tx_mlme_cfm(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	u16 host_tag = fapi_get_u16(skb, u.mlme_send_frame_cfm.host_tag);
	u16 req_status = fapi_get_u16(skb, u.mlme_send_frame_cfm.result_code);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tx_netdev_data *tx_priv = NULL;
	struct net_device *dev_to_send = dev;

	if (ndev_vif->is_fw_test)
		return 0;

	if (!(host_tag & SLSI_HOST_TAG_ARP_MASK))
		return 0;

	if (host_tag & SLSI_HOST_TAG_VLAN_ID_MASK) {
		dev_to_send = slsi_tx_mlme_get_vlan_netdev(sdev, dev, host_tag);
		if (!dev_to_send)
			return -ENODEV;
	}

	read_lock_bh(&txbp_priv.vif_lock);
	tx_priv = ndev_vif->tx_netdev_data;
	if (!tx_priv) {
		read_unlock_bh(&txbp_priv.vif_lock);
		return -ENODEV;
	}

	spin_lock_bh(&tx_priv->arp_lock);
	if (req_status != FAPI_RESULTCODE_SUCCESS)
		netdev_tx_completed_queue(&dev_to_send->_tx[SLSI_TX_ARP_Q_INDEX], 1, 1);
	spin_unlock_bh(&tx_priv->arp_lock);
	read_unlock_bh(&txbp_priv.vif_lock);

	return 0;
}

static __always_inline void slsi_txbp_schedule_napi(struct net_device *dev, struct tx_netdev_data *tx_priv)
{
	struct tx_struct *tx_info;

	if (!tx_priv) {
		SLSI_WARN_NODEV("BP: tx_priv is NULL\n");
		return;
	}

	tx_info = &tx_priv->tx;
	if (!tx_info) {
		SLSI_WARN_NODEV("BP: tx_info is NULL\n");
		return;
	}

	if (!test_bit(SLSI_TX_NAPI_ENABLED, &tx_info->napi_state)) {
		SLSI_WARN_NODEV("BP: napi is disabled\n");
		return;
	}

	if (!slsi_txbp_has_resource(tx_priv))
		return;


#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	if (!test_and_set_bit(SLSI_TX_LBM_RUNNING, &tx_info->lbm_bh_state))
		slsi_lbm_run_bh(tx_priv->bh_tx);
#else
	if (tx_info->target_cpu != bp_fixed_cpu)
		tx_info->target_cpu = bp_fixed_cpu;

	if (tx_info->target_cpu == smp_processor_id()) {
		if (!test_and_set_bit(SLSI_TX_NAPI_POLLING, &tx_info->napi_state)) {
			if (bp_log & LOG_FILTER_TX_NAPI)
				SLSI_NET_INFO(dev, "BP: schedule tx on current cpu %u\n", tx_info->target_cpu);
			napi_schedule(&tx_info->napi);
		}
	} else {
		if (cpu_online(tx_info->target_cpu)) {
			if (!test_and_set_bit(SLSI_TX_NAPI_POLLING, &tx_info->napi_state)) {
				if (bp_log & LOG_FILTER_TX_NAPI)
					SLSI_NET_INFO(dev, "BP: schedule tx on cpu %u\n", tx_info->target_cpu);
				smp_call_function_single_async(tx_info->target_cpu, &tx_info->csd);
			}
		} else {
			SLSI_NET_INFO(dev, "BP: CPU is offline %u\n", tx_info->target_cpu);
			if (!test_and_set_bit(SLSI_TX_NAPI_POLLING, &tx_info->napi_state))
				napi_schedule(&tx_info->napi);
		}
	}
#endif
}

#if defined(CONFIG_SCSC_PCIE_CHIP)
int slsi_tx_mx_service_claim_complete_cb(void *service, void *data)
{
	struct tx_struct *tx = (struct tx_struct *)data;
	struct net_device *dev = tx->ndev;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tx_netdev_data *tx_priv = (struct tx_netdev_data *)ndev_vif->tx_netdev_data;

	if (!tx_priv) {
		return -EINVAL;
	}

	spin_lock_bh(&tx->mx_claim_lock);
	tx->mx_claim &= ~MX_CLAIM_STATE_DEFERRED;
	if (unlikely((tx->mx_claim & MX_CLAIM_STATE_CLAIMED) > 0))
		SLSI_NET_ERR(tx->ndev, "PCIe is already claimed\n");
	tx->mx_claim |= MX_CLAIM_STATE_CLAIMED;
	slsi_tx_wake_all_queues(dev);
	slsi_txbp_schedule_napi(dev, tx_priv);
	spin_unlock_bh(&tx->mx_claim_lock);
	return 0;
}
#endif
static __always_inline void slsi_txbp_enqueue(struct net_device *dev, struct netdev_vif *ndev_vif, struct sk_buff *skb)
{
	struct tx_netdev_data *tx_priv = (struct tx_netdev_data *)ndev_vif->tx_netdev_data;
	struct txq_stats *stat;
	const u16 queue_mapping = skb->queue_mapping;
#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
	struct bh_struct *bh = tx_priv->bh_tx;
	int target;
	u32 state;
#ifdef CONFIG_SCSC_WLAN_CPUHP_MONITOR
	struct slsi_dev *sdev = ndev_vif->sdev;
	cpumask_t except_cpus;
	int proper;
#endif
#endif
	u32 cpu;
	ktime_t now, wake;

	skb_queue_tail(&tx_priv->q[queue_mapping], skb);
	tx_priv->ac_presence_update |= (0x1 << queue_mapping);

#if (KERNEL_VERSION(5, 2, 0) <= LINUX_VERSION_CODE)
	if (!netdev_xmit_more()) {
#else
	if (!skb->xmit_more) {
#endif
		const u32 max_queue_len = slsi_txbp_get_max_qlen(tx_priv, queue_mapping);

		if (max_queue_len <= READ_ONCE(tx_priv->q[queue_mapping].qlen)) {
			if (bp_log & LOG_FILTER_Q_STATUS)
				SLSI_NET_INFO(dev, "BP: Queue Stop[%hu]=%u Limit=%u\n",
					      queue_mapping, READ_ONCE(tx_priv->q[queue_mapping].qlen),
					      max_queue_len);

			stat = &tx_priv->qstat[queue_mapping];
			now = ktime_get();
			wake = ktime_sub(now, stat->wake);
			stat->cumulated_wake = ktime_add(wake, stat->cumulated_wake);
			stat->stop = now;
			netif_stop_subqueue(dev, queue_mapping);
		}

		cpu = smp_processor_id();
#ifdef CONFIG_SCSC_WLAN_LOAD_BALANCE_MANAGER
		state = TRAFFIC_MON_CLIENT_STATE_HIGH;
		if (bh->cpu_affinity->state >= state) {
			if (cpu == bh->cpu_affinity->cpu[state]) {
				if (cpu == tx_priv->dedicated_napi) {
#ifdef CONFIG_SCSC_WLAN_CPUHP_MONITOR
					cpumask_clear(&except_cpus);
					cpumask_set_cpu(cpu, &except_cpus);
					proper = slsi_find_proper_cpu(bh, state, NP_TX_0,
								      sdev->cpu_cluster_map[cpu], &except_cpus);
					target = (bp_fixed_cpu >= 0) ? bp_fixed_cpu : proper;
#else
					target = (bp_fixed_cpu >= 0) ? bp_fixed_cpu : (cpu + 1) % SLSI_NR_CPUS;
#endif
				} else {
					target = -1;
				}
				bh->cpu_affinity->curr_cpu = cpu;
				slsi_lbm_state_change_for_affinity(bh, state, target);
			}
		}
#else
		if (ndev_vif->traffic_mon_state == TRAFFIC_MON_CLIENT_STATE_LOW) {
			if (cpu != tx_priv->tx.target_cpu)
				bp_fixed_cpu = cpu;
		} else {
			if (cpu == TXBP_NAPI_CPU_PERF)
				bp_fixed_cpu = (TXBP_NAPI_CPU_PERF + 1) % SLSI_NR_CPUS;
			else if (tx_priv->tx.target_cpu != TXBP_NAPI_CPU_PERF)
				bp_fixed_cpu = TXBP_NAPI_CPU_PERF;
		}
#endif
		slsi_txbp_schedule_napi(dev, tx_priv);
	}
}

/* 1. Multicast Tx */
static __always_inline int slsi_tx_transmit_ap_multicast(struct slsi_dev *sdev, struct net_device *dev,
							 struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_skb_cb *cb;
	struct ethhdr *ehdr = eth_hdr(skb);
	enum slsi_traffic_q tq;
	u8 vif_index;
	u8 peer_index;
	void *header = NULL;
	u16 group_key_index = 0;

	header = (void *)skb_push(skb, fapi_sig_size(ma_unitdata_req));
	memset(header, 0, fapi_sig_size(ma_unitdata_req));
	tq = slsi_frame_priority_to_ac_queue(skb->priority);
	if (ndev_vif->iftype == NL80211_IFTYPE_AP_VLAN) {
		group_key_index = ndev_vif->group_key_index;
		rcu_read_lock();
		vif_index = ((struct netdev_vif *)netdev_priv(ndev_vif->netdev_ap))->ifnum;
		rcu_read_unlock();
	} else {
		vif_index = ndev_vif->ifnum;
	}
	peer_index = 0; /* Reerved peer index for M/B cast */
	fapi_set_u16(skb, id,           MA_UNITDATA_REQ);
	fapi_set_u16(skb, receiver_pid, 0);
	fapi_set_u16(skb, sender_pid,   SLSI_TX_PROCESS_ID_MIN);
	fapi_set_u32(skb, fw_reference, 0);
	fapi_set_u16(skb, u.ma_unitdata_req.vif, vif_index);
	fapi_set_u16(skb, u.ma_unitdata_req.configuration_option, FAPI_OPTION_INLINE);
	fapi_set_u16(skb, u.ma_unitdata_req.flow_id, FAPI_PRIORITY_CONTENTION >> 8);
	fapi_set_memcpy(skb, u.ma_unitdata_req.address, ehdr->h_source);
	fapi_set_u16(skb, u.ma_unitdata_req.spare_1, group_key_index);

	cb = slsi_skb_cb_init(skb);
	cb->sig_length = fapi_sig_size(ma_unitdata_req);
	cb->data_length = skb->len;
	cb->peer_idx = peer_index;

	slsi_txbp_enqueue(dev, ndev_vif, skb);
	return NETDEV_TX_OK;
}

#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
static __always_inline int slsi_tx_transmit_nan_multicast(struct slsi_dev *sdev, struct net_device *dev,
							  struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	const int len = skb->len;
	struct slsi_peer *peer;
	enum slsi_traffic_q tq;
	u8 vif_index;
	struct slsi_skb_cb *cb;
	u8 i = 0;
	void *header = NULL;

	slsi_spinlock_lock(&ndev_vif->peer_lock);

	peer = slsi_get_peer_from_mac(sdev, dev, eth_hdr(skb)->h_dest);
	if (!peer) {
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		SLSI_NET_WARN(dev, "no peer record for " MACSTR ", drop Tx frame\n", MAC2STR(eth_hdr(skb)->h_dest));
		return -EINVAL;
	}
	if (!peer->authorized) {
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		SLSI_NET_WARN(dev, "Peer port is blocked\n");
		return -EINVAL;
	}

	header = (void *)skb_push(skb, fapi_sig_size(ma_unitdata_req));
	memset(header, 0, fapi_sig_size(ma_unitdata_req));
	tq = slsi_frame_priority_to_ac_queue(skb->priority);
	vif_index = ndev_vif->ifnum;
	fapi_set_u16(skb, id,           MA_UNITDATA_REQ);
	fapi_set_u16(skb, receiver_pid, 0);
	fapi_set_u16(skb, sender_pid,   SLSI_TX_PROCESS_ID_MIN);
	fapi_set_u32(skb, fw_reference, 0);

	fapi_set_u16(skb, u.ma_unitdata_req.configuration_option, FAPI_OPTION_INLINE | FAPI_OPTION_GROUP);
	fapi_set_memcpy(skb, u.ma_unitdata_req.address, sdev->nan_cluster_id);
	SLSI_NET_DBG_HEX(dev, SLSI_TX, skb->data, skb->len < 128 ? skb->len : 128, "\n");

	cb = slsi_skb_cb_init(skb);
	cb->sig_length = fapi_sig_size(ma_unitdata_req);
	cb->data_length = skb->len;

	peer->sinfo.tx_bytes += len;

	/**
	 * The driver shall send (duplicate) frames to all VIFs that
	 * have the same source address (SA).  i.e. find other peers on same
	 * netdevice interface and duplicate the packet for each peer
	 */
	for (i = 0; i < SLSI_PEER_INDEX_MAX; i++) {
		if (ndev_vif->peer_sta_record[i] && ndev_vif->peer_sta_record[i]->valid) {
			/* duplicate the SKB and send to
			 * peer that has a different NDL
			 */
			struct sk_buff *duplicate_skb = skb_copy(skb, GFP_ATOMIC);

			if (!duplicate_skb) {
				SLSI_NET_WARN(dev, "NAN: Tx: multicast: failed to alloc duplicate SKB\n");
				continue;
			}

			if (peer->qos_enabled)
				fapi_set_u16(duplicate_skb, u.ma_unitdata_req.flow_id,
					     (duplicate_skb->priority & 0xff) |
					     ((ndev_vif->peer_sta_record[i]->flow_id) & 0xff00));
			else
				fapi_set_u16(duplicate_skb, u.ma_unitdata_req.flow_id,
					     ((FAPI_PRIORITY_CONTENTION >> 8) & 0xff) |
					     ((ndev_vif->peer_sta_record[i]->flow_id) & 0xff00));

			fapi_set_u16(duplicate_skb, u.ma_unitdata_req.vif, ndev_vif->peer_sta_record[i]->ndl_vif);
			cb = slsi_skb_cb_get(duplicate_skb);
			cb->peer_idx = ndev_vif->peer_sta_record[i]->aid;

			/* overwrite the multicast destination address with that of Peer */
			ether_addr_copy(eth_hdr(duplicate_skb)->h_dest, ndev_vif->peer_sta_record[i]->address);
			slsi_txbp_enqueue(dev, ndev_vif, duplicate_skb);
		}
	}
	slsi_spinlock_unlock(&ndev_vif->peer_lock);
	consume_skb(skb);
	/* What about the original if we passed in a copy ? */
	return NETDEV_TX_OK;
}
#endif

/* 2. Unicast Tx */
static __always_inline int slsi_tx_transmit_normal(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	const int len = skb->len;
	struct slsi_peer  *peer;
	enum slsi_traffic_q tq;
	u8 vif;
	u8 peer_index;
	struct slsi_skb_cb  *cb;
	int ret = NETDEV_TX_OK;
	void *header = NULL;

	slsi_spinlock_lock(&ndev_vif->peer_lock);
	peer = slsi_get_peer_from_mac(sdev, dev, eth_hdr(skb)->h_dest);
	if (!peer) {
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		SLSI_NET_WARN(dev, "no peer record for " MACSTR ", drop Tx frame\n", MAC2STR(eth_hdr(skb)->h_dest));
		return -EINVAL;
	}
	if (!peer->authorized) {
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		SLSI_NET_WARN(dev, "Peer port is blocked\n");
		return -EINVAL;
	}

	header = (void *)skb_push(skb, fapi_sig_size(ma_unitdata_req));
	memset(header, 0, fapi_sig_size(ma_unitdata_req));
	tq = slsi_frame_priority_to_ac_queue(skb->priority);

	if (ndev_vif->ifnum < SLSI_NAN_DATA_IFINDEX_START)
		vif = ndev_vif->ifnum;
	else
		vif = peer->ndl_vif;

	if (ndev_vif->iftype == NL80211_IFTYPE_AP_VLAN) {
		rcu_read_lock();
		vif = ((struct netdev_vif *)netdev_priv(ndev_vif->netdev_ap))->ifnum;
		rcu_read_unlock();
	}

	peer_index = MAP_QS_TO_AID(peer->queueset);
	fapi_set_u16(skb, id,           MA_UNITDATA_REQ);
	fapi_set_u16(skb, receiver_pid, 0);
	fapi_set_u16(skb, sender_pid,   SLSI_TX_PROCESS_ID_MIN);
	fapi_set_u32(skb, fw_reference, 0);
	fapi_set_u16(skb, u.ma_unitdata_req.vif, vif);
	fapi_set_u16(skb, u.ma_unitdata_req.configuration_option, FAPI_OPTION_INLINE);

	/* by default the priority is set to contention. It is overridden and set appropriate
	 * priority if peer supports QoS. The broadcast/multicast frames are sent in non-QoS .
	 */
	if (peer->qos_enabled)
		fapi_set_u16(skb, u.ma_unitdata_req.flow_id, (skb->priority & 0xff) | (peer->flow_id & 0xff00));
	else
		fapi_set_u16(skb, u.ma_unitdata_req.flow_id, ((FAPI_PRIORITY_CONTENTION >> 8) & 0xff) |
			     (peer->flow_id & 0xff00));

	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
		if (ndev_vif->sta.tdls_enabled && slsi_is_tdls_peer(dev, peer))
			fapi_set_memcpy(skb, u.ma_unitdata_req.address, ndev_vif->sta.bssid);
		else
			fapi_set_memcpy(skb, u.ma_unitdata_req.address, eth_hdr(skb)->h_dest);
	} else if (ndev_vif->vif_type == FAPI_VIFTYPE_AP || ndev_vif->iftype == NL80211_IFTYPE_AP_VLAN) {
		fapi_set_memcpy(skb, u.ma_unitdata_req.address, eth_hdr(skb)->h_source);
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
	} else if (ndev_vif->ifnum >= SLSI_NAN_DATA_IFINDEX_START) {
		fapi_set_memcpy(skb, u.ma_unitdata_req.address, sdev->nan_cluster_id);
#endif
	}

	cb = slsi_skb_cb_init(skb);
	cb->sig_length = fapi_sig_size(ma_unitdata_req);
	cb->data_length = skb->len;
	cb->peer_idx = peer_index;

	peer->sinfo.tx_bytes += len;
	slsi_traffic_mon_event_tx(sdev, dev, len);
	slsi_tdls_manager_event_tx(sdev, dev, skb, tq);
	slsi_txbp_enqueue(dev, ndev_vif, skb);
	slsi_spinlock_unlock(&ndev_vif->peer_lock);
	return ret;
}

__always_inline int slsi_tx_transmit_data(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tx_netdev_data *tx_priv = NULL;
	const bool is_mcast = is_multicast_ether_addr(eth_hdr(skb)->h_dest);
	int ret;

	if (unlikely(!ndev_vif->activated))
		return -ENODEV;

	read_lock(&txbp_priv.vif_lock);
	tx_priv = (struct tx_netdev_data *)ndev_vif->tx_netdev_data;
	if (unlikely(!tx_priv)) {
		read_unlock(&txbp_priv.vif_lock);
		return -ENODEV;
	}

	if (is_msdu_enable())
		ethr_ii_to_subframe_msdu(skb);

	if ((ndev_vif->vif_type == FAPI_VIFTYPE_AP ||
	     ndev_vif->iftype == NL80211_IFTYPE_AP_VLAN) && is_mcast)
		ret = slsi_tx_transmit_ap_multicast(sdev, dev, skb);
#ifdef CONFIG_SCSC_WIFI_NAN_ENABLE
	/* For NAN vif_index is set to ndl_vif */
	else if (ndev_vif->ifnum >= SLSI_NAN_DATA_IFINDEX_START && is_mcast)
		ret = slsi_tx_transmit_nan_multicast(sdev, dev, skb);
#endif
	else
		ret = slsi_tx_transmit_normal(sdev, dev, skb);
	read_unlock(&txbp_priv.vif_lock);

	return ret;
}

int slsi_tx_done(struct slsi_dev *sdev, u32 colour, bool more)
{
	u16 vif;
	u8 ac;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	struct tx_netdev_data *tx_priv = NULL;

	rcu_read_lock();
	read_lock(&txbp_priv.vif_lock);
	if (!colour && !more) {
		write_lock(&txbp_priv.cod_lock);
		list_for_each_entry(tx_priv, &txbp_priv.vif_list, list) {
			for (ac = 0 ; ac < AC_CATEGORIES ; ac++) {
				txbp_priv.cod -= tx_priv->ac_completed[ac];
				tx_priv->netdev_cod -= tx_priv->ac_completed[ac];
				tx_priv->ac_cod[ac] -= tx_priv->ac_completed[ac];
				tx_priv->ac_completed[ac] = 0;
			}
		}
		write_unlock(&txbp_priv.cod_lock);
		list_for_each_entry(tx_priv, &txbp_priv.vif_list, list) {
			show_cod(tx_priv);
			slsi_txbp_schedule_napi(tx_priv->tx.ndev, tx_priv);
		}
		read_unlock(&txbp_priv.vif_lock);
		rcu_read_unlock();
		return 0;
	}

	vif = colour & 0xffff;
	ac = (colour >> 16) & 0xff;

	dev = slsi_get_netdev_rcu(sdev, vif);
	if (dev) {
		ndev_vif = netdev_priv(dev);
		tx_priv = (struct tx_netdev_data *)ndev_vif->tx_netdev_data;
		if (!tx_priv) {
			SLSI_NET_WARN(dev, "tx_priv Null\n");
			read_unlock(&txbp_priv.vif_lock);
			rcu_read_unlock();
			return 0;
		}
		tx_priv->ac_completed[ac]++;
	} else {
		read_unlock(&txbp_priv.vif_lock);
		rcu_read_unlock();
		return -ENODEV;
	}

	if (!more) {
		write_lock(&txbp_priv.cod_lock);
		list_for_each_entry(tx_priv, &txbp_priv.vif_list, list) {
			for (ac = 0 ; ac < AC_CATEGORIES ; ac++) {
				txbp_priv.cod -= tx_priv->ac_completed[ac];
				tx_priv->netdev_cod -= tx_priv->ac_completed[ac];
				tx_priv->ac_cod[ac] -= tx_priv->ac_completed[ac];
				tx_priv->ac_completed[ac] = 0;
			}
		}
		write_unlock(&txbp_priv.cod_lock);
		list_for_each_entry(tx_priv, &txbp_priv.vif_list, list) {
			show_cod(tx_priv);
			slsi_txbp_schedule_napi(tx_priv->tx.ndev, tx_priv);
		}
	}
	read_unlock(&txbp_priv.vif_lock);
	rcu_read_unlock();

	return 0;
}

__always_inline int slsi_tx_transmit_lower(struct slsi_dev *sdev, struct sk_buff *skb)
{
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	struct slsi_peer  *peer;
	u16               vif;
	u8                *dest;

	/* All MA-UNITDATA.request must be in A-MSDU subframe format.
	 *
	 * The AMSDU frame type is an AMSDU payload ready to be prepended by
	 * an 802.11 frame header by the firmware. The AMSDU subframe header
	 * is identical to an Ethernet header in terms of addressing, so it
	 * is safe to access the destination address through the ethernet
	 * structure.
	 */

	vif = fapi_get_vif(skb);
	dest = eth_hdr(skb)->h_dest;

	rcu_read_lock();
	dev = slsi_get_netdev_rcu(sdev, vif);
	if (!dev) {
		SLSI_ERR(sdev, "netdev(%d) No longer exists\n", vif);
		rcu_read_unlock();
		return -EINVAL;
	}

	ndev_vif = netdev_priv(dev);
	rcu_read_unlock();

	if (is_multicast_ether_addr(dest) && ndev_vif->vif_type == FAPI_VIFTYPE_AP) {
		slsi_txbp_enqueue(dev, ndev_vif, skb);
		return NETDEV_TX_OK;
	}

	slsi_spinlock_lock(&ndev_vif->peer_lock);
	peer = slsi_get_peer_from_mac(sdev, dev, dest);
	if (!peer) {
		SLSI_ERR(sdev, "no peer record for " MACSTR ", dropping TX frame\n", MAC2STR(dest));
		slsi_spinlock_unlock(&ndev_vif->peer_lock);
		return -EINVAL;
	}
	slsi_debug_frame(sdev, dev, skb, "TX");

	if ((fapi_get_u16(skb, u.ma_unitdata_req.flow_id) & 0xFF) == (FAPI_PRIORITY_CONTENTION >> 8))
		skb->priority = FAPI_PRIORITY_QOS_UP0;
	else
		skb->priority = fapi_get_u16(skb, u.ma_unitdata_req.flow_id) & 0xFF;

	slsi_txbp_enqueue(dev, ndev_vif, skb);
	slsi_spinlock_unlock(&ndev_vif->peer_lock);

	return NETDEV_TX_OK;
}
