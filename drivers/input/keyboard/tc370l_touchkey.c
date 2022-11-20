/* tc370.c -- Linux driver for coreriver chip as touchkey
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
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/i2c/tc370l_touchkey.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#define CONFIG_SEC_DEBUG_TK_LOG
#endif

#ifdef CONFIG_SEC_DEBUG_TK_LOG
#include <mach/sec_debug.h>
#endif

extern unsigned int lpcharge;

#ifdef CONFIG_SEC_DEBUG_TK_LOG
#define tk_debug_dbg(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
})

#define tk_debug_info(mode, dev, fmt, ...)	\
({								\
	if (mode) {							\
		dev_info(dev, fmt, ## __VA_ARGS__);		\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_info(dev, fmt, ## __VA_ARGS__);	\
})

#define tk_debug_err(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_err(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);	\
	}				\
	else					\
		dev_err(dev, fmt, ## __VA_ARGS__); \
})
#else
#define tk_debug_dbg(mode, dev, fmt, ...)	dev_dbg(dev, fmt, ## __VA_ARGS__)
#define tk_debug_info(mode, dev, fmt, ...)	dev_info(dev, fmt, ## __VA_ARGS__)
#define tk_debug_err(mode, dev, fmt, ...)	dev_err(dev, fmt, ## __VA_ARGS__)
#endif

#define ISP_SPEED_UP
/*#define ISP_DEBUG*/

/* registers */
#define TC370_KEYCODE			0x00
#define TC370_FWVER			0x01
#define TC370_MDVER			0x02
#define TC370_MODE			0x03
#define TC370_1KEY_THRES_H	0x04
#define TC370_1KEY_CH_PCK_H	0x06
#define TC370_1KEY_DIFF_DATA	0x08
#define TC370_1KEY_RAW_DATA	0x0A
#define TC370_2KEY_THRES_H	0x0C
#define TC370_2KEY_CH_PCK_H	0x0E
#define TC370_2KEY_DIFF_DATA	0x10
#define TC370_2KEY_RAW_DATA	0x12

#define TC370_CH_PCK_H_OFFSET	0x00
#define TC370_CH_PCK_L_OFFSET	0x01

/*#define TC370_CHECKS_H 0x00*/
/*#define TC370_CHECKS_L 0x00*/

/* command */
#define TC370_CMD_ADDR			0x00
#define TC370_CMD_LED_ON		0x10
#define TC370_CMD_LED_OFF		0x20

/* mask */
#define TC370_KEY_INDEX_MASK	0x03
#define TC370_KEY_PRESS_MASK	0x08

/* firmware */
#define TC370_FW_PATH_SDCARD	"/sdcard/tc370.bin"

#define TK_UPDATE_PASS		0
#define TK_UPDATE_DOWN		1
#define TK_UPDATE_FAIL		2

/* ISP command */
#define TC370_ISP_ACK			1
#define TC370_ISP_SP_SIGNAL         0b010101011111000
#define TC370_NUM_OF_ISP_SP_SIGNAL	15
#define TC370_FW_ER_MAX_LEN         0x4000
#define TC370_FW_WT_MAX_LEN		0X3000

/* ISP delay */
#define TC370_POWERON_DELAY	200
#define TC370_DCR_RD_RETRY	50

enum {
	FW_INKERNEL,
	FW_SDCARD,
};

struct fw_image {
	u8 hdr_ver;
	u8 hdr_len;
	u16 first_fw_ver;
	u16 second_fw_ver;
	u16 third_ver;
	u32 fw_len;
	u8 data[0];
} __attribute__ ((packed));


struct tc370_data {
	struct device *sec_touchkey;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct tc370_platform_data *pdata;
	struct mutex lock;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct fw_image *fw_img;
	const struct firmware *fw;
	char phys[32];
	int irq;
	u16 threhold_recent;
	u16 threhold_back;
	int key_num;
	int *keycode;
	int mode;
	int (*power) (bool on);
	u8 fw_ver;
	u8 md_ver;
	u8 fw_update_status;
	bool enabled;
	bool fw_downloding;
	bool glove_mode;
	int	udelay;
	int	src_fw_ver;
};

extern struct class *sec_class;
static void release_all_fingers(struct tc370_data *data)
{
	struct i2c_client *client = data->client;
	int i;

	tk_debug_dbg(true, &client->dev, "[TK] %s\n", __func__);

	for (i = 1; i < data->key_num; i++) {
		input_report_key(data->input_dev,
			data->keycode[i], 0);
	}
	input_sync(data->input_dev);
}

static void tc370_reset(struct tc370_data *data)
{
	release_all_fingers(data);

	disable_irq(data->irq);
	data->pdata->keyled(false);
	data->pdata->power(false);

	msleep(50);

	data->pdata->power(true);
	data->pdata->keyled(true);
	msleep(50);

	enable_irq(data->irq);
	msleep(50);
}

static void tc370_reset_probe(struct tc370_data *data)
{
	data->pdata->keyled(false);
	data->pdata->power(false);

	msleep(50);

	data->pdata->power(true);
	data->pdata->keyled(true);
	msleep(100);
}

int get_fw_version(struct tc370_data *data, bool probe)
{
	struct i2c_client *client = data->client;
	int retry = 3;
	int buf;

	if ((!data->enabled) || data->fw_downloding)
		return -1;

	buf = i2c_smbus_read_byte_data(client, TC370_FWVER);
	if (buf < 0) {
		while (retry--) {
			tk_debug_err(true, &client->dev, "%s read fail(%d)\n",
				__func__, retry);
			if (probe)
				tc370_reset_probe(data);
			else
				tc370_reset(data);
			buf = i2c_smbus_read_byte_data(client, TC370_FWVER);
			if (buf > 0)
				break;
		}
		if (retry <= 0) {
			tk_debug_err(true, &client->dev, "%s read fail\n", __func__);
			data->fw_ver = 0;
			return -1;
		}
	}
	data->fw_ver = (u8)buf;
	tk_debug_info(true, &client->dev, "fw_ver : 0x%x\n", data->fw_ver);

	return 0;
}

static irqreturn_t tc370_interrupt(int irq, void *dev_id)
{
	struct tc370_data *data = dev_id;
	struct i2c_client *client = data->client;
	int ret, retry;
	u8 key_val, index;
	bool press;

	if ((!data->enabled) || data->fw_downloding)
		return IRQ_HANDLED;

	ret = i2c_smbus_read_byte_data(client, TC370_KEYCODE);
	if (ret < 0) {
		retry = 3;
		while (retry--) {
			tk_debug_err(true, &client->dev, "%s read fail ret=%d(retry:%d)\n",
				__func__, ret, retry);
			msleep(10);
			ret = i2c_smbus_read_byte_data(client, TC370_KEYCODE);
			if (ret > 0)
				break;
		}
		if (retry <= 0) {
			tc370_reset(data);
			return IRQ_HANDLED;
		}
	}
	key_val = (u8)ret;
	index = key_val & TC370_KEY_INDEX_MASK;
	press = !!(key_val & TC370_KEY_PRESS_MASK);

	if (press) {
		input_report_key(data->input_dev, data->keycode[index], 0);
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
		tk_debug_info(true, &client->dev, "key R\n");
#else
		tk_debug_info(true, &client->dev,
			"key R : %d(%d)\n", data->keycode[index], key_val);
#endif
	} else {
		input_report_key(data->input_dev, data->keycode[index], 1);
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
		tk_debug_info(true, &client->dev, "key P\n");
#else
		tk_debug_info(true, &client->dev,
			"key P : %d(%d)\n", data->keycode[index], key_val);
#endif
	}
	input_sync(data->input_dev);

	return IRQ_HANDLED;
}

static ssize_t tc370_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc370_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[2] = {0, };

	if ((!data->enabled) || data->fw_downloding) {
		tk_debug_err(true, &client->dev, "%s: device is disabled\n.", __func__);
		return -EPERM;
	}

	ret = i2c_smbus_read_i2c_block_data(client, TC370_1KEY_THRES_H, 2, buff);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s: failed to read threshold_h (%d)\n",
			__func__, ret);
		return ret;
	}

	data->threhold_recent = (buff[0] << 8) | buff[1];

	ret = i2c_smbus_read_i2c_block_data(client, TC370_2KEY_THRES_H, 2, buff);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s: failed to read threshold_h (%d)\n",
			__func__, ret);
		return ret;
	}

	data->threhold_back = (buff[0] << 8) | buff[1];

/*	return sprintf(buf, "%d, %d\n", data->threhold_recent, data->threhold_back);*/
	return sprintf(buf, "%d\n", data->threhold_recent);

}

static int touchled_cmd_reversed;
static int touchkey_led_status;

static ssize_t tc370_led_control(struct device *dev,
	 struct device_attribute *attr, const char *buf, size_t count)
{
	struct tc370_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int scan_buffer;
	int ret;
	u8 cmd = 0;

	ret = sscanf(buf, "%d", &scan_buffer);
	if (ret != 1) {
		tk_debug_err(true, &client->dev, "%s: cmd read err\n", __func__);
		return count;
	}

	if (!(scan_buffer == 0 || scan_buffer == 1)) {
		tk_debug_err(true, &client->dev, "%s: wrong command(%d)\n",
			__func__, scan_buffer);
		return count;
	}

	if (scan_buffer == 1) {
		tk_debug_info(true, &client->dev, "led on\n");
		cmd = TC370_CMD_LED_ON;
	} else {
		tk_debug_info(true, &client->dev, "led off\n");
		cmd = TC370_CMD_LED_OFF;
	}

	if ((!data->enabled) || data->fw_downloding) {
		touchled_cmd_reversed = 1;
		goto out;
	}

	ret = i2c_smbus_write_byte_data(client, TC370_CMD_ADDR, cmd);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s Error turn on led(%d)\n", __func__, ret);
		touchled_cmd_reversed = 1;
		goto out;
	}
	msleep(30);
out:
	touchkey_led_status = cmd;

	return count;
}

static int load_fw_in_kernel(struct tc370_data *data)
{
	struct i2c_client *client = data->client;
	int ret;

	ret = request_firmware(&data->fw, data->pdata->fw_name, &client->dev);
	if (ret) {
		tk_debug_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		return -1;
	}
	data->fw_img = (struct fw_image *)data->fw->data;

	tk_debug_info(true, &client->dev, "0x%x firm (size=%d)\n",
		data->fw_img->first_fw_ver, data->fw_img->fw_len);
	tk_debug_info(true, &client->dev, "%s done\n", __func__);

	return 0;
}

static int load_fw_sdcard(struct tc370_data *data)
{
	struct i2c_client *client = data->client;
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;

	old_fs = get_fs();
	set_fs(get_ds());
	fp = filp_open(TC370_FW_PATH_SDCARD, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		tk_debug_err(true, &client->dev, "%s %s open error\n",
			__func__, TC370_FW_PATH_SDCARD);
		ret = -ENOENT;
		goto fail_sdcard_open;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;

	data->fw_img = kzalloc((size_t)fsize, GFP_KERNEL);
	if (!data->fw_img) {
		tk_debug_err(true, &client->dev, "%s fail to kzalloc for fw\n", __func__);
		filp_close(fp, current->files);
		ret = -ENOMEM;
		goto fail_sdcard_kzalloc;
	}

	nread = vfs_read(fp, (char __user *)data->fw_img, fsize, &fp->f_pos);
	if (nread != fsize) {
		tk_debug_err(true, &client->dev,
				"%s fail to vfs_read file\n", __func__);
		ret = -EINVAL;
		goto fail_sdcard_size;
	}
	filp_close(fp, current->files);
	set_fs(old_fs);

	tk_debug_info(true, &client->dev, "fw_size : %lu\n", nread);
	tk_debug_info(true, &client->dev, "%s done\n", __func__);

	return ret;

fail_sdcard_size:
	kfree(&data->fw_img);
fail_sdcard_kzalloc:
	filp_close(fp, current->files);
fail_sdcard_open:
	set_fs(old_fs);

	return ret;
}

static inline void setsda(struct tc370_data *data, int state)
{
	if (state)
		gpio_direction_input(data->pdata->gpio_sda);
	else
		gpio_direction_output(data->pdata->gpio_sda, 0);
}

static inline void setscl(struct tc370_data *data, int state)
{
	if (state)
		gpio_direction_input(data->pdata->gpio_scl);
	else
		gpio_direction_output(data->pdata->gpio_scl, 0);
}

static inline int getsda(struct tc370_data *data)
{
	return gpio_get_value(data->pdata->gpio_sda);
}

static inline int getscl(struct tc370_data *data)
{
	return gpio_get_value(data->pdata->gpio_scl);
}

static inline void sdalo(struct tc370_data *data)
{
	setsda(data, 0);
	udelay((data->udelay + 1) / 2);
}

static inline void sdahi(struct tc370_data *data)
{
	setsda(data, 1);
	udelay((data->udelay + 1) / 2);
}

static inline void scllo(struct tc370_data *data)
{
	setscl(data, 0);
	udelay((data->udelay + 1) / 2);
}

static int sclhi(struct tc370_data *data)
{
	int i;

	setscl(data, 1);

	for (i = 0 ; i < 20; ++i) {
		if (getscl(data))
			break;
		udelay(10);
	}

	udelay(data->udelay);

	return 0;
}

static void isp_start(struct tc370_data *data)
{
	setsda(data, 0);
	udelay(data->udelay);
	scllo(data);
}

static void isp_stop(struct tc370_data *data)
{
	sdalo(data);
	sclhi(data);
	setsda(data, 1);
	udelay(data->udelay);
}

static int isp_recvdbyte(struct tc370_data *data)
{
	struct i2c_client *client = data->client;
	int i;
	u8 indata = 0;
	sdahi(data);
	for (i = 0; i < 8 ; i++) {
		if (sclhi(data) < 0) { /* timed out */
			tk_debug_err(true, &client->dev, "%s: timeout at bit "
				"#%d\n", __func__, 7 - i);
			return -ETIMEDOUT;
		}

		indata = indata << 1;
		if (getsda(data))
			indata |= 0x01;

		setscl(data, 0);

		udelay(i == 7 ? data->udelay / 2 : data->udelay);
	}
	return indata;
}

static int isp_sendbyte(struct tc370_data *data, u8 c)
{
	struct i2c_client *client = data->client;
	int i;
	int sb;
	int ack = 0;

	/* assert: scl is low */
	for (i = 7; i >= 0; i--) {
		sb = (c >> i) & 0x1;
		setsda(data, sb);
		udelay((data->udelay + 1) / 2);

		if (sclhi(data) < 0) { /* timed out */
			tk_debug_err(true, &client->dev, "%s: %#x, timeout at bit #%d\n",
				__func__, (int)c, i);
			return -ETIMEDOUT;
		}
		scllo(data);
	}
	sdahi(data);

	if (sclhi(data) < 0) { /* timed out */
		tk_debug_err(true, &client->dev, "%s: %#x, timeout at bit #%d\n",
			__func__, (int)c, i);
		return -ETIMEDOUT;
	}

	ack = !getsda(data);

	scllo(data);

#if defined(ISP_VERY_VERBOSE_DEBUG)
	tk_debug_info(true, &client->dev, "%s: %#x %s\n", __func__, (int)c,
		 ack ? "A" : "NA");
#endif
	return ack;
}

static int isp_master_recv(struct tc370_data *data, u8 addr, u8 *val)
{
	struct i2c_client *client = data->client;
	int ret;
	int retries = 2;

retry:
	isp_start(data);

	ret = isp_sendbyte(data, addr);
	if (ret != TC370_ISP_ACK) {
		tk_debug_err(true, &client->dev, "%s: %#x %s\n", __func__, addr, "NA");
		if (retries-- > 0) {
			tk_debug_err(true, &client->dev, "%s: retry (%d)\n", __func__,
				retries);
			goto retry;
		}
		return -EIO;
	}
	*val = isp_recvdbyte(data);
	isp_stop(data);

	return 0;
}

static int isp_master_send(struct tc370_data *data, u8 msg_1, u8 msg_2)
{
	struct i2c_client *client = data->client;
	int ret;
	int retries = 2;

retry:
	isp_start(data);
	ret = isp_sendbyte(data, msg_1);
	if (ret != TC370_ISP_ACK) {
		tk_debug_err(true, &client->dev, "%s: %#x %s\n", __func__, msg_1, "NA");
		if (retries-- > 0) {
			tk_debug_err(true, &client->dev, "%s: retry (%d)\n", __func__,
				retries);
			goto retry;
		}
		return -EIO;
	}
	ret = isp_sendbyte(data, msg_2);
	if (ret != TC370_ISP_ACK) {
		tk_debug_err(true, &client->dev, "%s: %#x %s\n", __func__, msg_2, "NA");
		if (retries-- > 0) {
			tk_debug_err(true, &client->dev, "%s: retry (%d)\n", __func__,
				retries);
			goto retry;
		}
		return -EIO;
	}
	isp_stop(data);

	return 0;
}
static void isp_sp_signal(struct tc370_data *data)
{
	int i;
	unsigned long flags;

	local_irq_save(flags);
	for (i = TC370_NUM_OF_ISP_SP_SIGNAL - 1; i >= 0; i--) {
		int sb = (TC370_ISP_SP_SIGNAL >> i) & 0x1;
		setscl(data, sb);
		udelay(3);
		setsda(data, 0);
		udelay(10);
		setsda(data, 1);
		udelay(10);

		if (i == 5)
			udelay(30);
	}

	sclhi(data);
	local_irq_restore(flags);
}

static int raw_dbgir3(struct tc370_data *data, u8 data2, u8 data1, u8 data0)
{
	struct i2c_client *client = data->client;
	int ret = 0;

	ret = ret | isp_master_send(data, 0xc2, data2);
	if (ret < 0)
		goto err_isp_master_send;
	ret = ret | isp_master_send(data, 0xc4, data1);
	if (ret < 0)
		goto err_isp_master_send;
	ret = ret | isp_master_send(data, 0xc6, data0);
	if (ret < 0)
		goto err_isp_master_send;
	ret = ret | isp_master_send(data, 0xc0, 0x80);
	if (ret < 0)
		goto err_isp_master_send;

	return 0;
err_isp_master_send:
	tk_debug_err(true, &client->dev, "fail to dbgir3 %#x,%#x,%#x (%d)\n",
		data2, data1, data0, ret);
	return ret;
}

static int raw_dbgir2(struct tc370_data *data, u8 data1, u8 data0)
{
	struct i2c_client *client = data->client;
	int ret = 0;

	ret = ret | isp_master_send(data, 0xc2, data1);
	if (ret < 0)
		goto err_raw_dbir2;
	ret = ret | isp_master_send(data, 0xc4, data0);
	if (ret < 0)
		goto err_raw_dbir2;
	ret = ret | isp_master_send(data, 0xc0, 0x80);
	if (ret < 0)
		goto err_raw_dbir2;

	return 0;
err_raw_dbir2:
	tk_debug_err(true, &client->dev, "fail to dbgir2 %#x,%#x (%d)\n",
		data1, data0, ret);
	return ret;
}

static int raw_spchl(struct tc370_data *data, u8 data1, u8 data0)
{
	struct i2c_client *client = data->client;
	int ret = 0;

	ret = ret | isp_master_send(data, 0xd2, data0);
	ret = ret | isp_master_send(data, 0xd0, data1);

	if (ret < 0) {
		tk_debug_err(true, &client->dev, "fail to spchl %#x,%#x (%d)\n",
			data1, data0, ret);
		return ret;
	}

	return 0;
}
static int isp_common_set(struct tc370_data *data)
{
	struct i2c_client *client = data->client;
	int ret = 0;

	ret = ret | raw_dbgir3(data, 0x75 , 0x8f, 0x00);
	if (ret < 0)
		goto err_write_common_set;
	ret = ret | raw_dbgir3(data, 0x75 , 0xc6, 0x0e);
	if (ret < 0)
		goto err_write_common_set;
	ret = ret | raw_dbgir3(data, 0x75 , 0xf7, 0xc1);
	if (ret < 0)
		goto err_write_common_set;
	ret = ret | raw_dbgir3(data, 0x75 , 0xf7, 0x1e);
	if (ret < 0)
		goto err_write_common_set;
	ret = ret | raw_dbgir3(data, 0x75 , 0xf7, 0xec);
	if (ret < 0)
		goto err_write_common_set;
	ret = ret | raw_dbgir3(data, 0x75 , 0xf7, 0x81);
	if (ret < 0)
		goto err_write_common_set;

	return 0;

err_write_common_set:
	tk_debug_err(true, &client->dev, "fail to %s (%d)\n", __func__, ret);
	return ret;
}

static int isp_ers_timing_set(struct tc370_data *data)
{
	struct i2c_client *client = data->client;
	int ret = 0;

	ret = ret | raw_dbgir3(data, 0x75, 0xf2, 0x90);
	if (ret < 0)
		goto err_ers_timing_set;
	ret = ret | raw_dbgir3(data, 0x75, 0xf3, 0xd0);
	if (ret < 0)
		goto err_ers_timing_set;
	ret = ret | raw_dbgir3(data, 0x75, 0xf4, 0x03);
	if (ret < 0)
		goto err_ers_timing_set;
//	ret = ret | raw_dbgir3(data, 0x75, 0xf1, 0x80);
//	if (ret < 0)
//		goto err_ers_timing_set;

	return 0;
err_ers_timing_set:
	tk_debug_err(true, &client->dev, "fail to %s (%d)\n", __func__, ret);
	return ret;
}

static int isp_pgm_timing_set(struct tc370_data *data)
{
	struct i2c_client *client = data->client;
	int ret = 0;

	ret = ret | raw_dbgir3(data, 0x75, 0xf2, 0x94);
	if (ret < 0)
		goto err_pgm_timing_set;
	ret = ret | raw_dbgir3(data, 0x75, 0xf3, 0x01);
	if (ret < 0)
		goto err_pgm_timing_set;
	ret = ret | raw_dbgir3(data, 0x75, 0xf4, 0x00);
	if (ret < 0)
		goto err_pgm_timing_set;

	return 0;
err_pgm_timing_set:
	tk_debug_err(true, &client->dev, "fail to %s (%d)\n", __func__, ret);
	return ret;
}

static void tc370_reset_for_isp(struct tc370_data *data, bool start)
{
	if (start) {
		data->pdata->keyled(false);
		data->pdata->power_isp(false);

		gpio_direction_output(data->pdata->gpio_scl, 0);
		gpio_direction_output(data->pdata->gpio_sda, 0);
		gpio_direction_output(data->pdata->gpio_int, 0);

		msleep(TC370_POWERON_DELAY);

		gpio_direction_output(data->pdata->gpio_scl, 1);
		gpio_direction_output(data->pdata->gpio_sda, 1);
		gpio_direction_input(data->pdata->gpio_int);

		data->pdata->power_isp(true);
		data->pdata->keyled(true);

		msleep(20);
	} else {
		data->pdata->keyled(false);
		data->pdata->power_isp(false);

		msleep(TC370_POWERON_DELAY);

		data->pdata->power(true);
		data->pdata->keyled(true);

		gpio_direction_input(data->pdata->gpio_sda);
		gpio_direction_input(data->pdata->gpio_scl);

		msleep(TC370_POWERON_DELAY);
	}
}

static int tc370_erase_fw(struct tc370_data *data)
{
	struct i2c_client *client = data->client;
	int ret;
	u16 addr = 0;
	int dcr_rd_cnt;
	u8 val;

	tc370_reset_for_isp(data, true);

	isp_sp_signal(data);
	ret = isp_common_set(data);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "fail to %s (%d)\n", __func__, ret);
		return ret;
	}

	ret = isp_ers_timing_set(data);
	isp_master_send(data, 0xf8, 0x01);
	isp_master_send(data, 0xc8, 0xff);
	isp_master_send(data, 0xca, 0x42);

	while (addr < TC370_FW_ER_MAX_LEN) {
#if defined(ISP_DEBUG)
		tk_debug_info(true, &client->dev, "fw erase addr=x0%4x\n", addr);
#endif
		raw_dbgir3(data, 0x75, 0xf1, 0x80);
		raw_dbgir3(data, 0x90, (u8)(addr >> 8), (u8)(addr & 0xff));

		raw_spchl(data, 0xff, 0x3a);
		isp_master_send(data, 0xc0, 0x14);

		val = 0;
		dcr_rd_cnt = TC370_DCR_RD_RETRY;
		do {
			isp_master_recv(data, 0xc1, &val);
			if (dcr_rd_cnt-- < 0) {
				tk_debug_err(true, &client->dev, "%s: fail to update "
					"dcr\n", __func__);
				return -ENOSYS;
			}
			usleep_range(10000, 15000);
		} while (val != 0x12);
#if defined(ISP_VERBOSE_DEBUG)
			tk_debug_info(true, &client->dev, "dcr_rd_cnt=%d\n", dcr_rd_cnt);
#endif
		addr += 0x400;
	}

	return 0;

}

static int tc370_write_fw(struct tc370_data *data)
{
	struct i2c_client *client = data->client;
	u16 addr = 0;
//	int dcr_rd_cnt;
	u8 val;
	u8 __fw_data;

	tc370_reset_for_isp(data, true);

	isp_sp_signal(data);
	isp_common_set(data);

	tk_debug_info(true, &client->dev, "%#x %#x\n", addr, data->fw_img->fw_len);
	isp_pgm_timing_set(data);
	isp_master_send(data, 0xf8, 0x01);
	isp_master_send(data, 0xc8, 0xff);
	isp_master_send(data, 0xca, 0x20);

	raw_dbgir3(data, 0x90, (u8)(addr >> 8), (u8)(addr & 0xff));
	raw_spchl(data, 0xff, 0x1e);

	while (addr < data->fw_img->fw_len) {
#if defined(ISP_DEBUG)
		tk_debug_info(true, &client->dev, "fw write addr=%#x\n", addr);
#endif
		__fw_data = data->fw_img->data[addr];
		raw_dbgir2(data, 0x74, __fw_data);
		raw_dbgir3(data, 0x75, 0xf1, 0x80);
		isp_master_send(data, 0xc0, 0x14);

#if !defined(ISP_SPEED_UP)
		val = 0;
		dcr_rd_cnt = TC370_DCR_RD_RETRY;
		do {
			isp_master_recv(data, 0xc1, &val);
			if (dcr_rd_cnt-- < 0) {
				tk_debug_err(true, &client->dev, "%s: fail to "
					"update dcr\n", __func__);
				return -ENOSYS;
			}
			usleep_range(900, 1000);
		} while (val != 0x12);
#endif
		isp_master_recv(data, 0xd9, &val);

		if (data->fw_img->data[addr] != val) {
			tk_debug_err(true, &client->dev, "fail to verify at %#x (%#x)\n",
				addr, data->fw_img->data[addr]);
			return -EIO;
		}
#if defined(ISP_VERBOSE_DEBUG)
		tk_debug_info(true, &client->dev, "dcr_rd_cnt=%d\n", dcr_rd_cnt);
#endif
		/* increase address */
		isp_master_send(data, 0xc0, 0x08);
		addr++;
	}

	return 0;

}

/*static int tc370_verify_fw(struct tc370_data *data)
{
	struct i2c_client *client = data->client;
	u16 addr = 0;
//	int dcr_rd_cnt;
	u8 val;

	tc370_reset_for_isp(data, true);

	isp_sp_signal(data);
	isp_common_set(data);

	isp_master_send(data, 0xf8, 0x01);
	isp_master_send(data, 0xc8, 0xff);
	isp_master_send(data, 0xca, 0x2a);
	raw_dbgir3(data, 0x90, (u8)(addr >> 8), (u8)(addr & 0xff));
	raw_spchl(data, 0xff, 0x28);

	while (addr < data->fw_img->fw_len) {

#if defined(ISP_DEBUG)
		tk_debug_info(true, &client->dev, "fw read addr=%#x\n", addr);
#endif

		do {
			isp_master_send(data, 0xc0, 0x14);

#if !defined(ISP_SPEED_UP)
			val = 0;
			dcr_rd_cnt = TC370_DCR_RD_RETRY;
			do {
				isp_master_recv(data, 0xc1, &val);
				if (dcr_rd_cnt-- < 0) {
					tk_debug_err(true, &client->dev, "%s: fail to "
						"update dcr\n", __func__);
					return -ENOSYS;
				}
				usleep_range(900, 1000);
			} while (val != 0x12);
#endif
#if defined(ISP_VERBOSE_DEBUG)
			tk_debug_info(true, &client->dev, "dcr_rd_cnt=%d\n", dcr_rd_cnt);
#endif
			isp_master_send(data, 0xc0, 0x08);
			isp_master_recv(data, 0xd9, &val);

			if (data->fw_img->data[addr] != val) {
				tk_debug_err(true, &client->dev, "fail to verify at "
					"%#x (%#x)\n", addr,
					data->fw_img->data[addr]);
				return -EIO;
			}
			addr++;
		} while (addr % 0x20);
	}

	return 0;

}*/

static void tc370_release_fw(struct tc370_data *data, u8 fw_path)
{
	if (fw_path == FW_INKERNEL)
		release_firmware(data->fw);
	else if (fw_path == FW_SDCARD)
		kfree(data->fw_img);
}

static int tc370_flash_fw(struct tc370_data *data, u8 fw_path)
{
	struct i2c_client *client = data->client;
	int retry = 5;
	int ret;

	tk_debug_info(true, &client->dev, "tc370_flash_fw\n");

	do {
		ret = tc370_erase_fw(data);
		if (ret)
			tk_debug_err(true, &client->dev, "%s erase fail(retry=%d)\n",
				__func__, retry);
		else
			break;
	} while (retry-- > 0);
	if (retry < 0)
		goto err_tc370_flash_fw;

	retry = 5;
	do {
		ret = tc370_write_fw(data);

//		ret = tc370_verify_fw(data);
		if (ret)
			tk_debug_err(true, &client->dev, "%s verify fail(retry=%d)\n",
				__func__, retry);
		else
			break;
	} while (retry-- > 0);

	tc370_reset_for_isp(data, false);

	if (retry < 0)
		goto err_tc370_flash_fw;

	return 0;

err_tc370_flash_fw:
	return -1;
}

static int tc370_fw_update(struct tc370_data *data, u8 fw_path, bool force)
{
	struct i2c_client *client = data->client;
	int retry = 4;
	int ret;

	if (fw_path == FW_INKERNEL) {
		if (!force) {
			ret = get_fw_version(data, false);
			if (ret)
				return -1;
		}

		ret = load_fw_in_kernel(data);
		if (ret)
			return -1;
		data->src_fw_ver = data->fw_img->first_fw_ver;

		if (!force && (data->fw_ver >= data->fw_img->first_fw_ver) && (data->fw_ver != 0xFF)) {
			tk_debug_info(true, &client->dev, "do not need firm update (0x%x, 0x%x)\n",
				data->fw_ver, data->fw_img->first_fw_ver);
			tc370_release_fw(data, fw_path);
			return 0;
		}
	} else if (fw_path == FW_SDCARD) {
		ret = load_fw_sdcard(data);
		if (ret)
			return -1;
	}

	while (retry--) {
		data->fw_downloding = true;
		ret = tc370_flash_fw(data, fw_path);
		data->fw_downloding = false;
		if (ret) {
			tk_debug_err(true, &client->dev, "%s tc370_flash_fw fail (%d)\n",
				__func__, retry);
			continue;
		}

		ret = get_fw_version(data, false);
		if (ret) {
			tk_debug_err(true, &client->dev, "%s get_fw_version fail (%d)\n",
				__func__, retry);
			continue;
		}
		if ((data->fw_ver != data->fw_img->first_fw_ver) || (data->fw_ver == 0xFF)) {
			tk_debug_err(true, &client->dev, "%s fw version fail (0x%x, 0x%x)(%d)\n",
				__func__, data->fw_ver, data->fw_img->first_fw_ver, retry);
			continue;
		}
		break;
	}

	if (retry > 0)
		tk_debug_info(true, &client->dev, "%s success\n", __func__);

	tc370_release_fw(data, fw_path);

	return ret;
}

static ssize_t tc370_update_store(struct device *dev,
	 struct device_attribute *attr, const char *buf, size_t count)
{
	struct tc370_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 fw_path;

	switch(*buf) {
	case 's':
	case 'S':
		fw_path = FW_INKERNEL;
		break;
	case 'i':
	case 'I':
		fw_path = FW_SDCARD;
		break;
	default:
		tk_debug_err(true, &client->dev, "%s wrong command fail\n", __func__);
		data->fw_update_status = TK_UPDATE_FAIL;
		return count;
	}

	data->fw_update_status = TK_UPDATE_DOWN;

	disable_irq(data->irq);
	ret = tc370_fw_update(data, fw_path, false);
	enable_irq(data->irq);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s fail\n", __func__);
		data->fw_update_status = TK_UPDATE_FAIL;
	} else
		data->fw_update_status = TK_UPDATE_PASS;

	return count;
}

static ssize_t tc370_firm_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc370_data *data = dev_get_drvdata(dev);
	int ret;

	if (data->fw_update_status == TK_UPDATE_PASS)
		ret = sprintf(buf, "PASS\n");
	else if (data->fw_update_status == TK_UPDATE_DOWN)
		ret = sprintf(buf, "DOWNLOADING\n");
	else if (data->fw_update_status == TK_UPDATE_FAIL)
		ret = sprintf(buf, "FAIL\n");
	else
		ret = sprintf(buf, "NG\n");

	return ret;
}

static ssize_t tc370_firm_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc370_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "0x%02x\n", data->src_fw_ver);
}

static ssize_t tc370_firm_version_read_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc370_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;

	ret = get_fw_version(data, false);
	if (ret < 0)
		tk_debug_err(true, &client->dev, "%s: failed to read firmware version (%d)\n",
			__func__, ret);

	return sprintf(buf, "0x%02x\n", data->fw_ver);
}

static ssize_t recent_key_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc370_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[2];
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		tk_debug_err(true, &client->dev, "can't excute %s\n", __func__);
		return -1;
	}

	ret = i2c_smbus_read_i2c_block_data(client, TC370_1KEY_CH_PCK_H, 2, buff);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s read fail(%d)\n", __func__, ret);
		return -1;
	}
	value = (buff[TC370_CH_PCK_H_OFFSET] << 8) |
		buff[TC370_CH_PCK_L_OFFSET];

	return sprintf(buf, "%d\n", value);
}

static ssize_t back_key_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc370_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[2];
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		tk_debug_err(true, &client->dev, "can't excute %s\n", __func__);
		return -1;
	}

	ret = i2c_smbus_read_i2c_block_data(client, TC370_2KEY_CH_PCK_H, 2, buff);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s read fail(%d)\n", __func__, ret);
		return -1;
	}
	value = (buff[TC370_CH_PCK_H_OFFSET] << 8) |
		buff[TC370_CH_PCK_L_OFFSET];

	return sprintf(buf, "%d\n", value);
}

static ssize_t recent_key_raw(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc370_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[2];
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		tk_debug_err(true, &client->dev, "can't excute %s\n", __func__);
		return -1;
	}

	ret = i2c_smbus_read_i2c_block_data(client, TC370_1KEY_RAW_DATA, 2, buff);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s read fail(%d)\n", __func__, ret);
		return -1;
	}
	value = (buff[0] << 8) | buff[1];

	return sprintf(buf, "%d\n", value);
}

static ssize_t back_key_raw(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tc370_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int ret;
	u8 buff[2];
	int value;

	if ((!data->enabled) || data->fw_downloding) {
		tk_debug_err(true, &client->dev, "can't excute %s\n", __func__);
		return -1;
	}

	ret = i2c_smbus_read_i2c_block_data(client, TC370_2KEY_RAW_DATA, 2, buff);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s read fail(%d)\n", __func__, ret);
		return -1;
	}
	value = (buff[0] << 8) | buff[1];

	return sprintf(buf, "%d\n", value);
}

static DEVICE_ATTR(touchkey_threshold, S_IRUGO, tc370_threshold_show, NULL);
static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		tc370_led_control);
static DEVICE_ATTR(touchkey_firm_update, S_IRUGO | S_IWUSR | S_IWGRP,
		NULL, tc370_update_store);
static DEVICE_ATTR(touchkey_firm_update_status, S_IRUGO,
		tc370_firm_status_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO,
		tc370_firm_version_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO,
		tc370_firm_version_read_show, NULL);
static DEVICE_ATTR(touchkey_recent, S_IRUGO, recent_key_show, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, back_key_show, NULL);
static DEVICE_ATTR(touchkey_raw_data3, S_IRUGO, recent_key_raw, NULL);
static DEVICE_ATTR(touchkey_raw_data1, S_IRUGO, back_key_raw, NULL);

static struct attribute *sec_touchkey_attributes[] = {
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_brightness.attr,
	&dev_attr_touchkey_firm_update.attr,
	&dev_attr_touchkey_firm_update_status.attr,
	&dev_attr_touchkey_firm_version_phone.attr,
	&dev_attr_touchkey_firm_version_panel.attr,
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_raw_data3.attr,
	&dev_attr_touchkey_raw_data1.attr,
	NULL,
};

static struct attribute_group sec_touchkey_attr_group = {
	.attrs = sec_touchkey_attributes,
};

static int tc370_fw_check(struct tc370_data *data)
{
	struct i2c_client *client = data->client;
	int ret;
	bool update = false;

	ret = get_fw_version(data, true);
	if (ret < 0) {
		if (data->pdata->panel_connect) {
			/* tsp connect check */
			tk_debug_err(true, &client->dev,
				"%s: i2c fail. but lcd connected\n",
				__func__);
			tk_debug_err(true, &client->dev,
				"excute firm update\n");
			update = true;
		} else {
			tk_debug_err(true, &client->dev,
				"%s: i2c fail...[%d], addr[%d]\n",
				__func__, ret, data->client->addr);
			tk_debug_err(true, &client->dev,
				"%s: touchkey driver unload\n", __func__);
			return ret;
		}
	}

	ret = tc370_fw_update(data, FW_INKERNEL, update);
	if (ret)
		return -1;

	return 0;
}

static int tc370_stop(struct tc370_data *data)
{
	mutex_lock(&data->lock);

	if (!data->enabled) {
		tk_debug_err(true, &data->client->dev, "Touch key already disabled\n");
		goto err_stop_out;
	}

	tk_debug_info(true, &data->client->dev, "%s\n", __func__);

	disable_irq(data->irq);
	data->enabled = false;
	release_all_fingers(data);
	data->pdata->power(false);
	data->pdata->keyled(false);

err_stop_out:
	mutex_unlock(&data->lock);
	return 0;
}

static int tc370_start(struct tc370_data *data)
{
	mutex_lock(&data->lock);

	if (data->enabled) {
		tk_debug_err(true, &data->client->dev, "Touch key already enabled\n");
		goto err_start_out;
	}

	tk_debug_info(true, &data->client->dev, "%s\n", __func__);

	data->enabled = true;
	data->pdata->keyled(true);
	data->pdata->power(true);
	msleep(200);

	if (touchled_cmd_reversed) {
		touchled_cmd_reversed = 0;
		i2c_smbus_write_byte_data(data->client, TC370_CMD_ADDR, touchkey_led_status);
		tk_debug_err(true, &data->client->dev, "%s: Turning LED is reserved %d\n", __func__,touchkey_led_status);
		msleep(30);
	}
	enable_irq(data->irq);

err_start_out:
	mutex_unlock(&data->lock);

	return 0;
}

static void tc370_input_close(struct input_dev *dev)
{
	struct tc370_data *data = input_get_drvdata(dev);

	tk_debug_dbg(true, &data->client->dev, "%s: users=%d\n", __func__,
		   data->input_dev->users);

	tc370_stop(data);
}

static int tc370_input_open(struct input_dev *dev)
{
	struct tc370_data *data = input_get_drvdata(dev);

	tk_debug_dbg(true, &data->client->dev, "%s: users=%d\n", __func__,
		   data->input_dev->users);

	tc370_start(data);

	return 0;
}

static int __devinit tc370_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct input_dev *input_dev;
	struct tc370_data *data;
	int ret;
	int i;

	if (lpcharge == 1) {
		tk_debug_err(true, &client->dev, "%s : Do not load driver due to : lpm %d\n",
			 __func__, lpcharge);
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		tk_debug_err(true, &client->dev,
			"i2c_check_functionality fail\n");
		return -EIO;
	}

	data = kzalloc(sizeof(struct tc370_data), GFP_KERNEL);
	if (!data) {
		tk_debug_err(true, &client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc_data;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		tk_debug_err(true, &client->dev,
			"Failed to allocate memory for input device\n");
		ret = -ENOMEM;
		goto err_alloc_input;
	}

	data->client = client;
	data->input_dev = input_dev;
	data->pdata = client->dev.platform_data;
	if (data->pdata == NULL) {
		tk_debug_err(true, &client->dev, "failed to get platform data\n");
		ret = -EINVAL;
		goto err_platform_data;
	}
	data->irq = -1;
	mutex_init(&data->lock);

	data->key_num = data->pdata->key_num;
	tk_debug_info(true, &client->dev, "number of keys = %d\n", data->key_num);
	data->keycode = data->pdata->keycode;
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	for (i = 1; i < data->key_num; i++)
		tk_debug_info(true, &client->dev, "keycode[%d]= %3d\n", i, data->keycode[i]);
#endif
	i2c_set_clientdata(client, data);

	data->pdata->keyled(true);
	data->pdata->power(true);
	data->enabled = true;

	msleep(TC370_POWERON_DELAY);

	ret = tc370_fw_check(data);
	if (ret) {
		tk_debug_err(true, &client->dev,
			"failed to firmware check(%d)\n", ret);
		goto err_fw_check;
	}

	snprintf(data->phys, sizeof(data->phys),
		"%s/input0", dev_name(&client->dev));
	input_dev->name = "sec_touchkey";
	input_dev->phys = data->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->open = tc370_input_open;
	input_dev->close = tc370_input_close;
	data->udelay = data->pdata->udelay;

	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_LED, input_dev->evbit);
	set_bit(LED_MISC, input_dev->ledbit);
	for (i = 1; i < data->key_num; i++)
		set_bit(data->keycode[i], input_dev->keybit);
	input_set_drvdata(input_dev, data);

	ret = input_register_device(input_dev);
	if (ret) {
		tk_debug_err(true, &client->dev, "fail to register input_dev (%d)\n",
			ret);
		goto err_register_input_dev;
	}

	ret = request_threaded_irq(client->irq, NULL, tc370_interrupt,
				IRQF_TRIGGER_FALLING, TC370_NAME, data);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "fail to request irq (%d).\n",
			client->irq);
		goto err_request_irq;
	}
	data->irq = client->irq;

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = tc370_early_suspend;
	data->early_suspend.resume = tc370_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	data->sec_touchkey = device_create(sec_class,
		NULL, 0, data, "sec_touchkey");
	if (IS_ERR(data->sec_touchkey))
		tk_debug_err(true, &client->dev,
			"Failed to create device for the touchkey sysfs\n");

	ret = sysfs_create_group(&data->sec_touchkey->kobj,
		&sec_touchkey_attr_group);
	if (ret)
		tk_debug_err(true, &client->dev, "Failed to create sysfs group\n");
	
	ret = sysfs_create_link(&data->sec_touchkey->kobj,
		&data->input_dev->dev.kobj, "input");
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "Failed to create symbolic link\n");
	}
	tk_debug_info(true, &client->dev, "%s done\n", __func__);
	return 0;

err_request_irq:
	input_unregister_device(input_dev);
err_register_input_dev:
err_fw_check:
	mutex_destroy(&data->lock);
	data->pdata->keyled(false);
	data->pdata->power(false);
err_platform_data:
	input_free_device(input_dev);
err_alloc_input:
	kfree(data);
err_alloc_data:
	return ret;
}

static int __devexit tc370_remove(struct i2c_client *client)
{
	struct tc370_data *data = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	free_irq(data->irq, data);
	input_unregister_device(data->input_dev);
	input_free_device(data->input_dev);
	mutex_destroy(&data->lock);
	data->pdata->keyled(false);
	data->pdata->power(false);
	gpio_free(data->pdata->gpio_int);
	gpio_free(data->pdata->gpio_sda);
	gpio_free(data->pdata->gpio_scl);
	kfree(data);

	return 0;
}

static void tc370_shutdown(struct i2c_client *client)
{
	struct tc370_data *data = i2c_get_clientdata(client);

	data->pdata->keyled(false);
	data->pdata->power(false);
}

#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND
#define touchkey_suspend	NULL
#define touchkey_resume	NULL

static void tc370_early_suspend(struct early_suspend *h)
{
	struct tc370_data *data;
	data = container_of(h, struct tc370_data, early_suspend);
	tc370_stop(data);
}

static void tc370_late_resume(struct early_suspend *h)
{
	struct tc370_data *data;
	data = container_of(h, struct tc370_data, early_suspend);
	tc370_start(data);
}
#else
static int tc370_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tc370_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->input_dev->mutex);

	if (data->input_dev->users)
		tc370_stop(data);

	mutex_unlock(&data->input_dev->mutex);

	tk_debug_dbg(true, &data->client->dev, "%s\n", __func__);
	return 0;
}

static int tc370_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tc370_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->input_dev->mutex);

	if (data->input_dev->users)
		tc370_start(data);

	mutex_unlock(&data->input_dev->mutex);

	tk_debug_dbg(true, &data->client->dev, "%s\n", __func__);
	return 0;
}

#endif

static const struct dev_pm_ops tc370_pm_ops = {
	.suspend = tc370_suspend,
	.resume = tc370_resume,
};
#endif /* CONFIG_PM */

static const struct i2c_device_id tc370_id[] = {
	{TC370_NAME, 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, tc370_id);

static struct i2c_driver tc370_driver = {
	.probe = tc370_probe,
	.remove = __devexit_p(tc370_remove),
	.shutdown = tc370_shutdown,
	.driver = {
		.name = TC370_NAME,
		.owner = THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	= &tc370_pm_ops,
#endif
	},
	.id_table = tc370_id,
};

static int __devinit tc370_init(void)
{
	return i2c_add_driver(&tc370_driver);
}

static void __exit tc370_exit(void)
{
	i2c_del_driver(&tc370_driver);
}

late_initcall(tc370_init);
module_exit(tc370_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Touchkey driver for Coreriver tc370");
MODULE_LICENSE("GPL");
