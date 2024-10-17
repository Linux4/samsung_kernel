/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_CM_IF_H__
#define __KUNIT_MOCK_CM_IF_H__

#include "../scsc_wifi_cm_if.h"

#define slsi_sm_recovery_service_stop(args...)		kunit_mock_slsi_sm_recovery_service_stop(args)
#define slsi_sm_service_driver_unregister(args...)	kunit_mock_slsi_sm_service_driver_unregister(args)
#define slsi_wlan_service_notifier_unregister(args...)	kunit_mock_slsi_wlan_service_notifier_unregister(args)
#define slsi_sm_recovery_service_open(args...)		kunit_mock_slsi_sm_recovery_service_open(args)
#define slsi_get_sdev(args...)				kunit_mock_slsi_get_sdev(args)
#define slsi_wlan_service_notifier_register(args...)	kunit_mock_slsi_wlan_service_notifier_register(args)
#define slsi_sm_service_failed(args...)			kunit_mock_slsi_sm_service_failed(args)
#define slsi_is_test_mode_enabled(args...)		kunit_mock_slsi_is_test_mode_enabled(args)
#define slsi_sm_recovery_service_close(args...)		kunit_mock_slsi_sm_recovery_service_close(args)
#define slsi_check_rf_test_mode(args...)		kunit_mock_slsi_check_rf_test_mode(args)
#define slsi_sm_wlan_service_stop(args...)		kunit_mock_slsi_sm_wlan_service_stop(args)
#define slsi_sm_recovery_service_start(args...)		kunit_mock_slsi_sm_recovery_service_start(args)
#define slsi_sm_wlan_service_start(args...)		kunit_mock_slsi_sm_wlan_service_start(args)
#define slsi_sm_service_driver_register(args...)	kunit_mock_slsi_sm_service_driver_register(args)
#define slsi_sm_wlan_service_open(args...)		kunit_mock_slsi_sm_wlan_service_open(args)
#define slsi_sm_wlan_service_close(args...)		kunit_mock_slsi_sm_wlan_service_close(args)

static struct slsi_dev *cm_if_sdev;
static bool kunit_enable_test_mode;

static void test_cm_if_set_sdev(struct slsi_dev *sdev)
{
	cm_if_sdev = sdev;
}

static void test_cm_if_set_test_mode(bool enable)
{
	kunit_enable_test_mode = enable;
}

static int kunit_mock_slsi_sm_recovery_service_stop(struct slsi_dev *sdev)
{
	return 0;
}

static void kunit_mock_slsi_sm_service_driver_unregister(void)
{
	return;
}

static int kunit_mock_slsi_wlan_service_notifier_unregister(struct notifier_block *nb)
{
	return 0;
}

static int kunit_mock_slsi_sm_recovery_service_open(struct slsi_dev *sdev)
{
	return 0;
}

static struct slsi_dev *kunit_mock_slsi_get_sdev(void)
{
	return cm_if_sdev;
}

static int kunit_mock_slsi_wlan_service_notifier_register(struct notifier_block *nb)
{
	return 0;
}

static void kunit_mock_slsi_sm_service_failed(struct slsi_dev *sdev, const char *reason, bool is_work)
{
	return;
}

static bool kunit_mock_slsi_is_test_mode_enabled(void)
{
	return kunit_enable_test_mode;
}

static int kunit_mock_slsi_sm_recovery_service_close(struct slsi_dev *sdev)
{
	return 0;
}

static int kunit_mock_slsi_check_rf_test_mode(struct slsi_dev *sdev)
{
	return 0;
}

static int kunit_mock_slsi_sm_wlan_service_stop(struct slsi_dev *sdev)
{
	return 0;
}

static int kunit_mock_slsi_sm_recovery_service_start(struct slsi_dev *sdev)
{
	return 0;
}

static int kunit_mock_slsi_sm_wlan_service_start(struct slsi_dev *sdev)
{
	return 0;
}

static int kunit_mock_slsi_sm_service_driver_register(void)
{
	return 0;
}

static int kunit_mock_slsi_sm_wlan_service_open(struct slsi_dev *sdev)
{
	return 0;
}

static void kunit_mock_slsi_sm_wlan_service_close(struct slsi_dev *sdev)
{
	return;
}
#endif
