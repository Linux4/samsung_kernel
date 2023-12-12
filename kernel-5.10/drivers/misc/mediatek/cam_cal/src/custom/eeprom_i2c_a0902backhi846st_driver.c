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
#include <linux/dma-mapping.h>
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#include "eeprom_i2c_a0902backhi846st_driver.h"

static struct i2c_client *g_pstI2CclientG;
#define EEPROM_I2C_MSG_SIZE_READ 2
#define EEPROM_I2C_WRITE_MSG_LENGTH_MAX 32
struct hi846_otp_struct
{
	int Module_Info_Flag;
	int WB_FLAG;
	int wb_data[14];
	int LSC_FLAG;
	int AF_FLAG;
}hi846_otp;

static int Hi846_Otp_Read_I2C_CAM_CAL(u16 a_u2Addr)
{
	int i4RetValue = 0;
	u16 data;
	char puReadCmd[2] = { (char)(a_u2Addr >> 8), (char)(a_u2Addr & 0xFF) };
	struct i2c_msg msg[EEPROM_I2C_MSG_SIZE_READ];
    u8 a_puBuff[1];
	msg[0].addr = g_pstI2CclientG->addr;
	msg[0].flags = g_pstI2CclientG->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = puReadCmd;

	msg[1].addr = g_pstI2CclientG->addr;
	msg[1].flags = g_pstI2CclientG->flags & I2C_M_TEN;
	msg[1].flags |= I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = a_puBuff;

	i4RetValue = i2c_transfer(g_pstI2CclientG->adapter, msg,
				EEPROM_I2C_MSG_SIZE_READ);

	if (i4RetValue != EEPROM_I2C_MSG_SIZE_READ) {
		printk("Hi846 I2C read data failed!!\n");
		return -1;
	}
	data = a_puBuff[0];
	return data;
}

static int Hi846_Otp_Write_I2C_CAM_CAL(u16 a_u2Addr,u16 parameter)
{
	int i4RetValue = 0;
	char puCmd[4];
	struct i2c_msg msg;
	puCmd[0] = (char)(a_u2Addr >> 8);
	puCmd[1] = (char)(a_u2Addr & 0xFF);
	puCmd[2] = (char)(parameter >> 8);
	puCmd[3] = (char)(parameter & 0xFF);
	msg.addr = g_pstI2CclientG->addr;
	msg.flags = g_pstI2CclientG->flags & I2C_M_TEN;
	msg.len = 4;
	msg.buf = puCmd;

	i4RetValue = i2c_transfer(g_pstI2CclientG->adapter, &msg, 1);

	if (i4RetValue != 1) {
		printk("Hi846 I2C write data failed!!\n");
		return -1;
	}
	mdelay(5);
	return 0;
}

static int Hi846_Otp_Write_I2C_CAM_CAL_U8(u16 a_u2Addr,u8 parameter)
{
	int i4RetValue = 0;
	char puCmd[3];
	struct i2c_msg msg;
	puCmd[0] = (char)(a_u2Addr >> 8);
	puCmd[1] = (char)(a_u2Addr & 0xFF);
	puCmd[2] = (char)(parameter & 0xFF);
	msg.addr = g_pstI2CclientG->addr;
	msg.flags = g_pstI2CclientG->flags & I2C_M_TEN;
	msg.len = 3;
	msg.buf = puCmd;

	i4RetValue = i2c_transfer(g_pstI2CclientG->adapter, &msg, 1);

	if (i4RetValue != 1) {
		printk("Hi846 I2C write data failed!!\n");
		return -1;
	}
	mdelay(5);
	return 0;
}
static u16 read_cmos_sensor(u16 addr)
{
	u16 get_byte = 0xFF;
    get_byte = Hi846_Otp_Read_I2C_CAM_CAL(addr);

	return get_byte;
}


static void Hi846_Otp_init(void)
{
	printk("Hi846 otp_init_setting");
    Hi846_Otp_Write_I2C_CAM_CAL(0x0A00, 0x0000),
    Hi846_Otp_Write_I2C_CAM_CAL(0x2000, 0x0000),
    Hi846_Otp_Write_I2C_CAM_CAL(0x2002, 0x00FF),
    Hi846_Otp_Write_I2C_CAM_CAL(0x2004, 0x0000),
    Hi846_Otp_Write_I2C_CAM_CAL(0x2008, 0x3FFF),
    Hi846_Otp_Write_I2C_CAM_CAL(0x23FE, 0xC056),
    Hi846_Otp_Write_I2C_CAM_CAL(0x326E, 0x0000),
    Hi846_Otp_Write_I2C_CAM_CAL(0x0A00, 0x0000),
    Hi846_Otp_Write_I2C_CAM_CAL(0x0E04, 0x0012),
    Hi846_Otp_Write_I2C_CAM_CAL(0x0F08, 0x2F04),
    Hi846_Otp_Write_I2C_CAM_CAL(0x0F30, 0x001F),
    Hi846_Otp_Write_I2C_CAM_CAL(0x0F36, 0x001F),
    Hi846_Otp_Write_I2C_CAM_CAL(0x0F04, 0x3A00),
    Hi846_Otp_Write_I2C_CAM_CAL(0x0F32, 0x025A),
    Hi846_Otp_Write_I2C_CAM_CAL(0x0F38, 0x025A),
    Hi846_Otp_Write_I2C_CAM_CAL(0x0F2A, 0x4124),
    Hi846_Otp_Write_I2C_CAM_CAL(0x006A, 0x0100),
    Hi846_Otp_Write_I2C_CAM_CAL(0x004C, 0x0100),

    Hi846_Otp_Write_I2C_CAM_CAL_U8(0x0a02, 0x01);
    Hi846_Otp_Write_I2C_CAM_CAL_U8(0x0a00, 0x00);
	mdelay(10);
    Hi846_Otp_Write_I2C_CAM_CAL_U8(0x0f02, 0x00);
    Hi846_Otp_Write_I2C_CAM_CAL_U8(0x071a, 0x01);
    Hi846_Otp_Write_I2C_CAM_CAL_U8(0x071b, 0x09);
    Hi846_Otp_Write_I2C_CAM_CAL_U8(0x0d04, 0x00);
    Hi846_Otp_Write_I2C_CAM_CAL_U8(0x0d00, 0x07);
    Hi846_Otp_Write_I2C_CAM_CAL_U8(0x003e, 0x10);	// OTP R/W mode
    Hi846_Otp_Write_I2C_CAM_CAL_U8(0x0a00, 0x01);
	mdelay(1);
}
static u16 OTP_read_cmos_sensor(u16 addr)
{
    u16 data;
    Hi846_Otp_Write_I2C_CAM_CAL_U8(0x070a, (addr >> 8) & 0xff); //start address H
    Hi846_Otp_Write_I2C_CAM_CAL_U8(0x070b, addr& 0x00ff); //start address L
    Hi846_Otp_Write_I2C_CAM_CAL_U8(0x0702, 0x01); //single read
 // mDELAY(10);
    data = read_cmos_sensor(0x0708); //OTP data read
    return data;
}

int hi846_iReadData(unsigned int ui4_offset,
	unsigned int ui4_length, unsigned char *pinputdata)
{
	u32 u4CurrentOffset;
    int i, addr = 0;
	int checksum = 0;
	int module_info_checksum = 0 , wb_data_checksum = 0 , lsc_data_checksum = 0 , af_data_checksum = 0;
	u4CurrentOffset = ui4_offset;
    printk("HI846ST  ui4_offset = %d",u4CurrentOffset);
	Hi846_Otp_init();
	//1. read module info
	hi846_otp.Module_Info_Flag = OTP_read_cmos_sensor(0x0201);
	if (hi846_otp.Module_Info_Flag == 0x01)
	{
		 addr = 0x0202;
		 printk("HI846_Sensor:This is group1 otp sensor!!\n"); 
	}
	else if (hi846_otp.Module_Info_Flag == 0x13)
	{
		addr = 0x0211;
		printk("HI846_Sensor:This is group2 otp sensor!!\n"); 
	}
	else
		addr = 0;
	printk("HI846ST module info start addr = 0x%x \n", addr);

	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x070a, (addr >> 8) & 0xff);
	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x070b, addr & 0xff);
	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x0702, 0x01);

	for (i =0; i <14; i++)
	{
		pinputdata[i] = read_cmos_sensor(0x708);
		checksum +=pinputdata[i];
		printk("A0902_BACK_HI846ST[i] module_ifo_data[%d] = 0x%x",i,pinputdata[i]);
	}
	module_info_checksum = read_cmos_sensor(0x708);
	checksum = (checksum % 0xFF) + 1;
	printk("HI846ST module_info_checksum = %d, %d  ", module_info_checksum, checksum);
	if (checksum == module_info_checksum)
		{
		    printk("HI846_Sensor: Module information checksum PASS\n ");
		}
	else
		{
		    printk("HI846_Sensor: Module information checksum Fail\n ");
		}

	//2. read awb
	hi846_otp.WB_FLAG = OTP_read_cmos_sensor(0x0220);
	if (hi846_otp.WB_FLAG == 0x01)
		addr = 0x0221;
	else if (hi846_otp.WB_FLAG == 0x13)
		addr = 0x0230;
	else
		addr = 0;
	printk("HI846ST wb start addr = 0x%x \n", addr);

	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x070a, (addr >> 8) & 0xff);
	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x070b, addr & 0xff);
	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x0702, 0x01);

	checksum = 0;
	for (i =0; i <14; i++)
	{
		pinputdata[i+14] = read_cmos_sensor(0x708);
		checksum += pinputdata[i+14];
		printk("HI846ST[%d] awb_data[%d] = 0x%x  ", i+14,i,pinputdata[i+14]);
	}
	wb_data_checksum = read_cmos_sensor(0x708);
	checksum = (checksum % 0xFF) + 1;
	printk("A0902_BACK_HI846ST awb_data_checksum = %d, %d  ", wb_data_checksum, checksum);
	if (checksum == wb_data_checksum)
	{
	  printk("HI846_Sensor: awb data checksum PASS\n ");
	}
	else
	{
	  printk("HI846_Sensor: awb data checksum Fail\n ");
	}

	//3. read lsc
	hi846_otp.LSC_FLAG = OTP_read_cmos_sensor(0x023f);
	if (hi846_otp.LSC_FLAG == 0x01)
		addr = 0x0240;
	else if (hi846_otp.LSC_FLAG == 0x13)
		addr = 0x098d;
	else
		addr = 0;
	printk("HI846ST lsc start addr = 0x%x \n", addr);

	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x070a, (addr >> 8) & 0xff);
	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x070b, addr & 0xff);
	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x0702, 0x01);

	checksum = 0;
	for (i =0; i <1868; i++)
	{
		pinputdata[i+28] = read_cmos_sensor(0x708);
		checksum += pinputdata[i+28];
		pr_debug("HI846ST[%d] lsc_data[%d] = 0x%x", i+28, i,pinputdata[i+28]);
	}
	lsc_data_checksum = read_cmos_sensor(0x708);
	checksum = (checksum % 0xFF) + 1;
	printk("HI846ST lsc_data_checksum = %d, %d  ", lsc_data_checksum, checksum);
	if (checksum == lsc_data_checksum)
		{
		printk("HI846_Sensor: lsc data checksum PASS\n ");
		}
	else
		{
		printk("HI846_Sensor: lsc data checksum Fail\n ");
		}

	//4. read af
	hi846_otp.AF_FLAG = OTP_read_cmos_sensor(0x10da);
	if (hi846_otp.AF_FLAG == 0x01)
		addr = 0x10db;
	else if (hi846_otp.AF_FLAG == 0x13)
		addr = 0x10ea;
	else
		addr = 0;
	printk("HI846ST af start addr = 0x%x \n", addr);

	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x070a, (addr >> 8) & 0xff);
	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x070b, addr & 0xff);
	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x0702, 0x01);

	checksum = 0;
	for (i =0; i < 14; i++)
	{
		pinputdata[i+1896] = read_cmos_sensor(0x708);
		checksum += pinputdata[i+1896];
		printk("HI846ST[%d] af_data[%d] = 0x%x  ", i+1896,i,pinputdata[i+1896]);
	}
	af_data_checksum = read_cmos_sensor(0x708);
	checksum = (checksum % 0xFF) + 1;
	printk("HI846ST af_data_checksum = %d, %d  ", af_data_checksum, checksum);
	if (checksum == af_data_checksum)
		{
		printk("HI846_Sensor: af data checksum PASS\n ");
		}
	else
		{
		printk("HI846_Sensor: af data checksum Fail\n ");
		}

	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x0a00, 0x00); //complete
	mdelay(10);
	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x003e, 0x00); //complete
	Hi846_Otp_Write_I2C_CAM_CAL_U8(0x0a00, 0x01); //complete

	printk("%s Exit \n",__func__);
	return 0;
}

unsigned int a0902backhi846st_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
	if (hi846_iReadData(addr, size, data) == 0)
		return size;
	else
		return 0;
}