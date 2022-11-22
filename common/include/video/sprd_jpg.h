/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef	_SPRD_JPG_H
#define _SPRD_JPG_H

#include <linux/ioctl.h>

/*76k vsp address space size*/
#define SPRD_JPG_MAP_SIZE 0x6000

#define SPRD_JPG_IOCTL_MAGIC 'm'
#define JPG_CONFIG_FREQ _IOW(SPRD_JPG_IOCTL_MAGIC, 1, unsigned int)
#define JPG_GET_FREQ    _IOR(SPRD_JPG_IOCTL_MAGIC, 2, unsigned int)
#define JPG_ENABLE      _IO(SPRD_JPG_IOCTL_MAGIC, 3)
#define JPG_DISABLE     _IO(SPRD_JPG_IOCTL_MAGIC, 4)
#define JPG_ACQUAIRE    _IO(SPRD_JPG_IOCTL_MAGIC, 5)
#define JPG_RELEASE     _IO(SPRD_JPG_IOCTL_MAGIC, 6)
#define JPG_START       _IO(SPRD_JPG_IOCTL_MAGIC, 7)
#define JPG_RESET       _IO(SPRD_JPG_IOCTL_MAGIC, 8)
//#define JPG_REG_IRQ       _IO(SPRD_JPG_IOCTL_MAGIC,9)
//#define JPG_UNREG_IRQ       _IO(SPRD_JPG_IOCTL_MAGIC,10) 
#define JPG_ACQUAIRE_MBIO_VLC_DONE _IOR(SPRD_JPG_IOCTL_MAGIC, 9,unsigned int)
	     
enum sprd_jpg_frequency_e{ 
	JPG_FREQENCY_LEVEL_0 = 0,
	JPG_FREQENCY_LEVEL_1 = 1,
	JPG_FREQENCY_LEVEL_2 = 2,
	JPG_FREQENCY_LEVEL_3 = 3
};

/*
ioctl command description
the JPG module user must mmap the jpg module address space to user-space to access 
the hardware. 
JPG_ACQUAIRE:aquaire the jpg module lock
JPG_RELEASE:release the jpg module lock
all other commands must be sent only when the JPG user posses the lock
JPG_ENABLE:enable jpg module clock
JPG_DISABLE:disable jpg module clock
JPG_START:all the preparing work is done and start the jpg module, the jpg module finishes 
its job after this command retruns.
JPG_RESET:reset jpg module hardware
JPG_CONFIG_FREQ/JPG_GET_FREQ:set/get jpg module frequency,the parameter is of 
type sprd_jpg_frequency_e, the smaller the faster
*/

#endif

