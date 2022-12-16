// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/backlight.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/lcd.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/reboot.h>
#include <video/mipi_display.h>

#include "../smcdsd_board.h"
#include "../smcdsd_panel.h"
#include "smcdsd_notify.h"

#include "a02_nt36675_csot_param.h"
#include "dd.h"

#define dbg_info(fmt, ...)	pr_info(pr_fmt("%s: %3d: %s: " fmt), "lcd panel", __LINE__, __func__, ##__VA_ARGS__)

#define PANEL_STATE_SUSPENED	0
#define PANEL_STATE_RESUMED	1

#define DSI_WRITE(cmd, size)		do {				\
{	\
	int tx_ret = 0;	\
	tx_ret = smcdsd_dsi_tx_data(lcd, cmd, size);		\
	if (tx_ret < 0)							\
		dev_info(&lcd->ld->dev, "%s: tx_ret(%d) failed to write %02x %s\n", __func__, tx_ret, cmd[0], #cmd);	\
}	\
} while (0)

struct lcd_info {
	unsigned int			connected;
	unsigned int			brightness;
	unsigned int			state;
	unsigned int			enable;
	unsigned int			shutdown;

	struct lcd_device		*ld;
	struct backlight_device		*bd;

	union {
		struct {
			u8		reserved;
			u8		id[LDI_LEN_ID];
		};
		u32			value;
	} id_info;

	int				lux;

	struct mipi_dsi_lcd_common	*pdata;
	struct mutex			lock;
	unsigned int			need_primary_lock;

	struct notifier_block		fb_notif_panel;
	struct notifier_block		reboot_notifier;
	struct i2c_client		*blic_client;

	unsigned int			fac_info;
	unsigned int			fac_done;

	unsigned int			conn_det_enable;
	unsigned int			conn_det_count;
	struct workqueue_struct		*conn_workqueue;
	struct work_struct		conn_work;
};

static struct lcd_info *get_lcd_info(struct platform_device *p)
{
	return platform_get_drvdata(p);
}

static void set_lcd_info(struct platform_device *p, void *data)
{
	platform_set_drvdata(p, data);
}

static struct mipi_dsi_lcd_common *get_lcd_common_info(struct platform_device *p)
{
	struct device *parent = p->dev.parent;

	return dev_get_drvdata(parent);
}

static int smcdsd_dsi_tx_data(struct lcd_info *lcd, u8 *cmd, u32 len)
{
	int ret = 0;
	int retry = 2;

	if (!lcd->connected)
		return ret;

	/*
	 * We assume that all the TX function will be called in lcd->lock
	 * If not, Stop here for debug.
	 */
	if (IS_ENABLED(CONFIG_MTK_FB) && !mutex_is_locked(&lcd->lock)) {
		dev_info(&lcd->ld->dev, "%s: fail. lcd->lock should be locked.\n", __func__);
		BUG();
	}

try_write:
	if (len == 1)
		ret = lcd->pdata->tx(lcd->pdata, MIPI_DSI_DCS_SHORT_WRITE, (unsigned long)cmd, len, lcd->need_primary_lock);
	else if (len == 2)
		ret = lcd->pdata->tx(lcd->pdata, MIPI_DSI_DCS_SHORT_WRITE_PARAM, (unsigned long)cmd, len, lcd->need_primary_lock);
	else
		ret = lcd->pdata->tx(lcd->pdata, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)cmd, len, lcd->need_primary_lock);

	if (ret < 0) {
		if (--retry)
			goto try_write;
		else
			dev_info(&lcd->ld->dev, "%s: fail. %02x, ret: %d\n", __func__, cmd[0], ret);
	}

	return ret;
}

static int smcdsd_dsi_tx_set(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0, i;

	for (i = 0; i < num; i++) {
		if (seq[i].cmd) {
			ret = smcdsd_dsi_tx_data(lcd, seq[i].cmd, seq[i].len);
			if (ret < 0) {
				dev_info(&lcd->ld->dev, "%s: %dth fail\n", __func__, i);
				return ret;
			}
		}
		if (seq[i].sleep)
			usleep_range(seq[i].sleep, seq[i].sleep + (seq[i].sleep >> 1));
	}
	return ret;
}

static int _smcdsd_dsi_rx_data(struct lcd_info *lcd, u8 cmd, u32 size, u8 *buf, u32 offset)
{
	int ret = 0, rx_size = 0;
	int retry = 2;

	if (!lcd->connected)
		return ret;

try_read:
	rx_size = lcd->pdata->rx(lcd->pdata, MIPI_DSI_DCS_READ, offset, cmd, size, buf, lcd->need_primary_lock);
	dev_info(&lcd->ld->dev, "%s: %2d(%2d), %02x, %*ph%s\n", __func__, size, rx_size, cmd,
		min_t(u32, min_t(u32, size, rx_size), 10), buf, (rx_size > 10) ? "..." : "");
	if (rx_size != size) {
		if (--retry)
			goto try_read;
		else {
			dev_info(&lcd->ld->dev, "%s: fail. %02x, %d(%d)\n", __func__, cmd, size, rx_size);
			ret = -EPERM;
		}
	}

	return ret;
}

static int _smcdsd_dsi_rx_data_offset(struct lcd_info *lcd, u8 addr, u32 size, u8 *buf, u32 offset)
{
	int ret = 0;

	ret = _smcdsd_dsi_rx_data(lcd, addr, size, buf, offset);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);

	return ret;
}

static int smcdsd_dsi_rx_data(struct lcd_info *lcd, u8 addr, u32 size, u8 *buf)
{
	int ret = 0, remain, limit, offset, slice;

	limit = 10;
	remain = size;
	offset = 0;

again:
	slice = (remain > limit) ? limit : remain;
	ret = _smcdsd_dsi_rx_data_offset(lcd, addr, slice, &buf[offset], offset);
	if (ret < 0) {
		dev_info(&lcd->ld->dev, "%s: fail\n", __func__);
		return ret;
	}

	remain -= limit;
	offset += limit;

	if (remain > 0)
		goto again;

	return ret;
}

static int smcdsd_dsi_rx_info(struct lcd_info *lcd, u8 reg, u32 len, u8 *buf)
{
	int ret = 0, i;

	ret = smcdsd_dsi_rx_data(lcd, reg, len, buf);
	if (ret < 0) {
		dev_info(&lcd->ld->dev, "%s: fail. %02x, ret: %d\n", __func__, reg, ret);
		goto exit;
	}

	dev_dbg(&lcd->ld->dev, "%s: %02xh\n", __func__, reg);
	for (i = 0; i < len; i++)
		dev_dbg(&lcd->ld->dev, "%02dth value is %02x, %3d\n", i + 1, buf[i], buf[i]);

exit:
	return ret;
}

static int i2c_lcd_bias_array_write(struct i2c_client *client, u8 *ptr, u8 len)
{
	unsigned int i = 0, delay;
	int ret = 0;
	u8 type = 0, command = 0, value = 0;
	struct lcd_info *lcd = NULL;

	if (!client)
		return ret;

	lcd = i2c_get_clientdata(client);
	if (!lcd)
		return ret;

	if (!lcdtype) {
		dev_info(&lcd->ld->dev, "%s: lcdtype: %d\n", __func__, lcdtype);
		return ret;
	}

	if (len % 3) {
		dev_info(&lcd->ld->dev, "%s: length(%d) invalid\n", __func__, len);
		return ret;
	}

	for (i = 0; i < len; i += 3) {
		type = ptr[i + 0];
		command = ptr[i + 1];
		value = ptr[i + 2];

		if (type == TYPE_DELAY) {
			delay = command * (u8)USEC_PER_MSEC;
			usleep_range(delay, delay + (delay >> 1));
		} else {
			ret = i2c_smbus_write_byte_data(client, command, value);
			if (ret < 0)
				dev_info(&lcd->ld->dev, "%s: fail. %2x, %2x, %d\n", __func__, command, value, ret);
		}
	}

	return ret;
}

static int smcdsd_panel_set_brightness(struct lcd_info *lcd, int force)
{
	int ret = 0;
	unsigned char bl_reg[LDI_LEN_BRIGHTNESS] = {0, };

	lcd->brightness = lcd->bd->props.brightness;

	if (!force && (lcd->state != PANEL_STATE_RESUMED || !lcd->enable)) {
		dev_info(&lcd->ld->dev, "%s: brightness: %d(%d), panel_state: %d, %d\n", __func__,
		lcd->brightness, lcd->bd->props.brightness, lcd->state, lcd->enable);
		goto exit;
	}

	bl_reg[0] = LDI_REG_BRIGHTNESS;
	bl_reg[1] = brightness_table[lcd->brightness];

	DSI_WRITE(bl_reg, ARRAY_SIZE(bl_reg));

	dev_info(&lcd->ld->dev, "%s: brightness: %3d, %3d(%2x), lx: %d\n", __func__,
		lcd->brightness, brightness_table[lcd->brightness], bl_reg[1], lcd->lux);

exit:
	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return brightness_table[lcd->brightness];
}

static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	struct lcd_info *lcd = bl_get_data(bd);

	mutex_lock(&lcd->lock);
	if (lcd->state == PANEL_STATE_RESUMED) {
		ret = smcdsd_panel_set_brightness(lcd, 0);
		if (ret < 0)
			dev_info(&lcd->ld->dev, "%s: failed to set brightness\n", __func__);
	}
	mutex_unlock(&lcd->lock);

	return ret;
}

static int panel_check_fb_brightness(struct backlight_device *bd, struct fb_info *fb)
{
	return 0;
}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
	.check_fb = panel_check_fb_brightness,
};

static int nt36675_read_id(struct lcd_info *lcd)
{
	int i = 0, ret = 0;
	struct mipi_dsi_lcd_common *pdata = lcd->pdata;
	static char *LDI_BIT_DESC_ID[BITS_PER_BYTE * LDI_LEN_ID] = {
		[0 ... 23] = "ID Read Fail",
	};

	lcd->id_info.value = 0;

	for (i = 0; i < LDI_LEN_ID; i++) {
		ret = smcdsd_dsi_rx_info(lcd, LDI_REG_ID + i, 1, &lcd->id_info.id[i]);
		if (ret < 0)
			break;
	}

	if (ret < 0 || !lcd->id_info.value) {
		pdata->lcdconnected = lcd->connected = 0;
		dev_info(&lcd->ld->dev, "%s: connected lcd is invalid\n", __func__);

		if (lcdtype)
			smcdsd_abd_save_bit(&pdata->abd, BITS_PER_BYTE * LDI_LEN_ID, cpu_to_be32(lcd->id_info.value), LDI_BIT_DESC_ID);
	}

	dev_info(&lcd->ld->dev, "%s: %x\n", __func__, cpu_to_be32(lcd->id_info.value));

	return ret;
}

static int nt36675_read_init_info(struct lcd_info *lcd)
{
	int ret = 0;

	struct mipi_dsi_lcd_common *pdata = lcd->pdata;

	pdata->lcdconnected = lcd->connected = lcdtype ? 1 : 0;

	lcd->id_info.id[0] = (lcdtype & 0xFF0000) >> 16;
	lcd->id_info.id[1] = (lcdtype & 0x00FF00) >> 8;
	lcd->id_info.id[2] = (lcdtype & 0x0000FF) >> 0;

	dev_info(&lcd->ld->dev, "%s: %x\n", __func__, cpu_to_be32(lcd->id_info.value));

	if (!lcd->fac_info)
		return ret;

	nt36675_read_id(lcd);

	return ret;
}

static int nt36675_exit(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	/* DO NOT REMOVE: back to page 1 for setting DCS commands */
	DSI_WRITE(SEQ_NT36675_BACK_TO_PAGE_1_A, ARRAY_SIZE(SEQ_NT36675_BACK_TO_PAGE_1_A));
	DSI_WRITE(SEQ_NT36675_BACK_TO_PAGE_1_B, ARRAY_SIZE(SEQ_NT36675_BACK_TO_PAGE_1_B));

	DSI_WRITE(SEQ_NT36675_DISPLAY_OFF, ARRAY_SIZE(SEQ_NT36675_DISPLAY_OFF));

	msleep(20);

	DSI_WRITE(SEQ_NT36675_SLEEP_IN, ARRAY_SIZE(SEQ_NT36675_SLEEP_IN));

	return ret;
}

static int nt36675_init(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	nt36675_read_init_info(lcd);

	smcdsd_dsi_tx_set(lcd, LCD_SEQ_INIT_1, ARRAY_SIZE(LCD_SEQ_INIT_1));

	/* DO NOT REMOVE: back to page 1 for setting DCS commands */
	DSI_WRITE(SEQ_NT36675_BACK_TO_PAGE_1_A, ARRAY_SIZE(SEQ_NT36675_BACK_TO_PAGE_1_A));
	DSI_WRITE(SEQ_NT36675_BACK_TO_PAGE_1_B, ARRAY_SIZE(SEQ_NT36675_BACK_TO_PAGE_1_B));

	DSI_WRITE(SEQ_NT36675_BRIGHTNESS, ARRAY_SIZE(SEQ_NT36675_BRIGHTNESS));
	DSI_WRITE(SEQ_NT36675_BRIGHTNESS_ON, ARRAY_SIZE(SEQ_NT36675_BRIGHTNESS_ON));

	DSI_WRITE(SEQ_NT36675_SLEEP_OUT, ARRAY_SIZE(SEQ_NT36675_SLEEP_OUT));
	msleep(100);
	DSI_WRITE(SEQ_NT36675_DISPLAY_ON, ARRAY_SIZE(SEQ_NT36675_DISPLAY_ON));

	return ret;
}

static int fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct lcd_info *lcd = NULL;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
	case FB_EARLY_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	lcd = container_of(self, struct lcd_info, fb_notif_panel);

	fb_blank = *(int *)evdata->data;

	dev_info(&lcd->ld->dev, "%s: %02lx, %d\n", __func__, event, fb_blank);

	if (evdata->info->node)
		return NOTIFY_DONE;

	if (IS_ENABLED(CONFIG_MTK_FB))
		mutex_lock(&lcd->lock);

	if (IS_EARLY(event) && fb_blank == FB_BLANK_POWERDOWN) {
		lcd->enable = 0;
	} else if (IS_AFTER(event) && fb_blank == FB_BLANK_UNBLANK && !lcd->shutdown) {
		lcd->enable = 1;

		smcdsd_panel_set_brightness(lcd, 1);
	}

	if (IS_ENABLED(CONFIG_MTK_FB))
		mutex_unlock(&lcd->lock);

	return NOTIFY_DONE;
}

static int panel_reboot_notify(struct notifier_block *self,
			unsigned long event, void *unused)
{
	struct lcd_info *lcd = container_of(self, struct lcd_info, reboot_notifier);

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	mutex_lock(&lcd->lock);
	lcd->enable = 0;
	lcd->shutdown = 1;
	mutex_unlock(&lcd->lock);

	return NOTIFY_DONE;
}

static int nt36675_register_notifier(struct lcd_info *lcd)
{
	lcd->fb_notif_panel.notifier_call = fb_notifier_callback;
	smcdsd_register_notifier(&lcd->fb_notif_panel);

	lcd->reboot_notifier.notifier_call = panel_reboot_notify;
	register_reboot_notifier(&lcd->reboot_notifier);

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	return 0;
}

static int i2c_lcd_bias_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct lcd_info *lcd = NULL;
	int ret = 0;

	if (id && id->driver_data)
		lcd = (struct lcd_info *)id->driver_data;

	if (!lcd) {
		dbg_info("%s: failed to find driver_data for lcd\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_info(&lcd->ld->dev, "%s: need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	i2c_set_clientdata(client, lcd);

	lcd->blic_client = client;

	dev_info(&lcd->ld->dev, "%s: %s %s\n", __func__, dev_name(&client->adapter->dev), of_node_full_name(client->dev.of_node));

exit:
	return ret;
}

static struct i2c_device_id i2c_lcd_bias_id[] = {
	{"i2c_lcd_bias", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, i2c_lcd_bias_id);

static const struct of_device_id i2c_lcd_bias_dt_ids[] = {
	{ .compatible = "mediatek,i2c_lcd_bias" },
	{ }
};

MODULE_DEVICE_TABLE(of, i2c_lcd_bias_dt_ids);

static struct i2c_driver i2c_lcd_bias_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "i2c_lcd_bias",
		.of_match_table	= of_match_ptr(i2c_lcd_bias_dt_ids),
	},
	.id_table = i2c_lcd_bias_id,
	.probe = i2c_lcd_bias_probe,
};

static int nt36675_probe(struct lcd_info *lcd)
{
	dev_info(&lcd->ld->dev, "+ %s\n", __func__);

	lcd->bd->props.max_brightness = EXTEND_BRIGHTNESS;
	lcd->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;

	lcd->state = PANEL_STATE_RESUMED;
	lcd->enable = 1;
	lcd->lux = -1;

	mutex_lock(&lcd->lock);
	nt36675_read_init_info(lcd);
	smcdsd_panel_set_brightness(lcd, 1);
	mutex_unlock(&lcd->lock);

	lcd->fac_info = lcd->fac_done = IS_ENABLED(CONFIG_SEC_FACTORY) ? 1 : 0;

	i2c_lcd_bias_id[0].driver_data = (kernel_ulong_t)lcd;
	i2c_add_driver(&i2c_lcd_bias_driver);

	dev_info(&lcd->ld->dev, "- %s\n", __func__);

	return 0;
}

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%s_%02X%02X%02X\n", LCD_TYPE_VENDOR, lcd->id_info.id[0], lcd->id_info.id[1], lcd->id_info.id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%02x %02x %02x\n", lcd->id_info.id[0], lcd->id_info.id[1], lcd->id_info.id[2]);

	return strlen(buf);
}

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;
	char *pos = buf;

	for (i = 0; i <= EXTEND_BRIGHTNESS; i++)
		pos += sprintf(pos, "%3d %4d\n", i, brightness_table[i]);

	return pos - buf;
}

static ssize_t lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", lcd->lux);

	return strlen(buf);
}

static ssize_t lux_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (lcd->lux != value) {
		mutex_lock(&lcd->lock);
		lcd->lux = value;
		mutex_unlock(&lcd->lock);
	}

	return size;
}

static void panel_conn_uevent(struct lcd_info *lcd)
{
	char *uevent_conn_str[3] = {"CONNECTOR_NAME=UB_CONNECT", "CONNECTOR_TYPE=HIGH_LEVEL", NULL};

	if (!lcd->fac_info)
		return;

	if (!lcd->conn_det_enable)
		return;

	kobject_uevent_env(&lcd->ld->dev.kobj, KOBJ_CHANGE, uevent_conn_str);

	dev_info(&lcd->ld->dev, "%s: %s, %s\n", __func__, uevent_conn_str[0], uevent_conn_str[1]);
}

static void panel_conn_work(struct work_struct *work)
{
	struct lcd_info *lcd = container_of(work, struct lcd_info, conn_work);

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	panel_conn_uevent(lcd);
}

static irqreturn_t panel_conn_det_handler(int irq, void *dev_id)
{
	struct lcd_info *lcd = (struct lcd_info *)dev_id;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	queue_work(lcd->conn_workqueue, &lcd->conn_work);

	lcd->conn_det_count++;

	return IRQ_HANDLED;
}

static ssize_t conn_det_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int gpio_active = of_gpio_get_active("gpio_con");

	if (gpio_active < 0)
		sprintf(buf, "%d\n", -1);
	else
		sprintf(buf, "%s\n", gpio_active ? "disconnected" : "connected");

	dev_info(&lcd->ld->dev, "%s: %s\n", __func__, buf);

	return strlen(buf);
}

static ssize_t conn_det_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int value;
	int rc;
	int gpio_active = of_gpio_get_active("gpio_con");

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (gpio_active < 0)
		return -EINVAL;

	dev_info(&lcd->ld->dev, "%s: %u, %u\n", __func__, lcd->conn_det_enable, value);

	if (lcd->conn_det_enable != value) {
		mutex_lock(&lcd->lock);
		lcd->conn_det_enable = value;
		mutex_unlock(&lcd->lock);

		dev_info(&lcd->ld->dev, "%s: %s\n", __func__, gpio_active ? "disconnected" : "connected");
		if (lcd->conn_det_enable && gpio_active)
			panel_conn_uevent(lcd);
	}

	return size;
}

static void panel_conn_register(struct lcd_info *lcd)
{
	struct mipi_dsi_lcd_common *pdata = lcd->pdata;
	struct abd_protect *abd = &pdata->abd;
	int gpio = 0, gpio_active = 0;

	if (!pdata) {
		dev_info(&lcd->ld->dev, "%s: pdata is invalid\n", __func__);
		return;
	}

	if (!lcd->connected) {
		dev_info(&lcd->ld->dev, "%s: lcd connected: %d\n", __func__, lcd->connected);
		return;
	}

	gpio = of_get_gpio_with_name("gpio_con");
	if (gpio < 0) {
		dev_info(&lcd->ld->dev, "%s: gpio_con is %d\n", __func__, gpio);
		return;
	}

	gpio_active = of_gpio_get_active("gpio_con");
	if (gpio_active) {
		dev_info(&lcd->ld->dev, "%s: gpio_con_active is %d\n", __func__, gpio_active);
		return;
	}

	INIT_WORK(&lcd->conn_work, panel_conn_work);

	lcd->conn_workqueue = create_singlethread_workqueue("lcd_conn_workqueue");
	if (!lcd->conn_workqueue) {
		dev_info(&lcd->ld->dev, "%s: create_singlethread_workqueue fail\n", __func__);
		return;
	}

	smcdsd_abd_pin_register_handler(abd, gpio_to_irq(gpio), panel_conn_det_handler, lcd);

	if (lcd->fac_info)
		smcdsd_abd_con_register(abd);
}

static ssize_t fac_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", lcd->fac_info);

	return strlen(buf);
}

static const struct attribute_group lcd_sysfs_attr_group;

static ssize_t fac_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	struct mipi_dsi_lcd_common *pdata = lcd->pdata;
	struct abd_protect *abd = &pdata->abd;
	int value;
	int rc;

	rc = kstrtouint(buf, 0, &value);
	if (rc < 0)
		return rc;

	dev_info(&lcd->ld->dev, "%s: %u(%u)\n", __func__, lcd->fac_info, value);

	mutex_lock(&lcd->lock);
	lcd->fac_info = value;
	mutex_unlock(&lcd->lock);

	if (!lcd->fac_done) {
		lcd->fac_done = 1;
		smcdsd_abd_con_register(abd);
		sysfs_update_group(&lcd->ld->dev.kobj, &lcd_sysfs_attr_group);
	}

	return size;
}

static DEVICE_ATTR_RO(lcd_type);
static DEVICE_ATTR_RO(window_type);
static DEVICE_ATTR_RO(brightness_table);
static DEVICE_ATTR_RW(lux);
static DEVICE_ATTR_RW(conn_det);
static DEVICE_ATTR_RW(fac_info);

static struct attribute *lcd_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_brightness_table.attr,
	&dev_attr_lux.attr,
	&dev_attr_conn_det.attr,
	NULL,
};

static umode_t lcd_sysfs_is_visible(struct kobject *kobj,
	struct attribute *attr, int index)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct lcd_info *lcd = dev_get_drvdata(dev);
	umode_t mode = attr->mode;

	if (attr == &dev_attr_conn_det.attr) {
		if (!lcd->fac_info && !lcd->fac_done)
			mode = 0;
	}

	return mode;
}

static const struct attribute_group lcd_sysfs_attr_group = {
	.is_visible = lcd_sysfs_is_visible,
	.attrs = lcd_sysfs_attributes,
};

static void lcd_init_sysfs(struct lcd_info *lcd)
{
	int ret = 0;
	struct i2c_client *clients[] = {lcd->blic_client, NULL};

	ret = sysfs_create_group(&lcd->ld->dev.kobj, &lcd_sysfs_attr_group);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "sysfs_create_group fail\n");

	ret = sysfs_create_file(&lcd->bd->dev.kobj, &dev_attr_fac_info.attr);
	if (ret < 0)
		dev_info(&lcd->ld->dev, "sysfs_create_file fail\n");

	init_debugfs_backlight(lcd->bd, brightness_table, clients);

	init_debugfs_param("blic_init", &LM36274_INIT, U8_MAX, ARRAY_SIZE(LM36274_INIT), 3);
	init_debugfs_param("blic_exit", &LM36274_EXIT, U8_MAX, ARRAY_SIZE(LM36274_EXIT), 3);
}

static int smcdsd_panel_probe(struct platform_device *p)
{
	struct device *dev = &p->dev;
	int ret = 0;
	struct lcd_info *lcd;

	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		dbg_info("%s: failed to allocate for lcd\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	lcd->ld = lcd_device_register("panel", dev, lcd, NULL);
	if (IS_ERR(lcd->ld)) {
		dbg_info("%s: failed to register lcd device\n", __func__);
		ret = PTR_ERR(lcd->ld);
		goto exit;
	}

	lcd->bd = backlight_device_register("panel", dev, lcd, &panel_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		dbg_info("%s: failed to register backlight device\n", __func__);
		ret = PTR_ERR(lcd->bd);
		goto exit;
	}

	mutex_init(&lcd->lock);
	lcd->need_primary_lock = 1;

	lcd->pdata = get_lcd_common_info(p);

	set_lcd_info(p, lcd);

	nt36675_probe(lcd);

	nt36675_register_notifier(lcd);

	lcd_init_sysfs(lcd);

	dev_info(&lcd->ld->dev, "%s: %s: done\n", kbasename(__FILE__), __func__);
exit:
	return ret;
}

static int smcdsd_panel_init(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: state(%d)\n", __func__, lcd->state);

	if (lcd->state == PANEL_STATE_SUSPENED) {
		nt36675_init(lcd);

		lcd->state = PANEL_STATE_RESUMED;
	}

	dev_info(&lcd->ld->dev, "- %s: state(%d) connected(%d)\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_reset(struct platform_device *p, unsigned int on)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: on(%d)\n", __func__, on);

	if (on)
		run_list(&p->dev, "panel_reset_enable");
	else
		run_list(&p->dev, "panel_reset_disable");

	dev_info(&lcd->ld->dev, "- %s: state(%d) connected(%d)\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_power(struct platform_device *p, unsigned int on)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: on(%d)\n", __func__, on);

	if (on) {
		run_list(&p->dev, "panel_power_enable");

		if (get_regulator_use_count(NULL, "gpio_lcd_bl_en") >= 2)
			dev_info(&lcd->ld->dev, "%s: i2c init cmd skip(%d)\n", __func__,
				get_regulator_use_count(NULL, "gpio_lcd_bl_en"));
		else
			i2c_lcd_bias_array_write(lcd->blic_client, LM36274_INIT, ARRAY_SIZE(LM36274_INIT));
	} else {
		if (get_regulator_use_count(NULL, "gpio_lcd_bl_en") >= 2)
			dev_info(&lcd->ld->dev, "%s: i2c exit cmd skip(%d)\n", __func__,
				get_regulator_use_count(NULL, "gpio_lcd_bl_en"));
		else
			i2c_lcd_bias_array_write(lcd->blic_client, LM36274_EXIT, ARRAY_SIZE(LM36274_EXIT));

		run_list(&p->dev, "panel_power_disable");
	}

	dev_info(&lcd->ld->dev, "- %s: state(%d) connected(%d)\n", __func__, lcd->state, lcd->connected);

	return 0;
}

static int smcdsd_panel_exit(struct platform_device *p)
{
	struct lcd_info *lcd = get_lcd_info(p);

	dev_info(&lcd->ld->dev, "+ %s: state(%d)\n", __func__, lcd->state);

	if (lcd->state == PANEL_STATE_SUSPENED)
		goto exit;

	nt36675_exit(lcd);

	lcd->state = PANEL_STATE_SUSPENED;

	dev_info(&lcd->ld->dev, "- %s: state(%d) connected(%d)\n", __func__, lcd->state, lcd->connected);

exit:
	return 0;
}

static int smcdsd_panel_path_lock(struct platform_device *p, bool locking)
{
	struct lcd_info *lcd = get_lcd_info(p);

	if (locking) {
		mutex_lock(&lcd->lock);
		lcd->need_primary_lock = 0;
	} else {
		lcd->need_primary_lock = 1;
		mutex_unlock(&lcd->lock);
	}

	dbg_info("%s: path_lock(%d)\n", __func__, locking);

	return 0;
}

struct mipi_dsi_lcd_driver nt36675_mipi_lcd_driver = {
	.driver = {
		.name = "nt36675",
	},
	.probe		= smcdsd_panel_probe,
	.init		= smcdsd_panel_init,
	.exit		= smcdsd_panel_exit,
	.panel_reset	= smcdsd_panel_reset,
	.panel_power	= smcdsd_panel_power,
	.path_lock	= smcdsd_panel_path_lock,
};
__XX_ADD_LCD_DRIVER(nt36675_mipi_lcd_driver);

static int __init panel_conn_init(void)
{
	struct lcd_info *lcd = NULL;
	struct mipi_dsi_lcd_common *pdata = NULL;
	struct platform_device *pdev = NULL;

	pdev = of_find_device_by_path("/panel");
	if (!pdev) {
		dbg_info("%s: of_find_device_by_path fail\n", __func__);
		return 0;
	}

	pdata = platform_get_drvdata(pdev);
	if (!pdata) {
		dbg_info("%s: platform_get_drvdata fail\n", __func__);
		return 0;
	}

	if (!pdata->drv) {
		dbg_info("%s: lcd_driver invalid\n", __func__);
		return 0;
	}

	if (!pdata->drv->pdev) {
		dbg_info("%s: lcd_driver pdev invalid\n", __func__);
		return 0;
	}

	if (pdata->drv != this_driver)
		return 0;

	lcd = get_lcd_info(pdata->drv->pdev);
	if (!lcd) {
		dbg_info("get_lcd_info invalid\n");
		return 0;
	}

	panel_conn_register(lcd);

	dev_info(&lcd->ld->dev, "%s: %s: done\n", kbasename(__FILE__), __func__);

	return 0;
}
late_initcall_sync(panel_conn_init);

