// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

/* Include platform define if necessary */
#ifdef EEPROM_PLATFORM_DEFINE
#include "eeprom_platform_def.h"
#endif

static DEFINE_SPINLOCK(g_spinLock);


/************************************************************
 * I2C read function (Common)
 ************************************************************/
static struct i2c_client *g_pstI2CclientG;

/* add for linux-4.4 */
#ifndef I2C_WR_FLAG
#define I2C_WR_FLAG		(0x1000)
#define I2C_MASK_FLAG	(0x00ff)
#endif

#define I2C_WRITE_ID 0x6E
#define dump_en

#define EEPROM_I2C_MSG_SIZE_READ 2
#ifndef EEPROM_I2C_READ_MSG_LENGTH_MAX
#define EEPROM_I2C_READ_MSG_LENGTH_MAX 32
#endif
#ifndef EEPROM_I2C_WRITE_MSG_LENGTH_MAX
#define EEPROM_I2C_WRITE_MSG_LENGTH_MAX 32
#endif
#ifndef EEPROM_WRITE_EN
#define EEPROM_WRITE_EN 1
#endif

static int Read_I2C_CAM_CAL(u16 a_u2Addr, u32 ui4_length, u8 *a_puBuff)
{
	int i4RetValue = 0;
	char puReadCmd[2] = { (char)(a_u2Addr >> 8), (char)(a_u2Addr & 0xFF) };
	struct i2c_msg msg[EEPROM_I2C_MSG_SIZE_READ];


	if (ui4_length > EEPROM_I2C_READ_MSG_LENGTH_MAX) {
		pr_debug("exceed one transition %d bytes limitation\n",
			 EEPROM_I2C_READ_MSG_LENGTH_MAX);
		return -1;
	}
	spin_lock(&g_spinLock);
	g_pstI2CclientG->addr =
		g_pstI2CclientG->addr & (I2C_MASK_FLAG | I2C_WR_FLAG);
	spin_unlock(&g_spinLock);

	msg[0].addr = g_pstI2CclientG->addr;
	msg[0].flags = g_pstI2CclientG->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = puReadCmd;

	msg[1].addr = g_pstI2CclientG->addr;
	msg[1].flags = g_pstI2CclientG->flags & I2C_M_TEN;
	msg[1].flags |= I2C_M_RD;
	msg[1].len = ui4_length;
	msg[1].buf = a_puBuff;

	i4RetValue = i2c_transfer(g_pstI2CclientG->adapter,
				msg,
				EEPROM_I2C_MSG_SIZE_READ);

	spin_lock(&g_spinLock);
	g_pstI2CclientG->addr = g_pstI2CclientG->addr & I2C_MASK_FLAG;
	spin_unlock(&g_spinLock);

	if (i4RetValue != EEPROM_I2C_MSG_SIZE_READ) {
		pr_debug("I2C read data failed!!\n");
		return -1;
	}

	return 0;
}

int iReadData_CAM_CAL(unsigned int ui4_offset,
	unsigned int ui4_length, unsigned char *pinputdata)
{
	int i4RetValue = 0;
	int i4ResidueDataLength;
	u32 u4IncOffset = 0;
	u32 u4CurrentOffset;
	u8 *pBuff;

	i4ResidueDataLength = (int)ui4_length;
	u4CurrentOffset = ui4_offset;
	pBuff = pinputdata;
	do {
		if (i4ResidueDataLength >= EEPROM_I2C_READ_MSG_LENGTH_MAX) {
			i4RetValue = Read_I2C_CAM_CAL(
				(u16) u4CurrentOffset,
				EEPROM_I2C_READ_MSG_LENGTH_MAX, pBuff);
			if (i4RetValue != 0) {
				pr_debug("I2C iReadData failed!!\n");
				return -1;
			}
			u4IncOffset += EEPROM_I2C_READ_MSG_LENGTH_MAX;
			i4ResidueDataLength -= EEPROM_I2C_READ_MSG_LENGTH_MAX;
			u4CurrentOffset = ui4_offset + u4IncOffset;
			pBuff = pinputdata + u4IncOffset;
		} else {
			i4RetValue =
			    Read_I2C_CAM_CAL(
			    (u16) u4CurrentOffset, i4ResidueDataLength, pBuff);
			if (i4RetValue != 0) {
				pr_debug("I2C iReadData failed!!\n");
				return -1;
			}
			u4IncOffset += i4ResidueDataLength;
			i4ResidueDataLength = 0;
			u4CurrentOffset = ui4_offset + u4IncOffset;
			pBuff = pinputdata + u4IncOffset;
			/* break; */
		}
	} while (i4ResidueDataLength > 0);



	return 0;
}

static int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
		u8 *a_pRecvData, u16 a_sizeRecvData)
{
	int  i4RetValue = 0;
	struct i2c_msg msg[EEPROM_I2C_MSG_SIZE_READ];

	msg[0].addr = g_pstI2CclientG->addr;
	msg[0].flags = g_pstI2CclientG->flags & I2C_M_TEN;
	msg[0].len = a_sizeSendData;
	msg[0].buf = a_pSendData;

	msg[1].addr = g_pstI2CclientG->addr;
	msg[1].flags = g_pstI2CclientG->flags & I2C_M_TEN;
	msg[1].flags |= I2C_M_RD;
	msg[1].len = a_sizeRecvData;
	msg[1].buf = a_pRecvData;

	i4RetValue = i2c_transfer(g_pstI2CclientG->adapter,
				msg,
				EEPROM_I2C_MSG_SIZE_READ);

	if (i4RetValue != EEPROM_I2C_MSG_SIZE_READ) {
		pr_debug("I2C read failed!!\n");
		return -1;
	}
	return 0;
}

static int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData)
{
	int  i4RetValue = 0;
	struct i2c_msg msg;
	int i = 1;

	msg.addr = g_pstI2CclientG->addr;
	msg.flags = 0;
	msg.len = a_sizeSendData;
	msg.buf = a_pSendData;

	i4RetValue = i2c_transfer(g_pstI2CclientG->adapter,
				&msg,
				i);

	if (i4RetValue != i) {
		pr_debug("I2C write failed!!\n");
		return -1;
	}
	return 0;
}

static u16 read_otp(u32 addr)
{
	u16 get_byte = 0;
	char pu_send_cmd[1] = { (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 1, (u8 *)&get_byte, 1);

	return get_byte;
}

static void write_otp(u32 addr, u32 para)
{
	char pu_send_cmd[2] = { (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 2);
}

static u16 read_otp_16bit(u32 addr)
{
	u16 get_byte = 0;
	char pu_send_cmd[2] = { (char)((addr>>8) & 0xFF), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1);

	return get_byte;
}

static void write_otp_16bit(u32 addr, u32 para)
{
	char pu_send_cmd[3] = { (char)((addr>>8) & 0xFF), (char)(addr & 0xFF),
		(char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 3);
}

unsigned int Common_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	g_pstI2CclientG = client;
	if (iReadData_CAM_CAL(addr, size, data) == 0)
		return size;
	else
		return 0;
}

unsigned int Otp_read_region_SR846(struct i2c_client *client,
	unsigned int addr, unsigned char *data, unsigned int size)
{
	u16 ret = 0;

	u16 OTP_addr = addr & 0xFFFF;
	u8 OTP_addr_1 = OTP_addr & 0xFF;
	u8 OTP_addr_2 = (OTP_addr >> 8) & 0xFF;

	u32 readsize = size;
	u8 *read_data = data;

	g_pstI2CclientG = client;

	pr_debug("readsize: 0x%x\n", size);
	pr_debug("addr: 0x%08x\n", addr);

	//Fast sleep on
	write_otp_16bit(0x0A02, 0x01);
	//Stand by on
	write_otp_16bit(0x0A20, 0x00);
	write_otp_16bit(0x0A00, 0x00);

	mdelay(10);

	//PLL disable
	write_otp_16bit(0x0F02, 0x00);
	//CP TRIM_H
	write_otp_16bit(0x071A, 0x01);
	//IPGM TRIM_H
	write_otp_16bit(0x071B, 0x09);
	//Fsync(OTP busy) Output Enable
	write_otp_16bit(0x0D04, 0x01);
	//Fsync(OTP busy) Output Drivability
	write_otp_16bit(0x0D00, 0x07);
	//OTP R/W mode
	write_otp_16bit(0x003E, 0x10);
	write_otp_16bit(0x0A20, 0x01);
	//Stand by off
	write_otp_16bit(0x0A00, 0x01);

	//Start address H
	write_otp_16bit(0x070A, OTP_addr_2);
	//Start address L
	write_otp_16bit(0x070B, OTP_addr_1);
	//Single read
	write_otp_16bit(0x0702, 0x01);

	for (; ret < readsize; ret++) {
		//OTP data read
		*(read_data + ret) = read_otp_16bit(0x0708);
	}

	//Stand by on
	write_otp_16bit(0x0A20, 0x00);
	write_otp_16bit(0x0A00, 0x00);

	mdelay(100);

	//Display mode
	write_otp_16bit(0x003E, 0x00);
	write_otp_16bit(0x0A20, 0x01);
	//Stand by off
	write_otp_16bit(0x0A00, 0x01);

	pr_debug("OTP read done\n");
	return readsize;
}

unsigned int Otp_read_region_SR846D(struct i2c_client *client,
	unsigned int addr, unsigned char *data, unsigned int size)
{
	u16 ret = 0;

	u16 OTP_addr = addr & 0xFFFF;
	u8 OTP_addr_1 = OTP_addr & 0xFF;
	u8 OTP_addr_2 = (OTP_addr >> 8) & 0xFF;

	u32 readsize = size;
	u8 *read_data = data;

	g_pstI2CclientG = client;

	pr_debug("readsize: 0x%x\n", size);
	pr_debug("addr: 0x%08x\n", addr);

	//Fast sleep on
	write_otp_16bit(0x0A02, 0x01);
	//Stand by on
	write_otp_16bit(0x0A20, 0x00);
	write_otp_16bit(0x0A00, 0x00);

	mdelay(10);

	//PLL disable
	write_otp_16bit(0x0F02, 0x00);
	//CP TRIM_H
	write_otp_16bit(0x071A, 0x01);
	//IPGM TRIM_H
	write_otp_16bit(0x071B, 0x09);
	//Fsync(OTP busy) Output Enable
	write_otp_16bit(0x0D04, 0x01);
	//Fsync(OTP busy) Output Drivability
	write_otp_16bit(0x0D00, 0x07);
	//OTP R/W mode
	write_otp_16bit(0x003E, 0x10);
	write_otp_16bit(0x0A20, 0x01);
	//Stand by off
	write_otp_16bit(0x0A00, 0x01);

	//Start address H
	write_otp_16bit(0x070A, OTP_addr_2);
	//Start address L
	write_otp_16bit(0x070B, OTP_addr_1);
	//Single read
	write_otp_16bit(0x0702, 0x01);

	for (; ret < readsize; ret++) {
		//OTP data read
		*(read_data + ret) = read_otp_16bit(0x0708);
	}

	//Stand by on
	write_otp_16bit(0x0A20, 0x00);
	write_otp_16bit(0x0A00, 0x00);

	mdelay(100);

	//Display mode
	write_otp_16bit(0x003E, 0x00);
	write_otp_16bit(0x0A20, 0x01);
	//Stand by off
	write_otp_16bit(0x0A00, 0x01);

	pr_debug("OTP read done\n");
	return readsize;
}

unsigned int Otp_read_region_GC5035(struct i2c_client *client,
	unsigned int addr, unsigned char *data, unsigned int size)
{
	u8 OTP_clk_en_rd = 0;
	u8 OTP_clk_en_wr = 0;
	u8 OTP_read_pulse = 0;
	u8 do_read_opt = 0;
	u8 checkflag = 0;
	u32 OTP_addr = addr*8 + 0x1020;
	u32 OTP_addr_1 = OTP_addr&0xFF;
	u32 OTP_addr_2 = (OTP_addr>>8)&0xFF;
	u16 i = 0;
	u32 readsize = size;
	u8 *read_data = data;

	g_pstI2CclientG = client;
	pr_debug("readsize: 0x%x\n", size);
	pr_debug("addr: 0x%x\n", addr);

	//Initial setting
	OTP_clk_en_rd = read_otp(0xfa);
	OTP_clk_en_wr = (OTP_clk_en_rd | 0x10);
	write_otp(0xfa, OTP_clk_en_wr);
	OTP_clk_en_rd = read_otp(0xfa);

	write_otp(0xf5, 0xe9);
	write_otp(0xfe, 0x02);
	write_otp(0x67, 0xc0);
	write_otp(0x59, 0x3f);
	write_otp(0x55, 0x80);
	write_otp(0x65, 0x80);
	write_otp(0x66, 0x03);

	for (; i < readsize; i++) {
		//Set page
		write_otp(0xfe, 0x02);

		//OTP access address
		write_otp(0x69, OTP_addr_2);
		write_otp(0x6a, OTP_addr_1);

		OTP_addr = OTP_addr + 8;
		OTP_addr_1 = OTP_addr&0xFF;
		OTP_addr_2 = (OTP_addr>>8)&0xFF;

		mdelay(1);

		//Do read OTP
		OTP_read_pulse = read_otp(0xf3);
		do_read_opt = (OTP_read_pulse | 0x20);
		write_otp(0xf3, do_read_opt);

		while (1) {
			checkflag = read_otp(0x6f);
			if ((checkflag & 0x04) != 1)
				break;
			mdelay(1);
		}
		*(read_data+i) = read_otp(0x6c);
	}
	return size;
}

unsigned int Otp_read_region_GC02M1B(struct i2c_client *client,
	unsigned int addr, unsigned char *data, unsigned int size)
{
	u32 OTP_addr = addr*8 + 0x78;
	u32 OTP_addr_1 = OTP_addr&0xFF;
	u16 i = 0;
	u32 readsize = size;
	u8 *read_data = data;

	g_pstI2CclientG = client;
	pr_debug("readsize: 0x%x\n", size);
	pr_debug("addr: 0x%x\n", addr);

	//OTP enable OTP clock enable
	write_otp(0xf3, 0x30);
	//Set Page
	write_otp(0xfe, 0x02);

	for (; i < readsize; i++) {
		//OTP access address
		write_otp(0x17, OTP_addr_1);

		OTP_addr = OTP_addr + 8;
		OTP_addr_1 = OTP_addr&0xFF;

		//OTP read pulse
		write_otp(0xf3, 0x34);
		//Read
		*(read_data+i) = read_otp(0x19);
	}
	return size;
}
unsigned int Otp_read_region_GC5035_A01(struct i2c_client *client,
	unsigned int addr, unsigned char *data, unsigned int size)
{
	u8 OTP_clk_en_rd = 0;
	u8 OTP_clk_en_wr = 0;
	u8 OTP_read_pulse = 0;
	u8 do_read_opt = 0;
	u8 checkflag = 0;

	u32 OTP_addr = addr*8 + 0x1020;
	/* u32 OTP_addr = addr*8 + 0x1080; */
	u32 OTP_addr_1 = OTP_addr&0xFF;
	u32 OTP_addr_2 = (OTP_addr>>8)&0xFF;
	u16 i = 0;
	u32 readsize = size;
	u8 *read_data = data;

	g_pstI2CclientG = client;
	pr_debug("readsize: 0x%x\n", size);
	pr_debug("addr: 0x%x\n", addr);

	//Initial setting
	OTP_clk_en_rd = read_otp(0xfa);
	OTP_clk_en_wr = (OTP_clk_en_rd | 0x10);
	write_otp(0xfa, OTP_clk_en_wr);
	OTP_clk_en_rd = read_otp(0xfa);

	write_otp(0xf5, 0xe9);
	write_otp(0xfe, 0x02);
	write_otp(0x67, 0xc0);
	write_otp(0x59, 0x3f);
	write_otp(0x55, 0x80);
	write_otp(0x65, 0x80);
	write_otp(0x66, 0x03);

	for (; i < readsize; i++) {
		//Set page
		write_otp(0xfe, 0x02);

		//OTP access address
		write_otp(0x69, OTP_addr_2);
		write_otp(0x6a, OTP_addr_1);

		OTP_addr = OTP_addr + 8;
		OTP_addr_1 = OTP_addr&0xFF;
		OTP_addr_2 = (OTP_addr>>8)&0xFF;

		mdelay(1);

		//Do read OTP
		OTP_read_pulse = read_otp(0xf3);
		do_read_opt = (OTP_read_pulse | 0x20);
		write_otp(0xf3, do_read_opt);

		while (1) {
			checkflag = read_otp(0x6f);
			if ((checkflag & 0x04) != 1)
				break;
			mdelay(1);
		}
		*(read_data+i) = read_otp(0x6c);
	}
	return size;
}

