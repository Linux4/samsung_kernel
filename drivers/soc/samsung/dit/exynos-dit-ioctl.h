/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DIT_IOCTL_H__
#define __DIT_IOCTL_H__

#include <uapi/linux/if.h>

#define OFFLOAD_DEV_NAME "dit"

#define OFFLOAD_IOC_MAGIC	('D')

/* IOffloadControl */
#define OFFLOAD_IOCTL_INIT_OFFLOAD	_IO(OFFLOAD_IOC_MAGIC, 0x00)
#define OFFLOAD_IOCTL_STOP_OFFLOAD	_IO(OFFLOAD_IOC_MAGIC, 0x01)
#define OFFLOAD_IOCTL_SET_LOCAL_PRFIX	_IO(OFFLOAD_IOC_MAGIC, 0x02)
#define OFFLOAD_IOCTL_GET_FORWD_STATS	_IO(OFFLOAD_IOC_MAGIC, 0x03)
#define OFFLOAD_IOCTL_SET_DATA_LIMIT	_IO(OFFLOAD_IOC_MAGIC, 0x04)
#define OFFLOAD_IOCTL_SET_UPSTRM_PARAM	_IO(OFFLOAD_IOC_MAGIC, 0x05)
#define OFFLOAD_IOCTL_ADD_DOWNSTREAM	_IO(OFFLOAD_IOC_MAGIC, 0x06)
#define OFFLOAD_IOCTL_REMOVE_DOWNSTRM	_IO(OFFLOAD_IOC_MAGIC, 0x07)

/* IOffloadConfig */
#define OFFLOAD_IOCTL_CONF_SET_HANDLES	_IO(OFFLOAD_IOC_MAGIC, 0x10)

enum offload_cb_event {
	OFFLOAD_STARTED = 1,
	OFFLOAD_STOPPED_ERROR = 2,
	OFFLOAD_STOPPED_UNSUPPORTED = 3,
	OFFLOAD_SUPPORT_AVAILABLE = 4,
	OFFLOAD_STOPPED_LIMIT_REACHED = 5,
};

struct forward_stat {
	uint64_t rxBytes;
	uint64_t txBytes;
};

struct iface_info {
	char	iface[IFNAMSIZ];
	char	prefix[48];
};

struct dit_cb_event {
	int32_t cb_event;
};

#endif
