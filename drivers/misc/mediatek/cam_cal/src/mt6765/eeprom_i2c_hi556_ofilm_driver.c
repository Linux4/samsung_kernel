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

#include "eeprom_i2c_hi556_ofilm_driver.h"

static struct i2c_client *g_pstI2CclientG;

#define hi556_ofilm_OTP_DEBUG  1
#define hi556_ofilm_I2C_ID     0x50 /*0x20*/
struct hi556_ofilm_otp_struct
{
	int Module_Info_Flag;
	int module_ifo_data[15];
	int WB_FLAG;
	int wb_data[17];
	int LSC_FLAG;
	int lsc_data[1869];
	int AF_FLAG;
	int af_data[5];
}hi556_ofilm_otp;
static u16 read_cmos_sensor(u16 addr)
{
	u16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, hi556_ofilm_I2C_ID);

	return get_byte;
}

static void write_cmos_sensor(u16 addr, u16 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 4, hi556_ofilm_I2C_ID);
}

static void write_cmos_sensor_8(u16 addr, u16 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, hi556_ofilm_I2C_ID);
}



static void hi556_ofilm_otp_init(void)
{
	printk("hi556_ofilm otp_init_setting\n");
	//sensor init --start
	write_cmos_sensor(0x0e00, 0x0102);
	write_cmos_sensor(0x0e02, 0x0102);
	write_cmos_sensor(0x0e0c, 0x0100);
	write_cmos_sensor(0x27fe, 0xe000);
	write_cmos_sensor(0x0b0e, 0x8600);
	write_cmos_sensor(0x0d04, 0x0100);
	write_cmos_sensor(0x0d02, 0x0707);
	write_cmos_sensor(0x0f30, 0x6e25);
	write_cmos_sensor(0x0f32, 0x7067);
	write_cmos_sensor(0x0f02, 0x0106);
	write_cmos_sensor(0x0a04, 0x0000);
	write_cmos_sensor(0x0e0a, 0x0001);
	write_cmos_sensor(0x004a, 0x0100);
	write_cmos_sensor(0x003e, 0x1000);
	write_cmos_sensor(0x0a00, 0x0100);
	//sensor init --end
	//otp read setting
	write_cmos_sensor_8(0x0A02, 0x01);
	write_cmos_sensor_8(0x0A00, 0x00);
	mdelay(10); 
	write_cmos_sensor_8(0x0F02, 0x00);
	write_cmos_sensor_8(0x011A, 0x01);
	write_cmos_sensor_8(0x011B, 0x09);
	write_cmos_sensor_8(0x0d04, 0x01);
	write_cmos_sensor_8(0x0d02, 0x07);
	write_cmos_sensor_8(0x003E, 0x10);
	write_cmos_sensor_8(0x0A00, 0x01);
}
static u16 OTP_read_cmos_sensor(u16 addr)
{
    u16 data;   
	write_cmos_sensor_8 (0x10a, ((addr )>>8)&0xff); // start address H
	write_cmos_sensor_8 (0x10b, addr&0xff); // start address L
	write_cmos_sensor_8 (0x102, 0x01); // single read
 //    mDELAY(10);
    data = read_cmos_sensor(0x0108); //OTP data read  
       return data;
}

int hi556_ofilm_iReadData(unsigned int ui4_offset,
	unsigned int ui4_length, unsigned char *pinputdata)
{       
        //int i4RetValue = 0;
	//int i4ResidueDataLength;
	u32 u4CurrentOffset;
        //pBuff = pinputdata;
        int i, addr = 0;
	int checksum = 0; 
	int module_info_checksum = 0 , wb_data_checksum = 0 , lsc_data_checksum = 0 ;
        //i4ResidueDataLength = (int)ui4_length;
	u4CurrentOffset = ui4_offset;
        printk("HI556_OFILM  ui4_offset = %d",u4CurrentOffset);
	hi556_ofilm_otp_init();

	//1. read module info
	hi556_ofilm_otp.Module_Info_Flag = OTP_read_cmos_sensor(0x401);
	if (hi556_ofilm_otp.Module_Info_Flag == 0x37)
		addr = 0x424;
	else if (hi556_ofilm_otp.Module_Info_Flag == 0x13)
		addr = 0x413;
	else if (hi556_ofilm_otp.Module_Info_Flag == 0x01)
		addr = 0x402;
	else
		addr = 0;
	printk("HI556_OFILM module info start addr = 0x%x \n", addr);

	write_cmos_sensor_8 (0x10a, ((addr )>>8)&0xff); // start address H
	write_cmos_sensor_8 (0x10b, addr&0xff); // start address L
	write_cmos_sensor_8 (0x102, 0x01); // single read

	for (i =0; i <16; i++) 
	{
		pinputdata[i] = read_cmos_sensor(0x108);
		checksum +=pinputdata[i];
		printk("HI556_OFILM[%d] module_ifo_data[%d] = 0x%x", i,i,pinputdata[i]);
	}
	module_info_checksum = read_cmos_sensor(0x108);
	checksum = (checksum % 0xFF) + 1;
	printk("HI556_OFILM module_info_checksum = %d, %d  ", module_info_checksum, checksum);
	if (checksum == module_info_checksum)
		{
		printk("HI556_OFILM_Sensor: Module information checksum PASS\n ");
		}
	else
		{
		printk("HI556_OFILM_Sensor: Module information checksum Fail\n ");
		}

	//2. read wb
	hi556_ofilm_otp.WB_FLAG = OTP_read_cmos_sensor(0x435);
	if (hi556_ofilm_otp.WB_FLAG == 0x37)
		addr = 0x472;
	else if (hi556_ofilm_otp.WB_FLAG == 0x13)
		addr = 0x454;
	else if (hi556_ofilm_otp.WB_FLAG == 0x01)
		addr = 0x436;
	else
		addr = 0;
	printk("HI556_OFILM wb start addr = 0x%x \n", addr);

	write_cmos_sensor_8 (0x10a, ((addr )>>8)&0xff); // start address H
	write_cmos_sensor_8 (0x10b, addr&0xff); // start address L
	write_cmos_sensor_8 (0x102, 0x01); // single read

	checksum = 0;
	for (i =0; i <29; i++) 
	{
		pinputdata[i+16] = read_cmos_sensor(0x108);
		checksum += pinputdata[i+16];
		printk("HI556_OFILM[%d] wb_data[%d] = 0x%x  ", i+16,i,pinputdata[i+16]);
	}
	wb_data_checksum = read_cmos_sensor(0x108);
	checksum = (checksum % 0xFF) + 1;
	printk("HI556_OFILM wb_data_checksum = %d, %d  ", wb_data_checksum, checksum);
	if (checksum == wb_data_checksum)
		{
		printk("HI556_OFILM_Sensor: wb data checksum PASS\n ");
		}
	else
		{
		printk("HI556_OFILM_Sensor: wb data checksum Fail\n ");
		}

	//3. read lsc
	hi556_ofilm_otp.LSC_FLAG = OTP_read_cmos_sensor(0x490);
	if (hi556_ofilm_otp.LSC_FLAG == 0x37)
		addr = 0x1339;
	else if (hi556_ofilm_otp.LSC_FLAG == 0x13)
		addr = 0xbe5;
	else if (hi556_ofilm_otp.LSC_FLAG == 0x01)
		addr = 0x491;
	else
		addr = 0;
	printk("HI556_OFILM lsc start addr = 0x%x \n", addr);

	write_cmos_sensor_8 (0x10a, ((addr )>>8)&0xff); // start address H
	write_cmos_sensor_8 (0x10b, addr&0xff); // start address L
	write_cmos_sensor_8 (0x102, 0x01); // single read

	checksum = 0;
	for (i =0; i <1868; i++) 
	{
		pinputdata[i+45] = read_cmos_sensor(0x108);
		checksum += pinputdata[i+45];
		pr_debug("HI556_OFILM[%d] lsc_data[%d] = 0x%x", i+45, i,pinputdata[i+45]);
	}
	lsc_data_checksum = read_cmos_sensor(0x108);
	checksum = (checksum % 0xFF) + 1;
	printk("HI556_OFILM lsc_data_checksum = %d, %d  ", lsc_data_checksum, checksum);
	if (checksum == lsc_data_checksum)
		{
		printk("HI556_OFILM_Sensor: lsc data checksum PASS\n ");
		}
	else
		{
		printk("HI556_OFILM_Sensor: lsc data checksum Fail\n ");
		}

	//4. read af
/*	hi556_ofilm_otp.AF_FLAG = OTP_read_cmos_sensor(0x184b);
	if (hi556_ofilm_otp.AF_FLAG == 0x01)
		addr = 0x184c;
	else if (hi556_ofilm_otp.AF_FLAG == 0x13)
		addr = 0x1851;
	else if (hi556_ofilm_otp.AF_FLAG == 0x37)
		addr = 0x1856;
	else
		addr = 0;
	printk("HI556_OFILM af start addr = 0x%x \n", addr);

	write_cmos_sensor_8(0x070a, (addr >> 8) & 0xff);
	write_cmos_sensor_8(0x070b, addr & 0xff);
	write_cmos_sensor_8(0x0702, 0x01);

	checksum = 0;
	for (i =0; i <4; i++) 
	{
		pinputdata[i+1898] = read_cmos_sensor(0x108);
		checksum += pinputdata[i+1898];
		printk("HI556_OFILM[%d] af_data[%d] = 0x%x  ", i+1898,i,pinputdata[i+1898]);
	}
	af_data_checksum = read_cmos_sensor(0x708);
	checksum = (checksum % 0xFF) + 1;
	printk("HI556_OFILM af_data_checksum = %d, %d  ", af_data_checksum, checksum);
	if (checksum == af_data_checksum)
		{
		printk("HI556_OFILM_Sensor: af data checksum PASS\n ");
		}
	else
		{
		printk("HI556_OFILM_Sensor: af data checksum Fail\n ");
		}*/

	write_cmos_sensor_8(0x0a00, 0x00); //complete
	mdelay(10);
	write_cmos_sensor_8(0x003e, 0x00); //complete
	write_cmos_sensor_8(0x0a00, 0x01); //complete

	printk("%s Exit \n",__func__);
	return 0;
}

unsigned int hi556_ofilm_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
	if (hi556_ofilm_iReadData(addr, size, data) == 0)
		return size;
	else
		return 0;
}

