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

#include "eeprom_i2c_o2103_sc520syx_front_driver.h"

static struct i2c_client *g_pstI2CclientG;

/*  hs04 code for DEAL6398A-1070 by chenjun at 2022/08/24 start */
/*  hs04 code for DEAL6398A-1070 by chenjun at 2022/08/21 start */
#define SC520CS_SYX_I2C_ADDR		0x6C
#define SC520CS_SYX_OTP_BASE_ADDR	0x80E6
#define SC520CS_SYX_RET_SUCCESS		0
#define SC520CS_SYX_RET_FAIL		1
#define SC520CS_SYX_DEBUG_ON		0

static void write_cmos_sensor8(u16 addr, u16 para)
{
    char pu_send_cmd[3] = {(char)(addr >> 8),
        (char)(addr & 0xFF), (char)(para & 0xFF)};

    iWriteRegI2C(pu_send_cmd, 3, SC520CS_SYX_I2C_ADDR);
}

static u16 read_cmos_sensor(u16 addr)
{
	u16 get_byte = 0xFF;
	int ret = -1;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };

	ret = iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, SC520CS_SYX_I2C_ADDR);
	if(ret == -1) {
		pr_info("sc520cs read_cmos_sensor fail addr=0x%x, data=0x%x\n", addr, get_byte);
		get_byte = 0xFF;
	}

	return get_byte;
}
/*  hs04 code for DEVAL6398A-462 by liluling at 2022/09/14 start */
static int sc520cs_set_threshold(u8 threshold)
{
	int threshold_reg1[3] ={ 0x4c,0x4c,0x4c};
	int threshold_reg2[3] ={ 0x38,0x28,0x48};
	int threshold_reg3[3] ={ 0xc1,0xc1,0xc1};
	write_cmos_sensor8(0x36b0,threshold_reg1[threshold]);
	write_cmos_sensor8(0x36b1,threshold_reg2[threshold]);
	write_cmos_sensor8(0x36b2,threshold_reg3[threshold]);
	pr_info("sc520cs_set_threshold %d\n", threshold);
	return SC520CS_SYX_RET_SUCCESS;
}
/*  hs04 code for DEVAL6398A-462 by liluling at 2022/09/14 end */
static int sc520cs_load_data(unsigned int start_addr,unsigned int end_addr)
{
	int delay = 0;

	pr_info("sc520cs_load_data start_addr=0x%x, end_addr=0x%x\n", start_addr, end_addr);

	write_cmos_sensor8(0x4408,(start_addr>>8)&0x0f);
	write_cmos_sensor8(0x4409,start_addr&0xff);
	write_cmos_sensor8(0x440a,(end_addr>>8)&0x0f);
	write_cmos_sensor8(0x440b,end_addr&0xff);

	write_cmos_sensor8(0x4401,0x13);

	if(start_addr < 0x8800) {
		write_cmos_sensor8(0x4412,0x01);
		write_cmos_sensor8(0x4407,0x0c);
	} else {
		write_cmos_sensor8(0x4412,0x03);
		write_cmos_sensor8(0x4407,0x00);
	}

	write_cmos_sensor8(0x4400,0x11);
	mdelay(50);

	while((read_cmos_sensor(0x4420)&0x01) == 0x01){
		delay++;
		pr_info("sc520cs_load_data waitting, OTP is still busy for loading %d times\n", delay);
		if(delay == 10) {
			pr_info("sc520cs_load_data fail, load timeout!!!\n");
			return SC520CS_SYX_RET_FAIL;
		}
		mdelay(10);
	}
	pr_info("sc520cs_load_data success\n");
	return SC520CS_SYX_RET_SUCCESS;
}
/*  hs04 code for DEAL6398A-1070 by chenjun at 2022/08/24 end */

static int sc520cs_sensor_otp_read_data(unsigned int ui4_offset,unsigned int ui4_length, unsigned char *pinputdata)
{
	int i;
	unsigned int checksum_cal = 0;
	unsigned int checksum = 0;

	for(i=0; i<ui4_length; i++) {
		pinputdata[i] = read_cmos_sensor(ui4_offset + i);
		checksum_cal += pinputdata[i];
		#if SC520CS_SYX_DEBUG_ON
		pr_info("sc520cs_sensor_otp_read_data addr=0x%x, data=0x%x\n", ui4_offset + i, pinputdata[i]);
		#endif
	}

	checksum = pinputdata[i-1];
	checksum_cal -= checksum;
	checksum_cal = checksum_cal%255+1;

	if(checksum_cal != checksum){
		pr_info("sc520cs_sensor_otp_read_data checksum fail, checksum_cal=0x%x, checksum=0x%x\n", checksum_cal, checksum);
		return SC520CS_SYX_RET_FAIL;
	}

	pr_info("sc520cs_sensor_otp_read_data success\n");
	return SC520CS_SYX_RET_SUCCESS;
}

static int sc520cs_sensor_otp_read_module_info(unsigned char *pinputdata)
{
	int flag = 0;
	int ret = SC520CS_SYX_RET_FAIL;

	flag = read_cmos_sensor(0x80E6);
	pr_info("sc520cs_sensor_otp_read_module_info flag = 0x%x\n", flag);

	switch(flag) {
		case 0x01:
			ret = sc520cs_sensor_otp_read_data(0x80E7, 10, pinputdata);
			break;
		case 0x13:
			ret = sc520cs_sensor_otp_read_data(0x80F1, 10, pinputdata);
			break;
		default:
			break;
	}

	return ret;
}

static int sc520cs_sensor_otp_read_awb_info(unsigned char *pinputdata)
{
	int flag = 0;
	int ret = SC520CS_SYX_RET_FAIL;

	flag = read_cmos_sensor(0x80FB);
	pr_info("sc520cs_sensor_otp_read_awb_info flag = 0x%x\n", flag);

	switch(flag) {
		case 0x01:
			ret = sc520cs_sensor_otp_read_data(0x80FC, 9, pinputdata);
			break;
		case 0x13:
			ret = sc520cs_sensor_otp_read_data(0x8105, 9, pinputdata);
			break;
		default:
			break;
	}

	return ret;
}


static int sc520cs_sensor_otp_read_lsc_info(unsigned char *pinputdata)
{
	int flag = 0;
	unsigned int read_addr = 0x810F;
	unsigned int checksum_cal = 0;
	unsigned int checksum = 0;
	int ret = SC520CS_SYX_RET_FAIL;
	int i,j;

	flag = read_cmos_sensor(0x810E);
	pr_info("sc520cs_sensor_otp_read_lsc_info flag = 0x%x\n", flag);

	switch(flag) {
		case 0x01:
			for(i=0; read_addr<=0x87FF; i++) {
				pinputdata[i] = read_cmos_sensor(read_addr);
				checksum_cal += pinputdata[i];
				read_addr++;
				#if SC520CS_SYX_DEBUG_ON
				pr_info("sc520cs_sensor_otp_read_data addr=0x%x, data=0x%x\n", read_addr, pinputdata[i]);
				#endif
			}
			read_addr = 0x8826;
			sc520cs_load_data(0x8800, 0x8FFF);
			for(j=i; read_addr<=0x8881; j++) {
				pinputdata[j] = read_cmos_sensor(read_addr);
				checksum_cal += pinputdata[j];
				read_addr++;
				#if SC520CS_SYX_DEBUG_ON
				pr_info("sc520cs_sensor_otp_read_data addr=0x%x, data=0x%x\n", read_addr, pinputdata[j]);
				#endif
			}

			checksum = pinputdata[j-1];
			checksum_cal -= checksum;
			checksum_cal = checksum_cal%255+1;

			if(checksum_cal != checksum){
				pr_info("sc520cs_sensor_otp_read_data checksum fail, checksum_cal=0x%x, checksum=0x%x\n", checksum_cal, checksum);
				ret = SC520CS_SYX_RET_FAIL;
			} else {
				pr_info("sc520cs_sensor_otp_read_data success\n");
				ret = SC520CS_SYX_RET_SUCCESS;
			}
			break;
		case 0x13:
			sc520cs_load_data(0x8800, 0x8FFF);
			ret = sc520cs_sensor_otp_read_data(0x8882, 1869, pinputdata);
			break;
		default:
			break;
	}

	return ret;
}

static int sc520cs_sensor_otp_read_all_data(unsigned int ui4_offset,unsigned int ui4_length, unsigned char *pinputdata)
{
	int ret = SC520CS_SYX_RET_FAIL;
	int threshold = 0;

	write_cmos_sensor8(0x3106, 0x05);

	for(threshold=0; threshold<3; threshold++)
	{
		sc520cs_set_threshold(threshold);
		sc520cs_load_data(0x8000, 0x87FF);

		pr_info("read module info in treshold R%d\n", threshold);
		ret = sc520cs_sensor_otp_read_module_info(pinputdata);
		if(ret == SC520CS_SYX_RET_FAIL) {
			pr_info("read module info in treshold R%d fail\n", threshold);
			continue;
		} else {
			pr_info("read module info in treshold R%d success\n", threshold);
			break;
		}
	}
	if(ret == SC520CS_SYX_RET_FAIL) {
		pr_info("read module info in treshold R1 R2 R3 all failed!!!\n");
		return ret;
	}

	pr_info("read awb info in treshold R%d\n", threshold);
	ret = sc520cs_sensor_otp_read_awb_info(pinputdata+10);
	if(ret == SC520CS_SYX_RET_FAIL) {
		pr_info("read awb info in treshold R%d fail\n", threshold);
		for(threshold=0; threshold<3; threshold++)
		{
			sc520cs_set_threshold(threshold);
			sc520cs_load_data(0x8000, 0x87FF);

			pr_info("read awb info in treshold R%d\n", threshold);
			ret = sc520cs_sensor_otp_read_awb_info(pinputdata+10);
			if(ret == SC520CS_SYX_RET_FAIL) {
				pr_info("read awb info in treshold R%d fail\n", threshold);
				continue;
			} else {
				pr_info("read awb info in treshold R%d success\n", threshold);
				break;
			}
		}
	}
	if(ret == SC520CS_SYX_RET_FAIL) {
		pr_info("read awb info in treshold R1 R2 R3 all failed!!!\n");
		return ret;
	}
	pr_info("read awb info in treshold R%d success\n", threshold);

	pr_info("read lsc info in treshold R%d\n", threshold);
	ret = sc520cs_sensor_otp_read_lsc_info(pinputdata+19);
	if(ret == SC520CS_SYX_RET_FAIL) {
		pr_info("read lsc info in treshold R%d fail\n", threshold);
		for(threshold=0; threshold<3; threshold++)
		{
			sc520cs_set_threshold(threshold);
			sc520cs_load_data(0x8000, 0x87FF);

			pr_info("read lsc info in treshold R%d\n", threshold);
			ret = sc520cs_sensor_otp_read_lsc_info(pinputdata+19);
			if(ret == SC520CS_SYX_RET_FAIL) {
				pr_info("read lsc info in treshold R%d fail\n", threshold);
				continue;
			} else {
				pr_info("read lsc info in treshold R%d success\n", threshold);
				break;
			}
		}
	}
	if(ret == SC520CS_SYX_RET_FAIL) {
		pr_info("read lsc info in treshold R1 R2 R3 all failed!!!\n");
		return ret;
	}
	pr_info("read lsc info in treshold R%d success\n", threshold);

	write_cmos_sensor8(0x4408,0x00);
	write_cmos_sensor8(0x4409,0x00);
	write_cmos_sensor8(0x440a,0x07);
	write_cmos_sensor8(0x440b,0xff);

	write_cmos_sensor8(0x3106, 0x01);

	return ret;
}

static int sc520syx_iReadData(unsigned int ui4_offset,unsigned int ui4_length, unsigned char *pinputdata)
{
	int ret = SC520CS_SYX_RET_FAIL;
	ret= sc520cs_sensor_otp_read_all_data(ui4_offset, ui4_length, pinputdata);
	if(ret == SC520CS_SYX_RET_FAIL) {
		pr_info("sc520syx_iReadData failed!!!\n");
		return 1;
	}
	pr_info("sc520syx_iReadData success!!!\n");
	return 0;
}

unsigned int o2103_sc520syx_front_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
	addr += SC520CS_SYX_OTP_BASE_ADDR;
	if (sc520syx_iReadData(addr, size, data) == 0)
		return size;
	else
		return 0;
}
/*  hs04 code for DEAL6398A-1070 by chenjun at 2022/08/21 end */
