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
#include "eeprom_i2c_a0601ddfrontsc820cs_driver.h"

static struct i2c_client *g_pstI2CclientG;
#define EEPROM_I2C_MSG_SIZE_READ 2
#define EEPROM_I2C_WRITE_MSG_LENGTH_MAX 32
#define SC820CS_ST_RET_FAIL 1
#define SC820CS_ST_RET_SUCCESS 0
#define SC820CS_ST_PAGE2 2
#define SC820CS_ST_DEBUG_ON 0

#define module_flag						0x827A
#define awb_flag						0x8295
#define lsc_flag						0x82B6
#define module_group1_startadd			0x827B
#define module_group2_startadd			0x8288
#define module_length					13						//include checksum
#define awb_group1_startadd				0x8296
#define awb_group2_startadd				0x82A6
#define awb_length						16						//include checksum
#define lsc_group1_startadd				0x82B7
#define lsc_group2_startadd				0x8BEC
#define lsc_page2_group1_length			329
#define lsc_wholepage_length			390			
#define	lsc_page6_group1_length			369
#define	lsc_page6_group2_length			20
#define	lsc_page11_group2_length		288
#define	lsc_group1_checksum				0x8BEB
#define	lsc_group2_checksum				0x959A
static int otp_group = 0;
/* Tab A9 code for SR-AX6739A-01-762 by liluling at 20230707 start */
static int SC820CS_Otp_Read_I2C_CAM_CAL(u16 a_u2Addr)
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
		printk("SC820CS I2C read data failed!!\n");
		return -1;
	}
	data = a_puBuff[0];
	return data;
}

static int SC820CS_Otp_Write_I2C_CAM_CAL_U8(u16 a_u2Addr,u8 parameter)
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
		printk("SC820CS I2C write data failed!!\n");
		return -1;
	}
	mdelay(5);
	return 0;
}
static u16 read_cmos_sensor(u16 addr)
{
	u16 get_byte = 0xFF;
    get_byte = SC820CS_Otp_Read_I2C_CAM_CAL(addr);

	return get_byte;
}

static int sc820cs_set_threshold(u8 threshold)//set thereshold
{
	int threshold_reg1[3] ={ 0x48,0x48,0x48};
	int threshold_reg2[3] ={ 0x38,0x18,0x58};
	int threshold_reg3[3] ={ 0x41,0x41,0x41};
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x36b0,threshold_reg1[threshold]);
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x36b1,threshold_reg2[threshold]);
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x36b2,threshold_reg3[threshold]);
	pr_info("sc820cs_set_threshold %d\n", threshold);
	return SC820CS_ST_RET_SUCCESS;
}

static int sc820cs_set_page_and_load_data(int page)//set page
{
	uint64_t Startaddress = 0;
	uint64_t EndAddress = 0;
	int delay = 0;
	int pag = 0;
	Startaddress = page * 0x200 + 0x7E00;//set start address in page
	EndAddress = Startaddress + 0x1ff;//set end address in page
	pag = page*2 -1;//change page
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x4408,(Startaddress>>8)&0xff);  //set start address in high 8 bit
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x4409,Startaddress&0xff); //set start address in low 8 bit
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x440a,(EndAddress>>8)&0xff); //set end address in high 8 bit
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x440b,EndAddress&0xff); //set end address in low 8 bit


	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x4401,0x13); // address set finished
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x4412,pag&0xff); // set page
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x4407,0x00);// set page finished
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x4400,0x11);// manual load begin
	mdelay(10);
	while((read_cmos_sensor(0x4420)&0x01) == 0x01)// wait for manual load finished
	{
		delay++;
		pr_info("sc820cs_set_page waitting, OTP is still busy for loading %d times\n", delay);
		if(delay == 10)
		{
			pr_info("sc820cs_set_page fail, load timeout!!!\n");
			return SC820CS_ST_RET_FAIL;
		}
		mdelay(10);
	}
	pr_info("sc820cs_set_page success\n");
	return SC820CS_ST_RET_SUCCESS;
}

//read otp module_info && awb_info data && checksum
static int sc820cs_sensor_otp_read_data(unsigned int ui4_offset,unsigned int ui4_length, unsigned char *pinputdata)
{
	int i;
	unsigned int checksum_cal = 0;
	unsigned int checksum = 0;

	for(i=0; i<ui4_length; i++)
	{
		pinputdata[i] = read_cmos_sensor(ui4_offset + i);
		checksum_cal += pinputdata[i];
		#if SC820CS_ST_DEBUG_ON
		pr_info("sc820cs_sensor_otp_read_data addr=0x%x, data=0x%x\n", ui4_offset + i, pinputdata[i]);
		#endif
	}

	checksum = pinputdata[i-1];
	checksum_cal -= checksum;
	checksum_cal = checksum_cal%255+1;
	if(checksum_cal != checksum)
	{
		pr_info("sc820cs_sensor_otp_read_data checksum fail, checksum_cal=0x%x, checksum=0x%x\n", checksum_cal, checksum);
		return SC820CS_ST_RET_FAIL;
	}
	pr_info("sc820cs_sensor_otp_read_data success\n");
	return SC820CS_ST_RET_SUCCESS;
}

//read lsc otp data
static unsigned int sc820cs_sensor_otp_read_lsc_data(int page,unsigned int ui4_length,unsigned char *pinputdata)
{
	int i;
	unsigned int checksum_cal = 0;
	uint64_t Startaddress = 0;
	//uint64_t EndAddress = 0;
	unsigned int ui4_lsc_offset = 0;
	unsigned int ui4_lsc_length = 0;
	Startaddress = page * 0x200 + 0x7E00;//set start address in page
	//EndAddress = Startaddress + 0x1ff;//set end address in page
	ui4_lsc_offset = Startaddress+122;
	ui4_lsc_length = ui4_length;
	switch(page)
	{
		case 2:
		ui4_lsc_offset = lsc_group1_startadd;
		break;
		case 6:
		ui4_lsc_offset = otp_group==2 ? lsc_group2_startadd:ui4_lsc_offset;
		break;
		default:
		break;
	}
	for(i =0; i<ui4_lsc_length; i++)
	{
		pinputdata[i] = read_cmos_sensor(ui4_lsc_offset + i);
		checksum_cal += pinputdata[i];
		#if SC820CS_ST_DEBUG_ON
		pr_info("sc820cs_sensor_otp_read_lsc_data addr=0x%x, data=0x%x\n", ui4_lsc_offset + i, pinputdata[i]);
		#endif
	}
	checksum_cal = checksum_cal%255;
	pr_info("sc820cs_sensor read otp page:%d lsc_data success,checksum_cal:0x%x\n",page,checksum_cal);
	return checksum_cal;

}

//read otp module_info
static int sc820cs_sensor_otp_read_module_info(unsigned char *pinputdata)
{
	int ret = SC820CS_ST_RET_FAIL;

	pr_info("sc820cs_sensor_otp_read_module_info begin!\n");

	if(read_cmos_sensor(module_flag) == 0x01)
	{
		ret = sc820cs_sensor_otp_read_data(module_group1_startadd, module_length, pinputdata);
		otp_group = 1;
		if(ret == SC820CS_ST_RET_SUCCESS)
		{
		   pr_info("sc820cs_sensor_otp_read_module_info group1 checksum success!\n");
		}
		return ret;
	}
	else if(read_cmos_sensor(module_flag) == 0x13)
	{
		ret = sc820cs_sensor_otp_read_data(module_group2_startadd, module_length, pinputdata);
		otp_group = 2;
		if(ret == SC820CS_ST_RET_SUCCESS)
		{
		   pr_info("sc820cs_sensor_otp_read_module_info group2 checksum success!\n");
		}
		return ret;
	}
	else
	{
		pr_info("sc820cs_sensor_otp_read_module_info flag failed:%d\n",read_cmos_sensor(module_flag));
	}
	pr_info("sc820cs_sensor_otp_read_module_info end!\n");
	return ret;
}

//read otp awb_info
static int sc820cs_sensor_otp_read_awb_info_group1(unsigned char *pinputdata)
{

	int ret = SC820CS_ST_RET_FAIL;

	pr_info("sc820cs_sensor_otp_read_awb_info group1 begin!\n");

	if(read_cmos_sensor(awb_flag) == 0x01)
	{
		ret = sc820cs_sensor_otp_read_data(awb_group1_startadd, awb_length, pinputdata);
	}
	else
	{
		pr_info("sc820cs_sensor_otp_read_awb_info group1 flag failed!\n");
	}
    pr_info("sc820cs_sensor_otp_read_awb_info group1 end!\n");
	return ret;
}

static int sc820cs_sensor_otp_read_awb_info_group2(unsigned char *pinputdata)
{

	int ret = SC820CS_ST_RET_FAIL;

	pr_info("sc820cs_sensor_otp_read_awb_info group2 begin!\n");

	if(read_cmos_sensor(awb_flag) == 0x13)
	{
		ret = sc820cs_sensor_otp_read_data(awb_group2_startadd, awb_length, pinputdata);
	}
	else
	{
		pr_info("sc820cs_sensor_otp_read_awb_info group2 flag failed!\n");
	}
    pr_info("sc820cs_sensor_otp_read_awb_info group2 end!\n");
	return ret;
}

//read otp group1 lsc_info
static int sc820cs_sensor_otp_read_lsc_info_group1(unsigned char *pinputdata)
{
	int ret = SC820CS_ST_RET_FAIL;
	int page = SC820CS_ST_PAGE2;
	unsigned int checksum_cal = 0;
	unsigned int checksum = 0;
	pr_info("sc820cs_sensor_otp_read_lsc_info group1 begin!\n");
	if(read_cmos_sensor(lsc_flag) == 0x01)
	{
		checksum_cal += sc820cs_sensor_otp_read_lsc_data(page, lsc_page2_group1_length, pinputdata);
		page = page + 1;

        for(;page <= 5;page++)
		{
			sc820cs_set_page_and_load_data(page);//set page--3,4,5 && load data
			checksum_cal += sc820cs_sensor_otp_read_lsc_data(page,lsc_wholepage_length,pinputdata + lsc_page2_group1_length + (page-3)*lsc_wholepage_length);
		}

        sc820cs_set_page_and_load_data(page);//set page6
		checksum_cal += sc820cs_sensor_otp_read_lsc_data(page,lsc_page6_group1_length,pinputdata + lsc_page2_group1_length + (page-3)*lsc_wholepage_length);
		checksum_cal = checksum_cal%255+1;
		checksum  = read_cmos_sensor(lsc_group1_checksum);
		pinputdata[0x74C] = checksum;
		if(checksum_cal == checksum)
		{
			pr_info("sc820cs_sensor_otp_read_lsc_info group1 checksum pass!\n");
			ret = SC820CS_ST_RET_SUCCESS;
		}
		else
		{
			pr_info("sc820cs_sensor_otp_read_lsc_info group1 checksum fail,checksum_cal:0x%x,checksum:0x%x!\n",checksum_cal,checksum);
			
            ret = SC820CS_ST_RET_FAIL;
		}
	}
	else
	{
		pr_info("sc820cs_sensor_otp_read lsc_info group1 flag failed!\n");
		return ret;
	}
	pr_info("sc820cs_sensor_otp_read_lsc_info group1 end!\n");
    return ret;
}

//read otp group2 lsc_info
static int sc820cs_sensor_otp_read_lsc_info_group2(unsigned char *pinputdata)
{
	int ret = SC820CS_ST_RET_FAIL;
	int page = SC820CS_ST_PAGE2;
	unsigned int checksum_cal = 0;
	unsigned int checksum = 0;
	pr_info("sc820cs_sensor_otp_read_lsc_info group2 begin!\n");
	if(read_cmos_sensor(lsc_flag) == 0x13)
	{
		page = page + 4;
		sc820cs_set_page_and_load_data(page);//set page:6
		checksum_cal += sc820cs_sensor_otp_read_lsc_data(page, lsc_page6_group2_length, pinputdata);
		page++;
        for(;page <= 10;page++)
		{
			sc820cs_set_page_and_load_data(page);//set page:7,8,9,10 && load data
			checksum_cal += sc820cs_sensor_otp_read_lsc_data(page, lsc_wholepage_length, pinputdata + lsc_page6_group2_length + (page-7)*lsc_wholepage_length);
		}

        sc820cs_set_page_and_load_data(page);//set page:11
		checksum_cal += sc820cs_sensor_otp_read_lsc_data(page, lsc_page11_group2_length, pinputdata + lsc_page6_group2_length + (page-7)*lsc_wholepage_length);//read page:11 otp data
		checksum_cal = checksum_cal%255+1;
		checksum  = read_cmos_sensor(lsc_group2_checksum);
		pinputdata[0x74C] = checksum;
		if(checksum_cal == checksum)
		{
			pr_info("sc820cs_sensor_otp_read_lsc_info group2 checksum pass,checksum_cal:0x%x\n",checksum_cal);
			ret = SC820CS_ST_RET_SUCCESS;
		}
		else
		{
			pr_info("sc820cs_sensor_otp_read_lsc_info group2 checksum fail,checksum_cal:0x%x,checksum:0x%x!\n",checksum_cal,checksum);
			
            ret = SC820CS_ST_RET_FAIL;
		}
	}
	else
	{
		pr_info("sc820cs_sensor_otp_read lsc_info group2 flag failed!\n");
		return ret;
	}
	pr_info("sc820cs_sensor_otp_read_lsc_info group2 end!\n");
    return ret;
}

//read otp af data
/*static int sc820cs_sensor_otp_read_af_info(unsigned char *pinputdata)
{
	int ret = SC820CS_ST_RET_FAIL;
	sc820cs_set_page_and_load_data(11);
	pr_info("sc820cs_sensor_otp_read af_info begin!\n");
	if(read_cmos_sensor(0x959d) == 0x01)
	{
		ret = sc820cs_sensor_otp_read_data(0x959e, 0xF, pinputdata);
	}
	else if(read_cmos_sensor(0x959d) == 0x13)
	{
		ret = sc820cs_sensor_otp_read_data(0x95ad, 0xF, pinputdata);

	}
	else
	{
		pr_info("sc820cs_sensor_otp_read af_info flag failed!\n");
	}
	pr_info("sc820cs_sensor_otp_read af_info end!\n");
	return ret;
}*/

static int sc820cs_sensor_otp_read_group1(unsigned int ui4_offset,unsigned int ui4_length, unsigned char *pinputdata)
{
	int threshold = 0;
    int ret = SC820CS_ST_RET_FAIL;
	//read awb,lsc,Af
	for(threshold=0; threshold<3; threshold++)
	{
		sc820cs_set_threshold(threshold);
		sc820cs_set_page_and_load_data(SC820CS_ST_PAGE2);

		pr_info("sc820cs read awb info group1 in treshold R%d\n", threshold);
		ret = sc820cs_sensor_otp_read_awb_info_group1(pinputdata + module_length);
		if(ret == SC820CS_ST_RET_FAIL)
		{
			pr_info("sc820cs read awb info group1 in treshold R%d fail\n", threshold);
			continue;
		}

		else
		{
			pr_info("sc820cs read awb info group1 in treshold R%d checksum success\n", threshold);
			ret = sc820cs_sensor_otp_read_lsc_info_group1(pinputdata + module_length + awb_length);
			if(ret == SC820CS_ST_RET_FAIL)
			{
				pr_info("sc820cs read lsc info group1 in treshold R%d fail\n", threshold);
				continue;
			}
			else
			{
				pr_info("sc820cs read lsc info group1 in treshold R%d success\n", threshold);
				break;
				/*ret = sc820cs_sensor_otp_read_af_info(pinputdata+0x76B);
				if(ret == SC820CS_ST_RET_FAIL)
				{
					pr_info("sc820cs read af info group1 in treshold R%d fail\n", threshold);
					continue;
				}
				else
				{
					pr_info("sc820cs read af info group1 in treshold R%d success\n", threshold);
					break;
				}*/
			}
		}

	}
	if(ret == SC820CS_ST_RET_FAIL)
	{
		pr_info("sc820cs read otp data group1 in treshold R1 R2 R3 all failed!!!\n");
		return ret;
	}
	return ret;
}

static int sc820cs_sensor_otp_read_group2(unsigned int ui4_offset,unsigned int ui4_length, unsigned char *pinputdata)
{
	int threshold = 0;
	int ret = SC820CS_ST_RET_FAIL;
	//read awb,lsc,Af
	for(threshold=0; threshold<3; threshold++)
	{
		sc820cs_set_threshold(threshold);
		sc820cs_set_page_and_load_data(SC820CS_ST_PAGE2);

		pr_info("sc820cs read awb info group2 in treshold R%d\n", threshold);
		ret = sc820cs_sensor_otp_read_awb_info_group2(pinputdata + module_length);
		if(ret == SC820CS_ST_RET_FAIL)
		{
			pr_info("sc820cs read awb info group2 in treshold R%d fail\n", threshold);
			continue;
		}

		else
		{
			pr_info("sc820cs read awb info group2 in treshold R%d checksum success\n", threshold);
			ret = sc820cs_sensor_otp_read_lsc_info_group2(pinputdata + module_length + awb_length);
			if(ret == SC820CS_ST_RET_FAIL)
			{
				pr_info("sc820cs read lsc info group2 in treshold R%d fail\n", threshold);
				continue;
			}
			else
			{
				pr_info("sc820cs read lsc info group2 in treshold R%d checksum success\n", threshold);
				break;
				/*ret = sc820cs_sensor_otp_read_af_info(pinputdata+0x76B);
				if(ret == SC820CS_ST_RET_FAIL)
				{
					pr_info("sc820cs read af info group2 in treshold R%d fail\n", threshold);
					continue;
				}
				else
				{
					pr_info("sc820cs read af info group2 in treshold R%d success\n", threshold);
					break;
				}*/
			}
		}
		
	}
	if(ret == SC820CS_ST_RET_FAIL)
	{
		pr_info("sc820cs read otp data group2 in treshold R1 R2 R3 all failed!!!\n");
		return ret;
	}
	return ret;
}

static int sc820cs_sensor_otp_read_all_data(unsigned int ui4_offset,unsigned int ui4_length, unsigned char *pinputdata)
{
	int ret = SC820CS_ST_RET_FAIL;
	int threshold = 0;
    int delay = 0;

    //read module info to judge the otp data which group
	for(threshold=0; threshold<3; threshold++)
	{
		sc820cs_set_threshold(threshold);
		sc820cs_set_page_and_load_data(SC820CS_ST_PAGE2);

		pr_info("sc820cs read module info in treshold R%d\n", threshold);
		ret = sc820cs_sensor_otp_read_module_info(pinputdata);
		if(ret == SC820CS_ST_RET_FAIL)
		{
			pr_info("sc820cs read module info in treshold R%d fail\n", threshold);
			continue;
		}
		else
		{
			pr_info("sc820cs read module info in treshold R%d success\n", threshold);
			break;
		}
	}
    if(ret == SC820CS_ST_RET_FAIL)
	{
		pr_info("sc820cs read module info in treshold R1 R2 R3 all failed!!!\n");
		return SC820CS_ST_RET_FAIL;
	}
	if(otp_group == 1) // group1
	{
        ret = sc820cs_sensor_otp_read_group1(ui4_offset,ui4_length,pinputdata);
	}
	else if(otp_group == 2) // group2
	{
        ret = sc820cs_sensor_otp_read_group2(ui4_offset,ui4_length,pinputdata);
	}
	else
	{
		pr_info("This otp belong to group NULL\n");
		return SC820CS_ST_RET_FAIL;
	}
	if(ret == SC820CS_ST_RET_FAIL)
	{
		pr_info("sc820cs read lsc info in treshold R1 R2 R3 all failed!!!\n");
		return ret;
	}
	pr_info("read sc820cs otp data success\n");

	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x4408,0x00);
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x4409,0x00);
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x440a,0x01);
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x440b,0xff);

    SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x4401,0x13);
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x4407,0x0e);
	SC820CS_Otp_Write_I2C_CAM_CAL_U8(0x4400, 0x11);
	mdelay(10);
	while((read_cmos_sensor(0x4420)&0x01) == 0x01)// wait for manual load finished
	{
		delay++;
		pr_info("sc820cs_set_page waitting, OTP is still busy for loading %d times\n", delay);
		if(delay == 10)
		{
			pr_info("sc820cs_set_page fail, load timeout!!!\n");
			return SC820CS_ST_RET_FAIL;
		}
		mdelay(10);
	}

	return ret;
}

static int sc820csst_iReadData(unsigned int ui4_offset,unsigned int ui4_length, unsigned char *pinputdata)
{
	int ret = SC820CS_ST_RET_FAIL;
	ret= sc820cs_sensor_otp_read_all_data(ui4_offset, ui4_length, pinputdata);
	if(ret == SC820CS_ST_RET_FAIL)
	{
		pr_info("sc820cs iReadData failed!!!\n");
		pr_info("sc820cs read otp data info in treshold R1 R2 R3 all failed already!!!\n");
		return SC820CS_ST_RET_SUCCESS;
	}
	pr_info("sc820cs iReadData success!!!\n");
	return SC820CS_ST_RET_SUCCESS;
}

unsigned int a0601ddfrontsc820cs_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
	if (sc820csst_iReadData(addr, size, data) == 0)
	{
		return size;
	}
	else
	{
		return SC820CS_ST_RET_SUCCESS;
	}
}
/* Tab A9 code for SR-AX6739A-01-762 by liluling at 20230707 end */