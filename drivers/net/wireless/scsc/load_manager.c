// SPDX-License-Identifier: GPL-2.0
/******************************************************************************
 *
 * Copyright (c) 2014 - 2020 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#include "load_manager.h"
#include "traffic_monitor.h"
#include "debug.h"
#include "mgt.h"
#include "netif.h"
#if IS_ENABLED(CONFIG_SCSC_WLAN_RX_NAPI_GRO)
#include "sap_ma.h"
#endif
#ifdef CONFIG_SCSC_WLAN_CPUHP_MONITOR
#include "slsi_cpuhp_monitor.h"
#endif

#define MBPS (1000 * 1000)
static void slsi_lbm_traffic_mon_irq_affinity_cb(void *client_ctx, u32 state, u32 tput_tx, u32 tput_rx);
static void slsi_lbm_traffic_mon_rps_affinity_cb(void *client_ctx, u32 state, u32 tput_tx, u32 tput_rx);

static int slsi_lbm_rps_map_set(struct net_device *dev, char *buf, size_t len);
static int slsi_lbm_napi_poll(struct napi_struct *napi, int budget);
static void trigger_napi(void *data);

static struct load_manager load_man;

#define RPS_MAP_LEN 3
#define TH_MID 0
#define TH_HIGH 1
static uint napi_cpu[NAPI_NUM_MAX][TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE];
module_param_array_named(napi0_cpu, napi_cpu[0], uint, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(napi0_cpu, "NAPI0 cpu for each state");
module_param_array_named(napi1_cpu, napi_cpu[1], uint, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(napi1_cpu, "NAPI1 cpu for each state");
module_param_array_named(napi2_cpu, napi_cpu[2], uint, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(napi2_cpu, "NAPI2 cpu for each state");

char map_buf[TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE][RPS_MAP_LEN];
static char *rps_ptr[TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE];
static char *rps_map[] = {map_buf[0], map_buf[1], map_buf[2], map_buf[3]};
module_param_array_named(rps_map, rps_map, charp, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rps_map, "RPS cpu map for each state");

static uint rps_threshold_in_mbps[2];
module_param_array(rps_threshold_in_mbps, uint, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(rps_threshold_in_mbps, "Threshold of RPS");

static uint napi_threshold_in_mbps[2];
module_param_array(napi_threshold_in_mbps, uint, NULL, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(napi_threshold_in_mbps, "Threshold of NAPI");

static bool set_property_disable;
module_param(set_property_disable, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(set_property_disable, "disable set property for debug");

static uint saturation_trigger_threshold = 5;

int slsi_lbm_set_property(struct slsi_dev *sdev)
{
	struct scsc_service *service;
	int i, j, ret;

	if (!sdev || !sdev->service) {
		SLSI_ERR_NODEV("sdev or service is null\n");
		return -EINVAL;
	}
	service = sdev->service;

	/* This function is called when Wi-Fi on.
	 * To customize the property values for debug, disable reset values
	 */
	if (set_property_disable)
		return 0;

	ret = scsc_mx_service_property_read_string(service, "cpu_table_rps", (char **)rps_ptr, 4);
	if (ret <= 0) {
		for (i = 0; i < TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE; i++)
			memcpy(rps_map[i], "00", RPS_MAP_LEN);
	} else {
		for (i = 0; i < TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE; i++)
			memcpy(rps_map[i], rps_ptr[i], RPS_MAP_LEN);
	}

	ret = scsc_mx_service_property_read_u32(service, "cpu_table_napi", (unsigned int *)napi_cpu, 12);
	if (ret) {
		for (i = 0; i < NAPI_NUM_MAX; i++)
			for (j = 0; j < TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE; j++)
				napi_cpu[i][j] = 0;
	}

	ret = scsc_mx_service_property_read_u32(service, "threshold_table_napi", napi_threshold_in_mbps, 2);
	if (ret) {
		napi_threshold_in_mbps[TH_MID] = 0;
		napi_threshold_in_mbps[TH_HIGH] = 0;
	}

	ret = scsc_mx_service_property_read_u32(service, "threshold_table_rps", rps_threshold_in_mbps, 2);
	if (ret) {
		rps_threshold_in_mbps[TH_MID] = 0;
		rps_threshold_in_mbps[TH_HIGH] = 0;
	}

	return 0;
}

static void push_rps_event(struct net_device *dev, u32 state)
{
	struct ctrl_event *event;

	event = kmalloc(sizeof(*event), GFP_ATOMIC);
	if (!event) {
		SLSI_NET_ERR(dev, "RPS event cannot be allocated: state %u\n", state);
		return;
	}
	SLSI_NET_DBG3(dev, SLSI_LBM, "PUSH RPS event state %u\n", state);
	event->type = RPS_T;
	event->event.rps.dev = dev;
	event->event.rps.state = state;
	spin_lock_bh(&load_man.ctrl_event_list_lock);
	list_add_tail(&event->list, &load_man.ctrl_event_list);
	spin_unlock_bh(&load_man.ctrl_event_list_lock);
	schedule_work(&load_man.lbm_ctrl_work);
}

static void push_cpu_affinity_event(struct bh_struct *bh, u32 state)
{
	struct ctrl_event *event;

	event = kmalloc(sizeof(*event), GFP_ATOMIC);
	if (!event) {
		SLSI_ERR_NODEV("CPU affinity event cannot be allocated: state %u\n", state);
		return;
	}
	SLSI_DBG3_NODEV(SLSI_LBM, "PUSH affinity event state %u\n", state);
	event->type = CPU_AFFINITY_T;
	event->event.cpu_affinity.bh = bh;
	event->event.cpu_affinity.state = state;
	spin_lock_bh(&load_man.ctrl_event_list_lock);
	list_add_tail(&event->list, &load_man.ctrl_event_list);
	spin_unlock_bh(&load_man.ctrl_event_list_lock);
	schedule_work(&load_man.lbm_ctrl_work);
}

static void lbm_ctrl_work_func(struct work_struct *data)
{
	struct ctrl_event *event;

	while (1) {
		spin_lock_bh(&load_man.ctrl_event_list_lock);
		if (list_empty(&load_man.ctrl_event_list)) {
			spin_unlock_bh(&load_man.ctrl_event_list_lock);
			break;
		}

		event = list_first_entry(&load_man.ctrl_event_list, struct ctrl_event, list);
		list_del(&event->list);
		spin_unlock_bh(&load_man.ctrl_event_list_lock);

		switch (event->type) {
		case RPS_T:
		{
			struct net_device *dev = event->event.rps.dev;
			struct netdev_vif *ndev_vif = netdev_priv(event->event.rps.dev);
			char *target_rps;

			if (!ndev_vif->rps)
				break;

			target_rps = ndev_vif->rps->rps[event->event.rps.state];

			SLSI_DBG3_NODEV(SLSI_LBM, "POP RPS event state %u(%s)\n", event->event.rps.state, target_rps);
			slsi_lbm_rps_map_set(dev, target_rps, strlen(target_rps));
		}
		break;
		case CPU_AFFINITY_T:
		{
			struct bh_struct *bh, *tmp;
			struct list_head *pos, *n;
			struct slsi_dev *sdev = load_man.sdev;
			bool skip = true;

			SLSI_MUTEX_LOCK(sdev->start_stop_mutex);
			if (sdev->device_state != SLSI_DEVICE_STATE_STARTED) {
				SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
				break;
			}

			bh = event->event.cpu_affinity.bh;

			mutex_lock(&sdev->hip.hip_mutex);
			read_lock_bh(&load_man.bh_list_lock);
			list_for_each_safe(pos, n, &load_man.bh_list) {
				tmp = list_entry(pos, struct bh_struct, list);

				if (bh == tmp) {
					skip = false;
					break;
				}
			}

			if (skip || !sdev->service) {
				read_unlock_bh(&load_man.bh_list_lock);
				mutex_unlock(&sdev->hip.hip_mutex);
				SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
				break;
			}

			if (bh->cpu_affinity) {
#if defined(CONFIG_SCSC_QOS)
#if defined(CONFIG_SOC_S5E9815) || defined(CONFIG_SOC_S5E9925) || defined(CONFIG_SOC_S5E8535) \
	|| defined(CONFIG_SOC_S5E8835)
				struct scsc_service *service;

				/**
				 * In case where irq affinity set is failed,
				 * we allow that IRQ and napi are scheduled in different core.
				 */
				int target_cpu = bh->cpu_affinity->cpu[event->event.cpu_affinity.state];

				bh->cpu_affinity->curr_cpu = target_cpu;

				SLSI_DBG3_NODEV(SLSI_LBM, "POP affinity event state %u(%d)\n",
						event->event.cpu_affinity.state, target_cpu);

				if (bh->cpu_affinity->idx == NP_TX_0) {
					read_unlock_bh(&load_man.bh_list_lock);
					mutex_unlock(&sdev->hip.hip_mutex);
					SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
					break;
				}
				read_unlock_bh(&load_man.bh_list_lock);
				service = sdev->service;
#if defined(CONFIG_SOC_S5E9925)
				/* MSI affinity is not supported (yet), msi
				 * argument will be ignored */
				if (scsc_service_set_affinity_cpu(service, 0, target_cpu) != 0)
#else
				if (scsc_service_set_affinity_cpu(service, target_cpu) != 0)
#endif
					SLSI_ERR_NODEV("failed to change IRQ affinity (CPU%d)\n", target_cpu);
				read_lock_bh(&load_man.bh_list_lock);
#endif
#endif
			}
			read_unlock_bh(&load_man.bh_list_lock);
			mutex_unlock(&sdev->hip.hip_mutex);
			SLSI_MUTEX_UNLOCK(sdev->start_stop_mutex);
		}
		break;
		}
		kfree(event);
	}
}

void slsi_lbm_init(struct slsi_dev *sdev)
{
	int cpu;

	load_man.sdev = sdev;
	for_each_cpu_and(cpu, cpu_possible_mask, cpu_online_mask) {
		load_man.cpu_avail[cpu] = true;
		SLSI_DBG4(sdev, SLSI_LBM, "CPU%d is online\n", cpu);
	}
#ifdef CONFIG_SCSC_WLAN_CPUHP_MONITOR
	load_man.cpuhp_node = slsi_cpuhp_monitor_register_callback(slsi_lbm_cpuhp_online_cb,
								   slsi_lbm_cpuhp_offline_cb, sdev);
#endif
	SLSI_INFO(sdev, "Init load balance manager\n");

	rwlock_init(&load_man.bh_list_lock);
	INIT_LIST_HEAD(&load_man.bh_list);
	INIT_LIST_HEAD(&load_man.ctrl_event_list);
	init_dummy_netdev(&load_man.napi_netdev);
	spin_lock_init(&load_man.ctrl_event_list_lock);
	INIT_WORK(&load_man.lbm_ctrl_work, lbm_ctrl_work_func);
}

void slsi_lbm_deinit(struct slsi_dev *sdev)
{
	struct list_head *pos, *n;
	struct bh_struct *tmp;
	int i;

	for (i = 0; i < SLSI_NR_CPUS; i++)
		load_man.cpu_avail[i] = 0;
#ifdef CONFIG_SCSC_WLAN_CPUHP_MONITOR
	slsi_cpuhp_monitor_unregister_callback(load_man.cpuhp_node);
#endif

	SLSI_INFO(sdev, "Deinit load balance manager\n");
	cancel_work_sync(&load_man.lbm_ctrl_work);

	/* release all heads in list, hp_cpu init */
	list_for_each_safe(pos, n, &load_man.bh_list) {
		tmp = list_entry(pos, struct bh_struct, list);
		slsi_lbm_unregister_bh(tmp);
	}
}

int slsi_lbm_netdev_activate(struct slsi_dev *sdev, struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (ndev_vif->vif_type == FAPI_VIFTYPE_PRECONNECT)
		return 0;

	SLSI_NET_DBG3(dev, SLSI_LBM, "iface%d is activated\n", ndev_vif->ifnum);
	if (!(SLSI_IS_VIF_INDEX_P2P(ndev_vif) || SLSI_IS_VIF_INDEX_NAN(ndev_vif)) &&
	    (rps_threshold_in_mbps[TH_MID] || rps_threshold_in_mbps[TH_HIGH])) {
		if (!ndev_vif->rps)
			ndev_vif->rps = slsi_lbm_register_rps_control(dev, rps_threshold_in_mbps[TH_MID],
								      rps_threshold_in_mbps[TH_HIGH]);
	} else {
		slsi_lbm_rps_map_set(dev, rps_map[TRAFFIC_MON_CLIENT_STATE_LOW], RPS_MAP_LEN);
	}

	return 0;
}

int slsi_lbm_netdev_deactivate(struct slsi_dev *sdev, struct net_device *dev, struct netdev_vif *ndev_vif)
{
	struct list_head *pos, *n;
	struct bh_struct *bh;
	bool perf_in_use = false;
#if IS_ENABLED(CONFIG_SCSC_WLAN_RX_NAPI_GRO)
	int i;
#endif

	SLSI_NET_DBG3(dev, SLSI_LBM, "iface%d is deactivated\n", ndev_vif->ifnum);

#if IS_ENABLED(CONFIG_SCSC_WLAN_RX_NAPI_GRO)
	slsi_sap_ma_lock();
	for (i = 0; i < SLSI_NR_CPUS; i++)
		napi_gro_flush(&sdev->hip.hip_priv->bh_dat->bh_priv.napi.cpu_info[i].napi_instance, false);
	slsi_sap_ma_unlock();
#endif

	if (ndev_vif->rps)
		slsi_lbm_unregister_rps_control(dev);
	else /*Set 00 to release */
		slsi_lbm_rps_map_set(dev, rps_map[TRAFFIC_MON_CLIENT_STATE_LOW], RPS_MAP_LEN);

	/* If all activated interface is in LOW state, change CPU affinity to LOW. */
	read_lock_bh(&load_man.bh_list_lock);
	list_for_each_safe(pos, n, &load_man.bh_list) {
		bh = list_entry(pos, struct bh_struct, list);

		if (!bh->cpu_affinity)
			continue;

		if (bh->cpu_affinity->state !=  TRAFFIC_MON_CLIENT_STATE_LOW) {
			perf_in_use = true;
			break;
		}
	}
	read_unlock_bh(&load_man.bh_list_lock);

	if (!perf_in_use)
		push_cpu_affinity_event(bh, TRAFFIC_MON_CLIENT_STATE_LOW);

	return 0;
}

struct bh_struct *slsi_lbm_register_napi(struct slsi_dev *sdev, int (*napi_poll)(struct napi_struct *, int),
					 int irq, int idx)
{
	struct net_device *dev = &load_man.napi_netdev;
	struct hip_priv *hip_priv = sdev->hip.hip_priv;
	struct bh_struct *bh_tmp;
	struct napi_struct *napi_ins;
	int i;

	bh_tmp = kzalloc(sizeof(*bh_tmp), GFP_ATOMIC);
	if (!bh_tmp)
		return NULL;

	/**
	 * bh_struct
	 */
	bh_tmp->sdev = sdev;
	bh_tmp->hip_priv = hip_priv;
	bh_tmp->type = NP_T;
	bh_tmp->irq = irq;
	bh_tmp->cpu_affinity = NULL;
	bh_tmp->io_saturation = NULL;

	/**
	 * bh_priv
	 */
	bh_tmp->bh_priv.napi.bh = bh_tmp;
	bh_tmp->bh_priv.napi.napi_state = 0;
	bh_tmp->bh_priv.napi.poll = napi_poll;

	SLSI_DBG3_NODEV(SLSI_LBM, "Register napi%d to load balance manager(0x%lx)\n", idx, bh_tmp);

	for (i = 0; i < SLSI_NR_CPUS; i++) {
		napi_ins = &bh_tmp->bh_priv.napi.cpu_info[i].napi_instance;
		bh_tmp->bh_priv.napi.cpu_info[i].csd.func = trigger_napi;
		bh_tmp->bh_priv.napi.cpu_info[i].csd.info = napi_ins;
		bh_tmp->bh_priv.napi.cpu_info[i].priv = &bh_tmp->bh_priv.napi;

		if (idx != NP_TX_0)
			netif_napi_add(dev, napi_ins, slsi_lbm_napi_poll, NAPI_POLL_WEIGHT);
		else
			netif_tx_napi_add(dev, napi_ins, slsi_lbm_napi_poll, NAPI_POLL_WEIGHT);
	}

	write_lock_bh(&load_man.bh_list_lock);
	list_add(&bh_tmp->list, &load_man.bh_list);
	write_unlock_bh(&load_man.bh_list_lock);

	return bh_tmp;
}

struct bh_struct *slsi_lbm_register_tasklet(struct slsi_dev *sdev, void (*tasklet_func)(unsigned long data), int irq)
{
	struct hip_priv *hip_priv = sdev->hip.hip_priv;
	struct slsi_hip *hip = &sdev->hip;
	struct bh_struct *bh_tmp;

	bh_tmp = kzalloc(sizeof(*bh_tmp), GFP_ATOMIC);
	if (!bh_tmp)
		return NULL;

	bh_tmp->sdev = sdev;
	bh_tmp->hip_priv = hip_priv;
	bh_tmp->type = TL_T;
	bh_tmp->irq = irq;

	SLSI_DBG3_NODEV(SLSI_LBM, "Register tasklet to load balance manager(0x%lx)\n", bh_tmp);

	bh_tmp->bh_priv.tasklet.bh_s = bh_tmp;
	tasklet_init(&bh_tmp->bh_priv.tasklet.bh, tasklet_func, (unsigned long)hip);

	write_lock_bh(&load_man.bh_list_lock);
	list_add(&bh_tmp->list, &load_man.bh_list);
	write_unlock_bh(&load_man.bh_list_lock);

	return bh_tmp;
}

struct bh_struct *slsi_lbm_register_workqueue(struct slsi_dev *sdev,
					      void (*workqueue_func)(struct work_struct *data),
					      int irq)
{
	struct hip_priv *hip_priv = sdev->hip.hip_priv;
	struct bh_struct *bh_tmp;

	bh_tmp = kzalloc(sizeof(*bh_tmp), GFP_ATOMIC);
	if (!bh_tmp)
		return NULL;

	bh_tmp->sdev = sdev;
	bh_tmp->hip_priv = hip_priv;
	bh_tmp->type = WQ_T;
	bh_tmp->irq = irq;

	SLSI_DBG3_NODEV(SLSI_LBM, "Register work to load balance manager(0x%lx)\n", bh_tmp);

	INIT_WORK(&bh_tmp->bh_priv.work.bh, workqueue_func);
	bh_tmp->bh_priv.work.bh_s = bh_tmp;

	write_lock_bh(&load_man.bh_list_lock);
	list_add(&bh_tmp->list, &load_man.bh_list);
	write_unlock_bh(&load_man.bh_list_lock);

	return bh_tmp;
}

struct cpu_affinity_ctrl_info *slsi_lbm_register_cpu_affinity_control(struct bh_struct *bh, int idx)
{
	u8 state;
	u32 dir;

	if (!bh) {
		SLSI_ERR_NODEV("invalid bh handle\n");
		return NULL;
	}

	if (bh->type != NP_T) {
		SLSI_ERR_NODEV("invalid bh type %d\n", bh->type);
		return NULL;
	}

	bh->cpu_affinity = kmalloc(sizeof(*bh->cpu_affinity), GFP_ATOMIC);
	if (!bh->cpu_affinity) {
		SLSI_ERR_NODEV("cannot allocate cpu_affinity\n");
		return NULL;
	}

	for (state = TRAFFIC_MON_CLIENT_STATE_LOW ; state < TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE ; state++)
		bh->cpu_affinity->cpu[state] = napi_cpu[idx][state];
	bh->cpu_affinity->mid = napi_threshold_in_mbps[TH_MID] * MBPS;
	bh->cpu_affinity->high = napi_threshold_in_mbps[TH_HIGH] * MBPS;
	bh->cpu_affinity->state = TRAFFIC_MON_CLIENT_STATE_LOW;
	bh->cpu_affinity->curr_cpu = napi_cpu[idx][TRAFFIC_MON_CLIENT_STATE_LOW];
	bh->cpu_affinity->idx = idx;
	SLSI_INFO_NODEV("th_mid:%d, th_high:%d, CPU low:%d, CPU mid:%d, CPU high:%d, CPU over:%d\n",
			napi_threshold_in_mbps[TH_MID], napi_threshold_in_mbps[TH_HIGH],
			napi_cpu[idx][0], napi_cpu[idx][1], napi_cpu[idx][2], napi_cpu[idx][3]);

	if (idx < NP_TX_0)
		dir = TRAFFIC_MON_DIR_RX;
	else
		dir = TRAFFIC_MON_DIR_TX;

	SLSI_DBG3_NODEV(SLSI_LBM, "Register affinity controller for bh(type:%d)\n", bh->type);
	if (slsi_traffic_mon_client_register(bh->sdev, bh, TRAFFIC_MON_CLIENT_MODE_EVENTS, bh->cpu_affinity->mid,
					     bh->cpu_affinity->high, dir, slsi_lbm_traffic_mon_irq_affinity_cb)) {
		SLSI_WARN(bh->sdev, "failed to add IRQ affinity controller\n");
		kfree(bh->cpu_affinity);
		bh->cpu_affinity = NULL;
		return NULL;
	}
	slsi_lbm_state_change_for_affinity(bh, TRAFFIC_MON_CLIENT_STATE_LOW, -1);

	return bh->cpu_affinity;
}

struct io_saturatioin_ctrl_info *slsi_lbm_register_io_saturation_control(struct bh_struct *bh)
{
	if (!bh) {
		SLSI_ERR_NODEV("invalid bh handle\n");
		return NULL;
	}
	bh->io_saturation = kmalloc(sizeof(*bh->io_saturation), GFP_ATOMIC);
	if (!bh->io_saturation) {
		SLSI_ERR_NODEV("cannot allocate io_saturation\n");
		return NULL;
	}
	SLSI_DBG3_NODEV(SLSI_LBM, "Register saturation controller for bh(type:%d)\n", bh->type);
	bh->io_saturation->saturation_cnt = 0;
	bh->io_saturation->saturated = false;
	bh->io_saturation->saturation_trigger_threshold = saturation_trigger_threshold;
	return bh->io_saturation;
}

struct rps_ctrl_info *slsi_lbm_register_rps_control(struct net_device *dev, const int mid, const int high)
{
	struct netdev_vif	*ndev_vif = netdev_priv(dev);
	struct rps_ctrl_info	*rps_handle;
	u8 state;

	if (ndev_vif->rps) {
		SLSI_NET_DBG4(dev, SLSI_LBM, "RPS controller is already set(ifnum:%d)\n", ndev_vif->ifnum);
		return ndev_vif->rps;
	}

	rps_handle = kmalloc(sizeof(*rps_handle), GFP_ATOMIC);
	if (!rps_handle) {
		SLSI_ERR_NODEV("Cannot allocate rps\n");
		return NULL;
	}

	SLSI_NET_DBG3(dev, SLSI_LBM, "Register RPS controller for ifnum:%d\n", ndev_vif->ifnum);
	ndev_vif->rps = rps_handle;

	ndev_vif->rps->mid = mid * MBPS;
	ndev_vif->rps->high = high * MBPS;
	for (state = TRAFFIC_MON_CLIENT_STATE_LOW ; state < TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE ; state++)
		memcpy(ndev_vif->rps->rps[state], rps_map[state], RPS_MAP_LEN);
	ndev_vif->rps->state = TRAFFIC_MON_CLIENT_STATE_LOW;

	SLSI_NET_INFO(dev, "th_mid:%d, th_high:%d, CPU low:%s, CPU mid:%s, CPU high:%s, CPU over:%s\n",
		      mid, high, ndev_vif->rps->rps[0], ndev_vif->rps->rps[1],
		      ndev_vif->rps->rps[2], ndev_vif->rps->rps[3]);

	if (slsi_traffic_mon_client_register(ndev_vif->sdev, dev, TRAFFIC_MON_CLIENT_MODE_EVENTS, ndev_vif->rps->mid,
					     ndev_vif->rps->high, TRAFFIC_MON_DIR_DEFAULT,
					     slsi_lbm_traffic_mon_rps_affinity_cb)) {
		SLSI_NET_WARN(dev, "failed to register RPS controller cb\n");
		kfree(ndev_vif->rps);
		ndev_vif->rps = NULL;
	}

	return ndev_vif->rps;
}

int slsi_lbm_unregister_bh(struct bh_struct *bh)
{
	struct list_head *pos, *n;
	struct ctrl_event *event;
	int i;
	int err = -ENOENT;

	if (!bh) {
		SLSI_ERR_NODEV("bh is already unregistered!\n");
		return err;
	}
	SLSI_DBG3_NODEV(SLSI_LBM, "unregister bh(type:%d, p:0x%lx)\n", bh->type, bh);

	/* release all heads in list */
	switch (bh->type) {
	case NP_T:
		if (test_and_clear_bit(SLSI_HIP_NAPI_STATE_ENABLED, &bh->bh_priv.napi.napi_state))
			for (i = 0; i < SLSI_NR_CPUS; i++)
				napi_disable(&bh->bh_priv.napi.cpu_info[i].napi_instance);

		for (i = 0; i < SLSI_NR_CPUS; i++)
			netif_napi_del(&bh->bh_priv.napi.cpu_info[i].napi_instance);
		slsi_lbm_unregister_cpu_affinity_control(bh->sdev, bh);
		slsi_lbm_unregister_io_saturation_control(bh);
		break;
	case WQ_T:
		cancel_work_sync(&bh->bh_priv.work.bh);
		break;
	case TL_T:
		tasklet_kill(&bh->bh_priv.tasklet.bh);
		break;
	default:
		SLSI_ERR_NODEV("bh type is not available(%d)\n", bh->type);
		return err;
	}

	spin_lock_bh(&load_man.ctrl_event_list_lock);
	list_for_each_safe(pos, n, &load_man.ctrl_event_list) {
		event = list_entry(pos, struct ctrl_event, list);
		if (event->type == CPU_AFFINITY_T && event->event.cpu_affinity.bh == bh) {
			list_del(pos);
			kfree(event);
		}
	}
	spin_unlock_bh(&load_man.ctrl_event_list_lock);

	write_lock_bh(&load_man.bh_list_lock);
	list_del(&bh->list);
	write_unlock_bh(&load_man.bh_list_lock);
	kfree(bh);

	return 0;
}

void slsi_lbm_unregister_cpu_affinity_control(struct slsi_dev *sdev, struct bh_struct *bh)
{
	if (!bh) {
		SLSI_ERR_NODEV("invalid bh handle\n");
		return;
	}
	if (!bh->cpu_affinity)
		return;

	slsi_traffic_mon_client_unregister(sdev, bh);
	load_man.cpu_avail[bh->cpu_affinity->curr_cpu] = true;

	SLSI_DBG3_NODEV(SLSI_LBM, "Unregister affinity controller for type:%d\n", bh->type);
	kfree(bh->cpu_affinity);
	bh->cpu_affinity = NULL;
}

void slsi_lbm_unregister_io_saturation_control(struct bh_struct *bh)
{
	if (!bh) {
		SLSI_ERR_NODEV("invalid bh handle\n");
		return;
	}
	if (!bh->io_saturation)
		return;

	SLSI_DBG3_NODEV(SLSI_LBM, "Unregister saturation controller for type:%d\n", bh->type);
	kfree(bh->io_saturation);
	bh->io_saturation = NULL;
}

void slsi_lbm_unregister_rps_control(struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct ctrl_event *event;
	struct list_head  *pos, *n;

	if (!ndev_vif->rps)
		return;

	SLSI_NET_DBG3(dev, SLSI_LBM, "Unregister RPS controller for ifnum:%d\n", ndev_vif->ifnum);
	slsi_traffic_mon_client_unregister(ndev_vif->sdev, dev);

	spin_lock_bh(&load_man.ctrl_event_list_lock);
	list_for_each_safe(pos, n, &load_man.ctrl_event_list) {
		event = list_entry(pos, struct ctrl_event, list);
		if (event->type == RPS_T && event->event.rps.dev == dev) {
			list_del(pos);
			kfree(event);
		}
	}
	spin_unlock_bh(&load_man.ctrl_event_list_lock);

	kfree(ndev_vif->rps);
	ndev_vif->rps = NULL;
}

static void trigger_napi(void *data)
{
	struct napi_struct *napi = (struct napi_struct *)data;

	SLSI_DBG4_NODEV(SLSI_LBM, "schedule NAPI in changed CPU\n");
	napi_schedule(napi);
}

static int slsi_lbm_napi_poll(struct napi_struct *napi, int budget)
{
	struct napi_cpu_info *napi_cpu_info = container_of(napi, struct napi_cpu_info, napi_instance);
	struct bh_struct	*bh_s = napi_cpu_info->priv->bh;
	struct slsi_dev		*sdev = bh_s->sdev;
	int work_done;

	if (bh_s->cpu_affinity) {
		int current_cpu;
		int target_cpu;
		int idx = bh_s->cpu_affinity->idx;

		current_cpu = smp_processor_id();
		target_cpu = bh_s->cpu_affinity->cpu[bh_s->cpu_affinity->state];

		/* Check cpu and switch if it needs to do */
		if (current_cpu != target_cpu) {
			napi_complete_done(napi, 0);
			if (idx < NP_TX_0)
				SLSI_DBG3_NODEV(SLSI_LBM, "CPU affinity was changed, move to CPU%d\n", target_cpu);
			smp_call_function_single_async(target_cpu, &bh_s->bh_priv.napi.cpu_info[target_cpu].csd);
			return 0;
		}
	}

	work_done = bh_s->bh_priv.napi.poll(napi, budget);

	if (bh_s->io_saturation && bh_s->cpu_affinity) {
		int current_cpu, cpu_high;

		current_cpu = bh_s->cpu_affinity->curr_cpu;

		if (work_done >= budget) {
			bh_s->io_saturation->saturation_cnt++;

			cpu_high = bh_s->cpu_affinity->cpu[TRAFFIC_MON_CLIENT_STATE_HIGH];
			if (current_cpu < cpu_high && !bh_s->io_saturation->saturated &&
			    bh_s->io_saturation->saturation_cnt > bh_s->io_saturation->saturation_trigger_threshold) {
				SLSI_DBG3_NODEV(SLSI_LBM, "NAPI is saturated, move to override state\n");
				bh_s->io_saturation->saturation_cnt = 0;
				bh_s->io_saturation->saturated = true;
				slsi_traffic_mon_override(sdev);
			}
		} else {
			bh_s->io_saturation->saturation_cnt = 0;
			bh_s->io_saturation->saturated = false;
		}
	}

	return work_done;
}

int slsi_lbm_run_bh(struct bh_struct *bh)
{
	if (!bh)
		return 0;

	switch (bh->type) {
	case NP_T:
		napi_schedule(&bh->bh_priv.napi.cpu_info[smp_processor_id()].napi_instance);
		break;
	case WQ_T:
#ifdef CONFIG_SCSC_WLAN_HIP5
		if (!queue_work(bh->hip_priv->hip_workq, &bh->bh_priv.work.bh))
#else
		if (!queue_work(bh->hip_priv->hip4_workq, &bh->bh_priv.work.bh))
#endif
			SLSI_DBG3_NODEV(SLSI_LBM, "wq_ctrl is already scheduled\n");
		break;
	case TL_T:
		tasklet_hi_schedule(&bh->bh_priv.tasklet.bh);
		break;
	default:
		SLSI_ERR_NODEV("Unsupported bh(type:%d)\n", bh->type);
	}

	return 0;
}

static void slsi_lbm_traffic_mon_irq_affinity_cb(void *client_ctx, u32 state, u32 tput_tx, u32 tput_rx)
{
	struct bh_struct *bh = (struct bh_struct *)client_ctx;
	struct slsi_dev *sdev = bh->sdev;
	u32 old_state;

	if (!bh->cpu_affinity)
		return;

	sdev->agg_dev_throughput_tx = tput_tx;
	sdev->agg_dev_throughput_rx = tput_rx;

	old_state = bh->cpu_affinity->state;

	if (state == old_state)
		return;

	SLSI_INFO_NODEV("traffic monitor: event (current:%d new:%d, tput_tx:%u bps, tput_rx:%u bps)\n",
			old_state, state, tput_tx, tput_rx);
	/* if the state change is from override to High, or vice versa, there is no change in configuration */
	if (state >= TRAFFIC_MON_CLIENT_STATE_HIGH && old_state >= TRAFFIC_MON_CLIENT_STATE_HIGH)
		return;

	bh->cpu_affinity->state = state;
	slsi_lbm_state_change_for_affinity(bh, state, -1);
}

static void slsi_lbm_traffic_mon_rps_affinity_cb(void *client_ctx, u32 state, u32 tput_tx, u32 tput_rx)
{
	struct net_device *dev = (struct net_device *)client_ctx;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct rps_ctrl_info *rps = ndev_vif->rps;
	u32 old_state;

	sdev->agg_dev_throughput_tx = tput_tx;
	sdev->agg_dev_throughput_rx = tput_rx;

	old_state = rps->state;

	if (state == old_state)
		return;

	SLSI_NET_INFO(dev, "traffic monitor: event (current:%d new:%d, tput_tx:%u bps, tput_rx:%u bps)\n",
		      old_state, state, tput_tx, tput_rx);
	/* if the state change is from override to High, or vice versa, there is no change in configuration */
	if (state >= TRAFFIC_MON_CLIENT_STATE_HIGH && old_state >= TRAFFIC_MON_CLIENT_STATE_HIGH)
		return;

	rps->state = state;
	push_rps_event(dev, state);
}

#ifdef CONFIG_SCSC_WLAN_CPUHP_MONITOR
static void cpu_status_change_for_affinity(struct bh_struct *bh, u32 state, int cpu, bool is_online)
{
	int idx = bh->cpu_affinity->idx;

	if (is_online) {
		if (state == bh->cpu_affinity->state && cpu == napi_cpu[idx][state]) {
			/* if bh is in current state and default cpu is same with online cpu */
			if (load_man.cpu_avail[cpu]) {
				/* if online cpu is available, move to online cpu */
				SLSI_DBG4_NODEV(SLSI_LBM, "state:%d default cpu:%d, bh cpu:%d\n",
						state, cpu, bh->cpu_affinity->cpu[state]);
				bh->cpu_affinity->cpu[state] = cpu;
				load_man.cpu_avail[bh->cpu_affinity->curr_cpu] = true;
				load_man.cpu_avail[cpu] = false;
				push_cpu_affinity_event(bh, state);
			}
		}
	} else {
		if (state == bh->cpu_affinity->state && cpu == bh->cpu_affinity->cpu[state]) {
			/* if bh is in current state and bh use offline cpu, find usable cpu */
			int target_cpu = napi_cpu[idx][state];

			while (target_cpu < SLSI_NR_CPUS) {
				/* cpu is available */
				if (load_man.cpu_avail[target_cpu]) {
					bh->cpu_affinity->cpu[state] = target_cpu;
					break;
				}
				target_cpu++;
			}

			/* if there is no available cpu, use the cpu for low state */
			if (target_cpu >= SLSI_NR_CPUS)
				bh->cpu_affinity->cpu[state] = napi_cpu[idx][TRAFFIC_MON_CLIENT_STATE_LOW];

			SLSI_DBG4_NODEV(SLSI_LBM, "state:%d default cpu:%d, bh cpu:%d\n",
					state, napi_cpu[idx][state], bh->cpu_affinity->cpu[state]);
			load_man.cpu_avail[bh->cpu_affinity->cpu[state]] = false;
			push_cpu_affinity_event(bh, state);
		}
	}
	SLSI_DBG4_NODEV(SLSI_LBM, "napi%d_cpu[%d]:%d, cpu_afficpu[%d]:%d\n", idx,
			state, napi_cpu[idx][state], state, bh->cpu_affinity->cpu[state]);
}

static int cpu_status_change_for_rps(struct net_device *dev, u32 state, int cpu, bool is_online)
{
	struct netdev_vif *ndev_vif;
	int ifnum;
	bool is_changed = false;
	unsigned long tmp, tmp_ndev;

	ndev_vif = netdev_priv(dev);
	ifnum = ndev_vif->ifnum;

	if (sscanf(ndev_vif->rps->rps[state], "%x\n", &tmp_ndev) < 0)
		return -EINVAL;

	if (is_online) {
		/* for netdevs, ndev_vif->rps[state] update.
		 * 1. default rps map has online cpu but netdev rps map doesn't have the cpu.
		 */
		if (sscanf(rps_map[state], "%x\n", &tmp) < 0)
			return -EINVAL;

		if (test_bit(cpu, &tmp) && !test_and_set_bit(cpu, &tmp_ndev)) {
			char buf[3];

			snprintf(buf, RPS_MAP_LEN, "%02x\n", tmp_ndev);
			memcpy(ndev_vif->rps->rps[state], buf, RPS_MAP_LEN);
			is_changed = true;
		}
	} else {
		/* for netdevs, ndev_vif->rps[state] update.
		 * 1. If some netdev rps maps have offline cpu, clear the bit in the rps map.
		 */
		if (test_and_clear_bit(cpu, &tmp_ndev)) {
			char buf[3];

			snprintf(buf, RPS_MAP_LEN, "%02x\n", tmp_ndev);
			memcpy(ndev_vif->rps->rps[state], buf, RPS_MAP_LEN);
			is_changed = true;
		}
	}

	if (is_changed) {
		SLSI_NET_DBG4(dev, SLSI_LBM, "ifnum:%d state%d, default rps:%s ndev rps:%s\n",
			      ifnum, state, rps_map[state], ndev_vif->rps->rps[state]);

		if (state == ndev_vif->rps->state)
			push_rps_event(dev, state);
	}

	return 0;
}

int slsi_lbm_cpuhp_online_cb(int cpu, void *data)
{
	struct slsi_dev *sdev = (struct slsi_dev *)data;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	struct list_head *pos, *n;
	struct bh_struct *bh;
	int ifnum, state;

	SLSI_INFO_NODEV("CPU%d is online\n", cpu);
	load_man.cpu_avail[cpu] = true;

	write_lock_bh(&load_man.bh_list_lock);
	list_for_each_safe(pos, n, &load_man.bh_list) {
		for (state = TRAFFIC_MON_CLIENT_STATE_LOW; state < TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE; state++) {
			bh = list_entry(pos, struct bh_struct, list);
			if (!bh->cpu_affinity)
				continue;
			cpu_status_change_for_affinity(bh, state, cpu, true);
		}
	}
	write_unlock_bh(&load_man.bh_list_lock);

	for (ifnum = SLSI_NET_INDEX_WLAN; ifnum < SLSI_NET_INDEX_NAN; ifnum++) {
		for (state = TRAFFIC_MON_CLIENT_STATE_LOW; state < TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE; state++) {
			dev = slsi_get_netdev_rcu(sdev, ifnum);
			ndev_vif = netdev_priv(dev);

			if (!ndev_vif->rps)
				continue;
			cpu_status_change_for_rps(dev, state, cpu, true);
		}
	}

	return 0;
}

int slsi_lbm_cpuhp_offline_cb(int cpu, void *data)
{
	struct slsi_dev *sdev = (struct slsi_dev *)data;
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	struct list_head *pos, *n;
	struct bh_struct *bh;
	int ifnum, state;

	SLSI_INFO_NODEV("CPU%d is offline\n", cpu);
	load_man.cpu_avail[cpu] = false;

	write_lock_bh(&load_man.bh_list_lock);
	list_for_each_safe(pos, n, &load_man.bh_list) {
		for (state = TRAFFIC_MON_CLIENT_STATE_LOW; state < TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE; state++) {
			bh = list_entry(pos, struct bh_struct, list);
			if (!bh->cpu_affinity)
				continue;
			cpu_status_change_for_affinity(bh, state, cpu, false);
		}
	}
	write_unlock_bh(&load_man.bh_list_lock);

	for (ifnum = SLSI_NET_INDEX_WLAN; ifnum < SLSI_NET_INDEX_NAN; ifnum++) {
		for (state = TRAFFIC_MON_CLIENT_STATE_LOW; state < TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE; state++) {
			dev = slsi_get_netdev_rcu(sdev, ifnum);
			ndev_vif = netdev_priv(dev);

			if (!ndev_vif->rps)
				continue;
			cpu_status_change_for_rps(dev, state, cpu, false);
		}
	}

	return 0;
}
#endif

void slsi_lbm_state_change_for_affinity(struct bh_struct *bh, u32 state, int force_cpu)
{
	int idx = bh->cpu_affinity->idx;
	int target_cpu;

	if (state == TRAFFIC_MON_CLIENT_STATE_LOW) {
		load_man.cpu_avail[bh->cpu_affinity->curr_cpu] = true;
		push_cpu_affinity_event(bh, state);
		return;
	}

	target_cpu = (force_cpu >= 0) ? force_cpu : napi_cpu[idx][state];
	if (load_man.cpu_avail[target_cpu]) {
		/* if target_cpu is available, use target_cpu */
		bh->cpu_affinity->cpu[state] = target_cpu;
		load_man.cpu_avail[bh->cpu_affinity->curr_cpu] = true;
		load_man.cpu_avail[target_cpu] = false;
		push_cpu_affinity_event(bh, state);
	} else if (bh->cpu_affinity->curr_cpu == target_cpu || idx >= NP_TX_0) {
		/* if target cpu is same as previous state cpu, use target_cpu */
		bh->cpu_affinity->cpu[state] = target_cpu;
		push_cpu_affinity_event(bh, state);
	} else {
		/* if other bh is using target_cpu,find usable cpu */
		while (target_cpu < SLSI_NR_CPUS) {
			/* cpu is available */
			if (load_man.cpu_avail[target_cpu]) {
				bh->cpu_affinity->cpu[state] = target_cpu;
				break;
			}
			target_cpu++;
		}
		/* if there is no available cpu, use the cpu for low state */
		if (target_cpu >= SLSI_NR_CPUS)
			bh->cpu_affinity->cpu[state] = napi_cpu[idx][TRAFFIC_MON_CLIENT_STATE_LOW];
		load_man.cpu_avail[bh->cpu_affinity->curr_cpu] = true;
		load_man.cpu_avail[bh->cpu_affinity->cpu[state]] = false;
		push_cpu_affinity_event(bh, state);
	}

	SLSI_DBG4_NODEV(SLSI_LBM, "napi%d_cpu[%d]:%d, cpu_afficpu[%d]:%d\n", idx,
			state, napi_cpu[idx][state], state, bh->cpu_affinity->cpu[state]);
}

static int slsi_lbm_rps_map_set(struct net_device *dev, char *buf, size_t len)
{
	struct rps_map *old_map, *map;
	cpumask_var_t mask;
	int err, cpu, i;
	static DEFINE_MUTEX(rps_map_mutex);

	if (!alloc_cpumask_var(&mask, GFP_KERNEL))
		return -ENOMEM;

	err = bitmap_parse(buf, len, cpumask_bits(mask), nr_cpumask_bits);
	if (err) {
		free_cpumask_var(mask);
		SLSI_NET_WARN(dev, "CPU bitmap parse failed\n");
		return err;
	}

	map = kzalloc(max_t(unsigned int, RPS_MAP_SIZE(cpumask_weight(mask)), L1_CACHE_BYTES), GFP_KERNEL);
	if (!map) {
		free_cpumask_var(mask);
		SLSI_NET_WARN(dev, "CPU mask alloc failed\n");
		return -ENOMEM;
	}

	i = 0;
	for_each_cpu_and(cpu, mask, cpu_online_mask)
		map->cpus[i++] = cpu;

	if (i) {
		map->len = i;
	} else {
		kfree(map);
		map = NULL;
	}

	mutex_lock(&rps_map_mutex);
	old_map = rcu_dereference_protected(dev->_rx->rps_map, mutex_is_locked(&rps_map_mutex));
	rcu_assign_pointer(dev->_rx->rps_map, map);

#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE)
	if (map)
		static_branch_inc(&rps_needed);
	if (old_map)
		static_branch_dec(&rps_needed);
#else
	if (map)
		static_key_slow_inc(&rps_needed);
	if (old_map)
		static_key_slow_dec(&rps_needed);
#endif
	mutex_unlock(&rps_map_mutex);

	if (old_map)
		kfree_rcu(old_map, rcu);

	free_cpumask_var(mask);
	SLSI_NET_INFO(dev, "rps_cpus map set(%s)\n", buf);
	return len;
}

void slsi_lbm_freeze(void)
{
	struct list_head *pos, *n;
	struct bh_struct *tmp;
	int i;

	SLSI_DBG3_NODEV(SLSI_LBM, "cancel all bh in load balance manager list\n");
	list_for_each_safe(pos, n, &load_man.bh_list) {
		tmp = list_entry(pos, struct bh_struct, list);

		switch (tmp->type) {
		case NP_T:
			if (test_and_clear_bit(SLSI_HIP_NAPI_STATE_ENABLED, &tmp->bh_priv.napi.napi_state))
				for (i = 0; i < SLSI_NR_CPUS; i++)
					napi_disable(&tmp->bh_priv.napi.cpu_info[i].napi_instance);
			break;
		case WQ_T:
			cancel_work_sync(&tmp->bh_priv.work.bh);
			break;
		case TL_T:
			tasklet_kill(&tmp->bh_priv.tasklet.bh);
			break;
		default:
			break;
		}
	}
}

void slsi_lbm_setup(struct bh_struct *bh)
{
	struct slsi_dev	*sdev;
	int i;

	if (!bh) {
		SLSI_ERR_NODEV("bh handle is NULL\n");
		return;
	}

	if (bh->type != NP_T) {
		SLSI_ERR_NODEV("Invalid bh->type %d\n", bh->type);
		return;
	}

	sdev = bh->sdev;

	if (!sdev || !sdev->service)
		return;

	if (atomic_read(&sdev->hip.hip_state) != SLSI_HIP_STATE_STARTED)
		return;

	SLSI_DBG3_NODEV(SLSI_LBM, "bh->type %d\n", bh->type);
	if (!test_and_set_bit(SLSI_HIP_NAPI_STATE_ENABLED, &bh->bh_priv.napi.napi_state))
		for (i = 0; i < SLSI_NR_CPUS; i++)
			napi_enable(&bh->bh_priv.napi.cpu_info[i].napi_instance);
}

unsigned int slsi_lbm_get_napi_cpu(int idx, u32 state)
{
	return napi_cpu[idx][state];
}
