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

#include "eeprom_i2c_gc02m1_jk_driver.h"

static struct i2c_client *g_pstI2CclientG;

#define GC02M1_JK_OTP_DEBUG  1
#define GC02M1_JK_I2C_ID     0x6E /*0x20*/

static u16 read_cmos_sensor(u16 addr)
{
	u16 get_byte=0;
	char pu_send_cmd[1] = {(char)(addr & 0xff)};
	iReadRegI2C(pu_send_cmd, 1, (u8 *)&get_byte, 1, GC02M1_JK_I2C_ID);

	return get_byte;
}

static void write_cmos_sensor(u16 addr, u8 para)
{
	char pu_send_cmd[2] = {(char)(addr & 0xff), (char)(para & 0xff) };

	iWriteRegI2C(pu_send_cmd, 2, GC02M1_JK_I2C_ID);
}

static void gc02m1_otp_init(void)
{
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfc, 0x01);
	write_cmos_sensor(0xf4, 0x41);
	write_cmos_sensor(0xf5, 0xc0);
	write_cmos_sensor(0xf6, 0x44);
	write_cmos_sensor(0xf8, 0x38);
	write_cmos_sensor(0xf9, 0x82);
	write_cmos_sensor(0xfa, 0x00);
	write_cmos_sensor(0xfd, 0x80);
	write_cmos_sensor(0xfc, 0x81);
	write_cmos_sensor(0xf7, 0x01);
	write_cmos_sensor(0xfc, 0x80);
	write_cmos_sensor(0xfc, 0x80);
	write_cmos_sensor(0xfc, 0x80);
	write_cmos_sensor(0xfc, 0x80);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xf3, 0x30);
	write_cmos_sensor(0xfe, 0x02);
}

static void gc02m1_otp_close(void)
{
	write_cmos_sensor(0xf7, 0x01);
	write_cmos_sensor(0xfe, 0x00);
}

static u16 gc02m1_otp_read_group(u16 addr, u8 *data, u16 length)
{
	u16 i = 0;
	for (i = 0; i < 15; i++) {
		write_cmos_sensor(0x17, (0x78 + (i * 8)));
		write_cmos_sensor(0xf3, 0x34);
		data[i] = read_cmos_sensor(0x19);
#if GC02M1_JK_OTP_DEBUG
	pr_info("gc02m1_jk addr = 0x%x, data = 0x%x\n", (0x78 + (i * 7)), data[i]);
#endif
	}
	return 0;
}




int gc02m1_jk_iReadData(unsigned int ui4_offset,
	unsigned int ui4_length, unsigned char *pinputdata)
{
	int i4RetValue = 0;
	int i4ResidueDataLength;
	u32 u4CurrentOffset;
	u8 *pBuff;

	pr_debug("ui4_offset = 0x%x, ui4_length = %d \n", ui4_offset, ui4_length);
	i4ResidueDataLength = (int)ui4_length;
	u4CurrentOffset = ui4_offset;
	pBuff = pinputdata;

	gc02m1_otp_init();
	i4RetValue =
			gc02m1_otp_read_group(
			(u16) u4CurrentOffset, pBuff, i4ResidueDataLength);
		if (i4RetValue != 0) {
			pr_debug("I2C iReadData failed!!\n");
			return -1;
		}
	gc02m1_otp_close();

	return 0;
}

unsigned int gc02m1_jk_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
	if (gc02m1_jk_iReadData(addr, size, data) == 0)
		return size;
	else
		return 0;
}

