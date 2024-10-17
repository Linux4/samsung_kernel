// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-txbp.h"
#include "kunit-mock-netif.h"
#include "kunit-mock-mlme.h"
#include "../tdls_manager.c"


/* unit test function definition */
static void test_slsi_tdls_manager_address_changed(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	slsi_tdls_manager_address_changed(dev);
}

static void test_slsi_tdls_manager_add_peer_to_candidate_list(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tdls_manager *manager = kunit_kzalloc(test, sizeof(struct tdls_manager), GFP_KERNEL);
	struct tdls_peer *exist_peer = kunit_kzalloc(test, sizeof(struct tdls_peer), GFP_KERNEL);
	struct tdls_peer *new_peer = kunit_kzalloc(test, sizeof(struct tdls_peer), GFP_KERNEL);
	struct sorted_peer_entry *entry;
	struct sorted_peer_entry *tmp;

	INIT_LIST_HEAD(&ndev_vif->sta.tdls_candidate_setup_list);
	entry = kunit_kzalloc(test, sizeof(struct sorted_peer_entry), GFP_KERNEL);
	entry->peer = exist_peer;
	INIT_LIST_HEAD(&entry->list);
	list_add_tail(&entry->list, &ndev_vif->sta.tdls_candidate_setup_list);

	SLSI_ETHER_COPY(new_peer->mac_addr, SLSI_DEFAULT_HW_MAC_ADDR);
	KUNIT_EXPECT_EQ(test, 1, slsi_tdls_manager_add_peer_to_candidate_list(ndev_vif, manager, new_peer));

	list_for_each_entry_safe(entry, tmp, &ndev_vif->sta.tdls_candidate_setup_list, list) {
		if (ether_addr_equal(new_peer->mac_addr, entry->peer->mac_addr)) {
			list_del(&entry->list);
			kfree(entry);
		}
	}
}

static void test_slsi_tdls_manager_remove_candidate_list(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tdls_manager *manager = kunit_kzalloc(test, sizeof(struct tdls_manager), GFP_KERNEL);
	struct tdls_peer *peer = kunit_kzalloc(test, sizeof(struct tdls_peer), GFP_KERNEL);
	struct sorted_peer_entry *entry;

	INIT_LIST_HEAD(&ndev_vif->sta.tdls_candidate_setup_list);
	SLSI_ETHER_COPY(peer->mac_addr, SLSI_DEFAULT_HW_MAC_ADDR);
	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	entry->peer = peer;
	INIT_LIST_HEAD(&entry->list);
	list_add_tail(&entry->list, &ndev_vif->sta.tdls_candidate_setup_list);

	KUNIT_EXPECT_EQ(test, 1, slsi_tdls_manager_remove_candidate_list(ndev_vif, manager, peer));
}

static void test_slsi_lock_tdls_tcp_ack_lock(struct kunit *test)
{
	struct netdev_vif *ndev_vif = kunit_kzalloc(test, sizeof(struct netdev_vif), GFP_KERNEL);
	struct slsi_spinlock *peer_lock = kunit_kzalloc(test, sizeof(struct slsi_spinlock), GFP_KERNEL);
	struct slsi_spinlock *tcp_ack_lock = kunit_kzalloc(test, sizeof(struct slsi_spinlock), GFP_KERNEL);
	ndev_vif->peer_lock = *peer_lock;
	ndev_vif->tcp_ack_lock = *tcp_ack_lock;

	slsi_lock_tdls_tcp_ack_lock(ndev_vif);
	slsi_lock_tdls_tcp_ack_unlock(ndev_vif);
}

static void test_slsi_lock_tdls_tcp_ack_unlock(struct kunit *test)
{
	struct netdev_vif *ndev_vif = kunit_kzalloc(test, sizeof(struct netdev_vif), GFP_KERNEL);
	struct slsi_spinlock *peer_lock = kunit_kzalloc(test, sizeof(struct slsi_spinlock), GFP_KERNEL);
	struct slsi_spinlock *tcp_ack_lock = kunit_kzalloc(test, sizeof(struct slsi_spinlock), GFP_KERNEL);
	ndev_vif->peer_lock = *peer_lock;
	ndev_vif->tcp_ack_lock = *tcp_ack_lock;

	slsi_lock_tdls_tcp_ack_lock(ndev_vif);
	slsi_lock_tdls_tcp_ack_unlock(ndev_vif);
}

static void test_slsi_tdls_manager_find_peer(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct tdls_manager *manager = kunit_kzalloc(test, sizeof(struct tdls_manager), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u8 mac[ETH_ALEN] = {0};

	ndev_vif->activated = true;
	ndev_vif->sta.tdls_manager.active = 1;

	slsi_tdls_manager_find_peer(dev, manager, mac);
}

static void test_slsi_tdls_manager_peer_sort(struct kunit *test)
{
	struct sorted_peer_entry *peer_a = kunit_kzalloc(test, sizeof(struct sorted_peer_entry), GFP_KERNEL);
	struct sorted_peer_entry *peer_b = kunit_kzalloc(test, sizeof(struct sorted_peer_entry), GFP_KERNEL);

	peer_a->peer = kunit_kzalloc(test, sizeof(struct tdls_peer), GFP_KERNEL);
	peer_b->peer = kunit_kzalloc(test, sizeof(struct tdls_peer), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 1, slsi_tdls_manager_peer_sort(NULL, &peer_a->list, &peer_b->list));
}

static void test_enqueue_peer_state_transition_internal(struct kunit *test)
{
	struct tdls_manager *manager = kunit_kzalloc(test, sizeof(struct tdls_manager), GFP_KERNEL);
	struct tdls_peer *peer = kunit_kzalloc(test, sizeof(struct tdls_peer), GFP_KERNEL);
	struct netdev_vif *ndev_vif;
	enum tdls_peer_state next_state = SLSI_TDLS_PEER_INACTIVE;
	struct sk_buff *frame = alloc_skb(1, GFP_KERNEL);
	struct state_transition_request *entry;
	struct state_transition_request *next;

	peer->dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	ndev_vif = netdev_priv(peer->dev);

	manager->active = 1;
	ndev_vif->activated = true;
	ndev_vif->sta.tdls_manager.active = 1;

	INIT_LIST_HEAD(&manager->state_transition_queue);
	enqueue_peer_state_transition_internal(manager, peer, next_state, frame);

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		list_del(&entry->list);
		if (entry->frame)
			consume_skb(entry->frame);
		kfree(entry);
	}
}

static void test_enqueue_peer_state_transition(struct kunit *test)
{
	struct tdls_manager *manager = kunit_kzalloc(test, sizeof(struct tdls_manager), GFP_KERNEL);
	struct tdls_peer *peer = kunit_kzalloc(test, sizeof(struct tdls_peer), GFP_KERNEL);
	struct netdev_vif *ndev_vif;
	enum tdls_peer_state next_state = SLSI_TDLS_PEER_INACTIVE;
	struct sk_buff *frame = alloc_skb(1, GFP_KERNEL);
	struct state_transition_request *entry;
	struct state_transition_request *next;

	peer->dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	ndev_vif = netdev_priv(peer->dev);

	manager->active = 1;
	ndev_vif->activated = true;
	ndev_vif->sta.tdls_manager.active = 1;
	INIT_LIST_HEAD(&manager->state_transition_queue);

	enqueue_peer_state_transition(manager, peer, next_state, frame);

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		list_del(&entry->list);
		if (entry->frame)
			consume_skb(entry->frame);
		kfree(entry);
	}
}

static void test_slsi_tdls_manager_indication_timeout(struct kunit *test)
{
	struct tdls_peer *peer = kunit_kzalloc(test, sizeof(struct tdls_peer), GFP_KERNEL);
	struct sorted_peer_entry *entry;

	struct delayed_work *delayed_work_instance = &peer->tdls_peer_ind_timeout_work;
	struct work_struct *work = &delayed_work_instance->work;
	struct netdev_vif *ndev_vif;
	struct slsi_vif_sta *vif_sta;

	peer->dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	ndev_vif = netdev_priv(peer->dev);
	ndev_vif->sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	ndev_vif->sdev->netdev[SLSI_NET_INDEX_WLAN] = peer->dev;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->activated = true;
	ndev_vif->sta.tdls_manager.active = 0;
	vif_sta = &ndev_vif->sta;
	peer->manager = &vif_sta->tdls_manager;

	INIT_LIST_HEAD(&ndev_vif->sta.tdls_candidate_setup_list);
	SLSI_ETHER_COPY(peer->mac_addr, SLSI_DEFAULT_HW_MAC_ADDR);
	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	entry->peer = peer;
	INIT_LIST_HEAD(&entry->list);
	list_add_tail(&entry->list, &ndev_vif->sta.tdls_candidate_setup_list);

	slsi_tdls_manager_indication_timeout(work);
}

static void test_slsi_tdls_manager_peer_blocked_timeout(struct kunit *test)
{
	struct tdls_peer *t_peer = kunit_kzalloc(test, sizeof(struct tdls_peer), GFP_KERNEL);
	struct delayed_work *delayed_work_instance = &t_peer->tdls_peer_blocked_work;
	struct work_struct *work = &delayed_work_instance->work;
	struct netdev_vif *ndev_vif;
	struct state_transition_request *entry;
	struct state_transition_request *next;

	t_peer->manager = kunit_kzalloc(test, sizeof(struct tdls_manager), GFP_KERNEL);
	t_peer->state = SLSI_TDLS_PEER_BLOCKED;
	t_peer->manager->active = 1;
	t_peer->dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);

	ndev_vif = netdev_priv(t_peer->dev);
	ndev_vif->activated = true;
	ndev_vif->sta.tdls_manager.active = 1;
	INIT_LIST_HEAD(&t_peer->manager->state_transition_queue);

	slsi_tdls_manager_peer_blocked_timeout(&t_peer->tdls_peer_blocked_work);

	list_for_each_entry_safe(entry, next, &t_peer->manager->state_transition_queue, list) {
		list_del(&entry->list);
		if (entry->frame)
			consume_skb(entry->frame);
		kfree(entry);
	}
}

static void test_slsi_tdls_manager_connected_ind(struct kunit *test)
{
	struct tdls_peer *t_peer = kunit_kzalloc(test, sizeof(struct tdls_peer), GFP_KERNEL);
	struct netdev_vif *ndev_vif;
	struct tdls_manager *manager;
	u16 flow_id = 0;

	t_peer->dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	ndev_vif = netdev_priv(t_peer->dev);
	manager = &ndev_vif->sta.tdls_manager;
	ndev_vif->ifnum = 0;
	ndev_vif->sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	ndev_vif->sdev->netdev[ndev_vif->ifnum] = t_peer->dev;

	slsi_tdls_manager_connected_ind(manager, t_peer, flow_id);

	ndev_vif->activated = true;
	slsi_tdls_manager_connected_ind(manager, t_peer, flow_id);

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	slsi_tdls_manager_connected_ind(manager, t_peer, flow_id);

	flow_id = 1024;
	slsi_tdls_manager_connected_ind(manager, t_peer, flow_id);

	sprintf(t_peer->mac_addr, "000000");
	ndev_vif->peer_sta_record[0] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);

	slsi_tdls_manager_connected_ind(manager, t_peer, flow_id);

	ndev_vif->sta.tdls_max_peer = SLSI_TDLS_MAX_PEERS;
	slsi_tdls_manager_connected_ind(manager, t_peer, flow_id);
}

static void test_slsi_tdls_manager_disconnected_ind(struct kunit *test)
{
	struct tdls_peer *t_peer = kunit_kzalloc(test, sizeof(struct tdls_peer), GFP_KERNEL);
	struct netdev_vif *ndev_vif;
	struct slsi_vif_sta *vif_sta;
	struct tdls_manager *manager;
	struct state_transition_request *entry;
	struct state_transition_request *next;
	u16 reason_code = FAPI_REASONCODE_TDLS_NO_TRAFFIC;

	t_peer->dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	ndev_vif = netdev_priv(t_peer->dev);
	vif_sta = &ndev_vif->sta;
	manager = &vif_sta->tdls_manager;

	slsi_tdls_manager_disconnected_ind(manager, t_peer, reason_code);

	ndev_vif->activated = true;
	ndev_vif->sta.tdls_enabled = true;
	ndev_vif->sta.tdls_manager.active = 1;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->peer_sta_record[1] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	sprintf(ndev_vif->peer_sta_record[1]->address, t_peer->mac_addr);
	ndev_vif->peer_sta_record[1]->valid = true;
	ndev_vif->peer_sta_record[1]->aid = 1;
	slsi_tdls_manager_disconnected_ind(manager, t_peer, reason_code);

	reason_code = FAPI_REASONCODE_TDLS_ROAMED;
	manager->active = 1;
	INIT_LIST_HEAD(&manager->state_transition_queue);
	slsi_tdls_manager_disconnected_ind(manager, t_peer, reason_code);

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		list_del(&entry->list);
		if (entry->frame)
			consume_skb(entry->frame);
		kfree(entry);
	}

	reason_code = FAPI_REASONCODE_UNKNOWN;
	slsi_tdls_manager_disconnected_ind(manager, t_peer, reason_code);

	ndev_vif->vif_type = FAPI_VIFTYPE_NAN;
	ndev_vif->ifnum = SLSI_NET_INDEX_NAN;
	slsi_tdls_manager_disconnected_ind(manager, t_peer, reason_code);
}

static void test_slsi_tdls_manager_peer_state_transition_handler(struct kunit *test)
{
	struct tdls_peer *peer = kunit_kzalloc(test, sizeof(struct tdls_peer), GFP_KERNEL);
	struct netdev_vif *ndev_vif;
	struct slsi_vif_sta *vif_sta;
	struct tdls_manager *manager;
	struct sk_buff *frame = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, 0, 100);
	struct state_transition_request *entry;
	struct state_transition_request *next;
	u8 mac[ETH_ALEN] = {0};

	peer->dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	ndev_vif = netdev_priv(peer->dev);
	vif_sta = &ndev_vif->sta;
	manager = &vif_sta->tdls_manager;

	ndev_vif->ifnum = 0;
	ndev_vif->sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	ndev_vif->sdev->netdev[ndev_vif->ifnum] = peer->dev;
	ndev_vif->activated = true;

	manager->active = 1;
	INIT_LIST_HEAD(&manager->state_transition_queue);
	INIT_LIST_HEAD(&vif_sta->tdls_candidate_setup_list);

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	eth_random_addr(peer->mac_addr);
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_DISCOVER, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	eth_broadcast_addr(peer->mac_addr);
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_DISCOVER, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	eth_zero_addr(peer->mac_addr);
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_DISCOVER, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	eth_random_addr(peer->mac_addr);
	slsi_tdls_manager_add_peer_to_candidate_list(ndev_vif, manager, peer);
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_CANDIDATE_SETUP, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}
	slsi_tdls_manager_remove_candidate_list(ndev_vif, manager, peer);

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	eth_broadcast_addr(peer->mac_addr);
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_CANDIDATE_SETUP, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	eth_zero_addr(peer->mac_addr);
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_CANDIDATE_SETUP, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	slsi_tdls_manager_remove_candidate_list(ndev_vif, manager, peer);

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	eth_random_addr(peer->mac_addr);
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_SETUP, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);
	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	eth_broadcast_addr(peer->mac_addr);
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_SETUP, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);
	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	eth_zero_addr(peer->mac_addr);
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_SETUP, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);
	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_DISCOVERED_IND, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);
	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	frame->data = kunit_kzalloc(test, fapi_sig_size(mlme_tdls_peer_ind), GFP_KERNEL);
	fapi_set_u16(frame, u.mlme_tdls_peer_ind.flow_id, 1);
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_CONNECTED_IND, frame);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);
	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_DISCONNECTED_IND, frame);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);
	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	eth_random_addr(peer->mac_addr);
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_TEARDOWN, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);
	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	eth_broadcast_addr(peer->mac_addr);
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_TEARDOWN, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);
	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	eth_zero_addr(peer->mac_addr);
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_TEARDOWN, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);
	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_CFM_EINVAL, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);
	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_CFM_EOPNOTSUPP, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_CFM_SUCCESS, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);
	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer = slsi_tdls_manager_init_peer(peer->dev, mac, manager);

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_INACTIVE, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);
	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	peer->state = SLSI_TDLS_PEER_ACTIVE;
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_BLOCKED, NULL);
	slsi_tdls_manager_peer_state_transition_handler(&manager->peer_state_transition_manager);
	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		if (entry) {
			list_del(&entry->list);
			if (entry->frame)
				consume_skb(entry->frame);
			kfree(entry);
		}
	}

	consume_skb(frame);
}

static void test_slsi_tdls_manager_active_peer_mgmt(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	u8 mac[ETH_ALEN] = {0};
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tdls_manager *manager = &ndev_vif->sta.tdls_manager;

	struct delayed_work *delayed_work_instance = &manager->active_peer_manager;
	struct work_struct *work = &delayed_work_instance->work;
	struct tdls_peer *peer;
	struct state_transition_request *entry;
	struct state_transition_request *next;

	ndev_vif->sta.tdls_manager.active = 1;
	ndev_vif->activated = true;
	manager->active = 1;
	tdls_supported_by_fw = true;

	peer = slsi_tdls_manager_init_peer(dev, mac, manager);
	peer->state = SLSI_TDLS_PEER_ACTIVE;
	//list_add_tail to tdls_inactive_peer_list
	peer->no_traffic_count = SLSI_TDLS_NO_TRAFFIC_MAX_COUNT +1;
	INIT_LIST_HEAD(&manager->state_transition_queue);
	INIT_LIST_HEAD(&ndev_vif->sta.tdls_candidate_setup_list);
	slsi_tdls_manager_active_peer_mgmt(work);

	hlist_del(&peer->hlist);
	kfree(peer);

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		list_del(&entry->list);
		if (entry->frame)
			consume_skb(entry->frame);
		kfree(entry);
	}

	peer = slsi_tdls_manager_init_peer(dev, mac, manager);
	//list_add_tail to tdls_candidate_peer_list
	peer->tx_packets[0].count = tdls_manager_discovery_threshold;
	peer->tx_packets[0].ischecked = true;
	slsi_tdls_manager_active_peer_mgmt(work);

	hlist_del(&peer->hlist);
	kfree(peer);

	peer = slsi_tdls_manager_init_peer(dev, mac, manager);
	peer->state = SLSI_TDLS_PEER_CONNECTED_IND;
	slsi_tdls_manager_active_peer_mgmt(work);

	hlist_del(&peer->hlist);
	kfree(peer);

	peer = slsi_tdls_manager_init_peer(dev, mac, manager);
	peer->state = SLSI_TDLS_PEER_TEARDOWN;
	slsi_tdls_manager_active_peer_mgmt(work);

	hlist_del(&peer->hlist);
	kfree(peer);
}

static void test_slsi_tdls_manager_init_peer(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	u8 mac[ETH_ALEN] = {0};
	struct tdls_manager *manager = kunit_kzalloc(test, sizeof(struct tdls_manager), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tdls_peer *t_peer;

	ndev_vif->activated = true;
	ndev_vif->sta.tdls_manager.active = 1;
	t_peer = slsi_tdls_manager_init_peer(dev, mac, manager);

	KUNIT_EXPECT_PTR_NE(test, NULL, t_peer);

	if (t_peer) {
		hlist_del(&t_peer->hlist);
		kfree(t_peer);
	}
}

static void test_slsi_tdls_manager_event_tx(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tdls_manager *manager = &ndev_vif->sta.tdls_manager;
	struct sk_buff *skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, 0, 100);
	struct tdls_peer *t_peer;
	u8 ac = 0;
	u8 mac[ETH_ALEN] = {0x0C, 0x0C, 0x1C, 0xAA, 0xB1, 0x1F};

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	skb->mac_header = 1;
	ether_addr_copy(eth_hdr(skb)->h_dest, mac);
	tdls_supported_by_fw = true;
	manager->active = 1;
	ndev_vif->sta.sta_bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	ndev_vif->activated = true;
	t_peer = slsi_tdls_manager_init_peer(dev, mac, manager);

	slsi_tdls_manager_event_tx(NULL, dev, skb, ac);
	if (t_peer) {
		hlist_del(&t_peer->hlist);
		kfree(t_peer);
	}

	slsi_tdls_manager_event_tx(NULL, dev, skb, ac);

	t_peer = slsi_tdls_manager_find_peer(dev, manager, eth_hdr(skb)->h_dest);
	if (t_peer) {
		hlist_del(&t_peer->hlist);
		kfree(t_peer);
	}

	consume_skb(skb);
}

static void test_slsi_tdls_manager_on_vif_activated(struct kunit *test)
{
	struct slsi_dev *sdev = NULL;
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	KUNIT_EXPECT_EQ(test, 0, slsi_tdls_manager_on_vif_activated(sdev, dev, ndev_vif));

	sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.tdls_manager.active = 1;
	INIT_LIST_HEAD(&ndev_vif->sta.tdls_manager.state_transition_queue);

	KUNIT_EXPECT_EQ(test, 0, slsi_tdls_manager_on_vif_activated(sdev, dev, ndev_vif));
}

static void test_slsi_tdls_manager_on_vif_deactivated(struct kunit *test)
{
	struct tdls_peer *peer = kunit_kzalloc(test, sizeof(struct tdls_peer), GFP_KERNEL);
	struct netdev_vif *ndev_vif;
	struct tdls_manager *manager;
	u8 mac[ETH_ALEN] = {0};

	peer->dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	ndev_vif = netdev_priv(peer->dev);
	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	manager = &ndev_vif->sta.tdls_manager;
	manager->active = 1;

	INIT_LIST_HEAD(&manager->state_transition_queue);
	slsi_tdls_manager_init_peer(peer->dev, mac, manager);
	enqueue_peer_state_transition_internal(manager, peer, SLSI_TDLS_PEER_DISCOVER, NULL);
	slsi_tdls_manager_on_vif_deactivated(NULL, peer->dev, ndev_vif);
}

static void test_slsi_tdls_manager_event_discovered(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct tdls_manager *manager = &ndev_vif->sta.tdls_manager;
	struct sk_buff *skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, 0, 100);
	struct state_transition_request *entry;
	struct state_transition_request *next;
	u8 mac[ETH_ALEN] = {0};

	fapi_set_memset(skb, u.mlme_tdls_peer_ind.peer_sta_address, 0);
	slsi_tdls_manager_event_discovered(NULL, dev, skb);

	ndev_vif->activated = true;
	manager->active = 1;
	INIT_LIST_HEAD(&manager->state_transition_queue);
	slsi_tdls_manager_init_peer(dev, mac, manager);

	slsi_tdls_manager_event_discovered(NULL, dev, skb);

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		hlist_del(&entry->peer->hlist);
		list_del(&entry->list);
		kfree(entry->peer);
		kfree(entry);
	}

	consume_skb(skb);
}

static void test_slsi_tdls_manager_event_connected(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff *skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, 0, 100);
	struct state_transition_request *entry;
	struct state_transition_request *next;
	u8 mac[ETH_ALEN] = {0};

	fapi_set_memset(skb, u.mlme_tdls_peer_ind.peer_sta_address, 0);

	ndev_vif->activated = true;
	ndev_vif->sta.tdls_manager.active = 1;
	INIT_LIST_HEAD(&ndev_vif->sta.tdls_manager.state_transition_queue);
	INIT_LIST_HEAD(&ndev_vif->sta.tdls_candidate_setup_list);

	slsi_tdls_manager_event_connected(NULL, dev, skb);

	list_for_each_entry_safe(entry, next, &ndev_vif->sta.tdls_manager.state_transition_queue, list) {
		hlist_del(&entry->peer->hlist);
		list_del(&entry->list);
		kfree(entry->peer);
		if (entry->frame)
			consume_skb(entry->frame);
		kfree(entry);
	}

	consume_skb(skb);
}

static void test_slsi_tdls_manager_event_disconnected(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff *skb = fapi_alloc(mlme_tdls_peer_ind, MLME_TDLS_PEER_IND, 0, 100);
	struct tdls_peer *peer = kunit_kzalloc(test, sizeof(struct tdls_peer), GFP_KERNEL);
	struct tdls_manager *manager = &ndev_vif->sta.tdls_manager;
	struct state_transition_request *entry;
	struct state_transition_request *next;
	u8 mac[ETH_ALEN] = {0};

	fapi_set_memset(skb, u.mlme_tdls_peer_ind.peer_sta_address, 0);
	ndev_vif->activated = true;
	manager->active = 1;
	INIT_LIST_HEAD(&manager->state_transition_queue);
	slsi_tdls_manager_init_peer(dev, mac, manager);
	slsi_tdls_manager_event_disconnected(NULL, dev, skb);

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		hlist_del(&entry->peer->hlist);
		list_del(&entry->list);
		if (entry->frame)
			consume_skb(entry->frame);
		kfree(entry->peer);
		kfree(entry);
	}

	consume_skb(skb);
}

static void test_slsi_tdls_manager_oper(struct kunit *test)
{
	struct wiphy *wiphy = kunit_kzalloc(test, sizeof(struct wiphy), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u8 peer[ETH_ALEN] = {0};
	enum nl80211_tdls_operation oper = NL80211_TDLS_DISCOVERY_REQ;
	struct tdls_manager *manager = &ndev_vif->sta.tdls_manager;
	struct state_transition_request *entry;
	struct state_transition_request *next;
	ndev_vif->sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	ndev_vif->activated = true;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	manager->active = 1;
	INIT_LIST_HEAD(&manager->state_transition_queue);

	wiphy->flags = WIPHY_FLAG_SUPPORTS_TDLS;
	KUNIT_EXPECT_EQ(test, 0, slsi_tdls_manager_oper(wiphy, dev, peer, oper));

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		hlist_del(&entry->peer->hlist);
		list_del(&entry->list);
		kfree(entry->peer);
		entry->peer = NULL;
		kfree(entry);
		entry = NULL;
	}

	oper = NL80211_TDLS_SETUP;
	KUNIT_EXPECT_EQ(test, 0, slsi_tdls_manager_oper(wiphy, dev, peer, oper));

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		hlist_del(&entry->peer->hlist);
		list_del(&entry->list);
		kfree(entry->peer);
		kfree(entry);
	}

	oper = NL80211_TDLS_TEARDOWN;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_tdls_manager_oper(wiphy, dev, peer, oper));
	slsi_tdls_manager_init_peer(dev, peer, manager);

	KUNIT_EXPECT_EQ(test, 0, slsi_tdls_manager_oper(wiphy, dev, peer, oper));

	list_for_each_entry_safe(entry, next, &manager->state_transition_queue, list) {
		hlist_del(&entry->peer->hlist);
		list_del(&entry->list);
		kfree(entry->peer);
		kfree(entry);
	}

	oper = NL80211_TDLS_ENABLE_LINK;
	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, slsi_tdls_manager_oper(wiphy, dev, peer, oper));
}


/* Test fictures */
static int tdls_manager_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
	return 0;
}

static void tdls_manager_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case tdls_manager_test_cases[] = {
	KUNIT_CASE(test_slsi_tdls_manager_address_changed),
	KUNIT_CASE(test_slsi_tdls_manager_add_peer_to_candidate_list),
	KUNIT_CASE(test_slsi_tdls_manager_remove_candidate_list),
	KUNIT_CASE(test_slsi_lock_tdls_tcp_ack_lock),
	KUNIT_CASE(test_slsi_lock_tdls_tcp_ack_unlock),
	KUNIT_CASE(test_slsi_tdls_manager_find_peer),
	KUNIT_CASE(test_slsi_tdls_manager_peer_sort),
	KUNIT_CASE(test_enqueue_peer_state_transition_internal),
	KUNIT_CASE(test_enqueue_peer_state_transition),
	KUNIT_CASE(test_slsi_tdls_manager_indication_timeout),
	KUNIT_CASE(test_slsi_tdls_manager_peer_blocked_timeout),
	KUNIT_CASE(test_slsi_tdls_manager_connected_ind),
	KUNIT_CASE(test_slsi_tdls_manager_disconnected_ind),
	KUNIT_CASE(test_slsi_tdls_manager_peer_state_transition_handler),
	KUNIT_CASE(test_slsi_tdls_manager_on_vif_activated),
	KUNIT_CASE(test_slsi_tdls_manager_init_peer),
	KUNIT_CASE(test_slsi_tdls_manager_event_tx),
	KUNIT_CASE(test_slsi_tdls_manager_active_peer_mgmt),
	KUNIT_CASE(test_slsi_tdls_manager_on_vif_deactivated),
	KUNIT_CASE(test_slsi_tdls_manager_event_discovered),
	KUNIT_CASE(test_slsi_tdls_manager_event_connected),
	KUNIT_CASE(test_slsi_tdls_manager_event_disconnected),
	KUNIT_CASE(test_slsi_tdls_manager_oper),
	{}
};

static struct kunit_suite tdls_manager_test_suite[] = {
	{
		.name = "tdls_manager-test",
		.test_cases = tdls_manager_test_cases,
		.init = tdls_manager_test_init,
		.exit = tdls_manager_test_exit,
	}
};

kunit_test_suites(tdls_manager_test_suite);
