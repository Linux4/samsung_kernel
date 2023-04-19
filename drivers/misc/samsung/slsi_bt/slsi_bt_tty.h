/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * S.LSI Bluetooth                                                            *
 *                                                                            *
 ******************************************************************************/
#ifndef __SLSI_BT_TTY_H__
#define __SLSI_BT_TTY_H__
#include "hci_trans.h"

#ifdef CONFIG_SLSI_BT_TTY_BAUD
#define SLSI_BT_TTY_BAUD	        CONFIG_SLSI_BT_TTY_BAUD
#else
#define SLSI_BT_TTY_BAUD	        4800000
#endif

/* Uart driver name */
#ifdef CONFIG_SLSI_BT_TTY_DEVICE_NAME
#define SLSI_BT_TTY_DEV_NAME            CONFIG_SLSI_BT_TTY_DEVICE_NAME
#else
#define SLSI_BT_TTY_DEV_NAME            "ttySAC1"       /* using this device */
#endif

int slsi_bt_tty_init(void);
void slsi_bt_tty_exit(void);

int slsi_bt_tty_open(struct hci_trans **htr);
void slsi_bt_tty_close(void);
long slsi_bt_tty_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

#endif /* __SLSI_BT_TTY_H__ */
