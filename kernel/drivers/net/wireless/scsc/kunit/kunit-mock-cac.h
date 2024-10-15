/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_CAC_H__
#define __KUNIT_MOCK_CAC_H__

#include "../cac.h"

#define cac_ctrl_delete_tspec(args...)		kunit_mock_cac_ctrl_delete_tspec(args)
#define cac_ctrl_config_tspec(args...)		kunit_mock_cac_ctrl_config_tspec(args)
#define cac_get_active_tspecs(args...)		kunit_mock_cac_get_active_tspecs(args)
#define cac_ctrl_create_tspec(args...)		kunit_mock_cac_ctrl_create_tspec(args)
#define cac_rx_wmm_action(args...)		kunit_mock_cac_rx_wmm_action(args)
#define cac_delete_tspec_list(args...)		kunit_mock_cac_delete_tspec_list(args)
#define cac_update_roam_traffic_params(args...)	kunit_mock_cac_update_roam_traffic_params(args)
#define cac_ctrl_send_addts(args...)		kunit_mock_cac_ctrl_send_addts(args)
#define cac_ctrl_send_delts(args...)		kunit_mock_cac_ctrl_send_delts(args)


static int kunit_mock_cac_ctrl_delete_tspec(struct slsi_dev *sdev, char *args)
{
	return 0;
}

static int kunit_mock_cac_ctrl_config_tspec(struct slsi_dev *sdev, char *args)
{
	return 0;
}

static int kunit_mock_cac_get_active_tspecs(struct cac_activated_tspec **tspecs)
{
	return 0;
}

static int kunit_mock_cac_ctrl_create_tspec(struct slsi_dev *sdev, char *args)
{
	return 0;
}

static void kunit_mock_cac_rx_wmm_action(struct slsi_dev *sdev, struct net_device *netdev,
					 struct ieee80211_mgmt *data, size_t len)
{
	return;
}

static void kunit_mock_cac_delete_tspec_list(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_cac_update_roam_traffic_params(struct slsi_dev *sdev, struct net_device *dev)
{
	return;
}

static int kunit_mock_cac_ctrl_send_addts(struct slsi_dev *sdev, char *args)
{
	return 0;
}

static int kunit_mock_cac_ctrl_send_delts(struct slsi_dev *sdev, char *args)
{
	return 0;
}
#endif
