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

#include "eeprom_i2c_hi846_driver.h"

static struct i2c_client *g_pstI2CclientG;

#define hi846_OTP_DEBUG  1
#define hi846_I2C_ID     0x42 /*0x20*/
struct hi846_otp_struct
{
	int Module_Info_Flag;
	int module_ifo_data[15];
	int WB_FLAG;
	int wb_data[17];
	int LSC_FLAG;
	int lsc_data[1869];
	int AF_FLAG;
	int af_data[5];
}hi846_otp;
static u16 read_cmos_sensor(u16 addr)
{
	u16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, hi846_I2C_ID);

	return get_byte;
}

static void write_cmos_sensor(u16 addr, u16 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 4, hi846_I2C_ID);
}

static void write_cmos_sensor_8(u16 addr, u16 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, hi846_I2C_ID);
}



static void hi846_otp_init(void)
{
	printk("hi846 otp_init_setting\n");
	//sensor init --start
	write_cmos_sensor(0x0a00, 0x0000);
	write_cmos_sensor(0x2000, 0x0000);
	write_cmos_sensor(0x2002, 0x00FF);
	write_cmos_sensor(0x2004, 0x0000);
	write_cmos_sensor(0x2008, 0x3FFF);
	write_cmos_sensor(0x23fe, 0xC056);
	write_cmos_sensor(0x0a00, 0x0000);
	write_cmos_sensor(0x0e04, 0x0012);
	write_cmos_sensor(0x0f08, 0x2f04);
	write_cmos_sensor(0x0f30, 0x001F);
	write_cmos_sensor(0x0f36, 0x001F);
	write_cmos_sensor(0x0f04, 0x3A00);
	write_cmos_sensor(0x0f32, 0x025A);
	write_cmos_sensor(0x0f38, 0x025A);
	write_cmos_sensor(0x0f2a, 0x4124);
	write_cmos_sensor(0x006a, 0x0100);
	write_cmos_sensor(0x004c, 0x0100);
	//sensor init --end
	//otp read setting
	write_cmos_sensor_8(0x0a02, 0x01);
	write_cmos_sensor_8(0x0a00, 0x00);
	mdelay(10);
	write_cmos_sensor_8(0x0f02, 0x00);
	write_cmos_sensor_8(0x071a, 0x01);
	write_cmos_sensor_8(0x071b, 0x09);
	write_cmos_sensor_8(0x0d04, 0x00);
	write_cmos_sensor_8(0x0d00, 0x07);
	write_cmos_sensor_8(0x003e, 0x10);	// OTP R/W mode
	write_cmos_sensor_8(0x0a00, 0x01);	// stand by off
}
static u16 OTP_read_cmos_sensor(u16 addr)
{
    u16 data;   
    write_cmos_sensor_8(0x070a, (addr & 0xff00)>> 8); //start address H        
    write_cmos_sensor_8(0x070b, addr& 0x00ff); //start address L
    write_cmos_sensor_8(0x0702, 0x01); //single read
 //    mDELAY(10);
    data = read_cmos_sensor(0x0708); //OTP data read  
       return data;
}

int hi846_iReadData(unsigned int ui4_offset,
	unsigned int ui4_length, unsigned char *pinputdata)
{       
        //int i4RetValue = 0;
	//int i4ResidueDataLength;
	u32 u4CurrentOffset;
        //pBuff = pinputdata;
        int i, addr = 0;
	int checksum = 0; 
	int module_info_checksum = 0 , wb_data_checksum = 0 , lsc_data_checksum = 0 , af_data_checksum = 0;
        //i4ResidueDataLength = (int)ui4_length;
	u4CurrentOffset = ui4_offset;
        printk("HI846  ui4_offset = %d",u4CurrentOffset);
	hi846_otp_init();

	//1. read module info
	hi846_otp.Module_Info_Flag = OTP_read_cmos_sensor(0x201);
	if (hi846_otp.Module_Info_Flag == 0x01)
		addr = 0x202;
	else if (hi846_otp.Module_Info_Flag == 0x13)
		addr = 0x211;
	else if (hi846_otp.Module_Info_Flag == 0x37)
		addr = 0x220;
	else
		addr = 0;
	printk("HI846 module info start addr = 0x%x \n", addr);

	write_cmos_sensor_8(0x070a, (addr >> 8) & 0xff);
	write_cmos_sensor_8(0x070b, addr & 0xff);
	write_cmos_sensor_8(0x0702, 0x01);

	for (i =0; i <14; i++) 
	{
		pinputdata[i] = read_cmos_sensor(0x708);
		checksum +=pinputdata[i];
		printk("HI846[i] module_ifo_data[%d] = 0x%x", i,i,pinputdata[i]);
	}
	module_info_checksum = read_cmos_sensor(0x708);
	checksum = (checksum % 0xFF) + 1;
	printk("HI846 module_info_checksum = %d, %d  ", module_info_checksum, checksum);
	if (checksum == module_info_checksum)
		{
		printk("HI846_Sensor: Module information checksum PASS\n ");
		}
	else
		{
		printk("HI846_Sensor: Module information checksum Fail\n ");
		}

	//2. read wb
	hi846_otp.WB_FLAG = OTP_read_cmos_sensor(0x22f);
	if (hi846_otp.WB_FLAG == 0x01)
		addr = 0x230;
	else if (hi846_otp.WB_FLAG == 0x13)
		addr = 0x241;
	else if (hi846_otp.WB_FLAG == 0x37)
		addr = 0x252;
	else
		addr = 0;
	printk("HI846 wb start addr = 0x%x \n", addr);

	write_cmos_sensor_8(0x070a, (addr >> 8) & 0xff);
	write_cmos_sensor_8(0x070b, addr & 0xff);
	write_cmos_sensor_8(0x0702, 0x01);

	checksum = 0;
	for (i =0; i <16; i++) 
	{
		pinputdata[i+14] = read_cmos_sensor(0x708);
		checksum += pinputdata[i+14];
		printk("HI846[%d] wb_data[%d] = 0x%x  ", i+14,i,pinputdata[i+14]);
	}
	wb_data_checksum = read_cmos_sensor(0x708);
	checksum = (checksum % 0xFF) + 1;
	printk("HI846 wb_data_checksum = %d, %d  ", wb_data_checksum, checksum);
	if (checksum == wb_data_checksum)
		{
		printk("HI846_Sensor: wb data checksum PASS\n ");
		}
	else
		{
		printk("HI846_Sensor: wb data checksum Fail\n ");
		}

	//3. read lsc
	hi846_otp.LSC_FLAG = OTP_read_cmos_sensor(0x263);
	if (hi846_otp.LSC_FLAG == 0x01)
		addr = 0x264;
	else if (hi846_otp.LSC_FLAG == 0x13)
		addr = 0x9b1;
	else if (hi846_otp.LSC_FLAG == 0x37)
		addr = 0x10fe;
	else
		addr = 0;
	printk("HI846 lsc start addr = 0x%x \n", addr);

	write_cmos_sensor_8(0x070a, (addr >> 8) & 0xff);
	write_cmos_sensor_8(0x070b, addr & 0xff);
	write_cmos_sensor_8(0x0702, 0x01);

	checksum = 0;
	for (i =0; i <1868; i++) 
	{
		pinputdata[i+30] = read_cmos_sensor(0x708);
		checksum += pinputdata[i+30];
		pr_debug("HI846[%d] lsc_data[%d] = 0x%x", i+30, i,pinputdata[i+30]);
	}
	lsc_data_checksum = read_cmos_sensor(0x708);
	checksum = (checksum % 0xFF) + 1;
	printk("HI846 lsc_data_checksum = %d, %d  ", lsc_data_checksum, checksum);
	if (checksum == lsc_data_checksum)
		{
		printk("HI846_Sensor: lsc data checksum PASS\n ");
		}
	else
		{
		printk("HI846_Sensor: lsc data checksum Fail\n ");
		}

	//4. read af
	hi846_otp.AF_FLAG = OTP_read_cmos_sensor(0x184b);
	if (hi846_otp.AF_FLAG == 0x01)
		addr = 0x184c;
	else if (hi846_otp.AF_FLAG == 0x13)
		addr = 0x1851;
	else if (hi846_otp.AF_FLAG == 0x37)
		addr = 0x1856;
	else
		addr = 0;
	printk("HI846 af start addr = 0x%x \n", addr);

	write_cmos_sensor_8(0x070a, (addr >> 8) & 0xff);
	write_cmos_sensor_8(0x070b, addr & 0xff);
	write_cmos_sensor_8(0x0702, 0x01);

	checksum = 0;
	for (i =0; i <4; i++) 
	{
		pinputdata[i+1898] = read_cmos_sensor(0x708);
		checksum += pinputdata[i+1898];
		printk("HI846[%d] af_data[%d] = 0x%x  ", i+1898,i,pinputdata[i+1898]);
	}
	af_data_checksum = read_cmos_sensor(0x708);
	checksum = (checksum % 0xFF) + 1;
	printk("HI846 af_data_checksum = %d, %d  ", af_data_checksum, checksum);
	if (checksum == af_data_checksum)
		{
		printk("HI846_Sensor: af data checksum PASS\n ");
		}
	else
		{
		printk("HI846_Sensor: af data checksum Fail\n ");
		}

	write_cmos_sensor_8(0x0a00, 0x00); //complete
	mdelay(10);
	write_cmos_sensor_8(0x003e, 0x00); //complete
	write_cmos_sensor_8(0x0a00, 0x01); //complete

	printk("%s Exit \n",__func__);
	return 0;
}

unsigned int hi846_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
	if (hi846_iReadData(addr, size, data) == 0)
		return size;
	else
		return 0;
}

