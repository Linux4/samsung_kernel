/*****************************************************************************
 *
 * Copyright (c) 2021 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#ifndef __TDLS_MANAGER_H__
#define __TDLS_MANAGER_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/list_sort.h>

#include "dev.h"
#define TDLS_PEER_HASH_TABLE_SIZE BIT(4)
struct tdls_manager {
	u8 active;
	/**
	 * spinlock_t to synchronize peer_hash_table.
	 */
	spinlock_t peer_hash_lock;
	/**
	 * Peer hash table for traffic monitor.
	 */
	struct hlist_head peer_hash_table[TDLS_PEER_HASH_TABLE_SIZE];
	/**
	 * Manage active peer based on traffic and
	 * triggers discovery and teardown TDLS actions.
	 * It is called with 1 sec interval.
	 */
	struct delayed_work active_peer_manager;
	/**
	 * Synchronize with state_transition_queue.
	 * While holding this lock, we can guarantee that no state transition happens.
	 * Noting that peer is removed by peer_state_transition_manager, we can guarantee that
	 * peer record is always valid under this mutex.
	 */
	struct mutex state_transition_queue_mutex;
	/**
	 * State transition request queue.
	 */
	struct list_head state_transition_queue;
	/**
	 * Handle all state transition request for serialization
	 */
	struct work_struct peer_state_transition_manager;
};

/**
 * Lock: __netif_tx_lock spinlock
 * Context: Process with BHs disabled or BH
 * Description: This function assumes mac header is set.
 */
void slsi_tdls_manager_event_tx(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb, u8 ac);

/**
 * Lock: vif_mutex, start_stop_mutex
 * Context: Process
 * Description: It is called at the end of slsi_vif_activated before enabling activated flag.
 * We can allocate per vif data in this function.
 * We can access the priv data when ndev_vif->activated is true.
 */
int slsi_tdls_manager_on_vif_activated(struct slsi_dev *sdev, struct net_device *dev, struct netdev_vif *ndev_vif);

/**
 * Lock: vif_mutex, start_stop_mutex
 * Context: Process
 * Description: It is called at the end of slsi_vif_deactivated.
 * We should free the resources allocated in on_vif_activated.
 * At this stage, ndev_vif->activated is false.
 */
void slsi_tdls_manager_on_vif_deactivated(struct slsi_dev *sdev, struct net_device *dev, struct netdev_vif *ndev_vif);

/**
 * Lock: vif_mutex
 * Context: Process
 * Description: We should NOT free skb.
 * On success, FAPI_TDLSACTION_SETUP should be requested.
 */
void slsi_tdls_manager_event_discovered(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);

/**
 * Lock: vif_mutex, rtnl_lock
 * Context: Process
 * Description: We should NOT free skb.
 * slsi_peer_add should be called in this function.
 */
void slsi_tdls_manager_event_connected(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);

/**
 * Lock: vif_mutex
 * Context: Process
 * Description: We should NOT free skb.
 * slsi_peer_remove should be called in this function.
 */
void slsi_tdls_manager_event_disconnected(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);

/**
 * Driver callback for tdls_oper in cfg80211_ops.
 */
int slsi_tdls_manager_oper(struct wiphy *wiphy, struct net_device *dev, const u8 *peer,
			   enum nl80211_tdls_operation oper);

/**
 * Driver callback called when IP address is changed.
 */
void slsi_tdls_manager_address_changed(struct net_device *dev);
#endif

