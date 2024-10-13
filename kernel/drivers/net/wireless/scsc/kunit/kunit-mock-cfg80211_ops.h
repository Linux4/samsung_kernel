/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_CFG80211_OPS_H__
#define __KUNIT_MOCK_CFG80211_OPS_H__

#include "../cfg80211_ops.h"
#include "kunit-mock-kernel.h"

#define slsi_start_ap(args...)			kunit_mock_slsi_start_ap(args)
#define slsi_cfg80211_update_wiphy(args...)	kunit_mock_slsi_cfg80211_update_wiphy(args)
#define slsi_cfg80211_free(args...)		kunit_mock_slsi_cfg80211_free(args)
#define slsi_wlan_mgmt_tx(args...)		kunit_mock_slsi_wlan_mgmt_tx(args)
#define slsi_cfg80211_unregister(args...)	kunit_mock_slsi_cfg80211_unregister(args)
#define slsi_del_station(args...)		kunit_mock_slsi_del_station(args)
#define slsi_cfg80211_register(args...)		kunit_mock_slsi_cfg80211_register(args)
#define slsi_cfg80211_new(args...)		kunit_mock_slsi_cfg80211_new(args)

static struct slsi_dev *cfg80211_sdev;

static void test_cfg80211_set_sdev(struct slsi_dev *sdev)
{
	cfg80211_sdev = sdev;
}

static int kunit_mock_slsi_start_ap(struct wiphy *wiphy, struct net_device *dev,
				    struct cfg80211_ap_settings *settings)
{
	return 0;
}

static void kunit_mock_slsi_cfg80211_update_wiphy(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_cfg80211_free(struct slsi_dev *sdev)
{
	return;
}

static int kunit_mock_slsi_wlan_mgmt_tx(struct slsi_dev *sdev, struct net_device *dev,
					struct ieee80211_channel *chan, unsigned int wait,
					const u8 *buf, size_t len, bool dont_wait_for_ack, u64 *cookie)
{
	return 0;
}

static void kunit_mock_slsi_cfg80211_unregister(struct slsi_dev *sdev)
{
	return;
}

static int kunit_mock_slsi_del_station(struct wiphy *wiphy, struct net_device *dev,
				       struct station_del_parameters *del_params)
{
	return 0;
}

static int kunit_mock_slsi_cfg80211_register(struct slsi_dev *sdev)
{
	if (!sdev || !sdev->wiphy)
		return -EINVAL;

	return wiphy_register(sdev->wiphy);
}

static struct slsi_dev *kunit_mock_slsi_cfg80211_new(struct device *dev)
{
	return cfg80211_sdev;
}
#endif
