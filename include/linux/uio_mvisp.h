/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * (C) Copyright 2012 Marvell International Ltd.
 * All Rights Reserved
 */

#ifndef _UIO_MVISP_H_
#define _UIO_MVISP_H_

struct mvisp_get_uio_name {
	char driver[16];
};

#define MVISP_IOC_MAGIC 'C'
#define MVISP_UIO_NAME \
		_IOWR(MVISP_IOC_MAGIC, 1, struct mvisp_get_uio_name)
#define MVISP_POWER_ON	_IO(MVISP_IOC_MAGIC, 2)
#define MVISP_POWER_OFF	_IO(MVISP_IOC_MAGIC, 3)
#define MVISP_CLK_ON		_IO(MVISP_IOC_MAGIC, 4)
#define MVISP_CLK_OFF	_IO(MVISP_IOC_MAGIC, 5)
#define MVISP_LOCK		_IOW(MVISP_IOC_MAGIC, 6, unsigned int)
#define MVISP_UNLOCK		_IO(MVISP_IOC_MAGIC, 7)
#define MVISP_GETSET_INFO	_IOW(MVISP_IOC_MAGIC, 8, unsigned int)
#define UIO_MVISP_NAME	"uio-mvisp"

#endif /* _UIO_MVISP_H_ */
