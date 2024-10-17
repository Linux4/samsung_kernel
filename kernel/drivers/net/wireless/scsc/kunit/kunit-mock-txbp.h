/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_TXBP_H__
#define __KUNIT_MOCK_TXBP_H__

#include "../txbp.h"
#include "../tx_api.h"

#define slsi_dev_detach_post(args...)		kunit_mock_slsi_dev_detach_post(args)
#define slsi_dev_attach_post(args...)		kunit_mock_slsi_dev_attach_post(args)
#define slsi_tx_get_number_of_queues(args...)	kunit_mock_slsi_tx_get_number_of_queues(args)
#define slsi_tx_done(args...)			kunit_mock_slsi_tx_done(args)
#define slsi_tx_timeout(args...)		kunit_mock_slsi_tx_timeout(args)
#define slsi_tx_transmit_data(args...)		kunit_mock_slsi_tx_transmit_data(args)
#define slsi_tx_transmit_lower(args...)		kunit_mock_slsi_tx_transmit_lower(args)
#define slsi_tx_setup_net_device(args...)	kunit_mock_slsi_tx_setup_net_device(args)
#define slsi_peer_add_post(args...)		kunit_mock_slsi_peer_add_post(args)
#define slsi_peer_remove_post(args...)		kunit_mock_slsi_peer_remove_post(args)
#define slsi_vif_activated_post(args...)	kunit_mock_slsi_vif_activated_post(args)
#define slsi_vif_deactivated_post(args...)	kunit_mock_slsi_vif_deactivated_post(args)
#define slsi_tx_ps_port_control(args...)	kunit_mock_slsi_tx_ps_port_control(args)
#define slsi_tx_tdls_update(args...)		kunit_mock_slsi_tx_tdls_update(args)
#define slsi_tx_select_queue(args...)		kunit_mock_slsi_tx_select_queue(args)
#define slsi_tx_get_packet_type(args...)	kunit_mock_slsi_tx_get_packet_type(args)
#define slsi_tx_transmit_ctrl(args...)		kunit_mock_slsi_tx_transmit_ctrl(args)
#define slsi_tx_mlme_ind(args...)		kunit_mock_slsi_tx_mlme_ind(args)
#define slsi_tx_mlme_cfm(args...)		kunit_mock_slsi_tx_mlme_cfm(args)

#ifdef CONFIG_SCSC_WLAN_TX_API
#define slsi_txbp_suspend(arg)
#define slsi_txbp_resume(arg)
#endif


static void kunit_mock_slsi_dev_detach_post(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_dev_attach_post(struct slsi_dev *sdev)
{
	return;
}

static int kunit_mock_slsi_tx_get_number_of_queues(void)
{
	return 6;
}

static int kunit_mock_slsi_tx_done(struct slsi_dev *sdev, u32 colour, bool more)
{
	return 0;
}

static void kunit_mock_slsi_tx_timeout(struct net_device *dev)
{
	return;
}

static int kunit_mock_slsi_tx_transmit_data(struct slsi_dev *sdev, struct net_device *dev,
					    struct sk_buff *skb)
{
	return 0;
}

static int kunit_mock_slsi_tx_transmit_lower(struct slsi_dev *sdev, struct sk_buff *skb)
{
	return 0;
}


static void kunit_mock_slsi_tx_setup_net_device(struct net_device *dev)
{
	return;
}

static bool kunit_mock_slsi_peer_add_post(struct slsi_dev *sdev, struct net_device *dev,
					  struct netdev_vif *ndev_vif, struct slsi_peer *peer)
{
	return true;
}

static void kunit_mock_slsi_peer_remove_post(struct slsi_dev *sdev, struct net_device *dev,
					     struct netdev_vif *ndev_vif, struct slsi_peer *peer)
{
	return;
}

static bool kunit_mock_slsi_vif_activated_post(struct slsi_dev *sdev, struct net_device *dev,
					       struct netdev_vif *ndev_vif)
{
	return true;
}

static bool kunit_mock_slsi_vif_deactivated_post(struct slsi_dev *sdev, struct net_device *dev,
						 struct netdev_vif *ndev_vif)
{
	return true;
}

static int kunit_mock_slsi_tx_ps_port_control(struct slsi_dev *sdev, struct net_device *dev,
					      struct slsi_peer *peer, enum slsi_sta_conn_state s)
{
	return 0;
}

static bool kunit_mock_slsi_tx_tdls_update(struct slsi_dev *sdev, struct net_device *dev,
					   struct slsi_peer *sta_peer, struct slsi_peer *tdls_peer,
					   bool connection)
{
	return true;
}

#if (KERNEL_VERSION(5, 2, 0) <= LINUX_VERSION_CODE)
static u16 kunit_mock_slsi_tx_select_queue(struct net_device *dev, struct sk_buff *skb,
					   struct net_device *sb_dev)
#elif (KERNEL_VERSION(4, 19, 0) <= LINUX_VERSION_CODE)
static u16 kunit_mock_slsi_tx_select_queue(struct net_device *dev, struct sk_buff *skb,
					   struct net_device *sb_dev, select_queue_fallback_t fallback)
#else
static u16 kunit_mock_slsi_tx_select_queue(struct net_device *dev, struct sk_buff *skb,
					   void *accel_priv, select_queue_fallback_t fallback)
#endif
{
	return 0;
}

static enum slsi_tx_packet_t kunit_mock_slsi_tx_get_packet_type(struct sk_buff *skb)
{
	if (skb->queue_mapping < SLSI_TX_DATA_QUEUE_NUM)
		return SLSI_DATA_PKT;
	return SLSI_CTRL_PKT;
}

static int kunit_mock_slsi_tx_transmit_ctrl(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	return 0;
}

static int kunit_mock_slsi_tx_mlme_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	return 0;
}

static int kunit_mock_slsi_tx_mlme_cfm(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	return 0;
}
#endif
