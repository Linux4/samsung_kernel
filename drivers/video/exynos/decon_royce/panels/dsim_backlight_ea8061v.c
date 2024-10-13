/* linux/drivers/video/exynos_decon/panel/dsim_backlight.c
 *
 * Header file for Samsung MIPI-DSI Backlight driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#include <linux/backlight.h>

#include "../dsim.h"
#include "dsim_backlight.h"
#include "panel_info.h"

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#endif


#ifdef CONFIG_PANEL_AID_DIMMING

static unsigned int get_actual_br_value(struct dsim_device *dsim, int index, int pi)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info[pi];

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
static unsigned char *get_gamma_from_index(struct dsim_device *dsim, int index, int pi)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info[pi];

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

static unsigned char *get_aid_from_index(struct dsim_device *dsim, int index, int pi)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info[pi];

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

static unsigned char *get_elvss_from_index(struct dsim_device *dsim, int index, int caps, int pi)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info[pi];

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_elvess_err;
	}

	if(caps)
		return (unsigned char *)dimming_info[index].elvCaps;
	else
		return (unsigned char *)dimming_info[index].elv;

get_elvess_err:
	return NULL;
}


static void dsim_panel_gamma_ctrl(struct dsim_device *dsim, int pi)
{
	u8 *gamma = NULL;
	gamma = get_gamma_from_index(dsim, dsim->priv.br_index, pi);
	if (gamma == NULL) {
		dsim_err("%s :faied to get gamma\n", __func__);
		return;
	}

	if (dsim_write_hl_data(dsim, gamma, GAMMA_CMD_CNT) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);
}

static void dsim_panel_aid_ctrl(struct dsim_device *dsim, int pi)
{
	u8 *aid = NULL;
	aid = get_aid_from_index(dsim, dsim->priv.br_index, pi);
	if (aid == NULL) {
		dsim_err("%s : faield to get aid value\n", __func__);
		return;
	}
	if (dsim_write_hl_data(dsim, aid, AID_CMD_CNT) < 0)
		dsim_err("%s : failed to write aid\n", __func__);
}

static void dsim_panel_set_elvss(struct dsim_device *dsim, int pi)
{
	u8 *elvss = NULL;
	unsigned char SEQ_ELVSS[ELVSS_CMD_CNT];

	elvss = get_elvss_from_index(dsim, dsim->priv.br_index, dsim->priv.caps_enable, pi);
	if (elvss == NULL) {
		dsim_err("%s : failed to get elvss value\n", __func__);
		return;
	}
	memcpy(SEQ_ELVSS, elvss, ELVSS_CMD_CNT);
	if (UNDER_0(dsim->priv.temperature))
		SEQ_ELVSS[ELVSS_CMD_CNT - 1] = elvss[ELVSS_CMD_CNT];

	if (dsim_write_hl_data(dsim, SEQ_ELVSS, ELVSS_CMD_CNT) < 0)
		dsim_err("%s : failed to write elvss \n", __func__);
}


static int dsim_panel_set_acl(struct dsim_device *dsim, int force, int pi)
{
	int ret = 0, level = ACL_STATUS_15P;
	struct panel_private *panel = &dsim->priv;

	if (panel == NULL) {
			dsim_err("%s : panel is NULL\n", __func__);
			goto exit;
	}

	if (dsim->priv.siop_enable || LEVEL_IS_HBM(dsim->priv.bd->props.brightness))  // auto acl or hbm is acl on
		goto acl_update;

	if (!dsim->priv.acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if(force || dsim->priv.current_acl != panel->acl_cutoff_tbl[level][1]) {
		if((ret = dsim_write_hl_data(dsim,  panel->acl_opr_tbl[level], 2)) < 0) {
			dsim_err("fail to write acl opr command.\n");
			goto exit;
		}
		if((ret = dsim_write_hl_data(dsim, panel->acl_cutoff_tbl[level], 2)) < 0) {
			dsim_err("fail to write acl command.\n");
			goto exit;
		}
		dsim->priv.current_acl = panel->acl_cutoff_tbl[level][1];
		dsim_info("acl: %d, brightness: %d\n", dsim->priv.current_acl, dsim->priv.bd->props.brightness);
	}
exit:
	if (!ret)
		ret = -EPERM;
	return ret;
}


static int dsim_panel_set_tset(struct dsim_device *dsim, int force, int pi)
{
	int ret = 0;
	int tset = 0;
	unsigned char SEQ_TSET[TSET_LEN] = {TSET_REG, };

	tset = (dsim->priv.temperature < 0) ? BIT(7) | abs(dsim->priv.temperature) : dsim->priv.temperature;

	if(force || dsim->priv.tset[pi][TSET_LEN - 2] != tset) {
		dsim->priv.tset[pi][TSET_LEN - 2] = SEQ_TSET[TSET_LEN - 1] = tset;
		if ((ret = dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET))) < 0) {
			dsim_err("fail to write tset command.\n");
			ret = -EPERM;
		}
		dsim_info("%s temperature: %d, tset: %d\n",
			__func__, dsim->priv.temperature, SEQ_TSET[TSET_LEN - 1]);
	}
	return ret;
}

static int dsim_panel_set_hbm(struct dsim_device *dsim, int force, int pi)
{
	int ret = 0, p_br;
	bool bIsHbm;
	struct panel_private *panel = &dsim->priv;
	unsigned char SEQ_HBM[2] = {ELVSS_REG, };

	if (panel == NULL) {
		dsim_err("%s : panel is NULL\n", __func__);
		goto exit;
	}
	p_br = panel->bd->props.brightness;
	bIsHbm = LEVEL_IS_HBM(dsim->priv.bd->props.brightness);

	//B6h: the 4th para.
	if (bIsHbm)
		SEQ_HBM[1] = panel->hbm_tbl[HBM_STATUS_ON][pi]; // Display Lab confirm: HBM does not need to check temperature, it's smaller than 0xA
	else {
		if (UNDER_MINUS_20(dsim->priv.temperature))
			SEQ_HBM[1] = panel->hbm_tbl[HBM_STATUS_OFF][pi] - 0xA;
		else
			SEQ_HBM[1] = panel->hbm_tbl[HBM_STATUS_OFF][pi];
	}

	dsim_info("hbm: %d, hbm_tbl: %d, bIsHbm: %d\n", dsim->priv.current_hbm, SEQ_HBM[1], bIsHbm);

	if(force || dsim->priv.current_hbm != SEQ_HBM[1]) {
		dsim->priv.current_hbm = SEQ_HBM[1];
		if((ret = dsim_write_hl_data(dsim, EA8061V_SEQ_HBM_PARA_SKIP, ARRAY_SIZE(EA8061V_SEQ_HBM_PARA_SKIP))) < 0) {
			dsim_err("fail to write skip command.\n");
			ret = -EPERM;
		}

		if((ret = dsim_write_hl_data(dsim, SEQ_HBM, ARRAY_SIZE(SEQ_HBM))) < 0) {
			dsim_err("fail to write hbm command.\n");
			ret = -EPERM;
		}
		dsim_dbg("hbm: %d, brightness: %d\n", dsim->priv.current_hbm, dsim->priv.bd->props.brightness);
	}
exit:
	return ret;
}

static int low_level_set_brightness(struct dsim_device *dsim ,int force, int pi)
{

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1)) < 0)
		dsim_err("%s : fail to write F1 on command.\n", __func__);

	dsim_panel_set_tset(dsim, force, pi);

	dsim_panel_gamma_ctrl(dsim, pi);

	dsim_panel_aid_ctrl(dsim, pi);

	dsim_panel_set_elvss(dsim, pi);

	dsim_panel_set_hbm(dsim, force, pi);

	dsim_panel_set_acl(dsim, force, pi);

	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
		dsim_err("%s : failed to write gamma update\n", __func__);

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F1, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F1)) < 0)
		dsim_err("%s : fail to write F1 off command\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("%s : fail to write F0 off command.\n", __func__);

	return 0;
}

static int get_acutal_br_index(struct dsim_device *dsim, int br, int pi)
{
	int i;
	int min;
	int gap;
	int index = 0;
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = panel->dim_info[pi];

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

int dsim_panel_set_brightness(struct dsim_device *dsim, int force)
{
	int ret = 0;
#ifndef CONFIG_PANEL_AID_DIMMING
	dsim_info("%s:this panel does not support dimming \n", __func__);
#else
	struct dim_data *dimming;
	struct panel_private *panel = &dsim->priv;
	int p_br = panel->bd->props.brightness;
	int acutal_br = 0;
	int real_br = 0;
	int prev_index = panel->br_index;
	int pi = get_panel_index_init();

	dimming = (struct dim_data *)panel->dim_data[pi];
	if ((dimming == NULL) || (panel->br_tbl == NULL)) {
		dsim_info("%s : this panel does not support dimming\n", __func__);
		return ret;
	}
	dsim_info("%s +\n",__func__);

	acutal_br = panel->br_tbl[p_br];
	panel->br_index = get_acutal_br_index(dsim, acutal_br, pi);
	real_br = get_actual_br_value(dsim, panel->br_index, pi);
	panel->caps_enable = CAPS_IS_ON(real_br);
	panel->acl_enable = ACL_IS_ON(real_br);

	if(panel->siop_enable)					// check auto acl
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

	ret = low_level_set_brightness(dsim, force, pi);
	if (ret) {
		dsim_err("%s failed to set brightness : %d\n", __func__, acutal_br);
	}
	mutex_unlock(&panel->lock);

set_br_exit:
#endif
	dsim_info("%s -\n",__func__);
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

	if (brightness < UI_MIN_BRIGHTNESS || brightness > EXTEND_BRIGHTNESS) {
		pr_alert("Brightness %d is out of range\n", brightness);
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


int dsim_backlight_probe(struct dsim_device *dsim)
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
