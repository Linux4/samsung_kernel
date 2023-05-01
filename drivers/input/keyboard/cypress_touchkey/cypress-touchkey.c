/*
 * cypress_touchkey.c - Platform data for cypress touchkey driver
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define SEC_TOUCHKEY_DEBUG
/* #define SEC_TOUCHKEY_VERBOSE_DEBUG */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/earlysuspend.h>
#include <linux/i2c/cypress_touchkey_234.h>
#include <linux/regulator/consumer.h>
#include <asm/mach-types.h>
#include <mach/msm8930-gpio.h>

#include "cypress_tkey_fw.h"
#include "next_fw_update.h"

#ifdef CONFIG_LEDS_CLASS
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/leds.h>
#endif

/* use extra keys : recent key, home key */
#undef USE_EXTRA_KEY
/* use auto calibration command */
#if defined(CONFIG_MACH_SERRANO)
#define USE_AUTO_CAL
#else
#undef USE_AUTO_CAL
#endif
/* use brightness level */
#undef USE_BRIGHTNESS_LEVEL

#define CYPRESS_GEN		0X00
#define CYPRESS_FW_VER		0X01
#define CYPRESS_MODULE_VER	0X02
#define CYPRESS_2ND_HOST	0X03
#define CYPRESS_THRESHOLD	0X04
#define CYPRESS_AUTO_CAL_FLG	0X05

#define CYPRESS_LED_ON		0X10
#define CYPRESS_LED_OFF		0X20
#define CYPRESS_DATA_UPDATE	0X40
#define CYPRESS_AUTO_CAL	0X50
#define CYPRESS_LED_CONTROL_ON	0X60
#define CYPRESS_LED_CONTROL_OFF	0X70
#define CYPRESS_SLEEP		0X80

#ifdef CONFIG_LEDS_CLASS
#define TOUCHKEY_BACKLIGHT	"button-backlight"
#endif

/* bit masks*/
#define PRESS_BIT_MASK		0X08
#define KEYCODE_BIT_MASK	0X07

#define TOUCHKEY_LOG(k, v) dev_notice(&info->client->dev, "key[%d] %d\n", k, v);
#define FUNC_CALLED dev_notice(&info->client->dev, "%s: called.\n", __func__);

#define NUM_OF_RETRY_UPDATE	3
#define NUM_OF_KEY		4

struct cypress_touchkey_info {
	struct i2c_client			*client;
	struct cypress_touchkey_platform_data	*pdata;
	struct input_dev			*input_dev;
	struct early_suspend			early_suspend;
	struct device	*dev;
	char			phys[32];
	unsigned char			keycode[NUM_OF_KEY];
	u8			sensitivity[NUM_OF_KEY];
	int			irq;
	void (*power_onoff)(int);
	int			touchkey_update_status;
	u8			ic_vendor;
/*
* ic_vendor value == 0x00 : cypress IC,
* ic_vendor value == 0xA1 : nextchip IC
*/
	u8	module_ver;
	u8	ic_fw_ver;
#ifdef CONFIG_LEDS_CLASS
	struct led_classdev			leds;
	enum led_brightness			brightness;
	struct mutex			touchkey_led_mutex;
	struct mutex			fw_lock;
	struct workqueue_struct			*led_wq;
	struct work_struct			led_work;
#endif
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cypress_touchkey_early_suspend(struct early_suspend *h);
static void cypress_touchkey_late_resume(struct early_suspend *h);
#endif

static int touchkey_led_status;
static int touchled_cmd_reversed;

#ifdef CONFIG_LEDS_CLASS
static void cypress_touchkey_led_work(struct work_struct *work)
{
	struct cypress_touchkey_info *info =
		container_of(work, struct cypress_touchkey_info, led_work);
	u8 buf;
	int ret;

	if (info->brightness == LED_OFF)
		buf = CYPRESS_LED_OFF;
	else
		buf = CYPRESS_LED_ON;

	dev_info(&info->client->dev, "%s: led : %s\n", __func__,
				buf == CYPRESS_LED_ON ? "on" : "off");

	mutex_lock(&info->touchkey_led_mutex);

	ret = i2c_smbus_write_byte_data(info->client, CYPRESS_GEN, buf);
	if (ret < 0) {
		dev_err(&info->client->dev,
			"%s: [Touchkey] i2c write error [%d]\n",
			__func__, ret);
		touchled_cmd_reversed = 1;
	}

	touchkey_led_status = buf;

	mutex_unlock(&info->touchkey_led_mutex);
}

static void cypress_touchkey_brightness_set(struct led_classdev *led_cdev,
			enum led_brightness brightness)
{
	/* Must not sleep, use a workqueue if needed */
	struct cypress_touchkey_info *info =
		container_of(led_cdev, struct cypress_touchkey_info, leds);

	info->brightness = brightness;

	queue_work(info->led_wq, &info->led_work);
}
#endif

#ifdef USE_BRIGHTNESS_LEVEL
static int vol_mv_level = 33;

static void change_touch_key_led_voltage(int vol_mv)
{
	struct regulator *tled_regulator;
	int ret;
	vol_mv_level = vol_mv;

	tled_regulator = regulator_get(NULL, "8921_l10");
	if (IS_ERR(tled_regulator)) {
		pr_err("%s: failed to get resource %s\n", __func__,
			"touch_led");
		return;
	}
	ret = regulator_set_voltage(tled_regulator,
		vol_mv * 100000, vol_mv * 100000);
	if (ret)
		printk(KERN_ERR"error setting voltage\n");
	regulator_put(tled_regulator);
}

static ssize_t brightness_control(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int data;

	if (sscanf(buf, "%d\n", &data) == 1) {
		printk(KERN_ERR"[TouchKey] touch_led_brightness: %d\n", data);
		change_touch_key_led_voltage(data);
	} else {
		printk(KERN_ERR "[TouchKey] touch_led_brightness Error\n");
	}
	return size;
}

static ssize_t brightness_level_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int count;

	count = snprintf(buf, 10, "%d\n", vol_mv_level);

	printk(KERN_DEBUG "[TouchKey] Touch LED voltage = %d\n", vol_mv_level);
	return count;
}
#endif

static irqreturn_t cypress_touchkey_interrupt(int irq, void *dev_id)
{
	struct cypress_touchkey_info *info = dev_id;
	u8 buf[10] = {0,};
	int code;
	int press;
	int ret;

	ret = gpio_get_value(info->pdata->gpio_int);
	/*if (ret) {
		if (gpio_get_value(GPIO_VIB_EN) && machine_is_EXPRESS()) {
			dev_err(&info->client->dev,
					"tkey intr && vibe on.\n");
		} else {
			dev_err(&info->client->dev,
					"not real interrupt (%d).\n", ret);
			goto out;
		}
	}*/

#if defined(SEC_TOUCHKEY_VERBOSE_DEBUG)
	ret = i2c_smbus_read_i2c_block_data(info->client,
			CYPRESS_GEN, ARRAY_SIZE(buf), buf);
	if (ret != ARRAY_SIZE(buf)) {
		dev_err(&info->client->dev, "interrupt failed with %d.\n", ret);
		goto out;
	}
	print_hex_dump(KERN_DEBUG, "cypress_touchkey: ",
			DUMP_PREFIX_OFFSET, 32, 1, buf, 10, false);
#else
	buf[0] = i2c_smbus_read_byte_data(info->client, CYPRESS_GEN);
	if (buf[0] < 0) {
		dev_err(&info->client->dev, "interrupt failed with %d.\n", ret);
		goto out;
	}
#endif
	press = !(buf[0] & PRESS_BIT_MASK);
	code = (int)(buf[0] & KEYCODE_BIT_MASK) - 1;
	dev_dbg(&info->client->dev,
		"[TouchKey]press=%d, code=%d\n", press, code);

	if (code < 0) {
		dev_err(&info->client->dev,
				"not profer interrupt 0x%2X.\n", buf[0]);
		goto out;
	}

	input_report_key(info->input_dev, info->keycode[code], press);
	input_sync(info->input_dev);
#if defined(SEC_TOUCHKEY_DEBUG)
	TOUCHKEY_LOG(info->keycode[code], press);
#endif

out:
	return IRQ_HANDLED;
}

static void cypress_touchkey_con_hw(struct cypress_touchkey_info *dev_info,
								bool flag)
{
	struct cypress_touchkey_info *info =  dev_info;

	info->pdata->touchkey_led_en(flag);

 #if defined(SEC_TOUCHKEY_DEBUG)
	dev_notice(&info->client->dev,
			"%s : called with flag %d.\n", __func__, flag);
#endif
}

#ifdef USE_AUTO_CAL
static int cypress_touchkey_auto_cal(struct cypress_touchkey_info *dev_info)
{
	struct cypress_touchkey_info *info = dev_info;
	u8 data[6] = { 0, };
	int count = 0;
	int ret = 0;
	unsigned short retry = 0;
	while (retry < 3) {

		ret = i2c_smbus_read_i2c_block_data(info->client,
				CYPRESS_GEN, 4, data);
		if (ret < 0) {
			printk(KERN_ERR "[TouchKey]i2c read fail.\n");
	return ret;
}
		data[0] = 0x50;
		data[3] = 0x01;

		count = i2c_smbus_write_i2c_block_data(info->client,
				CYPRESS_GEN, 4, data);
		printk(KERN_DEBUG
				"[TouchKey] data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		msleep(130);

		ret = i2c_smbus_read_i2c_block_data(info->client,
				CYPRESS_GEN, 6, data);

		if ((data[5] & 0x80)) {
			printk(KERN_DEBUG "[Touchkey] autocal Enabled\n");
			break;
		} else {
			printk(KERN_DEBUG "[Touchkey] autocal disabled, retry %d\n",
					retry);
		}
		retry = retry + 1;
	}

	if (retry == 3)
		printk(KERN_DEBUG "[Touchkey] autocal failed\n");

	return count;
}
#endif

static int cypress_touchkey_led_on(struct cypress_touchkey_info *dev_info)
{
	struct cypress_touchkey_info *info = dev_info;
	u8 buf = CYPRESS_LED_ON;

	int ret;
	ret = i2c_smbus_write_byte_data(info->client, CYPRESS_GEN, buf);
	if (ret < 0) {
		dev_err(&info->client->dev,
				"[Touchkey] i2c write error [%d]\n", ret);
	}
	return ret;
}

static int cypress_touchkey_led_off(struct cypress_touchkey_info *dev_info)
{
	struct cypress_touchkey_info *info = dev_info;
	u8 buf = CYPRESS_LED_OFF;

	int ret;
	ret = i2c_smbus_write_byte_data(info->client, CYPRESS_GEN, buf);
	if (ret < 0) {
		dev_err(&info->client->dev,
				"[Touchkey] i2c write error [%d]\n", ret);
	}
	return ret;
}

static ssize_t touch_version_read(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	u8 data;
	int count;

	msleep(40);
	data = i2c_smbus_read_byte_data(info->client, CYPRESS_FW_VER);
	count = snprintf(buf, 20, "0x%02x\n", data);

	dev_dbg(&info->client->dev,
		"[TouchKey] %s : FW Ver 0x%02x\n", __func__, data);

	return count;
}

static ssize_t touch_version_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int count;
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);

	if (info->ic_vendor == 0xA1)
		count = snprintf(buf, 20, "0x%02x\n", BIN_FW_VERSION_NEXT);
	else
		count = snprintf(buf, 20, "0x%02x\n", BIN_FW_VERSION);
	return count;
}

static ssize_t touchkey_firm_status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	int count = 0;
	dev_dbg(&info->client->dev, "[TouchKey] touchkey_update_status: %d\n",
						info->touchkey_update_status);
	if (info->touchkey_update_status == 0)
		count = snprintf(buf, 20, "PASS\n");
	else if (info->touchkey_update_status == 1)
		count = snprintf(buf, 20, "Downloading\n");
	else if (info->touchkey_update_status == -1)
		count = snprintf(buf, 20, "Fail\n");
	return count;
}

static ssize_t touch_update_write(struct device *dev,
			 struct device_attribute *attr,
			 const char *buf, size_t size)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	int count = 0, ret = 0;
	int retry = NUM_OF_RETRY_UPDATE;
	char buff[16] = {0};
	u8 data;

#if defined(CONFIG_MACH_SERRANO_VZW)  || defined(CONFIG_MACH_SERRANO_USC)
		if (info->module_ver != 0x1) {
			dev_err(dev, "[TOUCHKEY] %x module does not support fw update!\n", info->module_ver);
			return size;
		}
#endif

	info->touchkey_update_status = 1;
	dev_err(dev, "[TOUCHKEY] touch_update_write!\n");

	disable_irq(info->irq);
	mutex_lock(&info->fw_lock);

	if (info->ic_vendor == 0x00) {
		while (retry--) {
			if (ISSP_main() == 0) {
				dev_err(&info->client->dev,
					"[TOUCHKEY] Update success!\n");
			#ifdef USE_AUTO_CAL
				cypress_touchkey_auto_cal(info);
			#endif
				info->touchkey_update_status = 0;
				count = 1;
				break;
			}
			dev_err(&info->client->dev,
				"[TOUCHKEY] Touchkey_update failed... retry...\n");
		}

	} else {
		printk(KERN_ERR "%s: This is nextchip Touchkey IC = %d\n",
				__func__, ret);
		while (retry--) {
			info->power_onoff(0);
			msleep(30);
			info->power_onoff(1);

			next_i2c_fw_update(info->client);

			info->power_onoff(0);
			msleep(30);
			info->power_onoff(1);
			msleep(200);

			ret = i2c_smbus_read_byte_data(info->client,
								CYPRESS_FW_VER);

			if (ret == BIN_FW_VERSION_NEXT) {
				info->touchkey_update_status = 0;
				break;
			} else {
				dev_err(&info->client->dev,
					"fw version is 0x%02x, retry!\n", ret);
			}
		}

	}

	if (retry <= 0) {
		cypress_touchkey_con_hw(info, false);
		msleep(300);
		count = 0;
		dev_err(&info->client->dev, "[TOUCHKEY]Touchkey_update fail\n");
		info->touchkey_update_status = -1;
	}

	enable_irq(info->irq);
	mutex_unlock(&info->fw_lock);
	msleep(500);

	data = i2c_smbus_read_byte_data(info->client, CYPRESS_FW_VER);
	count = snprintf(buff, sizeof(buff), "0x%02x\n", data);
	dev_err(&info->client->dev,
		"[TouchKey] %s : FW Ver 0x%02x\n", __func__, data);

	return count;
}

static ssize_t touch_led_control(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	int data;

	dev_dbg(&info->client->dev, "called %s\n", __func__);
	data = kstrtoul(buf, (int)NULL, 0);
	if (data == 1) {
		if (data)
			cypress_touchkey_led_on(info);
		else
			cypress_touchkey_led_off(info);
#if defined(SEC_TOUCHKEY_DEBUG)
		dev_dbg(&info->client->dev,
			"[TouchKey] touch_led_control : %d\n", data);
#endif
	} else
		dev_dbg(&info->client->dev, "[TouchKey] touch_led_control Error\n");

	return size;
}

static ssize_t touch_sensitivity_control(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	int ret;

	if (info->touchkey_update_status)
		return size;

	ret = i2c_smbus_write_byte_data(info->client,
			CYPRESS_GEN, CYPRESS_DATA_UPDATE);
	if (ret < 0) {
		dev_err(&info->client->dev,
			"[Touchkey] fail to CYPRESS_DATA_UPDATE.\n");
		return ret;
	}
	msleep(150);
	return size;
}

static ssize_t touchkey_back_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	static u16 back_sensitivity;
	u8 data[2] = {0,};
	int ret;

	if (info->touchkey_update_status)
		return snprintf(buf, 20, "%d\n", back_sensitivity);

	ret = i2c_smbus_read_i2c_block_data(info->client, CYPRESS_DIFF_BACK,
		ARRAY_SIZE(data), data);
	if (ret != ARRAY_SIZE(data)) {
		dev_err(&info->client->dev,
			"[TouchKey] fail to read back sensitivity.\n");
		return ret;
	}

	back_sensitivity = ((0x00FF & data[0])<<8) | data[1];

	dev_dbg(&info->client->dev, "called %s , data : %d %d\n",
			__func__, data[0], data[1]);
	return snprintf(buf, 20, "%d\n", back_sensitivity);

}

static ssize_t touchkey_menu_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	static u16 menu_sensitivity;
	u8 data[2] = {0,};
	int ret;

	if (info->touchkey_update_status)
		return snprintf(buf, 20, "%d\n", menu_sensitivity);

	ret = i2c_smbus_read_i2c_block_data(info->client, CYPRESS_DIFF_MENU,
		ARRAY_SIZE(data), data);
	if (ret != ARRAY_SIZE(data)) {
		dev_err(&info->client->dev,
			"[TouchKey] fail to read menu sensitivity.\n");
		return ret;
	}
	menu_sensitivity = ((0x00FF & data[0])<<8) | data[1];

	dev_dbg(&info->client->dev, "called %s , data : %d %d\n",
			__func__, data[0], data[1]);
	return snprintf(buf, 20, "%d\n", menu_sensitivity);


}

#ifdef USE_EXTRA_KEY
static ssize_t touchkey_home_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	static u16 home_sensitivity;
	u8 data[2] = {0,};
	int ret;

	ret = i2c_smbus_read_i2c_block_data(info->client, CYPRESS_DIFF_HOME,
		ARRAY_SIZE(data), data);
	if (ret != ARRAY_SIZE(data)) {
		dev_err(&info->client->dev,
			"[TouchKey] fail to read home sensitivity.\n");
		return ret;
	}

	home_sensitivity = ((0x00FF & data[0])<<8) | data[1];

	dev_dbg(&info->client->dev, "called %s , data : %d %d\n",
			__func__, data[0], data[1]);
	return snprintf(buf, 20, "%d\n", home_sensitivity);

}


static ssize_t touchkey_recent_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	static u16 recent_sensitivity;
	u8 data[2] = {0,};
	int ret;

	ret = i2c_smbus_read_i2c_block_data(info->client,
		CYPRESS_DIFF_RECENT, ARRAY_SIZE(data), data);
	if (ret != ARRAY_SIZE(data)) {
		dev_err(&info->client->dev,
			"[TouchKey] fail to read recent sensitivity.\n");
		return ret;
	}
	recent_sensitivity = ((0x00FF & data[0])<<8) | data[1];

	dev_dbg(&info->client->dev, "called %s , data : %d %d\n",
			__func__, data[0], data[1]);
	return snprintf(buf, 20, "%d\n", recent_sensitivity);

}
#endif

static ssize_t touchkey_raw_data0_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	static u16 raw_data0;
	u8 data[2] = {0,};
	int ret;

	if (info->touchkey_update_status)
		return snprintf(buf, 20, "%d\n", raw_data0);

	ret = i2c_smbus_read_i2c_block_data(info->client, CYPRESS_RAW_DATA_MENU,
		ARRAY_SIZE(data), data);
	if (ret != ARRAY_SIZE(data)) {
		dev_err(&info->client->dev,
			"[TouchKey] fail to read menu raw data.\n");
		return ret;
	}

	raw_data0 = ((0x00FF & data[0])<<8) | data[1];

	dev_dbg(&info->client->dev, "called %s , data : %d %d\n",
			__func__, data[0], data[1]);
	return snprintf(buf, 20, "%d\n", raw_data0);

}

static ssize_t touchkey_raw_data1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	static u16 raw_data1;
	u8 data[2] = {0,};
	int ret;

	if (info->touchkey_update_status)
		return snprintf(buf, 20, "%d\n", raw_data1);

	ret = i2c_smbus_read_i2c_block_data(info->client, CYPRESS_RAW_DATA_BACK,
		ARRAY_SIZE(data), data);
	if (ret != ARRAY_SIZE(data)) {
		dev_err(&info->client->dev,
			"[TouchKey] fail to read back raw data.\n");
		return ret;
	}

	raw_data1 = ((0x00FF & data[0])<<8) | data[1];
	dev_dbg(&info->client->dev, "called %s , data : %d %d\n",
			__func__, data[0], data[1]);
	return snprintf(buf, 20, "%d\n", raw_data1);

}

#ifdef USE_EXTRA_KEY
static ssize_t touchkey_raw_data2_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	static u16 raw_data2;
	u8 data[2] = {0,};
	int ret;

	ret = i2c_smbus_read_i2c_block_data(info->client, CYPRESS_RAW_DATA_HOME,
		ARRAY_SIZE(data), data);
	if (ret != ARRAY_SIZE(data)) {
		dev_err(&info->client->dev,
			"[TouchKey] fail to read home raw data.\n");
		return ret;
	}

	raw_data2 = ((0x00FF & data[0])<<8) | data[1];

	dev_dbg(&info->client->dev, "called %s , data : %d %d\n",
			__func__, data[0], data[1]);
	return snprintf(buf, 20, "%d\n", raw_data2);

}

static ssize_t touchkey_raw_data3_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	static u16 raw_data3;
	u8 data[2] = {0,};
	int ret;

	ret = i2c_smbus_read_i2c_block_data(info->client,
		CYPRESS_RAW_DATA_RECENT, ARRAY_SIZE(data), data);
	if (ret != ARRAY_SIZE(data)) {
		dev_err(&info->client->dev,
			"[TouchKey] fail to read recent raw data.\n");
		return ret;
	}

	raw_data3 = ((0x00FF & data[0])<<8) | data[1];

	dev_dbg(&info->client->dev, "called %s , data : %d %d\n",
			__func__, data[0], data[1]);
	return snprintf(buf, 20, "%d\n", raw_data3);

}
#endif

static ssize_t touchkey_idac0_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	static u8 idac0;
	u8 data = 0;

	data = i2c_smbus_read_byte_data(info->client, CYPRESS_IDAC_MENU);

	dev_dbg(&info->client->dev, "called %s , data : %d\n", __func__, data);
	idac0 = data;
	return snprintf(buf, 20, "%d\n", idac0);

}

static ssize_t touchkey_idac1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	static u8 idac1;
	u8 data = 0;

	data = i2c_smbus_read_byte_data(info->client, CYPRESS_IDAC_BACK);

	dev_dbg(&info->client->dev, "called %s , data : %d\n", __func__, data);
	idac1 = data;
	return snprintf(buf, 20, "%d\n", idac1);

}

static ssize_t touchkey_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	static u8 touchkey_threshold;
	u8 data = 0;

	data = i2c_smbus_read_byte_data(info->client, CYPRESS_THRESHOLD);

	dev_dbg(&info->client->dev, "called %s , data : %d\n", __func__, data);
	touchkey_threshold = data;
	return snprintf(buf, 20, "%d\n", touchkey_threshold);
}

#ifdef USE_AUTO_CAL
static ssize_t touch_autocal_testmode(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	int count = 0;
	int on_off;
	if (sscanf(buf, "%d\n", &on_off) == 1) {
		printk(KERN_ERR "[TouchKey] Test Mode : %d\n", on_off);
	if (on_off == 1) {
		count = i2c_smbus_write_byte_data(info->client,
			CYPRESS_GEN, CYPRESS_DATA_UPDATE);
		}
	} else {
		cypress_touchkey_con_hw(info, false);
		msleep(50);
		cypress_touchkey_con_hw(info, true);
		msleep(50);
		cypress_touchkey_auto_cal(info);
	}

	return count;
}

static ssize_t autocalibration_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);
	int data;

	sscanf(buf, "%d\n", &data);

	if (data == 1)
		cypress_touchkey_auto_cal(info);
	return 0;
}

static ssize_t autocalibration_status(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	u8 data[6];
	int ret;
	struct cypress_touchkey_info *info = dev_get_drvdata(dev);

	printk(KERN_DEBUG "[Touchkey] %s\n", __func__);

	ret = i2c_smbus_read_i2c_block_data(info->client,
				CYPRESS_GEN, 6, data);
	if ((data[5] & 0x80))
		return snprintf(buf, 10, "Enabled\n");
	else
		return snprintf(buf, 10, "Disabled\n");
}
#endif
static DEVICE_ATTR(touchkey_firm_update, S_IWUSR | S_IWGRP,
				NULL, touch_update_write);
static DEVICE_ATTR(touchkey_firm_update_status,
				S_IRUGO, touchkey_firm_status_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO,
				touch_version_read, NULL);
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO,
				touch_version_show, NULL);
static DEVICE_ATTR(touchkey_brightness, S_IRUGO,
				NULL, touch_led_control);
static DEVICE_ATTR(touch_sensitivity, S_IRUGO | S_IWUSR | S_IWGRP,
				NULL, touch_sensitivity_control);
static DEVICE_ATTR(touchkey_back, S_IRUGO, touchkey_back_show, NULL);
static DEVICE_ATTR(touchkey_menu, S_IRUGO, touchkey_menu_show, NULL);
static DEVICE_ATTR(touchkey_raw_data0, S_IRUGO, touchkey_raw_data0_show, NULL);
static DEVICE_ATTR(touchkey_raw_data1, S_IRUGO, touchkey_raw_data1_show, NULL);
#ifdef USE_EXTRA_KEY
static DEVICE_ATTR(touchkey_home, S_IRUGO, touchkey_home_show, NULL);
static DEVICE_ATTR(touchkey_recent, S_IRUGO, touchkey_recent_show, NULL);
static DEVICE_ATTR(touchkey_raw_data2, S_IRUGO, touchkey_raw_data2_show, NULL);
static DEVICE_ATTR(touchkey_raw_data3, S_IRUGO, touchkey_raw_data3_show, NULL);
#endif
static DEVICE_ATTR(touchkey_idac0, S_IRUGO, touchkey_idac0_show, NULL);
static DEVICE_ATTR(touchkey_idac1, S_IRUGO, touchkey_idac1_show, NULL);
static DEVICE_ATTR(touchkey_threshold, S_IRUGO, touchkey_threshold_show, NULL);
#ifdef USE_AUTO_CAL
static DEVICE_ATTR(touchkey_autocal_start, S_IRUGO | S_IWUSR | S_IWGRP,
				NULL, touch_autocal_testmode);
static DEVICE_ATTR(autocal_enable, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		   autocalibration_enable);
static DEVICE_ATTR(autocal_stat, S_IRUGO | S_IWUSR | S_IWGRP,
		   autocalibration_status, NULL);
#endif
#ifdef USE_BRIGHTNESS_LEVEL
static DEVICE_ATTR(touchkey_brightness_level, S_IRUGO | S_IWUSR | S_IWGRP,
				brightness_level_show, brightness_control);
#endif

static struct attribute *touchkey_attributes[] = {
	&dev_attr_touchkey_firm_update.attr,
	&dev_attr_touchkey_firm_update_status.attr,
	&dev_attr_touchkey_firm_version_phone.attr,
	&dev_attr_touchkey_firm_version_panel.attr,
	&dev_attr_touchkey_brightness.attr,
	&dev_attr_touch_sensitivity.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_menu.attr,
	&dev_attr_touchkey_raw_data0.attr,
	&dev_attr_touchkey_raw_data1.attr,
#ifdef USE_EXTRA_KEY
	&dev_attr_touchkey_home.attr,
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_raw_data2.attr,
	&dev_attr_touchkey_raw_data3.attr,
#endif
	&dev_attr_touchkey_idac0.attr,
	&dev_attr_touchkey_idac1.attr,
	&dev_attr_touchkey_threshold.attr,
#ifdef USE_AUTO_CAL
	&dev_attr_touchkey_autocal_start.attr,
	&dev_attr_autocal_enable.attr,
	&dev_attr_autocal_stat.attr,
#endif
#ifdef USE_BRIGHTNESS_LEVEL
	&dev_attr_touchkey_brightness_level.attr,
#endif
	NULL,
};

static struct attribute_group touchkey_attr_group = {
	.attrs = touchkey_attributes,
};

static int cypress_touchkey_i2c_check(struct cypress_touchkey_info *info)
{
	int retry = NUM_OF_RETRY_UPDATE;
	int ret = 0;
	u8 data[3] = { 0, };

	while (retry--) {
		ret = i2c_smbus_read_i2c_block_data(info->client, CYPRESS_GEN, 4, data);
		if (ret >= 0) {
			info->ic_fw_ver = data[1];
			info->module_ver = data[2];
			dev_info(&info->client->dev, "Touchkey ic_fw_ver: 0x%02x, module_ver =0x%02x\n",
				info->ic_fw_ver, info->module_ver);
			break;
		}
	}
	return ret;
}

static int __devinit cypress_touchkey_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct cypress_touchkey_platform_data *pdata =
					client->dev.platform_data;
	struct cypress_touchkey_info *info;
	struct input_dev *input_dev;
	int ret = 0;
	int i;
	int retry = NUM_OF_RETRY_UPDATE;
	int ic_fw_ver;
	bool skip_fw_update;

	printk(KERN_ERR "%s start\n", __func__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "fail to memory allocation.\n");
		goto err_mem_alloc;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "fail to allocate input device.\n");
		goto err_input_dev_alloc;
	}

	info->client = client;
	info->input_dev = input_dev;
	info->pdata = client->dev.platform_data;
	info->irq = client->irq;
	info->power_onoff = pdata->power_onoff;
	info->touchkey_update_status = 0;
	memcpy(info->keycode, pdata->touchkey_keycode,
			sizeof(pdata->touchkey_keycode));
	snprintf(info->phys, sizeof(info->phys),
			"%s/input0", dev_name(&client->dev));
	input_dev->name = "sec_touchkey";
	input_dev->phys = info->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_LED, input_dev->evbit);
	set_bit(LED_MISC, input_dev->ledbit);
	for (i = 0; i < ARRAY_SIZE(info->keycode); i++)
		set_bit(info->keycode[i], input_dev->keybit);

	input_set_drvdata(input_dev, info);

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev, "failed to register input dev (%d).\n",
			ret);
		goto err_reg_input_dev;
	}

	i2c_set_clientdata(client, info);

	cypress_touchkey_con_hw(info, true);
	msleep(30);

	ret = request_threaded_irq(client->irq, NULL,
			cypress_touchkey_interrupt,
			IRQF_TRIGGER_FALLING, client->dev.driver->name, info);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to request IRQ %d (err: %d).\n",
				client->irq, ret);
		goto err_req_irq;
	}

	mutex_init(&info->fw_lock);

	info->ic_vendor = i2c_smbus_read_byte_data(client, 0x12);
	dev_err(&client->dev, "touchkey IC ver. 0x12: 0x%02x\n",
				info->ic_vendor);

	ret = cypress_touchkey_i2c_check(info);
	if (ret < 0) {
		retry = 1;
		dev_err(&client->dev, "i2c_check failed.....\n");
	}

	skip_fw_update = pdata->skip_fw_update;
	dev_err(&client->dev, "skip_fw_update = %d\n", skip_fw_update);

#if defined(CONFIG_MACH_SERRANO_ATT)
	if (!skip_fw_update && info->ic_fw_ver < BIN_FW_VERSION)
#else
	if (0/*!skip_fw_update && ic_fw_ver < BIN_FW_VERSION*/)
#endif
	{
		dev_err(&client->dev, "[TOUCHKEY] touchkey_update Start!!\n");
		disable_irq(client->irq);

		while (retry--) {
			if (ISSP_main() == 0) {
				printk(KERN_ERR "[TOUCHKEY] Update success!\n");
				enable_irq(client->irq);
				break;
			}
			info->power_onoff(0);
			msleep(70);
			info->power_onoff(1);
			msleep(50);
			dev_err(&client->dev,
				"[TouchKey] Touchkey_update failed... retry...%d\n",retry);
		}

		if (retry <= 0) {
			cypress_touchkey_con_hw(info, false);
			info->power_onoff(0);
			dev_err(&client->dev, "[TouchKey] tkey driver unload\n");
			goto err_fw_update;
		} else {
			msleep(500);
			ic_fw_ver = i2c_smbus_read_byte_data(info->client,
					CYPRESS_FW_VER);
			dev_err(&client->dev,
				"[TouchKey] %s : FW Ver 0x%02x\n", __func__, ic_fw_ver);
		}
	} else {
		dev_err(&client->dev, "[TouchKey] FW update does not need!\n");
	}

#ifdef USE_AUTO_CAL
	cypress_touchkey_auto_cal(info);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
		info->early_suspend.level =
				EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
		info->early_suspend.suspend = cypress_touchkey_early_suspend;
		info->early_suspend.resume = cypress_touchkey_late_resume;
		register_early_suspend(&info->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */

	info->dev = device_create(sec_class, NULL, 0, NULL, "sec_touchkey");

	ret = IS_ERR(info->dev);
	if (ret) {
		dev_err(&client->dev, "Failed to create device(info->dev)!\n");
		goto err_device_create;
	} else {
		dev_set_drvdata(info->dev, info);
		ret = sysfs_create_group(&info->dev->kobj,
					&touchkey_attr_group);
		if (ret) {
			dev_err(&client->dev, "Failed to create sysfs group\n");
			goto err_sysfs;
		}
	}

#ifdef CONFIG_LEDS_CLASS
		mutex_init(&info->touchkey_led_mutex);
		info->led_wq = create_singlethread_workqueue("cypress_touchkey");
		if (!info->led_wq)
			dev_err(&client->dev, "fail to create led workquewe.\n");
		else
			INIT_WORK(&info->led_work, cypress_touchkey_led_work);

		info->leds.name = TOUCHKEY_BACKLIGHT;
		info->leds.brightness = LED_FULL;
		info->leds.max_brightness = LED_FULL;
		info->leds.brightness_set = cypress_touchkey_brightness_set;

		ret = led_classdev_register(&client->dev, &info->leds);
		if (ret)
			goto err_led_class_dev;
#endif

	printk(KERN_ERR "%s: TKEY probe done.\n", __func__);
	return 0;

#ifdef CONFIG_LEDS_CLASS
err_led_class_dev:
	if (info->led_wq)
		destroy_workqueue(info->led_wq);
	mutex_destroy(&info->touchkey_led_mutex);
#endif
	sysfs_remove_group(&info->dev->kobj, &touchkey_attr_group);
err_sysfs:
err_device_create:
err_fw_update:
	mutex_destroy(&info->fw_lock);
	if (info->irq >= 0)
		free_irq(info->irq, info);
err_req_irq:
	input_unregister_device(input_dev);
err_reg_input_dev:
	input_free_device(input_dev);
	input_dev = NULL;
err_input_dev_alloc:
	kfree(info);
	return -ENXIO;
err_mem_alloc:
	return ret;
}

static int __devexit cypress_touchkey_remove(struct i2c_client *client)
{
	struct cypress_touchkey_info *info = i2c_get_clientdata(client);
	if (info->irq >= 0)
		free_irq(info->irq, info);
	mutex_destroy(&info->touchkey_led_mutex);
	led_classdev_unregister(&info->leds);
	input_unregister_device(info->input_dev);
	input_free_device(info->input_dev);
	kfree(info);
	return 0;
}

#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLYSUSPEND)
static int cypress_touchkey_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cypress_touchkey_info *info = i2c_get_clientdata(client);
	int ret = 0;

	disable_irq(info->irq);
	cypress_touchkey_con_hw(info, false);
	info->power_onoff(0);
	return ret;
}

static int cypress_touchkey_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cypress_touchkey_info *info = i2c_get_clientdata(client);
	int ret = 0;
	info->power_onoff(1);
	cypress_touchkey_con_hw(info, true);
	msleep(300);
	if (touchled_cmd_reversed) {
		touchled_cmd_reversed = 0;
		ret = i2c_smbus_write_byte_data(info->client,
				CYPRESS_GEN, touchkey_led_status);
		if (ret < 0)
			printk(KERN_ERR "cypress: resume LED on fail %d\n",
					ret);
		else
			printk(KERN_ERR "cypress: LED returned on\n");
	}
#ifdef USE_AUTO_CAL
	cypress_touchkey_auto_cal(info);
#endif
	enable_irq(info->irq);

	return ret;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cypress_touchkey_early_suspend(struct early_suspend *h)
{
	struct cypress_touchkey_info *info;
	info = container_of(h, struct cypress_touchkey_info, early_suspend);
	cypress_touchkey_suspend(&info->client->dev);
}

static void cypress_touchkey_late_resume(struct early_suspend *h)
{
	struct cypress_touchkey_info *info;
	info = container_of(h, struct cypress_touchkey_info, early_suspend);
	cypress_touchkey_resume(&info->client->dev);
}
#endif

static const struct i2c_device_id cypress_touchkey_id[] = {
	{"cypress_touchkey", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, cypress_touchkey_id);

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static const struct dev_pm_ops cypress_touchkey_pm_ops = {
	.suspend	= cypress_touchkey_suspend,
	.resume		= cypress_touchkey_resume,
};
#endif

struct i2c_driver cypress_touchkey_driver = {
	.probe = cypress_touchkey_probe,
	.remove = cypress_touchkey_remove,
	.driver = {
		.name = "cypress_touchkey",
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
		.pm	= &cypress_touchkey_pm_ops,
#endif
		   },
	.id_table = cypress_touchkey_id,
};

static int __init cypress_touchkey_init(void)
{

	int ret = 0;
	printk(KERN_ERR "%s: init\n", __func__);
	ret = i2c_add_driver(&cypress_touchkey_driver);
	if (ret) {
		printk(KERN_ERR "cypress touch keypad registration failed. ret= %d\n",
			ret);
	}
	printk(KERN_ERR "%s: init done %d\n", __func__, ret);

	return ret;
}

static void __exit cypress_touchkey_exit(void)
{
	i2c_del_driver(&cypress_touchkey_driver);
}

late_initcall(cypress_touchkey_init);
module_exit(cypress_touchkey_exit);

MODULE_DESCRIPTION("Touchkey driver for Cypress touchkey controller ");
MODULE_LICENSE("GPL");
