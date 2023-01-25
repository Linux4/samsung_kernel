/*
 *  STMicroelectronics k2dh acceleration sensor driver
 *
 *  Copyright (C) 2010 Samsung Electronics Co.Ltd
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef __k2dh_ACC_HEADER__
#define __k2dh__ACC_HEADER__

#include <linux/types.h>
#include <linux/ioctl.h>

extern struct class *sec_class;

struct k2dh_acceldata {
	__s16 x;
	__s16 y;
	__s16 z;
};

struct accel_platform_data_k2dh {
	int (*accel_get_position) (void);
	 /* Change axis or not for user-level
	 * If it is true, driver reports adjusted axis-raw-data
	 * to user-space based on accel_get_position() value,
	 * or if it is false, driver reports original axis-raw-data */
	bool axis_adjust;
};


/* dev info */
#define ACC_DEV_NAME "accelerometer"

/* k2dh ioctl command label */
#define k2dh_IOCTL_BASE 'a'
#define k2dh_IOCTL_SET_DELAY		_IOW(k2dh_IOCTL_BASE, 0, int64_t)
#define k2dh_IOCTL_GET_DELAY		_IOR(k2dh_IOCTL_BASE, 1, int64_t)
#define k2dh_IOCTL_READ_ACCEL_XYZ	_IOR(k2dh_IOCTL_BASE, 8, \
						struct k2dh_acceldata)
#define k2dh_IOCTL_SET_ENABLE		_IOW(k2dh_IOCTL_BASE, 9, int)
#endif
