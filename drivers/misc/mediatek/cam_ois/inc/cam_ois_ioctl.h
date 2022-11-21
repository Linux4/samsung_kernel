/*
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _CAM_OIS_IOCTL_H
#define _CAM_OIS_IOCTL_H

#include <linux/ioctl.h>
#ifdef CONFIG_COMPAT
/*64 bit*/
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#define CAM_OIS_MAGIC 'o'
/*IOCTRL(inode * ,file * ,cmd ,arg )*/
/*S means "set through a ptr"*/
/*T means "tell by a arg value"*/
/*G means "get by a ptr"*/
/*Q means "get by return a value"*/
/*X means "switch G and S atomically"*/
/*H means "switch T and Q atomically"*/

/**********************************************
 *
 **********************************************/

/*CAM_OIS write*/
#define CAM_OISIOC_S_WRITE _IOW(CAM_OIS_MAGIC, 0, struct stCAM_OSI_INFO_STRUCT)
/*CAM_OIS read*/
#define CAM_OISIOC_G_READ _IOWR(CAM_OIS_MAGIC, 5, struct stCAM_OSI_INFO_STRUCT)

#ifdef CONFIG_COMPAT
#define COMPAT_CAM_OISIOC_S_WRITE \
	_IOW(CAM_CALAGIC, 0, struct COMPAT_stCAM_OIS_INFO_STRUCT)
#define COMPAT_CAM_OISIOC_G_READ \
	_IOWR(CAM_CALAGIC, 5, struct COMPAT_stCAM_OIS_INFO_STRUCT)
#endif
#endif /*_CAM_OIS_IOCTL_H*/
