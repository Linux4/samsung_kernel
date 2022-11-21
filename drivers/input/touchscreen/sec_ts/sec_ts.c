/* drivers/input/touchscreen/sec_ts.c
 *
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * http://www.samsungsemi.com/
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/time.h>
#include <linux/completion.h>
#include <linux/wakelock.h>
#include "sec_ts.h"

static struct device *sec_ts_dev;
EXPORT_SYMBOL(sec_ts_dev);

#if defined( SEC_TS_SUPPORT_STRINGLIB ) && !defined( CONFIG_SAMSUNG_PRODUCT_SHIP )
static ssize_t sec_ts_brane_read( struct file*, struct kobject*, struct bin_attribute*, char*, loff_t, size_t );
static ssize_t sec_ts_brane_write( struct file*, struct kobject*, struct bin_attribute*, char*, loff_t, size_t );

static struct semaphore sec_ts_brane_sema;
static struct bin_attribute sec_ts_brane_dev_attr =
{
	.attr =
	{
		.name = "brane",
		.mode = S_IRUGO,
	},
	.size = PAGE_SIZE << 1,
	.read = sec_ts_brane_read,
	.write = sec_ts_brane_write,
	//.mmap = 
};
static unsigned int sec_ts_brane_read_addr = 0;
#endif

#ifdef USE_OPEN_CLOSE
static int sec_ts_input_open(struct input_dev *dev);
static void sec_ts_input_close(struct input_dev *dev);
#endif
static void sec_ts_reset_work(struct work_struct *work);

static int sec_ts_stop_device(struct sec_ts_data *ts);
static int sec_ts_start_device(struct sec_ts_data *ts);

void sec_ts_release_all_finger(struct sec_ts_data *ts);

u8 lv1cmd;
u8* read_lv1_buff;
static int lv1_readsize;
static int lv1_readremain;
static int lv1_readoffset;

static ssize_t sec_ts_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);
static ssize_t sec_ts_regreadsize_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);
static inline ssize_t sec_ts_store_error(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t sec_ts_enter_recovery_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);
static ssize_t sec_ts_regread_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t sec_ts_gesture_status_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static inline ssize_t sec_ts_show_error(struct device *dev,
		struct device_attribute *attr, char *buf);

static DEVICE_ATTR(sec_ts_reg, (S_IWUSR | S_IWGRP), NULL, sec_ts_reg_store);
static DEVICE_ATTR(sec_ts_regreadsize, (S_IWUSR | S_IWGRP), NULL, sec_ts_regreadsize_store);
static DEVICE_ATTR(sec_ts_enter_recovery, (S_IWUSR | S_IWGRP), NULL, sec_ts_enter_recovery_store);
static DEVICE_ATTR(sec_ts_regread, S_IRUGO, sec_ts_regread_show, NULL);
static DEVICE_ATTR(sec_ts_gesture_status, S_IRUGO, sec_ts_gesture_status_show, NULL);

static struct attribute *cmd_attributes[] = {
	&dev_attr_sec_ts_reg.attr,
	&dev_attr_sec_ts_regreadsize.attr,
	&dev_attr_sec_ts_enter_recovery.attr,
	&dev_attr_sec_ts_regread.attr,
	&dev_attr_sec_ts_gesture_status.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

static inline ssize_t sec_ts_show_error(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	tsp_debug_err(true, &ts->client->dev, "sec_ts :%s read only function, %s\n", __func__, attr->attr.name );
	return -EPERM;
}

static inline ssize_t sec_ts_store_error(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	tsp_debug_err(true, &ts->client->dev, "sec_ts :%s write only function, %s\n", __func__, attr->attr.name );
	return -EPERM;
}

#ifdef CONFIG_SECURE_TOUCH
static irqreturn_t sec_ts_irq_thread(int irq, void *ptr);

/**
 * Sysfs attr group for secure touch & interrupt handler for Secure world.
 * @atomic : syncronization for secure_enabled
 * @pm_retime : set rpm_resume or rpm_ilde
 */

static void secure_touch_notify(struct sec_ts_data *ts)
{
	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);
	sysfs_notify(&ts->input_dev->dev.kobj, NULL, "secure_touch");
}

static irqreturn_t secure_filter_interrupt(struct sec_ts_data *ts)
{
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		if (atomic_cmpxchg(&ts->secure_pending_irqs, 0, 1) == 0) {
			tsp_debug_info(true, &ts->client->dev, "%s: pending irq:%d\n",
						__func__, (int)atomic_read(&ts->secure_pending_irqs));
			secure_touch_notify(ts);
		} else {
			tsp_debug_info(true, &ts->client->dev, "%s: --\n", __func__);
		}

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int secure_touch_clk_prepare_enable(struct sec_ts_data *ts)
{
	int ret;

	if (!ts->core_clk || !ts->iface_clk) {
		tsp_debug_err(true, &ts->client->dev, "%s: error clk\n", __func__);
		return -ENODEV;
	}

	ret = clk_prepare_enable(ts->core_clk);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: failed core clk\n", __func__);
		goto err_core_clk;
	}

	ret = clk_prepare_enable(ts->iface_clk);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: failed iface clk\n", __func__);
		goto err_iface_clk;
	}

	return 0;

err_iface_clk:
	clk_disable_unprepare(ts->core_clk);
err_core_clk:
	return -ENODEV;
}

static void secure_touch_clk_unprepare_disable(struct sec_ts_data *ts)
{
	if (!ts->core_clk || !ts->iface_clk) {
		tsp_debug_err(true, &ts->client->dev, "%s: error clk\n", __func__);
		return;
	}

	clk_disable_unprepare(ts->core_clk);
	clk_disable_unprepare(ts->iface_clk);
}

static ssize_t secure_touch_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->secure_enabled));
}

static ssize_t secure_touch_enable_store(struct device *dev,
			struct device_attribute *addr, const char *buf, size_t count)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int data, ret;

	ret = sscanf(buf, "%d", &data);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: failed to read:%d\n",
					__func__, ret);
		return -EINVAL;
	}

	if (data == 1) {
		/* Enable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
			tsp_debug_err(true, &ts->client->dev, "%s: already enabled\n", __func__);
			return -EBUSY;
		}

		sec_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_TOUCH);

		if (pm_runtime_get(ts->client->adapter->dev.parent) < 0) {
			tsp_debug_err(true, &ts->client->dev, "%s: failed to get pm_runtime\n", __func__);
			return -EIO;
		}

		if (secure_touch_clk_prepare_enable(ts) < 0) {
			pm_runtime_put(ts->client->adapter->dev.parent);
			tsp_debug_err(true, &ts->client->dev, "%s: failed to clk enable\n", __func__);
			return -ENXIO;
		}
		
		INIT_COMPLETION(ts->secure_powerdown);
		INIT_COMPLETION(ts->secure_interrupt);

		sec_ts_release_all_finger(ts);

		msleep(10);

		synchronize_irq(ts->client->irq);
		atomic_set(&ts->secure_enabled, 1);
		atomic_set(&ts->secure_pending_irqs, 0);

		tsp_debug_info(true, &ts->client->dev, "%s: secure touch enable\n", __func__);
	} else if (data == 0) {
		/* Disable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
			tsp_debug_err(true, &ts->client->dev, "%s: already disabled\n", __func__);
			return count;
		}

		secure_touch_clk_unprepare_disable(ts);
		pm_runtime_put(ts->client->adapter->dev.parent);
		atomic_set(&ts->secure_enabled, 0);
		secure_touch_notify(ts);
		msleep(10);

		sec_ts_release_all_finger(ts);

		sec_ts_irq_thread(ts->client->irq, ts);
		complete(&ts->secure_interrupt);
		complete(&ts->secure_powerdown);

		tsp_debug_info(true, &ts->client->dev, "%s: secure touch disable\n", __func__);

		ret = sec_ts_release_tmode(ts);
		if (ret < 0) {
			tsp_debug_err(true, &ts->client->dev, "%s: failed to release tmode\n",
					__func__);
			return -EIO;
		}

	} else {
		tsp_debug_err(true, &ts->client->dev, "%s: unsupport value:%d\n", __func__, data);
		return -EINVAL;
	}

	return count;
}

static ssize_t secure_touch_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
		tsp_debug_err(true, &ts->client->dev, "%s: disabled\n", __func__);
		goto secure_touch_out;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, -1, 0) == -1) {
		tsp_debug_err(true, &ts->client->dev, "%s: pending irq -1\n", __func__);
		goto secure_touch_out;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, 1, 0) != 1) {
		tsp_debug_err(true, &ts->client->dev, "%s: pending irq is not 1\n", __func__);
		goto secure_touch_out;
	}

	complete(&ts->secure_interrupt);

	return snprintf(buf, PAGE_SIZE, "1");

secure_touch_out:
	return snprintf(buf, PAGE_SIZE, "0");
}

static DEVICE_ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
				secure_touch_enable_show, secure_touch_enable_store);
static DEVICE_ATTR(secure_touch, S_IRUGO, secure_touch_show, NULL);

static struct attribute *secure_attr[] = {
	&dev_attr_secure_touch_enable.attr,
	&dev_attr_secure_touch.attr,
	NULL,
};

static struct attribute_group secure_attr_group = {
	.attrs = secure_attr,
};


static int secure_touch_init(struct sec_ts_data *ts)
{
	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);

	init_completion(&ts->secure_interrupt);
	init_completion(&ts->secure_powerdown);

	ts->core_clk = clk_get(&ts->client->dev, "core_clk");
	if (IS_ERR_OR_NULL(ts->core_clk)) {
		tsp_debug_err(true, &ts->client->dev, "%s: failed to get core_clk: %ld\n",
					__func__, PTR_ERR(ts->core_clk));
		goto err_core_clk;
	}

	ts->iface_clk = clk_get(&ts->client->dev, "iface_clk");
	if (IS_ERR_OR_NULL(ts->iface_clk)) {
		tsp_debug_err(true, &ts->client->dev, "%s: failed to get iface_clk: %ld\n",
					__func__, PTR_ERR(ts->iface_clk));
		goto err_iface_clk;
	}

	return 0;

err_iface_clk:
	clk_put(ts->core_clk);
err_core_clk:
	ts->core_clk = NULL;
	ts->iface_clk = NULL;

	return ENODEV;
}

static void secure_touch_stop(struct sec_ts_data *ts, bool stop)
{
	if (atomic_read(&ts->secure_enabled)) {
		atomic_set(&ts->secure_pending_irqs, -1);
		secure_touch_notify(ts);

		if (stop)
			wait_for_completion_interruptible(&ts->secure_powerdown);

		tsp_debug_info(true, &ts->client->dev, "%s: %d\n", __func__, stop);
	}
}
#endif

int sec_ts_i2c_write(struct sec_ts_data * ts, u8 reg, u8 * data, int len)
{
	u8 buf[I2C_WRITE_BUFFER_SIZE + 1];
	int ret;
	unsigned char retry;
	struct i2c_msg msg;

	if (len > I2C_WRITE_BUFFER_SIZE) {
		tsp_debug_err(true, &ts->client->dev, "sec_ts_i2c_write len is larger than buffer size\n");
		return -1;
	}

	if(ts->power_status == SEC_TS_STATE_POWER_OFF) {
		tsp_debug_err(true, &ts->client->dev, "%s: fail to enable glove status, POWER_STATUS=OFF\n", __func__);
		goto err;
	}

	buf[0] = reg;
	memcpy(buf + 1, data, len);

	msg.addr = ts->client->addr;
	msg.flags = 0;
	msg.len = len + 1;
	msg.buf = buf;
	mutex_lock(&ts->i2c_mutex);
	for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
		if ((ret = i2c_transfer(ts->client->adapter, &msg, 1)) == 1)
			break;

		tsp_debug_err(true, &ts->client->dev, "%s: I2C retry %d\n", __func__, retry + 1);
		usleep_range(1 * 1000, 1 * 1000);
	}

	mutex_unlock(&ts->i2c_mutex);

	if (retry == SEC_TS_I2C_RETRY_CNT) {
		tsp_debug_err(true, &ts->client->dev, "%s: I2C write over retry limit\n", __func__);
		ret = -EIO;
		schedule_delayed_work(&ts->reset_work,
					msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
	}

	if (ret == 1)
		return 0;
err:
	return -EIO;
}

int sec_ts_i2c_read(struct sec_ts_data * ts, u8 reg, u8 * data, int len)
{
	u8 buf[4];
	int ret;
	unsigned char retry;
	struct i2c_msg msg[2];
	int remain = len;

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		tsp_debug_err(true, &ts->client->dev, "%s: fail to enable glove status, POWER_STATUS=OFF\n", __func__);
		goto err;
	}

	buf[0] = reg;

	msg[0].addr = ts->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = buf;

	msg[1].addr = ts->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = data;

	mutex_lock(&ts->i2c_mutex);

	if (len <= ts->i2c_burstmax) {

		for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
			ret = i2c_transfer(ts->client->adapter, msg, 2);
			if (ret == 2)
				break;
			usleep_range(1 * 1000, 1 * 1000);
		}

	} else {
		/*
		 * I2C read buffer is 256 byte. do not support long buffer over than 256.
		 * So, try to seperate reading data about 256 bytes.
		 */

		for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
			ret = i2c_transfer(ts->client->adapter, msg, 1);
			if (ret == 1)
				break;
			usleep_range(1 * 1000, 1 * 1000);
		}

		do {
			if (remain > ts->i2c_burstmax)
				msg[1].len = ts->i2c_burstmax;
			else
				msg[1].len = remain;

			remain -= ts->i2c_burstmax;

			for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
				ret = i2c_transfer(ts->client->adapter, &msg[1], 1);
				if (ret == 1)
			break;
				usleep_range(1 * 1000, 1 * 1000);
			}

			msg[1].buf += msg[1].len;

		} while (remain > 0);

	}

	mutex_unlock(&ts->i2c_mutex);

	if (retry == SEC_TS_I2C_RETRY_CNT) {
		tsp_debug_err(true, &ts->client->dev, "%s: I2C read over retry limit\n", __func__);
		ret = -EIO;
		schedule_delayed_work(&ts->reset_work,
					msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
	}

	return ret;

err:
	return -EIO;
}

static int sec_ts_i2c_write_burst(struct sec_ts_data *ts, u8 *data, int len)
{
	int ret;
	int retry;

	mutex_lock(&ts->i2c_mutex);
	for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
		if ((ret = i2c_master_send(ts->client, data, len)) == len)
			break;

		usleep_range(1 * 1000, 1 * 1000);
	}

	mutex_unlock(&ts->i2c_mutex);
	if (retry == SEC_TS_I2C_RETRY_CNT) {
		tsp_debug_err(true, &ts->client->dev, "%s: I2C write over retry limit\n", __func__);
		ret = -EIO;
	}

	return ret;
}

static int sec_ts_i2c_read_bulk(struct sec_ts_data *ts, u8 *data, int len)
{
	int ret;
	unsigned char retry;
	int remain = len;
	struct i2c_msg msg;

	msg.addr = ts->client->addr;
	msg.flags = I2C_M_RD;
	msg.len = len;
	msg.buf = data;

	mutex_lock(&ts->i2c_mutex);

	do {
		if (remain > ts->i2c_burstmax)
			msg.len = ts->i2c_burstmax;
		else
			msg.len = remain;

		remain -= ts->i2c_burstmax;

		for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
			ret = i2c_transfer(ts->client->adapter, &msg, 1);
			if (ret == 1)
				break;
			usleep_range(1 * 1000, 1 * 1000);
		}

	if (retry == SEC_TS_I2C_RETRY_CNT) {
		tsp_debug_err(true, &ts->client->dev, "%s: I2C read over retry limit\n", __func__);
		ret = -EIO;

		break;
	}
		msg.buf += msg.len;

	} while (remain > 0);

	mutex_unlock(&ts->i2c_mutex);

	if (ret == 1)
		return 0;

	return -EIO;
}

#ifdef SEC_TS_SUPPORT_STRINGLIB
static int sec_ts_read_from_string(struct sec_ts_data *ts,
		unsigned short *reg, unsigned char *data, int length)
{

	return 0;
#if 0
	unsigned char string_reg[3];
	unsigned char *buf;
	int ret;

	string_reg[0] = 0xD0;
	string_reg[1] = (*reg >> 8) & 0xFF;
	string_reg[2] = *reg & 0xFF;

	buf = kzalloc(length + 1, GFP_KERNEL);
	if (buf == NULL) {
		tsp_debug_info(true, &info->client->dev,
				"%s: kzalloc error.\n", __func__);
		return -1;
	}

	ret = sec_ts_i2c_read(ts, string_reg, 3, buf, length + 1);
	if (ret >= 0)
		memcpy(data, &buf[1], length);

	kfree(buf);
	return ret;
#endif
}
/*
 * int sec_ts_write_to_string(struct sec_ts_data *, unsigned short *, unsigned char *, int)
 * send command or write specfic value to the string area.
 * string area means guest image or brane firmware.. etc..
 */
static int sec_ts_write_to_string(struct sec_ts_data *ts,
		unsigned short *reg, unsigned char *data, int length)
{

	return 0;
#if 0
	struct i2c_msg xfer_msg[3];
	unsigned char *regAdd;
	int ret;

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		tsp_debug_err(true, &ts->client->dev, "%s: Sensor stopped\n", __func__);
		return 0;
	}

	regAdd = kzalloc(length + 6, GFP_KERNEL);
	if (regAdd == NULL) {
		tsp_debug_info(true, &ts->client->dev,
				"%s: kzalloc error.\n", __func__);
		return -1;
	}

	mutex_lock(&ts->i2c_mutex);

	/* msg[0], length 3*/
	regAdd[0] = 0xb3;
	regAdd[1] = 0x20;
	regAdd[2] = 0x01;

	xfer_msg[0].addr = ts->client->addr;
	xfer_msg[0].len = 3;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = &regAdd[0];
	/* msg[0], length 3*/

	/* msg[1], length 4*/
	regAdd[3] = 0xb1;
	regAdd[4] = (*reg >> 8) & 0xFF;
	regAdd[5] = *reg & 0xFF;

	memcpy(&regAdd[6], data, length);

	/*regAdd[3] : B1 address, [4], [5] : String Address, [6]...: data */

	xfer_msg[1].addr = ts->client->addr;
	xfer_msg[1].len = 3 + length;
	xfer_msg[1].flags = 0;
	xfer_msg[1].buf = &regAdd[3];
	/* msg[1], length 4*/

	ret = i2c_transfer(ts->client->adapter, xfer_msg, 2);
	if (ret == 2) {
		tsp_debug_info(true, &ts->client->dev,
				"%s: string command is OK.\n", __func__);

		regAdd[0] = FTS_CMD_NOTIFY;
		regAdd[1] = *reg & 0xFF;
		regAdd[2] = (*reg >> 8) & 0xFF;

		xfer_msg[0].addr = ts->client->addr;
		xfer_msg[0].len = 3;
		xfer_msg[0].flags = 0;
		xfer_msg[0].buf = regAdd;

		ret = i2c_transfer(ts->client->adapter, xfer_msg, 1);
		if (ret != 1)
			tsp_debug_info(true, &ts->client->dev,
					"%s: string notify is failed.\n", __func__);
		else
			tsp_debug_info(true, &ts->client->dev,
					"%s: string notify is OK[%X].\n", __func__, *data);

	} else
		tsp_debug_info(true, &ts->client->dev,
				"%s: string command is failed. ret: %d\n", __func__, ret);

	mutex_unlock(&ts->i2c_mutex);
	kfree(regAdd);

	return ret;
#endif
}
#endif

#if defined(CONFIG_TOUCHSCREEN_DUMP_MODE)

struct delayed_work * p_ghost_check;
extern void sec_ts_run_rawdata_all(struct sec_ts_data *ts);

static void sec_ts_check_rawdata(struct work_struct *work)
{
	struct sec_ts_data *ts = container_of(work, struct sec_ts_data, ghost_check.work);

	if (ts->tsp_dump_lock == 1) {
		tsp_debug_err(true, &ts->client->dev, "%s, ignored ## already checking..\n", __func__);
		return;
	}
	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		tsp_debug_err(true, &ts->client->dev, "%s, ignored ## IC is power off\n", __func__);
		return;
	}

	ts->tsp_dump_lock = 1;
	tsp_debug_err(true, &ts->client->dev, "%s, start ##\n", __func__);
	sec_ts_run_rawdata_all((void *)ts);
	msleep(100);

	tsp_debug_err(true, &ts->client->dev, "%s, done ##\n", __func__);
	ts->tsp_dump_lock = 0;

}

void dump_tsp_log(void)
{
	printk(KERN_ERR "sec_ts %s: start \n", __func__);

#if defined(CONFIG_SAMSUNG_LPM_MODE)
	if (poweroff_charging) {
		printk(KERN_ERR "%s, ignored ## lpm charging Mode!!\n", __func__);
		return;
	}
#endif
	if (p_ghost_check == NULL) {
		printk(KERN_ERR "%s, ignored ## tsp probe fail!!\n", __func__);
		return;
	}
	schedule_delayed_work(p_ghost_check, msecs_to_jiffies(100));
}
#else
void dump_tsp_log(void)
{
	printk(KERN_ERR "FTS %s: not support\n", __func__);
}

#endif


#if defined( SEC_TS_SUPPORT_STRINGLIB ) && !defined( CONFIG_SAMSUNG_PRODUCT_SHIP )
static struct sec_ts_data* sec_ts_brane_get_info( struct kobject *obj )
{
	char *comm;
	struct device *dev;

	rcu_read_lock();
	comm = current->group_leader ? current->group_leader->comm : current->comm;
	if( (comm[0] != 'v') || (comm[1] != 'i') || (comm[2] != 'c') || (comm[3] != 'e') || (comm[4] != '.')
		|| (comm[5] != 'M') || (comm[6] != 'u') || (comm[7] != 'l') || (comm[8] != 't') || (comm[9] != 'i')
		|| (comm[10] != 'v') || (comm[11] != 'e') || (comm[12] != 'r') || (comm[13] != 's') || (comm[14] != 'e') || comm[15] )
	{
		rcu_read_unlock();
		return NULL;
	}
	rcu_read_unlock();
	dev = container_of( obj, struct device, kobj );
	if( !dev ){ return NULL; }

	return dev_get_drvdata( dev );
}

static ssize_t sec_ts_brane_read( struct file *fl, struct kobject *obj, struct bin_attribute *attr, char *buf, loff_t offset, size_t count )
{
	//unsigned char cmd[3];
	unsigned short addr;
	struct sec_ts_data *ts;

	if( offset || !count ){ return 0; }
	ts = sec_ts_brane_get_info( obj );
	if( !ts ){ return 0; }
	if(ts->power_status == SEC_TS_STATE_POWER_OFF)
	{
		tsp_debug_info( true, &ts->client->dev, "[brane] %s - 0x%X, %d, touch is stopped.\n", __func__, sec_ts_brane_read_addr, (int)count );
		return 0;
	}

	tsp_debug_info( true, &ts->client->dev, "[brane] %s - 0x%X, %d\n", __func__, sec_ts_brane_read_addr, (int)count );
	//cmd[0] = 0x92;
	//cmd[1] = (sec_ts_brane_read_addr >> 8) & 0xFF;
	//cmd[2] = sec_ts_brane_read_addr & 0xFF;
	//if( 0 > sec_ts_i2c_read( info, cmd, 3, buf, count ) )
	//	return 0;
	addr = (unsigned short)sec_ts_brane_read_addr;
	if( 0 > sec_ts_read_from_string( ts, &addr, buf, count ) )
		return 0;

	return count;
}

static ssize_t sec_ts_brane_write( struct file *fl, struct kobject *obj, struct bin_attribute *attr, char *buf, loff_t offset, size_t count )
{
	unsigned short addr;
	struct sec_ts_data *ts;

	if( offset || (count < 6) ){ return 0; }
	ts = sec_ts_brane_get_info( obj );
	if( !ts ){ return 0; }

	switch( buf[1] )
	{
	case 0:
		if(ts->power_status == SEC_TS_STATE_POWER_OFF)
		{
			tsp_debug_info( true, &ts->client->dev, "[brane] %s - 0x%X, %d, touch is stopped.\n", __func__, *(unsigned int*)&buf[2], (int)count - 6 );
			return 0;
		}
		tsp_debug_info( true, &ts->client->dev, "[brane] %s - 0x%X, %d\n", __func__, *(unsigned int*)&buf[2], (int)count - 6 );
		addr = (unsigned short)*(unsigned int*)&buf[2];
		if( 0 > sec_ts_write_to_string( ts, &addr, &buf[6], count - 6 ) )
			return 0;
		break;
	case 1:
		if(ts->power_status == SEC_TS_STATE_POWER_OFF)
			return 0;
		break;
	case 2:
		sema_init( &sec_ts_brane_sema, 0 );
		break;
	case 3:
		if( -EINTR == down_interruptible( &sec_ts_brane_sema ) )
			return 0;
		break;
	case 4:
		up( &sec_ts_brane_sema );
		break;
	case 5:
		sec_ts_brane_read_addr = *(unsigned int*)&buf[2];
		break;
	}

	return count;
}
#endif

void sec_ts_delay(unsigned int ms)
{
	if (ms < 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}

int sec_ts_wait_for_ready(struct sec_ts_data *ts, unsigned int ack)
{
	int rc = -1;
	int retry = 0;
	u8 tBuff[SEC_TS_EVENT_BUFF_SIZE];

	while (sec_ts_i2c_read(ts, SEC_TS_READ_ONE_EVENT, tBuff, SEC_TS_EVENT_BUFF_SIZE)) {
		if (tBuff[0] == TYPE_STATUS_EVENT_ACK) {
			if (tBuff[1] == ack) {
				rc = 0;
				break;
			}
		}

		if (retry++ > SEC_TS_WAIT_RETRY_CNT) {
			tsp_debug_err(true, &ts->client->dev, "%s: Time Over\n", __func__);
			break;
	}
		sec_ts_delay(20);
}

	tsp_debug_info(true, &ts->client->dev,
		"%s: %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X [%d]\n",
		__func__, tBuff[0], tBuff[1], tBuff[2], tBuff[3],
		tBuff[4], tBuff[5], tBuff[6], tBuff[7], retry);

	return rc;
}

int sec_ts_read_calibration_report(struct sec_ts_data *ts)
{
	int ret;
	u8 buf[5] = { 0 };

	buf[0] = SEC_TS_READ_CALIBRATION_REPORT;

	ret = sec_ts_i2c_read(ts, buf[0], &buf[1], 4);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: failed to read, %d\n", __func__, ret);
		return ret;
	}

	tsp_debug_info(true, &ts->client->dev, "%s: count:%d, pass count:%d, fail count:%d, status:0x%X\n",
				__func__, buf[1], buf[2], buf[3], buf[4]);

	return buf[4];
}

#define MAX_EVENT_COUNT 128
static void sec_sec_ts_read_event(struct sec_ts_data *ts)
{
	int ret;
	int is_event_remain = 0;
	int t_id;
	int event_id;
	int read_event_count = 0;
	bool print_log = false;
	u8 read_event_buff[SEC_TS_EVENT_BUFF_SIZE];
	struct sec_ts_event_coordinate *p_event_coord;
	struct sec_ts_coordinate *coordinate;

	/* in LPM, waiting blsp block resume */
	if (ts->power_status == SEC_TS_STATE_LPM_SUSPEND) {
		wake_lock_timeout(&ts->wakelock, msecs_to_jiffies(1 * MSEC_PER_SEC));
		/* waiting for blsp block resuming, if not occurs i2c error */
		ret = wait_for_completion_interruptible_timeout(&ts->resume_done, msecs_to_jiffies(1 * MSEC_PER_SEC));
		if (ret == 0) {
			tsp_debug_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled\n", __func__);
			return;
		}
		if (ret < 0) {
			tsp_debug_err(true, &ts->client->dev, "%s: LPM: -ERESTARTSYS if interrupted, %d\n", __func__, ret);
			return;
		}
		tsp_debug_err(true, &ts->client->dev, "%s: run LPM interrupt handler, %d\n", __func__, (int) ret);
		/* run lpm interrupt handler */
	}

	/* repeat READ_ONE_EVENT until buffer is empty(No event) */
	do {

		ret = sec_ts_i2c_read(ts, SEC_TS_READ_ONE_EVENT, read_event_buff, SEC_TS_EVENT_BUFF_SIZE);
		if (ret < 0) {
			tsp_debug_err(true, &ts->client->dev, "%s: i2c read one event failed\n", __func__);
			return ;
		}
#if 0
		tsp_debug_info(true, &ts->client->dev,
				"%s: ==== STATUS %x, %x(%X,%X), %x, %x, %x, %x, %x, %x ====\n", __func__,
				read_event_buff[0], read_event_buff[1], (read_event_buff[1] >> 4) & 0x0F,
				read_event_buff[1] & 0x0F, read_event_buff[2],
				read_event_buff[3], read_event_buff[4], read_event_buff[5],
				read_event_buff[6], read_event_buff[7]);
#endif

		if (read_event_count > MAX_EVENT_COUNT) {

			tsp_debug_err(true, &ts->client->dev, "%s: event buffer overflow\n", __func__);

			/* write clear event stack command when read_event_count > MAX_EVENT_COUNT */
			ret = sec_ts_i2c_write(ts, SEC_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
			if (ret < 0)
				tsp_debug_err(true, &ts->client->dev, "%s: i2c write clear event failed\n", __func__);

			return;
		}

		event_id = read_event_buff[0] >> 6;

		switch (event_id) {
		case SEC_TS_STATUS_EVENT:
			/* tchsta == 0 && ttype == 0 && eid == 0 : buffer empty */
			if (read_event_buff[0] != 0)
				tsp_debug_info(true, &ts->client->dev, "%s: STATUS %x %x %x %x %x %x %x %x (%d)\n", __func__,
					read_event_buff[0], read_event_buff[1], read_event_buff[2],
					read_event_buff[3], read_event_buff[4], read_event_buff[5],
						read_event_buff[6], read_event_buff[7], is_event_remain);

			/* watchdog reset -> send SENSEON command */
			if ((read_event_buff[0] == TYPE_STATUS_EVENT_ACK) &&
				(read_event_buff[1] == SEC_TS_ACK_BOOT_COMPLETE) &&
				(read_event_buff[2] == 0x20)) {
				 ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SENSE_ON, NULL, 0);
				 if (ret < 0)
					  tsp_debug_err(true, &ts->client->dev, "%s: fail to write Sense_on\n", __func__);
			}

			is_event_remain = 0;
			break;

		case SEC_TS_COORDINATE_EVENT:
			p_event_coord = (struct sec_ts_event_coordinate *)read_event_buff;

			t_id = (p_event_coord->tid - 1);

			if (t_id < MAX_SUPPORT_TOUCH_COUNT + MAX_SUPPORT_HOVER_COUNT) {
				print_log = true;
				coordinate = &ts->coord[t_id];
				coordinate->id = t_id;
				coordinate->action = p_event_coord->tchsta;
				coordinate->x = (p_event_coord->x_11_4 << 4) | (p_event_coord->x_3_0);
				coordinate->y = (p_event_coord->y_11_4 << 4) | (p_event_coord->y_3_0);
				coordinate->z = p_event_coord->z;
				coordinate->ttype = p_event_coord->ttype & MASK_3_BITS;
				coordinate->major = p_event_coord->major;
				coordinate->minor = p_event_coord->minor;
				coordinate->palm = (coordinate->ttype == SEC_TS_TOUCHTYPE_PALM);
#ifdef SEC_TS_SUPPORT_HOVERING
				if ((t_id == SEC_TS_EVENTID_HOVER) && coordinate->ttype == SEC_TS_TOUCHTYPE_PROXIMITY) {
					if (coordinate->action == SEC_TS_COORDINATE_ACTION_RELEASE) {
						input_mt_slot(ts->input_dev, 0);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
						tsp_debug_dbg(true, &ts->client->dev,
								"%s: Hover - Release - tid=%d, touch_count=%d\n",
								__func__, t_id, ts->touch_count);
					} else {
						input_mt_slot(ts->input_dev, 0);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);

						input_report_key(ts->input_dev, BTN_TOUCH, false);
						input_report_key(ts->input_dev, BTN_TOOL_FINGER, true);

						input_report_abs(ts->input_dev, ABS_MT_POSITION_X, coordinate->x);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, coordinate->y);

						if (coordinate->action == SEC_TS_COORDINATE_ACTION_PRESS)
							tsp_debug_dbg(true, &ts->client->dev,
									"%s: Hover - Press - tid=%d, touch_count=%d\n",
									__func__, t_id, ts->touch_count);
						else if (coordinate->action == SEC_TS_COORDINATE_ACTION_MOVE)
							tsp_debug_dbg(true, &ts->client->dev,
									"%s: Hover - Move - tid=%d, touch_count=%d\n",
									__func__, t_id, ts->touch_count);
					}
				}
#endif
				if ((coordinate->ttype == SEC_TS_TOUCHTYPE_NORMAL)
						|| (coordinate->ttype == SEC_TS_TOUCHTYPE_PALM)
						|| (coordinate->ttype == SEC_TS_TOUCHTYPE_GLOVE)) {

					if (coordinate->action == SEC_TS_COORDINATE_ACTION_RELEASE) {

						input_mt_slot(ts->input_dev, t_id);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);

						if (ts->touch_count > 0)
							ts->touch_count--;
						if (ts->touch_count == 0) {
							input_report_key(ts->input_dev, BTN_TOUCH, 0);
							input_report_key(ts->input_dev, BTN_TOOL_FINGER, 0);
						}

					} else if (coordinate->action == SEC_TS_COORDINATE_ACTION_PRESS) {
						ts->touch_count++;
						input_mt_slot(ts->input_dev, t_id);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
						input_report_key(ts->input_dev, BTN_TOUCH, 1);
						input_report_key(ts->input_dev, BTN_TOOL_FINGER, 1);

						input_report_abs(ts->input_dev, ABS_MT_POSITION_X, coordinate->x);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, coordinate->y);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, coordinate->major);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, coordinate->minor);
						input_report_abs(ts->input_dev, ABS_MT_PALM, coordinate->palm);
#ifdef CONFIG_SEC_FACTORY
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE, coordinate->z);
#endif
					} else if (coordinate->action == SEC_TS_COORDINATE_ACTION_MOVE) {
						if ((coordinate->ttype == SEC_TS_TOUCHTYPE_GLOVE) && !ts->touchkey_glove_mode_status) {
							ts->touchkey_glove_mode_status = true;
							input_report_switch(ts->input_dev, SW_GLOVE, 1);
						}

						input_mt_slot(ts->input_dev, t_id);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
						input_report_key(ts->input_dev, BTN_TOUCH, 1);
						input_report_key(ts->input_dev, BTN_TOOL_FINGER, 1);

						input_report_abs(ts->input_dev, ABS_MT_POSITION_X, coordinate->x);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, coordinate->y);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, coordinate->major);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, coordinate->minor);
						input_report_abs(ts->input_dev, ABS_MT_PALM, coordinate->palm);
#ifdef CONFIG_SEC_FACTORY
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE, coordinate->z);
#endif
						coordinate->lx = coordinate->x;
						coordinate->ly = coordinate->y;
					} else {
						tsp_debug_dbg(true, &ts->client->dev,
								"%s: do not support coordinate action(%d)\n", __func__, coordinate->action);
					}
				} else {
					tsp_debug_dbg(true, &ts->client->dev,
							"%s: do not support coordinate type(%d)\n", __func__, coordinate->ttype);
				}
			} else {
				tsp_debug_err(true, &ts->client->dev, "%s: tid(%d) is  out of range\n", __func__, t_id);
			}

			is_event_remain = 1;
			break;

		case SEC_TS_GESTURE_EVENT:
			if (read_event_buff[0] & 0x01) {
				struct sec_ts_gesture_status *p_gesture_status = (struct sec_ts_gesture_status *)read_event_buff;

				/* gesture->status & 0x1 : wake up event */
				tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__,
							p_gesture_status->scode == SEC_TS_GESTURE_CODE_AOD ? "AOD" :
							p_gesture_status->scode == SEC_TS_GESTURE_CODE_SPAY ? "SPAY" : "OTHER");

				/* will be fixed after merge String Liabrary : SPAY or Double Tab */
				if (p_gesture_status->scode == SEC_TS_GESTURE_CODE_AOD)
					ts->scrub_id = 0x08;//-> temp value : Double Tap event value is 8
				else if (p_gesture_status->scode == SEC_TS_GESTURE_CODE_SPAY)
					ts->scrub_id = 0x04;//-> temp value : SPAY event value is 4

				input_report_key(ts->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(ts->input_dev);
				input_report_key(ts->input_dev, KEY_BLACK_UI_GESTURE, 0);
				
				wake_lock_timeout(&ts->wakelock, msecs_to_jiffies(3 * MSEC_PER_SEC));

			} else {

				tsp_debug_info(true, &ts->client->dev, "%s: GESTURE  %x %x %x %x %x %x %x %x\n", __func__,
					read_event_buff[0], read_event_buff[1], read_event_buff[2],
						read_event_buff[3], read_event_buff[4], read_event_buff[5],
						read_event_buff[6], read_event_buff[7]);
			}

			is_event_remain = 1;
			break;

		default:
			tsp_debug_err(true, &ts->client->dev, "%s: unknown event  %x %x %x %x %x %x\n", __func__,
					read_event_buff[0], read_event_buff[1], read_event_buff[2],
					read_event_buff[3], read_event_buff[4], read_event_buff[5]);

			is_event_remain = 1;
			break;

		}

		if(print_log){
			if (coordinate->action == SEC_TS_COORDINATE_ACTION_PRESS) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
					tsp_debug_info(true, &ts->client->dev,
					"%s: [P] tID:%d, x:%d, y:%d, z:%d, major:%d, minor:%d, tc:%d, type:%X\n",
					__func__, t_id, coordinate->x, coordinate->y, coordinate->z,
					coordinate->major, coordinate->minor, ts->touch_count,
					coordinate->ttype);
#else
					tsp_debug_info(true, &ts->client->dev,
					"%s: [P] tID:%d, tc:%d, type:%X\n",
					__func__, t_id, ts->touch_count, coordinate->ttype);
#endif
				coordinate->action = SEC_TS_COORDINATE_ACTION_MOVE;

			} else if (coordinate->action == SEC_TS_COORDINATE_ACTION_RELEASE) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
					tsp_debug_info(true, &ts->client->dev,
					"%s: [R] tID:%d mc: %d tc:%d lx:%d ly:%d, v:%02X%02X, cal:%X\n",
						__func__, t_id, coordinate->mcount, ts->touch_count,
					coordinate->lx, coordinate->ly,
						ts->plat_data->img_version_of_ic[2],
					ts->plat_data->img_version_of_ic[3],
					ts->cal_status);
#else
				tsp_debug_info(true, &ts->client->dev,
					"%s: [R] tID:%d mc: %d tc:%d, v:%02X%02X, cal:%X\n",
						__func__, t_id, coordinate->mcount, ts->touch_count,
						ts->plat_data->img_version_of_ic[2],
					ts->plat_data->img_version_of_ic[3],
					ts->cal_status);
#endif
				coordinate->action = SEC_TS_COORDINATE_ACTION_NONE;
				coordinate->mcount = 0;
			} else if (coordinate->action == SEC_TS_COORDINATE_ACTION_MOVE) {
				coordinate->mcount++;
			}/* else {
				tsp_debug_info(true, &ts->client->dev,
						"%s: undefined status: %X\n",
						__func__, coordinate->action);
			}*/

			print_log = false;
		}
	} while (is_event_remain);

	input_sync(ts->input_dev);
	
}

static irqreturn_t sec_ts_irq_thread(int irq, void *ptr)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)ptr;

#ifdef CONFIG_SECURE_TOUCH
	if (secure_filter_interrupt(ts) == IRQ_HANDLED) {
		wait_for_completion_interruptible_timeout(&ts->secure_interrupt,
					msecs_to_jiffies(5 * MSEC_PER_SEC));

		tsp_debug_info(true, &ts->client->dev,
					"%s: secure interrupt handled\n", __func__);

		return IRQ_HANDLED;
	}
#endif

	sec_sec_ts_read_event(ts);

	return IRQ_HANDLED;
}

int get_tsp_status(void)
{
	return 0;
}
EXPORT_SYMBOL(get_tsp_status);

void sec_ts_set_charger(bool enable)
{
	return;
/*
	int ret;
	u8 noise_mode_on[] = {0x01};
	u8 noise_mode_off[] = {0x00};

	if (enable) {
		tsp_debug_info(true, &ts->client->dev, "sec_ts_set_charger : charger CONNECTED!!\n");
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_NOISE_MODE, noise_mode_on, sizeof(noise_mode_on));
		if (ret < 0)
			tsp_debug_err(true, &ts->client->dev, "sec_ts_set_charger: fail to write NOISE_ON\n");
	} else {
		tsp_debug_info(true, &ts->client->dev, "sec_ts_set_charger : charger DISCONNECTED!!\n");
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_NOISE_MODE, noise_mode_off, sizeof(noise_mode_off));
		if (ret < 0)
			tsp_debug_err(true, &ts->client->dev, "sec_ts_set_charger: fail to write NOISE_OFF\n");
	}
 */
}
EXPORT_SYMBOL(sec_ts_set_charger);

int sec_ts_glove_mode_enables(struct sec_ts_data *ts, int mode)
{
	int ret;

	if (mode) {
		ts->touch_functions = (ts->touch_functions | SEC_TS_BIT_SETFUNC_GLOVE | SEC_TS_BIT_SETFUNC_MUTUAL);
	} else {
		ts->touch_functions = ((ts->touch_functions & (~SEC_TS_BIT_SETFUNC_GLOVE)) | SEC_TS_BIT_SETFUNC_MUTUAL);
	}

	if (ts->power_status == SEC_TS_STATE_POWER_OFF){
		tsp_debug_err(true, &ts->client->dev, "%s: pwr off, glove:%d, status:%x\n", __func__,
					mode, ts->touch_functions);
		goto glove_enable_err;
	}

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, &(ts->touch_functions), 1);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: Failed to send command", __func__);
		goto glove_enable_err;
	}

	tsp_debug_info(true, &ts->client->dev, "%s: glove:%d, status:%x\n", __func__,
		mode ,ts->touch_functions);

	return 0;

glove_enable_err:
	return -EIO;
}
EXPORT_SYMBOL(sec_ts_glove_mode_enables);

#ifdef SEC_TS_SUPPORT_HOVERING
int sec_ts_hover_enables(struct sec_ts_data *ts, int enables)
{
	int ret;

	if (enables) {
		ts->touch_functions = (ts->touch_functions | SEC_TS_BIT_SETFUNC_HOVER | SEC_TS_BIT_SETFUNC_MUTUAL);
	} else {
		ts->touch_functions = ((ts->touch_functions & (~SEC_TS_BIT_SETFUNC_HOVER)) | SEC_TS_BIT_SETFUNC_MUTUAL);
	}

	if (ts->power_status == SEC_TS_STATE_POWER_OFF){
		tsp_debug_err(true, &ts->client->dev, "%s: pwr off, hover:%d, status:%x\n", __func__,
					enables, ts->touch_functions);
		goto hover_enable_err;
	}

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, &(ts->touch_functions), 1);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: Failed to send command", __func__);
		goto hover_enable_err;
	}

	tsp_debug_info(true, &ts->client->dev, "%s: hover:%d, status:%x\n", __func__,
		enables ,ts->touch_functions);

	return 0;

hover_enable_err:
	return -EIO;
}
EXPORT_SYMBOL(sec_ts_hover_enables);
#endif

int sec_ts_set_cover_type(struct sec_ts_data *ts, bool enable)
{
	int ret;

	tsp_debug_info(true, &ts->client->dev, "%s, en:%d, %d\n", __func__, enable, ts->cover_type);


	switch (ts->cover_type) {

	case SEC_TS_FLIP_WALLET:
	case SEC_TS_LED_COVER:
	case SEC_TS_MONTBLANC_COVER:
	case SEC_TS_CHARGER_COVER:
	case SEC_TS_COVER_NOTHING1:
	case SEC_TS_COVER_NOTHING2:
		ts->cover_cmd = 0;
		break;

	case SEC_TS_VIEW_COVER:
	case SEC_TS_VIEW_WIRELESS:
	case SEC_TS_VIEW_WALLET:		
	case SEC_TS_CLEAR_FLIP_COVER :
	case SEC_TS_QWERTY_KEYBOARD_EUR :
	case SEC_TS_QWERTY_KEYBOARD_KOR :
		ts->cover_cmd = (u8)ts->cover_type;
		break;
	default:
		ts->cover_cmd = 0;
		tsp_debug_err(true, &ts->client->dev, "%s: not chage touch state, %d\n",
				__func__, ts->cover_type);
		break;
	}

	if (enable) {
		ts->touch_functions = (ts->touch_functions | SEC_TS_BIT_SETFUNC_COVER | SEC_TS_BIT_SETFUNC_MUTUAL);
	} else {
		ts->touch_functions = ((ts->touch_functions & (~SEC_TS_BIT_SETFUNC_COVER)) | SEC_TS_BIT_SETFUNC_MUTUAL);
	}

	if (ts->power_status == SEC_TS_STATE_POWER_OFF){
		tsp_debug_err(true, &ts->client->dev, "%s: pwr off, close:%d, status:%x\n", __func__,
					enable, ts->touch_functions);
		goto cover_enable_err;
	}else if(enable && (ts->cover_cmd == 0)) {	// SEC_TS_FLIP_WALLET not set.
		tsp_debug_err(true, &ts->client->dev, "%s: don't need set, type:%d, cmd:%d, close:%d, status:%x\n", __func__,
					ts->cover_type, ts->cover_cmd, enable, ts->touch_functions);
		goto cover_enable_err;
	}

	if (enable) {
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_COVERTYPE, &ts->cover_cmd, 1);
		if (ret < 0) {
			tsp_debug_err(true, &ts->client->dev, "%s: Failed to send covertype command: %d", __func__, ts->cover_cmd);
			goto cover_enable_err;
		}
	}

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, &(ts->touch_functions), 1);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: Failed to send command", __func__);
		goto cover_enable_err;
	}

	tsp_debug_info(true, &ts->client->dev, "%s: en:%d, status:%x\n", __func__,
		enable ,ts->touch_functions);

	return 0;

cover_enable_err:
	return -EIO;


}
EXPORT_SYMBOL(sec_ts_set_cover_type);

/* for debugging--------------------------------------------------------------------------------------*/
static ssize_t sec_ts_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
        struct sec_ts_data *ts = dev_get_drvdata(dev);

        if(ts->power_status == SEC_TS_STATE_POWER_OFF){
                tsp_debug_info(true, &ts->client->dev, "%s: Power off state\n", __func__);
                return -EIO;
        }

        if (size > 0)
		sec_ts_i2c_write_burst(ts, (u8 *)buf, size);

        tsp_debug_info(true, &ts->client->dev, "sec_ts_reg: 0x%x, 0x%x, size %d\n", buf[0], buf[1], (int)size);
        return size;
}

static ssize_t sec_ts_regread_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	int length;
	int remain;
	int offset;

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		tsp_debug_info(true, &ts->client->dev, "%s: Power off state\n", __func__);
		return -EIO;
	}

	disable_irq(ts->client->irq);

	read_lv1_buff = (u8 *)kzalloc(sizeof(u8)*lv1_readsize, GFP_KERNEL);
		if (!read_lv1_buff) {
			tsp_debug_err(true, &ts->client->dev, "%s kzalloc failed\n", __func__);
		goto malloc_err;
		}

		mutex_lock(&ts->device_mutex);
	remain = lv1_readsize;
	offset = 0;
	do
	{
		if(remain >= ts->i2c_burstmax)
			length = ts->i2c_burstmax;
		else
			length = remain;

		if( offset == 0 )
			ret = sec_ts_i2c_read(ts, lv1cmd, &read_lv1_buff[offset], length);
		else
			ret = sec_ts_i2c_read_bulk(ts, &read_lv1_buff[offset], length);

	if (ret < 0) {
			tsp_debug_err(true, &ts->client->dev, "%s: i2c read %x command, remain =%d\n", __func__, lv1cmd, remain);
			goto i2c_err;
	}

		remain -= length;
		offset += length;
	} while(remain > 0);

	tsp_debug_info(true, &ts->client->dev, "%s: lv1_readsize = %d\n", __func__, lv1_readsize);
	memcpy(buf, read_lv1_buff + lv1_readoffset, lv1_readsize);

i2c_err:
	kfree(read_lv1_buff);
malloc_err:
	mutex_unlock(&ts->device_mutex);
	lv1_readremain = 0;
	enable_irq(ts->client->irq);

	return lv1_readsize;
}

static ssize_t sec_ts_gesture_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	mutex_lock(&ts->device_mutex);
	memcpy(buf, ts->gesture_status, sizeof(ts->gesture_status));
	tsp_debug_info(true, &ts->client->dev,
				"sec_sec_ts_gesture_status_show GESTURE STATUS %x %x %x %x %x %x\n",
				ts->gesture_status[0], ts->gesture_status[1], ts->gesture_status[2],
				ts->gesture_status[3], ts->gesture_status[4], ts->gesture_status[5]);
	mutex_unlock(&ts->device_mutex);

	return sizeof(ts->gesture_status);
}

static ssize_t sec_ts_regreadsize_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	lv1cmd = buf[0];
	lv1_readsize = ((unsigned int)buf[4] << 24) |
		((unsigned int)buf[3]<<16) | ((unsigned int) buf[2]<<8) | ((unsigned int)buf[1]<<0);
	lv1_readoffset = 0;
	lv1_readremain = 0;
	return size;
}

static ssize_t sec_ts_enter_recovery_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	struct sec_ts_plat_data *pdata = dev->platform_data;
	int ret;
	u8 on = (u8)buf[0];

	if (on == 1) {
		disable_irq(ts->client->irq);
		gpio_free(pdata->gpio);

		tsp_debug_info(true, &ts->client->dev, "%s: gpio free\n", __func__);
		if (gpio_is_valid(pdata->gpio)) {
			ret = gpio_request_one(pdata->gpio, GPIOF_OUT_INIT_LOW, "sec,tsp_int");
			tsp_debug_info(true, &ts->client->dev, "%s: gpio request one\n", __func__);
			if (ret) {
				tsp_debug_err(true, &ts->client->dev, "Unable to request tsp_int [%d]\n", pdata->gpio);
				return -EINVAL;
			}
		} else {
			tsp_debug_err(true, &ts->client->dev, "Failed to get irq gpio\n");
			return -EINVAL;
		}

		pdata->power(ts, false);
		sec_ts_delay(100);
		pdata->power(ts, true);
	} else {
		gpio_free(pdata->gpio);

		if (gpio_is_valid(pdata->gpio)) {
			ret = gpio_request_one(pdata->gpio, GPIOF_DIR_IN, "sec,tsp_int");
			if (ret) {
				tsp_debug_err(true, &ts->client->dev, "Unable to request tsp_int [%d]\n", pdata->gpio);
				return -EINVAL;
			}
		} else {
			tsp_debug_err(true, &ts->client->dev, "Failed to get irq gpio\n");
			return -EINVAL;
		}

		pdata->power(ts, false);
		sec_ts_delay(500);
		pdata->power(ts, true);
		sec_ts_delay(500);

		/* AFE Calibration */
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_CALIBRATION_AMBIENT, NULL, 0);
		if (ret < 0)
			tsp_debug_err(true, &ts->client->dev, "%s: fail to write AFE_CAL\n", __func__);

		sec_ts_delay(1000);
		enable_irq(ts->client->irq);
	}

	return size;
}

#ifdef SEC_TS_SUPPORT_TA_MODE
static void sec_ts_charger_config(struct sec_ts_data *ts, int status)
{
	int ret;

	if (status == 0x01 || status == 0x03){
		ts->touch_functions = (ts->touch_functions | SEC_TS_BIT_SETFUNC_CHARGER | SEC_TS_BIT_SETFUNC_MUTUAL);
	} else {
		ts->touch_functions = ((ts->touch_functions & (~SEC_TS_BIT_SETFUNC_CHARGER)) | SEC_TS_BIT_SETFUNC_MUTUAL);
	}

	if (ts->power_status == SEC_TS_STATE_POWER_OFF){
		tsp_debug_err(true, &ts->client->dev, "%s: pwr off, chg:%d, status:%x\n", __func__,
					enable, ts->touch_functions);
		goto charger_enable_err;
	}

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, &(ts->touch_functions), 1);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: Failed to send command", __func__);
		goto charger_enable_err;
	}

	tsp_debug_info(true, &ts->client->dev, "%s: chg:%d, status:%x\n", __func__,
		enable ,ts->touch_functions);

	return 0;

charger_enable_err:
	return -EIO;

}

static void sec_ts_ta_cb(struct sec_ts_callbacks *cb, int status)
{
	struct sec_ts_data *ts =
		container_of(cb, struct sec_ts_data, callbacks);

	tsp_debug_err(true, &ts->client->dev, "[TSP] %s: status : %x\n", __func__, status);

	ts->ta_status = status;
	/* if do not completed driver loading, ta_cb will not run. */
/*
	if (!rmi4_data->init_done.done) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: until driver loading done.\n", __func__);
		return;
	}

	if (rmi4_data->touch_stopped || rmi4_data->doing_reflash) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: device is in suspend state or reflash.\n",
				__func__);
		return;
	}
*/

	sec_ts_charger_config(ts, status);
}
#endif

static int sec_ts_raw_device_init(struct sec_ts_data *ts)
{
	int ret;

	sec_ts_dev = device_create(sec_class, NULL, 0, ts, "sec_ts");

	ret = IS_ERR(sec_ts_dev);
	if (ret) {
		tsp_debug_err(true, &ts->client->dev, "%s: fail - device_create\n", __func__);
		return ret;
	}

	ret = sysfs_create_group(&sec_ts_dev->kobj, &cmd_attr_group);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: fail - sysfs_create_group\n", __func__);
		goto err_sysfs;
	}
#if defined( SEC_TS_SUPPORT_STRINGLIB ) && !defined( CONFIG_SAMSUNG_PRODUCT_SHIP )
	sema_init( &sec_ts_brane_sema, 0 );
	if( 0 > sysfs_create_bin_file( &sec_ts_dev->kobj, &sec_ts_brane_dev_attr ) )
		tsp_debug_err( true, &ts->client->dev, "[brane] FTS Failed to create brane\n" );
#endif
/*
	ret = sysfs_create_link(&sec_ts_dev->kobj,
			&ts->input_dev->dev.kobj, "input");
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "%s: fail - sysfs_create_link\n", __func__);
		goto err_sysfs;
	}
*/
	return ret;
err_sysfs:
	tsp_debug_err(true, &ts->client->dev, "%s: fail\n", __func__);
	return ret;
}

/* for debugging--------------------------------------------------------------------------------------*/

static int sec_ts_pinctrl_configure(struct sec_ts_data *ts, bool enable)
{
	struct pinctrl_state *state;

	tsp_debug_info(true, &ts->client->dev, "%s: %s\n", __func__, enable ? "ACTIVE" : "SUSPEND");

	if (enable) {
		state = pinctrl_lookup_state(ts->plat_data->pinctrl, "tsp_active");
		if (IS_ERR(ts->plat_data->pins_default))
			tsp_debug_err(true, &ts->client->dev, "could not get active pinstate\n");
	} else {
		state = pinctrl_lookup_state(ts->plat_data->pinctrl, "tsp_suspend");
		if (IS_ERR(ts->plat_data->pins_sleep))
			tsp_debug_err(true, &ts->client->dev, "could not get suspend pinstate\n");
	}

	if (!IS_ERR_OR_NULL(state))
		return pinctrl_select_state(ts->plat_data->pinctrl, state);

	return 0;

}

static int sec_ts_power_ctrl(struct sec_ts_data *ts, bool enable)
{	// NA7
	struct device *dev = &ts->client->dev;

	dev_err(dev, "%s: %d\n", __func__, enable);

	if (enable) {
		if(ts->plat_data->vdd_en_gpio > 0){
			gpio_direction_output(ts->plat_data->vdd_en_gpio, enable);
			dev_err(dev, "%s: vdd_en_gpio is enabled[OK]\n", __func__);
		}else{
			dev_err(dev, "%s: vdd_en_gpio is fail \n", __func__);
		}
	} else {
		if(ts->plat_data->vdd_en_gpio > 0){
			gpio_direction_output(ts->plat_data->vdd_en_gpio, enable);
			dev_err(dev, "%s: vdd_en_gpio is enabled[OK]\n", __func__);
		}else{
			dev_err(dev, "%s: vdd_en_gpio is fail \n", __func__);
		}
	}

	return 0;
}

static int sec_ts_parse_dt(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct sec_ts_plat_data *pdata = dev->platform_data;
	struct device_node *np = dev->of_node;
	u32 coords[2], lines[2];
	u32 temp;
	int ret = 0;

	pdata->gpio = of_get_named_gpio(np, "sec,irq_gpio", 0);
	if (gpio_is_valid(pdata->gpio)) {
		ret = gpio_request_one(pdata->gpio, GPIOF_DIR_IN, "sec,tsp_int");
		if (ret) {
			tsp_debug_err(true, &client->dev, "Unable to request tsp_int [%d]\n", pdata->gpio);
			return -EINVAL;
		}
	} else {
		tsp_debug_err(true, &client->dev, "Failed to get irq gpio\n");
		return -EINVAL;
	}
	client->irq = gpio_to_irq(pdata->gpio);

	if (of_property_read_u32(np, "sec,i2c-burstmax", &pdata->i2c_burstmax)) {
		tsp_debug_err(true, &client->dev, "Failed to get irq_type property\n");
		return -EINVAL;
	}

	if (of_property_read_u32_array(np, "sec,max_coords", coords, 2)) {
		tsp_debug_err(true, &client->dev, "Failed to get max_coords property\n");
		return -EINVAL;
	}
	pdata->max_x = coords[0] - 1;
	pdata->max_y = coords[1] - 1;

	if (of_property_read_u32_array(np, "sec,num_lines", lines, 2)) {
		tsp_debug_info(true, &client->dev, "skipped to get num_lines property\n");
	} else {
		pdata->num_rx = lines[0];
		pdata->num_tx = lines[1];
		tsp_debug_info(true, &client->dev, "num_of[rx,tx]: [%d,%d]\n",
				pdata->num_rx, pdata->num_tx);
	}

	// NA7
	pdata->vdd_en_gpio = of_get_named_gpio(np, "sec,vdd_en", 0);
	if(pdata->vdd_en_gpio <= 0){
		tsp_debug_err(true, &client->dev, "Failed to get vdd_en_gpio name property\n");
		pdata->vdd_en_gpio = 0;
	}else{
		if (gpio_is_valid(pdata->vdd_en_gpio))
			gpio_request(pdata->vdd_en_gpio, "sec_vdd_en_gpio");
	}

	of_property_read_string(np, "sec,firmware_name", &pdata->firmware_name);
	of_property_read_string(np, "sec,parameter_name", &pdata->parameter_name);

	if (of_property_read_string_index(np, "sec,project_name", 0, &pdata->project_name))
		tsp_debug_info(true, &client->dev, "skipped to get project_name property\n");
	if (of_property_read_string_index(np, "sec,project_name", 1, &pdata->model_name))
		tsp_debug_info(true, &client->dev, "skipped to get model_name property\n");
/* QCOM */
	if (of_property_read_string(np, "sec,tsp-project", &pdata->project_name))
		tsp_debug_info(true, &client->dev, "skipped to get project_name property\n");
	if (of_property_read_string(np, "sec,tsp-model", &pdata->model_name))
		tsp_debug_info(true, &client->dev, "skipped to get model_name property\n");

	temp = get_lcd_attached("GET");
	pdata->panel_revision = (temp >> 8) & 0xFF;
	pdata->power = sec_ts_power_ctrl;

	if (of_property_read_u32(np, "sec,bringup", &pdata->bringup) < 0)
		pdata->bringup = 0;

	tsp_debug_err(true, &client->dev, "%s: i2c buffer limit: %d, lcd_id:%06X, bringup:%d, FW:%s\n",
			__func__, pdata->i2c_burstmax, temp, pdata->bringup, pdata->firmware_name);

	return ret;
}

static int sec_ts_setup_drv_data(struct i2c_client *client)
{
	int ret = 0;
	struct sec_ts_data *ts;
	struct sec_ts_plat_data *pdata;

	/* parse dt */
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct sec_ts_plat_data), GFP_KERNEL);

		if (!pdata) {
			tsp_debug_err(true, &client->dev, "Failed to allocate platform data\n");
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;
		ret = sec_ts_parse_dt(client);
		if (ret) {
			tsp_debug_err(true, &client->dev, "Failed to parse dt\n");
			return ret;
		}
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata) {
		tsp_debug_err(true, &client->dev, "No platform data found\n");
		return -EINVAL;
	}
	if (!pdata->power) {
		tsp_debug_err(true, &client->dev, "No power contorl found\n");
		return -EINVAL;
	}

	pdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pdata->pinctrl)) {
		tsp_debug_err(true, &client->dev, "could not get pinctrl\n");
		return PTR_ERR(pdata->pinctrl);
	}

	ts = kzalloc(sizeof(struct sec_ts_data), GFP_KERNEL);
	if (!ts) {
		tsp_debug_err(true, &client->dev, "%s: Failed to alloc mem for info\n", __func__);
		return -ENOMEM;
	}

	ts->client = client;
	ts->plat_data = pdata;
	ts->crc_addr = 0x0001FE00;
	ts->fw_addr = 0x00002000;
	ts->para_addr = 0x18000;
	ts->sec_ts_i2c_read = sec_ts_i2c_read;
	ts->sec_ts_i2c_write = sec_ts_i2c_write;
	ts->sec_ts_i2c_write_burst = sec_ts_i2c_write_burst;
	ts->sec_ts_i2c_read_bulk = sec_ts_i2c_read_bulk;
	ts->i2c_burstmax = pdata->i2c_burstmax;

	INIT_DELAYED_WORK(&ts->reset_work, sec_ts_reset_work);

	i2c_set_clientdata(client, ts);

	return ret;
}

static int sec_ts_read_information(struct sec_ts_data *ts)
{
	unsigned char data[13] = { 0 };
	int ret;

	memset(data, 0x0, 3);
	ret = sec_ts_i2c_read(ts, SEC_TS_READ_DEVICE_ID, data, 3);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev,
					"%s: failed to read device id(%d)\n",
					__func__, ret);
		return ret;
	}

	tsp_debug_info(true, &ts->client->dev,
				"%s: %X, %X, %X\n",
				__func__, data[0], data[1], data[2]);

	memset(data, 0x0, 9);
	ret = sec_ts_i2c_read(ts, SEC_TS_READ_SUB_ID, data, 13);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev,
					"%s: failed to read sub id(%d)\n",
					__func__, ret);
		return ret;
	}

	tsp_debug_info(true, &ts->client->dev,
				"%s: AP/BL(%X), DEV1:%X, DEV2:%X, nTX:%X, nRX:%X, rX:%d, rY:%d\n",
				__func__, data[0], data[1], data[2], data[3], data[4],
				(data[5] << 8) | data[6], (data[7] << 8) | data[8]);

	/* Set X,Y Resolution from IC information. */
	if (((data[5] << 8) | data[6]) > 0)
		ts->plat_data->max_x = ((data[5] << 8) | data[6]) - 1;	// NA7

	if (((data[7] << 8) | data[8]) > 0)
		ts->plat_data->max_y = ((data[7] << 8) | data[8]) - 1;	// NA7

	ts->tx_count = data[3];
	ts->rx_count = data[4];

	memset(data, 0x0, 1);
	ret = sec_ts_i2c_read(ts, SEC_TS_READ_BOOT_STATUS, data, 1);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev,
					"%s: failed to read sub id(%d)\n",
					__func__, ret);
		return ret;
	}

	tsp_debug_info(true, &ts->client->dev,
				"%s: STATUS : %X\n",
				__func__, data[0]);

	return ret;
}

#define T_BUFF_SIZE 4
static int sec_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct sec_ts_data *ts;
	static char sec_ts_phys[64] = { 0 };
	int ret = 0;

	tsp_debug_info(true, &client->dev, "%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		tsp_debug_err(true, &client->dev, "%s : EIO err!\n", __func__);
		return -EIO;
	}

	if (get_lcd_attached("GET") == 0) {
		dev_err(&client->dev,
			"%s : LCD is not attached\n", __func__);
		return -EIO;
	}

	ret = sec_ts_setup_drv_data(client);
	if (ret < 0) {
		tsp_debug_err(true, &client->dev, "%s: Failed to set up driver data\n", __func__);
		goto err_setup_drv_data;
	}

	ts = (struct sec_ts_data *)i2c_get_clientdata(client);
	if (!ts) {
		tsp_debug_err(true, &client->dev, "%s: Failed to get driver data\n", __func__);
		ret = -ENODEV;
		goto err_get_drv_data;
	}

	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		tsp_debug_err(true, &ts->client->dev, "%s: allocate device err!\n", __func__);
		ret = -ENOMEM;
		goto err_allocate_device;
	}

	ts->input_dev->name = "sec_touchscreen";
	snprintf(sec_ts_phys, sizeof(sec_ts_phys), "%s/input1",
			ts->input_dev->name);
	ts->input_dev->phys = sec_ts_phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->dev.parent = &client->dev;
	ts->touch_count = 0;
	ts->sec_ts_i2c_write = sec_ts_i2c_write;
	ts->sec_ts_i2c_read = sec_ts_i2c_read;
#ifdef SEC_TS_SUPPORT_STRINGLIB
	ts->sec_ts_read_from_string = sec_ts_read_from_string;
	ts->sec_ts_write_to_string = sec_ts_write_to_string;
#endif
#ifdef USE_OPEN_CLOSE
	ts->input_dev->open = sec_ts_input_open;
	ts->input_dev->close = sec_ts_input_close;
#endif

	mutex_init(&ts->lock);
	mutex_init(&ts->device_mutex);
	mutex_init(&ts->i2c_mutex);

	wake_lock_init(&ts->wakelock, WAKE_LOCK_SUSPEND, "tsp_wakelock");
	init_completion(&ts->resume_done);

	tsp_debug_err(true, &client->dev, "%s init resource\n", __func__);

	sec_ts_pinctrl_configure(ts, true);

	/* Enable Power */
	ts->plat_data->power(ts, true);
	ts->power_status = SEC_TS_STATE_POWER_ON;
	sec_ts_delay(70);
	sec_ts_wait_for_ready(ts, SEC_TS_ACK_BOOT_COMPLETE);

	tsp_debug_err(true, &client->dev, "%s power enable\n", __func__);

#ifdef SEC_TS_FW_UPDATE_ON_PROBE
	ret = sec_ts_firmware_update_on_probe(ts);
	if (ret < 0)
		goto err_init;
#else
	tsp_debug_info(true, &ts->client->dev, "%s: fw update on probe disabled!\n", __func__);
#endif

	ret = sec_ts_read_information(ts);
	if (ret < 0)
		goto err_init;

	/* Sense_on */
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SENSE_ON, NULL, 0);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "sec_ts_probe: fail to write Sense_on\n");
		goto err_init;
	}

	ts->pFrame = kzalloc(ts->tx_count * ts->rx_count * 2, GFP_KERNEL);
	if (!ts->pFrame) {
		tsp_debug_err(true, &ts->client->dev, "%s: allocate pFrame err!\n", __func__);
		ret = -ENOMEM;
		goto err_allocate_frame;
	}

#ifdef CONFIG_TOUCHSCREN_SEC_TS_GLOVEMODE
	input_set_capability(ts->input_dev, EV_SW, SW_GLOVE);
#endif
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, ts->input_dev->keybit);
	set_bit(KEY_BLACK_UI_GESTURE, ts->input_dev->keybit);
#ifdef SEC_TS_SUPPORT_TOUCH_KEY
	if (ts->plat_data->support_mskey) {
		int i;

		for (i = 0 ; i < ts->plat_data->num_touchkey ; i++)
			set_bit(ts->plat_data->touchkey[i].keycode, ts->input_dev->keybit);

		set_bit(EV_LED, ts->input_dev->evbit);
		set_bit(LED_MISC, ts->input_dev->ledbit);
	}
#endif
#ifdef SEC_TS_SUPPORT_SIDE_GESTURE
	if (ts->plat_data->support_sidegesture) {
		set_bit(KEY_SIDE_GESTURE, ts->input_dev->keybit);
		set_bit(KEY_SIDE_GESTURE_RIGHT, ts->input_dev->keybit);
		set_bit(KEY_SIDE_GESTURE_LEFT, ts->input_dev->keybit);
	}
#endif
#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
#endif

	ts->input_dev->evbit[0] = BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

	input_mt_init_slots(ts->input_dev, MAX_SUPPORT_TOUCH_COUNT, INPUT_MT_DIRECT);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->plat_data->max_x, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->plat_data->max_y, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PALM, 0, 1, 0, 0);
#ifdef CONFIG_SEC_FACTORY
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
#endif

#if defined(SEC_TS_SUPPORT_SIDE_GESTURE)
	if (ts->plat_data->support_sidegesture)
		input_set_abs_params(ts->input_dev, ABS_MT_GRIP, 0, 1, 0, 0);
#endif
	input_set_drvdata(ts->input_dev, ts);
	i2c_set_clientdata(client, ts);

	ret = input_register_device(ts->input_dev);
	if (ret) {
		tsp_debug_err(true, &ts->client->dev, "%s: Unable to register %s input device\n", __func__, ts->input_dev->name);
		goto err_input_register_device;
	}

	tsp_debug_info(true, &ts->client->dev, "sec_ts_probe request_irq = %d\n" , client->irq);

	ret = request_threaded_irq(client->irq, NULL, sec_ts_irq_thread,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, SEC_TS_I2C_NAME, ts);
	if (ret < 0) {
		tsp_debug_err(true, &ts->client->dev, "sec_ts_probe: Unable to request threaded irq\n");
		goto err_irq;
	}

#ifdef SEC_TS_SUPPORT_TA_MODE
	ts->callbacks.inform_charger = sec_ts_ta_cb;
	if (ts->plat_data->register_cb)
		ts->plat_data->register_cb(&ts->callbacks);
#endif

	sec_ts_raw_device_init(ts);
	sec_ts_fn_init(ts);

#ifdef CONFIG_SECURE_TOUCH
	if (sysfs_create_group(&ts->input_dev->dev.kobj, &secure_attr_group) < 0)
		tsp_debug_err(true, &ts->client->dev, "%s: do not make secure group\n", __func__);
	else
		secure_touch_init(ts);
#endif

	device_init_wakeup(&client->dev, true);

#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
	INIT_DELAYED_WORK(&ts->ghost_check, sec_ts_check_rawdata);
	p_ghost_check = &ts->ghost_check;
#endif

	ts->probe_done = true;

	return 0;

err_irq:
	input_unregister_device(ts->input_dev);
	ts->input_dev = NULL;
err_input_register_device:
	if (ts->input_dev)
		input_free_device(ts->input_dev);
	kfree(ts->pFrame);
err_allocate_frame:
err_init:
	ts->plat_data->power(ts, false);
	wake_lock_destroy(&ts->wakelock);
err_allocate_device:
err_get_drv_data:
	if (gpio_is_valid(ts->plat_data->gpio))
		gpio_free(ts->plat_data->gpio);
	kfree(ts);
err_setup_drv_data:
	return ret;
}

void sec_ts_release_all_finger(struct sec_ts_data *ts)
{
	int i;

	for (i = 0; i < MAX_SUPPORT_TOUCH_COUNT; i++) {
		input_mt_slot(ts->input_dev, i);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
		ts->coord[i].mcount = 0;
	}

	input_report_key(ts->input_dev, BTN_TOUCH, false);
	input_report_key(ts->input_dev, BTN_TOOL_FINGER, false);
	input_report_switch(ts->input_dev, SW_GLOVE, false);
	ts->touchkey_glove_mode_status = false;
	ts->touch_count = 0;

	input_sync(ts->input_dev);
}

static int sec_ts_gesture_mode(struct sec_ts_data *ts)
{
	char mode;
	int ret;

	if(ts->lowpower_mode == 0){
		return 1;
	}

	tsp_debug_info(true, &ts->client->dev, "%s, mode:%d\n", __func__, ts->lowpower_mode);

	ret = ts->sec_ts_i2c_read(ts, SEC_TS_CMD_GESTURE_MODE, &mode, 1);
	if (ret < 0){
		tsp_debug_err(true, &ts->client->dev, "%s: Failed to read mode\n", __func__);
		return 0;
	}

	if(ts->lowpower_mode & SEC_TS_MODE_LSI_SPAY){		
		mode |= SEC_TS_MODE_LSI_SPAY;
		ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_GESTURE_MODE, &mode, 1);
		if (ret < 0){
			tsp_debug_err(true, &ts->client->dev, "%s: Failed to write mode: SPay, mode:%d\n", __func__, mode);
			return 0;
		}else{
			tsp_debug_info(true, &ts->client->dev, "%s, SPay enable\n", __func__);
		}
	}
	return 1;
}

static int sec_ts_set_lowpowermode(struct sec_ts_data *ts, bool mode)
{
	int ret;
	u8 buff;

	tsp_debug_err(true, &ts->client->dev, "%s: %s\n", __func__,
			mode == TO_LOWPOWER_MODE ? "ENTER" :"EXIT");

	if (mode == TO_LOWPOWER_MODE)
		sec_ts_gesture_mode(ts);

	buff = mode;
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_POWER_MODE, &buff, 1);
	if (ret < 0)
		tsp_debug_err(true, &ts->client->dev,
				"%s: failed\n", __func__);

	if (device_may_wakeup(&ts->client->dev)) {
		if (mode)
			enable_irq_wake(ts->client->irq);
		else
			disable_irq_wake(ts->client->irq);
	}

	return ret;
}

static void sec_ts_reset_work(struct work_struct *work)
{
	struct sec_ts_data *ts = container_of(work, struct sec_ts_data,
						reset_work.work);
	u8 temp_lpm, temp_status;

	if (!ts->probe_done)
		return;

	if (ts->power_status == SEC_TS_STATE_POWER_OFF){
		tsp_debug_err(true, &ts->client->dev, "%s ic pwr off\n", __func__);
		return;
	}

	tsp_debug_info(true, &ts->client->dev, "%s start\n", __func__);

	temp_lpm = ts->lowpower_mode;
	temp_status = ts->power_status;

	/* Reset-routine must go to power off state  */
	ts->lowpower_mode = 0;

#ifdef USE_OPEN_CLOSE
	sec_ts_input_close(ts->input_dev);
	sec_ts_delay(30);
	sec_ts_input_open(ts->input_dev);
#else
	sec_ts_stop_device(ts);
	sec_ts_delay(30);
	sec_ts_start_device(ts);
#endif
	ts->lowpower_mode = temp_lpm;

	if((temp_status != SEC_TS_STATE_POWER_ON) && (temp_status != SEC_TS_STATE_POWER_OFF) ){
		if (ts->lowpower_mode){
			sec_ts_set_lowpowermode(ts, TO_LOWPOWER_MODE);		
			ts->power_status = temp_status;
		}
	}

	tsp_debug_info(true, &ts->client->dev, "%s done\n", __func__);
}

#ifdef USE_OPEN_CLOSE
static int sec_ts_input_open(struct input_dev *dev)
{
	struct sec_ts_data *ts = input_get_drvdata(dev);
	int ret;

	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);
#ifdef CONFIG_SECURE_TOUCH
	secure_touch_stop(ts, 0);
#endif

	if ((ts->lowpower_mode) || (ts->power_status == SEC_TS_STATE_LPM_RESUME)) {
		sec_ts_release_all_finger(ts);
#ifdef USE_RESET_EXIT_LPM
		sec_ts_stop_device(ts);
		sec_ts_delay(30);
		sec_ts_start_device(ts);
#else
		sec_ts_set_lowpowermode(ts, TO_TOUCH_MODE);
#endif
	} else {
		ret = sec_ts_start_device(ts);
		if (ret < 0)
			tsp_debug_err(true, &ts->client->dev, "%s: Failed to start device\n", __func__);
	}

	return 0;
}

static void sec_ts_input_close(struct input_dev *dev)
{
	struct sec_ts_data *ts = input_get_drvdata(dev);

	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);
#ifdef CONFIG_SECURE_TOUCH
	secure_touch_stop(ts, 1);
#endif

	if (ts->lowpower_mode){
		sec_ts_release_all_finger(ts);
		sec_ts_set_lowpowermode(ts, TO_LOWPOWER_MODE);
		ts->power_status = SEC_TS_STATE_LPM_RESUME;
	}else
		sec_ts_stop_device(ts);
}
#endif
static int sec_ts_remove(struct i2c_client *client)
{
	struct sec_ts_data *ts = i2c_get_clientdata(client);

	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);

	sec_ts_fn_remove(ts);
	free_irq(client->irq, ts);

	device_init_wakeup(&client->dev, false);
	wake_lock_destroy(&ts->wakelock);

	input_mt_destroy_slots(ts->input_dev);
	input_unregister_device(ts->input_dev);

	ts->input_dev = NULL;
	ts->plat_data->power(ts, false);

	kfree(ts);
	return 0;
}

static void sec_ts_shutdown(struct i2c_client *client)
{
	struct sec_ts_data *ts = i2c_get_clientdata(client);

	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);

	sec_ts_remove(client);
}

static int sec_ts_stop_device(struct sec_ts_data *ts)
{
	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->device_mutex);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		tsp_debug_err(true, &ts->client->dev, "%s: already power off\n", __func__);
		goto out;
	}

	ts->power_status = SEC_TS_STATE_POWER_OFF;

	disable_irq(ts->client->irq);
	sec_ts_release_all_finger(ts);

	ts->plat_data->power(ts, false);

	if (ts->plat_data->enable_sync)
		ts->plat_data->enable_sync(false);

	// sec_ts_pinctrl_configure(ts, false); // NA7

out:
	mutex_unlock(&ts->device_mutex);
	return 0;
}

static int sec_ts_start_device(struct sec_ts_data *ts)
{
	int ret;

	tsp_debug_info(true, &ts->client->dev, "%s\n", __func__);

	// sec_ts_pinctrl_configure(ts, true); // NA7

	mutex_lock(&ts->device_mutex);

	if (ts->power_status == SEC_TS_STATE_POWER_ON) {
		tsp_debug_err(true, &ts->client->dev, "%s: already power on\n", __func__);
		goto out;
	}

	sec_ts_release_all_finger(ts);

	ts->plat_data->power(ts, true);
	sec_ts_delay(70);
	ts->power_status = SEC_TS_STATE_POWER_ON;
	sec_ts_wait_for_ready(ts, SEC_TS_ACK_BOOT_COMPLETE);

	if (ts->plat_data->enable_sync)
		ts->plat_data->enable_sync(true);

#ifdef SEC_TS_SUPPORT_TA_MODE
	if (ts->ta_status)
		sec_ts_charger_config(ts, ts->ta_status);
#endif

	if(ts->flip_enable){
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_COVERTYPE, &ts->cover_cmd, 1);

		ts->touch_functions = ts->touch_functions | SEC_TS_BIT_SETFUNC_COVER | SEC_TS_BIT_SETFUNC_MUTUAL; 
		tsp_debug_info(true, &ts->client->dev,
				"%s: cover cmd write type:%d, mode:%x, ret:%d", __func__, ts->touch_functions, ts->cover_cmd, ret);
	}else{
		ts->touch_functions = ((ts->touch_functions & (~SEC_TS_BIT_SETFUNC_COVER)) | SEC_TS_BIT_SETFUNC_MUTUAL);
	}

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, &ts->touch_functions, 1);
	if (ret < 0)
		tsp_debug_err(true, &ts->client->dev,
				"%s: Failed to send glove_mode command", __func__);

	/* Sense_on */
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SENSE_ON, NULL, 0);
	if (ret < 0)
		tsp_debug_err(true, &ts->client->dev, "sec_ts_probe: fail to write Sense_on\n");

	enable_irq(ts->client->irq);

out:
	mutex_unlock(&ts->device_mutex);
	return 0;
}

#ifdef CONFIG_PM
static int sec_ts_pm_suspend(struct device *dev)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
/*
	mutex_lock(&ts->input_dev->mutex);
	if (ts->input_dev->users)
		sec_ts_stop_device(ts);
	mutex_unlock(&ts->input_dev->mutex);
*/

	if (ts->lowpower_mode) {
		ts->power_status = SEC_TS_STATE_LPM_SUSPEND;
		INIT_COMPLETION(ts->resume_done);
	}

	return 0;
}

static int sec_ts_pm_resume(struct device *dev)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
/*
	mutex_lock(&ts->input_dev->mutex);
	if (ts->input_dev->users)
		sec_ts_start_device(ts);
	mutex_unlock(&ts->input_dev->mutex);
*/

	if (ts->lowpower_mode) {
		ts->power_status = SEC_TS_STATE_LPM_RESUME;
		complete_all(&ts->resume_done);
	}

	return 0;
}
#endif

static const struct i2c_device_id sec_ts_id[] = {
	{ SEC_TS_I2C_NAME, 0 },
	{ },
};

#ifdef CONFIG_PM
static const struct dev_pm_ops sec_ts_dev_pm_ops = {
	.suspend = sec_ts_pm_suspend,
	.resume = sec_ts_pm_resume,
};
#endif

#ifdef CONFIG_OF
static struct of_device_id sec_ts_match_table[] = {
	{ .compatible = "sec,sec_ts",},
	{ },
};
#else
#define sec_ts_match_table NULL
#endif

static struct i2c_driver sec_ts_driver = {
	.probe		= sec_ts_probe,
	.remove		= sec_ts_remove,
	.shutdown	= sec_ts_shutdown,
	.id_table	= sec_ts_id,
	.driver = {
		.owner    = THIS_MODULE,
		.name	= SEC_TS_I2C_NAME,
#ifdef CONFIG_OF
		.of_match_table = sec_ts_match_table,
#endif
#ifdef CONFIG_PM
		.pm = &sec_ts_dev_pm_ops,
#endif
	},
};

static int __init sec_ts_init(void)
{
	pr_err("%s\n", __func__);

#ifdef CONFIG_SAMSUNG_LPM_MODE
	if (poweroff_charging) {
		pr_err("%s : Do not load driver due to : lpm %d\n",
				__func__, poweroff_charging);
		return -ENODEV;
	}
#endif
	return i2c_add_driver(&sec_ts_driver);
}

static void __exit sec_ts_exit(void)
{
	i2c_del_driver(&sec_ts_driver);
}

MODULE_AUTHOR("Hyobae, Ahn<hyobae.ahn@samsung.com>");
MODULE_DESCRIPTION("Samsung Electronics TouchScreen driver");
MODULE_LICENSE("GPL");

module_init(sec_ts_init);
module_exit(sec_ts_exit);
