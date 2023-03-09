/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth HCI Uart Transport Layer & H4                                    *
 *                                                                            *
 ******************************************************************************/
#ifndef __HCI_H4_H__
#define __HCI_H4_H__
#include "hci_pkt.h"
#include "hci_trans.h"

#define HCI_H4_PKT_TYPE_SIZE          1

int hci_h4_init(struct hci_trans *htr, bool reverse);
void hci_h4_deinit(struct hci_trans *htr);

#endif /* __SLSI_HCI_H4_H__ */
