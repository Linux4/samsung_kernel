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

/* Melfas TSP test mode */
#include <linux/platform_data/mms_ts_test.h>

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
#define MMS_VSC_CMD_EXIT		0X05
#define MMS_THRESHOLD		0x05

#define MMS_TSP_REVISION	0xF0
#define MMS_HW_REVISION		0xF1
#define MMS_COMPAT_GROUP	0xF2


#define BOOT_VERSION 0x4
#define CORE_VERSION 0x34
#define FW_VERSION 0x5

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
#define MMS_CORE_VERSION		0xE2
#define MMS_CONFIG_VERSION		0xE3
#define MMS_POWER_CONTROL		0xB0
#define TSP_BUF_SIZE 1024

#define TSP_CMD_STR_LEN 32
#define TSP_CMD_RESULT_STR_LEN 512
#define TSP_CMD_PARAM_NUM 8
	
/* Universal commands */	
#define MMS_CMD_SET_LOG_MODE		0x20

/* Event types */
#define MMS_LOG_EVENT			0xD
#define MMS_NOTIFY_EVENT		0xE
#define MMS_ERROR_EVENT			0xF
#define MMS_TOUCH_KEY_EVENT		0x40
	
/* Firmware file name */
#define FW_NAME				"melfas/KANAS_NEW_TSP_FW.mfsb"	//new tsp fw
#define FW_SUB_NAME			"melfas/KANAS_OLD_TSP_FW.mfsb"  //old tsp fw

/////// Force Firmware update ///////
#define EXTRA_FW_PATH			"/sdcard/mms_ts.fw"
////////////////////////////////////
extern struct class *sec_class;

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
	
	struct list_head			cmd_list_head;
	u8			cmd_state;
	char			cmd[TSP_CMD_STR_LEN];
	int			cmd_param[TSP_CMD_PARAM_NUM];
	char			cmd_result[TSP_CMD_RESULT_STR_LEN];
	struct mutex			cmd_lock;
	bool			cmd_is_running;

	struct mutex 			lock;
	bool				enabled;
	bool ft_flag;
	u8			fw_boot_ver;
	u8			fw_core_ver;
	u8			fw_ic_ver;

	struct cdev			cdev;
	dev_t				mms_dev;
	struct class			*class;
	u16 *get_data;

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

#define TSP_CMD(name, func) .cmd_name = name, .cmd_func = func

struct tsp_cmd {
	struct list_head	list;
	const char	*cmd_name;
	void	(*cmd_func)(void *device_data);
};
static void get_threshold(void *device_data);
static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void get_config_ver(void *device_data);
static void not_support_cmd(void *device_data);
struct tsp_cmd tsp_cmds[] = {
	{TSP_CMD("get_threshold", get_threshold),},
	{TSP_CMD("fw_update", fw_update),},
	{TSP_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{TSP_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{TSP_CMD("get_config_ver", get_config_ver),},
//	{TSP_CMD("module_off_master", module_off_master),},
//	{TSP_CMD("module_on_master", module_on_master),},
//	{TSP_CMD("module_off_slave", not_support_cmd),},
//	{TSP_CMD("module_on_slave", not_support_cmd),},
//	{TSP_CMD("get_chip_vendor", get_chip_vendor),},
//	{TSP_CMD("get_chip_name", get_chip_name),},
//	{TSP_CMD("get_x_num", get_x_num),},
//	{TSP_CMD("get_y_num", get_y_num),},
//	{TSP_CMD("get_reference", get_reference),},
//	{TSP_CMD("get_cm_abs", get_cm_abs),},
//	{TSP_CMD("get_cm_delta", get_cm_delta),},
//	{TSP_CMD("get_intensity", get_intensity),},
//	{TSP_CMD("run_reference_read", run_reference_read),},
//	{TSP_CMD("run_cm_abs_read", run_cm_abs_read),},
//	{TSP_CMD("run_cm_delta_read", run_cm_delta_read),},
//	{TSP_CMD("run_intensity_read", run_intensity_read),},
	{TSP_CMD("not_support_cmd", not_support_cmd),},
};
static int esd_cnt;
static void mms_report_input_data(struct mms_ts_info *info, u8 sz, u8 *buf);
static void mms_ts_early_suspend(struct early_suspend *h);
static void mms_ts_late_resume(struct early_suspend *h);
static int mms_ts_config(struct mms_ts_info *info);

/* mms_ts_enable - wake-up func (VDD on)  */
static void mms_ts_enable(struct mms_ts_info *info)
{
	if (info->enabled)
		return;
	mutex_lock(&info->lock);

	/*
	gpio_direction_output(info->pdata->gpio_int, 0);
	udelay(5);
	gpio_direction_input(info->pdata->gpio_int);
	*/

	/* use vdd control instead */
	info->pdata->vdd_on(1);
	msleep(50);
	info->enabled = true;
	enable_irq(info->irq);
	mutex_unlock(&info->lock);

}

/* mms_ts_disable - sleep func (VDD off) */
static void mms_ts_disable(struct mms_ts_info *info)
{
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
printk("%s  reboot 1  %d",__func__,__LINE__);
	i2c_smbus_write_byte_data(info->client, MMS_POWER_CONTROL, 1);
	i2c_lock_adapter(adapter);
	msleep(50);
	info->pdata->vdd_on(0);
	msleep(150);

	info->pdata->vdd_on(1);
	msleep(300);

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

	if (gpio_get_value(info->pdata->gpio_int)) // wifi debug interrupt check
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
			while (gpio_get_value(info->pdata->gpio_int))  //wifi debug interrupt check
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
			//	key_code = KEY_MENU;
				key_code = KEY_RECENT;
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
			dev_notice(&client->dev,"P [%2d],([%3d],[%4d]) , major=%d,  pressure=%d",id, x, y, tmp[4], tmp[5]);
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

printk("%s FW %d \n",__func__,__LINE__);
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
		}, {
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
			printk("%s FW %d \n",__func__,__LINE__);

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
	printk("%s FW %d \n",__func__,__LINE__);

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
	printk(" FW VER \n",__func__,__LINE__);
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
 /////// Force Firmware update ///////
static int mms_flash_fw(struct mms_ts_info *info,const u8 *data,int size) 
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
	fw_hdr = (struct mms_bin_hdr *)data; /////// Force Firmware update ///////
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
	//	printk("[TSP] FW name %s \n",fw);
		memset(ver, 0xff, sizeof(ver));
	} else {
		print_hex_dump(KERN_INFO, "mms_ts fw ver : ", DUMP_PREFIX_NONE, 16, 1,
				ver, MAX_SECTION_NUM, false);
	}

	for (i = 0; i < fw_hdr->section_num; i++, offset += sizeof(struct mms_fw_img)) {
		img[i] = (struct mms_fw_img *)(data + offset); /////// Force Firmware update ///////
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

			if ((ret = mms_flash_section(info, img[i], data + fw_hdr->binary_offset))) {
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
	struct i2c_client *client = info->client;
	int retires = 3;
	int ret;
	printk("%s FW %d \n",__func__,__LINE__);
	u8 ver[MAX_SECTION_NUM];
	const struct firmware *fw_sub;
	const char *fw_name = FW_SUB_NAME;
	printk("[TSP] %s \n",FW_SUB_NAME);
	

	if (!fw) {
		dev_err(&info->client->dev, "failed to read firmware\n");
			printk("[TSP] %s      %d\n",__func__,__LINE__);
		complete_all(&info->init_done);
		return;
	}
	get_fw_version(info->client,ver);

	if(ver[2]< 0x50){
		dev_info(&info->client->dev, "old TSP : (version 03) firmware-update\n");
		fw_name = kstrdup(fw_name, GFP_KERNEL);
		ret = request_firmware(&fw_sub, fw_name, &info->client->dev);
		if (!fw_sub) {
			dev_err(&info->client->dev, "failed to read(config-ver 03) firmware\n");
			complete_all(&info->init_done);
			return;
		}

		do {
			ret = mms_flash_fw(info,fw_sub->data,fw_sub->size);
		} while (ret && --retires);

		mms_ts_config(info);
		release_firmware(fw_sub);
		release_firmware(fw);
			printk("[TSP] %s      %d\n",__func__,__LINE__);
		return;
	}

	do {
		ret = mms_flash_fw(info,fw->data,fw->size);
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
printk("%s interrupt call   %d \n",__func__,__LINE__);
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
	printk("%s report event   %d \n",__func__,__LINE__);
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
	struct mms_ts_info *info = input_get_drvdata(dev);
	int ret;
	ret = wait_for_completion_interruptible_timeout(&info->init_done,
			msecs_to_jiffies(90 * MSEC_PER_SEC));

	if (ret > 0) {
		if (info->irq != -1) {
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
printk("%s  config 1 %d \n",__func__,__LINE__);
	struct i2c_client *client = info->client;
	int ret;
	printk("%s  config 2  %d \n",__func__,__LINE__);
	ret = request_threaded_irq(client->irq, NULL, mms_ts_interrupt,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"mms_ts", info);
printk("%s  irq registered  %d\n",__func__,__LINE__);
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
	printk("%s  reboot called  %d\n",__func__,__LINE__);
	mms_reboot(info);
	complete_all(&info->init_done);
	printk("%s  init done  %d\n",__func__,__LINE__);

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
		count = gpio_get_value(info->pdata->gpio_int);  //wifi debug interrupt check
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

static void set_default_result(struct mms_ts_info *info)
{
	char delim = ':';
	printk("[TSP] %s      %d\n",__func__,__LINE__);
	memset(info->cmd_result, 0x00, ARRAY_SIZE(info->cmd_result));
	memcpy(info->cmd_result, info->cmd, strlen(info->cmd));
	strncat(info->cmd_result, &delim, 1);
}

static void set_cmd_result(struct mms_ts_info *info, char *buff, int len)
{
	strncat(info->cmd_result, buff, len);
}
static ssize_t show_close_tsp_test(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);

	//get_raw_data_all(info, MMS_VSC_CMD_EXIT);
	info->ft_flag = 0;

	return snprintf(buf, TSP_BUF_SIZE, "%u\n", 0);
}
static void get_fw_ver_bin(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
        printk("[TSP] %s      %d\n",__func__,__LINE__);
	set_default_result(info);
	snprintf(buff, sizeof(buff), "ME%02x%02x%02x",
		BOOT_VERSION, CORE_VERSION, FW_VERSION);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}
static void fw_update(void *device_data)
{
printk("[TSP] %s %d\n",__func__,__LINE__);
}

static void not_support_cmd(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	char buff[16] = {0};

	set_default_result(info);
	sprintf(buff, "%s", "NA");
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 4;
	dev_info(&info->client->dev, "%s: \"%s(%d)\"\n", __func__,
				buff, strnlen(buff, sizeof(buff)));
	return;
}

static void get_fw_ver_ic(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;
	int ret = 0;
	u8 ver[MAX_SECTION_NUM];
	struct i2c_client *client = info->client;
	u8 fw_ver[3];
	u8 fw_core_ver[3];
	u8 fw_config_ver[3];

	i2c_smbus_read_i2c_block_data(client, MMS_FW_VERSION, 3, (u8 *)fw_ver);
	i2c_smbus_read_i2c_block_data(client, MMS_CORE_VERSION, 3, (u8 *)fw_core_ver);
	i2c_smbus_read_i2c_block_data(client, MMS_CONFIG_VERSION, 3, (u8 *)fw_config_ver);
	//printk("[TSP]Firmware info ME00%02x%02x%02x\n",fw_ver[0],fw_ver[1],fw_ver[2]);
	//printk("[TSP]Firmware core info ME00%02x%02x%02x\n",fw_core_ver[0],fw_core_ver[1],fw_core_ver[2]);
	//printk("[TSP]Firmware config info ME00%02x%02x%02x\n",fw_config_ver[0],fw_config_ver[1],fw_config_ver[2]);
	char buff[16] = {0};
	set_default_result(info);
/*	if (info->enabled)
{
        printk("[TSP] %s      %d\n",__func__,__LINE__);
		get_fw_version(client, ver);
}*/
        printk("[TSP] %s      %d\n",__func__,__LINE__);
	/*snprintf(buff, sizeof(buff), "ME00%02x%02x",
		info->fw_core_ver, info->fw_ic_ver);*/
		snprintf(buff, sizeof(buff), "ME%02x%02x%02x",
		fw_ver[0], fw_core_ver[0],fw_config_ver[0]);
	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}
static void get_config_ver(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[20] = {0};

	set_default_result(info);

	snprintf(buff, sizeof(buff), "KANAS_Me_0528");

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}
static void get_threshold(void *device_data)
{
	struct mms_ts_info *info = (struct mms_ts_info *)device_data;

	char buff[16] = {0};
	int threshold;

	set_default_result(info);

	disable_irq(info->irq);
	threshold = i2c_smbus_read_byte_data(info->client, 0x05);
	enable_irq(info->irq);
	if (threshold < 0) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
		info->cmd_state = 3;
		return;
	}
	snprintf(buff, sizeof(buff), "%d", threshold);

	set_cmd_result(info, buff, strnlen(buff, sizeof(buff)));
	info->cmd_state = 2;
	dev_info(&info->client->dev, "%s: %s(%d)\n", __func__,
			buff, strnlen(buff, sizeof(buff)));
}

//melfas test mode
#ifdef __MMS_TEST_MODE__
struct mms_ts_test test;


static ssize_t store_cmd(struct device *dev, struct device_attribute
		*devattr, const char *buf, size_t count)
{
	if(buf==NULL)
	{
	printk("[TSP] %s      %d\n",__func__,__LINE__);
	return count;
	}
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	char *cur, *start, *end;
	char buff[TSP_CMD_STR_LEN] = {0};
	int len, i;
	struct tsp_cmd *tsp_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;


	if (info->cmd_is_running == true) {
		dev_err(&info->client->dev, "tsp_cmd: other cmd is running.\n");
		goto err_out;
	}


	/* check lock  */
	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = true;
	mutex_unlock(&info->cmd_lock);

	info->cmd_state = 1;

	for (i = 0; i < ARRAY_SIZE(info->cmd_param); i++)
		info->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;
	memset(info->cmd, 0x00, ARRAY_SIZE(info->cmd));
	memcpy(info->cmd, buf, len);

	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(buff, buf, cur - buf);
	else
		memcpy(buff, buf, len);

	/* find command */
	list_for_each_entry(tsp_cmd_ptr, &info->cmd_list_head, list) {
		if (!strcmp(buff, tsp_cmd_ptr->cmd_name)) {
			cmd_found = true;
			break;
		}
	}

	/* set not_support_cmd */
	if (!cmd_found) {
		list_for_each_entry(tsp_cmd_ptr, &info->cmd_list_head, list) {
			if (!strcmp("not_support_cmd", tsp_cmd_ptr->cmd_name))
				break;
		}
	}

	/* parsing parameters */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(buff, 0x00, ARRAY_SIZE(buff));
		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(buff, start, end - start);
				*(buff + strlen(buff)) = '\0';
				if (kstrtoint(buff, 10,
					info->cmd_param + param_cnt) < 0)
					goto err_out;
				start = cur + 1;
				memset(buff, 0x00, ARRAY_SIZE(buff));
				param_cnt++;
			}
			cur++;
		} while (cur - buf <= len);
	}

	dev_info(&client->dev, "cmd = %s\n", tsp_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		dev_info(&client->dev, "cmd param %d= %d\n", i,
							info->cmd_param[i]);

	tsp_cmd_ptr->cmd_func(info);


err_out:
	return count;
}

static ssize_t show_cmd_status(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	char buff[16] = {0};

	dev_info(&info->client->dev, "tsp cmd: status:%d\n", info->cmd_state);

	if (info->cmd_state == 0)
		snprintf(buff, sizeof(buff), "WAITING");

	else if (info->cmd_state == 1)
		snprintf(buff, sizeof(buff), "RUNNING");

	else if (info->cmd_state == 2)
		snprintf(buff, sizeof(buff), "OK");

	else if (info->cmd_state == 3)
		snprintf(buff, sizeof(buff), "FAIL");

	else if (info->cmd_state == 4)
		snprintf(buff, sizeof(buff), "NOT_APPLICABLE");

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", buff);
}

static ssize_t show_cmd_result(struct device *dev, struct device_attribute
		*devattr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);

	dev_info(&info->client->dev, "tsp cmd: result: %s\n", info->cmd_result);

	mutex_lock(&info->cmd_lock);
	info->cmd_is_running = false;
	mutex_unlock(&info->cmd_lock);

	info->cmd_state = 0;

	return snprintf(buf, TSP_BUF_SIZE, "%s\n", info->cmd_result);
}


static ssize_t fw_ver_kernel_temp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 buff;
	
	//Not implemented here yet
	//mms_ts_fw_load_built_in(info);
	//mms_ts_fw_unload_built_in(info);

	buff = info->fw_core_ver;

	printk(&client->dev, "[fw_ver_kernel_temp_show]%s: \"0x%X\"\n", __func__, buff);
	return sprintf(buf, "0x%X\n", buff);
}

static ssize_t fw_ver_ic_temp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 fw_ver[3];

	i2c_smbus_read_i2c_block_data(client, MMS_FW_VERSION, 3, (u8 *)fw_ver);
	printk("[threshold_temp_show] i2c_smbus_read_byte_data...\n");
	printk(&client->dev, "%s: \"0x%X\"\n", __func__, fw_ver[0]);
	return sprintf(buf, "0x%X\n", fw_ver[0]);

}

static ssize_t threshold_temp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 buff;

	buff = i2c_smbus_read_byte_data(info->client, MMS_THRESHOLD);
	printk("[threshold_temp_show] i2c_smbus_read_byte_data...\n");
	printk(&client->dev, "%s: \"%d\"\n", __func__, buff);
	
	return sprintf(buf, "%d\n", buff);
}
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
/////// Force Firmware update ///////
static ssize_t mms_force_update(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
        struct i2c_client *client = to_i2c_client(dev);
	struct file *fp; 
	mm_segment_t old_fs;
	size_t fw_size, nread;
	int error = 0;
	int ret;
	int result = 0;
 	disable_irq(client->irq);
	old_fs = get_fs();
	set_fs(KERNEL_DS);  
 
	fp = filp_open(EXTRA_FW_PATH, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		dev_err(&info->client->dev,
		"%s: failed to open %s.\n", __func__, EXTRA_FW_PATH);
		error = -ENOENT;
		goto open_err;
	}
 	fw_size = fp->f_path.dentry->d_inode->i_size;
	if (0 < fw_size) {
		unsigned char *fw_data;
		fw_data = kzalloc(fw_size, GFP_KERNEL);
		nread = vfs_read(fp, (char __user *)fw_data,fw_size, &fp->f_pos);
		dev_info(&info->client->dev,
		"%s: start, file path %s, size %u Bytes\n", __func__,EXTRA_FW_PATH, fw_size);
		if (nread != fw_size) {
			    dev_err(&info->client->dev,
			    "%s: failed to read firmware file, nread %u Bytes\n", __func__, nread);
		    error = -EIO;
		} else{
			result=mms_flash_fw(info,fw_data,fw_size);
		}
		kfree(fw_data);
	}
 	filp_close(fp, current->files);
	ret = snprintf(buf,PAGE_SIZE,"%s\n",info->get_data);
	memset(info->get_data,0,4096);
open_err:
	enable_irq(client->irq);
	set_fs(old_fs);
	return result;
}
static DEVICE_ATTR(force_update, 0666, mms_force_update, NULL);
/////// Force Firmware update ///////
static struct attribute *mms_attrs[] = {
	&dev_attr_force_update.attr, /////// Force Firmware update ///////
	&dev_attr_intensity.attr,
	&dev_attr_cmdelta.attr,
	&dev_attr_cmabs.attr,
	&dev_attr_cmjitter.attr,
	NULL,
};

static const struct attribute_group mms_attr_group = {
	.attrs = mms_attrs,
};


static DEVICE_ATTR(close_tsp_test, S_IRUGO, show_close_tsp_test, NULL);
static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, store_cmd);
static DEVICE_ATTR(cmd_status, S_IRUGO, show_cmd_status, NULL);
static DEVICE_ATTR(cmd_result, S_IRUGO, show_cmd_result, NULL);


static struct attribute *sec_touch_factory_attributes[] = {
		&dev_attr_close_tsp_test.attr,
		&dev_attr_cmd.attr,
		&dev_attr_cmd_status.attr,
		&dev_attr_cmd_result.attr,

		NULL,
};

static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_factory_attributes,
};

/* added the tsp firmware version and panel info attribute, to be registered in probe.*/
static DEVICE_ATTR(tsp_firm_version_panel, S_IRUGO, fw_ver_ic_temp_show, NULL);
static DEVICE_ATTR(tsp_firm_version_phone, S_IRUGO, fw_ver_kernel_temp_show,
									NULL);
static DEVICE_ATTR(tsp_threshold, S_IRUGO, threshold_temp_show, NULL);


static struct attribute *touchscreen_temp_attributes[] = {
	&dev_attr_tsp_firm_version_panel.attr,
	&dev_attr_tsp_firm_version_phone.attr,
	&dev_attr_tsp_threshold.attr,
	NULL,
};

static struct attribute_group touchscreen_temp_attr_group = {
	.attrs = touchscreen_temp_attributes,
};

#endif
static int __init mms_ts_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mms_ts_info *info;
	struct input_dev *input_dev;
	int ret = 0;
	int i;
	
	struct device *fac_dev_ts;
	struct device *fac_dev_temp_ts;
	
	const char *fw_name = FW_NAME;
	printk("probe enter \n");


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
printk("probe mutex init \n");
	mutex_init(&info->lock);

	client->irq = gpio_to_irq(info->pdata->gpio_int);
	input_mt_init_slots(input_dev, MAX_FINGER_NUM,0);

	snprintf(info->phys, sizeof(info->phys),
		"%s/input0", dev_name(&client->dev));

	input_dev->name = "Melfas MMS100s Touch Screen";
	input_dev->phys = info->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

printk("probe dev init \n");
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, MAX_WIDTH, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, MAX_PRESSURE, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, info->pdata->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, info->pdata->max_y, 0, 0);

	input_set_drvdata(input_dev, info);

#if MMS_HAS_TOUCH_KEY
	__set_bit(EV_KEY, input_dev->evbit);
	//__set_bit(KEY_MENU, input_dev->keybit);
	__set_bit(KEY_RECENT, input_dev->keybit);
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
				   IRQF_TRIGGER_LOW  | IRQF_ONESHOT,
				  " mms_ts", info);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to register interrupt\n");
	}
	info->irq = client->irq;
	udelay(150);	

printk("%s  enable FW %d\n",__func__,__LINE__);
	info->fw_name = kstrdup(fw_name, GFP_KERNEL);
printk("%s  enable tsp  %d\n",__func__,__LINE__);
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
printk("%s  ALLOCH  %d\n",__func__,__LINE__);
	if (alloc_chrdev_region(&info->mms_dev, 0, 1, "mms_ts")) {
		dev_err(&client->dev, "failed to allocate device region\n");
		return -ENOMEM;
	}
printk("%s  FOPS  %d\n",__func__,__LINE__);
	cdev_init(&info->cdev, &mms_fops);
	info->cdev.owner = THIS_MODULE;

	if (cdev_add(&info->cdev, info->mms_dev, 1)) {
		dev_err(&client->dev, "failed to add ch dev\n");
		return -EIO;
	}
printk("%s  CLASS CREATE  %d\n",__func__,__LINE__);
//device_create(sec_class, NULL, info->mms_dev, NULL, "mms_ts");
	printk("%s  device CREATE 1  %d\n",__func__,__LINE__);
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
	printk("%s  sysfs CREATE 1  %d\n",__func__,__LINE__);
	INIT_LIST_HEAD(&info->cmd_list_head);
	for (i = 0; i < ARRAY_SIZE(tsp_cmds); i++)
		list_add_tail(&tsp_cmds[i].list, &info->cmd_list_head);

	mutex_init(&info->cmd_lock);
	info->cmd_is_running = false;
	
	fac_dev_ts = device_create(sec_class,
			NULL, 0 , info, "tsp");
	printk("%s  device CREATE 2  %d\n",__func__,__LINE__);
	if (IS_ERR(fac_dev_ts))
		dev_err(&client->dev, "Failed to create device for the sysfs\n");

	ret = sysfs_create_group(&fac_dev_ts->kobj,
			       &sec_touch_factory_attr_group);
	printk("%s  sysfs CREATE 2  %d\n",__func__,__LINE__);
	/*if (ret < 0) {
		dev_err(&client->dev, "Failed to create sysfs group\n");
		goto err_sysfs_create_group_touch;
	}*/
	/* the device attrbutes for the firmware version phonel, panel*/
	fac_dev_temp_ts = device_create(sec_class, NULL, (dev_t)NULL, info,
							"sec_touchscreen");
	printk("%s  device CREATE 3  %d\n",__func__,__LINE__);
							
	if (IS_ERR(fac_dev_temp_ts)) {
		dev_err(&client->dev, "Failed to create fac tsp temp dev\n");
		ret = -ENODEV;
		fac_dev_temp_ts = NULL;
	}
	ret = sysfs_create_group(&fac_dev_temp_ts->kobj,
						&touchscreen_temp_attr_group);
	printk("%s  sysfs CREATE 3  %d\n",__func__,__LINE__);
	/*if (ret) {
		dev_err(&client->dev,
			"Failed to create sysfs (touchscreen_temp_attr_group)."
									"\n");
		ret = (ret > 0) ? -ret : ret;
		goto err_sysfs_create_group_touch_temp;
	}*/
	
	test.get_data = kzalloc(4096, GFP_KERNEL);
	test.client = client;
	test.pdata = info->pdata;
	test.tx_num = info->tx_num;
	test.rx_num = info->rx_num;
	test.key_num = info->key_num;
	test.irq = info->irq;
#endif

/*remove the temp attribute group for touchscreen firmware version and panel info*/
/*err_sysfs_create_group_touch_temp:
	sysfs_remove_group(&fac_dev_temp_ts->kobj,
				&touchscreen_temp_attr_group);

err_sysfs_create_group_touch:
	sysfs_remove_group(&fac_dev_ts->kobj,
				&sec_touch_factory_attr_group);*/

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


static struct i2c_driver mms_ts_driver = {
	.probe		= mms_ts_probe,
	.remove		= mms_ts_remove,
	.driver		= {
				.name	= "mms_ts",
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
				.pm	= &mms_ts_pm_ops,
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

