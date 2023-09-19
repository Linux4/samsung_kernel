/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_BA_H__
#define __KUNIT_MOCK_BA_H__

#include "../ba.h"

#define slsi_ba_process_frame(args...)		kunit_mock_slsi_ba_process_frame(args)
#define slsi_rx_ba_stop_all(args...)		kunit_mock_slsi_rx_ba_stop_all(args)
#define slsi_rx_ba_init(args...)		kunit_mock_slsi_rx_ba_init(args)
#define slsi_ba_update_window(args...)		kunit_mock_slsi_ba_update_window(args)
#define slsi_handle_blockack(args...)		kunit_mock_slsi_handle_blockack(args)
#define slsi_ba_process_complete(args...)	kunit_mock_slsi_ba_process_complete(args)
#define slsi_ba_check(args...)			kunit_mock_slsi_ba_check(args)


static int kunit_mock_slsi_ba_process_frame(struct net_device *dev, struct slsi_peer *peer,
			  struct sk_buff *skb, u16 sequence_number, u16 tid)
{
	return 0;
}

static void kunit_mock_slsi_rx_ba_stop_all(struct net_device *dev, struct slsi_peer *peer)
{
	return;
}

static void kunit_mock_slsi_rx_ba_init(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_ba_update_window(struct net_device *dev,
				  struct slsi_ba_session_rx *ba_session_rx, u16 sequence_number)
{
	return;
}

static void kunit_mock_slsi_handle_blockack(struct net_device *dev, struct slsi_peer *peer,
			  u16 reason_code, u16 user_priority, u16 buffer_size, u16 sequence_number)
{
	return;
}

static void kunit_mock_slsi_ba_process_complete(struct net_device *dev, bool ctx_napi)
{
	return;
}

static bool kunit_mock_slsi_ba_check(struct slsi_peer *peer, u16 tid)
{
	if (tid > FAPI_PRIORITY_QOS_UP7)
		return false;

	if (peer && !peer->ba_session_rx[tid])
		return false;

	return true;
}
#endif
