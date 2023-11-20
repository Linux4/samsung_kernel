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


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/debugfs.h>
#include <video/sensor_drv_k.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/vmalloc.h>
#include <linux/sprd_mm.h>
#include "../sensor_drv_sprd.h"
#include "sensor_otp.h"
#define SENSOR_I2C_FREQ_400			(0x04 << 5)
#define SENSOR_I2C_REG_16BIT		(0x01<<1)
#define SENSOR_I2C_REG_8BIT			(0x00<<1)
#define DW9714_VCM_SLAVE_ADDR		(0x18>>1)

extern int sensor_identify(void* data);

int sensor_reloadinfo_thread (void *data)
{
	/*identify sensor and save result*/
	sensor_identify(data);

	/*opt operation*/

	return 0;
}

