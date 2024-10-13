// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-kernel.h"
#include "kunit-mock-misc.h"
#include "kunit-mock-nl80211_vendor.h"
#include "kunit-mock-dev.h"
#include "kunit-mock-hip4_sampler.h"
#include "kunit-mock-hip.h"
#include "kunit-mock-mgt.h"
#include "../cm_if.c"

static void test_slsi_wlan_service_notifier_register(struct kunit *test)
{
	struct notifier_block *nb = kunit_kzalloc(test, sizeof(struct notifier_block), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, slsi_wlan_service_notifier_register(nb));
}

static void test_slsi_wlan_service_notifier_unregister(struct kunit *test)
{
	struct notifier_block *nb = kunit_kzalloc(test, sizeof(struct notifier_block), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, slsi_wlan_service_notifier_unregister(nb));
}

static void test_wlan_suspend(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	KUNIT_EXPECT_EQ(test, 0, wlan_suspend(&sdev->mx_wlan_client));
}

static void test_wlan_resume(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	KUNIT_EXPECT_EQ(test, 0, wlan_resume(&sdev->mx_wlan_client));
}

static void test_slsi_append_log_to_system_buffer(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct netdev_vif *ndev_vif;

	sdev->netdev[SLSI_NET_INDEX_WLAN] = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	ndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_WLAN]);
	ndev_vif->is_available = true;

	slsi_append_log_to_system_buffer(sdev);
}

static void test_wlan_failure_notification(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct mx_syserr_decode err;
	err.subsys = 3, err.level = 2, err.type = 3, err.subcode = 10;

	KUNIT_EXPECT_EQ(test, 2, wlan_failure_notification(&sdev->mx_wlan_client, &err));
}

static void test_wlan_failure_reset_v2(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u8 level = 3;
	u16 scsc_syserr_code = 12;

	wlan_failure_reset_v2(&sdev->mx_wlan_client, level, scsc_syserr_code);

	level = 5;
	wlan_failure_reset_v2(&sdev->mx_wlan_client, level, scsc_syserr_code);

	level = SLSI_WIFI_CM_IF_SYSTEM_ERROR_PANIC;
	sdev->forced_se_7 = true;
	wlan_failure_reset_v2(&sdev->mx_wlan_client, level, scsc_syserr_code);
}

static void test_wlan_stop_on_failure_v2(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct mx_syserr_decode err;
	struct netdev_vif *wlan_dev_vif;

	err.level = SLSI_WIFI_CM_IF_SYSTEM_ERROR_PANIC;
	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_PROBING);
	sdev->netdev_up_count = 1;
	sdev->netdev[SLSI_NET_INDEX_WLAN] = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	wlan_dev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_WLAN]);
	wlan_dev_vif->vif_type = FAPI_VIFTYPE_PRECONNECT;
	KUNIT_EXPECT_TRUE(test, wlan_stop_on_failure_v2(&sdev->mx_wlan_client, &err));

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_STOPPED);
	KUNIT_EXPECT_TRUE(test, wlan_stop_on_failure_v2(&sdev->mx_wlan_client, &err));
}

static void test_slsi_wlan_service_probe(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	enum scsc_module_client_reason reason = SCSC_MODULE_CLIENT_REASON_HW_PROBE;

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	slsi_wlan_service_probe(&wlan_driver, sdev->service->mx, reason);

	reason = SCSC_MODULE_CLIENT_REASON_RECOVERY_WPAN;
	slsi_wlan_service_probe(&wlan_driver, sdev->service->mx, reason);

	reason = SCSC_MODULE_CLIENT_REASON_RECOVERY_WLAN;
	slsi_wlan_service_probe(&wlan_driver, sdev->service->mx, reason);

	reason = SCSC_MODULE_CLIENT_REASON_RECOVERY;
	slsi_wlan_service_probe(&wlan_driver, sdev->service->mx, reason);
}

static void test_service_clean_up_locked(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	service_clean_up_locked(sdev);
}

static void test_slsi_wlan_service_remove(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct mx_syserr_decode err;
	enum scsc_module_client_reason reason;

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);

	cm_ctx.sdev = NULL;
	reason = SCSC_MODULE_CLIENT_REASON_RECOVERY_WPAN;
	slsi_wlan_service_remove(&wlan_driver, sdev->service->mx, reason);

	cm_ctx.sdev = sdev;
	slsi_wlan_service_remove(&wlan_driver, sdev->service->mx, reason);

	reason = SCSC_MODULE_CLIENT_REASON_RECOVERY_WLAN;
	slsi_wlan_service_remove(&wlan_driver, sdev->service->mx, reason);

	reason = SCSC_MODULE_CLIENT_REASON_RECOVERY;
	recovery_in_progress = 0;
	sdev->recovery_timeout = 321;
	sdev->netdev_up_count = 1;
	slsi_wlan_service_remove(&wlan_driver, sdev->service->mx, reason);

	recovery_in_progress = 1;
	sdev->recovery_next_state = SCSC_WIFI_CM_IF_STATE_STOPPING;
	atomic_set(&sdev->cm_if.reset_level, SLSI_WIFI_CM_IF_SYSTEM_ERROR_PANIC);
	slsi_wlan_service_remove(&wlan_driver, sdev->service->mx, reason);

	sdev->recovery_next_state = SCSC_WIFI_CM_IF_STATE_STOPPED;
	slsi_wlan_service_remove(&wlan_driver, sdev->service->mx, reason);

	reason = SCSC_MODULE_CLIENT_REASON_HW_PROBE;
	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_STARTING);
	slsi_wlan_service_remove(&wlan_driver, sdev->service->mx, reason);

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_STARTED);
	slsi_wlan_service_remove(&wlan_driver, sdev->service->mx, reason);
}

static void test_slsi_hip_block_bh(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	slsi_hip_block_bh(sdev);
}

static void test_slsi_sm_service_driver_register(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_service_driver_register());
}

static void test_slsi_sm_service_driver_unregister(struct kunit *test)
{
	slsi_sm_service_driver_unregister();
}

static void test_slsi_sm_service_failed(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	const char *reason = "KUNIT service failed";

	sdev->service = NULL;
	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_STARTING);
	slsi_sm_service_failed(sdev, reason, false);

	sdev->fail_reported = false;
	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_STARTED);
	slsi_sm_service_failed(sdev, reason, true);
}

static void test_slsi_is_test_mode_enabled(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, EnableTestMode, slsi_is_test_mode_enabled());
}

static void test_slsi_sm_recovery_service_stop(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_sm_recovery_service_stop(sdev));

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->service->id = 13;
	KUNIT_EXPECT_EQ(test, -EIO, slsi_sm_recovery_service_stop(sdev));

	sdev->service->id = 11;
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_recovery_service_stop(sdev));
}

static void test_slsi_sm_recovery_service_close(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_sm_recovery_service_close(sdev));

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->service->id = 13;
	KUNIT_EXPECT_EQ(test, -EIO, slsi_sm_recovery_service_close(sdev));

	sdev->service->id = 11;
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_recovery_service_close(sdev));
}

static void test_slsi_sm_recovery_service_open(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->service->id = 30;
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_recovery_service_open(sdev));

	sdev->service->id = 10;
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_recovery_service_open(sdev));
}

static void test_slsi_sm_recovery_service_start(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_PROBING);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_sm_recovery_service_start(sdev));

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_PROBED);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_sm_recovery_service_start(sdev));

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->service->id = 100;
	KUNIT_EXPECT_EQ(test, -EIO, slsi_sm_recovery_service_start(sdev));

	sdev->service->id = 19;
	sdev->device_config.host_state = 12;
	KUNIT_EXPECT_EQ(test, -EIO, slsi_sm_recovery_service_start(sdev));

	sdev->device_config.host_state = 11;
	KUNIT_EXPECT_EQ(test, -EIO, slsi_sm_recovery_service_start(sdev));

	sdev->service->id = 18;
	KUNIT_EXPECT_EQ(test, -EILSEQ, slsi_sm_recovery_service_start(sdev));

	sdev->service->id = 10;
	sdev->hip.hip_priv->host_pool_id_dat = 11;
	KUNIT_EXPECT_EQ(test, -EIO, slsi_sm_recovery_service_start(sdev));

	sdev->service->id = 10;
	sdev->hip.hip_priv->host_pool_id_dat = 5;
	sdev->device_config.host_state = 100;
	KUNIT_EXPECT_EQ(test, -EILSEQ, slsi_sm_recovery_service_start(sdev));

	sdev->device_config.host_state = 10;
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_recovery_service_start(sdev));
}

static void test_slsi_sm_wlan_service_open(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_PROBING);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_sm_wlan_service_open(sdev));

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_PROBED);
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_wlan_service_open(sdev));
}

static void test_slsi_sm_wlan_service_start(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_PROBING);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_sm_wlan_service_start(sdev));

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_PROBED);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_sm_wlan_service_start(sdev));

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->service->id = 100;
	KUNIT_EXPECT_EQ(test, -EIO, slsi_sm_wlan_service_start(sdev));

	sdev->service->id = 19;
	sdev->device_config.host_state = 12;
	KUNIT_EXPECT_EQ(test, -EIO, slsi_sm_wlan_service_start(sdev));

	sdev->device_config.host_state = 11;
	KUNIT_EXPECT_EQ(test, -EIO, slsi_sm_wlan_service_start(sdev));

	sdev->service->id = 18;
	KUNIT_EXPECT_EQ(test, -EILSEQ, slsi_sm_wlan_service_start(sdev));

	sdev->service->id = 10;
	sdev->hip.hip_priv->host_pool_id_dat = 11;
	KUNIT_EXPECT_EQ(test, -EILSEQ, slsi_sm_wlan_service_start(sdev));

	sdev->service->id = 10;
	sdev->hip.hip_priv->host_pool_id_dat = 5;
	sdev->device_config.host_state = 100;
	KUNIT_EXPECT_EQ(test, -EILSEQ, slsi_sm_wlan_service_start(sdev));

	sdev->device_config.host_state = 10;
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_wlan_service_start(sdev));
}

static void test_slsi_sm_wlan_service_stop_wait_locked(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	__slsi_sm_wlan_service_stop_wait_locked(sdev);
}

static void test_slsi_sm_wlan_service_stop(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_STARTED);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_sm_wlan_service_stop(sdev));

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->recovery_timeout = 321;
	sdev->service->id = 13;
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_wlan_service_stop(sdev));

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_BLOCKED);
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_wlan_service_stop(sdev));

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_PROBING);
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_wlan_service_stop(sdev));

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_PROBED);
	sdev->service->id = 10;
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_wlan_service_stop(sdev));

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_PROBED);
	sdev->service->id = 118;
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_wlan_service_stop(sdev));

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_PROBED);
	sdev->service->id = 11;
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_wlan_service_stop(sdev));

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_PROBED);
	sdev->service->id = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_sm_wlan_service_stop(sdev));
}


static void test_slsi_sm_wlan_service_close(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	sdev->service = kunit_kzalloc(test, sizeof(struct scsc_service), GFP_KERNEL);
	sdev->service->id = 13;
	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_PROBED);
	slsi_sm_wlan_service_close(sdev);

	atomic_set(&sdev->cm_if.cm_if_state, SCSC_WIFI_CM_IF_STATE_STOPPED);
	slsi_sm_wlan_service_close(sdev);

	sdev->service->id = 117;
	slsi_sm_wlan_service_close(sdev);

	sdev->service->id = 119;
	slsi_sm_wlan_service_close(sdev);
}

static void test_slsi_get_sdev(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	cm_ctx.sdev = sdev;
	KUNIT_EXPECT_PTR_EQ(test, sdev, slsi_get_sdev());
}

/*
 * Test fictures
 */
static int cm_if_test_init(struct kunit *test)
{
	test_dev_init(test);
	mutex_init(&slsi_start_mutex);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void cm_if_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
	return;
}

/*
 * KUnit testcase definitions
 */
static struct kunit_case cm_if_test_cases[] = {
	KUNIT_CASE(test_slsi_wlan_service_notifier_register),
	KUNIT_CASE(test_slsi_wlan_service_notifier_unregister),
	KUNIT_CASE(test_wlan_suspend),
	KUNIT_CASE(test_wlan_resume),
	KUNIT_CASE(test_slsi_append_log_to_system_buffer),
	KUNIT_CASE(test_wlan_failure_notification),
	KUNIT_CASE(test_wlan_failure_reset_v2),
	KUNIT_CASE(test_wlan_stop_on_failure_v2),
	KUNIT_CASE(test_slsi_wlan_service_probe),
	KUNIT_CASE(test_service_clean_up_locked),
	KUNIT_CASE(test_slsi_wlan_service_remove),
	KUNIT_CASE(test_slsi_hip_block_bh),
	KUNIT_CASE(test_slsi_sm_service_driver_register),
	KUNIT_CASE(test_slsi_sm_service_driver_unregister),
	KUNIT_CASE(test_slsi_sm_service_failed),
	KUNIT_CASE(test_slsi_is_test_mode_enabled),
	KUNIT_CASE(test_slsi_sm_recovery_service_stop),
	KUNIT_CASE(test_slsi_sm_recovery_service_close),
	KUNIT_CASE(test_slsi_sm_recovery_service_open),
	KUNIT_CASE(test_slsi_sm_recovery_service_start),
	KUNIT_CASE(test_slsi_sm_wlan_service_open),
	KUNIT_CASE(test_slsi_sm_wlan_service_start),
	KUNIT_CASE(test_slsi_sm_wlan_service_stop_wait_locked),
	KUNIT_CASE(test_slsi_sm_wlan_service_stop),
	KUNIT_CASE(test_slsi_sm_wlan_service_close),
	KUNIT_CASE(test_slsi_get_sdev),
	{}
};

static struct kunit_suite cm_if_test_suite[] = {
	{
		.name = "kunit-cm_if-ops-test",
		.test_cases = cm_if_test_cases,
		.init = cm_if_test_init,
		.exit = cm_if_test_exit,
	}
};

kunit_test_suites(cm_if_test_suite);
