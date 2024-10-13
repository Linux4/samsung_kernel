/*
 * Copyright (C) 2012 Senodia.
 *
 * Author: Tori Xu <xuezhi_xu@senodia.com>
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

/*
 * Definitions for senodia compass chip.
 */
#ifndef MAGNETIC_H
#define MAGNETIC_H

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/ioctl.h>

/*
 * ABS(min~max)
 */
#define ABSMIN_MAG	-32768
#define ABSMAX_MAG	32767

/*
 * device id
 */
#define ST480_DEVICE_ID 0x7C

/*
 * I2C name
 */
#define ST480_I2C_NAME "st480"

/*
 * I2C addr
 */
#define ST480_I2C_ADDR 0x0c

/*
 * IC Package size(choose your ic size)
 */
#define ST480_SIZE_3X3_QFN	0
#define ST480_SIZE_2X2_BGA	0
#define ST480_SIZE_1_6X1_6_LGA	1
#define ST480_SIZE_1_6X1_6_BGA	0

/*
 * register shift
 */
#define ST480_REG_DRR_SHIFT 2

/*
 * BURST MODE(INT)
 */
#define ST480_BURST_MODE 0
#define BURST_MODE_CMD 0x1F
#define BURST_MODE_DATA_LOW 0x01

/*
 * SINGLE MODE
 */
#define ST480_SINGLE_MODE 1
#define SINGLE_MEASUREMENT_MODE_CMD 0x3F

/*
 * register
 */
#define READ_MEASUREMENT_CMD 0x4F
#define WRITE_REGISTER_CMD 0x60
#define READ_REGISTER_CMD 0x50
#define EXIT_REGISTER_CMD 0x80
#define MEMORY_RECALL_CMD 0xD0
#define MEMORY_STORE_CMD 0xE0
#define RESET_CMD 0xF0

#define CALIBRATION_REG (0x02 << ST480_REG_DRR_SHIFT)
#define CALIBRATION_DATA_LOW 0x1C
#define CALIBRATION_DATA_HIGH 0x00

#define ONE_INIT_DATA_LOW 0x7C
#define ONE_INIT_DATA_HIGH 0x00
#define ONE_INIT_REG (0x00 << ST480_REG_DRR_SHIFT)

#define TWO_INIT_DATA_LOW 0x00
#define TWO_INIT_DATA_HIGH 0x00
#define TWO_INIT_REG (0x02 << ST480_REG_DRR_SHIFT)

/*
 * Miscellaneous set. If unsure,don't modify them!
 */
#define IC_CHECK 0

#define ST480_DATA_TRANSFER 1
#define ST480_SMBUS_READ_BYTE_BLOCK 0
#define ST480_SMBUS_WRITE_BYTE_BLOCK 0
#define ST480_SMBUS_READ_BYTE 0
#define ST480_SMBUS_WRITE_BYTE 0

#define MAX_FAILURE_COUNT 3
#define ST480_DEFAULT_DELAY   30
#define ST480_AUTO_TEST 0
#define OLD_KERNEL_VERSION 0

#if ST480_AUTO_TEST
#include <linux/kthread.h>
#endif

/*
 * Debug
 */
#define SENODIA_DEBUG_MSG       0
#define SENODIA_DEBUG_FUNC      0

#if SENODIA_DEBUG_MSG
#define SENODIADBG(format, ...) printk(KERN_INFO "SENODIA " format "\n", ## __VA_ARGS__)
#else
#define SENODIADBG(format, ...)
#endif

#if SENODIA_DEBUG_FUNC
#define SENODIAFUNC(func) printk(KERN_INFO "SENODIA " func " is called\n")
#else
#define SENODIAFUNC(func)
#endif
/*******************************************************************/

struct st480_platform_data {
        u8 axis_map_x;
        u8 axis_map_y;
        u8 axis_map_z;

        u8 negate_x;
        u8 negate_y;
        u8 negate_z;
};

#define SENODIAIO                   0xA1

/* IOCTLs for hal*/
#define ECS_IOCTL_APP_SET_MFLAG		_IOW(SENODIAIO, 0x10, short)
#define ECS_IOCTL_APP_GET_MFLAG		_IOR(SENODIAIO, 0x11, short)
#define ECS_IOCTL_APP_SET_DELAY		_IOW(SENODIAIO, 0x12, short)
#define ECS_IOCTL_APP_GET_DELAY		_IOR(SENODIAIO, 0x13, short)
#define ECS_IOCTL_APP_SET_MVFLAG	_IOW(SENODIAIO, 0x14, short)
#define ECS_IOCTL_APP_GET_MVFLAG	_IOR(SENODIAIO, 0x15, short)

#endif

