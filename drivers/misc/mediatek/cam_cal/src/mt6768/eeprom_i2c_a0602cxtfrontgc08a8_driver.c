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

#include "eeprom_i2c_a0602cxtfrontgc08a8_driver.h"

static struct i2c_client *g_pstI2CclientG;

#define GC08A3_OTP_DEBUG  1
#define GC08A3_I2C_ID     0x62 /*0x20*/

static u16 read_cmos_sensor(u16 addr)
{
	u16 get_byte=0;
	char pu_send_cmd[2] = { (char)((addr >> 8) & 0xff), (char)(addr & 0xff) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, GC08A3_I2C_ID);

	return get_byte;
}

static void write_cmos_sensor(u16 addr, u8 para)
{
	char pu_send_cmd[3] = { (char)((addr >> 8) & 0xff), (char)(addr & 0xff), (char)(para & 0xff) };

	iWriteRegI2C(pu_send_cmd, 3, GC08A3_I2C_ID);
}

static void gc08a3_otp_init(void)
{
	write_cmos_sensor(0x0315, 0x80);
	write_cmos_sensor(0x031c, 0x60);

	write_cmos_sensor(0x0324, 0x42);
	write_cmos_sensor(0x0316, 0x09);
	write_cmos_sensor(0x0a67, 0x80);
	write_cmos_sensor(0x0313, 0x00);
	write_cmos_sensor(0x0a53, 0x0e);
	write_cmos_sensor(0x0a65, 0x17);
	write_cmos_sensor(0x0a68, 0xa1);
	write_cmos_sensor(0x0a47, 0x00);
	write_cmos_sensor(0x0a58, 0x00);
	write_cmos_sensor(0x0ace, 0x0c);
}

static void gc08a3_otp_close(void)
{
	write_cmos_sensor(0x0316, 0x01);
	write_cmos_sensor(0x0a67, 0x00);
}

static u16 gc08a3_otp_read_group(u16 addr, u8 *data, u16 length)
{
	u16 i = 0;

	write_cmos_sensor(0x0313, 0x00);
	write_cmos_sensor(0x0a69, (addr >> 8) & 0xff);
	write_cmos_sensor(0x0a6a, addr & 0xff);
	write_cmos_sensor(0x0313, 0x20);
	write_cmos_sensor(0x0313, 0x12);

	for (i = 0; i < length; i++) {
		data[i] = read_cmos_sensor(0x0a6c);
#if GC08A3_OTP_DEBUG
	pr_debug("addr = 0x%x, data = 0x%x\n", addr + i * 8, data[i]);
#endif
	}
	return 0;
}


int gc08a3_iReadData(unsigned int ui4_offset,
	unsigned int ui4_length, unsigned char *pinputdata)
{
	int i4RetValue = 0;

	pr_debug("gc08a3 otp ui4_offset = 0x%x, ui4_length = %d \n", ui4_offset, ui4_length);

	gc08a3_otp_init();
	mdelay(10);

	i4RetValue = gc08a3_otp_read_group(ui4_offset, pinputdata, ui4_length);
	if (i4RetValue != 0) {
		pr_debug("I2C iReadData failed!!\n");
		return -1;
	}

	gc08a3_otp_close();

	return 0;
}

unsigned int a0602cxtfrontgc08a8_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
	if (gc08a3_iReadData(addr, size, data) == 0)
		return size;
	else
		return 0;
}

