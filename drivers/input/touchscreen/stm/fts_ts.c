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
//#include <asm/system.h>
#include <linux/atomic.h>
#endif

#ifdef FTS_SUPPORT_SIDE_GESTURE
#include <linux/wakelock.h>
struct wake_lock  report_wake_lock;
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
struct fts_touchkey fts_touchkeys[] = {
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

extern int get_lcd_attached(char*);
extern int boot_mode_recovery;
#ifdef CONFIG_SAMSUNG_LPM_MODE
extern int poweroff_charging;
#endif

#ifdef USE_OPEN_CLOSE
static int fts_input_open(struct input_dev *dev);
static void fts_input_close(struct input_dev *dev);
#ifdef USE_OPEN_DWORK
static void fts_open_work(struct work_struct *work);
#endif
#endif

#if defined(USE_RESET_WORK_EXIT_LOWPOWERMODE) || defined(ESD_CHECK)
static void fts_reset_work(struct work_struct *work);
#endif

#ifdef FTS_SUPPORT_MAINSCREEN_DISBLE
extern void set_mainscreen_disable_cmd(struct fts_ts_info *info, bool on);
#endif

#ifdef TSP_RAWDATA_DUMP
extern void run_cx_data_read(void *device_data);
extern void run_rawcap_read(void *device_data);
extern void run_delta_read(void *device_data);
static void ghost_touch_check(struct work_struct *work);
struct delayed_work * p_ghost_check;
#endif

static int fts_stop_device(struct fts_ts_info *info);
static int fts_start_device(struct fts_ts_info *info);
static int fts_irq_enable(struct fts_ts_info *info, bool enable);
void fts_release_all_finger(struct fts_ts_info *info);

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

			dev_err(&info->client->dev, "%s: SECURETOUCH_secure_touch_enable_reset",__func__);
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
			dev_err(&info->client->dev, "%s: SECURETOUCH_secure_touch_enable_set",__func__);
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

	if (atomic_read(&info->st_enabled) == 0) {
		return -EBADF;
	}

	if (atomic_cmpxchg(&info->st_pending_irqs, -1, 0) == -1) {
		return -EINVAL;
	}

	if (atomic_cmpxchg(&info->st_pending_irqs, 1, 0) == 1) {
		val = 1;
	}

	dev_err(&info->client->dev, "%s %d: SECURETOUCH_fts_secure_touch_show ",__func__, val);
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
	struct i2c_client *client;
	int ret;

	if (info->touch_stopped) {
		dev_err(&info->client->dev, "%s: Sensor stopped\n", __func__);
		goto exit;
	}

	mutex_lock(&info->i2c_mutex);

	if (info->slave_addr == FTS_SLAVE_ADDRESS)
		client = info->client;
	else
		client = info->client_sub;

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
	struct i2c_client *client;
	int ret;
	int retry = 3;

	if (info->touch_stopped) {
		dev_err(&info->client->dev, "%s: Sensor stopped\n", __func__);
		goto exit;
	}
	mutex_lock(&info->i2c_mutex);

	if (info->slave_addr == FTS_SLAVE_ADDRESS)
		client = info->client;
	else
		client = info->client_sub;

	xfer_msg[0].addr = client->addr;
	xfer_msg[0].len = cnum;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = reg;

	xfer_msg[1].addr = client->addr;
	xfer_msg[1].len = num;
	xfer_msg[1].flags = I2C_M_RD;
	xfer_msg[1].buf = buf;

	do{
	ret = i2c_transfer(client->adapter, xfer_msg, 2);
		if (ret < 0){
			dev_err(&client->dev, "%s failed(%d). ret:%d, addr:%x\n", __func__,retry, ret, xfer_msg[0].addr);
			msleep(10);
		}else{
			break;
		}
	}while(--retry>0);

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

	string_reg[0] = 0xD0;
	string_reg[1] = (*reg >> 8) & 0xFF;
	string_reg[2] = *reg & 0xFF;

	if (info->digital_rev == FTS_DIGITAL_REV_1) {
		return fts_read_reg(info, string_reg, 3, data, length);
	} else {
		int rtn;
		buf = kzalloc(length + 1, GFP_KERNEL);
		if (buf == NULL) {
			tsp_debug_info(true, &info->client->dev,
					"%s: kzalloc error.\n", __func__);
			return -1;
		}

		rtn = fts_read_reg(info, string_reg, 3, buf, length + 1);
		if (rtn >= 0)
			memcpy(data, &buf[1], length);

		kfree(buf);
		return rtn;
	}
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

	regAdd = kzalloc(length + 6, GFP_KERNEL);
	if (regAdd == NULL) {
		tsp_debug_info(true, &info->client->dev,
				"%s: kzalloc error.\n", __func__);
		return -1;
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
				"FTS %s Feature (%02X %02X) , ret = %d \n",
				(enable) ? "Enable" : "Disable", regAdd[0], regAdd[1], ret);
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
static bool need_force_firmup = 0;
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

	if((val[5]==0)&&(val[6]==0)){
		dev_err(&info->client->dev, "%s: invalid version, need firmup\n", __func__);
		need_force_firmup = 1;
	}

	return ret;
}

static int fts_wait_for_ready(struct fts_ts_info *info)
{
	unsigned char regAdd = READ_ONE_EVENT;
	unsigned char data[FTS_EVENT_SIZE] = {0, };
	int retry = 0, err_cnt = 0, ret;

	while (fts_read_reg(info, &regAdd, 1, data, FTS_EVENT_SIZE)) {

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

		if (retry++ > 30) {
			dev_err(&info->client->dev, "%s: time out\n", __func__);
			ret = -FTS_ERROR_TIMEOUT;
			break;
		}

		fts_delay(10);
	}

	dev_info(&info->client->dev,
		"%s: %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X\n",
		__func__, data[0], data[1], data[2], data[3],
		data[4], data[5], data[6], data[7]);

	return ret;
}

static int fts_get_version_info(struct fts_ts_info *info)
{
	unsigned char regAdd[3];
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
			"%s: Firmware: 0x%04X, Config: 0x%04X, Main: 0x%04X\n",
			__func__, info->fw_version_of_ic,
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
	} else if (info->digital_rev == FTS_DIGITAL_REV_2){
		 ret = fts_read_reg(info, &cmd[0], 3, rData, 3);
		 info->roi_addr = (rData[2] <<8 ) | rData[1];
	}
	printk(KERN_ERR "[FTS] roidelta_read Address 0x%2x\n", info->roi_addr);

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
	} else if (info->digital_rev == FTS_DIGITAL_REV_2){
		ret = fts_read_reg(info, regAdd, 3, rData, 3);
		info->noise_param->pAddr[0] = (rData[2] <<8 ) | rData[1];
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
void fts_release_all_key(struct fts_ts_info *info)
{

	input_report_key(info->input_dev, KEY_RECENT,KEY_RELEASE);
	input_report_key(info->input_dev, KEY_BACK, KEY_RELEASE);
	tsp_debug_info(true, &info->client->dev, "[TSP_KEY] %s\n", __func__);

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
	} else {
		fts_get_version_info(info);
	}

	ret = fts_read_chip_id(info);
	if(need_force_firmup == 1){
		dev_err(info->dev, "%s: Firmware empty, so force firmup\n",__func__);
		need_force_firmup = 0;
	}else if (ret < 0)
		return ret;

	fts_get_version_info(info);

#ifdef FTS_SUPPORT_QEEXO_ROI
	fts_get_roi_address(info);
#endif

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
		fts_enable_feature(info, FTS_FEATURE_SIDE_GUSTURE, true);
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

	info->fts_power_mode = TSP_POWERDOWN_MODE;

#ifdef FTS_SUPPORT_TOUCH_KEY
	info->tsp_keystatus = 0x00;
#endif

#ifdef FTS_SUPPORT_2NDSCREEN_FLAG
	info->SIDE_Flag = 0;
	info->previous_SIDE_value = 0;
#endif

#ifdef SEC_TSP_FACTORY_TEST
	ret = fts_get_channel_info(info);
	if (ret < 0) {
		dev_info(&info->client->dev, "%s: failed get channel info\n", __func__);
		return ret;
	}

	info->pFrame =
	    kzalloc(info->SenseChannelLength * info->ForceChannelLength * 2,
		    GFP_KERNEL);
	if (info->pFrame == NULL) {
		dev_info(&info->client->dev, "%s: pFrame kzalloc Failed\n", __func__);
		return 1;
	}

	info->cx_data = kzalloc(info->SenseChannelLength * info->ForceChannelLength, GFP_KERNEL);
	if (!info->cx_data)
		dev_err(&info->client->dev, "%s: cx_data kzalloc Failed\n", __func__);

#endif

	fts_command(info, FORCECALIBRATION);
	fts_command(info, FLUSHBUFFER);

	fts_interrupt_set(info, INT_ENABLE);

	regAdd[0] = READ_STATUS;
	ret= fts_read_reg(info, regAdd, 1, (unsigned char *)val, 4);
	dev_info(&info->client->dev, "FTS ReadStatus(0x84) : %02X %02X %02X %02X\n", val[0],
	       val[1], val[2], val[3]);

	dev_info(&info->client->dev, "FTS Initialized\n");

	return 0;
}

static void fts_unknown_event_handler(struct fts_ts_info *info,
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
	int y_devide = info->dt_data->max_y / 3;

	if (flag) { // Y
		if (coord < y_devide)
			return 'H'; // high
		else if (coord < y_devide * 2)
			return 'M'; // mid
		else
			return 'L'; // low
	}
	else { // X
		if (coord < 1440)
			return '0'; // main screen
		else if (coord < 1530)
			return '1'; // wide 2nd screen
		else
			return '2'; // narrow 2nd screen
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
	unsigned char TouchID = 0, EventID = 0;
	unsigned char LastLeftEvent = 0;
	int x = 0, y = 0, z = 0;
	int bw = 0, bh = 0, palm = 0, sumsize = 0;
	unsigned short string_addr;
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
	unsigned char string_clear = 0;

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

					/* ID 0010 OCTA schematic is reversed touchkey map.
					 * it will be applied only 0010 OCTA */
					if (info->panel_revision == 0x02) {
						key_recent = TOUCH_KEY_BACK;
						key_back = TOUCH_KEY_RECENT;
					} else {
						key_recent = TOUCH_KEY_RECENT;
						key_back = TOUCH_KEY_BACK;
					}

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

					input_sync(info->input_dev);
				}

				info->tsp_keystatus = input_keys;
			}
			break;
#endif
		case 0x0B:
		case 0xDB:
#ifdef FTS_SUPPORT_SIDE_GESTURE
				if ((data[1 + EventNum * FTS_EVENT_SIZE] == 0xE0) ||
					 (data[1 + EventNum * FTS_EVENT_SIZE] == 0xE1)) {

				if (info->dt_data->support_sidegesture) {

					int direction, distance;
					direction = data[2 + EventNum * FTS_EVENT_SIZE];
					distance = *(int *)&data[3 + EventNum * FTS_EVENT_SIZE];

					wake_lock_timeout(&report_wake_lock, 3*HZ);

					input_report_key(info->input_dev, KEY_SIDE_GESTURE, 1);
					tsp_debug_info(true, &info->client->dev,
					"%s: [Gesture] %02X %02X %02X %02X %02X %02X %02X %02X\n",
					__func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7]);

					input_sync(info->input_dev);
					usleep(1000);

					input_report_key(info->input_dev, KEY_SIDE_GESTURE, 0);

				}
				else
					fts_unknown_event_handler(info, &data[EventNum * FTS_EVENT_SIZE]);
			}
			else if(data[1 + EventNum * FTS_EVENT_SIZE] == 0xBB) {
					 int sideLongPressfingerID = 0;
					 sideLongPressfingerID = data[2 + EventNum * FTS_EVENT_SIZE];

					 //Todo : event processing
					longpress_release[sideLongPressfingerID-1] = 1;

					 tsp_debug_info(true, &info->client->dev,
							   "%s: [Side Long Press]id:%d %02X %02X %02X %02X %02X %02X %02X %02X\n",
							   __func__,sideLongPressfingerID, data[0], data[1], data[2], data[3],
							   data[4], data[5], data[6], data[7]);
			}
			 else
#endif
#ifdef ESD_CHECK
			if(data[1 + EventNum * FTS_EVENT_SIZE] == 0xED) {

				 tsp_debug_info(true, &info->client->dev,
				 "%s: [ESD error] %02X %02X %02X %02X %02X %02X %02X %02X\n",
				 __func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7]);

				 //schedule_delay_work(&info->reset_work.work);
				 schedule_delayed_work(&info->reset_work, msecs_to_jiffies(10));

			}
			else
#endif
				fts_unknown_event_handler(info, &data[EventNum * FTS_EVENT_SIZE]);
			break;
		case EVENTID_ERROR:
			 /* Get Auto tune fail event */
			if (data[1 + EventNum * FTS_EVENT_SIZE] == 0x08) {
				if (data[2 + EventNum * FTS_EVENT_SIZE] == 0x00) {
					dev_info(&info->client->dev, "[FTS] Fail Mutual Auto tune\n");
				}
				else if (data[2 + EventNum * FTS_EVENT_SIZE] == 0x01) {
					dev_info(&info->client->dev, "[FTS] Fail Self Auto tune\n");
				}
			} else if (data[1 + EventNum * FTS_EVENT_SIZE] == 0x09) // Get detect SYNC fail event
				tsp_debug_info(true, &info->client->dev, "[FTS] Fail detect SYNC\n");
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

			if(info->fts_power_mode == FTS_POWER_STATE_LOWPOWER){
				break;
			}

			info->touch_count++;

		case EVENTID_MOTION_POINTER:

			if(info->fts_power_mode == FTS_POWER_STATE_LOWPOWER){
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
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER,
						   1 + (palm << 1));

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
			info->finger[TouchID].lx = x;
			info->finger[TouchID].ly = y;

			break;

		case EVENTID_LEAVE_POINTER:

			if(info->fts_power_mode == FTS_POWER_STATE_LOWPOWER){
				break;
			}

			if (info->touch_count <= 0) {
				dev_err(&info->client->dev, "%s %d: count 0\n", __func__, __LINE__);
				fts_release_all_finger(info);
				break;
			}

			info->touch_count--;

			input_mt_slot(info->input_dev, TouchID);

#if defined(FTS_SUPPORT_SIDE_GESTURE)
			if(longpress_release[TouchID] == 1){
				input_report_abs(info->input_dev, ABS_MT_GRIP, 1);
				dev_info(&info->client->dev, "[FTS] GRIP [%d] %s\n",TouchID, longpress_release[TouchID] ? "LONGPRESS" : "RELEASE");
				longpress_release[TouchID] = 0;

				input_sync(info->input_dev);
			}
#endif
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER, 0);

			if (info->touch_count == 0) {
				/* Clear BTN_TOUCH when All touch are released  */
				input_report_key(info->input_dev, BTN_TOUCH, 0);
				input_report_key(info->input_dev, KEY_REAR_CAMERA_DETECTED, 0);
				input_report_key(info->input_dev, KEY_FRONT_CAMERA_DETECTED, 0);
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
				if (info->dt_data->support_sidegesture)
					input_report_key(info->input_dev, KEY_SIDE_GESTURE, 0);
#endif
			}
			break;
		case EVENTID_STATUS_EVENT:
			if (data[1 + EventNum * FTS_EVENT_SIZE] == 0x0C) {
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
			} else if (data[1 + EventNum * FTS_EVENT_SIZE] == 0x0d) {
				unsigned char regAdd[4] = {0xB0, 0x01, 0x29, 0x01};
				fts_write_reg(info, &regAdd[0], 4);

				info->hover_ready = true;

				dev_info(&info->client->dev, "[FTS] Received the Hover Raw Data Ready Event\n");
			} else {
				fts_unknown_event_handler(info,
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
			dev_info(&info->client->dev,"[TB] side scroll flag: event: %02X, thr: %02X\n",scroll_flag, scroll_thr);

             // TODO : Report function call this area

			if(scroll_flag == 1){
				input_report_key(info->input_dev, BTN_R_FLICK_FLAG, 1);
				input_report_key(info->input_dev, BTN_L_FLICK_FLAG, 0);
				input_sync(info->input_dev);
			}else if(scroll_flag == 2){
				input_report_key(info->input_dev, BTN_R_FLICK_FLAG, 0);
				input_report_key(info->input_dev, BTN_L_FLICK_FLAG, 1);
				input_sync(info->input_dev);
			}

		 	break;
#endif

		case EVENTID_FROM_STRING:
			string_addr = FTS_CMD_STRING_ACCESS;
			fts_read_from_string(info, &string_addr, string_data, 6);
			dev_info(&info->client->dev,
					"%s: [String] %02X %02X %02X %02X %02X %02X %02X %02X || %04X: %02X, %02X\n",
					__func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7],
					string_addr, string_data[0], string_data[1]);

			switch(string_data[1]) {
			case FTS_STRING_EVENT_REAR_CAM:
				dev_info(&info->client->dev, "%s: REAR_CAM\n", __func__);
				input_report_key(info->input_dev, KEY_REAR_CAMERA_DETECTED, 1);
				break;
			case FTS_STRING_EVENT_FRONT_CAM:
				dev_info(&info->client->dev, "%s: FRONT_CAM\n", __func__);
				input_report_key(info->input_dev, KEY_FRONT_CAMERA_DETECTED, 1);
				break;
			case FTS_STRING_EVENT_WATCH_STATUS:
			case FTS_STRING_EVENT_FAST_ACCESS:
			case FTS_STRING_EVENT_DIRECT_INDICATOR:
				dev_info(&info->client->dev, "%s: SCRUB[%X]\n", __func__, string_data[1]);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				info->scrub_id = (string_data[1] >> 2) & 0x3;
				info->scrub_x = string_data[2] | (string_data[3] << 8);
				info->scrub_y = string_data[4] | (string_data[5] << 8);
				break;
			default:
				dev_info(&info->client->dev, "%s: no event:%X\n", __func__, string_data[1]);
				break;
			}
/*
			if (string_data[1] & FTS_STRING_EVENT_REAR_CAM) {
				input_report_key(info->input_dev, KEY_REAR_CAMERA_DETECTED, 1);

			} else if (string_data[1] & FTS_STRING_EVENT_FRONT_CAM) {
				input_report_key(info->input_dev, KEY_FRONT_CAMERA_DETECTED, 1);
			} else if ((string_data[1] & FTS_STRING_EVENT_WATCH_STATUS) ||
					(string_data[1] & FTS_STRING_EVENT_FAST_ACCESS)) {
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);

				info->scrub_id = (string_data[1] >> 2) & 0x3;
				info->scrub_x = string_data[2] | (string_data[3] << 8);
				info->scrub_y = string_data[4] | (string_data[5] << 8);
			}
*/
			input_sync(info->input_dev);

			if (0/*!info->temp*/) {
				/* request by display lab, buf clear */
				string_clear |= string_data[1];

				string_addr = FTS_CMD_STRING_ACCESS + 1;
				string_data[1] &= ~(string_clear);

				dev_info(&info->client->dev,
						"%s: clear bit : string_data[1]: %X, string_clear: %X\n",
						__func__, string_data[1], string_clear);

				fts_write_to_string(info, &string_addr, &string_data[1], 1);
			} else {
				dev_info(&info->client->dev,
						"%s: do not clear string_bit\n",
						__func__);
			}

			input_report_key(info->input_dev, KEY_REAR_CAMERA_DETECTED, 0);
			input_report_key(info->input_dev, KEY_FRONT_CAMERA_DETECTED, 0);
			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);

			break;

		default:
			fts_unknown_event_handler(info, &data[EventNum * FTS_EVENT_SIZE]);
			continue;
		}
#ifdef FTS_SUPPORT_2NDSCREEN_FLAG
		if ( currentSideFlag != info->previous_SIDE_value ) {

			dev_info(&info->client->dev,"[TB] 2nd screen flag was changed,  old:%d c:%d f:%d\n", info->previous_SIDE_value, currentSideFlag, info->SIDE_Flag);
			info->SIDE_Flag = currentSideFlag;
			// TODO : Report function call this area

			input_report_key(info->input_dev, BTN_SUBSCREEN_FLAG, !(!(info->SIDE_Flag)) );
		}
		info->previous_SIDE_value = currentSideFlag;
#endif

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		if (EventID == EVENTID_ENTER_POINTER)
			dev_info(&info->client->dev,
				"[P] tID:%d x:%d y:%d w:%d h:%d z:%d s:%d p:%d tc:%d tm:%d\n",
				TouchID, x, y, bw, bh, z, sumsize, palm, info->touch_count, info->touch_mode);
		else if (EventID == EVENTID_HOVER_ENTER_POINTER)
			dev_info(&info->client->dev,
				"[HP] tID:%d x:%d y:%d z:%d\n",
				TouchID, x, y, z);
#else
		if (EventID == EVENTID_ENTER_POINTER) {
			dev_info(&info->client->dev,
				"[P] tID:%d loc:%c%c tc:%d tm:%d\n",
				TouchID,
				location_detect(info, y, 1), location_detect(info, x, 0),
				info->touch_count, info->touch_mode);
		}
		else if (EventID == EVENTID_HOVER_ENTER_POINTER)
			dev_info(&info->client->dev,
				"[HP] tID:%d\n", TouchID);
#endif
		else if (EventID == EVENTID_LEAVE_POINTER) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			dev_info(&info->client->dev,
				"[R] tID:%d mc: %d tc:%d lx:%d ly:%d Ver[%02X%04X%01X%01X%01X]\n",
				TouchID, info->finger[TouchID].mcount, info->touch_count,
				info->finger[TouchID].lx, info->finger[TouchID].ly,
				info->panel_revision, info->fw_main_version_of_ic,
				info->flip_enable, info->mshover_enabled, info->mainscr_disable);
#else
			dev_info(&info->client->dev,
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

#ifdef TSP_BOOSTER
	if(EventID == EVENTID_ENTER_POINTER || EventID == EVENTID_LEAVE_POINTER)
		if (info->tsp_booster->dvfs_set)
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

static void fts_cam_work(struct work_struct *work)
{
	struct fts_ts_info *info = container_of(work, struct fts_ts_info,
						cam_work.work);

	dev_info(&info->client->dev, "%s\n", __func__);

	input_report_key(info->input_dev, KEY_REAR_CAMERA_DETECTED, 0);
	input_report_key(info->input_dev, KEY_FRONT_CAMERA_DETECTED, 0);
	input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);

	input_sync(info->input_dev);
}


#if defined(CONFIG_SECURE_TOUCH)
static void fts_secure_touch_notify(struct fts_ts_info *info)
{
	dev_err(&info->client->dev, "%s: SECURETOUCH_NOTIFY",__func__);
	sysfs_notify(&info->input_dev->dev.kobj, NULL, "secure_touch");
}

static irqreturn_t fts_filter_interrupt(struct fts_ts_info *info)
{
/*
//	dev_err(&info->client->dev, "%s: SECURETOUCH_FILTER_INTR_enter",__func__);
//	dev_err(&info->client->dev, "%s: SECURETOUCH_cmpxchg st_pending_irqs=%d",
//							__func__, atomic_read(&info->st_pending_irqs));
*/
	if (atomic_read(&info->st_enabled)) {
		if (atomic_cmpxchg(&info->st_pending_irqs, 0, 1) == 0) {
			dev_err(&info->client->dev, "%s: SECURETOUCH_cmpxchg",__func__);
			dev_err(&info->client->dev, "%s: SECURETOUCH_cmpxchg st_pending_irqs=%d",
								__func__, atomic_read(&info->st_pending_irqs));
			fts_secure_touch_notify(info);
		}
		dev_err(&info->client->dev, "%s: SECURETOUCH_FILTER_INTR",__func__);
		return IRQ_HANDLED;
	}
/*
//	dev_err(&info->client->dev, "%s: SECURETOUCH_FILTER_INTR_irq-none",__func__);
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
/*
//	dev_err(&info->client->dev, "%s: SECURETOUCH_irq",__func__);
*/
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
		if (info->fts_power_mode == FTS_POWER_STATE_LOWPOWER_SUSPEND) {

			wake_lock_timeout(&info->wakelock, 2 * HZ);

			dev_info(&info->client->dev, "%s: PM_SUSPEND\n", __func__);

			ret = wait_for_completion_interruptible_timeout((&info->resume_done),
						msecs_to_jiffies(1 * MSEC_PER_SEC));
			dev_info(&info->client->dev, "%s: PM_RESUMED, ret:%d\n", __func__, ret);

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

	dev_info(&info->client->dev, "%s: key led on:%d \n", __func__, on);

	if(!info->keyled_vreg){
		dev_info(&info->client->dev, "%s: failed L23 is null \n", __func__);
	}else{
		if(on){
			if(regulator_is_enabled(info->keyled_vreg)){
				dev_info(&info->client->dev, "L23 already enabled, error, %d \n", __LINE__);
			}else{
				ret = regulator_enable(info->keyled_vreg);
				if (ret) {
					dev_info(&info->client->dev, "L23 enable failed rc=%d\n", ret);
					return 2;
				}
			}
		}else{
			if(regulator_is_enabled(info->keyled_vreg)){
				ret = regulator_disable(info->keyled_vreg);
				if (ret) {
					dev_info(&info->client->dev, "L23 disable failed rc=%d\n", ret);
					return 3;
				}
			}else{
				dev_info(&info->client->dev, "L23 already disabled, error, %d \n", __LINE__);
			}
		}
	}

	return ret;
}
#endif

static void fts_power_ctrl(struct fts_ts_info *info, bool enable)
{
	struct device *dev = &info->client->dev;
	int retval, i;

	if (info->dt_data->external_ldo > 0) {
		retval = gpio_direction_output(info->dt_data->external_ldo, enable);
		dev_info(dev, "%s: sensor_en[3.3V][%d] is %s[%s]\n",
				__func__, info->dt_data->external_ldo,
				enable ? "enabled" : "disabled", (retval < 0) ? "NG" : "OK");
	}

	if (enable) {
		/* Enable regulators according to the order */
		for (i = 0; i < info->dt_data->num_of_supply; i++) {
			if (regulator_is_enabled(info->supplies[i].consumer)) {
				dev_err(dev, "%s: %s is already enabled\n", __func__,
					info->supplies[i].supply);
			} else {
				retval = regulator_enable(info->supplies[i].consumer);
				if (retval) {
					dev_err(dev, "%s: Fail to enable regulator %s[%d]\n",
						__func__, info->supplies[i].supply, retval);
					goto err;
				}
				dev_info(dev, "%s: %s is enabled[OK]\n",
					__func__, info->supplies[i].supply);
			}
		}

	} else {
		/* Disable regulator */
		for (i = 0; i < info->dt_data->num_of_supply; i++) {
			if (regulator_is_enabled(info->supplies[i].consumer)) {
				retval = regulator_disable(info->supplies[i].consumer);
				if (retval) {
					dev_err(dev, "%s: Fail to disable regulator %s[%d]\n",
						__func__, info->supplies[i].supply, retval);
					goto err;
				}
				dev_info(dev, "%s: %s is disabled[OK]\n",
					__func__, info->supplies[i].supply);
			} else {
				dev_err(dev, "%s: %s is already disabled\n", __func__,
					info->supplies[i].supply);
			}
		}

	}

	return;

err:
	if (enable) {
		enable = 0;
		for (i = 0; i < info->dt_data->num_of_supply; i++) {
			if (regulator_is_enabled(info->supplies[i].consumer)) {
				retval = regulator_disable(info->supplies[i].consumer);
				dev_err(dev, "%s: %s is disabled[%s]\n",
						__func__, info->supplies[i].supply,
						(retval < 0) ? "NG" : "OK");
			}
		}

		if (info->dt_data->external_ldo > 0) {
			retval = gpio_direction_output(info->dt_data->external_ldo, enable);
			dev_err(dev, "%s: sensor_en[3.3V][%d] is disabled[%s]\n",
					__func__, info->dt_data->external_ldo,
					(retval < 0) ? "NG" : "OK");
		}
	} else {
		enable = 1;
		for (i = 0; i < info->dt_data->num_of_supply; i++) {
			if (!regulator_is_enabled(info->supplies[i].consumer)) {
				retval = regulator_enable(info->supplies[i].consumer);
				dev_err(dev, "%s: %s is enabled[%s]\n",
						__func__, info->supplies[i].supply,
						(retval < 0) ? "NG" : "OK");
			}
		}

		if (info->dt_data->external_ldo > 0) {
			retval = gpio_direction_output(info->dt_data->external_ldo, enable);
			dev_err(dev, "%s: sensor_en[3.3V][%d] is enabled[%s]\n",
					__func__, info->dt_data->external_ldo,
					(retval < 0) ? "NG" : "OK");
		}
	}
}

#if defined(CONFIG_SECURE_TOUCH)
static void fts_secure_touch_init(struct fts_ts_info *info)
{
	dev_err(&info->client->dev, "%s: SECURETOUCH_INIT\n",__func__);
	init_completion(&info->st_powerdown);
#ifdef ST_INT_COMPLETE
	init_completion(&info->st_interrupt);
#endif
}
#endif

#ifdef CONFIG_OF
static int fts_parse_dt(struct device *dev,
		struct fts_device_tree_data *dt_data)
{
	struct device_node *np = dev->of_node;
	int rc;
	u32 coords[2];
	u32 func[2];
	int i;

	/* additional regulator info */
	rc = of_property_read_string(np, "stm,sub_pmic", &dt_data->sub_pmic);
	if (rc < 0)
		dev_info(dev, "%s: Unable to read stm,sub_pmic\n", __func__);

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

	rc = of_property_read_u32(np, "stm,supply-num", &dt_data->num_of_supply);
	if (dt_data->num_of_supply > 0) {
		dt_data->name_of_supply = kzalloc(sizeof(char *) * dt_data->num_of_supply, GFP_KERNEL);
		for (i = 0; i < dt_data->num_of_supply; i++) {
			rc = of_property_read_string_index(np, "stm,supply-name",
				i, &dt_data->name_of_supply[i]);
			if (rc && (rc != -EINVAL)) {
				dev_err(dev, "%s: Unable to read %s\n", __func__,
						"stm,supply-name");
				return rc;
			}
			dev_info(dev, "%s: supply%d: %s\n", __func__, i, dt_data->name_of_supply[i]);
		}
	}

	rc = of_property_read_u32_array(np, "stm,support_func", func, 2);
	if (rc < 0) {
		dev_info(dev, "%s: Unable to read stm,tsp-coords\n", __func__);
		return rc;
	}

	dt_data->support_hover = func[0];
	dt_data->support_mshover = func[1];

	/* extra config value
	 * @config[0] : Can set pmic regulator voltage.
	 * @config[1][2][3] is not fixed.
	 */
	rc = of_property_read_u32_array(np, "stm,tsp-extra_config", dt_data->extra_config, 4);
	if (rc < 0)
		dev_info(dev, "%s: Unable to read stm,tsp-extra_config\n", __func__);

	/* project info */
	rc = of_property_read_string(np, "stm,tsp-project", &dt_data->project);
	if (rc < 0) {
		dev_info(dev, "%s: Unable to read stm,tsp-project\n", __func__);
		dt_data->project = "0";
	}

	if (dt_data->extra_config[2] > 0)
		pr_err("%s: OCTA ID = %d\n", __func__, gpio_get_value(dt_data->extra_config[2]));

/* TB project side gesture */

#ifdef FTS_SUPPORT_SIDE_GESTURE
		dt_data->support_sidegesture = true;
#endif


	pr_err("%s: power= %d[%s], tsp_int= %d, X= %d, Y= %d, project= %s, config[%d][%d][%d][%d], support_hover[%d], mshover[%d]\n",
		__func__, dt_data->external_ldo, dt_data->sub_pmic, dt_data->irq_gpio,
			dt_data->max_x, dt_data->max_y, dt_data->project, dt_data->extra_config[0],
			dt_data->extra_config[1],dt_data->extra_config[2],dt_data->extra_config[3],
			dt_data->support_hover, dt_data->support_mshover);

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
	pr_info("%s\n", __func__);

	ret = gpio_request(info->dt_data->irq_gpio, "stm,irq_gpio");
	if (ret) {
		pr_err("%s: unable to request irq_gpio [%d]\n",
				__func__, info->dt_data->irq_gpio);
		return;
	}

	if (info->dt_data->external_ldo > 0) {
		ret = gpio_request(info->dt_data->external_ldo, "stm,external_ldo");
		if (ret) {
			pr_err("%s: unable to request external_ldo [%d]\n",
					__func__, info->dt_data->external_ldo);
			return;
		}
	}

#ifdef FTS_SUPPORT_TOUCH_KEY
	pr_info("%s: keyled L23 setting, %d \n", __func__,__LINE__);

	info->keyled_vreg = regulator_get(&info->client->dev,"vdd_keyled");
	if (IS_ERR(info->keyled_vreg)) {
		pr_err("%s: failed L23 request error \n", __func__);
		info->keyled_vreg = NULL;
		return;
	}
	ret = regulator_set_voltage(info->keyled_vreg, 3300000, 3300000);
	if (ret){
		pr_err("%s: L23 3.3V setting error\n", __func__);
		info->keyled_vreg = NULL;
	}
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
#ifdef SEC_TSP_FACTORY_TEST
	int ret;
#endif
/*
	if (boot_mode_recovery == 1){
		dev_err(&client->dev, "%s: recovery mode\n", __func__);
		return -EIO;
	}
*/

	if (!get_lcd_attached("GET")){
		dev_err(&client->dev, "%s: LCD is not attached\n", __func__);
		return -EIO;
	}

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
	} else	{
		dt_data = client->dev.platform_data;
		printk(KERN_ERR "TSP failed to align dtsi %s", __func__);
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

	info->client_sub = i2c_new_dummy(client->adapter, FTS_SLAVE_ADDRESS_SUB);
	if (!info->client_sub) {
		dev_err(&client->dev, "Fail to register sub client[0x%x]\n",
			 FTS_SLAVE_ADDRESS_SUB);
		retval = -ENODEV;
		goto err_create_client_sub;
	}

#ifdef USE_OPEN_DWORK
	INIT_DELAYED_WORK(&info->open_work, fts_open_work);
#endif

	INIT_DELAYED_WORK(&info->cam_work, fts_cam_work);
	info->delay_time = 300;

#if defined(USE_RESET_WORK_EXIT_LOWPOWERMODE) || defined(ESD_CHECK)
	INIT_DELAYED_WORK(&info->reset_work, fts_reset_work);
#endif

#ifdef TSP_RAWDATA_DUMP
	INIT_DELAYED_WORK(&info->ghost_check, ghost_touch_check);
	p_ghost_check = &info->ghost_check;
#endif
#ifdef SEC_TSP_FACTORY_TEST
	INIT_DELAYED_WORK(&info->cover_cmd_work, clear_cover_cmd_work);
#endif

	if (info->dt_data->support_hover) {
		dev_info(&info->client->dev, "FTS Support Hover Event \n");
	} else {
		dev_info(&info->client->dev, "FTS Not support Hover Event \n");
	}

	fts_request_gpio(info);

	info->supplies = kzalloc(
		sizeof(struct regulator_bulk_data) * info->dt_data->num_of_supply, GFP_KERNEL);
	if (!info->supplies) {
		dev_err(&client->dev,
			"%s: Failed to alloc mem for supplies\n", __func__);
		retval = -ENOMEM;
		goto err_alloc_regulator;
	}
	for (i = 0; i < info->dt_data->num_of_supply; i++)
		info->supplies[i].supply = info->dt_data->name_of_supply[i];

	retval = regulator_bulk_get(&client->dev, info->dt_data->num_of_supply,
				 info->supplies);

	ret = regulator_set_voltage(info->supplies[0].consumer,3300000, 3300000);
	if (ret) dev_info(&info->client->dev, "%s, 3.3 set_vtg failed rc=%d\n", __func__, ret);

	fts_power_ctrl(info, true);
	msleep(10);
#ifdef TSP_BOOSTER
		info->tsp_booster = kzalloc(sizeof(struct input_booster), GFP_KERNEL);
		if (!info->tsp_booster) {
			dev_err(&client->dev,
				"%s: Failed to alloc mem for tsp_booster\n", __func__);
			goto err_get_tsp_booster;
		} else {
			input_booster_init_dvfs(info->tsp_booster, INPUT_BOOSTER_ID_TSP);
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

	temp = get_lcd_attached("GET");
	info->lcd_id[2] = temp & 0xFF;
	info->lcd_id[1] = (temp >> 8) & 0xFF;
	info->lcd_id[0] = (temp >> 16) & 0xFF;
	info->panel_revision = info->lcd_id[1] >> 4;
	info->edge_grip_mode = true;	// default;

	info->irq_enabled = false;
	info->touch_stopped = false;
	info->stop_device = fts_stop_device;
	info->start_device = fts_start_device;
	info->fts_command = fts_command;
	info->fts_read_reg = fts_read_reg;
	info->fts_write_reg = fts_write_reg;
	info->fts_systemreset = fts_systemreset;
	info->fts_get_version_info = fts_get_version_info;
	info->fts_wait_for_ready = fts_wait_for_ready;
	info->fts_power_ctrl = fts_power_ctrl;
	info->fts_write_to_string = fts_write_to_string;
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

	set_bit(KEY_REAR_CAMERA_DETECTED, info->input_dev->keybit);
	set_bit(KEY_FRONT_CAMERA_DETECTED, info->input_dev->keybit);
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
	if (info->dt_data->support_sidegesture)
		set_bit(KEY_SIDE_GESTURE, info->input_dev->keybit);
#endif

	input_mt_init_slots(info->input_dev, FINGER_MAX, INPUT_MT_DIRECT);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
			0, info->dt_data->max_x, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
			0, info->dt_data->max_y, 0, 0);
	mutex_init(&info->lock);
	mutex_init(&(info->device_mutex));
	mutex_init(&info->i2c_mutex);
	wake_lock_init(&info->wakelock, WAKE_LOCK_SUSPEND, "fts_wakelock");

	info->enabled = false;
	mutex_lock(&info->lock);

	if (info->client->addr == FTS_SLAVE_ADDRESS)
		info->slave_addr = info->client->addr;
	else
		info->slave_addr = FTS_SLAVE_ADDRESS_SUB;

/* Try To I2C access with FTS Touch IC */
	retval = fts_init(info);
	if (retval < 0) {
		if (info->slave_addr == FTS_SLAVE_ADDRESS)
			info->slave_addr = FTS_SLAVE_ADDRESS_SUB;
		else
			info->slave_addr = FTS_SLAVE_ADDRESS;

		retval = fts_init(info);
	}
	info->reinit_done = true;

	mutex_unlock(&info->lock);
	if (retval < 0) {
		dev_err(&info->client->dev, "FTS fts_init fail!\n");
		goto err_fts_init;
	}

	dev_info(&info->client->dev, "%s: addr is 0x%02X\n",
			__func__, info->slave_addr);

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
#ifdef FTS_SUPPORT_SIDE_GESTURE
	wake_lock_init(&report_wake_lock, WAKE_LOCK_SUSPEND,"report_wake_lock");
#endif
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
	INIT_LIST_HEAD(&info->cmd_list_head);

	info->cmd_buffer_size = 0;
	for (i = 0; i < ARRAY_SIZE(stm_ft_cmds); i++){
		list_add_tail(&stm_ft_cmds[i].list, &info->cmd_list_head);
		if(stm_ft_cmds[i].cmd_name)
			info->cmd_buffer_size += strlen(stm_ft_cmds[i].cmd_name) + 1;
	}
	info->cmd_result = kzalloc(info->cmd_buffer_size, GFP_KERNEL);
	if(!info->cmd_result){
		tsp_debug_err(true, &info->client->dev, "FTS Failed to allocate cmd result\n");
		goto err_alloc_cmd_result;
	}

	mutex_init(&info->cmd_lock);
	info->cmd_is_running = false;

	info->fac_dev_ts = device_create(sec_class, NULL, FTS_ID0, info, "tsp");
	if (IS_ERR(info->fac_dev_ts)) {
		dev_err(&info->client->dev, "FTS Failed to create device for the sysfs\n");
		goto err_sysfs;
	}

	dev_set_drvdata(info->fac_dev_ts, info);

	ret = sysfs_create_group(&info->fac_dev_ts->kobj,
				 &sec_touch_factory_attr_group);
	if (ret < 0) {
		dev_err(&info->client->dev, "FTS Failed to create sysfs group\n");
		goto err_sysfs;
	}
#endif

	ret = sysfs_create_link(&info->fac_dev_ts->kobj,
		&info->input_dev->dev.kobj, "input");
	if (ret < 0) {
		dev_err(&info->client->dev,
			"%s: Failed to create input symbolic link\n",
			__func__);
	}

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->dt_data->support_mskey) {
		//info->fac_dev_tk = sec_device_create(info, "sec_touchkey");
		info->fac_dev_tk = device_create(sec_class, NULL, 0, info, "sec_touchkey");
		if (IS_ERR(info->fac_dev_tk))
			tsp_debug_err(true, &info->client->dev, "Failed to create device for the touchkey sysfs\n");
		else {
			dev_set_drvdata(info->fac_dev_tk, info);

			retval = sysfs_create_group(&info->fac_dev_tk->kobj,
						 &sec_touchkey_factory_attr_group);
			if (retval < 0)
				tsp_debug_err(true, &info->client->dev, "FTS Failed to create sysfs group\n");
			else {
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
		ret = sysfs_create_file(&info->input_dev->dev.kobj,
				&attrs[i].attr);
		if (ret < 0) {
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
err_sysfs:
	if(info->cmd_result)
		kfree(info->cmd_result);
err_alloc_cmd_result:
	if (info->irq_enabled)
		fts_irq_enable(info, false);
#endif

err_enable_irq:
	input_unregister_device(info->input_dev);
	info->input_dev = NULL;

err_register_input:
	if (info->input_dev)
		input_free_device(info->input_dev);

err_fts_init:
	if (info->cx_data)
		kfree(info->cx_data);
	if (info->pFrame)
		kfree(info->pFrame);
	wake_lock_destroy(&info->wakelock);
err_input_allocate_device:
#ifdef TSP_BOOSTER
	kfree(info->tsp_booster);
err_get_tsp_booster:
#endif
	fts_power_ctrl(info, false);
	kfree(info->supplies);
err_alloc_regulator:
	p_ghost_check = NULL;
err_create_client_sub:
	kfree(info->noise_param);
err_alloc_noise_param:
	kfree(info);

	return retval;
}

static int fts_remove(struct i2c_client *client)
{
	struct fts_ts_info *info = i2c_get_clientdata(client);
#if defined(CONFIG_SECURE_TOUCH)
	int i = 0;
#endif
	dev_info(&info->client->dev, "FTS removed \n");

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, FLUSHBUFFER);

	fts_irq_enable(info, false);

#if defined(CONFIG_SECURE_TOUCH)
	for (i = 0; i < ARRAY_SIZE(attrs); i++) {
		sysfs_remove_file(&info->input_dev->dev.kobj,
				&attrs[i].attr);
	}
#endif
	input_mt_destroy_slots(info->input_dev);

#ifdef SEC_TSP_FACTORY_TEST
	sysfs_remove_group(&info->fac_dev_ts->kobj,
			   &sec_touch_factory_attr_group);

	device_destroy(sec_class, FTS_ID0);

	if (info->cmd_result)
		kfree(info->cmd_result);

	list_del(&info->cmd_list_head);

	mutex_destroy(&info->cmd_lock);

	if (info->cx_data)
		kfree(info->cx_data);

	if (info->pFrame)
		kfree(info->pFrame);
#endif

	mutex_destroy(&info->lock);

	input_unregister_device(info->input_dev);
	info->input_dev = NULL;
#ifdef TSP_BOOSTER
	kfree(info->tsp_booster);
#endif
	fts_power_ctrl(info, false);

	kfree(info);

	return 0;
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
	int retval;

	dev_dbg(&info->client->dev, "%s\n", __func__);

#ifdef USE_OPEN_DWORK
	schedule_delayed_work(&info->open_work,
			      msecs_to_jiffies(TOUCH_OPEN_DWORK_TIME));
#else
	retval = fts_start_device(info);
	if (retval < 0) {
		dev_err(&info->client->dev,
			"%s: Failed to start device\n", __func__);
		goto out;
	}
#endif
	tsp_debug_err(true, &info->client->dev, "FTS cmd after wakeup : h%d \n",
			info->retry_hover_enable_after_wakeup);

	if(info->retry_hover_enable_after_wakeup == 1){
		unsigned char regAdd[4] = {0xB0, 0x01, 0x29, 0x41};
		fts_write_reg(info, &regAdd[0], 4);
		fts_command(info, FTS_CMD_HOVER_ON);
		info->hover_ready = false;
		info->hover_enabled = true;
	}
out:
	return retval;
}

static void fts_input_close(struct input_dev *dev)
{
	struct fts_ts_info *info = input_get_drvdata(dev);

	dev_dbg(&info->client->dev, "%s\n", __func__);

#ifdef USE_OPEN_DWORK
	cancel_delayed_work(&info->open_work);
#endif

	fts_stop_device(info);

	info->retry_hover_enable_after_wakeup = 0;
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
	info->pFrame =
	    kzalloc(info->SenseChannelLength * info->ForceChannelLength * 2,
		    GFP_KERNEL);
	if (info->pFrame == NULL) {
		dev_info(&info->client->dev, "FTS pFrame kzalloc Failed\n");
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
	fts_wait_for_ready(info);

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

void fts_release_all_finger(struct fts_ts_info *info)
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
	input_report_key(info->input_dev, KEY_REAR_CAMERA_DETECTED, 0);
	input_report_key(info->input_dev, KEY_FRONT_CAMERA_DETECTED, 0);
	input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);

#ifdef FTS_SUPPORT_SIDE_GESTURE
	if (info->dt_data->support_sidegesture)
		input_report_key(info->input_dev, KEY_SIDE_GESTURE, 0);
#endif

	input_sync(info->input_dev);

	input_mt_slot(info->input_dev, 0);

#ifdef TSP_BOOSTER
	if (info->tsp_booster->dvfs_set)
		info->tsp_booster->dvfs_set(info->tsp_booster, -1);
#endif
}

#if defined(CONFIG_SECURE_TOUCH)
static void fts_secure_touch_stop(struct fts_ts_info *info, int blocking)
{
	dev_err(&info->client->dev, "%s: SECURETOUCH_STOP\n",__func__);
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
		set_mainscreen_disable_cmd((void *)info,0);
#endif
		if(info->edge_grip_mode == false){
			dev_info(&info->client->dev, "%s edge grip enable flag:%d, clear 1\n", __func__, info->edge_grip_mode);
			longpress_grip_enable_mode(info, 1);		// default
			grip_check_enable_mode(info, 1);			// default
		}
#endif
		dev_info(&info->client->dev, "%s lowpower flag:%d\n", __func__, info->lowpower_flag);

		info->fts_power_mode = FTS_POWER_STATE_LOWPOWER;

		fts_command(info, FLUSHBUFFER);

		//fts_command(info, FTS_CMD_HOVER_ON);
		//fts_delay(20);

#ifdef FTS_SUPPORT_SIDE_GESTURE
		if (info->dt_data->support_sidegesture) {
			fts_enable_feature(info, FTS_FEATURE_SIDE_GUSTURE, true);
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
		fts_power_ctrl(info, false);
		info->fts_power_mode = FTS_POWER_STATE_POWER_DOWN;
	}
out:
	mutex_unlock(&info->device_mutex);

	return 0;
}

static int fts_start_device(struct fts_ts_info *info)
{
	unsigned short addr = FTS_CMD_STRING_ACCESS;
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
				ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
				if (ret < 0)
					dev_err(&info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
			}
			goto out;
		}
#ifdef USE_RESET_WORK_EXIT_LOWPOWERMODE
		schedule_work(&info->reset_work.work);
		goto out;
#endif

#ifdef FTS_ADDED_RESETCODE_IN_LPLM

		disable_irq(info->irq);
		info->reinit_done = false;

		fts_reinit(info);

		info->reinit_done = true;
		enable_irq(info->irq);

		if (info->fts_mode) {
			ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
			if (ret < 0)
				dev_err(&info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		}
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

		if (info->fts_mode) {
			ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
			if (ret < 0)
				dev_err(&info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		}
	}
out:
	mutex_unlock(&info->device_mutex);

	info->fts_power_mode = FTS_POWER_STATE_ACTIVE;

	return 0;
}

#ifdef TSP_RAWDATA_DUMP
int rawdata_read_lock;
static void ghost_touch_check(struct work_struct *work)
{
	struct fts_ts_info *info = container_of(work, struct fts_ts_info,
						ghost_check.work);
	int i;

	if(rawdata_read_lock == 1)
		dev_err(&info->client->dev, "%s, ## checking.. ignored.\n", __func__);

	rawdata_read_lock = 1;
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

	rawdata_read_lock = 0;

}

void tsp_dump(void)
{
#ifdef CONFIG_SAMSUNG_LPM_MODE
	if (poweroff_charging)
		return;
#endif
	if (!p_ghost_check)
		return;

	printk(KERN_ERR "FTS %s: start \n", __func__);
	schedule_delayed_work(p_ghost_check, msecs_to_jiffies(100));
}
#else
void tsp_dump(void)
{
	printk(KERN_ERR "FTS %s: not support\n", __func__);
}
#endif


#if defined(USE_RESET_WORK_EXIT_LOWPOWERMODE) || defined(ESD_CHECK)
static void fts_reset_work(struct work_struct *work)
{
	struct fts_ts_info *info = container_of(work, struct fts_ts_info,
						reset_work.work);
#ifdef ESD_CHECK
	bool temp_lpm;

	temp_lpm = info->lowpower_mode;
	info->lowpower_mode = 0;

	dev_dbg(&info->client->dev, "%s, reset IC off, lpm:%d\n", __func__, temp_lpm);
	fts_stop_device(info);

	msleep(100);
	dev_dbg(&info->client->dev, "%s, reset IC on\n", __func__);
	if (fts_start_device(info) < 0) {
		dev_err(&info->client->dev,
			"%s: Failed to start device\n", __func__);
	}

	info->lowpower_mode = temp_lpm;

#else
	int ret;
	unsigned short addr = FTS_CMD_STRING_ACCESS;
	dev_info(&info->client->dev, "%s\n", __func__);

	if (device_may_wakeup(&info->client->dev))
		disable_irq_wake(info->client->irq);

	fts_interrupt_set(info, INT_DISABLE);
	disable_irq(info->irq);

	fts_command(info, FLUSHBUFFER);
	fts_command(info, SLEEPIN);
	fts_release_all_finger(info);
#ifdef FTS_SUPPORT_TOUCH_KEY
	fts_release_all_key(info);
#endif

	info->touch_stopped = true;
	fts_power_ctrl(info, false);

	fts_power_ctrl(info, true);

	info->touch_stopped = false;
	info->reinit_done = false;

	fts_reinit(info);

	info->reinit_done = true;

	enable_irq(info->irq);

	if (info->fts_mode) {
		ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
		if (ret < 0)
			dev_err(&info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
	}

	info->fts_power_mode = FTS_POWER_STATE_ACTIVE;
#endif
}
#endif

#ifdef CONFIG_PM
static int fts_ts_suspend(struct device *dev)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	if (info->lowpower_mode)
		init_completion(&info->resume_done);

	info->fts_power_mode = FTS_POWER_STATE_LOWPOWER_SUSPEND;

	return 0;
}

static int fts_ts_resume(struct device *dev)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	if (info->lowpower_mode)
		complete_all(&info->resume_done);

	info->fts_power_mode = FTS_POWER_STATE_LOWPOWER_RESUME;

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

