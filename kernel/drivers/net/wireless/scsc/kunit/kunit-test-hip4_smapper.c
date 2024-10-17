// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-kernel.h"
#include "kunit-mock-misc.h"
#include "../hip4_smapper.c"

static void test_hip4_smapper_alloc_bank(struct kunit *test)
{
	struct slsi_dev *sdev;
	struct hip_priv *priv;

	sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, 0, hip4_smapper_alloc_bank(sdev, priv, 0, 100, true));
}

static void test_hip4_smapper_allocate_skb_buffer_entry(struct kunit *test)
{
	struct slsi_dev *sdev;
	struct hip4_smapper_bank *bank;

	sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	bank = kunit_kzalloc(test, sizeof(struct hip4_smapper_bank), GFP_KERNEL);
	bank->skbuff_dma = kunit_kzalloc(test, sizeof(uint64_t) * 10, GFP_KERNEL);
	bank->skbuff = kunit_kzalloc(test, sizeof(struct sk_buff*), GFP_KERNEL);
	bank->skbuff[0] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	bank->align = 0;

	KUNIT_EXPECT_NE(test, 0, hip4_smapper_allocate_skb_buffer_entry(sdev, bank, 0));
}

static void test_hip4_smapper_allocate_skb_buffers(struct kunit *test)
{
	struct slsi_dev *sdev;
	struct hip4_smapper_bank *bank;

	sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	bank = kunit_kzalloc(test, sizeof(struct hip4_smapper_bank), GFP_KERNEL);
	bank->skbuff_dma = kunit_kzalloc(test, sizeof(uint64_t) * 10, GFP_KERNEL);
	bank->skbuff = kunit_kzalloc(test, sizeof(struct sk_buff*), GFP_KERNEL);
	bank->align = 0;
	bank->entries = 1;

	KUNIT_EXPECT_NE(test, 0, hip4_smapper_allocate_skb_buffers(sdev, bank));
}

static void test_hip4_smapper_free_skb_buffers(struct kunit *test)
{
	struct slsi_dev *sdev;
	struct hip4_smapper_bank *bank;

	sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	bank = kunit_kzalloc(test, sizeof(struct hip4_smapper_bank), GFP_KERNEL);
	bank->skbuff_dma = kunit_kzalloc(test, sizeof(uint64_t) * 10, GFP_KERNEL);
	bank->skbuff = kunit_kzalloc(test, sizeof(struct sk_buff*), GFP_KERNEL);
	bank->entry_size = 0;
	bank->align = 0;
	bank->entries = 1;
	bank->skbuff[0] = alloc_skb(bank->entry_size, GFP_ATOMIC);

	KUNIT_EXPECT_EQ(test, 0, hip4_smapper_free_skb_buffers(sdev, bank));
}

static void test_hip4_smapper_program(struct kunit *test)
{
	struct slsi_dev *sdev;
	struct hip4_smapper_bank *bank;

	sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	bank = kunit_kzalloc(test, sizeof(struct hip4_smapper_bank), GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, 0, hip4_smapper_program(sdev, bank));
}

static void test_hip4_smapper_refill_isr(struct kunit *test)
{
	struct slsi_hip *data;
	struct hip_priv *priv;
	struct hip4_smapper_bank *bank;
	struct hip4_smapper_control *control;
	u32 *tc1 = 0x0;
	u32 *tc2 = 0xff;

	control = kunit_kzalloc(test, sizeof(struct hip4_smapper_control), GFP_KERNEL);
	priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	data = kunit_kzalloc(test, sizeof(struct slsi_hip), GFP_KERNEL);
	bank = kunit_kzalloc(test, sizeof(struct hip4_smapper_bank), GFP_KERNEL);

	control->mbox_ptr = &tc1;
	priv->pm_qos_state = 1;
	atomic_set(&priv->in_suspend, 0);
	data->hip_priv = priv;

	memcpy(&(data->hip_priv->smapper_control), control, sizeof(struct hip4_smapper_control));
	memcpy(&(data->hip_priv->smapper_banks), bank, sizeof(struct hip4_smapper_bank));

	bank = &data->hip_priv->smapper_banks[0];
	bank->in_use = true;
	bank->bank = 1;

	data->hip_priv->smapper_control.mbox_ptr = &tc1;

	hip4_smapper_refill_isr(1, data);

	data->hip_priv->smapper_control.mbox_ptr = &tc2;

	hip4_smapper_refill_isr(1, data);
}

static void test_hip4_smapper_consume_entry(struct kunit *test)
{
	struct slsi_dev *sdev;
	struct hip_priv *priv;
	struct sk_buff *skb_fapi;
	struct hip4_smapper_descriptor *desc;
	struct slsi_skb_cb *cb;

	sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	priv->smapper_banks[0].skbuff_dma = kunit_kzalloc(test, sizeof(uint64_t) * 10, GFP_KERNEL);
	priv->smapper_banks[0].skbuff = kunit_kzalloc(test, sizeof(struct sk_buff*), GFP_KERNEL);
	priv->smapper_banks[0].skbuff[0] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	skb_fapi = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb_fapi->data = kunit_kzalloc(test, sizeof(char)*100, GFP_KERNEL);
	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	memset(skb_fapi->data, 0, sizeof(u16));

	desc = (struct hip4_smapper_descriptor *)skb_fapi->data;
	desc->bank_num = 0;
	desc->entry_num = 0;
	desc->entry_size = 0;
	KUNIT_EXPECT_EQ(test, 0, hip4_smapper_consume_entry(sdev, &(sdev->hip), skb_fapi));

	cb = slsi_skb_cb_get(skb_fapi);
	if (cb)
		kfree_skb(cb->skb_addr);
}

static void test_hip4_smapper_get_skb_data(struct kunit *test)
{
	struct slsi_dev *sdev;
	struct hip_priv *priv;
	struct sk_buff *skb;
	struct slsi_skb_cb *cb;

	sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	cb = slsi_skb_cb_get(skb);

	hip4_smapper_get_skb_data(sdev, &sdev->hip, skb);

	cb->skb_addr = (void *)kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	hip4_smapper_get_skb_data(sdev, &sdev->hip, skb);
}

static void test_hip4_smapper_get_skb(struct kunit *test)
{
	struct slsi_dev *sdev;
	struct hip_priv *priv;
	struct sk_buff *skb_fapi;
	struct slsi_skb_cb *cb;

	sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	skb_fapi = alloc_skb(sizeof(struct sk_buff), GFP_ATOMIC);

	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	cb = slsi_skb_cb_get(skb_fapi);
	cb->skb_addr = (void *)kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	KUNIT_EXPECT_PTR_EQ(test, cb->skb_addr, hip4_smapper_get_skb(sdev, &(sdev->hip), skb_fapi));
}

static void test_hip4_smapper_free_mapped_skb(struct kunit *test)
{
	struct sk_buff *skb;
	struct slsi_skb_cb *cb;

	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	cb = slsi_skb_cb_get(skb);
	cb->free_ma_unitdat = 0;
	cb->skb_addr = alloc_skb(sizeof(struct sk_buff), GFP_ATOMIC);
	hip4_smapper_free_mapped_skb(skb);
}

static void test_hip4_smapper_init(struct kunit *test)
{
	struct slsi_dev *sdev;
	struct hip_priv *priv;

	sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->hip.hip_control = kunit_kzalloc(test, sizeof(struct hip4_hip_control), GFP_KERNEL);
	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);

	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	priv->smapper_control.mbox_scb = 1;
	priv->smapper_control.th_req = 0;
	priv->smapper_control.fh_ind = 0;

	KUNIT_EXPECT_EQ(test, 0, hip4_smapper_init(sdev, &(sdev->hip)));

	kfree(priv->smapper_control.mbox_ptr);
}

static void test_hip4_smapper_deinit(struct kunit *test)
{
	struct slsi_dev *sdev;
	struct hip_priv *priv;
	struct hip4_smapper_control *control;
	u8 i;

	sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	for (i = RX_0; i < END_RX_BANKS; i++) {
		priv->smapper_banks[i].skbuff_dma = kzalloc(sizeof(dma_addr_t), GFP_KERNEL);
		priv->smapper_banks[i].entry_size = i;
		priv->smapper_banks[i].skbuff = kzalloc(priv->smapper_banks[i].entry_size, GFP_KERNEL);
		priv->smapper_banks[i].align = 0;
		priv->smapper_banks[i].entries = 0;
	}
	control = kunit_kzalloc(test, sizeof(struct hip4_smapper_control), GFP_KERNEL);
	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	memcpy(&(priv->smapper_control), control, sizeof(struct hip4_smapper_control));
	hip4_smapper_deinit(sdev, &(sdev->hip));
}

/* Test features*/
static int hip4_smapper_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void hip4_smapper_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case hip4_smapper_test_cases[] = {
	/* hip4_smapper.c */
	KUNIT_CASE(test_hip4_smapper_alloc_bank),
	KUNIT_CASE(test_hip4_smapper_allocate_skb_buffer_entry),
	KUNIT_CASE(test_hip4_smapper_allocate_skb_buffers),
	KUNIT_CASE(test_hip4_smapper_free_skb_buffers),
	KUNIT_CASE(test_hip4_smapper_program),
	KUNIT_CASE(test_hip4_smapper_refill_isr),
	KUNIT_CASE(test_hip4_smapper_consume_entry),
	KUNIT_CASE(test_hip4_smapper_get_skb_data),
	KUNIT_CASE(test_hip4_smapper_get_skb),
	KUNIT_CASE(test_hip4_smapper_free_mapped_skb),
	KUNIT_CASE(test_hip4_smapper_init),
	KUNIT_CASE(test_hip4_smapper_deinit),
	{}
};

static struct kunit_suite hip4_smapper_test_suite[] = {
	{
		.name = "kunit-hip4-smapper-test",
		.test_cases = hip4_smapper_test_cases,
		.init = hip4_smapper_test_init,
		.exit = hip4_smapper_test_exit,
	}
};

kunit_test_suites(hip4_smapper_test_suite);
