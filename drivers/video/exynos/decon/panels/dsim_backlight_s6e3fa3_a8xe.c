/* linux/drivers/video/exynos/decon/panel/dsim_backlight_s6e3fa3_a8xe.c
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

unsigned int get_actual_br_value(struct dsim_device *dsim, int index)
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

static unsigned char *get_elvss_from_index(struct dsim_device *dsim, int index, int caps)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_elvess_err;
	}

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

	if (dsim_write_hl_data(dsim, gamma, OLED_CMD_GAMMA_CNT) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);
}

#ifdef AID_INTERPOLATION
int get_acutal_br_index(struct dsim_device *dsim, int br)
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

static void dsim_panel_dynamic_aid_ctrl(struct dsim_device *dsim)
{
	u8 aid[S6E3FA3_AID_CMD_CNT] = { 0, };
	int current_pbr;
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto aid_ctrl_err;
	}

	if (panel->inter_aor_tbl == NULL) {
		dsim_err("%s : interpolation aor table is NULL\n", __func__);
		goto aid_ctrl_err;
	}

	memcpy(aid, SEQ_AID_SETTING, sizeof(SEQ_AID_SETTING));

	current_pbr = panel->bd->props.brightness;
	current_pbr = (current_pbr > EXTEND_BRIGHTNESS) ? EXTEND_BRIGHTNESS : current_pbr;

	aid[1] = panel->inter_aor_tbl[current_pbr * 2];
	aid[2] = panel->inter_aor_tbl[current_pbr * 2 + 1];

	dsim_info("%s %d aid : 0x%02x : 0x%02x : 0x%02x\n", __func__, current_pbr, aid[0], aid[1], aid[2]);

	if (dsim_write_hl_data(dsim, aid, S6E3FA3_AID_CMD_CNT) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);

aid_ctrl_err:
	return;
}
static void dsim_panel_aid_interpolation(struct dsim_device *dsim)
{
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);

	dsim_panel_dynamic_aid_ctrl(dsim);

	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);

	return;
}


#endif

#ifndef AID_INTERPOLATION
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


static void dsim_panel_aid_ctrl(struct dsim_device *dsim)
{
	u8 *aid = NULL;
	unsigned char SEQ_AID[S6E3FA3_AID_CMD_CNT];

	aid = get_aid_from_index(dsim, dsim->priv.br_index);
	if (aid == NULL) {
		dsim_err("%s : failed to get elvss value\n", __func__);
		return;
	}
	SEQ_AID[0] = aid[0];
	memcpy(&SEQ_AID[1], dsim->priv.aid, S6E3FA3_AID_CMD_CNT - 1);	// HA2/HF3 not read. but no danger
	memcpy(&SEQ_AID[S6E3FA3_AID_REG_OFFSET], aid + 1, S6E3FA3_AID_CMD_CNT - S6E3FA3_AID_REG_OFFSET + 1);

	if (dsim_write_hl_data(dsim, SEQ_AID, S6E3FA3_AID_CMD_CNT) < 0)
		dsim_err("%s : failed to write aid \n", __func__);

}
#endif

static int dsim_panel_set_tset(struct dsim_device *dsim, int force)
{
	int ret = 0;
	int tset = 0;
	unsigned char SEQ_TSET[S6E3FA3_TSET_LEN] = {0, };

	SEQ_TSET[0] = S6E3FA3_TSET_REG;
	tset = (dsim->priv.temperature < 0) ? BIT(7) | abs(dsim->priv.temperature) : dsim->priv.temperature;

	if(force || dsim->priv.tset[S6E3FA3_TSET_LEN - 2] != tset) {
		memcpy(&SEQ_TSET[1], dsim->priv.tset, S6E3FA3_TSET_LEN - 1);
		SEQ_TSET[S6E3FA3_TSET_LEN - 1] = tset;
		if ((ret = dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET))) < 0) {
			dsim_err("fail to write tset command.\n");
			ret = -EPERM;
		}
		dsim->priv.tset[S6E3FA3_TSET_LEN - 2] = SEQ_TSET[S6E3FA3_TSET_LEN - 1];
		dsim_info("%s temperature: %d, tset: %d\n",
			__func__, dsim->priv.temperature, SEQ_TSET[S6E3FA3_TSET_LEN - 1]);
	}
	return ret;
}

static void dsim_panel_set_elvss(struct dsim_device *dsim)
{
	u8 *elvss = NULL;
	unsigned char ELVSS[S6E3FA3_ELVSS_LEN] = {0, };
	struct panel_private *panel = &dsim->priv;
	int br_nit;

	elvss = get_elvss_from_index(dsim, dsim->priv.br_index, dsim->priv.caps_enable);
	if (elvss == NULL) {
		dsim_err("%s : failed to get elvss value\n", __func__);
		return;
	}
	memcpy(&ELVSS[1], dsim->priv.elvss_set, sizeof(dsim->priv.elvss_set));
	memcpy(ELVSS, elvss, S6E3FA3_ELVSS_CMD_CNT);

	br_nit = get_actual_br_value(dsim, panel->br_index);
	if (!br_nit) {
		dsim_err("%s : failed to get brightness value\n", __func__);
		return;
	}
	if(br_nit > S6E3FA3_MAX_BRIGHTNESS && br_nit < S6E3FA3_HBM_BRIGHTNESS) {
		ELVSS[S6E3FA3_ELVSS_LEN - 1] = dsim->priv.hbm_elvss;
	}
	else if(br_nit <= 15) {
		ELVSS[S6E3FA3_ELVSS_LEN - 1] = 0x0D;
	}

	if (dsim_write_hl_data(dsim, ELVSS, S6E3FA3_ELVSS_LEN) < 0)
		dsim_err("%s : failed to write elvss \n", __func__);

}


static int dsim_panel_set_acl(struct dsim_device *dsim, int force)
{
	int ret = 0, level, enabled;
	int br_nit;
	struct panel_private *panel = &dsim->priv;

	if (panel == NULL) {
			dsim_err("%s : panel is NULL\n", __func__);
			goto exit;
	}

	br_nit = get_actual_br_value(dsim, panel->br_index);
	if (!br_nit) {
		dsim_err("%s : failed to get brightness value\n", __func__);
		br_nit = 2;
	}

	enabled = panel->acl_enable;
	level = (enabled == ACL_STATUS_OFF) ? OPR_STATUS_ACL_OFF : OPR_STATUS_ACL_ON;

	if (panel->siop_enable) {
		// siop = weak temperature acl on
		enabled = ACL_STATUS_ON;
		level = OPR_STATUS_ACL_ON;
	}
	else if (br_nit > S6E3FA3_MAX_BRIGHTNESS) {
		// hbm interpolation and hbm
		enabled = ACL_STATUS_ON;
		level = OPR_STATUS_ACL_HBM;
	}

	if(force || panel->current_acl != level) {
		if((ret = dsim_write_hl_data(dsim,  panel->acl_opr_tbl[level], S6E3FA3_ACL_OPR_LEN)) < 0) {
			dsim_err("fail to write acl opr command.\n");
			goto exit;
		}
		if((ret = dsim_write_hl_data(dsim, panel->acl_cutoff_tbl[enabled], S6E3FA3_ACL_CMD_LEN)) < 0) {
			dsim_err("fail to write acl command.\n");
			goto exit;
		}
		panel->current_acl = level;
		dsim_info("acl level: %d(0x%02x 0x%02x), enabled: %d(0x%02x)\n",
			level, panel->acl_opr_tbl[level][S6E3FA3_ACL_OPR_IDX_START], panel->acl_opr_tbl[level][S6E3FA3_ACL_OPR_IDX_RATIO],
			enabled, panel->acl_cutoff_tbl[enabled][S6E3FA3_ACL_CMD_LEN - 1]);
	}
exit:
	if (!ret)
		ret = -EPERM;
	return ret;
}

static int dsim_panel_set_vint(struct dsim_device *dsim, int force)
{
	int ret = 0;
	int nit = 0;
	int i, level = 0;
	int arraySize = ARRAY_SIZE(VINT_DIM_TABLE);
	struct panel_private* panel = &(dsim->priv);
	unsigned char SEQ_VINT[S6E3FA3_VINT_LEN] = {S6E3FA3_VINT_REG, S6E3FA3_VINT_1ST, 0x1E};
	unsigned char *vint_tbl = (unsigned char *)VINT_TABLE;

	level = arraySize - 1;

	if(panel->temperature < -20)
		goto set_vint;
#ifdef CONFIG_LCD_HMT
	if(panel->hmt_on == HMT_ON)
		goto set_vint;
#endif
	nit = get_actual_br_value(dsim, panel->br_index);

	for (i = 0; i < arraySize; i++) {
		if (nit <= VINT_DIM_TABLE[i]) {
			level = i;
			SEQ_VINT[S6E3FA3_VINT_LEN - 1] = vint_tbl[level];
			goto set_vint;
		}
	}
set_vint:
	if(force || panel->current_vint != SEQ_VINT[S6E3FA3_VINT_LEN - 1]) {
		if ((ret = dsim_write_hl_data(dsim, SEQ_VINT, ARRAY_SIZE(SEQ_VINT))) < 0) {
			dsim_err("fail to write vint command.\n");
			ret = -EPERM;
		}
		panel->current_vint = SEQ_VINT[S6E3FA3_VINT_LEN - 1];
		dsim_info("vint: %02x\n", panel->current_vint);
	}
	return ret;
}

static int dsim_panel_set_hbm(struct dsim_device *dsim, int force)
{
	int ret = 0, level = LEVEL_IS_HBM(dsim->priv.bd->props.brightness);
	struct panel_private *panel = &dsim->priv;

	if (panel == NULL) {
		dsim_err("%s : panel is NULL\n", __func__);
		goto exit;
	}
#ifdef CONFIG_LCD_DOZE_MODE
	if(panel->curr_alpm_mode != ALPM_OFF) {
		dsim_info("panel is ALPM/HLPM mode. skip hbm command.\n");
		goto exit;
	}
#endif
	if(force || panel->current_hbm != panel->hbm_tbl[level][1]) {
		panel->current_hbm = panel->hbm_tbl[level][1];
		if((ret = dsim_write_hl_data(dsim, panel->hbm_tbl[level], ARRAY_SIZE(SEQ_HBM_OFF))) < 0) {
			dsim_err("fail to write hbm command.\n");
			ret = -EPERM;
		}
		dsim_info("hbm: %d, brightness: %d\n", panel->current_hbm, panel->bd->props.brightness);
	}
exit:
	return ret;
}

static int low_level_set_brightness(struct dsim_device *dsim ,int force)
{

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);

	dsim_panel_gamma_ctrl(dsim);

#ifdef AID_INTERPOLATION
	dsim_panel_dynamic_aid_ctrl(dsim);
#else
	dsim_panel_aid_ctrl(dsim);
#endif

	dsim_panel_set_elvss(dsim);

	dsim_panel_set_vint(dsim, force);

	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);

	dsim_panel_set_acl(dsim, force);

	dsim_panel_set_tset(dsim, force);

#ifdef CONFIG_LCD_ALPM
	if (!(dsim->priv.current_alpm && dsim->priv.alpm))
#endif
		dsim_panel_set_hbm(dsim, force);

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
			dsim_err("%s : fail to write F0 on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("%s : fail to write F0 on command\n", __func__);

	return 0;
}

#endif

static int get_panel_acl_on(int br, int adaptive_control)
{
	int retVal = ACL_STATUS_ON;

	if (br == S6E3FA3_MAX_BRIGHTNESS && adaptive_control == 0)
		retVal = ACL_STATUS_OFF;

	return retVal;
}

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

#ifdef CONFIG_LCD_HMT
	if(panel->hmt_on == HMT_ON) {
		pr_info("%s hmt is enabled, plz set hmt brightness \n", __func__);
		goto set_br_exit;
	}
#endif
#ifdef CONFIG_LCD_DOZE_MODE
	if(dsim->dsim_doze == DSIM_DOZE_STATE_DOZE
		|| dsim->dsim_doze == DSIM_DOZE_STATE_DOZE_SUSPEND) {
		dsim_info("%s : doze state, skip set_brightness.\n", __func__);
		goto set_br_exit;
	}
#endif
	dimming = (struct dim_data *)panel->dim_data;
	if ((dimming == NULL) || (panel->br_tbl == NULL)) {
		dsim_info("%s : this panel does not support dimming\n", __func__);
		return ret;
	}

	acutal_br = panel->br_tbl[p_br];
	panel->br_index = get_acutal_br_index(dsim, acutal_br);
	real_br = get_actual_br_value(dsim, panel->br_index);
	panel->acl_enable = get_panel_acl_on(real_br, panel->adaptive_control);

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		goto set_br_exit;
	}

#ifdef AID_INTERPOLATION
	if (!force && panel->br_index == prev_index) {
		mutex_lock(&panel->lock);

		dsim_panel_aid_interpolation(dsim);

		ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
			mutex_unlock(&panel->lock);
			goto set_br_exit;
		}

		dsim_panel_set_acl(dsim, 1);

		ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
			mutex_unlock(&panel->lock);
			goto set_br_exit;
		}

		mutex_unlock(&panel->lock);

		goto set_br_exit;
	}
#else
	if (!force && panel->br_index == prev_index)
		goto set_br_exit;
#endif

	if ((acutal_br == 0) || (real_br == 0))
		goto set_br_exit;

	dsim_info("%s : platform : %d, : mapping : %d, real : %d, index : %d\n",
		__func__, p_br, acutal_br, real_br, panel->br_index+1);
	mutex_lock(&panel->lock);

	ret = low_level_set_brightness(dsim, force);
	if (ret) {
		dsim_err("%s failed to set brightness : %d\n", __func__, acutal_br);
	}
	mutex_unlock(&panel->lock);

set_br_exit:
#endif
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
#ifdef CONFIG_LCD_DOZE_MODE
	if(dsim->dsim_doze == DSIM_DOZE_STATE_DOZE ||
		dsim->dsim_doze == DSIM_DOZE_STATE_DOZE_SUSPEND) {
		dsim_err("cannot set brightness in doze mode\n");
		goto exit_set;

	}
#endif
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

	panel->bd->props.max_brightness = EXTEND_BRIGHTNESS;
	panel->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;

#ifdef CONFIG_LCD_HMT
	panel->hmt_on = HMT_OFF;
	panel->hmt_brightness = DEFAULT_HMT_BRIGHTNESS;
	panel->hmt_br_index = 0;
#endif
	return ret;
}
