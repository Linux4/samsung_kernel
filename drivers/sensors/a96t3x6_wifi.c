/* a96t3x6.c -- Linux driver for A96T3X6 chip as grip sensor
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
#include <linux/wakelock.h>
#include <asm/unaligned.h>
#include <linux/regulator/consumer.h>
#include <linux/sec_sysfs.h>
#include <linux/sensor/sensors_core.h>
#include <linux/pinctrl/consumer.h>
#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#include "a96t3x6_wifi.h"

struct a96t3x6_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct device *dev;
	struct mutex lock;
	struct pinctrl *pinctrl;
	struct delayed_work debug_work;

	const struct firmware *firm_data_bin;
	const u8 *firm_data_ums;
	char phys[32];
	long firm_size;
	int irq;
	struct wake_lock grip_wake_lock;
	u16 grip_p_thd;
	u16 grip_r_thd;
	u16 grip_n_thd;
	u16 grip_s1;
	u16 grip_s2;
	u16 grip_baseline;
	u16 grip_raw1;
	u16 grip_raw2;
	u16 grip_event;
	bool sar_mode;
	bool sar_enable;
	bool sar_enable_off;
#ifdef CONFIG_SEC_FACTORY
	int irq_count;
	int abnormal_mode;
	s16 diff;
	s16 max_diff;
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

	int ldo_en;			/* ldo_en pin gpio */
	int grip_int;			/* irq pin gpio */
	struct regulator *dvdd_vreg;	/* regulator */
	int (*power)(void *, bool on);	/* power onoff function ptr */
	const char *fw_path;
	bool bringup;
	bool probe_done;
	int firmup_cmd;
#if defined(CONFIG_MUIC_NOTIFIER)
	struct notifier_block cpuidle_muic_nb;
#endif
};

static int a96t3x6_i2c_read_checksum(struct a96t3x6_data *data);
static void a96t3x6_reset(struct a96t3x6_data *data);


static int a96t3x6_mode_enable(struct i2c_client *client, u8 cmd_reg, u8 cmd)
{
	return i2c_smbus_write_byte_data(client, cmd_reg, cmd);
}

static void a96t3x6_set_enable(struct a96t3x6_data *data, bool enable)
{
	SENSOR_INFO("%s\n", enable ? "turned on" : "turned off");

	if (enable == true) {
		a96t3x6_mode_enable(data->client, REG_SAR_ENABLE, CMD_ON);
		enable_irq(data->irq);
		enable_irq_wake(data->irq);
	} else {
		a96t3x6_mode_enable(data->client, REG_SAR_ENABLE, CMD_OFF);
		disable_irq(data->irq);
		disable_irq_wake(data->irq);
	}
}

static int a96t3x6_i2c_read(struct i2c_client *client,
		u8 reg, u8 *val, unsigned int len)
{
	struct a96t3x6_data *data = i2c_get_clientdata(client);
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

		SENSOR_ERR("fail(address set)(%d)(%d)\n", retry, ret);
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
		SENSOR_ERR("fail(data read)(%d)(%d)\n", retry, ret);
		usleep_range(10000, 11000);
	}
	mutex_unlock(&data->lock);
	return ret;
}

static int a96t3x6_i2c_read_data(struct i2c_client *client, u8 *val, unsigned int len)
{
	struct a96t3x6_data *data = i2c_get_clientdata(client);
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
		SENSOR_ERR("fail(data read)(%d)\n", retry);
		usleep_range(10000, 11000);
	}
	mutex_unlock(&data->lock);
	return ret;
}

static int a96t3x6_i2c_write(struct i2c_client *client, u8 reg, u8 *val)
{
	struct a96t3x6_data *data = i2c_get_clientdata(client);
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
		SENSOR_ERR("fail(%d)\n", retry);
		usleep_range(10000, 11000);
	}
	mutex_unlock(&data->lock);
	return ret;
}

static void a96t3x6_sar_only_mode(struct a96t3x6_data *data, int on)
{
	int retry = 3;
	int ret;
	u8 cmd;
	u8 r_buf;
	int mode_retry = 5;

	if (data->sar_mode == on) {
		SENSOR_INFO("skip already %s\n", (on == 1) ? "sar only mode" : "normal mode");
		return;
	}

	if (on == 1)
		cmd = CMD_ON;
	else
		cmd = CMD_OFF;

	SENSOR_INFO("%s, cmd=%x\n", (on == 1) ? "sar only mode" : "normal mode", cmd);

sar_mode:
	while (retry > 0) {
		ret = a96t3x6_i2c_write(data->client, REG_SAR_MODE, &cmd);
		if (ret < 0) {
			SENSOR_ERR("i2c read fail(%d), retry %d\n", ret, retry);
			retry--;
			msleep(20);
			continue;
		}
		break;
	}

	msleep(40);

	ret = a96t3x6_i2c_read(data->client, REG_SAR_MODE, &r_buf, 1);
	if (ret < 0)
		SENSOR_ERR("i2c read fail(%d)\n", ret);

	SENSOR_INFO("read reg = %x\n", r_buf);

	if ((r_buf != cmd) && (mode_retry > 0)) {
		SENSOR_INFO("change fail retry %d\n", 6 - mode_retry--);

		if (mode_retry == 0)
			a96t3x6_reset(data);

		goto sar_mode;
	}

	if (r_buf == CMD_ON)
		data->sar_mode = 1;
	else
		data->sar_mode = 0;
}

static void a96t3x6_sar_sensing(struct a96t3x6_data *data, int on)
{
	int ret;
	u8 cmd;

	if (on == 1)
		cmd = CMD_ON;
	else
		cmd = CMD_OFF;

	SENSOR_INFO("%s\n", (on) ? "on" : "off");

	ret = a96t3x6_i2c_write(data->client, REG_SAR_SENSING, &cmd);
	if (ret < 0)
		SENSOR_ERR("fail(%d)\n", ret);
}

static void a96t3x6_reset_for_bootmode(struct a96t3x6_data *data)
{
	data->power(data, false);
	msleep(50);
	data->power(data, true);
}

static void a96t3x6_reset(struct a96t3x6_data *data)
{
	if (data->enabled == false)
		return;

	SENSOR_INFO("start\n");
	disable_irq_nosync(data->irq);

	data->enabled = false;

	a96t3x6_reset_for_bootmode(data);
	msleep(RESET_DELAY);

	a96t3x6_sar_only_mode(data, 1);
	if (data->sar_enable)
		a96t3x6_set_enable(data, 1);

	data->enabled = true;

	enable_irq(data->irq);
	SENSOR_INFO("done\n");
}

static void a96t3x6_grip_sw_reset(struct a96t3x6_data *data)
{
	int ret;
	u8 cmd = CMD_SW_RESET;

	SENSOR_INFO("\n");
	ret = a96t3x6_i2c_write(data->client, REG_SW_RESET, &cmd);
	if (ret < 0)
		SENSOR_ERR("fail(%d)\n", ret);
}

static int a96t3x6_get_hallic_state(struct a96t3x6_data *data)
{
	char hall_buf[6];
	int ret = -ENODEV;
	int hall_state = -1;
	mm_segment_t old_fs;
	struct file *filep;

	memset(hall_buf, 0, sizeof(hall_buf));
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filep = filp_open(HALL_PATH, O_RDONLY, 0666);
	if (IS_ERR(filep)) {
		set_fs(old_fs);
		return hall_state;
	}

	ret = filep->f_op->read(filep, hall_buf,
		sizeof(hall_buf) - 1, &filep->f_pos);
	if (ret != sizeof(hall_buf) - 1)
		goto exit;

	if (strcmp(hall_buf, "CLOSE") == 0)
		hall_state = HALL_CLOSE_STATE;

exit:
	filp_close(filep, current->files);
	set_fs(old_fs);

	return hall_state;
}

static void a96t3x6_debug_work_func(struct work_struct *work)
{
	struct a96t3x6_data *data = container_of((struct delayed_work *)work,
		struct a96t3x6_data, debug_work);

	static int hall_prev_state;
	int hall_state;

	hall_state = a96t3x6_get_hallic_state(data);
	if (hall_state == HALL_CLOSE_STATE && hall_prev_state != hall_state) {
		SENSOR_INFO("%s - hall is closed\n", __func__);
		a96t3x6_grip_sw_reset(data);
	}
	hall_prev_state = hall_state;

	schedule_delayed_work_on(1, &data->debug_work, msecs_to_jiffies(2000));
}

static void a96t3x6_set_debug_work(struct a96t3x6_data *data, u8 enable,
		unsigned int time_ms)
{
	SENSOR_INFO("%s\n", __func__);
	
	if (enable == 1) {
		schedule_delayed_work_on(1, &data->debug_work,
			msecs_to_jiffies(time_ms));
	} else {
		cancel_delayed_work_sync(&data->debug_work);
	}
}

static irqreturn_t a96t3x6_interrupt(int irq, void *dev_id)
{
	struct a96t3x6_data *data = dev_id;
	struct i2c_client *client = data->client;
	int ret, retry;
	u8 buf;
	int grip_data;
	u8 grip_press = 0;
#ifdef CONFIG_SEC_FACTORY
	u8 r_buf[2];
#endif

	wake_lock(&data->grip_wake_lock);

	ret = a96t3x6_i2c_read(client, REG_BTNSTATUS, &buf, 1);
	if (ret < 0) {
		retry = 3;
		while (retry--) {
			SENSOR_ERR("read fail(%d)\n", retry);
			ret = a96t3x6_i2c_read(client, REG_BTNSTATUS, &buf, 1);
			if (ret == 0)
				break;
			usleep_range(10000, 11000);
		}
		if (retry == 0) {
			a96t3x6_reset(data);
			wake_unlock(&data->grip_wake_lock);
			return IRQ_HANDLED;
		}
	}

	SENSOR_INFO("buf = 0x%02x\n", buf);

	grip_data = (buf >> 4) & 0x03;
	grip_press = !(grip_data % 2);

	if (grip_data) {
		if (grip_press)
			input_report_rel(data->input_dev, REL_MISC, 1);
		else
			input_report_rel(data->input_dev, REL_MISC, 2);
		data->grip_event = grip_press;
	}
#ifdef CONFIG_SEC_FACTORY
	ret = a96t3x6_i2c_read(client, REG_SAR_DIFFDATA, r_buf, 2);
	if (ret < 0) {
		retry = 3;
		while (retry--) {
			SENSOR_ERR("read failed(%d)\n", retry);
			ret = a96t3x6_i2c_read(client,
					REG_SAR_DIFFDATA, r_buf, 2);
			if (ret == 0)
				break;
			usleep_range(10 * 1000, 10 * 1000);
		}
	}

	data->diff = (r_buf[0] << 8) | r_buf[1];
	if (data->abnormal_mode) {
		if (data->grip_event) {
			if (data->max_diff < data->diff)
				data->max_diff = data->diff;
			data->irq_count++;
		}
	}
#endif
	input_sync(data->input_dev);
	if (grip_data)
		SENSOR_INFO("%s %x\n", grip_press ? "grip P" : "grip R", buf);

	wake_unlock(&data->grip_wake_lock);
	return IRQ_HANDLED;
}

static ssize_t grip_sar_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", !data->sar_enable_off);
}

static ssize_t grip_sar_enable_store(struct device *dev,
		 struct device_attribute *attr, const char *buf, size_t count)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	int force;
	int ret;
	u8 cmd;

	ret = sscanf(buf, "%2d", &force);
	if (ret != 1) {
		SENSOR_ERR("cmd read err\n");
		return count;
	}

	if (!(force >= 0 && force <= 3)) {
		SENSOR_ERR("wrong command(%d)\n", force);
		return count;
	}

	/*force 0:0ff, 1:on, 2:force off, 3:force off->on  */

	if (force == 3) {
		data->sar_enable_off = false;
		SENSOR_INFO("Power back off force off -> on (%d)\n", data->sar_enable);
		if (!data->sar_enable)
			force = 1;
		else
			return count;
	}

	if (data->sar_enable_off) {
		if (force == 1)
			data->sar_enable = true;
		else
			data->sar_enable = false;
		SENSOR_INFO("skip, Power back off force off mode (%d)\n",
					data->sar_enable);
		return count;
	}

	if (force == 1) {
		cmd = 0x20;
	} else if (force == 2) {		//test app : Power back off _ force off
		cmd = 0x10;
		data->sar_enable_off = true;
	} else {
		cmd = 0x10;
	}
	SENSOR_INFO("force(%d)\n", force);

	if (force == 1) {
		data->sar_enable = true;
	} else {
		input_report_rel(data->input_dev, REL_MISC, 2);
		data->grip_event = 0;
		data->sar_enable = false;
	}

	a96t3x6_set_enable(data, data->sar_enable);	

	SENSOR_INFO("force(%d)\n", force);

	return count;
}

static ssize_t grip_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	u8 r_buf[4];
	int ret;

	ret = a96t3x6_i2c_read(data->client, REG_SAR_THRESHOLD, r_buf, 4);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		data->grip_p_thd = 0;
		data->grip_r_thd = 0;
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	data->grip_p_thd = (r_buf[0] << 8) | r_buf[1];
	data->grip_r_thd = (r_buf[2] << 8) | r_buf[3];

	ret = a96t3x6_i2c_read(data->client, REG_SAR_NOISE_THRESHOLD, r_buf, 2);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		data->grip_n_thd = 0;
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	data->grip_n_thd = (r_buf[0] << 8) | r_buf[1];

	return sprintf(buf, "%d,%d,%d\n", data->grip_p_thd, data->grip_r_thd, data->grip_n_thd);
}
static ssize_t grip_total_cap_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	u8 r_buf[2];
	u8 cmd;
	int ret;
	int value;

	cmd = 0x20;
	ret = a96t3x6_i2c_write(data->client, REG_SAR_TOTALCAP, &cmd);
	if (ret < 0)
		SENSOR_ERR("write fail(%d)\n", ret);

	usleep_range(10, 20);

	ret = a96t3x6_i2c_read(data->client, REG_SAR_TOTALCAP_READ, r_buf, 2);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	value = (r_buf[0] << 8) | r_buf[1];

	return snprintf(buf, PAGE_SIZE, "%d\n", value / 100);
}

static ssize_t grip_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	u8 r_buf[4];
	int ret;

	ret = a96t3x6_i2c_read(data->client, REG_SAR_DIFFDATA, r_buf, 4);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		data->grip_s1 = 0;
		data->grip_s2 = 0;
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	data->grip_s1 = (r_buf[0] << 8) | r_buf[1];
	data->grip_s2 = (r_buf[2] << 8) | r_buf[3];

	return sprintf(buf, "%d,%d\n", data->grip_s1, data->grip_s2);
}

static ssize_t grip_diff_debug_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	u8 r_buf[4];
	u16 sardiff[2];
	int ret;

	ret = a96t3x6_i2c_read(data->client, REG_SAR_DIFFDATA_D, r_buf, 4);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	sardiff[0] = (r_buf[0] << 8) | r_buf[1];
	sardiff[1] = (r_buf[2] << 8) | r_buf[3];

	return sprintf(buf, "%d,%d\n", sardiff[0], sardiff[1]);
}

static ssize_t grip_baseline_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	u8 r_buf[2];
	int ret;

	ret = a96t3x6_i2c_read(data->client, REG_SAR_BASELINE, r_buf, 2);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n",  ret);
		data->grip_baseline = 0;
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	data->grip_baseline = (r_buf[0] << 8) | r_buf[1];

	return snprintf(buf, PAGE_SIZE, "%d\n", data->grip_baseline);
}

static ssize_t grip_raw_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	u8 r_buf[4];
	int ret;

	ret = a96t3x6_i2c_read(data->client, REG_SAR_RAWDATA, r_buf, 4);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		data->grip_raw1 = 0;
		data->grip_raw2 = 0;
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	data->grip_raw1 = (r_buf[0] << 8) | r_buf[1];
	data->grip_raw2 = 0;

	return sprintf(buf, "%d,%d\n", data->grip_raw1,
			data->grip_raw2);
}

static ssize_t grip_raw_debug_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	u8 r_buf[4];
	u16 raw;
	int ret;

	ret = a96t3x6_i2c_read(data->client, REG_SAR_RAWDATA_D, r_buf, 4);
	if (ret < 0) {
		SENSOR_ERR("fail(%d)\n", ret);
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	}
	raw = (r_buf[0] << 8) | r_buf[1];

	return sprintf(buf, "%d\n", raw);
}

static ssize_t grip_gain_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d,%d,%d,%d\n", 0, 0, 0, 0);
}

static ssize_t grip_check_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->grip_event);
}

static ssize_t grip_sw_reset(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	u8 cmd;
	int ret;

	ret = kstrtou8(buf, 2, &cmd);
	if (ret) {
		SENSOR_ERR("cmd read err\n");
		return count;
	}

	if (!(cmd == 1)) {
		SENSOR_ERR("wrong command(%d)\n", cmd);
		return count;
	}

	data->grip_event = 0;

	SENSOR_INFO("cmd(%d)\n", cmd);

	a96t3x6_grip_sw_reset(data);

	return count;
}

static ssize_t grip_sensing_change(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	int ret, earjack;

	ret = sscanf(buf, "%2d", &earjack);
	if (ret != 1) {
		SENSOR_ERR("cmd read err\n");
		return count;
	}

	if (!(earjack == 0 || earjack == 1)) {
		SENSOR_ERR("wrong command(%d)\n", earjack);
		return count;
	}

	if (earjack == 1)	//EarJack inserted
		a96t3x6_sar_sensing(data, 0);
	else
		a96t3x6_sar_sensing(data, 1);

	SENSOR_INFO("earjack (%d)\n", earjack);

	return count;
}

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
static ssize_t grip_sar_press_threshold_store(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);

	int ret;
	int threshold;
	u8 cmd[2];

	ret = sscanf(buf, "%11d", &threshold);
	if (ret != 1) {
		SENSOR_ERR("failed to read thresold, buf is %s\n", buf);
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

	SENSOR_INFO("buf : %d, threshold : %d\n", threshold,
			(cmd[0] << 8) | cmd[1]);

	ret = a96t3x6_i2c_write(data->client, REG_SAR_THRESHOLD, &cmd[0]);
	if (ret != 0) {
		SENSOR_INFO("failed to write press_threhold data1");
		goto press_threshold_out;
	}
	ret = a96t3x6_i2c_write(data->client, REG_SAR_THRESHOLD + 0x01, &cmd[1]);
	if (ret != 0) {
		SENSOR_INFO("failed to write press_threhold data2");
		goto press_threshold_out;
	}
press_threshold_out:
	return count;
}

static ssize_t grip_sar_release_threshold_store(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);

	int ret;
	int threshold;
	u8 cmd[2];

	ret = sscanf(buf, "%11d", &threshold);
	if (ret != 1) {
		SENSOR_ERR("failed to read thresold, buf is %s\n", buf);
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

	SENSOR_INFO("buf : %d, threshold : %d\n", threshold,
				(cmd[0] << 8) | cmd[1]);

	ret = a96t3x6_i2c_write(data->client, REG_SAR_THRESHOLD + 0x02,
				&cmd[0]);
	SENSOR_INFO("ret : %d\n", ret);

	if (ret != 0) {
		SENSOR_INFO("failed to write release_threshold_data1");
		goto release_threshold_out;
	}
	ret = a96t3x6_i2c_write(data->client, REG_SAR_THRESHOLD + 0x03,
				&cmd[1]);
	SENSOR_INFO("ret : %d\n", ret);
	if (ret != 0) {
		SENSOR_INFO("failed to write release_threshold_data2");
		goto release_threshold_out;
	}
release_threshold_out:
	return count;
}

static ssize_t grip_mode_change(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	int ret, mode;

	ret = sscanf(buf, "%2d", &mode);
	if (ret != 1) {
		SENSOR_ERR("cmd read err\n");
		return count;
	}

	if (!(mode == 0 || mode == 1)) {
		SENSOR_ERR("wrong command(%d)\n", mode);
		return count;
	}

	SENSOR_INFO("mode(%d)\n", mode);

	a96t3x6_sar_only_mode(data, mode);

	return count;
}
#endif

#ifdef CONFIG_SEC_FACTORY
static ssize_t a96t3x6_irq_count_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	int result = 0;

	if (data->irq_count)
		result = -1;

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", result,
			data->irq_count, data->max_diff);
}

static ssize_t a96t3x6_irq_count_store(struct device *dev,
		 struct device_attribute *attr, const char *buf, size_t count)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	u8 onoff;
	int ret;

	ret = kstrtou8(buf, 10, &onoff);
	if (ret < 0) {
		SENSOR_ERR("kstrtou8 failed.(%d)\n", ret);
		return count;
	}

	mutex_lock(&data->lock);
	if (onoff == 0) {
		data->abnormal_mode = 0;
	} else if (onoff == 1) {
		data->abnormal_mode = 1;
		data->irq_count = 0;
		data->max_diff = 0;
	} else {
		SENSOR_ERR("Invalid value.(%d)\n", onoff);
	}
	mutex_unlock(&data->lock);

	SENSOR_INFO("result : %d\n", onoff);
	return count;
}
#endif

static ssize_t grip_vendor_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}
static ssize_t grip_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t bin_fw_ver(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", data->fw_ver_bin);
}

static int a96t3x6_get_fw_version(struct a96t3x6_data *data, bool bootmode)
{
	struct i2c_client *client = data->client;
	u8 buf;
	int ret;
	int retry = 3;

	ret = a96t3x6_i2c_read(client, REG_FW_VER, &buf, 1);
	if (ret < 0) {
		while (retry--) {
			SENSOR_ERR("read fail(%d)\n", retry);
			if (!bootmode)
				a96t3x6_reset(data);
			else
				return -1;
			ret = a96t3x6_i2c_read(client, REG_FW_VER, &buf, 1);
			if (ret == 0)
				break;
		}
		if (retry <= 0)
			return -1;
	}
	data->fw_ver = buf;

	retry = 3;
	ret = a96t3x6_i2c_read(client, REG_MODEL_NO, &buf, 1);
	if (ret < 0) {
		while (retry--) {
			SENSOR_ERR("read fail(%d)\n", retry);
			if (!bootmode)
				a96t3x6_reset(data);
			else
				return -1;
			ret = a96t3x6_i2c_read(client, REG_MODEL_NO, &buf, 1);
			if (ret == 0)
				break;
		}
		if (retry <= 0)
			return -1;
	}
	data->md_ver = buf;

	SENSOR_INFO("fw = 0x%x, md = 0x%x\n", data->fw_ver, data->md_ver);
	return 0;
}

static ssize_t read_fw_ver(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	int ret;

	ret = a96t3x6_get_fw_version(data, false);
	if (ret < 0) {
		SENSOR_ERR("read fail\n");
		data->fw_ver = 0;
	}

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", data->fw_ver);
}

static int a96t3x6_load_fw_kernel(struct a96t3x6_data *data)
{
	int ret = 0;

	ret = request_firmware(&data->firm_data_bin,
		data->fw_path, &data->client->dev);
	if (ret) {
		SENSOR_ERR("request_firmware fail.\n");
		return ret;
	}
	data->firm_size = data->firm_data_bin->size;
	data->fw_ver_bin = data->firm_data_bin->data[5];
	data->md_ver_bin = data->firm_data_bin->data[1];
	SENSOR_INFO("fw = 0x%x, md = 0x%x\n", data->fw_ver_bin, data->md_ver_bin);

	data->checksum_h_bin = data->firm_data_bin->data[8];
	data->checksum_l_bin = data->firm_data_bin->data[9];

	SENSOR_INFO("crc 0x%x 0x%x\n", data->checksum_h_bin, data->checksum_l_bin);

	return ret;
}

static int a96t3x6_load_fw(struct a96t3x6_data *data, u8 cmd)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;

	switch (cmd) {
	case BUILT_IN:
		break;

	case SDCARD:
		old_fs = get_fs();
		set_fs(get_ds());
		fp = filp_open(TK_FW_PATH_SDCARD, O_RDONLY, 0400);
		if (IS_ERR(fp)) {
			SENSOR_ERR("%s open error\n", TK_FW_PATH_SDCARD);
			ret = -ENOENT;
			goto fail_sdcard_open;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		data->firm_data_ums = kzalloc((size_t)fsize, GFP_KERNEL);
		if (!data->firm_data_ums) {
			SENSOR_ERR("fail to kzalloc for fw\n");
			ret = -ENOMEM;
			goto fail_sdcard_kzalloc;
		}

		nread = vfs_read(fp,
			(char __user *)data->firm_data_ums, fsize, &fp->f_pos);
		if (nread != fsize) {
			SENSOR_ERR("fail to vfs_read file\n");
			ret = -EINVAL;
			goto fail_sdcard_size;
		}
		filp_close(fp, current->files);
		set_fs(old_fs);
		data->firm_size = nread;
		break;

	default:
		ret = -1;
		break;
	}
	SENSOR_INFO("fw_size : %lu, success\n", data->firm_size);
	return ret;

fail_sdcard_size:
	kfree(&data->firm_data_ums);
fail_sdcard_kzalloc:
	filp_close(fp, current->files);
fail_sdcard_open:
	set_fs(old_fs);
	return ret;
}

static int a96t3x6_check_busy(struct a96t3x6_data *data)
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
		SENSOR_ERR("busy %d\n", count);
	return ret;
}

static int a96t3x6_i2c_read_checksum(struct a96t3x6_data *data)
{
	unsigned char buf[6] = {0xAC, 0x9E, 0x10, 0x00, 0x3F, 0xFF};
	unsigned char buf2[1] = {0x00};
	unsigned char checksum[6] = {0, };
	int ret;

	i2c_master_send(data->client, buf, 6);
	usleep_range(5000, 6000);

	i2c_master_send(data->client, buf2, 1);
	usleep_range(5000, 6000);

	ret = a96t3x6_i2c_read_data(data->client, checksum, 6);

	SENSOR_INFO("ret:%d [%X][%X][%X][%X][%X]\n", ret,
			checksum[0], checksum[1], checksum[2], checksum[4], checksum[5]);
	data->checksum_h = checksum[4];
	data->checksum_l = checksum[5];
	return 0;
}

static int a96t3x6_fw_write(struct a96t3x6_data *data, unsigned char *addrH,
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
		SENSOR_ERR("write fail[%x%x], %d\n", *addrH, *addrL, ret);
		return ret;
	}

	usleep_range(3000, 3000);

	a96t3x6_check_busy(data);

	return 0;
}

static int a96t3x6_fw_mode_enter(struct a96t3x6_data *data)
{
	unsigned char buf[2] = {0xAC, 0x5B};
	u8 ic_ver = 0;
	int ret = 0;

	ret = i2c_master_send(data->client, buf, 2);
	if (ret != 2) {
		SENSOR_ERR("write fail\n");
		return -1;
	}

	ret = i2c_master_recv(data->client, &ic_ver, 1);
	SENSOR_INFO("%2x, %2x\n", data->firmup_cmd, ic_ver);
	if (data->firmup_cmd != ic_ver) {
		SENSOR_ERR("ic not matched, firmup fail\n");
		return -2;
	}
	return 0;
}

static int a96t3x6_flash_erase(struct a96t3x6_data *data)
{
	unsigned char buf[2] = {0xAC, 0x2D};
	int ret = 0;

	ret = i2c_master_send(data->client, buf, 2);
	if (ret != 2) {
		SENSOR_ERR("write fail\n");
		return -1;
	}

	return 0;

}

static int a96t3x6_fw_mode_exit(struct a96t3x6_data *data)
{
	unsigned char buf[2] = {0xAC, 0xE1};
	int ret = 0;

	ret = i2c_master_send(data->client, buf, 2);
	if (ret != 2) {
		SENSOR_ERR("write fail\n");
		return -1;
	}

	return 0;
}

static int a96t3x6_fw_update(struct a96t3x6_data *data, u8 cmd)
{
	int ret, i = 0;
	int count;
	unsigned short address;
	unsigned char addrH, addrL;
	unsigned char buf[32] = {0, };

	SENSOR_INFO("start\n");

	count = data->firm_size / 32;
	address = 0x800;

	SENSOR_INFO("reset\n");
	a96t3x6_reset_for_bootmode(data);
	msleep(BOOT_DELAY);
	ret = a96t3x6_fw_mode_enter(data);
	if (ret < 0) {
		SENSOR_ERR("a96t3x6_fw_mode_enter fail\n");
		return ret;
	}
	usleep_range(5 * 1000, 5 * 1000);
	SENSOR_INFO("fw_mode_cmd sent\n");

	ret = a96t3x6_flash_erase(data);
	msleep(1400);
	SENSOR_INFO("fw_write start\n");

	for (i = 1; i < count; i++) {
		/* first 32byte is header */
		addrH = (unsigned char)((address >> 8) & 0xFF);
		addrL = (unsigned char)(address & 0xFF);
		if (cmd == BUILT_IN)
			memcpy(buf, &data->firm_data_bin->data[i * 32], 32);
		else if (cmd == SDCARD)
			memcpy(buf, &data->firm_data_ums[i * 32], 32);

		ret = a96t3x6_fw_write(data, &addrH, &addrL, buf);
		if (ret < 0) {
			SENSOR_ERR("err, no device : %d\n", ret);
			return ret;
		}

		address += 0x20;

		memset(buf, 0, 32);
	}
	ret = a96t3x6_i2c_read_checksum(data);
	SENSOR_INFO("checksum read%d\n", ret);

	ret = a96t3x6_fw_mode_exit(data);
	SENSOR_INFO("fw_write end\n");

	return ret;
}

static void a96t3x6_release_fw(struct a96t3x6_data *data, u8 cmd)
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

static int a96t3x6_flash_fw(struct a96t3x6_data *data, bool probe, u8 cmd)
{
	int retry = 2;
	int ret;
	int block_count;
	const u8 *fw_data;

	ret = a96t3x6_get_fw_version(data, probe);
	if (ret)
		data->fw_ver = 0;

	ret = a96t3x6_load_fw(data, cmd);
	if (ret) {
		SENSOR_ERR("fw load fail\n");
		return ret;
	}

	switch (cmd) {
	case BUILT_IN:
		fw_data = data->firm_data_bin->data;
		break;

	case SDCARD:
		fw_data = data->firm_data_ums;
		break;

	default:
		return -1;
	}

	block_count = (int)(data->firm_size / 32);

	while (retry--) {
		ret = a96t3x6_fw_update(data, cmd);
		if (ret < 0)
			break;

		if (cmd == BUILT_IN) {
			if ((data->checksum_h != data->checksum_h_bin) ||
				(data->checksum_l != data->checksum_l_bin)) {
				SENSOR_ERR("checksum fail.(0x%x,0x%x),(0x%x,0x%x) retry:%d\n",
						data->checksum_h, data->checksum_l,
						data->checksum_h_bin, data->checksum_l_bin, retry);
				ret = -1;
				continue;
			}
		}
		a96t3x6_reset_for_bootmode(data);
		msleep(RESET_DELAY);
		ret = a96t3x6_get_fw_version(data, true);
		if (ret) {
			SENSOR_ERR("fw version read fail\n");
			ret = -1;
			continue;
		}

		if (data->fw_ver == 0) {
			SENSOR_ERR("fw version fail (0x%x)\n", data->fw_ver);
			ret = -1;
			continue;
		}

		if ((cmd == BUILT_IN) && (data->fw_ver != data->fw_ver_bin)) {
			SENSOR_ERR("fw version fail 0x%x, 0x%x\n",
						data->fw_ver, data->fw_ver_bin);
			ret = -1;
			continue;
		}
		ret = 0;
		break;
	}

	a96t3x6_release_fw(data, cmd);

	return ret;
}

static ssize_t grip_fw_update(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
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
		ret = a96t3x6_load_fw_kernel(data);
		if (ret)
			SENSOR_ERR("failed(%d)\n", ret);
		else
			SENSOR_ERR("fw version read success (%d)\n", ret);
	}
	ret = a96t3x6_flash_fw(data, false, cmd);

	a96t3x6_sar_only_mode(data, 1);

	data->enabled = true;
	enable_irq(data->irq);
	if (ret) {
		SENSOR_ERR("fail\n");
		data->fw_update_state = 0;

	} else {
		SENSOR_INFO("success\n");
		data->fw_update_state = 0;
	}

fw_update_out:
	SENSOR_ERR("%d\n", data->fw_update_state);

	return count;
}

static ssize_t grip_fw_update_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);
	int count = 0;

	SENSOR_INFO("%d\n", data->fw_update_state);

	if (data->fw_update_state == 0)
		count = snprintf(buf, PAGE_SIZE, "PASS\n");
	else if (data->fw_update_state == 1)
		count = snprintf(buf, PAGE_SIZE, "Downloading\n");
	else if (data->fw_update_state == 2)
		count = snprintf(buf, PAGE_SIZE, "Fail\n");

	return count;
}

static ssize_t a96t3x6_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->sar_enable);
}

static ssize_t a96t3x6_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 value;
	bool enable;
	int ret;
	struct a96t3x6_data *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &value);
	if (ret) {
		SENSOR_ERR("Invalid argument\n");
		return size;
	}
	enable = value ? true : false;

	if (data->sar_enable == enable)
		return size;

	a96t3x6_set_enable(data, enable);
	data->sar_enable = enable;

	return size;
}

static DEVICE_ATTR(grip_threshold, 0444, grip_threshold_show, NULL);
static DEVICE_ATTR(grip_total_cap, 0444, grip_total_cap_show, NULL);
static DEVICE_ATTR(grip_sar_enable, 0664, grip_sar_enable_show, grip_sar_enable_store);
static DEVICE_ATTR(grip_sw_reset, 0220, NULL, grip_sw_reset);
static DEVICE_ATTR(grip_earjack, 0220, NULL, grip_sensing_change);
static DEVICE_ATTR(grip, 0444, grip_show, NULL);
static DEVICE_ATTR(grip_diff_d, 0444, grip_diff_debug_show, NULL);
static DEVICE_ATTR(grip_baseline, 0444, grip_baseline_show, NULL);
static DEVICE_ATTR(grip_raw, 0444, grip_raw_show, NULL);
static DEVICE_ATTR(grip_raw_d, 0444, grip_raw_debug_show, NULL);
static DEVICE_ATTR(grip_gain, 0444, grip_gain_show, NULL);
static DEVICE_ATTR(grip_check, 0444, grip_check_show, NULL);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
static DEVICE_ATTR(grip_sar_only_mode, 0220, NULL, grip_mode_change);
static DEVICE_ATTR(grip_sar_press_threshold, 0220,
		NULL, grip_sar_press_threshold_store);
static DEVICE_ATTR(grip_sar_release_threshold, 0220,
		NULL, grip_sar_release_threshold_store);
#endif
#ifdef CONFIG_SEC_FACTORY
static DEVICE_ATTR(grip_irq_count, 0664, a96t3x6_irq_count_show,
			a96t3x6_irq_count_store);
#endif
static DEVICE_ATTR(name, 0444, grip_name_show, NULL);
static DEVICE_ATTR(vendor, 0444, grip_vendor_show, NULL);
static DEVICE_ATTR(grip_firm_version_phone, 0444, bin_fw_ver, NULL);
static DEVICE_ATTR(grip_firm_version_panel, 0444, read_fw_ver, NULL);
static DEVICE_ATTR(grip_firm_update, 0220, NULL, grip_fw_update);
static DEVICE_ATTR(grip_firm_update_status, 0444, grip_fw_update_status, NULL);

static struct device_attribute *grip_sensor_attributes[] = {
	&dev_attr_grip_threshold,
	&dev_attr_grip_total_cap,
	&dev_attr_grip_sar_enable,
	&dev_attr_grip_sw_reset,
	&dev_attr_grip_earjack,
	&dev_attr_grip,
	&dev_attr_grip_diff_d,
	&dev_attr_grip_baseline,
	&dev_attr_grip_raw,
	&dev_attr_grip_raw_d,
	&dev_attr_grip_gain,
	&dev_attr_grip_check,
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	&dev_attr_grip_sar_only_mode,
	&dev_attr_grip_sar_press_threshold,
	&dev_attr_grip_sar_release_threshold,
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
	NULL,
};

static DEVICE_ATTR(enable, 0664, a96t3x6_enable_show, a96t3x6_enable_store);

static struct attribute *a96t3x6_attributes[] = {
	&dev_attr_enable.attr,
	NULL
};

static struct attribute_group a96t3x6_attribute_group = {
	.attrs = a96t3x6_attributes
};

static int a96t3x6_fw_check(struct a96t3x6_data *data)
{
	int ret;
	bool force = false;

	if (data->bringup) {
		SENSOR_INFO("bring up mode. skip firmware check\n");
		return 0;
	}

	ret = a96t3x6_get_fw_version(data, true);
	if (ret)
		SENSOR_ERR("i2c fail(%d), addr[%d]\n", ret, data->client->addr);

	ret = a96t3x6_load_fw_kernel(data);
	if (ret)
		SENSOR_ERR("failed load_fw_kernel(%d)\n", ret);
	else
		SENSOR_ERR("fw version read success (%d)\n", ret);

	if (data->md_ver != data->md_ver_bin) {
		SENSOR_ERR("MD version is different.(IC %x, BN %x). Do force FW update\n",
			data->md_ver, data->md_ver_bin);
		force = true;
	}

	if (data->fw_ver < data->fw_ver_bin || data->fw_ver > 0xf0
				|| force == true) {
		SENSOR_ERR("excute fw update (0x%x -> 0x%x)\n",
			data->fw_ver, data->fw_ver_bin);
		ret = a96t3x6_flash_fw(data, true, BUILT_IN);
		if (ret)
			SENSOR_ERR("failed to a96t3x6_flash_fw (%d)\n", ret);
		else
			SENSOR_INFO("fw update success\n");
	}
	return ret;
}

static int a96t3x6_power_onoff(void *pdata, bool on)
{
	struct a96t3x6_data *data = (struct a96t3x6_data *)pdata;

	int ret = 0;

	if (data->ldo_en) {
		gpio_set_value(data->ldo_en, on);
		SENSOR_INFO("ldo_en power %d\n", on);
	}

	data->dvdd_vreg = regulator_get(NULL, "vtouch_2.8v");
	if (IS_ERR(data->dvdd_vreg)) {
		data->dvdd_vreg = NULL;
		SENSOR_ERR("dvdd_vreg get error, ignoring\n");
	}

	if (on) {
		if (data->dvdd_vreg) {
			ret = regulator_enable(data->dvdd_vreg);
			if (ret) {
				SENSOR_ERR("dvdd reg enable fail\n");
				return ret;
			}
		}
	} else {
		if (data->dvdd_vreg) {
			ret = regulator_disable(data->dvdd_vreg);
			if (ret) {
				SENSOR_ERR("dvdd reg disable fail\n");
				return ret;
			}
		}
	}
	regulator_put(data->dvdd_vreg);

	SENSOR_INFO("%s\n", on ? "on" : "off");

	return ret;
}

static int a96t3x6_irq_init(struct device *dev,
			struct a96t3x6_data *data)
{
	int ret = 0;

	ret = gpio_request(data->grip_int, "a96t3x6_IRQ");
	if (ret < 0) {
		SENSOR_ERR("gpio %d request failed (%d)\n", data->grip_int, ret);
		return ret;
	}

	ret = gpio_direction_input(data->grip_int);
	if (ret < 0) {
		SENSOR_ERR("failed to set direction input gpio %d(%d)\n",
				data->grip_int, ret);
				gpio_free(data->grip_int);
				return ret;
	}
	// assigned power function to function ptr
	data->power = a96t3x6_power_onoff;

	return ret;
}

static int a96t3x6_parse_dt(struct a96t3x6_data *data, struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct pinctrl *p;
	int ret;
	enum of_gpio_flags flags;

	data->grip_int = of_get_named_gpio(np, "a96t3x6,irq_gpio", 0);
	if (data->grip_int < 0) {
		SENSOR_ERR("Cannot get grip_int\n");
		return data->grip_int;
	}

	data->ldo_en = of_get_named_gpio_flags(np, "a96t3x6,ldo_en", 0, &flags);
	if (data->ldo_en < 0) {
		SENSOR_ERR("fail to get ldo_en\n");
		data->ldo_en = 0;
	} else {
		ret = gpio_request(data->ldo_en, "a96t3x6_ldo_en");
		if (ret < 0) {
			SENSOR_ERR("gpio %d request failed %d\n", data->ldo_en, ret);
			return ret;
		}
		gpio_direction_output(data->ldo_en, 0);
	}

	ret = of_property_read_string(np, "a96t3x6,fw_path", (const char **)&data->fw_path);
	if (ret < 0) {
		SENSOR_ERR("failed to read fw_path %d\n", ret);
		data->fw_path = TK_FW_PATH_BIN;
	}
	SENSOR_INFO("fw path %s\n", data->fw_path);

	data->bringup = of_property_read_bool(np, "a96t3x6,bringup");

	ret = of_property_read_u32(np, "a96t3x6,firmup_cmd", &data->firmup_cmd);
	if (ret < 0)
		data->firmup_cmd = 0;

	p = pinctrl_get_select_default(dev);
	if (IS_ERR(p)) {
		SENSOR_INFO("failed pinctrl_get\n");
		return -EINVAL;
	}

	SENSOR_INFO("grip_int:%d, ldo_en:%d\n", data->grip_int, data->ldo_en);

	return 0;
}

#if defined(CONFIG_MUIC_NOTIFIER)
static int a96t3x6_cpuidle_muic_notifier(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct a96t3x6_data *grip_data;
	u8 cmd = CMD_ON;

	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;

	grip_data = container_of(nb, struct a96t3x6_data, cpuidle_muic_nb);
	switch (attached_dev) {
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			cmd = CMD_OFF;
			SENSOR_INFO("TA/USB is inserted\n");
		}
		else if (action == MUIC_NOTIFY_CMD_DETACH) {
			cmd = CMD_ON;
			SENSOR_INFO("TA/USB is removed\n");
		}
		a96t3x6_i2c_write(grip_data->client, REG_TSPTA, &cmd);
		break;
	default:
		break;
	}

	SENSOR_INFO("dev=%d, action=%lu\n", attached_dev, action);

	return NOTIFY_DONE;
}
#endif

static int a96t3x6_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct a96t3x6_data *data;
	struct input_dev *input_dev;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		SENSOR_ERR("i2c_check_functionality fail\n");
		return -EIO;
	}

	data = kzalloc(sizeof(struct a96t3x6_data), GFP_KERNEL);
	if (!data) {
		SENSOR_ERR("Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		SENSOR_ERR("Failed to allocate memory for input device\n");
		ret = -ENOMEM;
		goto err_input_alloc;
	}

	data->client = client;
	data->input_dev = input_dev;
	data->probe_done = false;
	wake_lock_init(&data->grip_wake_lock, WAKE_LOCK_SUSPEND, "grip wake lock");

	ret = a96t3x6_parse_dt(data, &client->dev);
	if (ret) {
		SENSOR_ERR("failed to a96t3x6_parse_dt\n");
		goto err_config;
	}

	ret = a96t3x6_irq_init(&client->dev, data);
	if (ret) {
		SENSOR_ERR("failed to init reg\n");
		goto pwr_config;
	}

	if (data->power) {
		data->power(data, true);
		msleep(RESET_DELAY);
	}

	data->irq = -1;
	client->irq = gpio_to_irq(data->grip_int);
	mutex_init(&data->lock);

	i2c_set_clientdata(client, data);

	ret = a96t3x6_fw_check(data);
	if (ret) {
		SENSOR_ERR("failed to firmware check (%d)\n", ret);
		goto err_reg_input_dev;
	}

	input_dev->name = MODULE_NAME;
	input_dev->id.bustype = BUS_I2C;

	input_set_capability(input_dev, EV_REL, REL_MISC);
	input_set_drvdata(input_dev, data);

	INIT_DELAYED_WORK(&data->debug_work, a96t3x6_debug_work_func);

	ret = input_register_device(input_dev);
	if (ret) {
		SENSOR_ERR("failed to register input dev (%d)\n",
			ret);
		goto err_reg_input_dev;
	}

	ret = sensors_create_symlink(&data->input_dev->dev.kobj,
					data->input_dev->name);
	if (ret < 0) {
		SENSOR_ERR("Failed to create sysfs symlink\n");
		goto err_sysfs_symlink;
	}

	ret = sysfs_create_group(&data->input_dev->dev.kobj,
				&a96t3x6_attribute_group);
	if (ret < 0) {
		SENSOR_ERR("Failed to create sysfs group\n");
		goto err_sysfs_group;
	}

	ret = sensors_register(&data->dev, data, grip_sensor_attributes,
				MODULE_NAME);
	if (ret) {
		SENSOR_ERR("could not register grip_sensor(%d)\n", ret);
		goto err_sensor_register;
	}

	data->enabled = true;

	ret = request_threaded_irq(client->irq, NULL, a96t3x6_interrupt,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT, MODEL_NAME, data);

	if (ret < 0) {
		SENSOR_ERR("Failed to register interrupt\n");
		goto err_req_irq;
	}
	data->irq = client->irq;
	data->dev = &client->dev;

	device_init_wakeup(&client->dev, true);

	a96t3x6_sar_only_mode(data, 1);
	a96t3x6_set_debug_work(data, 1, 20000);
	
#if defined(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&data->cpuidle_muic_nb,
		a96t3x6_cpuidle_muic_notifier, MUIC_NOTIFY_DEV_CPUIDLE);
#endif
	SENSOR_ERR("done\n");
	data->probe_done = true;
	return 0;

err_req_irq:
	sensors_unregister(data->dev, grip_sensor_attributes);
err_sensor_register:
	sysfs_remove_group(&data->input_dev->dev.kobj,
			&a96t3x6_attribute_group);
err_sysfs_group:
	sensors_remove_symlink(&data->input_dev->dev.kobj, input_dev->name);
err_sysfs_symlink:
	input_unregister_device(input_dev);
err_reg_input_dev:
	mutex_destroy(&data->lock);
	gpio_free(data->grip_int);
	if (data->power)
		data->power(data, false);
pwr_config:
err_config:
	wake_lock_destroy(&data->grip_wake_lock);
	input_free_device(input_dev);
err_input_alloc:
	kfree(data);
err_alloc:
	SENSOR_ERR("failed\n");
	return ret;
}

static int a96t3x6_remove(struct i2c_client *client)
{
	struct a96t3x6_data *data = i2c_get_clientdata(client);

	if (data->enabled)
		data->power(data, false);

	data->enabled = false;
	device_init_wakeup(&client->dev, false);
	wake_lock_destroy(&data->grip_wake_lock);
	cancel_delayed_work_sync(&data->debug_work);

	if (data->irq >= 0)
		free_irq(data->irq, data);
	sensors_unregister(data->dev, grip_sensor_attributes);
	sysfs_remove_group(&data->input_dev->dev.kobj,
				&a96t3x6_attribute_group);
	sensors_remove_symlink(&data->input_dev->dev.kobj,
				data->input_dev->name);
	input_unregister_device(data->input_dev);
	input_free_device(data->input_dev);
	kfree(data);

	return 0;
}

static int a96t3x6_suspend(struct device *dev)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);

	SENSOR_INFO("%s\n", __func__);
	a96t3x6_set_debug_work(data, 0, 1000);

	return 0;
}

static int a96t3x6_resume(struct device *dev)
{
	struct a96t3x6_data *data = dev_get_drvdata(dev);

	SENSOR_INFO("%s\n", __func__);
	a96t3x6_set_debug_work(data, 1, 1000);

	return 0;
}

static void a96t3x6_shutdown(struct i2c_client *client)
{
	struct a96t3x6_data *data = i2c_get_clientdata(client);

	a96t3x6_set_debug_work(data, 0, 1000);

	if (data->enabled) {
		disable_irq(data->irq);
		data->power(data, false);
	}
	data->enabled = false;
}

static const struct i2c_device_id a96t3x6_device_id[] = {
	{MODULE_NAME, 0},	
};

MODULE_DEVICE_TABLE(i2c, a96t3x6_device_id);

#ifdef CONFIG_OF
static const struct of_device_id a96t3x6_match_table[] = {
	{ .compatible = "a96t3x6",},
	{ },
};
#else
#define a96t3x6_match_table NULL
#endif

static const struct dev_pm_ops a96t3x6_pm_ops = {
	.suspend = a96t3x6_suspend,
	.resume = a96t3x6_resume,
};

static struct i2c_driver a96t3x6_driver = {
	.probe = a96t3x6_probe,
	.remove = a96t3x6_remove,
	.shutdown = a96t3x6_shutdown,
	.id_table = a96t3x6_device_id,
	.driver = {
		   .name = MODEL_NAME,
		   .of_match_table = a96t3x6_match_table,
		   .pm = &a96t3x6_pm_ops
	},
};

static int __init a96t3x6_init(void)
{
	return i2c_add_driver(&a96t3x6_driver);
}

static void __exit a96t3x6_exit(void)
{
	i2c_del_driver(&a96t3x6_driver);
}

module_init(a96t3x6_init);
module_exit(a96t3x6_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Grip sensor driver for A96T3X6 'chip");
MODULE_LICENSE("GPL");