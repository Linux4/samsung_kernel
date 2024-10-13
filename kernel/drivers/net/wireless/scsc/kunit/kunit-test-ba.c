// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "../netif.h"
#include "kunit-common.h"
#include "kunit-mock-sap_ma.h"
#include "kunit-mock-ba_replay.h"
#include "kunit-mock-hip4_smapper.h"
#include "kunit-mock-sap_mlme.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-kernel.h"
#include "../ba.c"

#define TEST_GET_PEER(ndev_vif, idx)  ((struct slsi_peer *)(ndev_vif)->peer_sta_record[(idx)])
#define TEST_TID (0)

static void test_ndev_vif_init(struct kunit *test, struct netdev_vif *ndev_vif)
{
	struct slsi_peer *peer;
	int i = 0, j = 0;

	for (i = 0; i < SLSI_ADHOC_PEER_CONNECTIONS_MAX; i++) {
		ndev_vif->peer_sta_record[i] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ndev_vif->peer_sta_record[i]);
		peer = ndev_vif->peer_sta_record[i];

		for (j = 0; j < NUM_BA_SESSIONS_PER_PEER; j++) {
			peer->ba_session_rx[j] = kunit_kzalloc(test, sizeof(struct slsi_ba_session_rx), GFP_KERNEL);
			KUNIT_ASSERT_NOT_ERR_OR_NULL(test, peer->ba_session_rx[j]);
		}
	}
}

static void test_slsi_rx_ba_init(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	slsi_rx_ba_init(sdev);
}

static void test_slsi_rx_ba_alloc_buffer(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_ba_session_rx *buffer = NULL;
	int i;

	for (i = 0; i < SLSI_MAX_RX_BA_SESSIONS; i++) {
		buffer = slsi_rx_ba_alloc_buffer(dev);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buffer);
		KUNIT_EXPECT_TRUE(test, sdev->rx_ba_buffer_pool[i].used);
	}

	for (i = 0; i < SLSI_MAX_RX_BA_SESSIONS; i++) {
		buffer = slsi_rx_ba_alloc_buffer(dev);
		KUNIT_EXPECT_EQ(test, NULL, buffer);
	}
}

static void test_slsi_rx_ba_free_buffer(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = TEST_GET_PEER(ndev_vif, TEST_TID);

	slsi_rx_ba_free_buffer(dev, peer, TEST_TID);
	KUNIT_EXPECT_EQ(test, NULL, peer->ba_session_rx[TEST_TID]);
}

static void test_slsi_ba_process_complete(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff *next_skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, next_skb);

	ndev_vif->ba_complete.next = next_skb;
	ndev_vif->ba_complete.prev = &ndev_vif->ba_complete;
	next_skb->next = &ndev_vif->ba_complete;
	next_skb->prev = &ndev_vif->ba_complete;

	slsi_ba_process_complete(dev, true);
}

static void test_slsi_ba_signal_process_complete(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	slsi_ba_signal_process_complete(dev);
}

static void test_ba_update_expected_sn(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = TEST_GET_PEER(ndev_vif, TEST_TID);

	dev->name[0] = 't';
	peer->ba_session_rx[TEST_TID]->expected_sn = 0;
	peer->ba_session_rx[TEST_TID]->buffer_size = 1;
	peer->ba_session_rx[TEST_TID]->buffer[0].active = true;

	ba_update_expected_sn(dev, peer->ba_session_rx[TEST_TID], 1);
	KUNIT_EXPECT_EQ(test, 1, peer->ba_session_rx[TEST_TID]->expected_sn);
}

static void test_ba_scroll_window(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = TEST_GET_PEER(ndev_vif, TEST_TID);

	ndev_vif->ba_complete.next = &ndev_vif->ba_complete;
	ndev_vif->ba_complete.prev = &ndev_vif->ba_complete;

	peer->ba_session_rx[TEST_TID]->start_sn = 0;
	peer->ba_session_rx[TEST_TID]->expected_sn = 0;
	peer->ba_session_rx[TEST_TID]->buffer_size = 1;
	peer->ba_session_rx[TEST_TID]->buffer[0].active = true;

	ba_scroll_window(dev, peer->ba_session_rx[TEST_TID], 0);
	KUNIT_EXPECT_EQ(test, 1, peer->ba_session_rx[TEST_TID]->expected_sn);
}

static void test_ba_delete_ba_on_old_frame(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = TEST_GET_PEER(ndev_vif, TEST_TID);

	ba_delete_ba_on_old_frame(dev, peer, peer->ba_session_rx[TEST_TID], 1);
	KUNIT_EXPECT_TRUE(test, peer->ba_session_rx[TEST_TID]->closing);
}

static void test_ba_consume_frame_or_get_buffer_index(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = TEST_GET_PEER(ndev_vif, TEST_TID);
	struct slsi_ba_session_rx *ba_session_rx = peer->ba_session_rx[TEST_TID];
	bool stop_timer = 0;
	struct slsi_ba_frame_desc *frame_desc = kunit_kzalloc(test, sizeof(struct slsi_ba_frame_desc), GFP_KERNEL);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, frame_desc);

	ndev_vif->ba_complete.next = &ndev_vif->ba_complete;
	ndev_vif->ba_complete.prev = &ndev_vif->ba_complete;
	dev->name[0] = 't';

	ba_session_rx->buffer[0].active = false;
	ba_session_rx->buffer[1].active = true;
	ba_session_rx->timer_on = 1;
	ba_session_rx->expected_sn = 1;
	ba_session_rx->buffer_size = 1;
	KUNIT_EXPECT_EQ(test, -1, ba_consume_frame_or_get_buffer_index(dev, peer, ba_session_rx,
								       2, frame_desc, &stop_timer));

	ba_session_rx->buffer_size = 2;
	ba_session_rx->start_sn = 1;
	ba_session_rx->expected_sn = 1;
	KUNIT_EXPECT_EQ(test, 0, ba_consume_frame_or_get_buffer_index(dev, peer, ba_session_rx,
								      0xFFF + 2, frame_desc, &stop_timer));

	ba_session_rx->buffer_size = 2;
	ba_session_rx->start_sn = 1;
	ba_session_rx->expected_sn = 1;
	ba_session_rx->buffer[0].active = true;
	KUNIT_EXPECT_EQ(test, -1, ba_consume_frame_or_get_buffer_index(dev, peer, ba_session_rx,
								       0xFFF + 2, frame_desc, &stop_timer));

	KUNIT_EXPECT_EQ(test, -1, ba_consume_frame_or_get_buffer_index(dev, peer, ba_session_rx,
								       0xFFF, frame_desc, &stop_timer));

	ba_session_rx->highest_received_sn = 20000;
	KUNIT_EXPECT_EQ(test, -1, ba_consume_frame_or_get_buffer_index(dev, peer, ba_session_rx,
								       0xFFF, frame_desc, &stop_timer));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	peer->aid = SLSI_TDLS_PEER_INDEX_MIN;
	KUNIT_EXPECT_EQ(test, -1, ba_consume_frame_or_get_buffer_index(dev, peer, ba_session_rx,
								       0xFFF, frame_desc, &stop_timer));

	ba_session_rx->trigger_ba_after_ssn = 0;
	KUNIT_EXPECT_EQ(test, -1, ba_consume_frame_or_get_buffer_index(dev, peer, ba_session_rx,
								       0xFFF, frame_desc, &stop_timer));
}

static void test_slsi_ba_aging_timeout_handler(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_ba_session_rx *ba_session_rx = TEST_GET_PEER(ndev_vif, TEST_TID)->ba_session_rx[TEST_TID];

	ba_session_rx->dev = dev;
	ba_session_rx->active = true;
	ba_session_rx->buffer_size = 1;
	ba_session_rx->expected_sn = 0;
	ba_session_rx->occupied_slots = 1;
	ba_session_rx->buffer[0].active = true;

	ndev_vif->ba_complete.next = &ndev_vif->ba_complete;
	ndev_vif->ba_complete.prev = &ndev_vif->ba_complete;
	dev->name[0] = 't';

#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	slsi_ba_aging_timeout_handler(&ba_session_rx->ba_age_timer);
#else
	slsi_ba_aging_timeout_handler((unsigned long)ba_session_rx);
#endif

	ba_session_rx->buffer[0].active = false;
	ba_session_rx->active = true;
	ba_session_rx->occupied_slots = 1;

#if KERNEL_VERSION(4, 15, 0) <= LINUX_VERSION_CODE
	slsi_ba_aging_timeout_handler(&ba_session_rx->ba_age_timer);
#else
	slsi_ba_aging_timeout_handler((unsigned long)ba_session_rx);
#endif
}

static void test_slsi_ba_process_frame(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = TEST_GET_PEER(ndev_vif, TEST_TID);
	struct sk_buff *skb = NULL;
	u16 sequence_number = 0;
	u16 tid = 0;

	ndev_vif->ba_complete.next = &ndev_vif->ba_complete;
	ndev_vif->ba_complete.prev = &ndev_vif->ba_complete;
	dev->name[0] = 't';

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_ba_process_frame(dev, peer, skb, sequence_number, FAPI_PRIORITY_QOS_UP7 + 1));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ba_process_frame(dev, peer, skb, sequence_number, tid));

	peer->ba_session_rx[TEST_TID]->active = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ba_process_frame(dev, peer, skb, sequence_number, tid));

	sequence_number = 0xFFF + 2;
	peer->ba_session_rx[TEST_TID]->active = true;
	peer->ba_session_rx[TEST_TID]->timer_on = 1;
	peer->ba_session_rx[TEST_TID]->buffer_size = 2;
	peer->ba_session_rx[TEST_TID]->start_sn = 1;
	peer->ba_session_rx[TEST_TID]->expected_sn = 1;
	peer->ba_session_rx[TEST_TID]->buffer[0].active = false;
	KUNIT_EXPECT_EQ(test, 0, slsi_ba_process_frame(dev, peer, skb, sequence_number, tid));

	sequence_number = 1;
	peer->ba_session_rx[TEST_TID]->occupied_slots = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_ba_process_frame(dev, peer, skb, sequence_number, tid));

	peer->ba_session_rx[TEST_TID]->timer_on = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_ba_process_frame(dev, peer, skb, sequence_number, tid));
}

static void test_slsi_ba_check(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = TEST_GET_PEER(ndev_vif, TEST_TID);
	int tid;

	tid = FAPI_PRIORITY_QOS_UP7 + 1;
	KUNIT_EXPECT_EQ(test, false, slsi_ba_check(peer, tid));

	tid = 0;
	KUNIT_EXPECT_EQ(test, false, slsi_ba_check(peer, tid));

	peer->ba_session_rx[tid]->active = true;
	KUNIT_EXPECT_EQ(test, true, slsi_ba_check(peer, tid));

	peer->ba_session_rx[tid]->active = false;
	KUNIT_EXPECT_EQ(test, false, slsi_ba_check(peer, tid));

	peer->ba_session_rx[tid] = NULL;
	KUNIT_EXPECT_EQ(test, false, slsi_ba_check(peer, tid));
}

static void test_slsi_rx_ba_stop(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_ba_session_rx *ba_session_rx = TEST_GET_PEER(ndev_vif, TEST_TID)->ba_session_rx[TEST_TID];

	ndev_vif->ba_complete.next = &ndev_vif->ba_complete;
	ndev_vif->ba_complete.prev = &ndev_vif->ba_complete;

	__slsi_rx_ba_stop(dev, ba_session_rx);
	KUNIT_EXPECT_FALSE(test, ba_session_rx->active);

	dev->name[0] = 't';
	ba_session_rx->expected_sn = 1;
	ba_session_rx->buffer_size = 1;
	ba_session_rx->active = true;
	ba_session_rx->buffer[0].active = true;
	__slsi_rx_ba_stop(dev, ba_session_rx);
	KUNIT_EXPECT_FALSE(test, ba_session_rx->active);
}

static void test_slsi_rx_ba_stop_lock_held(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_ba_session_rx *ba_session_rx = TEST_GET_PEER(ndev_vif, TEST_TID)->ba_session_rx[TEST_TID];

	slsi_rx_ba_stop_lock_held(dev, ba_session_rx);
}

static void test_slsi_rx_ba_stop_lock_unheld(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_ba_session_rx *ba_session_rx = TEST_GET_PEER(ndev_vif, TEST_TID)->ba_session_rx[TEST_TID];

	slsi_rx_ba_stop_lock_unheld(dev, ba_session_rx);
}

static void test_slsi_rx_ba_stop_all(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = TEST_GET_PEER(ndev_vif, TEST_TID);
	int i;

	ndev_vif->ba_complete.next = &ndev_vif->ba_complete;
	ndev_vif->ba_complete.prev = &ndev_vif->ba_complete;
	dev->name[0] = 't';

	for (i = 0; i < NUM_BA_SESSIONS_PER_PEER; i++) {
		peer->ba_session_rx[i]->active = true;
		peer->ba_session_rx[i]->expected_sn = 1;
		peer->ba_session_rx[i]->buffer_size = 1;
		peer->ba_session_rx[i]->buffer[0].active = true;
	}

	slsi_rx_ba_stop_all(dev, peer);

	for (i = 0; i < NUM_BA_SESSIONS_PER_PEER; i++)
		KUNIT_EXPECT_EQ(test, NULL, peer->ba_session_rx[i]);
}

static void test_slsi_rx_ba_start(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = TEST_GET_PEER(ndev_vif, TEST_TID);
	u16 tid = 0;
	u16 buffer_size = 1;
	u16 start_sn = 1;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_rx_ba_start(dev, peer, peer->ba_session_rx[TEST_TID], tid, 0, start_sn));

	peer->ba_session_rx[TEST_TID]->expected_sn = 1;
	peer->ba_session_rx[TEST_TID]->buffer_size = 1;
	peer->ba_session_rx[TEST_TID]->active = true;
	KUNIT_EXPECT_EQ(test, 0,
			slsi_rx_ba_start(dev, peer, peer->ba_session_rx[TEST_TID], tid, buffer_size, start_sn));

	peer->ba_session_rx[TEST_TID]->expected_sn = 1;
	peer->ba_session_rx[TEST_TID]->buffer_size = 1;

	ndev_vif->ba_complete.next = &ndev_vif->ba_complete;
	ndev_vif->ba_complete.prev = &ndev_vif->ba_complete;
	dev->name[0] = 't';

	peer->ba_session_rx[TEST_TID]->active = true;
	peer->ba_session_rx[TEST_TID]->expected_sn = 1;
	peer->ba_session_rx[TEST_TID]->buffer_size = 1;
	peer->ba_session_rx[TEST_TID]->buffer[0].active = true;
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_ba_start(dev, peer, peer->ba_session_rx[TEST_TID], tid, buffer_size, 0));
}

static void test_slsi_ba_update_window(struct kunit *test)
{
#define TEST_SEQ_NUM (4096)
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_ba_session_rx *ba_session_rx = TEST_GET_PEER(ndev_vif, TEST_TID)->ba_session_rx[TEST_TID];

	ba_session_rx->active = false;
	slsi_ba_update_window(dev, ba_session_rx, 0);

	ba_session_rx->expected_sn = 0;
	ba_session_rx->buffer_size = 1;
	ba_session_rx->active = true;

	ndev_vif->ba_complete.next = &ndev_vif->ba_complete;
	ndev_vif->ba_complete.prev = &ndev_vif->ba_complete;
	slsi_ba_update_window(dev, ba_session_rx, TEST_SEQ_NUM);
}

static void test_slsi_handle_blockack(struct kunit *test)
{
#define TEST_TID_1 (1)
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_peer *peer = TEST_GET_PEER(ndev_vif, TEST_TID);

	slsi_handle_blockack(dev, peer, FAPI_REASONCODE_START, FAPI_PRIORITY_QOS_UP7 + 1, 1, 0);
	slsi_handle_blockack(dev, peer, FAPI_REASONCODE_START, TEST_TID, 1, 0);
	KUNIT_EXPECT_TRUE(test, peer->ba_session_rx[TEST_TID]->active);

	peer->ba_session_rx[TEST_TID]->expected_sn = 1;
	peer->ba_session_rx[TEST_TID]->buffer_size = 0;
	slsi_handle_blockack(dev, peer, FAPI_REASONCODE_START, TEST_TID, 0, 1);
	KUNIT_EXPECT_EQ(test, NULL, peer->ba_session_rx[TEST_TID]);

	peer->ba_session_rx[TEST_TID_1]->active = false;
	slsi_handle_blockack(dev, peer, FAPI_REASONCODE_END, TEST_TID_1, 1, 0);
	KUNIT_EXPECT_EQ(test, NULL, peer->ba_session_rx[TEST_TID_1]);

	slsi_handle_blockack(dev, peer, FAPI_REASONCODE_UNSPECIFIED_REASON, TEST_TID_1, 1, 0);
	slsi_handle_blockack(dev, peer, 0, TEST_TID_1, 1, 0);
}

static int ba_test_init(struct kunit *test)
{
	struct netdev_vif *ndev_vif;

	test_dev_init(test);

	ndev_vif = netdev_priv(TEST_TO_DEV(test));

	test_ndev_vif_init(test, ndev_vif);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void ba_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

static struct kunit_case ba_test_cases[] = {
	KUNIT_CASE(test_slsi_rx_ba_init),
	KUNIT_CASE(test_slsi_rx_ba_alloc_buffer),
	KUNIT_CASE(test_slsi_rx_ba_free_buffer),
	KUNIT_CASE(test_slsi_ba_process_complete),
	KUNIT_CASE(test_slsi_ba_signal_process_complete),
	KUNIT_CASE(test_ba_update_expected_sn),
	KUNIT_CASE(test_ba_scroll_window),
	KUNIT_CASE(test_ba_delete_ba_on_old_frame),
	KUNIT_CASE(test_ba_consume_frame_or_get_buffer_index),
	KUNIT_CASE(test_slsi_ba_aging_timeout_handler),
	KUNIT_CASE(test_slsi_ba_process_frame),
	KUNIT_CASE(test_slsi_ba_check),
	KUNIT_CASE(test_slsi_rx_ba_stop),
	KUNIT_CASE(test_slsi_rx_ba_stop_lock_held),
	KUNIT_CASE(test_slsi_rx_ba_stop_lock_unheld),
	KUNIT_CASE(test_slsi_rx_ba_stop_all),
	KUNIT_CASE(test_slsi_rx_ba_start),
	KUNIT_CASE(test_slsi_ba_update_window),
	KUNIT_CASE(test_slsi_handle_blockack),
	{}
};

static struct kunit_suite ba_test_suite[] = {
	{
		.name = "kunit-ba-test",
		.test_cases = ba_test_cases,
		.init = ba_test_init,
		.exit = ba_test_exit,
	}
};

kunit_test_suites(ba_test_suite);
