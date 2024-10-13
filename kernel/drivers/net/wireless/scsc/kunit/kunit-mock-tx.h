/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_TX_H__
#define __KUNIT_MOCK_TX_H__

#include "../tx.h"

#define slsi_tx_unpause_queues(args...)	kunit_mock_slsi_tx_unpause_queues(args)
#define slsi_tx_data(args...)		kunit_mock_slsi_tx_data(args)
#define slsi_tx_pause_queues(args...)	kunit_mock_slsi_tx_pause_queues(args)
#define slsi_tx_dhcp(args...)		kunit_mock_slsi_tx_dhcp(args)
#define slsi_tx_eapol(args...)		kunit_mock_slsi_tx_eapol(args)
#define slsi_tx_control(args...)	kunit_mock_slsi_tx_control(args)
#define slsi_tx_data_lower(args...)	kunit_mock_slsi_tx_data_lower(args)
#define slsi_tx_arp(args...)		kunit_mock_slsi_tx_arp(args)
#define is_msdu_enable(args...)		false


static void kunit_mock_slsi_tx_unpause_queues(struct slsi_dev *sdev)
{
	return;
}

static int kunit_mock_slsi_tx_data(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	if (dev->flags == 0)
		return 0;
	else if (dev->flags == 1)
		return -ENOSPC;
	else if (dev->flags == 2)
		return 1;
	return 0;
}

static void kunit_mock_slsi_tx_pause_queues(struct slsi_dev *sdev)
{
	return;
}

static int kunit_mock_slsi_tx_dhcp(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	return 0;
}

static int kunit_mock_slsi_tx_eapol(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	return 0;
}

static int kunit_mock_slsi_tx_control(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif;

	if (dev) {
		ndev_vif = netdev_priv(dev);

		if (ndev_vif->mgmt_tx_cookie == 0x9876) {
			return -EINVAL;
		} else if (ndev_vif->mgmt_tx_cookie == 0x1234) {
			ndev_vif->sig_wait.cfm = sdev->sig_wait.cfm;
		} else if (ndev_vif->mgmt_tx_cookie == 0x193A) {
			ndev_vif->sig_wait.cfm = sdev->sig_wait.cfm;
			ndev_vif->sig_wait.mib_error = sdev->sig_wait.mib_error;
		} else if (ndev_vif->mgmt_tx_cookie == 0x865A) {
			ndev_vif->sig_wait.ind = sdev->sig_wait.ind;
			ndev_vif->sig_wait.cfm = sdev->sig_wait.cfm;
		}
	} else {
		if (sdev->netdev[SLSI_NET_INDEX_WLAN]) {
			ndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_WLAN]);

			if (ndev_vif->mgmt_tx_cookie == 0x9876) {
				return -EINVAL;
			} else if (ndev_vif->mgmt_tx_cookie == 0x1234) {
				sdev->sig_wait.cfm = ndev_vif->sig_wait.cfm;
			} else if (ndev_vif->mgmt_tx_cookie == 0x193A) {
				sdev->sig_wait.cfm = ndev_vif->sig_wait.cfm;
				sdev->sig_wait.mib_error = ndev_vif->sig_wait.mib_error;
			} else if (ndev_vif->mgmt_tx_cookie == 0x865A) {
				sdev->sig_wait.ind = ndev_vif->sig_wait.ind;
				sdev->sig_wait.cfm = ndev_vif->sig_wait.cfm;
			} else if (ndev_vif->mgmt_tx_cookie == 0xABCD) {
				struct netdev_vif *p2pdev_vif;

				if (sdev->netdev[SLSI_NET_INDEX_P2P]) {
					p2pdev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_P2P]);

					if (p2pdev_vif->is_available) {
						sdev->sig_wait.cfm = ndev_vif->sig_wait.cfm;
						p2pdev_vif->is_available = false;
					} else {
						sdev->sig_wait.cfm = p2pdev_vif->sig_wait.cfm;
						p2pdev_vif->is_available = true;
					}
				}
			}
		}

		kfree_skb(skb);		//only for slsi_cdev_write() in udi.c
	}

	return 0;
}

static int kunit_mock_slsi_tx_data_lower(struct slsi_dev *sdev, struct sk_buff *skb)
{
	return 0;
}

static int kunit_mock_slsi_tx_arp(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	return 0;
}
#endif
