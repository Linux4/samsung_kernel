/* a96t396.c -- Linux driver for a96t396 chip as grip sensor
 *
 * Copyright (C) 2017 Samsung Electronics Co.Ltd
 * Author: YunJae Hwang <yjz.hwang@samsung.com>
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
#include <linux/pm_wakeup.h>
#include <asm/unaligned.h>
#include <linux/regulator/consumer.h>
#include <linux/sec_class.h>
#include <linux/pinctrl/consumer.h>
#if defined(CONFIG_SENSORS_CORE_AP)
#include <linux/sensor/sensors_core.h>
#endif
#if IS_ENABLED(CONFIG_CCIC_NOTIFIER) || IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
#include <linux/fs.h>
#include <linux/miscdevice.h>
#endif
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
#include <linux/hall/hall_ic_notifier.h>
#define HALL_NAME		"hall"
#define HALL_CERT_NAME		"certify_hall"
#define HALL_FLIP_NAME		"flip"
#define HALL_ATTACH		1
#define HALL_DETACH		0
#endif

#if IS_ENABLED(CONFIG_TABLET_MODEL_CONCEPT)
#if IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V3)
#include "../input/sec_input/stm32/pogo_notifier_v3.h"
#elif IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V2) || IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO)
#include <linux/input/pogo_i2c_notifier.h>
#endif
#endif

#include "a96t396.h"

#define SENSOR_ATTR_SIZE 55

#define TYPE_USB   1
#define TYPE_HALL  2
#define TYPE_BOOT  3
#define TYPE_FORCE 4
#define TYPE_COVER 5

#define SHCEDULE_INTERVAL       2

#ifdef CONFIG_SENSORS_FW_VENDOR
static struct delayed_work* gp_fw_work[GRIP_MAX_CNT];
static int probe_count;
#endif

#ifdef CONFIG_SENSORS_A96T396_2CH
struct multi_channel {
	u16 grip_p_thd_2ch;
	u16 grip_r_thd_2ch;
	u16 grip_n_thd_2ch;
	u16 grip_baseline_2ch;
	u16 grip_raw_2ch;
	u16 grip_raw_d_2ch;
	u16 diff_2ch;
	u16 diff_d_2ch;
	u16 grip_event_2ch;

	s16 max_diff_2ch;
	s16 max_normal_diff_2ch;

	int noti_enable;
	int is_unknown_mode;
	bool first_working;
};
#endif


struct a96t396_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct input_dev *noti_input_dev;
	struct device *dev;
	struct mutex lock;
	struct work_struct pdic_attach_reset_work;
	struct work_struct pdic_detach_reset_work;
	struct work_struct reset_work;
	struct delayed_work debug_work;
	struct delayed_work firmware_work;
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	struct miscdevice miscdev;
#endif

	const struct firmware *firm_data_bin;
	const u8 *firm_data_ums;
	long firm_size;
	int irq;
	struct wakeup_source *grip_ws;
	u16 grip_p_thd;
	u16 grip_r_thd;
	u16 grip_n_thd;
	u16 grip_baseline;
	u16 grip_raw;
	u16 grip_raw_d;
	u16 grip_event;
	u16 diff;
	u16 diff_d;
	bool current_state;
	bool expect_state;
#ifdef CONFIG_SEC_FACTORY
	int irq_count;
	int abnormal_mode;
	s16 max_diff;
	s16 max_normal_diff;
#endif
	int irq_en_cnt;
	u8 buf;
	u8 ic_num;

#ifdef CONFIG_SENSORS_A96T396_2CH
	struct multi_channel *mul_ch;
#endif

	u8 fw_update_state;
	u8 fw_ver;
	u8 md_ver;
	u8 fw_ver_bin;
	u8 md_ver_bin;
	u8 checksum_h;
	u8 checksum_h_bin;
	u8 checksum_l;
	u8 checksum_l_bin;
	bool enabled;
	bool skip_event;
	bool resume_called;

	int ldo_en;			/* ldo_en pin gpio */
	int grip_int;			/* irq pin gpio */
	const char *dvdd_vreg_name;	/* regulator name */
	struct regulator *dvdd_vreg;	/* regulator */
	int (*power)(void *, bool on);	/* power onoff function ptr */
	const char *fw_path;
	bool probe_done;
	int firmup_cmd;
	int debug_count;
	int firmware_count;
	bool crc_check;
	int multi_use;
	int unknown_ch_selection;
#if IS_ENABLED(CONFIG_CCIC_NOTIFIER) || IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	struct notifier_block pdic_nb;
	int pdic_status;
	int pdic_pre_attach;
	int pre_attach;
	int pre_otg_attach;
#endif
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
	struct notifier_block hall_nb;
#endif
#if IS_ENABLED(CONFIG_TABLET_MODEL_CONCEPT)
#if IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V3) || IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V2) || IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO)
	struct notifier_block pogo_nb;
	struct delayed_work init_work;
#endif
#endif
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	int ioctl_pass;

	u8 read_reg;
	u8 read_reg_count;
	int read_flag;
#endif
	int noti_enable;
	int is_unknown_mode;
	int motion;
	bool first_working;
};

static void a96t396_check_first_working(struct a96t396_data *data);
#ifdef CONFIG_SENSORS_A96T396_2CH
static void a96t396_2ch_check_first_working(struct a96t396_data *data);
#endif
static void a96t396_reset(struct a96t396_data *data);
static void a96t396_diff_getdata(struct a96t396_data *data, bool log);
#ifdef CONFIG_SENSORS_A96T396_2CH
static void a96t396_2ch_diff_getdata(struct a96t396_data *data, bool log);
#endif
static void a96t396_check_first_status(struct a96t396_data *data, int enable);
#ifdef CONFIG_SENSORS_FW_VENDOR
static int a96t396_fw_check(struct a96t396_data *data);
static void a96t396_set_firmware_work(struct a96t396_data *data, u8 enable,
		unsigned int time_ms);
#endif

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP

static long a96t396_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int a96t396_open(struct inode *inode, struct file *file);
static int a96t396_release(struct inode *inode, struct file *file);


static const struct file_operations fwload_fops = {
	.owner =		THIS_MODULE,
	.compat_ioctl =	a96t396_ioctl,
	.unlocked_ioctl =	a96t396_ioctl,
	.open =			a96t396_open,
	.release =		a96t396_release
};
#endif

static int a96t396_i2c_read(struct i2c_client *client,
	u8 reg, u8 *val, unsigned int len)
{
	struct a96t396_data *data = i2c_get_clientdata(client);
	struct i2c_msg msg;
	int ret;
	int retry = 3;

	mutex_lock(&data->lock);
	msg.addr = client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 1;
	msg.buf = &reg;
	while (retry--) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0)
			break;

		GRIP_ERR("fail(address set)(%d)(%d)\n", retry, ret);
		usleep_range(10000, 11000);
	}
	if (ret < 0) {
		mutex_unlock(&data->lock);
		return ret;
	}
	retry = 3;
	msg.flags = 1;/*I2C_M_RD*/
	msg.len = len;
	msg.buf = val;
	while (retry--) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0) {
			mutex_unlock(&data->lock);
			return 0;
		}
		GRIP_ERR("fail(data read)(%d)(%d)\n", retry, ret);
		usleep_range(10000, 11000);
	}
	mutex_unlock(&data->lock);
	return ret;
}

static int a96t396_i2c_read_data(struct i2c_client *client, u8 *val,
	unsigned int len)
{
	struct a96t396_data *data = i2c_get_clientdata(client);
	struct i2c_msg msg;
	int ret;
	int retry = 3;

	mutex_lock(&data->lock);
	msg.addr = client->addr;
	msg.flags = 1;/*I2C_M_RD*/
	msg.len = len;
	msg.buf = val;
	while (retry--) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0) {
			mutex_unlock(&data->lock);
			return 0;
		}
		GRIP_ERR("fail(data read)(%d)\n", retry);
		usleep_range(10000, 11000);
	}
	mutex_unlock(&data->lock);
	return ret;
}

static int a96t396_i2c_write(struct i2c_client *client, u8 reg, u8 *val)
{
	struct a96t396_data *data = i2c_get_clientdata(client);
	struct i2c_msg msg[1];
	unsigned char buf[2];
	int ret;
	int retry = 3;

	mutex_lock(&data->lock);
	buf[0] = reg;
	buf[1] = *val;
	msg->addr = client->addr;
	msg->flags = I2C_M_WR;
	msg->len = 2;
	msg->buf = buf;

	while (retry--) {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (ret >= 0) {
			mutex_unlock(&data->lock);
			return 0;
		}
		GRIP_ERR("fail(%d)\n", retry);
		usleep_range(10000, 11000);
	}
	mutex_unlock(&data->lock);
	return ret;
}


 // @enable: turn it on or off.

static void a96t396_set_enable(struct a96t396_data *data, int enable)
{
	u8 cmd;
	int ret;

	if (data->current_state == enable) {
		GRIP_INFO("old = %d, new = %d, skip exception case\n",
			data->current_state, enable);
		return;
	}

	GRIP_INFO("old enable = %d, new enable = %d\n",
				data->current_state, enable);
	if (enable) {
		data->buf = 0;
		cmd = CMD_ON;

		ret = a96t396_i2c_write(data->client, REG_SAR_ENABLE, &cmd);
		if (ret < 0)
			GRIP_ERR("failed to enable grip irq\n");

		a96t396_check_first_status(data, enable);

		enable_irq(data->irq);
		enable_irq_wake(data->irq);

		data->irq_en_cnt++;

	} else {
		cmd = CMD_OFF;

		disable_irq_wake(data->irq);
		disable_irq(data->irq);

		ret = a96t396_i2c_write(data->client, REG_SAR_ENABLE, &cmd);
		if (ret < 0)
			GRIP_ERR("failed to disable grip irq\n");
	}
	data->current_state = enable;
}

static void grip_always_active(struct a96t396_data *data, int on)
{
	int ret;
	u8 cmd, r_buf;

	GRIP_INFO("Grip always active mode %d\n", on);

	if (on == 1)
		cmd = GRIP_ALWAYS_ACTIVE_ENABLE;
	else
		cmd = GRIP_ALWAYS_ACTIVE_DISABLE;

	ret = a96t396_i2c_write(data->client, REG_GRIP_ALWAYS_ACTIVE, &cmd);
	if (ret < 0) {
		GRIP_ERR("i2c write fail(%d)\n", ret);
		return;
	}

	msleep(20);

	ret = a96t396_i2c_read(data->client, REG_GRIP_ALWAYS_ACTIVE, &r_buf, 1);
	if (ret < 0) {
		GRIP_ERR("i2c read fail(%d)\n", ret);
		return;
	}

	if ((cmd == GRIP_ALWAYS_ACTIVE_ENABLE && r_buf == GRIP_ALWAYS_ACTIVE_ENABLE) ||
			(cmd == GRIP_ALWAYS_ACTIVE_DISABLE && r_buf == GRIP_ALWAYS_ACTIVE_DISABLE))
		GRIP_INFO("cmd 0x%x, return 0x%x\n", cmd, r_buf);
	else
		GRIP_INFO("always_active set fail cmd : 0x%x, Wrong value 0x%x\n", cmd, r_buf);
}

static void a96t396_reset_for_bootmode(struct a96t396_data *data)
{
	GRIP_INFO("\n");

	data->power(data, false);
	usleep_range(50000, 50010);
	data->power(data, true);
}

static void a96t396_reset(struct a96t396_data *data)
{
	if (data->enabled == false)
		return;

	GRIP_INFO("start\n");
	disable_irq_nosync(data->irq);

	data->enabled = false;

	a96t396_reset_for_bootmode(data);
	usleep_range(RESET_DELAY, RESET_DELAY);

	if (data->current_state)
		a96t396_set_enable(data, 1);

	data->enabled = true;

	GRIP_INFO("done\n");
}

static void a96t396_diff_getdata(struct a96t396_data *data, bool log)
{
	int ret;
	u8 r_buf[2] = {0,};

	ret = a96t396_i2c_read(data->client, REG_SAR_DIFFDATA, r_buf, 2);
	if (ret < 0)
		GRIP_ERR("i2c read failed\n");

	data->diff = (r_buf[0] << 8) | r_buf[1];

	if (log)
		GRIP_INFO("%u\n", data->diff);
}

#ifdef CONFIG_SENSORS_A96T396_2CH
static void a96t396_2ch_diff_getdata(struct a96t396_data *data, bool log)
{
	int ret;
	u8 r_buf[2] = {0,};

	ret = a96t396_i2c_read(data->client, REG_SAR_DIFFDATA_D_2CH, r_buf, 2);
	if (ret < 0)
		GRIP_ERR("i2c read failed\n");

	data->mul_ch->diff_2ch = (r_buf[0] << 8) | r_buf[1];

	if (log)
		GRIP_INFO("2ch %u\n", data->mul_ch->diff_2ch);
}
#endif

static void a96t396_enter_unknown_mode(struct a96t396_data *data, int type)
{
	int enable = 0;

	if (data->noti_enable && !data->skip_event) {
		enable = data->noti_enable;
		data->motion = 0;
		data->first_working = false;
		if (data->is_unknown_mode == UNKNOWN_OFF) {
			data->is_unknown_mode = UNKNOWN_ON;
			input_report_rel(data->input_dev, REL_X, data->is_unknown_mode);
			input_sync(data->input_dev);
			GRIP_INFO("UNKNOWN Re-enter\n");
		} else {
			GRIP_INFO("already UNKNOWN\n");
		}
	}
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use) {
		if (data->mul_ch->noti_enable && !data->skip_event) {
			data->motion = 0;
			enable = data->mul_ch->noti_enable;
			data->mul_ch->first_working = false;
			if (data->mul_ch->is_unknown_mode == UNKNOWN_OFF) {
				data->mul_ch->is_unknown_mode = UNKNOWN_ON;
				input_report_rel(data->input_dev, REL_Y, data->mul_ch->is_unknown_mode);
				input_sync(data->input_dev);
				GRIP_INFO("2ch UNKNOWN Re-enter\n");
			} else {
				GRIP_INFO("2ch already UNKNOWN\n");
			}
		}
	}
#endif
	if (enable) {
		GRIP_INFO("enable : %d\n", enable);
		input_report_rel(data->noti_input_dev, REL_X, type);
		input_sync(data->noti_input_dev);
	}
}

static void a96t396_check_first_status(struct a96t396_data *data, int enable)
{
	u8 r_buf[2];
	u16 grip_thd;
#ifdef CONFIG_SENSORS_A96T396_2CH
	u16 grip_thd_2ch;
#endif
	if (data->skip_event == true) {
		GRIP_INFO("skip event..\n");
		return;
	}

	a96t396_i2c_read(data->client, REG_SAR_THRESHOLD, r_buf, 2);
	grip_thd = (r_buf[0] << 8) | r_buf[1];

	a96t396_diff_getdata(data, true);
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use) {
		a96t396_i2c_read(data->client, REG_SAR_THRESHOLD_2CH, r_buf, 2);
		grip_thd_2ch = (r_buf[0] << 8) | r_buf[1];

		a96t396_2ch_diff_getdata(data, true);
	}
#endif
	if (grip_thd < data->diff)
		input_report_rel(data->input_dev, REL_MISC, 1);
	else
		input_report_rel(data->input_dev, REL_MISC, 2);
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use) {
		if (grip_thd_2ch < data->mul_ch->diff_2ch)
			input_report_rel(data->input_dev, REL_DIAL, 1);
		else
			input_report_rel(data->input_dev, REL_DIAL, 2);
	}
#endif
	input_report_rel(data->input_dev, REL_X, data->is_unknown_mode);
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use)
		input_report_rel(data->input_dev, REL_Y, data->mul_ch->is_unknown_mode);
#endif
	input_sync(data->input_dev);
}

static void a96t396_check_diff_and_cap(struct a96t396_data *data)
{
	u8 r_buf[2] = {0, 0};
	int ret;
	int value = 0;

	ret = a96t396_i2c_read(data->client, REG_SAR_TOTALCAP_READ, r_buf, 2);
	if (ret < 0)
		GRIP_ERR("fail(%d)\n", ret);

	value = (r_buf[0] << 8) | r_buf[1];
	GRIP_INFO("Cap Read %d\n", value);

	a96t396_diff_getdata(data, true);
}

static void pdic_attach_reset_work_func(struct work_struct *work)
{
	struct a96t396_data *data = container_of((struct work_struct *)work,
						struct a96t396_data, pdic_attach_reset_work);
	u8 cmd = CMD_OFF;

	a96t396_i2c_write(data->client, REG_TSPTA, &cmd);

	GRIP_INFO("pdic_attach");
}

static void pdic_detach_reset_work_func(struct work_struct *work)
{
	struct a96t396_data *data = container_of((struct work_struct *)work,
						struct a96t396_data, pdic_detach_reset_work);
	u8 cmd = CMD_ON;

	a96t396_i2c_write(data->client, REG_TSPTA, &cmd);

	GRIP_INFO("pdic_detach");

}

static void a96t396_grip_sw_reset(struct a96t396_data *data)
{
	int ret;
	u8 cmd = CMD_SW_RESET;

	GRIP_INFO("\n");

	a96t396_check_diff_and_cap(data);
	usleep_range(10000, 10010);

	ret = a96t396_i2c_write(data->client, REG_SW_RESET, &cmd);
	if (ret < 0)
		GRIP_ERR("fail(%d)\n", ret);
	else
		usleep_range(35000, 35010);
}

static void reset_work_func(struct work_struct *work)
{
	struct a96t396_data *data = container_of((struct work_struct *)work,
						struct a96t396_data, reset_work);

	a96t396_grip_sw_reset(data);
}

#ifdef CONFIG_SENSORS_FW_VENDOR
static void a96t396_firmware_work_func(struct work_struct *work)
{
	struct a96t396_data *data = container_of((struct delayed_work *)work,
		struct a96t396_data, firmware_work);
	int ret;
	int next_idx = data->ic_num;

	GRIP_INFO("called\n");

	if (probe_count <= GRIP_MAX_CNT) {
		if (probe_count == GRIP_MAX_CNT) {
			GRIP_INFO("All chip probe sequences are complete!\n");
			probe_count = GRIP_MAX_CNT + 1;
		}
		schedule_delayed_work(&data->firmware_work,
				msecs_to_jiffies(500));
		return;
	}

	ret = a96t396_fw_check(data);
	if (ret) {
		if (data->firmware_count++ < FIRMWARE_VENDOR_CALL_CNT) {
			GRIP_ERR("failed to load firmware (%d)\n",
				data->firmware_count);
			schedule_delayed_work(&data->firmware_work,
					msecs_to_jiffies(500));
			return;
		}
		GRIP_ERR("final retry failed\n");
	} else {
		GRIP_INFO("fw check success\n");
		gp_fw_work[data->ic_num] = NULL;
	}

	while (next_idx < GRIP_MAX_CNT - 1) {
		next_idx = next_idx + 1;
		if (gp_fw_work[next_idx] == NULL) {
			GRIP_INFO("skip GRIP[%d] fw download\n", next_idx);
		} else {
			GRIP_INFO("schedule GRIP[%d] fw download\n", next_idx);
			schedule_delayed_work(gp_fw_work[next_idx],
				msecs_to_jiffies(500));
			break;
		}
	}
}
#endif
static void a96t396_debug_work_func(struct work_struct *work)
{
	struct a96t396_data *data = container_of((struct delayed_work *)work,
		struct a96t396_data, debug_work);

	if (data->resume_called == true) {
		data->resume_called = false;
		schedule_delayed_work(&data->debug_work, msecs_to_jiffies(1000));
		return;
	}

	if (data->current_state) {
#ifdef CONFIG_SEC_FACTORY
		if (data->abnormal_mode) {
			a96t396_diff_getdata(data, true);
			if (data->max_normal_diff < data->diff)
				data->max_normal_diff = data->diff;
#ifdef CONFIG_SENSORS_A96T396_2CH
			if (data->multi_use) {
				a96t396_2ch_diff_getdata(data, true);
				if (data->mul_ch->max_normal_diff_2ch < data->mul_ch->diff_2ch)
					data->mul_ch->max_normal_diff_2ch = data->mul_ch->diff_2ch;
			}
#endif
		} else {
#endif
			if (data->debug_count >= GRIP_LOG_TIME) {
				a96t396_diff_getdata(data, true);
				if (data->is_unknown_mode == UNKNOWN_ON && data->motion)
					a96t396_check_first_working(data);
#ifdef CONFIG_SENSORS_A96T396_2CH
				if (data->multi_use) {
					a96t396_2ch_diff_getdata(data, true);
					if (data->mul_ch->is_unknown_mode == UNKNOWN_ON && data->motion)
						a96t396_2ch_check_first_working(data);
				}
#endif
				data->debug_count = 0;
			} else {
				if (data->is_unknown_mode == UNKNOWN_ON && data->motion) {
					a96t396_diff_getdata(data, false);
					a96t396_check_first_working(data);
				}
#ifdef CONFIG_SENSORS_A96T396_2CH
				if (data->multi_use) {
					if (data->mul_ch->is_unknown_mode == UNKNOWN_ON && data->motion) {
						a96t396_2ch_diff_getdata(data, true);
						a96t396_2ch_check_first_working(data);
					}
				}
#endif

				data->debug_count++;
			}
#ifdef CONFIG_SEC_FACTORY
		}
#endif
	}

	schedule_delayed_work(&data->debug_work, msecs_to_jiffies(2000));
}

static void a96t396_set_debug_work(struct a96t396_data *data, u8 enable,
	unsigned int time_ms)
{
	GRIP_INFO("enable = %d\n", enable);

	if (enable == 1) {
		data->debug_count = 0;
		schedule_delayed_work(&data->debug_work,
			msecs_to_jiffies(time_ms));
	} else {
		cancel_delayed_work_sync(&data->debug_work);
	}
}
#ifdef CONFIG_SENSORS_FW_VENDOR
static void a96t396_set_firmware_work(struct a96t396_data *data, u8 enable,
	unsigned int time_ms)
{
	GRIP_INFO("%s\n", __func__, enable ? "enabled" : "disabled");

	if (enable == 1) {
		data->firmware_count = 0;
		schedule_delayed_work(&data->firmware_work,
			msecs_to_jiffies(time_ms * 1000));
	} else {
		cancel_delayed_work_sync(&data->firmware_work);
	}
}
#endif
static irqreturn_t a96t396_interrupt(int irq, void *dev_id)
{
	struct a96t396_data *data = dev_id;
	struct i2c_client *client = data->client;
	int ret;
	u8 buf;
	int grip_data = 0;
	u8 grip_press = 0;
#ifdef CONFIG_SENSORS_A96T396_2CH
	int grip_data_2ch = 0;
	u8 grip_press_2ch = 0;
#endif

	__pm_stay_awake(data->grip_ws);

	ret = a96t396_i2c_read(client, REG_BTNSTATUS, &buf, 1);
	if (ret < 0) {
		GRIP_ERR("Failed to get status value. reset!\n");
		a96t396_reset(data);
		__pm_relax(data->grip_ws);
		return IRQ_HANDLED;
	}

	GRIP_INFO("buf = 0x%02x\n", buf);

	if (data->buf != buf) {
		grip_data = (buf ^ data->buf) & 0x01;
		grip_press = buf & 0x01;
#ifdef CONFIG_SENSORS_A96T396_2CH
		if (data->multi_use) {
			grip_data_2ch = (buf ^ data->buf) & 0x02;
			grip_press_2ch = (buf & 0x02) / 2;
			GRIP_INFO("data: %d, data_2: %d, press : %d, press_2 : %d\n",
						grip_data, grip_data_2ch, grip_press, grip_press_2ch);
		}
#endif
		data->buf = buf;
	} else {
		GRIP_ERR("int was generated, but data is same\n");
	}
	if (grip_data) {
		if (data->skip_event) {
			GRIP_INFO("int was generated, but event skipped\n");
		} else {
			if (grip_press) {
				input_report_rel(data->input_dev, REL_MISC, 1);
				if (data->is_unknown_mode == UNKNOWN_ON && data->motion)
					data->first_working = true;
			} else {
				input_report_rel(data->input_dev, REL_MISC, 2);
				if (data->is_unknown_mode == UNKNOWN_ON && data->motion) {
					if (data->first_working) {
						GRIP_INFO("unknown mode off\n");
						data->is_unknown_mode = UNKNOWN_OFF;
					}
				}
			}
			input_report_rel(data->input_dev, REL_X, data->is_unknown_mode);

			input_sync(data->input_dev);
			data->grip_event = grip_press;
		}
	}
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use) {
		if (grip_data_2ch) {
			if (data->skip_event) {
				GRIP_INFO("2ch int was generated, but event skipped\n");
			} else {
				if (grip_press_2ch) {
					if (data->mul_ch->is_unknown_mode == UNKNOWN_ON && data->motion)
						data->mul_ch->first_working = true;
					input_report_rel(data->input_dev, REL_DIAL, 1);
				} else {
					input_report_rel(data->input_dev, REL_DIAL, 2);
					if (data->mul_ch->is_unknown_mode == UNKNOWN_ON && data->motion) {
						if (data->mul_ch->first_working) {
							GRIP_INFO("unknown mode off\n");
							data->mul_ch->is_unknown_mode = UNKNOWN_OFF;
						}
					}
				}
				input_report_rel(data->input_dev, REL_Y, data->mul_ch->is_unknown_mode);
				input_sync(data->input_dev);
				data->mul_ch->grip_event_2ch = grip_press_2ch;
			}
		}
	}
#endif
	a96t396_diff_getdata(data, true);
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use)
		a96t396_2ch_diff_getdata(data, true);
#endif

#ifdef CONFIG_SEC_FACTORY
	if (data->abnormal_mode) {
		if (data->grip_event) {
			if (data->max_diff < data->diff)
				data->max_diff = data->diff;
			data->irq_count++;
		}
#ifdef CONFIG_SENSORS_A96T396_2CH
		if (data->multi_use) {
			if (data->mul_ch->grip_event_2ch) {
				if (data->mul_ch->max_diff_2ch < data->mul_ch->diff_2ch)
					data->mul_ch->max_diff_2ch = data->mul_ch->diff_2ch;
				data->irq_count++;
			}
		}
#endif
	}
#endif
	if (grip_data)
		GRIP_INFO("%s %x\n", grip_press ? "grip P" : "grip R", buf);
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use) {
		if (grip_data_2ch)
			GRIP_INFO("2ch %s %x\n", grip_press_2ch ? "grip P" : "grip R", buf);
	}
#endif
	__pm_relax(data->grip_ws);
	return IRQ_HANDLED;
}

static int a96t396_get_raw_data(struct a96t396_data *data)
{
	int ret;
	u8 r_buf[4] = {0,};

	ret = a96t396_i2c_read(data->client, REG_SAR_RAWDATA, r_buf, 2);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		data->grip_raw = 0;
		return ret;
	}

	data->grip_raw = (r_buf[0] << 8) | r_buf[1];

	GRIP_INFO("grip_raw = %d\n", data->grip_raw);

	return ret;
}

#ifdef CONFIG_SENSORS_A96T396_2CH
static int a96t396_get_2ch_raw_data(struct a96t396_data *data)
{
	int ret;
	u8 r_buf[4] = {0,};

	ret = a96t396_i2c_read(data->client, REG_SAR_RAWDATA_2CH, r_buf, 2);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		data->mul_ch->grip_raw_2ch = 0;
		return ret;
	}

	data->mul_ch->grip_raw_2ch = (r_buf[0] << 8) | r_buf[1];

	GRIP_INFO("2ch grip_raw = %d\n", data->mul_ch->grip_raw_2ch);

	return ret;
}
#endif

static ssize_t grip_sar_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", !data->skip_event);
}

static ssize_t grip_sar_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	int ret, enable;

	ret = sscanf(buf, "%2d", &enable);
	if (ret != 1) {
		GRIP_ERR("cmd read err\n");
		return count;
	}

	if (!(enable >= 0 && enable <= 3)) {
		GRIP_ERR("wrong command(%d)\n", enable);
		return count;
	}

	GRIP_INFO("enable = %d\n", enable);

	/* enable 0:off, 1:on, 2:skip event , 3:cancel skip event */
	if (enable == 2) {
		data->skip_event = true;
		data->motion = 1;
		data->is_unknown_mode = UNKNOWN_OFF;
		data->first_working = false;
		input_report_rel(data->input_dev, REL_MISC, 2);
		input_report_rel(data->input_dev, REL_X, UNKNOWN_OFF);
#ifdef CONFIG_SENSORS_A96T396_2CH
		if (data->multi_use) {
			data->mul_ch->is_unknown_mode = UNKNOWN_OFF;
			data->mul_ch->first_working = false;
			input_report_rel(data->input_dev, REL_DIAL, 2);
			input_report_rel(data->input_dev, REL_Y, UNKNOWN_OFF);

		}
#endif
		input_sync(data->input_dev);
	} else if (enable == 3) {
		data->skip_event = false;
	} else {
		a96t396_set_enable(data, enable);
	}

	return count;
}

static ssize_t grip_threshold_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	u8 r_buf[4];
	int ret;

	ret = a96t396_i2c_read(data->client, REG_SAR_THRESHOLD, r_buf, 2);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		data->grip_p_thd = 0;
		return snprintf(buf, PAGE_SIZE, "%u\n", 0);
	}
	data->grip_p_thd = (r_buf[0] << 8) | r_buf[1];

	ret = a96t396_i2c_read(data->client, REG_SAR_RELEASE_THRESHOLD, r_buf, 2);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		data->grip_r_thd = 0;
		return snprintf(buf, PAGE_SIZE, "%u\n", 0);
	}
	data->grip_r_thd = (r_buf[0] << 8) | r_buf[1];

	ret = a96t396_i2c_read(data->client, REG_SAR_NOISE_THRESHOLD, r_buf, 2);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		data->grip_n_thd = 0;
		return snprintf(buf, PAGE_SIZE, "%u\n", 0);
	}
	data->grip_n_thd = (r_buf[0] << 8) | r_buf[1];

	return sprintf(buf, "%u,%u,%u\n",
		data->grip_p_thd, data->grip_r_thd, data->grip_n_thd);
}
static ssize_t grip_total_cap_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	u8 r_buf[2];
	int ret;
	int value;

	ret = a96t396_i2c_read(data->client, REG_SAR_TOTALCAP_READ, r_buf, 2);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		return snprintf(buf, PAGE_SIZE, "%u\n", 0);
	}
	value = (r_buf[0] << 8) | r_buf[1];

	return snprintf(buf, PAGE_SIZE, "%d\n", value / 100);
}

static ssize_t grip_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	int ret;
	u8 r_buf[4] = {0,};

	ret = a96t396_i2c_read(data->client, REG_SAR_DIFFDATA, r_buf, 4);
	if (ret < 0)
		GRIP_ERR("read failed(%d)\n", ret);

	data->diff = (r_buf[0] << 8) | r_buf[1];
	data->diff_d = (r_buf[2] << 8) | r_buf[3];

	return sprintf(buf, "%u,%u\n", data->diff, data->diff_d);
}

static ssize_t grip_baseline_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	u8 r_buf[2];
	int ret;

	ret = a96t396_i2c_read(data->client, REG_SAR_BASELINE, r_buf, 2);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		data->grip_baseline = 0;
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	data->grip_baseline = (r_buf[0] << 8) | r_buf[1];

	return snprintf(buf, PAGE_SIZE, "%u\n", data->grip_baseline);
}

static ssize_t grip_raw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	int ret;

	ret = a96t396_get_raw_data(data);
	if (ret < 0)
		return sprintf(buf, "%d\n", 0);
	else
		return sprintf(buf, "%u,%u\n", data->grip_raw,
				data->grip_raw_d);
}

static ssize_t grip_ref_cap_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	u8 r_buf[2];
	int ref_cap;
	int ret;

	ret = a96t396_i2c_read(data->client, REG_REF_CAP, r_buf, 2);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}

	ref_cap = (r_buf[0] << 8) | r_buf[1];
	do_div(ref_cap, 100);

	GRIP_INFO("Ref Cap : %x,%x\n", r_buf[0], r_buf[1]);
	GRIP_INFO("Ref Cap / 100 : %d\n", ref_cap);

	return sprintf(buf, "%d\n", ref_cap);
}

static ssize_t grip_gain_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	u8 ch1_rst_buf, ref_rst_buf;
	u8 ch1_int_buf, ref_int_buf;
	int ret;

	ret = a96t396_i2c_read(data->client, REG_GAINDATA, &ch1_rst_buf, 1);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	ret = a96t396_i2c_read(data->client, REG_GAINDATA + 3, &ch1_int_buf, 1);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}

	ret = a96t396_i2c_read(data->client, REG_REF_GAINDATA, &ref_rst_buf, 1);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	ret = a96t396_i2c_read(data->client, REG_REF_GAINDATA + 3, &ref_int_buf, 1);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}

	GRIP_INFO("Gain : %d,%d\n", (int)ch1_rst_buf, (int)ch1_int_buf);
	GRIP_INFO("Ref Gain : %d,%d\n", (int)ref_rst_buf, (int)ref_int_buf);

	return sprintf(buf, "%d,%d,%d,%d\n", (int)ch1_rst_buf, (int)ch1_int_buf, (int)ref_rst_buf, (int)ref_int_buf);
}

#ifdef CONFIG_SENSORS_A96T396_2CH
static ssize_t grip_gain_2ch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	u8 ch2_rst_buf, ref_rst_buf;
	u8 ch2_int_buf, ref_int_buf;
	int ret;

	ret = a96t396_i2c_read(data->client, REG_GAINDATA_2CH, &ch2_rst_buf, 1);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	ret = a96t396_i2c_read(data->client, REG_GAINDATA_2CH + 3, &ch2_int_buf, 1);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}

	ret = a96t396_i2c_read(data->client, REG_REF_GAINDATA, &ref_rst_buf, 1);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	ret = a96t396_i2c_read(data->client, REG_REF_GAINDATA + 3, &ref_int_buf, 1);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}

	GRIP_INFO("Gain : %d,%d\n", (int)ch2_rst_buf, (int)ch2_int_buf);
	GRIP_INFO("Ref Gain : %d,%d\n", (int)ref_rst_buf, (int)ref_int_buf);

	return sprintf(buf, "%d,%d,%d,%d\n", (int)ch2_rst_buf, (int)ch2_int_buf, (int)ref_rst_buf, (int)ref_int_buf);
}
#endif

static ssize_t grip_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	a96t396_diff_getdata(data, true);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->grip_event);
}

#ifdef CONFIG_SENSORS_A96T396_2CH
static ssize_t grip_ch_count_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "2\n");
}

static ssize_t grip_2ch_threshold_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	u8 r_buf[4];
	int ret;

	ret = a96t396_i2c_read(data->client, REG_SAR_THRESHOLD_2CH, r_buf, 2);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		data->mul_ch->grip_p_thd_2ch = 0;
		return snprintf(buf, PAGE_SIZE, "%u\n", 0);
	}
	data->mul_ch->grip_p_thd_2ch = (r_buf[0] << 8) | r_buf[1];

	ret = a96t396_i2c_read(data->client, REG_SAR_RELEASE_THRESHOLD_2CH, r_buf, 2);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		data->mul_ch->grip_r_thd_2ch = 0;
		return snprintf(buf, PAGE_SIZE, "%u\n", 0);
	}
	data->mul_ch->grip_r_thd_2ch = (r_buf[0] << 8) | r_buf[1];

	ret = a96t396_i2c_read(data->client, REG_SAR_NOISE_THRESHOLD_2CH, r_buf, 2);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		data->mul_ch->grip_n_thd_2ch = 0;
		return snprintf(buf, PAGE_SIZE, "%u\n", 0);
	}
	data->mul_ch->grip_n_thd_2ch = (r_buf[0] << 8) | r_buf[1];

	return sprintf(buf, "%u,%u,%u\n", data->mul_ch->grip_p_thd_2ch,
				data->mul_ch->grip_r_thd_2ch, data->mul_ch->grip_n_thd_2ch);
}

static ssize_t grip_2ch_total_cap_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	u8 r_buf[2];
	int ret;
	int value;

	ret = a96t396_i2c_read(data->client, REG_SAR_TOTALCAP_READ_2CH, r_buf, 2);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		return snprintf(buf, PAGE_SIZE, "%u\n", 0);
	}
	value = (r_buf[0] << 8) | r_buf[1];

	return snprintf(buf, PAGE_SIZE, "%d\n", value / 100);
}

static ssize_t grip_2ch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	int ret;
	u8 r_buf[4] = {0,};

	ret = a96t396_i2c_read(data->client, REG_SAR_DIFFDATA_D_2CH, r_buf, 4);
	if (ret < 0)
		GRIP_ERR("read failed(%d)\n", ret);

	data->mul_ch->diff_2ch = (r_buf[0] << 8) | r_buf[1];
	data->mul_ch->diff_d_2ch = (r_buf[2] << 8) | r_buf[3];

	return sprintf(buf, "%u,%u\n", data->mul_ch->diff_2ch, data->mul_ch->diff_d_2ch);
}

static ssize_t grip_2ch_baseline_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	u8 r_buf[2];
	int ret;

	ret = a96t396_i2c_read(data->client, REG_SAR_BASELINE_2CH, r_buf, 2);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		data->mul_ch->grip_baseline_2ch = 0;
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	data->mul_ch->grip_baseline_2ch = (r_buf[0] << 8) | r_buf[1];

	return snprintf(buf, PAGE_SIZE, "%u\n", data->mul_ch->grip_baseline_2ch);
}

static ssize_t grip_2ch_raw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	int ret;

	ret = a96t396_get_2ch_raw_data(data);
	if (ret < 0)
		return sprintf(buf, "%d\n", 0);
	else
		return sprintf(buf, "%u,%u\n", data->mul_ch->grip_raw_2ch,
				data->mul_ch->grip_raw_d_2ch);
}


static ssize_t grip_2ch_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	a96t396_2ch_diff_getdata(data, true);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->mul_ch->grip_event_2ch);
}


static ssize_t grip_2ch_unknown_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
		(data->mul_ch->is_unknown_mode == 1) ? "UNKNOWN" : "NORMAL");
}

#endif

static ssize_t grip_sw_reset_ready_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	int ret;
	int retry = 10;
	u8 r_buf[1] = {0};

	GRIP_INFO("Wait start\n");

	/* To garuantee grip sensor sw reset delay*/
	msleep(500);

	while (retry--) {
		ret = a96t396_i2c_read(data->client, REG_SW_RESET, r_buf, 1);
		if (r_buf[0] == 0x20)
			break;
		else if (r_buf[0] == 0x11)
			GRIP_INFO("reset in progress(%d)\n", retry);
		if (ret < 0) {
			GRIP_ERR("i2c failed(%d)\n", retry);
			return snprintf(buf, PAGE_SIZE, "0\n");
		}
		msleep(100);
	}

	if (r_buf[0] == 0x20) {
		GRIP_INFO("reset done");
		a96t396_check_diff_and_cap(data);

		return snprintf(buf, PAGE_SIZE, "1\n");
	}

	GRIP_INFO("expect 0x20 read 0x%x\n", r_buf[0]);
	return snprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t grip_sw_reset(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	u8 cmd;
	int ret;

	ret = kstrtou8(buf, 2, &cmd);
	if (ret) {
		GRIP_ERR("cmd read err\n");
		return count;
	}

	if (!(cmd == 1)) {
		GRIP_ERR("wrong command(%d)\n", cmd);
		return count;
	}

	data->grip_event = 0;
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use)
		data->mul_ch->grip_event_2ch = 0;
#endif
	GRIP_INFO("cmd(%d)\n", cmd);

	a96t396_grip_sw_reset(data);

	return count;
}

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
static int string_to_hex(char *pbSrc, unsigned char *pbDest)
{
	char *p = pbSrc;
	char msb = 0, lsb = 0;
	int tmplen = 0, cnt = 0;

	tmplen = strlen(p);
	while (cnt < (tmplen / 2)) {
		msb = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
		lsb = (*(++ p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p) - 48 - 7 : *(p) - 48;
		pbDest[cnt] = ((msb & 0x0f) << 4 | (lsb & 0x0f));
		p++;
		cnt++;
	}
	if (tmplen % 2 != 0)
		pbDest[cnt] = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;

	return tmplen / 2 + tmplen % 2;

}

static ssize_t grip_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 r_buf[6] = {0,};
	u8 i = 0;
	char *p = buf;
	struct a96t396_data *data = dev_get_drvdata(dev);

	if (data->read_flag) {
		if (data->read_reg_count > 6)
			data->read_reg_count = 6;
		a96t396_i2c_read(data->client, data->read_reg, r_buf, data->read_reg_count);
		for (i = 0; i < data->read_reg_count; i++)
			p += snprintf(p, PAGE_SIZE, "(0x%02x)=0x%02x\n", data->read_reg + i, r_buf[i]);

		return (p-buf);
	}
	for (i = 0; i < 0x91; i++) {
		a96t396_i2c_read(data->client, i, r_buf, 1);
		p += snprintf(p, PAGE_SIZE, "(0x%02x)=0x%02x\n", i, r_buf[0]);
	}
	return (p-buf);
}

static ssize_t grip_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	unsigned int val, reg, opt;
	u8 reg_temp_buf[512];
	u8 reg_buf[256];
	int reg_buf_len;
	u8 i;

	if (sscanf(buf, "%x,%x,%x", &reg, &val, &opt) == 3) {
		GRIP_INFO("%s, read reg = 0x%02x\n", __func__, *(u8 *)&reg);
		data->read_reg = *((u8 *)&reg);
		data->read_reg_count = *((u8 *)&val);
		if (!opt)
			data->read_flag = 1;
		else
			data->read_flag = 0;
	} else if (sscanf(buf, "%x,%x", &reg, &val) == 2) {
		GRIP_INFO("%s,reg = 0x%02x, val = 0x%02x\n", __func__, *(u8 *)&reg, (u8 *)&val);
		a96t396_i2c_write(data->client, *((u8 *)&reg), (u8 *)&val);
	} else {
		sscanf(buf, "%511s", reg_temp_buf);
		reg_buf_len = string_to_hex(reg_temp_buf, reg_buf);
		for (i = 0; i < reg_buf_len-1; i++) {
			a96t396_i2c_write(data->client, reg_buf[0]+i, &reg_buf[i+1]);
			GRIP_INFO("%s,reg = 0x%02x, val = 0x%02x\n", __func__,    reg_buf[0]+i, reg_buf[i+1]);
		}
	}
	return size;
}

static ssize_t grip_sar_press_threshold_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	int ret;
	int threshold;
	u8 cmd[2];

	ret = sscanf(buf, "%11d", &threshold);
	if (ret != 1) {
		GRIP_ERR("failed to read threshold, buf is %s\n", buf);
		return count;
	}

	if (threshold > 0xff) {
		cmd[0] = (threshold >> 8) & 0xff;
		cmd[1] = 0xff & threshold;
	} else if (threshold < 0) {
		cmd[0] = 0x0;
		cmd[1] = 0x0;
	} else {
		cmd[0] = 0x0;
		cmd[1] = (u8)threshold;
	}

	GRIP_INFO("buf : %d, threshold : %d\n", threshold,
			(cmd[0] << 8) | cmd[1]);

	ret = a96t396_i2c_write(data->client, REG_SAR_THRESHOLD, &cmd[0]);
	if (ret != 0) {
		GRIP_INFO("failed to write press_threshold data1");
		goto press_threshold_out;
	}
	ret = a96t396_i2c_write(data->client, REG_SAR_THRESHOLD + 0x01, &cmd[1]);
	if (ret != 0) {
		GRIP_INFO("failed to write press_threshold data2");
		goto press_threshold_out;
	}
press_threshold_out:
	return count;
}

static ssize_t grip_sar_release_threshold_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	int ret;
	int threshold;
	u8 cmd[2];

	ret = sscanf(buf, "%11d", &threshold);
	if (ret != 1) {
		GRIP_ERR("failed to read threshold, buf is %s\n", buf);
		return count;
	}

	if (threshold > 0xff) {
		cmd[0] = (threshold >> 8) & 0xff;
		cmd[1] = 0xff & threshold;
	} else if (threshold < 0) {
		cmd[0] = 0x0;
		cmd[1] = 0x0;
	} else {
		cmd[0] = 0x0;
		cmd[1] = (u8)threshold;
	}

	GRIP_INFO("buf : %d, threshold : %d\n", threshold,
				(cmd[0] << 8) | cmd[1]);

	ret = a96t396_i2c_write(data->client, REG_SAR_THRESHOLD + 0x02,
				&cmd[0]);
	GRIP_INFO("ret : %d\n", ret);

	if (ret != 0) {
		GRIP_INFO("failed to write release_threshold_data1");
		goto release_threshold_out;
	}
	ret = a96t396_i2c_write(data->client, REG_SAR_THRESHOLD + 0x03,
				&cmd[1]);
	GRIP_INFO("ret : %d\n", ret);
	if (ret != 0) {
		GRIP_INFO("failed to write release_threshold_data2");
		goto release_threshold_out;
	}
release_threshold_out:
	return count;
}

#ifdef CONFIG_SENSORS_A96T396_2CH
static ssize_t grip_2ch_sar_press_threshold_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	int ret;
	int threshold;
	u8 cmd[2];

	ret = sscanf(buf, "%11d", &threshold);
	if (ret != 1) {
		GRIP_ERR("failed to read threshold, buf is %s\n", buf);
		return count;
	}

	if (threshold > 0xff) {
		cmd[0] = (threshold >> 8) & 0xff;
		cmd[1] = 0xff & threshold;
	} else if (threshold < 0) {
		cmd[0] = 0x0;
		cmd[1] = 0x0;
	} else {
		cmd[0] = 0x0;
		cmd[1] = (u8)threshold;
	}

	GRIP_INFO("buf : %d, threshold : %d\n", threshold,
			(cmd[0] << 8) | cmd[1]);

	ret = a96t396_i2c_write(data->client, REG_SAR_THRESHOLD_2CH, &cmd[0]);
	if (ret != 0) {
		GRIP_INFO("failed to write press_threshold data1");
		goto press_threshold_out;
	}
	ret = a96t396_i2c_write(data->client, REG_SAR_THRESHOLD_2CH + 0x01, &cmd[1]);
	if (ret != 0) {
		GRIP_INFO("failed to write press_threshold data2");
		goto press_threshold_out;
	}
press_threshold_out:
	return count;
}

static ssize_t grip_2ch_sar_release_threshold_store(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t count)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	int ret;
	int threshold;
	u8 cmd[2];

	ret = sscanf(buf, "%11d", &threshold);
	if (ret != 1) {
		GRIP_ERR("failed to read threhsold, buf is %s\n", buf);
		return count;
	}

	if (threshold > 0xff) {
		cmd[0] = (threshold >> 8) & 0xff;
		cmd[1] = 0xff & threshold;
	} else if (threshold < 0) {
		cmd[0] = 0x0;
		cmd[1] = 0x0;
	} else {
		cmd[0] = 0x0;
		cmd[1] = (u8)threshold;
	}

	GRIP_INFO("buf : %d, threshold : %d\n", threshold,
				(cmd[0] << 8) | cmd[1]);

	ret = a96t396_i2c_write(data->client, REG_SAR_THRESHOLD_2CH + 0x02,
				&cmd[0]);
	GRIP_INFO("ret : %d\n", ret);

	if (ret != 0) {
		GRIP_INFO("failed to write release_threshold_data1");
		goto release_threshold_out;
	}
	ret = a96t396_i2c_write(data->client, REG_SAR_THRESHOLD_2CH + 0x03,
				&cmd[1]);
	GRIP_INFO("ret : %d\n", ret);
	if (ret != 0) {
		GRIP_INFO("failed to write release_threshold_data2");
		goto release_threshold_out;
	}
release_threshold_out:
	return count;
}
#endif
#endif

#ifdef CONFIG_SEC_FACTORY
static ssize_t a96t396_irq_count_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	int result = 0;
	s16 max_diff_val = 0;

	if (data->irq_count) {
		result = -1;
		max_diff_val = data->max_diff;
	} else {
		max_diff_val = data->max_normal_diff;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", result,
			data->irq_count, max_diff_val);
}

static ssize_t a96t396_irq_count_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	u8 onoff;
	int ret;

	ret = kstrtou8(buf, 10, &onoff);
	if (ret < 0) {
		GRIP_ERR("kstrtou8 failed.(%d)\n", ret);
		return count;
	}

	mutex_lock(&data->lock);
	if (onoff == 0) {
		data->abnormal_mode = 0;
	} else if (onoff == 1) {
		data->abnormal_mode = 1;
		data->irq_count = 0;
		data->max_diff = 0;
		data->max_normal_diff = 0;
	} else {
		GRIP_ERR("Invalid value.(%d)\n", onoff);
	}
	mutex_unlock(&data->lock);

	GRIP_INFO("result : %d\n", onoff);
	return count;
}
#ifdef CONFIG_SENSORS_A96T396_2CH
static ssize_t a96t396_irq_count_2ch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	int result = 0;
	s16 max_diff_val_2ch = 0;

	if (data->irq_count) {
		result = -1;
		max_diff_val_2ch = data->mul_ch->max_diff_2ch;
	} else {
		max_diff_val_2ch = data->mul_ch->max_normal_diff_2ch;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", result,
			data->irq_count, max_diff_val_2ch);
}

static ssize_t a96t396_irq_count_2ch_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	u8 onoff;
	int ret;

	ret = kstrtou8(buf, 10, &onoff);
	if (ret < 0) {
		GRIP_ERR("kstrtou8 failed.(%d)\n", ret);
		return count;
	}

	mutex_lock(&data->lock);
	if (onoff == 0) {
		data->abnormal_mode = 0;
	} else if (onoff == 1) {
		data->abnormal_mode = 1;
		data->irq_count = 0;
		data->mul_ch->max_diff_2ch = 0;
		data->mul_ch->max_normal_diff_2ch = 0;
	} else {
		GRIP_ERR("Invalid value.(%d)\n", onoff);
	}
	mutex_unlock(&data->lock);

	GRIP_INFO("result : %d\n", onoff);
	return count;
}
#endif
#endif

static ssize_t grip_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}
static ssize_t grip_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", device_name[data->ic_num]);
}

static ssize_t bin_fw_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x%02x\n",
		data->md_ver_bin, data->fw_ver_bin);
}

static int a96t396_get_fw_version(struct a96t396_data *data, bool bootmode)
{
	struct i2c_client *client = data->client;
	u8 buf;
	int ret;

	grip_always_active(data, 1);

	ret = a96t396_i2c_read(client, REG_FW_VER, &buf, 1);
	if (ret < 0) {
		GRIP_ERR("read fail(%d)\n", ret);
		if (!bootmode)
			a96t396_reset(data);
		else
			goto err_grip_revert_mode;
		ret = a96t396_i2c_read(client, REG_FW_VER, &buf, 1);
		if (ret < 0)
			goto err_grip_revert_mode;
	}
	data->fw_ver = buf;

	ret = a96t396_i2c_read(client, REG_MODEL_NO, &buf, 1);
	if (ret < 0) {
		GRIP_ERR("read fail(%d)\n", ret);
		if (!bootmode)
			a96t396_reset(data);
		else
			goto err_grip_revert_mode;
		ret = a96t396_i2c_read(client, REG_MODEL_NO, &buf, 1);
		if (ret < 0)
			goto err_grip_revert_mode;
	}
	data->md_ver = buf;

	GRIP_INFO(" fw = 0x%x, md = 0x%x\n", data->fw_ver, data->md_ver);

	grip_always_active(data, 0);

	return 0;

err_grip_revert_mode:
	grip_always_active(data, 0);

	return -1;
}

static ssize_t read_fw_ver_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	int ret;

	ret = a96t396_get_fw_version(data, false);
	if (ret < 0) {
		GRIP_ERR("read fail\n");
		data->fw_ver = 0;
	}

	return snprintf(buf, PAGE_SIZE, "0x%02x%02x\n",
		data->md_ver, data->fw_ver);
}

static int a96t396_load_fw_kernel(struct a96t396_data *data)
{
	int ret = 0;

	ret = request_firmware(&data->firm_data_bin,
		data->fw_path, &data->client->dev);
	if (ret) {
		GRIP_ERR("request_firmware fail.\n");
		return ret;
	}
	data->firm_size = data->firm_data_bin->size;
	data->fw_ver_bin = data->firm_data_bin->data[5];
	data->md_ver_bin = data->firm_data_bin->data[1];
	GRIP_INFO("fw = 0x%x, md = 0x%x\n", data->fw_ver_bin, data->md_ver_bin);

	data->checksum_h_bin = data->firm_data_bin->data[8];
	data->checksum_l_bin = data->firm_data_bin->data[9];

	GRIP_INFO("crc 0x%x 0x%x\n", data->checksum_h_bin, data->checksum_l_bin);

	return ret;
}

static int a96t396_load_fw(struct a96t396_data *data, u8 cmd)
{
	int ret = 0;
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	int retry = 20;
#endif

	switch (cmd) {
	case BUILT_IN:
		break;

	case SDCARD:
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
		while (retry > 0 && data->ioctl_pass == 0) {
			msleep(100);
			GRIP_INFO("Grip sensor FW loading %d", 20 - retry);
			retry--;
		}
		if (retry == 0)
			return -1;
#endif
		break;
	default:
		return -1;
	}
	GRIP_INFO("fw_size : %lu, success (%u)\n", data->firm_size, cmd);

	return ret;
}

static int a96t396_check_busy(struct a96t396_data *data)
{
	int ret, count = 0;
	unsigned char val = 0x00;

	do {
		ret = i2c_master_recv(data->client, &val, sizeof(val));

		if (val)
			count++;
		else
			break;

		if (count > 1000)
			break;
	} while (1);

	if (count > 1000)
		GRIP_ERR("busy %d\n", count);
	return ret;
}

static int a96t396_i2c_read_checksum(struct a96t396_data *data)
{
	unsigned char buf[6] = {0xAC, 0x9E, 0x04, 0x00, 0x37, 0xFF};
	unsigned char buf2[1] = {0x00};
	unsigned char checksum[6] = {0, };
	int ret;

	i2c_master_send(data->client, buf, 6);
	usleep_range(5000, 6000);

	i2c_master_send(data->client, buf2, 1);
	usleep_range(5000, 6000);

	ret = a96t396_i2c_read_data(data->client, checksum, 6);

	GRIP_INFO("ret:%d [%X][%X][%X][%X][%X]\n", ret,
			checksum[0], checksum[1], checksum[2], checksum[4], checksum[5]);
	data->checksum_h = checksum[4];
	data->checksum_l = checksum[5];
	return ret;
}
#ifdef CONFIG_SENSORS_A96T396_CRC_CHECK
static int a96t396_crc_check(struct a96t396_data *data)
{
	unsigned char cmd = 0xAA;
	unsigned char val = 0xFF;
	unsigned char retry = 2;
	int ret;

/*
* abov grip fw uses active/deactive mode in each period
* To check crc check, make the mode as always active mode.
*/

	grip_always_active(data, 1);

	/* crc check */
	ret = a96t396_i2c_write(data->client, REG_FW_VER, &cmd);
	if (ret < 0) {
		GRIP_INFO("crc checking enter failed\n");
		grip_always_active(data, 0);
		return ret;
	}
	// Note: The final decision of 'write result' is done in 'a96t396_flash_fw()'.
	data->crc_check = CRC_FAIL;

	while (retry--) {
		msleep(400);

		ret = a96t396_i2c_read(data->client, REG_FW_VER, &val, 1);
		if (ret < 0) {
			GRIP_INFO("crc read failed\n");
			continue;
		}

		ret = (int)val;
		if (val == CRC_FAIL) {
			GRIP_INFO("crc check fail 0x%2x\n", val);
		} else {

			data->crc_check = CRC_PASS;
			GRIP_INFO("crc check normal 0x%2x\n", val);
			break;
		}
	}

	grip_always_active(data, 0);
	return ret;
}
#endif
static int a96t396_fw_write(struct a96t396_data *data, unsigned char *addrH,
	unsigned char *addrL, unsigned char *val)
{
	int length = 36, ret = 0;
	unsigned char buf[36];

	buf[0] = 0xAC;
	buf[1] = 0x7A;
	memcpy(&buf[2], addrH, 1);
	memcpy(&buf[3], addrL, 1);
	memcpy(&buf[4], val, 32);

	ret = i2c_master_send(data->client, buf, length);
	if (ret != length) {
		GRIP_ERR("write fail[%x%x], %d\n", *addrH, *addrL, ret);
		return ret;
	}

	usleep_range(3000, 3000);

	a96t396_check_busy(data);

	return 0;
}

static int a96t396_fw_mode_enter(struct a96t396_data *data)
{
	unsigned char buf[2] = {0xAC, 0x5B};
	u8 cmd = 0;
	int ret = 0;

	GRIP_INFO("cmd send\n");
	ret = i2c_master_send(data->client, buf, 2);
	if (ret != 2) {
		GRIP_ERR("write fail\n");
		return -1;
	}

	ret = i2c_master_recv(data->client, &cmd, 1);
	GRIP_INFO("cmd receive %2x, %2x\n", data->firmup_cmd, cmd);
	if (data->firmup_cmd != cmd) {
		GRIP_ERR("cmd not matched, firmup fail (ret = %d)\n", ret);
		return -2;
	}

	return 0;
}

static int a96t396_flash_erase(struct a96t396_data *data)
{
	unsigned char buf[2] = {0xAC, 0x2D};
	int ret = 0;

	ret = i2c_master_send(data->client, buf, 2);
	if (ret != 2) {
		GRIP_ERR("write fail\n");
		return -1;
	}

	return 0;

}

static int a96t396_fw_mode_exit(struct a96t396_data *data)
{
	unsigned char buf[2] = {0xAC, 0xE1};
	int ret = 0;

	ret = i2c_master_send(data->client, buf, 2);
	if (ret != 2) {
		GRIP_ERR("write fail\n");
		return -1;
	}

	usleep_range(RESET_DELAY, RESET_DELAY);

	return 0;
}

static int a96t396_fw_update(struct a96t396_data *data, u8 cmd)
{
	int ret, i = 0;
	int count;
	int retry = 3;
	unsigned short address;
	unsigned char addrH, addrL;
	unsigned char buf[32] = {0, };

	GRIP_INFO("start\n");

	count = data->firm_size / 32;
	address = USER_CODE_ADDRESS;

	while (retry > 0) {
		a96t396_reset_for_bootmode(data);
		usleep_range(BOOT_DELAY, BOOT_DELAY);

		ret = a96t396_fw_mode_enter(data);
		if (ret < 0)
			GRIP_ERR("a96t396_fw_mode_enter fail, retry : %d\n",
				((5-retry)+1));
		else
			break;
		retry--;
	}

	if (ret < 0 && retry == 0) {
		GRIP_ERR("a96t396_fw_mode_enter fail\n");
		return ret;
	}
	usleep_range(5000, 5010);
	GRIP_INFO("fw_mode_cmd sent\n");

	ret = a96t396_flash_erase(data);

	if (ret < 0) {
		GRIP_ERR("a96t396_fw_erase_fail\n");
		return ret;
	}
	usleep_range(FLASH_DELAY, FLASH_DELAY);

	GRIP_INFO("fw_write start\n");
	for (i = 1; i < count; i++) {
		/* first 32byte is header */
		addrH = (unsigned char)((address >> 8) & 0xFF);
		addrL = (unsigned char)(address & 0xFF);
		if (cmd == BUILT_IN)
			memcpy(buf, &data->firm_data_bin->data[i * 32], 32);
		else if (cmd == SDCARD)
			memcpy(buf, &data->firm_data_ums[i * 32], 32);

		ret = a96t396_fw_write(data, &addrH, &addrL, buf);
		if (ret < 0) {
			GRIP_ERR("err, no device : %d\n", ret);
			return ret;
		}

		address += 0x20;

		memset(buf, 0, 32);
	}

	ret = a96t396_i2c_read_checksum(data);
	GRIP_INFO("checksum read%d\n", ret);

	ret = a96t396_fw_mode_exit(data);
	GRIP_INFO("fw_write end\n");

	return ret;
}

static void a96t396_release_fw(struct a96t396_data *data, u8 cmd)
{
	switch (cmd) {
	case BUILT_IN:
		release_firmware(data->firm_data_bin);
		break;

	case SDCARD:
		kfree(data->firm_data_ums);
		break;

	default:
		break;
	}
}

static int a96t396_flash_fw(struct a96t396_data *data, bool probe, u8 cmd)
{
	int retry = 2;
	int ret;
	int block_count;
	const u8 *fw_data;

	ret = a96t396_get_fw_version(data, probe);
	if (ret)
		data->fw_ver = 0;

	ret = a96t396_load_fw(data, cmd);
	if (ret) {
		GRIP_ERR("fw load fail\n");
		return ret;
	}

	switch (cmd) {
	case BUILT_IN:
		fw_data = data->firm_data_bin->data;
		break;

	case SDCARD:
		if (data->firm_data_ums == NULL) {
			GRIP_ERR("data is NULL fail\n");
			return -1;
		}
		fw_data = data->firm_data_ums;
		break;

	default:
		return -1;
	}

	block_count = (int)(data->firm_size / 32);

	while (retry--) {
		ret = a96t396_fw_update(data, cmd);
		if (ret < 0)
			break;

		if (cmd == BUILT_IN) {
			if ((data->checksum_h != data->checksum_h_bin) ||
				(data->checksum_l != data->checksum_l_bin)) {
				GRIP_ERR("checksum fail.(0x%x,0x%x),(0x%x,0x%x) retry:%d\n",
						data->checksum_h, data->checksum_l,
						data->checksum_h_bin, data->checksum_l_bin, retry);
				ret = -1;
				continue;
			}
#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SENSORS_A96T396_CRC_CHECK)
			a96t396_crc_check(data);
			if (data->crc_check == CRC_FAIL) {
				GRIP_INFO("CRC fail. retry:%d\n", retry);
				ret = -1;
				continue;
			}
#endif
		}

		a96t396_reset_for_bootmode(data);
		usleep_range(RESET_DELAY, RESET_DELAY);

		ret = a96t396_get_fw_version(data, true);
		if (ret) {
			GRIP_ERR("fw version read fail\n");
			ret = -1;
			continue;
		}

		if (data->fw_ver == 0) {
			GRIP_ERR("fw version fail (0x%x)\n", data->fw_ver);
			ret = -1;
			continue;
		}

		if ((cmd == BUILT_IN) && (data->fw_ver != data->fw_ver_bin)) {
			GRIP_ERR("fw version fail 0x%x, 0x%x\n",
						data->fw_ver, data->fw_ver_bin);
			ret = -1;
			continue;
		}
		ret = 0;
		break;
	}

	a96t396_release_fw(data, cmd);

	return ret;
}

static ssize_t grip_fw_update(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	int ret;
	u8 cmd;

	switch (*buf) {
	case 's':
	case 'S':
		cmd = BUILT_IN;
		break;
	case 'i':
	case 'I':
		cmd = SDCARD;
		break;
	default:
		data->fw_update_state = 2;
		goto fw_update_out;
	}

	data->fw_update_state = 1;
	disable_irq(data->irq);
	data->enabled = false;

	if (cmd == BUILT_IN) {
		ret = a96t396_load_fw_kernel(data);
		if (ret) {
			GRIP_ERR("failed to load firmware(%d)\n", ret);
			goto fw_update_out;
		} else {
			GRIP_INFO("fw version read success (%d)\n", ret);
		}
	} else {
		data->firm_data_ums = NULL;
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
		data->ioctl_pass = 0;
		GRIP_INFO("Requesting SDcard firmware loading to HAL!");
		input_report_rel(data->input_dev, REL_WHEEL, SDCARD_FW_LOARDING);
		input_sync(data->input_dev);
#endif
	}
	ret = a96t396_flash_fw(data, false, cmd);

	data->enabled = true;
	enable_irq(data->irq);
	if (ret) {
		GRIP_ERR("failed to flash firmware(%d)\n", ret);
		data->fw_update_state = 2;
	} else {
		GRIP_INFO("success\n");
		data->fw_update_state = 0;
	}

	if (data->current_state) {
		cmd = CMD_ON;
		ret = a96t396_i2c_write(data->client, REG_SAR_ENABLE, &cmd);
		if (ret < 0)
			GRIP_INFO("failed to enable grip irq\n");

		a96t396_check_first_status(data, 1);
	}

#if defined(CONFIG_SENSORS_A96T396_LDO_SHARE)
	GRIP_INFO("a96t396 register recovery\n");
	input_report_rel(data->input_dev, REL_WHEEL, 1);
	input_sync(data->input_dev);
#endif

fw_update_out:
	GRIP_INFO("fw_update_state = %d\n", data->fw_update_state);

	return count;
}

static ssize_t grip_fw_update_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	int count = 0;

	GRIP_INFO("%d\n", data->fw_update_state);

	if (data->fw_update_state == 0)
		count = snprintf(buf, PAGE_SIZE, "PASS\n");
	else if (data->fw_update_state == 1)
		count = snprintf(buf, PAGE_SIZE, "Downloading\n");
	else if (data->fw_update_state == 2)
		count = snprintf(buf, PAGE_SIZE, "Fail\n");

	return count;
}

static ssize_t grip_irq_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	int status = 0;

	status = gpio_get_value(data->grip_int);
	GRIP_INFO("status=%d\n", status);

	return snprintf(buf, PAGE_SIZE, "%d\n", status);
}

static ssize_t grip_irq_en_cnt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	GRIP_INFO("irq_en_cnt=%d\n", data->irq_en_cnt);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->irq_en_cnt);
}

static ssize_t grip_crc_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
#ifndef CONFIG_SENSORS_A96T396_CRC_CHECK
	int ret;
	unsigned char cmd[3] = {0x0A, 0x00, 0x10};
	unsigned char checksum[2] = {0, };

	i2c_master_send(data->client, cmd, 3);
	usleep_range(50 * 1000, 50 * 1000);

	ret = a96t396_i2c_read(data->client, 0x0A, checksum, 2);

	if (ret < 0) {
		GRIP_ERR("i2c read fail\n");
		return snprintf(buf, PAGE_SIZE, "NG,0000\n");
	}

	GRIP_INFO("CRC:%02x%02x, BIN:%02x%02x\n", checksum[0], checksum[1],
		data->checksum_h_bin, data->checksum_l_bin);

	if ((checksum[0] != data->checksum_h_bin) ||
		(checksum[1] != data->checksum_l_bin))
		return snprintf(buf, PAGE_SIZE, "NG,%02x%02x\n",
			checksum[0], checksum[1]);
	else
		return snprintf(buf, PAGE_SIZE, "OK,%02x%02x\n",
			checksum[0], checksum[1]);
#else
{
	int val;

	val = a96t396_crc_check(data);

	if (data->crc_check == CRC_PASS)
		return snprintf(buf, PAGE_SIZE, "OK,%02x\n", val);
	else
		return snprintf(buf, PAGE_SIZE, "NG,%02x\n", val);
}
#endif
}

static ssize_t a96t396_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->current_state);
}

#if defined(CONFIG_SENSORS_A96T396_LDO_SHARE)
static ssize_t grip_register_recover_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
	int ret = 0;
	u8 reg_value = 0;
	u8 cmd = 0;
	u8 check = 0;

	GRIP_INFO("Register recover\n");
	ret = kstrtou8(buf, 10, &check);

	if (check == 1) {
		//register reset
		ret = a96t396_i2c_read(data->client, REG_SAR_ENABLE, &reg_value, 1);
		if (ret < 0) {
			GRIP_ERR("fail(%d)\n", ret);
			return size;
		}

		GRIP_INFO("reg=0x24 val=%02X\n", reg_value);

		if (data->current_state) {
			if (reg_value == CMD_OFF) {
				GRIP_ERR("REG_SAR_ENABLE register recover after HW reset\n");
				cmd = CMD_ON;
				ret = a96t396_i2c_write(data->client, REG_SAR_ENABLE, &cmd);
				if (ret < 0)
					GRIP_INFO("failed to enable grip irq\n");
				a96t396_check_first_status(data, 1);
			}
		}

		GRIP_INFO("reg=0x25 val=%02X\n", reg_value);
	} else {
		GRIP_INFO("Unsupported Command\n");
	}

	return size;
}
#endif

static ssize_t grip_motion_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	if (data->motion)
		return snprintf(buf, PAGE_SIZE, "motion_detect\n");
	else
		return snprintf(buf, PAGE_SIZE, "motion_non_detect\n");
}

static ssize_t grip_motion_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	int ret;
	struct a96t396_data *data = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		GRIP_INFO("Invalid Argument\n");
		return ret;
	}

	if (val == 0) {
		GRIP_INFO("motion event off\n");
		data->motion = val;
	} else if (val == 1) {
		GRIP_INFO("motion event\n");
		data->motion = val;
	} else {
		GRIP_INFO("Invalid Argument : %u\n", val);
	}

	GRIP_INFO("%u\n", val);
	return count;
}

static ssize_t grip_unknown_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
		(data->is_unknown_mode == 1) ? "UNKNOWN" : "NORMAL");
}

static ssize_t grip_unknown_state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	int ret;
	struct a96t396_data *data = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 10, &val);
	if (ret) {
		GRIP_INFO("Invalid Argument\n");
		return ret;
	}

	if (val == 1)
		a96t396_enter_unknown_mode(data, TYPE_FORCE);
	else if (val == 0)
		data->is_unknown_mode = UNKNOWN_OFF;
	else
		GRIP_INFO("Invalid Argument(%d)\n", val);

	GRIP_INFO("%u\n", val);
	return count;
}

static ssize_t a96t396_noti_enable_store(struct device *dev,
				     struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	u8 enable;
	struct a96t396_data *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("[GRIP_%d] invalid argument\n", data->ic_num, __func__);
		return size;
	}

	GRIP_INFO("new_value=%d\n", (int)enable);
	if (data->unknown_ch_selection & 1)
		data->noti_enable = enable;
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use) {
		if (data->unknown_ch_selection & 2)
			data->mul_ch->noti_enable = enable;
		if (data->noti_enable || data->mul_ch->noti_enable)
			a96t396_enter_unknown_mode(data, TYPE_BOOT);
	} else if (data->noti_enable) {
		a96t396_enter_unknown_mode(data, TYPE_BOOT);
	}
#else
	if (data->noti_enable)
		a96t396_enter_unknown_mode(data, TYPE_BOOT);
#endif
	return size;
}

static ssize_t a96t396_noti_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct a96t396_data *data = dev_get_drvdata(dev);
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use)
		GRIP_INFO("noti_enable = %d, 2ch noti_enable : %d\n", data->noti_enable, data->mul_ch->noti_enable);
	else
		GRIP_INFO("noti_enable = %d\n", data->noti_enable);
#else
	GRIP_INFO("noti_enable = %d\n", data->noti_enable);
#endif
	return sprintf(buf, "%d\n", data->noti_enable);
}


static DEVICE_ATTR(grip_threshold, 0444, grip_threshold_show, NULL);
static DEVICE_ATTR(grip_total_cap, 0444, grip_total_cap_show, NULL);
static DEVICE_ATTR(grip_sar_enable, 0664, grip_sar_enable_show, grip_sar_enable_store);
static DEVICE_ATTR(grip_sw_reset_ready, 0444, grip_sw_reset_ready_show, NULL);
static DEVICE_ATTR(grip_sw_reset, 0220, NULL, grip_sw_reset);
static DEVICE_ATTR(grip, 0444, grip_show, NULL);
static DEVICE_ATTR(grip_baseline, 0444, grip_baseline_show, NULL);
static DEVICE_ATTR(grip_raw, 0444, grip_raw_show, NULL);
static DEVICE_ATTR(grip_ref_cap, 0444, grip_ref_cap_show, NULL);
static DEVICE_ATTR(grip_gain, 0444, grip_gain_show, NULL);
static DEVICE_ATTR(grip_check, 0444, grip_check_show, NULL);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
static DEVICE_ATTR(grip_reg_rw, 0664, grip_reg_show, grip_reg_store);
static DEVICE_ATTR(grip_sar_press_threshold, 0220,
		NULL, grip_sar_press_threshold_store);
static DEVICE_ATTR(grip_sar_release_threshold, 0220,
		NULL, grip_sar_release_threshold_store);
#endif
#ifdef CONFIG_SEC_FACTORY
static DEVICE_ATTR(grip_irq_count, 0664, a96t396_irq_count_show,
			a96t396_irq_count_store);
#endif
static DEVICE_ATTR(name, 0444, grip_name_show, NULL);
static DEVICE_ATTR(vendor, 0444, grip_vendor_show, NULL);
static DEVICE_ATTR(grip_firm_version_phone, 0444, bin_fw_ver_show, NULL);
static DEVICE_ATTR(grip_firm_version_panel, 0444, read_fw_ver_show, NULL);
static DEVICE_ATTR(grip_firm_update, 0220, NULL, grip_fw_update);
static DEVICE_ATTR(grip_firm_update_status, 0444, grip_fw_update_status_show, NULL);
static DEVICE_ATTR(grip_irq_state, 0444, grip_irq_state_show, NULL);
static DEVICE_ATTR(grip_irq_en_cnt, 0444, grip_irq_en_cnt_show, NULL);
static DEVICE_ATTR(grip_crc_check, 0444, grip_crc_check_show, NULL);
static DEVICE_ATTR(motion, 0664, grip_motion_show, grip_motion_store);
static DEVICE_ATTR(unknown_state, 0664,
	grip_unknown_state_show, grip_unknown_state_store);
static DEVICE_ATTR(noti_enable, 0664, a96t396_noti_enable_show, a96t396_noti_enable_store);
#ifdef CONFIG_SENSORS_A96T396_LDO_SHARE
static DEVICE_ATTR(grip_register_recover, 0220, NULL, grip_register_recover_store);
#endif
#ifdef CONFIG_SENSORS_A96T396_2CH
static DEVICE_ATTR(unknown_state_2ch, 0444, grip_2ch_unknown_state_show, NULL);

static DEVICE_ATTR(grip_gain_2ch, 0444, grip_gain_2ch_show, NULL);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
static DEVICE_ATTR(grip_sar_press_threshold_2ch, 0220,
		NULL, grip_2ch_sar_press_threshold_store);
static DEVICE_ATTR(grip_sar_release_threshold_2ch, 0220,
		NULL, grip_2ch_sar_release_threshold_store);
#endif
#ifdef CONFIG_SEC_FACTORY
static DEVICE_ATTR(grip_irq_count_2ch, 0664, a96t396_irq_count_2ch_show,
			a96t396_irq_count_2ch_store);
#endif
static DEVICE_ATTR(ch_count, 0444, grip_ch_count_show, NULL);
static DEVICE_ATTR(grip_threshold_2ch, 0444, grip_2ch_threshold_show, NULL);
static DEVICE_ATTR(grip_total_cap_2ch, 0444, grip_2ch_total_cap_show, NULL);
static DEVICE_ATTR(grip_2ch, 0444, grip_2ch_show, NULL);
static DEVICE_ATTR(grip_baseline_2ch, 0444, grip_2ch_baseline_show, NULL);
static DEVICE_ATTR(grip_raw_2ch, 0444, grip_2ch_raw_show, NULL);
static DEVICE_ATTR(grip_check_2ch, 0444, grip_2ch_check_show, NULL);
#endif

static struct device_attribute *grip_sensor_attributes[] = {
	&dev_attr_grip_threshold,
	&dev_attr_grip_total_cap,
	&dev_attr_grip_sar_enable,
	&dev_attr_grip_sw_reset,
	&dev_attr_grip_sw_reset_ready,
	&dev_attr_grip,
	&dev_attr_grip_baseline,
	&dev_attr_grip_raw,
	&dev_attr_grip_ref_cap,
	&dev_attr_grip_gain,
	&dev_attr_grip_check,
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	&dev_attr_grip_sar_press_threshold,
	&dev_attr_grip_sar_release_threshold,
	&dev_attr_grip_reg_rw,
#endif
#ifdef CONFIG_SEC_FACTORY
	&dev_attr_grip_irq_count,
#endif
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_grip_firm_version_phone,
	&dev_attr_grip_firm_version_panel,
	&dev_attr_grip_firm_update,
	&dev_attr_grip_firm_update_status,
	&dev_attr_grip_irq_state,
	&dev_attr_grip_irq_en_cnt,
	&dev_attr_grip_crc_check,
#ifdef CONFIG_SENSORS_A96T396_LDO_SHARE
	&dev_attr_grip_register_recover,
#endif
	&dev_attr_motion,
	&dev_attr_unknown_state,
	&dev_attr_noti_enable,
	NULL,
};

#ifdef CONFIG_SENSORS_A96T396_2CH
static struct device_attribute *multi_sensor_attrs[] = {
	&dev_attr_grip_gain_2ch,
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	&dev_attr_grip_sar_press_threshold_2ch,
	&dev_attr_grip_sar_release_threshold_2ch,
#endif
#ifdef CONFIG_SEC_FACTORY
	&dev_attr_grip_irq_count_2ch,
#endif
	&dev_attr_ch_count,
	&dev_attr_grip_threshold_2ch,
	&dev_attr_grip_total_cap_2ch,
	&dev_attr_grip_2ch,
	&dev_attr_grip_baseline_2ch,
	&dev_attr_grip_raw_2ch,
	&dev_attr_grip_check_2ch,
	&dev_attr_unknown_state_2ch,
	NULL,
};
#endif

static DEVICE_ATTR(enable, 0664, a96t396_enable_show, grip_sar_enable_store);

static struct attribute *a96t396_attributes[] = {
	&dev_attr_enable.attr,
	NULL
};

static struct attribute_group a96t396_attribute_group = {
	.attrs = a96t396_attributes
};

static int a96t396_checksum_for_usermode(struct a96t396_data *data)
{
	int ret = 0;
	int length = 3;
	unsigned char cmd[3] = {0x0A, 0x00, 0x10};
	unsigned char checksum[2] = {0, };

	ret = i2c_master_send(data->client, cmd, length);
	if (ret != length) {
		GRIP_ERR("i2c_write fail %d\n", ret);
		return -1;
	}

	usleep_range(10 * 1000, 11 * 1000);

	ret = a96t396_i2c_read(data->client, 0x0A, checksum, 2);
	if (ret < 0) {
		GRIP_ERR("i2c read fail : %d\n", ret);
		return -1;
	}

	GRIP_INFO("CRC:%02x%02x, BIN:%02x%02x\n", checksum[0], checksum[1],
		data->checksum_h_bin, data->checksum_l_bin);

	if ((checksum[0] != data->checksum_h_bin) || (checksum[1] != data->checksum_l_bin))
		ret = -1;

	return ret;
}

static int a96t396_fw_check(struct a96t396_data *data)
{
	int ret;
	bool force = false;
	u8 r_buf[4] = {0,};

	ret = a96t396_load_fw_kernel(data);

	if (ret) {
#ifdef CONFIG_SENSORS_FW_VENDOR
		GRIP_ERR("fw was not loaded yet from ueventd\n");
		return ret;
#else
		GRIP_ERR("failed load_fw_kernel(%d)\n", ret);
#endif
	} else
		GRIP_INFO("fw version read success (%d)\n", ret);

	ret = a96t396_get_fw_version(data, true);
	if (ret)
		GRIP_ERR("i2c fail(%d), addr[%d]\n", ret, data->client->addr);

	if (data->md_ver != data->md_ver_bin) {
		GRIP_ERR("MD version is different.(IC %x, BN %x). Do force FW update\n",
			data->md_ver, data->md_ver_bin);
		force = true;
	} else if (data->fw_ver == data->fw_ver_bin) {
		ret = a96t396_checksum_for_usermode(data);
		if (ret < 0) {
			GRIP_ERR("checksum fail\n");
			force = true;
		}
	}

	if (data->fw_ver < data->fw_ver_bin || data->fw_ver > TEST_FIRMWARE_DETECT_VER
				|| force == true || data->crc_check == CRC_FAIL) {
		GRIP_ERR("excute fw update (0x%x -> 0x%x)\n",
			data->fw_ver, data->fw_ver_bin);
		ret = a96t396_flash_fw(data, true, BUILT_IN);
		if (ret)
			GRIP_ERR("failed to a96t396_flash_fw (%d)\n", ret);
		else
			GRIP_INFO("fw update success\n");
	}

	ret = a96t396_i2c_read(data->client, REG_SAR_THRESHOLD, r_buf, 4);
	if (ret < 0) {
		GRIP_ERR("fail(%d)\n", ret);
		data->grip_p_thd = 0;
#ifdef CONFIG_SENSORS_A96T396_2CH
		if (data->multi_use)
			data->mul_ch->grip_p_thd_2ch = 0;
#endif
	}
	data->grip_p_thd = (r_buf[0] << 8) | r_buf[1];
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use) {
		data->mul_ch->grip_p_thd_2ch = (r_buf[2] << 8) | r_buf[3];
	}
#endif

	return ret;
}

static int a96t396_power_onoff(void *pdata, bool on)
{
	struct a96t396_data *data = (struct a96t396_data *)pdata;

	int ret = 0;
	int voltage = 0;
	int reg_enabled = 0;

	if (data->ldo_en) {
		ret = gpio_request(data->ldo_en, "a96t396_ldo_en");
		if (ret < 0) {
			GRIP_ERR("gpio %d request failed %d\n", data->ldo_en, ret);
			return ret;
		}
		gpio_set_value(data->ldo_en, on);
		GRIP_INFO("ldo_en power %d\n", gpio_get_value(data->ldo_en));
		gpio_free(data->ldo_en);
	}

	if (data->dvdd_vreg_name) {
		if (data->dvdd_vreg == NULL) {
			data->dvdd_vreg = regulator_get(NULL, data->dvdd_vreg_name);
			if (IS_ERR(data->dvdd_vreg)) {
				data->dvdd_vreg = NULL;
				GRIP_ERR("failed to get dvdd_vreg %s\n", data->dvdd_vreg_name);
			}
		}
	}

	if (data->dvdd_vreg) {
		voltage = regulator_get_voltage(data->dvdd_vreg);
		reg_enabled = regulator_is_enabled(data->dvdd_vreg);
		GRIP_INFO("dvdd_vreg reg_enabled=%d voltage=%d\n", reg_enabled, voltage);
	}

	if (on) {
		if (data->dvdd_vreg) {
			if (reg_enabled == 0) {
				ret = regulator_enable(data->dvdd_vreg);
				if (ret) {
					GRIP_ERR("dvdd reg enable fail\n");
					return ret;
				}
				GRIP_INFO("dvdd_vreg turned on\n");
			}
		}
	} else {
		if (data->dvdd_vreg) {
			if (reg_enabled == 1) {
				ret = regulator_disable(data->dvdd_vreg);
				if (ret) {
					GRIP_ERR("dvdd reg disable fail\n");
					return ret;
				}
				GRIP_INFO("dvdd_vreg turned off\n");
			}
		}
	}
	GRIP_INFO("%s\n", on ? "on" : "off");

	return ret;
}

static int a96t396_irq_init(struct device *dev,
	struct a96t396_data *data)
{
	int ret = 0;

	ret = gpio_request(data->grip_int, "a96t396_IRQ");
	if (ret < 0) {
		GRIP_ERR("gpio %d request failed (%d)\n", data->grip_int, ret);
		return ret;
	}

	ret = gpio_direction_input(data->grip_int);
	if (ret < 0) {
		GRIP_ERR("failed to set direction input gpio %d(%d)\n",
				data->grip_int, ret);
		gpio_free(data->grip_int);
		return ret;
	}
	// assigned power function to function ptr
	data->power = a96t396_power_onoff;

	return ret;
}

static int a96t396_parse_dt(struct a96t396_data *data, struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct pinctrl *p;
	int ret = 0;
	enum of_gpio_flags flags;

	data->grip_int = of_get_named_gpio(np, "irq_gpio", 0);
	if (data->grip_int < 0) {
		GRIP_ERR("Can't get grip_int\n");
		return data->grip_int;
	}

	data->ldo_en = of_get_named_gpio_flags(np, "ldo_en", 0, &flags);
	if (data->ldo_en < 0) {
		GRIP_INFO("set ldo_en 0\n");
		data->ldo_en = 0;
	} else {
#if IS_ENABLED(CONFIG_SENSORS_A96T396)
		if (data->ic_num == MAIN_GRIP)
			ret = gpio_request(data->ldo_en, "a96t396_ldo_en");
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB)
		if (data->ic_num == SUB_GRIP)
			ret = gpio_request(data->ldo_en, "a96t396_sub_ldo_en");
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB2)
		if (data->ic_num == SUB2_GRIP)
			ret = gpio_request(data->ldo_en, "a96t396_sub2_ldo_en");
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_WIFI)
		if (data->ic_num == WIFI_GRIP)
			ret = gpio_request(data->ldo_en, "a96t396_wifi_ldo_en");
#endif
		if (ret < 0) {
			GRIP_ERR("gpio %d request failed %d\n", data->ldo_en, ret);
			return ret;
		}
		gpio_direction_output(data->ldo_en, 1);
		gpio_free(data->ldo_en);
	}

	if (of_property_read_string_index(np, "dvdd_vreg_name", 0,
					(const char **)&data->dvdd_vreg_name)) {
		data->dvdd_vreg_name = NULL;
	}
	GRIP_INFO("dvdd_vreg_name: %s\n", data->dvdd_vreg_name);


	ret = of_property_read_string(np, "fw_path", (const char **)&data->fw_path);
	if (ret < 0) {
		GRIP_INFO("use TK_FW_PATH_BIN %d\n", ret);
		data->fw_path = TK_FW_PATH_BIN;
	}
	GRIP_INFO("fw path %s\n", data->fw_path);


	ret = of_property_read_u32(np, "firmup_cmd", &data->firmup_cmd);
	if (ret < 0)
		data->firmup_cmd = 0;


	ret = of_property_read_u32(np, "multi_use", &data->multi_use);
	if (ret < 0) {
		GRIP_INFO("set multi_use 0\n");
		data->multi_use = 0;
	}


	ret = of_property_read_u32(np, "unknown_ch_selection", &data->unknown_ch_selection);
	if (ret < 0) {
		GRIP_INFO("set select unknown ch 3\n");
		data->unknown_ch_selection = 3;
	}
	p = pinctrl_get_select_default(dev);
	if (IS_ERR(p))
		GRIP_INFO("failed pinctrl_get\n");

	GRIP_INFO("grip_int:%d, ldo_en:%d\n", data->grip_int, data->ldo_en);

	return 0;
}

#if (IS_ENABLED(CONFIG_CCIC_NOTIFIER) || IS_ENABLED(CONFIG_PDIC_NOTIFIER)) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int a96t396_pdic_handle_notification(struct notifier_block *nb,
					     unsigned long action, void *pdic_data)
{
	PD_NOTI_ATTACH_TYPEDEF usb_typec_info = *(PD_NOTI_ATTACH_TYPEDEF *)pdic_data;
	struct a96t396_data *data = container_of(nb, struct a96t396_data, pdic_nb);

#if IS_ENABLED(CONFIG_TABLET_MODEL_CONCEPT)
	GRIP_INFO("tablet model : reset skip!\n");
	return 0;
#endif

	if (usb_typec_info.id != PDIC_NOTIFY_ID_ATTACH && usb_typec_info.id != PDIC_NOTIFY_ID_OTG) {
#if defined(CONFIG_SEC_FACTORY)
		if (usb_typec_info.id == PDIC_NOTIFY_ID_RID) {
			PD_NOTI_RID_TYPEDEF usb_fac_cable_info = *(PD_NOTI_RID_TYPEDEF *)pdic_data;
			switch (usb_fac_cable_info.rid) {
			case RID_301K:
			case RID_523K:
			case RID_619K:
				schedule_work(&data->pdic_detach_reset_work);
				GRIP_INFO("fac cable exception code rid : %d", usb_fac_cable_info.rid);
				break;
			default:
				break;
			}
		}
#endif
		return 0;
	}

	if (data->pre_attach == usb_typec_info.attach)
		return 0;

	GRIP_INFO("src %d id %d attach %d\n", usb_typec_info.src, usb_typec_info.id, usb_typec_info.attach);

	if (usb_typec_info.attach)
		schedule_work(&data->pdic_attach_reset_work);
	else
		schedule_work(&data->pdic_detach_reset_work);

	//usb host (otg)
	if (usb_typec_info.rprd == PDIC_NOTIFY_HOST) {
		data->pre_otg_attach = usb_typec_info.rprd;
		GRIP_INFO("otg attach");
	} else if (data->pre_otg_attach) {
		data->pre_otg_attach = 0;
		GRIP_INFO("otg detach");
	}

	a96t396_enter_unknown_mode(data, TYPE_USB);

	data->pre_attach = usb_typec_info.attach;

	return 0;
}
#endif


#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
static int a96t396_hall_notifier(struct notifier_block *nb,
				unsigned long flip_cover, void *hall_data)
{
#if IS_ENABLED(CONFIG_TABLET_MODEL_CONCEPT)
	struct hall_notifier_context *hall_notifier = hall_data;
#endif
	struct a96t396_data *data = container_of(nb, struct a96t396_data,
					hall_nb);
	if (data == NULL) {
		GRIP_ERR("data is null\n");
		return 0;
	}

#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use) {
		GRIP_INFO("set flip unknown mode(flip %s,prev %d %d)\n",
			flip_cover ? "close" : "open", data->is_unknown_mode, data->mul_ch->is_unknown_mode);
	} else {
		GRIP_INFO("set flip unknown mode(flip %s,prev %d)\n",
			flip_cover ? "close" : "open", data->is_unknown_mode);
	}
#else
	GRIP_INFO("set flip unknown mode(flip %s,prev %d)\n",
		flip_cover ? "close" : "open", data->is_unknown_mode);
#endif

#if IS_ENABLED(CONFIG_TABLET_MODEL_CONCEPT)
	if (flip_cover) {
		GRIP_INFO("%s attach\n", hall_notifier->name);
		if (strncmp(hall_notifier->name, "certify_hall", sizeof("certify_hall") - 1) == 0 ||
			strncmp(hall_notifier->name, "hall_wacom", sizeof("hall_wacom") - 1) == 0) {
			schedule_work(&data->reset_work);
			GRIP_INFO("reset only without unknown, %s\n", hall_notifier->name);
		} else if (strncmp(hall_notifier->name, "hall", sizeof("hall") - 1) == 0)
			GRIP_INFO("reset skip, %s\n", hall_notifier->name);
		else
			GRIP_INFO("%s is not defined, hall_notifier_name\n", hall_notifier->name);
	}
#else
	if (flip_cover)
		schedule_work(&data->reset_work);
	a96t396_enter_unknown_mode(data, TYPE_HALL);
#endif
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_TABLET_MODEL_CONCEPT)
#if IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V3) || IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V2) || IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO)
static int a96t396_pogo_notifier(struct notifier_block *nb,
		unsigned long action, void *pogo_data)
{
	struct a96t396_data *data = container_of(nb, struct a96t396_data,
					pogo_nb);

	switch (action) {
	case POGO_NOTIFIER_ID_ATTACHED:
#if defined(CONFIG_HALL_NOTIFIER)
		schedule_work(&data->reset_work);
#else
		a96t396_grip_sw_reset(data);
#endif
		GRIP_INFO("pogo attach\n");
		break;
	case POGO_NOTIFIER_ID_DETACHED:
		GRIP_INFO("pogo dettach\n");
		break;
	};

	return 0;
}
#endif
#endif

static void a96t396_check_first_working(struct a96t396_data *data)
{
	if (data->grip_p_thd < data->diff) {
		if (!data->first_working) {
			data->first_working = true;
			GRIP_INFO("first working detected %d\n", data->diff);
		}
	} else {
		if (data->first_working) {
			data->is_unknown_mode = UNKNOWN_OFF;
			GRIP_INFO("Release detected %d, unknown mode off\n", data->diff);
		}
	}
}

#ifdef CONFIG_SENSORS_A96T396_2CH
static void a96t396_2ch_check_first_working(struct a96t396_data *data)
{
	if (data->multi_use) {
		if (data->mul_ch->grip_p_thd_2ch < data->mul_ch->diff_2ch) {
			if (!data->mul_ch->first_working) {
				data->mul_ch->first_working = true;
				GRIP_INFO("first working 2ch detected %d\n", data->mul_ch->diff_2ch);
			}
		} else {
			if (data->mul_ch->first_working) {
				data->mul_ch->is_unknown_mode = UNKNOWN_OFF;
				GRIP_INFO("Release 2ch detected %d, unknown mode off\n", data->mul_ch->diff_2ch);
			}
		}
	}
}
#endif

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP

static int a96t396_ioctl_fw_hal_to_kerenl(struct a96t396_data *data, void __user *p)
{
	int status = 0;
	struct a96t396_fw_data *temp = NULL;

	GRIP_INFO("ioctl fw_hal_to_kernel");
	if (p != NULL) {
		temp = vmalloc(sizeof(struct a96t396_fw_data));
		if (temp == NULL) {
			status = -1;
			GRIP_INFO("fw memory alloc fail");
			return status;
		}
		status = copy_from_user(temp, p, sizeof(struct a96t396_fw_data));
		GRIP_INFO("ioctl status %d, filesize = %d", status, temp->fw_size);
		if (temp->fw_size) {
			data->firm_data_ums = kzalloc(temp->fw_size, GFP_KERNEL);
			memcpy((char __user *)data->firm_data_ums, temp->fw_data, temp->fw_size);
			data->firm_size = temp->fw_size;
			data->ioctl_pass = temp->pass;
			GRIP_INFO("firmware ioctl_pass %d!", data->ioctl_pass);
			if (data->firm_size > 0)
				data->ioctl_pass = 1;
			if (data->ioctl_pass == 0) {
				GRIP_INFO("firmware loading fail %d!", data->ioctl_pass);
				status = -1;
			} else
				GRIP_INFO("firmware loading done!");
		}
		vfree(temp);
	} else {
		GRIP_INFO("ioctl fw loading fail!");
	}

	return status;
}

static int a96t396_ioctl_handler(struct a96t396_data *data,
				  unsigned int cmd, unsigned long arg,
				  void __user *p)
{
	int status = 0;

	if (data == NULL) {
		pr_err("[GRIP] probe failed");
		return -1;
	}

	switch (cmd) {
	case A96T396_SET_FW_DATA:
		status = a96t396_ioctl_fw_hal_to_kerenl(data, p);
		break;
	default:
		status = -1;
	}

	return status;
}

static int a96t396_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int a96t396_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long a96t396_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int status = 0;
	struct a96t396_data *data = NULL;

	data = container_of(file->private_data, struct a96t396_data,
			    miscdev);

	status = a96t396_ioctl_handler(data, cmd, arg, (void __user *)arg);

	return status;
}


static void cleanup_miscdev(struct a96t396_data *data)
{
	if (!IS_ERR(data->miscdev.this_device) &&
			data->miscdev.this_device != NULL) {
		misc_deregister(&data->miscdev);
	}
}

#endif

#if IS_ENABLED(CONFIG_TABLET_MODEL_CONCEPT)
#if IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V3) || IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V2) || IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO)
static void a96t396_init_work_func(struct work_struct *work)
{
	struct a96t396_data *data = container_of((struct delayed_work *)work,
		struct a96t396_data, init_work);

	GRIP_INFO("register pogo_notifier\n");

	pogo_notifier_register(&data->pogo_nb, a96t396_pogo_notifier,
					POGO_NOTIFY_DEV_SENSOR);
}
#endif
#endif

static int a96t396_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct a96t396_data *data;
	struct input_dev *input_dev;
	struct input_dev *noti_input_dev;

	int ret;
	int ic_num = 0;

#ifdef CONFIG_SENSORS_A96T396_2CH
	struct device_attribute *sensor_attributes[SENSOR_ATTR_SIZE];
#endif

#ifdef CONFIG_SENSORS_FW_VENDOR
	static int fw_count;

	probe_count = probe_count + 1;
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396)
	if (strcmp(client->name, "a96t396") == 0)
		ic_num = MAIN_GRIP;
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB)
	if (strcmp(client->name, "a96t396_sub") == 0)
		ic_num = SUB_GRIP;
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB2)
	if (strcmp(client->name, "a96t396_sub2") == 0)
		ic_num = SUB2_GRIP;
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_WIFI)
	if (strcmp(client->name, "a96t396_wifi") == 0)
		ic_num = WIFI_GRIP;
#endif

	pr_info("[GRIP_%s] %s: start (0x%x)\n", grip_name[ic_num], __func__, client->addr);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[GRIP_%s] i2c_check_functionality fail\n", grip_name[ic_num]);
		return -EIO;
	}

	data = kzalloc(sizeof(struct a96t396_data), GFP_KERNEL);
	if (!data) {
		pr_info("[GRIP %d] Failed to allocate memory\n", ic_num);
		ret = -ENOMEM;
		goto err_alloc;
	}
	data->ic_num = ic_num;

	input_dev = input_allocate_device();
	if (!input_dev) {
		GRIP_ERR("Failed to allocate memory for input device\n");
		ret = -ENOMEM;
		goto err_input_alloc;
	}

	data->client = client;
	data->input_dev = input_dev;
	data->probe_done = false;
	data->current_state = false;
	data->skip_event = false;
	data->buf = 0x00;
	data->crc_check = CRC_PASS;
	data->grip_ws = wakeup_source_register(&client->dev, "grip_wake_lock");
	data->is_unknown_mode = UNKNOWN_OFF;
	data->first_working = false;
	data->motion = 1;

	ret = a96t396_parse_dt(data, &client->dev);
	if (ret) {
		GRIP_ERR("failed to a96t396_parse_dt\n");
		input_free_device(input_dev);
		goto err_config;
	}

	GRIP_INFO("multi_channel : %d", data->multi_use);
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use) {
		data->mul_ch = kzalloc(sizeof(struct multi_channel), GFP_KERNEL);
		if (!data->mul_ch) {
			GRIP_ERR("multi_channel alloc failed");
			data->multi_use = 0;
			input_free_device(input_dev);
			goto err_config;
		}
		data->mul_ch->is_unknown_mode = UNKNOWN_OFF;
		data->mul_ch->first_working = false;
	}

#endif


	ret = a96t396_irq_init(&client->dev, data);
	if (ret) {
		GRIP_ERR("failed to init reg\n");
		input_free_device(input_dev);
		goto pwr_config;
	}

	if (data->power) {
		data->power(data, true);
		usleep_range(RESET_DELAY, RESET_DELAY);
	}

	data->irq = -1;
	client->irq = gpio_to_irq(data->grip_int);
	mutex_init(&data->lock);

	i2c_set_clientdata(client, data);
#ifndef CONFIG_SENSORS_FW_VENDOR
#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SENSORS_A96T396_CRC_CHECK)
	a96t396_crc_check(data);
#endif
	ret = a96t396_fw_check(data);
	if (ret) {
		GRIP_ERR("failed to firmware check (%d)\n", ret);
		input_free_device(input_dev);
		goto err_reg_input_dev;
	}
#else
{
	/*
	 * Add probe fail routine if i2c is failed
	 * non fw IC returns 0 from ALL register but i2c is success.
	 */
	u8 buf;

	ret = a96t396_i2c_read(client, REG_MODEL_NO, &buf, 1);
	if (ret) {
		GRIP_ERR("i2c is failed %d\n", ret);
		input_free_device(input_dev);
		goto err_reg_input_dev;
	} else {
		GRIP_INFO("i2c is normal, model_no = 0x%2x\n", buf);
	}
}
#endif
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	GRIP_INFO("Setting up misc dev");
	data->miscdev.minor = MISC_DYNAMIC_MINOR;

	data->miscdev.name = module_name[data->ic_num];
	data->miscdev.fops = &fwload_fops;

	GRIP_INFO("Misc device reg:%s", data->miscdev.name);

	ret = misc_register(&data->miscdev);
	if (ret != 0)
		GRIP_INFO("Failed to register misc dev: %d", ret);
#endif
	input_dev->name = module_name[data->ic_num];
	input_dev->id.bustype = BUS_I2C;

	input_set_capability(input_dev, EV_REL, REL_MISC);
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use) {
		input_set_capability(input_dev, EV_REL, REL_DIAL);
		input_set_capability(input_dev, EV_REL, REL_Y);
	}
#endif
#if defined(CONFIG_SENSORS_A96T396_LDO_SHARE) || !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	input_set_capability(input_dev, EV_REL, REL_WHEEL);
#endif
	input_set_capability(input_dev, EV_REL, REL_X);
	input_set_drvdata(input_dev, data);

	noti_input_dev = input_allocate_device();
	if (!noti_input_dev) {
		GRIP_ERR("noti_input_allocate_device failed\n");
		input_free_device(input_dev);
		goto err_noti_input_alloc;
	}

	data->dev = &client->dev;
	data->noti_input_dev = noti_input_dev;

	noti_input_dev->name = NOTI_MODULE_NAME;
	noti_input_dev->id.bustype = BUS_I2C;

	input_set_capability(noti_input_dev, EV_REL, REL_X);
	input_set_drvdata(noti_input_dev, data);

	INIT_DELAYED_WORK(&data->debug_work, a96t396_debug_work_func);
	INIT_WORK(&data->pdic_attach_reset_work, pdic_attach_reset_work_func);
	INIT_WORK(&data->pdic_detach_reset_work, pdic_detach_reset_work_func);
	INIT_WORK(&data->reset_work, reset_work_func);
#ifdef CONFIG_SENSORS_FW_VENDOR
	INIT_DELAYED_WORK(&data->firmware_work, a96t396_firmware_work_func);
#endif
#if IS_ENABLED(CONFIG_TABLET_MODEL_CONCEPT)
#if IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V3) || IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V2) || IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO)
	INIT_DELAYED_WORK(&data->init_work, a96t396_init_work_func);
#endif
#endif
	ret = input_register_device(input_dev);
	if (ret) {
		GRIP_ERR("failed to register input dev (%d)\n", ret);
		input_free_device(input_dev);
		input_free_device(noti_input_dev);
		goto err_reg_input_dev;
	}

	if (data->unknown_ch_selection)
		ret = input_register_device(noti_input_dev);
	else
		ret = -1;

	if (ret) {
		input_free_device(noti_input_dev);
		GRIP_ERR("failed to register input dev for noti (%d)\n", ret);
		if (data->unknown_ch_selection)
			goto err_register_input_dev_noti;
	}

#if defined(CONFIG_SENSORS_CORE_AP)
	ret = sensors_create_symlink(&input_dev->dev.kobj,
					 input_dev->name);
	if (ret < 0) {
		pr_err("[GRIP_%d] fail to create symlink %d\n", data->ic_num, ret);
		goto err_sysfs_symlink;
	}

	ret = sysfs_create_group(&data->input_dev->dev.kobj, &a96t396_attribute_group);
	if (ret < 0) {
		pr_err("[GRIP_%d] fail to create sysfs group (%d)\n", data->ic_num, ret);
		goto err_sysfs_group;
	}

#ifdef CONFIG_SENSORS_A96T396_2CH
	memcpy(sensor_attributes, grip_sensor_attributes, sizeof(grip_sensor_attributes));
	if (data->multi_use) {
		int multi_sensor_attrs_size = sizeof(multi_sensor_attrs) / sizeof(ssize_t *);
		int grip_sensor_attr_size = sizeof(grip_sensor_attributes) / sizeof(ssize_t *);

		if (SENSOR_ATTR_SIZE <  multi_sensor_attrs_size + grip_sensor_attr_size) {
			GRIP_ERR("failed : mem size of sensor_attr is exceeded size : %d, %d\n", multi_sensor_attrs_size, grip_sensor_attr_size);
			goto err_sysfs_group;
		}
		memcpy(sensor_attributes + grip_sensor_attr_size - 1, multi_sensor_attrs, sizeof(multi_sensor_attrs));
	}
	ret = sensors_register(&data->dev, data, sensor_attributes,
				(char *)module_name[data->ic_num]);
#else
	ret = sensors_register(&data->dev, data, grip_sensor_attributes,
				(char *)module_name[data->ic_num]);
#endif
#else // !CONFIG_SENSORS_CORE_AP
	ret = sensors_create_symlink(data->input_dev);
	if (ret < 0) {
		GRIP_ERR("Failed to create sysfs symlink\n");
		goto err_sysfs_symlink;
	}

	ret = sysfs_create_group(&data->input_dev->dev.kobj,
				&a96t396_attribute_group);
	if (ret < 0) {
		GRIP_ERR("Failed to create sysfs group\n");
		goto err_sysfs_group;
	}
#ifdef CONFIG_SENSORS_A96T396_2CH
	memcpy(sensor_attributes, grip_sensor_attributes, sizeof(grip_sensor_attributes));
	if (data->multi_use) {
		int multi_sensor_attrs_size = sizeof(multi_sensor_attrs) / sizeof(ssize_t *);
		int grip_sensor_attr_size = sizeof(grip_sensor_attributes) / sizeof(ssize_t *);

		if (SENSOR_ATTR_SIZE <  multi_sensor_attrs_size + grip_sensor_attr_size) {
			GRIP_ERR("failed : mem size of sensor_attr is exceeded size : %d, %d\n", multi_sensor_attrs_size, grip_sensor_attr_size);
			goto err_sysfs_group;
		}
		memcpy(sensor_attributes + grip_sensor_attr_size - 1, multi_sensor_attrs, sizeof(multi_sensor_attrs));
	}
	ret = sensors_register(data->dev, data, sensor_attributes,
				(char *)module_name[data->ic_num]);
#else
	ret = sensors_register(data->dev, data, grip_sensor_attributes,
				(char *)module_name[data->ic_num]);
#endif
#endif
	if (ret) {
		GRIP_ERR("could not register grip_sensor(%d)\n", ret);
		goto err_sensor_register;
	}

	data->enabled = true;

	ret = request_threaded_irq(client->irq, NULL, a96t396_interrupt,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, device_name[data->ic_num], data);

	disable_irq(client->irq);

	if (ret < 0) {
		GRIP_ERR("Failed to register interrupt\n");
		goto err_req_irq;
	}
	data->irq = client->irq;
	data->dev = &client->dev;

	device_init_wakeup(&client->dev, true);

	a96t396_set_debug_work(data, ON, 20000);
#ifdef CONFIG_SENSORS_FW_VENDOR
	if (fw_count == 0)
		a96t396_set_firmware_work(data, ON, SHCEDULE_INTERVAL);
	gp_fw_work[ic_num] = &data->firmware_work;
	fw_count = fw_count + 1;
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	data->pdic_status = OFF;
	data->pdic_pre_attach = 0;
	manager_notifier_register(&data->pdic_nb,
					a96t396_pdic_handle_notification,
					MANAGER_NOTIFY_PDIC_SENSORHUB);
#endif

#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
	GRIP_INFO("register hall notifier\n");
	data->hall_nb.priority = 1;
	data->hall_nb.notifier_call = a96t396_hall_notifier;
	hall_notifier_register(&data->hall_nb);
#endif
#if IS_ENABLED(CONFIG_TABLET_MODEL_CONCEPT)
#if IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V3) || IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO_V2) || IS_ENABLED(CONFIG_KEYBOARD_STM32_POGO)
	schedule_delayed_work(&data->init_work, msecs_to_jiffies(5000));
#endif
#endif
	GRIP_INFO("done\n");
	data->probe_done = true;
	data->resume_called = false;
	return 0;

err_req_irq:
	sensors_unregister(data->dev, grip_sensor_attributes);
err_sensor_register:
	sysfs_remove_group(&data->input_dev->dev.kobj,
			&a96t396_attribute_group);
err_sysfs_group:
#if defined(CONFIG_SENSORS_CORE_AP)
	sensors_remove_symlink(&data->input_dev->dev.kobj, data->input_dev->name);
#else
	sensors_remove_symlink(input_dev);
#endif
err_sysfs_symlink:
	input_unregister_device(noti_input_dev);
err_register_input_dev_noti:
	input_unregister_device(input_dev);
err_noti_input_alloc:
err_reg_input_dev:
	mutex_destroy(&data->lock);
	gpio_free(data->grip_int);
#ifndef CONFIG_SENSORS_A96T396_LDO_SHARE
	if (data->power)
		data->power(data, false);
#endif
pwr_config:
err_config:
	wakeup_source_unregister(data->grip_ws);
err_input_alloc:
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use)
		kfree(data->mul_ch);
#endif
	kfree(data);
err_alloc:
	pr_info("[GRIP %s] Failed\n", grip_name[ic_num]);
	return ret;
}

static int a96t396_remove(struct i2c_client *client)
{
	struct a96t396_data *data = i2c_get_clientdata(client);

	if (data->enabled)
		data->power(data, false);

	data->enabled = false;
	device_init_wakeup(&client->dev, false);
	wakeup_source_unregister(data->grip_ws);
	cancel_delayed_work_sync(&data->debug_work);
#ifdef CONFIG_SENSORS_FW_VENDOR
	cancel_delayed_work_sync(&data->firmware_work);
#endif
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	cleanup_miscdev(data);
#endif
	if (data->irq >= 0)
		free_irq(data->irq, data);
	sensors_unregister(data->dev, grip_sensor_attributes);
	sysfs_remove_group(&data->input_dev->dev.kobj,
				&a96t396_attribute_group);
#if defined(CONFIG_SENSORS_CORE_AP)
	sensors_remove_symlink(&data->input_dev->dev.kobj, data->input_dev->name);
#else
	sensors_remove_symlink(data->input_dev);
#endif
	input_unregister_device(data->input_dev);
	input_free_device(data->input_dev);
#ifdef CONFIG_SENSORS_A96T396_2CH
	if (data->multi_use)
		kfree(data->mul_ch);
#endif
	kfree(data);

	return 0;
}

static int a96t396_suspend(struct device *dev)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	data->resume_called = false;
	GRIP_INFO("current_state : %d\n", data->current_state);
	a96t396_set_debug_work(data, 0, 0);

#if !defined(CONFIG_SENSORS_CORE_AP)
	if (data->current_state)
		disable_irq(data->irq);
#endif
	cancel_work_sync(&data->pdic_attach_reset_work);
	cancel_work_sync(&data->pdic_detach_reset_work);
	cancel_work_sync(&data->reset_work);

	return 0;
}

static int a96t396_resume(struct device *dev)
{
	struct a96t396_data *data = dev_get_drvdata(dev);

	GRIP_INFO("current_state : %d\n", data->current_state);
	data->resume_called = true;
	a96t396_set_debug_work(data, 1, 0);

#if !defined(CONFIG_SENSORS_CORE_AP)
	if (data->current_state)
		enable_irq(data->irq);
#endif
	return 0;
}

static void a96t396_shutdown(struct i2c_client *client)
{
	struct a96t396_data *data = i2c_get_clientdata(client);

	a96t396_set_debug_work(data, 0, 0);

	if (data->enabled) {
		disable_irq(data->irq);
		data->power(data, false);
	}
	data->enabled = false;

	cancel_work_sync(&data->pdic_attach_reset_work);
	cancel_work_sync(&data->pdic_detach_reset_work);
	cancel_work_sync(&data->reset_work);
}

static const struct dev_pm_ops a96t396_pm_ops = {
	.suspend = a96t396_suspend,
	.resume = a96t396_resume,
};

#if IS_ENABLED(CONFIG_SENSORS_A96T396)
static const struct i2c_device_id a96t396_device_id[] = {
	{"grip_sensor", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, a96t396_device_id);

#ifdef CONFIG_OF
static const struct of_device_id a96t396_match_table[] = {
	{ .compatible = "a96t396",},
	{ },
};
#else
#define a96t396_match_table NULL
#endif

static struct i2c_driver a96t396_driver = {
	.probe = a96t396_probe,
	.remove = a96t396_remove,
	.shutdown = a96t396_shutdown,
	.id_table = a96t396_device_id,
	.driver = {
		.name = "A96T396",
		.owner = THIS_MODULE,
		.of_match_table = a96t396_match_table,
		.pm = &a96t396_pm_ops
	},
};
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB)
static const struct i2c_device_id a96t396_sub_device_id[] = {
	{"grip_sensor_sub", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, a96t396_sub_device_id);
#ifdef CONFIG_OF
static const struct of_device_id a96t396_sub_match_table[] = {
	{ .compatible = "a96t396_sub",},
	{ },
};
#else
#define a96t396_sub_match_table NULL
#endif

static struct i2c_driver a96t396_sub_driver = {
	.probe = a96t396_probe,
	.remove = a96t396_remove,
	.shutdown = a96t396_shutdown,
	.id_table = a96t396_sub_device_id,
	.driver = {
		.name = "A96T396_SUB",
		.owner = THIS_MODULE,
		.of_match_table = a96t396_sub_match_table,
		.pm = &a96t396_pm_ops
	},
};
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB2)
static const struct i2c_device_id a96t396_sub2_device_id[] = {
	{"grip_sensor_sub2", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, a96t396_sub2_device_id);
#ifdef CONFIG_OF
static const struct of_device_id a96t396_sub2_match_table[] = {
	{ .compatible = "a96t396_sub2",},
	{ },
};
#else
#define a96t396_sub2_match_table NULL
#endif

static struct i2c_driver a96t396_sub2_driver = {
	.probe = a96t396_probe,
	.remove = a96t396_remove,
	.shutdown = a96t396_shutdown,
	.id_table = a96t396_sub2_device_id,
	.driver = {
		.name = "A96T396_SUB2",
		.owner = THIS_MODULE,
		.of_match_table = a96t396_sub2_match_table,
		.pm = &a96t396_pm_ops
	},
};
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_WIFI)
static const struct i2c_device_id a96t396_wifi_device_id[] = {
	{"grip_sensor_wifi", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, a96t396_wifi_device_id);
#ifdef CONFIG_OF
static const struct of_device_id a96t396_wifi_match_table[] = {
	{ .compatible = "a96t396_wifi",},
	{ },
};
#else
#define a96t396_wifi_match_table NULL
#endif

static struct i2c_driver a96t396_wifi_driver = {
	.probe = a96t396_probe,
	.remove = a96t396_remove,
	.shutdown = a96t396_shutdown,
	.id_table = a96t396_wifi_device_id,
	.driver = {
		.name = "A96T396_WIFI",
		.owner = THIS_MODULE,
		.of_match_table = a96t396_wifi_match_table,
		.pm = &a96t396_pm_ops
	},
};
#endif

static int __init a96t396_init(void)
{
	int ret = 0;

#if IS_ENABLED(CONFIG_SENSORS_A96T396)
	ret = i2c_add_driver(&a96t396_driver);
	if (ret != 0)
		pr_err("[GRIP] a96t396_driver probe fail\n");
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB)
	ret = i2c_add_driver(&a96t396_sub_driver);
	if (ret != 0)
		pr_err("[GRIP_SUB] a96t396_sub_driver probe fail\n");
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB2)
	ret = i2c_add_driver(&a96t396_sub2_driver);
	if (ret != 0)
		pr_err("[GRIP_SUB2] a96t396_sub2_driver probe fail\n");
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_WIFI)
	ret = i2c_add_driver(&a96t396_wifi_driver);
	if (ret != 0)
		pr_err("[GRIP_WIFI] a96t396_wifi_driver probe fail\n");
#endif
	return ret;
}

static void __exit a96t396_exit(void)
{
#if IS_ENABLED(CONFIG_SENSORS_A96T396)
	i2c_del_driver(&a96t396_driver);
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB)
	i2c_del_driver(&a96t396_sub_driver);
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_SUB2)
	i2c_del_driver(&a96t396_sub2_driver);
#endif
#if IS_ENABLED(CONFIG_SENSORS_A96T396_WIFI)
	i2c_del_driver(&a96t396_wifi_driver);
#endif
}

module_init(a96t396_init);
module_exit(a96t396_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Grip sensor driver for a96t396 chip");
MODULE_LICENSE("GPL");
