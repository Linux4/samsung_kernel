/*
 * @file mstdrv.h
 * @brief Header file for MST driver
 * Copyright (c) 2015~2019, Samsung Electronics Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MST_DRV_H
#define MST_DRV_H

#include "../../drivers/misc/tzdev/4.2.0/include/tzdev/tee_client_api.h"

/* defines */
#define MST_DRV_DEV			"mst_drv"
#define TAG				"[sec_mst]"

/* for logging */
#include <linux/printk.h>
void mst_printk(int level, const char *fmt, ...);
#define DEV_ERR			(1)
#define DEV_WARN		(2)
#define DEV_NOTI		(3)
#define DEV_INFO		(4)
#define DEV_DEBUG		(5)
#define MST_LOG_LEVEL		DEV_INFO

#define mst_err(fmt, ...)	mst_printk(DEV_ERR, fmt, ## __VA_ARGS__);
#define mst_warn(fmt, ...)	mst_printk(DEV_WARN, fmt, ## __VA_ARGS__);
#define mst_noti(fmt, ...)	mst_printk(DEV_NOTI, fmt, ## __VA_ARGS__);
#define mst_info(fmt, ...)	mst_printk(DEV_INFO, fmt, ## __VA_ARGS__);
#define mst_debug(fmt, ...)	mst_printk(DEV_DEBUG, fmt, ## __VA_ARGS__);

struct workqueue_struct *cluster_freq_ctrl_wq;
struct delayed_work dwork;

/* for TEEgris */
/* enum definitions */
enum cmd_drv_client
{
	/* ta cmd */
	CMD_OPEN,
	CMD_CLOSE,
	CMD_IOCTL,
	CMD_TRACK1,
	CMD_TRACK2,
	CMD_MAX
};

/* ta uuid definition */
static TEEC_UUID uuid_ta = {
	0,0,0,{0,0,0x6d,0x73,0x74,0x5f,0x54,0x41}
};

#endif /* MST_DRV_H */
