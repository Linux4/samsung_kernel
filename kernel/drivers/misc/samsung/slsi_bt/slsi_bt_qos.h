/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * S.LSI Bluetooth Service Control Driver                                     *
 *                                                                            *
 * This driver is tightly coupled with scsc maxwell driver and BT conroller's *
 * data structure.                                                            *
 *                                                                            *
 ******************************************************************************/
#ifndef __SLSI_BT_QOS_H__
#define __SLSI_BT_QOS_H__
#include "../scsc/scsc_mx_impl.h"

#ifdef CONFIG_SCSC_QOS
struct slsi_bt_qos;

struct slsi_bt_qos *slsi_bt_qos_start(struct scsc_service *service);
void slsi_bt_qos_stop(struct slsi_bt_qos *qos);
void slsi_bt_qos_update(struct slsi_bt_qos *qos, int count);

void slsi_bt_qos_service_init(void);
void slsi_bt_qos_service_exit(void);
#else
struct slsi_bt_qos {
};
#define slsi_bt_qos_start(service)      NULL
#define slsi_bt_qos_stop(qos)
#define slsi_bt_qos_update(qos, count)
#define slsi_bt_qos_service_init()
#define slsi_bt_qos_service_exit()
#endif

#endif /* __SLSI_BT_QOS_H__ */
