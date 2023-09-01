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

#include "eeprom_i2c_s5k4h7_hlt_driver.h"

static struct i2c_client *g_pstI2CclientG;

#define S5K4H7_HLT_OTP_DEBUG  0
#define S5K4H7_HLT_I2C_ID     0x20

#define S5K4H7_HLT_GROUP_NUM 3
#define S5K4H7_HLT_PAGE_FINAL_ADDR 0x0A43
#define S5K4H7_HLT_PAGE_START_OFFSET 0X0A04
#define S5K4H7_HLT_GROUP_ID_ADDR 0x0A3D
static int s5k4h7_hlt_group_valid_flag[S5K4H7_HLT_GROUP_NUM] = {0x01, 0x13, 0x37};

static u16 read_cmos_sensor(u16 addr)
{
	u16 get_byte=0;
	char pu_send_cmd[2] = { (char)((addr >> 8) & 0xff), (char)(addr & 0xff) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, S5K4H7_HLT_I2C_ID);

	return get_byte;
}

static void write_cmos_sensor(u16 addr, u8 para)
{
	char pu_send_cmd[3] = { (char)((addr >> 8) & 0xff), (char)(addr & 0xff), (char)(para & 0xff) };

	iWriteRegI2C(pu_send_cmd, 3, S5K4H7_HLT_I2C_ID);
}

static void s5k4h7_hlt_otp_init(void)
{
	/*TabA7 Lite code for SR-AL5628-01-55 by zhongquan at 2022/11/26 start*/
	write_cmos_sensor(0x0100, 0x01);
	mdelay(2);
	write_cmos_sensor(0x0A02, 0x00);
	write_cmos_sensor(0x0A00, 0x01);
	mdelay(2);
	/*TabA7 Lite code for SR-AL5628-01-55 by zhongquan at 2022/11/26 end*/
}



static u16 s5k4h7_hlt_otp_read_group(u32 addr, u8 *data, int length)
{
	u16 i = 0;
	u16 groupid = 0;
	u8 group=0;
	u16 curr_addr;
	u16 curr_page;
	u32 calc_checksum=0;
	u32 read_checksum=0;

	curr_page=addr>>16;
	curr_addr=addr&0x0000ffff;

	groupid = read_cmos_sensor(0x0A3D);
	if(groupid==s5k4h7_hlt_group_valid_flag[0]){
		group=0;
	}else if(groupid==s5k4h7_hlt_group_valid_flag[1]){
		group=1;
	}else if(groupid==s5k4h7_hlt_group_valid_flag[2]){
		group=2;
	}else{
		pr_debug("s5k4h7_hlt group id invalid 0x%x", groupid);
		return -1;
	}
	curr_addr+=group;
	curr_page+=group*30;

	while (i<length) {
		/*page set*/
		write_cmos_sensor(0x0A02, curr_page);
		/*otp enable read*/
		write_cmos_sensor(0x0A00, 0x01);
		/*TabA7 Lite code for SR-AL5628-01-55 by zhongquan at 2022/11/26 start*/
		mdelay(1);
		/*TabA7 Lite code for SR-AL5628-01-55 by zhongquan at 2022/11/26 end*/
		while(curr_addr<=S5K4H7_HLT_PAGE_FINAL_ADDR){
			data[i] = read_cmos_sensor(curr_addr);
			calc_checksum += data[i];
			#if S5K4H7_HLT_OTP_DEBUG
				pr_debug("s5k4h7_hlt addr = 0x%x, data = 0x%x\n", curr_addr, data[i]);
			#endif
			curr_addr++;
			i++;
			if(i>=length){
				read_checksum=read_cmos_sensor(curr_addr);
				break;
			}
		}
		curr_addr=S5K4H7_HLT_PAGE_START_OFFSET;
		curr_page++;
	}
	calc_checksum=calc_checksum%255+1;
	if(calc_checksum!=read_checksum){
		pr_debug("s5k4h7_hlt otp checksum fail, calc = 0x%x, read = 0x%x", calc_checksum, read_checksum);
		return -1;
	}

	pr_debug("s5k4h7_hlt otp read success, size = %d", i);
	return 0;
}


int s5k4h7_hlt_iReadData(unsigned int ui4_offset,
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

	s5k4h7_hlt_otp_init();

	i4RetValue = s5k4h7_hlt_otp_read_group(u4CurrentOffset, pBuff, i4ResidueDataLength);
	write_cmos_sensor(0x0A00, 0x00);
	/*TabA7 Lite code for SR-AL5628-01-55 by zhongquan at 2022/11/26 start*/
	mdelay(2);
	/*TabA7 Lite code for SR-AL5628-01-55 by zhongquan at 2022/11/26 end*/
	if (i4RetValue != 0) {
		pr_debug("I2C iReadData failed!!\n");
		return -1;
	}

	return 0;
}

unsigned int s5k4h7_hlt_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
	if (s5k4h7_hlt_iReadData(addr, size, data) == 0)
		return size;
	else
		return 0;
}
