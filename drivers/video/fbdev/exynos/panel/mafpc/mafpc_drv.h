/*
 * linux/drivers/video/fbdev/exynos/panel/mafpc/mafpc_drv.h
 *
 * Header file for AOD Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAFPC_DRV_H__
#define __MAFPC_DRV_H__

#include <linux/kernel.h>
#include <linux/miscdevice.h>

#define MAFPC_DEV_NAME "mafpc"

#define MAFPC_CTRL_CMD_SIZE 39
#define MAFPC_CTRL_CMD_OFFSET 5

#define MAFPC_HEADER_SIZE 1
#define MAFPC_HEADER 'M'

#define MAFPC_UPDATED_FROM_SVC		0x01
#define MAFPC_UPDATED_TO_DEV		0x10

#define MAFPC_V4L2_DEV_NAME	"mafpc-sd"

struct mafpc_device {
	int id;
	struct device *dev;
	struct miscdevice miscdev;
	struct v4l2_subdev sd;

	bool req_update;
	
	/* To prevent rapid image quality change due to mAFPC application, 
	Instant_on performs at frame done after Instant_on ioctl.*/
	bool req_instant_on;

	wait_queue_head_t wq_wait;
	struct task_struct *thread;

	bool enable;
	struct mutex mafpc_lock;

	struct panel_device *panel;

	u8 *img_buf;
	int img_size;

	u8 ctrl_cmd[MAFPC_CTRL_CMD_SIZE];

	u8 *scale_buf;
	u8 scale_size;

	u32 written;

	u8 *factory_crc;
	int factory_crc_len;

	/* table: platform brightness -> scale factor index */
	u8 *scale_map_br_tbl;
	int scale_map_br_tbl_len;

};

struct mafpc_info {
	char *name;
	u8 *mafpc_img;
	u32 mafpc_img_len;
	u8 *mafpc_scale_factor;
	u32 mafpc_scale_factor_len;
	u8 *mafpc_crc_value;
	u32 mafpc_crc_value_len;
	u8* mafpc_scale_map_br_tbl;
	int mafpc_scale_map_br_tbl_len;
};

#define MAFPC_IOCTL_MAGIC	'M'

#define IOCTL_MAFPC_ON			_IOW(MAFPC_IOCTL_MAGIC, 1, int)
#define IOCTL_MAFPC_ON_INSTANT		_IOW(MAFPC_IOCTL_MAGIC, 2, int)
#define IOCTL_MAFPC_OFF			_IOW(MAFPC_IOCTL_MAGIC, 3, int)
#define IOCTL_MAFPC_OFF_INSTANT		_IOW(MAFPC_IOCTL_MAGIC, 4, int)

#define MAFPC_V4L2_IOC_BASE		'V'

#define V4L2_IOCTL_MAFPC_PROBE		_IOR(MAFPC_V4L2_IOC_BASE, 0, int *)
#define V4L2_IOCTL_MAFPC_UDPATE_REQ	_IOR(MAFPC_V4L2_IOC_BASE, 1, int *)
#define V4L2_IOCTL_MAFPC_WAIT_COMP	_IOR(MAFPC_V4L2_IOC_BASE, 2, int *)
#define V4L2_IOCTL_MAFPC_GET_INFO	_IOR(MAFPC_V4L2_IOC_BASE, 3, int *)

#define V4L2_IOCTL_MAFPC_PANEL_INIT	_IOR(MAFPC_V4L2_IOC_BASE, 4, int *)
#define V4L2_IOCTL_MAFPC_PANEL_EXIT 	_IOR(MAFPC_V4L2_IOC_BASE, 5, int *)

#define V4L2_IOCL_MAFPC_FRAME_DONE	_IOR(MAFPC_V4L2_IOC_BASE, 6, int *)

#endif
