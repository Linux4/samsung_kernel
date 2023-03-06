/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/sec_sysfs.h>
#include <linux/sec_batt.h>

#include "issp_extern.h"
#include "cypress_touchkey.h"

#ifdef TK_HAS_FIRMWARE_UPDATE

/* don't modify this */
static u8 fw_ver_file = 0x0;
static u8 md_ver_file = 0x0;
u8 module_divider[] = {0, 0xff};

u8 *firmware_data;
#endif

static int tkey_code[] = { 0,
	KEY_RECENT, KEY_BACK,
};
#define TKEY_CNT ARRAY_SIZE(tkey_code)
static const int tkey_cnt = TKEY_CNT;

u8 regs[] = {KEYCODE_REG,
			CYPRESS_REG_FW_VER,
			CYPRESS_REG_CRC};

enum {
	iKeycode = 0,
	iFwver,
	iCRC,
};

char *str_states[] = {"on_irq", "off_irq", "on_i2c", "off_i2c"};
enum {
	I_STATE_ON_IRQ = 0,
	I_STATE_OFF_IRQ,
	I_STATE_ON_I2C,
	I_STATE_OFF_I2C,
};

static void cypress_config_gpio_i2c(struct touchkey_i2c *tkey_i2c, int onoff);
static int touchkey_autocalibration(struct touchkey_i2c *tkey_i2c);
static int touchkey_i2c_check(struct touchkey_i2c *tkey_i2c);
static void cypress_touchkey_interrupt_set_dual(struct i2c_client *client);


#if defined(TK_USE_4KEY)
static u8 home_sensitivity;
static u8 search_sensitivity;
#endif

static bool touchkey_probe;

static const struct i2c_device_id sec_touchkey_id[] = {
	{"sec_touchkey", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sec_touchkey_id);

extern int get_touchkey_firmware(char *version);
static int led_status;
static int led_reversed;

static void touchkey_enable_irq(struct touchkey_i2c *tkey_i2c, int enable)
{
	static int depth = 0;

	mutex_lock(&tkey_i2c->irq_lock);
	if (enable == 1) {
		if (depth == 1)
			enable_irq(tkey_i2c->irq);
		if (depth)
			--depth;
	} else if (enable == 0){
		if (depth == 0)
			disable_irq(tkey_i2c->irq);
		++depth;

		/* forced enable */
	} else {
		if (depth) {
			depth = 0;
			enable_irq(tkey_i2c->irq);
		}
	}
	mutex_unlock(&tkey_i2c->irq_lock);

#ifdef WACOM_IRQ_DEBUG
	printk(KERN_DEBUG"touchkey:Enable %d, depth %d\n", (int)enable, depth);
#endif
}

#ifdef LED_LDO_WITH_REGULATOR
static void change_touch_key_led_voltage(int vol_mv)
{
	struct regulator *tled_regulator;

	tled_regulator = regulator_get(NULL, TK_LED_REGULATOR_NAME);
	if (IS_ERR(tled_regulator)) {
		pr_err("%s: failed to get resource %s\n", __func__,
		       "touchkey_led");
		return;
	}
	regulator_set_voltage(tled_regulator, vol_mv * 1000, vol_mv * 1000);
	regulator_put(tled_regulator);
}

static ssize_t brightness_control(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	int data;

	if (sscanf(buf, "%d\n", &data) == 1) {
		dev_err(dev, "%s: %d\n", __func__, data);
		change_touch_key_led_voltage(data);
	} else {
		dev_err(dev, "%s Error\n", __func__);
	}

	return size;
}
#endif

static int i2c_touchkey_read(struct touchkey_i2c *tkey_i2c,
		u8 reg, u8 *val, unsigned int len)
{
	struct i2c_client *client = tkey_i2c->client;
	int ret = 0;
	int retry = 3;
#if !defined(TK_USE_GENERAL_SMBUS)
	struct i2c_msg msg[1];
#endif

	mutex_lock(&tkey_i2c->i2c_lock);

	if ((client == NULL) || !(tkey_i2c->enabled)) {
		dev_err(&client->dev, "Touchkey is not enabled. %d\n",
		       __LINE__);
		ret = -ENODEV;
		goto out_i2c_read;
	}

	while (retry--) {
#if defined(TK_USE_GENERAL_SMBUS)
		ret = i2c_smbus_read_i2c_block_data(client,
				reg, len, val);
#else
		msg->addr = client->addr;
		msg->flags = I2C_M_RD;
		msg->len = len;
		msg->buf = val;
		ret = i2c_transfer(client->adapter, msg, 1);
#endif
		if (ret < 0) {
			dev_err(&client->dev, "%s:error(%d)\n", __func__, ret);
			usleep_range(10000, 10000);
			continue;
		}
		break;
	}
out_i2c_read:
	mutex_unlock(&tkey_i2c->i2c_lock);
	return ret;
}

static int i2c_touchkey_write(struct touchkey_i2c *tkey_i2c,
		u8 reg, u8 *val, unsigned int len)
{
	struct i2c_client *client = tkey_i2c->client;
	int ret = 0;
	int retry = 3;
#if !defined(TK_USE_GENERAL_SMBUS)
	struct i2c_msg msg[1];
#endif

	mutex_lock(&tkey_i2c->i2c_lock);

	if ((client == NULL) || !(tkey_i2c->enabled)) {
		dev_err(&client->dev, "Touchkey is not enabled. %d\n",
		       __LINE__);
		ret = -ENODEV;
		goto out_i2c_write;
	}

	while (retry--) {
#if defined(TK_USE_GENERAL_SMBUS)
		ret = i2c_smbus_write_i2c_block_data(client,
				reg, len, val);
#else
		msg->addr = client->addr;
		msg->flags = I2C_M_WR;
		msg->len = len;
		msg->buf = val;
		ret = i2c_transfer(client->adapter, msg, 1);
#endif

		if (ret < 0) {
			dev_err(&client->dev, "%s:error(%d)\n", __func__, ret);
			usleep_range(10000, 10000);
			continue;
		}
		break;
	}

out_i2c_write:
	mutex_unlock(&tkey_i2c->i2c_lock);
	return ret;
}

static int touchkey_autocalibration(struct touchkey_i2c *tkey_i2c)
{
	u8 data[6] = { 0, };
	int count = 0;
	int ret = 0;
	unsigned short retry = 0;

	if (tkey_i2c->autocal == false) {
		printk(KERN_DEBUG"touchkey:pass autocal\n");
		return 0;
	}

	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		printk(KERN_DEBUG"touchkey:%s, wakelock active\n", __func__);
		return 0;
	}

	while (retry < 3) {
		ret = i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, 4);
		if (ret < 0) {
			dev_err(&tkey_i2c->client->dev, "%s: Failed to read Keycode_reg %d times\n",
				__func__, retry);
			return ret;
		}
		dev_dbg(&tkey_i2c->client->dev,
				"data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		/* Send autocal Command */
		data[0] = 0x50;
		data[3] = 0x01;

		count = i2c_touchkey_write(tkey_i2c, KEYCODE_REG, data, 4);

		msleep(130);

		/* Check autocal status */
		ret = i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, 6);

		if (data[5] & TK_BIT_AUTOCAL) {
			dev_info(&tkey_i2c->client->dev, "%s: Run Autocal\n", __func__);
			break;
		} else {
			dev_err(&tkey_i2c->client->dev,	"%s: Error to set Autocal, retry %d\n",
			       __func__, retry);
		}
		retry = retry + 1;
	}

	if (retry == 3)
		dev_err(&tkey_i2c->client->dev, "%s: Failed to Set the Autocalibration\n", __func__);

	return count;
}

static void tkey_led_on(struct touchkey_i2c *tkey_i2c, bool on)
{
	const int ledCmd[] = {TK_CMD_LED_OFF, TK_CMD_LED_ON};
	int ret;

	led_status = on;

	if (!tkey_i2c->enabled) {
		led_reversed = 1;
		/*printk(KERN_DEBUG"touchkey:led reserved\n");*/
		return;
	}

	if (tkey_i2c->pdata->led_by_ldo) {
		tkey_i2c->pdata->led_power_on(tkey_i2c, on);
	} else {
		ret = i2c_touchkey_write(tkey_i2c, KEYCODE_REG, (u8 *) &ledCmd[on?1:0], 1);
		if (ret < 0) {
			dev_err(&tkey_i2c->client->dev, "%s: Error turn on led %d\n",
				__func__, ret);
			led_reversed = 1;
			return;
		}
		msleep(30);
	}
}

#if defined(TK_INFORM_CHARGER)
static int touchkey_ta_setting(struct touchkey_i2c *tkey_i2c)
{
	u8 data[6] = { 0, };
	int count = 0;
	int ret = 0;
	unsigned short retry = 0;

	while (retry < 3) {
		ret = i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, 4);
		if (ret < 0) {
			dev_err(&tkey_i2c->client->dev, "%s: Failed to read Keycode_reg %d times\n",
				__func__, retry);
			return ret;
		}
		dev_dbg(&tkey_i2c->client->dev,
				"data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		/* Send autocal Command */

		if (tkey_i2c->charging_mode) {
			dev_info(&tkey_i2c->client->dev, "%s: TA connected!!!\n", __func__);
			data[0] = 0x90;
			data[3] = 0x10;
		} else {
			dev_info(&tkey_i2c->client->dev, "%s: TA disconnected!!!\n", __func__);
			data[0] = 0x90;
			data[3] = 0x20;
		}

		count = i2c_touchkey_write(tkey_i2c, KEYCODE_REG, data, 4);

		msleep(100);
		dev_dbg(&tkey_i2c->client->dev,
				"write data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		/* Check autocal status */
		ret = i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, 6);

		if (tkey_i2c->charging_mode) {
			if (data[5] & TK_BIT_TA_ON) {
				dev_dbg(&tkey_i2c->client->dev, "%s: TA mode is Enabled\n", __func__);
				break;
			} else {
				dev_err(&tkey_i2c->client->dev, "%s: Error to enable TA mode, retry %d\n",
					__func__, retry);
			}
		} else {
			if (!(data[5] & TK_BIT_TA_ON)) {
				dev_dbg(&tkey_i2c->client->dev, "%s: TA mode is Disabled\n", __func__);
				break;
			} else {
				dev_err(&tkey_i2c->client->dev, "%s: Error to disable TA mode, retry %d\n",
					__func__, retry);
			}
		}
		retry = retry + 1;
	}

	if (retry == 3)
		dev_err(&tkey_i2c->client->dev, "%s: Failed to set the TA mode\n", __func__);

	return count;

}

static void touchkey_ta_cb(struct touchkey_callbacks *cb, bool ta_status)
{
	struct touchkey_i2c *tkey_i2c =
			container_of(cb, struct touchkey_i2c, callbacks);
	struct i2c_client *client = tkey_i2c->client;

	tkey_i2c->charging_mode = ta_status;

	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		printk(KERN_DEBUG"touchkey:%s, wakelock active\n", __func__);
		return ;
	}

	if (tkey_i2c->enabled)
		touchkey_ta_setting(tkey_i2c);
}
#endif

#if defined(CONFIG_KEYBOARD_KLIMTVE) && defined(CONFIG_MUIC_NOTIFIER)
static int touchkey_ta_setting(struct touchkey_i2c *tkey_i2c)
{
	u8 data[6] = { 0, };
	int count = 0;
	int ret = 0;
	unsigned short retry = 0;

	while (retry < 3) {
		ret = i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, 4);
		if (ret < 0) {
			dev_err(&tkey_i2c->client->dev, "%s: Failed to read Keycode_reg %d times\n",
				__func__, retry);
			return ret;
		}
		dev_dbg(&tkey_i2c->client->dev,
				"data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		/* Send autocal Command */

		if (tkey_i2c->charging_mode) {
			dev_info(&tkey_i2c->client->dev, "%s: TA connected!!!\n", __func__);
			data[0] = 0x90;
			data[3] = 0x10;
		} else {
			dev_info(&tkey_i2c->client->dev, "%s: TA disconnected!!!\n", __func__);
			data[0] = 0x90;
			data[3] = 0x20;
		}

		count = i2c_touchkey_write(tkey_i2c, KEYCODE_REG, data, 4);

		msleep(100);
		dev_dbg(&tkey_i2c->client->dev,
				"write data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		/* Check autocal status */
		ret = i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, 6);

		if (tkey_i2c->charging_mode) {
			if (data[5] & TK_BIT_TA_ON) {
				dev_dbg(&tkey_i2c->client->dev, "%s: TA mode is Enabled\n", __func__);
				break;
			} else {
				dev_err(&tkey_i2c->client->dev, "%s: Error to enable TA mode, retry %d\n",
					__func__, retry);
			}
		} else {
			if (!(data[5] & TK_BIT_TA_ON)) {
				dev_dbg(&tkey_i2c->client->dev, "%s: TA mode is Disabled\n", __func__);
				break;
			} else {
				dev_err(&tkey_i2c->client->dev, "%s: Error to disable TA mode, retry %d\n",
					__func__, retry);
			}
		}
		retry = retry + 1;
	}

	if (retry == 3)
		dev_err(&tkey_i2c->client->dev, "%s: Failed to set the TA mode\n", __func__);

	return count;

}

static int touchkey_cable_check(muic_attached_dev_t attached_dev)
{
	int current_cable_type = -1;

	switch (attached_dev)
	{
	case ATTACHED_DEV_NONE_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_MHL_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
		current_cable_type = 0;
		break;
	default:
		current_cable_type = 1;
		break;
	}

	return current_cable_type;

}

static int touchkey_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
	const char *cmd;
	int cable_type;
	struct touchkey_i2c *tkey_i2c =
			container_of(nb, struct touchkey_i2c, touchkey_nb);

	switch (action) {
	case MUIC_NOTIFY_CMD_DETACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_DETACH:
		cmd = "DETACH";
		cable_type = 0;
		break;
	case MUIC_NOTIFY_CMD_ATTACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_ATTACH:
		cmd = "ATTACH";
		cable_type = touchkey_cable_check(attached_dev);
		break;
	default:
		cmd = "ERROR";
		cable_type = -1;
		break;
	}

	pr_info("%s: cmd=%s, attached_dev=%d, cable_type=%d\n", __func__, cmd, attached_dev, cable_type);

	if (cable_type == 1)
		tkey_i2c->charging_mode = true;
	else
		tkey_i2c->charging_mode = false;

	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		printk(KERN_DEBUG"touchkey:%s, wakelock active\n", __func__);
		return 0;
	}

	if (tkey_i2c->enabled)
		touchkey_ta_setting(tkey_i2c);

	return 1;
}
#endif

#if defined(CONFIG_GLOVE_TOUCH)
static void touchkey_glove_change_work(struct work_struct *work)
{
	u8 data[6] = { 0, };
	int ret = 0;
	unsigned short retry = 0;
	bool value;
	u8 glove_bit;
	struct touchkey_i2c *tkey_i2c =
			container_of(work, struct touchkey_i2c,
			glove_change_work.work);

#ifdef TKEY_FLIP_MODE
	if (tkey_i2c->enabled_flip) {
		dev_info(&tkey_i2c->client->dev,"As flip cover mode enabled, skip glove mode set\n");
		tkey_i2c->tsk_glove_mode_status = false;
		return;
	}
#endif

	mutex_lock(&tkey_i2c->tsk_glove_lock);
	value = tkey_i2c->tsk_glove_mode_status;
	mutex_unlock(&tkey_i2c->tsk_glove_lock);

	if (!tkey_i2c->enabled) {
		tkey_i2c->tsk_glove_mode_status = false;
		return;
	}

	while (retry < 3) {
		ret = i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, 4);
		if (ret < 0) {
			dev_err(&tkey_i2c->client->dev, "%s: Failed to read Keycode_reg %d times\n",
				__func__, retry);
			return;
		}

		dev_dbg(&tkey_i2c->client->dev,
				"data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		if (value) {
			/* Send glove Command */
				data[0] = 0xA0;
				data[3] = 0x30;
		} else {
				data[0] = 0xA0;
				data[3] = 0x40;
		}

		i2c_touchkey_write(tkey_i2c, KEYCODE_REG, data, 4);
		msleep(130);

		dev_dbg(&tkey_i2c->client->dev,
				"data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
				data[0], data[1], data[2], data[3]);

		/* Check autocal status */
		ret = i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, 6);

		glove_bit = !!(data[5] & TK_BIT_GLOVE);

		if (value == glove_bit) {
			dev_dbg(&tkey_i2c->client->dev, "%s:Glove mode is %s\n",
				__func__, value ? "enabled" : "disabled");
			break;
		} else
			dev_err(&tkey_i2c->client->dev, "%s:Error to set glove_mode val %d, bit %d, retry %d\n",
				__func__, value, glove_bit, retry);

		retry = retry + 1;
	}
	if (retry == 3)
		dev_err(&tkey_i2c->client->dev, "%s: Failed to set the glove mode\n", __func__);
}

void touchkey_glovemode(struct touchkey_i2c *tkey_i2c, int on)
{
	if (!touchkey_probe) {
		dev_err(&tkey_i2c->client->dev, "%s: Touchkey is not probed\n", __func__);
		return;
	}
	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		printk(KERN_DEBUG"touchkey:%s, wakelock active\n", __func__);
		return ;
	}

	mutex_lock(&tkey_i2c->tsk_glove_lock);

	/* protect duplicated execution */
	if (on == tkey_i2c->tsk_glove_mode_status) {
		dev_info(&tkey_i2c->client->dev, "%s pass. cmd %d, cur status %d\n",
			__func__, on, tkey_i2c->tsk_glove_mode_status);
		goto end_glovemode;
	}

	cancel_delayed_work(&tkey_i2c->glove_change_work);

	tkey_i2c->tsk_glove_mode_status = on;
	schedule_delayed_work(&tkey_i2c->glove_change_work,
		msecs_to_jiffies(TK_GLOVE_DWORK_TIME));

	dev_info(&tkey_i2c->client->dev, "Touchkey glove %s\n", on ? "On" : "Off");

 end_glovemode:
	mutex_unlock(&tkey_i2c->tsk_glove_lock);
}
#endif

#ifdef TKEY_FLIP_MODE
void touchkey_flip_cover(struct touchkey_i2c *tkey_i2c, int value)
{
	tkey_i2c->enabled_flip = value;

	if (!touchkey_probe) {
		dev_err(&tkey_i2c->client->dev, "%s: Touchkey is not probed\n", __func__);
		return;
	}
	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		printk(KERN_DEBUG"touchkey:%s, wakelock active\n", __func__);
		return ;
	}

#if 0
	if (tkey_i2c->firmware_id != TK_MODULE_20065) {
		dev_err(&tkey_i2c->client->dev,
				"%s: Do not support old module\n",
				__func__);
		return;
	}
#endif
	if (value == 1)
		touchkey_enable_irq(tkey_i2c, false);
	else
		touchkey_enable_irq(tkey_i2c, true);

	dev_err(&tkey_i2c->client->dev, "%s: Flip mode %s\n", __func__, value ? "on" : "off");

	return;
}
#endif

static int touchkey_enable_status_update(struct touchkey_i2c *tkey_i2c)
{
	unsigned char data = 0x40;
	int ret;

	ret = i2c_touchkey_write(tkey_i2c, KEYCODE_REG, &data, 1);
	if (ret < 0) {
		dev_err(&tkey_i2c->client->dev, "%s, err(%d)\n", __func__, ret);
		tkey_i2c->status_update = false;
		return ret;
	}

	tkey_i2c->status_update = true;
	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	msleep(20);

	return 0;
}

enum {
	TK_CMD_READ_THRESHOLD = 0,
	TK_CMD_READ_IDAC,
	TK_CMD_READ_SENSITIVITY,
	TK_CMD_READ_RAW,
};

const u8 fac_reg_index[] = {
	CYPRESS_REG_THRESHOLD,
	CYPRESS_REG_IDAC,
	CYPRESS_REG_DIFF,
	CYPRESS_REG_RAW,
};

struct FAC_CMD {
	u8 cmd;
	u8 opt1; // 0, 1, 2, 3
	u16 result;
};

static u8 touchkey_get_read_size(u8 cmd)
{
	switch (cmd) {
	case TK_CMD_READ_RAW:
	case TK_CMD_READ_SENSITIVITY:
		return 2;
	case TK_CMD_READ_IDAC:
	case TK_CMD_READ_THRESHOLD:
		return 1;
		break;
	default:
		break;
	}
	return 0;
}

static int touchkey_fac_read_data(struct device *dev,
		char *buf, struct FAC_CMD *cmd)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int ret;
	u8 size;
	u8 base_index;
	u8 data[26] = { 0, };
	int i;
	u16 max_val = 0;

	if (unlikely(!tkey_i2c->status_update)) {
		ret = touchkey_enable_status_update(tkey_i2c);
		if (ret < 0)
			goto out_fac_read_data;
	}

	size = touchkey_get_read_size(cmd->cmd);
	if (size == 0) {
		printk(KERN_DEBUG"touchkey:wrong size %d\n", size);
		goto out_fac_read_data;
	}

	if (cmd->opt1 > 4) {
		printk(KERN_DEBUG"touchkey:wrong opt1 %d\n", cmd->opt1);
		goto out_fac_read_data;
	}

	base_index = fac_reg_index[cmd->cmd] + size * cmd->opt1;
	if (base_index > 25) {
		printk(KERN_DEBUG"touchkey:wrong index %d, cmd %d, size %d, opt1 %d\n",
			base_index, cmd->cmd, size, cmd->opt1);
		goto out_fac_read_data;
	}

	ret = i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, base_index + size);
	if (ret <  0) {
		printk(KERN_DEBUG"touchkey:i2c read failed\n");
		goto out_fac_read_data;
	}

	/* make value */
	cmd->result = 0;
	for (i = size - 1; i >= 0; --i) {
		cmd->result = cmd->result | (data[base_index++] << (8 * i));
		max_val |= 0xff << (8 * i);
	}

	/* garbage check */
	if (unlikely(cmd->result == max_val)) {
		printk(KERN_DEBUG"touchkey:cmd %d opt1 %d, max value\n", cmd->cmd, cmd->opt1);
		cmd->result = 0;
	}

 out_fac_read_data:
	return sprintf(buf, "%d\n", cmd->result);
}

static ssize_t touchkey_raw_data0_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_RAW, 0, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_raw_data1_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_RAW, 1, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}
#if !defined(TK_USE_RECENT)
static ssize_t touchkey_raw_data2_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_RAW, 2, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_raw_data3_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_RAW, 3, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}
#endif
static ssize_t touchkey_idac0_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_IDAC, 0, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_idac1_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_IDAC, 1, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}
#if !defined(TK_USE_RECENT)
static ssize_t touchkey_idac2_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_IDAC, 2, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_idac3_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_IDAC, 3, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}
#endif
static ssize_t touchkey_threshold_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_THRESHOLD, 0, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}

#if defined(TK_USE_4KEY)
static ssize_t touchkey_menu_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[18] = { 0, };
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, 18);

	dev_dbg(&tkey_i2c->client->dev, "called %s data[10] =%d,data[11] = %d\n", __func__,
		data[10], data[11]);
	menu_sensitivity = ((0x00FF & data[10]) << 8) | data[11];

	return sprintf(buf, "%d\n", menu_sensitivity);
}

static ssize_t touchkey_home_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[18] = { 0, };
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, 18);

	dev_dbg(&tkey_i2c->client->dev, "called %s data[12] =%d,data[13] = %d\n", __func__,
		data[12], data[13]);
	home_sensitivity = ((0x00FF & data[12]) << 8) | data[13];

	return sprintf(buf, "%d\n", home_sensitivity);
}

static ssize_t touchkey_back_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[18] = { 0, };
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, 18);

	dev_dbg(&tkey_i2c->client->dev, "called %s data[14] =%d,data[15] = %d\n", __func__,
		data[14], data[15]);
	back_sensitivity = ((0x00FF & data[14]) << 8) | data[15];

	return sprintf(buf, "%d\n", back_sensitivity);
}

static ssize_t touchkey_search_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	u8 data[18] = { 0, };
	int ret;

	dev_dbg(&tkey_i2c->client->dev, "called %s\n", __func__);
	ret = i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, 18);

	dev_dbg(&tkey_i2c->client->dev, "called %s data[16] =%d,data[17] = %d\n", __func__,
		data[16], data[17]);
	search_sensitivity = ((0x00FF & data[16]) << 8) | data[17];

	return sprintf(buf, "%d\n", search_sensitivity);
}
#else
static ssize_t touchkey_menu_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_SENSITIVITY, 0, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}

static ssize_t touchkey_back_show(struct device *dev,
struct device_attribute *attr, char *buf)
{
	struct FAC_CMD cmd = {TK_CMD_READ_SENSITIVITY, 1, 0};
	return touchkey_fac_read_data(dev, buf, &cmd);
}
#endif

#if defined(TK_HAS_FIRMWARE_UPDATE)
/* To check firmware compatibility */
int get_module_class(u8 ver)
{
	static int size = ARRAY_SIZE(module_divider);
	int i;

	if (size == 2)
		return 0;

	for (i = size - 1; i > 0; --i) {
		if (ver < module_divider[i] &&
			ver >= module_divider[i-1])
			return i;
	}

	return 0;
}

bool is_same_module_class(struct touchkey_i2c *tkey_i2c)
{
	int class_ic, class_file;

	if (md_ver_file == tkey_i2c->md_ver_ic)
		return true;

	class_file = get_module_class(md_ver_file);
	class_ic = get_module_class(tkey_i2c->md_ver_ic);

	printk(KERN_DEBUG"touchkey:module class, IC %d, File %d\n", class_ic, class_file);

	if (class_file == class_ic)
		return true;

	return false;
}

int tkey_load_fw_built_in(struct touchkey_i2c *tkey_i2c)
{
	int retry = 3;
	int ret;

	while (retry--) {
		ret =
			request_firmware(&tkey_i2c->firm_data, tkey_i2c->pdata->fw_path,
			&tkey_i2c->client->dev);
		if (ret < 0) {
			printk(KERN_ERR
				"touchkey:Unable to open firmware. ret %d retry %d\n",
				ret, retry);
			continue;
		}
		break;
	}

	if (ret < 0)
		return ret;

	tkey_i2c->fw_img = (struct fw_image *)tkey_i2c->firm_data->data;

	return ret;
}

int tkey_load_fw_sdcard(struct touchkey_i2c *tkey_i2c)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;
	u8 *ums_data = NULL;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(TKEY_FW_PATH, O_RDONLY, S_IRUSR);

	if (IS_ERR(fp)) {
		printk(KERN_ERR "touchkey:failed to open %s.\n", TKEY_FW_PATH);
		ret = -ENOENT;
		set_fs(old_fs);
		return ret;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	printk(KERN_NOTICE
		"touchkey:start, file path %s, size %ld Bytes\n",
		TKEY_FW_PATH, fsize);

	ums_data = kmalloc(fsize, GFP_KERNEL);
	if (IS_ERR(ums_data)) {
		printk(KERN_ERR
			"touchkey:%s, kmalloc failed\n", __func__);
		ret = -EFAULT;
		goto malloc_error;
	}

	nread = vfs_read(fp, (char __user *)ums_data,
		fsize, &fp->f_pos);
	printk(KERN_NOTICE "touchkey:nread %ld Bytes\n", nread);
	if (nread != fsize) {
		printk(KERN_ERR
			"touchkey:failed to read firmware file, nread %ld Bytes\n",
			nread);
		ret = -EIO;
		kfree(ums_data);
		goto read_err;
	}

	filp_close(fp, current->files);
	set_fs(old_fs);

	tkey_i2c->fw_img = (struct fw_image *)ums_data;

	return 0;

read_err:
malloc_error:
	filp_close(fp, current->files);
	set_fs(old_fs);
	return ret;
}

void touchkey_unload_fw(struct touchkey_i2c *tkey_i2c)
{
	switch (tkey_i2c->fw_path) {
	case FW_BUILT_IN:
		release_firmware(tkey_i2c->firm_data);
		break;
	case FW_IN_SDCARD:
		kfree(tkey_i2c->fw_img);
		break;
	default:
		break;
	}

	tkey_i2c->fw_path = FW_NONE;
	tkey_i2c->fw_img = NULL;
	tkey_i2c->firm_data = NULL;
	firmware_data = NULL;
}

int touchkey_load_fw(struct touchkey_i2c *tkey_i2c, u8 fw_path)
{
	int ret = 0;
	struct fw_image *fw_img;

	switch (fw_path) {
	case FW_BUILT_IN:
		ret = tkey_load_fw_built_in(tkey_i2c);
		break;
	case FW_IN_SDCARD:
		ret = tkey_load_fw_sdcard(tkey_i2c);
		break;
	default:
		printk(KERN_DEBUG"touchkey:unknown path(%d)\n", fw_path);
		goto err_load_fw;
	}

	if (ret < 0)
		goto err_load_fw;

	fw_img = tkey_i2c->fw_img;

	/* header check  */
	if (fw_img->hdr_ver == 1 && fw_img->hdr_len == 16) {
		firmware_data = (u8 *)fw_img->data;
		if (fw_path == FW_BUILT_IN) {
			fw_ver_file = fw_img->first_fw_ver;
			md_ver_file = fw_img->second_fw_ver;
		}
	} else {
		printk(KERN_DEBUG"touchkey:no hdr\n");
		firmware_data = (u8 *)fw_img;
	}

	return ret;
 err_load_fw:
	firmware_data = NULL;
	return ret;
}

/* update condition */
int check_update_condition(struct touchkey_i2c *tkey_i2c)
{
	int ret = 0;

	/* check update condition */
	ret = is_same_module_class(tkey_i2c);
	if (ret == false) {
		printk(KERN_DEBUG"touchkey:md classes are different\n");
		return TK_EXIT_UPDATE;
	}

	if (tkey_i2c->md_ver_ic != md_ver_file)
		return TK_RUN_UPDATE;
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
	if (tkey_i2c->fw_ver_ic != fw_ver_file)
		return TK_RUN_UPDATE;
#else
	if (tkey_i2c->fw_ver_ic < fw_ver_file)
		return TK_RUN_UPDATE;

	/* if ic ver is higher than file, exit */
	if (tkey_i2c->fw_ver_ic > fw_ver_file)
		return TK_EXIT_UPDATE;
#endif

	return TK_EXIT_UPDATE;
}

int tkey_crc_check(struct touchkey_i2c *tkey_i2c)
{
	char data[3] = {0, };
	int retry = 3;
	int ret;
	unsigned short chk_ic;

	while (retry--) {
		ret = i2c_touchkey_read(tkey_i2c, regs[iCRC], data, 2);
		if (ret < 0) {
			dev_err(&tkey_i2c->client->dev, "retry crc read(%d)\n", retry);
			msleep(10);
			continue;
		}
		break;
	}
	if (ret < 0) {
		dev_err(&tkey_i2c->client->dev, "Failed to read CRC\n");
		return 0;
	}

	chk_ic = (data[0] << 8) | data[1];
	dev_info(&tkey_i2c->client->dev, "checksum, ic:%#x, bin:%#x\n",
				chk_ic, tkey_i2c->fw_img->checksum);

	return (tkey_i2c->fw_img->checksum == chk_ic);
}

int touchkey_fw_update(struct touchkey_i2c *tkey_i2c, u8 fw_path, bool bforced)
{
	int ret;

	ret = touchkey_load_fw(tkey_i2c, fw_path);
	if (ret < 0) {
		printk(KERN_DEBUG"touchkey:failed to load fw data\n");
		return ret;
	}
	tkey_i2c->fw_path = fw_path;

	/* f/w info */
	dev_info(&tkey_i2c->client->dev, "fw ver %#x, new fw ver %#x\n",
		tkey_i2c->fw_ver_ic, fw_ver_file);
	dev_info(&tkey_i2c->client->dev, "module ver %#x, new module ver %#x\n",
		tkey_i2c->md_ver_ic, md_ver_file);

	if (unlikely(bforced))
		goto run_fw_update;

	if (tkey_i2c->pdata->fw_crc_check) {
		ret = tkey_crc_check(tkey_i2c);
		if (ret == false) {
			dev_info(&tkey_i2c->client->dev, "crc error, run fw update\n");
			goto run_fw_update;
		}
	} else {
		dev_info(&tkey_i2c->client->dev, "skip crc check\n");
	}

	ret = check_update_condition(tkey_i2c);

	if (ret == TK_EXIT_UPDATE) {
		dev_info(&tkey_i2c->client->dev, "pass fw update\n");
		touchkey_unload_fw(tkey_i2c);
	}
	else if (ret == TK_RUN_CHK) {
		dev_info(&tkey_i2c->client->dev, "run checksum\n");
		/*tkey_i2c->do_checksum = true;*/
	}
	/* else do update */

 run_fw_update:
	queue_work(tkey_i2c->fw_wq, &tkey_i2c->update_work);

	return 0;
}

static void touchkey_i2c_update_work(struct work_struct *work)
{
	struct touchkey_i2c *tkey_i2c =
		container_of(work, struct touchkey_i2c, update_work);
	int ret = 0;
	int retry = 5;
#if defined(CONFIG_CHAGALL)
	return;
#endif
	touchkey_enable_irq(tkey_i2c, false);
	wake_lock(&tkey_i2c->fw_wakelock);

	if (tkey_i2c->fw_path == FW_NONE)
		goto end_fw_update;

	dev_info(&tkey_i2c->client->dev, "start update\n");

	tkey_i2c->update_status = TK_UPDATE_DOWN;
	cypress_config_gpio_i2c(tkey_i2c, 0);

	while (retry--) {
		ret = ISSP_main(tkey_i2c);
		if (ret != 0) {
			dev_err(&tkey_i2c->client->dev, "failed to update f/w. retry\n");
			continue;
		}

		dev_info(&tkey_i2c->client->dev, "finish f/w update\n");
		tkey_i2c->update_status = TK_UPDATE_PASS;
		break;
	}
	cypress_config_gpio_i2c(tkey_i2c, 1);

	msleep(50);
	tkey_i2c->pdata->power_on(tkey_i2c, 0);
	msleep(10);
	tkey_i2c->pdata->power_on(tkey_i2c, 1);
	msleep(300);

	if (retry <= 0 && ret != 0) {
		tkey_i2c->pdata->power_on(tkey_i2c, 0);
		tkey_i2c->update_status = TK_UPDATE_FAIL;
		dev_err(&tkey_i2c->client->dev, "failed to update f/w\n");
		goto err_fw_update;
	}

	ret = touchkey_i2c_check(tkey_i2c);
	if (ret < 0)
		goto err_fw_update;

	dev_info(&tkey_i2c->client->dev, "f/w ver = %#X, module ver = %#X\n",
		tkey_i2c->fw_ver_ic, tkey_i2c->md_ver_ic);

 err_fw_update:
	touchkey_unload_fw(tkey_i2c);
 end_fw_update:
	wake_unlock(&tkey_i2c->fw_wakelock);
	touchkey_autocalibration(tkey_i2c);
	touchkey_enable_irq(tkey_i2c, true);
}
#endif

static irqreturn_t touchkey_interrupt(int irq, void *dev_id)
{
	struct touchkey_i2c *tkey_i2c = dev_id;
	u8 data[3];
	int ret;
	bool glove_mode_status;
	int i;
	int keycode_data[tkey_cnt];
	int keycode_type = 0;
	int pressed;

	if (unlikely(!touchkey_probe)) {
		dev_err(&tkey_i2c->client->dev, "%s: Touchkey is not probed\n", __func__);
		return IRQ_HANDLED;
	}

#ifdef CONFIG_GLOVE_TOUCH
	glove_mode_status = tkey_i2c->tsk_glove_mode_status;
#else
	glove_mode_status = 0;
#endif

	ret = i2c_touchkey_read(tkey_i2c, regs[iKeycode], data, 1);
	if (ret < 0)
		return IRQ_HANDLED;

	if (tkey_i2c->fw_ver_ic >= tkey_i2c->pdata->multi_fw_ver && tkey_i2c->support_multi_touch == 1) {
		keycode_data[1] = data[0] & 0x3;
		keycode_data[2] = (data[0] >> 2) & 0x3;

		for (i = 1; i < tkey_cnt; i++) {
			if (keycode_data[i]) {
				input_report_key(tkey_i2c->input_dev, tkey_code[i], (keycode_data[i] % 2));
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				dev_info(&tkey_i2c->client->dev, "keycode: %d %s %d\n", tkey_code[i],
								(keycode_data[i] % 2) ? "PRESS" : "RELEASE", glove_mode_status);
#else
				dev_info(&tkey_i2c->client->dev, " %s %d\n",
								(keycode_data[i] % 2) ? "PRESS" : "RELEASE", glove_mode_status);
#endif
			}
		}

		input_sync(tkey_i2c->input_dev);
	} else {
		keycode_type = (data[0] & TK_BIT_KEYCODE);
		pressed = !(data[0] & TK_BIT_PRESS_EV);

		if (keycode_type <= 0 || keycode_type >= tkey_cnt) {
			dev_err(&tkey_i2c->client->dev, "keycode_type err\n");
			return IRQ_HANDLED;
		}
		input_report_key(tkey_i2c->input_dev,
				 tkey_code[keycode_type], pressed);
		input_sync(tkey_i2c->input_dev);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		dev_info(&tkey_i2c->client->dev, "keycode:%d pressed:%d %#x, %d\n",
		tkey_code[keycode_type], pressed, tkey_i2c->fw_ver_ic, glove_mode_status);
#else
		dev_info(&tkey_i2c->client->dev, "pressed:%d %#x, %d\n",
			pressed, tkey_i2c->fw_ver_ic, glove_mode_status);
#endif
	}
	return IRQ_HANDLED;
}

static int touchkey_stop(struct touchkey_i2c *tkey_i2c)
{
	int i;

	mutex_lock(&tkey_i2c->lock);

	if (!tkey_i2c->enabled) {
		dev_err(&tkey_i2c->client->dev, "Touch key already disabled\n");
		goto err_stop_out;
	}
	tkey_i2c->enabled = false;

	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		printk(KERN_DEBUG"touchkey:%s, wakelock active\n", __func__);
		goto err_stop_out;
	}

	touchkey_enable_irq(tkey_i2c, false);

	/* release keys */
	for (i = 1; i < tkey_cnt; ++i) {
		input_report_key(tkey_i2c->input_dev,
				 tkey_code[i], 0);
	}
	input_sync(tkey_i2c->input_dev);

#if defined(CONFIG_GLOVE_TOUCH)
	/*cancel or waiting before pwr off*/
	tkey_i2c->tsk_glove_mode_status = false;
	if (false == tkey_i2c->pdata->glove_mode_keep_status)
		tkey_i2c->tsk_enable_glove_mode = false;
	cancel_delayed_work(&tkey_i2c->glove_change_work);
#endif

	tkey_i2c->pdata->led_power_on(tkey_i2c, 0);
	tkey_i2c->pdata->power_on(tkey_i2c, 0);

	tkey_i2c->status_update = false;

	led_reversed |= led_status;

 err_stop_out:
	mutex_unlock(&tkey_i2c->lock);

	return 0;
}

static int touchkey_start(struct touchkey_i2c *tkey_i2c)
{
#ifdef TEST_JIG_MODE
	unsigned char get_touch = 0x40;
#endif

	mutex_lock(&tkey_i2c->lock);

	if (tkey_i2c->enabled) {
		dev_err(&tkey_i2c->client->dev, "Touch key already enabled\n");
		goto err_start_out;
	}
	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		printk(KERN_DEBUG"touchkey:%s, wakelock active\n", __func__);
		goto err_start_out;
	}

	tkey_i2c->pdata->power_on(tkey_i2c, 1);
	if (tkey_i2c->autocal)
		msleep(50);
	else
		msleep(300);

	if (!tkey_i2c->pdata->led_by_ldo)
		tkey_i2c->pdata->led_power_on(tkey_i2c, 1);

	tkey_i2c->enabled = true;

	/* CYPRESS Firmware setting interrupt type : dual or single interrupt */
	cypress_touchkey_interrupt_set_dual(tkey_i2c->client);

	touchkey_autocalibration(tkey_i2c);

	if (led_reversed) {
		led_reversed = 0;
		tkey_led_on(tkey_i2c, true);
	}

#ifdef TEST_JIG_MODE
	i2c_touchkey_write(tkey_i2c, KEYCODE_REG, &get_touch, 1);
#endif

#if defined(TK_INFORM_CHARGER)
	if (tkey_i2c->charging_mode)
		touchkey_ta_setting(tkey_i2c);
#endif

#if defined(CONFIG_KEYBOARD_KLIMTVE) && defined(CONFIG_MUIC_NOTIFIER)
	if (tkey_i2c->charging_mode)
		touchkey_ta_setting(tkey_i2c);
#endif

#if defined(CONFIG_GLOVE_TOUCH)
	if (tkey_i2c->tsk_enable_glove_mode)
		touchkey_glovemode(tkey_i2c, 1);
#endif
	touchkey_enable_irq(tkey_i2c, true);
 err_start_out:
	mutex_unlock(&tkey_i2c->lock);

	return 0;
}

#ifdef TK_USE_OPEN_DWORK
static void touchkey_open_work(struct work_struct *work)
{
	int retval;
	struct touchkey_i2c *tkey_i2c =
			container_of(work, struct touchkey_i2c,
			open_work.work);

	if (tkey_i2c->enabled) {
		dev_err(&tkey_i2c->client->dev, "Touch key already enabled\n");
		return;
	}

	retval = touchkey_start(tkey_i2c);
	if (retval < 0)
		dev_err(&tkey_i2c->client->dev,
				"%s: Failed to start device\n", __func__);
}
#endif

static int touchkey_input_open(struct input_dev *dev)
{
	struct touchkey_i2c *data = input_get_drvdata(dev);
	int ret;

	printk(KERN_DEBUG"touchkey:%s\n", __func__);

	ret = wait_for_completion_interruptible_timeout(&data->init_done,
			msecs_to_jiffies(1/*90 * MSEC_PER_SEC*/));

	if (ret < 0) {
		dev_err(&data->client->dev,
			"error while waiting for device to init (%d)\n", ret);
		ret = -ENXIO;
		goto err_open;
	}
	if (ret == 0) {
		dev_err(&data->client->dev,
			"timedout while waiting for device to init\n");
		ret = -ENXIO;
		goto err_open;
	}
#ifdef TK_USE_OPEN_DWORK
	schedule_delayed_work(&data->open_work,
					msecs_to_jiffies(TK_OPEN_DWORK_TIME));
#else
	ret = touchkey_start(data);
	if (ret)
		goto err_open;
#endif

	dev_dbg(&data->client->dev, "%s\n", __func__);

	return 0;

err_open:
	return ret;
}

static void touchkey_input_close(struct input_dev *dev)
{
	struct touchkey_i2c *data = input_get_drvdata(dev);

#ifdef TK_USE_OPEN_DWORK
	cancel_delayed_work(&data->open_work);
#endif
	touchkey_stop(data);

	dev_dbg(&data->client->dev, "%s\n", __func__);
}

#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND
#define touchkey_suspend	NULL
#define touchkey_resume	NULL

static int sec_touchkey_early_suspend(struct early_suspend *h)
{
	struct touchkey_i2c *tkey_i2c =
		container_of(h, struct touchkey_i2c, early_suspend);

	touchkey_stop(tkey_i2c);

	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	return 0;
}

static int sec_touchkey_late_resume(struct early_suspend *h)
{
	struct touchkey_i2c *tkey_i2c =
		container_of(h, struct touchkey_i2c, early_suspend);

	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	touchkey_start(tkey_i2c);

	return 0;
}
#else
static int touchkey_suspend(struct device *dev)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	if (touchkey_probe != true) {
		printk(KERN_ERR "%s Touchkey is not enabled. \n",
		       __func__);
		return 0;
	}
	mutex_lock(&tkey_i2c->input_dev->mutex);

	if (tkey_i2c->input_dev->users)
		touchkey_stop(tkey_i2c);

	mutex_unlock(&tkey_i2c->input_dev->mutex);

	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	return 0;
}

static int touchkey_resume(struct device *dev)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	if (touchkey_probe != true) {
		printk(KERN_ERR "%s Touchkey is not enabled. \n",
		       __func__);
		return 0;
	}
	mutex_lock(&tkey_i2c->input_dev->mutex);

	if (tkey_i2c->input_dev->users)
		touchkey_start(tkey_i2c);

	mutex_unlock(&tkey_i2c->input_dev->mutex);

	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	return 0;
}
#endif
static SIMPLE_DEV_PM_OPS(touchkey_pm_ops, touchkey_suspend, touchkey_resume);

#endif

static void cypress_touchkey_interrupt_set_dual(struct i2c_client *client)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(&client->dev);
	int ret = 0;
	int retry = 5;
	u8 data[3] = {0, };

	if (tkey_i2c->fw_ver_ic <= tkey_i2c->pdata->multi_fw_ver) {
		dev_err(&client->dev, "%s: not support this version\n", __func__);
		return;
	}

	while (retry--) {
		data[0] = TK_CMD_DUAL_DETECTION;
		data[1] = 0x00;
		data[2] = TK_BIT_DETECTION_CONFIRM;

		ret = i2c_smbus_write_i2c_block_data(client, TK_DUAL_REG, 3, &data[0]);
		if (ret < 0) {
			dev_err(&client->dev, "%s: i2c write error. (%d)\n", __func__, ret);
			msleep(30);
			continue;
		}
		msleep(50);

		data[0] = CYPRESS_DETECTION_FLAG;

		ret = i2c_smbus_read_i2c_block_data(client, data[0], 1, &data[1]);
		if (ret < 0) {
			dev_err( &client->dev, "%s: i2c read error. (%d)\n", __func__, ret);
			msleep(30);
			continue;
		}

		if (data[1] != 1) {
			dev_err(&client->dev,
				"%s: interrupt set: 0x%X, failed.\n", __func__, data[1]);
			continue;
		}
		tkey_i2c->support_multi_touch = data[1];

		dev_info(&client->dev, "%s: interrupt set: 0x%X\n", __func__, tkey_i2c->support_multi_touch);
		break;
	}

}

static int touchkey_i2c_check(struct touchkey_i2c *tkey_i2c)
{
	char data[3] = { 0, };
	int ret = 0;
	int retry = 3;

	while (retry--) {
		ret = i2c_touchkey_read(tkey_i2c, regs[iFwver], data, 3);
		if (ret < 0) {
			dev_err(&tkey_i2c->client->dev, "retry i2c check(%d)\n", retry);
			msleep(30);
			continue;
		}
		break;
	}

	if (ret < 0) {
		dev_err(&tkey_i2c->client->dev, "Failed to read Module version\n");
		tkey_i2c->fw_ver_ic = 0;
		tkey_i2c->md_ver_ic = 0;
		return ret;
	}

	tkey_i2c->fw_ver_ic = data[0];
	tkey_i2c->md_ver_ic = data[1];

	/* CYPRESS Firmware setting interrupt type : dual or single interrupt */
	cypress_touchkey_interrupt_set_dual(tkey_i2c->client);

	return ret;
}

static ssize_t touchkey_led_control(struct device *dev,
	struct device_attribute *attr, const char *buf,
	size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int data;
	int ret;

	if (wake_lock_active(&tkey_i2c->fw_wakelock)) {
		printk(KERN_DEBUG"touchkey:%s, wakelock active\n", __func__);
		return size;
	}

	ret = sscanf(buf, "%d", &data);
	if (ret != 1) {
		dev_err(&tkey_i2c->client->dev, "%s, %d err\n",
			__func__, __LINE__);
		return size;
	}
	if (data != 0 && data != 1) {
		dev_err(&tkey_i2c->client->dev, "%s wrong cmd %x\n",
			__func__, data);
		return size;
	}

	tkey_led_on(tkey_i2c, data);

	return size;
}

static ssize_t autocalibration_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int data;

	sscanf(buf, "%d\n", &data);
	dev_dbg(&tkey_i2c->client->dev, "%s %d\n", __func__, data);

	if (data == 1)
		touchkey_autocalibration(tkey_i2c);

	return size;
}

static ssize_t autocalibration_status(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	u8 data[6];
	int ret;
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);

	ret = i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, 6);
	if ((data[5] & TK_BIT_AUTOCAL))
		return sprintf(buf, "Enabled\n");
	else
		return sprintf(buf, "Disabled\n");

}

#if defined(CONFIG_GLOVE_TOUCH)
static ssize_t glove_mode_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int data;

	sscanf(buf, "%d\n", &data);
	dev_dbg(&tkey_i2c->client->dev, "%s %d\n", __func__, data);

	tkey_i2c->tsk_enable_glove_mode = data;
	touchkey_glovemode(tkey_i2c, data);

	return size;
}
#endif

#ifdef TKEY_FLIP_MODE
static ssize_t flip_cover_mode_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int flip_mode_on;

	sscanf(buf, "%d\n", &flip_mode_on);
	dev_info(&tkey_i2c->client->dev, "%s %d\n", __func__, flip_mode_on);

	/* ignoring duplicate Flip_Close request if it is already in closed state */
	flip_mode_on = !!flip_mode_on;
	if (tkey_i2c->flip_status == flip_mode_on)
		return size;

	touchkey_flip_cover(tkey_i2c, flip_mode_on);

	/* glove mode control */
	if (flip_mode_on) {
		tkey_i2c->tsk_glove_mode_status = false;
		if (false == tkey_i2c->pdata->glove_mode_keep_status)
			tkey_i2c->tsk_enable_glove_mode = false;
	} else {
		if (tkey_i2c->tsk_enable_glove_mode)
			touchkey_glovemode(tkey_i2c, 1);
	}
	tkey_i2c->flip_status = flip_mode_on;
	return size;
}
#endif

static ssize_t touch_sensitivity_control(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	unsigned char data = 0x40;
	i2c_touchkey_write(tkey_i2c, KEYCODE_REG, &data, 1);
	dev_dbg(&tkey_i2c->client->dev, "%s\n", __func__);
	msleep(20);
	return size;
}

static ssize_t set_touchkey_update_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	u8 fw_path;

	switch(*buf) {
	case 's':
	case 'S':
		fw_path = FW_BUILT_IN;
		break;
	case 'i':
	case 'I':
		fw_path = FW_IN_SDCARD;
		break;
	default:
		return size;
	}

	touchkey_fw_update(tkey_i2c, fw_path, true);

	msleep(3000);
	cancel_work_sync(&tkey_i2c->update_work);

	return size;
}

static ssize_t set_touchkey_firm_version_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	printk(KERN_DEBUG"touchkey:firm_ver_bin %0#4x\n", fw_ver_file);
	return sprintf(buf, "%0#4x\n", fw_ver_file);
}

static ssize_t set_touchkey_firm_version_read_show(struct device *dev,
						   struct device_attribute
						   *attr, char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	char data[3] = { 0, };
	int count;

	i2c_touchkey_read(tkey_i2c, KEYCODE_REG, data, 3);
	count = sprintf(buf, "0x%02x\n", data[1]);

	dev_info(&tkey_i2c->client->dev, "Touch_version_read 0x%02x\n", data[1]);
	dev_info(&tkey_i2c->client->dev, "Module_version_read 0x%02x\n", data[2]);
	return count;
}

static ssize_t set_touchkey_firm_status_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);
	int count = 0;

	dev_info(&tkey_i2c->client->dev, "Touch_update_read: update_status %d\n",
	       tkey_i2c->update_status);

	if (tkey_i2c->update_status == TK_UPDATE_PASS)
		count = sprintf(buf, "PASS\n");
	else if (tkey_i2c->update_status == TK_UPDATE_DOWN)
		count = sprintf(buf, "Downloading\n");
	else if (tkey_i2c->update_status == TK_UPDATE_FAIL)
		count = sprintf(buf, "Fail\n");

	return count;
}

static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		   touchkey_led_control);
#ifdef TK_USE_RECENT
static DEVICE_ATTR(touchkey_recent, S_IRUGO | S_IWUSR | S_IWGRP,
		   touchkey_menu_show, NULL);
#else
static DEVICE_ATTR(touchkey_menu, S_IRUGO | S_IWUSR | S_IWGRP,
		   touchkey_menu_show, NULL);
#endif
static DEVICE_ATTR(touchkey_back, S_IRUGO | S_IWUSR | S_IWGRP,
		   touchkey_back_show, NULL);

#if defined(TK_USE_4KEY)
static DEVICE_ATTR(touchkey_home, S_IRUGO, touchkey_home_show, NULL);
static DEVICE_ATTR(touchkey_search, S_IRUGO, touchkey_search_show, NULL);
#endif

static DEVICE_ATTR(touch_sensitivity, S_IRUGO | S_IWUSR | S_IWGRP,
		   NULL, touch_sensitivity_control);
static DEVICE_ATTR(touchkey_firm_update, S_IRUGO | S_IWUSR | S_IWGRP,
		   NULL, set_touchkey_update_store);
static DEVICE_ATTR(touchkey_firm_update_status, S_IRUGO | S_IWUSR | S_IWGRP,
		   set_touchkey_firm_status_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO | S_IWUSR | S_IWGRP,
		   set_touchkey_firm_version_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO | S_IWUSR | S_IWGRP,
		   set_touchkey_firm_version_read_show, NULL);
#ifdef LED_LDO_WITH_REGULATOR
static DEVICE_ATTR(touchkey_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
		   NULL, brightness_control);
#endif
#ifdef TK_USE_RECENT
static DEVICE_ATTR(touchkey_recent_raw, S_IRUGO, touchkey_raw_data0_show, NULL);
static DEVICE_ATTR(touchkey_back_raw, S_IRUGO, touchkey_raw_data1_show, NULL);
static DEVICE_ATTR(touchkey_recent_idac, S_IRUGO, touchkey_idac0_show, NULL);
static DEVICE_ATTR(touchkey_back_idac, S_IRUGO, touchkey_idac1_show, NULL);
#else
static DEVICE_ATTR(touchkey_raw_data0, S_IRUGO, touchkey_raw_data0_show, NULL);
static DEVICE_ATTR(touchkey_raw_data1, S_IRUGO, touchkey_raw_data1_show, NULL);
static DEVICE_ATTR(touchkey_raw_data2, S_IRUGO, touchkey_raw_data2_show, NULL);
static DEVICE_ATTR(touchkey_raw_data3, S_IRUGO, touchkey_raw_data3_show, NULL);
static DEVICE_ATTR(touchkey_idac0, S_IRUGO, touchkey_idac0_show, NULL);
static DEVICE_ATTR(touchkey_idac1, S_IRUGO, touchkey_idac1_show, NULL);
static DEVICE_ATTR(touchkey_idac2, S_IRUGO, touchkey_idac2_show, NULL);
static DEVICE_ATTR(touchkey_idac3, S_IRUGO, touchkey_idac3_show, NULL);
#endif
static DEVICE_ATTR(touchkey_threshold, S_IRUGO, touchkey_threshold_show, NULL);
static DEVICE_ATTR(autocal_enable, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		   autocalibration_enable);
static DEVICE_ATTR(autocal_stat, S_IRUGO | S_IWUSR | S_IWGRP,
		   autocalibration_status, NULL);
#if defined(CONFIG_GLOVE_TOUCH)
static DEVICE_ATTR(glove_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		   glove_mode_enable);
#endif
#ifdef TKEY_FLIP_MODE
static DEVICE_ATTR(flip_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		   flip_cover_mode_enable);
#endif

static struct attribute *touchkey_attributes[] = {
	&dev_attr_brightness.attr,
#ifdef TK_USE_RECENT
	&dev_attr_touchkey_recent.attr,
#else
	&dev_attr_touchkey_menu.attr,
#endif
	&dev_attr_touchkey_back.attr,
#if defined(TK_USE_4KEY)
	&dev_attr_touchkey_home.attr,
	&dev_attr_touchkey_search.attr,
#endif
	&dev_attr_touch_sensitivity.attr,
	&dev_attr_touchkey_firm_update.attr,
	&dev_attr_touchkey_firm_update_status.attr,
	&dev_attr_touchkey_firm_version_phone.attr,
	&dev_attr_touchkey_firm_version_panel.attr,
#ifdef LED_LDO_WITH_REGULATOR
	&dev_attr_touchkey_brightness.attr,
#endif
#ifdef TK_USE_RECENT
	&dev_attr_touchkey_recent_raw.attr,
	&dev_attr_touchkey_back_raw.attr,
	&dev_attr_touchkey_recent_idac.attr,
	&dev_attr_touchkey_back_idac.attr,
#else
	&dev_attr_touchkey_raw_data0.attr,
	&dev_attr_touchkey_raw_data1.attr,
	&dev_attr_touchkey_raw_data2.attr,
	&dev_attr_touchkey_raw_data3.attr,
	&dev_attr_touchkey_idac0.attr,
	&dev_attr_touchkey_idac1.attr,
	&dev_attr_touchkey_idac2.attr,
	&dev_attr_touchkey_idac3.attr,
#endif
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_autocal_enable.attr,
	&dev_attr_autocal_stat.attr,
#if defined(CONFIG_GLOVE_TOUCH)
	&dev_attr_glove_mode.attr,
#endif
#ifdef TKEY_FLIP_MODE
	&dev_attr_flip_mode.attr,
#endif
	NULL,
};

static struct attribute_group touchkey_attr_group = {
	.attrs = touchkey_attributes,
};

static int touchkey_pinctrl_init(struct touchkey_i2c *tkey_i2c)
{
	struct device *dev = &tkey_i2c->client->dev;
	int i;

	tkey_i2c->pinctrl_irq = devm_pinctrl_get(dev);
	if (IS_ERR(tkey_i2c->pinctrl_irq)) {
		printk(KERN_DEBUG"%s: Failed to get pinctrl\n", __func__);
		goto err_pinctrl_get;
	}
	for (i = 0; i < 2; ++i) {
		tkey_i2c->pin_state[i] = pinctrl_lookup_state(tkey_i2c->pinctrl_irq, str_states[i]);
		if (IS_ERR(tkey_i2c->pin_state[i])) {
			printk(KERN_DEBUG"%s: Failed to get pinctrl state\n", __func__);
			goto err_pinctrl_get_state;
		}
	}

	/* for h/w i2c */
	if (!tkey_i2c->pdata->i2c_gpio) {
		dev = tkey_i2c->client->dev.parent->parent;
		printk(KERN_DEBUG"%s: use dev's parent\n", __func__);
	}

	tkey_i2c->pinctrl_i2c = devm_pinctrl_get(dev);
	if (IS_ERR(tkey_i2c->pinctrl_i2c)) {
		printk(KERN_DEBUG"%s: Failed to get pinctrl\n", __func__);
		goto err_pinctrl_get_i2c;
	}
	for (i = 2; i < 4; ++i) {
		tkey_i2c->pin_state[i] = pinctrl_lookup_state(tkey_i2c->pinctrl_i2c, str_states[i]);
		if (IS_ERR(tkey_i2c->pin_state[i])) {
			printk(KERN_DEBUG"%s: Failed to get pinctrl state\n", __func__);
			goto err_pinctrl_get_state_i2c;
		}
	}

	return 0;

err_pinctrl_get_state_i2c:
	devm_pinctrl_put(tkey_i2c->pinctrl_i2c);
err_pinctrl_get_i2c:
err_pinctrl_get_state:
	devm_pinctrl_put(tkey_i2c->pinctrl_irq);
err_pinctrl_get:
	return -ENODEV;
}

int touchkey_pinctrl(struct touchkey_i2c *tkey_i2c, int state)
{
	struct pinctrl *pinctrl = tkey_i2c->pinctrl_irq;
	int ret = 0;

	if (state >= I_STATE_ON_I2C)
		if (false == tkey_i2c->pdata->i2c_gpio)
			pinctrl = tkey_i2c->pinctrl_i2c;

	ret = pinctrl_select_state(pinctrl, tkey_i2c->pin_state[state]);
	if (ret < 0) {
		dev_err(&tkey_i2c->client->dev,
			"%s: Failed to configure irq pin\n", __func__);
		return ret;
	}

	return 0;
}

static int touchkey_power_on(void *data, bool on) {

	struct touchkey_i2c *tkey_i2c = (struct touchkey_i2c *)data;
	struct device *dev = &tkey_i2c->client->dev;
	struct regulator *regulator;
	static bool enabled;
	int ret = 0;
	int state = on ? I_STATE_ON_IRQ : I_STATE_OFF_IRQ;

	if (enabled == on) {
		dev_err(dev,
			"%s : TK power already %s\n", __func__,(on)?"on":"off");
		return ret;
	}

	dev_info(dev, "%s: %s",__func__,(on)?"on":"off");

	regulator = regulator_get(NULL, TK_REGULATOR_NAME);
	if (IS_ERR(regulator)) {
		dev_err(dev,
			"%s : TK regulator_get failed\n", __func__);
		return -EIO;
	}

	if (on) {
		ret = regulator_enable(regulator);
		if (ret) {
			dev_err(dev,
				"%s: Failed to enable avdd: %d\n", __func__, ret);
			return ret;
		}
	} else {
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
	}

	ret = touchkey_pinctrl(tkey_i2c, state);
	if (ret < 0)
		dev_err(dev,
		"%s: Failed to configure irq pin\n", __func__);

	regulator_put(regulator);

	enabled = on;

	return 1;
}
static int touchkey_led_power_on(void *data, bool on)
{
	struct touchkey_i2c *tkey_i2c = (struct touchkey_i2c *)data;
	struct regulator *regulator;
	static bool enabled;
	int ret = 0;

	if (enabled == on) {
		dev_err(&tkey_i2c->client->dev,
			"TK led power already %s\n", on ? "on" : "off");
		return ret;
	}

	dev_info(&tkey_i2c->client->dev, "led turned %s", on ? "on" : "off");

	regulator = regulator_get(NULL, TK_LED_REGULATOR_NAME);
	if (IS_ERR(regulator)) {
		dev_err(&tkey_i2c->client->dev,
			"Failed to get regulator: %d\n", ret);
		return 0;
	}

	if (on) {
		ret = regulator_enable(regulator);
		if (ret) {
			dev_err(&tkey_i2c->client->dev,
				"Failed to enable led: %d\n", ret);
			return ret;
		}
	} else {
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
	}

	regulator_put(regulator);

	enabled = on;

	return 1;
}

static void cypress_config_gpio_i2c(struct touchkey_i2c *tkey_i2c, int onoff)
{
	int ret;
	int state = onoff ? I_STATE_ON_I2C : I_STATE_OFF_I2C;

	ret = touchkey_pinctrl(tkey_i2c, state);
	if (ret < 0)
		dev_err(&tkey_i2c->client->dev,
		"%s: Failed to configure i2c pin\n", __func__);
}

#ifdef CONFIG_OF
static void cypress_request_gpio(struct i2c_client *client, struct touchkey_platform_data *pdata)
{
	int ret;

	if (!pdata->i2c_gpio) {
		ret = gpio_request(pdata->gpio_scl, "touchkey_scl");
		if (ret) {
			dev_err(&client->dev,
				"%s: unable to request touchkey_scl [%d]\n",
				__func__, pdata->gpio_scl);
			return;
		}

		ret = gpio_request(pdata->gpio_sda, "touchkey_sda");
		if (ret) {
			dev_err(&client->dev,"%s: unable to request touchkey_sda [%d]\n",
				__func__, pdata->gpio_sda);
			return;
		}
	}

	ret = gpio_request(pdata->gpio_int, "touchkey_irq");
	if (ret) {
		dev_err(&client->dev,"%s: unable to request touchkey_irq [%d]\n",
			__func__, pdata->gpio_int);
		return;
	}
}

static struct touchkey_platform_data *cypress_parse_dt(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct touchkey_platform_data *pdata;
	struct device_node *np = dev->of_node;
	int ret = 0;

	if (!np)
		return ERR_PTR(-ENOENT);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "failed to allocate platform data\n");
		return ERR_PTR(-ENOMEM);
	}
	dev->platform_data = pdata;

	/* reset, irq gpio info */
	pdata->gpio_scl = of_get_named_gpio_flags(np, "cypress,scl-gpio",
		0, &pdata->scl_gpio_flags);
	pdata->gpio_sda = of_get_named_gpio_flags(np, "cypress,sda-gpio",
		0, &pdata->sda_gpio_flags);
	pdata->gpio_int = of_get_named_gpio_flags(np, "cypress,irq-gpio",
		0, &pdata->irq_gpio_flags);
	pdata->i2c_gpio = of_property_read_bool(np, "cypress,i2c-gpio");

	if(of_property_read_u8(np, "cypress,fw_crc_check", &pdata->fw_crc_check)){
		printk(KERN_ERR"touchkey:Can't find cypress,fw_crc_check!\n");
		pdata->fw_crc_check = 1;
	}
	printk(KERN_INFO"touchkey:Check FW CRC status %s\n",
						pdata->fw_crc_check ? "ON" : "OFF");

	if(of_property_read_u8(np, "cypress,tk_use_lcdtype_check", &pdata->tk_use_lcdtype_check)){
		printk(KERN_ERR"touchkey:Can't find cypress,tk_use_lcdtype_check!\n");
		pdata->tk_use_lcdtype_check = 1;
	}
	printk(KERN_INFO"touchkey:Check lcd type for conneter status %s\n",
						pdata->tk_use_lcdtype_check ? "ON" : "OFF");

	ret = of_property_read_u32(np, "cypress,ic_type", &pdata->ic_type);
	if (ret) {
		printk(KERN_ERR"touchkey:failed to read ic_type %d\n", ret);
		pdata->ic_type = CYPRESS_IC_20055;
	}

	ret = of_property_read_u32(np,"cypress,multi-touchkey-fw-version",&pdata->multi_fw_ver);
	if (ret) {
		printk(KERN_ERR"touchkey:failed to multi-touchkey-fw-version %d\n", ret);
		pdata->multi_fw_ver = 0xff;
	}

	ret = of_property_read_string(np, "cypress,fw_path", (const char **)&pdata->fw_path);
	if (ret) {
		printk(KERN_ERR"touchkey:failed to read fw_path %d\n", ret);
		pdata->fw_path = FW_PATH;
	}

	if (of_find_property(np, "cypress,boot_on_ldo", NULL))
		pdata->boot_on_ldo = true;
	if (of_find_property(np, "cypress,led_by_ldo", NULL))
		pdata->led_by_ldo = true;
	if (of_find_property(np, "cypress,glove_mode_keep_status", NULL))
		pdata->glove_mode_keep_status = true;

#if 0
	printk(KERN_DEBUG"touchkey:scl %d, sda %d, int %d, i2c-gpio %d, ic %d\n",
		pdata->gpio_scl, pdata->gpio_sda, pdata->gpio_int, pdata->i2c_gpio, pdata->ic_type);
	printk(KERN_DEBUG"touchkey:fw path %s\n", pdata->fw_path);
#endif

	cypress_request_gpio(client, pdata);

	return pdata;
}
#endif

static void cypress_reset_regs(void)
{
	/* set regs for 20075 */
	regs[iKeycode] = 0x03;
	regs[iFwver] = 0x04;

	/*printk(KERN_DEBUG"touchkey:%s, %#x %#x\n", __func__, regs[0], regs[1]);*/
}

static int i2c_touchkey_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct touchkey_platform_data *pdata = client->dev.platform_data;
	irqreturn_t (*int_handler)(int, void *) = touchkey_interrupt;
	struct touchkey_i2c *tkey_i2c;
	bool bforced = false;
	struct input_dev *input_dev;
	int i;
	int ret = 0;

	if (lpcharge == 1) {
		dev_err(&client->dev, "%s : Do not load driver due to : lpm %d\n",
			__func__, lpcharge);
		return -ENODEV;
	}

	if (!pdata) {
		pdata = cypress_parse_dt(client);
		if (!pdata) {
			dev_err(&client->dev, "%s: no pdata\n", __func__);
			return -EINVAL;
		}
		if (pdata->ic_type == CYPRESS_IC_20075)
			cypress_reset_regs();
	}

	/*Check I2C functionality */
	ret = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (ret == 0) {
		dev_err(&client->dev, "No I2C functionality found\n");
		return -ENODEV;
	}

	/*Obtain kernel memory space for touchkey i2c */
	tkey_i2c = kzalloc(sizeof(struct touchkey_i2c), GFP_KERNEL);
	if (NULL == tkey_i2c) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "Failed to allocate input device\n");
		ret = -ENOMEM;
		goto err_allocate_input_device;
	}

	input_dev->name = "sec_touchkey";
	input_dev->phys = "sec_touchkey/input0";
	input_dev->id.bustype = BUS_HOST;
	input_dev->dev.parent = &client->dev;
	input_dev->open = touchkey_input_open;
	input_dev->close = touchkey_input_close;

	/*tkey_i2c*/
	tkey_i2c->pdata = pdata;
	tkey_i2c->input_dev = input_dev;
	tkey_i2c->client = client;
	tkey_i2c->irq = client->irq;
	tkey_i2c->name = "sec_touchkey";
	tkey_i2c->status_update = false;
	tkey_i2c->pdata->power_on = touchkey_power_on;
	tkey_i2c->pdata->led_power_on = touchkey_led_power_on;

	init_completion(&tkey_i2c->init_done);
	mutex_init(&tkey_i2c->lock);
	mutex_init(&tkey_i2c->i2c_lock);
	mutex_init(&tkey_i2c->irq_lock);
#ifdef TK_USE_OPEN_DWORK
	INIT_DELAYED_WORK(&tkey_i2c->open_work, touchkey_open_work);
#endif
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_LED, input_dev->evbit);
	set_bit(LED_MISC, input_dev->ledbit);
	set_bit(EV_KEY, input_dev->evbit);
#ifdef CONFIG_VT_TKEY_SKIP_MATCH
	set_bit(EV_TOUCHKEY, input_dev->evbit);
#endif
	INIT_WORK(&tkey_i2c->update_work, touchkey_i2c_update_work);
	wake_lock_init(&tkey_i2c->fw_wakelock, WAKE_LOCK_SUSPEND, "touchkey");

	for (i = 1; i < tkey_cnt; i++)
		set_bit(tkey_code[i], input_dev->keybit);

	input_set_drvdata(input_dev, tkey_i2c);
	i2c_set_clientdata(client, tkey_i2c);

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev, "Failed to register input device\n");
		goto err_register_device;
	}

	/* pinctrl */
	ret = touchkey_pinctrl_init(tkey_i2c);
	if (ret < 0) {
		dev_err(&tkey_i2c->client->dev,
			"%s: Failed to init pinctrl: %d\n", __func__, ret);
		goto err_register_device;
	}

	tkey_i2c->pdata->power_on(tkey_i2c, 1);
	if (false == tkey_i2c->pdata->boot_on_ldo) {
		if (tkey_i2c->autocal)
			msleep(50);
		else
			msleep(300);
	}
	tkey_i2c->enabled = true;
	tkey_i2c->flip_status = 0;

	/*sysfs*/
	tkey_i2c->dev = sec_device_create(tkey_i2c, "sec_touchkey");

	ret = IS_ERR(tkey_i2c->dev);
	if (ret) {
		dev_err(&client->dev, "Failed to create device(tkey_i2c->dev)!\n");
		goto err_device_create;
	}
	dev_set_drvdata(tkey_i2c->dev, tkey_i2c);

	ret = sysfs_create_group(&tkey_i2c->dev->kobj,
				&touchkey_attr_group);
	if (ret) {
		dev_err(&client->dev, "Failed to create sysfs group\n");
		goto err_sysfs_init;
	}

	ret = sysfs_create_link(&tkey_i2c->dev->kobj,
					&tkey_i2c->input_dev->dev.kobj, "input");
	if (ret) {
		dev_err(&client->dev, "Failed to connect link\n");
		goto err_create_symlink;
	}

	tkey_i2c->fw_wq = create_singlethread_workqueue(client->name);
	if (!tkey_i2c->fw_wq) {
		dev_err(&client->dev, "fail to create workqueue for fw_wq\n");
		ret = -ENOMEM;
		goto err_create_fw_wq;
	}

	if (tkey_i2c->pdata->tk_use_lcdtype_check) {
		dev_err(&client->dev, "LCD type Print : 0x%06X\n", lcdtype);
		if (lcdtype == 0) {
			dev_err(&client->dev, "Device wasn't connected to board\n");
			ret = -ENODEV;
			goto err_i2c_check;
		}
	}

	ret = touchkey_i2c_check(tkey_i2c);
	if (ret < 0) {
		dev_err(&client->dev, "i2c_check failed\n");

		if (tkey_i2c->pdata->tk_use_lcdtype_check)
			bforced = true;
		else
			goto err_i2c_check;
	}

	cypress_config_gpio_i2c(tkey_i2c, 1);

#if defined(CONFIG_GLOVE_TOUCH)
	mutex_init(&tkey_i2c->tsk_glove_lock);
	INIT_DELAYED_WORK(&tkey_i2c->glove_change_work, touchkey_glove_change_work);
	tkey_i2c->tsk_glove_mode_status = false;
#endif

	ret =
		request_threaded_irq(tkey_i2c->irq, NULL, int_handler,
				IRQF_DISABLED | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT, tkey_i2c->name, tkey_i2c);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to request irq(%d) - %d\n",
			tkey_i2c->irq, ret);
		goto err_request_threaded_irq;
	}
	printk(KERN_DEBUG"touchkey:init irq %d\n", tkey_i2c->irq);

	if (false == tkey_i2c->pdata->led_by_ldo)
		tkey_i2c->pdata->led_power_on(tkey_i2c, 1);

#if defined(TK_HAS_FIRMWARE_UPDATE)
//tkey_firmupdate_retry_byreboot:

	ret = touchkey_fw_update(tkey_i2c, FW_BUILT_IN, bforced);
	if (ret) {
		dev_err(&client->dev, "Failed to update fw(%d)\n", ret);
		goto err_firmware_update;
	}
#endif

#if defined(TK_INFORM_CHARGER)
	tkey_i2c->callbacks.inform_charger = touchkey_ta_cb;
	if (tkey_i2c->pdata->register_cb) {
		dev_info(&client->dev, "Touchkey TA information\n");
		tkey_i2c->pdata->register_cb(&tkey_i2c->callbacks);
	}
#endif

#if defined(CONFIG_KEYBOARD_KLIMTVE) && defined(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&tkey_i2c->touchkey_nb,
		touchkey_handle_notification, MUIC_NOTIFY_DEV_TSP);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	tkey_i2c->early_suspend.suspend =
		(void *)sec_touchkey_early_suspend;
	tkey_i2c->early_suspend.resume =
		(void *)sec_touchkey_late_resume;
	register_early_suspend(&tkey_i2c->early_suspend);
#endif

/*	touchkey_stop(tkey_i2c); */
	complete_all(&tkey_i2c->init_done);
	touchkey_probe = true;

	return 0;

#if defined(TK_HAS_FIRMWARE_UPDATE)
err_firmware_update:
	tkey_i2c->pdata->led_power_on(tkey_i2c, 0);
	touchkey_enable_irq(tkey_i2c, false);
	free_irq(tkey_i2c->irq, tkey_i2c);
#endif
err_request_threaded_irq:
#ifdef CONFIG_GLOVE_TOUCH
	mutex_destroy(&tkey_i2c->tsk_glove_lock);
#endif
err_i2c_check:
	destroy_workqueue(tkey_i2c->fw_wq);
err_create_fw_wq:
	sysfs_delete_link(&tkey_i2c->dev->kobj,
		&tkey_i2c->input_dev->dev.kobj, "input");
err_create_symlink:
	sysfs_remove_group(&tkey_i2c->dev->kobj,
		&touchkey_attr_group);
err_sysfs_init:
	sec_device_destroy(tkey_i2c->dev->devt);
err_device_create:
	tkey_i2c->pdata->power_on(tkey_i2c, 0);
	input_unregister_device(input_dev);
	input_dev = NULL;
err_register_device:
	wake_lock_destroy(&tkey_i2c->fw_wakelock);
	mutex_destroy(&tkey_i2c->irq_lock);
	mutex_destroy(&tkey_i2c->i2c_lock);
	mutex_destroy(&tkey_i2c->lock);
	input_free_device(input_dev);
err_allocate_input_device:
	kfree(tkey_i2c);
	return ret;
}

void touchkey_shutdown(struct i2c_client *client)
{
	struct touchkey_i2c *tkey_i2c = i2c_get_clientdata(client);

	if (!tkey_i2c)
		return;

	tkey_i2c->pdata->power_on(tkey_i2c, 0);
	tkey_i2c->pdata->led_power_on(tkey_i2c, 0);

	printk(KERN_DEBUG"touchkey:%s\n", __func__);

	sec_device_destroy(tkey_i2c->dev->devt);
}

#ifdef CONFIG_OF
static struct of_device_id cypress_touchkey_dt_ids[] = {
	{ .compatible = "cypress,cypress_touchkey" },
	{ }
};
#endif

struct i2c_driver touchkey_i2c_driver = {
	.driver = {
		.name = "sec_touchkey",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &touchkey_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(cypress_touchkey_dt_ids),
#endif
	},
	.id_table = sec_touchkey_id,
	.probe = i2c_touchkey_probe,
	.shutdown = &touchkey_shutdown,
};

static int __init touchkey_init(void)
{
#ifdef TEST_JIG_MODE
	unsigned char get_touch = 0x40;
#endif

	i2c_add_driver(&touchkey_i2c_driver);

#ifdef TEST_JIG_MODE
	i2c_touchkey_write(tkey_i2c, KEYCODE_REG, &get_touch, 1);
#endif
	return 0;
}

static void __exit touchkey_exit(void)
{
	i2c_del_driver(&touchkey_i2c_driver);
	touchkey_probe = false;
}

module_init(touchkey_init);
module_exit(touchkey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("@@@");
MODULE_DESCRIPTION("touch keypad");
