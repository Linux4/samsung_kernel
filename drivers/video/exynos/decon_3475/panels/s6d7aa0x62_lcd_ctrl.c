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


static int dsim_panel_set_brightness(struct dsim_device *dsim, int force)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char bl_reg[2] = {BRIGHTNESS_REG, };
	int bl = panel->bd->props.brightness;

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		goto set_br_exit;
	}

	/* Platfrom : DDI reg Matching*/
	/* 003 : 003 */
	/* 126 : 095 */
	/* 255 : 221 */

	if (bl > 126)
		bl = (bl - 126) * 126 / 129 + 95;
	else if ( bl >= 3)
		bl = (bl - 3) * 92 / 123 + 3;
	else
		bl = 0x00;

	if (LEVEL_IS_HBM(panel->auto_brightness) && (panel->bd->props.brightness == 255))
		bl = 0xFF;

	bl_reg[1] = bl;

	dsim_info("%s: platform BL : %d panel BL reg : [%3d] %02x\n",
		__func__, panel->bd->props.brightness, bl, bl_reg[1]);

	if (dsim_write_hl_data(dsim, bl_reg, ARRAY_SIZE(bl_reg)) < 0)
		dsim_err("%s : fail to write brightness cmd.\n", __func__);

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
	struct panel_private *priv = bl_get_data(bd);
	struct dsim_device *dsim;

	dsim = container_of(priv, struct dsim_device, priv);

	if (brightness < UI_MIN_BRIGHTNESS || brightness > UI_MAX_BRIGHTNESS) {
		pr_alert("Brightness should be in the range of 0 ~ 255\n");
		ret = -EINVAL;
		goto exit_set;
	}

	ret = dsim_panel_set_brightness(dsim, 0);
	if (ret) {
		dsim_err("%s : fail to set brightness\n", __func__);
		goto exit_set;
	}
exit_set:
	return ret;

}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
};

static int dsim_backlight_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	panel->bd = backlight_device_register("panel", dsim->dev, &dsim->priv,
					&panel_backlight_ops, NULL);
	if (IS_ERR(panel->bd)) {
		dsim_err("%s:failed register backlight\n", __func__);
		ret = PTR_ERR(panel->bd);
	}

	panel->bd->props.max_brightness = UI_MAX_BRIGHTNESS;
	panel->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;
	panel->bd->props.state = 0;

	return ret;
}


static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "BOE_%02X%02X%02X\n", priv->id[0], priv->id[1], priv->id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%x %x %x\n", priv->id[0], priv->id[1], priv->id[2]);

	return strlen(buf);
}

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *panel = dev_get_drvdata(dev);
	char *pos = buf;
	int nit, i;

	if (!panel || IS_ERR_OR_NULL(panel->br_tbl))
		return strlen(buf);

	for (i = 0; i <= UI_MAX_BRIGHTNESS; i++) {
		nit = panel->br_tbl[i];
		pos += sprintf(pos, "%3d %3d\n", i, nit);
	}
	return pos - buf;
}

static ssize_t auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", priv->auto_brightness);

	return strlen(buf);
}

static ssize_t auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (priv->auto_brightness != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, priv->auto_brightness, value);
			mutex_lock(&priv->lock);
			priv->auto_brightness = value;
			mutex_unlock(&priv->lock);
			dsim_panel_set_brightness(dsim, 0);
		}
	}
	return size;
}

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", priv->siop_enable);

	return strlen(buf);
}

static ssize_t siop_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (priv->siop_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, priv->siop_enable, value);
			mutex_lock(&priv->lock);
			priv->siop_enable = value;
			mutex_unlock(&priv->lock);

			dsim_panel_set_brightness(dsim, 1);
		}
	}
	return size;
}

static ssize_t power_reduce_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", priv->acl_enable);

	return strlen(buf);
}
static ssize_t power_reduce_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);
	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);

	if (rc < 0)
		return rc;
	else {
		if (priv->acl_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, priv->acl_enable, value);
			mutex_lock(&priv->lock);
			priv->acl_enable = value;
			mutex_unlock(&priv->lock);
			dsim_panel_set_brightness(dsim, 1);
		}
	}
	return size;
}

static ssize_t dump_register_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	char *pos = buf;
	u8 reg, len;
	int ret, i;
	u8 *dump = NULL;

	dsim = container_of(priv, struct dsim_device, priv);

	reg = priv->dump_info[0];
	len = priv->dump_info[1];

	if (!reg || !len || reg > 0xff || len > 0xff)
		goto exit;

	dump = kzalloc(len * sizeof(u8), GFP_KERNEL);

	if (priv->state == PANEL_STATE_RESUMED) {
		if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
			dsim_err("fail to write test on f0 command.\n");
		if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
			dsim_err("fail to write test on fc command.\n");

		ret = dsim_read_hl_data(dsim, reg, len, dump);

		if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
			dsim_err("fail to write test off fc command.\n");
		if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
			dsim_err("fail to write test off f0 command.\n");
	}

	pos += sprintf(pos, "+ [%02X]\n", reg);
	for (i = 0; i < len; i++)
		pos += sprintf(pos, "%2d: %02x\n", i + 1, dump[i]);
	pos += sprintf(pos, "- [%02X]\n", reg);

	kfree(dump);
exit:
	return pos - buf;
}

static ssize_t dump_register_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	unsigned int reg, len;
	int ret;

	ret = sscanf(buf, "%x %d", &reg, &len);

	dev_info(dev, "%s: %x %d\n", __func__, reg, len);

	if (ret < 0)
		return ret;
	else {
		if (!reg || !len || reg > 0xff || len > 0xff)
			return -EINVAL;

		len = (len < 3) ? 3 : len;
		len = (len > 0xff) ? 3 : len;

		priv->dump_info[0] = reg;
		priv->dump_info[1] = len;
	}

	return size;
}

#ifdef CONFIG_DECON_MIPI_DSI_CLK_SWITCH
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct dsim_device *dsim = NULL;
	struct panel_private *priv = NULL;
	int fb_blank;

	switch (event) {
	case FB_EARLY_EVENT_BLANK:
		break;
	default:
		return 0;
	}

	dsim = container_of(self, struct dsim_device, fb_notif_panel);
	priv = &dsim->priv;

	fb_blank = *(int *)evdata->data;

	if (evdata->info->node != 0)
		return 0;

	if (fb_blank == FB_BLANK_UNBLANK) {
		mutex_lock(&priv->lock);
		dsim->clks_param.clks.hs_clk = dsim->hs_clk_list[dsim->hs_clk_mode];
		dsim->clks_param.clks.esc_clk = dsim->lcd_info.esc_clk;
		dsim_info("%s: %d, %d\n", __func__, fb_blank, dsim->hs_clk_mode);
		mutex_unlock(&priv->lock);
	}

	return 0;
}

static ssize_t mipi_switch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	dsim = container_of(priv, struct dsim_device, priv);

	sprintf(buf, "%d\n", dsim->hs_clk_mode);

	return strlen(buf);
}

static ssize_t mipi_switch_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int mode_index, sim_index;
	int ret;

	dsim = container_of(priv, struct dsim_device, priv);

	ret = sscanf(buf, "%d %d", &mode_index, &sim_index);

	if (ret < 0)
		return ret;
	else {
		if ((mode_index < 0 || mode_index >= dsim->hs_clk_size)
			|| (sim_index < 0 || sim_index >= 2)) {
			dsim_info("%s : out of range input[%d, %d] range[0 ~ %d, 0 ~ 1]\n",
				__func__, mode_index, sim_index, dsim->hs_clk_size - 1);
			return -EINVAL;
		}

		mutex_lock(&priv->lock);

		dsim->hs_clk_mode = mode_index;

		if ((sim_index) && (dsim->state != DSIM_STATE_SUSPEND)) {
			dsim->clks_param.clks.hs_clk = dsim->hs_clk_list[dsim->hs_clk_mode];
			dsim->clks_param.clks.esc_clk = dsim->lcd_info.esc_clk;

			dsim_reg_set_clocks(dsim->id, &dsim->clks_param.clks, DSIM_LANE_CLOCK | dsim->data_lane, 1);

			dsim_info("%s : hs clk is set to %d [index : %d]\n", __func__,
				dsim->hs_clk_list[dsim->hs_clk_mode], dsim->hs_clk_mode);
		} else {
			dsim_info("%s : hs clk will be set to %d [index : %d]\n", __func__,
				dsim->hs_clk_list[dsim->hs_clk_mode], dsim->hs_clk_mode);
		}

		mutex_unlock(&priv->lock);
	}
	return size;
}
#endif

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(dump_register, 0664, dump_register_show, dump_register_store);
#ifdef CONFIG_DECON_MIPI_DSI_CLK_SWITCH
static DEVICE_ATTR(mipi_switch, 0664, mipi_switch_show, mipi_switch_store);
#endif
static void lcd_init_sysfs(struct dsim_device *dsim)
{
	int ret = -1;

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_window_type);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_brightness_table);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->priv.bd->dev, &dev_attr_auto_brightness);
	if (ret < 0)
		dev_err(&dsim->priv.bd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_siop_enable);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_power_reduce);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_dump_register);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
}

static int s6d7aa0x62_read_init_info(struct dsim_device *dsim)
{
	int i = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("MDD : %s was called\n", __func__);

	if (lcdtype == 0) {
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	panel->id[0] = (lcdtype & 0xFF0000) >> 16;
	panel->id[1] = (lcdtype & 0x00FF00) >> 8;
	panel->id[2] = (lcdtype & 0x0000FF) >> 0;

	dsim_info("READ ID : ");
	for (i = 0; i < S6D7AA0X62_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

read_exit:
	return 0;
}

static int s6d7aa0x62_displayon(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_DISPLAY_ON\n", __func__);
		goto displayon_err;
	}

	ret = dsim_write_hl_data(dsim, SEQ_BR_OPEN, ARRAY_SIZE(SEQ_BR_OPEN));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_BR_OPEN\n", __func__);
		goto displayon_err;
	}

displayon_err:
	return ret;

}

static int s6d7aa0x62_exit(struct dsim_device *dsim)
{
	int ret = 0;
	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_DISPLAY_OFF\n", __func__);
		goto exit_err;
	}
	msleep(50);

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_IN\n", __func__);
		goto exit_err;
	}
	msleep(150);

exit_err:
	return ret;
}

static int s6d7aa0x62_init(struct dsim_device *dsim)
{
	int ret;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F1\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_1, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_1\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_2, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_2));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_2\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_3, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_3));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_1\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_4, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_4));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_4\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_5, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_5));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_5\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_6, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_6));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_6\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_7, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_7));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_7\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_8, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_8));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_8\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_9, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_9));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_9\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_10, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_10));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_10\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_11, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_11));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_11\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_12, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_12));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_12\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_13, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_13));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_13\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_14, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_14));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_14\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_15, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_15));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_15\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_16, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_16));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_16\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_17, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_17));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_17\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_17, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_17));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_17\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_18, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_18));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_18\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_19, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_19));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_19\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_20, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_20));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_20\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_21, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_21));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_21\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_22, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_22));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_22\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_S6D7AA0X62_INIT_23, ARRAY_SIZE(SEQ_S6D7AA0X62_INIT_23));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S6D7AA0X62_INIT_23\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_FA, ARRAY_SIZE(SEQ_GAMMA_FA));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_GAMMA_FA\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_FB, ARRAY_SIZE(SEQ_GAMMA_FB));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_GAMMA_FB\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F1, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F1\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}

	msleep(130);
init_exit:
	return ret;
}

static int s6d7aa0x62_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("MDD : %s was called\n", __func__);

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;


	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;

	}

	s6d7aa0x62_read_init_info(dsim);

	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}

probe_exit:
	return ret;
}

struct dsim_panel_ops s6d7aa0x62_panel_ops = {
	.early_probe = NULL,
	.probe		= s6d7aa0x62_probe,
	.displayon	= s6d7aa0x62_displayon,
	.exit		= s6d7aa0x62_exit,
	.init		= s6d7aa0x62_init,
};


static int dsim_panel_early_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	panel->ops = dsim_panel_get_priv_ops(dsim);

	if (panel->ops->early_probe)
		ret = panel->ops->early_probe(dsim);

	return ret;
}

static int dsim_panel_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
#ifdef CONFIG_DECON_MIPI_DSI_CLK_SWITCH
	struct device_node *node = NULL;
	int i = 0;
	int clk_cnt_property = 0;
	struct property *prop;
#endif

	dsim->lcd = lcd_device_register("panel", dsim->dev, &dsim->priv, NULL);
	if (IS_ERR(dsim->lcd)) {
		dsim_err("%s : faield to register lcd device\n", __func__);
		ret = PTR_ERR(dsim->lcd);
		goto probe_err;
	}
	ret = dsim_backlight_probe(dsim);
	if (ret) {
		dsim_err("%s : failed to prbe backlight driver\n", __func__);
		goto probe_err;
	}

	panel->lcdConnected = PANEL_CONNECTED;
	panel->state = PANEL_STATE_RESUMED;
	panel->temperature = NORMAL_TEMPERATURE;
	panel->acl_enable = 0;
	panel->current_acl = 0;
	panel->auto_brightness = 0;
	panel->siop_enable = 0;
	panel->current_hbm = 0;
	panel->current_vint = 0;
	mutex_init(&panel->lock);

	if (panel->ops->probe) {
		ret = panel->ops->probe(dsim);
		if (ret) {
			dsim_err("%s : failed to probe panel\n", __func__);
			goto probe_err;
		}
	}

#if defined(CONFIG_EXYNOS_DECON_LCD_SYSFS)
	lcd_init_sysfs(dsim);
#endif
#ifdef CONFIG_DECON_MIPI_DSI_CLK_SWITCH
	node = of_parse_phandle(dsim->dev->of_node, "lcd_info", 0);
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
		dsim_info("	dsim HS-clk [%2d] : %d\n", i, dsim->hs_clk_list[i]);

	dsim->fb_notif_panel.notifier_call = fb_notifier_callback;
	fb_register_client(&dsim->fb_notif_panel);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_mipi_switch);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

#endif

probe_err:
	return ret;
}

static int dsim_panel_displayon(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if (panel->state == PANEL_STATE_SUSPENED) {
		if (panel->ops->init) {
			ret = panel->ops->init(dsim);
			if (ret) {
				dsim_err("%s : failed to panel init\n", __func__);
				panel->state = PANEL_STATE_SUSPENED;
				goto displayon_err;
			}
		}
	}

	if (panel->ops->displayon) {
		ret = panel->ops->displayon(dsim);
		if (ret) {
			dsim_err("%s : failed to panel display on\n", __func__);
			goto displayon_err;
		}
	}
	panel->state = PANEL_STATE_RESUMED;
	dsim_panel_set_brightness(dsim, 1);

displayon_err:
	return ret;
}

static int dsim_panel_suspend(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	mutex_lock(&panel->lock);

	if (panel->state == PANEL_STATE_SUSPENED)
		goto suspend_err;

	panel->state = PANEL_STATE_SUSPENDING;

	if (panel->ops->exit) {
		ret = panel->ops->exit(dsim);
		if (ret) {
			dsim_err("%s : failed to panel exit\n", __func__);
			goto suspend_err;
		}
	}
	panel->state = PANEL_STATE_SUSPENED;

suspend_err:
	mutex_unlock(&panel->lock);

	return ret;
}

static int dsim_panel_resume(struct dsim_device *dsim)
{
	int ret = 0;
	return ret;
}

struct mipi_dsim_lcd_driver s6d7aa0x62_mipi_lcd_driver = {
	.early_probe = dsim_panel_early_probe,
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
	.resume		= dsim_panel_resume,
};

