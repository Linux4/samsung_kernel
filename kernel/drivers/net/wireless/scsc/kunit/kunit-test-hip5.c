// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-load_manager.h"
#include "kunit-mock-log_clients.h"
#include "kunit-mock-mbulk.h"
#include "kunit-mock-netif.h"
#include "kunit-mock-misc.h"
#include "kunit-mock-hip.h"
#include "kunit-mock-traffic_monitor.h"
#include "kunit-mock-hip4_sampler.h"
#include "kunit-mock-dpd_mmap.h"
#include "../hip5.c"

static void alloc_common_mem_for_test(struct kunit *test, struct slsi_dev *sdev)
{
	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->hip.hip_priv->hip = &sdev->hip;
	sdev->hip.hip_control = kunit_kzalloc(test, sizeof(struct hip5_hip_control), GFP_KERNEL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
}

static void test_slsi_hip_init(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *ndev;

	alloc_common_mem_for_test(test, sdev);
	ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	sdev->netdev[SLSI_NET_INDEX_WLAN] = ndev;

	slsi_hip_init(&sdev->hip);
	kfree(sdev->hip.hip_priv->bh_dat);
	kfree(sdev->hip.hip_priv);
}

static void test_slsi_hip_init_control_table(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u8 *hip_ptr = kunit_kzalloc(test, sizeof(u8) * HIP5_WLAN_MIB_OFFSET + HIP5_WLAN_MIB_SIZE, GFP_KERNEL);
	char mib_fname = 0x3;

	for (int i = 0 ; i < SLSI_WLAN_MAX_MIB_FILE; i++) {
		sdev->mib[i].mib_data = kunit_kzalloc(test, sizeof(u8), GFP_KERNEL);
		sdev->mib[i].mib_file_name = &mib_fname;
		sdev->mib[i].mib_len = 10;
	}
	alloc_common_mem_for_test(test, sdev);

	slsi_hip_init_control_table(sdev, &sdev->hip, hip_ptr, sdev->hip.hip_control, sdev->service);
}

static void test_slsi_hip_free_control_slots_count(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);
	KUNIT_EXPECT_EQ(test, 0, slsi_hip_free_control_slots_count(&sdev->hip));
}

static void test_hip5_opt_aggr_check(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb;

	alloc_common_mem_for_test(test, sdev);
	skb = fapi_alloc(ma_unitdata_req, MA_UNITDATA_REQ, 0, 1);
	((struct fapi_vif_signal_header *)(skb)->data)->vif  = 0;
	sdev->hip.hip_priv->hip5_opt_tx_q[0].vif_index = 0;
	memset(sdev->hip.hip_priv->hip5_opt_tx_q[0].addr3, 0x1, sizeof(u8) * ETH_ALEN);
	KUNIT_EXPECT_TRUE(test, hip5_opt_aggr_check(sdev, &sdev->hip, skb));

	/* Cannot find match */
	sdev->hip.hip_priv->hip5_opt_tx_q[0].vif_index = 5;
	((struct fapi_vif_signal_header *)(skb)->data)->vif  = 1;
	KUNIT_EXPECT_FALSE(test, hip5_opt_aggr_check(sdev, &sdev->hip, skb));
}

static void test_hip5_opt_tx_frame(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb;

	skb = fapi_alloc(ma_unitdata_req, MA_UNITDATA_REQ, 0, 1);
	((struct fapi_signal *)(skb)->data)->id = FAPI_SAP_TYPE_MA;
	skb->dev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_priv->wake_lock_tx.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	sdev->hip.hip_control->init.magic_number = 0x0;

	wakeup_source_add(sdev->hip.hip_priv->wake_lock_tx.ws);
	slsi_wake_lock_init(NULL, &sdev->hip.hip_priv->wake_lock_tx.ws, "hip_wake_lock_tx");
	KUNIT_EXPECT_EQ(test, -ENOSPC, hip5_opt_tx_frame(&sdev->hip, skb, true, 0, 0, 0));
}

static void test_hip5_dump_dbg(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb;
	struct mbulk *m;

	alloc_common_mem_for_test(test, sdev);
	skb = fapi_alloc(ma_unitdata_req, MA_UNITDATA_REQ, 0, 100);
	m = kunit_kzalloc(test, sizeof(struct mbulk), GFP_KERNEL);
	skb_put(skb, 24);

	hip5_dump_dbg(&sdev->hip, m, skb, sdev->service);
	kfree_skb(skb);
}

static void test_hip5_read_index(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_control->init.magic_number = 0x0;

	sdev->hip.hip_priv->version = 5;
	sdev->hip.hip_priv->scbrd_base = sdev->hip.hip_control->scoreboard;

	KUNIT_EXPECT_EQ(test, 65535, hip5_read_index(&sdev->hip, 2, 0));
	sdev->hip.hip_control->init.magic_number = HIP5_CONFIG_INIT_MAGIC_NUM;

	KUNIT_EXPECT_EQ(test, 0, hip5_read_index(&sdev->hip, 2, 0));
}

static void test_hip5_skb_to_mbulk(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb;

	skb = fapi_alloc(ma_unitdata_req, MA_UNITDATA_REQ, 0, 100);
	skb_put(skb, 10);

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_priv->unidat_req_headroom = 10;
	sdev->hip.hip_priv->unidat_req_tailroom = 10;
	sdev->hip.hip_priv->version = 5;
	sdev->hip.hip_priv->scbrd_base = sdev->hip.hip_control->scoreboard;
	sdev->hip.hip_control->init.magic_number = 0x0;

	KUNIT_EXPECT_PTR_NE(test, NULL, hip5_skb_to_mbulk(&sdev->hip.hip_priv, skb, MBULK_POOL_ID_DATA, 0x0));
	kfree_skb(skb);
}

static void test_hip5_opt_log_wakeup_signal(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb = fapi_alloc(ma_unitdata_req, MA_UNITDATA_REQ, 0, 1);

	hip5_opt_log_wakeup_signal(sdev, skb, false);
	kfree_skb(skb);
}

static void test_hip5_opt_hip_signal_to_skb(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct scsc_service *service;
	struct hip5_hip_control *ctrl;
	struct hip_priv *hip_priv;
	struct hip5_hip_entry *hip5_entry;
	struct hip5_opt_bulk_desc *bulk_desc;
	struct sk_buff_head *skb_list;
	void *bulk_data;
	void *sig_ptr;
	u8 padding;
	u16 idx_r, idx_w;
	struct hip5_opt_hip_signal *hip_signal;
	scsc_mifram_ref to_free[MBULK_MAX_CHAIN + 1] = { 0 };

	idx_r = 1;
	idx_w = 1;
	bulk_data = kunit_kzalloc(test, sizeof(u8) * 1000, GFP_KERNEL);
	skb_list = kunit_kzalloc(test, sizeof(struct sk_buff_head), GFP_KERNEL);
	__skb_queue_head_init(skb_list);
	alloc_common_mem_for_test(test, sdev);
	ctrl = sdev->hip.hip_control;
	hip_priv = sdev->hip.hip_priv;
	service = sdev->service;

	hip5_entry = (struct hip5_hip_entry *)(&ctrl->q_tlv[2].array[idx_r]);
	hip5_entry->data_len = fapi_sig_size(ma_unitdata_req) << 8;
	hip5_entry->bd[0].buf_addr = (MA_UNITDATA_REQ | 0x4444 << 16);
	hip5_entry->bdx[1].buf_sz = 0x1111;
	hip5_entry->bdx[1].buf_addr = bulk_data;
	hip_signal = (struct hip5_opt_hip_signal *)(hip5_entry);

	sig_ptr = (void *)hip_signal + 4;
	padding = (8 - ((hip_signal->sig_len + 4)  % 8)) & 0x7;
	bulk_desc = (struct hip5_opt_bulk_desc *)(sig_ptr + hip_signal->sig_len + padding);

	hip5_entry->num_bd = 0;
	hip5_entry->bdx[1].len = HIP5_OPT_BD_CHAIN_START + 1 << 8;

	KUNIT_EXPECT_EQ(test, 0, hip5_opt_hip_signal_to_skb(sdev, service, hip_priv, ctrl, HIP5_MIF_Q_TH_DAT0,
							    to_free, &idx_r, &idx_w, skb_list));

	hip5_entry->num_bd = 2;

	KUNIT_EXPECT_EQ(test, 0, hip5_opt_hip_signal_to_skb(sdev, service, hip_priv, ctrl, HIP5_MIF_Q_TH_DAT0,
							    to_free, &idx_r, &idx_w, skb_list));

	hip5_entry->bdx[1].len = HIP5_OPT_BD_CHAIN_START + 2 << 8;

	KUNIT_EXPECT_EQ(test, 0, hip5_opt_hip_signal_to_skb(sdev, service, hip_priv, ctrl, HIP5_MIF_Q_TH_DAT0,
							    to_free, &idx_r, &idx_w, skb_list));

	hip5_entry->num_bd = 4;

	KUNIT_EXPECT_NE(test, 0, hip5_opt_hip_signal_to_skb(sdev, service, hip_priv, ctrl, HIP5_MIF_Q_TH_DAT0,
							    to_free, &idx_r, &idx_w, skb_list));
}

static void test_hip5_bd_to_skb(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct scsc_service *service;
	struct hip5_hip_control *ctrl;
	struct hip_priv *hip_priv;
	struct hip5_hip_entry *hip5_entry;
	struct hip5_hip_entry *hip5_chained_entry;
	struct sk_buff *skb;
	u8 padding;
	u16 idx_r, idx_w;
	struct hip5_opt_hip_signal *hip_signal;
	scsc_mifram_ref to_free[MBULK_MAX_CHAIN + 1] = { 0 };

	idx_r = 1;
	idx_w = 1;
	alloc_common_mem_for_test(test, sdev);
	ctrl = sdev->hip.hip_control;
	hip_priv = sdev->hip.hip_priv;
	service = sdev->service;

	hip5_entry = (struct hip5_hip_entry *)(&ctrl->q_tlv[2].array[idx_r]);
	hip5_entry->sig_len = fapi_sig_size(ma_unitdata_req);
	hip5_entry->num_bd = 1;
	hip5_entry->data_len = 30;
	hip5_entry->bd[0].buf_addr = kunit_kzalloc(test, sizeof(struct mbulk), GFP_KERNEL);
	hip5_entry->bd[0].len = 0;
	hip5_entry->bd[0].offset = 0;
	hip5_entry->flag = HIP5_HIP_ENTRY_CHAIN_HEAD;

	/* chained HIP entries */
	hip5_chained_entry = (struct hip5_hip_entry *)(&ctrl->q_tlv[2].array[idx_r + 1]);
	hip5_chained_entry->sig_len = fapi_sig_size(ma_unitdata_req);
	hip5_chained_entry->num_bd = 1;
	hip5_chained_entry->data_len = 30;
	hip5_chained_entry->flag = HIP5_HIP_ENTRY_CHAIN_END;
	hip5_chained_entry->bd[0].len = 0;

	skb = hip5_bd_to_skb(sdev, service, hip_priv, hip5_entry, to_free, &idx_r, &idx_w);
	KUNIT_EXPECT_NE(test, NULL, skb);
	kfree_skb(skb);
}

static void test_hip5_mbulk_to_skb(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct scsc_service *service;
	struct hip_priv *hip_priv;
	struct mbulk *mem_ptr = kunit_kzalloc(test, sizeof(struct mbulk) + 100, GFP_KERNEL);
	struct mbulk *mem_chained = kunit_kzalloc(test, sizeof(struct mbulk) + 100, GFP_KERNEL);
	struct sk_buff *skb;
	scsc_mifram_ref to_free[MBULK_MAX_CHAIN + 1] = { 0 };

	alloc_common_mem_for_test(test, sdev);
	hip_priv = sdev->hip.hip_priv;
	service = sdev->service;
	mem_ptr->sig_bufsz = 10;
	mem_ptr->len = 10;
	mem_ptr->flag = MBULK_F_CHAIN_HEAD | MBULK_F_CHAIN | MBULK_F_READONLY;
	mem_ptr->chain_next_offset = (scsc_mifram_ref)mem_chained;
	KUNIT_EXPECT_EQ(test, NULL, hip5_mbulk_to_skb(service, hip_priv, mem_ptr, to_free, false));

	mem_ptr->flag = MBULK_F_CHAIN_HEAD | MBULK_F_CHAIN | MBULK_F_SIG;
	skb = hip5_mbulk_to_skb(service, hip_priv, mem_ptr, to_free, false);
	KUNIT_EXPECT_NE(test, NULL, skb);
	kfree_skb(skb);
}

static void test_hip5_opt_hip_signal_add(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb;
	struct mbulk *mem_ptr = kunit_kzalloc(test, sizeof(struct mbulk) + 100, GFP_KERNEL);

	skb = fapi_alloc(ma_unitdata_req, MA_UNITDATA_REQ, 0, 1);

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_priv->version = 5;
	sdev->hip.hip_priv->scbrd_base = sdev->hip.hip_control->scoreboard;
	sdev->hip.hip_control->init.magic_number = HIP5_CONFIG_INIT_MAGIC_NUM;

	KUNIT_EXPECT_EQ(test, 0,
			hip5_opt_hip_signal_add(&sdev->hip, HIP5_MIF_Q_FH_DAT0, 0, sdev->service, mem_ptr, skb));
	kfree_skb(skb);
}

static void test_hip5_q_add_bd_entry(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct mbulk *mem_ptr = kunit_kzalloc(test, sizeof(struct mbulk) + 100, GFP_KERNEL);
	struct fapi_signal *fapi_sig = kunit_kzalloc(test, sizeof(struct fapi_signal), GFP_KERNEL);

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_priv->scbrd_base = sdev->hip.hip_control->scoreboard;
	sdev->hip.hip_control->init.magic_number = HIP5_CONFIG_INIT_MAGIC_NUM;
	KUNIT_EXPECT_EQ(test, 0,
			hip5_q_add_bd_entry(&sdev->hip, HIP5_MIF_Q_FH_DAT0, 0, sdev->service, mem_ptr, fapi_sig));
}

static void test_hip5_wq_ctrl(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct work_priv *work_priv = kunit_kzalloc(test, sizeof(struct work_priv), GFP_KERNEL);
	void *scbrd_base;

	alloc_common_mem_for_test(test, sdev);
	work_priv->bh_s = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	work_priv->bh_s->sdev = sdev;
	work_priv->bh_s->hip_priv = sdev->hip.hip_priv;

	sdev->hip.hip_control->init.magic_number = HIP5_CONFIG_INIT_MAGIC_NUM;
	sdev->hip.hip_priv->scbrd_base = sdev->hip.hip_control->scoreboard;
	sdev->hip.hip_priv->wake_lock_ctrl.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	sdev->hip.hip_priv->version = 5;
	wakeup_source_add(sdev->hip.hip_priv->wake_lock_ctrl.ws);

	scbrd_base = sdev->hip.hip_priv->scbrd_base;
	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_TH_CTRL][1])) = 0;
	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_TH_CTRL][0])) = 1;

	hip5_wq_ctrl(&work_priv->bh);

	sdev->hip.hip_priv->version = 4;
	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_TH_CTRL][1])) = 0;
	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_TH_CTRL][0])) = 1;

	hip5_wq_ctrl(&work_priv->bh);
}

static void test_hip5_irq_handler_stub(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);
	atomic_set(&sdev->hip.hip_priv->closing, 0);
	hip5_irq_handler_stub(0, (void *)&sdev->hip);

	atomic_set(&sdev->hip.hip_priv->closing, 1);
	hip5_irq_handler_stub(0, (void *)&sdev->hip);
}

static void test_hip5_irq_handler_ctrl(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_priv->wake_lock_ctrl.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	wakeup_source_add(sdev->hip.hip_priv->wake_lock_ctrl.ws);

	atomic_set(&sdev->hip.hip_priv->closing, 0);
	hip5_irq_handler_ctrl(0, (void *)&sdev->hip);

	atomic_set(&sdev->hip.hip_priv->closing, 1);
	hip5_irq_handler_ctrl(0, (void *)&sdev->hip);
}

static void test_hip5_q_add_signal(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_priv->scbrd_base = sdev->hip.hip_control->scoreboard;
	sdev->hip.hip_control->init.magic_number = HIP5_CONFIG_INIT_MAGIC_NUM;

	KUNIT_EXPECT_EQ(test, 0, hip5_q_add_signal(&sdev->hip, HIP5_MIF_Q_FH_DAT0, 0, sdev->service));
}

static void test_slsi_hip_sched_wq_ctrl(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_control->init.magic_number = 0x0;
	sdev->hip.hip_priv->wake_lock_ctrl.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	wakeup_source_add(sdev->hip.hip_priv->wake_lock_ctrl.ws);
	slsi_wake_lock_init(NULL, &sdev->hip.hip_priv->wake_lock_ctrl.ws, "hip_wake_lock_ctrl");

	slsi_hip_sched_wq_ctrl(&sdev->hip);
}

#if defined(CONFIG_SCSC_PCIE_CHIP)
static void test_hip5_fb_mx_claim_complete_cb(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u8 *data = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, hip5_fb_mx_claim_complete_cb(sdev->service, data));
}

static void test_hip5_napi_mx_claim_complete_cb(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u8 *data = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, hip5_napi_mx_claim_complete_cb(sdev->service, data));
}
#endif

static void test_hip5_tl_fb(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct work_priv *work_priv = kunit_kzalloc(test, sizeof(struct work_priv), GFP_KERNEL);
	void *scbrd_base;

	alloc_common_mem_for_test(test, sdev);
	work_priv->bh_s = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	work_priv->bh_s->sdev = sdev;
	work_priv->bh_s->hip_priv = sdev->hip.hip_priv;
	sdev->hip.hip_control->init.magic_number = HIP5_CONFIG_INIT_MAGIC_NUM;
	sdev->hip.hip_priv->scbrd_base = sdev->hip.hip_control->scoreboard;
	sdev->hip.hip_priv->wake_lock_tx.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	sdev->hip.hip_priv->version = 5;
	wakeup_source_add(sdev->hip.hip_priv->wake_lock_tx.ws);
	scbrd_base = sdev->hip.hip_priv->scbrd_base;

	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_FH_RFBC][1])) = 2049;
	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_FH_RFBC][0])) = 1;
	hip5_tl_fb(&sdev->hip);

	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_FH_RFBC][1])) = 0;
	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_FH_RFBC][0])) = 1;
	hip5_tl_fb(&sdev->hip);
}

static void test_hip5_irq_handler_fb(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_priv->wake_lock_tx.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	wakeup_source_add(sdev->hip.hip_priv->wake_lock_tx.ws);

	atomic_set(&sdev->hip.hip_priv->closing, 0);
	hip5_irq_handler_fb(0, &sdev->hip);

	atomic_set(&sdev->hip.hip_priv->closing, 1);
	hip5_irq_handler_fb(0, &sdev->hip);
}

static void test_hip5_napi_complete(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct napi_struct *napi = kunit_kzalloc(test, sizeof(struct napi_struct), GFP_KERNEL);

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_priv->wake_lock_data.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	wakeup_source_add(sdev->hip.hip_priv->wake_lock_data.ws);

	atomic_set(&sdev->hip.hip_priv->closing, 0);
	hip5_napi_complete(sdev, napi, &sdev->hip);

	atomic_set(&sdev->hip.hip_priv->closing, 1);
	hip5_napi_complete(sdev, napi, &sdev->hip);
}

static void test_hip5_napi_poll(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct napi_cpu_info *cpu_info = kunit_kzalloc(test, sizeof(struct napi_cpu_info), GFP_KERNEL);
	struct napi_priv *napi_priv = kunit_kzalloc(test, sizeof(struct napi_priv), GFP_KERNEL);
	void *scbrd_base;

	alloc_common_mem_for_test(test, sdev);
	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STARTED);
	napi_priv->bh = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	cpu_info->priv = napi_priv;
	napi_priv->bh->hip_priv = sdev->hip.hip_priv;
	sdev->hip.hip_control->init.magic_number = HIP5_CONFIG_INIT_MAGIC_NUM;
	sdev->hip.hip_priv->scbrd_base = sdev->hip.hip_control->scoreboard;
	sdev->hip.hip_priv->wake_lock_data.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	wakeup_source_add(sdev->hip.hip_priv->wake_lock_data.ws);
	scbrd_base = sdev->hip.hip_priv->scbrd_base;

	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_TH_DAT0][1])) = 0;
	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_TH_DAT0][0])) = 2049;
	KUNIT_EXPECT_EQ(test, 0, hip5_napi_poll(&cpu_info->napi_instance, 60));

	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_TH_DAT0][1])) = 1;
	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_TH_DAT0][0])) = 0;
	KUNIT_EXPECT_NE(test, 0, hip5_napi_poll(&cpu_info->napi_instance, 60));

	sdev->hip.hip_priv->version = 5;
	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_TH_DAT0][1])) = 0;
	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_TH_DAT0][0])) = 1;
	KUNIT_EXPECT_EQ(test, 0, hip5_napi_poll(&cpu_info->napi_instance, 60));

	sdev->hip.hip_priv->version = 4;
	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_TH_DAT0][1])) = 0;
	*((u16 *)(scbrd_base + q_idx_layout[HIP5_MIF_Q_TH_DAT0][0])) = 1;
	KUNIT_EXPECT_NE(test, 0, hip5_napi_poll(&cpu_info->napi_instance, 60));
}

static void test_hip5_irq_handler_dat(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_priv->wake_lock_data.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	sdev->hip.hip_priv->bh_dat = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);

	wakeup_source_add(sdev->hip.hip_priv->wake_lock_data.ws);
	atomic_set(&sdev->hip.hip_priv->closing, 0);
	hip5_irq_handler_dat(0, &sdev->hip);

	atomic_set(&sdev->hip.hip_priv->closing, 1);
	hip5_irq_handler_dat(0, &sdev->hip);
}

static void test_hip5_pm_qos_work(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct work_struct *data;

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_priv->hip = &sdev->hip;
	data = &sdev->hip.hip_priv->pm_qos_work;
	hip5_pm_qos_work(data);
}

static void test_hip5_traffic_monitor_cb(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);

	hip5_traffic_monitor_cb(&sdev->hip, TRAFFIC_MON_CLIENT_STATE_HIGH, 0, 0);
	hip5_traffic_monitor_cb(&sdev->hip, TRAFFIC_MON_CLIENT_STATE_MID, 0, 0);
	hip5_traffic_monitor_cb(&sdev->hip, TRAFFIC_MON_CLIENT_STATE_LOW, 0, 0);
}

#if IS_ENABLED(CONFIG_SCSC_LOGRING)
static void test_hip5_traffic_monitor_logring_cb(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);

	hip5_traffic_monitor_logring_cb(&sdev->hip, TRAFFIC_MON_CLIENT_STATE_HIGH, 0, 0);
	hip5_traffic_monitor_logring_cb(&sdev->hip, TRAFFIC_MON_CLIENT_STATE_LOW, 0, 0);
}
#endif

static void test_slsi_hip_transmit_frame(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb;
	struct hip5_hip_q *q;

	scsc_mifram_ref phy_m = 0;

	skb = fapi_alloc(ma_unitdata_req, MA_UNITDATA_REQ, 0, 200);
	skb_put(skb, sizeof(struct fapi_signal) + 30);

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_priv->unidat_req_headroom = 10;
	sdev->hip.hip_priv->unidat_req_tailroom = 10;
	sdev->hip.hip_priv->version = 5;
	sdev->hip.hip_priv->scbrd_base = sdev->hip.hip_control->scoreboard;

	sdev->hip.hip_control->init.magic_number = 0x0;
	sdev->hip.hip_priv->wake_lock_tx.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	wakeup_source_add(sdev->hip.hip_priv->wake_lock_tx.ws);

	KUNIT_EXPECT_EQ(test, 0, slsi_hip_transmit_frame(&sdev->hip, skb, false, 0, 1, 1));
	KUNIT_EXPECT_NE(test, 0, slsi_hip_transmit_frame(&sdev->hip, skb, true, 0, 1, 1));

	sdev->hip.hip_priv->version = 4;
	KUNIT_EXPECT_NE(test, 0, slsi_hip_transmit_frame(&sdev->hip, skb, true, 0, 1, 1));
	consume_skb(skb);

	skb = fapi_alloc(ma_unitdata_req, MA_UNITDATA_REQ, 0, 200);
	skb_put(skb, sizeof(struct fapi_signal) + 30);
	KUNIT_EXPECT_NE(test, 0, slsi_hip_transmit_frame(&sdev->hip, skb, false, 0, 1, 1));
	consume_skb(skb);

	skb = fapi_alloc(ma_unitdata_req, MA_UNITDATA_REQ, 0, 200);
	skb_put(skb, sizeof(struct fapi_signal) + 30);
	KUNIT_EXPECT_NE(test, 0, slsi_hip_transmit_frame(&sdev->hip, skb, true, 0, 1, 1));
	consume_skb(skb);
}

static void test_hip5_opt_aggr_tx_frame(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb;
	struct sk_buff *sk;

	skb = fapi_alloc(ma_unitdata_req, MA_UNITDATA_REQ, 0, 200);
	skb_put(skb, 100);
	alloc_common_mem_for_test(test, sdev);

	sdev->hip.hip_priv->version = 5;
	sdev->hip.hip_priv->scbrd_base = sdev->hip.hip_control->scoreboard;
	sdev->hip.hip_control->init.magic_number = 0x0;
	sdev->hip.hip_priv->wake_lock_tx.ws = kunit_kzalloc(test, sizeof(struct wakeup_source), GFP_KERNEL);
	wakeup_source_add(sdev->hip.hip_priv->wake_lock_tx.ws);

	sdev->hip.hip_control->init.magic_number = HIP5_CONFIG_INIT_MAGIC_NUM;
	skb_queue_head_init(&sdev->hip.hip_priv->hip5_opt_tx_q[0].tx_q);
	__skb_queue_head(&sdev->hip.hip_priv->hip5_opt_tx_q[0].tx_q, skb);
	hip5_opt_aggr_tx_frame(sdev->service, &sdev->hip);
}

static void test_slsi_hip_from_host_intr_set(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_priv->version = 4;

	slsi_hip_from_host_intr_set(sdev->service, &sdev->hip);
}

static void test_slsi_hip_setup(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);
	sdev->hip.hip_priv->version = 4;
	sdev->hip.hip_control->init.conf_hip5_ver = 4;

	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STARTED);
	KUNIT_EXPECT_EQ(test, 0, slsi_hip_setup(&sdev->hip));

	sdev->hip.hip_control->init.conf_hip5_ver = 5;
	KUNIT_EXPECT_EQ(test, 0, slsi_hip_setup(&sdev->hip));
}

static void test_slsi_hip_suspend(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);
	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STARTED);

	slsi_hip_suspend(&sdev->hip);
}

static void test_slsi_hip_resume(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);
	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STARTED);

	slsi_hip_resume(&sdev->hip);
}

static void test_slsi_hip_freeze(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	alloc_common_mem_for_test(test, sdev);

	sdev->hip.hip_priv->hip_workq = alloc_workqueue("hip5", 0, 0);
	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STARTED);
	slsi_hip_freeze(&sdev->hip);
}

static void test_slsi_hip_deinit(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	struct work_struct *work = kunit_kzalloc(test, sizeof(struct work_struct), GFP_KERNEL);
	struct workqueue_struct *hip5_wq;

	alloc_common_mem_for_test(test, sdev);
	sdev->netdev[SLSI_NET_INDEX_WLAN] = ndev;
	sdev->device_wq = work;

	sdev->hip.hip_priv->hip_workq = alloc_workqueue("hip5", 0, 0);
	hip5_wq = sdev->hip.hip_priv->hip_workq;
	INIT_LIST_HEAD(&sdev->traffic_mon_clients.client_list);
	slsi_hip_deinit(&sdev->hip);
}

static int hip5_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void hip5_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case hip5_test_cases[] = {
	KUNIT_CASE(test_slsi_hip_init),
	KUNIT_CASE(test_slsi_hip_init_control_table),
	KUNIT_CASE(test_hip5_opt_aggr_check),
	KUNIT_CASE(test_hip5_opt_tx_frame),
	KUNIT_CASE(test_hip5_read_index),
	KUNIT_CASE(test_hip5_dump_dbg),
	KUNIT_CASE(test_hip5_skb_to_mbulk),
	KUNIT_CASE(test_hip5_opt_log_wakeup_signal),
	KUNIT_CASE(test_hip5_opt_hip_signal_to_skb),
	KUNIT_CASE(test_hip5_bd_to_skb),
	KUNIT_CASE(test_hip5_mbulk_to_skb),
	KUNIT_CASE(test_hip5_opt_hip_signal_add),
	KUNIT_CASE(test_hip5_q_add_bd_entry),
	KUNIT_CASE(test_hip5_wq_ctrl),
	KUNIT_CASE(test_hip5_irq_handler_stub),
	KUNIT_CASE(test_hip5_irq_handler_ctrl),
	KUNIT_CASE(test_hip5_q_add_signal),
	KUNIT_CASE(test_slsi_hip_sched_wq_ctrl),
	KUNIT_CASE(test_hip5_tl_fb),
	KUNIT_CASE(test_hip5_irq_handler_fb),
	KUNIT_CASE(test_hip5_napi_complete),
#if defined(CONFIG_SCSC_PCIE_CHIP)
	KUNIT_CASE(test_hip5_fb_mx_claim_complete_cb),
	KUNIT_CASE(test_hip5_napi_mx_claim_complete_cb),
#endif
	KUNIT_CASE(test_hip5_napi_poll),
	KUNIT_CASE(test_hip5_irq_handler_dat),
	KUNIT_CASE(test_hip5_pm_qos_work),
	KUNIT_CASE(test_hip5_traffic_monitor_cb),
	KUNIT_CASE(test_hip5_traffic_monitor_logring_cb),
	KUNIT_CASE(test_slsi_hip_free_control_slots_count),
	KUNIT_CASE(test_slsi_hip_transmit_frame),
	KUNIT_CASE(test_hip5_opt_aggr_tx_frame),
	KUNIT_CASE(test_slsi_hip_from_host_intr_set),
	KUNIT_CASE(test_slsi_hip_setup),
	KUNIT_CASE(test_slsi_hip_suspend),
	KUNIT_CASE(test_slsi_hip_resume),
	KUNIT_CASE(test_slsi_hip_freeze),
	KUNIT_CASE(test_slsi_hip_deinit),
	{}
};

static struct kunit_suite hip5_test_suite[] = {
	{
		.name = "kunit-hip5-test",
		.test_cases = hip5_test_cases,
		.init = hip5_test_init,
		.exit = hip5_test_exit,
	}
};

kunit_test_suites(hip5_test_suite);
