/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_TDLS_MANAGER_H__
#define __KUNIT_MOCK_TDLS_MANAGER_H__

#include "../tdls_manager.h"

#define slsi_tdls_manager_event_tx(args...)		kunit_mock_slsi_tdls_manager_event_tx(args)
#define slsi_tdls_manager_on_vif_deactivated(args...)	kunit_mock_slsi_tdls_manager_on_vif_deactivated(args)
#define slsi_tdls_manager_event_connected(args...)	kunit_mock_slsi_tdls_manager_event_connected(args)
#define slsi_tdls_manager_oper(args...)			kunit_mock_slsi_tdls_manager_oper(args)
#define slsi_tdls_manager_event_disconnected(args...)	kunit_mock_slsi_tdls_manager_event_disconnected(args)
#define slsi_tdls_manager_on_vif_activated(args...)	kunit_mock_slsi_tdls_manager_on_vif_activated(args)
#define slsi_tdls_manager_event_discovered(args...)	kunit_mock_slsi_tdls_manager_event_discovered(args)


static void kunit_mock_slsi_tdls_manager_event_tx(struct slsi_dev *sdev, struct net_device *dev,
						  struct sk_buff *skb, u8 ac)
{
	return;
}

static void kunit_mock_slsi_tdls_manager_on_vif_deactivated(struct slsi_dev *sdev, struct net_device *dev,
							    struct netdev_vif *ndev_vif)
{
	return;
}

static void kunit_mock_slsi_tdls_manager_event_connected(struct slsi_dev *sdev, struct net_device *dev,
							 struct sk_buff *skb)
{
	return;
}

static int kunit_mock_slsi_tdls_manager_oper(struct wiphy *wiphy, struct net_device *dev, const u8 *peer,
			   enum nl80211_tdls_operation oper)
{
	return 0;
}

static void kunit_mock_slsi_tdls_manager_event_disconnected(struct slsi_dev *sdev, struct net_device *dev,
							    struct sk_buff *skb)
{
	return;
}

static int kunit_mock_slsi_tdls_manager_on_vif_activated(struct slsi_dev *sdev, struct net_device *dev,
							 struct netdev_vif *ndev_vif)
{
	return 0;
}

static void kunit_mock_slsi_tdls_manager_event_discovered(struct slsi_dev *sdev, struct net_device *dev,
							  struct sk_buff *skb)
{
	return;
}
#endif
