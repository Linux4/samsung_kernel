/*
 * Copyright (C) 2010 MEMSIC, Inc.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/*
 * Definitions for ACC magnetic sensor chip.
 */
#ifndef __PTACCS_H__
#define __PTACCS_H__

#include <linux/ioctl.h>

/* Use 'e' as magic number */
#define ACC_IOM			'e'

/* IOCTLs for ACC device */
#define ACC_IOC_SET_MODE		_IOW(ACC_IOM, 0x00, short)
#define ACC_IOC_SET_DELAY		_IOW(ACC_IOM, 0x01, short)
#define ACC_IOC_GET_DELAY		_IOR(ACC_IOM, 0x02, short)

#define ACC_IOC_SET_AFLAG		_IOW(ACC_IOM, 0x10, short)
#define ACC_IOC_GET_AFLAG		_IOR(ACC_IOM, 0x11, short)
#define ACC_IOC_SET_MFLAG		_IOW(ACC_IOM, 0x12, short)
#define ACC_IOC_GET_MFLAG		_IOR(ACC_IOM, 0x13, short)
#define ACC_IOC_SET_OFLAG		_IOW(ACC_IOM, 0x14, short)
#define ACC_IOC_GET_OFLAG		_IOR(ACC_IOM, 0x15, short)

#define ACC_IOC_SET_APARMS		_IOW(ACC_IOM, 0x20, int[3])

#define ACC_IOC_SET_YPR		_IOW(ACC_IOM, 0x30, int[3])

#endif /* __PTACCS_H__ */

