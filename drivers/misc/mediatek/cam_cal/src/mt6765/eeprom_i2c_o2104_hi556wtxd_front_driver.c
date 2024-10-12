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

#include "eeprom_i2c_o2104_hi556wtxd_front_driver.h"

static struct i2c_client *g_pstI2CclientG;

#define o2104_hi556wtxd_front_OTP_DEBUG  1
#define o2104_hi556wtxd_front_I2C_ID     0x50 /*0x20*/
struct o2104_hi556wtxd_front_otp_struct
{
	int Module_Info_Flag;
	int module_ifo_data[15];
	int WB_FLAG;
	int wb_data[17];
	int LSC_FLAG;
	int lsc_data[1869];
	int AF_FLAG;
	int af_data[5];
}o2104_hi556wtxd_front_otp;
static u16 read_cmos_sensor(u16 addr)
{
	u16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, o2104_hi556wtxd_front_I2C_ID);

	return get_byte;
}

static void write_cmos_sensor(u16 addr, u16 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para >> 8), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 4, o2104_hi556wtxd_front_I2C_ID);
}

static void write_cmos_sensor_8(u16 addr, u16 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8),
		(char)(addr & 0xFF), (char)(para & 0xFF)};

	iWriteRegI2C(pu_send_cmd, 3, o2104_hi556wtxd_front_I2C_ID);
}



static void o2104_hi556wtxd_front_otp_init(void)
{
	printk("o2104_hi556wtxd_front otp_init_setting\n");
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

/*hs04 code for DEAL6398A-565 by chenjun at 2022/08/09 start */
int o2104_hi556wtxd_front_iReadData(unsigned int ui4_offset,
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
    pr_info("o2104_hi556wtxd_front  ui4_offset = %d",u4CurrentOffset);
	o2104_hi556wtxd_front_otp_init();

	//1. read module info
	if (OTP_read_cmos_sensor(0x401) == 0x01) {
		o2104_hi556wtxd_front_otp.Module_Info_Flag = OTP_read_cmos_sensor(0x401);
		addr = 0x402;
	}
	else if (OTP_read_cmos_sensor(0x40C) == 0x01) {
		o2104_hi556wtxd_front_otp.Module_Info_Flag = OTP_read_cmos_sensor(0x40C);
		addr = 0x40D;
	}
	else if (OTP_read_cmos_sensor(0x417) == 0x01) {
		o2104_hi556wtxd_front_otp.Module_Info_Flag = OTP_read_cmos_sensor(0x417);
		addr = 0x418;
	}
	else {
		addr = 0;
	}
	pr_info("o2104_hi556wtxd_front module info start addr = 0x%x \n", addr);

	write_cmos_sensor_8 (0x10a, ((addr )>>8)&0xff); // start address H
	write_cmos_sensor_8 (0x10b, addr&0xff); // start address L
	write_cmos_sensor_8 (0x102, 0x01); // single read

	for (i =0; i <9; i++)
	{
		pinputdata[i] = read_cmos_sensor(0x108);
		checksum +=pinputdata[i];
		pr_info("o2104_hi556wtxd_front[%d]  module_ifo_data[%d] = 0x%x", i,i,pinputdata[i]);
	}
	module_info_checksum = read_cmos_sensor(0x108);
	checksum = (checksum % 0xFF) + 1;
	pr_info("o2104_hi556wtxd_front module_info_checksum = %d, %d  ", module_info_checksum, checksum);
	if (checksum == module_info_checksum)
	{
		pr_info("o2104HI556TXD_FRONT_Sensor: Module information checksum PASS\n ");
	}
	else
	{
		pr_info("o2104HI556TXD_FRONT_Sensor: Module information checksum Fail\n ");
	}

	//2. read wb
	if (OTP_read_cmos_sensor(0x422) == 0x01) {
		o2104_hi556wtxd_front_otp.WB_FLAG = OTP_read_cmos_sensor(0x422);
		addr = 0x423;
	}
	else if (OTP_read_cmos_sensor(0x42C) == 0x01) {
		o2104_hi556wtxd_front_otp.WB_FLAG = OTP_read_cmos_sensor(0x42C);
		addr = 0x42D;
	}
	else if (OTP_read_cmos_sensor(0x436) == 0x01) {
		o2104_hi556wtxd_front_otp.WB_FLAG = OTP_read_cmos_sensor(0x436);
		addr = 0x437;
	}
	else {
		addr = 0;
	}
	pr_info("o2104_hi556wtxd_front wb start addr = 0x%x \n", addr);

	write_cmos_sensor_8 (0x10a, ((addr )>>8)&0xff); // start address H
	write_cmos_sensor_8 (0x10b, addr&0xff); // start address L
	write_cmos_sensor_8 (0x102, 0x01); // single read

	checksum = 0;
	for (i =0; i <8; i++)
	{
		pinputdata[i+9] = read_cmos_sensor(0x108);
		checksum += pinputdata[i+9];
		pr_info("o2104_hi556wtxd_front[%d] wb_data[%d] = 0x%x  ", i+9,i+9,pinputdata[i+9]);
	}
	wb_data_checksum = read_cmos_sensor(0x108);
	checksum = (checksum % 0xFF) + 1;
	pr_info("o2104_hi556wtxd_front wb_data_checksum = %d, %d  ", wb_data_checksum, checksum);
	if (checksum == wb_data_checksum)
	{
		pr_info("o2104HI556TXD_FRONT_Sensor: wb data checksum PASS\n ");
	}
	else
	{
		pr_info("o2104HI556TXD_FRONT_Sensor: wb data checksum Fail\n ");
	}

	//3. read lsc
	if (OTP_read_cmos_sensor(0x440) == 0x01) {
		o2104_hi556wtxd_front_otp.LSC_FLAG = OTP_read_cmos_sensor(0x440);
		addr = 0x441;
	}
	else if (OTP_read_cmos_sensor(0xB8E) == 0x01) {
		o2104_hi556wtxd_front_otp.LSC_FLAG = OTP_read_cmos_sensor(0xB8E);
		addr = 0xB8F;
	}
	else if (OTP_read_cmos_sensor(0x12DC) == 0x01) {
		o2104_hi556wtxd_front_otp.LSC_FLAG = OTP_read_cmos_sensor(0x12DC);
		addr = 0x12DD;
	}
	else {
		addr = 0;
	}
	pr_info("o2104_hi556wtxd_front lsc start addr = 0x%x \n", addr);

	write_cmos_sensor_8 (0x10a, ((addr )>>8)&0xff); // start address H
	write_cmos_sensor_8 (0x10b, addr&0xff); // start address L
	write_cmos_sensor_8 (0x102, 0x01); // single read

	checksum = 0;
	for (i =0; i <1868; i++)
	{
		pinputdata[i+17] = read_cmos_sensor(0x108);
		checksum += pinputdata[i+17];
		pr_debug("O2104_hi556wtxd_front[%d] lsc_data[%d] = 0x%x", i+17, i+17,pinputdata[i+17]);
	}
	lsc_data_checksum = read_cmos_sensor(0x108);
	checksum = (checksum % 0xFF) + 1;
	pr_info("o2104_hi556wtxd_front lsc_data_checksum = %d, %d  ", lsc_data_checksum, checksum);
	if (checksum == lsc_data_checksum)
	{
		pr_info("O2104HI556TXD_FRONT_Sensor: lsc data checksum PASS\n ");
	}
	else
	{
		pr_info("O2104HI556TXD_FRONT_Sensor: lsc data checksum Fail\n ");
	}

	write_cmos_sensor_8(0x0a00, 0x00); //complete
	mdelay(10);
	write_cmos_sensor_8(0x003e, 0x00); //complete
	write_cmos_sensor_8(0x0a00, 0x01); //complete

	pr_info("%s Exit \n",__func__);
	return 0;
}
/*hs04 code for DEAL6398A-565 by chenjun at 2022/08/09 end */

unsigned int o2104_hi556wtxd_front_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
	if (o2104_hi556wtxd_front_iReadData(addr, size, data) == 0)
		return size;
	else
		return 0;
}

