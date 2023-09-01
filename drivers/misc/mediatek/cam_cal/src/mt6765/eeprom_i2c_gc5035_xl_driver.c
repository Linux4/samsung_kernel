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

#include "eeprom_i2c_gc5035_xl_driver.h"
#define kal_uint32 unsigned int
static struct i2c_client *g_pstI2CclientG;

#define gc5035_xl_OTP_DEBUG  1
#define gc5035_xl_I2C_ID     0x6e /*0x20*/
struct gc5035_xl_otp_struct
{
	int Module_Info_Flag;
	int module_ifo_data[15];
	int WB_FLAG;
	int wb_data[17];
	int LSC_FLAG;
	int lsc_data[1869];
	int AF_FLAG;
	int af_data[5];
}gc5035_xl_otp;
/*static u16 read_cmos_sensor(u16 addr)
{
	u16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, gc5035_xl_I2C_ID);

	return get_byte;
}*/

/*static void write_cmos_sensor(u16 addr, u16 para)
{
	char pu_send_cmd[4] = {(char)(addr),
		(char)(addr), (char)(para), (char)(para)};

	iWriteRegI2C(pu_send_cmd, 2, gc5035_xl_I2C_ID);
}*/
/*
static void write_cmos_sensor_8(u16 addr, u16 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, gc5035_xl_I2C_ID);
}*/



static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[2] = { (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 2, gc5035_xl_I2C_ID);
}


static u16 read_cmos_sensor(u16 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[1] = { (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 1, (u8 *)&get_byte, 1, gc5035_xl_I2C_ID);

	return get_byte;
}

/*
static u16 read_cmos_sensor(u16 addr)
{
	u16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, gc5035_xl_I2C_ID);

	return get_byte;
}*/

static kal_uint8 gc5035_otp_read_byte(kal_uint16 addr)
{
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x69, (addr >> 8) & 0x1f);
	write_cmos_sensor(0x6a, addr & 0xff);
	write_cmos_sensor(0xf3, 0x20);
	write_cmos_sensor(0xf3, 0x12);

	return read_cmos_sensor(0x6c);
}

static void gc5035_xl_otp_init(void)
{
	printk("gc5035_xl otp_init_setting\n");
	//sensor init --start
	write_cmos_sensor(0xfc, 0x01);
	write_cmos_sensor(0xf4, 0x40);
	write_cmos_sensor(0xf5, 0xe9);
	write_cmos_sensor(0xf6, 0x14);
	write_cmos_sensor(0xf8, 0x49);
	write_cmos_sensor(0xf9, 0x82);
	write_cmos_sensor(0xfa, 0x00);
	write_cmos_sensor(0xfc, 0x81);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0x36, 0x01);
	write_cmos_sensor(0xd3, 0x87);
	write_cmos_sensor(0x36, 0x00);
	write_cmos_sensor(0x33, 0x00);
	write_cmos_sensor(0xf7, 0x01);
	write_cmos_sensor(0xfc, 0x8e);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xee, 0x30);
	write_cmos_sensor(0xfa, 0x10);
	write_cmos_sensor(0xf5, 0xe9);
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x67, 0xc0);
	write_cmos_sensor(0x59, 0x3f);
	write_cmos_sensor(0x55, 0x84);
	write_cmos_sensor(0x65, 0x80);
	write_cmos_sensor(0x66, 0x03);
	write_cmos_sensor(0xfe, 0x00); //otp init
}

int gc5035_xl_iReadData(unsigned int ui4_offset,
	unsigned int ui4_length, unsigned char *pinputdata)
{       
        //int i4RetValue = 0;
	//int i4ResidueDataLength;
	u32 u4CurrentOffset;
        //pBuff = pinputdata;
        int i, addr = 0;
	int checksum = 0; 
	int module_info_checksum = 0 , wb_data_checksum = 0;
        //i4ResidueDataLength = (int)ui4_length;
	u4CurrentOffset = ui4_offset;
        printk("GC5035_xl  ui4_offset = %d",u4CurrentOffset);
	gc5035_xl_otp_init();

	//1. read module info
	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x69, (0xa48 >> 8) & 0x1f);
	write_cmos_sensor(0x6a, 0xa48 & 0xff);
	write_cmos_sensor(0xf3, 0x20);
	write_cmos_sensor(0xf3, 0x12);

	gc5035_xl_otp.Module_Info_Flag = read_cmos_sensor(0x6c);//gc5035_otp_read_byte(0xa48);
	if (gc5035_xl_otp.Module_Info_Flag == 0x37)
		addr = 0xb10;
	else if (gc5035_xl_otp.Module_Info_Flag == 0x13)
		addr = 0xab0;
	else if (gc5035_xl_otp.Module_Info_Flag == 0x01)
		addr = 0xa50;
	else
		addr = 0;
	printk("GC5035_XL module info start addr = 0x%x %d \n", addr,gc5035_xl_otp.Module_Info_Flag);

	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x69, (addr >> 8) & 0x1f);          //otp起始地址高位
	write_cmos_sensor(0x6a, addr & 0xff);                 //otp起始地址低位
	write_cmos_sensor(0xf3, 0x20);
	write_cmos_sensor(0xf3, 0x12);

	for (i = 0; i < 11; i++)                                    //otp读取数据个数
	{
	pinputdata[i] = read_cmos_sensor(0x6c);  //otp data
		//pinputdata[i] = read_cmos_sensor(0x6c);
	checksum +=pinputdata[i];
	printk("GC5035_XL[%d] module_ifo_data[%d] = 0x%x", i,i,pinputdata[i]);
	}
	module_info_checksum = read_cmos_sensor(0x6c);


	write_cmos_sensor(0xf3, 0x00);
	checksum = (checksum % 0xFF) + 1;
	printk("GC5035_XL module_info_checksum = %d, %d  ", module_info_checksum, checksum);
	if (checksum == module_info_checksum)
		{
		printk("GC5035_XL_Sensor: Module information checksum PASS\n ");
		}
	else
		{
		printk("GC5035_XL_Sensor: Module information checksum Fail\n ");
		}

	//2. read wb
	gc5035_xl_otp.WB_FLAG = gc5035_otp_read_byte(0xb70);
	if (gc5035_xl_otp.WB_FLAG == 0x37)
		addr = 0xc48;
	else if (gc5035_xl_otp.WB_FLAG == 0x13)
		addr = 0xbe0;
	else if (gc5035_xl_otp.WB_FLAG == 0x01)
		addr = 0xb78;
	else
		addr = 0;
	printk("GC5035_XL wb start addr = 0x%x \n", addr);

	//write_cmos_sensor_8 (0x10a, ((addr )>>8)&0xff); // start address H
	//write_cmos_sensor_8 (0x10b, addr&0xff); // start address L
	//write_cmos_sensor_8 (0x102, 0x01); // single read

	checksum = 0;

	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x69, (addr >> 8) & 0x1f);          //otp起始地址高位
	write_cmos_sensor(0x6a, addr & 0xff);                 //otp起始地址低位
	write_cmos_sensor(0xf3, 0x20);
	write_cmos_sensor(0xf3, 0x12);

	for (i =0; i <12; i++) 
	{
		pinputdata[i+11] = read_cmos_sensor(0x6c);
		checksum += pinputdata[i+11];
		printk("GC5035_XL[%d] wb_data[%d] = 0x%x  ", i+11,i,pinputdata[i+11]);
	}
	wb_data_checksum = read_cmos_sensor(0x6c);
	write_cmos_sensor(0xf3, 0x00);
	checksum = (checksum % 0xFF) + 1;
	printk("GC5035_XL wb_data_checksum = %d, %d  ", wb_data_checksum, checksum);
	if (checksum == wb_data_checksum)
		{
		printk("GC5035_XL_Sensor: wb data checksum PASS\n ");
		}
	else
		{
		printk("GC5035_XL_Sensor: wb data checksum Fail\n ");
		}

	//3. read lsc
/*	gc5035_xl_otp.LSC_FLAG = OTP_read_cmos_sensor(0x490);
	if (gc5035_xl_otp.LSC_FLAG == 0x37)
		addr = 0x1339;
	else if (gc5035_xl_otp.LSC_FLAG == 0x13)
		addr = 0xbe5;
	else if (gc5035_xl_otp.LSC_FLAG == 0x01)
		addr = 0x491;
	else
		addr = 0;
	printk("GC5035_XL lsc start addr = 0x%x \n", addr);

	write_cmos_sensor_8 (0x10a, ((addr )>>8)&0xff); // start address H
	write_cmos_sensor_8 (0x10b, addr&0xff); // start address L
	write_cmos_sensor_8 (0x102, 0x01); // single read

	checksum = 0;
	for (i =0; i <1868; i++) 
	{
		pinputdata[i+30] = read_cmos_sensor(0x108);
		checksum += pinputdata[i+30];
		printk("GC5035_XL[%d] lsc_data[%d] = 0x%x", i+30, i,pinputdata[i+30]);
	}
	lsc_data_checksum = read_cmos_sensor(0x108);
	checksum = (checksum % 0xFF) + 1;
	printk("GC5035_XL lsc_data_checksum = %d, %d  ", lsc_data_checksum, checksum);
	if (checksum == lsc_data_checksum)
		{
		printk("GC5035_XL_Sensor: lsc data checksum PASS\n ");
		}
	else
		{
		printk("GC5035_XL_Sensor: lsc data checksum Fail\n ");
		}*/

	//4. read af
/*	gc5035_xl_otp.AF_FLAG = OTP_read_cmos_sensor(0x184b);
	if (gc5035_xl_otp.AF_FLAG == 0x01)
		addr = 0x184c;
	else if (gc5035_xl_otp.AF_FLAG == 0x13)
		addr = 0x1851;
	else if (gc5035_xl_otp.AF_FLAG == 0x37)
		addr = 0x1856;
	else
		addr = 0;
	printk("GC5035_XL af start addr = 0x%x \n", addr);

	write_cmos_sensor_8(0x070a, (addr >> 8) & 0xff);
	write_cmos_sensor_8(0x070b, addr & 0xff);
	write_cmos_sensor_8(0x0702, 0x01);

	checksum = 0;
	for (i =0; i <4; i++) 
	{
		pinputdata[i+1898] = read_cmos_sensor(0x108);
		checksum += pinputdata[i+1898];
		printk("GC5035_XL[%d] af_data[%d] = 0x%x  ", i+1898,i,pinputdata[i+1898]);
	}
	af_data_checksum = read_cmos_sensor(0x708);
	checksum = (checksum % 0xFF) + 1;
	printk("GC5035_XL af_data_checksum = %d, %d  ", af_data_checksum, checksum);
	if (checksum == af_data_checksum)
		{
		printk("GC5035_XL_Sensor: af data checksum PASS\n ");
		}
	else
		{
		printk("GC5035_XL_Sensor: af data checksum Fail\n ");
		}*/

	write_cmos_sensor(0xfe, 0x02);
	write_cmos_sensor(0x67, 0x00);
	write_cmos_sensor(0xfe, 0x00);
	write_cmos_sensor(0xfa, 0x00);

	printk("%s Exit \n",__func__);
	return 0;
}

unsigned int gc5035_xl_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
	if (gc5035_xl_iReadData(addr, size, data) == 0)
		return size;
	else
		return 0;
}

