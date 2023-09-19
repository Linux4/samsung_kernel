// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-misc.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-slsi_cpuhp_monitor.h"
#include "kunit-mock-traffic_monitor.h"
#include "../load_manager.c"

int *test_napi_poll(struct napi_struct *napi, int weight)
{
	struct napi_cpu_info *napi_cpu_info = container_of(napi, struct napi_cpu_info, napi_instance);

	printk("%s called.\n", __func__);

	if (napi_cpu_info->priv->bh->cpu_affinity->cpu[TRAFFIC_MON_CLIENT_STATE_LOW] <= smp_processor_id())
		if (weight > 0) return 0;

	return weight;
}

/* unit test function definition */
void test_slsi_lbm_set_property(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	set_property_disable = false;

	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_set_property(sdev));

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_lbm_set_property(NULL));
}

void test_push_rps_event(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u32 state = TRAFFIC_MON_CLIENT_STATE_LOW;
	dev->_rx = kunit_kzalloc(test, sizeof(struct netdev_rx_queue), GFP_KERNEL);
	ndev_vif->rps = kunit_kzalloc(test, sizeof(struct rps_ctrl_info), GFP_KERNEL);

	slsi_lbm_init(sdev);
	push_rps_event(dev, state);
	slsi_lbm_deinit(sdev);
}

void test_push_cpu_affinity_event(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct bh_struct *bh = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	u32 state = TRAFFIC_MON_CLIENT_STATE_MID;

	slsi_lbm_init(sdev);
	push_cpu_affinity_event(bh, state);

	slsi_lbm_deinit(sdev);
}

void test_lbm_ctrl_work_func(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct bh_struct *bh = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	u32 state = TRAFFIC_MON_CLIENT_STATE_LOW;

	dev->_rx = kunit_kzalloc(test, sizeof(struct netdev_rx_queue), GFP_KERNEL);
	ndev_vif->rps = kunit_kzalloc(test, sizeof(struct rps_ctrl_info), GFP_KERNEL);

	slsi_lbm_init(sdev);
	push_rps_event(dev, state);
	lbm_ctrl_work_func(NULL);

	bh->cpu_affinity = kzalloc(sizeof(struct cpu_affinity_ctrl_info), GFP_KERNEL);
	bh->cpu_affinity->idx = NP_RX_0;
	push_cpu_affinity_event(bh, state);
	lbm_ctrl_work_func(NULL);
	slsi_lbm_deinit(sdev);
}

void test_slsi_lbm_init(struct kunit *test)
{
	slsi_lbm_init(NULL);
	slsi_lbm_deinit(NULL);
}

void test_slsi_lbm_deinit(struct kunit *test)
{
	slsi_lbm_init(NULL);
	slsi_lbm_deinit(NULL);
}

void test_slsi_lbm_netdev_activate(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	dev->_rx = kunit_kzalloc(test, sizeof(struct netdev_rx_queue), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_netdev_activate(sdev, dev));

	if (dev->_rx->rps_map) {
		kfree(dev->_rx->rps_map);
		dev->_rx->rps_map = NULL;
	}

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->service->id = SCSC_SERVICE_ID_INVALID;
	slsi_lbm_set_property(sdev);
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_netdev_activate(sdev, dev));

	ndev_vif->sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_netdev_activate(sdev, dev));

	if (ndev_vif->rps) {
		kfree(ndev_vif->rps);
		ndev_vif->rps = NULL;
	}
}

void test_slsi_lbm_netdev_deactivate(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct bh_struct *bh;

	slsi_lbm_init(NULL);
	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_netdev_deactivate(NULL, dev, ndev_vif));
	slsi_lbm_deinit(NULL);

	ndev_vif->rps = kzalloc(sizeof(struct rps_ctrl_info), GFP_KERNEL);
	dev->_rx = kunit_kzalloc(test, sizeof(struct netdev_rx_queue), GFP_KERNEL);

	slsi_lbm_init(NULL);
	bh = slsi_lbm_register_napi(sdev, NULL, 0, 0);
	if (bh) {
		bh->cpu_affinity = kzalloc(sizeof(struct cpu_affinity_ctrl_info), GFP_KERNEL);
		bh->cpu_affinity->state = TRAFFIC_MON_CLIENT_STATE_MID;
	}
	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_netdev_deactivate(NULL, dev, ndev_vif));

	if (dev->_rx->rps_map) {
		kfree(dev->_rx->rps_map);
		dev->_rx->rps_map = NULL;
	}

	slsi_lbm_deinit(NULL);
}

void test_slsi_lbm_register_napi(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	int irq = 0;
	int idx = NP_TX_0;

	slsi_lbm_init(sdev);
	KUNIT_EXPECT_PTR_NE(test, NULL, slsi_lbm_register_napi(sdev, NULL, irq, idx));
	slsi_lbm_deinit(sdev);

	idx = NP_RX_0;
	slsi_lbm_init(sdev);
	KUNIT_EXPECT_PTR_NE(test, NULL, slsi_lbm_register_napi(sdev, NULL, irq, idx));
	slsi_lbm_deinit(sdev);
}

void test_slsi_lbm_register_tasklet(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	int irq = 0;

	slsi_lbm_init(sdev);
	KUNIT_EXPECT_PTR_NE(test, NULL, slsi_lbm_register_tasklet(sdev, NULL, irq));
	slsi_lbm_deinit(sdev);
}

void test_slsi_lbm_register_workqueue(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	int irq = 0;

	slsi_lbm_init(sdev);
	KUNIT_EXPECT_PTR_NE(test, NULL, slsi_lbm_register_workqueue(sdev, NULL, irq));
	slsi_lbm_deinit(sdev);
}

void test_slsi_lbm_register_cpu_affinity_control(struct kunit *test)
{
	struct bh_struct *bh = NULL;
	int idx = 0;

	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_lbm_register_cpu_affinity_control(bh, idx));

	bh = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	bh->type = TL_T;
	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_lbm_register_cpu_affinity_control(bh, idx));

	bh->type = NP_T;
	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_lbm_register_cpu_affinity_control(bh, idx));

	bh->sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	slsi_lbm_init(bh->sdev);
	KUNIT_EXPECT_PTR_NE(test, NULL, slsi_lbm_register_cpu_affinity_control(bh, idx));
	if (bh->cpu_affinity) {
		slsi_lbm_unregister_cpu_affinity_control(bh->sdev, bh);
	}

	slsi_lbm_deinit(bh->sdev);
}

void test_slsi_lbm_register_io_saturation_control(struct kunit *test)
{
	struct bh_struct *bh = NULL;

	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_lbm_register_io_saturation_control(bh));

	bh = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	KUNIT_EXPECT_PTR_NE(test, NULL, slsi_lbm_register_io_saturation_control(bh));

	if (bh->io_saturation) {
		slsi_lbm_unregister_io_saturation_control(bh);
	}
}

void test_slsi_lbm_register_rps_control(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	const int mid = 50;
	const int high = 100;

	KUNIT_EXPECT_PTR_EQ(test, NULL, slsi_lbm_register_rps_control(dev, mid, high));

	ndev_vif->sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	KUNIT_EXPECT_PTR_NE(test, NULL, slsi_lbm_register_rps_control(dev, mid, high));

	if (ndev_vif->rps) {
		kfree(ndev_vif->rps);
		ndev_vif->rps = NULL;
	}

	ndev_vif->rps = kunit_kzalloc(test, sizeof(struct rps_ctrl_info), GFP_KERNEL);
	KUNIT_EXPECT_PTR_NE(test, NULL, slsi_lbm_register_rps_control(dev, mid, high));
}

void test_slsi_lbm_unregister_bh(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct bh_struct *bh = NULL;

	KUNIT_EXPECT_EQ(test, -ENOENT, slsi_lbm_unregister_bh(bh));

	bh = slsi_lbm_register_napi(sdev, NULL, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_unregister_bh(bh));

	bh = slsi_lbm_register_workqueue(sdev, NULL, 0);
	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_unregister_bh(bh));

	bh = slsi_lbm_register_tasklet(sdev, NULL, 0);
	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_unregister_bh(bh));

	bh->type = -1;
	KUNIT_EXPECT_EQ(test, -ENOENT, slsi_lbm_unregister_bh(bh));
}

void test_slsi_lbm_unregister_cpu_affinity_control(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct bh_struct *bh = NULL;

	slsi_lbm_unregister_cpu_affinity_control(sdev, bh);

	bh = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	bh->cpu_affinity = kzalloc(sizeof(struct cpu_affinity_ctrl_info), GFP_KERNEL);
	slsi_lbm_unregister_cpu_affinity_control(sdev, bh);
}

void test_slsi_lbm_unregister_io_saturation_control(struct kunit *test)
{
	struct bh_struct *bh = NULL;

	slsi_lbm_unregister_io_saturation_control(bh);

	bh = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	bh->io_saturation = kzalloc(sizeof(struct io_saturatioin_ctrl_info), GFP_KERNEL);
	slsi_lbm_unregister_io_saturation_control(bh);
}

void test_slsi_lbm_unregister_rps_control(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	ndev_vif->rps = kzalloc(sizeof(struct rps_ctrl_info), GFP_KERNEL);

	slsi_lbm_unregister_rps_control(dev);
}

void test_trigger_napi(struct kunit *test)
{
	struct napi_struct *napi = kunit_kzalloc(test, sizeof(struct napi_struct), GFP_KERNEL);

	trigger_napi((void *)napi);
}

void test_slsi_lbm_napi_poll(struct kunit *test)
{
	struct napi_cpu_info *napi_cpu_info = kunit_kzalloc(test, sizeof(struct napi_cpu_info), GFP_KERNEL);
	int budget = 0;
	int work_done = budget;

	napi_cpu_info->priv = kunit_kzalloc(test, sizeof(struct napi_priv), GFP_KERNEL);
	napi_cpu_info->priv->bh = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	
	napi_cpu_info->priv->bh->cpu_affinity = kunit_kzalloc(test, sizeof(struct cpu_affinity_ctrl_info), GFP_KERNEL);
	napi_cpu_info->priv->bh->cpu_affinity->state = TRAFFIC_MON_CLIENT_STATE_LOW;
	napi_cpu_info->priv->bh->cpu_affinity->cpu[TRAFFIC_MON_CLIENT_STATE_LOW] = smp_processor_id() +1;

	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_napi_poll(&napi_cpu_info->napi_instance, budget));

	napi_cpu_info->priv->bh->cpu_affinity->cpu[TRAFFIC_MON_CLIENT_STATE_LOW] = smp_processor_id();
	napi_cpu_info->priv->bh->bh_priv.napi.poll = test_napi_poll;
	napi_cpu_info->priv->bh->io_saturation = kunit_kzalloc(test, sizeof(struct io_saturatioin_ctrl_info), GFP_KERNEL);
	napi_cpu_info->priv->bh->cpu_affinity->curr_cpu = 0;
	napi_cpu_info->priv->bh->cpu_affinity->cpu[TRAFFIC_MON_CLIENT_STATE_HIGH] = 8;

	KUNIT_EXPECT_EQ(test, work_done, slsi_lbm_napi_poll(&napi_cpu_info->napi_instance, budget));

	budget = 1;
	KUNIT_EXPECT_EQ(test, work_done, slsi_lbm_napi_poll(&napi_cpu_info->napi_instance, budget));
}

void test_slsi_lbm_run_bh(struct kunit *test)
{
	struct bh_struct *bh = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);

	bh->type = NP_T;
	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_run_bh(bh));

	bh->type = WQ_T;
	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_run_bh(bh));

	bh->type = TL_T;
	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_run_bh(bh));

	bh->type = -1;
	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_run_bh(bh));
}

void test_slsi_lbm_traffic_mon_irq_affinity_cb(struct kunit *test)
{
	struct bh_struct *bh = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	u32 state = TRAFFIC_MON_CLIENT_STATE_MID;
	u32 tput_tx = 100000000;
	u32 tput_rx = 200000000;

	bh->sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	bh->cpu_affinity = kunit_kzalloc(test, sizeof(struct cpu_affinity_ctrl_info), GFP_KERNEL);
	bh->cpu_affinity->state = TRAFFIC_MON_CLIENT_STATE_LOW;

	slsi_lbm_init(bh->sdev);
	slsi_lbm_traffic_mon_irq_affinity_cb((void *)bh, state, tput_tx, tput_rx);
	slsi_lbm_deinit(bh->sdev);
}

void test_slsi_lbm_traffic_mon_rps_affinity_cb(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u32 state = TRAFFIC_MON_CLIENT_STATE_MID;
	u32 tput_tx = 100000000;
	u32 tput_rx = 200000000;

	ndev_vif->sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	ndev_vif->rps = kunit_kzalloc(test, sizeof(struct rps_ctrl_info), GFP_KERNEL);
	dev->_rx = kunit_kzalloc(test, sizeof(struct netdev_rx_queue), GFP_KERNEL);

	slsi_lbm_init(ndev_vif->sdev);
	slsi_lbm_traffic_mon_rps_affinity_cb((void *)dev, state, tput_tx, tput_rx);
	slsi_lbm_deinit(ndev_vif->sdev);
}

#ifdef CONFIG_SCSC_WLAN_CPUHP_MONITOR
void test_cpu_status_change_for_affinity(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct bh_struct *bh = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	u32 state = TRAFFIC_MON_CLIENT_STATE_LOW;
	int cpu = 0;
	bool is_online = true;

	bh->cpu_affinity = kunit_kzalloc(test, sizeof(struct cpu_affinity_ctrl_info), GFP_KERNEL);
	bh->cpu_affinity->idx = 0;
	bh->cpu_affinity->state = state;

	slsi_lbm_init(NULL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	slsi_lbm_set_property(sdev);

	cpu_status_change_for_affinity(bh, state, cpu, is_online);
	slsi_lbm_deinit(NULL);

	is_online = false;
	bh->cpu_affinity->cpu[state] = cpu;

	slsi_lbm_init(NULL);
	cpu_status_change_for_affinity(bh, state, cpu, is_online);
	slsi_lbm_deinit(NULL);
}

void test_cpu_status_change_for_rps(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u32 state = 0;
	int cpu = 1;
	bool is_online = false;

	ndev_vif->rps = kunit_kzalloc(test, sizeof(struct rps_ctrl_info), GFP_KERNEL);
	dev->_rx = kunit_kzalloc(test, sizeof(struct netdev_rx_queue), GFP_KERNEL);
	ndev_vif->rps->state = state;
	ndev_vif->rps->rps[state][0] = '5';
	ndev_vif->rps->rps[state][1] = '0';
	KUNIT_EXPECT_EQ(test, 0, cpu_status_change_for_rps(dev, state, cpu, is_online));

	is_online = true;
	ndev_vif->rps->rps[state][0] = '6';
	ndev_vif->rps->rps[state][1] = '0';
	KUNIT_EXPECT_EQ(test, 0, cpu_status_change_for_rps(dev, state, cpu, is_online));
}

void test_slsi_lbm_cpuhp_online_cb(struct kunit *test)
{
	int cpu = 0;
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	int ifnum, state;

	for (ifnum = SLSI_NET_INDEX_WLAN; ifnum < SLSI_NET_INDEX_NAN; ifnum++) {
		for (state = TRAFFIC_MON_CLIENT_STATE_LOW; state < TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE; state++) {
			dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
			sdev->netdev[ifnum] = dev;
			ndev_vif = netdev_priv(dev);
			if (ifnum == SLSI_NET_INDEX_WLAN && state == TRAFFIC_MON_CLIENT_STATE_LOW)
				ndev_vif->rps = kunit_kzalloc(test, sizeof(struct rps_ctrl_info), GFP_KERNEL);
		}
	}

	slsi_lbm_init(sdev);
	slsi_lbm_register_napi(sdev, NULL, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_cpuhp_online_cb(cpu, (void *)sdev));
	slsi_lbm_deinit(sdev);
}

void test_slsi_lbm_cpuhp_offline_cb(struct kunit *test)
{
	int cpu = 0;
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	int ifnum, state;

	for (ifnum = SLSI_NET_INDEX_WLAN; ifnum < SLSI_NET_INDEX_NAN; ifnum++) {
		for (state = TRAFFIC_MON_CLIENT_STATE_LOW; state < TRAFFIC_MON_CLIENT_MAX_NUM_OF_STATE; state++) {
			dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
			sdev->netdev[ifnum] = dev;
			ndev_vif = netdev_priv(dev);
			if (ifnum == SLSI_NET_INDEX_WLAN && state == TRAFFIC_MON_CLIENT_STATE_LOW)
				ndev_vif->rps = kunit_kzalloc(test, sizeof(struct rps_ctrl_info), GFP_KERNEL);
		}
	}

	slsi_lbm_init(sdev);
	slsi_lbm_register_napi(sdev, NULL, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, slsi_lbm_cpuhp_offline_cb(cpu, (void *)sdev));
	slsi_lbm_deinit(sdev);
}
#endif

void test_state_change_for_affinity(struct kunit *test)
{
	struct bh_struct *bh = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	u32 state = TRAFFIC_MON_CLIENT_STATE_LOW;

	bh->cpu_affinity = kunit_kzalloc(test, sizeof(struct cpu_affinity_ctrl_info), GFP_KERNEL);
	bh->cpu_affinity->idx = 0;

	slsi_lbm_init(NULL);
	slsi_lbm_state_change_for_affinity(bh, state, -1);
	slsi_lbm_deinit(NULL);

	state = TRAFFIC_MON_CLIENT_STATE_MID;

	slsi_lbm_init(NULL);
	slsi_lbm_state_change_for_affinity(bh, state, -1);
	slsi_lbm_deinit(NULL);

	bh->cpu_affinity->curr_cpu = 1;
	slsi_lbm_state_change_for_affinity(bh, state, -1);
	slsi_lbm_deinit(NULL);
}

void test_slsi_lbm_rps_map_set(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	char *buf = "1a";
	size_t len = 3;

	dev->_rx = kunit_kzalloc(test, sizeof(struct netdev_rx_queue), GFP_KERNEL);
	slsi_lbm_rps_map_set(dev, buf, len);

	if (dev->_rx->rps_map) {
		kfree(dev->_rx->rps_map);
		dev->_rx->rps_map = NULL;
	}
}

void test_slsi_lbm_freeze(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct bh_struct *bh;

	slsi_lbm_init(NULL);
	bh = slsi_lbm_register_napi(sdev, NULL, 0, 0);

	bh->type = NP_T;
	slsi_lbm_freeze();

	bh->type = WQ_T;
	slsi_lbm_freeze();

	bh->type = TL_T;
	slsi_lbm_freeze();

	bh->type = -1;
	slsi_lbm_freeze();
	slsi_lbm_deinit(NULL);
}

void test_slsi_lbm_setup(struct kunit *test)
{
	struct bh_struct *bh = NULL;
	int budget = 0;
	int work_done = budget;

	slsi_lbm_setup(bh);

	bh = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	bh->sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	bh->sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);

	atomic_set(&bh->sdev->hip.hip_state, SLSI_HIP_STATE_STARTED);

	slsi_lbm_setup(bh);

	bh->type = TL_T;
	slsi_lbm_setup(bh);
}


/* Test fictures */
static int load_manager_test_init(struct kunit *test)
{
	set_property_disable = false;
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
	return 0;
}

static void load_manager_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case load_manager_test_cases[] = {
	KUNIT_CASE(test_slsi_lbm_set_property),
	KUNIT_CASE(test_slsi_lbm_init),
	KUNIT_CASE(test_push_cpu_affinity_event),
	KUNIT_CASE(test_lbm_ctrl_work_func),
	KUNIT_CASE(test_push_rps_event),
	KUNIT_CASE(test_slsi_lbm_netdev_activate),
	KUNIT_CASE(test_slsi_lbm_netdev_deactivate),
	KUNIT_CASE(test_slsi_lbm_register_napi),
	KUNIT_CASE(test_slsi_lbm_register_tasklet),
	KUNIT_CASE(test_slsi_lbm_register_workqueue),
	KUNIT_CASE(test_slsi_lbm_register_cpu_affinity_control),
	KUNIT_CASE(test_slsi_lbm_register_io_saturation_control),
	KUNIT_CASE(test_slsi_lbm_register_rps_control),
	KUNIT_CASE(test_trigger_napi),
	KUNIT_CASE(test_slsi_lbm_napi_poll),
	KUNIT_CASE(test_slsi_lbm_run_bh),
	KUNIT_CASE(test_slsi_lbm_traffic_mon_irq_affinity_cb),
	KUNIT_CASE(test_slsi_lbm_traffic_mon_rps_affinity_cb),
#ifdef CONFIG_SCSC_WLAN_CPUHP_MONITOR
	KUNIT_CASE(test_cpu_status_change_for_affinity),
	KUNIT_CASE(test_cpu_status_change_for_rps),
	KUNIT_CASE(test_slsi_lbm_cpuhp_online_cb),
	KUNIT_CASE(test_slsi_lbm_cpuhp_offline_cb),
#endif
	KUNIT_CASE(test_state_change_for_affinity),
	KUNIT_CASE(test_slsi_lbm_rps_map_set),
	KUNIT_CASE(test_slsi_lbm_freeze),
	KUNIT_CASE(test_slsi_lbm_setup),
	KUNIT_CASE(test_slsi_lbm_unregister_bh),
	KUNIT_CASE(test_slsi_lbm_unregister_cpu_affinity_control),
	KUNIT_CASE(test_slsi_lbm_unregister_io_saturation_control),
	KUNIT_CASE(test_slsi_lbm_unregister_rps_control),
	KUNIT_CASE(test_slsi_lbm_deinit),
	{}
};

static struct kunit_suite load_manager_test_suite[] = {
	{
		.name = "load_manager-test",
		.test_cases = load_manager_test_cases,
		.init = load_manager_test_init,
		.exit = load_manager_test_exit,
	}
};

kunit_test_suites(load_manager_test_suite);
