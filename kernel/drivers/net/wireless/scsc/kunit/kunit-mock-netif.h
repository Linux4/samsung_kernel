/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_NETIF_H__
#define __KUNIT_MOCK_NETIF_H__

#include "../netif.h"
#include "kunit-mock-kernel.h"

#define slsi_netif_remove_locked(args...)		kunit_mock_slsi_netif_remove_locked(args)
#define slsi_netif_register_locked(args...)		kunit_mock_slsi_netif_register_locked(args)
#define slsi_net_downgrade_pri(args...)			kunit_mock_slsi_net_downgrade_pri(args)
#define slsi_netif_remove(args...)			kunit_mock_slsi_netif_remove(args)
#define slsi_net_randomize_nmi_ndi(args...)		kunit_mock_slsi_net_randomize_nmi_ndi(args)
#define slsi_netif_add_locked(args...)			kunit_mock_slsi_netif_add_locked(args)
#define slsi_tdls_move_packets(args...)			kunit_mock_slsi_tdls_move_packets(args)
#define slsi_get_priority_from_tos_dscp(args...)	kunit_mock_slsi_get_priority_from_tos_dscp(args)
#define slsi_netif_register_rtlnl_locked(args...)	kunit_mock_slsi_netif_register_rtlnl_locked(args)
#define slsi_netif_init(args...)			kunit_mock_slsi_netif_init(args)
#define slsi_netif_remove_all(args...)			kunit_mock_slsi_netif_remove_all(args)
#define slsi_netif_register(args...)			kunit_mock_slsi_netif_register(args)
#define slsi_frame_priority_to_ac_queue(args...)	kunit_mock_slsi_frame_priority_to_ac_queue(args)
#define slsi_netif_set_tid_config(args...)		kunit_mock_slsi_netif_set_tid_config(args)
#define slsi_net_downgrade_ac(args...)			kunit_mock_slsi_net_downgrade_ac(args)
#define slsi_ac_to_tids(args...)			kunit_mock_slsi_ac_to_tids(args)
#define slsi_netif_remove_rtlnl_locked(args...)		kunit_mock_slsi_netif_remove_rtlnl_locked(args)
#define slsi_get_priority_from_tos(args...)		kunit_mock_slsi_get_priority_from_tos(args)
#define slsi_netif_dynamic_iface_add(args...)		kunit_mock_slsi_netif_dynamic_iface_add(args)
#define slsi_netif_deinit(args...)			kunit_mock_slsi_netif_deinit(args)
#define slsi_netif_set_tid_changed_tid(args...)		kunit_mock_slsi_netif_set_tid_changed_tid(args)
#define slsi_netif_set_tid_change_tid


static void kunit_mock_slsi_netif_remove_locked(struct slsi_dev *sdev, struct net_device *dev,
						bool is_cfg80211)
{
	return;
}

static int kunit_mock_slsi_netif_register_locked(struct slsi_dev *sdev, struct net_device *dev,
						 bool is_cfg80211)
{
	return 0;
}

static void kunit_mock_slsi_net_downgrade_pri(struct net_device *dev, struct slsi_peer *peer,
					      struct sk_buff *skb)
{
	return;
}

static void kunit_mock_slsi_netif_remove(struct slsi_dev *sdev, struct net_device *dev,
					 bool is_cfg80211)
{
	return;
}

static void kunit_mock_slsi_net_randomize_nmi_ndi(struct slsi_dev *sdev)
{
	return;
}

static int kunit_mock_slsi_netif_add_locked(struct slsi_dev *sdev, const char *name, int ifnum)
{
	return 0;
}

static void kunit_mock_slsi_tdls_move_packets(struct slsi_dev *sdev, struct net_device *dev,
					      struct slsi_peer *sta_peer,
					      struct slsi_peer *tdls_peer,
					      bool connection)
{
	return;
}

static u16 kunit_mock_slsi_get_priority_from_tos_dscp(u8 *frame, u16 proto)
{
	return 0;
}

static int kunit_mock_slsi_netif_register_rtlnl_locked(struct slsi_dev *sdev, struct net_device *dev,
						       bool is_cfg80211)
{
	return 0;
}

static int kunit_mock_slsi_netif_init(struct slsi_dev *sdev)
{
	if (!sdev || sdev->netdev[0])
		return -EINVAL;

	return 0;
}

static void kunit_mock_slsi_netif_remove_all(struct slsi_dev *sdev, bool is_cfg80211)
{
	return;
}

static int kunit_mock_slsi_netif_register(struct slsi_dev *sdev, struct net_device *dev,
					  bool is_cfg80211)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int err = 0;

#if (KERNEL_VERSION(5, 12, 0) <= LINUX_VERSION_CODE)
	if (is_cfg80211)
		err = cfg80211_register_netdevice(dev);
	else
		err = register_netdevice(dev);
#else
	err = register_netdevice(dev);
#endif

	return err;
}

static enum slsi_traffic_q kunit_mock_slsi_frame_priority_to_ac_queue(u16 priority)
{
	return (enum slsi_traffic_q)0;
}

static int kunit_mock_slsi_netif_set_tid_config(struct slsi_dev *sdev, struct net_device *dev,
						u8 mode, u32 uid, u8 tid)
{
	return 0;
}

static bool kunit_mock_slsi_net_downgrade_ac(struct net_device *dev, struct sk_buff *skb)
{
	return false;
}

static int kunit_mock_slsi_ac_to_tids(enum slsi_traffic_q ac, int *tids)
{
	return 0;
}

static void kunit_mock_slsi_netif_remove_rtlnl_locked(struct slsi_dev *sdev, struct net_device *dev,
						      bool is_cfg80211)
{
	return;
}

static u16 kunit_mock_slsi_get_priority_from_tos(u8 *frame, u16 proto)
{
	return 0;
}

static int kunit_mock_slsi_netif_dynamic_iface_add(struct slsi_dev *sdev, const char *name)
{
	return 0;
}

static void kunit_mock_slsi_netif_deinit(struct slsi_dev *sdev, bool is_cfg80211)
{
	return;
}

static void kunit_mock_slsi_netif_set_tid_changed_tid(struct net_device *dev, struct sk_buff *skb)
{
	return;
}
#endif
