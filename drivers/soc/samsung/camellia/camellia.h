/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __CAMELLIA_DEV_H__
#define __CAMELLIA_DEV_H__

/* IOCTL commands */
#define CAMELLIA_IOC_MAGIC 'c'

#define CAMELLIA_IOCTL_SET_TIMEOUT _IOW(CAMELLIA_IOC_MAGIC, 1, uint64_t)

#define CAMELLIA_IOCTL_GET_HANDLE \
	_IOWR(CAMELLIA_IOC_MAGIC, 4, struct camellia_memory_pair)

#define CAMELLIA_IOCTL_GET_VERSION _IOR(CAMELLIA_IOC_MAGIC, 10, struct camellia_version)

#define CAMELLIA_TURN_ON 1
#define CAMELLIA_TURN_OFF 2

#define CAMELLIA_IOCTL_TURN_CAMELLIA _IOW(CAMELLIA_IOC_MAGIC, 20, uint64_t)

struct camellia_memory_pair {
	unsigned long address;
	unsigned long handle;
};

struct camellia_version {
	uint32_t major;
	uint32_t minor;
};

#endif /* __CAMELLIA_DEV_H__ */
