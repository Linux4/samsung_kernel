/*****************************************************************************
 *
 * Copyright (c) 2021 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/list_sort.h>
#include <linux/ktime.h>
#include <linux/netdevice.h>
#include <net/neighbour.h>
#include <net/route.h>
#include <scsc/scsc_warn.h>
#include "debug.h"
#include "mlme.h"
#include "mgt.h"
#include "nl80211_vendor.h"
#include "tdls_manager.h"

#ifdef CONFIG_SCSC_WLAN_TX_API
#include "tx_api.h"
#endif

static const u8 broadcast_mac[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define SLSI_TDLS_NO_TRAFFIC_MAX_COUNT (20)
#define SLSI_TDLS_MAX_TIME_NS_AFTER_LAST_TX (100000000)

#define TDLS_PEER_IND_TIMEOUT (msecs_to_jiffies(20000))
#define TDLS_PEER_BLOCKED_TIMEOUT (msecs_to_jiffies(5000))

static bool tdls_supported_by_fw;
static int tdls_manager_discovery_threshold = 100;
module_param(tdls_manager_discovery_threshold, int, 0644);
MODULE_PARM_DESC(tdls_manager_discovery_threshold, "The number of packets to trigger TDLS dicovery");

void slsi_tdls_manager_address_changed(struct net_device *dev)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct rtable *rt;
	struct neighbour *neigh;
	struct flowi4 flow4 = {};
	__be32 gw_addr;

	flow4.flowi4_oif = dev->ifindex;
	flow4.daddr = 10 | (1 << 24); /* any ip address for obtaining gateway ip */

	rt = ip_route_output_key(dev_net(dev), &flow4);
	if (!rt)
		return;
#if (KERNEL_VERSION(5, 2, 0) <= LINUX_VERSION_CODE)
	gw_addr = rt->rt_gw4;
#else
	gw_addr = rt->rt_gateway;
#endif
	neigh = neigh_lookup(&arp_tbl, &gw_addr, dev);
	if (!neigh)
		goto out;
	SLSI_ETHER_COPY(ndev_vif->sta.tdls_dgw_macaddr, neigh->ha);
	SLSI_NET_DBG1(dev, SLSI_TDLS, "The patcket sent to %pM is ignored.\n", ndev_vif->sta.tdls_dgw_macaddr);
	neigh_release(neigh);
out:
	ip_rt_put(rt);
}

static int slsi_tdls_manager_add_peer_to_candidate_list(struct netdev_vif *ndev_vif, struct tdls_manager *manager, struct tdls_peer *t_peer)
{
	struct sorted_peer_entry *entry;

	if (ether_addr_equal(ndev_vif->sta.tdls_dgw_macaddr, t_peer->mac_addr))
		return 0;

	WLBT_WARN_ON(!mutex_is_locked(&manager->state_transition_queue_mutex));

	if (!list_empty(&ndev_vif->sta.tdls_candidate_setup_list)) {
		struct sorted_peer_entry *tmp;

		list_for_each_entry_safe(entry, tmp, &ndev_vif->sta.tdls_candidate_setup_list, list) {
			if (ether_addr_equal(t_peer->mac_addr, entry->peer->mac_addr))
				return 0;
		}
	}

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (entry) {
		entry->peer = t_peer;
		INIT_LIST_HEAD(&entry->list);
		list_add_tail(&entry->list, &ndev_vif->sta.tdls_candidate_setup_list);
		ndev_vif->sta.tdls_candidate_setup_count++;
		return 1;
	}
	return 0;
}

static int slsi_tdls_manager_remove_candidate_list(struct netdev_vif *ndev_vif, struct tdls_manager *manager, struct tdls_peer *t_peer)
{
	WLBT_WARN_ON(!mutex_is_locked(&manager->state_transition_queue_mutex));

	if (!list_empty(&ndev_vif->sta.tdls_candidate_setup_list)) {
		struct sorted_peer_entry *entry;
		struct sorted_peer_entry *tmp;

		list_for_each_entry_safe(entry, tmp, &ndev_vif->sta.tdls_candidate_setup_list, list) {
			if (ether_addr_equal(t_peer->mac_addr, entry->peer->mac_addr)) {
				list_del(&entry->list);
				kfree(entry);
				ndev_vif->sta.tdls_candidate_setup_count--;
				return 1;
			}
		}
	}
	return 0;
}

static inline void slsi_lock_tdls_tcp_ack_lock(struct netdev_vif *ndev_vif)
{
	/* slsi_tdls_move_packets() accesses netdev_vif->ack_suppression records which is protected
	 * by (&ndev_vif->tcp_ack_lock), but due to order dependency it can NOT take (&ndev_vif->tcp_ack_lock)
	 * after (&ndev_vif->peer_lock).
	 * so acquire (&ndev_vif->tcp_ack_lock) first and then (&ndev_vif->peer_lock)
	 */
	slsi_spinlock_lock(&ndev_vif->tcp_ack_lock);
	slsi_spinlock_lock(&ndev_vif->peer_lock);
}

static inline void slsi_lock_tdls_tcp_ack_unlock(struct netdev_vif *ndev_vif)
{
	/* unlock tcp_ack_lock here as slsi_peer_remove can call transmit in same context
	 * that will deadlock on tcp_ack_lock. While unlocking, maintain order between peer_lock
	 * and tcp_ack_lock
	 */
	slsi_spinlock_unlock(&ndev_vif->peer_lock);
	slsi_spinlock_unlock(&ndev_vif->tcp_ack_lock);
}

/**
 * Should be called with peer_hash_lock held.
 */
static struct tdls_peer *slsi_tdls_manager_find_peer(struct net_device *dev, struct tdls_manager *manager, const u8 *mac)
{
	u32 hash = 0;
	struct hlist_head *bucket = NULL;
	struct tdls_peer *t_peer = NULL;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (WLBT_WARN(!ndev_vif->activated, "VIF Not Activated"))
		return NULL;

	/**
	 * tdls for this netdevice is in deactivated process.
	 * do not return peer and let tdls function gracefully fail.
	 */
	if (!ndev_vif->sta.tdls_manager.active)
		return NULL;

	hash = jhash(mac, ETH_ALEN, 0);
	bucket = &manager->peer_hash_table[hash & (TDLS_PEER_HASH_TABLE_SIZE - 1)];
	hlist_for_each_entry(t_peer, bucket, hlist) {
		if (ether_addr_equal(mac, t_peer->mac_addr))
			return t_peer;
	}
	return NULL;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 13, 0))
static int slsi_tdls_manager_peer_sort(void *priv, const struct list_head *a, const struct list_head *b)
#else
static int slsi_tdls_manager_peer_sort(void *priv, struct list_head *a, struct list_head *b)
#endif
{
	struct sorted_peer_entry *peer_a = container_of(a, struct sorted_peer_entry, list);
	struct sorted_peer_entry *peer_b = container_of(b, struct sorted_peer_entry, list);

	if (peer_a->peer->tdls_weight > peer_b->peer->tdls_weight)
		return -1;
	return 1;
}

/**
 * This function MUST NOT be called directly other then
 * enqueue_peer_state_transition and
 * slsi_tdls_manager_peer_state_transition_handler.
 * Caller should claim state_transition_queue_mutex.
 */
void enqueue_peer_state_transition_internal(struct tdls_manager *manager, struct tdls_peer *peer,
					    enum tdls_peer_state next_state, struct sk_buff *frame)
{
	struct state_transition_request *request;
	struct net_device *dev = peer->dev;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	if (!manager->active)
		return;

	if (!ndev_vif->activated)
		return;

	if (!ndev_vif->sta.tdls_manager.active)
		return;

	if (peer->state == SLSI_TDLS_PEER_INACTIVE) {
		/**
		 * we should not make further state transition when
		 * peer->state reached to SLSI_TDLS_PEER_INACTIVE.
		 */
		return;
	}

	if (next_state == SLSI_TDLS_PEER_INACTIVE)
		peer->state = SLSI_TDLS_PEER_INACTIVE;

	if (frame) {
		frame = skb_clone(frame, GFP_KERNEL);
		if (!frame)
			return;
	}

	request = kmalloc(sizeof(*request), GFP_KERNEL);
	if (!request) {
		consume_skb(frame);
		return;
	}
	request->next_state = next_state;
	request->peer = peer;
	request->frame = frame;

	INIT_LIST_HEAD(&request->list);
	list_add_tail(&request->list, &manager->state_transition_queue);
}

/**
 * Enqueue state transition.
 * Caller should claim state_transition_queue_mutex.
 */
void enqueue_peer_state_transition(struct tdls_manager *manager, struct tdls_peer *peer,
				   enum tdls_peer_state next_state, struct sk_buff *frame)
{
	if (!manager->active)
		return;

	enqueue_peer_state_transition_internal(manager, peer, next_state, frame);
	schedule_work(&manager->peer_state_transition_manager);
}

static void slsi_tdls_manager_indication_timeout(struct work_struct *work)
{
	struct delayed_work *delayed_work_instance = container_of(work, struct delayed_work, work);
	struct tdls_peer *t_peer = container_of(delayed_work_instance, struct tdls_peer, tdls_peer_ind_timeout_work);
	struct tdls_manager *manager = t_peer->manager;
	struct slsi_vif_sta *vif_sta = container_of(manager, struct slsi_vif_sta, tdls_manager);
	struct netdev_vif *ndev_vif = container_of(vif_sta, struct netdev_vif, sta);
	struct net_device *dev = slsi_get_netdev(ndev_vif->sdev, ndev_vif->ifnum);

	mutex_lock(&manager->state_transition_queue_mutex);
	if (slsi_tdls_manager_remove_candidate_list(ndev_vif, manager, t_peer)) {
		struct tdls_peer *peer = kmalloc(sizeof(*peer), GFP_KERNEL);

		if (peer) {
			peer->dev = dev;
			peer->state = SLSI_TDLS_PEER_ACTIVE;
			peer->manager = manager;
			ether_addr_copy(peer->mac_addr, broadcast_mac);
			SLSI_NET_DBG1(dev, SLSI_TDLS, "Delete peer:%pM from candidate list\n", t_peer->mac_addr);
			enqueue_peer_state_transition(manager, peer, SLSI_TDLS_PEER_CANDIDATE_SETUP, NULL);
		}
	}
	enqueue_peer_state_transition(manager, t_peer, SLSI_TDLS_PEER_INACTIVE, NULL);
	mutex_unlock(&manager->state_transition_queue_mutex);
}

static void slsi_tdls_manager_peer_blocked_timeout(struct work_struct *work)
{
	struct delayed_work *delayed_work_instance = container_of(work, struct delayed_work, work);
	struct tdls_peer *t_peer = container_of(delayed_work_instance, struct tdls_peer, tdls_peer_blocked_work);
	struct tdls_manager *manager = t_peer->manager;

	mutex_lock(&manager->state_transition_queue_mutex);
	if (t_peer->state == SLSI_TDLS_PEER_BLOCKED)
		enqueue_peer_state_transition(manager, t_peer, SLSI_TDLS_PEER_INACTIVE, NULL);
	mutex_unlock(&manager->state_transition_queue_mutex);
}

static void slsi_tdls_manager_connected_ind(struct tdls_manager *manager, struct tdls_peer *t_peer, u16 flow_id)
{
	struct slsi_vif_sta *vif_sta = container_of(manager, struct slsi_vif_sta, tdls_manager);
	struct netdev_vif *ndev_vif = container_of(vif_sta, struct netdev_vif, sta);
	struct net_device *dev = slsi_get_netdev(ndev_vif->sdev, ndev_vif->ifnum);
	struct slsi_peer *peer;
	u16 peer_index = (flow_id >> 8);

	ndev_vif->sta.tdls_enabled = true;

	if (!ndev_vif->activated) {
		SLSI_NET_ERR(dev, "VIF not activated\n");
		return;
	}

	if (WLBT_WARN(ndev_vif->vif_type != FAPI_VIFTYPE_STATION, "STA VIF"))
		return;

	if (peer_index < SLSI_TDLS_PEER_INDEX_MIN || peer_index > SLSI_TDLS_PEER_INDEX_MAX) {
		SLSI_NET_ERR(dev, "Received incorrect peer_index: %d\n", peer_index);
		return;
	}
	SLSI_NET_DBG1(dev, SLSI_MLME, "TDLS session connected\n");
	slsi_lock_tdls_tcp_ack_lock(ndev_vif);
	/* Check for MAX client */
	if (ndev_vif->sta.tdls_peer_sta_records + 1 > ndev_vif->sta.tdls_max_peer) {
		SLSI_NET_ERR(dev, "MAX TDLS peer limit reached. Ignore ind for peer_index:%d\n", peer_index);
		slsi_lock_tdls_tcp_ack_unlock(ndev_vif);
		return;
	}

	peer = slsi_peer_add(ndev_vif->sdev, dev, t_peer->mac_addr, peer_index);

	if (!peer) {
		SLSI_NET_ERR(dev, "Peer NOT Created\n");
		slsi_lock_tdls_tcp_ack_unlock(ndev_vif);
		return;
	}

	/* QoS is mandatory for TDLS - enable QoS for TDLS peer by default */
	peer->qos_enabled = true;
	peer->flow_id = flow_id;

	slsi_ps_port_control(ndev_vif->sdev, dev, peer, SLSI_STA_CONN_STATE_CONNECTED);

#ifdef CONFIG_SCSC_WLAN_TX_API
	slsi_tx_tdls_update(ndev_vif->sdev, dev, ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET], peer, true);
#else
	/* Move TDLS packets from STA_Q to TDLS_Q */
	slsi_tdls_move_packets(ndev_vif->sdev, dev, ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET], peer, true);
#endif
	slsi_lock_tdls_tcp_ack_unlock(ndev_vif);
}

static void slsi_tdls_manager_disconnected_ind(struct tdls_manager *manager, struct tdls_peer *t_peer, u16 reason_code)
{
	struct slsi_vif_sta *vif_sta = container_of(manager, struct slsi_vif_sta, tdls_manager);
	struct netdev_vif *ndev_vif = container_of(vif_sta, struct netdev_vif, sta);
	struct net_device *dev = (struct net_device *)((void *)ndev_vif - netdev_priv((struct net_device *)0));
	struct slsi_peer *peer;

	if (!ndev_vif->activated) {
		SLSI_NET_ERR(dev, "VIF not activated\n");
		return;
	}

	SLSI_NET_DBG1(dev, SLSI_MLME, "TDLS session disconnected\n");

	slsi_lock_tdls_tcp_ack_lock(ndev_vif);

	peer = slsi_get_peer_from_mac(ndev_vif->sdev, dev, t_peer->mac_addr);
	if (WLBT_WARN(!peer || peer->aid == 0, "peer NOT found by MAC address\n")) {
		slsi_lock_tdls_tcp_ack_unlock(ndev_vif);
		return;
	}

	slsi_ps_port_control(ndev_vif->sdev, dev, peer, SLSI_STA_CONN_STATE_DISCONNECTED);

#ifdef CONFIG_SCSC_WLAN_TX_API
	slsi_tx_tdls_update(ndev_vif->sdev, dev, ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET], peer, false);
#else
	/* Move TDLS packets from TDLS_Q to STA_Q */
	slsi_tdls_move_packets(ndev_vif->sdev, dev, ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET], peer, false);
#endif
	slsi_lock_tdls_tcp_ack_unlock(ndev_vif);

	/* take peer_lock again as, it is a prerequisite for slsi_peer_remove */
	slsi_spinlock_lock(&ndev_vif->peer_lock);
	slsi_peer_remove(ndev_vif->sdev, dev, peer);
	slsi_spinlock_unlock(&ndev_vif->peer_lock);

	switch (reason_code) {
	case FAPI_REASONCODE_TDLS_INSUFFICIENT_TRAFFIC:
	case FAPI_REASONCODE_TDLS_NO_TRAFFIC:
		SLSI_NET_DBG1(dev, SLSI_MLME, "FAPI_REASONCODE : %x\n", reason_code);
		break;

	case FAPI_REASONCODE_TDLS_ROAMED:
		if (delayed_work_pending(&t_peer->tdls_peer_blocked_work))
			cancel_delayed_work_sync(&t_peer->tdls_peer_blocked_work);
		schedule_delayed_work(&t_peer->tdls_peer_blocked_work,
				      TDLS_PEER_BLOCKED_TIMEOUT);

		mutex_lock(&manager->state_transition_queue_mutex);
		enqueue_peer_state_transition(manager, t_peer, SLSI_TDLS_PEER_BLOCKED, NULL);
		mutex_unlock(&manager->state_transition_queue_mutex);
		SLSI_NET_DBG1(dev, SLSI_MLME, "FAPI_REASONCODE_TDLS_ROAMED: %x\n", reason_code);
		break;

	default:
		SLSI_NET_DBG1(dev, SLSI_MLME, "reason_code:%x\n", reason_code);
		break;
	}
}

static void slsi_tdls_manager_peer_state_transition_handler(struct work_struct *work)
{
	struct tdls_manager *manager = container_of(work, struct tdls_manager, peer_state_transition_manager);
	struct slsi_vif_sta *vif_sta = container_of(manager, struct slsi_vif_sta, tdls_manager);
	struct netdev_vif *ndev_vif = container_of(vif_sta, struct netdev_vif, sta);
	struct net_device *dev = (struct net_device *)((void *)ndev_vif - netdev_priv((struct net_device *)0));
	struct state_transition_request *entry;
	struct state_transition_request *tmp;
	struct sorted_peer_entry *entry_candidate;
	struct sorted_peer_entry *tmp_candidate;

	if (!manager->active)
		return;

	/**
	 * All state transition is handled by this work essentially to serialize
	 * state transition with holding state_transition_queue_mutex.
	 * Peer is removed from hash table when peer is in SLSI_TDLS_PEER_INACTIVE.
	 */
	rtnl_lock();
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	mutex_lock(&manager->state_transition_queue_mutex);

	list_for_each_entry_safe(entry, tmp, &manager->state_transition_queue, list) {
		int err = 0;
		switch (entry->next_state) {
		case SLSI_TDLS_PEER_DISCOVER:
			if (entry->peer->state == SLSI_TDLS_PEER_INACTIVE)
				break;
			entry->peer->state = SLSI_TDLS_PEER_DISCOVER;
			err = slsi_mlme_tdls_action(ndev_vif->sdev, dev, entry->peer->mac_addr,
						    FAPI_TDLSACTION_DISCOVERY);
			if (!err) {
				entry->peer->state = SLSI_TDLS_PEER_CFM_SUCCESS;
				if (delayed_work_pending(&entry->peer->tdls_peer_ind_timeout_work)) {
					mutex_unlock(&manager->state_transition_queue_mutex);
					cancel_delayed_work_sync(&entry->peer->tdls_peer_ind_timeout_work);
					mutex_lock(&manager->state_transition_queue_mutex);
				}
				schedule_delayed_work(&entry->peer->tdls_peer_ind_timeout_work,
						      TDLS_PEER_IND_TIMEOUT);
			} else if (err == -EOPNOTSUPP) {
				entry->peer->state = SLSI_TDLS_PEER_CFM_EOPNOTSUPP;
				enqueue_peer_state_transition(manager, entry->peer, SLSI_TDLS_PEER_INACTIVE, NULL);
			} else {
				entry->peer->state = SLSI_TDLS_PEER_CFM_EINVAL;
				enqueue_peer_state_transition(manager, entry->peer, SLSI_TDLS_PEER_INACTIVE, NULL);
			}
			break;

		case SLSI_TDLS_PEER_CANDIDATE_SETUP:
			err = slsi_mlme_tdls_action(ndev_vif->sdev, dev, entry->peer->mac_addr, FAPI_TDLSACTION_CANDIDATE_SETUP);
			if (list_empty(&ndev_vif->sta.tdls_candidate_setup_list)) {
				/* Delete virtual tdls_peer for processing the candidate setup list */
				kfree(entry->peer);
				break;
			}
			if (!err) {
				list_for_each_entry_safe(entry_candidate, tmp_candidate, &ndev_vif->sta.tdls_candidate_setup_list, list) {
					entry->peer->initiator = SLSI_TDLS_INITIATOR_DRIVER;
					entry_candidate->peer->state = SLSI_TDLS_PEER_CFM_SUCCESS;
					if (delayed_work_pending(&entry_candidate->peer->tdls_peer_ind_timeout_work)) {
						mutex_unlock(&manager->state_transition_queue_mutex);
						cancel_delayed_work_sync(&entry_candidate->peer->tdls_peer_ind_timeout_work);
						mutex_lock(&manager->state_transition_queue_mutex);
					}
					schedule_delayed_work(&entry_candidate->peer->tdls_peer_ind_timeout_work, TDLS_PEER_IND_TIMEOUT);
				}
			} else if (err == -EOPNOTSUPP) {
				list_for_each_entry_safe(entry_candidate, tmp_candidate, &ndev_vif->sta.tdls_candidate_setup_list, list) {
					if (entry_candidate->peer->state == SLSI_TDLS_PEER_INACTIVE)
						continue;
					entry_candidate->peer->state = SLSI_TDLS_PEER_CFM_EOPNOTSUPP;
					enqueue_peer_state_transition(manager, entry_candidate->peer, SLSI_TDLS_PEER_INACTIVE, NULL);
				}
			} else {
				list_for_each_entry_safe(entry_candidate, tmp_candidate, &ndev_vif->sta.tdls_candidate_setup_list, list) {
					if (entry_candidate->peer->state == SLSI_TDLS_PEER_INACTIVE)
						continue;
					entry_candidate->peer->state = SLSI_TDLS_PEER_CFM_EINVAL;
					enqueue_peer_state_transition(manager, entry_candidate->peer, SLSI_TDLS_PEER_INACTIVE, NULL);
				}
			}
			/* Delete virtual tdls_peer for processing the candidate setup list */
			kfree(entry->peer);
			break;

		case SLSI_TDLS_PEER_SETUP:
			if (entry->peer->state == SLSI_TDLS_PEER_INACTIVE)
				break;
			entry->peer->state = SLSI_TDLS_PEER_SETUP;
			err = slsi_mlme_tdls_action(ndev_vif->sdev, dev, entry->peer->mac_addr, FAPI_TDLSACTION_FORCED_SETUP);
			if (!err) {
				entry->peer->state = SLSI_TDLS_PEER_CFM_SUCCESS;
				if (delayed_work_pending(&entry->peer->tdls_peer_ind_timeout_work)) {
					mutex_unlock(&manager->state_transition_queue_mutex);
					cancel_delayed_work_sync(&entry->peer->tdls_peer_ind_timeout_work);
					mutex_lock(&manager->state_transition_queue_mutex);
				}
				schedule_delayed_work(&entry->peer->tdls_peer_ind_timeout_work,
						      TDLS_PEER_IND_TIMEOUT);
			} else if (err == -EOPNOTSUPP) {
				entry->peer->state = SLSI_TDLS_PEER_CFM_EOPNOTSUPP;
				enqueue_peer_state_transition(manager, entry->peer, SLSI_TDLS_PEER_INACTIVE, NULL);
			} else {
				entry->peer->state = SLSI_TDLS_PEER_CFM_EINVAL;
				enqueue_peer_state_transition(manager, entry->peer, SLSI_TDLS_PEER_INACTIVE, NULL);
			}
			break;

		case SLSI_TDLS_PEER_DISCOVERED_IND:
			if (entry->peer->state == SLSI_TDLS_PEER_INACTIVE)
				break;
			entry->peer->state = SLSI_TDLS_PEER_DISCOVERED_IND;
			if (delayed_work_pending(&entry->peer->tdls_peer_ind_timeout_work)) {
				mutex_unlock(&manager->state_transition_queue_mutex);
				cancel_delayed_work_sync(&entry->peer->tdls_peer_ind_timeout_work);
				mutex_lock(&manager->state_transition_queue_mutex);
			}
			enqueue_peer_state_transition(manager, entry->peer, SLSI_TDLS_PEER_SETUP, NULL);
			break;

		case SLSI_TDLS_PEER_CONNECTED_IND:
			if (entry->peer->state == SLSI_TDLS_PEER_INACTIVE)
				break;
			entry->peer->state = SLSI_TDLS_PEER_CONNECTED_IND;
			if (delayed_work_pending(&entry->peer->tdls_peer_ind_timeout_work)) {
				mutex_unlock(&manager->state_transition_queue_mutex);
				cancel_delayed_work_sync(&entry->peer->tdls_peer_ind_timeout_work);
				mutex_lock(&manager->state_transition_queue_mutex);
			}
			slsi_tdls_manager_connected_ind(manager, entry->peer,
							fapi_get_u16(entry->frame, u.mlme_tdls_peer_ind.flow_id));
			break;

		case SLSI_TDLS_PEER_DISCONNECTED_IND:
			if (entry->peer->state == SLSI_TDLS_PEER_INACTIVE)
				break;
			entry->peer->state = SLSI_TDLS_PEER_DISCONNECTED_IND;
			if (delayed_work_pending(&entry->peer->tdls_peer_ind_timeout_work)) {
				mutex_unlock(&manager->state_transition_queue_mutex);
				cancel_delayed_work_sync(&entry->peer->tdls_peer_ind_timeout_work);
				mutex_lock(&manager->state_transition_queue_mutex);
			}
			slsi_tdls_manager_disconnected_ind(manager, entry->peer,
							   fapi_get_u16(entry->frame,
									u.mlme_tdls_peer_ind.reason_code));
			if (entry->peer->state != SLSI_TDLS_PEER_BLOCKED)
				enqueue_peer_state_transition(manager, entry->peer, SLSI_TDLS_PEER_INACTIVE, NULL);
			break;

		case SLSI_TDLS_PEER_TEARDOWN:
			if (entry->peer->state == SLSI_TDLS_PEER_INACTIVE)
				break;
			entry->peer->state = SLSI_TDLS_PEER_TEARDOWN;
			err = slsi_mlme_tdls_action(ndev_vif->sdev, dev, entry->peer->mac_addr,
						    FAPI_TDLSACTION_TEARDOWN);
			if (!err) {
				entry->peer->state = SLSI_TDLS_PEER_CFM_SUCCESS;
				if (delayed_work_pending(&entry->peer->tdls_peer_ind_timeout_work)) {
					mutex_unlock(&manager->state_transition_queue_mutex);
					cancel_delayed_work_sync(&entry->peer->tdls_peer_ind_timeout_work);
					mutex_lock(&manager->state_transition_queue_mutex);
				}
				schedule_delayed_work(&entry->peer->tdls_peer_ind_timeout_work,
						      TDLS_PEER_IND_TIMEOUT);
			} else if (err == -EOPNOTSUPP) {
				entry->peer->state = SLSI_TDLS_PEER_CFM_EOPNOTSUPP;
			} else {
				entry->peer->state = SLSI_TDLS_PEER_CFM_EINVAL;
			}
			break;

		case SLSI_TDLS_PEER_CFM_EINVAL:
			SLSI_NET_DBG1(dev, SLSI_MLME, "Invalid state transition request: SLSI_TDLS_PEER_CFM_EINVAL\n");
			break;

		case SLSI_TDLS_PEER_CFM_EOPNOTSUPP:
			SLSI_NET_DBG1(dev, SLSI_MLME,
				      "Invalid state transition request: SLSI_TDLS_PEER_CFM_EOPNOTSUPP\n");
			break;

		case SLSI_TDLS_PEER_CFM_SUCCESS:
			SLSI_NET_DBG1(dev, SLSI_MLME, "Invalid state transition request: SLSI_TDLS_PEER_CFM_SUCCESS\n");
			break;

		case SLSI_TDLS_PEER_INACTIVE:
			if (delayed_work_pending(&entry->peer->tdls_peer_ind_timeout_work)) {
				mutex_unlock(&manager->state_transition_queue_mutex);
				cancel_delayed_work_sync(&entry->peer->tdls_peer_ind_timeout_work);
				mutex_lock(&manager->state_transition_queue_mutex);
			}
			slsi_tdls_manager_remove_candidate_list(ndev_vif, manager, entry->peer);
			spin_lock_bh(&manager->peer_hash_lock);
			hlist_del_init(&entry->peer->hlist);
			kfree(entry->peer);
			spin_unlock_bh(&manager->peer_hash_lock);
			break;

		default:
			SLSI_NET_DBG1(dev, SLSI_MLME, "Invalid state transition request: %d\n", entry->next_state);
			break;
		}

		list_del(&entry->list);
		consume_skb(entry->frame);
		kfree(entry);
	}
	mutex_unlock(&manager->state_transition_queue_mutex);
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	rtnl_unlock();
}

static void slsi_tdls_manager_active_peer_mgmt(struct work_struct *work)
{
	u8 i = 0;
	u8 ac = 0;
	struct delayed_work *delayed_work_instance = container_of(work, struct delayed_work, work);
	struct tdls_manager *manager = container_of(delayed_work_instance, struct tdls_manager, active_peer_manager);
	struct slsi_vif_sta *vif_sta = container_of(manager, struct slsi_vif_sta, tdls_manager);
	struct netdev_vif *ndev_vif = container_of(vif_sta, struct netdev_vif, sta);
	struct net_device *dev = (struct net_device *)((void *)ndev_vif - netdev_priv((struct net_device *)0));
	struct tdls_peer *t_peer = NULL;
	int tdls_peers_in_transient_state = 0;
	int tdls_peers_in_connected_state = 0;
	int available_tdls_slots = 0;
	u8 changed_list = 0;
	ktime_t now = ktime_get();

	LIST_HEAD(tdls_candidate_peer_list);
	LIST_HEAD(tdls_inactive_peer_list);

	if (!tdls_supported_by_fw)
		return;
	if (!manager->active)
		return;

	mutex_lock(&manager->state_transition_queue_mutex);
	spin_lock_bh(&manager->peer_hash_lock);

	if (ether_addr_equal(ndev_vif->sta.tdls_dgw_macaddr, broadcast_mac))
		slsi_tdls_manager_address_changed(dev);

	for (i = 0 ; i < TDLS_PEER_HASH_TABLE_SIZE ; i++) {
		if (hlist_empty(&manager->peer_hash_table[i]))
			continue;
		hlist_for_each_entry(t_peer, &manager->peer_hash_table[i], hlist) {
			struct sorted_peer_entry *entry;

			switch (t_peer->state) {
			case SLSI_TDLS_PEER_ACTIVE:
				t_peer->tdls_weight = 0;
				for (ac = 0 ; ac < SLSI_TDLS_TX_AC ; ac++) {
					if (t_peer->tx_packets[ac].count)
						t_peer->no_traffic_count = 0;
					if (t_peer->tx_packets[ac].count >= tdls_manager_discovery_threshold) {
						if (t_peer->tx_packets[ac].ischecked || (now - t_peer->tx_packets[ac].last_sent) < SLSI_TDLS_MAX_TIME_NS_AFTER_LAST_TX) {
							if (t_peer->tdls_weight < t_peer->tx_packets[ac].count)
								t_peer->tdls_weight = t_peer->tx_packets[ac].count;
						} else {
							t_peer->tx_packets[ac].ischecked = true;
						}
					} else {
						t_peer->tx_packets[ac].ischecked = false;
					}
					t_peer->tx_packets[ac].count = 0;
				}
				if (t_peer->tdls_weight >= tdls_manager_discovery_threshold) {
					t_peer->tdls_weight = 0;
					entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
					if (entry) {
						entry->peer = t_peer;
						INIT_LIST_HEAD(&entry->list);
						list_add_tail(&entry->list, &tdls_candidate_peer_list);
					}
				}
				if (++t_peer->no_traffic_count > SLSI_TDLS_NO_TRAFFIC_MAX_COUNT) {
					t_peer->no_traffic_count = 0;
					entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
					if (entry) {
						entry->peer = t_peer;
						INIT_LIST_HEAD(&entry->list);
						list_add_tail(&entry->list, &tdls_inactive_peer_list);
					}
				}
			break;
			case SLSI_TDLS_PEER_INACTIVE:
			case SLSI_TDLS_PEER_BLOCKED:
			break;
			case SLSI_TDLS_PEER_CONNECTED_IND:
				tdls_peers_in_connected_state++;
			break;
			default:
				tdls_peers_in_transient_state++;
			break;
			}
		}
	}
	available_tdls_slots = SLSI_TDLS_MAX_PEERS - tdls_peers_in_transient_state - tdls_peers_in_connected_state;
	spin_unlock_bh(&manager->peer_hash_lock);

	if (!list_empty(&tdls_candidate_peer_list)) {
		struct sorted_peer_entry *entry;
		struct sorted_peer_entry *tmp;

		list_sort(NULL, &tdls_candidate_peer_list, slsi_tdls_manager_peer_sort);

		list_for_each_entry_safe(entry, tmp, &tdls_candidate_peer_list, list) {
			if (available_tdls_slots > 0) {
				if (slsi_tdls_manager_add_peer_to_candidate_list(ndev_vif, manager, entry->peer)) {
					changed_list = 1;
					SLSI_NET_DBG1(dev, SLSI_TDLS, "Add peer:%pM to candidate list\n", entry->peer->mac_addr);
					available_tdls_slots--;
				}
			}
			list_del(&entry->list);
			kfree(entry);
		}
	}

	if (!list_empty(&tdls_inactive_peer_list)) {
		struct sorted_peer_entry *entry;
		struct sorted_peer_entry *tmp;

		list_for_each_entry_safe(entry, tmp, &tdls_inactive_peer_list, list) {
			if (slsi_tdls_manager_remove_candidate_list(ndev_vif, manager, entry->peer)) {
				changed_list = 1;
				SLSI_NET_DBG1(dev, SLSI_TDLS, "Delete peer:%pM from candidate list\n", t_peer->mac_addr);
			}
			enqueue_peer_state_transition(manager, entry->peer, SLSI_TDLS_PEER_INACTIVE, NULL);
			list_del(&entry->list);
			kfree(entry);
		}
	}

	/* Create a virtual tdls_peer to process the candidate setup list */
	if (changed_list) {
		t_peer = kmalloc(sizeof(*t_peer), GFP_KERNEL);
		if (t_peer) {
			t_peer->dev = dev;
			t_peer->state = SLSI_TDLS_PEER_ACTIVE;
			t_peer->manager = manager;
			ether_addr_copy(t_peer->mac_addr, broadcast_mac);
			SLSI_NET_DBG1(dev, SLSI_TDLS, "Try to TDLS Candidate Setup for %d peers\n", ndev_vif->sta.tdls_candidate_setup_count);
			enqueue_peer_state_transition(manager, t_peer, SLSI_TDLS_PEER_CANDIDATE_SETUP, NULL);
		}
	}

	if (ndev_vif->sta.tdls_manager.active)
		schedule_delayed_work(&manager->active_peer_manager, msecs_to_jiffies(1000));
	mutex_unlock(&manager->state_transition_queue_mutex);
}

static struct tdls_peer *slsi_tdls_manager_init_peer(struct net_device *dev,
						     const u8 *mac,
						     struct tdls_manager *manager)
{
	u32 hash = 0;
	u8 i = 0;
	struct hlist_head *bucket = NULL;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tdls_peer *t_peer = NULL;

	if (WLBT_WARN(!ndev_vif->activated, "VIF Not Activated"))
		return NULL;

	if (!ndev_vif->sta.tdls_manager.active)
		return NULL;

	t_peer = kmalloc(sizeof(*t_peer), GFP_ATOMIC);
	if (WLBT_WARN(!t_peer, "TDLS Peer Not available"))
		return NULL;

	ether_addr_copy(t_peer->mac_addr, mac);
	for (i = 0 ; i < SLSI_TDLS_TX_AC ; i++) {
		t_peer->tx_packets[i].count = 0;
		t_peer->tx_packets[i].last_sent = 0;
		t_peer->tx_packets[i].ischecked = false;
	}

	t_peer->dev = dev;
	t_peer->no_traffic_count = 0;
	t_peer->tdls_weight = 0;
	t_peer->state = SLSI_TDLS_PEER_ACTIVE;
	t_peer->initiator = SLSI_TDLS_INITIATOR_UNKNOWN;
	SLSI_NET_DBG1(dev, SLSI_MLME, "peer mac_addr: %pM\n", t_peer->mac_addr);

	INIT_DELAYED_WORK(&t_peer->tdls_peer_ind_timeout_work, slsi_tdls_manager_indication_timeout);
	INIT_DELAYED_WORK(&t_peer->tdls_peer_blocked_work, slsi_tdls_manager_peer_blocked_timeout);

	t_peer->manager = manager;

	hash = jhash(t_peer->mac_addr, ETH_ALEN, 0);
	bucket = &ndev_vif->sta.tdls_manager.peer_hash_table[hash & (TDLS_PEER_HASH_TABLE_SIZE - 1)];

	spin_lock_bh(&ndev_vif->sta.tdls_manager.peer_hash_lock);
	hlist_add_head(&t_peer->hlist, bucket);
	spin_unlock_bh(&ndev_vif->sta.tdls_manager.peer_hash_lock);

	return t_peer;
}

void slsi_tdls_manager_event_tx(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb, u8 ac)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_skb_cb *cb;
	u8 *peer_mac;
	struct tdls_peer *t_peer;

	if (ndev_vif->vif_type != FAPI_VIFTYPE_STATION)
		return;
	if (!tdls_supported_by_fw)
		return;
	if (!ndev_vif->sta.tdls_manager.active)
		return;
	if (!ndev_vif->sta.sta_bss)
		return;
	if (!skb_mac_header_was_set(skb))
		return;

	peer_mac = eth_hdr(skb)->h_dest;
	/* Ignore broadcast / multicast packets. */
	if (peer_mac[0] & 0x1)
		return;
	if (ether_addr_equal(peer_mac, ndev_vif->sta.sta_bss->bssid))
		return;
	if (ether_addr_equal(peer_mac, ndev_vif->sta.tdls_dgw_macaddr))
		return;

	cb = slsi_skb_cb_get(skb);
	spin_lock_bh(&ndev_vif->sta.tdls_manager.peer_hash_lock);

	t_peer = slsi_tdls_manager_find_peer(dev, &ndev_vif->sta.tdls_manager, peer_mac);
	if (t_peer) {
		t_peer->tx_packets[ac].count++;
		t_peer->tx_packets[ac].last_sent = ktime_get();
		spin_unlock_bh(&ndev_vif->sta.tdls_manager.peer_hash_lock);
		return;
	}

	spin_unlock_bh(&ndev_vif->sta.tdls_manager.peer_hash_lock);

	t_peer = slsi_tdls_manager_init_peer(dev, peer_mac, &ndev_vif->sta.tdls_manager);
	if (!t_peer)
		return;

	t_peer->tx_packets[ac].count++;
	t_peer->tx_packets[ac].last_sent = ktime_get();
}

int slsi_tdls_manager_on_vif_activated(struct slsi_dev *sdev, struct net_device *dev, struct netdev_vif *ndev_vif)
{
	u8 i;
	int err = 0;

	err = slsi_mib_get_sta_tdls_activated(sdev, dev, &tdls_supported_by_fw);
	if (err) {
		SLSI_NET_ERR(dev, "FW doesn't activate TDLS, check FW mib status\n");
		return 0;
	}
	slsi_mib_get_sta_tdls_max_peer(sdev, dev, ndev_vif);
	SLSI_NET_DBG1(dev, SLSI_MLME, "tdls support: %s\n", tdls_supported_by_fw?"support":"not support");
	if (ndev_vif->vif_type != FAPI_VIFTYPE_STATION)
		return 0;

	if (ndev_vif->sta.tdls_manager.active)
		slsi_tdls_manager_on_vif_deactivated(sdev, dev, ndev_vif);
	spin_lock_init(&ndev_vif->sta.tdls_manager.peer_hash_lock);

	for (i = 0 ; i < TDLS_PEER_HASH_TABLE_SIZE ; i++)
		INIT_HLIST_HEAD(&ndev_vif->sta.tdls_manager.peer_hash_table[i]);

	INIT_LIST_HEAD(&ndev_vif->sta.tdls_manager.state_transition_queue);

	mutex_init(&ndev_vif->sta.tdls_manager.state_transition_queue_mutex);

	INIT_WORK(&ndev_vif->sta.tdls_manager.peer_state_transition_manager,
		slsi_tdls_manager_peer_state_transition_handler);

	INIT_DELAYED_WORK(&ndev_vif->sta.tdls_manager.active_peer_manager,
		slsi_tdls_manager_active_peer_mgmt);

	ndev_vif->sta.tdls_manager.active = 1;
	SLSI_ETHER_COPY(ndev_vif->sta.tdls_dgw_macaddr, broadcast_mac);

	schedule_delayed_work(&ndev_vif->sta.tdls_manager.active_peer_manager, msecs_to_jiffies(1000));

	return 0;
}

void slsi_tdls_manager_on_vif_deactivated(struct slsi_dev *sdev, struct net_device *dev, struct netdev_vif *ndev_vif)
{
	u8 i;
	struct tdls_peer *t_peer;

	if (ndev_vif->vif_type != FAPI_VIFTYPE_STATION)
		return;
	if (!ndev_vif->sta.tdls_manager.active)
		return;
	ndev_vif->sta.tdls_manager.active = 0;
	SLSI_ETHER_COPY(ndev_vif->sta.tdls_dgw_macaddr, broadcast_mac);

	if (delayed_work_pending(&ndev_vif->sta.tdls_manager.active_peer_manager))
		cancel_delayed_work_sync(&ndev_vif->sta.tdls_manager.active_peer_manager);

	if (work_pending(&ndev_vif->sta.tdls_manager.peer_state_transition_manager))
		cancel_work_sync(&ndev_vif->sta.tdls_manager.peer_state_transition_manager);

	mutex_lock(&ndev_vif->sta.tdls_manager.state_transition_queue_mutex);
	if (!list_empty(&ndev_vif->sta.tdls_manager.state_transition_queue)) {
		struct state_transition_request *entry;
		struct state_transition_request *tmp;

		list_for_each_entry_safe(entry, tmp, &ndev_vif->sta.tdls_manager.state_transition_queue, list) {
			list_del(&entry->list);
			kfree(entry);
		}
	}
	mutex_unlock(&ndev_vif->sta.tdls_manager.state_transition_queue_mutex);

	for (i = 0 ; i < TDLS_PEER_HASH_TABLE_SIZE ; i++) {
		struct hlist_node *n;

		hlist_for_each_entry_safe(t_peer, n, &ndev_vif->sta.tdls_manager.peer_hash_table[i], hlist) {
			if (delayed_work_pending(&t_peer->tdls_peer_ind_timeout_work))
				cancel_delayed_work_sync(&t_peer->tdls_peer_ind_timeout_work);
			if (delayed_work_pending(&t_peer->tdls_peer_blocked_work))
				cancel_delayed_work_sync(&t_peer->tdls_peer_blocked_work);
			hlist_del_init(&t_peer->hlist);
			kfree(t_peer);
		}
	}

	tdls_supported_by_fw = false;
}

void slsi_tdls_manager_event_discovered(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	u8 *peer_mac = fapi_get_buff(skb, u.mlme_tdls_peer_ind.peer_sta_address);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tdls_peer *t_peer;

	mutex_lock(&ndev_vif->sta.tdls_manager.state_transition_queue_mutex);

	spin_lock_bh(&ndev_vif->sta.tdls_manager.peer_hash_lock);
	t_peer = slsi_tdls_manager_find_peer(dev, &ndev_vif->sta.tdls_manager, peer_mac);
	spin_unlock_bh(&ndev_vif->sta.tdls_manager.peer_hash_lock);

	if (t_peer) {
		enqueue_peer_state_transition(&ndev_vif->sta.tdls_manager,
					      t_peer, SLSI_TDLS_PEER_DISCOVERED_IND, skb);
		SLSI_NET_DBG1(dev, SLSI_MLME, "peer mac_addr: %pM\n", t_peer->mac_addr);
	} else {
		SLSI_NET_DBG1(dev, SLSI_MLME, "No peer info. Invalid discovery event.\n");
	}

	mutex_unlock(&ndev_vif->sta.tdls_manager.state_transition_queue_mutex);
}

void slsi_tdls_manager_event_connected(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct tdls_peer *t_peer = NULL;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u8 *peer_mac = fapi_get_buff(skb, u.mlme_tdls_peer_ind.peer_sta_address);
	struct tdls_manager *manager = NULL;

	mutex_lock(&ndev_vif->sta.tdls_manager.state_transition_queue_mutex);

	spin_lock_bh(&ndev_vif->sta.tdls_manager.peer_hash_lock);
	t_peer = slsi_tdls_manager_find_peer(dev, &ndev_vif->sta.tdls_manager, peer_mac);
	spin_unlock_bh(&ndev_vif->sta.tdls_manager.peer_hash_lock);

	/* In case of a remote peer intiated */
	if (!t_peer) {
		t_peer = slsi_tdls_manager_init_peer(dev, peer_mac, &ndev_vif->sta.tdls_manager);
		if (!t_peer) {
			mutex_unlock(&ndev_vif->sta.tdls_manager.state_transition_queue_mutex);
			return;
		}
		t_peer->initiator = SLSI_TDLS_INITIATOR_REMOTE_PEER;
		SLSI_NET_DBG1(dev, SLSI_MLME, "Peer initiated TDLS Setup has started.\n");
		if (delayed_work_pending(&t_peer->tdls_peer_blocked_work)) {
			mutex_unlock(&ndev_vif->sta.tdls_manager.state_transition_queue_mutex);
			cancel_delayed_work_sync(&t_peer->tdls_peer_blocked_work);
			mutex_lock(&ndev_vif->sta.tdls_manager.state_transition_queue_mutex);
		}
	}
	manager = t_peer->manager;
	slsi_tdls_manager_remove_candidate_list(ndev_vif, manager, t_peer);
	SLSI_NET_DBG1(dev, SLSI_MLME, "peer mac_addr: %pM\n", t_peer->mac_addr);

	/* In case of that peer connect event received on existing connection */
	if (t_peer->state != SLSI_TDLS_PEER_CONNECTED_IND)
		enqueue_peer_state_transition(&ndev_vif->sta.tdls_manager, t_peer, SLSI_TDLS_PEER_CONNECTED_IND, skb);

	mutex_unlock(&ndev_vif->sta.tdls_manager.state_transition_queue_mutex);
}

void slsi_tdls_manager_event_disconnected(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct tdls_peer *t_peer = NULL;
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u8 *peer_mac = fapi_get_buff(skb, u.mlme_tdls_peer_ind.peer_sta_address);

	mutex_lock(&ndev_vif->sta.tdls_manager.state_transition_queue_mutex);

	spin_lock_bh(&ndev_vif->sta.tdls_manager.peer_hash_lock);
	t_peer = slsi_tdls_manager_find_peer(dev, &ndev_vif->sta.tdls_manager, peer_mac);
	spin_unlock_bh(&ndev_vif->sta.tdls_manager.peer_hash_lock);

	if (t_peer) {
		SLSI_NET_DBG1(dev, SLSI_MLME, "peer mac_addr: %pM\n", t_peer->mac_addr);
		enqueue_peer_state_transition(&ndev_vif->sta.tdls_manager, t_peer, SLSI_TDLS_PEER_DISCONNECTED_IND,
					      skb);
	}

	mutex_unlock(&ndev_vif->sta.tdls_manager.state_transition_queue_mutex);
}

int slsi_tdls_manager_oper(struct wiphy *wiphy, struct net_device *dev, const u8 *peer,
			   enum nl80211_tdls_operation oper)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct tdls_peer *t_peer;
	int err = 0;

	SLSI_NET_DBG3(dev, SLSI_CFG80211, "oper:%d, vif_type:%d, vif_index:%d\n", oper, ndev_vif->vif_type,
		      ndev_vif->ifnum);

	if (!(wiphy->flags & WIPHY_FLAG_SUPPORTS_TDLS))
		return -EOPNOTSUPP;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	if (!ndev_vif->activated || SLSI_IS_VIF_INDEX_P2P_GROUP(sdev, ndev_vif) ||
	    ndev_vif->sta.vif_status != SLSI_VIF_STATUS_CONNECTED) {
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
		return -EOPNOTSUPP;
	}

	mutex_lock(&ndev_vif->sta.tdls_manager.state_transition_queue_mutex);
	spin_lock_bh(&ndev_vif->sta.tdls_manager.peer_hash_lock);
	t_peer = slsi_tdls_manager_find_peer(dev, &ndev_vif->sta.tdls_manager, peer);
	spin_unlock_bh(&ndev_vif->sta.tdls_manager.peer_hash_lock);

	switch (oper) {
	case NL80211_TDLS_DISCOVERY_REQ:
		if (!t_peer) {
			t_peer = slsi_tdls_manager_init_peer(dev, peer, &ndev_vif->sta.tdls_manager);
			if (!t_peer) {
				err = -ENOMEM;
				goto exit_slsi_tdls_manager_oper;
			}
		}
		t_peer->initiator = SLSI_TDLS_INITIATOR_FRWK;
		enqueue_peer_state_transition(&ndev_vif->sta.tdls_manager, t_peer, SLSI_TDLS_PEER_DISCOVER, NULL);
	break;

	case NL80211_TDLS_SETUP:
		if (!t_peer) {
			t_peer = slsi_tdls_manager_init_peer(dev, peer, &ndev_vif->sta.tdls_manager);
			if (!t_peer) {
				err = -ENOMEM;
				goto exit_slsi_tdls_manager_oper;
			}
		}
		t_peer->initiator = SLSI_TDLS_INITIATOR_FRWK;
		enqueue_peer_state_transition(&ndev_vif->sta.tdls_manager, t_peer, SLSI_TDLS_PEER_SETUP, NULL);
	break;

	case NL80211_TDLS_TEARDOWN:
		if (!t_peer) {
			err = -EINVAL;
			goto exit_slsi_tdls_manager_oper;
		}
		enqueue_peer_state_transition(&ndev_vif->sta.tdls_manager, t_peer, SLSI_TDLS_PEER_TEARDOWN, NULL);
	break;

	default:
		err = -EOPNOTSUPP;
	break;
	}

exit_slsi_tdls_manager_oper:
	mutex_unlock(&ndev_vif->sta.tdls_manager.state_transition_queue_mutex);
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return err;
}
