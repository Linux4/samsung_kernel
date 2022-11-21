/*
 * drivers/video/decon_7580/panels/s6e3fa2_lcd_ctrl.c
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
#include "../decon.h"
#include "dsim_panel.h"

#include "s6e3fa2_param.h"

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#include "dimming_core.h"
#include "s6e3fa2_aid_dimming.h"
#endif

#define LCD_S6E3FA2 0
#define LCD_EA8064G 1



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


struct mipi_dsim_lcd_driver s6e3fa2_mipi_lcd_driver = {
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
	327,	329,	331,	333,	335,	338,	340,	342,	344,	347,	349,	350,	350,	350,	350,	350,
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
	{.br = 2, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl2nit, .cTbl = ctbl2nit, .aid = aid9793, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 3, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl3nit, .cTbl = ctbl3nit, .aid = aid9711, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 4, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl4nit, .cTbl = ctbl4nit, .aid = aid9633, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 5, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl5nit, .cTbl = ctbl5nit, .aid = aid9551, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 6, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl6nit, .cTbl = ctbl6nit, .aid = aid9468, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 7, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl7nit, .cTbl = ctbl7nit, .aid = aid9385, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 8, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl8nit, .cTbl = ctbl8nit, .aid = aid9303, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 9, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl9nit, .cTbl = ctbl9nit, .aid = aid9215, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 10, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl10nit, .cTbl = ctbl10nit, .aid = aid9127, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 11, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl11nit, .cTbl = ctbl11nit, .aid = aid9044, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 12, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl12nit, .cTbl = ctbl12nit, .aid = aid8967, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 13, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl13nit, .cTbl = ctbl13nit, .aid = aid8879, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 14, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl14nit, .cTbl = ctbl14nit, .aid = aid8786, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 15, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl15nit, .cTbl = ctbl15nit, .aid = aid8704, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 16, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl16nit, .cTbl = ctbl16nit, .aid = aid8611, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 17, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl17nit, .cTbl = ctbl17nit, .aid = aid8528, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 19, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl19nit, .cTbl = ctbl19nit, .aid = aid8363, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 20, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl20nit, .cTbl = ctbl20nit, .aid = aid8177, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 21, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl21nit, .cTbl = ctbl21nit, .aid = aid8135, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 22, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl22nit, .cTbl = ctbl22nit, .aid = aid8053, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 24, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl24nit, .cTbl = ctbl24nit, .aid = aid7929, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 25, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl25nit, .cTbl = ctbl25nit, .aid = aid7846, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 27, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl27nit, .cTbl = ctbl27nit, .aid = aid7665, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 29, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl29nit, .cTbl = ctbl29nit, .aid = aid7495, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 30, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl30nit, .cTbl = ctbl30nit, .aid = aid7397, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 32, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl32nit, .cTbl = ctbl32nit, .aid = aid7221, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 34, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl34nit, .cTbl = ctbl34nit, .aid = aid7040, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 37, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl37nit, .cTbl = ctbl37nit, .aid = aid6761, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 39, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl39nit, .cTbl = ctbl39nit, .aid = aid6586, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 41, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl41nit, .cTbl = ctbl41nit, .aid = aid6374, .elvAcl = elvAcl4, .elv = elv4},
	{.br = 44, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl44nit, .cTbl = ctbl44nit, .aid = aid6105, .elvAcl = elvAcl4, .elv = elv4},
	{.br = 47, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl47nit, .cTbl = ctbl47nit, .aid = aid5795, .elvAcl = elvAcl3, .elv = elv3},
	{.br = 50, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl50nit, .cTbl = ctbl50nit, .aid = aid5511, .elvAcl = elvAcl3, .elv = elv3},
	{.br = 53, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl53nit, .cTbl = ctbl53nit, .aid = aid5191, .elvAcl = elvAcl2, .elv = elv2},
	{.br = 56, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl56nit, .cTbl = ctbl56nit, .aid = aid4861, .elvAcl = elvAcl1, .elv = elv1},
	{.br = 60, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl60nit, .cTbl = ctbl60nit, .aid = aid4411, .elvAcl = elvAcl0, .elv = elv0},
	{.br = 64, .refBr = 115, .cGma = gma2p15, .rTbl = rtbl64nit, .cTbl = ctbl64nit, .aid = aid4179, .elvAcl = elvAcl0, .elv = elv0},
	{.br = 68, .refBr = 121, .cGma = gma2p15, .rTbl = rtbl68nit, .cTbl = ctbl68nit, .aid = aid4179, .elvAcl = elvAcl0, .elv = elv0},
	{.br = 72, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl72nit, .cTbl = ctbl72nit, .aid = aid4179, .elvAcl = elvAcl0, .elv = elv0},
	{.br = 77, .refBr = 136, .cGma = gma2p15, .rTbl = rtbl77nit, .cTbl = ctbl77nit, .aid = aid4179, .elvAcl = elvAcl0, .elv = elv0},
	{.br = 82, .refBr = 143, .cGma = gma2p15, .rTbl = rtbl82nit, .cTbl = ctbl82nit, .aid = aid4179, .elvAcl = elvAcl1, .elv = elv1},
	{.br = 87, .refBr = 151, .cGma = gma2p15, .rTbl = rtbl87nit, .cTbl = ctbl87nit, .aid = aid4179, .elvAcl = elvAcl1, .elv = elv1},
	{.br = 93, .refBr = 161, .cGma = gma2p15, .rTbl = rtbl93nit, .cTbl = ctbl93nit, .aid = aid4179, .elvAcl = elvAcl1, .elv = elv1},
	{.br = 98, .refBr = 169, .cGma = gma2p15, .rTbl = rtbl98nit, .cTbl = ctbl98nit, .aid = aid4179, .elvAcl = elvAcl2, .elv = elv2},
	{.br = 105, .refBr = 180, .cGma = gma2p15, .rTbl = rtbl105nit, .cTbl = ctbl105nit, .aid = aid4179, .elvAcl = elvAcl2, .elv = elv2},
	{.br = 111, .refBr = 189, .cGma = gma2p15, .rTbl = rtbl111nit, .cTbl = ctbl111nit, .aid = aid4179, .elvAcl = elvAcl3, .elv = elv3},
	{.br = 119, .refBr = 200, .cGma = gma2p15, .rTbl = rtbl119nit, .cTbl = ctbl119nit, .aid = aid4179, .elvAcl = elvAcl3, .elv = elv3},
	{.br = 126, .refBr = 213, .cGma = gma2p15, .rTbl = rtbl126nit, .cTbl = ctbl126nit, .aid = aid4179, .elvAcl = elvAcl4, .elv = elv4},
	{.br = 134, .refBr = 224, .cGma = gma2p15, .rTbl = rtbl134nit, .cTbl = ctbl134nit, .aid = aid4179, .elvAcl = elvAcl4, .elv = elv4},
	{.br = 143, .refBr = 236, .cGma = gma2p15, .rTbl = rtbl143nit, .cTbl = ctbl143nit, .aid = aid4179, .elvAcl = elvAcl5, .elv = elv5},
	{.br = 152, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl152nit, .cTbl = ctbl152nit, .aid = aid4179, .elvAcl = elvAcl6, .elv = elv6},
	{.br = 162, .refBr = 245, .cGma = gma2p15, .rTbl = rtbl162nit, .cTbl = ctbl162nit, .aid = aid3626, .elvAcl = elvAcl7, .elv = elv7},
	{.br = 172, .refBr = 245, .cGma = gma2p15, .rTbl = rtbl172nit, .cTbl = ctbl172nit, .aid = aid3177, .elvAcl = elvAcl7, .elv = elv7},
	{.br = 183, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl183nit, .cTbl = ctbl183nit, .aid = aid2794, .elvAcl = elvAcl7, .elv = elv7},
	{.br = 195, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl195nit, .cTbl = ctbl195nit, .aid = aid2299, .elvAcl = elvAcl7, .elv = elv7},
	{.br = 207, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl207nit, .cTbl = ctbl207nit, .aid = aid1725, .elvAcl = elvAcl7, .elv = elv7},
	{.br = 220, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl220nit, .cTbl = ctbl220nit, .aid = aid1105, .elvAcl = elvAcl7, .elv = elv7},
	{.br = 234, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl234nit, .cTbl = ctbl234nit, .aid = aid0429, .elvAcl = elvAcl7, .elv = elv7},
	{.br = 249, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl249nit, .cTbl = ctbl249nit, .aid = aid0072, .elvAcl = elvAcl7, .elv = elv7},		/* 249 ~ 360nit acl off */
	{.br = 265, .refBr = 265, .cGma = gma2p15, .rTbl = rtbl265nit, .cTbl = ctbl265nit, .aid = aid0072, .elvAcl = elvAcl8, .elv = elv8},
	{.br = 282, .refBr = 282, .cGma = gma2p15, .rTbl = rtbl282nit, .cTbl = ctbl282nit, .aid = aid0072, .elvAcl = elvAcl9, .elv = elv9},
	{.br = 300, .refBr = 300, .cGma = gma2p15, .rTbl = rtbl300nit, .cTbl = ctbl300nit, .aid = aid0072, .elvAcl = elvAcl10, .elv = elv10},
	{.br = 316, .refBr = 316, .cGma = gma2p15, .rTbl = rtbl316nit, .cTbl = ctbl316nit, .aid = aid0072, .elvAcl = elvAcl10, .elv = elv10},
	{.br = 333, .refBr = 333, .cGma = gma2p15, .rTbl = rtbl333nit, .cTbl = ctbl333nit, .aid = aid0072, .elvAcl = elvAcl11, .elv = elv11},
	{.br = 350, .refBr = 350, .cGma = gma2p20, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid0072, .elvAcl = elvAcl12, .elv = elv12},
	{.br = 350, .refBr = 350, .cGma = gma2p20, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid0072, .elvAcl = elvAcl12, .elv = elv12},
};

struct SmtDimInfo EA8064G_daisy_dimming_info_[MAX_BR_INFO + 1] = {				/* add hbm array */
	{.br = 2, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl2nit, .cTbl = ctbl2nit, .aid = aid9793_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 3, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl3nit, .cTbl = ctbl3nit, .aid = aid9711_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 4, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl4nit, .cTbl = ctbl4nit, .aid = aid9633_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 5, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl5nit, .cTbl = ctbl5nit, .aid = aid9551_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 6, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl6nit, .cTbl = ctbl6nit, .aid = aid9468_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 7, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl7nit, .cTbl = ctbl7nit, .aid = aid9385_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 8, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl8nit, .cTbl = ctbl8nit, .aid = aid9303_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 9, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl9nit, .cTbl = ctbl9nit, .aid = aid9215_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 10, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl10nit, .cTbl = ctbl10nit, .aid = aid9127_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 11, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl11nit, .cTbl = ctbl11nit, .aid = aid9044_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 12, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl12nit, .cTbl = ctbl12nit, .aid = aid8967_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 13, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl13nit, .cTbl = ctbl13nit, .aid = aid8879_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 14, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl14nit, .cTbl = ctbl14nit, .aid = aid8786_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 15, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl15nit, .cTbl = ctbl15nit, .aid = aid8704_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 16, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl16nit, .cTbl = ctbl16nit, .aid = aid8611_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 17, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl17nit, .cTbl = ctbl17nit, .aid = aid8528_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 19, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl19nit, .cTbl = ctbl19nit, .aid = aid8363_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 20, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl20nit, .cTbl = ctbl20nit, .aid = aid8177_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 21, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl21nit, .cTbl = ctbl21nit, .aid = aid8135_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 22, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl22nit, .cTbl = ctbl22nit, .aid = aid8053_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 24, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl24nit, .cTbl = ctbl24nit, .aid = aid7929_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 25, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl25nit, .cTbl = ctbl25nit, .aid = aid7846_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 27, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl27nit, .cTbl = ctbl27nit, .aid = aid7665_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 29, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl29nit, .cTbl = ctbl29nit, .aid = aid7495_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 30, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl30nit, .cTbl = ctbl30nit, .aid = aid7397_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 32, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl32nit, .cTbl = ctbl32nit, .aid = aid7221_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 34, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl34nit, .cTbl = ctbl34nit, .aid = aid7040_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 37, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl37nit, .cTbl = ctbl37nit, .aid = aid6761_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 39, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl39nit, .cTbl = ctbl39nit, .aid = aid6586_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 41, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl41nit, .cTbl = ctbl41nit, .aid = aid6374_ea, .elvAcl = elvAcl4_ea, .elv = elv4_ea},
	{.br = 44, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl44nit, .cTbl = ctbl44nit, .aid = aid6105_ea, .elvAcl = elvAcl4_ea, .elv = elv4_ea},
	{.br = 47, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl47nit, .cTbl = ctbl47nit, .aid = aid5795_ea, .elvAcl = elvAcl3_ea, .elv = elv3_ea},
	{.br = 50, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl50nit, .cTbl = ctbl50nit, .aid = aid5511_ea, .elvAcl = elvAcl3_ea, .elv = elv3_ea},
	{.br = 53, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl53nit, .cTbl = ctbl53nit, .aid = aid5191_ea, .elvAcl = elvAcl2_ea, .elv = elv2_ea},
	{.br = 56, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl56nit, .cTbl = ctbl56nit, .aid = aid4861_ea, .elvAcl = elvAcl1_ea, .elv = elv1_ea},
	{.br = 60, .refBr = 110, .cGma = gma2p15, .rTbl = rtbl60nit, .cTbl = ctbl60nit, .aid = aid4411_ea, .elvAcl = elvAcl0_ea, .elv = elv0_ea},
	{.br = 64, .refBr = 115, .cGma = gma2p15, .rTbl = rtbl64nit, .cTbl = ctbl64nit, .aid = aid4179_ea, .elvAcl = elvAcl0_ea, .elv = elv0_ea},
	{.br = 68, .refBr = 121, .cGma = gma2p15, .rTbl = rtbl68nit, .cTbl = ctbl68nit, .aid = aid4179_ea, .elvAcl = elvAcl0_ea, .elv = elv0_ea},
	{.br = 72, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl72nit, .cTbl = ctbl72nit, .aid = aid4179_ea, .elvAcl = elvAcl0_ea, .elv = elv0_ea},
	{.br = 77, .refBr = 136, .cGma = gma2p15, .rTbl = rtbl77nit, .cTbl = ctbl77nit, .aid = aid4179_ea, .elvAcl = elvAcl0_ea, .elv = elv0_ea},
	{.br = 82, .refBr = 143, .cGma = gma2p15, .rTbl = rtbl82nit, .cTbl = ctbl82nit, .aid = aid4179_ea, .elvAcl = elvAcl1_ea, .elv = elv1_ea},
	{.br = 87, .refBr = 151, .cGma = gma2p15, .rTbl = rtbl87nit, .cTbl = ctbl87nit, .aid = aid4179_ea, .elvAcl = elvAcl1_ea, .elv = elv1_ea},
	{.br = 93, .refBr = 161, .cGma = gma2p15, .rTbl = rtbl93nit, .cTbl = ctbl93nit, .aid = aid4179_ea, .elvAcl = elvAcl1_ea, .elv = elv1_ea},
	{.br = 98, .refBr = 169, .cGma = gma2p15, .rTbl = rtbl98nit, .cTbl = ctbl98nit, .aid = aid4179_ea, .elvAcl = elvAcl2_ea, .elv = elv2_ea},
	{.br = 105, .refBr = 180, .cGma = gma2p15, .rTbl = rtbl105nit, .cTbl = ctbl105nit, .aid = aid4179_ea, .elvAcl = elvAcl2_ea, .elv = elv2_ea},
	{.br = 111, .refBr = 189, .cGma = gma2p15, .rTbl = rtbl111nit, .cTbl = ctbl111nit, .aid = aid4179_ea, .elvAcl = elvAcl3_ea, .elv = elv3_ea},
	{.br = 119, .refBr = 200, .cGma = gma2p15, .rTbl = rtbl119nit, .cTbl = ctbl119nit, .aid = aid4179_ea, .elvAcl = elvAcl3_ea, .elv = elv3_ea},
	{.br = 126, .refBr = 213, .cGma = gma2p15, .rTbl = rtbl126nit, .cTbl = ctbl126nit, .aid = aid4179_ea, .elvAcl = elvAcl4_ea, .elv = elv4_ea},
	{.br = 134, .refBr = 224, .cGma = gma2p15, .rTbl = rtbl134nit, .cTbl = ctbl134nit, .aid = aid4179_ea, .elvAcl = elvAcl4_ea, .elv = elv4_ea},
	{.br = 143, .refBr = 236, .cGma = gma2p15, .rTbl = rtbl143nit, .cTbl = ctbl143nit, .aid = aid4179_ea, .elvAcl = elvAcl5_ea, .elv = elv5_ea},
	{.br = 152, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl152nit, .cTbl = ctbl152nit, .aid = aid4179_ea, .elvAcl = elvAcl6_ea, .elv = elv6_ea},
	{.br = 162, .refBr = 245, .cGma = gma2p15, .rTbl = rtbl162nit, .cTbl = ctbl162nit, .aid = aid3626_ea, .elvAcl = elvAcl7_ea, .elv = elv7_ea},
	{.br = 172, .refBr = 245, .cGma = gma2p15, .rTbl = rtbl172nit, .cTbl = ctbl172nit, .aid = aid3177_ea, .elvAcl = elvAcl7_ea, .elv = elv7_ea},
	{.br = 183, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl183nit, .cTbl = ctbl183nit, .aid = aid2794_ea, .elvAcl = elvAcl7_ea, .elv = elv7_ea},
	{.br = 195, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl195nit, .cTbl = ctbl195nit, .aid = aid2299_ea, .elvAcl = elvAcl7_ea, .elv = elv7_ea},
	{.br = 207, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl207nit, .cTbl = ctbl207nit, .aid = aid1725_ea, .elvAcl = elvAcl7_ea, .elv = elv7_ea},
	{.br = 220, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl220nit, .cTbl = ctbl220nit, .aid = aid1105_ea, .elvAcl = elvAcl7_ea, .elv = elv7_ea},
	{.br = 234, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl234nit, .cTbl = ctbl234nit, .aid = aid0429_ea, .elvAcl = elvAcl7_ea, .elv = elv7_ea},
	{.br = 249, .refBr = 249, .cGma = gma2p15, .rTbl = rtbl249nit, .cTbl = ctbl249nit, .aid = aid0072_ea, .elvAcl = elvAcl7_ea, .elv = elv7_ea},		/* 249 ~ 360nit acl off */
	{.br = 265, .refBr = 265, .cGma = gma2p15, .rTbl = rtbl265nit, .cTbl = ctbl265nit, .aid = aid0072_ea, .elvAcl = elvAcl8_ea, .elv = elv8_ea},
	{.br = 282, .refBr = 282, .cGma = gma2p15, .rTbl = rtbl282nit, .cTbl = ctbl282nit, .aid = aid0072_ea, .elvAcl = elvAcl9_ea, .elv = elv9_ea},
	{.br = 300, .refBr = 300, .cGma = gma2p15, .rTbl = rtbl300nit, .cTbl = ctbl300nit, .aid = aid0072_ea, .elvAcl = elvAcl10_ea, .elv = elv10_ea},
	{.br = 316, .refBr = 316, .cGma = gma2p15, .rTbl = rtbl316nit, .cTbl = ctbl316nit, .aid = aid0072_ea, .elvAcl = elvAcl10_ea, .elv = elv10_ea},
	{.br = 333, .refBr = 333, .cGma = gma2p15, .rTbl = rtbl333nit, .cTbl = ctbl333nit, .aid = aid0072_ea, .elvAcl = elvAcl11_ea, .elv = elv11_ea},
	{.br = 350, .refBr = 350, .cGma = gma2p20, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid0072_ea, .elvAcl = elvAcl12_ea, .elv = elv12_ea},
	{.br = 350, .refBr = 350, .cGma = gma2p20, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid0072_ea, .elvAcl = elvAcl12_ea, .elv = elv12_ea},
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

	if (dsim->priv.panel_type == LCD_EA8064G)
		diminfo = EA8064G_daisy_dimming_info_;
	else
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
		dimming->vt_mtp[i] = 0;

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

static int s6e3fa2_read_init_info(struct dsim_device *dsim, unsigned char *mtp)
{
	int i = 0;
	int ret;
	struct panel_private *panel = &dsim->priv;
	unsigned char bufForCoordi[S6E3FA2_COORDINATE_LEN] = {0,};
	unsigned char buf[S6E3FA2_MTP_DATE_SIZE] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);

	/* id */
	ret = dsim_read_hl_data(dsim, S6E3FA2_ID_REG, S6E3FA2_ID_LEN, dsim->priv.id);
	if (ret != S6E3FA2_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < S6E3FA2_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	if (dsim->priv.id[S6E3FA2_ID_LEN-1] >> 6) {
		dsim->priv.panel_type = LCD_EA8064G;
		dsim->lcd_info.ddi_type = TYPE_OF_MAGNA_DDI;

		ACL_OPR_TABLE[0] = SEQ_ACL_OFF_OPR_AVR_EA8064G;
		ACL_OPR_TABLE[1] = SEQ_ACL_ON_OPR_AVR_EA8064G;

		dsim_info("Panel : EA8064G (Magna Chip)\n");
	} else {
		dsim->priv.panel_type = LCD_S6E3FA2;
		dsim_info("Panel : S6E3FA2 (S.LSI Chip)\n");
	}

	/* mtp */
	ret = dsim_read_hl_data(dsim, S6E3FA2_MTP_ADDR, S6E3FA2_MTP_DATE_SIZE, buf);
	if (ret != S6E3FA2_MTP_DATE_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(mtp, buf, S6E3FA2_MTP_SIZE);
	memcpy(dsim->priv.date, &buf[40], ARRAY_SIZE(dsim->priv.date));
	dsim_info("READ MTP SIZE : %d\n", S6E3FA2_MTP_SIZE);
	dsim_info("=========== MTP INFO ===========\n");
	for (i = 0; i < S6E3FA2_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

	/* elvss */
	ret = dsim_read_hl_data(dsim, ELVSS_REG, ELVSS_LEN, dsim->priv.elvss);
	if (ret != ELVSS_LEN) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}

	/* coordinate */
	ret = dsim_read_hl_data(dsim, S6E3FA2_COORDINATE_REG, S6E3FA2_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E3FA2_COORDINATE_LEN) {
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
	ret = dsim_read_hl_data(dsim, S6E3FA2_CODE_REG, S6E3FA2_CODE_LEN, dsim->priv.code);
	if (ret != S6E3FA2_CODE_LEN) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ code : ");
	for (i = 0; i < S6E3FA2_CODE_LEN; i++)
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

static int s6e3fa2_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[S6E3FA2_MTP_SIZE] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;

	ret = s6e3fa2_read_init_info(dsim, mtp);
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


static int s6e3fa2_displayon(struct dsim_device *dsim)
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

static int s6e3fa2_exit(struct dsim_device *dsim)
{
	int ret = 0;
	dsim_info("MDD : %s was called\n", __func__);
	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
		goto exit_err;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
		goto exit_err;
	}

	msleep(120);

exit_err:
	return ret;
}

static int s6e3fa2_init(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	if (dsim->priv.panel_type == LCD_EA8064G) {
		ret = dsim_panel_set_brightness(dsim, 1);
		if (ret)
			dsim_err("%s : fail to set brightness\n", __func__);
	}

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

	if (dsim->priv.panel_type == LCD_EA8064G) {
		ret = dsim_write_hl_data(dsim, SEQ_SOURCE_CONTROL, ARRAY_SIZE(SEQ_SOURCE_CONTROL));
		if (ret < 0) {
			dsim_err(":%s fail to write CMD : SEQ_SOURCE_CONTROL\n", __func__);
			goto init_exit;
		}
	}

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


struct dsim_panel_ops s6e3fa2_panel_ops = {
	.early_probe = NULL,
	.probe		= s6e3fa2_probe,
	.displayon	= s6e3fa2_displayon,
	.exit		= s6e3fa2_exit,
	.init		= s6e3fa2_init,
};

