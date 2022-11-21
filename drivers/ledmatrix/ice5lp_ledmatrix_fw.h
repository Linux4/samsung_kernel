/*
 * Copyright (C) 2013 Samsung Electronics
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
 */

/* FIRMWARE VERSION : 22 72 */
/* TARGET NAME	    : K3G / KQ */
/* CHIP		    : 640 */
#ifndef _ICE5LP_LEDMATRIX_FW_H_
#define _ICE5LP_LEDMATRIX_FW_H_

#define GPIO_LEVEL_LOW          0
#define GPIO_LEVEL_HIGH         1
#define GPIO_LEVEL_NONE         2

static int ice5lp_check_fwdone(struct i2c_client *client);

#define IR_IOCTL_BASE		'I'
#define IR_IOCTL_SET_FREQ	_IOW(IR_IOCTL_BASE, 1, int)
#define IR_IOCTL_SET_SIZE	_IOW(IR_IOCTL_BASE, 2, int)
#define IR_IOCTL_SET_DATA	_IOW(IR_IOCTL_BASE, 3, int*)
#define IR_IOCTL_START		_IO(IR_IOCTL_BASE, 4)
#define IR_IOCTL_STOP		_IO(IR_IOCTL_BASE, 5)

#endif /* _ICE5LP_LEDMATRIX_FW_H_ */
