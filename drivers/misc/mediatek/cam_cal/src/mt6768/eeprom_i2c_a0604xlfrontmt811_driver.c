/*
 * Copyright (C) 2018 MediaTek Inc.
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
#define PFX "CAM_CAL_a0604xlfrontmt811"
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
#include "i2c-mtk.h"
#include <linux/dma-mapping.h>
#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#include "eeprom_i2c_a0604xlfrontmt811_driver.h"

#define EEPROM_I2C_MSG_SIZE_READ 2


struct a0604xlfrontmt811_otp_struct
{
	int Module_Info_Flag;
	int module_ifo_data[15];
	int WB_FLAG;
	int wb_data[17];
	int LSC_FLAG;
	int lsc_data[1869];
	int AF_FLAG;
	int af_data[5];
}a0604xlfrontmt811_otp;


/************************************************************
 * I2C read function (Custom)
 * Customer's driver can put on here
 * Below is an example
 ************************************************************/
static int iReadRegI2C(struct i2c_client *client,
		u8 *a_pSendData, u16 a_sizeSendData,
		u8 *a_pRecvData, u16 a_sizeRecvData)
{
	int i4RetValue = 0;
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

	i4RetValue = i2c_transfer(client->adapter, msg,
				EEPROM_I2C_MSG_SIZE_READ);

	if (i4RetValue != EEPROM_I2C_MSG_SIZE_READ) {
		pr_debug("I2C read failed!!\n");
		return -1;
	}
	return 0;
}

static int sensor_reg_read(struct i2c_client *client, const u16 addr)
{
	u8 buf[2] = {addr >> 8, addr & 0xff};
	u32 ret;
	struct i2c_msg msgs[] = {
		{
			.addr  = client->addr,
			.flags = 0,
			.len   = 2,
			.buf   = buf,
		}, {
			.addr  = client->addr,
			.flags = I2C_M_RD,
			.len   = 2,
			.buf   = buf,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));

	if (ret < 0) {
		pr_warning("reading register %x from %x failed\n", addr, client->addr);
		return ret;
	}

	return buf[0] & 0xff; /* no sign-extension */
}

static sensor_reg_write (struct i2c_client *client, const u16 addr, const u8 data)
{
	struct i2c_msg msg;
	unsigned char tx[3];
	u32 ret;

	msg.addr = client->addr;
	msg.buf = tx;
	msg.len = 3;
	msg.flags = 0;

	tx[0] = addr >> 8;
	tx[1] = addr & 0xff;
	tx[2] = data;

	ret = i2c_transfer(client->adapter, &msg, 1);

	if (ret == 1)
		return 0;
	else {
		pr_warning("write register %x from %x failed\n", addr, client->addr);
		return -EIO;
	}
}

static int a0604xlfrontmt811_read_otp(struct i2c_client *client,
				u32 start_addr, u8 *data, u32 otp_len)
{
	u32 rtn = 0;
	u8 cnt = 0;
	//int addr = 0;
	//int i, j,k = 0;
	//int module_info_checksum = 0 , wb_data_checksum = 0 , lsc_data_checksum = 0 ;
	u16 max_len_once = 0x1000;
	u16 addr_offset = 0x1000;
	u16 map_addr = start_addr + addr_offset;
	u8 pu_send_cmd[2] = { (u8) (map_addr >> 8), (u8) (map_addr & 0xFF) };
	printk("a0604xlfrontmt811_read_otp start_addr = %x size = %d data read = %d\n", start_addr, otp_len, rtn);

	if (otp_len > 8192) {
		pr_warning("otp read len can not over 8K");
		rtn = -1;
		return rtn;
	}
	do {
		/* 1. Enable otp work and ecc bypass */
		sensor_reg_write(client, 0x0c00, 0x01);
		sensor_reg_write(client, 0x0c18, 0x01);
		/* 2. Set otp read act addr */
		sensor_reg_write(client, 0x0c13, (start_addr >> 8) & 0xff);
		sensor_reg_write(client, 0x0c14, start_addr & 0xff);
		/* 3. Set otp read mode */
		sensor_reg_write(client, 0x0c16, 0x70);
		/* 4. Wait for read complete status setting.*/
		do {
			if (sensor_reg_read(client, 0x0c1a) == 0x01)
				break;
			cnt++;
		} while (cnt < 10);
		if (cnt > 9) {
			pr_warning("otp read data fail");
			rtn = -1;
			break;
		}
		/* 5. Read otp data once or twice */
		if (otp_len > max_len_once) {
			rtn = iReadRegI2C(client, pu_send_cmd, 2, data, max_len_once);
			pu_send_cmd[0] += (max_len_once >> 8);
			sensor_reg_write(client, 0x0c13, ((start_addr + max_len_once) >> 8) & 0xff);
			sensor_reg_write(client, 0x0c14, (start_addr + max_len_once) & 0xff);
			sensor_reg_write(client, 0x0c16, 0x70);
			do {
				if (sensor_reg_read(client, 0x0c1a) == 0x01)
					break;
				cnt++;
			} while (cnt < 10);
			if (cnt > 9) {
				pr_warning("otp read data fail");
				rtn = -1;
				break;
			}
			rtn = iReadRegI2C(client, pu_send_cmd, 2, data + max_len_once, otp_len - max_len_once);
		}
		else
			rtn = iReadRegI2C(client, pu_send_cmd, 2, data, otp_len);
	} while(0);
	/* 6. Exist otp working status */
	sensor_reg_write(client, 0x0c17, 0x01);
	/* 7. Disable otp work */
	sensor_reg_write(client, 0x0c00, 0x00);
	printk("a0604xlfrontmt811_read_otp start_addr = %x size = %d data read = %d\n", start_addr, otp_len, rtn);

	return rtn;
}

unsigned int a0604xlfrontmt811_read_region(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size)
{
	/* otp_offset need update when otp data write offset confirm */
	//u16 otp_offset = 0x0604;
	//addr += otp_offset;
	if (a0604xlfrontmt811_read_otp(client, addr, data, size) == 0)
		return size;
	else
		return 0;
}

