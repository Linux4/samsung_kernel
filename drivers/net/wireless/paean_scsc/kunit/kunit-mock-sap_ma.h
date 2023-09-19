/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SAP_MA_H__
#define __KUNIT_MOCK_SAP_MA_H__

#include "../sap_ma.h"

#define sap_ma_init(args...)			kunit_mock_sap_ma_init(args)
#define sap_ma_deinit(args...)			kunit_mock_sap_ma_deinit(args)
#define slsi_rx_amsdu_deaggregate(args...)	kunit_mock_slsi_rx_amsdu_deaggregate(args)


static int kunit_mock_sap_ma_init(void)
{
	return 0;
}

static int kunit_mock_sap_ma_deinit(void)
{
	return 0;
}

static int kunit_mock_slsi_rx_amsdu_deaggregate(struct net_device *dev, struct sk_buff *skb,
						struct sk_buff_head *msdu_list)
{
	return 0;
}
#endif
