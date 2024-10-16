/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth HCI Property                                                     *
 *                                                                            *
 ******************************************************************************/
#ifndef __SLSI_BT_PROPERTY_H__
#define __SLSI_BT_PROPERTY_H__
#include <linux/skbuff.h>
#include "hci_trans.h"

int slsi_bt_property_recv(struct hci_trans *htr, struct sk_buff *skb);

int slsi_bt_property_set_logmask(struct hci_trans *htr,
		const unsigned int *val, const unsigned char len);

#endif /* __SLSI_BT_PROPERTY_H__ */
