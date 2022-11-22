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

#define EEPROM_I2C_MSG_SIZE_READ 2
#ifndef EEPROM_I2C_READ_MSG_LENGTH_MAX
#define EEPROM_I2C_READ_MSG_LENGTH_MAX 32
#endif
#ifndef EEPROM_I2C_WRITE_MSG_LENGTH_MAX
#define EEPROM_I2C_WRITE_MSG_LENGTH_MAX 32
#endif

void set_global_i2c_client(struct i2c_client *client)
{
	spin_lock(&g_spinLock);
	g_pstI2CclientG = client;
	spin_unlock(&g_spinLock);
}

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
	/*
	 * u8 *pdata = a_pSendData;
	 * u8 *pend = a_pSendData + a_sizeSendData;
	 */
	int i = 1;

	msg.addr = g_pstI2CclientG->addr;
	msg.flags = 0;
	msg.len = a_sizeSendData;
	msg.buf = a_pSendData;

	i4RetValue = i2c_transfer(g_pstI2CclientG->adapter,
				&msg, i);

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

unsigned int Common_read_region(struct i2c_client *client, struct stCAM_CAL_INFO_STRUCT *sensor_info,
				unsigned int addr, unsigned char *data, unsigned int size)
{
	spin_lock(&g_spinLock);
	g_pstI2CclientG = client;
	spin_unlock(&g_spinLock);
	if (iReadData_CAM_CAL(addr, size, data) == 0)
		return size;
	else
		return 0;
}

unsigned int Otp_read_region_SR846D(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	unsigned int ret = 0;

	u16 OTP_addr = addr & 0xFFFF;
	u16 OTP_page_select = (addr >> 16) & 0xFFFF;
	u8 OTP_addr_1 = OTP_addr & 0xFF;
	u8 OTP_addr_2 = (OTP_addr >> 8) & 0xFF;

	u8 Complete = 0x00;
	unsigned int count = 0;
	u32 readsize = size;
	u8 *read_data = data;

	g_pstI2CclientG = client;
	pr_debug("readsize: 0x%x\n", size);
	pr_debug("addr: 0x%08x\n", addr);

	if (OTP_addr_1 > 0x43 || OTP_addr_2 != 0x0A || OTP_page_select > 0xFF) {
		pr_debug("Wrong OTP address:0x%04x, page:%d\n"
			, OTP_addr, OTP_page_select);
		return ret;
	}

	//Streaming On
	pr_debug("Streaming On\n");
	write_otp(0x0100, 0x01);
	msleep(50);

	//Initial State
	write_otp(0x0A00, 0x04);
	//Write OTP Page
	write_otp(0x0A02, OTP_page_select);
	//Write Read CMD
	write_otp(0x0A00, 0x01);

	for (; ret < readsize; ret++) {
		//Check If Start Read A New Page
		if (OTP_addr_1 == 0x04) {
			//Initial State
			write_otp(0x0A00, 0x04);
			//Write OTP Page
			write_otp(0x0A02, OTP_page_select);
			//Write Read CMD
			write_otp(0x0A00, 0x01);
			//Check Complete Flag
			count = 0;
			do {
				if (count == 100) {
					pr_debug("OTP read too much time! More than 100ms\n");
					return ret;
				}
				usleep_range(1000, 1001);
				Complete = read_otp(0x0A01) & 0x01;
				count++;
			} while (!Complete);
		}
		*(read_data + ret) = read_otp(OTP_addr);
		pr_debug("otp_data[0x%04x] = 0x%04x\n", ret, *(read_data + ret));

		OTP_addr++;
		//Turn Next Page
		if ((OTP_addr&0xFF) > 0x43) {
			//Initial State
			write_otp(0x0A00, 0x04);
			write_otp(0x0A00, 0x00);
			//Round-up
			OTP_addr = (OTP_addr & 0xFF00) + 0x0004;
			OTP_page_select++;
			if (OTP_page_select > 0xFF) {
				pr_debug("Last Page last address!\n");
				if (ret < readsize)
					pr_debug("Overflow...\n");
				return ret + 1;
			}
			pr_debug("Turn next page in OTP address:0x%04x, page:%d\n"
				, OTP_addr, OTP_page_select);
		}
		OTP_addr_1 = OTP_addr & 0xFF;
		OTP_addr_2 = (OTP_addr >> 8) & 0xFF;
	}
	//Initial State
	write_otp(0x0A00, 0x04);
	write_otp(0x0A00, 0x00);
	pr_debug("OTP read done\n");
	return readsize;
}

