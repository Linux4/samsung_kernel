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
#ifndef __SLSI_BT_CONTROLLER_H__
#define __SLSI_BT_CONTROLLER_H__

/* BT Configuration file */
#define SLSI_BT_CONF      "bt.hcf"

#ifdef CONFIG_SCSC_BT_ADDRESS_IN_FILE
#define SLSI_BT_ADDR      CONFIG_SCSC_BT_ADDRESS_FILENAME
#define SLSI_BT_ADDR_LEN  (6)
#endif

/* slsi bt control APIs */
int slsi_bt_controller_init(void);
void slsi_bt_controller_exit(void);

int slsi_bt_controller_start(void);
int slsi_bt_controller_stop(void);

void* slsi_bt_controller_get_mx(void);
void slsi_bt_controller_update_fw_log_filter(unsigned long long en[2]);

#endif /*  __SLSI_BT_CONTROLLER_H__ */
