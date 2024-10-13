// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-kernel.h"
#include "kunit-mock-misc.h"
#include "kunit-mock-mbulk.h"
#include "kunit-mock-hip.h"
#include "kunit-mock-hip4_sampler.h"
#include "kunit-mock-load_manager.h"
#include "kunit-mock-log_clients.h"
#include "kunit-mock-dpd_mmap.h"
#include "../hip4.c"


/* Test function */
/* hip4.c */
static void test_hip4_update_index(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u32 q = 0;
	u8 value = 0;

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	hip4_update_index(&sdev->hip, q, 0, value);
}

static void test_hip4_read_index(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 scbrd_base;

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	memset(&scbrd_base + 66, 0x1, sizeof(u8));
	memset(&scbrd_base + 2, 0x0, sizeof(u8));

	sdev->hip.hip_priv->version = 4;
	sdev->hip.hip_priv->scbrd_base = &scbrd_base;

	KUNIT_EXPECT_EQ(test, 1, hip4_read_index(&sdev->hip, 2, 0));
}

static void test_hip4_dump_dbg(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct mbulk *m = kunit_kzalloc(test, sizeof(struct mbulk), GFP_KERNEL);
	u32 q;

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, sizeof(unsigned char) * 16, GFP_KERNEL);
	skb->len = 24;

	hip4_dump_dbg(&sdev->hip, m, skb, sdev->service);
}

static void test_slsi_hip_from_host_intr_set(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);

	slsi_hip_from_host_intr_set(sdev->service, &sdev->hip);
}

static void test_hip4_skb_to_mbulk(struct kunit *test)
{

	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct sk_buff *skb;
	struct slsi_skb_cb *cb;
	struct skb_shared_info *skb_sh;
	char data[10];
	mbulk_colour colour;

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb_sh = kunit_kzalloc(test, sizeof(struct skb_shared_info), GFP_KERNEL);

	cb = slsi_skb_cb_get(skb);
	sdev->hip.hip_priv->unidat_req_headroom = 10;
	sdev->hip.hip_priv->unidat_req_tailroom = 10;
	cb->sig_length = 0;
	skb->len = 10;
	skb->data = data;

	KUNIT_EXPECT_PTR_NE(test, NULL, hip4_skb_to_mbulk(&sdev->hip.hip_priv, skb,
							  MBULK_POOL_ID_DATA, 0x0));
}

static void test_hip4_mbulk_to_skb(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct mbulk *m = kunit_kzalloc(test, sizeof(struct mbulk), GFP_KERNEL);
	scsc_mifram_ref to_free;

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);

	to_free = 0;
	m->len = 10;
	m->chain_next_offset = 0x1;
	m->sig_bufsz = 10;

	m->flag = MBULK_F_CHAIN_HEAD | MBULK_F_CHAIN;
	KUNIT_EXPECT_PTR_EQ(test, NULL, hip4_mbulk_to_skb(sdev->service, sdev->hip.hip_priv, m, &to_free, true));

	m->flag = MBULK_F_SIG;
	KUNIT_EXPECT_PTR_NE(test, NULL, hip4_mbulk_to_skb(sdev->service, sdev->hip.hip_priv, m, &to_free, true));

	m->chain_next_offset = NULL;
	KUNIT_EXPECT_PTR_NE(test, NULL, hip4_mbulk_to_skb(sdev->service, sdev->hip.hip_priv, m, &to_free, true));
}

static void test_hip4_q_add_signal(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct hip4_hip_q *q = kunit_kzalloc(test, sizeof(struct hip4_hip_q), GFP_KERNEL);
	scsc_mifram_ref phy_m = 0;

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);

	sdev->hip.hip_control = kunit_kzalloc(test, sizeof(struct hip4_hip_control), GFP_KERNEL);
	memcpy(sdev->hip.hip_control->q, q, sizeof(struct hip4_hip_q));

	KUNIT_EXPECT_EQ(test, 0, hip4_q_add_signal(&sdev->hip, HIP4_MIF_Q_FH_CTRL, phy_m, sdev->service));
}

static void test_hip4_watchdog(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct timer_list *t = kunit_kzalloc(test, sizeof(struct timer_list), GFP_KERNEL);

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);

	sdev->hip.hip_priv->watchdog_timer_active.counter = 1;
	set_bit(HIP4_MIF_Q_FH_RFB, sdev->hip.hip_priv->irq_bitmap);

#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	timer_setup(&sdev->hip.hip_priv->watchdog, hip4_watchdog, 0);
#else
	setup_timer(&sdev->hip.hip_priv->watchdog, hip4_watchdog, (unsigned long)(&sdev->hip));
#endif
}

static void test_hip_wlan_get_rtc_time(struct kunit *test)
{
	struct rtc_time *tm = kunit_kzalloc(test, sizeof(struct rtc_time), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, slsi_hip_wlan_get_rtc_time(tm));
}

static void test_hip4_tl_fb(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct hip4_hip_q *q = kunit_kzalloc(test, sizeof(struct hip4_hip_q) * 6, GFP_KERNEL);
	u8 scbrd_base;

	memset(&scbrd_base + 66, 0x1, sizeof(u8));
	memset(&scbrd_base + 2, 0x0, sizeof(u8));

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	sdev->hip.hip_priv->version = 4;
	sdev->hip.hip_priv->scbrd_base = &scbrd_base;
	sdev->hip.hip_priv->hip4_wake_lock_tx.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	wakeup_source_add(sdev->hip.hip_priv->hip4_wake_lock_tx.ws);

	sdev->hip.hip_control = kunit_kzalloc(test, sizeof(struct hip4_hip_control), GFP_KERNEL);
	memcpy(sdev->hip.hip_control->q, q, sizeof(struct hip4_hip_q));

	sdev->hip.sdev = sdev;
	sdev->hip.hip_priv->hip = &sdev->hip;

	hip4_tl_fb(&sdev->hip);
}

static void test_hip4_irq_handler_fb(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	sdev->hip.hip_priv->minor = 0;
	sdev->hip.hip_priv->bh_rfb = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	sdev->hip.hip_priv->hip4_wake_lock_tx.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	wakeup_source_add(sdev->hip.hip_priv->hip4_wake_lock_tx.ws);

	atomic_set(&sdev->hip.hip_priv->watchdog_timer_active, 1);
	hip4_irq_handler_fb(1, (void *)&sdev->hip);
}

static void test_hip4_wq_ctrl(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct work_priv w_priv;
	u8 scbrd_base[50];

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	w_priv.bh_s = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	w_priv.bh_s->hip_priv = sdev->hip.hip_priv;

	memset(scbrd_base + 66, 0x1, sizeof(u8));
	memset(scbrd_base + 2, 0x0, sizeof(u8));

	sdev->hip.hip_priv->version = 4;
	sdev->hip.hip_control = kunit_kzalloc(test, sizeof(struct hip4_hip_control), GFP_KERNEL);
	sdev->hip.hip_priv->scbrd_base = scbrd_base;
	sdev->hip.hip_priv->hip = &sdev->hip;
	sdev->hip.hip_priv->hip4_wake_lock_ctrl.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	wakeup_source_add(sdev->hip.hip_priv->hip4_wake_lock_ctrl.ws);

	hip4_wq_ctrl(&w_priv.bh);
}

static void test_hip4_irq_handler_ctrl(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	sdev->hip.hip_priv->version = 4;
	sdev->hip.hip_control = kunit_kzalloc(test, sizeof(struct hip4_hip_control), GFP_KERNEL);
	sdev->hip.hip_priv->hip4_wake_lock_ctrl.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	wakeup_source_add(sdev->hip.hip_priv->hip4_wake_lock_ctrl.ws);
	atomic_set(&sdev->hip.hip_priv->watchdog_timer_active, 1);

	hip4_irq_handler_ctrl(1, &sdev->hip);
}

static void test_hip_sched_wq_ctrl(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u8 scbrd_base[50];

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	memset(scbrd_base + 66, 0x1, sizeof(u8));
	memset(scbrd_base + 2, 0x0, sizeof(u8));

	sdev->hip.hip_priv->version = 4;
	sdev->hip.hip_control = kunit_kzalloc(test, sizeof(struct hip4_hip_control), GFP_KERNEL);
	sdev->hip.hip_priv->scbrd_base = scbrd_base;
	sdev->hip.hip_priv->hip4_wake_lock_ctrl.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	wakeup_source_add(sdev->hip.hip_priv->hip4_wake_lock_ctrl.ws);

	slsi_hip_sched_wq_ctrl(&sdev->hip);
}

static void test_hip4_irq_handler_dat(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->hip.hip_priv->hip4_wake_lock_data.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	sdev->hip.hip_priv->bh_dat = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	wakeup_source_add(sdev->hip.hip_priv->hip4_wake_lock_data.ws);
	atomic_set(&sdev->hip.hip_priv->watchdog_timer_active, 1);
	set_bit(SLSI_HIP_NAPI_STATE_ENABLED, &sdev->hip.hip_priv->bh_dat->bh_priv.napi.napi_state);

	hip4_irq_handler_dat(1, &sdev->hip);
}

static void test_hip4_pm_qos_work(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	sdev->hip.hip_priv->hip = &sdev->hip;

	hip4_pm_qos_work(&sdev->hip.hip_priv->pm_qos_work);
}

static void test_hip4_traffic_monitor_cb(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->hip.hip_priv->hip = &sdev->hip;

	hip4_traffic_monitor_cb(&(sdev->hip), 2, 10, 10);
	hip4_traffic_monitor_cb(&(sdev->hip), 0, 10, 10);
}

static void test_hip4_traffic_monitor_logring_cb(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	hip4_traffic_monitor_logring_cb(sdev->hip.hip_priv, 2, 10, 10);
	hip4_traffic_monitor_logring_cb(sdev->hip.hip_priv, 0, 10, 10);
}

static void test_hip_init(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *ndev;
	struct scsc_service *service;

	ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->netdev[SLSI_NET_INDEX_WLAN] = ndev;

	slsi_hip_init(&sdev->hip);
	kfree(sdev->hip.hip_priv->bh_dat);
	kfree(sdev->hip.hip_priv);
}

static void test_hip_free_control_slots_count(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, slsi_hip_free_control_slots_count(&sdev->hip));
}

static void test_slsi_hip_transmit_frame(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct hip4_hip_q *q;
	u8 scbrd_base;
	scsc_mifram_ref phy_m;

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	q = kunit_kzalloc(test, sizeof(struct hip4_hip_q) * 1, GFP_KERNEL);
	phy_m = 0;
	memset(&scbrd_base + 66, 0x1, sizeof(u8));
	memset(&scbrd_base + 2, 0x0, sizeof(u8));

	skb->data = kunit_kzalloc(test, sizeof(struct fapi_signal) + 100, GFP_KERNEL);

	sdev->hip.hip_priv->version = 4;
	sdev->hip.hip_control = kunit_kzalloc(test, sizeof(struct hip4_hip_control), GFP_KERNEL);
	memcpy(sdev->hip.hip_control->q, q, sizeof(struct hip4_hip_q));
	sdev->hip.hip_priv->hip4_wake_lock_tx.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	sdev->hip.hip_priv->scbrd_base = &scbrd_base;
	wakeup_source_add(sdev->hip.hip_priv->hip4_wake_lock_tx.ws);

	KUNIT_EXPECT_EQ(test, 0, slsi_hip_transmit_frame(&sdev->hip, skb, true, 0, 1, 1));
}

static void test_hip_setup(struct kunit *test)
{

	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);

	sdev->hip.hip_priv->version = 4;
	sdev->hip.hip_control = kunit_kzalloc(test, sizeof(struct hip4_hip_control), GFP_KERNEL);
	sdev->hip.hip_control->init.conf_hip4_ver = 4;

	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STARTED);
	KUNIT_EXPECT_EQ(test, 0, slsi_hip_setup(&sdev->hip));

	sdev->hip.hip_control->init.conf_hip4_ver = 5;
	KUNIT_EXPECT_EQ(test, 0, slsi_hip_setup(&sdev->hip));

}

static void test_hip_suspend(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STARTED);

	slsi_hip_suspend(&sdev->hip);
}

static void test_hip_resume(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);

	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STARTED);

	slsi_hip_resume(&sdev->hip);
}

static void test_hip_freeze(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);

	sdev->hip.hip_priv->hip4_workq = alloc_workqueue("hip4", 0, 0);
	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STARTED);
	slsi_hip_freeze(&sdev->hip);
}

static void test_hip_deinit(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	struct work_struct *work = kunit_kzalloc(test, sizeof(struct work_struct), GFP_KERNEL);
	/* For freeing hip4_workq */
	struct workqueue_struct *hip4_wq;

	sdev->hip.hip_priv = kzalloc(sizeof(struct hip_priv), GFP_KERNEL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);

	sdev->netdev[SLSI_NET_INDEX_WLAN] = ndev;
	sdev->device_wq = work;

	sdev->hip.hip_priv->hip4_workq = alloc_workqueue("hip4", 0, 0);
	hip4_wq = sdev->hip.hip_priv->hip4_workq;
	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	slsi_hip_deinit(&sdev->hip);
}

/* Test fictures*/
static int hip4_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void hip4_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case hip4_test_cases[] = {
	KUNIT_CASE(test_hip_init),
	KUNIT_CASE(test_hip4_tl_fb),
	KUNIT_CASE(test_hip4_update_index),
	KUNIT_CASE(test_hip4_read_index),
	KUNIT_CASE(test_hip4_dump_dbg),
	KUNIT_CASE(test_slsi_hip_from_host_intr_set),
	KUNIT_CASE(test_hip4_skb_to_mbulk),
	KUNIT_CASE(test_hip4_mbulk_to_skb),
	KUNIT_CASE(test_hip4_q_add_signal),
	KUNIT_CASE(test_hip4_watchdog),
	KUNIT_CASE(test_hip_wlan_get_rtc_time),
	KUNIT_CASE(test_hip4_irq_handler_fb),
	KUNIT_CASE(test_hip4_wq_ctrl),
	KUNIT_CASE(test_hip4_irq_handler_ctrl),
	KUNIT_CASE(test_hip_sched_wq_ctrl),
	KUNIT_CASE(test_hip4_irq_handler_dat),
	KUNIT_CASE(test_hip4_pm_qos_work),
	KUNIT_CASE(test_hip4_traffic_monitor_cb),
	KUNIT_CASE(test_hip4_traffic_monitor_logring_cb),
	KUNIT_CASE(test_hip_free_control_slots_count),
	KUNIT_CASE(test_slsi_hip_transmit_frame),
	KUNIT_CASE(test_hip_setup),
	KUNIT_CASE(test_hip_suspend),
	KUNIT_CASE(test_hip_resume),
	KUNIT_CASE(test_hip_freeze),
	KUNIT_CASE(test_hip_deinit),
	{}
};

static struct kunit_suite hip4_test_suite[] = {
	{
		.name = "kunit-hip4-test",
		.test_cases = hip4_test_cases,
		.init = hip4_test_init,
		.exit = hip4_test_exit,
	}
};

kunit_test_suites(hip4_test_suite);
