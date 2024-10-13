
/****************************************************************************
 *
 * Copyright (c) 2014 - 2022 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SAP_MA_H__
#define __SAP_MA_H__

int sap_ma_init(void);
int sap_ma_deinit(void);

int slsi_rx_amsdu_deaggregate(struct net_device *dev, struct sk_buff *skb, struct sk_buff_head *msdu_list);
#ifdef CONFIG_SCSC_WLAN_NAPI_PER_NETDEV
void slsi_rx_data_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb);
void change_core_for_cpu(struct work_struct *data);
#endif
void slsi_sap_ma_lock(void);
void slsi_sap_ma_unlock(void);

#endif
