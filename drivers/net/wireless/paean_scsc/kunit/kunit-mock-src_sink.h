/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SLSI_SRC_SINK_H__
#define __KUNIT_MOCK_SLSI_SRC_SINK_H__

#include "../src_sink.h"

#define slsi_src_sink_cdev_ioctl_cfg(args...)	kunit_mock_slsi_src_sink_cdev_ioctl_cfg(args)
#define slsi_rx_gen_report(args...)		kunit_mock_slsi_rx_gen_report(args)
#define slsi_rx_sink_report(args...)		kunit_mock_slsi_rx_sink_report(args)


static long kunit_mock_slsi_src_sink_cdev_ioctl_cfg(struct slsi_dev *sdev, unsigned long arg)
{
	return 0;
}

static void kunit_mock_slsi_rx_gen_report(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	return;
}

static void kunit_mock_slsi_rx_sink_report(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	return;
}
#endif
