/*
 * Touchscreen driver for Melfas MMS-100s series 
 *
 * Copyright (C) 2013 Melfas Inc.
 * Author: DVK team <dvk@melfas.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/i2c/mms_ts.h>
#include <linux/completion.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>
#include <linux/regulator/consumer.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

/* Flag to enable touch key */
#define MMS_HAS_TOUCH_KEY		1
	
#define ESD_DETECT_COUNT		10

/*
 * ISC_XFER_LEN	- ISC unit transfer length.
 * Give number of 2 power n, where  n is between 2 and 10 
 * i.e. 4, 8, 16 ,,, 1024 
 */
#define ISC_XFER_LEN			1024

#define MMS_FLASH_PAGE_SZ		1024
#define ISC_BLOCK_NUM			(MMS_FLASH_PAGE_SZ / ISC_XFER_LEN)

#define FLASH_VERBOSE_DEBUG		1
#define MAX_SECTION_NUM			3

#define MAX_FINGER_NUM			5
#define FINGER_EVENT_SZ			6
#define MAX_WIDTH			30
#define MAX_PRESSURE			255
#define MAX_LOG_LENGTH			128

/* Registers */
#define MMS_MODE_CONTROL		0x01
#define MMS_TX_NUM			0x0B
#define MMS_RX_NUM			0x0C
#define MMS_KEY_NUM			0x0D
#define MMS_EVENT_PKT_SZ		0x0F
#define MMS_INPUT_EVENT			0x10
#define MMS_UNIVERSAL_CMD		0xA0
#define MMS_UNIVERSAL_RESULT		0xAF

#define MMS_CMD_ENTER_ISC		0x5F
#define MMS_FW_VERSION			0xE1
#define MMS_POWER_CONTROL		0xB0
	
/* Universal commands */	
#define MMS_CMD_SET_LOG_MODE		0x20

/* Event types */
#define MMS_LOG_EVENT			0xD
#define MMS_NOTIFY_EVENT		0xE
#define MMS_ERROR_EVENT			0xF
#define MMS_TOUCH_KEY_EVENT		0x40
	
/* Firmware file name */
#define FW_NAME				"mms_ts.fw"

enum{
	SYS_TXNUM = 3,
	SYS_RXNUM,
	SYS_CLEAR,
	SYS_ENABLE,
	SYS_DISABLE,
	SYS_INTERRUPT,
	SYS_RESET,
};

enum {
	GET_RX_NUM	= 1,
	GET_TX_NUM,
	GET_EVENT_DATA,
};

enum {
	LOG_TYPE_U08	= 2,
	LOG_TYPE_S08,
	LOG_TYPE_U16,
	LOG_TYPE_S16,
	LOG_TYPE_U32	= 8,
	LOG_TYPE_S32,
};

enum {
	ISC_ADDR		= 0xD5,

	ISC_CMD_READ_STATUS	= 0xD9,	
	ISC_CMD_READ		= 0x4000,
	ISC_CMD_EXIT		= 0x8200,
	ISC_CMD_PAGE_ERASE	= 0xC000,
	ISC_CMD_ERASE		= 0xC100,
	ISC_PAGE_ERASE_DONE	= 0x10000,
	ISC_PAGE_ERASE_ENTER	= 0x20000,
};

struct mms_ts_info {
	struct i2c_client 		*client;
	struct input_dev 		*input_dev;
	char 				phys[32];

	u8				tx_num;
	u8				rx_num;
	u8				key_num;
	int 				irq;
	
	int				data_cmd;

	struct mms_ts_platform_data 	*pdata;

	char 				*fw_name;
	struct completion 		init_done;
	struct early_suspend		early_suspend;

	struct mutex 			lock;
	bool				enabled;

	struct cdev			cdev;
	dev_t				mms_dev;
	struct class			*class;

	struct mms_log_data {
		u8			*data;
		int			cmd;
	} log;
};

struct mms_bin_hdr {
	char	tag[8];
	u16	core_version;
	u16	section_num;
	u16	contains_full_binary;
	u16	reserved0;

	u32	binary_offset;
	u32	binary_length;

	u32	extention_offset;	
	u32	reserved1;
	
} __attribute__ ((packed));

struct mms_fw_img {
	u16	type;
	u16	version;

	u16	start_page;
	u16	end_page;

	u32	offset;
	u32	length;

} __attribute__ ((packed));

struct isc_packet {
	u8	cmd;
	u32	addr;
	u8	data[0];
} __attribute__ ((packed));


static int esd_cnt;
static void mms_report_input_data(struct mms_ts_info *info, u8 sz, u8 *buf);
static void mms_ts_early_suspend(struct early_suspend *h);
static void mms_ts_late_resume(struct early_suspend *h);
static int mms_ts_config(struct mms_ts_info *info);

/* mms_ts_enable - wake-up func (VDD on)  */
static void mms_ts_enable(struct mms_ts_info *info)
{
pr_debug("%s  enable tsp  %d\n",__func__,__LINE__);
	if (info->enabled)
		return;
	pr_debug("%s    %d\n",__func__,__LINE__);
	mutex_lock(&info->lock);

	/*
	gpio_direction_output(info->pdata->gpio_resetb, 0);
	udelay(5);
	gpio_direction_input(info->pdata->gpio_resetb);
	*/

	/* use vdd control instead */
	info->pdata->vdd_on(1);
	msleep(50);
	pr_debug("%s    %d \n",__func__,__LINE__);
	info->enabled = true;
	pr_debug("%s    %d \n",__func__,__LINE__);
	enable_irq(info->irq);
pr_debug("%s    %d \n",__func__,__LINE__);
	mutex_unlock(&info->lock);

}

/* mms_ts_disable - sleep func (VDD off) */
static void mms_ts_disable(struct mms_ts_info *info)
{
pr_debug("%s  disable tsp  %d \n",__func__,__LINE__);
	if (!info->enabled)
		return;

	i2c_smbus_write_byte_data(info->client, MMS_POWER_CONTROL, 1);

	mutex_lock(&info->lock);

	disable_irq(info->irq);

	/*
	i2c_smbus_write_byte_data(info->client, MMS_MODE_CONTROL, 0);
	usleep_range(10000, 12000);
	*/

	/* use vdd control instead */
	msleep(50);
	info->pdata->vdd_on(0);
	msleep(50);

	info->enabled = false;

	mutex_unlock(&info->lock);
}

/* mms_reboot - IC reset */ 
static void mms_reboot(struct mms_ts_info *info)
{
	struct i2c_adapter *adapter = to_i2c_adapter(info->client->dev.parent);
pr_debug("%s  reboot 1  %d",__func__,__LINE__);
	i2c_smbus_write_byte_data(info->client, MMS_POWER_CONTROL, 1);
	i2c_lock_adapter(adapter);
	msleep(50);
	info->pdata->vdd_on(0);
	msleep(150);

	info->pdata->vdd_on(1);
	msleep(50);

	i2c_unlock_adapter(adapter);
}

/*
 * mms_clear_input_data - all finger point release
 */
static void mms_clear_input_data(struct mms_ts_info *info)
{
	int i;

	for (i = 0; i < MAX_FINGER_NUM; i++) {
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, false);
	}
	input_sync(info->input_dev);

	return;
}

/*
 * mms_fs_open, mms_fs_release, mms_event_handler, mms_fs_read, mms_fs_write
 * melfas debugging function
 */
static int mms_fs_open(struct inode *node, struct file *fp)
{
	struct mms_ts_info *info;
	struct i2c_client *client;
	struct i2c_msg msg;
	u8 buf[3] = {
		MMS_UNIVERSAL_CMD,
		MMS_CMD_SET_LOG_MODE,
		true,
	};

	info = container_of(node->i_cdev, struct mms_ts_info, cdev);
	client = info->client;

	disable_irq(info->irq);
	fp->private_data = info;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.buf = buf;
	msg.len = sizeof(buf);

	i2c_transfer(client->adapter, &msg, 1);

	info->log.data = kzalloc(MAX_LOG_LENGTH * 20 + 5, GFP_KERNEL);

	mms_clear_input_data(info);

	return 0;
}

static int mms_fs_release(struct inode *node, struct file *fp)
{
	struct mms_ts_info *info = fp->private_data;

	mms_clear_input_data(info);
	mms_reboot(info);

	kfree(info->log.data);
	enable_irq(info->irq);

	return 0;
}

static void mms_event_handler(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	int sz;
	int ret;
	int row_num;
	u8 reg = MMS_INPUT_EVENT;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = &reg,
			.len = 1,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = info->log.data,
		},

	};
	struct mms_log_pkt {
		u8	marker;
		u8	log_info;
		u8	code;
		u8	element_sz;
		u8	row_sz;
	} __attribute__ ((packed)) *pkt = (struct mms_log_pkt *)info->log.data;

	memset(pkt, 0, sizeof(*pkt));

	if (gpio_get_value(info->pdata->gpio_resetb))
		return;

	sz = i2c_smbus_read_byte_data(client, MMS_EVENT_PKT_SZ);
	msg[1].len = sz;

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (ret != ARRAY_SIZE(msg)) {
		dev_err(&client->dev,
			"failed to read %d bytes of data\n",
			sz);
		return;
	}

	if ((pkt->marker & 0xf) == MMS_LOG_EVENT) {
		if ((pkt->log_info & 0x7) == 0x1) {
			pkt->element_sz = 0;
			pkt->row_sz = 0;

			return;
		}

		switch (pkt->log_info >> 4) {
		case LOG_TYPE_U08:
		case LOG_TYPE_S08:
			msg[1].len = pkt->element_sz;
			break;
		case LOG_TYPE_U16:
		case LOG_TYPE_S16:
			msg[1].len = pkt->element_sz * 2;
			break;
		case LOG_TYPE_U32:
		case LOG_TYPE_S32:
			msg[1].len = pkt->element_sz * 4;
			break;
		default:
			dev_err(&client->dev, "invalied log type\n");
			return;
		}

		msg[1].buf = info->log.data + sizeof(struct mms_log_pkt);
		reg = MMS_UNIVERSAL_RESULT;
		row_num = pkt->row_sz ? pkt->row_sz : 1;

		while (row_num--) {
			while (gpio_get_value(info->pdata->gpio_resetb))
				;
			ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
			msg[1].buf += msg[1].len;
		};
	} else {
		mms_report_input_data(info, sz, info->log.data);
		memset(pkt, 0, sizeof(*pkt));
	}

	return;
}

/* mms_report_input_data - The position of a touch send to platfrom  */
static void mms_report_input_data(struct mms_ts_info *info, u8 sz, u8 *buf)
{
	int i;
	struct i2c_client *client = info->client;
	int id;
	int x;
	int y;
	int touch_major;
	int pressure;
	int key_code;
	int key_state;
	u8 *tmp;

	if (buf[0] == MMS_NOTIFY_EVENT) {
		dev_info(&client->dev, "TSP mode changed (%d)\n", buf[1]);
		goto out;
	} else if (buf[0] == MMS_ERROR_EVENT) {
		dev_info(&client->dev, "Error detected, restarting TSP\n");
		mms_clear_input_data(info);
		mms_reboot(info);
		esd_cnt++;
		if (esd_cnt>= ESD_DETECT_COUNT)
		{
			i2c_smbus_write_byte_data(info->client, MMS_MODE_CONTROL, 0x04);
			esd_cnt =0;
		}
		goto out;
	}

	for (i = 0; i < sz; i += FINGER_EVENT_SZ) {
		tmp = buf + i;
		esd_cnt =0;
		if (tmp[0] & MMS_TOUCH_KEY_EVENT) {
			switch (tmp[0] & 0xf) {
			case 1:
				key_code = KEY_MENU;
				break;
			case 2:
				key_code = KEY_BACK;
				break;
			default:
				dev_err(&client->dev, "unknown key type\n");
				goto out;
				break;
			}

			key_state = (tmp[0] & 0x80) ? 1 : 0;
			input_report_key(info->input_dev, key_code, key_state);

		} else {
			id = (tmp[0] & 0xf) -1;

			x = tmp[2] | ((tmp[1] & 0xf) << 8);
			y = tmp[3] | (((tmp[1] >> 4 ) & 0xf) << 8);
			touch_major = tmp[4];
			pressure = tmp[5];

			input_mt_slot(info->input_dev, id);
			
			if (!(tmp[0] & 0x80)) {
				input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, false);
				continue;
			}

			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, true);
			input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
			input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, touch_major);
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, pressure);
//			dev_notice(&client->dev,"P [%2d],([%3d],[%4d]) , major=%d,  pressure=%d",id, x, y, tmp[4], tmp[5]);
		}
	}

	input_sync(info->input_dev);

out:
	return;

}

static ssize_t mms_fs_read(struct file *fp, char *rbuf, size_t cnt, loff_t *fpos)
{
	struct mms_ts_info *info = fp->private_data;
	struct i2c_client *client = info->client;
	int ret = 0;

	switch (info->log.cmd) {
	case GET_RX_NUM:
		ret = copy_to_user(rbuf, &info->rx_num, 1);
		break;
	case GET_TX_NUM:
		ret = copy_to_user(rbuf, &info->tx_num, 1);
		break;
	case GET_EVENT_DATA:
		mms_event_handler(info);
		/* copy data without log marker */
		ret = copy_to_user(rbuf, info->log.data + 1, cnt);
		break;
	default:
		dev_err(&client->dev, "unknown command\n");
		ret = -EFAULT;
		break;
	}

	return ret;
}

static ssize_t mms_fs_write(struct file *fp, const char *wbuf, size_t cnt, loff_t *fpos)
{
	struct mms_ts_info *info = fp->private_data;
	struct i2c_client *client = info->client;
	u8 *buf;
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = cnt,
	};
	int ret = 0;

	mutex_lock(&info->lock);

	if (!info->enabled)
		goto tsp_disabled;
	
	msg.buf = buf = kzalloc(cnt + 1, GFP_KERNEL);

	if ((buf == NULL) || copy_from_user(buf, wbuf, cnt)) {
		dev_err(&client->dev, "failed to read data from user\n");
		ret = -EIO;
		goto out;
	}

	if (cnt == 1) {
		info->log.cmd = *buf;
	} else {
		if (i2c_transfer(client->adapter, &msg, 1) != 1) {
			dev_err(&client->dev, "failed to transfer data\n");
			ret = -EIO;
			goto out;
		}
	}

	ret = 0;

out:
	kfree(buf);
tsp_disabled:
	mutex_unlock(&info->lock);

	return ret;
}
/*
 * mms_isc_read_status - Check erase state function
 */
static int mms_isc_read_status(struct mms_ts_info *info, u32 val)
{
	struct i2c_client *client = info->client;
	u8 cmd = ISC_CMD_READ_STATUS;
	u32 result = 0;
	int cnt = 100;
	int ret = 0;

	do {
		i2c_smbus_read_i2c_block_data(client, cmd, 4, (u8 *)&result);
		if (result == val)
			break;
		msleep(1);
	} while (--cnt);

	if (!cnt) {
		dev_err(&client->dev,
			"status read fail. cnt : %d, val : 0x%x != 0x%x\n",
			cnt, result, val);
	}
	return ret;
}

static int mms_isc_transfer_cmd(struct mms_ts_info *info, int cmd)
{
	struct i2c_client *client = info->client;
	struct isc_packet pkt = { ISC_ADDR, cmd };
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = sizeof(struct isc_packet),
		.buf = (u8 *)&pkt,
	};

	return (i2c_transfer(client->adapter, &msg, 1) != 1);
}

/*
 * mms_isc_erase_page - ic page erase(1 kbyte)
 */
static int mms_isc_erase_page(struct mms_ts_info *info, int page)
{
	return mms_isc_transfer_cmd(info, ISC_CMD_PAGE_ERASE | page) ||
		mms_isc_read_status(info, ISC_PAGE_ERASE_DONE | ISC_PAGE_ERASE_ENTER | page);
}

/*
 * mms_isc_enter - isc enter command
 */
static int mms_isc_enter(struct mms_ts_info *info)
{
	return i2c_smbus_write_byte_data(info->client, MMS_CMD_ENTER_ISC, true);
}

static int mms_isc_exit(struct mms_ts_info *info)
{
	return mms_isc_transfer_cmd(info, ISC_CMD_EXIT);
}

/*
 * mms_flash_section - f/w section(boot,core,config) download func
 */
static int mms_flash_section(struct mms_ts_info *info, struct mms_fw_img *img, const u8 *data)
{
	struct i2c_client *client = info->client;
	struct isc_packet *isc_packet;
	int ret;
	int page, i;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = ISC_XFER_LEN,
		},
	};
	int ptr = img->offset;

	isc_packet = kzalloc(sizeof(*isc_packet) + ISC_XFER_LEN, GFP_KERNEL);
	isc_packet->cmd = ISC_ADDR;

	msg[0].buf = (u8 *)isc_packet;
	msg[1].buf = kzalloc(ISC_XFER_LEN, GFP_KERNEL);

	for (page = img->start_page; page <= img->end_page; page++) {
		mms_isc_erase_page(info, page);

		for (i = 0; i < ISC_BLOCK_NUM; i++, ptr += ISC_XFER_LEN) {
			/* flash firmware */
			u32 tmp = page * 256 + i * (ISC_XFER_LEN / 4);
			put_unaligned_le32(tmp, &isc_packet->addr);
			msg[0].len = sizeof(struct isc_packet) + ISC_XFER_LEN;

			memcpy(isc_packet->data, data + ptr, ISC_XFER_LEN);
			if (i2c_transfer(client->adapter, msg, 1) != 1)
				goto i2c_err;

			/* verify firmware */
			tmp |= ISC_CMD_READ;
			put_unaligned_le32(tmp, &isc_packet->addr);
			msg[0].len = sizeof(struct isc_packet);

			if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg))
				goto i2c_err;

			if (memcmp(isc_packet->data, msg[1].buf, ISC_XFER_LEN)) {
#if FLASH_VERBOSE_DEBUG
				print_hex_dump(KERN_ERR, "mms fw wr : ",
						DUMP_PREFIX_OFFSET, 16, 1,
						isc_packet->data, ISC_XFER_LEN, false);

				print_hex_dump(KERN_ERR, "mms fw rd : ",
						DUMP_PREFIX_OFFSET, 16, 1,
						msg[1].buf, ISC_XFER_LEN, false);
#endif
				dev_err(&client->dev, "flash verify failed\n");
				ret = -1;
				goto out;
			}

		}
	}

	dev_info(&client->dev, "section [%d] update succeeded\n", img->type);

	ret = 0;
	goto out;

i2c_err:
	dev_err(&client->dev, "i2c failed @ %s\n", __func__);
	ret = -1;

out:
	kfree(isc_packet);
	kfree(msg[1].buf);

	return ret;
}

/*
 * get_fw_version - f/w version read
 */
static int get_fw_version(struct i2c_client *client, u8 *buf)
{
	u8 cmd = MMS_FW_VERSION;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = &cmd,
			.len = 1,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = buf,
			.len = MAX_SECTION_NUM,
		},
	};

	return (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg));
}

/*
 * mms_flash_fw -flash_section of parent function
 * this function is IC version binary version compare and Download
 */ 
static int mms_flash_fw(const struct firmware *fw, struct mms_ts_info *info)
{
	int ret;
	int i;
	struct mms_bin_hdr *fw_hdr;
	struct mms_fw_img **img;
	struct i2c_client *client = info->client;
	u8 ver[MAX_SECTION_NUM];
	u8 target[MAX_SECTION_NUM];
	int offset = sizeof(struct mms_bin_hdr);
	int retires = 3;
	bool update_flag = false;
	bool isc_flag = true;
	
	fw_hdr = (struct mms_bin_hdr *)fw->data;
	img = kzalloc(sizeof(*img) * fw_hdr->section_num, GFP_KERNEL);

	while (retires--) {
		if (!get_fw_version(client, ver))
			break;
		else
			mms_reboot(info);
	}

	if (retires < 0) {
		dev_warn(&client->dev, "failed to obtain ver. info\n");
		isc_flag = false;
		memset(ver, 0xff, sizeof(ver));
	} else {
		print_hex_dump(KERN_INFO, "mms_ts fw ver : ", DUMP_PREFIX_NONE, 16, 1,
				ver, MAX_SECTION_NUM, false);
	}

	for (i = 0; i < fw_hdr->section_num; i++, offset += sizeof(struct mms_fw_img)) {
		img[i] = (struct mms_fw_img *)(fw->data + offset);
		target[i] = img[i]->version;

		if (ver[img[i]->type] != target[i]) {
			if(isc_flag){
				mms_isc_enter(info);
				isc_flag = false;
			}
			update_flag = true;
			dev_info(&client->dev,
				"section [%d] is need to be updated. ver : 0x%x, bin : 0x%x\n",
				img[i]->type, ver[img[i]->type], target[i]);

			if ((ret = mms_flash_section(info, img[i], fw->data + fw_hdr->binary_offset))) {
				mms_reboot(info);
				goto out;
			}
			//memset(ver, 0xff, sizeof(ver));
		} else {
			dev_info(&client->dev, "section [%d] is up to date\n", i);
		}
	}

	if (update_flag){
		mms_isc_exit(info);
		msleep(5);
		mms_reboot(info);
	}

	

	if (get_fw_version(client, ver)) {
		dev_err(&client->dev, "failed to obtain version after flash\n");
		ret = -1;
		goto out;
	} else {
		for (i = 0; i < fw_hdr->section_num; i++) {
			if (ver[img[i]->type] != target[i]) {
				dev_info(&client->dev,
					"version mismatch after flash. [%d] 0x%x != 0x%x\n",
					i, ver[img[i]->type], target[i]);

				ret = -1;
				goto out;
			}
		}
	}

	ret = 0;

out:
	kfree(img);

	return ret;
}

static void mms_fw_update_controller(const struct firmware *fw, void * context)
{
	struct mms_ts_info *info = context;
	int retires = 3;
	int ret;

	if (!fw) {
		dev_err(&info->client->dev, "failed to read firmware\n");
		complete_all(&info->init_done);
		return;
	}

	do {
		ret = mms_flash_fw(fw, info);
	} while (ret && --retires);

	if (!retires) {
		dev_err(&info->client->dev, "failed to flash firmware after retires\n");
	}
	mms_ts_config(info);
	release_firmware(fw);
}

/* mms_ts_interrupt - interrupt thread */
static irqreturn_t mms_ts_interrupt(int irq, void *dev_id)
{
pr_debug("%s interrupt call   %d \n",__func__,__LINE__);
	struct mms_ts_info *info = dev_id;
	struct i2c_client *client = info->client;
	u8 buf[MAX_FINGER_NUM * FINGER_EVENT_SZ] = { 0, };
	int ret;
	int sz;
	u8 reg = MMS_INPUT_EVENT;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = &reg,
			.len = 1,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = buf,
		},
	};

	sz = i2c_smbus_read_byte_data(client, MMS_EVENT_PKT_SZ);
	msg[1].len = sz;

	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));

	if (ret != ARRAY_SIZE(msg)) {
		dev_err(&client->dev,
			"failed to read %d bytes of touch data (%d)\n",
			sz, ret);
	} else {
	pr_debug("%s report event   %d \n",__func__,__LINE__);
		mms_report_input_data(info, sz, buf);
	}

	return IRQ_HANDLED;
}

/*
 * mms_ts_input_open - Register input device after call this function 
 * this function is wait firmware flash wait
 */ 
static int mms_ts_input_open(struct input_dev *dev)
{
pr_debug("%s  enter1  %d",__func__,__LINE__);
	struct mms_ts_info *info = input_get_drvdata(dev);
	int ret;
pr_debug("%s enterw %d",__func__,__LINE__);
	ret = wait_for_completion_interruptible_timeout(&info->init_done,
			msecs_to_jiffies(90 * MSEC_PER_SEC));

	if (ret > 0) {
		if (info->irq != -1) {
		pr_debug("%s    %d",__func__,__LINE__);
			mms_ts_enable(info);
			ret = 0;
		} else {
			ret = -ENXIO;
		}
	} else {
		dev_err(&dev->dev, "error while waiting for device to init\n");
		ret = -ENXIO;
	}

	return ret;
}

/*
 * mms_ts_input_close -If device power off state call this function
 */
static void mms_ts_input_close(struct input_dev *dev)
{
	struct mms_ts_info *info = input_get_drvdata(dev);

	mms_ts_disable(info);
}

/*
 * mms_ts_config - f/w check download & irq thread register
 */
static int mms_ts_config(struct mms_ts_info *info)
{
pr_debug("%s  config 1 %d \n",__func__,__LINE__);
	struct i2c_client *client = info->client;
	int ret;
	pr_debug("%s  config 2  %d \n",__func__,__LINE__);
	ret = request_threaded_irq(client->irq, NULL, mms_ts_interrupt,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"mms_ts", info);
pr_debug("%s  irq registered  %d\n",__func__,__LINE__);
	if (ret) {
		dev_err(&client->dev, "failed to register irq\n");
		goto out;
	}
	disable_irq(client->irq);
	info->irq = client->irq;
	barrier();

	info->tx_num = i2c_smbus_read_byte_data(client, MMS_TX_NUM);
	info->rx_num = i2c_smbus_read_byte_data(client, MMS_RX_NUM);
	info->key_num = i2c_smbus_read_byte_data(client, MMS_KEY_NUM);

	dev_info(&client->dev, "Melfas touch controller initialized\n");
	pr_debug("%s  reboot called  %d\n",__func__,__LINE__);
	mms_reboot(info);
	complete_all(&info->init_done);
	pr_debug("%s  init done  %d\n",__func__,__LINE__);

out:
	return ret;
}

/*
 * bin_report_read, bin_report_write, bin_sysfs_read, bin_sysfs_write
 * melfas debugging function
 */
static ssize_t bin_report_read(struct file *fp, struct kobject *kobj, struct bin_attribute *attr,
                                char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);
	count = 0;
	switch(info->data_cmd){
	case SYS_TXNUM:
		dev_info(&info->client->dev, "tx send %d \n",info->tx_num);
		buf[0]=info->tx_num;
		count =1;
		info->data_cmd = 0;
		break;
	case SYS_RXNUM:
		dev_info(&info->client->dev, "rx send%d\n", info->rx_num);
		buf[0]=info->rx_num;
		count =1;
		info->data_cmd = 0;
		break;
	case SYS_CLEAR:
		dev_info(&info->client->dev, "Input clear\n");
		mms_clear_input_data(info);
		count = 1;
		info->data_cmd = 0;
		break;
	case SYS_ENABLE:
		dev_info(&info->client->dev, "enable_irq  \n");
		enable_irq(info->irq);
		count = 1;
		info->data_cmd = 0;
		break;
	case SYS_DISABLE:
		dev_info(&info->client->dev, "disable_irq  \n");
		disable_irq(info->irq);
		count = 1;
		info->data_cmd = 0;
		break;
	case SYS_INTERRUPT:
		count = gpio_get_value(info->pdata->gpio_resetb);
		info->data_cmd = 0;
		break;
	case SYS_RESET:
		mms_reboot(info);
		dev_info(&info->client->dev, "read mms_reboot\n");
		count = 1;
		info->data_cmd = 0;
		break;
	}
	return count;
}

static ssize_t bin_report_write(struct file *fp, struct kobject *kobj, struct bin_attribute *attr,
                                char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);
	if(buf[0]==100){
		mms_report_input_data(info, buf[1], &buf[2]);
	}else{
		info->data_cmd=(int)buf[0];
	}
	return count;
}
static struct bin_attribute bin_attr_data = {
	.attr = {
	.name = "report_data",
	.mode = S_IRWXUGO,
},
	.size = PAGE_SIZE,
	.read = bin_report_read,
	.write = bin_report_write,
};

static ssize_t bin_sysfs_read(struct file *fp, struct kobject *kobj , struct bin_attribute *attr,
                          char *buf, loff_t off,size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct i2c_client *client = to_i2c_client(dev);
	 struct mms_ts_info *info = i2c_get_clientdata(client);
	struct i2c_msg msg;
	info->client = client;
	msg.addr = client->addr;
	msg.flags = I2C_M_RD ;
	msg.buf = (u8 *)buf;
	msg.len = count;

	switch (count)
	{
		case 65535:
			mms_reboot(info);
			dev_info(&client->dev, "read mms_reboot\n");
			return 0;

		default :
			if(i2c_transfer(client->adapter, &msg, 1) != 1){
				dev_err(&client->dev, "failed to transfer data\n");
				mms_reboot(info);
				return 0;
				}
			break;

	}

	return count;
}

static ssize_t bin_sysfs_write(struct file *fp, struct kobject *kobj, struct bin_attribute *attr,
                                char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);
	struct i2c_msg msg;

	msg.addr =client->addr;
	msg.flags = 0;
	msg.buf = (u8 *)buf;
	msg.len = count;


	if(i2c_transfer(client->adapter, &msg, 1) != 1){
		dev_err(&client->dev, "failed to transfer data\n");
		mms_reboot(info);
		return 0;
	}

	return count;
}

static struct bin_attribute bin_attr = {
	.attr = {
	.name = "mms_bin",
	.mode = S_IRWXUGO,
	},
	.size = MMS_FLASH_PAGE_SZ,
	.read = bin_sysfs_read,
	.write = bin_sysfs_write,
};

static struct file_operations mms_fops = {
	.owner		= THIS_MODULE,
	.open		= mms_fs_open,
	.release	= mms_fs_release,
	.read		= mms_fs_read,
	.write		= mms_fs_write,
};

//melfas test mode
#ifdef __MMS_TEST_MODE__
struct mms_ts_test test;
static ssize_t mms_intensity(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;
	dev_info(&info->client->dev, "Intensity Test\n");
	if(get_intensity(&test)!=0){
		dev_err(&info->client->dev, "Intensity Test failed\n");
		return -1;
	}
	ret = snprintf(buf,PAGE_SIZE,"%s\n",test.get_data);
	return ret;
}

static DEVICE_ATTR(intensity, 0666, mms_intensity, NULL);

static ssize_t mms_cmdelta(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;
	dev_info(&info->client->dev, "cm delta Test\n");
	get_cm_test_init(&test);
	get_cm_delta(&test);
	get_cm_test_exit(&test);
	
	ret = snprintf(buf,PAGE_SIZE,"%s\n",test.get_data);
	memset(test.get_data,0,4096);
	return ret;
}
static DEVICE_ATTR(cmdelta, 0666, mms_cmdelta, NULL);

static ssize_t mms_cmabs(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;
	dev_info(&info->client->dev, "cm delta Test\n");
	get_cm_test_init(&test);
	get_cm_abs(&test);
	get_cm_test_exit(&test);
	
	
	ret = snprintf(buf,PAGE_SIZE,"%s\n",test.get_data);
	memset(test.get_data,0,4096);
	return ret;
}
static DEVICE_ATTR(cmabs, 0666, mms_cmabs, NULL);

static ssize_t mms_cmjitter(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int ret;
	dev_info(&info->client->dev, "cm delta Test\n");
	get_cm_test_init(&test);
	get_cm_jitter(&test);
	get_cm_test_exit(&test);
	
	ret = snprintf(buf,PAGE_SIZE,"%s\n",test.get_data);
	memset(test.get_data,0,4096);
	return ret;
}
static DEVICE_ATTR(cmjitter, 0666, mms_cmjitter, NULL);

static struct attribute *mms_attrs[] = {
	&dev_attr_intensity.attr,
	&dev_attr_cmdelta.attr,
	&dev_attr_cmabs.attr,
	&dev_attr_cmjitter.attr,
	NULL,
};

static const struct attribute_group mms_attr_group = {
	.attrs = mms_attrs,
};
#endif

#ifdef CONFIG_OF
static const u8 mms_ts_keycode[] = {KEY_MENU, KEY_BACK};
static unsigned int gpio_touchkey_len_en;
static const char * vdd_name;

static void mms_ts_vdd_enable(bool on)
{
	static struct regulator *ts_vdd = NULL;

	if (ts_vdd == NULL) {
		ts_vdd = regulator_get(NULL, vdd_name);

		if (IS_ERR(ts_vdd)) {
			pr_err("Get regulator of TSP error!\n");
			return;
		}
	}
	if (on) {
		regulator_set_voltage(ts_vdd, 3000000, 3000000);
		regulator_enable(ts_vdd);
	}
	else if (regulator_is_enabled(ts_vdd)) {
		regulator_disable(ts_vdd);
	}
}

static void touchkey_led_vdd_enable(bool on)
{
	static int ret = 0;

	if (ret == 0) {
		ret = gpio_request(gpio_touchkey_len_en, "touchkey_led_en");
		if (ret) {
			printk("%s: request gpio error\n", __func__);
			return -EIO;
		}
		gpio_direction_output(gpio_touchkey_len_en, 0);
		ret = 1;
	}
	gpio_set_value(gpio_touchkey_len_en,on);
}

static struct mms_ts_platform_data * mms_ts_parse_dt(struct device *dev)
{
	struct mms_ts_platform_data *pdata;
	struct device_node *np = dev->of_node;
	int ret;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "Could not allocate struct mms_ts_platform_data");
		return NULL;
	}
	pdata->gpio_sda = of_get_gpio(np, 0);
	if(pdata->gpio_sda < 0){
		dev_err(dev, "fail to get gpio_sda\n");
		goto fail;
	}
	pdata->gpio_scl = of_get_gpio(np, 1);
	if(pdata->gpio_scl < 0){
		dev_err(dev, "fail to get gpio_scl\n");
		goto fail;
	}
	pdata->gpio_int = of_get_gpio(np, 2);
	if(pdata->gpio_int < 0){
		dev_err(dev, "fail to get gpio_int\n");
		goto fail;
	}
	gpio_touchkey_len_en = of_get_gpio(np, 3);
	if(gpio_touchkey_len_en < 0){
		dev_err(dev, "fail to gpio_touchkey_len_en\n");
		goto fail;
	}
	ret = of_property_read_string(np, "vdd_name", &vdd_name);
	if(ret){
		dev_err(dev, "fail to get vdd_name\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "max_x", &pdata->max_x);
	if(ret){
		dev_err(dev, "fail to get max_x\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "max_y", &pdata->max_y);
	if(ret){
		dev_err(dev, "fail to get max_y\n");
		goto fail;
	}
	ret = of_property_read_u32(np, "use_touchkey", &pdata->use_touchkey);
	if(ret){
		dev_err(dev, "fail to get use_touchkey\n");
		goto fail;
	}

	pdata->vdd_on = mms_ts_vdd_enable;
	pdata->tkey_led_vdd_on = touchkey_led_vdd_enable;
	pdata->touchkey_keycode = mms_ts_keycode;
	return pdata;
fail:
	kfree(pdata);
	return NULL;
}
#endif


static int __init mms_ts_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mms_ts_info *info;
	struct input_dev *input_dev;
	int ret = 0;
	const char *fw_name = FW_NAME;
	struct mms_ts_platform_data * pdata;

	printk("probe enter \n");

#ifdef CONFIG_OF
	struct device_node *np = client->dev.of_node;
	if (np && !client->dev.platform_data) {
		pdata = mms_ts_parse_dt(&client->dev);
		if(pdata) {
			client->dev.platform_data = pdata;
		} else {
			return -ENODEV;
		}
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	info = kzalloc(sizeof(struct mms_ts_info), GFP_KERNEL);


	input_dev = input_allocate_device();

	if (!input_dev) {
		dev_err(&client->dev, "Failed to allocated memory\n");
		return -ENOMEM;
	}

	info->client = client;
	info->input_dev = input_dev;
	info->pdata = client->dev.platform_data;
	init_completion(&info->init_done);
	info->irq = -1;
pr_debug("probe mutex init \n");
	mutex_init(&info->lock);

	client->irq = gpio_to_irq(info->pdata->gpio_int);
	input_mt_init_slots(input_dev, MAX_FINGER_NUM,0);

	snprintf(info->phys, sizeof(info->phys),
		"%s/input0", dev_name(&client->dev));

	input_dev->name = "Melfas MMS100s Touch Screen";
	input_dev->phys = info->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

pr_debug("probe dev init \n");
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, MAX_WIDTH, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, MAX_PRESSURE, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, info->pdata->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, info->pdata->max_y, 0, 0);

	input_set_drvdata(input_dev, info);

#if MMS_HAS_TOUCH_KEY
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(KEY_MENU, input_dev->keybit);
	__set_bit(KEY_BACK, input_dev->keybit);
#endif

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev, "failed to register input dev\n");
		return -EIO;
	}

	i2c_set_clientdata(client, info);

	//mms_reboot(info);
	mms_ts_enable(info);
	info->enabled = true;
	ret = request_threaded_irq(client->irq, NULL, mms_ts_interrupt,
	IRQF_TRIGGER_LOW  | IRQF_ONESHOT," mms_ts", info);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to register interrupt\n");
	}
	info->irq = client->irq;	

pr_debug("%s  enable FW %d\n",__func__,__LINE__);
	info->fw_name = kstrdup(fw_name, GFP_KERNEL);
pr_debug("%s  enable tsp  %d\n",__func__,__LINE__);
	ret = request_firmware_nowait(THIS_MODULE, true, fw_name, &client->dev,
					GFP_KERNEL, info, mms_fw_update_controller);
	if (ret) {
		dev_err(&client->dev, "failed to schedule firmware update\n");
		return -EIO;
	}

	esd_cnt = 0;
#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	info->early_suspend.suspend = mms_ts_early_suspend;
	info->early_suspend.resume = mms_ts_late_resume;
	register_early_suspend(&info->early_suspend);
#endif
pr_debug("%s  ALLOCH  %d\n",__func__,__LINE__);
	if (alloc_chrdev_region(&info->mms_dev, 0, 1, "mms_ts")) {
		dev_err(&client->dev, "failed to allocate device region\n");
		return -ENOMEM;
	}
pr_debug("%s  FOPS  %d\n",__func__,__LINE__);
	cdev_init(&info->cdev, &mms_fops);
	info->cdev.owner = THIS_MODULE;

	if (cdev_add(&info->cdev, info->mms_dev, 1)) {
		dev_err(&client->dev, "failed to add ch dev\n");
		return -EIO;
	}
pr_debug("%s  CLASS CREATE  %d\n",__func__,__LINE__);
	info->class = class_create(THIS_MODULE, "mms_ts");
	device_create(info->class, NULL, info->mms_dev, NULL, "mms_ts");

	if(sysfs_create_bin_file(&client->dev.kobj ,&bin_attr)){
		dev_err(&client->dev, "failed to create sysfs symlink\n");
		return -EAGAIN;
	}

	if(sysfs_create_bin_file(&client->dev.kobj ,&bin_attr_data)){
		dev_err(&client->dev, "failed to create sysfs symlink\n");
		return -EAGAIN;
	}

	if (sysfs_create_link(NULL, &client->dev.kobj, "mms_ts")) {
		dev_err(&client->dev, "failed to create sysfs symlink\n");
		return -EAGAIN;
	}
#ifdef __MMS_TEST_MODE__
	if (sysfs_create_group(&client->dev.kobj, &mms_attr_group)) {
		dev_err(&client->dev, "failed to create sysfs group\n");
		return -EAGAIN;
	}
	test.get_data = kzalloc(4096, GFP_KERNEL);
	test.client = client;
	test.pdata = info->pdata;
	test.tx_num = info->tx_num;
	test.rx_num = info->rx_num;
	test.key_num = info->key_num;
	test.irq = info->irq;
#endif
	dev_notice(&client->dev, "mms dev initialized\n");

	return 0;
}

static int __exit mms_ts_remove(struct i2c_client *client)
{
	struct mms_ts_info *info = i2c_get_clientdata(client);

	if (info->irq >= 0)
		free_irq(info->irq, info);

#ifdef __MMS_TEST_MODE__
	sysfs_remove_group(&info->client->dev.kobj, &mms_attr_group);
	kfree(test.get_data);
#endif
	sysfs_remove_link(NULL, "mms_ts");
	sysfs_remove_bin_file(&client->dev.kobj, &bin_attr);
	sysfs_remove_bin_file(&client->dev.kobj, &bin_attr_data);
	input_unregister_device(info->input_dev);
	unregister_early_suspend(&info->early_suspend);

	device_destroy(info->class, info->mms_dev);
	class_destroy(info->class);
#ifdef CONFIG_OF
	kfree(info->pdata);
#endif	
	kfree(info->fw_name);
	kfree(info);

	return 0;
}

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
static int mms_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);

	mutex_lock(&info->input_dev->mutex);

	if (info->input_dev->users) {
		mms_ts_disable(info);
		mms_clear_input_data(info);
	}

	mutex_unlock(&info->input_dev->mutex);
	return 0;

}

static int mms_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);

	mutex_lock(&info->input_dev->mutex);

	if (info->input_dev->users)
		mms_ts_enable(info);

	mutex_unlock(&info->input_dev->mutex);

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void mms_ts_early_suspend(struct early_suspend *h)
{
	struct mms_ts_info *info;
	info = container_of(h, struct mms_ts_info, early_suspend);
	mms_ts_suspend(&info->client->dev);
}

static void mms_ts_late_resume(struct early_suspend *h)
{
	struct mms_ts_info *info;
	info = container_of(h, struct mms_ts_info, early_suspend);
	mms_ts_resume(&info->client->dev);
}
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static const struct dev_pm_ops mms_ts_pm_ops = {
	.suspend	= mms_ts_suspend,
	.resume		= mms_ts_resume,
}
#endif

static const struct i2c_device_id mms_ts_id[] = {
	{"mms_ts", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, mms_ts_id);

#ifdef CONFIG_OF
static const struct of_device_id mms_ts_of_match[] = {
	{ .compatible = "Melfas,mms_ts", },
	{ }
};
#endif

static struct i2c_driver mms_ts_driver = {
	.probe		= mms_ts_probe,
	.remove		= mms_ts_remove,
	.driver		= {
		.name	= "mms_ts",
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
		.pm	= &mms_ts_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = mms_ts_of_match,
#endif
	},
	.id_table	= mms_ts_id,
};

static int __init mms_ts_init(void)
{
	return i2c_add_driver(&mms_ts_driver);
}

static void __exit mms_ts_exit(void)
{
	return i2c_del_driver(&mms_ts_driver);
}

module_init(mms_ts_init);
module_exit(mms_ts_exit);

MODULE_VERSION("1.6.1");
MODULE_DESCRIPTION("Touchscreen driver for MELFAS MMS-100s series");
MODULE_LICENSE("GPL");

