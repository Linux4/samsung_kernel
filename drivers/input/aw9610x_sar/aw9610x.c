/*
* aw9610x.c
*
* Copyright (c) 2020 AWINIC Technology CO., LTD
*
* Author: Alex <zhangpengbiao@awinic.com>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <linux/hardware_info.h>
#include "aw_bin_parse.h"
#include "aw9610x.h"
#include "aw9610x_reg.h"

#define AW9610X_I2C_NAME "aw9610x_sar"
#define AW9610X_DRIVER_VERSION "v0.1.9"

#define AW_READ_CHIPID_RETRIES		(5)
#define AW_I2C_RETRIES			(5)
#define AW9610X_SCAN_DEFAULT_TIME	(10000)
#define CALI_FILE_MAX_SIZE		(128)

#define LOG_TAG "<AW_LOG>"
#define LOG_INFO(fmt, args...)  pr_info(LOG_TAG "[INFO]" "<%s><%d>"fmt, __func__, __LINE__, ##args)
static int user_test = 0;
static uint8_t force_report_far = 1;
static int mEnabled;
aw9610x_t aw9610x_ptr;
#ifdef ANFR_TEST
#define MAX_INT_COUNT 20
#endif
/******************************************************
*
* aw9610x i2c write/read
*
******************************************************/
static int32_t
i2c_write(struct aw9610x *aw9610x, uint16_t reg_addr16, uint32_t reg_data32)
{
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = aw9610x->i2c;
	struct i2c_msg msg;
	uint8_t w_buf[6];

	/*reg_addr*/
	w_buf[0] = (u8)(reg_addr16 >> 8);
	w_buf[1] = (u8)(reg_addr16);
	/*data*/
	w_buf[2] = (u8)(reg_data32 >> 24);
	w_buf[3] = (u8)(reg_data32 >> 16);
	w_buf[4] = (u8)(reg_data32 >> 8);
	w_buf[5] = (u8)(reg_data32);

	msg.addr = i2c->addr;
	msg.flags = AW9610X_I2C_WR;
	msg.len = 6;
	/*2 bytes regaddr + 4 bytes data*/
	msg.buf = (unsigned char *)w_buf;

	ret = i2c_transfer(i2c->adapter, &msg, 1);
	if (ret < 0)
		AWLOGE(aw9610x->dev,
			"Write reg is 0x%x,error value = %d", reg_addr16, ret);

	return ret;
}

static int32_t
i2c_read(struct aw9610x *aw9610x, uint16_t reg_addr16, uint32_t *reg_data32)
{
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = aw9610x->i2c;
	struct i2c_msg msg[2];
	uint8_t w_buf[2];
	uint8_t buf[4];

	w_buf[0] = (unsigned char)(reg_addr16 >> 8);
	w_buf[1] = (unsigned char)(reg_addr16);
	msg[0].addr = i2c->addr;
	msg[0].flags = AW9610X_I2C_WR;
	msg[0].len = 2;
	msg[0].buf = (unsigned char *)w_buf;

	msg[1].addr = i2c->addr;
	msg[1].flags = AW9610X_I2C_RD;
	msg[1].len = 4;
	msg[1].buf = (unsigned char *)buf;

	ret = i2c_transfer(i2c->adapter, msg, 2);
	if (ret < 0)
		AWLOGE(aw9610x->dev,
			"Read reg is 0x%x,error value = %d", reg_addr16, ret);

	reg_data32[0] = ((u32)buf[3]) | ((u32)buf[2]<<8) |
			((u32)buf[1]<<16) | ((u32)buf[0]<<24);

	return ret;
}

static int32_t aw9610x_i2c_write(struct aw9610x *aw9610x,
				uint16_t reg_addr16, uint32_t reg_data32)
{
	int32_t ret = -1;
	uint8_t cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_write(aw9610x, reg_addr16, reg_data32);
		if (ret < 0)
			AWLOGE(aw9610x->dev,
					"write cnt = %d,error = %d", cnt, ret);
		else
			break;

		cnt++;
	}

	return ret;
}

static int32_t aw9610x_i2c_read(struct aw9610x *aw9610x,
				uint16_t reg_addr16, uint32_t *reg_data32)
{
	int32_t ret = -1;
	uint8_t cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_read(aw9610x, reg_addr16, reg_data32);
		if (ret < 0)
			AWLOGE(aw9610x->dev,
					"i2c_read cnt=%d error=%d", cnt, ret);
		else
			break;
		cnt++;
	}

	return ret;
}

static int32_t
aw9610x_i2c_write_bits(struct aw9610x *aw9610x, uint16_t reg_addr16,
				uint32_t mask, uint32_t reg_data32)
{
	uint32_t reg_val;

	aw9610x_i2c_read(aw9610x, reg_addr16, &reg_val);
	reg_val &= mask;
	reg_val |= reg_data32;
	aw9610x_i2c_write(aw9610x, reg_addr16, reg_val);

	return 0;
}

/******************************************************************************
*
* aw9610x i2c sequential write/read --- one first addr with multiple data.
*
******************************************************************************/
static int32_t i2c_write_seq(struct aw9610x *aw9610x)
{
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = aw9610x->i2c;
	struct i2c_msg msg;
	uint8_t w_buf[228];
	uint8_t addr_bytes = aw9610x->aw_i2c_package.addr_bytes;
	uint8_t msg_cnt = 0;
	uint8_t data_bytes = aw9610x->aw_i2c_package.data_bytes;
	uint8_t reg_num = aw9610x->aw_i2c_package.reg_num;
	uint8_t *p_reg_data = aw9610x->aw_i2c_package.p_reg_data;
	uint8_t msg_idx = 0;

	for (msg_idx = 0; msg_idx < addr_bytes; msg_idx++) {
		w_buf[msg_idx] = aw9610x->aw_i2c_package.init_addr[msg_idx];
		AWLOGI(aw9610x->dev, "w_buf_addr[%d] = 0x%02x",
						msg_idx, w_buf[msg_idx]);
	}
	msg_cnt = addr_bytes;
	for (msg_idx = 0; msg_idx < data_bytes * reg_num; msg_idx++) {
		w_buf[msg_cnt] = *p_reg_data++;
		msg_cnt++;
	}
	AWLOGD(aw9610x->dev, "%d", msg_cnt);
	p_reg_data = aw9610x->aw_i2c_package.p_reg_data;
	msg.addr = i2c->addr;
	msg.flags = AW9610X_I2C_WR;
	msg.len = msg_cnt;
	msg.buf = (uint8_t *)w_buf;
	ret = i2c_transfer(i2c->adapter, &msg, 1);
	if (ret < 0)
		AWLOGE(aw9610x->dev, "i2c write seq error %d", ret);

	return ret;
}

static int32_t i2c_read_seq(struct aw9610x *aw9610x, uint8_t *reg_data)
{
	int32_t ret =  -ENOMEM;
	struct i2c_client *i2c = aw9610x->i2c;
	struct i2c_msg msg[2];
	uint8_t w_buf[4];
	uint8_t buf[228];
	uint8_t data_bytes = aw9610x->aw_i2c_package.data_bytes;
	uint8_t reg_num = aw9610x->aw_i2c_package.reg_num;
	uint8_t addr_bytes = aw9610x->aw_i2c_package.addr_bytes;
	uint8_t msg_idx = 0;
	uint8_t msg_cnt = 0;

	/*
	* step 1 : according to addr_bytes assemble first_addr.
	* step 2 : initialize msg[0] including first_addr transfer to client.
	* step 3 : wait for client return reg_data.
	*/
	for (msg_idx = 0; msg_idx < addr_bytes; msg_idx++) {
		w_buf[msg_idx] = aw9610x->aw_i2c_package.init_addr[msg_idx];
		AWLOGD(aw9610x->dev, "w_buf_addr[%d] = 0x%02x",
					msg_idx, w_buf[msg_idx]);
	}
	msg[0].addr = i2c->addr;
	msg[0].flags = AW9610X_I2C_WR;
	msg[0].len = msg_idx;
	msg[0].buf = (uint8_t *)w_buf;

	/*
	* recieve client to msg[1].buf.
	*/
	msg_cnt = data_bytes * reg_num;
	msg[1].addr = i2c->addr;
	msg[1].flags = AW9610X_I2C_RD;
	msg[1].len = msg_cnt;
	msg[1].buf = (uint8_t *)buf;

	ret = i2c_transfer(i2c->adapter, msg, 2);
	if (ret < 0) {
		AWLOGE(aw9610x->dev, "i2c write error %d", ret);
		return ret;
	}

	for (msg_idx = 0; msg_idx < msg_cnt; msg_idx++) {
		reg_data[msg_idx] = buf[msg_idx];
		AWLOGD(aw9610x->dev, "buf = 0x%02x", buf[msg_idx]);
	}

	return ret;
}

static void
aw9610x_addrblock_load(struct device *dev, const char *buf)
{
	uint32_t addrbuf[4] = { 0 };
	uint8_t temp_buf[2] = { 0 };
	uint32_t i = 0;
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	uint8_t addr_bytes = aw9610x->aw_i2c_package.addr_bytes;
	uint8_t reg_num = aw9610x->aw_i2c_package.reg_num;

	for (i = 0; i < addr_bytes; i++) {
		if (reg_num < attr_buf[1]) {
			temp_buf[0] = buf[attr_buf[0] + i * 5];
			temp_buf[1] = buf[attr_buf[0] + i * 5 + 1];
		} else if (reg_num >= attr_buf[1] && reg_num < attr_buf[3]) {
			temp_buf[0] = buf[attr_buf[2] + i * 5];
			temp_buf[1] = buf[attr_buf[2] + i * 5 + 1];
		} else if (reg_num >= attr_buf[3] && reg_num < attr_buf[5]) {
			temp_buf[0] = buf[attr_buf[4] + i * 5];
			temp_buf[1] = buf[attr_buf[4] + i * 5 + 1];
		}
		if (sscanf(temp_buf, "%02x", &addrbuf[i]) == 1)
			aw9610x->aw_i2c_package.init_addr[i] =
							(uint8_t)addrbuf[i];
	}
}

/******************************************************
 *
 *the document of storage_spedata
 *
 ******************************************************/
static int32_t aw9610x_filedata_deal(struct aw9610x *aw9610x)
{
	struct file *fp = NULL;
	mm_segment_t fs;
	int8_t *buf;
	int8_t temp_buf[8] = { 0 };
	uint8_t i = 0;
	uint8_t j = 0;
	int32_t ret;
	uint32_t nv_flag = 0;
	uint8_t cali_file_name[20] = { 0 };

	snprintf(cali_file_name, sizeof(cali_file_name), "aw_cali_%d.bin", aw9610x->sar_num);
	AWLOGI(aw9610x->dev, "cali_file_name : %s", cali_file_name);

	fp = filp_open(cali_file_name, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		AWLOGE(aw9610x->dev, "open failed!");
		return -EINVAL;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	buf = (char *)kzalloc(CALI_FILE_MAX_SIZE, GFP_KERNEL);
	if (!buf) {
		AWLOGE(aw9610x->dev, "malloc failed!");
		filp_close(fp, NULL);
		set_fs(fs);
		return -EINVAL;
	}

	ret = vfs_read(fp, buf, CALI_FILE_MAX_SIZE, &(fp->f_pos));
	if (ret < 0) {
		AWLOGE(aw9610x->dev, "read failed");
		set_fs(fs);
		aw9610x->cali_flag = AW_CALI;
		return ret;
	} else if (ret == 0) {
		AWLOGE(aw9610x->dev, "read len = 0");
		set_fs(fs);
		aw9610x->cali_flag = AW_CALI;
		return ret;
	} else {
		for (i = 0; i < AW_SPE_REG_NUM; i++) {
			for (j = 0; j < AW_SPE_REG_DWORD; j++)
				temp_buf[j] = buf[AW_SPE_REG_DWORD * i + j];

			if (sscanf(temp_buf, "%08x",
					&aw9610x->nvspe_data[i]) == 1)
				AWLOGD(aw9610x->dev,
						"nv_spe_data[%d] = 0x%08x",
						i, aw9610x->nvspe_data[i]);
			}
	}

	set_fs(fs);
	filp_close(fp, NULL);
	kfree(buf);

	/* nvspe_datas come from nv*/
	for (i = 0; i < AW_SPE_REG_NUM; i++) {
		nv_flag |= aw9610x->nvspe_data[i];
		if (nv_flag != 0)
			break;
	}

	if (nv_flag == 0) {
		aw9610x->cali_flag = AW_CALI;
		AWLOGI(aw9610x->dev,
			"the chip need to cali! nv_flag = 0x%08x", nv_flag);
	} else {
		aw9610x->cali_flag = AW_NO_CALI;
		AWLOGI(aw9610x->dev,
			"chip not need to cali! nv_flag = 0x%08x", nv_flag);
	}

	return 0;
}

static int32_t
aw9610x_store_spedata_to_file(struct aw9610x *aw9610x, char *buf)
{
	struct file *fp = NULL;
	loff_t pos = 0;
	mm_segment_t fs;
	uint8_t cali_file_name[20] = { 0 };

	AWLOGD(aw9610x->dev, "buf = %s", buf);

	snprintf(cali_file_name, 20, "aw_cali_%d.bin", aw9610x->sar_num);
	AWLOGI(aw9610x->dev, "cali_file_name : %s", cali_file_name);

	fp = filp_open(cali_file_name, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		AWLOGE(aw9610x->dev, "open failed!");
		return -EINVAL;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);

	vfs_write(fp, buf, strlen(buf), &pos);

	set_fs(fs);

	AWLOGI(aw9610x->dev, "write successfully!");

	filp_close(fp, NULL);
	return 0;
}

/******************************************************
 *
 *configuration of special reg
 *
 ******************************************************/
static void aw9610x_get_calidata(struct aw9610x *aw9610x)
{
	uint8_t i = 0;
	uint32_t buf_size = 0;
	int32_t ret;
	uint32_t reg_val = 0;
	uint8_t temp_buf[9] = { 0 };
	uint8_t buf[CALI_FILE_MAX_SIZE] = { 0 };

	AWLOGD(aw9610x->dev, "enter");

	/*class 1 special reg*/
	for (i = 0; i < AW_CLA1_SPE_REG_NUM; i++)
		aw9610x_i2c_read(aw9610x,
		REG_AFECFG1_CH0 + i * AW_CL1SPE_CALI_OS, &aw9610x->spedata[i]);

	/*class 2 special reg*/
	for (; i < AW_SPE_REG_NUM; i++)
		aw9610x_i2c_read(aw9610x,
			REG_REFACFG + (i - AW_CHANNEL_MAX) *
				AW_CL2SPE_CALI_OS, &aw9610x->spedata[i]);

	for (i = AW_CLA1_SPE_REG_NUM; i < AW_SPE_REG_NUM; i++) {
		ret = aw9610x->spedata[i] & 0x07;
		switch (ret) {
		case AW_CHANNEL0:
			aw9610x_i2c_read(aw9610x, REG_VALID_CH0,
							&reg_val);
			break;
		case AW_CHANNEL1:
			aw9610x_i2c_read(aw9610x, REG_VALID_CH1,
							&reg_val);
			break;
		case AW_CHANNEL2:
			aw9610x_i2c_read(aw9610x, REG_VALID_CH2,
							&reg_val);
			break;
		case AW_CHANNEL3:
			aw9610x_i2c_read(aw9610x, REG_VALID_CH3,
							&reg_val);
			break;
		case AW_CHANNEL4:
			aw9610x_i2c_read(aw9610x, REG_VALID_CH4,
							&reg_val);
			break;

		default:
			return;
		}
		aw9610x->spedata[i] = ((reg_val >> 6) & 0x03fffff0) |
					(aw9610x->spedata[i] & 0xfc00000f);
	}
	/* spedatas come from register*/

	/* write spedatas to nv */
	for (i = 0; i < AW_SPE_REG_NUM; i++) {
		snprintf(temp_buf, sizeof(temp_buf), "%08x",
							aw9610x->spedata[i]);
		memcpy(buf + buf_size, temp_buf, strlen(temp_buf));
		buf_size = strlen(buf);
	}
	ret = aw9610x_store_spedata_to_file(aw9610x, buf);
	if (ret < 0) {
		AWLOGE(aw9610x->dev, "store spedata failed");
		return;
	}

	AWLOGD(aw9610x->dev, "successfully write_spereg_to_file");
}

static void aw9610x_class1_reg(struct aw9610x *aw9610x)
{
	int32_t i = 0;
	uint32_t reg_val;

	AWLOGD(aw9610x->dev, "enter");

	for (i = 0; i < AW_CLA1_SPE_REG_NUM; i++) {
		reg_val = (aw9610x->nvspe_data[i] >> 16) & 0x0000ffff;
		aw9610x_i2c_write_bits(aw9610x, REG_INITPROX0_CH0 +
				i * AW_CL1SPE_DEAL_OS, ~(0xffff), reg_val);
	}
}

static void aw9610x_class2_reg(struct aw9610x *aw9610x)
{
	int32_t i = 0;

	AWLOGD(aw9610x->dev, "enter");

	for (i = AW_CLA1_SPE_REG_NUM; i < AW_SPE_REG_NUM; i++) {
		aw9610x_i2c_write(aw9610x,
			REG_REFACFG + (i - AW_CLA1_SPE_REG_NUM) * AW_CL2SPE_DEAL_OS,
			aw9610x->nvspe_data[i]);
	}
}

static void aw9610x_spereg_deal(struct aw9610x *aw9610x)
{
	AWLOGD(aw9610x->dev, "enter!");

	aw9610x_class1_reg(aw9610x);
	aw9610x_class2_reg(aw9610x);
}

static void aw9610x_datablock_load(struct device *dev, const char *buf)
{
	uint32_t i = 0;
	uint8_t reg_data[220] = { 0 };
	uint32_t databuf[220] = { 0 };
	uint8_t temp_buf[2] = { 0 };
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	uint8_t addr_bytes = aw9610x->aw_i2c_package.addr_bytes;
	uint8_t data_bytes = aw9610x->aw_i2c_package.data_bytes;
	uint8_t reg_num = aw9610x->aw_i2c_package.reg_num;

	for (i = 0; i < data_bytes * reg_num; i++) {
		if (reg_num < attr_buf[1]) {
			temp_buf[0] = buf[attr_buf[0] + (addr_bytes + i) * 5];
			temp_buf[1] =
				buf[attr_buf[0] + (addr_bytes + i) * 5 + 1];
		} else if (reg_num >= attr_buf[1] && reg_num < attr_buf[3]) {
			temp_buf[0] = buf[attr_buf[2] + (addr_bytes + i) * 5];
			temp_buf[1] =
				buf[attr_buf[2] + (addr_bytes + i) * 5 + 1];
		} else if (reg_num >= attr_buf[3] && reg_num < attr_buf[5]) {
			temp_buf[0] = buf[attr_buf[4] + (addr_bytes + i) * 5];
			temp_buf[1] =
				buf[attr_buf[4] + (addr_bytes + i) * 5 + 1];
		}
		sscanf(temp_buf, "%02x", &databuf[i]);
		reg_data[i] = (uint8_t)databuf[i];
	}
	aw9610x->aw_i2c_package.p_reg_data = reg_data;
	i2c_write_seq(aw9610x);
}
#ifdef ANFR_TEST
static uint32_t read_ch0_offset(void)
{
	uint32_t coff_data = 0;
	aw9610x_t this = aw9610x_ptr;
	uint32_t reg_val = 0;
	aw9610x_i2c_read(this, REG_AFECFG1_CH0, &reg_val);
	coff_data = (reg_val >> 24) * 900 +
					((reg_val >> 16) & 0xff) * 13;
	return coff_data/1000;
}
static uint32_t read_ch2_offset(void)
{
	uint32_t coff_data = 0;
	aw9610x_t this = aw9610x_ptr;
	uint32_t reg_val = 0;
	aw9610x_i2c_read(this, REG_AFECFG1_CH0 + 40, &reg_val);
	coff_data = (reg_val >> 24) * 900 +
					((reg_val >> 16) & 0xff) * 13;
	return coff_data/1000;
}
static uint32_t read_ch3_offset(void)
{
	uint32_t coff_data = 0;
	aw9610x_t this = aw9610x_ptr;
	uint32_t reg_val = 0;
	aw9610x_i2c_read(this, REG_AFECFG1_CH0 + 60, &reg_val);
	coff_data = (reg_val >> 24) * 900 +
					((reg_val >> 16) & 0xff) * 13;
	return coff_data/1000;
}
#endif
static void aw9610x_power_on_prox_detection(struct aw9610x *aw9610x)
{
	int32_t ret = 0;
	uint32_t reg_data = 0;
	uint32_t temp_time = AW9610X_SCAN_DEFAULT_TIME;

	AWLOGD(aw9610x->dev, "enten");

	ret = aw9610x_filedata_deal(aw9610x);
	if ((aw9610x->cali_flag == AW_NO_CALI) && (ret >= 0))
		aw9610x_spereg_deal(aw9610x);

	aw9610x_i2c_write(aw9610x, REG_HOSTIRQEN, 0);
	aw9610x_i2c_write(aw9610x, REG_CMD, 0x0001);
	while ((temp_time)--) {
		aw9610x_i2c_read(aw9610x, REG_HOSTIRQSRC, &reg_data);
		reg_data = (reg_data >> 4) & 0x01;
		if (reg_data == 1) {
			AWLOGI(aw9610x->dev,
				"time = %d", temp_time);
			if ((aw9610x->cali_flag == AW_CALI) && ret >= 0)
				aw9610x_get_calidata(aw9610x);
			break;
		}
		msleep(1);
	}
	aw9610x_i2c_read(aw9610x, REG_STAT2, &reg_data);
	if (reg_data & 0x10000)
		aw9610x->power_prox = 1;
}

static void aw9610x_channel_scan_start(struct aw9610x *aw9610x)
{
	AWLOGD(aw9610x->dev, "enter");
	if (aw9610x->pwprox_dete == true) {
		 aw9610x_power_on_prox_detection(aw9610x);
	} else {
		aw9610x_i2c_write(aw9610x, REG_CMD, AW9610X_ACTIVE_MODE);
	}

	aw9610x_i2c_write(aw9610x, REG_HOSTIRQEN, aw9610x->hostirqen);
	aw9610x->mode = AW9610X_ACTIVE_MODE;
}

static void aw9610x_bin_valid_loaded(struct aw9610x *aw9610x,
						struct aw_bin *aw_bin_data_s)
{
	uint32_t i;
	int32_t ret = 0;
	uint16_t reg_addr;
	uint32_t reg_data;
	uint32_t start_addr = aw_bin_data_s->header_info[0].valid_data_addr;

	for (i = 0; i < aw_bin_data_s->header_info[0].valid_data_len;
						i += 6, start_addr += 6) {
		reg_addr = (aw_bin_data_s->info.data[start_addr]) |
				aw_bin_data_s->info.data[start_addr + 1] << 8;
		reg_data = aw_bin_data_s->info.data[start_addr + 2] |
			(aw_bin_data_s->info.data[start_addr + 3] << 8) |
			(aw_bin_data_s->info.data[start_addr + 4] << 16) |
			(aw_bin_data_s->info.data[start_addr + 5] << 24);
		if ((reg_addr == REG_EEDA0) || (reg_addr == REG_EEDA1))
			continue;
		if (reg_addr == REG_HOSTIRQEN) {
			aw9610x->hostirqen = reg_data;
			continue;
		}
		ret = aw9610x_i2c_write(aw9610x, reg_addr, reg_data);
		if (ret < 0)
			return ;

		AWLOGI(aw9610x->dev,
			"reg_addr = 0x%04x, reg_data = 0x%08x",
					reg_addr, reg_data);
	}
	AWLOGI(aw9610x->dev, "bin writen completely");

	aw9610x_channel_scan_start(aw9610x);
}

/***************************************************************************
* para loaded
****************************************************************************/
static int32_t aw9610x_para_loaded(struct aw9610x *aw9610x)
{
	int32_t i = 0;
	int32_t len = ARRAY_SIZE(aw9610x_reg_default);

	AWLOGD(aw9610x->dev, "start to download para!");

	for (i = 0; i < len; i = i + 2) {
		aw9610x_i2c_write(aw9610x,
				(uint16_t)aw9610x_reg_default[i],
				aw9610x_reg_default[i+1]);
		if (aw9610x_reg_default[i] == REG_HOSTIRQEN)
			aw9610x->hostirqen = aw9610x_reg_default[i+1];
		AWLOGI(aw9610x->dev, "reg_addr = 0x%04x, reg_data = 0x%08x",
						aw9610x_reg_default[i],
						aw9610x_reg_default[i+1]);
	}
	AWLOGI(aw9610x->dev, "para writen completely");

	aw9610x_channel_scan_start(aw9610x);

	return 0;
}

static void
aw9610x_cfg_all_loaded(const struct firmware *cont, void *context)
{
	int32_t ret;
	struct aw_bin *aw_bin;
	struct aw9610x *aw9610x = context;

	AWLOGD(aw9610x->dev, "enter");

	if (!cont) {
		AWLOGE(aw9610x->dev, "%s request failed", aw9610x->cfg_name);
		release_firmware(cont);
		return;
	} else {
		AWLOGI(aw9610x->dev,
			"%s request successfully", aw9610x->cfg_name);
	}

	aw_bin = kzalloc(cont->size + sizeof(struct aw_bin), GFP_KERNEL);
	if (!aw_bin) {
		kfree(aw_bin);
		release_firmware(cont);
		AWLOGE(aw9610x->dev, "failed to allcating memory!");
		return;
	}
	aw_bin->info.len = cont->size;
	memcpy(aw_bin->info.data, cont->data, cont->size);
	ret = aw_parsing_bin_file(aw_bin);
	if (ret < 0) {
		AWLOGE(aw9610x->dev, "[:aw9610x parse bin fail! ret = %d", ret);
		kfree(aw_bin);
		release_firmware(cont);
		return;
	}

	ret = strcmp(aw9610x->chip_name, aw_bin->header_info[0].chip_type);
	if (ret != 0) {
		AWLOGE(aw9610x->dev,
			"chip name(%s) incompatible with chip type(%s)",
			aw9610x->chip_name, aw_bin->header_info[0].chip_type);

		kfree(aw_bin);
		release_firmware(cont);
		return;
	}

	aw9610x_bin_valid_loaded(aw9610x, aw_bin);
	kfree(aw_bin);
	release_firmware(cont);
}

static int32_t aw9610x_cfg_update(struct aw9610x *aw9610x)
{
	AWLOGD(aw9610x->dev, "enter");

	if (aw9610x->firmware_flag == true) {
		snprintf(aw9610x->cfg_name, sizeof(aw9610x->cfg_name),
					"aw9610x_%d.bin", aw9610x->sar_num);

		request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
							aw9610x->cfg_name,
							aw9610x->dev,
							GFP_KERNEL,
							aw9610x,
							aw9610x_cfg_all_loaded);
	} else {
		aw9610x_para_loaded(aw9610x);
	}

	return AW_SAR_SUCCESS;
}

static void aw9610x_cfg_work_routine(struct work_struct *work)
{
	struct aw9610x
		*aw9610x = container_of(work, struct aw9610x, cfg_work.work);

	AWLOGD(aw9610x->dev, "enter");

	aw9610x_cfg_update(aw9610x);
}

static int32_t
aw9610x_sar_cfg_init(struct aw9610x *aw9610x, int32_t flag)
{
	uint32_t cfg_timer_val = 0;
	uint32_t i = 0;

	AWLOGD(aw9610x->dev, "enter");

	if (flag == AW_CFG_LOADED)
		cfg_timer_val = 20;
	else if (flag == AW_CFG_UNLOAD)
		cfg_timer_val = 5000;
	else
		return -AW_CFG_LOAD_TIME_FAILED;

	INIT_DELAYED_WORK(&aw9610x->cfg_work, aw9610x_cfg_work_routine);
	schedule_delayed_work(&aw9610x->cfg_work,
					msecs_to_jiffies(cfg_timer_val));

	for (i = 0; i < AW_CHANNEL_MAX; i++) {
		aw9610x->aw_pad[i].curr_state = 0;
		aw9610x->aw_pad[i].last_state = 0;
	}

	return AW_SAR_SUCCESS;
}

/*****************************************************
 *
 * first irq clear
 *
 *****************************************************/
static int32_t aw9610x_init_irq_handle(struct aw9610x *aw9610x)
{
	uint8_t cnt = 20;
	uint32_t reg_data;
	uint32_t trim0 = 0;
	uint32_t trim1 = 0;

	AWLOGD(aw9610x->dev, "enter");

	while (cnt--) {
		aw9610x_i2c_read(aw9610x, REG_HOSTIRQSRC, &reg_data);
		aw9610x->first_irq_flag = reg_data & 0x01;
		if (aw9610x->first_irq_flag == 1) {
			aw9610x_i2c_read(aw9610x, REG_EEDA0, &trim0);
			aw9610x_i2c_read(aw9610x, REG_EEDA1, &trim1);
			if ((trim0 + trim1) == 0) {
				AWLOGE(aw9610x->dev, "aw9610x trim error");
				return -AW_TRIM_ERROR;
			}
			AWLOGD(aw9610x->dev, "cnt = %d", cnt);
			return AW_SAR_SUCCESS;
		}
	}
	AWLOGE(aw9610x->dev, "hardware has trouble!");

	return -AW_IRQIO_FAILED;
}

/*****************************************************
 *
 * software reset
 *
 *****************************************************/
static void aw9610x_sw_reset(struct aw9610x *aw9610x)
{
	AWLOGD(aw9610x->dev, "enter");

	aw9610x_i2c_write(aw9610x, REG_HOSTCTRL2, 0);
	msleep(20);
}

static int32_t aw9610x_baseline_filter(struct aw9610x *aw9610x)
{
	int32_t ret = 0;
	uint8_t i = 0;
	uint32_t status0 = 0;
	uint32_t status1 = 0;

	ret = aw9610x_i2c_read(aw9610x, REG_STAT1, &status1);
	if (ret < 0)
		return ret;
	ret = aw9610x_i2c_read(aw9610x, REG_STAT0, &status0);
	if (ret < 0)
		return ret;

	for (i = 0; i < AW_CHANNEL_MAX; i++) {
		if (((status1 >> i) & 0x01) == 1) {
			if (aw9610x->satu_flag[i] == 0) {
				ret = aw9610x_i2c_read(aw9610x,
					REG_BLFILT1_CH0 + i * AW_CL1SPE_DEAL_OS,
					&aw9610x->satu_data[i]);
				if (ret < 0)
					return ret;
				ret = aw9610x_i2c_write(aw9610x,
				REG_BLFILT1_CH0 + i * AW_CL1SPE_DEAL_OS,
				((aw9610x->satu_data[i] | 0x1fc) & 0x3fffffff));
				if (ret < 0)
					return ret;
				aw9610x->satu_flag[i] = 1;
			}
		} else if (((status1 >> i) & 0x01) == 0) {
			if (aw9610x->satu_flag[i] == 1) {
				if (((status0 >> (i + 24)) & 0x01) == 0) {
					ret = aw9610x_i2c_write(aw9610x,
					REG_BLFILT1_CH0 + i * AW_CL1SPE_DEAL_OS,
					aw9610x->satu_data[i]);
					if (ret < 0)
						return ret;
					aw9610x->satu_flag[i] = 0;
				}
			}
		}
	}

	return ret;
}

static void aw9610x_saturat_release_handle(struct aw9610x *aw9610x)
{
	uint32_t satu_irq = 0;
	uint8_t i = 0;
	int32_t ret = 0;
	uint32_t status0 = 0;

	AWLOGD(aw9610x->dev, "enter");

	satu_irq = (aw9610x->irq_status >> 7) & 0x01;
	if (satu_irq == 1) {
		ret = aw9610x_baseline_filter(aw9610x);
		if (ret < 0)
			return;
	} else {
		ret = aw9610x_i2c_read(aw9610x, REG_STAT0, &status0);
		if (ret < 0)
			return;
		for (i = 0; i < AW_CHANNEL_MAX; i++) {
			if (aw9610x->satu_flag[i] == 1) {
				if (((status0 >> (i + 24)) & 0x01) == 0) {
					ret = aw9610x_i2c_write(aw9610x,
					REG_BLFILT1_CH0 + i * AW_CL1SPE_DEAL_OS,
					aw9610x->satu_data[i]);
					if (ret < 0)
						return;
					aw9610x->satu_flag[i] = 0;
				}
			}
		}
	}

	AWLOGI(aw9610x->dev, "satu_irq handle over!");
}

/******************************************************
 *
 * sys group attribute
 *
 ******************************************************/
static ssize_t aw9610x_set_reg(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	uint32_t databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2)
		aw9610x_i2c_write(aw9610x, (uint16_t)databuf[0],
							(uint32_t)databuf[1]);

	return count;
}

static ssize_t aw9610x_get_reg(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	ssize_t len = 0;
	uint32_t i = 0;
	uint32_t reg_val = 0;
	uint32_t reg_num = 0;

	reg_num = ARRAY_SIZE(aw9610x_reg_access);
	for (i = 0; i < reg_num; i++) {
		if (aw9610x_reg_access[i].rw & REG_RD_ACCESS) {
			aw9610x_i2c_read(aw9610x, aw9610x_reg_access[i].reg,
								&reg_val);
			len += snprintf(buf + len, PAGE_SIZE - len,
						"reg:0x%04x=0x%08x\n",
						aw9610x_reg_access[i].reg,
						reg_val);
		}
	}

	return len;
}

static ssize_t aw9610x_valid_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	ssize_t len = 0;
	uint8_t i = 0;
	int32_t reg_val = 0;

	for (i = 0; i < AW_CHANNEL_MAX; i++) {
		aw9610x_i2c_read(aw9610x, REG_VALID_CH0 + i * 4, &reg_val);
		reg_val /= AW_DATA_PROCESS_FACTOR;
		len += snprintf(buf+len, PAGE_SIZE-len, "VALID_CH%d = %d\n", i,
								reg_val);
	}

	return len;
}

static ssize_t aw9610x_baseline_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	ssize_t len = 0;
	uint8_t i = 0;
	int32_t reg_val = 0;

	for (i = 0; i < AW_CHANNEL_MAX; i++) {
		aw9610x_i2c_read(aw9610x, REG_BASELINE_CH0 + i * 4, &reg_val);
		reg_val /= AW_DATA_PROCESS_FACTOR;
		len += snprintf(buf+len, PAGE_SIZE-len, "BASELINE_CH%d = %d\n",
								i, reg_val);
	}

	return len;
}

static ssize_t aw9610x_diff_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	ssize_t len = 0;
	uint8_t i = 0;
	int32_t reg_val = 0;

	for (i = 0; i < AW_CHANNEL_MAX; i++) {
		aw9610x_i2c_read(aw9610x, REG_DIFF_CH0 + i * 4, &reg_val);
		reg_val /= AW_DATA_PROCESS_FACTOR;
		len += snprintf(buf+len, PAGE_SIZE-len, "DIFF_CH%d = %d\n", i,
								reg_val);
	}

	return len;
}

static ssize_t aw9610x_raw_data_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	ssize_t len = 0;
	uint8_t i = 0;
	int32_t reg_val = 0;

	for (i = 0; i < AW_CHANNEL_MAX; i++) {
		aw9610x_i2c_read(aw9610x, REG_RAW_CH0 + i * 4, &reg_val);
		reg_val /= AW_DATA_PROCESS_FACTOR;
		len += snprintf(buf+len, PAGE_SIZE-len, "RAW_DATA_CH%d = %d\n",
								i, reg_val);
	}

	return len;
}

static ssize_t aw9610x_psc_data_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	ssize_t len = 0;
	uint8_t i = 0;
	int32_t reg_val = 0;

	for (i = 0; i < AW_CHANNEL_MAX; i++) {
		aw9610x_i2c_read(aw9610x, REG_PSCBD_CH0 + i * 4, &reg_val);
		reg_val /= AW_DATA_PROCESS_FACTOR;
		len += snprintf(buf+len, PAGE_SIZE-len, "PSC_DATA_CH%d = %d\n",
								i, reg_val);
	}

	return len;
}

static ssize_t aw9610x_parasitic_data_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	ssize_t len = 0;
	uint8_t i = 0;
	uint32_t reg_val = 0;
	uint32_t coff_data = 0;
	uint32_t coff_data_int = 0;
	uint32_t coff_data_dec = 0;
	uint8_t temp_data[20] = { 0 };

	for (i = 0; i < AW_CHANNEL_MAX; i++) {
		aw9610x_i2c_read(aw9610x,
			REG_AFECFG1_CH0 + i * AW_CL1SPE_CALI_OS, &reg_val);
		coff_data = (reg_val >> 24) * 900 +
						((reg_val >> 16) & 0xff) * 13;
		coff_data_int = coff_data / 1000;
		coff_data_dec = coff_data % 1000;
		snprintf(temp_data, sizeof(temp_data), "%d.%d", coff_data_int,
								coff_data_dec);
		len += snprintf(buf+len, PAGE_SIZE-len,
				"PARASITIC_DATA_CH%d = %s pf\n", i, temp_data);
	}

	return len;
}

static ssize_t aw9610x_awrw_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	uint8_t reg_data[228] = { 0 };
	uint8_t i = 0;
	ssize_t len = 0;
	uint8_t reg_num = aw9610x->aw_i2c_package.reg_num;
	uint8_t data_bytes = aw9610x->aw_i2c_package.data_bytes;

	i2c_read_seq(aw9610x, reg_data);
	for (i = 0; i < reg_num * data_bytes; i++)
		len += snprintf(buf + len, PAGE_SIZE - len,
						"0x%02x,", reg_data[i]);

	len += snprintf(buf + len - 1, PAGE_SIZE - len, "\n");

	return len;
}

static ssize_t aw9610x_factory_cali_set(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	uint32_t databuf[1] = { 0 };

	if (sscanf(buf, "%d", &databuf[0]) == 1) {
		if ((databuf[0] == 1) && (aw9610x->pwprox_dete == true)) {
			aw9610x_get_calidata(aw9610x);
		} else {
			AWLOGE(aw9610x->dev, "aw_unsupport the pw_prox_dete=%d",
						aw9610x->pwprox_dete);
			return count;
		}
		aw9610x_sw_reset(aw9610x);
		aw9610x->cali_flag = AW_NO_CALI;
		aw9610x_sar_cfg_init(aw9610x, AW_CFG_LOADED);
	}

	return count;
}

static ssize_t aw9610x_power_prox_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	ssize_t len = 0;

	if (aw9610x->pwprox_dete == false) {
		len += snprintf(buf + len, PAGE_SIZE - len,
							"unsupport powerprox!");
		return len;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "power_prox: ");
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
							aw9610x->power_prox);

	return len;
}

static ssize_t aw9610x_awrw_set(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	uint32_t datatype[3] = { 0 };

	if (sscanf(buf, "%d %d %d", &datatype[0], &datatype[1],
							&datatype[2]) == 3) {
		aw9610x->aw_i2c_package.addr_bytes = (uint8_t)datatype[0];
		aw9610x->aw_i2c_package.data_bytes = (uint8_t)datatype[1];
		aw9610x->aw_i2c_package.reg_num = (uint8_t)datatype[2];

		aw9610x_addrblock_load(dev, buf);
		if (count > 7 + 5 * aw9610x->aw_i2c_package.addr_bytes)
			aw9610x_datablock_load(dev, buf);
	}

	return count;
}

static ssize_t aw9610x_set_update(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	ssize_t ret;
	uint32_t state;
	int32_t cfg_timer_val = 10;
	struct aw9610x *aw9610x = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 10, &state);
	if (ret) {
		AWLOGE(aw9610x->dev, "fail to set update");
		return ret;
	}
	if (state) {
		aw9610x_i2c_write(aw9610x, REG_HOSTIRQEN, 0);
		aw9610x_sw_reset(aw9610x);
		schedule_delayed_work(&aw9610x->cfg_work,
					msecs_to_jiffies(cfg_timer_val));
	}

	return count;
}

static ssize_t aw9610x_aot_cali_set(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	ssize_t ret;
	uint32_t state;
	uint32_t data_en = 0;
	struct aw9610x *aw9610x = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 10, &state);
	if (ret) {
		AWLOGE(aw9610x->dev, "fail to set aot cali");
		return ret;
	}
	aw9610x_i2c_read(aw9610x, REG_SCANCTRL0, &data_en);

	if (state != 0)
		aw9610x_i2c_write_bits(aw9610x, REG_SCANCTRL0, ~(0x3f << 8),
							(data_en & 0x3f) << 8);
	else
		AWLOGE(aw9610x->dev, "fail to set aot cali");

	return count;
}

static ssize_t aw9610x_get_satu(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	ssize_t len = 0;

	if (aw9610x->satu_release != 0)
		len += snprintf(buf + len, PAGE_SIZE - len,
			"satu_ralease function is supporting! the flag = %d\n",
							aw9610x->satu_release);
	else
		len += snprintf(buf + len, PAGE_SIZE - len,
			"satu_ralease function unsupport! the flag = %d\n",
							aw9610x->satu_release);

	return len;
}

static ssize_t aw9610x_set_satu(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	ssize_t ret;
	uint32_t state;
	struct aw9610x *aw9610x = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 10, &state);
	if (ret) {
		AWLOGE(aw9610x->dev, "fail to set satu");
		return ret;
	}
	if (state && (aw9610x->vers == AW9610X)) {
		aw9610x_saturat_release_handle(aw9610x);
		aw9610x->satu_release = AW9610X_FUNC_ON;
	} else {
		aw9610x->satu_release = AW9610X_FUNC_OFF;
	}

	return count;
}

static ssize_t aw9610x_operation_mode_set(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	ssize_t ret;
	struct aw9610x *aw9610x = dev_get_drvdata(dev);

	ret = kstrtouint(buf, 10, &aw9610x->mode);
	if (ret) {
		AWLOGE(aw9610x->dev, "fail to set operation mode");
		return ret;
	}

	if (aw9610x->mode == AW9610X_ACTIVE_MODE) {
		if (aw9610x->mode_flag1 == AW9610X_FUNC_ON)
			aw9610x_i2c_write(aw9610x, REG_HOSTCTRL1, 1);
		aw9610x_i2c_write(aw9610x, REG_CMD, AW9610X_ACTIVE_MODE);
		if (aw9610x->mode_flag0 == AW9610X_FUNC_ON)
			enable_irq(aw9610x->to_irq);
		aw9610x->mode_flag0 = AW9610X_FUNC_OFF;
	} else if (aw9610x->mode == AW9610X_SLEEP_MODE) {
		disable_irq(aw9610x->to_irq);
		if (aw9610x->mode_flag1 == AW9610X_FUNC_ON)
			aw9610x_i2c_write(aw9610x, REG_HOSTCTRL1, 1);
		aw9610x_i2c_write(aw9610x, REG_CMD, AW9610X_SLEEP_MODE);
		aw9610x->mode_flag0 = AW9610X_FUNC_ON;
	} else if ((aw9610x->mode == AW9610X_DEEPSLEEP_MODE) &&
					(aw9610x->vers == AW9610XA)) {
		aw9610x_i2c_write(aw9610x, REG_CMD, AW9610X_DEEPSLEEP_MODE);
		aw9610x->mode_flag1 = AW9610X_FUNC_ON;
		aw9610x->mode_flag0 = AW9610X_FUNC_OFF;
	} else {
		AWLOGE(aw9610x->dev, "failed to operation mode!");
		return aw9610x->mode;
	}

	return count;
}

static ssize_t aw9610x_operation_mode_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct aw9610x *aw9610x = dev_get_drvdata(dev);
	ssize_t len = 0;

	if (aw9610x->mode == AW9610X_ACTIVE_MODE)
		len += snprintf(buf + len, PAGE_SIZE - len,
						"operation mode: Active\n");
	else if (aw9610x->mode == AW9610X_SLEEP_MODE)
		len += snprintf(buf + len, PAGE_SIZE - len,
						"operation mode: Sleep\n");
	else if ((aw9610x->mode == AW9610X_DEEPSLEEP_MODE) &&
					(aw9610x->vers == AW9610XA))
		len += snprintf(buf + len, PAGE_SIZE - len,
						"operation mode: DeepSleep\n");
	else
		len += snprintf(buf + len, PAGE_SIZE - len,
					"operation mode: Unconfirmed\n");

	return len;
}

static DEVICE_ATTR(reg, 0664, aw9610x_get_reg, aw9610x_set_reg);
static DEVICE_ATTR(valid, 0664, aw9610x_valid_show, NULL);
static DEVICE_ATTR(baseline, 0664, aw9610x_baseline_show, NULL);
static DEVICE_ATTR(diff, 0664, aw9610x_diff_show, NULL);
static DEVICE_ATTR(raw_data, 0664, aw9610x_raw_data_show, NULL);
static DEVICE_ATTR(psc_data, 0664, aw9610x_psc_data_show, NULL);
static DEVICE_ATTR(parasitic_data, 0664, aw9610x_parasitic_data_show, NULL);
static DEVICE_ATTR(factory_cali, 0664, NULL, aw9610x_factory_cali_set);
static DEVICE_ATTR(aot_cali, 0664, NULL, aw9610x_aot_cali_set);
static DEVICE_ATTR(awrw, 0664, aw9610x_awrw_get, aw9610x_awrw_set);
static DEVICE_ATTR(update, 0644, NULL, aw9610x_set_update);
static DEVICE_ATTR(satu, 0644, aw9610x_get_satu, aw9610x_set_satu);
static DEVICE_ATTR(prox, 0644, aw9610x_power_prox_get, NULL);
static DEVICE_ATTR(operation_mode, 0644, aw9610x_operation_mode_get,
						aw9610x_operation_mode_set);

static struct attribute *aw9610x_sar_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_valid.attr,
	&dev_attr_baseline.attr,
	&dev_attr_diff.attr,
	&dev_attr_raw_data.attr,
	&dev_attr_psc_data.attr,
	&dev_attr_parasitic_data.attr,
	&dev_attr_awrw.attr,
	&dev_attr_factory_cali.attr,
	&dev_attr_aot_cali.attr,
	&dev_attr_update.attr,
	&dev_attr_satu.attr,
	&dev_attr_prox.attr,
	&dev_attr_operation_mode.attr,
	NULL
};

static struct attribute_group aw9610x_sar_attribute_group = {
	.attrs = aw9610x_sar_attributes
};

#if defined(CONFIG_SENSORS)
static int aw9610x_grip_flush(struct sensors_classdev *sensors_cdev,unsigned char val)
{
    aw9610x_t this = aw9610x_ptr;

    input_report_rel(this->aw_pad[0].input, REL_MAX, val);
    input_sync(this->aw_pad[0].input);

    AWLOGI(this->dev, "%s val=%u\n", __func__, val);
    return 0;
}

static int aw9610x_grip_wifi_flush(struct sensors_classdev *sensors_cdev,unsigned char val)
{
    aw9610x_t this = aw9610x_ptr;

    input_report_rel(this->aw_pad[3].input, REL_MAX, val);
    input_sync(this->aw_pad[3].input);

    AWLOGI(this->dev, "%s -%u\n", __func__, val);
    return 0;
}

static int aw9610x_grip_sub_flush(struct sensors_classdev *sensors_cdev,unsigned char val)
{
    aw9610x_t this = aw9610x_ptr;

    input_report_rel(this->aw_pad[2].input, REL_MAX, val);
    input_sync(this->aw_pad[2].input);

    AWLOGI(this->dev, "%s -%u\n", __func__, val);
    return 0;
}
#endif

/*****************************************************
*
* irq init
*
*****************************************************/
static void aw9610x_irq_handle(struct aw9610x *aw9610x)
{
	uint32_t curr_status = 0;
	uint8_t i = 0;
	uint8_t j = 0;
	#ifdef ANFR_TEST
	uint32_t ch_result[3] = {0};
	uint32_t data_en = 0;
	#endif
	AWLOGD(aw9610x->dev, "enter");
	
	
	aw9610x_i2c_read(aw9610x, REG_STAT0, &curr_status);
	j = aw9610x->sar_num;
	AWLOGD(aw9610x->dev, "pad = 0x%08x", curr_status);
	if(!force_report_far)
	{
		for (i = 0; i < AW_CHANNEL_MAX; i++)
		{
			if(aw9610x->channel_func & (1 << i))
			{
				input_report_rel(
				aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input,
							REL_MISC, 2);
				input_sync(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input);
			}
		}
	}
	else
	{
#ifdef ANFR_TEST
		if(aw9610x->aw_first_boot)
		{
			aw9610x_i2c_read(aw9610x, REG_SCANCTRL0, &data_en);
			AWLOGE(aw9610x->dev, "sar_test aw9610x_irq_handle REG_SCANCTRL0 = %02x\n",data_en);
			data_en |= (0x3f << 8);
        	aw9610x_i2c_write_bits(aw9610x, REG_SCANCTRL0, ~(0x3f << 8), data_en);
			aw9610x_i2c_read(aw9610x, REG_SCANCTRL0, &data_en);
			AWLOGE(aw9610x->dev, "sar_test2 aw9610x_irq_handle REG_SCANCTRL0 = %02x\n",data_en);
		}

			ch_result[0] = read_ch0_offset();
			ch_result[1] = read_ch2_offset();
			ch_result[2] = read_ch3_offset();			
	AWLOGE(aw9610x->dev, "ch0_back= %d ,%d, %d, ch_result[0] = %d, %d ,%d \n", aw9610x->back_cap[0],aw9610x->back_cap[1],aw9610x->back_cap[2], ch_result[0],ch_result[1],ch_result[2]);
#endif	
	for (i = 0; i < AW_CHANNEL_MAX; i++) {
        if(aw9610x->channel_func & (1 << i)){
		aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].curr_state =
			(((uint8_t)(curr_status >> (24 + i)) & 0x1)) |
			(((uint8_t)(curr_status >> (16 + i)) & 0x1) << 1) |
			(((uint8_t)(curr_status >> (8 + i)) & 0x1) << 2) |
			(((uint8_t)(curr_status >> (i)) & 0x1) << 3);
		AWLOGI(aw9610x->dev, "curr_state[%d] = 0x%x", j * AW_CHANNEL_MAX + i,
					aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].curr_state);
		#ifdef ANFR_TEST
		if(((aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].curr_state !=aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].last_state) &&(aw9610x->enable_flag & (1 << i))))
			aw9610x->interrupt_count++;
		if(aw9610x->interrupt_count > MAX_INT_COUNT)
			aw9610x->interrupt_count = MAX_INT_COUNT + 1;		
		AWLOGE(aw9610x->dev, "aw9610x->interrupt_count = %d \n", aw9610x->interrupt_count);		
		if ((aw9610x->aw_first_boot&&aw9610x->interrupt_count <= MAX_INT_COUNT)  && 
		(ch_result[0] >= aw9610x->back_cap[0]) && (ch_result[1] >= aw9610x->back_cap[1]) && (ch_result[2] >= aw9610x->back_cap[2]))
		{
			LOG_INFO("force all channel State=Near\n");

			input_report_rel(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input,
										 REL_MISC, 1);
			input_sync(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input);
			aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].last_state = 1;
			continue;
		}
		aw9610x->aw_first_boot = false;
		#endif
		    if ((aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].curr_state !=
					aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].last_state) &&
                    (aw9610x->enable_flag & (1 << i))) {
			switch (aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].curr_state) {
			case FAR:
				input_report_rel(
				aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input,
							REL_MISC, 2);
				break;
			case TRIGGER_TH0:
				input_report_rel(
					aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input,
							REL_MISC, 1);
				break;
			case TRIGGER_TH1:
				input_report_abs(
					aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input,
							ABS_DISTANCE, 2);
				break;
			case TRIGGER_TH2:
				input_report_abs(
					aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input,
							ABS_DISTANCE, 3);
				break;
			case TRIGGER_TH3:
				input_report_abs(
					aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input,
							ABS_DISTANCE, 4);
				break;
			default:
				AWLOGE(aw9610x->dev, "error abs distance");
				return;
			}
			input_sync(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input);
		}
		aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].last_state =
				aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].curr_state;
        }
		}
	}
}

static void aw9610x_farirq_handle(struct aw9610x *aw9610x)
{
	uint8_t th0_far = 0;

	th0_far = (aw9610x->irq_status >> 2) & 0x1;
	if (th0_far == 1)
		aw9610x->power_prox = AW9610X_FUNC_OFF;
}

static void aw9610x_interrupt_clear(struct aw9610x *aw9610x)
{
	int32_t ret = 0;

	AWLOGD(aw9610x->dev, "enter");

	ret = aw9610x_i2c_read(aw9610x, REG_HOSTIRQSRC, &aw9610x->irq_status);
	if (ret < 0) {
		AWLOGE(aw9610x->dev, "i2c IO error");
		return;
	} else {
		if ((aw9610x->satu_release == AW9610X_FUNC_ON) && (aw9610x->vers == AW9610X))
			aw9610x_saturat_release_handle(aw9610x);

		if (aw9610x->pwprox_dete == true)
			aw9610x_farirq_handle(aw9610x);
	}

	AWLOGI(aw9610x->dev, "IRQSRC = 0x%x", aw9610x->irq_status);

	/* multiple sar handle IO */
	switch (aw9610x->sar_num) {
	case AW_SAR0:
		break;
	case AW_SAR1:
		break;
	default:
		return;
	}

	aw9610x_irq_handle(aw9610x);
}

static irqreturn_t aw9610x_irq(int32_t irq, void *data)
{
	struct aw9610x *aw9610x = data;

	AWLOGD(aw9610x->dev, "enter");

	aw9610x_interrupt_clear(aw9610x);
	AWLOGD(aw9610x->dev, "exit");

	return IRQ_HANDLED;
}

static int32_t aw9610x_interrupt_init(struct aw9610x *aw9610x)
{
	int32_t irq_flags = 0;
	int32_t ret = 0;
	uint8_t i = 0;
	int8_t irq_gpio_name[100] = { 0 };

	AWLOGD(aw9610x->dev, "enter");

	for (i = 0; i < AW_CHANNEL_MAX; i++)
		aw9610x->satu_flag[i] = 0;

	snprintf(irq_gpio_name, sizeof(irq_gpio_name),
					"aw9610x_irq_gpio%d", aw9610x->sar_num);

	if (gpio_is_valid(aw9610x->irq_gpio)) {
		aw9610x->to_irq = gpio_to_irq(aw9610x->irq_gpio);

		ret = devm_gpio_request_one(aw9610x->dev,
					aw9610x->irq_gpio,
					GPIOF_DIR_IN | GPIOF_INIT_HIGH,
					irq_gpio_name);

		if (ret) {
			AWLOGE(aw9610x->dev,
				"request irq gpio failed, ret = %d", ret);
			ret = -AW_IRQIO_FAILED;
		} else {
			/* register irq handler */
			irq_flags = IRQF_TRIGGER_LOW | IRQF_ONESHOT;
			ret = devm_request_threaded_irq(&aw9610x->i2c->dev,
							aw9610x->to_irq, NULL,
							aw9610x_irq, irq_flags,
							"aw9610x_irq", aw9610x);
			if (ret != 0) {
				AWLOGE(aw9610x->dev,
						"failed to request IRQ %d: %d",
						aw9610x->to_irq, ret);
				ret = -AW_IRQ_REQUEST_FAILED;
			} else {
				AWLOGI(aw9610x->dev,
					"IRQ request successfully!");
				ret = AW_SAR_SUCCESS;
			}
		}
	} else {
		AWLOGE(aw9610x->dev, "irq gpio invalid!");
		return -AW_IRQIO_FAILED;
	}

	return ret;
}

/*****************************************************
 *
 * parse dts
 *
 *****************************************************/
static int32_t aw9610x_parse_dt(struct device *dev, struct aw9610x *aw9610x,
			   struct device_node *np)
{
	uint32_t val = 0;

	val = of_property_read_u32(np, "sar-num", &aw9610x->sar_num);
	if (val != 0) {
		AWLOGE(aw9610x->dev, "multiple sar failed!");
		return -AW_MULTIPLE_SAR_FAILED;
	} else {
		AWLOGI(aw9610x->dev, "sar num = %d", aw9610x->sar_num);
	}

	aw9610x->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (aw9610x->irq_gpio < 0) {
		aw9610x->irq_gpio = -1;
		AWLOGE(aw9610x->dev, "no irq gpio provided.");
		return -AW_IRQGPIO_FAILED;
	} else {
		AWLOGI(aw9610x->dev, "irq gpio provided ok.");
	}

	aw9610x->firmware_flag =
			of_property_read_bool(np, "aw9610x,using-firmware");
	AWLOGI(aw9610x->dev, "firmware_flag = <%d>", aw9610x->firmware_flag);

	aw9610x->pwprox_dete =
		of_property_read_bool(np, "aw9610x,using-pwon-prox-dete");
	AWLOGI(aw9610x->dev, "pwprox_dete = <%d>", aw9610x->pwprox_dete);

	aw9610x->satu_release =
		of_property_read_bool(np, "aw9610x,using-satu");
	AWLOGI(aw9610x->dev, "satu_release = <%d>", aw9610x->satu_release);

	/* +++for sensor channel indicate,add libo7.wt */
	of_property_read_u32(np, "grip-sensor", &aw9610x->grip_sensor_ch);
	of_property_read_u32(np, "grip-sensor-sub", &aw9610x->grip_sensor_sub_ch);
	of_property_read_u32(np, "grip-sensor-wifi", &aw9610x->grip_sensor_wifi_ch);
	
    val = of_property_read_u32(np, "channel-func", &aw9610x->channel_func);
    if (val != 0) {
        AWLOGE(aw9610x->dev, "channel func failed!");
        return -AW_CHANNEL_FUNC_ERR;
    } else {
        AWLOGI(aw9610x->dev, "channel func = %d", aw9610x->channel_func);
    }
	/* ---for sensor channel indicate,add libo7.wt */
	return AW_SAR_SUCCESS;
}

#ifdef AW_POWER_ON
static int32_t aw9610x_power_init(struct aw9610x *aw9610x)
{
	int32_t rc;

	AWLOGD(aw9610x->dev, "aw9610x power init enter");

	aw9610x->vcc = regulator_get(aw9610x->dev, "vcc");
	if (IS_ERR(aw9610x->vcc)) {
		rc = PTR_ERR(aw9610x->vcc);
		AWLOGE(aw9610x->dev, "regulator get failed vcc rc = %d", rc);
		return rc;
	}

	if (regulator_count_voltages(aw9610x->vcc) > 0) {
		rc = regulator_set_voltage(aw9610x->vcc,
					AW_VCC_MIN_UV, AW_VCC_MAX_UV);
		if (rc) {
			AWLOGE(aw9610x->dev,
				"regulator set vol failed rc = %d", rc);
			goto reg_vcc_put;
		}
	}

	return rc;

reg_vcc_put:
	regulator_put(aw9610x->vcc);
	return rc;
}

static void aw9610x_power_enable(struct aw9610x *aw9610x, bool on)
{
	int32_t rc = 0;

	AWLOGD(aw9610x->dev, "aw9610x power enable enter");

	if (on) {
		rc = regulator_enable(aw9610x->vcc);
		if (rc) {
			AWLOGE(aw9610x->dev,
				"regulator_enable vol failed rc = %d", rc);
		} else {
			aw9610x->power_enable = true;
			msleep(20);
		}
	} else {
		rc = regulator_disable(aw9610x->vcc);
		if (rc)
			AWLOGE(aw9610x->dev,
				"regulator_disable vol failed rc = %d", rc);
		else
			aw9610x->power_enable = false;
	}
}
#endif

/*****************************************************
 *
 * check chip id
 *
 *****************************************************/
static int32_t aw9610x_read_chipid(struct aw9610x *aw9610x)
{
	int32_t ret = -1;
	uint8_t cnt = 0;
	uint32_t reg_val = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		ret = aw9610x_i2c_read(aw9610x, REG_CHIP_ID, &reg_val);
		if (ret < 0) {
			AWLOGE(aw9610x->dev, "read CHIP ID failed: %d", ret);
		} else {
			reg_val = reg_val >> 16;
			break;
		}

		cnt++;
		usleep_range(2000, 3000);
	}

	if (reg_val == AW9610X_CHIP_ID) {
		AWLOGI(aw9610x->dev, "aw9610x detected");
		return AW_SAR_SUCCESS;
	} else {
		AWLOGE(aw9610x->dev,
			"unsupport dev, chipid is (0x%04x)", reg_val);
	}

	return -AW_CHIPID_FAILED;
}

static void aw9610x_i2c_set(struct i2c_client *i2c,
						struct aw9610x *aw9610x)
{
	aw9610x->dev = &i2c->dev;
	aw9610x->i2c = i2c;
	i2c_set_clientdata(i2c, aw9610x);
}

static int32_t aw9610x_version_init(struct aw9610x *aw9610x)
{
	uint32_t firmvers = 0;

	aw9610x_i2c_read(aw9610x, REG_FIRMVERSION, &firmvers);
	aw9610x->pad = firmvers >> 28;
	switch (aw9610x->pad) {
	case 4:
		aw9610x->pad -= 1;
		break;
	case 5:
		break;
	default:
		return -AW_VERS_ERR;
	}
	snprintf(aw9610x->chip_name, sizeof(aw9610x->chip_name),
						"AW9610%d", aw9610x->pad);
	aw9610x->chip_name[7] = '\0';

	aw9610x->vers = (firmvers >> 24) & 0xf;
	if (aw9610x->vers == AW9610XA)
		memcpy(aw9610x->chip_name + strlen(aw9610x->chip_name), "A", 1);

	aw9610x->chip_name[8] = '\0';

	AWLOGI(aw9610x->dev, "the IC is = %s", aw9610x->chip_name);

	return AW_SAR_SUCCESS;
}
/* +++input_dev attrabute enable for ss hal,libo7.wt */
/*********************************************
*FUNCTION
*  show_enable
*DESCRIPTION
*  SS Hal enable node
*PARAMETERS
*  dev  [in] the device
*  attr [in] attribute
*  buf  [in] 1:open 0:close
*RETURN
*  p-buf
*********************************************/
static ssize_t show_enable(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
    aw9610x_t this = aw9610x_ptr;
    int status;
    char *p = buf;
    struct input_dev* temp_input_dev;

    temp_input_dev = container_of(dev,struct input_dev,dev);

    LOG_INFO("%s: dev->name:%s\n", __func__,temp_input_dev->name);

    if(!strncmp(temp_input_dev->name,"grip_sensor",sizeof("grip_sensor")))
    {
        if(this->enable_flag & 0x01)
        {
            status = 1;
        }else{
            status = 0;
        }
    }
    if(!strncmp(temp_input_dev->name,"grip_sensor_wifi",sizeof("grip_sensor_wifi")))
    {
        if(this->enable_flag & 0x02)
        {
            status = 1;
        }else{
            status = 0;
        }
    }

	if(!strncmp(temp_input_dev->name,"grip_sensor_sub",sizeof("grip_sensor_sub")))
	{
		if(this->enable_flag & 0x04)
		{
			status = 1;
		}else{
			status = 0;
		}
	}


    p += snprintf(p, PAGE_SIZE, "%d", status);

	return (p-buf);
}
/*********************************************
*FUNCTION
*  store_enable
*DESCRIPTION
*  SS Hal enable node
*PARAMETERS
*  dev  [in] the device
*  attr [in] attribute
*  buf  [in] 1:open 0:close
*RETURN
*  count
*********************************************/
static ssize_t store_enable(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
    struct input_dev* temp_input_dev;
	uint32_t data_en = 0;
	aw9610x_t this = aw9610x_ptr;

    temp_input_dev = container_of(dev,struct input_dev,dev);

    LOG_INFO("%s: dev->name:%s:%s\n", __func__,temp_input_dev->name,buf);

    if(!strncmp(temp_input_dev->name,"grip_sensor",sizeof("grip_sensor")))
    {
        if (!strncmp(buf, "1", 1))
        {	
            LOG_INFO("name:grip_sensor:enable\n");
			// enable aw96105 channel0,libo7.wt
            aw9610x_i2c_write(this, REG_CMD, AW9610X_ACTIVE_MODE);
            enable_irq(this->to_irq);
	        AWLOGE(this->dev, "cap sensor enable,change to ACTIVE mode.");
            aw9610x_i2c_read(this, REG_SCANCTRL0, &data_en);
			AWLOGE(this->dev, "REG_SCANCTRL0 = %02x\n",data_en);
            aw9610x_i2c_write_bits(this, REG_SCANCTRL0, ~(0x3f << 8), (data_en & 0x3f) << 8);
			#ifdef ANFR_TEST
			this->back_cap[0] = read_ch0_offset();
			AWLOGE(this->dev, "this->back_cap[0] = %d\n",this->back_cap[0]);
		    if(this->aw_first_boot)
			    input_report_rel(this->aw_pad[0].input, REL_MISC, 1);
            else
			    input_report_rel(this->aw_pad[0].input, REL_MISC, 2);
            input_sync(this->aw_pad[0].input);
			#else
			input_report_rel(this->aw_pad[0].input, REL_MISC, 2);
            input_sync(this->aw_pad[0].input);
			#endif					

            this->enable_flag |= CAPSENSOR_ENABLE_FLAG_CH0;

        }else{
            LOG_INFO("name:grip_sensor:disable\n");
			// disable aw96105 channel0,libo7.wt
            this->enable_flag &= ~CAPSENSOR_ENABLE_FLAG_CH0;
        }
    }

    if(!strncmp(temp_input_dev->name,"grip_sensor_sub",sizeof("grip_sensor_sub")))
    {
        if (!strncmp(buf, "1", 1))
        {
            LOG_INFO("name:grip_sensor_sub:enable\n");
			// enable aw96105 channel2,libo7.wt
            aw9610x_i2c_write(this, REG_CMD, AW9610X_ACTIVE_MODE);
            enable_irq(this->to_irq);
	        AWLOGE(this->dev, "cap sensor enable,change to ACTIVE mode.");
            aw9610x_i2c_read(this, REG_SCANCTRL0, &data_en);
			AWLOGE(this->dev, "REG_SCANCTRL0 = %02x\n",data_en);
            aw9610x_i2c_write_bits(this, REG_SCANCTRL0, ~(0x3f << 8), (data_en & 0x3f) << 8);
			#ifdef ANFR_TEST
			this->back_cap[1] = read_ch2_offset();
			AWLOGE(this->dev, "this->back_cap[1] = %d\n",this->back_cap[1]);
		    if(this->aw_first_boot)
			    input_report_rel(this->aw_pad[2].input, REL_MISC, 1);
            else
			    input_report_rel(this->aw_pad[2].input, REL_MISC, 2);			
            input_sync(this->aw_pad[2].input);
			#else
			input_report_rel(this->aw_pad[2].input, REL_MISC, 2);
            input_sync(this->aw_pad[2].input);
			#endif

            this->enable_flag |= CAPSENSOR_ENABLE_FLAG_CH2;

        }else{
            LOG_INFO("name:grip_sensor_sub:disable\n");
			// disable aw96105 channel2,libo7.wt
            this->enable_flag &= ~CAPSENSOR_ENABLE_FLAG_CH2;
        }
    }


    if(!strncmp(temp_input_dev->name,"grip_sensor_wifi",sizeof("grip_sensor_wifi")))
    {
        if (!strncmp(buf, "1", 1))
        {
            LOG_INFO("name:grip_sensor_wifi:enable\n");
			// enable aw96105 channel3,libo7.wt
            aw9610x_i2c_write(this, REG_CMD, AW9610X_ACTIVE_MODE);
            enable_irq(this->to_irq);
	        AWLOGE(this->dev, "cap sensor enable,change to ACTIVE mode.");
            aw9610x_i2c_read(this, REG_SCANCTRL0, &data_en);
			AWLOGE(this->dev, "sar_test REG_SCANCTRL0 = %02x\n",data_en);
            aw9610x_i2c_write_bits(this, REG_SCANCTRL0, ~(0x3f << 8), (data_en & 0x3f) << 8);
			#ifdef ANFR_TEST
			this->back_cap[2] = read_ch3_offset();
			AWLOGE(this->dev, "this->back_cap[2] = %d\n",this->back_cap[2]);
		    if(this->aw_first_boot)
			    input_report_rel(this->aw_pad[3].input, REL_MISC, 1);
            else
			    input_report_rel(this->aw_pad[3].input, REL_MISC, 2);
            input_sync(this->aw_pad[3].input);
			#else
			input_report_rel(this->aw_pad[3].input, REL_MISC, 2);
            input_sync(this->aw_pad[3].input);
			#endif

            this->enable_flag |= CAPSENSOR_ENABLE_FLAG_CH3;
        }else{
            LOG_INFO("name:grip_sensor_wifi:disable\n");
			// disable aw96105 channel2,libo7.wt
            this->enable_flag &= ~CAPSENSOR_ENABLE_FLAG_CH3;
        }
    }
    if(!(this->enable_flag & this->channel_func)){
        mEnabled = 0;
        disable_irq(this->to_irq);
        aw9610x_i2c_write(this, REG_CMD, AW9610X_SLEEP_MODE);
        AWLOGE(this->dev, "all cap sensor disable,change to SLEEP mode.");
    }
	return count;
}
static DEVICE_ATTR(enable, 0660, show_enable, store_enable);
/* ---input_dev attrabute enable for ss hal,libo7.wt */

static int capsensor_set_enable(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	uint32_t data_en = 0;
    aw9610x_t this = aw9610x_ptr;

    if (enable == 1) {
        if(mEnabled == 0){
            mEnabled = 1;
            aw9610x_i2c_write(this, REG_CMD, AW9610X_ACTIVE_MODE);
            enable_irq(this->to_irq);
	        AWLOGE(this->dev, "cap sensor enable,change to ACTIVE mode.");
            aw9610x_i2c_read(this, REG_SCANCTRL0, &data_en);
            aw9610x_i2c_write_bits(this, REG_SCANCTRL0, ~(0x3f << 8), (data_en & 0x3f) << 8);
        }

        if(!strcmp(sensors_cdev->name, "grip_sensor")){
            input_report_rel(this->aw_pad[0].input, REL_MISC, 2);
            input_sync(this->aw_pad[0].input);
            this->enable_flag |= CAPSENSOR_ENABLE_FLAG_CH0;
        }else if(!strcmp(sensors_cdev->name, "grip_sensor_sub")){
            input_report_rel(this->aw_pad[2].input, REL_MISC, 2);
            input_sync(this->aw_pad[2].input);
            this->enable_flag |= CAPSENSOR_ENABLE_FLAG_CH2;
        }else if(!strcmp(sensors_cdev->name, "grip_sensor_wifi")){
            input_report_rel(this->aw_pad[3].input, REL_MISC, 2);
            input_sync(this->aw_pad[3].input);
            this->enable_flag |= CAPSENSOR_ENABLE_FLAG_CH3;
        }
    } else if (enable == 0) {
        if(!strcmp(sensors_cdev->name, "grip_sensor")){
            this->enable_flag &= ~CAPSENSOR_ENABLE_FLAG_CH0;
        }else if(!strcmp(sensors_cdev->name, "grip_sensor_sub")){
            this->enable_flag &= ~CAPSENSOR_ENABLE_FLAG_CH2;
        }else if(!strcmp(sensors_cdev->name, "grip_sensor_wifi")){
            this->enable_flag &= ~CAPSENSOR_ENABLE_FLAG_CH3;
        }
    }
	AWLOGE(this->dev, "%s enable = %d", sensors_cdev->name, enable);
    if(!(this->enable_flag & this->channel_func)){
        mEnabled = 0;
        disable_irq(this->to_irq);
        aw9610x_i2c_write(this, REG_CMD, AW9610X_SLEEP_MODE);
        AWLOGE(this->dev, "all cap sensor disable,change to SLEEP mode.");
    }
    return 0;
}

static ssize_t power_enable_store(struct class *class,
        struct class_attribute *attr,
        const char *buf, size_t count)
{
	aw9610x_t this = aw9610x_ptr;
	AWLOGE(this->dev, "power_enable_store=%c\n", *buf);
	if(!strncmp(buf,"1",1))
	{
		force_report_far = 1;
	}
	else if(!strncmp(buf,"0",1))
	{
		force_report_far = 0;
	}
	return count;
}


static ssize_t power_enable_show(struct class *class,
        struct class_attribute *attr,
        char *buf)
{
	

	if(force_report_far == 1)
    {
		return sprintf(buf,"%s\n","1");
	}
	else if(force_report_far == 0)
	{
		return sprintf(buf,"%s\n","0");
	}

    return 0;
}
//static CLASS_ATTR_RW(power_enable);
static CLASS_ATTR_RW(power_enable);

//static DEVICE_ATTR(power_enable, 0644, power_enable_show, power_enable_store);
static ssize_t user_test_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	return snprintf(buf, 8, "%d\n", user_test);
}

static ssize_t user_test_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
    int i, j;
    int ret = -1;
	uint32_t data_en = 0;
	aw9610x_t this = aw9610x_ptr;

	if (!count)
		return -EINVAL;

    ret = kstrtoint(buf, 10, &user_test);
    if (0 != ret) {
        AWLOGE(this->dev,"kstrtoint failed\n");
    }

    printk("aw:user_test:%d", user_test);
    if(user_test){
#ifdef ANFR_TEST
	    this->aw_first_boot = false;
        printk("aw: this->aw_first_boot= %d;", this->aw_first_boot);
	    printk("aw:user_test mode, exit force near mode!!!");
#endif

        printk("aw ch user_test mode cali ... \n");
	    aw9610x_i2c_read(this, REG_SCANCTRL0, &data_en);
		aw9610x_i2c_write_bits(this, REG_SCANCTRL0, ~(0x3f << 8),
							(data_en & 0x3f) << 8);

        j = this->sar_num;
        for (i = 0; i < AW_CHANNEL_MAX; i++)
        {
            if(this->channel_func & (1 << i))
            {
                input_report_rel(this->aw_pad[j * AW_CHANNEL_MAX + i].input,
							REL_MISC, 2);
                input_sync(this->aw_pad[j * AW_CHANNEL_MAX + i].input);
            }
        }
    }

	return count;
}
static CLASS_ATTR_RW(user_test);

/* +++for factory mode use,libo7.wt */
static ssize_t ch0_cap_diff_dump_show(struct class *class,
        struct class_attribute *attr,
        char *buf)
{
    uint32_t reg_val = 0;
	int32_t diff_reg_val = 0;
    aw9610x_t this = aw9610x_ptr;
    //char *p = buf;
	ssize_t len = 0;
	uint32_t coff_data = 0;
	uint32_t coff_data_int = 0;
	uint32_t coff_data_dec = 0;
	uint8_t temp_data[20] = { 0 };

    //CH0 background cap
    //aw9610x_i2c_read(this, REG_RAW_CH0, &reg_val);
    //reg_val /= AW_DATA_PROCESS_FACTOR;
    //p += snprintf(p, PAGE_SIZE, "CH0_background_cap=%d;", reg_val);

	aw9610x_i2c_read(this, REG_AFECFG1_CH0, &reg_val);
	coff_data = (reg_val >> 24) * 900 +
					((reg_val >> 16) & 0xff) * 13;
	coff_data_int = coff_data / 1000;
	coff_data_dec = coff_data % 1000;
	snprintf(temp_data, sizeof(temp_data), "%d.%d", coff_data_int,
							coff_data_dec);
	AWLOGE(this->dev, "CH0_background_cap=%s;", temp_data);
	len += snprintf(buf+len, PAGE_SIZE-len,
			"CH0_background_cap=%s;", temp_data);

    //CH0 refer channel cap
    //aw9610x_i2c_read(this, REG_RAW_CH0 + 4, &reg_val);
    //reg_val /= AW_DATA_PROCESS_FACTOR;
    //p += snprintf(p, PAGE_SIZE, "CH0_refer_channel_cap=%d;", reg_val);

	aw9610x_i2c_read(this, REG_AFECFG1_CH0 + 20, &reg_val);
	coff_data = (reg_val >> 24) * 900 +
					((reg_val >> 16) & 0xff) * 13;
	coff_data_int = coff_data / 1000;
	coff_data_dec = coff_data % 1000;
	snprintf(temp_data, sizeof(temp_data), "%d.%d", coff_data_int,
							coff_data_dec);
	AWLOGE(this->dev, "CH0_refer_channel_cap=%s;", temp_data);
	len += snprintf(buf+len, PAGE_SIZE-len,
			"CH0_refer_channel_cap=%s;", temp_data);

    //CH0 diff
    aw9610x_i2c_read(this, REG_DIFF_CH0, &diff_reg_val);
    diff_reg_val /= AW_DATA_PROCESS_FACTOR;
    len += snprintf(buf+len, PAGE_SIZE-len, "CH0_diff=%d", diff_reg_val);

    return (len);
}
static CLASS_ATTR_RO(ch0_cap_diff_dump);

static ssize_t ch2_cap_diff_dump_show(struct class *class,
        struct class_attribute *attr,
        char *buf)
{
    uint32_t reg_val = 0;
	int32_t diff_reg_val = 0;
    aw9610x_t this = aw9610x_ptr;
    //char *p = buf;
	ssize_t len = 0;
	uint32_t coff_data = 0;
	uint32_t coff_data_int = 0;
	uint32_t coff_data_dec = 0;
	uint8_t temp_data[20] = { 0 };

    //CH2 background cap
    //aw9610x_i2c_read(this, REG_RAW_CH0 + 8, &reg_val);
    //reg_val /= AW_DATA_PROCESS_FACTOR;
    //p += snprintf(p, PAGE_SIZE, "CH2_background_cap=%d;", reg_val);

	aw9610x_i2c_read(this, REG_AFECFG1_CH0 + 40, &reg_val);
	coff_data = (reg_val >> 24) * 900 +
					((reg_val >> 16) & 0xff) * 13;
	coff_data_int = coff_data / 1000;
	coff_data_dec = coff_data % 1000;
	snprintf(temp_data, sizeof(temp_data), "%d.%d", coff_data_int,
							coff_data_dec);
	AWLOGE(this->dev, "CH2_background_cap=%s;", temp_data);
	len += snprintf(buf+len, PAGE_SIZE-len,
			"CH2_background_cap=%s;", temp_data);

    //CH2 refer channel cap
    //aw9610x_i2c_read(this, REG_RAW_CH0 + 16, &reg_val);
    //reg_val /= AW_DATA_PROCESS_FACTOR;
    //p += snprintf(p, PAGE_SIZE, "CH2_refer_channel_cap=%d;", reg_val);

	aw9610x_i2c_read(this, REG_AFECFG1_CH0 + 80, &reg_val);
	coff_data = (reg_val >> 24) * 900 +
					((reg_val >> 16) & 0xff) * 13;
	coff_data_int = coff_data / 1000;
	coff_data_dec = coff_data % 1000;
	snprintf(temp_data, sizeof(temp_data), "%d.%d", coff_data_int,
							coff_data_dec);
	AWLOGE(this->dev, "CH2_refer_channel_cap=%s;", temp_data);
	len += snprintf(buf+len, PAGE_SIZE-len,
			"CH2_refer_channel_cap=%s;", temp_data);

    //CH2 diff
    aw9610x_i2c_read(this, REG_DIFF_CH0 + 8, &diff_reg_val);
    diff_reg_val /= AW_DATA_PROCESS_FACTOR;
    len += snprintf(buf+len, PAGE_SIZE-len, "CH2_diff=%d", diff_reg_val);

    return (len);
}
static CLASS_ATTR_RO(ch2_cap_diff_dump);

static ssize_t ch3_cap_diff_dump_show(struct class *class,
        struct class_attribute *attr,
        char *buf)
{
    uint32_t reg_val = 0;
	int32_t diff_reg_val = 0;
    aw9610x_t this = aw9610x_ptr;
    //char *p = buf;
	ssize_t len = 0;
	uint32_t coff_data = 0;
	uint32_t coff_data_int = 0;
	uint32_t coff_data_dec = 0;
	uint8_t temp_data[20] = { 0 };

    //CH3 background cap
    //aw9610x_i2c_read(this, REG_RAW_CH0 + 12, &reg_val);
    //reg_val /= AW_DATA_PROCESS_FACTOR;
    //p += snprintf(p, PAGE_SIZE, "CH3_background_cap=%d;", reg_val);

	aw9610x_i2c_read(this, REG_AFECFG1_CH0 + 60, &reg_val);
	coff_data = (reg_val >> 24) * 900 +
					((reg_val >> 16) & 0xff) * 13;
	coff_data_int = coff_data / 1000;
	coff_data_dec = coff_data % 1000;
	snprintf(temp_data, sizeof(temp_data), "%d.%d", coff_data_int,
							coff_data_dec);
	AWLOGE(this->dev, "CH3_background_cap=%s;", temp_data);
	len += snprintf(buf+len, PAGE_SIZE-len,
			"CH3_background_cap=%s;", temp_data);

    //CH3 refer channel cap
    //aw9610x_i2c_read(this, REG_RAW_CH0 + 16, &reg_val);
    //reg_val /= AW_DATA_PROCESS_FACTOR;
    //p += snprintf(p, PAGE_SIZE, "CH3_refer_channel_cap=%d;", reg_val);

	aw9610x_i2c_read(this, REG_AFECFG1_CH0 + 80, &reg_val);
	coff_data = (reg_val >> 24) * 900 +
					((reg_val >> 16) & 0xff) * 13;
	coff_data_int = coff_data / 1000;
	coff_data_dec = coff_data % 1000;
	snprintf(temp_data, sizeof(temp_data), "%d.%d", coff_data_int,
							coff_data_dec);
	AWLOGE(this->dev, "CH3_refer_channel_cap=%s;", temp_data);
	len += snprintf(buf+len, PAGE_SIZE-len,
			"CH3_refer_channel_cap=%s;", temp_data);

    //CH3 diff
    aw9610x_i2c_read(this, REG_DIFF_CH0 + 12, &diff_reg_val);
    diff_reg_val /= AW_DATA_PROCESS_FACTOR;
    len += snprintf(buf+len, PAGE_SIZE-len, "CH3_diff=%d", diff_reg_val);

    return (len);
}
static CLASS_ATTR_RO(ch3_cap_diff_dump);

static struct class capsense_class = {
    .name           = "capsense",
    .owner          = THIS_MODULE,
};
/* ---for factory mode use,libo7.wt */



static int32_t
aw9610x_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	aw9610x_t aw9610x;
	struct device_node *np = i2c->dev.of_node;
	int32_t ret = 0;
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t err_num = 0;
	uint32_t fw_ver = 0;
    char firmware_ver[HARDWARE_MAX_ITEM_LONGTH];

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		AWLOGE(&i2c->dev, "check_functionality failed.");
		return -EIO;
	}

	aw9610x = devm_kzalloc(&i2c->dev, sizeof(struct aw9610x), GFP_KERNEL);
	if (aw9610x == NULL) {
		AWLOGE(&i2c->dev, "failed to malloc memory!");
		ret = -AW_MALLOC_FAILED;
		goto err_malloc;
	}

	aw9610x_i2c_set(i2c, aw9610x);

#ifdef AW_POWER_ON
	/* aw9610x power init */
	ret = aw9610x_power_init(aw9610x);
	if (ret)
		AWLOGE(&i2c->dev, "aw9610x power init failed");
	else
		aw9610x_power_enable(aw9610x, true);
#endif

	/* aw9610x chip id */
	ret = aw9610x_read_chipid(aw9610x);
	if (ret != AW_SAR_SUCCESS) {
		AWLOGE(aw9610x->dev, "read chipid failed, ret=%d", ret);
		goto err_chipid;
	}

	ret = aw9610x_version_init(aw9610x);
	if (ret < 0)
		goto err_vers_load;

	ret = aw9610x_parse_dt(&i2c->dev, aw9610x, np);
	if (ret != AW_SAR_SUCCESS) {
		AWLOGE(aw9610x->dev, "irq gpio error!, ret = %d", ret);
		goto err_pase_dt;
	}

	aw9610x_sw_reset(aw9610x);

	ret = aw9610x_init_irq_handle(aw9610x);
	if (ret != AW_SAR_SUCCESS) {
		AWLOGE(aw9610x->dev, "the trouble ret = %d", ret);
		goto err_first_irq;
	}

	ret = aw9610x_interrupt_init(aw9610x);
	if (ret == -AW_IRQ_REQUEST_FAILED) {
		AWLOGE(aw9610x->dev, "request irq failed ret = %d", ret);
		goto err_requst_irq;
	}
	
	aw9610x->aw_pad = pad_event;
	#ifdef ANFR_TEST
	aw9610x->aw_first_boot = true;
	aw9610x->back_cap[0] = 0;
	aw9610x->back_cap[1] = 0;
	aw9610x->back_cap[2] = 0;
	#endif
	aw9610x_ptr = aw9610x;
	/* input device */
	j = aw9610x->sar_num;
	for (i = 0; i < AW_CHANNEL_MAX; i++) {
		aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input = input_allocate_device();
		if (!(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input)) {
			err_num = i;
			goto exit_input_alloc_failed;
		}
		aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input->name =
						pad_event[j * AW_CHANNEL_MAX + i].name;
		input_set_capability(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input, EV_REL, REL_MISC);
		input_set_capability(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input, EV_REL, REL_MAX);
		/* +++sar sensor input_dev name for ss hal,libo7.wt */				
		if (i == aw9610x->grip_sensor_ch){
			aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input->name = "grip_sensor";
		}
		if (i == aw9610x->grip_sensor_sub_ch){
			aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input->name = "grip_sensor_sub";
		}
		if (i == aw9610x->grip_sensor_wifi_ch){
			aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input->name = "grip_sensor_wifi";
		}
		/* ---sar sensor input_dev name for ss hal,libo7.wt */	

		ret = input_register_device(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input);
		if (ret) {
			AWLOGE(aw9610x->dev, "failed to register input device");
			input_free_device(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input);
			err_num = i;
			goto exit_input_register_device_failed;
		}
            input_report_rel(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input, REL_MISC, 2);
            input_sync(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input);
	}

	// for factory mode,libo7.wt
	sensors_capsensor_ch0_cdev.sensors_enable = capsensor_set_enable;
    sensors_capsensor_ch0_cdev.sensors_poll_delay = NULL;
#if defined(CONFIG_SENSORS)
    sensors_capsensor_ch0_cdev.sensors_flush = aw9610x_grip_flush;
#endif
    ret = sensors_classdev_register(&aw9610x->aw_pad[0].input->dev, &sensors_capsensor_ch0_cdev);
    if (ret < 0)
		AWLOGE(aw9610x->dev, "failed to register ch0 sensors capsense device");

    sensors_capsensor_ch2_cdev.sensors_enable = capsensor_set_enable;
    sensors_capsensor_ch2_cdev.sensors_poll_delay = NULL;
#if defined(CONFIG_SENSORS)
    sensors_capsensor_ch2_cdev.sensors_flush = aw9610x_grip_sub_flush;
#endif
    ret = sensors_classdev_register(&aw9610x->aw_pad[2].input->dev, &sensors_capsensor_ch2_cdev);
    if (ret < 0)
		AWLOGE(aw9610x->dev, "failed to register ch2 sensors capsense device");

    sensors_capsensor_ch3_cdev.sensors_enable = capsensor_set_enable;
    sensors_capsensor_ch3_cdev.sensors_poll_delay = NULL;
#if defined(CONFIG_SENSORS)
    sensors_capsensor_ch3_cdev.sensors_flush = aw9610x_grip_wifi_flush;
#endif
    ret = sensors_classdev_register(&aw9610x->aw_pad[3].input->dev, &sensors_capsensor_ch3_cdev);
    if (ret < 0)
		AWLOGE(aw9610x->dev, "failed to register ch3 sensors capsense device");

	// for factory mode,libo7.wt
    ret = class_register(&capsense_class);
    if (ret < 0) {
        AWLOGE(aw9610x->dev, "register capsense class failed (%d)\n", ret);
        return ret;
    }

    ret = class_create_file(&capsense_class, &class_attr_ch0_cap_diff_dump);
    if (ret < 0) {
        AWLOGE(aw9610x->dev, "Create ch0_cap_diff_dump file failed (%d)\n", ret);
        return ret;
    }

    ret = class_create_file(&capsense_class, &class_attr_ch2_cap_diff_dump);
    if (ret < 0) {
        AWLOGE(aw9610x->dev, "Create ch2_cap_diff_dump file failed (%d)\n", ret);
        return ret;
    }

    ret = class_create_file(&capsense_class, &class_attr_ch3_cap_diff_dump);
    if (ret < 0) {
        AWLOGE(aw9610x->dev, "Create ch3_cap_diff_dump file failed (%d)\n", ret);
        return ret;
    }

	
    ret = class_create_file(&capsense_class, &class_attr_power_enable);
    if (ret < 0) {
        AWLOGE(aw9610x->dev, "Create ch3_cap_diff_dump file failed (%d)\n", ret);
        return ret;
    }
    
    ret = class_create_file(&capsense_class, &class_attr_user_test);
    if (ret < 0) {
        AWLOGE(aw9610x->dev, "Create user_test file failed (%d)\n", ret);
        return ret;
    }

	/* attribute */
	ret = sysfs_create_group(&i2c->dev.kobj, &aw9610x_sar_attribute_group);
	if (ret < 0) {
		AWLOGE(aw9610x->dev, "error creating sysfs attr files");
		goto err_sysfs;
	}

	/* +++add input_dev attribute for ss hal,libo7.wt */
	for (i = 0; i < AW_CHANNEL_MAX; i++) {
		if(!strncmp(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input->name,"grip_sensor",sizeof("grip_sensor"))){
			device_create_file(&aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input->dev, &dev_attr_enable);
		}
		if(!strncmp(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input->name,"grip_sensor_sub",sizeof("grip_sensor_sub"))){
			device_create_file(&aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input->dev, &dev_attr_enable);
		}
		if(!strncmp(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input->name,"grip_sensor_wifi",sizeof("grip_sensor_wifi"))){
			device_create_file(&aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input->dev, &dev_attr_enable);
		}
	}
	/* ---add input_dev attribute for ss hal,libo7.wt */
	ret = aw9610x_sar_cfg_init(aw9610x, AW_CFG_UNLOAD);
	if (ret < 0) {
		AWLOGE(aw9610x->dev, "cfg situation not confirmed!");
		goto err_cfg;
	}
	mEnabled = 0;
    aw9610x->enable_flag = 0;


    aw9610x_i2c_read(aw9610x, REG_FIRMVERSION, &fw_ver);
    AWLOGE(aw9610x->dev, "read firmware version: V0.5\n");
    snprintf(firmware_ver,HARDWARE_MAX_ITEM_LONGTH,"AW96105DNR,FW:V0.5");
    hardwareinfo_set_prop(HARDWARE_SAR,firmware_ver);
	return AW_SAR_SUCCESS;

err_cfg:
err_sysfs:
	sysfs_remove_group(&i2c->dev.kobj, &aw9610x_sar_attribute_group);
exit_input_register_device_failed:
	for (i = 0; i < err_num; i++)
		input_unregister_device(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input);
exit_input_alloc_failed:
	for (i = 0; i < err_num; i++)
		input_free_device(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input);
err_requst_irq:
	if (gpio_is_valid(aw9610x->irq_gpio))
		devm_gpio_free(&i2c->dev, aw9610x->irq_gpio);
err_first_irq:
err_pase_dt:
err_vers_load:
err_chipid:
	if (aw9610x->power_enable) {
		regulator_disable(aw9610x->vcc);
		regulator_put(aw9610x->vcc);
	}
err_malloc:
	return ret;
}

static int32_t aw9610x_i2c_remove(struct i2c_client *i2c)
{
	struct aw9610x *aw9610x = i2c_get_clientdata(i2c);
	uint32_t i = 0;
	uint32_t j = aw9610x->sar_num;

	if (aw9610x->power_enable) {
		regulator_disable(aw9610x->vcc);
		regulator_put(aw9610x->vcc);
	}

	if (gpio_is_valid(aw9610x->irq_gpio))
		devm_gpio_free(&i2c->dev, aw9610x->irq_gpio);

	for (i = 0; i < AW_CHANNEL_MAX; i++)
		input_free_device(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input);

	for (i = 0; i < AW_CHANNEL_MAX; i++)
		input_unregister_device(aw9610x->aw_pad[j * AW_CHANNEL_MAX + i].input);

	sysfs_remove_group(&i2c->dev.kobj, &aw9610x_sar_attribute_group);
	return 0;
}

static int aw9610x_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct aw9610x *aw9610x = i2c_get_clientdata(client);

	AWLOGD(aw9610x->dev, "suspend enter");

	disable_irq(aw9610x->to_irq);
	aw9610x_i2c_write(aw9610x, REG_CMD, AW9610X_SLEEP_MODE);

	return 0;
}

static int aw9610x_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct aw9610x *aw9610x = i2c_get_clientdata(client);

	AWLOGD(aw9610x->dev, "resume enter");

	aw9610x_i2c_write(aw9610x, REG_CMD, AW9610X_ACTIVE_MODE);
	enable_irq(aw9610x->to_irq);

	return 0;
}

static void aw9610x_i2c_shutdown(struct i2c_client *i2c)
{
	struct aw9610x *aw9610x = i2c_get_clientdata(i2c);

	pr_info("%s enter", __func__);

	disable_irq(aw9610x->to_irq);
	aw9610x_i2c_write(aw9610x, REG_CMD, AW9610X_SLEEP_MODE);
}

static const struct dev_pm_ops aw9610x_pm_ops = {
	.suspend = aw9610x_suspend,
	.resume = aw9610x_resume,
};

static const struct of_device_id aw9610x_dt_match[] = {
	{ .compatible = "awinic,aw9610x_sar_0" },
	{ .compatible = "awinic,aw9610x_sar_1" },
	{ },
};

static const struct i2c_device_id aw9610x_i2c_id[] = {
	{ AW9610X_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aw9610x_i2c_id);

static struct i2c_driver aw9610x_i2c_driver = {
	.driver = {
		.name = AW9610X_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw9610x_dt_match),
		.pm = &aw9610x_pm_ops,
	},
	.probe = aw9610x_i2c_probe,
	.remove = aw9610x_i2c_remove,
	.shutdown = aw9610x_i2c_shutdown,
	.id_table = aw9610x_i2c_id,
};

static int32_t __init aw9610x_i2c_init(void)
{
	int32_t ret = 0;

	pr_info("aw9610x driver version %s\n", AW9610X_DRIVER_VERSION);

	ret = i2c_add_driver(&aw9610x_i2c_driver);
	if (ret) {
		pr_err("fail to add aw9610x device into i2c\n");
		return ret;
	}

	return 0;
}

late_initcall(aw9610x_i2c_init);

static void __exit aw9610x_i2c_exit(void)
{
	i2c_del_driver(&aw9610x_i2c_driver);
}
module_exit(aw9610x_i2c_exit);
MODULE_DESCRIPTION("AW9610X SAR Driver");

MODULE_LICENSE("GPL v2");
