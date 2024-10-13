/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#define PFX "CAM_CAL"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

// #include <unistd.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/of.h>
#include "cam_cal.h"
#include "cam_cal_define.h"
#include "cam_cal_list.h"
#include <linux/dma-mapping.h>
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#include "eeprom_i2c_sc500cs_dd_driver.h"

static struct i2c_client *g_pstI2CclientG;

#define SC500CS_DD_OTP_DEBUG  1
#define SC500CS_DD_I2C_ID     0x6C /*0x20*/

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, SC500CS_DD_I2C_ID);

	return get_byte;
}

static u16 sc500cs_otp_read_group(u16 addr, u8 *data, u16 length)
{
	u16 i = 0;
	for (i = 0; i < length; i++) {
		data[i] = read_cmos_sensor(addr + i);
#if SC500CS_DD_OTP_DEBUG
	pr_debug("sc500cs_dd addr = 0x%x, data = 0x%x\n", addr + i, data[i]);
#endif
	}
	return 0;
}




int sc500cs_dd_iReadData(unsigned int ui4_offset,
	unsigned int ui4_length, unsigned char *pinputdata)
{
	int i4RetValue = 0;
	int i4ResidueDataLength;
	u32 u4CurrentOffset;
	u8 *pBuff;

	pr_info("ui4_offset = 0x%x, ui4_length = %d \n", ui4_offset, ui4_length);
	i4ResidueDataLength = (int)ui4_length;
	u4CurrentOffset = ui4_offset;
	pBuff = pinputdata;

	i4RetValue =
			sc500cs_otp_read_group(
			(u16) u4CurrentOffset, pBuff, i4ResidueDataLength);
		if (i4RetValue != 0) {
			pr_info("I2C iReadData failed!!\n");
			return -1;
		}
	return 0;
}

unsigned int sc500cs_dd_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
	if (sc500cs_dd_iReadData(addr, size, data) == 0)
		return size;
	else
		return 0;
}

