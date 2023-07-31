/* SPDX-License-Identifier: GPL-2.0 */
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
#include <linux/dma-mapping.h>
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif
#include "eeprom_utils.h"

/* Include platform define if necessary */
#ifdef EEPROM_PLATFORM_DEFINE
#include "eeprom_platform_def.h"
#endif

/************************************************************
 * I2C read function (Common)
 ************************************************************/

/* add for linux-4.4 */
#ifndef I2C_WR_FLAG
#define I2C_WR_FLAG		(0x1000)
#define I2C_MASK_FLAG	(0x00ff)
#endif

#define EEPROM_I2C_MSG_SIZE_READ 2

#ifndef EEPROM_I2C_READ_MSG_LENGTH_MAX
#define EEPROM_I2C_READ_MSG_LENGTH_MAX 255
#endif
#ifndef EEPROM_I2C_WRITE_MSG_LENGTH_MAX
#define EEPROM_I2C_WRITE_MSG_LENGTH_MAX 32
#endif
#ifndef EEPROM_WRITE_EN
#define EEPROM_WRITE_EN 1
#endif

static int Read_I2C_CAM_CAL(struct i2c_client *client,
			    u16 a_u2Addr,
			    u32 ui4_length,
			    u8 *a_puBuff)
{
	int i4RetValue = 0;
	char puReadCmd[2] = { (char)(a_u2Addr >> 8), (char)(a_u2Addr & 0xFF) };
	struct i2c_msg msg[EEPROM_I2C_MSG_SIZE_READ];

	if (ui4_length > EEPROM_I2C_READ_MSG_LENGTH_MAX) {
		pr_debug("exceed one transition %d bytes limitation\n",
			 EEPROM_I2C_READ_MSG_LENGTH_MAX);
		return -1;
	}

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = puReadCmd;

	msg[1].addr = client->addr;
	msg[1].flags = client->flags & I2C_M_TEN;
	msg[1].flags |= I2C_M_RD;
	msg[1].len = ui4_length;
	msg[1].buf = a_puBuff;

	i4RetValue = i2c_transfer(client->adapter, msg,
				EEPROM_I2C_MSG_SIZE_READ);

	if (i4RetValue != EEPROM_I2C_MSG_SIZE_READ) {
		pr_debug("I2C read data failed!!\n");
		return -1;
	}

	return 0;
}

static int iReadData_CAM_CAL(struct i2c_client *client,
			     unsigned int ui4_offset,
			     unsigned int ui4_length,
			     unsigned char *pinputdata)
{
	int i4ResidueSize;
	u32 u4CurrentOffset, u4Size;
	u8 *pBuff;

	i4ResidueSize = (int)ui4_length;
	u4CurrentOffset = ui4_offset;
	pBuff = pinputdata;
	do {
		u4Size = (i4ResidueSize >= EEPROM_I2C_READ_MSG_LENGTH_MAX)
			? EEPROM_I2C_READ_MSG_LENGTH_MAX : i4ResidueSize;

		if (Read_I2C_CAM_CAL(client, (u16) u4CurrentOffset,
				     u4Size, pBuff) != 0) {
			pr_debug("I2C iReadData failed!!\n");
			return -1;
		}

		i4ResidueSize -= u4Size;
		u4CurrentOffset += u4Size;
		pBuff += u4Size;
	} while (i4ResidueSize > 0);

	return 0;
}

#if EEPROM_WRITE_EN
static int Write_I2C_CAM_CAL(struct i2c_client *client,
			     u16 a_u2Addr,
			     u32 ui4_length,
			     u8 *a_puBuff)
{
	int i4RetValue = 0;
	char puCmd[2 + EEPROM_I2C_WRITE_MSG_LENGTH_MAX];
	struct i2c_msg msg;

	if (ui4_length > EEPROM_I2C_WRITE_MSG_LENGTH_MAX) {
		pr_debug("exceed one transition %d bytes limitation\n",
			 EEPROM_I2C_WRITE_MSG_LENGTH_MAX);
		return -1;
	}

	puCmd[0] = (char)(a_u2Addr >> 8);
	puCmd[1] = (char)(a_u2Addr & 0xFF);
	memcpy(puCmd + 2, a_puBuff, ui4_length);

	msg.addr = client->addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = 2 + ui4_length;
	msg.buf = puCmd;

	i4RetValue = i2c_transfer(client->adapter, &msg, 1);

	if (i4RetValue != 1) {
		pr_debug("I2C write data failed!!\n");
		return -1;
	}

	/* Wait for write complete */
	mdelay(5);

	return 0;
}

static int iWriteData_CAM_CAL(struct i2c_client *client,
			     unsigned int ui4_offset,
			     unsigned int ui4_length,
			     unsigned char *pinputdata)
{
	int i4ResidueSize;
	u32 u4CurrentOffset, u4Size;
	u8 *pBuff;

	i4ResidueSize = (int)ui4_length;
	u4CurrentOffset = ui4_offset;
	pBuff = pinputdata;
	do {
		u4Size = (i4ResidueSize >= EEPROM_I2C_WRITE_MSG_LENGTH_MAX)
			? EEPROM_I2C_WRITE_MSG_LENGTH_MAX : i4ResidueSize;

		if (Write_I2C_CAM_CAL(client, (u16) u4CurrentOffset,
					u4Size, pBuff) != 0) {
			pr_debug("I2C iWriteData failed!!\n");
			return -1;
		}

		i4ResidueSize -= u4Size;
		u4CurrentOffset += u4Size;
		pBuff += u4Size;
	} while (i4ResidueSize > 0);

	return 0;
}
#endif

static int iReadRegI2C(struct i2c_client *client,
			u8 *a_pSendData, u16 a_sizeSendData,
			u8 *a_pRecvData, u16 a_sizeRecvData)
{
	int  i4RetValue = 0;
	struct i2c_msg msg[EEPROM_I2C_MSG_SIZE_READ];

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = a_sizeSendData;
	msg[0].buf = a_pSendData;

	msg[1].addr = client->addr;
	msg[1].flags = client->flags & I2C_M_TEN;
	msg[1].flags |= I2C_M_RD;
	msg[1].len = a_sizeRecvData;
	msg[1].buf = a_pRecvData;

	i4RetValue = i2c_transfer(client->adapter,
				msg,
				EEPROM_I2C_MSG_SIZE_READ);

	if (i4RetValue != EEPROM_I2C_MSG_SIZE_READ) {
		pr_debug("I2C read failed!!\n");
		return -1;
	}
	return 0;
}

static int iWriteRegI2C(struct i2c_client *client,
			u8 *a_pSendData, u16 a_sizeSendData)
{
	int  i4RetValue = 0;
	struct i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = a_sizeSendData;
	msg.buf = a_pSendData;

	i4RetValue = i2c_transfer(client->adapter,
				&msg,
				1);

	if (i4RetValue != 1) {
		pr_debug("I2C write failed!!\n");
		return -1;
	}
	return 0;
}

static u16 read_otp(struct i2c_client *client, u32 addr)
{
	u16 get_byte = 0;
	char pu_send_cmd[1] = { (char)(addr & 0xFF) };

	iReadRegI2C(client, pu_send_cmd, 1, (u8 *)&get_byte, 1);

	return get_byte;
}

static void write_otp(struct i2c_client *client, u32 addr, u32 para)
{
	char pu_send_cmd[2] = { (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(client, pu_send_cmd, 2);
}

static u16 read_otp_16bit(struct i2c_client *client, u32 addr)
{
	u16 get_byte = 0;
	char pu_send_cmd[2] = { (char)((addr >> 8) & 0xFF),
		(char)(addr & 0xFF) };

	iReadRegI2C(client, pu_send_cmd, 2, (u8 *)&get_byte, 1);

	return get_byte;
}

static void write_otp_16bit(struct i2c_client *client, u32 addr, u32 para)
{
	char pu_send_cmd[3] = { (char)((addr >> 8) & 0xFF),
		(char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(client, pu_send_cmd, 3);
}

unsigned int Common_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	unsigned int ret = 0;
	struct timeval t;

	EEPROM_PROFILE_INIT(&t);

	if (iReadData_CAM_CAL(client, addr, size, data) == 0)
		ret = size;

	EEPROM_PROFILE(&t, "common_read_time");

	return ret;
}

unsigned int Common_write_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	unsigned int ret = 0;
#if EEPROM_WRITE_EN
	struct timeval t;

	EEPROM_PROFILE_INIT(&t);

	if (iWriteData_CAM_CAL(client, addr, size, data) == 0)
		ret = size;

	EEPROM_PROFILE(&t, "common_write_time");
#else
	pr_debug("Write operation disabled\n");
#endif

	return ret;
}

unsigned int DW9763_write_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	unsigned int ret = 0;
#if EEPROM_WRITE_EN
	struct timeval t;

	int i4RetValue = 0;
	char puCmd[2];
	struct i2c_msg msg;

	EEPROM_PROFILE_INIT(&t);

	puCmd[0] = (char)(0x81);
	puCmd[1] = (char)(0xEE);

	msg.addr = client->addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = 2;
	msg.buf = puCmd;

	i4RetValue = i2c_transfer(client->adapter, &msg, 1);

	if (i4RetValue != 1) {
		pr_debug("I2C erase data failed!!\n");
		return -1;
	}

	/* Wait for erase complete */
	mdelay(30);

	if (iWriteData_CAM_CAL(client, addr, size, data) == 0)
		ret = size;

	EEPROM_PROFILE(&t, "DW9763_write_time");
#else
	pr_debug("Write operation disabled\n");
#endif

	return ret;
}

unsigned int Otp_read_region_S5K4HAYX(struct i2c_client *client,
	unsigned int addr, unsigned char *data, unsigned int size)
{
	u32 ret = 0;

	u16 OTP_addr = 0x0A04 + (addr + 0x0004) % 0x0040;
	u16 OTP_page_select = 0x0011 + (addr + 0x0004) / 0x0040;

	u8 Complete = 0x00;
	u32 count = 0;
	u32 readsize = size;
	u8 *read_data = data;

	pr_debug("readsize: 0x%x\n", size);
	pr_debug("addr: 0x%04x\n", addr);

	if (OTP_addr > 0x0A43 || OTP_addr < 0x0A04 || OTP_page_select > 0xFF) {
		pr_debug("Wrong OTP address:0x%04x, page:%d\n"
			, OTP_addr, OTP_page_select);
		return ret;
	}

	//Streaming On
	pr_debug("Streaming On\n");
	write_otp_16bit(client, 0x0100, 0x01);
	mdelay(1);

	do {
		//Write OTP Page
		write_otp_16bit(client, 0x0A02, OTP_page_select);
		//Write Read CMD
		write_otp_16bit(client, 0x0A00, 0x01);
		//Check Complete Flag
		count = 0;
		do {
			if (count == 100) {
				pr_debug("OTP read too much time! More than 100ms\n");
				return ret;
			}
			mdelay(1);
			Complete =
				read_otp_16bit(client, 0x0A01) & 0x01;
			count++;
		} while (!Complete);
		//Do Read Data
		do {
			*(read_data + ret) = read_otp_16bit(client, OTP_addr);
			ret++;
			OTP_addr++;
			if (ret == readsize) {
				//Initial State
				write_otp_16bit(client, 0x0A00, 0x04);
				write_otp_16bit(client, 0x0A00, 0x00);
				pr_debug("OTP read done\n");
				return readsize;
			}
		} while (OTP_addr < 0x0A44);
		//Make Initial State
		write_otp_16bit(client, 0x0A00, 0x04);
		write_otp_16bit(client, 0x0A00, 0x00);
		//Page Round-up
		OTP_addr = 0x0A04;
		OTP_page_select++;
		if (OTP_page_select > 0xFF) {
			pr_debug("Last Page last address!\n");
			return ret;
		}
	} while (ret < readsize);
	pr_debug("Unexpected exit\n");
	return ret;
}

unsigned int Otp_read_region_S5K3L6(struct i2c_client *client,
	unsigned int addr, unsigned char *data, unsigned int size)
{
	u32 ret = 0;

	u16 OTP_addr = 0x0A04 + (addr + 0x0004) % 0x0040;
	u16 OTP_page_select = 0x0034 + (addr + 0x0004) / 0x0040;

	u32 readsize = size;
	u8 *read_data = data;

	pr_debug("readsize: 0x%x\n", size);
	pr_debug("addr: 0x%04x\n", addr);

	if (OTP_addr > 0x0A43 || OTP_addr < 0x0A04 || OTP_page_select > 0xFF) {
		pr_debug("Wrong OTP address:0x%04x, page:%d\n"
			, OTP_addr, OTP_page_select);
		return ret;
	}

	//Make Initial State
	write_otp_16bit(client, 0x0A00, 0x04);

	do {
		//Write OTP Page
		write_otp_16bit(client, 0x0A02, OTP_page_select);
		//Write Read CMD
		write_otp_16bit(client, 0x0A00, 0x01);
		mdelay(1);
		//Do Read Data
		do {
			*(read_data + ret) = read_otp_16bit(client, OTP_addr);
			ret++;
			OTP_addr++;
			if (ret == readsize) {
				//Initial State
				write_otp_16bit(client, 0x0A00, 0x04);
				write_otp_16bit(client, 0x0A00, 0x00);
				pr_debug("OTP read done\n");
				return readsize;
			}
		} while (OTP_addr < 0x0A44);
		//Make Initial State
		write_otp_16bit(client, 0x0A00, 0x04);
		write_otp_16bit(client, 0x0A00, 0x00);
		//Page Round-up
		OTP_addr = 0x0A04;
		OTP_page_select++;
		if (OTP_page_select > 0xFF) {
			pr_debug("Last Page last address!\n");
			return ret;
		}
	} while (ret < readsize);
	pr_debug("Unexpected exit\n");
	return ret;
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

	pr_debug("readsize: 0x%x\n", size);
	pr_debug("addr: 0x%04x\n", addr);

	//Fast sleep on
	write_otp_16bit(client, 0x0A02, 0x01);
	//Stand by on
	write_otp_16bit(client, 0x0A20, 0x00);
	write_otp_16bit(client, 0x0A00, 0x00);

	mdelay(10);

	//PLL disable
	write_otp_16bit(client, 0x0F02, 0x00);
	//CP TRIM_H
	write_otp_16bit(client, 0x071A, 0x01);
	//IPGM TRIM_H
	write_otp_16bit(client, 0x071B, 0x09);
	//Fsync(OTP busy) Output Enable
	write_otp_16bit(client, 0x0D04, 0x01);
	//Fsync(OTP busy) Output Drivability
	write_otp_16bit(client, 0x0D00, 0x07);
	//OTP R/W mode
	write_otp_16bit(client, 0x003E, 0x10);
	write_otp_16bit(client, 0x0A20, 0x01);
	//Stand by off
	write_otp_16bit(client, 0x0A00, 0x01);

	//Start address H
	write_otp_16bit(client, 0x070A, OTP_addr_2);
	//Start address L
	write_otp_16bit(client, 0x070B, OTP_addr_1);
	//Single read
	write_otp_16bit(client, 0x0702, 0x01);

	for (; ret < readsize; ret++) {
		//OTP data read
		*(read_data + ret) = read_otp_16bit(client, 0x0708);
	}

	//Stand by on
	write_otp_16bit(client, 0x0A20, 0x00);
	write_otp_16bit(client, 0x0A00, 0x00);

	mdelay(100);

	//Display mode
	write_otp_16bit(client, 0x003E, 0x00);
	write_otp_16bit(client, 0x0A20, 0x01);
	//Stand by off
	write_otp_16bit(client, 0x0A00, 0x01);

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

	pr_debug("readsize: 0x%x\n", size);
	pr_debug("addr: 0x%04x\n", addr);

	//Initial setting
	OTP_clk_en_rd = read_otp(client, 0xfa);
	OTP_clk_en_wr = (OTP_clk_en_rd | 0x10);
	write_otp(client, 0xfa, OTP_clk_en_wr);
	OTP_clk_en_rd = read_otp(client, 0xfa);

	write_otp(client, 0xf5, 0xe9);
	write_otp(client, 0xfe, 0x02);
	write_otp(client, 0x67, 0xc0);
	write_otp(client, 0x59, 0x3f);
	write_otp(client, 0x55, 0x80);
	write_otp(client, 0x65, 0x80);
	write_otp(client, 0x66, 0x03);

	for (; i < readsize; i++) {
		//Set page
		write_otp(client, 0xfe, 0x02);

		//OTP access address
		write_otp(client, 0x69, OTP_addr_2);
		write_otp(client, 0x6a, OTP_addr_1);

		OTP_addr = OTP_addr + 8;
		OTP_addr_1 = OTP_addr&0xFF;
		OTP_addr_2 = (OTP_addr>>8)&0xFF;

		mdelay(1);

		//Do read OTP
		OTP_read_pulse = read_otp(client, 0xf3);
		do_read_opt = (OTP_read_pulse | 0x20);
		write_otp(client, 0xf3, do_read_opt);

		while (1) {
			checkflag = read_otp(client, 0x6f);
			if ((checkflag & 0x04) != 1)
				break;
			mdelay(1);
		}
		*(read_data+i) = read_otp(client, 0x6c);
	}
	return size;
}

unsigned int Otp_read_region_GC02M1B(struct i2c_client *client,
	unsigned int addr, unsigned char *data, unsigned int size)
{
	u32 OTP_addr = addr*8 + 0x78;
	u32 OTP_addr_1 = addr&0xFF;
	u16 i = 0;
	u32 readsize = size;
	u8 *read_data = data;

	pr_debug("readsize: 0x%x\n", size);
	pr_debug("addr: 0x%x\n", addr);

	//OTP enable OTP clock enable
	write_otp(client, 0xf3, 0x30);
	//Set Page
	write_otp(client, 0xfe, 0x02);

	for (; i < readsize; i++) {
		//OTP access address
		write_otp(client, 0x17, OTP_addr_1);

		OTP_addr = OTP_addr + 8;
		OTP_addr_1 = OTP_addr&0xFF;

		//OTP read pulse
		write_otp(client, 0xf3, 0x34);
		//Read
		*(read_data+i) = read_otp(client, 0x19);
	}
	return size;
}
unsigned int BL24SA64_write_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	unsigned int ret = 0;
#if EEPROM_WRITE_EN
	struct timeval t;

	unsigned char test_read = 0x00;
	unsigned char unlock_cmd = 0x40;
	unsigned char lock_cmd = 0x78;
	unsigned char unlock_val = 0x00 | ((client->addr) & 0x7) << 4; // 0x50 -> 0x00
	unsigned char lock_val = unlock_val | 0x0F; // 0x50 -> 0x0F
	unsigned int ori_addr = 0x00; // to store the current address during sending cmd.
	unsigned int exp_addr = 0x00; // to store the expected address after lock EEPROM.
	unsigned int i = 0;

	EEPROM_PROFILE_INIT(&t);

/************ test read EEPROM ************/

	exp_addr = client->addr;
	if (iReadData_CAM_CAL(client, 0x0008, 1, &test_read) < 0) {
		pr_debug("Read EEPROM ID failed\n");
		pr_debug("Start looping slave address 0x50 ~ 0x57\n");
		for (i = 0; i < 8; i++) {
			client->addr = 0x50+i;
			pr_debug("Change slave address to 0x%02x\n", client->addr);
			if (iReadData_CAM_CAL(client, 0x0008, 1, &test_read) == 0) {
				pr_debug("EEPROM ID = 0x%02x\n", test_read);
				break;
			}
		}
	} else
		pr_debug("EEPROM ID = 0x%02x\n", test_read);

	if (iReadData_CAM_CAL(client, 0x8000, 1, &test_read) < 0) {
		pr_debug("Read register failed\n");
		return -1;
	}
	pr_debug("Register ID = 0x%02x\n", test_read);

/************ unlock EEPROM ************/

	pr_debug("BL24SA64 write unlock 0x%02x\n", unlock_val);

	ori_addr = client->addr;
	client->addr = unlock_cmd;

	iWriteData_CAM_CAL(client, 0x8000, 1, &unlock_val);
	pr_debug("BL24SA64 unlock part1\n");

	client->addr = ori_addr;

	if (iWriteData_CAM_CAL(client, 0x8000, 1, &unlock_val) < 0) {
		pr_debug("Unlock protection failed!!\n");
		return -1;
	}
	pr_debug("BL24SA64 unlock done\n");

/************ test read EEPROM ************/

	if (iReadData_CAM_CAL(client, 0x8000, 1, &test_read) == 0)
		pr_debug("Register ID = 0x%02x\n", test_read);
	else {
		pr_debug("Read register failed!!\n");
		return -1;
	}

/************ write EEPROM ************/

	if (iWriteData_CAM_CAL(client, addr, size, data) < 0)
		pr_debug("Write EEPROM failed!!\n");
	else
		ret = size;
	pr_debug("Write EEPROM ret = %d\n", ret);

/************ lock EEPROM ************/

	pr_debug("BL24SA64 write lock 0x%02x\n", lock_val);

	ori_addr = client->addr;
	client->addr = lock_cmd;

	iWriteData_CAM_CAL(client, 0x8000, 1, &lock_val);
	pr_debug("BL24SA64 lock part1\n");

	client->addr = ori_addr;

	if (iWriteData_CAM_CAL(client, 0x8000, 1, &lock_val) < 0) {
		pr_debug("Lock protection failed!!\n");
		return -1;
	}
	pr_debug("BL24SA64 lock done\n");

/************ test read EEPROM ************/

	client->addr = exp_addr;
	if (iReadData_CAM_CAL(client, 0x8000, 1, &test_read) == 0)
		pr_debug("Register ID = 0x%02x\n", test_read);
	else {
		pr_debug("Failed to read register!!\n");
		return -1;
	}

/***************************************/

	EEPROM_PROFILE(&t, "BL24SA64_write_time");
#else
	pr_debug("Write operation disabled\n");
#endif

	return ret;
}
