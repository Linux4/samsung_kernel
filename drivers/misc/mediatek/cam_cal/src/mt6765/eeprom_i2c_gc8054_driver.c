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

#include "eeprom_i2c_gc8054_driver.h"

static struct i2c_client *g_pstI2CclientG;
/*TabA7 Lite for camera performance add by gaozhenyu 2021/06/15 start*/
#define gc8054_otp_DEBUG  1
#define GC8054_I2C_ID     0x62 /*0x20*/
/*TabA7 Lite for camera performance add by gaozhenyu 2021/06/15 end*/
static u16 read_cmos_sensor(u16 addr)
{
	u16 get_byte=0;
	char pu_send_cmd[2] = { (char)((addr >> 8) & 0xff), (char)(addr & 0xff) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, GC8054_I2C_ID);

	return get_byte;
}

static void write_cmos_sensor(u16 addr, u8 para)
{
	char pu_send_cmd[3] = { (char)((addr >> 8) & 0xff), (char)(addr & 0xff), (char)(para & 0xff) };

	iWriteRegI2C(pu_send_cmd, 3, GC8054_I2C_ID);
}

static void gc8054_otp_init(void)
{
	//write_cmos_sensor(0x031c, 0x60);
	write_cmos_sensor(0x0317, 0x08);
	write_cmos_sensor(0x0a67, 0x80);
	write_cmos_sensor(0x0313, 0x00);
	write_cmos_sensor(0x0a54, 0x00);
	write_cmos_sensor(0x0a65, 0x05);
	write_cmos_sensor(0x0a68, 0x11);
	write_cmos_sensor(0x0a59, 0x00);
}

static void gc8054_otp_close(void)
{
	write_cmos_sensor(0x0317, 0x00);
	write_cmos_sensor(0x0a67, 0x00);
}
/*TabA7 Lite for camera performance add by gaozhenyu 2021/06/15 start*/
static u16 gc8054_otp_read_group(u16 addr, u8 *data, u16 length)
{       
	u16 i = 0;
        u16 gc8054_moudle_flag_addr = 0x1410;
        u8 gc8054_group_moudle_flag = 0x00;
        u16 gc8054_moudle_addr = 0x00;
        u16 gc8054_awb_flag_addr = 0x1580;
        u8 gc8054_group_awb_flag = 0x00;
        u16 gc8054_awb_addr = 0x00;
        u16 gc8054_lsc_flag_addr = 0x1720;
        u8 gc8054_group_lsc_flag = 0x00;
        u16 gc8054_lsc_addr = 0x00;
        u16 gc8054_af_flag_addr = 0xC660;
        u8 gc8054_group_af_flag = 0x00;
        u16 gc8054_af_addr = 0x00;
        int checksum = 0; 
        //u16 y=length-1106;
        int module_info_checksum = 0 , wb_data_checksum = 0 , lsc_data_checksum = 0 , af_data_checksum = 0;
//moudle info
        write_cmos_sensor(0x0313, 0x00);
	write_cmos_sensor(0x0a69, (gc8054_moudle_flag_addr >> 8) & 0xff);
	write_cmos_sensor(0x0a6a, gc8054_moudle_flag_addr & 0xff);
	write_cmos_sensor(0x0313, 0x20);
	write_cmos_sensor(0x0313, 0x12);
        gc8054_group_moudle_flag = read_cmos_sensor(0x0a6c);
        if (gc8054_group_moudle_flag == 0x01)
		gc8054_moudle_addr = 0x1418;
	else if (gc8054_group_moudle_flag == 0x13)
		gc8054_moudle_addr = 0x1490;
	else if (gc8054_group_moudle_flag == 0x37)
		gc8054_moudle_addr = 0x1508;
	else
		gc8054_moudle_addr = 0;
       	write_cmos_sensor(0x0313, 0x00);
	write_cmos_sensor(0x0a69, (gc8054_moudle_addr >> 8) & 0xff);
	write_cmos_sensor(0x0a6a, gc8054_moudle_addr & 0xff);
	write_cmos_sensor(0x0313, 0x20);
	write_cmos_sensor(0x0313, 0x12);
	for (i = 0; i < 15; i++) {
		data[i] = read_cmos_sensor(0x0a6c);
                checksum = data[i];
	pr_debug("moudle_info:i = %d, data = 0x%x\n",i, data[i]);
	}
        module_info_checksum = data[14];
	checksum = (checksum % 0xFF);
	printk("gc8054 module_info_checksum = %d, %d  ", module_info_checksum, checksum);
        	if (checksum == module_info_checksum)
		{
		printk("gc8054 module_info_checksum Module information checksum PASS\n ");
		}
	else
		{
		printk("gc8054 module_info_checksum Module information checksum Fail\n ");
		}
//awb 
        write_cmos_sensor(0x0313, 0x00);
	write_cmos_sensor(0x0a69, (gc8054_awb_flag_addr >> 8) & 0xff);
	write_cmos_sensor(0x0a6a, gc8054_awb_flag_addr & 0xff);
	write_cmos_sensor(0x0313, 0x20);
	write_cmos_sensor(0x0313, 0x12);
        gc8054_group_awb_flag = read_cmos_sensor(0x0a6c);
        if (gc8054_group_awb_flag == 0x01)
		gc8054_awb_addr = 0x1588;
	else if (gc8054_group_awb_flag == 0x13)
		gc8054_awb_addr = 0x1610;
	else if (gc8054_group_awb_flag == 0x37)
		gc8054_awb_addr = 0x1698;
	else
		gc8054_awb_addr = 0;
       	write_cmos_sensor(0x0313, 0x00);
	write_cmos_sensor(0x0a69, (gc8054_awb_addr >> 8) & 0xff);
	write_cmos_sensor(0x0a6a, gc8054_awb_addr & 0xff);
	write_cmos_sensor(0x0313, 0x20);
	write_cmos_sensor(0x0313, 0x12);
	for (i = 0; i < 17; i++) {
		data[i+15] = read_cmos_sensor(0x0a6c);
                checksum = data[i+15];
	pr_debug("awb:i = %d, data = 0x%x\n",i+15, data[i+15]);
	}
        wb_data_checksum = data[31];
        checksum = (checksum % 0xFF);
	printk("gc8054 awb_checksum = %d, %d  ", wb_data_checksum, checksum);
        	if (checksum == wb_data_checksum)
		{
		printk("gc8054 wb_data_checksum awb checksum PASS\n ");
		}
	else
		{
		printk("gc8054 wb_data_checksum awb checksum Fail\n ");
		}
//lsc
        write_cmos_sensor(0x0313, 0x00);
	write_cmos_sensor(0x0a69, (gc8054_lsc_flag_addr >> 8) & 0xff);
	write_cmos_sensor(0x0a6a, gc8054_lsc_flag_addr & 0xff);
	write_cmos_sensor(0x0313, 0x20);
	write_cmos_sensor(0x0313, 0x12);
        gc8054_group_lsc_flag = read_cmos_sensor(0x0a6c);
        if (gc8054_group_lsc_flag == 0x01)
		gc8054_lsc_addr = 0x1728;
	else if (gc8054_group_lsc_flag == 0x13)
		gc8054_lsc_addr = 0x5190;
	else if (gc8054_group_lsc_flag == 0x37)
		gc8054_lsc_addr = 0x8BF8;
	else
		gc8054_lsc_addr = 0;
       	write_cmos_sensor(0x0313, 0x00);
	write_cmos_sensor(0x0a69, (gc8054_awb_addr >> 8) & 0xff);
	write_cmos_sensor(0x0a6a, gc8054_awb_addr & 0xff);
	write_cmos_sensor(0x0313, 0x20);
	write_cmos_sensor(0x0313, 0x12);
	for (i = 0; i < 1869; i++) {
		data[i+32] = read_cmos_sensor(0x0a6c);
                checksum = data[i+32];
	pr_debug("lsc:i = %d, data = 0x%x\n",i+32, data[i+32]);
	}
        lsc_data_checksum = data[1900];
        checksum = (checksum % 0xFF);
	printk("gc8054 lsc_data_checksum = %d, %d  ", lsc_data_checksum, checksum);
        	if (checksum == lsc_data_checksum)
		{
		printk("gc8054 lsc_data_checksum lsc checksum PASS\n ");
		}
	else
		{
		printk("gc8054 lsc_data_checksum lsc checksum Fail\n ");
		}
//af
        write_cmos_sensor(0x0313, 0x00);
	write_cmos_sensor(0x0a69, (gc8054_af_flag_addr >> 8) & 0xff);
	write_cmos_sensor(0x0a6a, gc8054_af_flag_addr & 0xff);
	write_cmos_sensor(0x0313, 0x20);
	write_cmos_sensor(0x0313, 0x12);
        gc8054_group_af_flag = read_cmos_sensor(0x0a6c);
        if (gc8054_group_af_flag == 0x01)
		gc8054_af_addr = 0xC668;
	else if (gc8054_group_af_flag == 0x13)
		gc8054_af_addr = 0xC6E0;
	else if (gc8054_group_af_flag == 0x37)
		gc8054_af_addr = 0xC758;
	else
		gc8054_af_addr = 0;
       	write_cmos_sensor(0x0313, 0x00);
	write_cmos_sensor(0x0a69, (gc8054_af_addr >> 8) & 0xff);
	write_cmos_sensor(0x0a6a, gc8054_af_addr & 0xff);
	write_cmos_sensor(0x0313, 0x20);
	write_cmos_sensor(0x0313, 0x12);
	for (i = 0; i < 15; i++) {
		data[i+1901] = read_cmos_sensor(0x0a6c);
                checksum = data[i+1901];
	pr_debug("af:i = %d, data = 0x%x\n",i+1901, data[i+1901]);
	}
        af_data_checksum = data[1915];
        checksum = (checksum % 0xFF);
	printk("gc8054 af_data_checksum = %d, %d  ", af_data_checksum, checksum);
        	if (checksum == af_data_checksum)
		{
		printk("gc8054 af_data_checksum af checksum PASS\n ");
		}
	else
		{
		printk("gc8054 af_data_checksum af checksum Fail\n ");
		}
//other
/*
pr_debug("other = %d\n", y);
for (i = 0; i < y; i++) {
		data[i+1106] = 0x00;
                pr_debug("other:i = %d, data = 0x%x\n",i+1106, data[i+1106]);
}
*/
	return 0;
}

/*TabA7 Lite for camera performance add by gaozhenyu 2021/06/15 end*/
/*TabA7 Lite for camera performance add by gaozhenyu 2021/06/15 start*/
/*TabA7 Lite code for SR-AX3565-01-320 by wangqi at 20210104 start*/
int gc8054_iReadData(unsigned int ui4_offset,
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

	gc8054_otp_init();

	i4RetValue =
			gc8054_otp_read_group(
			(u16) u4CurrentOffset, pBuff, i4ResidueDataLength);
		if (i4RetValue != 0) {
			pr_debug("I2C iReadData failed!!\n");
			return -1;
		}
	gc8054_otp_close();

	return 0;
}
/*TabA7 Lite code for SR-AX3565-01-320 by wangqi at 20210104 end*/

unsigned int gc8054_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
        printk("gc8054 eeprom enter\n ");
	if (gc8054_iReadData(addr, size, data) == 0)
		return size;
	else
		return 0;
}
/*TabA7 Lite for camera performance add by gaozhenyu 2021/06/15 end*/
