/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_DEBUG_FRAME_H__
#define __KUNIT_MOCK_DEBUG_FRAME_H__

#include "../debug.h"

#define slsi_debug_frame(args...)	kunit_mock_slsi_debug_frame(args)


static void kunit_mock_slsi_debug_frame(struct slsi_dev *sdev, struct net_device *dev,
					struct sk_buff *skb, const char *prefix)
{
	return;
}
#endif
