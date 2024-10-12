/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _CAM_CAL_H
#define _CAM_CAL_H

#include <linux/ioctl.h>
#ifdef CONFIG_COMPAT
/*64 bit*/
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#define CAM_CALAGIC 'i'
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

/*CAM_CAL write*/
#define CAM_CALIOC_S_WRITE _IOW(CAM_CALAGIC, 0, struct stCAM_CAL_INFO_STRUCT)
/*CAM_CAL read*/
#define CAM_CALIOC_G_READ _IOWR(CAM_CALAGIC, 1, struct stCAM_CAL_INFO_STRUCT)
/*CAM_CAL set sensor info*/
#define CAM_CALIOC_S_SENSOR_INFO \
	_IOW(CAM_CALAGIC, 2, struct CAM_CAL_SENSOR_INFO)
/*CAM_CAL get cal data*/
#define CAM_CALIOC_G_SENSOR_INFO \
	_IOWR(CAM_CALAGIC, 3, struct CAM_CAL_SENSOR_INFO)

#define CAM_CALIOC_S_CAM_OIS_CAL \
	_IOW(CAM_CALAGIC, 50, struct CAM_CAL_OIS_GYRO_CAL)

#ifdef CONFIG_COMPAT
#define COMPAT_CAM_CALIOC_S_WRITE \
	_IOW(CAM_CALAGIC, 0, struct COMPAT_stCAM_CAL_INFO_STRUCT)
#define COMPAT_CAM_CALIOC_G_READ \
	_IOWR(CAM_CALAGIC, 1, struct COMPAT_stCAM_CAL_INFO_STRUCT)
	/*CAM_CAL set sensor info*/
#define COMPAT_CAM_CALIOC_S_SENSOR_INFO \
	_IOW(CAM_CALAGIC, 2, struct COMPAT_CAM_CAL_SENSOR_INFO)
	/*CAM_CAL get cal data*/
#define COMPAT_CAM_CALIOC_G_SENSOR_INFO \
	_IOWR(CAM_CALAGIC, 3, struct COMPAT_CAM_CAL_SENSOR_INFO)
#endif
#endif /*_CAM_CAL_H*/
