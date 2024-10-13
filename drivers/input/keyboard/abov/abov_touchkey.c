/* abov_touchkey.c -- Linux driver for abov chip as touchkey
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Junkyeong Kim <jk0430.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/unaligned.h>
#include <linux/regulator/consumer.h>
#include <linux/sec_class.h>
#include <linux/pm_wakeup.h>
#include <linux/pinctrl/consumer.h>

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include "abov_touchkey.h"

static int abov_tk_i2c_read_checksum(struct abov_tk_info *info);
static void abov_tk_reset(struct abov_tk_info *info);

static int abov_mode_enable(struct i2c_client *client,u8 cmd_reg, u8 cmd)
{
	return i2c_smbus_write_byte_data(client, cmd_reg, cmd);
}

#if ABOV_ISP_FIRMUP_ROUTINE
static void abov_config_gpio_i2c(struct abov_tk_info *info, int onoff)
{
	struct device *i2c_dev = info->client->dev.parent->parent;
	struct pinctrl *pinctrl_i2c;

	if (onoff) {
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "on_i2c");
		if (IS_ERR(pinctrl_i2c))
			input_err(true, &info->client->dev, "%s: Failed to configure i2c pin\n", __func__);
	} else {
		pinctrl_i2c = devm_pinctrl_get_select(i2c_dev, "off_i2c");
		if (IS_ERR(pinctrl_i2c))
			input_err(true, &info->client->dev, "%s: Failed to configure i2c pin\n", __func__);
	}
}
#endif

static int abov_tk_i2c_read(struct i2c_client *client,
		u8 reg, u8 *val, unsigned int len)
{
	struct abov_tk_info *info = i2c_get_clientdata(client);
	struct i2c_msg msg;
	int ret;
	int retry = 3;

	mutex_lock(&info->lock);
	msg.addr = client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 1;
	msg.buf = &reg;
	while (retry--) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0)
			break;

		input_err(true, &client->dev, "%s fail(address set)(%d)\n",
			__func__, retry);
		usleep_range(10000, 11000);
	}
	if (ret <= 0) {
		input_err(true, &client->dev, "%s fail(address set) ret:(%d)\n",
			__func__, ret);
		mutex_unlock(&info->lock);
		return ret;
	}
	retry = 3;
	msg.flags = 1;/*I2C_M_RD*/
	msg.len = len;
	msg.buf = val;
	while (retry--) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0) {
			mutex_unlock(&info->lock);
			return 0;
		}
		input_err(true, &client->dev, "%s fail(data read)(%d)\n",
			__func__, retry);
		msleep(10);
	}
	mutex_unlock(&info->lock);
	return ret;
}

static int abov_tk_i2c_read_data(struct i2c_client *client, u8 *val, unsigned int len)
{
	struct abov_tk_info *info = i2c_get_clientdata(client);
	struct i2c_msg msg;
	int ret;
	int retry = 3;

	mutex_lock(&info->lock);
	msg.addr = client->addr;
	msg.flags = 1;/*I2C_M_RD*/
	msg.len = len;
	msg.buf = val;
	while (retry--) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0) {
			mutex_unlock(&info->lock);
			return 0;
		}
		dev_err(&client->dev, "%s fail(data read)(%d)\n",
			__func__, retry);
		usleep_range(10000, 11000);
	}
	mutex_unlock(&info->lock);
	return ret;
}

/*

static int abov_tk_i2c_write(struct i2c_client *client,
		u8 reg, u8 *val, unsigned int len)
{
	struct abov_tk_info *info = i2c_get_clientdata(client);
	struct i2c_msg msg[1];
	unsigned char data[2];
	int ret;
	int retry = 3;

	mutex_lock(&info->lock);
	data[0] = reg;
	data[1] = *val;
	msg->addr = client->addr;
	msg->flags = I2C_M_WR;
	msg->len = 2;
	msg->buf = data;

	while (retry--) {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (ret >= 0) {
			mutex_unlock(&info->lock);
			return 0;
		}
		input_err(true, &client->dev, "%s fail(%d)\n",
			__func__, retry);
		usleep_range(10000, 11000);
	}
	mutex_unlock(&info->lock);
	return ret;
}
*/
static void release_all_fingers(struct abov_tk_info *info)
{
	struct i2c_client *client = info->client;
	int i;

	input_info(true, &client->dev, "%s called (touchkey_count=%d)\n", __func__,info->touchkey_count);

	for (i = 1; i < info->touchkey_count; i++) {
		input_report_key(info->input_dev,
			touchkey_keycode[i], 0);
	}
	input_sync(info->input_dev);
}

static int abov_tk_reset_for_bootmode(struct abov_tk_info *info)
{
	int ret=0;

	info->pdata->power(info, false);
	usleep_range(50000, 50010);
	info->pdata->power(info, true);

	return ret;

}

static void abov_tk_reset(struct abov_tk_info *info)
{
	struct i2c_client *client = info->client;

	input_info(true,&client->dev, "%s start\n", __func__);
	disable_irq_nosync(info->irq);

	release_all_fingers(info);

	abov_tk_reset_for_bootmode(info);
	msleep(ABOV_RESET_DELAY);

	if (info->flip_mode){
		abov_mode_enable(client, ABOV_FLIP, CMD_FLIP_ON);
	} else {
		if (info->glovemode)
			abov_mode_enable(client, ABOV_GLOVE, CMD_GLOVE_ON);
	}
	if (info->keyboard_mode)
		abov_mode_enable(client, ABOV_KEYBOARD, CMD_MOBILE_KBD_ON); 

	enable_irq(info->irq);
	input_info(true,&client->dev, "%s end\n", __func__);
}

static irqreturn_t abov_tk_interrupt(int irq, void *dev_id)
{
	struct abov_tk_info *info = dev_id;
	struct i2c_client *client = info->client;
	int ret;
	u8 key_buf[3] = {0};
	int vol_down, vol_up, action_key;

	if (info->power_state == SEC_INPUT_STATE_LPM) {
		__pm_wakeup_event(info->sec_touchkey_ws, 500);

		ret = wait_for_completion_interruptible_timeout(&info->resume_done, msecs_to_jiffies(500));
		if (ret == 0) {
			input_err(true, &client->dev, "%s: LPM: pm resume is not handled\n", __func__);
			return ret;
		}

		if (ret < 0) {
			input_err(true, &client->dev, "%s: LPM: -ERESTARTSYS if interrupted, %d\n", __func__, ret);
			return ret;
		}

		input_info(true, &client->dev, "%s: run LPM interrupt handler, %d\n", __func__, ret);
	}

	ret = abov_tk_i2c_read(client, ABOV_BTNSTATUS_01, &key_buf[0], 1);
	if (ret < 0) {
		input_err(true, &client->dev, "%s read fail)\n",
				__func__);
		abov_tk_reset(info);
		return IRQ_HANDLED;
	}

	ret = abov_tk_i2c_read(client, ABOV_BTNSTATUS_02, &key_buf[1], 1);
	if (ret < 0) {
		input_err(true, &client->dev, "%s read fail\n",
				__func__);
		abov_tk_reset(info);
		return IRQ_HANDLED;
	}

	input_info(true, &client->dev, " %s buf 0 = 0x%02x buf 1 = 0x%02x\n", __func__, key_buf[0], key_buf[1]);

	action_key = key_buf[0] & 0x01;
	vol_down = (key_buf[0] >> 4) & 0x01;
	vol_up = key_buf[1] & 0x01;

	if(info->irq_checked){
		if(vol_up)
			info->irq_key_count[0]++;
		if(vol_down)
			info->irq_key_count[1]++;
		if(action_key)
			info->irq_key_count[2]++;
	}

	input_report_key(info->input_dev,
			KEY_PLAYPAUSE, action_key);
	input_report_key(info->input_dev,
			KEY_VOLUMEDOWN, vol_down);
	input_report_key(info->input_dev,
			KEY_VOLUMEUP, vol_up);

	input_sync(info->input_dev);

	return IRQ_HANDLED;

}

static ssize_t touchkey_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 r_buf;
	int ret;

	ret = abov_tk_i2c_read(client, ABOV_THRESHOLD, &r_buf, 1);
	if (ret < 0) {
		input_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		r_buf = 0;
	}
	return sprintf(buf, "%d\n", r_buf);
}

static void abov_get_data(struct abov_tk_info *info, int address)
{
	struct i2c_client *client = info->client;
	u8 r_buf[2 * MAX_KEY_NUM];
	int ret;
	int i;

	ret = abov_tk_i2c_read(client, address, r_buf, 2 * MAX_KEY_NUM);
	if (ret < 0) {
		input_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);

		for (i = 0; i < MAX_KEY_NUM; i++) {
			info->keys_s[i] = 0;
		}
		return;
	}

	for (i = 0; i < MAX_KEY_NUM; i++) {
		info->keys_s[i] = (r_buf[i * 2] << 8) | r_buf[i * 2 + 1];
	}
}

static ssize_t touchkey_key_1_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);

	abov_get_data(info, ABOV_DIFFDATA);

	return sprintf(buf, "%d\n", info->keys_s[0]);
}

static ssize_t touchkey_key_2_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);

	abov_get_data(info, ABOV_DIFFDATA);

	return sprintf(buf, "%d\n", info->keys_s[1]);
}

static ssize_t touchkey_key_3_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);

	abov_get_data(info, ABOV_DIFFDATA);

	return sprintf(buf, "%d\n", info->keys_s[2]);
}

static ssize_t touchkey_raw_key_1_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);

	abov_get_data(info, ABOV_RAWDATA);

	return sprintf(buf, "%d\n", info->keys_raw[0]);
}

static ssize_t touchkey_raw_key_2_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);

	abov_get_data(info, ABOV_RAWDATA);

	return sprintf(buf, "%d\n", info->keys_raw[1]);

}

static ssize_t touchkey_raw_key_3_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);

	abov_get_data(info, ABOV_RAWDATA);

	return sprintf(buf, "%d\n", info->keys_raw[2]);

}

static ssize_t touchkey_chip_name(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;

	input_dbg(true, &client->dev, "%s\n", __func__);
#ifdef CONFIG_KEYBOARD_ABOV_TOUCH_T316
	return sprintf(buf, "A96T316\n");
#else
	return sprintf(buf, "FT1804\n");
#endif
}

static ssize_t bin_fw_ver(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;

	input_dbg(true, &client->dev, "fw version bin : 0x%x\n", info->fw_ver_bin);

	return sprintf(buf, "0x%02x\n", info->fw_ver_bin);
}

static int get_tk_fw_version(struct abov_tk_info *info, bool bootmode)
{
	struct i2c_client *client = info->client;
	u8 buf;
	int ret;
	int retry = 10;

	ret = abov_tk_i2c_read(client, ABOV_FW_VER, &buf, 1);
	if (ret < 0) {
		while (retry--) {
			input_err(true, &client->dev, "%s read fail(%d)\n",
				__func__, retry);
			if (!bootmode)
				abov_tk_reset(info);
			else
				return -1;
			ret = abov_tk_i2c_read(client, ABOV_FW_VER, &buf, 1);
			if (ret == 0)
				break;
		}
		msleep(20);
		if (retry <= 0)
			return -1;
	}

	info->fw_ver = buf;
	input_info(true, &client->dev, "%s : 0x%x\n", __func__, buf);
	return 0;
}

static ssize_t read_fw_ver(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int ret;

	ret = get_tk_fw_version(info, false);
	if (ret < 0) {
		input_err(true, &client->dev, "%s read fail\n", __func__);
		info->fw_ver = 0;
	}

	return sprintf(buf, "0x%02x\n", info->fw_ver);
}

static int abov_load_fw(struct abov_tk_info *info, u8 cmd)
{
	struct i2c_client *client = info->client;
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;

	switch(cmd) {
	case BUILT_IN:
		ret = request_firmware(&info->firm_data_bin,
			info->pdata->fw_path, &client->dev);
		if (ret) {
			input_err(true, &client->dev,
				"%s request_firmware fail(%d)\n", __func__, cmd);
			return ret;
		}
		/* Header info
		* 0x00 0x91 : model info,
		* 0x00 0x00 : module info (Rev 0.0),
		* 0x00 0xF3 : F/W
		* 0x00 0x00 0x17 0x10 : checksum
		* ~ 22byte 0x00 */
		info->fw_model_number = info->firm_data_bin->data[1];
		info->fw_ver_bin = info->firm_data_bin->data[5];
		info->checksum_h_bin = info->firm_data_bin->data[8];
		info->checksum_l_bin = info->firm_data_bin->data[9];
		info->firm_size = info->firm_data_bin->size;

		input_info(true, &client->dev, "%s, bin version:%2X,%2X,%2X   crc:%2X,%2X\n", __func__, \
			info->firm_data_bin->data[1], info->firm_data_bin->data[3], info->fw_ver_bin, \
			info->checksum_h_bin, info->checksum_l_bin);
		break;

	case SDCARD:
		old_fs = get_fs();
		set_fs(get_ds());
		fp = filp_open(TK_FW_PATH_SDCARD, O_RDONLY, S_IRUSR);
		if (IS_ERR(fp)) {
			input_err(true, &client->dev,
				"%s %s open error\n", __func__, TK_FW_PATH_SDCARD);
			ret = -ENOENT;
			goto fail_sdcard_open;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info->firm_data_ums = kzalloc((size_t)fsize, GFP_KERNEL);
		if (!info->firm_data_ums) {
			input_err(true, &client->dev,
				"%s fail to kzalloc for fw\n", __func__);
			ret = -ENOMEM;
			goto fail_sdcard_kzalloc;
		}

		nread = vfs_read(fp,
			(char __user *)info->firm_data_ums, fsize, &fp->f_pos);
		if (nread != fsize) {
			input_err(true, &client->dev,
				"%s fail to vfs_read file\n", __func__);
			ret = -EINVAL;
			goto fail_sdcard_size;
		}
		filp_close(fp, current->files);
		set_fs(old_fs);

		info->firm_size = nread;
		info->checksum_h_bin = info->firm_data_ums[8];
		info->checksum_l_bin = info->firm_data_ums[9];

		input_info(true, &client->dev,"%s, bin version:%2X,%2X,%2X   crc:%2X,%2X\n", __func__, \
			info->firm_data_ums[1], info->firm_data_ums[3], info->firm_data_ums[5], \
			info->checksum_h_bin, info->checksum_l_bin);
		break;

	default:
		ret = -1;
		break;
	}
	input_info(true, &client->dev, "fw_size : %lu\n", info->firm_size);
	input_info(true, &client->dev, "%s success\n", __func__);
	return ret;

fail_sdcard_size:
	kfree(info->firm_data_ums);
fail_sdcard_kzalloc:
	filp_close(fp, current->files);
fail_sdcard_open:
	set_fs(old_fs);
	return ret;
}

#if ABOV_ISP_FIRMUP_ROUTINE
void abov_i2c_start(int scl, int sda)
{
	gpio_direction_output(sda, 1);
	gpio_direction_output(scl, 1);
	usleep_range(15, 17);
	gpio_direction_output(sda, 0);
	usleep_range(10, 12);
	gpio_direction_output(scl, 0);
	usleep_range(10, 12);
}

void abov_i2c_stop(int scl, int sda)
{
	gpio_direction_output(scl, 0);
	usleep_range(10, 12);
	gpio_direction_output(sda, 0);
	usleep_range(10, 12);
	gpio_direction_output(scl, 1);
	usleep_range(10, 12);
	gpio_direction_output(sda, 1);
}

void abov_testdelay(void)
{
	u8 i;
	u8 delay;

	/* 120nms */
	for (i = 0; i < 15; i++)
		delay = 0;
}


void abov_byte_send(u8 data, int scl, int sda)
{
	u8 i;
	for (i = 0x80; i != 0; i >>= 1) {
		gpio_direction_output(scl, 0);
		usleep_range(1,1);

		if (data & i)
			gpio_direction_output(sda, 1);
		else
			gpio_direction_output(sda, 0);

		usleep_range(1,1);
		gpio_direction_output(scl, 1);
		usleep_range(1,1);
	}
	usleep_range(1,1);

	gpio_direction_output(scl, 0);
	gpio_direction_input(sda);
	usleep_range(1,1);

	gpio_direction_output(scl, 1);
	usleep_range(1,1);

	gpio_get_value(sda);
	abov_testdelay();

	gpio_direction_output(scl, 0);
	gpio_direction_output(sda, 0);
	usleep_range(20,20);
}

u8 abov_byte_read(bool type, int scl, int sda)
{
	u8 i;
	u8 data = 0;
	u8 index = 0x7;

	gpio_direction_output(scl, 0);
	gpio_direction_input(sda);
	usleep_range(1,1);

	for (i = 0; i < 8; i++) {
		gpio_direction_output(scl, 0);
		usleep_range(1,1);
		gpio_direction_output(scl, 1);
		usleep_range(1,1);

		data = data | (u8)(gpio_get_value(sda) << index);
		index -= 1;
	}
		usleep_range(1,1);
	gpio_direction_output(scl, 0);

	gpio_direction_output(sda, 0);
		usleep_range(1,1);

	if (type) { /*ACK */
		gpio_direction_output(sda, 0);
		usleep_range(1,1);
		gpio_direction_output(scl, 1);
		usleep_range(1,1);
		gpio_direction_output(scl, 0);
		usleep_range(1,1);
	} else { /* NAK */
		gpio_direction_output(sda, 1);
		usleep_range(1,1);
		gpio_direction_output(scl, 1);
		usleep_range(1,1);
		gpio_direction_output(scl, 0);
		usleep_range(1,1);
		gpio_direction_output(sda, 0);
		usleep_range(1,1);
	}
	usleep_range(20,20);

	return data;
}

void abov_enter_mode(int scl, int sda)
{
	abov_i2c_start(scl, sda);
	abov_testdelay();
	abov_byte_send(ABOV_ID, scl, sda);
	abov_byte_send(0xAC, scl, sda);
	abov_byte_send(0x5B, scl, sda);
	abov_byte_send(0x2D, scl, sda);
	abov_i2c_stop(scl, sda);
}

void abov_firm_write(const u8 *fw_data, int block, int scl, int sda)
{
	int i, j;
	u16 pos = 0x20;
	u8 addr[2];
	u8 data[32] = {0, };

	addr[0] = 0x10;
	addr[1] = 0x00;
	for (i = 0; i < (block - 0x20); i++) {
		if (i % 8 == 0) {
			addr[0] = 0x10 + i/8;
			addr[1] = 0;
		} else
			addr[1] = addr[1] + 0x20;
		memcpy(data, fw_data + pos, 32);
		abov_i2c_start(scl, sda);
		abov_testdelay();
		abov_byte_send(ABOV_ID, scl, sda);
		abov_byte_send(0xAC, scl, sda);
		abov_byte_send(0x7A, scl, sda);
		abov_byte_send(addr[0], scl, sda);
		abov_byte_send(addr[1], scl, sda);
		for (j = 0; j < 32; j++)
			abov_byte_send(data[j], scl, sda);
		abov_i2c_stop(scl, sda);

		pos += 0x20;

		usleep_range(3000,3000); //usleep(2000); //msleep(2);
	}
}

void abov_read_address_set(int scl, int sda)
{
		abov_i2c_start(scl, sda);
		abov_testdelay();
		abov_byte_send(ABOV_ID, scl, sda);
		abov_byte_send(0xAC, scl, sda);
		abov_byte_send(0x9E, scl, sda);
		abov_byte_send(0x10, scl, sda); /* start addr H */
		abov_byte_send(0x00, scl, sda); /* start addr L */
		abov_byte_send(0x3F, scl, sda); /* end addr H  */
		abov_byte_send(0xFF, scl, sda); /* end addr L  */
		abov_i2c_stop(scl, sda);
}

void abov_checksum(struct abov_tk_info *info, int scl, int sda)
{
	struct i2c_client *client = info->client;

	u8 status;
	u8 bootver;
	u8 firmver;
	u8 checksumh;
	u8 checksuml;

	abov_read_address_set(scl, sda);
	msleep(5);

	abov_i2c_start(scl, sda);
	abov_testdelay();
	abov_byte_send(ABOV_ID, scl, sda);
	abov_byte_send(0x00, scl, sda);

	abov_i2c_start(scl, sda); /* restart */
	abov_testdelay();
	abov_byte_send(ABOV_ID + 1, scl, sda);
	status = abov_byte_read(true, scl, sda);
	bootver = abov_byte_read(true, scl, sda);
	firmver = abov_byte_read(true, scl, sda);
	checksumh = abov_byte_read(true, scl, sda);
	checksuml = abov_byte_read(false, scl, sda);
	abov_i2c_stop(scl, sda);
	msleep(3);

	info->checksum_h = checksumh;
	info->checksum_l = checksuml;

	input_dbg(true, &client->dev,
		"%s status(0x%x), boot(0x%x), firm(0x%x), cs_h(0x%x), cs_l(0x%x)\n",
		__func__, status, bootver, firmver, checksumh, checksuml);
}

void abov_exit_mode(int scl, int sda)
{
	abov_i2c_start(scl, sda);
	abov_testdelay();
	abov_byte_send(ABOV_ID, scl, sda);
	abov_byte_send(0xAC, scl, sda);
	abov_byte_send(0x5B, scl, sda);
	abov_byte_send(0xE1, scl, sda);
	abov_i2c_stop(scl, sda);
}

static int abov_fw_update(struct abov_tk_info *info,
				const u8 *fw_data, int block, int scl, int sda)
{
	input_dbg(true, &info->client->dev, "%s start (%d)\n",__func__,__LINE__);
	abov_config_gpio_i2c(info, 0);
	msleep(ABOV_BOOT_DELAY);
	abov_enter_mode(scl, sda);
	msleep(1100); //msleep(600);
	abov_firm_write(fw_data, block, scl, sda);
	abov_checksum(info, scl, sda);
	abov_config_gpio_i2c(info, 1);
	input_dbg(true, &info->client->dev, "%s end (%d)\n",__func__,__LINE__);
	return 0;
}
#endif

static int abov_tk_check_busy(struct abov_tk_info *info)
{
	int ret, count = 0;
	unsigned char val = 0x00;

	do {
		ret = i2c_master_recv(info->client, &val, sizeof(val));

		if (val & 0x01) {
			count++;
			if (count > 1000)
				input_err(true, &info->client->dev,
					"%s: busy %d\n", __func__, count);
				return ret;
		} else {
			break;
		}

	} while(1);

	return ret;
}

static int abov_tk_i2c_read_checksum(struct abov_tk_info *info)
{
	unsigned char data[6] = {0xAC, 0x9E, 0x04, 0x00, 0x37, 0xFF};
	unsigned char data2[1] = {0x00};
	unsigned char checksum[6] = {0, };
	int ret;

	i2c_master_send(info->client, data, 6);

	usleep_range(5000, 6000);

	i2c_master_send(info->client, data2, 1);
	usleep_range(5000, 6000);

	ret = abov_tk_i2c_read_data(info->client, checksum, 6);

	input_info(true, &info->client->dev, "%s: ret:%d [%X][%X][%X][%X][%X]\n",
			__func__, ret, checksum[0], checksum[1], checksum[2]
			, checksum[4], checksum[5]);
	info->checksum_h = checksum[4];
	info->checksum_l = checksum[5];

	return 0;
}

static int abov_tk_fw_write(struct abov_tk_info *info, unsigned char *addrH,
						unsigned char *addrL, unsigned char *val)
{
	int length = 36, ret = 0;
	unsigned char data[36];

	data[0] = 0xAC;
	data[1] = 0x7A;
	memcpy(&data[2], addrH, 1);
	memcpy(&data[3], addrL, 1);
	memcpy(&data[4], val, 32);

	ret = i2c_master_send(info->client, data, length);
	if (ret != length) {
		input_err(true, &info->client->dev,
			"%s: write fail[%x%x], %d\n", __func__, *addrH, *addrL, ret);
		return ret;
	}

	usleep_range(3000, 3000);

	abov_tk_check_busy(info);

	return 0;
}

static int abov_tk_fw_mode_enter(struct abov_tk_info *info)
{
	unsigned char data[2] = {0xAC, 0x5B};
	int ret = 0;

	ret = i2c_master_send(info->client, data, 2);
	if (ret != 2) {
		pr_err("%s: write fail\n", __func__);
		return -1;
	}

	return 0;

}


static int abov_tk_fw_mode_check(struct abov_tk_info *info)
{
	unsigned char buf;
	int ret;

	ret = abov_tk_i2c_read_data(info->client, &buf, 1);
	if(ret<0){
		pr_err("%s: write fail\n", __func__);
		return 0;
	}

	dev_info(&info->client->dev, "%s: ret:%02X\n",__func__, buf);

	if(buf == info->pdata->firmup_cmd)	
		return 1;


	pr_err("%s: value is same same,  %X, %X \n", __func__, info->pdata->firmup_cmd, buf);

	return 0;
}

static int abov_tk_flash_erase(struct abov_tk_info *info)
{
	unsigned char data[2] = {0xAC, 0x2D};
	int ret = 0;

	ret = i2c_master_send(info->client, data, 2);
	if (ret != 2) {
		pr_err("%s: write fail\n", __func__);
		return -1;
	}

	return 0;

}

static int abov_tk_fw_mode_exit(struct abov_tk_info *info)
{
	unsigned char data[2] = {0xAC, 0xE1};
	int ret = 0;

	ret = i2c_master_send(info->client, data, 2);
	if (ret != 2) {
		pr_err("%s: write fail\n", __func__);
		return -1;
	}
	msleep(150);

	return 0;

}

static int abov_tk_fw_update(struct abov_tk_info *info, u8 cmd)
{
	int ret, ii = 0;
	int count;
	unsigned short address;
	unsigned char addrH, addrL;
	unsigned char data[32] = {0, };


	input_info(true, &info->client->dev, "%s start\n", __func__);

	count = info->firm_size / 32;
	address = USER_CODE_ADDRESS;

	input_info(true, &info->client->dev, "%s reset\n", __func__);
	abov_tk_reset_for_bootmode(info);
	msleep(ABOV_BOOT_DELAY);
	ret = abov_tk_fw_mode_enter(info);
	if(ret<0){
		input_err(true, &info->client->dev,
			"%s:abov_tk_fw_mode_enter fail\n", __func__);
		return ret;
	}
	usleep_range(5 * 1000, 5 * 1000);
	input_info(true, &info->client->dev, "%s fw_mode_cmd sended\n", __func__);

	if(abov_tk_fw_mode_check(info) != 1) {
		pr_err("%s: err, flash mode is not: %d\n", __func__, ret);
		return 0;
	}

	ret = abov_tk_flash_erase(info);
	msleep(1400);
	input_info(true, &info->client->dev, "%s fw_write start\n", __func__);

	for (ii = 1; ii < count; ii++) {
		/* first 32byte is header */
		addrH = (unsigned char)((address >> 8) & 0xFF);
		addrL = (unsigned char)(address & 0xFF);
		if (cmd == BUILT_IN)
			memcpy(data, &info->firm_data_bin->data[ii * 32], 32);
		else if (cmd == SDCARD)
			memcpy(data, &info->firm_data_ums[ii * 32], 32);

		ret = abov_tk_fw_write(info, &addrH, &addrL, data);
		if (ret < 0) {
			input_err(true, &info->client->dev,
				"%s: err, no device : %d\n", __func__, ret);
			return ret;
		}

		address += 0x20;

		memset(data, 0, 32);
	}
	ret = abov_tk_i2c_read_checksum(info);
	input_dbg(true, &info->client->dev, "%s checksum readed\n", __func__);

	ret = abov_tk_fw_mode_exit(info);
	input_info(true, &info->client->dev, "%s fw_write end\n", __func__);

	return ret;
}

static void abov_release_fw(struct abov_tk_info *info, u8 cmd)
{
	switch(cmd) {
	case BUILT_IN:
		release_firmware(info->firm_data_bin);
		break;

	case SDCARD:
		kfree(info->firm_data_ums);
		break;

	default:
		break;
	}
}

static int abov_flash_fw(struct abov_tk_info *info, bool probe, u8 cmd)
{
	struct i2c_client *client = info->client;
	int retry = 2;
	int ret;
	int block_count;
	const u8 *fw_data;

	switch(cmd) {
	case BUILT_IN:
		fw_data = info->firm_data_bin->data;
		break;

	case SDCARD:
		fw_data = info->firm_data_ums;
		break;

	default:
		return -1;
		break;
	}

	block_count = (int)(info->firm_size / 32);

	while (retry--) {
		ret = abov_tk_fw_update(info, cmd);
		if (ret < 0)
			break;
#if ABOV_ISP_FIRMUP_ROUTINE
		abov_tk_reset_for_bootmode(info);
		abov_fw_update(info, fw_data, block_count,
			info->pdata->gpio_scl, info->pdata->gpio_sda);
#endif

		if ((info->checksum_h != info->checksum_h_bin) ||
			(info->checksum_l != info->checksum_l_bin)) {
			input_err(true, &client->dev,
				"%s checksum fail.(0x%x,0x%x),(0x%x,0x%x) retry:%d\n",
				__func__, info->checksum_h, info->checksum_l,
				info->checksum_h_bin, info->checksum_l_bin, retry);
			ret = -1;
			continue;
		}else
			input_info(true, &client->dev,"%s checksum successed.\n",__func__);

		abov_tk_reset_for_bootmode(info);
		msleep(ABOV_RESET_DELAY);
		ret = get_tk_fw_version(info, true);
		if (ret) {
			input_err(true, &client->dev, "%s fw version read fail\n", __func__);
			ret = -1;
			continue;
		}

		if (info->fw_ver == 0) {
			input_err(true, &client->dev, "%s fw version fail (0x%x)\n",
				__func__, info->fw_ver);
			ret = -1;
			continue;
		}

		if ((cmd == BUILT_IN) && (info->fw_ver != info->fw_ver_bin)){
			input_err(true, &client->dev, "%s fw version fail 0x%x, 0x%x\n",
				__func__, info->fw_ver, info->fw_ver_bin);
			ret = -1;
			continue;
		}
		ret = 0;
		break;
	}

	return ret;
}

static ssize_t touchkey_fw_update(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int ret;
	u8 cmd;

	switch(*buf) {
	case 's':
	case 'S':
		cmd = BUILT_IN;
		break;
	case 'i':
	case 'I':
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
		goto touchkey_fw_update_out;
#endif
		cmd = SDCARD;
		break;
	default:
		info->fw_update_state = 2;
		goto touchkey_fw_update_out;
	}

	ret = abov_load_fw(info, cmd);
	if (ret) {
		input_err(true, &client->dev,
			"%s fw load fail\n", __func__);
		info->fw_update_state = 2;
		goto touchkey_fw_update_out;
	}

	info->fw_update_state = 1;
	disable_irq(info->irq);
	ret = abov_flash_fw(info, false, cmd);
	if (info->flip_mode){
		abov_mode_enable(client, ABOV_FLIP, CMD_FLIP_ON);
	} else {
		if (info->glovemode)
			abov_mode_enable(client, ABOV_GLOVE, CMD_GLOVE_ON);
	}
	if (info->keyboard_mode)
		abov_mode_enable(client, ABOV_KEYBOARD, CMD_MOBILE_KBD_ON);

	enable_irq(info->irq);
	if (ret) {
		input_err(true, &client->dev, "%s fail\n", __func__);
		info->fw_update_state = 2;
	} else {
		input_info(true, &client->dev, "%s success\n", __func__);
		info->fw_update_state = 0;
	}

	abov_release_fw(info, cmd);

touchkey_fw_update_out:
	input_dbg(true, &client->dev, "%s : %d\n", __func__, info->fw_update_state);

	return count;
}

static ssize_t touchkey_fw_update_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int count = 0;

	input_info(true, &client->dev, "%s : %d\n", __func__, info->fw_update_state);

	if (info->fw_update_state == 0)
		count = sprintf(buf, "PASS\n");
	else if (info->fw_update_state == 1)
		count = sprintf(buf, "Downloading\n");
	else if (info->fw_update_state == 2)
		count = sprintf(buf, "Fail\n");

	return count;
}

static ssize_t abov_glove_mode(struct device *dev,
	 struct device_attribute *attr, const char *buf, size_t count)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int scan_buffer;
	int ret;
	u8 cmd;

	ret = sscanf(buf, "%d", &scan_buffer);
	input_info(true, &client->dev, "%s : %d\n", __func__, scan_buffer);


	if (ret != 1) {
		input_err(true, &client->dev, "%s: cmd read err\n", __func__);
		return count;
	}

	if (!(scan_buffer == 0 || scan_buffer == 1)) {
		input_err(true, &client->dev, "%s: wrong command(%d)\n",
			__func__, scan_buffer);
		return count;
	}

	if (info->glovemode == scan_buffer) {
		input_dbg(true, &client->dev, "%s same command(%d)\n",
			__func__, scan_buffer);
		return count;
	}

	if (scan_buffer == 1) {
		cmd = CMD_GLOVE_ON;
	} else {
		cmd = CMD_GLOVE_OFF;
	}

	ret = abov_mode_enable(client, ABOV_GLOVE, cmd);
	if (ret < 0) {
		input_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		return count;
	}

	info->glovemode = scan_buffer;

	return count;
}

static ssize_t abov_glove_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", info->glovemode);
}

static ssize_t keyboard_cover_mode_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int keyboard_mode_on;
	int ret;
	u8 cmd;

	input_dbg(true, &client->dev, "%s : Mobile KBD sysfs node called\n",__func__);

	sscanf(buf, "%d", &keyboard_mode_on);
	input_info(true, &client->dev, "%s : %d\n",
		__func__, keyboard_mode_on);

	if (!(keyboard_mode_on == 0 || keyboard_mode_on == 1)) {
		input_err(true, &client->dev, "%s: wrong command(%d)\n",
			__func__, keyboard_mode_on);
		return count;
	}

	if (info->keyboard_mode == keyboard_mode_on) {
		input_dbg(true, &client->dev, "%s same command(%d)\n",
			__func__, keyboard_mode_on);
		goto out;
	}

	if (keyboard_mode_on == 1) {
		cmd = CMD_MOBILE_KBD_ON;
	} else {
		cmd = CMD_MOBILE_KBD_OFF;
	}

	/* mobile keyboard use same register with glove mode */
	ret = abov_mode_enable(client, ABOV_KEYBOARD, cmd);
	if (ret < 0) {
		input_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		goto out;
	}

out:
	info->keyboard_mode = keyboard_mode_on;
	return count;
}

static ssize_t flip_cover_mode_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int flip_mode_on;
	int ret;
	u8 cmd;

	sscanf(buf, "%d\n", &flip_mode_on);
	input_info(true, &client->dev, "%s : %d\n", __func__, flip_mode_on);

	/* glove mode control */
	if (flip_mode_on) {
		cmd = CMD_FLIP_ON;
	} else {
		if (info->glovemode)
			cmd = CMD_GLOVE_ON;
		cmd = CMD_FLIP_OFF;
	}

	if (info->glovemode){
		ret = abov_mode_enable(client, ABOV_GLOVE, cmd);
		if (ret < 0) {
			input_err(true, &client->dev, "%s glove mode fail(%d)\n", __func__, ret);
			goto out;
		}
	} else{
		ret = abov_mode_enable(client, ABOV_FLIP, cmd);
		if (ret < 0) {
			input_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
			goto out;
		}
	}
out:
	info->flip_mode = flip_mode_on;
	return count;
}

static ssize_t touchkey_irq_count_show(struct device *dev, struct device_attribute *attr, char *buf)

{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;

	input_info(true, &client->dev, "%s - volup : %d, voldn : %d, playpause : %d \n", 
		__func__, info->irq_key_count[0], info->irq_key_count[1], info->irq_key_count[2]);

	return sprintf(buf, "%d,%d,%d\n", info->irq_key_count[0], info->irq_key_count[1], info->irq_key_count[2]);
}

static ssize_t touchkey_irq_count_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 onoff = 0;
	int i;

	if (buf[0] == 48) {
		onoff = 0;
	} else if (buf[0] == 49) {
		onoff = 1;
	} else {
		input_err(true,&client->dev, "%s [ERROR] Unknown value [%c]\n", __func__, buf[0]);
		return count;
	}
	
	if (onoff == 0) {
		info->irq_checked= 0;
	} else if (onoff == 1) {
		info->irq_checked = 1;
		for(i=0; i < MAX_KEY_NUM; i++)
			info->irq_key_count[i] = 0;
	} else {
		input_err(true, &client->dev, "%s - unknown value %d\n", __func__, onoff);
		return count;
	}

	input_info(true,&client->dev, "%s - %d [DONE]\n", __func__, onoff);
	return count;
}
static DEVICE_ATTR(touchkey_threshold, S_IRUGO, touchkey_threshold_show, NULL);
static DEVICE_ATTR(touchkey_volup, S_IRUGO, touchkey_key_3_show, NULL);
static DEVICE_ATTR(touchkey_voldn, S_IRUGO, touchkey_key_2_show, NULL);
static DEVICE_ATTR(touchkey_pause, S_IRUGO, touchkey_key_1_show, NULL);
static DEVICE_ATTR(touchkey_volup_raw, S_IRUGO, touchkey_raw_key_3_show, NULL);
static DEVICE_ATTR(touchkey_voldn_raw, S_IRUGO, touchkey_raw_key_2_show, NULL);
static DEVICE_ATTR(touchkey_pause_raw, S_IRUGO, touchkey_raw_key_1_show, NULL);
static DEVICE_ATTR(touchkey_chip_name, S_IRUGO, touchkey_chip_name, NULL);
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO, bin_fw_ver, NULL);
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO, read_fw_ver, NULL);
static DEVICE_ATTR(touchkey_firm_update, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
			touchkey_fw_update);
static DEVICE_ATTR(touchkey_firm_update_status, S_IRUGO | S_IWUSR | S_IWGRP,
			touchkey_fw_update_status, NULL);
static DEVICE_ATTR(glove_mode, S_IRUGO | S_IWUSR | S_IWGRP,
			abov_glove_mode_show, abov_glove_mode);
static DEVICE_ATTR(keyboard_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
			keyboard_cover_mode_enable);
static DEVICE_ATTR(touchkey_irq_count, S_IRUGO | S_IWUSR | S_IWGRP, 
            touchkey_irq_count_show, touchkey_irq_count_store);

static DEVICE_ATTR(flip_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		    flip_cover_mode_enable);

static struct attribute *sec_touchkey_attributes[] = {
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_touchkey_volup.attr,
	&dev_attr_touchkey_voldn.attr,
	&dev_attr_touchkey_pause.attr,
	&dev_attr_touchkey_volup_raw.attr,
	&dev_attr_touchkey_voldn_raw.attr,
	&dev_attr_touchkey_pause_raw.attr,
	&dev_attr_touchkey_chip_name.attr,
	&dev_attr_touchkey_firm_version_phone.attr,
	&dev_attr_touchkey_firm_version_panel.attr,
	&dev_attr_touchkey_firm_update.attr,
	&dev_attr_touchkey_firm_update_status.attr,
	&dev_attr_glove_mode.attr,
	&dev_attr_keyboard_mode.attr,
	&dev_attr_flip_mode.attr,
	&dev_attr_touchkey_irq_count.attr,
	NULL,
};

static struct attribute_group sec_touchkey_attr_group = {
	.attrs = sec_touchkey_attributes,
};

extern int get_samsung_lcd_attached(void);

static int abov_tk_fw_check(struct abov_tk_info *info)
{
	struct i2c_client *client = info->client;
	int ret, fw_update=0;
	u8 buf;

	ret = abov_load_fw(info, BUILT_IN);
	if (ret) {
		input_err(true, &client->dev,
			"%s fw load fail\n", __func__);
		return ret;
	}

	ret = get_tk_fw_version(info, true);

	ret = abov_tk_i2c_read(client, ABOV_MODEL_NUMBER, &buf, 1);
	if (ret < 0) {
		input_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
	}
	if(info->fw_model_number != buf){
		input_info(true, &client->dev, "fw model number = %x ic model number = %x \n", info->fw_model_number, buf);
		fw_update = 1;
		goto flash_fw;
	}

	if ((info->fw_ver == 0) || info->fw_ver < info->fw_ver_bin){
		input_dbg(true, &client->dev, "excute tk firmware update (0x%x -> 0x%x\n",
			info->fw_ver, info->fw_ver_bin);
		fw_update = 1;
	}

flash_fw:

	if(fw_update){
		ret = abov_flash_fw(info, true, BUILT_IN);
		if (ret) {
			input_err(true, &client->dev,
				"failed to abov_flash_fw (%d)\n", ret);
		} else {
			input_info(true, &client->dev,
				"fw update success\n");
		}
	}

	abov_release_fw(info, BUILT_IN);

	return ret;
}

static int abov_power_regulator_ctrl(void *data, bool on)
{
	struct abov_tk_info *info = (struct abov_tk_info *)data;
	struct i2c_client *client = info->client;
	int ret = 0;

	info->pdata->dvdd_vreg = regulator_get(NULL, info->pdata->dvdd_vreg_name);
	if (IS_ERR(info->pdata->dvdd_vreg)) {
		info->pdata->dvdd_vreg = NULL;
		input_err(true, &client->dev, "%s : dvdd_vreg get error, ignoring\n",__func__);
	}

	if (on) {
		if (info->pdata->dvdd_vreg) {
			ret = regulator_enable(info->pdata->dvdd_vreg);
			if(ret){
				input_err(true, &client->dev, "%s : dvdd reg enable fail\n", __func__);
				return ret;
			}
		}
	} else {
		if (info->pdata->dvdd_vreg) {
			ret = regulator_disable(info->pdata->dvdd_vreg);
			if(ret){
				input_err(true, &client->dev, "%s : dvdd reg disable fail\n", __func__);
				return ret;
			}
		}
	}
	regulator_put(info->pdata->dvdd_vreg); 

	input_info(true, &client->dev, "%s %s\n", __func__, on ? "on" : "off");

	return ret;
}

static int abov_pinctrl_configure(struct abov_tk_info *info,
							bool active)
{
	struct pinctrl_state *set_state;
	int retval;

	if (active) {
		set_state =
			pinctrl_lookup_state(info->pinctrl,
						"on_irq");
		if (IS_ERR(set_state)) {
			input_err(true, &info->client->dev,
				"%s: cannot get ts pinctrl active state\n", __func__);
			return PTR_ERR(set_state);
		}
	} else {
		set_state =
			pinctrl_lookup_state(info->pinctrl,
						"off_irq");
		if (IS_ERR(set_state)) {
			input_err(true, &info->client->dev,
				"%s: cannot get gpiokey pinctrl sleep state\n", __func__);
			return PTR_ERR(set_state);
		}
	}
	retval = pinctrl_select_state(info->pinctrl, set_state);
	if (retval) {
		input_err(true, &info->client->dev,
			"%s: cannot set ts pinctrl active state\n", __func__);
		return retval;
	}

	return 0;
}


#ifdef CONFIG_OF
static int abov_parse_dt(struct device *dev,
			struct abov_touchkey_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int ret;

	// TO DO : Regulator get if any 
	
	ret = of_property_read_string(np, "abov,dvdd_vreg_name", (const char **)&pdata->dvdd_vreg_name);
	if (ret) {
		input_err(true, dev, "touchkey:failed to get abov,dvdd_vreg_name %d\n", ret);
		return ret;
	}

	pdata->gpio_int = of_get_named_gpio(np, "abov,irq_gpio", 0);
	if(pdata->gpio_int < 0){
		input_err(true, dev, "unable to get gpio_int\n");
		return pdata->gpio_int;
	} else {
		if (gpio_is_valid(pdata->gpio_int)) {
			ret = gpio_request(pdata->gpio_int, "tkey_gpio_int");
			if(ret < 0){
				input_err(true, dev,
					"unable to request gpio_int\n");
				return ret;
			}

			ret = gpio_direction_input(pdata->gpio_int);
			if (ret) {
				input_err(true, dev, "%s: unable to set direction for gpio [%d]\n",
						__func__, pdata->gpio_int);
			}
		}
	}

	ret = of_property_read_string(np, "abov,fw_path", (const char **)&pdata->fw_path);
	if (ret) {
		input_err(true, dev, "touchkey:failed to read fw_path %d\n", ret);
		pdata->fw_path = TK_FW_PATH_BIN;
	}
	of_property_read_u32(np, "abov,firmup_cmd", &pdata->firmup_cmd);
	input_info(true, dev, "%s: fw path %s gpio_int:%d firmup_cmd:0x%x\n", __func__, pdata->fw_path, pdata->gpio_int, pdata->firmup_cmd);

	return 0;
}
#else
static int abov_parse_dt(struct device *dev,
			struct abov_touchkey_platform_data *pdata)
{
	return -ENODEV;
}
#endif /* CONFIG_OF */

static int abov_tk_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct abov_tk_info *info;
	struct abov_touchkey_platform_data *pdata;
	struct input_dev *input_dev;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev,
			"i2c_check_functionality fail\n");
		return -EIO;
	}

	info = kzalloc(sizeof(struct abov_tk_info), GFP_KERNEL);
	if (!info) {
		input_err(true, &client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		input_err(true, &client->dev,
			"Failed to allocate memory for input device\n");
		ret = -ENOMEM;
		goto err_input_alloc;
	}

	info->client = client;
	info->input_dev = input_dev;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct abov_touchkey_platform_data), GFP_KERNEL);
		if (!pdata) {
			input_err(true, &client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_config;
		}

		ret = abov_parse_dt(&client->dev, pdata);
		if (ret){
			input_err(true, &client->dev, "failed to abov_parse_dt\n");
			ret = -ENOMEM;
			goto err_config;
		}

		info->pdata = pdata;
	} else
		info->pdata = client->dev.platform_data;

	if (info->pdata == NULL) {
		input_err(true, &client->dev, "failed to get platform data\n");
		goto err_config;
	}

	info->pdata->power = abov_power_regulator_ctrl;

	info->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(info->pinctrl)) {
		if (PTR_ERR(info->pinctrl) == -EPROBE_DEFER)
			goto err_config;

		input_err(true, &client->dev, "%s: Target does not use pinctrl\n", __func__);
		info->pinctrl = NULL;
	} else {
		ret = abov_pinctrl_configure(info, true);
		if (ret)
			input_err(true, &client->dev,
				"%s: cannot set ts pinctrl active state\n", __func__);
	}


	if (info->pdata->power)
		info->pdata->power(info, true);

	if(!info->pdata->boot_on_ldo)
		msleep(ABOV_RESET_DELAY);

	info->irq = -1;
	client->irq = gpio_to_irq(info->pdata->gpio_int);

	mutex_init(&info->lock);
	info->sec_touchkey_ws = wakeup_source_register(&info->client->dev, "touchkey");
	device_init_wakeup(&info->client->dev, true);

	init_completion(&info->resume_done);
	complete_all(&info->resume_done);

	info->input_event = info->pdata->input_event;
	info->touchkey_count = sizeof(touchkey_keycode) / sizeof(int);
	i2c_set_clientdata(client, info);

	ret = abov_tk_fw_check(info);
	if (ret) {
		input_err(true, &client->dev,
			"failed to firmware check (%d)\n", ret);
		goto err_reg_input_dev;
	}

	snprintf(info->phys, sizeof(info->phys),
		 "%s/input0", dev_name(&client->dev));
	input_dev->name = "sec_touchkey";
	input_dev->phys = info->phys;
	input_dev->id.bustype = BUS_HOST;
	input_dev->dev.parent = &client->dev;

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(KEY_VOLUMEDOWN, input_dev->keybit);
	set_bit(KEY_VOLUMEUP, input_dev->keybit);
	set_bit(KEY_PLAYPAUSE, input_dev->keybit);
	input_set_drvdata(input_dev, info);

	ret = input_register_device(input_dev);
	if (ret) {
		input_err(true, &client->dev, "failed to register input dev (%d)\n",
			ret);
		goto err_reg_input_dev;
	}

	if (!info->pdata->irq_flag) {
		input_err(true, &client->dev, "no irq_flag\n");
		ret = request_threaded_irq(client->irq, NULL, abov_tk_interrupt,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT, ABOV_TK_NAME, info);
	} else {
		ret = request_threaded_irq(client->irq, NULL, abov_tk_interrupt,
			info->pdata->irq_flag, ABOV_TK_NAME, info);
	}
	if (ret < 0) {
		input_err(true, &client->dev, "Failed to register interrupt\n");
		goto err_req_irq;
	}
	info->irq = client->irq;
	enable_irq_wake(info->irq);

	info->dev = sec_device_create(info, "sec_touchkey");
	if (IS_ERR(info->dev))
		input_err(true, &client->dev,
		"Failed to create device for the touchkey sysfs\n");

	ret = sysfs_create_group(&info->dev ->kobj,
		&sec_touchkey_attr_group);
	if (ret)
		input_err(true, &client->dev, "Failed to create sysfs group\n");

	ret = sysfs_create_link(&info->dev ->kobj,
		&info->input_dev->dev.kobj, "input");
	if (ret < 0) {
		input_err(true, &client->dev,
			"%s: Failed to create input symbolic link\n",
			__func__);
	}

	input_err(true, &client->dev, "%s done\n", __func__);
	return 0;

err_req_irq:
	input_unregister_device(input_dev);
err_reg_input_dev:
	wakeup_source_unregister(info->sec_touchkey_ws);
	mutex_destroy(&info->lock);
	gpio_free(info->pdata->gpio_int);
	if (info->pdata->power)
		info->pdata->power(info, false);
err_config:
	input_free_device(input_dev);
err_input_alloc:
	kfree(info);
err_alloc:
	input_err(true, &client->dev, "%s fail\n",__func__);
	return ret;

}

static int abov_tk_remove(struct i2c_client *client)
{
	struct abov_tk_info *info = i2c_get_clientdata(client);

	if (info->irq >= 0)
		free_irq(info->irq, info);
	input_unregister_device(info->input_dev);
	input_free_device(info->input_dev);

	device_init_wakeup(&info->client->dev, false);
	wakeup_source_unregister(info->sec_touchkey_ws);
	kfree(info);

	return 0;
}

static void abov_tk_shutdown(struct i2c_client *client)
{

	input_info(true, &client->dev, "Inside abov_tk_shutdown \n");
}

#if defined(CONFIG_PM)
static int abov_tk_suspend(struct device *dev)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);

	input_info(true, dev, "%s \n", __func__);
	reinit_completion(&info->resume_done);
	info->power_state = SEC_INPUT_STATE_LPM;

	return 0;
}

static int abov_tk_resume(struct device *dev)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);

	input_info(true, dev, "%s \n", __func__);
	complete_all(&info->resume_done);
	info->power_state = SEC_INPUT_STATE_LPM;

	return 0;
}
#endif

 
#if defined(CONFIG_PM)
static const struct dev_pm_ops abov_tk_pm_ops = {
	.suspend = abov_tk_suspend,
	.resume = abov_tk_resume,
};
#endif

static const struct i2c_device_id abov_tk_id[] = {
	{ABOV_TK_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, abov_tk_id);

#ifdef CONFIG_OF
static struct of_device_id abov_match_table[] = {
	{ .compatible = "abov,touchkey",},
	{ },
};
#else
#define abov_match_table NULL
#endif

static struct i2c_driver abov_tk_driver = {
	.probe = abov_tk_probe,
	.remove = abov_tk_remove,
	.shutdown = abov_tk_shutdown,
	.driver = {
		   .name = ABOV_TK_NAME,
		   .of_match_table = abov_match_table,		   
#if defined(CONFIG_PM)
		   .pm = &abov_tk_pm_ops,
#endif 
	},
	.id_table = abov_tk_id,
};

static int __init touchkey_init(void)
{
	return i2c_add_driver(&abov_tk_driver);
}

static void __exit touchkey_exit(void)
{
	i2c_del_driver(&abov_tk_driver);
}

module_init(touchkey_init);
module_exit(touchkey_exit);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Touchkey driver for Abov MF18xx chip");
MODULE_LICENSE("GPL");
