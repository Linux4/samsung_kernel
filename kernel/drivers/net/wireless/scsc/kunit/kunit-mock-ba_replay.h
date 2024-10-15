/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_BA_REPLAY_H__
#define __KUNIT_MOCK_BA_REPLAY_H__

#include "../ba.h"

#define slsi_ba_replay_store_pn(args...)	kunit_mock_slsi_ba_replay_store_pn(args)
#define slsi_ba_replay_check_pn(args...)	kunit_mock_slsi_ba_replay_check_pn(args)
#define slsi_ba_replay_get_pn(args...)		kunit_mock_slsi_ba_replay_get_pn(args)
#define slsi_ba_replay_reset_pn(args...)	kunit_mock_slsi_ba_replay_reset_pn(args)


static void kunit_mock_slsi_ba_replay_store_pn(struct net_device *dev, struct slsi_peer *peer, struct sk_buff *skb)
{
	return;
}

static bool kunit_mock_slsi_ba_replay_check_pn(struct net_device *dev, struct slsi_ba_session_rx *ba_session_rx,
					       struct slsi_ba_frame_desc *frame_desc)
{
	if (dev->name[0] == 't')
		return 1;
	else
		return 0;
}

static void kunit_mock_slsi_ba_replay_get_pn(struct net_device *dev, struct slsi_peer *peer, struct sk_buff *skb,
					     struct slsi_ba_frame_desc *frame_desc)
{
	return;
}

static void kunit_mock_slsi_ba_replay_reset_pn(struct net_device *dev, struct slsi_peer *peer)
{
	return;
}
#endif
