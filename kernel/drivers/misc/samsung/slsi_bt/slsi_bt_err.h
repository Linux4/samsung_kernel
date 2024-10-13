/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth Uart Driver                                                      *
 *                                                                            *
 ******************************************************************************/
#ifndef __SLSI_BT_ERRH__
#define __SLSI_BT_ERRH__
#include <linux/seq_file.h>

enum {
	SLSI_BT_ERR_UNKNOWN        = 0x00,

	SLSI_BT_ERR_NOMEM          = 0x0c,
	SLSI_BT_UART_WRITE_FAIL,

	SLSI_BT_ERR_FORCE_CRASH    = 0x10,
	SLSI_BT_ERR_SEND_FAIL,
	SLSI_BT_ERR_BCSP_FAIL,
	SLSI_BT_ERR_BCSP_RESEND_LIMIT,
	SLSI_BT_ERR_TR_INIT_FAIL,

	/* 0x20 ~ 0x2F are for maxwell */
	SLSI_BT_ERR_MX_FAIL        = 0x20,
	SLSI_BT_ERR_MX_RESET,
};

#define SLSI_BT_ERR_HISTORY_SIZE        (8)

void slsi_bt_err(int reason);
int slsi_bt_err_status(void);

int slsi_bt_err_proc_show(struct seq_file *m, void *v);

int slsi_bt_err_init(void (*callback)(int reason, bool req_restart));
void slsi_bt_err_deinit(void);

#endif /* __SLSI_BT_ERRH__ */
