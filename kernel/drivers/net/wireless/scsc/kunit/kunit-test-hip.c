// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "../dev.h" 
#include "kunit-common.h"
#ifdef CONFIG_SCSC_WLAN_HIP5
#include "kunit-mock-hip5.h"
#else
#include "kunit-mock-hip4.h"
#endif
#include "kunit-mock-load_manager.h"
#include "kunit-mock-traffic_monitor.h"
#include "kunit-mock-hip4_smapper.h"
#include "kunit-mock-txbp.h"
#include "../hip.c"

static struct sap_api *sapi[SAP_TOTAL] = { 0 };

/* sap dummy functions */
int dummy_sap_version(u16 version)
{
	return 0;
}

int dummy_sap_handler(struct slsi_dev *sdev, struct sk_buff *skb)
{
	return 0;
}

int dummy_sap_txdone(struct slsi_dev *sdev, u32 colour, bool more)
{
	return 0;
}

/* Test function */
static void test_slsi_hip_sap_setup(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct hip_priv *priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
#ifdef CONFIG_SCSC_WLAN_HIP5
	struct hip5_hip_control *control = kunit_kzalloc(test, sizeof(struct hip5_hip_control), GFP_KERNEL);
#else
	struct hip4_hip_control *control = kunit_kzalloc(test, sizeof(struct hip4_hip_control), GFP_KERNEL);
#endif

	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	sdev->hip.hip_control = control;
#ifdef CONFIG_SCSC_WLAN_HIP5
	control->init.conf_hip5_ver = 4;
#else
	control->init.conf_hip4_ver = 4;
#endif

	KUNIT_EXPECT_EQ(test, 0, slsi_hip_sap_setup(sdev));
}

static void test_slsi_hip_service_notifier(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct hip_priv *priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	struct notifier_block *nb = NULL;
#ifdef CONFIG_SCSC_WLAN_HIP5
	struct hip5_hip_control *control = kunit_kzalloc(test, sizeof(struct hip5_hip_control), GFP_KERNEL);
#else
	struct hip4_hip_control *control = kunit_kzalloc(test, sizeof(struct hip4_hip_control), GFP_KERNEL);
#endif

	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	sdev->hip.hip_control = control;
#ifdef CONFIG_SCSC_WLAN_HIP5
	control->init.conf_hip5_ver = 4;
#else
	control->init.conf_hip4_ver = 4;
#endif

	KUNIT_EXPECT_EQ(test, NOTIFY_OK, slsi_hip_service_notifier(nb, SCSC_WIFI_STOP, sdev));
	KUNIT_EXPECT_EQ(test, NOTIFY_OK, slsi_hip_service_notifier(nb, SCSC_WIFI_FAILURE_RESET, sdev));
	KUNIT_EXPECT_EQ(test, NOTIFY_OK, slsi_hip_service_notifier(nb, SCSC_WIFI_SUSPEND, sdev));
	KUNIT_EXPECT_EQ(test, NOTIFY_OK, slsi_hip_service_notifier(nb, SCSC_WIFI_RESUME, sdev));
}

static void test_slsi_hip_cm_register(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct device *dev = kunit_kzalloc(test, sizeof(struct device), GFP_KERNEL);
	
	sdev->dev = dev;
	KUNIT_EXPECT_FALSE(test, slsi_hip_cm_register(sdev, dev));
}

static void test_slsi_hip_cm_unregister(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	slsi_hip_cm_unregister(sdev);
}

static void test_slsi_hip_start(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct hip_priv *priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	struct scsc_service *service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	struct net_device *ndev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	int mx = 1;

	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	sdev->service = service;
	sdev->maxwell_core = (struct scsc_mx *)&mx;
	sdev->netdev[SLSI_NET_INDEX_WLAN] = ndev;

	KUNIT_EXPECT_FALSE(test, slsi_hip_start(sdev));
}

static void test_slsi_hip_rx(struct kunit* test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct hip_priv *priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct notifier_block *nb;
#ifdef CONFIG_SCSC_WLAN_HIP5
	struct hip5_hip_control *control = kunit_kzalloc(test, sizeof(struct hip5_hip_control), GFP_KERNEL);
#else
	struct hip4_hip_control *control = kunit_kzalloc(test, sizeof(struct hip4_hip_control), GFP_KERNEL);
#endif

	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	sdev->hip.hip_control = control;
#ifdef CONFIG_SCSC_WLAN_HIP5
	control->init.conf_hip5_ver = 4;
#else
	control->init.conf_hip4_ver = 4;
#endif

	skb->data = (char*)kmalloc(sizeof(struct fapi_signal) + 100, GFP_KERNEL);
	INIT_LIST_HEAD(&sdev->log_clients.log_client_list);

	(((struct fapi_signal *)(skb)->data)->receiver_pid) = 0xCF01;
	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_MA;
	KUNIT_EXPECT_EQ(test, 0, slsi_hip_rx(sdev, skb));

	(((struct fapi_signal *)(skb)->data)->receiver_pid) = 0xCF00;
	KUNIT_EXPECT_EQ(test, 0, slsi_hip_rx(sdev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_MLME;
	KUNIT_EXPECT_EQ(test, 0, slsi_hip_rx(sdev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_DEBUG;
	KUNIT_EXPECT_EQ(test, 0, slsi_hip_rx(sdev, skb));
	
	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_TEST;
	KUNIT_EXPECT_EQ(test, 0, slsi_hip_rx(sdev, skb));
}

static void test_slsi_hip_tx_done(struct kunit* test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
#ifdef CONFIG_SCSC_WLAN_TX_API
	KUNIT_EXPECT_EQ(test, 0, slsi_hip_tx_done(sdev, 0, true));
#else
	KUNIT_EXPECT_EQ(test, 0, slsi_hip_tx_done(sdev, 1, 0, 0));
#endif
}

static void test_slsi_hip_setup_ext(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct hip_priv *priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);

	sdev->hip.hip_priv = priv;
	KUNIT_EXPECT_EQ(test, 0, slsi_hip_setup_ext(sdev));
}

static void test_slsi_hip_consume_smapper_entry(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct sk_buff *skb_fapi = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, slsi_hip_consume_smapper_entry(sdev, skb_fapi));
}

static void test_slsi_hip_get_skb_from_smapper(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct sk_buff *skb_fapi = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	KUNIT_EXPECT_PTR_EQ(test, slsi_hip_get_skb_from_smapper(sdev, skb_fapi), NULL);
}

static void test_slsi_hip_get_skb_data_from_smapper(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	
	KUNIT_EXPECT_PTR_EQ(test, slsi_hip_get_skb_data_from_smapper(sdev, skb), NULL);
}

static void test_slsi_hip_stop(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, slsi_hip_stop(sdev));
	
	atomic_set(&sdev->hip.hip_state, SLSI_HIP_STATE_STARTED);
	KUNIT_EXPECT_EQ(test, 0, slsi_hip_stop(sdev));
}

static void test_slsi_hip_reprocess_skipped_ctrl_bh(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	slsi_hip_reprocess_skipped_ctrl_bh(sdev);
}

/* Test features*/
static int hip_test_init(struct kunit *test)
{
	int i;
	for (i = 0; i < SAP_TOTAL; i++) {
		sapi[i] = kunit_kzalloc(test, sizeof(struct sap_api), GFP_KERNEL);
		sapi[i]->sap_class = i;
		sapi[i]->sap_notifier = NULL;
		sapi[i]->sap_version_supported = dummy_sap_version;
		sapi[i]->sap_handler = dummy_sap_handler;
		sapi[i]->sap_txdone = dummy_sap_txdone;
		slsi_hip_sap_register(sapi[i]);
	}

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void hip_test_exit(struct kunit *test)
{
	int i;

	for (i = 0; i < SAP_TOTAL; i++) {
		slsi_hip_sap_unregister(sapi[i]);
		kunit_kfree(test, sapi[i]);
	}

	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case hip_test_cases[] = {
	/* hip.c */	
	KUNIT_CASE(test_slsi_hip_sap_setup),
	KUNIT_CASE(test_slsi_hip_service_notifier),
	KUNIT_CASE(test_slsi_hip_cm_register),
	KUNIT_CASE(test_slsi_hip_cm_unregister),
	KUNIT_CASE(test_slsi_hip_start),
	KUNIT_CASE(test_slsi_hip_rx),
	KUNIT_CASE(test_slsi_hip_tx_done),
	KUNIT_CASE(test_slsi_hip_setup_ext),
	KUNIT_CASE(test_slsi_hip_consume_smapper_entry),
	KUNIT_CASE(test_slsi_hip_get_skb_from_smapper),
	KUNIT_CASE(test_slsi_hip_get_skb_data_from_smapper),
	KUNIT_CASE(test_slsi_hip_stop),
	KUNIT_CASE(test_slsi_hip_reprocess_skipped_ctrl_bh),
	{}
};

static struct kunit_suite hip_test_suite[] = {
	{
		.name = "kunit-hip-test",
		.test_cases = hip_test_cases,
		.init = hip_test_init,
		.exit = hip_test_exit,
	}
};

kunit_test_suites(hip_test_suite);
