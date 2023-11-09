/*
 * drivers/video/decon_3475/panels/ea8061s_lcd_ctrl.c
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
#include <linux/interrupt.h>
#include <linux/lcd.h>
#include <linux/backlight.h>

#include "../dsim.h"
#include "../decon.h"
#include "dsim_panel.h"

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#include "dimming_core.h"
#include "ea8061s_aid_dimming.h"
#endif

#include "s6e88a0_param.h"


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

/*====== for ea8061s =====*/
static void dsim_panel_gamma_ctrl_ea8061s(struct dsim_device *dsim)
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

static void dsim_panel_aid_ctrl_ea8061s(struct dsim_device *dsim)
{
	u8 *aid = NULL;
	aid = get_aid_from_index(dsim, dsim->priv.br_index);
	if (aid == NULL) {
		dsim_err("%s : faield to get aid value\n", __func__);
		return;
	}
	if (dsim_write_hl_data(dsim, aid, AID_CMD_CNT_EA8061S) < 0)
		dsim_err("%s : failed to write gamma\n", __func__);
}

static void dsim_panel_set_elvss_ea8061s(struct dsim_device *dsim)
{
	u8 *elvss = NULL;
	u8 elvss_val[3] = {0, };

	u8 B6_GW[2] = {0xB6, };
	int real_br = 0;

	real_br = get_actual_br_value(dsim, dsim->priv.br_index);
	elvss = get_elvss_from_index(dsim, dsim->priv.br_index, dsim->priv.acl_enable);

	if (elvss == NULL) {
		dsim_err("%s : failed to get elvss value\n", __func__);
		return;
	}

	B6_GW[1] = dsim->priv.elvss_hbm_default;

	if (real_br <= 29) {
		if (dsim->priv.temperature <= -20)
			B6_GW[1] = elvss[5];
		else if (dsim->priv.temperature > -20 && dsim->priv.temperature <= 0)
			B6_GW[1] = elvss[4];
		else
			B6_GW[1] = elvss[3];
	}

	elvss_val[0] = elvss[0];
	elvss_val[1] = elvss[1];
	elvss_val[2] = elvss[2];

	if (dsim_write_hl_data(dsim, elvss_val, ELVSS_LEN) < 0)
		dsim_err("%s : failed to write elvss\n", __func__);

	if (dsim_write_hl_data(dsim, SEQ_ELVSS_GLOBAL_EA8061S, ARRAY_SIZE(SEQ_ELVSS_GLOBAL_EA8061S)) < 0)
		dsim_err("%s : failed to SEQ_ELVSS_GLOBAL\n", __func__);

	if (dsim_write_hl_data(dsim, B6_GW, 2) < 0)
		dsim_err("%s : failed to write elvss\n", __func__);
}

static int dsim_panel_set_acl_ea8061s(struct dsim_device *dsim, int force)
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

static int dsim_panel_set_tset_ea8061s(struct dsim_device *dsim, int force)
{
	int ret = 0;
	int tset = 0;
	unsigned char SEQ_TSET[EA8061S_TSET_LEN] = {TSET_REG, 0x19};

	tset = ((dsim->priv.temperature >= 0) ? 0 : BIT(7)) | abs(dsim->priv.temperature);

	if (force || dsim->priv.tset[0] != tset) {
		dsim->priv.tset[0] = SEQ_TSET[1] = tset;

		if (dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET)) < 0) {
			dsim_err("fail to write tset command.\n");
			ret = -EPERM;
		}

		dsim_info("temperature: %d, tset: %d\n", dsim->priv.temperature, SEQ_TSET[1]);
	}
	return ret;
}

static int dsim_panel_set_hbm_ea8061s(struct dsim_device *dsim, int force)
{
	int ret = 0, level = LEVEL_IS_HBM(dsim->priv.auto_brightness);
	struct panel_private *panel = &dsim->priv;

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

		if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_S6E88A0, ARRAY_SIZE(SEQ_GAMMA_UPDATE_S6E88A0)) < 0) {
			dsim_err("%s : fail to write SEQ_GAMMA_UPDATE_S6E88A0 on command.\n", __func__);
			ret = -EPERM;
		}

		dsim_info("hbm: %d, auto_brightness: %d\n", dsim->priv.current_hbm, dsim->priv.auto_brightness);
	}
exit:
	return ret;
}

static int low_level_set_brightness_ea8061s(struct dsim_device *dsim, int force)
{
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);

	dsim_panel_gamma_ctrl_ea8061s(dsim);

	dsim_panel_aid_ctrl_ea8061s(dsim);

	dsim_panel_set_elvss_ea8061s(dsim);

	dsim_panel_set_acl_ea8061s(dsim, force);

	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_S6E88A0, ARRAY_SIZE(SEQ_GAMMA_UPDATE_S6E88A0)) < 0)
			dsim_err("%s : fail to write SEQ_GAMMA_UPDATE_S6E88A0 on command.\n", __func__);

	dsim_panel_set_tset_ea8061s(dsim, force);

	dsim_panel_set_hbm_ea8061s(dsim, force);

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
	int ret = 0;

	struct dim_data *dimming;
	struct panel_private *panel = &dsim->priv;
	int p_br = panel->bd->props.brightness;
	int acutal_br = 0;
	int real_br = 0;
	int prev_index = panel->br_index;

	dimming = (struct dim_data *)panel->dim_data;
	if ((dimming == NULL) || (panel->br_tbl == NULL)) {
		dsim_info("%s : this panel does not support dimming\n", __func__);
		return ret;
	}

	acutal_br = panel->br_tbl[p_br];
	panel->br_index = get_acutal_br_index(dsim, acutal_br);
	real_br = get_actual_br_value(dsim, panel->br_index);
	panel->acl_enable = ACL_IS_ON(real_br);

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

	ret = low_level_set_brightness_ea8061s(dsim, force);
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
	panel->bd->props.state = 0;

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

	year = priv->date[0] + 2011;
	month = priv->date[1];
	day = priv->date[2];
	hour = priv->date[3];
	min = priv->date[3];

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
static DEVICE_ATTR(manufacture_code, 0444, manufacture_code_show, NULL);
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


#ifdef CONFIG_PANEL_AID_DIMMING
static const unsigned char *HBM_TABLE[HBM_STATUS_MAX] = {SEQ_HBM_OFF, SEQ_HBM_ON};
static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {SEQ_ACL_OFF, SEQ_ACL_15};
static const unsigned char *ACL_OPR_TABLE[ACL_OPR_MAX] = {SEQ_ACL_OFF_OPR_AVR, SEQ_ACL_ON_OPR_AVR};

static const unsigned int br_tbl[256] = {
	2, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 20, 21, 22,
	24, 25, 27, 29, 30, 32, 34, 37, 39, 41, 41, 44, 44, 47, 47, 50, 50, 53, 53, 56,
	56, 56, 60, 60, 60, 64, 64, 64, 68, 68, 68, 72, 72, 72, 72, 77, 77, 77, 82,
	82, 82, 82, 87, 87, 87, 87, 93, 93, 93, 93, 98, 98, 98, 98, 98, 105, 105,
	105, 105, 111, 111, 111, 111, 111, 111, 119, 119, 119, 119, 119, 126, 126, 126,
	126, 126, 126, 134, 134, 134, 134, 134, 134, 134, 143, 143, 143, 143, 143, 143,
	152, 152, 152, 152, 152, 152, 152, 152, 162, 162, 162, 162, 162, 162, 162, 172,
	172, 172, 172, 172, 172, 172, 172, 183, 183, 183, 183, 183, 183, 183, 183, 183,
	195, 195, 195, 195, 195, 195, 195, 195, 207, 207, 207, 207, 207, 207, 207, 207,
	207, 207, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 234, 234, 234, 234,
	234, 234, 234, 234, 234, 234, 234, 249, 249, 249, 249, 249, 249, 249, 249, 249,
	249, 249, 249, 265, 265, 265, 265, 265, 265, 265, 265, 265, 265, 265, 265, 282,
	282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 300, 300, 300, 300,
	300, 300, 300, 300, 300, 300, 300, 300, 316, 316, 316, 316, 316, 316, 316, 316,
	316, 316, 316, 316, 333, 333, 333, 333, 333, 333, 333, 333, 333, 333, 333, 333, 350,
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

static struct SmtDimInfo daisy_dimming_info_ea8061s[MAX_BR_INFO + 1] = {				/* add hbm array */
	{.br = 5, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl5nit_ea, .cTbl = ctbl5nit_ea, .aid = aid5_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 6, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl6nit_ea, .cTbl = ctbl6nit_ea, .aid = aid6_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 7, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl7nit_ea, .cTbl = ctbl7nit_ea, .aid = aid7_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 8, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl8nit_ea, .cTbl = ctbl8nit_ea, .aid = aid8_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 9, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl9nit_ea, .cTbl = ctbl9nit_ea, .aid = aid9_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 10, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl10nit_ea, .cTbl = ctbl10nit_ea, .aid = aid10_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 11, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl11nit_ea, .cTbl = ctbl11nit_ea, .aid = aid11_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 12, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl12nit_ea, .cTbl = ctbl12nit_ea, .aid = aid12_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 13, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl13nit_ea, .cTbl = ctbl13nit_ea, .aid = aid13_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 14, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl14nit_ea, .cTbl = ctbl14nit_ea, .aid = aid14_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 15, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl15nit_ea, .cTbl = ctbl15nit_ea, .aid = aid15_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 16, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl16nit_ea, .cTbl = ctbl16nit_ea, .aid = aid16_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 17, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl17nit_ea, .cTbl = ctbl17nit_ea, .aid = aid17_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 19, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl19nit_ea, .cTbl = ctbl19nit_ea, .aid = aid19_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 20, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl20nit_ea, .cTbl = ctbl20nit_ea, .aid = aid20_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 21, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl21nit_ea, .cTbl = ctbl21nit_ea, .aid = aid21_ea, .elvAcl = elv_ea_8D, .elv = elv_ea_8D},
	{.br = 22, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl22nit_ea, .cTbl = ctbl22nit_ea, .aid = aid22_ea, .elvAcl = elv_ea_8F, .elv = elv_ea_8F},
	{.br = 24, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl24nit_ea, .cTbl = ctbl24nit_ea, .aid = aid24_ea, .elvAcl = elv_ea_91, .elv = elv_ea_91},
	{.br = 25, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl25nit_ea, .cTbl = ctbl25nit_ea, .aid = aid25_ea, .elvAcl = elv_ea_93, .elv = elv_ea_93},
	{.br = 27, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl27nit_ea, .cTbl = ctbl27nit_ea, .aid = aid27_ea, .elvAcl = elv_ea_95, .elv = elv_ea_95},
	{.br = 29, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl29nit_ea, .cTbl = ctbl29nit_ea, .aid = aid29_ea, .elvAcl = elv_ea_97, .elv = elv_ea_97},
	{.br = 30, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl30nit_ea, .cTbl = ctbl30nit_ea, .aid = aid30_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 32, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl32nit_ea, .cTbl = ctbl32nit_ea, .aid = aid32_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 34, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl34nit_ea, .cTbl = ctbl34nit_ea, .aid = aid34_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 37, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl37nit_ea, .cTbl = ctbl37nit_ea, .aid = aid37_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 39, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl39nit_ea, .cTbl = ctbl39nit_ea, .aid = aid39_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 41, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl41nit_ea, .cTbl = ctbl41nit_ea, .aid = aid41_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 44, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl44nit_ea, .cTbl = ctbl44nit_ea, .aid = aid44_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 47, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl47nit_ea, .cTbl = ctbl47nit_ea, .aid = aid47_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 50, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl50nit_ea, .cTbl = ctbl50nit_ea, .aid = aid50_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 53, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl53nit_ea, .cTbl = ctbl53nit_ea, .aid = aid53_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 56, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl56nit_ea, .cTbl = ctbl56nit_ea, .aid = aid56_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 60, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl60nit_ea, .cTbl = ctbl60nit_ea, .aid = aid60_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 64, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl64nit_ea, .cTbl = ctbl64nit_ea, .aid = aid64_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 68, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl68nit_ea, .cTbl = ctbl68nit_ea, .aid = aid68_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 72, .refBr = 128, .cGma = gma2p15, .rTbl = rtbl72nit_ea, .cTbl = ctbl72nit_ea, .aid = aid72_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 77, .refBr = 137, .cGma = gma2p15, .rTbl = rtbl77nit_ea, .cTbl = ctbl77nit_ea, .aid = aid72_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 82, .refBr = 146, .cGma = gma2p15, .rTbl = rtbl82nit_ea, .cTbl = ctbl82nit_ea, .aid = aid72_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 87, .refBr = 153, .cGma = gma2p15, .rTbl = rtbl87nit_ea, .cTbl = ctbl87nit_ea, .aid = aid72_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 93, .refBr = 163, .cGma = gma2p15, .rTbl = rtbl93nit_ea, .cTbl = ctbl93nit_ea, .aid = aid72_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 98, .refBr = 171, .cGma = gma2p15, .rTbl = rtbl98nit_ea, .cTbl = ctbl98nit_ea, .aid = aid72_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 105, .refBr = 183, .cGma = gma2p15, .rTbl = rtbl105nit_ea, .cTbl = ctbl105nit_ea, .aid = aid72_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 111, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl111nit_ea, .cTbl = ctbl111nit_ea, .aid = aid72_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 119, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl119nit_ea, .cTbl = ctbl119nit_ea, .aid = aid119_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 126, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl126nit_ea, .cTbl = ctbl126nit_ea, .aid = aid126_ea, .elvAcl = elv_ea_98, .elv = elv_ea_98},
	{.br = 134, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl134nit_ea, .cTbl = ctbl134nit_ea, .aid = aid134_ea, .elvAcl = elv_ea_97, .elv = elv_ea_97},
	{.br = 143, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl143nit_ea, .cTbl = ctbl143nit_ea, .aid = aid143_ea, .elvAcl = elv_ea_97, .elv = elv_ea_97},
	{.br = 152, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl152nit_ea, .cTbl = ctbl152nit_ea, .aid = aid152_ea, .elvAcl = elv_ea_96, .elv = elv_ea_96},
	{.br = 162, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl162nit_ea, .cTbl = ctbl162nit_ea, .aid = aid162_ea, .elvAcl = elv_ea_96, .elv = elv_ea_96},
	{.br = 172, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl172nit_ea, .cTbl = ctbl172nit_ea, .aid = aid172_ea, .elvAcl = elv_ea_95, .elv = elv_ea_95},
	{.br = 183, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl183nit_ea, .cTbl = ctbl183nit_ea, .aid = aid183_ea, .elvAcl = elv_ea_95, .elv = elv_ea_95},
	{.br = 195, .refBr = 206, .cGma = gma2p15, .rTbl = rtbl195nit_ea, .cTbl = ctbl195nit_ea, .aid = aid183_ea, .elvAcl = elv_ea_94, .elv = elv_ea_94},
	{.br = 207, .refBr = 217, .cGma = gma2p15, .rTbl = rtbl207nit_ea, .cTbl = ctbl207nit_ea, .aid = aid183_ea, .elvAcl = elv_ea_93, .elv = elv_ea_93},
	{.br = 220, .refBr = 229, .cGma = gma2p15, .rTbl = rtbl220nit_ea, .cTbl = ctbl220nit_ea, .aid = aid183_ea, .elvAcl = elv_ea_92, .elv = elv_ea_92},
	{.br = 234, .refBr = 243, .cGma = gma2p15, .rTbl = rtbl234nit_ea, .cTbl = ctbl234nit_ea, .aid = aid183_ea, .elvAcl = elv_ea_92, .elv = elv_ea_92},
	{.br = 249, .refBr = 256, .cGma = gma2p15, .rTbl = rtbl249nit_ea, .cTbl = ctbl249nit_ea, .aid = aid183_ea, .elvAcl = elv_ea_91, .elv = elv_ea_91},
	{.br = 265, .refBr = 272, .cGma = gma2p15, .rTbl = rtbl265nit_ea, .cTbl = ctbl265nit_ea, .aid = aid183_ea, .elvAcl = elv_ea_90, .elv = elv_ea_90},
	{.br = 282, .refBr = 288, .cGma = gma2p15, .rTbl = rtbl282nit_ea, .cTbl = ctbl282nit_ea, .aid = aid183_ea, .elvAcl = elv_ea_8F, .elv = elv_ea_8F},
	{.br = 300, .refBr = 304, .cGma = gma2p15, .rTbl = rtbl300nit_ea, .cTbl = ctbl300nit_ea, .aid = aid183_ea, .elvAcl = elv_ea_8E, .elv = elv_ea_8E},
	{.br = 316, .refBr = 320, .cGma = gma2p15, .rTbl = rtbl316nit_ea, .cTbl = ctbl316nit_ea, .aid = aid183_ea, .elvAcl = elv_ea_8D, .elv = elv_ea_8D},
	{.br = 333, .refBr = 334, .cGma = gma2p15, .rTbl = rtbl333nit_ea, .cTbl = ctbl333nit_ea, .aid = aid183_ea, .elvAcl = elv_ea_8C, .elv = elv_ea_8C},
	{.br = 350, .refBr = 350, .cGma = gma2p15, .rTbl = rtbl350nit_ea, .cTbl = ctbl350nit_ea, .aid = aid183_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
	{.br = 350, .refBr = 350, .cGma = gma2p15, .rTbl = rtbl350nit_ea, .cTbl = ctbl350nit_ea, .aid = aid183_ea, .elvAcl = elv_ea_8B, .elv = elv_ea_8B},
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

	dimming = kmalloc(sizeof(struct dim_data), GFP_KERNEL);
	if (!dimming) {
		dsim_err("failed to allocate memory for dim data\n");
		ret = -ENOMEM;
		goto error;
	}

	diminfo = daisy_dimming_info_ea8061s;
	dsim_info("%s : ea8061s\n", __func__);

	panel->dim_data = (void *)dimming;
	panel->dim_info = (void *)diminfo; /* dimming info */

	panel->br_tbl = (unsigned int *)br_tbl; /* backlight table */
	panel->hbm_tbl = (unsigned char **)HBM_TABLE; /* command hbm on and off */
	panel->acl_cutoff_tbl = (unsigned char **)ACL_CUTOFF_TABLE; /* ACL on and off command */
	panel->acl_opr_tbl = (unsigned char **)ACL_OPR_TABLE;

	/* CENTER GAMMA V255 */
	for (j = 0; j < CI_MAX; j++) {
		temp = ((mtp[pos] & 0x01) ? -1 : 1) * mtp[pos+1];
		dimming->t_gamma[V255][j] = (int)center_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
		pos += 2;
	}

	/* CENTER GAMME FOR V3 ~ V203 */
	for (i = V203; i > V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((mtp[pos] & 0x80) ? -1 : 1) * (mtp[pos] & 0x7f);
			dimming->t_gamma[i][j] = (int)center_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
		}
	}

	/* CENTER GAMMA FOR V0 */
	for (j = 0; j < CI_MAX; j++) {
		dimming->t_gamma[V0][j] = (int)center_gamma[V0][j] + temp;
		dimming->mtp[V0][j] = 0;
	}

	for (j = 0; j < CI_MAX; j++) {
		dimming->vt_mtp[j] = mtp[pos];
		pos++;
	}

	/* Center gamma */
	dimm_info("Center Gamma Info :\n");
	for (i = 0; i < VMAX; i++) {
		dsim_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE],
			dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE]);
	}


	dimm_info("VT MTP :\n");
	dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE],
			dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE]);

	/* MTP value get from ddi */
	dimm_info("MTP Info from ddi:\n");
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

static int ea8061s_read_init_info(struct dsim_device *dsim, unsigned char *mtp)
{
	int i = 0;
	int ret;
	struct panel_private *panel = &dsim->priv;
	unsigned char bufForDate[EA8061S_DATE_LEN] = {0,};
	unsigned char buf[EA8061S_MTP_DATE_SIZE] = {0, };
	unsigned char buf_HBM[HBM_GAMMA_READ_SIZE_EA8061S] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);

	/* id */
	ret = dsim_read_hl_data(dsim, EA8061S_ID_REG, EA8061S_ID_LEN, dsim->priv.id);
	if (ret != EA8061S_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < EA8061S_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	/* mtp */
	ret = dsim_read_hl_data(dsim, EA8061S_MTP_ADDR, EA8061S_MTP_DATE_SIZE, buf);
	if (ret != EA8061S_MTP_DATE_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(mtp, buf, EA8061S_MTP_SIZE);
	dsim_info("READ MTP SIZE : %d\n", EA8061S_MTP_SIZE);
	dsim_info("=========== MTP INFO ===========\n");
	for (i = 0; i < EA8061S_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

	/* date & coordinate */
	ret = dsim_read_hl_data(dsim, EA8061S_DATE_REG, EA8061S_DATE_LEN, bufForDate);
	if (ret != EA8061S_DATE_LEN) {
		dsim_err("fail to read date on command.\n");
		goto read_fail;
	}
	dsim->priv.date[0] = bufForDate[4] >> 4;
	dsim->priv.date[1] = bufForDate[4] & 0x0f;
	dsim->priv.date[2] = bufForDate[5];

	dsim->priv.coordinate[0] = bufForDate[0] << 8 | bufForDate[1];	/* X */
	dsim->priv.coordinate[1] = bufForDate[2] << 8 | bufForDate[3];	/* Y */

	/* Read HBM from B6h */
	ret = dsim_read_hl_data(dsim, HBM_GAMMA_READ_ADD_EA8061S, HBM_GAMMA_READ_SIZE_EA8061S, buf_HBM);
	if (ret != HBM_GAMMA_READ_SIZE_EA8061S) {
		dsim_err("fail to read HBM from B6h.\n");
		goto read_fail;
	}

	/* Read default 0xB6 8th value. */
	dsim->priv.elvss_hbm_default = buf_HBM[7];

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

static int ea8061s_displayon(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
		goto displayon_err;
	}

	usleep_range(12000, 13000);

displayon_err:
	return ret;
}

static int ea8061s_exit(struct dsim_device *dsim)
{
	int ret = 0;
	dsim_info("MDD : %s was called\n", __func__);
	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
		goto exit_err;
	}

	if (dsim->id)
		msleep(20);

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
		goto exit_err;
	}

	msleep(120);

exit_err:
	return ret;
}

static int ea8061s_init(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called : ea8061s\n", __func__);
	usleep_range(5000, 6000);

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

	/* common setting */

	ret = dsim_write_hl_data(dsim, SEQ_SOURCE_SLEW_EA8061S, ARRAY_SIZE(SEQ_SOURCE_SLEW_EA8061S));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SOURCE_SLEW_EA8061S\n", __func__);
		goto init_exit;
	}


	ret = dsim_write_hl_data(dsim, SEQ_AID_SET_EA8061S, ARRAY_SIZE(SEQ_AID_SET_EA8061S));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_AID_SET_EA8061S\n", __func__);
		goto init_exit;
	}


	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_S6E88A0, ARRAY_SIZE(SEQ_GAMMA_UPDATE_S6E88A0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_GAMMA_UPDATE_S6E88A0\n", __func__);
		goto init_exit;
	}


	ret = dsim_write_hl_data(dsim, SEQ_S_WIRE_EA8061S, ARRAY_SIZE(SEQ_S_WIRE_EA8061S));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S_WIRE_EA8061S\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}

	msleep(20);

	/* 1. Brightness setting */

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_AID_SET_EA8061S, ARRAY_SIZE(SEQ_AID_SET_EA8061S));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_AID_SET_EA8061S\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_ELVSS_SET_EA8061S, ARRAY_SIZE(SEQ_ELVSS_SET_EA8061S));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ELVSS_SET_EA8061S\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_ACL_SET_EA8061S, ARRAY_SIZE(SEQ_ACL_SET_EA8061S));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ACL_SET_EA8061S\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_S6E88A0, ARRAY_SIZE(SEQ_GAMMA_UPDATE_S6E88A0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ACL_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TSET_EA8061S, ARRAY_SIZE(SEQ_TSET_EA8061S));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSET\n", __func__);
		goto init_exit;
	}

	msleep(120);

	ret = dsim_write_hl_data(dsim, SEQ_MDNIE_1_EA8061S, ARRAY_SIZE(SEQ_MDNIE_1_EA8061S));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_MDNIE_1_EA8061S\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_MDNIE_2_EA8061S, ARRAY_SIZE(SEQ_MDNIE_2_EA8061S));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_MDNIE_2\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
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

static int ea8061s_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[S6E88A0_MTP_SIZE] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;

	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}

	ea8061s_read_init_info(dsim, mtp);

	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}

#ifdef CONFIG_PANEL_AID_DIMMING
	ret = init_dimming(dsim, mtp);
	if (ret)
		dsim_err("%s : failed to generate gamma tablen\n", __func__);
#endif

	dsim_panel_set_brightness(dsim, 1);

probe_exit:
	return ret;
}

struct dsim_panel_ops ea8061s_panel_ops = {
	.early_probe = NULL,
	.probe		= ea8061s_probe,
	.displayon	= ea8061s_displayon,
	.exit		= ea8061s_exit,
	.init		= ea8061s_init,
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

struct mipi_dsim_lcd_driver ea8061s_mipi_lcd_driver = {
	.early_probe = dsim_panel_early_probe,
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
	.resume		= dsim_panel_resume,
};

