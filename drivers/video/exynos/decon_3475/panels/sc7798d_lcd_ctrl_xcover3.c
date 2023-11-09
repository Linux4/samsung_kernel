/*
 * drivers/video/decon_3475/panels/sc7798d_lcd_ctrl.c
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <video/mipi_display.h>
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

#include "sc7798d_param_xcover3.h"

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#include "mdnie.h"
#include "mdnie_lite_table_xcover3.h"
#endif

#define BRIGHTNESS_REG			0x51
#define LEVEL_IS_HBM(level)		(level >= 6)


static int dsim_panel_set_cabc(struct dsim_device *dsim, int force)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	int bl = 0;

	if (panel == NULL) {
		dsim_err("%s : panel is NULL\n", __func__);
		goto exit;
	}

	bl = panel->bd->props.brightness;

	if ((panel->auto_brightness > 0 && panel->auto_brightness < 6) && (bl != 0xFF))
		panel->acl_enable = 1;
	else
		panel->acl_enable = 0;

	if (panel->acl_enable_force != ACL_TEST_DO_NOT_WORK)
		panel->acl_enable = panel->acl_enable_force - 1;

	if ((panel->acl_enable == panel->current_acl) && !force)
		return 0;

	if (panel->acl_enable) { /* cabc on */
		if (dsim_write_hl_data(dsim, SEQ_INIT1_1, ARRAY_SIZE(SEQ_INIT1_1)) < 0)
			dsim_err("%s : failed to write SEQ_INIT1_1\n", __func__);
		if (dsim_write_hl_data(dsim, SEQ_CABC_ON, ARRAY_SIZE(SEQ_CABC_ON)) < 0)
			dsim_err("%s : failed to write SEQ_CD_ON\n", __func__);

	} else { /* cabc off */
		if (dsim_write_hl_data(dsim, SEQ_INIT1_1, ARRAY_SIZE(SEQ_INIT1_1)) < 0)
			dsim_err("%s : failed to write SEQ_INIT1_1\n", __func__);

		if (dsim_write_hl_data(dsim, SEQ_CABC_OFF, ARRAY_SIZE(SEQ_CABC_OFF)) < 0)
			dsim_err("%s : failed to write SEQ_CD_ON\n", __func__);
	}

	panel->current_acl = panel->acl_enable;
	dsim_info("cabc: %d, auto_brightness: %d\n", dsim->priv.current_acl, dsim->priv.auto_brightness);

exit:
	if (!ret)
		ret = -EPERM;
	return ret;
}

static int dsim_panel_set_brightness(struct dsim_device *dsim, int force)
{
	int ret = 0;

	struct panel_private *panel = &dsim->priv;
	unsigned char bl_reg[2];
	int bl = panel->bd->props.brightness;

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		goto set_br_exit;
	}

	bl_reg[0] = BRIGHTNESS_REG;
	if (bl >= 8)
		bl_reg[1] = (bl - 3) * 207 / 252 + 3;
	else if (bl > 0)
		bl_reg[1] = 0x06;
	else
		bl_reg[1] = 0x00;

	if (LEVEL_IS_HBM(panel->auto_brightness) && (panel->bd->props.brightness == 255))
		bl_reg[1] = 0xF9;

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		return ret;
	}

	dsim_info("%s: platform BL : %d panel BL reg : %d\n", __func__, panel->bd->props.brightness, bl_reg[1]);

	if (dsim_write_hl_data(dsim, bl_reg, ARRAY_SIZE(bl_reg)) < 0)
		dsim_err("%s : fail to write brightness cmd.\n", __func__);

	dsim_panel_set_cabc(dsim, force);

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


static int sc7798d_read_init_info(struct dsim_device *dsim)
{
	int i = 0;
	int ret;
	struct panel_private *panel = &dsim->priv;

	dsim_info("MDD : %s was called\n", __func__);

	/* id */
	ret = dsim_read_hl_data(dsim, SC7798D_ID_REG, SC7798D_ID_LEN, dsim->priv.id);

	if (ret != SC7798D_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < SC7798D_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

read_exit:
	return 0;
}

static int sc7798d_displayon(struct dsim_device *dsim)
{
	return 0;
}

static int sc7798d_displayon_late(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
		goto displayon_err;
	}

	msleep(12);

	ret = dsim_write_hl_data(dsim, SEQ_INIT4_4, ARRAY_SIZE(SEQ_INIT4_4));

	dsim_panel_set_brightness(dsim, 1);

displayon_err:
	return ret;

}

static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct dsim_device *dsim = NULL;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
		break;
	default:
		return 0;
	}

	dsim = container_of(self, struct dsim_device, fb_notif_panel);

	fb_blank = *(int *)evdata->data;

	dsim_info("%s: %d\n", __func__, fb_blank);

	if (evdata->info->node != 0)
		return 0;

	if (fb_blank == FB_BLANK_UNBLANK)
		sc7798d_displayon_late(dsim);

	return 0;
}

static int sc7798d_exit(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = NULL;
	panel = &dsim->priv;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
		goto exit_err;
	}

	msleep(10);

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
		goto exit_err;
	}

	msleep(150);

exit_err:
	return ret;
}

static int sc7798d_init(struct dsim_device *dsim)
{
	int ret;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_INIT1_1, ARRAY_SIZE(SEQ_INIT1_1));
	ret = dsim_write_hl_data(dsim, SEQ_INIT1_2, ARRAY_SIZE(SEQ_INIT1_2));
	ret = dsim_write_hl_data(dsim, SEQ_INIT1_3, ARRAY_SIZE(SEQ_INIT1_3));
	ret = dsim_write_hl_data(dsim, SEQ_INIT1_4, ARRAY_SIZE(SEQ_INIT1_4));
	ret = dsim_write_hl_data(dsim, SEQ_INIT1_5, ARRAY_SIZE(SEQ_INIT1_5));

	msleep(12);

	ret = dsim_write_hl_data(dsim, SEQ_INIT2_1, ARRAY_SIZE(SEQ_INIT2_1));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_2, ARRAY_SIZE(SEQ_INIT2_2));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_3, ARRAY_SIZE(SEQ_INIT2_3));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_4, ARRAY_SIZE(SEQ_INIT2_4));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_5, ARRAY_SIZE(SEQ_INIT2_5));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_6, ARRAY_SIZE(SEQ_INIT2_6));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_7, ARRAY_SIZE(SEQ_INIT2_7));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_8, ARRAY_SIZE(SEQ_INIT2_8));
	ret = dsim_write_hl_data(dsim, SEQ_INIT2_9, ARRAY_SIZE(SEQ_INIT2_9));

	ret = dsim_write_hl_data(dsim, SEQ_INIT3_1, ARRAY_SIZE(SEQ_INIT3_1));
	
	ret = dsim_write_hl_data(dsim, SEQ_INIT4_1, ARRAY_SIZE(SEQ_INIT4_1));
	ret = dsim_write_hl_data(dsim, SEQ_INIT4_2, ARRAY_SIZE(SEQ_INIT4_2));
	ret = dsim_write_hl_data(dsim, SEQ_INIT4_3, ARRAY_SIZE(SEQ_INIT4_3));

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	msleep(150);

	return ret;
}

static int sc7798d_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("MDD : %s was called\n", __func__);

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;
	panel->acl_enable_force = ACL_TEST_DO_NOT_WORK;

	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}

	sc7798d_read_init_info(dsim);
	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}


	memset(&dsim->fb_notif_panel, 0, sizeof(dsim->fb_notif_panel));
	dsim->fb_notif_panel.notifier_call = fb_notifier_callback;
	fb_register_client(&dsim->fb_notif_panel);

probe_exit:
	return ret;
}

struct dsim_panel_ops sc7798d_panel_ops = {
	.early_probe	= NULL,
	.probe			= sc7798d_probe,
	.displayon		= sc7798d_displayon,
	.displayon_late	= sc7798d_displayon_late,
	.exit			= sc7798d_exit,
	.init			= sc7798d_init,
};


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

static ssize_t cabc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	struct panel_private *panel;

	dsim = container_of(priv, struct dsim_device, priv);
	panel = &dsim->priv;

	sprintf(buf, "%d\n", panel->acl_enable);

	return strlen(buf);
}

static ssize_t cabc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	struct panel_private *panel;
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);
	panel = &dsim->priv;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (value) /* cabc on */
			panel->acl_enable_force = ACL_TEST_ON;
		else /* cabc off */
			panel->acl_enable_force = ACL_TEST_OFF;
	}

	dsim_panel_set_brightness(dsim, 1);

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

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(cabc, 0664, cabc_show, cabc_store);
static DEVICE_ATTR(dump_register, 0664, dump_register_show, dump_register_store);

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

	ret = device_create_file(&dsim->priv.bd->dev, &dev_attr_cabc);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_dump_register);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
}


#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
static int mdnie_lite_write_set(struct dsim_device *dsim, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0, i;

	for (i = 0; i < num; i++) {
		if (seq[i].cmd) {
			ret = dsim_write_hl_data(dsim, seq[i].cmd, seq[i].len);
			if (ret != 0) {
				dsim_err("%s failed.\n", __func__);
				return ret;
			}
		}
		if (seq[i].sleep)
			usleep_range(seq[i].sleep * 1000 , seq[i].sleep * 1000);
	}
	return ret;
}

static int mdnie_lite_send_seq(struct dsim_device *dsim, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active\n", __func__);
		return -EIO;
	}

	mutex_lock(&panel->lock);
	ret = mdnie_lite_write_set(dsim, seq, num);

	mutex_unlock(&panel->lock);

	return ret;
}

static int mdnie_lite_read(struct dsim_device *dsim, u8 addr, u8 *buf, u32 size)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active\n", __func__);
		return -EIO;
	}
	mutex_lock(&panel->lock);
	ret = dsim_read_hl_data(dsim, addr, size, buf);

	mutex_unlock(&panel->lock);

	return ret;
}
#endif

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
#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
	u16 coordinate[2] = {0, };
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

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
	coordinate[0] = (u16)panel->coordinate[0];
	coordinate[1] = (u16)panel->coordinate[1];
	mdnie_register(&dsim->lcd->dev, dsim, (mdnie_w)mdnie_lite_send_seq, (mdnie_r)mdnie_lite_read, coordinate, &tune_info);
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

struct mipi_dsim_lcd_driver sc7798d_mipi_lcd_driver = {
	.early_probe = dsim_panel_early_probe,
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
	.resume		= dsim_panel_resume,
};

