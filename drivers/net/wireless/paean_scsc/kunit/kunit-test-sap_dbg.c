// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-dev.h"
#include "kunit-mock-hip.h"
#include "kunit-mock-rx.h"
#include "../sap_dbg.c"

static void __sap_dbg_test_cb(struct work_struct *work)
{
	printk("%s called.\n", __func__);
}

/* unit test function definition */
static void test_sap_dbg_version_supported(struct kunit *test)
{
	u16 version = FAPI_DEBUG_SAP_VERSION;
	KUNIT_EXPECT_EQ(test, 0, sap_dbg_version_supported(version));

	version = FAPI_TEST_SAP_VERSION;
	KUNIT_EXPECT_EQ(test, -EINVAL, sap_dbg_version_supported(version));
}

static void test_slsi_rx_debug(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	skb->data = kunit_kzalloc(test, fapi_sig_size(debug_fault_ind), GFP_KERNEL);
	fapi_set_u16(skb, id, DEBUG_FAULT_IND);
	fapi_set_u16(skb, u.debug_fault_ind.cpu, 0x8000);
	fapi_set_u16(skb, u.debug_fault_ind.faultid, 0x0100);
	fapi_set_u32(skb, u.debug_fault_ind.arg, 0x0400);
	fapi_set_u16(skb, u.debug_fault_ind.count, 8);
	fapi_set_u32(skb, u.debug_fault_ind.timestamp, ktime_get_real_ns());
	slsi_rx_debug(sdev, NULL, skb);

	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, fapi_sig_size(debug_word12_ind), GFP_KERNEL);
	fapi_set_u16(skb, id, DEBUG_WORD12IND);
	fapi_set_u16(skb, u.debug_word12_ind.module_id, 0x0100);
	fapi_set_u16(skb, u.debug_word12_ind.module_sub_id, 0x0110);
	fapi_set_u32(skb, u.debug_word12_ind.timestamp, ktime_get_real_ns());
	fapi_set_u16(skb, u.debug_word12_ind.debug_words[0], 0x0001);
	fapi_set_u16(skb, u.debug_word12_ind.debug_words[1], 0x0002);
	fapi_set_u16(skb, u.debug_word12_ind.debug_words[2], 0x0003);
	fapi_set_u16(skb, u.debug_word12_ind.debug_words[3], 0x0004);
	fapi_set_u16(skb, u.debug_word12_ind.debug_words[4], 0x0005);
	fapi_set_u16(skb, u.debug_word12_ind.debug_words[5], 0x0006);
	fapi_set_u16(skb, u.debug_word12_ind.debug_words[6], 0x0007);
	fapi_set_u16(skb, u.debug_word12_ind.debug_words[7], 0x0008);
	fapi_set_u16(skb, u.debug_word12_ind.debug_words[8], 0x0009);
	fapi_set_u16(skb, u.debug_word12_ind.debug_words[9], 0x0010);
	fapi_set_u16(skb, u.debug_word12_ind.debug_words[10], 0x0011);
	fapi_set_u16(skb, u.debug_word12_ind.debug_words[11], 0x0012);
	slsi_rx_debug(sdev, NULL, skb);

	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, fapi_sig_size(debug_words_ind), GFP_KERNEL);
	fapi_set_u16(skb, id, DEBUG_WORDS_IND);
	slsi_rx_debug(sdev, NULL, skb);
}

static void test_slsi_rx_dbg_sap(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	skb->data = kunit_kzalloc(test, fapi_sig_size(debug_fault_ind), GFP_KERNEL);
	fapi_set_u16(skb, id, DEBUG_FAULT_IND);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_dbg_sap(sdev, skb));

	fapi_set_u16(skb, id, DEBUG_PKT_SINK_REPORT_IND);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_dbg_sap(sdev, skb));

	fapi_set_u16(skb, u.debug_pkt_sink_report_ind.vif, SLSI_NET_INDEX_WLAN);
	sdev->netdev[SLSI_NET_INDEX_WLAN] = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_dbg_sap(sdev, skb));

	fapi_set_u16(skb, id, DEBUG_PKT_GEN_REPORT_IND);
	fapi_set_u16(skb, u.debug_pkt_gen_report_ind.vif, SLSI_NET_INDEX_WLAN);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_dbg_sap(sdev, skb));

	kunit_kfree(test, sdev->netdev[SLSI_NET_INDEX_WLAN]);
	sdev->netdev[SLSI_NET_INDEX_WLAN] = NULL;
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_dbg_sap(sdev, skb));

	fapi_set_u16(skb, id, TEST_SPARE_1_IND);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_dbg_sap(sdev, skb));
}

static void test_slsi_rx_dbg_sap_work(struct kunit *test)
{
	struct slsi_skb_work *w = kunit_kzalloc(test, sizeof(struct slsi_skb_work), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	w->sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	slsi_skb_work_init(w->sdev, NULL, w, "kunit_sap_dbg_work", __sap_dbg_test_cb);

	skb->data = kunit_kzalloc(test, fapi_sig_size(debug_fault_ind), GFP_KERNEL);
	fapi_set_u16(skb, id, DEBUG_FAULT_IND);

	slsi_skb_work_enqueue(w, skb);
	slsi_rx_dbg_sap_work(&w->work);

	slsi_skb_work_deinit(w);
}

static void test_sap_dbg_rx_handler(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, sap_dbg_rx_handler(sdev, NULL));
	KUNIT_EXPECT_EQ(test, 0, sap_dbg_rx_handler(sdev, skb));
}

static void test_sap_dbg_init(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, sap_dbg_init());
}

static void test_sap_dbg_deinit(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, sap_dbg_deinit());
}

/* Test fictures */
static int sap_dbg_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void sap_dbg_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case sap_dbg_test_cases[] = {
	KUNIT_CASE(test_sap_dbg_version_supported),
	KUNIT_CASE(test_slsi_rx_debug),
	KUNIT_CASE(test_slsi_rx_dbg_sap),
	KUNIT_CASE(test_slsi_rx_dbg_sap_work),
	KUNIT_CASE(test_sap_dbg_rx_handler),
	KUNIT_CASE(test_sap_dbg_init),
	KUNIT_CASE(test_sap_dbg_deinit),
	{}
};

static struct kunit_suite sap_dbg_test_suite[] = {
	{
		.name = "sap_dbg-test",
		.test_cases = sap_dbg_test_cases,
		.init = sap_dbg_test_init,
		.exit = sap_dbg_test_exit,
	}
};

kunit_test_suites(sap_dbg_test_suite);
