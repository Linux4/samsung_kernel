/*
 * drivers/video/decon_7580/panels/s6e3fa3_lcd_ctrl.c
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
#include <video/mipi_display.h>
#include "../dsim.h"
#include "dsim_panel.h"

#include "s6e3fa3_param.h"

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#include "dimming_core.h"
#include "s6e3fa3_aid_dimming.h"
#endif

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#include "mdnie.h"
#include "mdnie_lite_table_a8c.h"
#endif

#define LCD_EA8064G 1
#define LCD_S6E3FA3 3



#ifdef CONFIG_PANEL_AID_DIMMING
static unsigned int get_actual_br_value(struct dsim_device *dsim, int index)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_br_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return dimming_info[index].br;

get_br_err:
	return 0;
}

static unsigned char *get_gamma_from_index(struct dsim_device *dsim, int index)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_gamma_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return (unsigned char *)dimming_info[index].gamma;

get_gamma_err:
	return NULL;
}

static unsigned char *get_aid_from_index(struct dsim_device *dsim, int index)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_aid_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return (u8 *)dimming_info[index].aid;

get_aid_err:
	return NULL;
}

static unsigned char *get_elvss_from_index(struct dsim_device *dsim, int index, int acl)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_elvess_err;
	}

	if (acl)
		return (unsigned char *)dimming_info[index].elvAcl;
	else
		return (unsigned char *)dimming_info[index].elv;

get_elvess_err:
	return NULL;
}

static void dsim_panel_gamma_ctrl(struct dsim_device *dsim)
{
	u8 *gamma = NULL;
	gamma = get_gamma_from_index(dsim, dsim->priv.br_index);
	if (gamma == NULL) {
		dsim_err("%s :faied to get gamma\n", __func__);
		return;
	}

	if (dsim_write_hl_data(dsim, gamma, GAMMA_CMD_CNT) < 0)
		dsim_err("%s : failed to write gamma\n", __func__);
}

static void dsim_panel_aid_ctrl(struct dsim_device *dsim)
{
	u8 *aid = NULL;
	aid = get_aid_from_index(dsim, dsim->priv.br_index);
	if (aid == NULL) {
		dsim_err("%s : faield to get aid value\n", __func__);
		return;
	}
	if (dsim_write_hl_data(dsim, aid, AID_CMD_CNT) < 0)
		dsim_err("%s : failed to write gamma\n", __func__);
}

static void dsim_panel_set_elvss(struct dsim_device *dsim)
{
	u8 *elvss = NULL;
	u8 elvss_val[5] = {0, };

	elvss = get_elvss_from_index(dsim, dsim->priv.br_index, dsim->priv.acl_enable);
	if (elvss == NULL) {
		dsim_err("%s : failed to get elvss value\n", __func__);
		return;
	}

	elvss_val[0] = elvss[0];
	elvss_val[1] = elvss[1];
	if (dsim->priv.temperature <= 0)
		elvss_val[1] = elvss_val[1] + 0x04;
	elvss_val[2] = elvss[2];
	elvss_val[3] = dsim->priv.elvss[2];
	elvss_val[4] = dsim->priv.elvss[3];
	if (dsim->priv.temperature < -25)
		elvss_val[4] = elvss_val[4] - 0x05;

	if (dsim_write_hl_data(dsim, elvss_val, ELVSS_LEN + 1) < 0)
		dsim_err("%s : failed to write elvss\n", __func__);
}

static int dsim_panel_set_acl(struct dsim_device *dsim, int force)
{
	int ret = 0, level = ACL_STATUS_15P;
	struct panel_private *panel = &dsim->priv;

	if (panel == NULL) {
			dsim_err("%s : panel is NULL\n", __func__);
			goto exit;
	}

	if (dsim->priv.siop_enable || LEVEL_IS_HBM(dsim->priv.auto_brightness))  /* auto acl or hbm is acl on */
		goto acl_update;

	if (!dsim->priv.acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if (force || dsim->priv.current_acl != panel->acl_cutoff_tbl[level][1]) {
		if (dsim_write_hl_data(dsim,  panel->acl_opr_tbl[level], 2) < 0) {
			dsim_err("fail to write acl opr command.\n");
			ret = -EPERM;
			goto exit;
		}
		if (dsim_write_hl_data(dsim, panel->acl_cutoff_tbl[level], 2) < 0) {
			dsim_err("fail to write acl command.\n");
			ret = -EPERM;
			goto exit;
		}
		dsim->priv.current_acl = panel->acl_cutoff_tbl[level][1];
		dsim_info("acl: %d, auto_brightness: %d\n", dsim->priv.current_acl, dsim->priv.auto_brightness);
	}
exit:
	if (!ret)
		ret = -EPERM;
	return ret;
}

static int dsim_panel_set_tset(struct dsim_device *dsim, int force)
{
	int ret = 0;
	int tset = 0;
	unsigned char SEQ_TSET[TSET_LEN] = {TSET_REG, };

	tset = ((dsim->priv.temperature >= 0) ? 0 : BIT(7)) | abs(dsim->priv.temperature);

	if (force || dsim->priv.tset[0] != tset) {
		dsim->priv.tset[0] = SEQ_TSET[1] = tset;
		dsim->priv.tset[1] = SEQ_TSET[2] = 0x0A;

		if (dsim->priv.panel_type == LCD_EA8064G) {
			if (dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET)) < 0) {
				dsim_err("fail to write tset command.\n");
				ret = -EPERM;
			}
		} else {
			if (dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET) - 1) < 0) {
				dsim_err("fail to write tset command.\n");
				ret = -EPERM;
			}
		}
		dsim_info("temperature: %d, tset: %d\n", dsim->priv.temperature, SEQ_TSET[1]);
	}
	return ret;
}

static int dsim_panel_set_hbm(struct dsim_device *dsim, int force)
{
	int ret = 0, level = LEVEL_IS_HBM(dsim->priv.auto_brightness);
	struct panel_private *panel = &dsim->priv;

	if (dsim->support_lvds)
		return ret;

	if (panel == NULL) {
		dsim_err("%s : panel is NULL\n", __func__);
		goto exit;
	}

	if (force || dsim->priv.current_hbm != panel->hbm_tbl[level][1]) {
		dsim->priv.current_hbm = panel->hbm_tbl[level][1];
		if (dsim_write_hl_data(dsim, panel->hbm_tbl[level], ARRAY_SIZE(SEQ_HBM_OFF)) < 0) {
			dsim_err("fail to write hbm command.\n");
			ret = -EPERM;
		}
		dsim_info("hbm: %d, auto_brightness: %d\n", dsim->priv.current_hbm, dsim->priv.auto_brightness);
	}
exit:
	return ret;
}

static int low_level_set_brightness(struct dsim_device *dsim, int force)
{
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);

	dsim_panel_gamma_ctrl(dsim);

	dsim_panel_aid_ctrl(dsim);

	dsim_panel_set_elvss(dsim);

	if (dsim->priv.panel_type == LCD_EA8064G) {
		if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_EA8064G, ARRAY_SIZE(SEQ_GAMMA_UPDATE_EA8064G)) < 0)
			dsim_err("%s : failed to write gamma\n", __func__);
	} else {
		if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
			dsim_err("%s : failed to write gamma\n", __func__);

		if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L)) < 0)
			dsim_err("%s : failed to write gamma\n", __func__);
	}

	dsim_panel_set_acl(dsim, force);

	dsim_panel_set_tset(dsim, force);

	dsim_panel_set_hbm(dsim, force);

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
			dsim_err("%s : fail to write F0 on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("%s : fail to write F0 on command\n", __func__);

	return 0;
}

static int get_acutal_br_index(struct dsim_device *dsim, int br)
{
	int i;
	int min;
	int gap;
	int index = 0;
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming_info is NULL\n", __func__);
		return 0;
	}

	min = MAX_BRIGHTNESS;

	for (i = 0; i < MAX_BR_INFO; i++) {
		if (br > dimming_info[i].br)
			gap = br - dimming_info[i].br;
		else
			gap = dimming_info[i].br - br;

		if (gap == 0) {
			index = i;
			break;
		}

		if (gap < min) {
			min = gap;
			index = i;
		}
	}
	return index;
}
#endif

static int dsim_panel_set_brightness(struct dsim_device *dsim, int force)
{
	struct dim_data *dimming;
	struct panel_private *panel = &dsim->priv;
	int p_br = panel->bd->props.brightness;
	int acutal_br = 0;
	int real_br = 0;
	int prev_index = panel->br_index;
	int ret = 0;

	dimming = (struct dim_data *)panel->dim_data;
	if ((dimming == NULL) || (panel->br_tbl == NULL)) {
		dsim_info("%s : this panel does not support dimming\n", __func__);
		return ret;
	}

	acutal_br = panel->br_tbl[p_br];
	panel->br_index = get_acutal_br_index(dsim, acutal_br);
	real_br = get_actual_br_value(dsim, panel->br_index);

	if (LEVEL_IS_HBM(panel->auto_brightness) && (p_br == panel->bd->props.max_brightness)) {
		panel->br_index = HBM_INDEX;
		panel->acl_enable = 1;				/* hbm is acl on */
	}

	if (panel->siop_enable)					/* check auto acl */
		panel->acl_enable = 1;

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		goto set_br_exit;
	}

	dsim_info("%s : platform : %d, : mapping : %d, real : %d, index : %d\n",
		__func__, p_br, acutal_br, real_br, panel->br_index);

	if (!force && panel->br_index == prev_index)
		goto set_br_exit;

	if ((acutal_br == 0) || (real_br == 0))
		goto set_br_exit;

	mutex_lock(&panel->lock);

	ret = low_level_set_brightness(dsim, force);
	if (ret)
		dsim_err("%s failed to set brightness : %d\n", __func__, acutal_br);

	mutex_unlock(&panel->lock);

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

	return ret;
}



static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "SDC_%02X%02X%02X\n", priv->id[0], priv->id[1], priv->id[2]);

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

	if (panel->br_tbl == NULL)
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


static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "-20, -19, 0, 1\n";

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t temperature_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtoint(buf, 10, &value);

	if (rc < 0)
		return rc;
	else {
		mutex_lock(&priv->lock);
		priv->temperature = value;
		mutex_unlock(&priv->lock);

		dsim_panel_set_brightness(dsim, 1);
		dev_info(dev, "%s: %d, %d\n", __func__, value, priv->temperature);
	}

	return size;
}

static ssize_t color_coordinate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u, %u\n", priv->coordinate[0], priv->coordinate[1]);
	return strlen(buf);
}

static ssize_t manufacture_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	u16 year;
	u8 month, day, hour, min;

	year = ((priv->date[0] & 0xF0) >> 4) + 2011;
	month = priv->date[0] & 0xF;
	day = priv->date[1] & 0x1F;
	hour = priv->date[2] & 0x1F;
	min = priv->date[3] & 0x3F;

	sprintf(buf, "%d, %d, %d, %d:%d\n", year, month, day, hour, min);
	return strlen(buf);
}

static ssize_t read_mtp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return strlen(buf);
}

static ssize_t read_mtp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	unsigned int reg, pos, length, i;
	unsigned char readbuf[256] = {0xff, };
	unsigned char writebuf[2] = {0xB0, };

	dsim = container_of(priv, struct dsim_device, priv);

	sscanf(buf, "%x %d %d", &reg, &pos, &length);
	writebuf[1] = pos;
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("fail to write F0 on command.\n");

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
		dsim_err("fail to write test on fc command.\n");

	if (dsim_write_hl_data(dsim, writebuf, ARRAY_SIZE(writebuf)) < 0)
		dsim_err("fail to write global command.\n");

	if (dsim_read_hl_data(dsim, reg, length, readbuf) < 0)
		dsim_err("fail to read id on command.\n");

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("fail to write F0 on command.\n");

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
		dsim_err("fail to write test on fc command.\n");

	dsim_info("READ_Reg addr : %02x, start pos : %d len : %d\n", reg, pos, length);
	for (i = 0; i < length; i++)
		dsim_info("READ_Reg %dth : %02x\n", i + 1, readbuf[i]);

	return size;
}

#ifdef CONFIG_PANEL_AID_DIMMING
static void show_aid_log(struct dsim_device *dsim)
{
	int i, j, k;
	struct dim_data *dim_data = NULL;
	struct SmtDimInfo *dimming_info = NULL;
	struct panel_private *panel = &dsim->priv;
	u8 temp[256];


	if (panel == NULL) {
		dsim_err("%s:panel is NULL\n", __func__);
		return;
	}

	dim_data = (struct dim_data *)(panel->dim_data);
	if (dim_data == NULL) {
		dsim_info("%s:dimming is NULL\n", __func__);
		return;
	}

	dimming_info = (struct SmtDimInfo *)(panel->dim_info);
	if (dimming_info == NULL) {
		dsim_info("%s:dimming is NULL\n", __func__);
		return;
	}

	dsim_info("MTP VT : %d %d %d\n",
			dim_data->vt_mtp[CI_RED], dim_data->vt_mtp[CI_GREEN], dim_data->vt_mtp[CI_BLUE]);

	for (i = 0; i < VMAX; i++) {
		dsim_info("MTP V%d : %4d %4d %4d\n",
			vref_index[i], dim_data->mtp[i][CI_RED], dim_data->mtp[i][CI_GREEN], dim_data->mtp[i][CI_BLUE]);
	}

	for (i = 0; i < MAX_BR_INFO; i++) {
		memset(temp, 0, sizeof(temp));
		for (j = 1; j < OLED_CMD_GAMMA_CNT; j++) {
			if (j == 1 || j == 3 || j == 5)
				k = dimming_info[i].gamma[j++] * 256;
			else
				k = 0;
			snprintf(temp + strnlen(temp, 256), 256, " %d", dimming_info[i].gamma[j] + k);
		}
		dsim_info("nit :%3d %s\n", dimming_info[i].br, temp);
	}
}

static ssize_t aid_log_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);

	dsim = container_of(priv, struct dsim_device, priv);

	show_aid_log(dsim);
	return strlen(buf);
}
#endif

static ssize_t manufacture_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X\n",
		priv->code[0], priv->code[1], priv->code[2], priv->code[3], priv->code[4]);

	return strlen(buf);
}

static ssize_t cell_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		priv->date[0] , priv->date[1], priv->date[2], priv->date[3], priv->date[4],
		priv->date[5], priv->date[6], (priv->coordinate[0] & 0xFF00) >> 8, priv->coordinate[0] & 0x00FF,
		(priv->coordinate[1]&0xFF00)>>8, priv->coordinate[1]&0x00FF);

	return strlen(buf);
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

		if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
			dsim_err("fail to write test on fc command.\n");
		if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
			dsim_err("fail to write test on f0 command.\n");
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
static DEVICE_ATTR(manufacture_code, 0444, manufacture_code_show, NULL);
static DEVICE_ATTR(cell_id, 0444, cell_id_show, NULL);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(read_mtp, 0664, read_mtp_show, read_mtp_store);
#ifdef CONFIG_PANEL_AID_DIMMING
static DEVICE_ATTR(aid_log, 0444, aid_log_show, NULL);
#endif
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

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_manufacture_code);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_cell_id);
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

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_temperature);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_color_coordinate);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_manufacture_date);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

#ifdef CONFIG_PANEL_AID_DIMMING
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_aid_log);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_read_mtp);
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

int mdnie_lite_send_seq(struct dsim_device *dsim, struct lcd_seq_info *seq, u32 num)
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

int mdnie_lite_read(struct dsim_device *dsim, u8 addr, u8 *buf, u32 size)
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
	dsim_info("%s probe done\n", __FILE__);
probe_err:
	return ret;
}

static int dsim_panel_displayon(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if (panel->state == PANEL_STATE_SUSPENED) {
		panel->state = PANEL_STATE_RESUMED;
		if (panel->ops->init) {
			ret = panel->ops->init(dsim);
			if (ret) {
				dsim_err("%s : failed to panel init\n", __func__);
				panel->state = PANEL_STATE_SUSPENED;
				goto displayon_err;
			}
		}
	}

	dsim_panel_set_brightness(dsim, 1);

	if (panel->ops->displayon) {
		ret = panel->ops->displayon(dsim);
		if (ret) {
			dsim_err("%s : failed to panel display on\n", __func__);
			goto displayon_err;
		}
	}

displayon_err:
	return ret;
}

static int dsim_panel_suspend(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

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
	return ret;
}

static int dsim_panel_resume(struct dsim_device *dsim)
{
	int ret = 0;
	return ret;
}


struct mipi_dsim_lcd_driver s6e3fa3_mipi_lcd_driver = {
	.early_probe = dsim_panel_early_probe,
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
	.resume		= dsim_panel_resume,
};



#ifdef CONFIG_PANEL_AID_DIMMING
static const unsigned char *HBM_TABLE[HBM_STATUS_MAX] = {SEQ_HBM_OFF, SEQ_HBM_ON};
static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {SEQ_ACL_OFF, SEQ_ACL_15};
static const unsigned char *ACL_OPR_TABLE[ACL_OPR_MAX] = {SEQ_ACL_OFF_OPR_AVR, SEQ_ACL_ON_OPR_AVR};

static const unsigned int br_tbl[256] = {
	2,		2,		2,		2,		2,		2,		2,		3,		3,		3,		3,		4,		4,		4,		5,		5,
	5,		6,		6,		7,		7,		8,		8,		9,		9,		10,		10,		11,		11,		12,		13,		13,
	14,		14,		15,		16,		16,		17,		18,		19,		19,		20,		21,		21,		22,		23,		24,		25,
	25,		26,		27,		28,		29,		30,		30,		31,		32,		33,		34,		35,		36,		37,		38,		39,
	40,		41,		42,		43,		44,		45,		46,		47,		48,		49,		50,		51,		52,		53,		54,		55,
	56,		58,		59,		60,		61,		62,		63,		65,		66,		67,		68,		69,		70,		72,		73,		74,
	75,		77,		78,		79,		80,		82,		83,		84,		86,		87,		88,		90,		91,		92,		94,		95,
	96,		98,		99,		101,	102,	103,	105,	106,	108,	109,	111,	112,	113,	115,	116,	118,
	119,	121,	122,	124,	125,	127,	128,	130,	131,	133,	135,	136,	138,	139,	141,	142,
	144,	146,	147,	149,	151,	152,	154,	155,	157,	159,	160,	162,	164,	165,	167,	169,
	171,	172,	174,	176,	177,	179,	181,	183,	184,	186,	188,	190,	191,	193,	195,	197,
	199,	200,	202,	204,	206,	208,	210,	211,	213,	215,	217,	219,	221,	223,	225,	227,
	228,	230,	232,	234,	236,	238,	240,	242,	244,	246,	248,	250,	252,	254,	256,	258,
	260,	262,	264,	266,	268,	270,	272,	274,	276,	278,	280,	282,	284,	286,	288,	290,
	292,	295,	297,	299,	301,	303,	305,	307,	309,	312,	314,	316,	318,	320,	322,	324,
	327,	329,	331,	333,	335,	338,	340,	342,	344,	347,	349,	350,	350,	360,	360,	360,
};

static const short center_gamma[NUM_VREF][CI_MAX] = {
	{0x000, 0x000, 0x000},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x080, 0x080, 0x080},
	{0x100, 0x100, 0x100},
};

struct SmtDimInfo daisy_dimming_info[MAX_BR_INFO + 1] = {				/* add hbm array */
	{.br = 2, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl2nit, .cTbl = ctbl2nit, .aid = aid9845, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 3, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl3nit, .cTbl = ctbl3nit, .aid = aid9768, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 4, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl4nit, .cTbl = ctbl4nit, .aid = aid9685, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 5, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl5nit, .cTbl = ctbl5nit, .aid = aid9597, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 6, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl6nit, .cTbl = ctbl6nit, .aid = aid9520, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 7, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl7nit, .cTbl = ctbl7nit, .aid = aid9437, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 8, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl8nit, .cTbl = ctbl8nit, .aid = aid9354, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 9, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl9nit, .cTbl = ctbl9nit, .aid = aid9272, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 10, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl10nit, .cTbl = ctbl10nit, .aid = aid9189, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 11, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl11nit, .cTbl = ctbl11nit, .aid = aid9096, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 12, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl12nit, .cTbl = ctbl12nit, .aid = aid8982, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 13, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl13nit, .cTbl = ctbl13nit, .aid = aid8931, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 14, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl14nit, .cTbl = ctbl14nit, .aid = aid8802, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 15, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl15nit, .cTbl = ctbl15nit, .aid = aid8714, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 16, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl16nit, .cTbl = ctbl16nit, .aid = aid8626, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 17, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl17nit, .cTbl = ctbl17nit, .aid = aid8574, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 19, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl19nit, .cTbl = ctbl19nit, .aid = aid8414, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 20, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl20nit, .cTbl = ctbl20nit, .aid = aid8270, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 21, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl21nit, .cTbl = ctbl21nit, .aid = aid8192, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 22, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl22nit, .cTbl = ctbl22nit, .aid = aid8115, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 24, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl24nit, .cTbl = ctbl24nit, .aid = aid7944, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 25, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl25nit, .cTbl = ctbl25nit, .aid = aid7862, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 27, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl27nit, .cTbl = ctbl27nit, .aid = aid7639, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 29, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl29nit, .cTbl = ctbl29nit, .aid = aid7531, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 30, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl30nit, .cTbl = ctbl30nit, .aid = aid7454, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 32, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl32nit, .cTbl = ctbl32nit, .aid = aid7309, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 34, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl34nit, .cTbl = ctbl34nit, .aid = aid7133, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 37, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl37nit, .cTbl = ctbl37nit, .aid = aid6865, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 39, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl39nit, .cTbl = ctbl39nit, .aid = aid6730, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 41, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl41nit, .cTbl = ctbl41nit, .aid = aid6560, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 44, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl44nit, .cTbl = ctbl44nit, .aid = aid6312, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 47, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl47nit, .cTbl = ctbl47nit, .aid = aid6059, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 50, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl50nit, .cTbl = ctbl50nit, .aid = aid5790, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 53, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl53nit, .cTbl = ctbl53nit, .aid = aid5496, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 56, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl56nit, .cTbl = ctbl56nit, .aid = aid5238, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 60, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl60nit, .cTbl = ctbl60nit, .aid = aid4892, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 64, .refBr = 113, .cGma = gma2p15, .rTbl = rtbl64nit, .cTbl = ctbl64nit, .aid = aid4809, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 68, .refBr = 116, .cGma = gma2p15, .rTbl = rtbl68nit, .cTbl = ctbl68nit, .aid = aid4246, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 72, .refBr = 123, .cGma = gma2p15, .rTbl = rtbl72nit, .cTbl = ctbl72nit, .aid = aid4246, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 77, .refBr = 131, .cGma = gma2p15, .rTbl = rtbl77nit, .cTbl = ctbl77nit, .aid = aid4246, .elvAcl = delvAcl0, .elv = delv0},
	{.br = 82, .refBr = 139, .cGma = gma2p15, .rTbl = rtbl82nit, .cTbl = ctbl82nit, .aid = aid4246, .elvAcl = delvAcl1, .elv = delv1},
	{.br = 87, .refBr = 148, .cGma = gma2p15, .rTbl = rtbl87nit, .cTbl = ctbl87nit, .aid = aid4246, .elvAcl = delvAcl1, .elv = delv1},
	{.br = 93, .refBr = 158, .cGma = gma2p15, .rTbl = rtbl93nit, .cTbl = ctbl93nit, .aid = aid4246, .elvAcl = delvAcl1, .elv = delv1},
	{.br = 98, .refBr = 166, .cGma = gma2p15, .rTbl = rtbl98nit, .cTbl = ctbl98nit, .aid = aid4246, .elvAcl = delvAcl2, .elv = delv2},
	{.br = 105, .refBr = 177, .cGma = gma2p15, .rTbl = rtbl105nit, .cTbl = ctbl105nit, .aid = aid4246, .elvAcl = delvAcl2, .elv = delv2},
	{.br = 111, .refBr = 186, .cGma = gma2p15, .rTbl = rtbl111nit, .cTbl = ctbl111nit, .aid = aid4246, .elvAcl = delvAcl2, .elv = delv2},
	{.br = 119, .refBr = 198, .cGma = gma2p15, .rTbl = rtbl119nit, .cTbl = ctbl119nit, .aid = aid4246, .elvAcl = delvAcl3, .elv = delv3},
	{.br = 126, .refBr = 208, .cGma = gma2p15, .rTbl = rtbl126nit, .cTbl = ctbl126nit, .aid = aid4246, .elvAcl = delvAcl3, .elv = delv3},
	{.br = 134, .refBr = 220, .cGma = gma2p15, .rTbl = rtbl134nit, .cTbl = ctbl134nit, .aid = aid4246, .elvAcl = delvAcl4, .elv = delv4},
	{.br = 143, .refBr = 234, .cGma = gma2p15, .rTbl = rtbl143nit, .cTbl = ctbl143nit, .aid = aid4246, .elvAcl = delvAcl4, .elv = delv4},
	{.br = 152, .refBr = 247, .cGma = gma2p15, .rTbl = rtbl152nit, .cTbl = ctbl152nit, .aid = aid4246, .elvAcl = delvAcl5, .elv = delv5},
	{.br = 162, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl162nit, .cTbl = ctbl162nit, .aid = aid3807, .elvAcl = delvAcl5, .elv = delv5},
	{.br = 172, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl172nit, .cTbl = ctbl172nit, .aid = aid3404, .elvAcl = delvAcl5, .elv = delv5},
	{.br = 183, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl183nit, .cTbl = ctbl183nit, .aid = aid2841, .elvAcl = delvAcl5, .elv = delv5},
	{.br = 195, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl195nit, .cTbl = ctbl195nit, .aid = aid2319, .elvAcl = delvAcl5, .elv = delv5},
	{.br = 207, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl207nit, .cTbl = ctbl207nit, .aid = aid1772, .elvAcl = delvAcl6, .elv = delv6},
	{.br = 220, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl220nit, .cTbl = ctbl220nit, .aid = aid1193, .elvAcl = delvAcl6, .elv = delv6},
	{.br = 234, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl234nit, .cTbl = ctbl234nit, .aid = aid0584, .elvAcl = delvAcl6, .elv = delv6},
	{.br = 249, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl249nit, .cTbl = ctbl249nit, .aid = aid0093, .elvAcl = delvAcl7, .elv = delv7},		/* 249 ~ 360nit acl off */
	{.br = 265, .refBr = 265, .cGma = gma2p15, .rTbl = rtbl265nit, .cTbl = ctbl265nit, .aid = aid0093, .elvAcl = delvAcl7, .elv = delv7},
	{.br = 282, .refBr = 282, .cGma = gma2p15, .rTbl = rtbl282nit, .cTbl = ctbl282nit, .aid = aid0093, .elvAcl = delvAcl8, .elv = delv8},
	{.br = 300, .refBr = 300, .cGma = gma2p15, .rTbl = rtbl300nit, .cTbl = ctbl300nit, .aid = aid0093, .elvAcl = delvAcl9, .elv = delv9},
	{.br = 316, .refBr = 316, .cGma = gma2p15, .rTbl = rtbl316nit, .cTbl = ctbl316nit, .aid = aid0093, .elvAcl = delvAcl9, .elv = delv9},
	{.br = 333, .refBr = 333, .cGma = gma2p15, .rTbl = rtbl333nit, .cTbl = ctbl333nit, .aid = aid0093, .elvAcl = delvAcl10, .elv = delv10},
	{.br = 360, .refBr = 360, .cGma = gma2p20, .rTbl = rtbl360nit, .cTbl = ctbl360nit, .aid = aid0093, .elvAcl = delvAcl11, .elv = delv11},
	{.br = 360, .refBr = 360, .cGma = gma2p20, .rTbl = rtbl360nit, .cTbl = ctbl360nit, .aid = aid0093, .elvAcl = delvAcl11, .elv = delvAcl11},
};

static int init_dimming(struct dsim_device *dsim, u8 *mtp)
{
	int i, j;
	int pos = 0;
	int ret = 0;
	short temp;
	struct dim_data *dimming;
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *diminfo = NULL;

	dimming = kzalloc(sizeof(struct dim_data), GFP_KERNEL);
	if (!dimming) {
		dsim_err("failed to allocate memory for dim data\n");
		ret = -ENOMEM;
		goto error;
	}
	diminfo = daisy_dimming_info;

	panel->dim_data = (void *)dimming;
	panel->dim_info = (void *)diminfo;

	panel->br_tbl = (unsigned int *)br_tbl;
	panel->hbm_tbl = (unsigned char **)HBM_TABLE;
	panel->acl_cutoff_tbl = (unsigned char **)ACL_CUTOFF_TABLE;
	panel->acl_opr_tbl = (unsigned char **)ACL_OPR_TABLE;

	for (j = 0; j < CI_MAX; j++) {
		temp = ((mtp[pos] & 0x01) ? -1 : 1) * mtp[pos+1];
		dimming->t_gamma[V255][j] = (int)center_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
		pos += 2;
	}

	for (i = V203; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((mtp[pos] & 0x80) ? -1 : 1) * (mtp[pos] & 0x7f);
			dimming->t_gamma[i][j] = (int)center_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
		}
	}

	/* for vt */
	temp = (mtp[pos+1]) << 8 | mtp[pos];

	for (i = 0; i < CI_MAX; i++)
		dimming->vt_mtp[i] = (temp >> (i*4)) & 0x0f;

#ifdef SMART_DIMMING_DEBUG
	dimm_info("Center Gamma Info :\n");
	for (i = 0; i < VMAX; i++) {
		dsim_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE],
			dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE]);
	}
#endif
	dimm_info("VT MTP :\n");
	dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE],
			dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE]);

	dimm_info("MTP Info :\n");
	for (i = 0; i < VMAX; i++) {
		dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE],
			dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE]);
	}

	ret = generate_volt_table(dimming);
	if (ret) {
		dimm_err("[ERR:%s] failed to generate volt table\n", __func__);
		goto error;
	}

	for (i = 0; i < MAX_BR_INFO - 1; i++) {
		ret = cal_gamma_from_index(dimming, &diminfo[i]);
		if (ret) {
			dsim_err("failed to calculate gamma : index : %d\n", i);
			goto error;
		}
	}
	memcpy(diminfo[i].gamma, SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET));
error:
	return ret;

}
#endif

static int s6e3fa3_read_init_info(struct dsim_device *dsim, unsigned char *mtp)
{
	int i = 0;
	int ret;
	struct panel_private *panel = &dsim->priv;
	unsigned char bufForCoordi[S6E3FA3_COORDINATE_LEN] = {0,};
	unsigned char buf[S6E3FA3_MTP_DATE_SIZE] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);

	/* id */
	ret = dsim_read_hl_data(dsim, S6E3FA3_ID_REG, S6E3FA3_ID_LEN, dsim->priv.id);
	if (ret != S6E3FA3_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < S6E3FA3_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	/* mtp */
	ret = dsim_read_hl_data(dsim, S6E3FA3_MTP_ADDR, S6E3FA3_MTP_DATE_SIZE, buf);
	if (ret != S6E3FA3_MTP_DATE_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(mtp, buf, S6E3FA3_MTP_SIZE);
	memcpy(dsim->priv.date, &buf[40], ARRAY_SIZE(dsim->priv.date));
	dsim_info("READ MTP SIZE : %d\n", S6E3FA3_MTP_SIZE);
	dsim_info("=========== MTP INFO ===========\n");
	for (i = 0; i < S6E3FA3_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

	/* coordinate */
	ret = dsim_read_hl_data(dsim, S6E3FA3_COORDINATE_REG, S6E3FA3_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E3FA3_COORDINATE_LEN) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];	/* X */
	dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];	/* Y */
	dsim_info("READ coordi : ");
	for (i = 0; i < 2; i++)
		dsim_info("%d, ", dsim->priv.coordinate[i]);
	dsim_info("\n");

	/* code */
	ret = dsim_read_hl_data(dsim, S6E3FA3_CODE_REG, S6E3FA3_CODE_LEN, dsim->priv.code);
	if (ret != S6E3FA3_CODE_LEN) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ code : ");
	for (i = 0; i < S6E3FA3_CODE_LEN; i++)
		dsim_info("%x, ", dsim->priv.code[i]);
	dsim_info("\n");

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto read_fail;
	}
read_exit:
	return 0;

read_fail:
	return -ENODEV;

}

static int s6e3fa3_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[S6E3FA3_MTP_SIZE] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = LCD_S6E3FA3;

	ret = s6e3fa3_read_init_info(dsim, mtp);
	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}

#ifdef CONFIG_PANEL_AID_DIMMING
	ret = init_dimming(dsim, mtp);
	if (ret)
		dsim_err("%s : failed to generate gamma table\n", __func__);
#endif
probe_exit:
	return ret;

}


static int s6e3fa3_displayon(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
		goto displayon_err;
	}

displayon_err:
	return ret;

}

static int s6e3fa3_exit(struct dsim_device *dsim)
{
	int ret = 0;
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

static int s6e3fa3_init(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}

	msleep(20);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
		goto init_exit;
	}

	msleep(120);

	ret = dsim_write_hl_data(dsim, SEQ_TE_OUT, ARRAY_SIZE(SEQ_TE_OUT));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TE_OUT\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

init_exit:
	return ret;
}


struct dsim_panel_ops s6e3fa3_panel_ops = {
	.early_probe = NULL,
	.probe		= s6e3fa3_probe,
	.displayon	= s6e3fa3_displayon,
	.exit		= s6e3fa3_exit,
	.init		= s6e3fa3_init,
};

