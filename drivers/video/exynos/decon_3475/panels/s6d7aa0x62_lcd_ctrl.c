/*
 * drivers/video/decon_7580/panels/s6d7aa0x62_lcd_ctrl.c
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <video/mipi_display.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "../dsim.h"


#include <video/mipi_display.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/lcd.h>
#include <linux/backlight.h>

#include "../dsim.h"
#include "../decon.h"
#include "dsim_panel.h"

#include "s6d7aa0x62_param.h"

#define BRIGHTNESS_REG 0x51
#define LEVEL_IS_HBM(level)			(level >= 6)
#define S6D7AA0_ID 0x59b810

struct lcd_info {
	struct lcd_device *ld;
	struct backlight_device *bd;
	unsigned char id[3];

	int	temperature;
	unsigned int coordinate[2];
	unsigned int state;
	unsigned int auto_brightness;
	unsigned int br_index;
	unsigned int acl_enable;
	unsigned int acl_enable_force;
	unsigned int current_acl;
	unsigned int current_hbm;
	unsigned int current_acl_opr;
	unsigned int siop_enable;
	unsigned char dump_info[3];

	unsigned int *br_tbl;
	unsigned char **hbm_tbl;
	unsigned char **acl_cutoff_tbl;
	unsigned char **acl_opr_tbl;
	struct notifier_block	fb_notif_panel;

	struct mutex lock;
	struct dsim_device *dsim;
};


static int dsim_panel_set_brightness(struct lcd_info *lcd, int force)
{
	int ret = 0;
	unsigned char bl_reg[2] = {BRIGHTNESS_REG, };
	int bl = lcd->bd->props.brightness;

	if (lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s : panel is not active state..\n", __func__);
		goto set_br_exit;
	}

	/* Platfrom : DDI reg Matching*/
	/* 003 : 003 */
	/* 126 : 095 */
	/* 255 : 221 */

	if (bl > 126)
		bl = (bl - 126) * 126 / 129 + 95;
	else if (bl >= 3)
		bl = (bl - 3) * 92 / 123 + 3;
	else if (bl >= 1 )
		bl = 0x03;
	else
		bl = 0x00;

	if (LEVEL_IS_HBM(lcd->auto_brightness) && (lcd->bd->props.brightness == 255))
		bl = 0xFF;

	bl_reg[1] = bl;

	dev_info(&lcd->ld->dev, "%s: platform BL : %d panel BL reg : [%3d] %02x\n",
		__func__, lcd->bd->props.brightness, bl, bl_reg[1]);

	if (dsim_write_hl_data(lcd->dsim, bl_reg, ARRAY_SIZE(bl_reg)) < 0)
		dev_err(&lcd->ld->dev, "%s : failed to write brightness cmd.\n", __func__);

set_br_exit:
	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct lcd_info *lcd = bl_get_data(bd);

	if (brightness < UI_MIN_BRIGHTNESS || brightness > UI_MAX_BRIGHTNESS) {
		pr_alert("Brightness should be in the range of 0 ~ 255\n");
		ret = -EINVAL;
		goto exit_set;
	}

	ret = dsim_panel_set_brightness(lcd, 0);
	if (ret) {
		dev_err(&lcd->ld->dev, "%s : failed to set brightness\n", __func__);
		goto exit_set;
	}
exit_set:
	return ret;

}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
};


static int s6d7aa0x62_read_init_info(struct lcd_info *lcd)
{
	int i = 0;
	struct panel_private *priv = &lcd->dsim->priv;

	dev_info(&lcd->ld->dev, "MDD : %s was called\n", __func__);

	if (lcdtype == 0) {
		priv->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	lcd->id[0] = (lcdtype & 0xFF0000) >> 16;
	lcd->id[1] = (lcdtype & 0x00FF00) >> 8;
	lcd->id[2] = (lcdtype & 0x0000FF) >> 0;

	dev_info(&lcd->ld->dev, "READ ID : ");
	for (i = 0; i < S6D7AA0X62_ID_LEN; i++)
		dev_info(&lcd->ld->dev, "%02x, ", lcd->id[i]);
	dev_info(&lcd->ld->dev, "\n");

read_exit:
	return 0;
}

static int s6d7aa0x62_displayon(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(lcd->dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_DISPLAY_ON\n", __func__);
		goto displayon_err;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_BR_DEF, ARRAY_SIZE(SEQ_BR_DEF));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_BR_OPEN\n", __func__);
		goto displayon_err;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_BR_OPEN, ARRAY_SIZE(SEQ_BR_OPEN));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_BR_OPEN\n", __func__);
		goto displayon_err;
	}

displayon_err:
	return ret;

}

static int s6d7aa0x62_exit(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(lcd->dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_DISPLAY_OFF\n", __func__);
		goto exit_err;
	}
	msleep(50);

	ret = dsim_write_hl_data(lcd->dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_SLEEP_IN\n", __func__);
		goto exit_err;
	}
	msleep(150);

exit_err:
	return ret;
}

static int s6d7aa0x62_init(struct lcd_info *lcd)
{
	int ret;

	dev_info(&lcd->ld->dev, "MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_TEST_KEY_ON_F1\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_1, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_1));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_1\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_2, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_2));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_2\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_3, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_3));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_3\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_4, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_4));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_4\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_5, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_5));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_5\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_6, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_6));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_6\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_7, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_7));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_7\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_8, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_8));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_8\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_9, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_9));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_9\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_10, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_10));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_10\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_11, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_11));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_11\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_12, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_12));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_12\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_13, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_13));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_13\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_14, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_14));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_14\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_15, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_15));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_15\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_16, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_16));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_16\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_17, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_17));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_17\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_18, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_18));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_18\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_19, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_19));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_19\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_20, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_20));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_20\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_21, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_21));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_21\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_22, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_22));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_22\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S6D7AA0X62_INIT_23, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_23));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S6D7AA0X62_INIT_23\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_GAMMA_FA, ARRAY_SIZE(SEQ_GAMMA_FA));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_GAMMA_FA\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_GAMMA_FB, ARRAY_SIZE(SEQ_GAMMA_FB));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_GAMMA_FB\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_OFF_F1, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F1));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_TEST_KEY_OFF_F1\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}

	msleep(130);
init_exit:
	return ret;
}

static int s6d7aa0x62_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *priv = &dsim->priv;
	struct lcd_info *lcd = dsim->priv.par;

	dev_info(&lcd->ld->dev, "MDD : %s was called\n", __func__);

	priv->lcdConnected = PANEL_CONNECTED;
	lcd->bd->props.max_brightness = UI_MAX_BRIGHTNESS;
	lcd->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;
	lcd->dsim = dsim;
	lcd->state = PANEL_STATE_RESUMED;
	lcd->temperature = NORMAL_TEMPERATURE;
	lcd->acl_enable = 0;
	lcd->current_acl = 0;
	lcd->auto_brightness = 0;
	lcd->siop_enable = 0;
	lcd->current_hbm = 0;

	s6d7aa0x62_read_init_info(lcd);
	if (priv->lcdConnected == PANEL_DISCONNEDTED) {
		dev_err(&lcd->ld->dev, "dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}

probe_exit:
	return ret;
}


static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "BOE_%02X%02X%02X\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%x %x %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	return strlen(buf);
}

static ssize_t auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->auto_brightness);

	return strlen(buf);
}

static ssize_t auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->auto_brightness != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->auto_brightness, value);
			mutex_lock(&lcd->lock);
			lcd->auto_brightness = value;
			mutex_unlock(&lcd->lock);
			dsim_panel_set_brightness(lcd, 0);
		}
	}
	return size;
}

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->siop_enable);

	return strlen(buf);
}

static ssize_t siop_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->siop_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->siop_enable, value);
			mutex_lock(&lcd->lock);
			lcd->siop_enable = value;
			mutex_unlock(&lcd->lock);

			dsim_panel_set_brightness(lcd, 1);
		}
	}
	return size;
}

static ssize_t power_reduce_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->acl_enable);

	return strlen(buf);
}

static ssize_t power_reduce_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);

	if (rc < 0)
		return rc;
	else {
		if (lcd->acl_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->acl_enable, value);
			mutex_lock(&lcd->lock);
			lcd->acl_enable = value;
			mutex_unlock(&lcd->lock);
			dsim_panel_set_brightness(lcd, 1);
		}
	}
	return size;
}

static int dsim_read_hl_data_offset(struct lcd_info *lcd, u8 addr, u32 size, u8 *buf, u32 offset)
{
	unsigned char wbuf[] = {0xB0, 0};
	int ret = 0;

	wbuf[1] = offset;

	ret = dsim_write_hl_data(lcd->dsim, wbuf, ARRAY_SIZE(wbuf));
	if (ret < 0)
		dev_err(&lcd->ld->dev, "%s: failed to write wbuf\n", __func__);

	ret = dsim_read_hl_data(lcd->dsim, addr, size, buf);
	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s: fail\n", __func__);

	return ret;
}

static ssize_t dump_register_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char *pos = buf;
	u8 reg, len, offset;
	int ret, i;
	u8 *dump = NULL;

	reg = lcd->dump_info[0];
	len = lcd->dump_info[1];
	offset = lcd->dump_info[2];

	if (!reg || !len || reg > 0xff || len > 255 || offset > 255)
		goto exit;

	dump = kcalloc(len, sizeof(u8), GFP_KERNEL);

	if (lcd->state == PANEL_STATE_RESUMED) {
		if (dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
			dev_err(&lcd->ld->dev, "failed to write test on f0 command.\n");
		if (dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
			dev_err(&lcd->ld->dev, "failed to write test on fc command.\n");

		if (offset)
			ret = dsim_read_hl_data_offset(lcd, reg, len, dump, offset);
		else
			ret = dsim_read_hl_data(lcd->dsim, reg, len, dump);

		if (dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
			dev_err(&lcd->ld->dev, "failed to write test off fc command.\n");
		if (dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
			dev_err(&lcd->ld->dev, "failed to write test off f0 command.\n");
	}

	pos += sprintf(pos, "+ [%02X]\n", reg);
	for (i = 0; i < len; i++)
		pos += sprintf(pos, "%2d: %02x\n", i + offset + 1, dump[i]);
	pos += sprintf(pos, "- [%02X]\n", reg);

	dev_info(&lcd->ld->dev, "+ [%02X]\n", reg);
	for (i = 0; i < len; i++)
		dev_info(&lcd->ld->dev, "%2d: %02x\n", i + offset + 1, dump[i]);
	dev_info(&lcd->ld->dev, "- [%02X]\n", reg);

	kfree(dump);
exit:
	return pos - buf;
}

static ssize_t dump_register_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int reg, len, offset;
	int ret;

	ret = sscanf(buf, "%x %d %d", &reg, &len, &offset);

	if (ret == 2)
		offset = 0;

	dev_info(dev, "%s: %x %d %d\n", __func__, reg, len, offset);

	if (ret < 0)
		return ret;
	else {
		if (!reg || !len || reg > 0xff || len > 255 || offset > 255)
			return -EINVAL;

		lcd->dump_info[0] = reg;
		lcd->dump_info[1] = len;
		lcd->dump_info[2] = offset;
	}

	return size;
}


#ifdef CONFIG_DECON_MIPI_DSI_CLK_SWITCH
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct dsim_device *dsim;
	struct lcd_info *lcd;
	int fb_blank;

	switch (event) {
	case FB_EARLY_EVENT_BLANK:
		break;
	default:
		return 0;
	}

	lcd = container_of(self, struct lcd_info, fb_notif_panel);
	dsim = lcd->dsim;

	fb_blank = *(int *)evdata->data;

	if (evdata->info->node != 0)
		return 0;

	if (fb_blank == FB_BLANK_UNBLANK) {
		mutex_lock(&lcd->lock);
		dsim->clks_param.clks.hs_clk = dsim->hs_clk_list[dsim->hs_clk_mode];
		dsim->clks_param.clks.esc_clk = dsim->lcd_info.esc_clk;
		dev_info(&lcd->ld->dev, "%s: %d, %d\n", __func__, fb_blank, dsim->hs_clk_mode);
		mutex_unlock(&lcd->lock);
	}

	return 0;
}

static ssize_t mipi_switch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;

	sprintf(buf, "%d\n", dsim->hs_clk_mode);

	return strlen(buf);
}

static ssize_t mipi_switch_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	int mode_index, sim_index;
	int ret;


	ret = sscanf(buf, "%d %d", &mode_index, &sim_index);

	if (ret < 0)
		return ret;
	else {
		if ((mode_index < 0 || mode_index >= dsim->hs_clk_size)
			|| (sim_index < 0 || sim_index >= 2)) {
			dev_info(&lcd->ld->dev, "%s : out of range input[%d, %d] range[0 ~ %d, 0 ~ 1]\n",
				__func__, mode_index, sim_index, dsim->hs_clk_size - 1);
			return -EINVAL;
		}

		mutex_lock(&lcd->lock);

		dsim->hs_clk_mode = mode_index;

		if ((sim_index) && (lcd->state != DSIM_STATE_SUSPEND)) {
			dsim->clks_param.clks.hs_clk = dsim->hs_clk_list[dsim->hs_clk_mode];
			dsim->clks_param.clks.esc_clk = dsim->lcd_info.esc_clk;

			dsim_reg_set_clocks(dsim->id, &dsim->clks_param.clks, DSIM_LANE_CLOCK | dsim->data_lane, 1);

			dev_info(&lcd->ld->dev, "%s : hs clk is set to %d [index : %d]\n", __func__,
				dsim->hs_clk_list[dsim->hs_clk_mode], dsim->hs_clk_mode);
		} else {
			dev_info(&lcd->ld->dev, "%s : hs clk will be set to %d [index : %d]\n", __func__,
				dsim->hs_clk_list[dsim->hs_clk_mode], dsim->hs_clk_mode);
		}

		mutex_unlock(&lcd->lock);
	}
	return size;
}
#endif

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(dump_register, 0664, dump_register_show, dump_register_store);
#ifdef CONFIG_DECON_MIPI_DSI_CLK_SWITCH
static DEVICE_ATTR(mipi_switch, 0664, mipi_switch_show, mipi_switch_store);
#endif

static struct attribute *lcd_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_siop_enable.attr,
	&dev_attr_power_reduce.attr,
	&dev_attr_dump_register.attr,
	NULL,
};

static struct attribute *backlight_sysfs_attributes[] = {
	&dev_attr_auto_brightness.attr,
	NULL,
};

static const struct attribute_group lcd_sysfs_attr_group = {
	.attrs = lcd_sysfs_attributes,
};

static const struct attribute_group backlight_sysfs_attr_group = {
	.attrs = backlight_sysfs_attributes,
};

static void lcd_init_sysfs(struct lcd_info *lcd)
{
	int ret = 0;

	ret = sysfs_create_group(&lcd->ld->dev.kobj, &lcd_sysfs_attr_group);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add lcd sysfs\n");

	ret = sysfs_create_group(&lcd->bd->dev.kobj, &backlight_sysfs_attr_group);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add backlight sysfs\n");
}


static int dsim_panel_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd;
#ifdef CONFIG_DECON_MIPI_DSI_CLK_SWITCH
	struct device_node *node = NULL;
	int i = 0;
	int clk_cnt_property = 0;
	struct property *prop;
#endif

	dsim->priv.par = lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("%s : failed to allocate for lcd\n", __func__);
		ret = -ENOMEM;
		goto probe_err;
	}

	dsim->lcd = lcd->ld = lcd_device_register("panel", dsim->dev, lcd, NULL);
	if (IS_ERR(lcd->ld)) {
		pr_err("%s : failed to register lcd device\n", __func__);
		ret = PTR_ERR(lcd->ld);
		goto probe_err;
	}

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &panel_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("%s : failed to register backlight device\n", __func__);
		ret = PTR_ERR(lcd->bd);
		goto probe_err;
	}

	mutex_init(&lcd->lock);

	ret = s6d7aa0x62_probe(dsim);
	if (ret) {
		dev_err(&lcd->ld->dev, "%s : failed to probe panel\n", __func__);
		goto probe_err;
	}

#if defined(CONFIG_EXYNOS_DECON_LCD_SYSFS)
	lcd_init_sysfs(lcd);
#endif
#ifdef CONFIG_DECON_MIPI_DSI_CLK_SWITCH
	node = of_parse_phandle(dsim->dev->of_node, "lcd_info", 1);
	/* node = of_find_node_by_path("node name"); */
	prop = of_find_property(node, "timing,dsi-hs-clk", &clk_cnt_property);
	if (!prop)
		goto probe_err;

	dsim->hs_clk_size = clk_cnt_property / sizeof(u32);
	dsim->hs_clk_size = (dsim->hs_clk_size > ARRAY_SIZE(dsim->hs_clk_list)) ? ARRAY_SIZE(dsim->hs_clk_list) : dsim->hs_clk_size;

	if (dsim->hs_clk_size <= 1)
		goto probe_err;

	of_property_read_u32_array(node, "timing,dsi-hs-clk", dsim->hs_clk_list, dsim->hs_clk_size);

	for (i = 0; i < dsim->hs_clk_size; i++)
		dev_info(&lcd->ld->dev, "	dsim HS-clk [%2d] : %d\n", i, dsim->hs_clk_list[i]);

	lcd->fb_notif_panel.notifier_call = fb_notifier_callback;
	fb_register_client(&lcd->fb_notif_panel);

	ret = device_create_file(&lcd->ld->dev, &dev_attr_mipi_switch);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__);

#endif

	dev_info(&lcd->ld->dev, "%s: %s: done\n", kbasename(__FILE__), __func__);
probe_err:
	return ret;
}

static int dsim_panel_displayon(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;
	int ret = 0;

	if (lcd->state == PANEL_STATE_SUSPENED) {
		ret = s6d7aa0x62_init(lcd);
		if (ret) {
			dev_err(&lcd->ld->dev, "%s : failed to panel init\n", __func__);
			lcd->state = PANEL_STATE_SUSPENED;
			goto displayon_err;
		}
	}

	ret = s6d7aa0x62_displayon(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "%s : failed to panel display on\n", __func__);
		goto displayon_err;
	}

	lcd->state = PANEL_STATE_RESUMED;
	dsim_panel_set_brightness(lcd, 1);

displayon_err:
	return ret;
}

static int dsim_panel_suspend(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;
	int ret = 0;

	mutex_lock(&lcd->lock);

	if (lcd->state == PANEL_STATE_SUSPENED)
		goto suspend_err;

	lcd->state = PANEL_STATE_SUSPENDING;

	ret = s6d7aa0x62_exit(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "%s : failed to panel exit\n", __func__);
		goto suspend_err;
	}

	lcd->state = PANEL_STATE_SUSPENED;

suspend_err:
	mutex_unlock(&lcd->lock);

	return ret;
}

struct mipi_dsim_lcd_driver s6d7aa0x62_mipi_lcd_driver = {
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
};

