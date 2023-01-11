/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#ifndef __PPPRD_LINUX_H__
#define __PPPRD_LINUX_H__

#define PPP_FRAME_SIZE 0xC00

#define PPPRD_IOC_MAGIC 'X'

#define PPPRD_IOCTL_TEST _IOW(PPPRD_IOC_MAGIC, 1, int)
#define PPPRD_IOCTL_TIOPPPON _IOW(PPPRD_IOC_MAGIC, 2, int)
#define PPPRD_IOCTL_TIOPPPOFF _IOW(PPPRD_IOC_MAGIC, 3, int)
#define PPPRD_IOCTL_TIOPPPSETCB _IO(PPPRD_IOC_MAGIC, 4)
#define PPPRD_IOCTL_TIOPPPRESETCB _IO(PPPRD_IOC_MAGIC, 5)

#endif
