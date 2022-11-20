/* linux/drivers/video/backlight/s6d7aa0_gtactive2_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/of_device.h>
#include <video/mipi_display.h>
#include "../dsim.h"
#include "../decon.h"
#include "dsim_panel.h"

#include "s6d7aa0_gtactive2_param.h"
#include "backlight_tuning.h"

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#include "mdnie.h"
#include "mdnie_lite_table_gtactive2.h"
#endif



struct lcd_info {
	unsigned int			bl;
	unsigned int			brightness;
	unsigned int			current_bl;
	unsigned int			state;
	unsigned int			te_on;

	struct lcd_device		*ld;
	struct backlight_device		*bd;

	unsigned char			id[3];
	unsigned char			dump_info[3];

	struct dsim_device		*dsim;
	struct mutex			lock;
	int				lux;
	struct class			*mdnie_class;
};


static int dsim_write_hl_data(struct lcd_info *lcd, const u8 *cmd, u32 cmdSize)
{
	int ret;
	int retry;
	struct panel_private *priv = &lcd->dsim->priv;

	if (!priv->lcdConnected)
		return cmdSize;

	retry = 5;

try_write:
	if (cmdSize == 1)
		ret = dsim_write_data(lcd->dsim, MIPI_DSI_DCS_SHORT_WRITE, cmd[0], 0);
	else if (cmdSize == 2)
		ret = dsim_write_data(lcd->dsim, MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd[0], cmd[1]);
	else
		ret = dsim_write_data(lcd->dsim, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)cmd, cmdSize);

	if (ret != 0) {
		if (--retry)
			goto try_write;
		else
			dev_err(&lcd->ld->dev, "%s: fail. cmd: %x, ret: %d\n", __func__, cmd[0], ret);
	}

	return ret;
}

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
static int dsim_read_hl_data(struct lcd_info *lcd, u8 addr, u32 size, u8 *buf)
{
	int ret;
	int retry = 4;
	struct panel_private *priv = &lcd->dsim->priv;

	if (!priv->lcdConnected)
		return size;

try_read:
	ret = dsim_read_data(lcd->dsim, MIPI_DSI_DCS_READ, (u32)addr, size, buf);
	dev_info(&lcd->ld->dev, "%s: addr: %x, ret: %d\n", __func__, addr, ret);
	if (ret != size) {
		if (--retry)
			goto try_read;
		else
			dev_err(&lcd->ld->dev, "%s: fail. addr: %x, ret: %d\n", __func__, addr, ret);
	}

	return ret;
}
#endif

static int dsim_panel_set_brightness(struct lcd_info *lcd, int force)
{
	int ret = 0;
	unsigned char bl_reg[2];

	mutex_lock(&lcd->lock);

	lcd->brightness = lcd->bd->props.brightness;

	lcd->bl = lcd->brightness;

	if (!force && lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: panel is not active state\n", __func__);
		goto exit;
	}
	bl_reg[0] = BRIGHTNESS_REG;
	bl_reg[1] = brightness_table[lcd->brightness];

	dev_info(&lcd->ld->dev, "%s: platform BL : %d panel BL reg : %d\n", __func__, lcd->bd->props.brightness, bl_reg[1]);
	if (dsim_write_hl_data(lcd, bl_reg, ARRAY_SIZE(bl_reg)) < 0)
		dev_err(&lcd->ld->dev, "%s: failed to write brightness cmd.\n", __func__);
exit:
	mutex_unlock(&lcd->lock);

	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	struct lcd_info *lcd = bl_get_data(bd);

	if (lcd->state == PANEL_STATE_RESUMED) {
		ret = dsim_panel_set_brightness(lcd, 0);
		if (ret) {
			dev_err(&lcd->ld->dev, "%s: failed to set brightness\n", __func__);
			goto exit;
		}
	}

exit:
	return ret;
}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
};


static int s6d7aa0_read_init_info(struct lcd_info *lcd)
{
	struct panel_private *priv = &lcd->dsim->priv;
	int i = 0;

	dev_info(&lcd->ld->dev, "MDD : %s was called\n", __func__);

	if (lcdtype == 0) {
		priv->lcdConnected = 0;
		goto read_exit;
	}

	lcd->id[0] = (lcdtype & 0xFF0000) >> 16;
	lcd->id[1] = (lcdtype & 0x00FF00) >> 8;
	lcd->id[2] = (lcdtype & 0x0000FF) >> 0;

	dev_info(&lcd->ld->dev, "READ ID : ");
	for (i = 0; i < S6D7AA0_ID_LEN; i++)
		dev_info(&lcd->ld->dev, "%02x, ", lcd->id[i]);
	dev_info(&lcd->ld->dev, "\n");

read_exit:
	return 0;
}

static int s6d7aa0_displayon(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	ret = dsim_write_hl_data(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : DISPLAY_ON\n", __func__);
		goto displayon_err;
	}

	dsim_panel_set_brightness(lcd, 1);


displayon_err:
	return ret;

}

static int s6d7aa0_exit(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);
	ret = dsim_write_hl_data(lcd, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : DISPLAY_OFF\n", __func__);
		goto exit_err;
	}

	msleep(35);

	ret = dsim_write_hl_data(lcd, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SLEEP_IN\n", __func__);
		goto exit_err;
	}

	msleep(150);

exit_err:
	return ret;
}

static int s6d7aa0_init(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);
	msleep(100);
	ret = dsim_write_hl_data(lcd, SEQ_PASSWD1, ARRAY_SIZE(SEQ_PASSWD1));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_PASSWD1\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_PASSWD2, ARRAY_SIZE(SEQ_PASSWD2));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_PASSWD2\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_PASSWD3, ARRAY_SIZE(SEQ_PASSWD3));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_PASSWD3\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_OTP_RELOAD, ARRAY_SIZE(SEQ_OTP_RELOAD));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_OTP_RELOAD\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_DISP_CTL, ARRAY_SIZE(SEQ_DISP_CTL));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_DISP_CTL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_IF_CTL, ARRAY_SIZE(SEQ_IF_CTL));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_IF_CTL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_PORCH_CTL, ARRAY_SIZE(SEQ_PORCH_CTL));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_PORCH_CTL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_F3_PARAM, ARRAY_SIZE(SEQ_F3_PARAM));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_F3_PARAM\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_F4_PARAM, ARRAY_SIZE(SEQ_F4_PARAM));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_F4_PARAM\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_GLOBAL_PARA_1, ARRAY_SIZE(SEQ_GLOBAL_PARA_1));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_GLOBAL_PARA_1\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_PWR_CTL, ARRAY_SIZE(SEQ_PWR_CTL));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_PWR_CTL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_EE_PARAM, ARRAY_SIZE(SEQ_EE_PARAM));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_EE_PARAM\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_EF_PARAM, ARRAY_SIZE(SEQ_EF_PARAM));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_EF_PARAM\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_GOUT_CTL, ARRAY_SIZE(SEQ_GOUT_CTL));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_GOUT_CTL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_BATT_CTL, ARRAY_SIZE(SEQ_BATT_CTL));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_BATT_CTL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_E1_PARAM, ARRAY_SIZE(SEQ_E1_PARAM));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_E1_PARAM\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_FD_PARAM, ARRAY_SIZE(SEQ_FD_PARAM));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_FD_PARAM\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_FE_PARAM, ARRAY_SIZE(SEQ_FE_PARAM));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_FE_PARAM\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_SRC_CTL, ARRAY_SIZE(SEQ_SRC_CTL));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_SRC_CTL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_ID_123, ARRAY_SIZE(SEQ_ID_123));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_ID_123\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_GLOBAL_PARA_22, ARRAY_SIZE(SEQ_GLOBAL_PARA_22));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_GLOBAL_PARA_22\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_PGAMMA_CTL_1, ARRAY_SIZE(SEQ_PGAMMA_CTL_1));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_PGAMMA_CTL_1\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_GLOBAL_PARA_22, ARRAY_SIZE(SEQ_GLOBAL_PARA_22));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_GLOBAL_PARA_22\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_NGAMMA_CTL_1, ARRAY_SIZE(SEQ_NGAMMA_CTL_1));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_NGAMMA_CTL_1\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_GLOBAL_PARA_11, ARRAY_SIZE(SEQ_GLOBAL_PARA_11));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_GLOBAL_PARA_11\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_PGAMMA_CTL_2, ARRAY_SIZE(SEQ_PGAMMA_CTL_2));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_PGAMMA_CTL_2\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_GLOBAL_PARA_11, ARRAY_SIZE(SEQ_GLOBAL_PARA_11));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_GLOBAL_PARA_11\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_NGAMMA_CTL_2, ARRAY_SIZE(SEQ_NGAMMA_CTL_2));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_NGAMMA_CTL_2\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_PGAMMA_CTL_3, ARRAY_SIZE(SEQ_PGAMMA_CTL_3));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_PGAMMA_CTL_3\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_NGAMMA_CTL_3, ARRAY_SIZE(SEQ_NGAMMA_CTL_3));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_NGAMMA_CTL_3\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_BL_ON_CTL, ARRAY_SIZE(SEQ_BL_ON_CTL));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_BL_ON_CTL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_B3_PARAM, ARRAY_SIZE(SEQ_B3_PARAM));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_B3_PARAM\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_BACKLIGHT_CTL, ARRAY_SIZE(SEQ_BACKLIGHT_CTL));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_BACKLIGHT_CTL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_PWM_DUTY, ARRAY_SIZE(SEQ_PWM_DUTY)); // 51 7F
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_PWM_DIMMING_OFF\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_PWM_MANUAL, ARRAY_SIZE(SEQ_PWM_MANUAL)); // 51 7F
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_PWM_DIMMING_OFF\n", __func__);
		goto init_exit;
	}

	if (lcd->te_on) {
		dev_info(&lcd->ld->dev, "%s: te is on\n", __func__);
		ret = dsim_write_hl_data(lcd, SEQ_TE_HSYNC_ON, ARRAY_SIZE(SEQ_TE_HSYNC_ON));
		if (ret < 0) {
			dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_TE_HSYNC_ON\n", __func__);
			goto init_exit;
		}
	}

	msleep(10);
	ret = dsim_write_hl_data(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}
	msleep(120);
	ret = dsim_write_hl_data(lcd, SEQ_PASSWD1_LOCK, ARRAY_SIZE(SEQ_PASSWD1_LOCK));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_PASSWD1_LOCK\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_PASSWD2_LOCK, ARRAY_SIZE(SEQ_PASSWD2_LOCK));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_PASSWD2_LOCK\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(lcd, SEQ_PASSWD3_LOCK, ARRAY_SIZE(SEQ_PASSWD3_LOCK));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s: failed to write CMD : SEQ_PASSWD3_LOCK\n", __func__);
		goto init_exit;
	}

init_exit:
	return ret;
}

static int s6d7aa0_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *priv = &dsim->priv;
	struct lcd_info *lcd = dsim->priv.par;

	dev_info(&lcd->ld->dev, "%s: was called\n", __func__);

	priv->lcdConnected = 1;

	lcd->bd->props.max_brightness = EXTEND_BRIGHTNESS;
	lcd->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;

	lcd->dsim = dsim;
	lcd->state = PANEL_STATE_RESUMED;
	lcd->te_on = 0;

	s6d7aa0_read_init_info(lcd);
	if (!priv->lcdConnected) {
		dev_err(&lcd->ld->dev, "dsim : %s lcd was not connected\n", __func__);
		goto exit;
	}

	dev_info(&lcd->ld->dev, "%s: done\n", __func__);
exit:
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

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
		attr_store_for_each(lcd->mdnie_class, attr->attr.name, buf, size);
#endif
	}

	return size;
}

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;
	char *pos = buf;

	for (i = 0; i <= EXTEND_BRIGHTNESS; i++)
		pos += sprintf(pos, "%3d %3d\n", i, brightness_table[i]);

	return pos - buf;
}

static ssize_t hs_clk_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u32 val;
	struct lcd_info *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = NULL;

	if (!lcd || lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: lcd is not active state\n", __func__);
		return -EPERM;
	}

	dsim = lcd->dsim;
	if (!dsim || dsim->state != DSIM_STATE_HSCLKEN) {
		dev_info(&lcd->ld->dev, "%s: dsim is not active state\n", __func__);
		return -EPERM;
	}

	val = dsim_read_mask(dsim->id, DSIM_PLLCTRL, DSIM_PLLCTRL_PMS_MASK);
	sprintf(buf, "p: %d m: %d s: %d raw: 0x%08X\n", ((val >> 13) & 0x3f), ((val >> 3) & 0x1ff), (val & 0x7), val);

	return strlen(buf);
}

static ssize_t hs_clk_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int p, m, s;

	sscanf(buf, "%8d %8d %8d", &p, &m, &s);

	dev_info(dev, "%s: p:%d m:%d s:%d\n", __func__, p, m, s);

	lcd->dsim->lcd_info.dphy_pms.p = p;
	lcd->dsim->lcd_info.dphy_pms.m = m;
	lcd->dsim->lcd_info.dphy_pms.s = s;

	return size;
}

static ssize_t te_on_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "te_on: %d\n", lcd->te_on);

	dev_info(&lcd->ld->dev, "%s: te_on: %d\n", __func__, lcd->te_on);

	return strlen(buf);
}

static ssize_t te_on_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int input;
	int ret;

	ret = kstrtouint(buf, 0, &input);
	if (ret) {
		dev_info(&lcd->ld->dev, "%s: failed to write te_on value\n", __func__);
		return ret;
	}

	lcd->te_on = input ? 1 : 0;

	dev_info(&lcd->ld->dev, "%s: te_on: %d\n", __func__, lcd->te_on);

	return size;
}

static ssize_t porch_change_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct decon_device *decon = NULL;
	decon = (struct decon_device *)dsim->decon;

	sprintf(buf, "%s: vclk:%d hbp:%d hfp:%d hsa:%d vbp:%d vfp:%d vsa:%d\n", __func__, decon->pdata->disp_vclk,
		decon->lcd_info->hbp, decon->lcd_info->hfp, decon->lcd_info->hsa,
		decon->lcd_info->vbp, decon->lcd_info->vfp, decon->lcd_info->vsa);

	return strlen(buf);
}

static ssize_t porch_change_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	unsigned int hfp, hbp, hsa, vfp, vbp, vsa;
	unsigned long vclk;
	struct decon_device *decon = NULL;
	decon = (struct decon_device *)dsim->decon;

	sscanf(buf, "%8lu %8d %8d %8d %8d %8d %8d", &vclk, &hbp, &hfp, &hsa, &vbp, &vfp, &vsa);

	dev_info(dev, "%s: input: vclk:%lu hbp:%d hfp:%d hsa:%d vbp:%d vfp:%d vsa:%d\n",
		__func__, vclk, hbp, hfp, hsa, vbp, vfp, vsa);

	/* dsim */
	dsim->lcd_info.hbp = hbp;
	dsim->lcd_info.hfp = hfp;
	dsim->lcd_info.hsa = hsa;

	dsim->lcd_info.vbp = vbp;
	dsim->lcd_info.vfp = vfp;
	dsim->lcd_info.vsa = vsa;

	/* decon */
	decon->lcd_info->hbp = hbp;
	decon->lcd_info->hfp = hfp;
	decon->lcd_info->hsa = hsa;

	decon->lcd_info->vbp = vbp;
	decon->lcd_info->vfp = vfp;
	decon->lcd_info->vsa = vsa;

	decon->pdata->disp_pll_clk = vclk;
	decon->pdata->disp_vclk = vclk;

	return size;
}

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(lux, 0644, lux_show, lux_store);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(hs_clk, 0644, hs_clk_show, hs_clk_store);
static DEVICE_ATTR(te_on, 0644, te_on_show, te_on_store);
static DEVICE_ATTR(porch_change, 0644, porch_change_show, porch_change_store);


static struct attribute *lcd_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_lux.attr,
	&dev_attr_brightness_table.attr,
	&dev_attr_hs_clk.attr,
	&dev_attr_te_on.attr,
	&dev_attr_porch_change.attr,
	NULL,
};

static const struct attribute_group lcd_sysfs_attr_group = {
	.attrs = lcd_sysfs_attributes,
};

static void lcd_init_sysfs(struct lcd_info *lcd)
{
	int ret = 0;

	ret = sysfs_create_group(&lcd->ld->dev.kobj, &lcd_sysfs_attr_group);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add lcd sysfs\n");
}


#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
static int mdnie_lite_write_set(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0, i;

	for (i = 0; i < num; i++) {
		if (seq[i].cmd) {
			ret = dsim_write_hl_data(lcd, seq[i].cmd, seq[i].len);
			if (ret != 0) {
				dev_info(&lcd->ld->dev, "%s: %dth fail\n", __func__, i);
				return ret;
			}
		}
		if (seq[i].sleep)
			usleep_range(seq[i].sleep * 1000, seq[i].sleep * 1000);
	}
	return ret;
}

int mdnie_lite_send_seq(struct lcd_info *lcd, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0;

	mutex_lock(&lcd->lock);

	if (lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: panel is not active\n", __func__);
		ret = -EIO;
		goto exit;
	}

	ret = mdnie_lite_write_set(lcd, seq, num);

exit:
	mutex_unlock(&lcd->lock);

	return ret;
}

int mdnie_lite_read(struct lcd_info *lcd, u8 addr, u8 *buf, u32 size)
{
	int ret = 0;

	mutex_lock(&lcd->lock);

	if (lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s: panel is not active\n", __func__);
		ret = -EIO;
		goto exit;
	}

	ret = dsim_read_hl_data(lcd, addr, size, buf);

exit:
	mutex_unlock(&lcd->lock);

	return ret;
}
#endif


static int dsim_panel_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd;

	dsim->priv.par = lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("%s: failed to allocate for lcd\n", __func__);
		ret = -ENOMEM;
		goto probe_err;
	}

	dsim->lcd = lcd->ld = lcd_device_register("panel", dsim->dev, lcd, NULL);
	if (IS_ERR(lcd->ld)) {
		pr_err("%s: failed to register lcd device\n", __func__);
		ret = PTR_ERR(lcd->ld);
		goto probe_err;
	}

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &panel_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("%s: failed to register backlight device\n", __func__);
		ret = PTR_ERR(lcd->bd);
		goto probe_err;
	}

	mutex_init(&lcd->lock);

	ret = s6d7aa0_probe(dsim);
	if (ret) {
		dev_err(&lcd->ld->dev, "%s: failed to probe panel\n", __func__);
		goto probe_err;
	}

#if defined(CONFIG_EXYNOS_DECON_LCD_SYSFS)
	lcd_init_sysfs(lcd);
	init_bl_curve_debugfs(lcd->bd, brightness_table, NULL);
#endif

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
	mdnie_register(&lcd->ld->dev, lcd, (mdnie_w)mdnie_lite_send_seq, (mdnie_r)mdnie_lite_read, NULL, &tune_info);
	lcd->mdnie_class = get_mdnie_class();
#endif


	dev_info(&lcd->ld->dev, "%s: %s: done\n", kbasename(__FILE__), __func__);
probe_err:
	return ret;
}

static int dsim_panel_displayon(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;
	struct panel_private *priv = &lcd->dsim->priv;
	int ret = 0;

	dev_info(&lcd->ld->dev, "+%s\n", __func__);

	if (lcd->state == PANEL_STATE_SUSPENED) {
		ret = s6d7aa0_init(lcd);
		if (ret) {
			dev_info(&lcd->ld->dev, "%s: failed to panel init\n", __func__);
			goto displayon_err;
		}
	}

	ret = s6d7aa0_displayon(lcd);
	if (ret) {
		dev_info(&lcd->ld->dev, "%s: failed to panel display on\n", __func__);
		goto displayon_err;
	}

displayon_err:
	mutex_lock(&lcd->lock);
	lcd->state = PANEL_STATE_RESUMED;
	mutex_unlock(&lcd->lock);

	dev_info(&lcd->ld->dev, "-%s: %d\n", __func__, priv->lcdConnected);

	return ret;
}

static int dsim_panel_suspend(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;
	struct panel_private *priv = &lcd->dsim->priv;
	int ret = 0;

	dev_info(&lcd->ld->dev, "+%s\n", __func__);

	if (lcd->state == PANEL_STATE_SUSPENED)
		goto suspend_err;

	lcd->state = PANEL_STATE_SUSPENDING;

	ret = s6d7aa0_exit(lcd);
	if (ret) {
		dev_info(&lcd->ld->dev, "%s: failed to panel exit\n", __func__);
		goto suspend_err;
	}

suspend_err:
	mutex_lock(&lcd->lock);
	lcd->state = PANEL_STATE_SUSPENED;
	mutex_unlock(&lcd->lock);

	dev_info(&lcd->ld->dev, "-%s: %d\n", __func__, priv->lcdConnected);

	return ret;
}

struct mipi_dsim_lcd_driver s6d7aa0_mipi_lcd_driver = {
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
};

