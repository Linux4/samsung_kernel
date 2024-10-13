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

struct aid_dimming_dynamic_var aid_dimming_dynamic;
unsigned dynamic_lcd_type;

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

	if(caps)
		return (unsigned char *)dimming_info[index].elvCaps;
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
		dsim_err("%s : failed to write gamma \n", __func__);
}

static char dsim_panel_get_elvssoffset(struct dsim_device *dsim)
{
	int nit = 0;
	char retVal = 0x00;
	struct panel_private* panel = &(dsim->priv);

	nit = panel->br_tbl[panel->bd->props.brightness];

	if (nit > MAX_BRIGHTNESS) {
		retVal = -HBM_INTER_22TH_OFFSET[panel->br_index - 74];
		goto exit_get_elvss;
	}

	if(nit > 6)
		nit = 6;

	if(UNDER_MINUS_20(panel->temperature)) {
		retVal = aid_dimming_dynamic.elvss_minus_offset[2][nit - 2];
	} else if(UNDER_0(panel->temperature)) {
		retVal = aid_dimming_dynamic.elvss_minus_offset[1][nit - 2];
	} else {
		retVal = aid_dimming_dynamic.elvss_minus_offset[0][nit - 2];
	}

exit_get_elvss:
	pr_info("%s %d\n", __func__, retVal);
	return retVal;
}

#ifdef AID_INTERPOLATION
static void dsim_panel_aid_interpolation(struct dsim_device *dsim)
{
	int current_pbr;
	struct panel_private *panel = &dsim->priv;
	u8 aid[S6E3HF3_AID_CMD_CNT] = { 0, };
	if (dynamic_lcd_type == LCD_TYPE_S6E3HF3_WQHD) {
		memcpy(aid, S6E3HF3_SEQ_AOR, sizeof(S6E3HF3_SEQ_AOR));
	}
	else{
		memcpy(aid, S6E3HA3_SEQ_AOR, sizeof(S6E3HA3_SEQ_AOR));
	}

	current_pbr = panel->bd->props.brightness;
	current_pbr = (current_pbr > UI_MAX_BRIGHTNESS) ? UI_MAX_BRIGHTNESS : current_pbr;

	if (panel->inter_aor_tbl == NULL)
		return;
	aid[9] = panel->inter_aor_tbl[current_pbr * 2];
	aid[10] = panel->inter_aor_tbl[current_pbr * 2 + 1];

	dsim_info("%s %d = aid : %x : %x : %x\n", __func__, current_pbr, aid[0], aid[9], aid[10]);

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);

	if (dynamic_lcd_type == LCD_TYPE_S6E3HF3_WQHD) {
		if (dsim_write_hl_data(dsim, aid, S6E3HF3_AID_CMD_CNT) < 0)
			dsim_err("%s : failed to write gamma \n", __func__);
	} else {
		if (dsim_write_hl_data(dsim, aid, S6E3HA3_AID_CMD_CNT) < 0)
			dsim_err("%s : failed to write gamma \n", __func__);
	}
	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L)) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);

	return;
}

static void dsim_panel_dynamic_aid_ctrl(struct dsim_device *dsim)
{
	u8 aid[S6E3HF3_AID_CMD_CNT] = { 0, };
	int index = 0;
	int current_pbr;
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dynamic_lcd_type == LCD_TYPE_S6E3HF3_WQHD) {
		memcpy(aid, S6E3HF3_SEQ_AOR, sizeof(S6E3HF3_SEQ_AOR));
	} else {
		memcpy(aid, S6E3HA3_SEQ_AOR, sizeof(S6E3HA3_SEQ_AOR));
	}

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto aid_ctrl_err;
	}
	current_pbr = panel->bd->props.brightness;
	current_pbr = (current_pbr > UI_MAX_BRIGHTNESS) ? UI_MAX_BRIGHTNESS : current_pbr;
	index = dsim->priv.br_index + 1;

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	if (panel->inter_aor_tbl == NULL)
		return;

	aid[9] = panel->inter_aor_tbl[current_pbr * 2];
	aid[10] = panel->inter_aor_tbl[current_pbr * 2 + 1];

	dsim_info("%s %d aid : %x : %x : %x\n", __func__, current_pbr, aid[0], aid[9], aid[10]);

	if (dynamic_lcd_type == LCD_TYPE_S6E3HF3_WQHD) {
		if (dsim_write_hl_data(dsim, aid, S6E3HF3_AID_CMD_CNT) < 0)
			dsim_err("%s : failed to write gamma \n", __func__);
	} else {
		if (dsim_write_hl_data(dsim, aid, S6E3HA3_AID_CMD_CNT) < 0)
			dsim_err("%s : failed to write gamma \n", __func__);
	}

aid_ctrl_err:

	return;
}
#endif

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
	unsigned char SEQ_AID[AID_CMD_CNT_MAX] = { 0, };

	aid = get_aid_from_index(dsim, dsim->priv.br_index);
	if (aid == NULL) {
		dsim_err("%s : failed to get elvss value\n", __func__);
		return;
	}
	SEQ_AID[0] = aid[0];
	memcpy(&SEQ_AID[1], dsim->priv.aid, aid_dimming_dynamic.aid_cmd_cnt - 1);	// HA2/HF3 not read. but no danger
	memcpy(&SEQ_AID[aid_dimming_dynamic.aid_reg_offset], aid + 1, aid_dimming_dynamic.aid_cmd_cnt - aid_dimming_dynamic.aid_reg_offset + 1);

	if (dsim_write_hl_data(dsim, SEQ_AID, aid_dimming_dynamic.aid_cmd_cnt) < 0)
		dsim_err("%s : failed to write aid \n", __func__);

}

static int dsim_panel_set_tset(struct dsim_device *dsim, int force)
{
	int ret = 0;
	int tset = 0;
	unsigned char SEQ_TSET[TSET_LEN_MAX] = {0, };

	SEQ_TSET[0] = aid_dimming_dynamic.tset_reg;
	tset = (dsim->priv.temperature < 0) ? BIT(7) | abs(dsim->priv.temperature) : dsim->priv.temperature;

	if(force || dsim->priv.tset[aid_dimming_dynamic.tset_len - 2] != tset) {
		memcpy(&SEQ_TSET[1], dsim->priv.tset, aid_dimming_dynamic.tset_len - 1);
		dsim->priv.tset[aid_dimming_dynamic.tset_len - 2] = SEQ_TSET[aid_dimming_dynamic.tset_len - 1] = tset;
		if ((ret = dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET))) < 0) {
			dsim_err("fail to write tset command.\n");
			ret = -EPERM;
		}
		dsim_info("%s temperature: %d, tset: %d\n",
			__func__, dsim->priv.temperature, SEQ_TSET[aid_dimming_dynamic.tset_len - 1]);
	}
	return ret;
}

static void dsim_panel_set_elvss(struct dsim_device *dsim)
{
	u8 *elvss = NULL;
	unsigned char SEQ_ELVSS[ELVSS_LEN_MAX] = {0, };

	SEQ_ELVSS[0] = aid_dimming_dynamic.elvss_reg;
	elvss = get_elvss_from_index(dsim, dsim->priv.br_index, dsim->priv.caps_enable);
	if (elvss == NULL) {
		dsim_err("%s : failed to get elvss value\n", __func__);
		return;
	}
	memcpy(&SEQ_ELVSS[1], dsim->priv.elvss_set, aid_dimming_dynamic.elvss_len - 1);
	memcpy(SEQ_ELVSS, elvss, aid_dimming_dynamic.elvss_cmd_cnt);

	SEQ_ELVSS[aid_dimming_dynamic.elvss_len - 1] += dsim_panel_get_elvssoffset(dsim);

	if(dynamic_lcd_type == LCD_TYPE_S6E3HA2_WQHD) {
		dsim_info("%s elvss ha2\n", __func__);
		if (dsim_write_hl_data(dsim, SEQ_ELVSS, aid_dimming_dynamic.elvss_len) < 0)
			dsim_err("%s : failed to write elvss \n", __func__);
	} else {
		dsim_info("%s elvss ha3/hf3\n", __func__);
		memcpy(dsim->priv.tset, &SEQ_ELVSS[1], aid_dimming_dynamic.elvss_len - 1);
		dsim_panel_set_tset(dsim, 1);
	}
}


static int dsim_panel_set_acl(struct dsim_device *dsim, int force)
{
	int ret = 0, level, enabled;
	struct panel_private *panel = &dsim->priv;

	if (panel == NULL) {
			dsim_err("%s : panel is NULL\n", __func__);
			goto exit;
	}

	level = ACL_OPR_15P;
	enabled = ACL_STATUS_ON;

	// siop = weak temperature
	if (panel->siop_enable)  // auto acl or hbm is acl on
		goto acl_update;

	level = dsim->priv.acl_enable;
	enabled = (dsim->priv.acl_enable != ACL_OPR_OFF);

acl_update:
	if(force || dsim->priv.current_acl != level) {
//		dsim_info("acl : opr_tbl %d %d %02x %02x %02x %02x %02x\n", level, (int) ACL_OPR_LEN, panel->acl_opr_tbl[level][0], panel->acl_opr_tbl[level][1], panel->acl_opr_tbl[level][2], panel->acl_opr_tbl[level][3], panel->acl_opr_tbl[level][4] );
		if((ret = dsim_write_hl_data(dsim,  panel->acl_opr_tbl[level], ACL_OPR_LEN)) < 0) {
			dsim_err("fail to write acl opr command.\n");
			goto exit;
		}
//		dsim_info("acl : cutoff_tbl %d %d  %02x %02x \n", enabled, (int) ACL_CMD_LEN, panel->acl_cutoff_tbl[enabled][0], panel->acl_cutoff_tbl[enabled][1] );
		if((ret = dsim_write_hl_data(dsim, panel->acl_cutoff_tbl[enabled], ACL_CMD_LEN)) < 0) {
			dsim_err("fail to write acl command.\n");
			goto exit;
		}
		dsim->priv.current_acl = level;
		dsim_info("acl : %x, opr: %x\n",
			panel->acl_cutoff_tbl[enabled][1], panel->acl_opr_tbl[level][ACL_OPR_LEN-1]);
		dsim_info("acl: %d(%x), adaptive_control: %d\n", dsim->priv.current_acl, panel->acl_opr_tbl[level][ACL_OPR_LEN-1], dsim->priv.adaptive_control);
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
	unsigned char SEQ_VINT[VINT_LEN] = {VINT_REG, 0x8B, 0x21};
	unsigned char *vint_tbl = aid_dimming_dynamic.vint_dim_offset;

	level = arraySize - 1;

	SEQ_VINT[1] = aid_dimming_dynamic.vint_reg2;

	if(UNDER_MINUS_20(panel->temperature))
		goto set_vint;
#ifdef CONFIG_LCD_HMT
	if(panel->hmt_on == HMT_ON)
		goto set_vint;
#endif
	nit = get_actual_br_value(dsim, panel->br_index);

	for (i = 0; i < arraySize; i++) {
		if (nit <= VINT_DIM_TABLE[i]) {
			level = i;
			goto set_vint;
		}
	}
set_vint:
	if(force || panel->current_vint != vint_tbl[level]) {
		SEQ_VINT[VINT_LEN - 1] = vint_tbl[level];
		if ((ret = dsim_write_hl_data(dsim, SEQ_VINT, ARRAY_SIZE(SEQ_VINT))) < 0) {
			dsim_err("fail to write vint command.\n");
			ret = -EPERM;
		}
		panel->current_vint = vint_tbl[level];
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

	if(force || panel->current_hbm != panel->hbm_tbl[level][1]) {
		panel->current_hbm = panel->hbm_tbl[level][1];
		if((ret = dsim_write_hl_data(dsim, panel->hbm_tbl[level], ARRAY_SIZE(SEQ_HBM_OFF))) < 0) {
			dsim_err("fail to write hbm command.\n");
			ret = -EPERM;
		}
		dsim_info("hbm: %d, brightness: %d\n", panel->current_hbm, dsim->priv.bd->props.brightness);
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
	if (dynamic_lcd_type ==	LCD_TYPE_S6E3HA2_WQHD)
		dsim_panel_aid_ctrl(dsim);
	else
		dsim_panel_dynamic_aid_ctrl(dsim);
#else
	dsim_panel_aid_ctrl(dsim);
#endif

	dsim_panel_set_elvss(dsim);

	dsim_panel_set_vint(dsim, force);

	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L)) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);

	dsim_panel_set_acl(dsim, force);
	if(dynamic_lcd_type == LCD_TYPE_S6E3HA2_WQHD)
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

#endif

static int get_panel_acl_level(int p_br, int nit, int adaptive_control)
{
	int retVal = ACL_OPR_15P;

	if (255 <= p_br && 420 == nit && !adaptive_control)
		retVal = ACL_OPR_OFF;
	else if (255 <= p_br)
		retVal = ACL_OPR_8P;

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

	dimming = (struct dim_data *)panel->dim_data;
	if ((dimming == NULL) || (panel->br_tbl == NULL)) {
		dsim_info("%s : this panel does not support dimming\n", __func__);
		return ret;
	}

	acutal_br = panel->br_tbl[p_br];
	panel->br_index = get_acutal_br_index(dsim, acutal_br);
	real_br = get_actual_br_value(dsim, panel->br_index);
	panel->caps_enable = CAPS_IS_ON(real_br);
	panel->acl_enable = get_panel_acl_level(p_br, real_br, panel->adaptive_control);

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		goto set_br_exit;
	}

#ifdef AID_INTERPOLATION
    if (!force && panel->br_index == prev_index) {
		if (dynamic_lcd_type ==	LCD_TYPE_S6E3HA2_WQHD)
			goto set_br_exit;

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

#ifdef CONFIG_LCD_HMT
#ifdef CONFIG_PANEL_AID_DIMMING
static unsigned char *get_gamma_from_index_for_hmt(struct dsim_device *dsim, int index)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->hmt_dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_gamma_err;
	}

	if (index > HMT_MAX_BR_INFO - 1)
		index = HMT_MAX_BR_INFO - 1;

	return (unsigned char *)dimming_info[index].gamma;

get_gamma_err:
	return NULL;
}

static unsigned char *get_aid_from_index_for_hmt(struct dsim_device *dsim, int index)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->hmt_dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_aid_err;
	}

	if (index > HMT_MAX_BR_INFO - 1)
		index = HMT_MAX_BR_INFO - 1;

	return (u8 *)dimming_info[index].aid;

get_aid_err:
	return NULL;
}

static unsigned char *get_elvss_from_index_for_hmt(struct dsim_device *dsim, int index, int caps)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->hmt_dim_info;

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


static void dsim_panel_gamma_ctrl_for_hmt(struct dsim_device *dsim)
{
	u8 *gamma = NULL;
	gamma = get_gamma_from_index_for_hmt(dsim, dsim->priv.hmt_br_index);
	if (gamma == NULL) {
		dsim_err("%s :faied to get gamma\n", __func__);
		return;
	}

	if (dsim_write_hl_data(dsim, gamma, GAMMA_CMD_CNT) < 0)
		dsim_err("%s : failed to write hmt gamma \n", __func__);
}


static void dsim_panel_aid_ctrl_for_hmt(struct dsim_device *dsim)
{
	u8 *aid = NULL;
	unsigned char SEQ_AID[AID_CMD_CNT_MAX] = {0, };

	aid = get_aid_from_index_for_hmt(dsim, dsim->priv.hmt_br_index);

	if (aid == NULL) {
		dsim_err("%s : faield to get aid value\n", __func__);
		return;
	}

	SEQ_AID[0] = aid[0];
	memcpy(&SEQ_AID[1], dsim->priv.aid, aid_dimming_dynamic.aid_cmd_cnt - 1);
	memcpy(&SEQ_AID[aid_dimming_dynamic.aid_reg_offset], aid + 1, aid_dimming_dynamic.aid_cmd_cnt - aid_dimming_dynamic.aid_reg_offset + 1);

	if (dsim_write_hl_data(dsim, SEQ_AID, aid_dimming_dynamic.aid_cmd_cnt) < 0)
		dsim_err("%s : failed to write aid \n", __func__);

}

static void dsim_panel_set_elvss_for_hmt(struct dsim_device *dsim)
{
	u8 *elvss = NULL;
	unsigned char SEQ_ELVSS[ELVSS_LEN_MAX] = {0, };

	SEQ_ELVSS[0] = aid_dimming_dynamic.elvss_reg;
	elvss = get_elvss_from_index_for_hmt(dsim, dsim->priv.hmt_br_index, dsim->priv.acl_enable);
	if (elvss == NULL) {
		dsim_err("%s : failed to get elvss value\n", __func__);
		return;
	}
	memcpy(&SEQ_ELVSS[1], dsim->priv.elvss_set, aid_dimming_dynamic.elvss_len - 1);
	memcpy(SEQ_ELVSS, elvss, aid_dimming_dynamic.elvss_cmd_cnt);

	SEQ_ELVSS[aid_dimming_dynamic.elvss_len - 1] += dsim_panel_get_elvssoffset(dsim);
	dsim_info("%s elvss ha3/hf3\n", __func__);
	memcpy(dsim->priv.tset, &SEQ_ELVSS[1], aid_dimming_dynamic.elvss_len - 1);
	dsim_panel_set_tset(dsim, 1);
}

static int low_level_set_brightness_for_hmt(struct dsim_device *dsim ,int force)
{
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);

	dsim_panel_gamma_ctrl_for_hmt(dsim);

	dsim_panel_aid_ctrl_for_hmt(dsim);

	dsim_panel_set_elvss_for_hmt(dsim);

	dsim_panel_set_vint(dsim, force);

	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L)) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);

	dsim_panel_set_acl(dsim, force);

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("%s : fail to write F0 on command\n", __func__);

	return 0;
}

static int get_acutal_br_index_for_hmt(struct dsim_device *dsim, int br)
{
	int i;
	int min;
	int gap;
	int index = 0;
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = panel->hmt_dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming_info is NULL\n", __func__);
		return 0;
	}

	min = HMT_MAX_BRIGHTNESS;

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
int dsim_panel_set_brightness_for_hmt(struct dsim_device *dsim, int force)
{
	int ret = 0;
#ifndef CONFIG_PANEL_AID_DIMMING
	dsim_info("%s:this panel does not support dimming \n", __func__);
#else
	struct dim_data *dimming;
	struct panel_private *panel = &dsim->priv;
	int p_br = panel->hmt_brightness;
	int acutal_br = 0;
	int prev_index = panel->hmt_br_index;

	dimming = (struct dim_data *)panel->hmt_dim_data;
	if ((dimming == NULL) || (panel->hmt_br_tbl == NULL)) {
		dsim_info("%s : this panel does not support dimming\n", __func__);
		return ret;
	}

	acutal_br = panel->hmt_br_tbl[p_br];
	panel->acl_enable = 0;
	panel->hmt_br_index = get_acutal_br_index_for_hmt(dsim, acutal_br);
	if(panel->siop_enable)					// check auto acl
		panel->acl_enable = 1;
	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		goto set_br_exit;
	}
	if (!force && panel->hmt_br_index == prev_index)
		goto set_br_exit;

	dsim_info("%s : req : %d : nit : %d index : %d\n",
		__func__, p_br, acutal_br, panel->hmt_br_index);

	if (acutal_br == 0)
		goto set_br_exit;

	mutex_lock(&panel->lock);

	ret = low_level_set_brightness_for_hmt(dsim, force);
	if (ret) {
		dsim_err("%s failed to hmt set brightness : %d\n", __func__, acutal_br);
	}
	mutex_unlock(&panel->lock);

set_br_exit:
#endif
	return ret;
}
#endif

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

	panel->bd->props.max_brightness = EXTEND_BRIGHTNESS;
	panel->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;

#ifdef CONFIG_LCD_HMT
	panel->hmt_on = HMT_OFF;
	panel->hmt_brightness = DEFAULT_HMT_BRIGHTNESS;
	panel->hmt_br_index = 0;
#endif
	return ret;
}
