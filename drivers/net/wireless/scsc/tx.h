/* SPDX-License-Identifier: <SPDX License Expression> */
/*****************************************************************************
 *
 * Copyright (c) 2012 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_TX_H__
#define __SLSI_TX_H__

#ifdef CONFIG_SCSC_WLAN_TX_API
#include "tx_api.h"
#include "txbp.h"
#endif

int slsi_tx_eapol(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
int slsi_tx_arp(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
int slsi_tx_dhcp(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);

#ifdef CONFIG_SCSC_WLAN_TX_API
bool is_msdu_enable(void);
#endif

#endif
