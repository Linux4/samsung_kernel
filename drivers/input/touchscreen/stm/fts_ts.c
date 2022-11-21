/******************** (C) COPYRIGHT 2012 STMicroelectronics ********************
*
* File Name		: fts.c
* Authors		: AMS(Analog Mems Sensor) Team
* Description	: FTS Capacitive touch screen controller (FingerTipS)
*
********************************************************************************
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
* OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
* PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
* THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
********************************************************************************
* REVISON HISTORY
* DATE		| DESCRIPTION
* 03/09/2012| First Release
* 08/11/2012| Code migration
* 23/01/2013| SEC Factory Test
* 29/01/2013| Support Hover Events
* 08/04/2013| SEC Factory Test Add more - hover_enable, glove_mode, clear_cover_mode, fast_glove_mode
* 09/04/2013| Support Blob Information
*******************************************************************************/

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/serio.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/power_supply.h>
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#include <linux/input/mt.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include "fts_ts.h"

#if defined(CONFIG_SECURE_TOUCH)
#include <linux/completion.h>
#include <linux/pm_runtime.h>
#include <linux/errno.h>
#include <linux/atomic.h>
/*#include <asm/system.h>*/
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
static struct fts_touchkey fts_touchkeys[] = {
	{
		.value = 0x01,
		.keycode = KEY_RECENT,
		.name = "recent",
	},
	{
		.value = 0x02,
		.keycode = KEY_BACK,
		.name = "back",
	},
};
#endif

#if 0
&i2c_3 {		/* change BLSP7 */
	fts_i2c@49 {
		compatible = "stm,fts_ts";
		reg = <0x49>;
		interrupt-parent = <&tlmm>;
		interrupts = <65 0x0>;
		avdd-supply = <&s2mpb02_ldo17>;
		vddo-supply = <&pm8952_l6>;
		stm,supply-num = <0>;		// the number of regulators
		stm,supply-name = "s2mpb02_ldo17", "pm8952_l6";	// the name of regulators, in power-on order
		stm,irq-gpio = <&tlmm 65 0x0>;
		stm,tsp-coords = <4095 4095>;
		stm,grip_area = <512>;
		stm,support_func = <1 1>; // <support_hover, support_mshover>
		stm,i2c-pull-up = <1>;
		stm,tsp-project = "ZERO_F";
		stm,tsp-model = "G920";
		stm,tsp-extra_config = <0 0 0 0>;// <"pwrctrl", "pwrctrl(sub)", "octa_id", "N">
		stm,bringup = <1>;
	};
};
#endif

#ifdef USE_OPEN_CLOSE
static int fts_input_open(struct input_dev *dev);
static void fts_input_close(struct input_dev *dev);
#ifdef USE_OPEN_DWORK
static void fts_open_work(struct work_struct *work);
#endif
#endif

#if defined(USE_RESET_WORK_EXIT_LOWPOWERMODE) || defined(FTS_SUPPORT_ESD_CHECK)
static void fts_reset_work(struct work_struct *work);
#endif
static void fts_recovery_cx(struct fts_ts_info *info);

#ifdef TSP_RAWDATA_DUMP
static void ghost_touch_check(struct work_struct *work);
static struct delayed_work *p_ghost_check;
#endif

static int fts_stop_device(struct fts_ts_info *info);
static int fts_start_device(struct fts_ts_info *info);
static int fts_irq_enable(struct fts_ts_info *info, bool enable);
static void fts_release_all_finger(struct fts_ts_info *info);

#if defined(CONFIG_SECURE_TOUCH)
static void fts_secure_touch_notify(struct fts_ts_info *info);

static irqreturn_t fts_filter_interrupt(struct fts_ts_info *info);

static irqreturn_t fts_interrupt_handler(int irq, void *handle);

static ssize_t fts_secure_touch_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t fts_secure_touch_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t fts_secure_touch_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static struct device_attribute attrs[] = {
	__ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
			fts_secure_touch_enable_show,
			fts_secure_touch_enable_store),
	__ATTR(secure_touch, (S_IRUGO),
			fts_secure_touch_show,
			NULL),
};

static int fts_secure_touch_clk_prepare_enable(struct fts_ts_info *info)
{
	int ret;
	ret = clk_prepare_enable(info->iface_clk);
	if (ret) {
		dev_err(&info->client->dev,
			"%s: error on clk_prepare_enable(iface_clk):%d\n", __func__, ret);
		return ret;
	}

	ret = clk_prepare_enable(info->core_clk);
	if (ret) {
		clk_disable_unprepare(info->iface_clk);
		dev_err(&info->client->dev,
			"%s: error clk_prepare_enable(core_clk):%d\n", __func__, ret);
		return ret;
	}
	return ret;
}

static void fts_secure_touch_clk_disable_unprepare(
		struct fts_ts_info *info)
{
	clk_disable_unprepare(info->core_clk);
	clk_disable_unprepare(info->iface_clk);
}

static ssize_t fts_secure_touch_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&info->st_enabled));
}

/*
 * Accept only "0" and "1" valid values.
 * "0" will reset the st_enabled flag, then wake up the reading process.
 * The bus driver is notified via pm_runtime that it is not required to stay
 * awake anymore.
 * It will also make sure the queue of events is emptied in the controller,
 * in case a touch happened in between the secure touch being disabled and
 * the local ISR being ungated.
 * "1" will set the st_enabled flag and clear the st_pending_irqs flag.
 * The bus driver is requested via pm_runtime to stay awake.
 */
static ssize_t fts_secure_touch_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	unsigned long value;
	int err = 0;

	if (count > 2)
		return -EINVAL;

	err = kstrtoul(buf, 10, &value);
	if (err != 0)
		return err;

	err = count;

	switch (value) {
	case 0:
			if (atomic_read(&info->st_enabled) == 0)
				break;

			fts_secure_touch_clk_disable_unprepare(info);
			dev_err(&info->client->dev, "%s: SECURETOUCH_secure_touch_enable_reset", __func__);
			pm_runtime_put(info->client->adapter->dev.parent);
			atomic_set(&info->st_enabled, 0);
			fts_secure_touch_notify(info);
			fts_interrupt_handler(info->client->irq, info);
			complete(&info->st_powerdown);
#ifdef ST_INT_COMPLETE
			complete(&info->st_interrupt);
#endif
			break;

	case 1:
			if (atomic_read(&info->st_enabled)) {
				err = -EBUSY;
				break;
			}

			if (pm_runtime_get(info->client->adapter->dev.parent) < 0) {
				dev_err(&info->client->dev, "pm_runtime_get failed\n");
				err = -EIO;
				break;
			}

			if (fts_secure_touch_clk_prepare_enable(info) < 0) {
				pm_runtime_put(info->client->adapter->dev.parent);
				err = -EIO;
				break;
			}

			dev_err(&info->client->dev, "%s: SECURETOUCH_secure_touch_enable_set", __func__);
			INIT_COMPLETION(info->st_powerdown);
#ifdef ST_INT_COMPLETE
			INIT_COMPLETION(info->st_interrupt);
#endif
			atomic_set(&info->st_enabled, 1);
			synchronize_irq(info->client->irq);
			atomic_set(&info->st_pending_irqs, 0);
			break;

	default:
			dev_err(&info->client->dev, "unsupported value: %lu\n", value);
			err = -EINVAL;
			break;
	}

	return err;
}

static ssize_t fts_secure_touch_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int val = 0;

	dev_err(&info->client->dev, "%s: SECURETOUCH_fts_secure_touch_show &info->st_pending_irqs=%d ",
								__func__, atomic_read(&info->st_pending_irqs));

	if (atomic_read(&info->st_enabled) == 0)
		return -EBADF;

	if (atomic_cmpxchg(&info->st_pending_irqs, -1, 0) == -1)
		return -EINVAL;

	if (atomic_cmpxchg(&info->st_pending_irqs, 1, 0) == 1)
		val = 1;

	dev_err(&info->client->dev, "%s %d: SECURETOUCH_fts_secure_touch_show", __func__, val);
	dev_err(&info->client->dev, "%s: SECURETOUCH_fts_secure_touch_show &info->st_pending_irqs=%d ",
								__func__, atomic_read(&info->st_pending_irqs));
#ifdef ST_INT_COMPLETE
	complete(&info->st_interrupt);
#endif
	return scnprintf(buf, PAGE_SIZE, "%u", val);
}
#endif

static int fts_write_reg(struct fts_ts_info *info,
		  unsigned char *reg, unsigned short num_com)
{
	struct i2c_msg xfer_msg[2];
	struct i2c_client *client = info->client;
	int ret;

	if (info->touch_stopped) {
		dev_err(&info->client->dev, "%s: Sensor stopped\n", __func__);
		goto exit;
	}

	mutex_lock(&info->i2c_mutex);

	xfer_msg[0].addr = client->addr;
	xfer_msg[0].len = num_com;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = reg;

	ret = i2c_transfer(client->adapter, xfer_msg, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s failed. ret: %d, addr: %x\n",
					__func__, ret, xfer_msg[0].addr);

	mutex_unlock(&info->i2c_mutex);
	return ret;

 exit:
	return 0;
}

static int fts_read_reg(struct fts_ts_info *info, unsigned char *reg, int cnum,
		 unsigned char *buf, int num)
{
	struct i2c_msg xfer_msg[2];
	struct i2c_client *client = info->client;
	int ret;
	int retry = 3;

	if (info->touch_stopped) {
		dev_err(&info->client->dev, "%s: Sensor stopped\n", __func__);
		goto exit;
	}
	mutex_lock(&info->i2c_mutex);

	xfer_msg[0].addr = client->addr;
	xfer_msg[0].len = cnum;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = reg;

	xfer_msg[1].addr = client->addr;
	xfer_msg[1].len = num;
	xfer_msg[1].flags = I2C_M_RD;
	xfer_msg[1].buf = buf;

	do {
		ret = i2c_transfer(client->adapter, xfer_msg, 2);
		if (ret < 0) {
			dev_err(&client->dev, "%s failed(%d). ret:%d, addr:%x\n",
					__func__, retry, ret, xfer_msg[0].addr);
			usleep_range(10 * 1000, 10 * 1000);
		} else {
			break;
		}
	} while (--retry > 0);

	mutex_unlock(&info->i2c_mutex);
	return ret;

 exit:
	return 0;
}

/*
 * int fts_read_to_string(struct fts_ts_info *, unsigned short *reg, unsigned char *data, int length)
 * read specfic value from the string area.
 * string area means Display Lab algorithm
 */

static int fts_read_from_string(struct fts_ts_info *info,
					unsigned short *reg, unsigned char *data, int length)
{
	unsigned char string_reg[3];
	unsigned char *buf;
	int ret = 0;

	if (!info->dt_data->support_string_lib) {
		dev_err(&info->client->dev, "%s: Did not support String lib\n", __func__);
		return 0;
	}

	string_reg[0] = 0xD0;
	string_reg[1] = (*reg >> 8) & 0xFF;
	string_reg[2] = *reg & 0xFF;

	if (info->digital_rev == FTS_DIGITAL_REV_1) {
		ret = fts_read_reg(info, string_reg, 3, data, length);
	} else {
		int ret;
		buf = kzalloc(length + 1, GFP_KERNEL);
		if (buf == NULL) {
			tsp_debug_info(true, &info->client->dev,
					"%s: kzalloc error.\n", __func__);
			return -ENOMEM;
		}

		ret = fts_read_reg(info, string_reg, 3, buf, length + 1);
		if (ret >= 0)
			memcpy(data, &buf[1], length);

		kfree(buf);
	}

	return ret;
}
/*
 * int fts_write_to_string(struct fts_ts_info *, unsigned short *, unsigned char *, int)
 * send command or write specfic value to the string area.
 * string area means Display Lab algorithm
 */
static int fts_write_to_string(struct fts_ts_info *info,
					unsigned short *reg, unsigned char *data, int length)
{
	struct i2c_msg xfer_msg[3];
	unsigned char *regAdd;
	int ret;

	if (info->touch_stopped) {
		dev_err(&info->client->dev, "%s: Sensor stopped\n", __func__);
		return 0;
	}

	if (!info->dt_data->support_string_lib) {
		dev_err(&info->client->dev, "%s: Did not support String lib\n", __func__);
		return 0;
	}

	regAdd = kzalloc(length + 6, GFP_KERNEL);
	if (regAdd == NULL) {
		tsp_debug_info(true, &info->client->dev,
				"%s: kzalloc error.\n", __func__);
		return -ENOMEM;
	}

	mutex_lock(&info->i2c_mutex);

/* msg[0], length 3*/
	regAdd[0] = 0xb3;
	regAdd[1] = 0x20;
	regAdd[2] = 0x01;

	xfer_msg[0].addr = info->client->addr;
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

	xfer_msg[1].addr = info->client->addr;
	xfer_msg[1].len = 3 + length;
	xfer_msg[1].flags = 0;
	xfer_msg[1].buf = &regAdd[3];
/* msg[1], length 4*/

	ret = i2c_transfer(info->client->adapter, xfer_msg, 2);
	if (ret == 2) {
		dev_info(&info->client->dev,
				"%s: string command is OK.\n", __func__);

		regAdd[0] = FTS_CMD_NOTIFY;
		regAdd[1] = *reg & 0xFF;
		regAdd[2] = (*reg >> 8) & 0xFF;

		xfer_msg[0].addr = info->client->addr;
		xfer_msg[0].len = 3;
		xfer_msg[0].flags = 0;
		xfer_msg[0].buf = regAdd;

		ret = i2c_transfer(info->client->adapter, xfer_msg, 1);
		if (ret != 1)
			dev_info(&info->client->dev,
					"%s: string notify is failed.\n", __func__);
		else
			dev_info(&info->client->dev,
					"%s: string notify is OK[%X].\n", __func__, *data);

	} else {
		dev_info(&info->client->dev,
				"%s: string command is failed. ret: %d\n", __func__, ret);
	}

	mutex_unlock(&info->i2c_mutex);
	kfree(regAdd);

	return ret;

}

static void fts_delay(unsigned int ms)
{
	if (ms < 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}

static int fts_command(struct fts_ts_info *info, unsigned char cmd)
{
	unsigned char regAdd = 0;
	int ret;

	regAdd = cmd;
	ret = fts_write_reg(info, &regAdd, 1);
	if (ret < 0)
		dev_err(&info->client->dev, "%s failed. ret: %d\n",
					__func__, ret);
	else
		dev_info(&info->client->dev, "%s: cmd: %02X, ret: %d\n",
					__func__, cmd, ret);

	return ret;
}

static void fts_enable_feature(struct fts_ts_info *info, unsigned char cmd, int enable)
{
	unsigned char regAdd[2] = {0xC1, 0x00};
	int ret = 0;

	if (!enable)
		regAdd[0] = 0xC2;
	regAdd[1] = cmd;
	ret = fts_write_reg(info, &regAdd[0], 2);
	tsp_debug_info(true, &info->client->dev,
				"FTS %s Feature (%02X %02X) , ret = %d\n",
				enable ? "Enable" : "Disable", regAdd[0], regAdd[1], ret);
}

/* Cover Type
 * 0 : Flip Cover
 * 1 : S View Cover
 * 2 : N/A
 * 3 : S View Wireless Cover
 * 4 : N/A
 * 5 : S Charger Cover
 * 6 : S View Wallet Cover
 * 7 : LED Cover
 * 100 : Montblanc
 */
static void fts_set_cover_type(struct fts_ts_info *info, bool enable)
{
	dev_info(&info->client->dev, "%s: %d\n", __func__, info->cover_type);

	switch (info->cover_type) {
	case FTS_VIEW_WIRELESS:
	case FTS_VIEW_COVER:
		fts_enable_feature(info, FTS_FEATURE_COVER_GLASS, enable);
		break;
	case FTS_VIEW_WALLET:
		fts_enable_feature(info, FTS_FEATURE_COVER_WALLET, enable);
		break;
	case FTS_FLIP_WALLET:
	case FTS_LED_COVER:
	case FTS_MONTBLANC_COVER:
		fts_enable_feature(info, FTS_FEATURE_COVER_LED, enable);
		break;
	case FTS_CLEAR_FLIP_COVER :
		fts_enable_feature(info, FTS_FEATURE_COVER_CLEAR_FLIP, enable);
		break;
	case FTS_QWERTY_KEYBOARD_EUR :
	case FTS_QWERTY_KEYBOARD_KOR :
		fts_enable_feature(info, 0x0D, enable);
		break;
	case FTS_CHARGER_COVER:
	case FTS_COVER_NOTHING1:
	case FTS_COVER_NOTHING2:
	default:
		dev_err(&info->client->dev, "%s: not chage touch state, %d\n",
				__func__, info->cover_type);
		break;
	}

	info->flip_state = enable;

}
static int fts_change_scan_rate(struct fts_ts_info *info, unsigned char cmd)
{
	unsigned char regAdd[2] = {0xC3, 0x00};
	int ret = 0;

	regAdd[1] = cmd;

	ret = fts_write_reg(info, &regAdd[0], 2);
	if (ret < 0)
		dev_err(&info->client->dev, "%s failed. ret: %d\n",
					__func__, ret);
	else
		dev_info(&info->client->dev, "%s: cmd: %02X, ret: %d\n",
					__func__, cmd, ret);

	return ret;
}

static int fts_systemreset(struct fts_ts_info *info)
{
	unsigned char regAdd[4] = {0xB6, 0x00, 0x23, 0x01};
	int ret;

	ret = fts_write_reg(info, regAdd, 4);
	if (ret < 0)
		dev_err(&info->client->dev, "%s failed. ret: %d\n",
					__func__, ret);
	else
		dev_info(&info->client->dev, "%s: ret: %d\n",
					__func__, ret);

	fts_delay(10);

	return ret;
}

static int fts_interrupt_set(struct fts_ts_info *info, int enable)
{
	unsigned char regAdd[4] = {0xB6, 0x00, 0x1C, enable};
	int ret;

	ret = fts_write_reg(info, regAdd, 4);
	if (ret < 0)
		dev_err(&info->client->dev, "%s failed. ret: %d\n",
					__func__, ret);
	else
		dev_info(&info->client->dev, "%s: %s. ret: %d\n",
					__func__, enable ? "enable" : "disable", ret);

	return ret;
}

/*
 * static int fts_read_chip_id(struct fts_ts_info *info)
 * :
 */
static int fts_read_chip_id(struct fts_ts_info *info)
{

	unsigned char regAdd[3] = {0xB6, 0x00, 0x07};
	unsigned char val[7] = {0};
	int ret;

	ret = fts_read_reg(info, regAdd, 3, val, 7);
	if (ret < 0) {
		dev_err(&info->client->dev, "%s failed. ret: %d\n",
					__func__, ret);

		return ret;
	}

	dev_info(&info->client->dev, "%s: %02X %02X %02X %02X %02X %02X\n",
			__func__, val[1], val[2], val[3],
			val[4], val[5], val[6]);

	if (val[1] == FTS_ID0 && val[2] == FTS_ID2) {
		if (val[4] == 0x00 && val[5] == 0x00) {
			/* 00 39 6C 03 00 00 ==> Cx Corruption */
			fts_recovery_cx(info);
		} else {
			tsp_debug_info(true, &info->client->dev,
				"FTS Chip ID : %02X %02X\n", val[1], val[2]);
		}
	}

	if (val[1] != FTS_ID0) {
		dev_err(&info->client->dev, "%s: invalid chip id, %02X\n",
					__func__, val[1]);
		return -FTS_ERROR_INVALID_CHIP_ID;
	}

	if (val[2] == FTS_ID1) {
		info->digital_rev = FTS_DIGITAL_REV_1;
	} else if (val[2] == FTS_ID2) {
		info->digital_rev = FTS_DIGITAL_REV_2;
	} else {
		dev_err(&info->client->dev, "%s: invalid chip version id, %02X\n",
					__func__, val[2]);
		return -FTS_ERROR_INVALID_CHIP_VERSION_ID;
	}

	if ((val[5] == 0) && (val[6] == 0)) {
		dev_err(&info->client->dev, "%s: invalid version, need firmup\n", __func__);
		info->need_force_firmup = true;
	}

	return ret;
}

static int fts_wait_for_ready(struct fts_ts_info *info)
{
	unsigned char regAdd = READ_ONE_EVENT;
	unsigned char data[FTS_EVENT_SIZE] = {0, };
	int retry = 0, err_cnt = 0, ret;

	do {
		ret = fts_read_reg(info, &regAdd, 1, data, FTS_EVENT_SIZE);

		if (data[0] == EVENTID_CONTROLLER_READY) {
			ret = 0;
			break;
		}

		if (data[0] == EVENTID_ERROR) {
			if (err_cnt++ > 32) {
				ret = -FTS_ERROR_EVENT_ID;
				break;
			}
			continue;
		}

		if (ret < 0 && retry == 3) {
			dev_err(&info->client->dev, "%s: i2c error\n", __func__);
			ret = -FTS_ERROR_TIMEOUT;
			if (info->lowpower_mode) {
				schedule_delayed_work(&info->reset_work, msecs_to_jiffies(10));
			}
			break;
		}

		if (retry++ > 30) {
			dev_err(&info->client->dev, "%s: time out\n", __func__);
			ret = -FTS_ERROR_TIMEOUT;
			break;
		}

		fts_delay(10);
	} while (ret);

	dev_info(&info->client->dev,
		"%s: %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X\n",
		__func__, data[0], data[1], data[2], data[3],
		data[4], data[5], data[6], data[7]);

	return ret;
}

static int fts_get_version_info(struct fts_ts_info *info)
{
	unsigned char regAdd[3] = {0, 0, 0};
	unsigned char data[FTS_EVENT_SIZE] = {0, };
	int retry = 0, ret;

	ret = fts_command(info, FTS_CMD_RELEASEINFO);
	if (ret < 0) {
		dev_err(&info->client->dev, "%s: failed\n", __func__);
		return ret;
	}

	regAdd[0] = READ_ONE_EVENT;

	while (fts_read_reg(info, regAdd, 1, data, FTS_EVENT_SIZE)) {
		if (data[0] == EVENTID_INTERNAL_RELEASE_INFO) {
			info->fw_version_of_ic = ((data[3] << 8) | data[4]);
			info->config_version_of_ic = ((data[6] << 8) | data[5]);
			info->ic_product_id = data[2];
		} else if (data[0] == EVENTID_EXTERNAL_RELEASE_INFO) {
			info->fw_main_version_of_ic = ((data[1] << 8) | data[2]);
			break;
		}

		if (retry++ > 30) {
			dev_err(&info->client->dev, "%s: time out\n", __func__);
			ret = -FTS_ERROR_TIMEOUT;
			break;
		}
	}

	dev_info(&info->client->dev,
			"%s: ID: 0x%02X, Firmware: 0x%04X, Config: 0x%04X, Main: 0x%04X\n",
			__func__, info->ic_product_id, info->fw_version_of_ic,
			info->config_version_of_ic, info->fw_main_version_of_ic);

	return ret;
}


#ifdef FTS_SUPPORT_QEEXO_ROI
int fts_get_roi_address(struct fts_ts_info *info)
{
	int ret;
	char cmd[3] = {0xd0, 0x00, 0x4e};
	char rData[3] = {0};

	if (info->digital_rev == FTS_DIGITAL_REV_1) {
		ret = fts_read_reg(info, &cmd[0], 3, (unsigned char *)&info->roi_addr, 2);
	} else if (info->digital_rev == FTS_DIGITAL_REV_2) {
		 ret = fts_read_reg(info, &cmd[0], 3, rData, 3);
		 info->roi_addr = (rData[2] << 8 ) | rData[1];
	}

	dev_info(&info->client->dev, "%s: roidelta_read Address 0x%2x\n",
			__func__, info->roi_addr);

	return ret;
}
#endif

#ifdef FTS_SUPPORT_NOISE_PARAM
int fts_get_noise_param_address(struct fts_ts_info *info)
{
	unsigned char regAdd[3];
	unsigned char rData[3] = {0, };
	int ret = -1;
	int ii;

	regAdd[0] = 0xd0;
	regAdd[1] = 0x00;
	regAdd[2] = 0x40;

	if (info->digital_rev == FTS_DIGITAL_REV_1) {
		ret = fts_read_reg(info, regAdd, 3, (unsigned char *)info->noise_param->pAddr, 2);
	} else if (info->digital_rev == FTS_DIGITAL_REV_2) {
		ret = fts_read_reg(info, regAdd, 3, rData, 3);
		info->noise_param->pAddr[0] = (rData[2] << 8) | rData[1];
	}

	if (ret < 0) {
		dev_err(&info->client->dev, "%s: failed, ret: %d\n", __func__, ret);
		return ret;
	}

	for (ii = 1; ii < MAX_NOISE_PARAM; ii++)
		info->noise_param->pAddr[ii] = info->noise_param->pAddr[0] + (ii * 2);

	for (ii = 0; ii < MAX_NOISE_PARAM; ii++)
		dev_info(&info->client->dev, "%s: [%d] address: 0x%04X\n",
				__func__, ii, info->noise_param->pAddr[ii]);

	return ret;
}

static int fts_get_noise_param(struct fts_ts_info *info)
{
	int rc;
	unsigned char regAdd[3];
	unsigned char data[MAX_NOISE_PARAM * 2];
	struct fts_noise_param *noise_param;
	int i;
	unsigned char buf[3];

	noise_param = (struct fts_noise_param *)&info->noise_param;
	memset(data, 0x0, MAX_NOISE_PARAM * 2);

	for (i = 0; i < MAX_NOISE_PARAM; i++) {
		regAdd[0] = 0xb3;
		regAdd[1] = 0x00;
		regAdd[2] = 0x10;
		fts_write_reg(info, regAdd, 3);

		regAdd[0] = 0xb1;
		regAdd[1] = (info->noise_param->pAddr[i] >> 8) & 0xff;
		regAdd[2] = info->noise_param->pAddr[i] & 0xff;
		rc = fts_read_reg(info, regAdd, 3, buf, 3);

		info->noise_param->pData[i] = (buf[2] << 8) | buf[1];
	}

	for (i = 0; i < MAX_NOISE_PARAM; i++)
		dev_info(&info->client->dev, "%s: [%d] address: 0x%04X data: 0x%04X\n",
				__func__, i, info->noise_param->pAddr[i],
				info->noise_param->pData[i]);

	return rc;
}

static int fts_set_noise_param(struct fts_ts_info *info)
{
	int i;
	unsigned char regAdd[5];

	for (i = 0; i < MAX_NOISE_PARAM; i++) {
		regAdd[0] = 0xb3;
		regAdd[1] = 0x00;
		regAdd[2] = 0x10;
		fts_write_reg(info, regAdd, 3);

		regAdd[0] = 0xb1;
		regAdd[1] = (info->noise_param->pAddr[i] >> 8) & 0xff;
		regAdd[2] = info->noise_param->pAddr[i] & 0xff;
		regAdd[3] = info->noise_param->pData[i] & 0xff;
		regAdd[4] = (info->noise_param->pData[i] >> 8) & 0xff;
		fts_write_reg(info, regAdd, 5);
	}

	for (i = 0; i < MAX_NOISE_PARAM; i++)
		dev_info(&info->client->dev, "%s: [%d] address: 0x%04X, data: 0x%04X\n",
				__func__, i, info->noise_param->pAddr[i],
				info->noise_param->pData[i]);

	return 0;
}
#endif				// FTS_SUPPORT_NOISE_PARAM

#ifdef FTS_SUPPORT_TOUCH_KEY
static void fts_release_all_key(struct fts_ts_info *info)
{
	input_report_key(info->input_dev, KEY_RECENT, KEY_RELEASE);
	input_report_key(info->input_dev, KEY_BACK, KEY_RELEASE);
	tsp_debug_info(true, &info->client->dev, "[TSP_KEY] %s\n", __func__);

#ifdef TKEY_BOOSTER
	if ((info->tsp_keystatus & TOUCH_KEY_RECENT)
		|| (info->tsp_keystatus & TOUCH_KEY_BACK)) {
		if (info->tkey_booster && info->tkey_booster->dvfs_set)
			info->tkey_booster->dvfs_set(info->tkey_booster, 0);
	} else {
		if (info->tkey_booster && info->tkey_booster->dvfs_set)
			info->tkey_booster->dvfs_set(info->tkey_booster, 2);
	}
#endif
}
#endif

/* Added for samsung dependent codes such as Factory test,
 * Touch booster, Related debug sysfs.
 */
#include "fts_sec.c"

static int fts_init(struct fts_ts_info *info)
{
	unsigned char val[16] = {0, };
	unsigned char regAdd[8];
	int ret;

	fts_systemreset(info);

	ret = fts_wait_for_ready(info);
	if (ret == -FTS_ERROR_EVENT_ID) {
		info->fw_version_of_ic = 0;
		info->config_version_of_ic = 0;
		info->fw_main_version_of_ic = 0;
	}/* else {
		fts_get_version_info(info);
	}*/

	ret = fts_read_chip_id(info);
	if (info->need_force_firmup) {
		dev_err(info->dev, "%s: Firmware empty, so force firmup\n", __func__);
		info->need_force_firmup = false;
	} else if (ret < 0) {
		return ret;
	}

	fts_get_version_info(info);

#ifdef FTS_SUPPORT_QEEXO_ROI
	fts_get_roi_address(info);
#endif

	ret = fts_get_channel_info(info);
	if (ret < 0) {
		dev_info(&info->client->dev, "%s: failed get channel info\n", __func__);
		return ret;
	}

	ret = fts_fw_update_on_probe(info);
	if (ret < 0)
		dev_err(info->dev, "%s: Failed to firmware update\n",
				__func__);

	fts_command(info, SLEEPOUT);

	fts_command(info, SENSEON);

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif
#ifdef FTS_SUPPORT_SIDE_GESTURE
	if (info->dt_data->support_sidegesture)
		fts_enable_feature(info, FTS_FEATURE_SIDE_GESTURE, true);
#endif

#ifdef FTS_SUPPORT_NOISE_PARAM
	fts_get_noise_param_address(info);
#endif
	/* fts driver set functional feature */
	info->touch_count = 0;
	info->hover_enabled = false;
	info->hover_ready = false;
	info->flip_enable = false;
	info->mainscr_disable = false;

	info->deepsleep_mode = false;
	info->lowpower_mode = false;
	info->lowpower_flag = 0x00;

	info->power_state = STATE_ACTIVE;

#ifdef FTS_SUPPORT_TOUCH_KEY
	info->tsp_keystatus = 0x00;
#endif

#ifdef FTS_SUPPORT_2NDSCREEN_FLAG
	info->SIDE_Flag = 0;
	info->previous_SIDE_value = 0;
#endif

#ifdef SEC_TSP_FACTORY_TEST
/*
	ret = fts_get_channel_info(info);
	if (ret < 0) {
		dev_info(&info->client->dev, "%s: failed get channel info\n", __func__);
		return ret;
	}
*/
	if (info->SenseChannelLength && info->ForceChannelLength) {
		info->pFrame =
		    kzalloc(info->SenseChannelLength * info->ForceChannelLength * 2,
			    GFP_KERNEL);

		info->cx_data = kzalloc(info->SenseChannelLength * info->ForceChannelLength, GFP_KERNEL);
		if (!info->pFrame || !info->cx_data) {
			dev_err(&info->client->dev, "%s: cx_data kzalloc Failed\n", __func__);
			return -ENOMEM;
		}
	}
#endif

	fts_command(info, FORCECALIBRATION);
	fts_command(info, FLUSHBUFFER);

	fts_interrupt_set(info, INT_ENABLE);

	regAdd[0] = READ_STATUS;
	ret = fts_read_reg(info, regAdd, 1, (unsigned char *)val, 4);
	dev_info(&info->client->dev, "FTS ReadStatus(0x84) : %02X %02X %02X %02X\n", val[0],
	       val[1], val[2], val[3]);

	dev_info(&info->client->dev, "FTS Initialized\n");

	return 0;
}

static void fts_debug_msg_event_handler(struct fts_ts_info *info,
				      unsigned char data[])
{
	dev_info(&info->client->dev,
			"%s: %02X %02X %02X %02X %02X %02X %02X %02X\n",
			__func__, data[0], data[1], data[2], data[3],
			data[4], data[5], data[6], data[7]);
}

#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
static char location_detect(struct fts_ts_info *info, int coord, bool flag)
{
	/* flag ? coord = Y : coord = X */
	int y_devide = info->dt_data->max_y / 3;

	if (flag) {
		if (coord < y_devide)
			return 'H';
		else if (coord < y_devide * 2)
			return 'M';
		else
			return 'L';
	} else {
		if (coord < 1440)
			return '0';
		else if (coord < 1530)
			return '1';
		else
			return '2';
	}

	return 'E';
}
#endif

static unsigned char fts_event_handler_type_b(struct fts_ts_info *info,
					      unsigned char data[],
					      unsigned char LeftEvent)
{
	unsigned char EventNum = 0;
	unsigned char NumTouches = 0;
	unsigned char TouchID = 0, EventID = 0, status = 0;
	unsigned char LastLeftEvent = 0;
	int x = 0, y = 0, z = 0;
	int bw = 0, bh = 0, palm = 0, sumsize = 0;
	unsigned short string_addr;
	unsigned char string_pi[16] = {0, };
	unsigned char string_data[10] = {0, };
#ifdef FTS_SUPPORT_2NDSCREEN_FLAG
	u8 currentSideFlag = 0;
#endif
#ifdef FTS_USE_SIDE_SCROLL_FLAG
	int scroll_flag = 0;
	int scroll_thr = 0;
#endif
#ifdef FTS_SUPPORT_SIDE_GESTURE
	static int longpress_release[FINGER_MAX] = {0, };
#endif
	unsigned char tool_type = MT_TOOL_FINGER;
#ifdef USE_STYLUS_PEN
	bool finger_type = MT_TOOL_FINGER;
#endif

	for (EventNum = 0; EventNum < LeftEvent; EventNum++) {
#if 0
		dev_info(&info->client->dev, "%d %2x %2x %2x %2x %2x %2x %2x %2x\n",
			EventNum,
			data[EventNum * FTS_EVENT_SIZE],
			data[EventNum * FTS_EVENT_SIZE + 1],
			data[EventNum * FTS_EVENT_SIZE + 2],
			data[EventNum * FTS_EVENT_SIZE + 3],
			data[EventNum * FTS_EVENT_SIZE + 4],
			data[EventNum * FTS_EVENT_SIZE + 5],
			data[EventNum * FTS_EVENT_SIZE + 6],
			data[EventNum * FTS_EVENT_SIZE + 7]);
#endif


		EventID = data[EventNum * FTS_EVENT_SIZE] & 0x0F;

		if ((EventID >= 3) && (EventID <= 5)) {
			LastLeftEvent = 0;
			NumTouches = 1;

			TouchID = (data[EventNum * FTS_EVENT_SIZE] >> 4) & 0x0F;
		} else {
			LastLeftEvent =
			    data[7 + EventNum * FTS_EVENT_SIZE] & 0x0F;
			NumTouches =
			    (data[1 + EventNum * FTS_EVENT_SIZE] & 0xF0) >> 4;
			TouchID = data[1 + EventNum * FTS_EVENT_SIZE] & 0x0F;
			EventID = data[EventNum * FTS_EVENT_SIZE] & 0xFF;
			status = data[1 + EventNum * FTS_EVENT_SIZE] & 0xFF;
		}

		if (info->hover_present &&
				(EventID != EVENTID_HOVER_ENTER_POINTER) &&
				(EventID != EVENTID_HOVER_MOTION_POINTER)) {

			dev_info(&info->client->dev,
				"[HR] tID:%d Ver[%02X%04X%01X%01X%01X] --\n",
				TouchID,
				info->panel_revision, info->fw_main_version_of_ic,
				info->flip_enable, info->mshover_enabled, info->mainscr_disable);
			info->finger[TouchID].mcount = 0;

			input_mt_slot(info->input_dev, 0);
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER, 0);
			info->hover_present = false;
		}

		switch (EventID) {
		case EVENTID_NO_EVENT:
			dev_info(&info->client->dev, "%s: No Event\n", __func__);
			break;

#ifdef FTS_SUPPORT_TOUCH_KEY
		case EVENTID_MSKEY:
			if (info->dt_data->support_mskey) {
				unsigned char input_keys;

				input_keys = data[2 + EventNum * FTS_EVENT_SIZE];

				if (input_keys == 0x00)
					fts_release_all_key(info);
				else {
					unsigned char change_keys;
					unsigned char key_state;
					unsigned char key_recent;
					unsigned char key_back;

					key_recent = TOUCH_KEY_RECENT;
					key_back = TOUCH_KEY_BACK;

					change_keys = input_keys ^ info->tsp_keystatus;

					if (change_keys & key_recent) {
						key_state = input_keys & key_recent;

						input_report_key(info->input_dev, KEY_RECENT, key_state != 0 ? KEY_PRESS : KEY_RELEASE);
						tsp_debug_info(true, &info->client->dev, "[TSP_KEY] RECENT %s\n", key_state != 0 ? "P" : "R");
					}

					if (change_keys & key_back) {
						key_state = input_keys & key_back;

						input_report_key(info->input_dev, KEY_BACK, key_state != 0 ? KEY_PRESS : KEY_RELEASE);
						tsp_debug_info(true, &info->client->dev, "[TSP_KEY] BACK %s\n" , key_state != 0 ? "P" : "R");
					}
#ifdef TKEY_BOOSTER
					if (info->tkey_booster && info->tkey_booster->dvfs_set)
						info->tkey_booster->dvfs_set(info->tkey_booster, !!key_state);
#endif

					input_sync(info->input_dev);
				}

				info->tsp_keystatus = input_keys;
			}
			break;
#endif
		case EVENTID_SIDE_TOUCH:
		case EVENTID_SIDE_TOUCH_DEBUG: {
#if defined(FTS_SUPPORT_SIDE_GESTURE) || defined(FTS_SUPPORT_ESD_CHECK)
			unsigned char event_type = data[1 + EventNum * FTS_EVENT_SIZE];
#endif
#ifdef FTS_SUPPORT_SIDE_GESTURE
			if ((event_type == FTS_SIDEGESTURE_EVENT_SINGLE_STROKE) ||
				(event_type == FTS_SIDEGESTURE_EVENT_DOUBLE_STROKE) ||
				(event_type == FTS_SIDEGESTURE_EVENT_INNER_STROKE)) {

				if (info->dt_data->support_sidegesture) {

					int direction, distance;
					direction = data[2 + EventNum * FTS_EVENT_SIZE];
					distance = *(int *)&data[3 + EventNum * FTS_EVENT_SIZE];

					if (direction)
						input_report_key(info->input_dev, KEY_SIDE_GESTURE_RIGHT, 1);
					else
						input_report_key(info->input_dev, KEY_SIDE_GESTURE_LEFT, 1);

					tsp_debug_info(true, &info->client->dev,
						"%s: [Gesture] %02X %02X %02X %02X %02X %02X %02X %02X\n",
						__func__, data[0], data[1], data[2], data[3],
						data[4], data[5], data[6], data[7]);

					input_sync(info->input_dev);
					usleep_range(1000, 1000);

					if (direction)
						input_report_key(info->input_dev, KEY_SIDE_GESTURE_RIGHT, 0);
					else
						input_report_key(info->input_dev, KEY_SIDE_GESTURE_LEFT, 0);

				} else {
					fts_debug_msg_event_handler(info, &data[EventNum * FTS_EVENT_SIZE]);
				}

			}
			else if (event_type == FTS_SIDETOUCH_EVENT_LONG_PRESS) {
				int sideLongPressfingerID = 0;
				sideLongPressfingerID = data[2 + EventNum * FTS_EVENT_SIZE];

				//Todo : event processing
				longpress_release[sideLongPressfingerID - 1] = 1;

				tsp_debug_info(true, &info->client->dev,
					"%s: [Side Long Press]id:%d %02X %02X %02X %02X %02X %02X %02X %02X\n",
					__func__, sideLongPressfingerID, data[0], data[1],
					data[2], data[3], data[4], data[5], data[6], data[7]);
			}
			else
#endif
			{
#ifdef FTS_SUPPORT_ESD_CHECK
			if (event_type == FTS_SIDETOUCH_EVENT_REBOOT_BY_ESD) {
				tsp_debug_info(true, &info->client->dev,
				"%s: [ESD error] %02X %02X %02X %02X %02X %02X %02X %02X\n",
				__func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7]);

				schedule_delayed_work(&info->reset_work, msecs_to_jiffies(10));
			}
			else
#endif
				fts_debug_msg_event_handler(info, &data[EventNum * FTS_EVENT_SIZE]);
			}
			break;}
		case EVENTID_ERROR:
			 /* Get Auto tune fail event */
			if (data[1 + EventNum * FTS_EVENT_SIZE] == 0x08) {
				if (data[2 + EventNum * FTS_EVENT_SIZE] == 0x00)
					dev_info(&info->client->dev, "[FTS] Fail Mutual Auto tune\n");
				else if (data[2 + EventNum * FTS_EVENT_SIZE] == 0x01)
					dev_info(&info->client->dev, "[FTS] Fail Self Auto tune\n");

			} else if (data[1 + EventNum * FTS_EVENT_SIZE] == 0x09) {
				tsp_debug_info(true, &info->client->dev, "[FTS] Fail detect SYNC\n");
			}

			break;

		case EVENTID_HOVER_ENTER_POINTER:
		case EVENTID_HOVER_MOTION_POINTER:
			info->hover_present = true;

			x = ((data[4 + EventNum * FTS_EVENT_SIZE] & 0xF0) >> 4)
			    | ((data[2 + EventNum * FTS_EVENT_SIZE]) << 4);
			y = ((data[4 + EventNum * FTS_EVENT_SIZE] & 0x0F) |
			     ((data[3 + EventNum * FTS_EVENT_SIZE]) << 4));

			z = data[5 + EventNum * FTS_EVENT_SIZE];

			input_mt_slot(info->input_dev, 0);
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER, 1);

			input_report_key(info->input_dev, BTN_TOUCH, 0);
			input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);
#ifdef FTS_SUPPORT_2NDSCREEN_FLAG
			info->SIDE_Flag = 0;
			info->previous_SIDE_value = 0;
			input_report_key(info->input_dev, BTN_SUBSCREEN_FLAG, 0);
#endif
			input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
			input_report_abs(info->input_dev, ABS_MT_DISTANCE, 255 - z);
			break;

		case EVENTID_HOVER_LEAVE_POINTER:
			info->hover_present = false;

			input_mt_slot(info->input_dev, 0);
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER, 0);
			break;

		case EVENTID_ENTER_POINTER:
			if (info->power_state & STATE_LOWPOWER)
				break;

			info->touch_count++;

		case EVENTID_MOTION_POINTER:
			if (info->power_state & STATE_LOWPOWER) {
				dev_err(&info->client->dev, "%s %d: low power mode\n", __func__, __LINE__);
				fts_release_all_finger(info);
				break;
			}

			if (info->touch_count == 0) {
				dev_err(&info->client->dev, "%s %d: count 0\n", __func__, __LINE__);
				fts_release_all_finger(info);
				break;
			}

			if ((EventID == EVENTID_MOTION_POINTER) &&
				(info->finger[TouchID].state == EVENTID_LEAVE_POINTER)) {
				dev_err(&info->client->dev, "%s: state leave but point is moved.\n", __func__);
				break;
			}

			x = data[1 + EventNum * FTS_EVENT_SIZE] +
			    ((data[2 + EventNum * FTS_EVENT_SIZE] &
			      0x0f) << 8);
			y = ((data[2 + EventNum * FTS_EVENT_SIZE] &
			      0xf0) >> 4) + (data[3 +
						  EventNum *
						  FTS_EVENT_SIZE] << 4);
			bw = data[4 + EventNum * FTS_EVENT_SIZE];
			bh = data[5 + EventNum * FTS_EVENT_SIZE];

			palm = (data[6 + EventNum * FTS_EVENT_SIZE] >> 7) & 0x01;
			sumsize = (data[6 + EventNum * FTS_EVENT_SIZE] & 0x7f) << 1;

#ifdef FTS_SUPPORT_2NDSCREEN_FLAG
			currentSideFlag = (data[7 + EventNum * FTS_EVENT_SIZE] >> 7) & 0x01;
			z = data[7 + EventNum * FTS_EVENT_SIZE] & 0x7f;
#else
			z = data[7 + EventNum * FTS_EVENT_SIZE];
#endif

			input_mt_slot(info->input_dev, TouchID);

#ifdef USE_STYLUS_PEN
			/* TODO : need to read finger type in TSP IC. arrange specific register for this. */
			if (info->use_stylus)
				tool_type = finger_type;

			input_mt_report_slot_state(info->input_dev,
							tool_type, 1);
#else
			input_mt_report_slot_state(info->input_dev,
							tool_type,
							1 + (palm << 1));
#endif
			input_report_key(info->input_dev, BTN_TOUCH, 1);
			input_report_key(info->input_dev,
					 BTN_TOOL_FINGER, 1);
			input_report_abs(info->input_dev,
					 ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev,
					 ABS_MT_POSITION_Y, y);

			input_report_abs(info->input_dev,
					 ABS_MT_TOUCH_MAJOR, max(bw, bh));
			input_report_abs(info->input_dev,
					 ABS_MT_TOUCH_MINOR, min(bw, bh));
			input_report_abs(info->input_dev, ABS_MT_PALM,
					 palm);
#if defined(FTS_SUPPORT_SIDE_GESTURE)
			input_report_abs(info->input_dev, ABS_MT_GRIP, 0);
#endif
			if (z == 0)
				input_report_abs(info->input_dev,
						 ABS_MT_PRESSURE, 0x01);
			else
				input_report_abs(info->input_dev,
						 ABS_MT_PRESSURE, z & 0x3F);

			info->finger[TouchID].lx = x;
			info->finger[TouchID].ly = y;

			break;

		case EVENTID_LEAVE_POINTER:
			if (info->power_state & STATE_LOWPOWER)
				break;

			if (info->touch_count <= 0) {
				dev_err(&info->client->dev, "%s %d: count 0\n", __func__, __LINE__);
				fts_release_all_finger(info);
				break;
			}

			info->touch_count--;

			input_mt_slot(info->input_dev, TouchID);

#if defined(FTS_SUPPORT_SIDE_GESTURE)
			if (longpress_release[TouchID] == 1) {
				input_report_abs(info->input_dev, ABS_MT_GRIP, 1);
				dev_info(&info->client->dev, "[FTS] GRIP [%d] %s\n",
						TouchID, longpress_release[TouchID] ? "LONGPRESS" : "RELEASE");
				longpress_release[TouchID] = 0;

				input_sync(info->input_dev);
			}
#endif
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER, 0);

			if (info->touch_count == 0) {
				/* Clear BTN_TOUCH when All touch are released  */
				input_report_key(info->input_dev, BTN_TOUCH, 0);
				input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);

#ifdef FTS_USE_SIDE_SCROLL_FLAG
				input_report_key(info->input_dev, BTN_R_FLICK_FLAG, 0);
				input_report_key(info->input_dev, BTN_L_FLICK_FLAG, 0);
#endif
#ifdef FTS_SUPPORT_2NDSCREEN_FLAG
				info->SIDE_Flag = 0;
				info->previous_SIDE_value = 0;
				input_report_key(info->input_dev, BTN_SUBSCREEN_FLAG, 0);
#endif
#ifdef FTS_SUPPORT_SIDE_GESTURE
				if (info->dt_data->support_sidegesture) {
					input_report_key(info->input_dev, KEY_SIDE_GESTURE, 0);
					input_report_key(info->input_dev, KEY_SIDE_GESTURE_RIGHT, 0);
					input_report_key(info->input_dev, KEY_SIDE_GESTURE_LEFT, 0);
				}
#endif
			}
			break;
		case EVENTID_STATUS_EVENT:
			if (status == STATUS_EVENT_GLOVE_MODE) {
#ifdef CONFIG_GLOVE_TOUCH
				int tm;
				if (data[2 + EventNum * FTS_EVENT_SIZE] == 0x01)
					info->touch_mode = FTS_TM_GLOVE;
				else
					info->touch_mode = FTS_TM_NORMAL;

				tm = info->touch_mode;
				input_report_switch(info->input_dev, SW_GLOVE, tm);

				dev_info(&info->client->dev, "[FTS] GLOVE %s\n", tm ? "PRESS" : "RELEASE");
#endif
			} else if (status == STATUS_EVENT_RAW_DATA_READY) {
				unsigned char regAdd[4] = {0xB0, 0x01, 0x29, 0x01};
				fts_write_reg(info, &regAdd[0], 4);

				info->hover_ready = true;

				dev_info(&info->client->dev,
						"[FTS] Received the Hover Raw Data Ready Event\n");
			} else if (status == STATUS_EVENT_FORCE_CAL_MUTUAL) {
				tsp_debug_dbg(true, &info->client->dev,
						"[FTS] Received Force Calibration Mutual only Event\n");
			} else if (status == STATUS_EVENT_FORCE_CAL_SELF) {
				tsp_debug_dbg(true, &info->client->dev,
						"[FTS] Received Force Calibration Self only Event\n");
			} else if (status == STATUS_EVENT_WATERMODE_ON) {
				tsp_debug_info(true, &info->client->dev,
						"[FTS] Received Water Mode On Event\n");
			} else if (status == STATUS_EVENT_WATERMODE_OFF) {
				tsp_debug_info(true, &info->client->dev,
						"[FTS] Received Water Mode Off Event\n");
			} else if (status == STATUS_EVENT_MUTUAL_CAL_FRAME_CHECK) {
				tsp_debug_info(true, &info->client->dev,
						"[FTS] Received Mutual Calib Frame Check Event\n");
			} else if (status == STATUS_EVENT_SELF_CAL_FRAME_CHECK) {
				tsp_debug_info(true, &info->client->dev,
						"[FTS] Received Self Calib Frame Check Event\n");

			} else {
				fts_debug_msg_event_handler(info,
						  &data[EventNum *
							FTS_EVENT_SIZE]);
			}
			break;

#ifdef SEC_TSP_FACTORY_TEST
		case EVENTID_RESULT_READ_REGISTER:
			procedure_cmd_event(info, &data[EventNum * FTS_EVENT_SIZE]);
			break;
#endif


#ifdef FTS_USE_SIDE_SCROLL_FLAG
		case EVENTID_SIDE_SCROLL_FLAG:
			scroll_flag = data[3 + EventNum * FTS_EVENT_SIZE];
			scroll_thr  = data[6 + EventNum * FTS_EVENT_SIZE];
			dev_info(&info->client->dev,
					"[TB] side scroll flag: event: %02X, thr: %02X\n",
					scroll_flag, scroll_thr);

             // TODO : Report function call this area

			if (scroll_flag == 1) {
				input_report_key(info->input_dev, BTN_R_FLICK_FLAG, 1);
				input_report_key(info->input_dev, BTN_L_FLICK_FLAG, 0);
				input_sync(info->input_dev);

			} else if (scroll_flag == 2) {
				input_report_key(info->input_dev, BTN_R_FLICK_FLAG, 0);
				input_report_key(info->input_dev, BTN_L_FLICK_FLAG, 1);
				input_sync(info->input_dev);

			}
			break;
#endif

		case EVENTID_FROM_STRING:
			string_addr = FTS_CMD_STRING_ACCESS;
			fts_read_from_string(info, &string_addr, string_pi, sizeof(string_pi));

			string_addr = FTS_CMD_STRING_ACCESS + *(unsigned short *)&string_pi[STRING_OFFSET_REGISTER];

			fts_read_from_string(info, &string_addr, string_data, sizeof(string_data));

			dev_info(&info->client->dev,
					"%s: [String] %02X %02X %02X %02X %02X %02X %02X %02X || %04X: %02X, %02X\n",
					__func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7],
					string_addr, string_data[0], string_data[1]);

			switch (string_data[1]) {
			case FTS_STRING_EVENT_SPAY:
			case FTS_STRING_EVENT_SPAY1:
			case FTS_STRING_EVENT_SPAY2:
				dev_info(&info->client->dev, "%s: SPAY[%X]\n", __func__, string_data[1]);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				info->scrub_id = (string_data[1] >> 2) & 0xF;
				break;
			default:
				dev_info(&info->client->dev, "%s: no event:%X\n", __func__, string_data[1]);
				break;
			}

			input_sync(info->input_dev);

			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);

			break;

		default:
			fts_debug_msg_event_handler(info, &data[EventNum * FTS_EVENT_SIZE]);
			continue;
		}
#ifdef FTS_SUPPORT_2NDSCREEN_FLAG
		if (currentSideFlag != info->previous_SIDE_value) {

			dev_info(&info->client->dev, "[TB] 2nd screen flag was changed, old:%d c:%d f:%d\n",
					info->previous_SIDE_value, currentSideFlag, info->SIDE_Flag);
			info->SIDE_Flag = currentSideFlag;
			// TODO : Report function call this area

			input_report_key(info->input_dev, BTN_SUBSCREEN_FLAG, !(!(info->SIDE_Flag)));
		}
		info->previous_SIDE_value = currentSideFlag;
#endif

//		input_sync(info->input_dev);
//TEST
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		if (EventID == EVENTID_ENTER_POINTER)
			dev_err(&info->client->dev,
				"[P] tID:%d x:%d y:%d w:%d h:%d z:%d s:%d p:%d tc:%d tm:%d\n",
				TouchID, x, y, bw, bh, z, sumsize, palm, info->touch_count, info->touch_mode);
		else if (EventID == EVENTID_HOVER_ENTER_POINTER)
			dev_err(&info->client->dev,
				"[HP] tID:%d x:%d y:%d z:%d\n",
				TouchID, x, y, z);
#else
		if (EventID == EVENTID_ENTER_POINTER)
			dev_err(&info->client->dev,
				"[P] tID:%d loc:%c%c tc:%d tm:%d\n",
				TouchID,
				location_detect(info, y, 1), location_detect(info, x, 0),
				info->touch_count, info->touch_mode);
		else if (EventID == EVENTID_HOVER_ENTER_POINTER)
			dev_err(&info->client->dev,
				"[HP] tID:%d\n", TouchID);
#endif
		else if (EventID == EVENTID_LEAVE_POINTER) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			dev_err(&info->client->dev,
				"[R] tID:%d mc: %d tc:%d lx:%d ly:%d Ver[%02X%04X%01X%01X%01X]\n",
				TouchID, info->finger[TouchID].mcount, info->touch_count,
				info->finger[TouchID].lx, info->finger[TouchID].ly,
				info->panel_revision, info->fw_main_version_of_ic,
				info->flip_enable, info->mshover_enabled, info->mainscr_disable);
#else
			dev_err(&info->client->dev,
				"[R] tID:%d loc:%c%c mc: %d tc:%d Ver[%02X%04X%01X%01X%01X]\n",
				TouchID,
				location_detect(info, info->finger[TouchID].ly, 1),
				location_detect(info, info->finger[TouchID].lx, 0),
				info->finger[TouchID].mcount, info->touch_count,
				info->panel_revision, info->fw_main_version_of_ic,
				info->flip_enable, info->mshover_enabled, info->mainscr_disable);
#endif
			info->finger[TouchID].mcount = 0;
		}/* else if (EventID == EVENTID_HOVER_LEAVE_POINTER) {
			if (info->hover_present) {
				dev_info(&info->client->dev,
					"[HR] tID:%d Ver[%02X%04X%01X%01X] ++\n",
					TouchID,
					info->panel_revision, info->fw_main_version_of_ic,
					info->flip_enable, info->mshover_enabled);
				info->finger[TouchID].mcount = 0;
				info->hover_present = false;
			}
		}*/ else if (EventID == EVENTID_MOTION_POINTER) {
			info->finger[TouchID].mcount++;
		}

		if ((EventID == EVENTID_ENTER_POINTER) ||
			(EventID == EVENTID_MOTION_POINTER) ||
			(EventID == EVENTID_LEAVE_POINTER))
			info->finger[TouchID].state = EventID;
	}

	input_sync(info->input_dev);
//TEST
#ifdef TSP_BOOSTER
	if (EventID == EVENTID_ENTER_POINTER || EventID == EVENTID_LEAVE_POINTER)
		if (info->tsp_booster && info->tsp_booster->dvfs_set)
			info->tsp_booster->dvfs_set(info->tsp_booster, info->touch_count);
#endif
	return LastLeftEvent;
}

#ifdef FTS_SUPPORT_TA_MODE
static void fts_ta_cb(struct fts_callbacks *cb, int ta_status)
{
	struct fts_ts_info *info =
	    container_of(cb, struct fts_ts_info, callbacks);

	if (ta_status == 0x01 || ta_status == 0x03) {
		fts_command(info, FTS_CMD_CHARGER_PLUGGED);
		info->TA_Pluged = true;
		dev_info(&info->client->dev,
			 "%s: device_control : CHARGER CONNECTED, ta_status : %x\n",
			 __func__, ta_status);
	} else {
		fts_command(info, FTS_CMD_CHARGER_UNPLUGGED);
		info->TA_Pluged = false;
		dev_info(&info->client->dev,
			 "%s: device_control : CHARGER DISCONNECTED, ta_status : %x\n",
			 __func__, ta_status);
	}
}
#endif

#if defined(CONFIG_SECURE_TOUCH)
static void fts_secure_touch_notify(struct fts_ts_info *info)
{
	dev_err(&info->client->dev, "%s: SECURETOUCH_NOTIFY", __func__);
	sysfs_notify(&info->input_dev->dev.kobj, NULL, "secure_touch");
}

static irqreturn_t fts_filter_interrupt(struct fts_ts_info *info)
{
/*
	dev_err(&info->client->dev, "%s: SECURETOUCH_cmpxchg st_pending_irqs=%d",
							__func__, atomic_read(&info->st_pending_irqs));
*/
	if (atomic_read(&info->st_enabled)) {
		if (atomic_cmpxchg(&info->st_pending_irqs, 0, 1) == 0) {
			dev_err(&info->client->dev, "%s: SECURETOUCH_cmpxchg", __func__);
			dev_err(&info->client->dev, "%s: SECURETOUCH_cmpxchg st_pending_irqs=%d",
								__func__, atomic_read(&info->st_pending_irqs));
			fts_secure_touch_notify(info);
		}
		dev_err(&info->client->dev, "%s: SECURETOUCH_FILTER_INTR", __func__);
		return IRQ_HANDLED;
	}
/*
	dev_err(&info->client->dev, "%s: SECURETOUCH_FILTER_INTR_irq-none", __func__);
*/
	return IRQ_NONE;
}
#endif


/**
 * fts_interrupt_handler()
 *
 * Called by the kernel when an interrupt occurs (when the sensor
 * asserts the attention irq).
 *
 * This function is the ISR thread and handles the acquisition
 * and the reporting of finger data when the presence of fingers
 * is detected.
 */
static irqreturn_t fts_interrupt_handler(int irq, void *handle)
{
	struct fts_ts_info *info = handle;
	unsigned char regAdd[4] = {0xb6, 0x00, 0x45, READ_ALL_EVENT};
	unsigned short evtcount = 0;
	int ret;

#if defined(CONFIG_SECURE_TOUCH)
	if (IRQ_HANDLED == fts_filter_interrupt(info)) {
#ifdef ST_INT_COMPLETE
		ret = wait_for_completion_interruptible_timeout((&info->st_interrupt),
					msecs_to_jiffies(10 * MSEC_PER_SEC));
		dev_err(&info->client->dev, "%s: SECURETOUCH_IRQ_HANDLED, ret:%d",
					__func__, ret);
#else
		dev_err(&info->client->dev, "%s: SECURETOUCH_IRQ_HANDLED",
					__func__);
#endif
		goto out;
	}
#endif

	evtcount = 0;

	if (info->lowpower_mode) {
		if (info->power_state == STATE_LOWPOWER_SUSPEND) {

			wake_lock_timeout(&info->wakelock, 4 * HZ);

			dev_dbg(&info->client->dev, "%s: PM_SUSPEND\n", __func__);

			ret = wait_for_completion_interruptible_timeout((&info->resume_done),
						msecs_to_jiffies(1 * MSEC_PER_SEC));
			dev_dbg(&info->client->dev, "%s: PM_RESUMED, ret:%d\n", __func__, ret);

			if (ret == 0)
				return IRQ_HANDLED;

		}
	}
	fts_read_reg(info, &regAdd[0], 3, (unsigned char *)&evtcount, 2);
	evtcount = evtcount >> 10;

	if (evtcount > FTS_FIFO_MAX)
		evtcount = FTS_FIFO_MAX;

	if (evtcount > 0) {
		memset(info->data, 0x0, FTS_EVENT_SIZE * evtcount);
		fts_read_reg(info, &regAdd[3], 1, (unsigned char *)info->data,
				  FTS_EVENT_SIZE * evtcount);
		fts_event_handler_type_b(info, info->data, evtcount);
	} else {
		dev_info(&info->client->dev, "%s: No event[%02X]\n",
				__func__, evtcount);
	}
#if defined(CONFIG_SECURE_TOUCH)
out:
#endif
	return IRQ_HANDLED;
}

static int fts_irq_enable(struct fts_ts_info *info,
		bool enable)
{
	int retval = 0;

	if (enable) {
		if (info->irq_enabled)
			return retval;

		retval = request_threaded_irq(info->irq, NULL,
				fts_interrupt_handler, IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				FTS_TS_DRV_NAME, info);
		if (retval < 0) {
			dev_info(&info->client->dev,
					"%s: Failed to create irq thread %d\n",
					__func__, retval);
			return retval;
		}

		info->irq_enabled = true;
	} else {
		if (info->irq_enabled) {
			disable_irq(info->irq);
			free_irq(info->irq, info);
			info->irq_enabled = false;
		}
	}

	return retval;
}

#ifdef FTS_SUPPORT_TOUCH_KEY
static int fts_led_power_ctrl(struct fts_ts_info *info, bool on)
{
	int ret = 0;

	dev_info(&info->client->dev, "%s: vdd_keyled %s\n", __func__, on ? "ON" : "OFF");

	if (!info->keyled_vreg) {
		dev_info(&info->client->dev, "%s: failed vdd_keyled is null\n", __func__);
	} else {
		if (on) {
			if (regulator_is_enabled(info->keyled_vreg)) {
				dev_info(&info->client->dev, "vdd_keyled already enabled\n");
			} else {
				ret = regulator_enable(info->keyled_vreg);
				if (ret) {
					dev_err(&info->client->dev, "vdd_keyled enable failed rc=%d\n", ret);
					return 2;
				}
			}
		} else {
			if (regulator_is_enabled(info->keyled_vreg)) {
				ret = regulator_disable(info->keyled_vreg);
				if (ret) {
					dev_err(&info->client->dev, "vdd_keyled disable failed rc=%d\n", ret);
					return 3;
				}
			} else {
				dev_info(&info->client->dev, "vdd_keyled already disabled\n");
			}
		}
	}

	return ret;
}
#endif

static int fts_power_ctrl(struct fts_ts_info *info, bool enable)
{
	struct device *dev = &info->client->dev;
	int retval;
	static struct regulator *avdd, *vddo;

	if (!avdd) {
		avdd = devm_regulator_get(&info->client->dev, "avdd");

		if (IS_ERR(avdd)) {
			dev_err(dev, "%s: could not get avdd, rc = %ld\n",
				__func__, PTR_ERR(avdd));
			avdd = NULL;
		}
		if (!IS_ERR_OR_NULL(avdd)) {
			retval = regulator_set_voltage(avdd, 3300000, 3300000);
			if (retval)
				dev_err(dev, "%s: unable to set avdd voltage to 3.3V\n",
					__func__);

		}
	}

	if (!vddo) {
		vddo = devm_regulator_get(&info->client->dev, "vddo");

		if (IS_ERR(vddo)) {
			dev_err(dev, "%s: could not get vddo, rc = %ld\n",
				__func__, PTR_ERR(vddo));
			vddo = NULL;
		}

		if (!IS_ERR_OR_NULL(vddo)) {
			retval = regulator_set_voltage(vddo, 1800000, 1800000);
			if (retval)
				dev_err(dev, "%s: unable to set vddo voltage to 1.8V\n",
					__func__);
		}

	}

	if (enable) {
		if (0/*regulator_is_enabled(avdd)*/) {
			dev_err(dev, "%s: avdd is already enabled\n", __func__);
		} else {
			if (!IS_ERR_OR_NULL(avdd)) {
				retval = regulator_enable(avdd);
				if (retval) {
					dev_err(dev, "%s: Fail to enable regulator avdd[%d]\n",
						__func__, retval);
					goto err;
				}
			}
		}

		if (0/*regulator_is_enabled(vddo)*/) {
			dev_err(dev, "%s: vddo is already enabled\n", __func__);
		} else {
			if (!IS_ERR_OR_NULL(vddo)) {
				retval = regulator_enable(vddo);
				if (retval) {
					dev_err(dev, "%s: Fail to enable regulator vddo[%d]\n",
						__func__, retval);
					goto err;
				}
			}
		}
	} else {
		if (!IS_ERR_OR_NULL(vddo)) {
			if (regulator_is_enabled(vddo)) {
				retval = regulator_disable(vddo);
				if (retval) {
					dev_err(dev, "%s: Fail to disable regulator vddo[%d]\n",
						__func__, retval);
					goto err;
				}
			}
		}
		if (!IS_ERR_OR_NULL(avdd)) {
			if (regulator_is_enabled(avdd)) {
				retval = regulator_disable(avdd);
				if (retval) {
					dev_err(dev, "%s: Fail to disable regulator avdd[%d]\n",
						__func__, retval);
					goto err;
				}
			}
		}
	}

	if (info->dt_data->external_ldo > 0) {
		retval = gpio_direction_output(info->dt_data->external_ldo, enable);
		dev_info(dev, "%s: sensor_en[3.3V][%d] is %s[%s]\n",
				__func__, info->dt_data->external_ldo,
				enable ? "enabled" : "disabled", (retval < 0) ? "NG" : "OK");
	}

	dev_err(dev, "%s: [%s] avdd %s, vddo %s\n", __func__, enable ? "ON" : "OFF",
			IS_ERR_OR_NULL(avdd) ? "NULL" : regulator_is_enabled(avdd) ? "enabled" : "disabled",
			IS_ERR_OR_NULL(vddo) ? "NULL" : regulator_is_enabled(vddo) ? "enabled" : "disabled");

	return 0;

err:
	if (enable) {
		if (info->dt_data->external_ldo > 0) {
			retval = gpio_direction_output(info->dt_data->external_ldo, enable);
			dev_err(dev, "%s: sensor_en[3.3V][%d] is disabled[%s]\n",
					__func__, info->dt_data->external_ldo,
					(retval < 0) ? "NG" : "OK");
		}
	} else {
		if (info->dt_data->external_ldo > 0) {
			retval = gpio_direction_output(info->dt_data->external_ldo, enable);
			dev_err(dev, "%s: sensor_en[3.3V][%d] is enabled[%s]\n",
					__func__, info->dt_data->external_ldo,
					(retval < 0) ? "NG" : "OK");
		}
	}

	return retval;
}

#if defined(CONFIG_SECURE_TOUCH)
static void fts_secure_touch_init(struct fts_ts_info *info)
{
	int ret;

	dev_err(&info->client->dev, "%s: SECURETOUCH_INIT\n", __func__);
	init_completion(&info->st_powerdown);
#ifdef ST_INT_COMPLETE
	init_completion(&info->st_interrupt);
#endif
	info->core_clk = clk_get(&info->client->dev, "core_clk");
	if (IS_ERR(info->core_clk)) {
		ret = PTR_ERR(info->core_clk);
		dev_err(&info->client->dev, "%s: error on clk_get(core_clk):%d\n",
			__func__, ret);
		return;
	}

	info->iface_clk = clk_get(&info->client->dev, "iface_clk");
	if (IS_ERR(info->iface_clk)) {
		ret = PTR_ERR(info->core_clk);
		dev_err(&info->client->dev, "%s: error on clk_get(iface_clk):%d\n",
			__func__, ret);
		goto err_iface_clk;
	}

	return;

err_iface_clk:
	clk_put(info->core_clk);
	info->core_clk = NULL;
}
#endif

#ifdef CONFIG_OF
static int fts_parse_dt(struct device *dev,
		struct fts_device_tree_data *dt_data)
{
	struct device_node *np = dev->of_node;
	int rc;
	u32 coords[2];

	/* vdd, irq gpio info */
	dt_data->external_ldo = of_get_named_gpio(np, "stm,external_ldo", 0);
	dt_data->irq_gpio = of_get_named_gpio(np, "stm,irq-gpio", 0);

	rc = of_property_read_u32_array(np, "stm,tsp-coords", coords, 2);
	if (rc < 0) {
		dev_info(dev, "%s: Unable to read stm,tsp-coords\n", __func__);
		return rc;
	}

	dt_data->max_x = coords[0];
	dt_data->max_y = coords[1];

#ifdef FTS_SUPPORT_TOUCH_KEY
	dt_data->support_mskey = 1;
	dt_data->num_touchkey = 2;
	dt_data->touchkey = fts_touchkeys;
#endif

	/* project info */
	rc = of_property_read_string(np, "stm,tsp-project", &dt_data->project);
	if (rc < 0) {
		dev_info(dev, "%s: Unable to read stm,tsp-project\n", __func__);
		dt_data->project = "0";
	}

	/* model info */
	rc = of_property_read_string(np, "stm,tsp-model", &dt_data->model);
	if (rc < 0) {
		dev_info(dev, "%s: Unable to read stm,tsp-model\n", __func__);
		dt_data->model = FTS_DEVICE_NAME;
	}

	of_property_read_u32(np, "stm,support_gesture", &dt_data->support_sidegesture);

	rc = of_property_read_u32(np, "stm,grip_area", &dt_data->grip_area);
	if (rc < 0)
		dt_data->grip_area = -1;

	if (of_find_property(np, "stm,string-lib", NULL))
		dt_data->support_string_lib = FTS_ENABLE;
	else
		dt_data->support_string_lib = FTS_DISABLE;

	rc = of_property_read_u32(np, "stm,bringup", &dt_data->bringup);
	if (rc < 0)
		dt_data->bringup = 0;

	dev_err(dev, "%s: power= %d, tsp_int= %d, X= %d, Y= %d, grip_area= %d, project= %s, gesture[%d], string[%d], bringup[%d]\n",
		__func__, dt_data->external_ldo, dt_data->irq_gpio,
			dt_data->max_x, dt_data->max_y, dt_data->grip_area, dt_data->project,
			dt_data->support_sidegesture, dt_data->support_string_lib, dt_data->bringup);

	return 0;
}
#else
static int fts_parse_dt(struct device *dev,
		struct fts_device_tree_data *dt_data)
{
	return -ENODEV;
}
#endif

static void fts_request_gpio(struct fts_ts_info *info)
{
	int ret;
	dev_info(&info->client->dev, "%s\n", __func__);

	ret = gpio_request(info->dt_data->irq_gpio, "stm,irq_gpio");
	if (ret) {
		pr_err("%s: unable to request irq_gpio [%d]\n",
				__func__, info->dt_data->irq_gpio);
		return;
	}

	if (info->dt_data->external_ldo > 0) {
		ret = gpio_request(info->dt_data->external_ldo, "stm,external_ldo");
		if (ret) {
			dev_err(&info->client->dev, "%s: unable to request external_ldo [%d]\n",
					__func__, info->dt_data->external_ldo);
			return;
		}
	}

#ifdef FTS_SUPPORT_TOUCH_KEY
	dev_info(&info->client->dev, "%s: vdd_keyled setting, %d\n", __func__, __LINE__);

	info->keyled_vreg = regulator_get(&info->client->dev, "vdd_keyled");
	if (IS_ERR(info->keyled_vreg)) {
		pr_err("%s: failed vdd_keyled request error \n", __func__);
		info->keyled_vreg = NULL;
		return;
	}

	ret = regulator_set_voltage(info->keyled_vreg, 3300000, 3300000);
	if (ret)
		pr_err("%s: vdd_keyled 3.3V setting error\n", __func__);

#endif

}

static int fts_probe(struct i2c_client *client, const struct i2c_device_id *idp)
{
	int retval;
	struct fts_ts_info *info = NULL;
	struct fts_device_tree_data *dt_data = NULL;
	static char fts_ts_phys[64] = { 0 };
	int i = 0;
	int temp;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "FTS err = EIO!\n");
		return -EIO;
	}

	if (client->dev.of_node) {
		dt_data = devm_kzalloc(&client->dev,
				sizeof(struct fts_device_tree_data),
				GFP_KERNEL);
		if (!dt_data) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		retval = fts_parse_dt(&client->dev, dt_data);
		if (retval)
			return retval;
	} else {
		dt_data = client->dev.platform_data;
		dev_err(&client->dev, "%s: TSP failed to align dtsi\n", __func__);
	}

	info = kzalloc(sizeof(struct fts_ts_info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "FTS err = ENOMEM!\n");
		return -ENOMEM;
	}

#ifdef FTS_SUPPORT_NOISE_PARAM
	info->noise_param = kzalloc(sizeof(struct fts_noise_param), GFP_KERNEL);
	if (!info->noise_param) {
		dev_err(&info->client->dev, "%s: Failed to set noise param mem\n",
				__func__);
		retval = -ENOMEM;
		goto err_alloc_noise_param;
	}
#endif

	info->client = client;
	info->dt_data = dt_data;

#ifdef USE_OPEN_DWORK
	INIT_DELAYED_WORK(&info->open_work, fts_open_work);
#endif

	info->delay_time = 300;

#if defined(USE_RESET_WORK_EXIT_LOWPOWERMODE) || defined(FTS_SUPPORT_ESD_CHECK)
	INIT_DELAYED_WORK(&info->reset_work, fts_reset_work);
#endif

#ifdef TSP_RAWDATA_DUMP
	INIT_DELAYED_WORK(&info->ghost_check, ghost_touch_check);
	p_ghost_check = &info->ghost_check;
#endif

	fts_request_gpio(info);

	retval = fts_power_ctrl(info, true);
	if (retval) {
		dev_err(&client->dev,
			"%s: Failed to power on: %d\n", __func__, retval);
		goto err_power_on;
	}
	usleep_range(10 * 1000, 10 * 1000);

#ifdef TSP_BOOSTER
	info->tsp_booster = input_booster_allocate(INPUT_BOOSTER_ID_TSP);
	if (!info->tsp_booster) {
		dev_err(&client->dev,
			"%s: Failed to alloc mem for tsp_booster\n", __func__);
		goto err_get_tsp_booster;
	}
#endif
#ifdef TKEY_BOOSTER
	info->tkey_booster = input_booster_allocate(INPUT_BOOSTER_ID_TKEY);
	if (!info->tkey_booster) {
		dev_err(&client->dev,
			"%s: Failed to alloc mem for tkey_booster\n", __func__);
		goto err_get_tkey_booster;
	}
#endif

	info->dev = &info->client->dev;
	info->input_dev = input_allocate_device();
	if (!info->input_dev) {
		dev_info(&info->client->dev, "FTS err = ENOMEM!\n");
		retval = -ENOMEM;
		goto err_input_allocate_device;
	}

	info->input_dev->dev.parent = &client->dev;
	info->input_dev->name = "sec_touchscreen";
	snprintf(fts_ts_phys, sizeof(fts_ts_phys), "%s/input0",
		 info->input_dev->name);
	info->input_dev->phys = fts_ts_phys;
	info->input_dev->id.bustype = BUS_I2C;

	info->irq = client->irq = gpio_to_irq(info->dt_data->irq_gpio);
	dev_err(&info->client->dev, "%s: # irq : %d, gpio_to_irq[%d]\n",
			__func__, info->irq, info->dt_data->irq_gpio);

	if (info->dt_data->bringup)
		temp = 0x0;
	else
		temp = get_lcd_attached("GET");

	info->lcd_id[2] = temp & 0xFF;
	info->lcd_id[1] = (temp >> 8) & 0xFF;
	info->lcd_id[0] = (temp >> 16) & 0xFF;
	info->panel_revision = info->lcd_id[1] >> 4;

	dev_err(&info->client->dev, "%s: lcd id : %06X[%02X][%02X][%02X]\n",
			__func__, temp, info->lcd_id[0],
			info->lcd_id[1], info->lcd_id[2]);
	info->panel_revision = (info->lcd_id[1] >> 4) & 0xF;

	info->irq_enabled = false;
	info->touch_stopped = false;
	info->stop_device = fts_stop_device;
	info->start_device = fts_start_device;
	info->fts_delay = fts_delay;
	info->fts_command = fts_command;
	info->fts_read_reg = fts_read_reg;
	info->fts_write_reg = fts_write_reg;
	info->fts_systemreset = fts_systemreset;
	info->fts_get_version_info = fts_get_version_info;
	info->fts_wait_for_ready = fts_wait_for_ready;
	info->fts_power_ctrl = fts_power_ctrl;
	info->fts_write_to_string = fts_write_to_string;
	info->fts_read_from_string = fts_read_from_string;
	info->fts_change_scan_rate = fts_change_scan_rate;
#ifdef FTS_SUPPORT_TOUCH_KEY
	info->led_power = fts_led_power_ctrl;
#endif
#ifdef FTS_SUPPORT_NOISE_PARAM
	info->fts_get_noise_param_address = fts_get_noise_param_address;
#endif

#ifdef FTS_SUPPORT_QEEXO_ROI
	info->get_fts_roi = fts_get_roi_address;
#endif

#ifdef USE_OPEN_CLOSE
	info->input_dev->open = fts_input_open;
	info->input_dev->close = fts_input_close;
#endif

#ifdef CONFIG_GLOVE_TOUCH
	input_set_capability(info->input_dev, EV_SW, SW_GLOVE);
#endif
	set_bit(EV_SYN, info->input_dev->evbit);
	set_bit(EV_KEY, info->input_dev->evbit);
	set_bit(EV_ABS, info->input_dev->evbit);
#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, info->input_dev->propbit);
#endif
	set_bit(BTN_TOUCH, info->input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, info->input_dev->keybit);
#ifdef FTS_SUPPORT_2NDSCREEN_FLAG
	set_bit(BTN_SUBSCREEN_FLAG, info->input_dev->keybit);
#endif
#ifdef FTS_USE_SIDE_SCROLL_FLAG
	set_bit(BTN_R_FLICK_FLAG, info->input_dev->keybit);
	set_bit(BTN_L_FLICK_FLAG, info->input_dev->keybit);
#endif
#ifdef USE_STYLUS
	input_set_abs_params(info->input_dev,
				ABS_MT_TOOL_TYPE, 0,
				MT_TOOL_MAX, 0, 0);
#endif

	set_bit(KEY_BLACK_UI_GESTURE, info->input_dev->keybit);
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey) {
		for (i = 0 ; i < info->dt_data->num_touchkey ; i++)
			set_bit(info->dt_data->touchkey[i].keycode, info->input_dev->keybit);

		set_bit(EV_LED, info->input_dev->evbit);
		set_bit(LED_MISC, info->input_dev->ledbit);
	}
#endif
#ifdef FTS_SUPPORT_SIDE_GESTURE
	if (info->dt_data->support_sidegesture) {
		set_bit(KEY_SIDE_GESTURE, info->input_dev->keybit);
		set_bit(KEY_SIDE_GESTURE_RIGHT, info->input_dev->keybit);
		set_bit(KEY_SIDE_GESTURE_LEFT, info->input_dev->keybit);
	}
#endif

	input_mt_init_slots(info->input_dev, FINGER_MAX, INPUT_MT_DIRECT);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
			0, info->dt_data->max_x, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
			0, info->dt_data->max_y, 0, 0);
#if defined(CONFIG_SEC_FACTORY)
	input_set_abs_params(info->input_dev, ABS_MT_PRESSURE,
				 0, 0x3F, 0, 0);
#endif

	mutex_init(&info->lock);
	mutex_init(&(info->device_mutex));
	mutex_init(&info->i2c_mutex);
	wake_lock_init(&info->wakelock, WAKE_LOCK_SUSPEND, "fts_wakelock");

	info->enabled = false;

	mutex_lock(&info->lock);
	retval = fts_init(info);
	mutex_unlock(&info->lock);

	info->reinit_done = true;

	if (retval < 0) {
		dev_err(&info->client->dev, "FTS fts_init fail!\n");
		goto err_fts_init;
	}

	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR,
				 0, 255, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MINOR,
				 0, 255, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_PALM, 0, 1, 0, 0);
#if defined(FTS_SUPPORT_SIDE_GESTURE)
	input_set_abs_params(info->input_dev, ABS_MT_GRIP, 0, 1, 0, 0);
#endif
	input_set_abs_params(info->input_dev, ABS_MT_DISTANCE,
				 0, 255, 0, 0);

	input_set_drvdata(info->input_dev, info);
	i2c_set_clientdata(client, info);

	retval = input_register_device(info->input_dev);
	if (retval) {
		dev_err(&info->client->dev, "FTS input_register_device fail!\n");
		goto err_register_input;
	}

	for (i = 0; i < FINGER_MAX; i++) {
		info->finger[i].state = EVENTID_LEAVE_POINTER;
		info->finger[i].mcount = 0;
	}
	info->enabled = true;

	retval = fts_irq_enable(info, true);
	if (retval < 0) {
		dev_info(&info->client->dev,
						"%s: Failed to enable attention interrupt\n",
						__func__);
		goto err_enable_irq;
	}

#ifdef FTS_SUPPORT_TA_MODE
	info->register_cb = info->register_cb;

	info->callbacks.inform_charger = fts_ta_cb;
	if (info->register_cb)
		info->register_cb(&info->callbacks);
#endif

#ifdef SEC_TSP_FACTORY_TEST
	retval = sec_cmd_init(&info->sec, stm_ft_cmds,
			ARRAY_SIZE(stm_ft_cmds), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		tsp_debug_err(true, &info->client->dev,
			"%s: Failed to sec_cmd_init\n", __func__);
		return -ENODEV;
	}

	retval = sysfs_create_group(&info->sec.fac_dev->kobj,
				 &sec_touch_factory_attr_group);
	if (retval < 0) {
		tsp_debug_err(true, &info->client->dev,
			"%s: FTS Failed to create sysfs group\n", __func__);
		goto err_sysfs;
	}
#endif

	retval = sysfs_create_link(&info->sec.fac_dev->kobj,
		&info->input_dev->dev.kobj, "input");
	if (retval < 0) {
		tsp_debug_err(true, &info->client->dev,
			"%s: Failed to create input symbolic link\n",
			__func__);
	}

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey) {
		info->fac_dev_tk = device_create(sec_class, NULL, 0, info, "sec_touchkey");
		if (IS_ERR(info->fac_dev_tk))
			tsp_debug_err(true, &info->client->dev, "Failed to create device for the touchkey sysfs\n");
		else {
			dev_set_drvdata(info->fac_dev_tk, info);

			retval = sysfs_create_group(&info->fac_dev_tk->kobj,
						 &sec_touchkey_factory_attr_group);
			if (retval < 0) {
				tsp_debug_err(true, &info->client->dev, "FTS Failed to create sysfs group\n");
			} else {
				retval = sysfs_create_link(&info->fac_dev_tk->kobj,
							&info->input_dev->dev.kobj, "input");

				if (retval < 0)
					tsp_debug_err(true, &info->client->dev,
							"%s: Failed to create link\n", __func__);
			}
		}
	}
#endif

#if defined(CONFIG_SECURE_TOUCH)
	for (i = 0; i < ARRAY_SIZE(attrs); i++) {
		retval = sysfs_create_file(&info->input_dev->dev.kobj,
				&attrs[i].attr);
		if (retval < 0) {
			dev_err(&info->client->dev,
				"%s: Failed to create sysfs attributes\n",
				__func__);
		}
	}

	fts_secure_touch_init(info);
#endif

	device_init_wakeup(&client->dev, 1);

	dev_err(&info->client->dev, "%s done\n", __func__);

	return 0;

#ifdef SEC_TSP_FACTORY_TEST
	sysfs_remove_group(&info->sec.fac_dev->kobj,
			   &sec_touch_factory_attr_group);
err_sysfs:
	sec_cmd_exit(&info->sec, SEC_CLASS_DEVT_TSP);
#endif
	if (info->irq_enabled)
		fts_irq_enable(info, false);

err_enable_irq:
	input_unregister_device(info->input_dev);
	info->input_dev = NULL;
 err_register_input:
	if (info->input_dev)
		input_free_device(info->input_dev);

err_fts_init:
#ifdef SEC_TSP_FACTORY_TEST
	kfree(info->cx_data);
	kfree(info->pFrame);
#endif
	wake_lock_destroy(&info->wakelock);
err_input_allocate_device:
#ifdef TSP_BOOSTER
	input_booster_free(info->tsp_booster);
	info->tsp_booster = NULL;
err_get_tsp_booster:
#endif
#ifdef TKEY_BOOSTER
	input_booster_free(info->tkey_booster);
	info->tkey_booster = NULL;
err_get_tkey_booster:
#endif
	fts_power_ctrl(info, false);
err_power_on:
#ifdef TSP_RAWDATA_DUMP
	p_ghost_check = NULL;
#endif
	kfree(info->noise_param);
err_alloc_noise_param:
	kfree(info);

	pr_err("[TSP] %s failed, rc=%d\n", __func__, retval);

	return retval;
}

static int fts_remove(struct i2c_client *client)
{
	struct fts_ts_info *info = i2c_get_clientdata(client);
#if defined(CONFIG_SECURE_TOUCH)
	int i = 0;
#endif
	dev_info(&info->client->dev, "FTS removed\n");

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, FLUSHBUFFER);

	fts_irq_enable(info, false);

#ifdef TSP_RAWDATA_DUMP
	p_ghost_check = NULL;
#endif

#if defined(CONFIG_SECURE_TOUCH)
	for (i = 0; i < ARRAY_SIZE(attrs); i++) {
		sysfs_remove_file(&info->input_dev->dev.kobj,
				&attrs[i].attr);
	}
#endif
	input_mt_destroy_slots(info->input_dev);

#ifdef SEC_TSP_FACTORY_TEST
	sysfs_remove_group(&info->sec.fac_dev->kobj,
			   &sec_touch_factory_attr_group);
	sec_cmd_exit(&info->sec, SEC_CLASS_DEVT_TSP);

	kfree(info->cx_data);
	kfree(info->pFrame);
#endif

	mutex_destroy(&info->lock);

	wake_lock_destroy(&info->wakelock);

	input_unregister_device(info->input_dev);
	info->input_dev = NULL;
#ifdef TSP_BOOSTER
	input_booster_free(info->tsp_booster);
	info->tsp_booster = NULL;
#endif
#ifdef TKEY_BOOSTER
	input_booster_free(info->tkey_booster);
	info->tkey_booster = NULL;
#endif
	fts_power_ctrl(info, false);
#ifdef FTS_SUPPORT_TOUCH_KEY
	fts_led_power_ctrl(info, false);
#endif
	kfree(info);

	return 0;
}

static void fts_shutdown(struct i2c_client *client)
{
	struct fts_ts_info *info = i2c_get_clientdata(client);

	dev_info(&info->client->dev, "%s\n", __func__);

	fts_remove(client);
}

static void fts_recovery_cx(struct fts_ts_info *info)
{
	unsigned char regAdd[4] = {0};
	unsigned char buf[8] = {0};
	unsigned char cnt = 100;

	regAdd[0] = 0xB6;
	regAdd[1] = 0x00;
	regAdd[2] = 0x1E;
	regAdd[3] = 0x08;

	/* Loading FW to PRAM  without CRC Check */
	fts_write_reg(info,&regAdd[0], 4);
	fts_delay(30);

	fts_command(info,CX_TUNNING);
	fts_delay(300);

	fts_command(info,FTS_CMD_SAVE_CX_TUNING);
	fts_delay(200);

	do {
		int ret;
		regAdd[0] = READ_ONE_EVENT;
		ret = fts_read_reg(info, regAdd, 1, &buf[0], FTS_EVENT_SIZE);

		fts_delay(10);
		if (cnt-- == 0) break;
	} while (buf[0] != 0x16 || buf[1] != 0x04);


	fts_command(info, SLEEPOUT);
	fts_delay(50);

	fts_command(info, SENSEON);
	fts_delay(50);

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

	fts_command(info, FLUSHBUFFER);
}

#ifdef USE_OPEN_CLOSE
#ifdef USE_OPEN_DWORK
static void fts_open_work(struct work_struct *work)
{
	int retval;
	struct fts_ts_info *info = container_of(work, struct fts_ts_info,
						open_work.work);

	dev_info(&info->client->dev, "%s\n", __func__);

	retval = fts_start_device(info);
	if (retval < 0)
		dev_err(&info->client->dev,
			"%s: Failed to start device\n", __func__);
}
#endif
static int fts_input_open(struct input_dev *dev)
{
	struct fts_ts_info *info = input_get_drvdata(dev);

	dev_info(&info->client->dev, "%s\n", __func__);

#ifdef USE_OPEN_DWORK
	schedule_delayed_work(&info->open_work,
			      msecs_to_jiffies(TOUCH_OPEN_DWORK_TIME));
#else
	fts_start_device(info);
#endif

/* Hover feature is not using anymore */
/*
	tsp_debug_err(true, &info->client->dev, "FTS cmd after wakeup : h%d\n",
			info->retry_hover_enable_after_wakeup);

	if(info->retry_hover_enable_after_wakeup == 1){
		unsigned char regAdd[4] = {0xB0, 0x01, 0x29, 0x41};
		fts_write_reg(info, &regAdd[0], 4);
		fts_command(info, FTS_CMD_HOVER_ON);
		info->hover_ready = false;
		info->hover_enabled = true;
	}
*/
	return 0;
}

static void fts_input_close(struct input_dev *dev)
{
	struct fts_ts_info *info = input_get_drvdata(dev);

	dev_info(&info->client->dev, "%s\n", __func__);

#ifdef USE_OPEN_DWORK
	cancel_delayed_work(&info->open_work);
#endif

	fts_stop_device(info);
/*
	info->retry_hover_enable_after_wakeup = 0;
*/
}
#endif

#ifdef CONFIG_SEC_FACTORY
#include <linux/uaccess.h>
#define LCD_LDI_FILE_PATH	"/sys/class/lcd/panel/window_type"
static int fts_get_panel_revision(struct fts_ts_info *info)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *window_type;
	unsigned char lcdtype[4] = {0,};

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	window_type = filp_open(LCD_LDI_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(window_type)) {
		iRet = PTR_ERR(window_type);
		if (iRet != -ENOENT)
			dev_err(&info->client->dev, "%s: window_type file open fail\n", __func__);
		set_fs(old_fs);
		goto exit;
	}

	iRet = window_type->f_op->read(window_type, (u8 *)lcdtype, sizeof(u8) * 4, &window_type->f_pos);
	if (iRet != (sizeof(u8) * 4)) {
		dev_err(&info->client->dev, "%s: Can't read the lcd ldi data\n", __func__);
		iRet = -EIO;
	}

	/* The variable of lcdtype has ASCII values(40 81 45) at 0x08 OCTA,
	  * so if someone need a TSP panel revision then to read third parameter.*/
	info->panel_revision = lcdtype[3] & 0x0F;
	dev_info(&info->client->dev,
		"%s: update panel_revision 0x%02X\n", __func__, info->panel_revision);

	filp_close(window_type, current->files);
	set_fs(old_fs);

exit:
	return iRet;
}

static void fts_reinit_fac(struct fts_ts_info *info)
{
	int rc;

	info->touch_count = 0;

	fts_command(info, SLEEPOUT);
	fts_delay(50);

	fts_command(info, SENSEON);
	fts_delay(50);

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey)
		info->fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

#ifdef FTS_SUPPORT_NOISE_PARAM
	fts_get_noise_param_address(info);
#endif

	if (info->flip_enable)
		fts_set_cover_type(info, true);

	/* enable glove touch when flip cover is closed */
	if (info->fast_mshover_enabled)
		fts_command(info, FTS_CMD_SET_FAST_GLOVE_MODE);
	else if (info->mshover_enabled)
		fts_command(info, FTS_CMD_MSHOVER_ON);

	rc = fts_get_channel_info(info);
	if (rc >= 0) {
		dev_info(&info->client->dev, "FTS Sense(%02d) Force(%02d)\n",
		       info->SenseChannelLength, info->ForceChannelLength);
	} else {
		dev_info(&info->client->dev, "FTS read failed rc = %d\n", rc);
		dev_info(&info->client->dev, "FTS Initialise Failed\n");
		return;
	}

	if (info->pFrame)
		kfree(info->pFrame);
	info->pFrame = kzalloc(info->SenseChannelLength * info->ForceChannelLength * 2, GFP_KERNEL);

	if (info->cx_data)
		kfree(info->cx_data);
	info->cx_data = kzalloc(info->SenseChannelLength * info->ForceChannelLength, GFP_KERNEL);

	if (!info->pFrame || !info->cx_data) {
		dev_info(&info->client->dev, "mem alloc failed\n");
		return;
	}

	fts_command(info, FORCECALIBRATION);
	fts_command(info, FLUSHBUFFER);

	fts_interrupt_set(info, INT_ENABLE);

	dev_info(&info->client->dev, "FTS Re-Initialised\n");
}
#endif

static void fts_reinit(struct fts_ts_info *info)
{
	if (!info->lowpower_mode) {
		fts_wait_for_ready(info);
		fts_read_chip_id(info);
	}

	fts_systemreset(info);

	fts_wait_for_ready(info);

#ifdef CONFIG_SEC_FACTORY
	/* Read firmware version from IC when every power up IC.
	 * During Factory process touch panel can be changed manually.
	 */
	{
		unsigned short orig_fw_main_version_of_ic = info->fw_main_version_of_ic;

		fts_get_panel_revision(info);
		fts_get_version_info(info);

		if (info->fw_main_version_of_ic != orig_fw_main_version_of_ic) {
			fts_fw_init(info);
			fts_reinit_fac(info);
			return;
		}
	}
#endif

#ifdef FTS_SUPPORT_NOISE_PARAM
	fts_set_noise_param(info);
#endif

	fts_command(info, SLEEPOUT);
	fts_delay(50);

	fts_command(info, SENSEON);
	fts_delay(50);

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey)
		info->fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

	if (info->flip_enable)
		fts_set_cover_type(info, true);

	/* enable glove touch when flip cover is closed */
	if (info->fast_mshover_enabled)
		fts_command(info, FTS_CMD_SET_FAST_GLOVE_MODE);
	else if (info->mshover_enabled)
		fts_command(info, FTS_CMD_MSHOVER_ON);

#ifdef FTS_SUPPORT_TA_MODE
	if (info->TA_Pluged)
		fts_command(info, FTS_CMD_CHARGER_PLUGGED);
#endif

	info->touch_count = 0;
#ifdef FTS_SUPPORT_2NDSCREEN_FLAG
	info->SIDE_Flag = 0;
	info->previous_SIDE_value = 0;
#endif

	fts_command(info, FLUSHBUFFER);
	fts_interrupt_set(info, INT_ENABLE);
}

static void fts_release_all_finger(struct fts_ts_info *info)
{
	int i;

	for (i = 0; i < FINGER_MAX; i++) {
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);

		if ((info->finger[i].state == EVENTID_ENTER_POINTER) ||
			(info->finger[i].state == EVENTID_MOTION_POINTER)) {

			info->touch_count--;
			if (info->touch_count < 0)
				info->touch_count = 0;

			dev_info(&info->client->dev,
				"[R] tID:%d mc: %d tc:%d Ver[%02X%04X%01X%01X%01X]\n",
				i, info->finger[i].mcount, info->touch_count,
				info->panel_revision, info->fw_main_version_of_ic,
				info->flip_enable, info->mshover_enabled, info->mainscr_disable);
		}

		info->finger[i].state = EVENTID_LEAVE_POINTER;
		info->finger[i].mcount = 0;
	}

	input_report_key(info->input_dev, BTN_TOUCH, 0);
	input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);

#ifdef FTS_SUPPORT_2NDSCREEN_FLAG
	info->SIDE_Flag = 0;
	info->previous_SIDE_value = 0;
	input_report_key(info->input_dev, BTN_SUBSCREEN_FLAG, 0);
#endif
#ifdef FTS_USE_SIDE_SCROLL_FLAG
	input_report_key(info->input_dev, BTN_R_FLICK_FLAG, 0);
	input_report_key(info->input_dev, BTN_L_FLICK_FLAG, 0);
#endif

#ifdef CONFIG_GLOVE_TOUCH
	input_report_switch(info->input_dev, SW_GLOVE, false);
	info->touch_mode = FTS_TM_NORMAL;
#endif
	input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);

#ifdef FTS_SUPPORT_SIDE_GESTURE
	if (info->dt_data->support_sidegesture) {
		input_report_key(info->input_dev, KEY_SIDE_GESTURE, 0);
		input_report_key(info->input_dev, KEY_SIDE_GESTURE_RIGHT, 0);
		input_report_key(info->input_dev, KEY_SIDE_GESTURE_LEFT, 0);
	}
#endif

	input_sync(info->input_dev);

	input_mt_slot(info->input_dev, 0);

#ifdef TSP_BOOSTER
	if (info->tsp_booster && info->tsp_booster->dvfs_set)
		info->tsp_booster->dvfs_set(info->tsp_booster, -1);
#endif
}

#if defined(CONFIG_SECURE_TOUCH)
static void fts_secure_touch_stop(struct fts_ts_info *info, int blocking)
{
	dev_err(&info->client->dev, "%s: SECURETOUCH_STOP\n", __func__);
	if (atomic_read(&info->st_enabled)) {
		atomic_set(&info->st_pending_irqs, -1);
		fts_secure_touch_notify(info);

		if (blocking)
			wait_for_completion_interruptible(&info->st_powerdown);
	}
}
#endif

static int fts_stop_device(struct fts_ts_info *info)
{
	dev_info(&info->client->dev, "%s %s\n",
			__func__, info->lowpower_mode ? "enter low power mode" : "");

#if defined(CONFIG_SECURE_TOUCH)
	fts_secure_touch_stop(info, 1);
#endif
	mutex_lock(&info->device_mutex);

	if (info->touch_stopped) {
		dev_err(&info->client->dev, "%s already power off\n", __func__);
		goto out;
	}

	if (info->lowpower_mode) {
#ifdef FTS_ADDED_RESETCODE_IN_LPLM

		info->mainscr_disable = false;
		info->edge_grip_mode = false;

#else	// clear cmd list.
#ifdef FTS_SUPPORT_MAINSCREEN_DISBLE
		dev_info(&info->client->dev, "%s mainscreen disebla flag:%d, clear 0\n", __func__, info->mainscr_disable);
		set_mainscreen_disable_cmd((void *)info, 0);
#endif
		if (info->edge_grip_mode == false) {
			dev_info(&info->client->dev, "%s edge grip enable flag:%d, clear 1\n", __func__, info->edge_grip_mode);
			longpress_grip_enable_mode(info, 1);		// default
			grip_check_enable_mode(info, 1);			// default
		}
#endif
		dev_info(&info->client->dev, "%s lowpower flag:%d\n", __func__, info->lowpower_flag);

		info->power_state = STATE_LOWPOWER_RESUME;

		fts_command(info, FLUSHBUFFER);

#ifdef FTS_SUPPORT_SIDE_GESTURE
		if (info->dt_data->support_sidegesture) {
			fts_enable_feature(info, FTS_FEATURE_SIDE_GESTURE, true);
			fts_delay(20);
		}
#endif
		fts_command(info, FTS_CMD_LOWPOWER_MODE);

		if (device_may_wakeup(&info->client->dev))
			enable_irq_wake(info->client->irq);

		fts_command(info, FLUSHBUFFER);

		fts_release_all_finger(info);
#ifdef FTS_SUPPORT_TOUCH_KEY
		fts_release_all_key(info);
#endif
#ifdef FTS_SUPPORT_NOISE_PARAM
		fts_get_noise_param(info);
#endif
#ifdef FTS_SUPPORT_STRINGLIB
		if (info->fts_mode)
			fts_enable_feature(info, 0x0B, true);
#endif
	} else {
		fts_interrupt_set(info, INT_DISABLE);
		disable_irq(info->irq);

		fts_command(info, FLUSHBUFFER);
		fts_command(info, SLEEPIN);
		fts_release_all_finger(info);
#ifdef FTS_SUPPORT_TOUCH_KEY
		fts_release_all_key(info);
#endif
#ifdef FTS_SUPPORT_NOISE_PARAM
		fts_get_noise_param(info);
#endif
		info->touch_stopped = true;

		mutex_lock(&info->i2c_mutex);
		fts_power_ctrl(info, false);
		mutex_unlock(&info->i2c_mutex);
		info->power_state = STATE_POWEROFF;
	}
out:
	mutex_unlock(&info->device_mutex);

	return 0;
}

static int fts_start_device(struct fts_ts_info *info)
{
	unsigned short string_addr = FTS_CMD_STRING_ACCESS;
	unsigned char string_pi[16] = {0, };
	int ret;

	dev_info(&info->client->dev, "%s %s\n",
			__func__, info->lowpower_mode ? "exit low power mode" : "");

#if defined(CONFIG_SECURE_TOUCH)
	fts_secure_touch_stop(info, 1);
#endif
	mutex_lock(&info->device_mutex);

	if (!info->touch_stopped && !info->lowpower_mode) {
		dev_err(&info->client->dev, "%s already power on\n", __func__);
		goto out;
	}

	fts_release_all_finger(info);
#ifdef FTS_SUPPORT_TOUCH_KEY
	fts_release_all_key(info);
#endif

	if (info->lowpower_mode) {
		/* low power mode command is sent after LCD OFF. turn on touch power @ LCD ON */
		if (info->touch_stopped) {
			fts_power_ctrl(info, true);

			info->touch_stopped = false;
			info->reinit_done = false;

			fts_reinit(info);

			info->reinit_done = true;

			enable_irq(info->irq);

			if (info->fts_mode) {
				ret = info->fts_read_from_string(info, &string_addr, string_pi, sizeof(string_pi));
				if (ret < 0)
					dev_err(&info->client->dev, "%s: read failed. ret: %d\n", __func__, ret);

				string_addr = FTS_CMD_STRING_ACCESS + *(unsigned short *)&string_pi[STRING_OFFSET_REGISTER];

				ret = info->fts_write_to_string(info, &string_addr, &info->fts_mode, sizeof(info->fts_mode));
				if (ret < 0)
					dev_err(&info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
			}
			goto out;
		}
#ifdef USE_RESET_WORK_EXIT_LOWPOWERMODE
		schedule_work(&info->reset_work.work);
		goto out;
#endif

#ifdef FTS_ADDED_RESETCODE_IN_LPM
		disable_irq(info->irq);
		info->reinit_done = false;

		fts_reinit(info);

		info->reinit_done = true;
		enable_irq(info->irq);
#else
		fts_command(info, SLEEPIN);
		fts_delay(50);
		fts_command(info, SLEEPOUT);
		fts_delay(50);
		fts_command(info, SENSEON);
		fts_delay(50);
#ifdef FTS_SUPPORT_TOUCH_KEY
		if (info->dt_data->support_mskey) {
			info->fts_command(info, FTS_CMD_KEY_SENSE_ON);
			fts_delay(50);
		}
#endif
		fts_command(info, FLUSHBUFFER);
#endif
		if (device_may_wakeup(&info->client->dev))
			disable_irq_wake(info->client->irq);

	} else {
		fts_power_ctrl(info, true);

		info->touch_stopped = false;
		info->reinit_done = false;

		fts_reinit(info);

		info->reinit_done = true;

		if (info->flip_state != info->flip_enable) {
			dev_err(&info->client->dev, "%s: not equal cover state.(%d, %d)\n",
				__func__, info->flip_state, info->flip_enable);
			fts_set_cover_type(info, info->flip_enable);
		}

		enable_irq(info->irq);
	}

#ifdef FTS_SUPPORT_STRINGLIB
	if (info->fts_mode) {
		ret = info->fts_read_from_string(info, &string_addr, string_pi, sizeof(string_pi));
		if (ret < 0)
			dev_err(&info->client->dev, "%s: read failed. ret: %d\n", __func__, ret);

		string_addr = FTS_CMD_STRING_ACCESS + *(unsigned short *)&string_pi[STRING_OFFSET_REGISTER];

		ret = info->fts_write_to_string(info, &string_addr, &info->fts_mode, sizeof(info->fts_mode));
		if (ret < 0)
			dev_err(&info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
	}
#endif
out:
	mutex_unlock(&info->device_mutex);

	info->power_state = STATE_ACTIVE;

	return 0;
}

#ifdef TSP_RAWDATA_DUMP
static void ghost_touch_check(struct work_struct *work)
{
	struct fts_ts_info *info = container_of(work, struct fts_ts_info,
						ghost_check.work);
	int i;

	if (info->rawdata_read_lock == 1) {
		dev_err(&info->client->dev, "%s, ## checking.. ignored.\n", __func__);
		return;
	} else {
	        for (i = 0; i < 5; i++) {
		        dev_err(&info->client->dev, "%s, ## run Raw Cap data ##, %d\n", __func__, __LINE__);
		        run_rawcap_read((void *)info);

		        dev_err(&info->client->dev, "%s, ## run Delta ##, %d\n", __func__, __LINE__);
		        run_delta_read((void *)info);
		        msleep(50);
	        }
	        dev_err(&info->client->dev, "%s, ## run CX data ##, %d\n", __func__, __LINE__);
	        run_cx_data_read((void *)info);

	        dev_err(&info->client->dev, "%s, ## Done ##, %d\n", __func__, __LINE__);

	        info->rawdata_read_lock = 0;
	}
}

void tsp_dump(void)
{
#ifdef CONFIG_SAMSUNG_LPM_MODE
	if (poweroff_charging)
		return;
#endif
	if (!p_ghost_check)
		return;

	pr_err("[FTS] %s: start\n", __func__);
	schedule_delayed_work(p_ghost_check, msecs_to_jiffies(100));
}
#else
void tsp_dump(void)
{
	pr_err("[FTS] %s: not support\n", __func__);
}
#endif


#if defined(USE_RESET_WORK_EXIT_LOWPOWERMODE) || defined(FTS_SUPPORT_ESD_CHECK)
static void fts_reset_work(struct work_struct *work)
{
	struct fts_ts_info *info = container_of(work, struct fts_ts_info,
						reset_work.work);
	unsigned char orig_state;
	bool orig_lowpower;

	orig_state = info->power_state;
	orig_lowpower = info->lowpower_mode;

	info->lowpower_mode = FTS_DISABLE;

	dev_info(&info->client->dev, "%s, reset IC off, lpm:%d\n", __func__, orig_lowpower);
	fts_stop_device(info);

	msleep(100);

	info->lowpower_mode = orig_lowpower;
	dev_dbg(&info->client->dev, "%s, reset IC on\n", __func__);
	if (fts_start_device(info) < 0) {
		dev_err(&info->client->dev,
			"%s: Failed to start device\n", __func__);
	}

	if (orig_state & STATE_LOWPOWER)
		fts_stop_device(info);
}
#endif

#ifdef CONFIG_PM
static int fts_ts_suspend(struct device *dev)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	if (info->lowpower_mode)
		init_completion(&info->resume_done);

	info->power_state = STATE_LOWPOWER_SUSPEND;

	return 0;
}

static int fts_ts_resume(struct device *dev)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	if (info->lowpower_mode)
		complete_all(&info->resume_done);

	info->power_state = STATE_LOWPOWER_RESUME;

	return 0;
}


static const struct dev_pm_ops fts_ts_pm_ops = {
	.suspend = fts_ts_suspend,
	.resume  = fts_ts_resume,
};
#endif

static const struct i2c_device_id fts_device_id[] = {
	{FTS_TS_DRV_NAME, 0},
	{}
};

#ifdef CONFIG_OF
static struct of_device_id fts_match_table[] = {
	{ .compatible = "stm,fts_ts",},
	{ },
};
#else
#define fts_match_table   NULL
#endif

static struct i2c_driver fts_i2c_driver = {
	.driver = {
		.name = FTS_TS_DRV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = fts_match_table,
#endif
#ifdef CONFIG_PM
		.pm = &fts_ts_pm_ops,
#endif

	},
	.probe = fts_probe,
	.remove = fts_remove,
	.shutdown = fts_shutdown,
	.id_table = fts_device_id,
};

static int __init fts_driver_init(void)
{
#ifdef CONFIG_SAMSUNG_LPM_MODE
	pr_err("%s\n", __func__);

	if (poweroff_charging) {
		pr_err("%s : LPM Charging Mode!!\n", __func__);
		return 0;
	}
#endif
	return i2c_add_driver(&fts_i2c_driver);
}

static void __exit fts_driver_exit(void)
{
	i2c_del_driver(&fts_i2c_driver);
}

MODULE_DESCRIPTION("STMicroelectronics MultiTouch IC Driver");
MODULE_AUTHOR("STMicroelectronics, Inc.");
MODULE_LICENSE("GPL v2");

module_init(fts_driver_init);
module_exit(fts_driver_exit);

