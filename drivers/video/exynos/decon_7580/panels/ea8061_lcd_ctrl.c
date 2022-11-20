/*
 * drivers/video/decon_7580/panels/ea8061_lcd_ctrl.c
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


#include "ea8061_param.h"

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#include "dimming_core.h"
#include "ea8061_aid_dimming.h"
#endif

struct lcd_info {
	unsigned char DB[56];
	unsigned char B2[7];
	unsigned char D4[18];
};

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
	int level = LEVEL_IS_HBM(dsim->priv.auto_brightness), i = 0;
	u8 HBM_W[33] = {0xCA, };
	u8 *gamma = NULL;
	struct lcd_info *lcd = dsim->priv.par;

	if (level) {
		memcpy(&HBM_W[1], lcd->DB, 21);
		for (i = 22; i <= 30; i++)
			HBM_W[i] = 0x80;
		HBM_W[31] = 0x00;
		HBM_W[32] = 0x00;

		if (dsim_write_hl_data(dsim, HBM_W, ARRAY_SIZE(HBM_W)) < 0)
			dsim_err("%s : failed to write gamma\n", __func__);

	} else {
		gamma = get_gamma_from_index(dsim, dsim->priv.br_index);
		if (gamma == NULL) {
			dsim_err("%s :faied to get gamma\n", __func__);
			return;
		}
		if (dsim_write_hl_data(dsim, gamma, GAMMA_CMD_CNT) < 0)
			dsim_err("%s : failed to write gamma\n", __func__);
	}

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
	u8 B2_W[8] = {0xB2, };
	u8 D4_W[19] = {0xD4, };
	int tset = 0;
	int real_br = get_actual_br_value(dsim, dsim->priv.br_index);
	int level = LEVEL_IS_HBM(dsim->priv.auto_brightness);
	struct lcd_info *lcd = dsim->priv.par;

	tset = ((dsim->priv.temperature >= 0) ? 0 : BIT(7)) | abs(dsim->priv.temperature);

	elvss = get_elvss_from_index(dsim, dsim->priv.br_index, dsim->priv.acl_enable);
	if (elvss == NULL) {
		dsim_err("%s : failed to get elvss value\n", __func__);
		return;
	}

	if (level) {
		memcpy(&D4_W[1], SEQ_EA8061_ELVSS_SET_HBM_D4, ARRAY_SIZE(SEQ_EA8061_ELVSS_SET_HBM_D4));
		if (dsim->priv.temperature > 0)
			D4_W[3] = 0x48;
		else
			D4_W[3] = 0x4C;

		D4_W[18] = lcd->DB[33];

		if (dsim_write_hl_data(dsim, SEQ_EA8061_ELVSS_SET_HBM, ARRAY_SIZE(SEQ_EA8061_ELVSS_SET_HBM)) < 0)
			dsim_err("%s : failed to SEQ_EA8061_ELVSS_SET_HBMelvss\n", __func__);

		if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1)) < 0)
			dsim_err("%s : failed to SEQ_TEST_KEY_ON_F1\n", __func__);

		if (dsim_write_hl_data(dsim, D4_W, EA8061_MTP_D4_SIZE + 1) < 0)
			dsim_err("%s : failed to write elvss\n", __func__);

		if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F1, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F1)) < 0)
			dsim_err("%s : failed to SEQ_TEST_KEY_OFF_F1\n", __func__);

	} else {
		memcpy(&B2_W[1], lcd->B2, EA8061_MTP_B2_SIZE);
		memcpy(&B2_W[1], SEQ_CAPS_ELVSS_DEFAULT, ARRAY_SIZE(SEQ_CAPS_ELVSS_DEFAULT));
		memcpy(&D4_W[1], lcd->D4, EA8061_MTP_D4_SIZE);

		B2_W[1] = elvss[0];
		B2_W[7] = tset;

		if (dsim->priv.temperature > 0)
			D4_W[3] = 0x48;
		else
			D4_W[3] = 0x4C;

		if (real_br <= 29) {
			if (dsim->priv.temperature > 0)
				D4_W[18] = elvss[1];
			else if (dsim->priv.temperature > -20 && dsim->priv.temperature < 0)
				D4_W[18] = elvss[2];
			else
				D4_W[18] = elvss[3];
		}
		if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1)) < 0)
			dsim_err("%s : failed to SEQ_TEST_KEY_ON_F1\n", __func__);

		if (dsim_write_hl_data(dsim, B2_W, EA8061_MTP_B2_SIZE + 1) < 0)
			dsim_err("%s : failed to write elvss\n", __func__);

		if (dsim_write_hl_data(dsim, D4_W, EA8061_MTP_D4_SIZE + 1) < 0)
			dsim_err("%s : failed to write elvss\n", __func__);

		if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F1, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F1)) < 0)
			dsim_err("%s : failed to SEQ_TEST_KEY_OFF_F1\n", __func__);
	}

	dsim_info("%s: %d Tset: %x Temp: %d\n", __func__, level, D4_W[3], dsim->priv.temperature);
}

static int dsim_panel_set_acl(struct dsim_device *dsim, int force)
{
	int ret = 0, level = ACL_STATUS_8P;
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

static int low_level_set_brightness(struct dsim_device *dsim, int force)
{
	int ret;

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_EA8061_LTPS_STOP, ARRAY_SIZE(SEQ_EA8061_LTPS_STOP));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_EA8061_LTPS_STOP\n", __func__);
		goto init_exit;
	}
	dsim_panel_gamma_ctrl(dsim);

	dsim_panel_aid_ctrl(dsim);

	ret = dsim_write_hl_data(dsim, SEQ_EA8061_LTPS_UPDATE, ARRAY_SIZE(SEQ_EA8061_LTPS_UPDATE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_EA8061_LTPS_UPDATE\n", __func__);
		goto init_exit;
	}

	dsim_panel_set_elvss(dsim);

	dsim_panel_set_acl(dsim, force);

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("%s : fail to write F0 on command\n", __func__);

init_exit:
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
	struct panel_private *panel = &dsim->priv;
	int ret = 0;

#ifdef CONFIG_PANEL_AID_DIMMING
	int p_br = panel->bd->props.brightness;
	int acutal_br = 0;
	int real_br = 0;
	int prev_index = panel->br_index;
	struct dim_data *dimming;
#endif

	/* for ea8061 */
	dimming = (struct dim_data *)panel->dim_data;
	if ((dimming == NULL) || (panel->br_tbl == NULL)) {
		dsim_info("%s : this panel does not support dimming\n", __func__);
		return ret;
	}

	mutex_lock(&panel->lock);

	acutal_br = panel->br_tbl[p_br];
	panel->br_index = get_acutal_br_index(dsim, acutal_br);
	real_br = get_actual_br_value(dsim, panel->br_index);
	panel->acl_enable = 1;

	if (p_br >= 255 && panel->weakness_hbm_comp == 2)
		panel->acl_enable = 0;
	if (LEVEL_IS_HBM(panel->auto_brightness)) {
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

	ret = low_level_set_brightness(dsim, force);

	if (ret)
		dsim_err("%s failed to set brightness : %d\n", __func__, acutal_br);

set_br_exit:
	mutex_unlock(&panel->lock);
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


#ifdef CONFIG_PANEL_AID_DIMMING
static const unsigned char *ACL_CUTOFF_TABLE_8[ACL_STATUS_MAX] = {SEQ_ACL_OFF, SEQ_ACL_8};
static const unsigned char *ACL_CUTOFF_TABLE_15[ACL_STATUS_MAX] = {SEQ_ACL_OFF, SEQ_ACL_15};

static const unsigned int br_tbl[256] = {
	5, 5, 5, 5, 5, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 19, 20, 21, 22,
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

struct SmtDimInfo ea8061_dimming_info_D[MAX_BR_INFO + 1] = {				/* add hbm array */
	{.br = 5, .cTbl = D_ctbl5nit, .aid = D_aid9685, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_005},
	{.br = 6, .cTbl = D_ctbl6nit, .aid = D_aid9585, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_006},
	{.br = 7, .cTbl = D_ctbl7nit, .aid = D_aid9523, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_007},
	{.br = 8, .cTbl = D_ctbl8nit, .aid = D_aid9438, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_008},
	{.br = 9, .cTbl = D_ctbl9nit, .aid = D_aid9338, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_009},
	{.br = 10, .cTbl = D_ctbl10nit, .aid = D_aid9285, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_010},
	{.br = 11, .cTbl = D_ctbl11nit, .aid = D_aid9200, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_011},
	{.br = 12, .cTbl = D_ctbl12nit, .aid = D_aid9100, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_012},
	{.br = 13, .cTbl = D_ctbl13nit, .aid = D_aid9046, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_013},
	{.br = 14, .cTbl = D_ctbl14nit, .aid = D_aid8954, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_014},
	{.br = 15, .cTbl = D_ctbl15nit, .aid = D_aid8923, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_015},
	{.br = 16, .cTbl = D_ctbl16nit, .aid = D_aid8800, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_016},
	{.br = 17, .cTbl = D_ctbl17nit, .aid = D_aid8715, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_017},
	{.br = 19, .cTbl = D_ctbl19nit, .aid = D_aid8546, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_019},
	{.br = 20, .cTbl = D_ctbl20nit, .aid = D_aid8462, .elvAcl = elv0, .elv = elv0, .m_gray = D_m_gray_020},
	{.br = 21, .cTbl = D_ctbl21nit, .aid = D_aid8346, .elvAcl = elv1, .elv = elv1, .m_gray = D_m_gray_021},
	{.br = 22, .cTbl = D_ctbl22nit, .aid = D_aid8246, .elvAcl = elv2, .elv = elv2, .m_gray = D_m_gray_022},
	{.br = 24, .cTbl = D_ctbl24nit, .aid = D_aid8085, .elvAcl = elv3, .elv = elv3, .m_gray = D_m_gray_024},
	{.br = 25, .cTbl = D_ctbl25nit, .aid = D_aid7969, .elvAcl = elv4, .elv = elv4, .m_gray = D_m_gray_025},
	{.br = 27, .cTbl = D_ctbl27nit, .aid = D_aid7769, .elvAcl = elv5, .elv = elv5, .m_gray = D_m_gray_027},
	{.br = 29, .cTbl = D_ctbl29nit, .aid = D_aid7577, .elvAcl = elv6, .elv = elv6, .m_gray = D_m_gray_029},

	{.br = 30, .cTbl = D_ctbl30nit, .aid = D_aid7508, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_030},
	{.br = 32, .cTbl = D_ctbl32nit, .aid = D_aid7323, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_032},
	{.br = 34, .cTbl = D_ctbl34nit, .aid = D_aid7138, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_034},
	{.br = 37, .cTbl = D_ctbl37nit, .aid = D_aid6892, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_037},
	{.br = 39, .cTbl = D_ctbl39nit, .aid = D_aid6715, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_039},
	{.br = 41, .cTbl = D_ctbl41nit, .aid = D_aid6531, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_041},
	{.br = 44, .cTbl = D_ctbl44nit, .aid = D_aid6262, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_044},
	{.br = 47, .cTbl = D_ctbl47nit, .aid = D_aid6000, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_047},
	{.br = 50, .cTbl = D_ctbl50nit, .aid = D_aid5731, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_050},
	{.br = 53, .cTbl = D_ctbl53nit, .aid = D_aid5454, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_053},
	{.br = 56, .cTbl = D_ctbl56nit, .aid = D_aid5177, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_056},
	{.br = 60, .cTbl = D_ctbl60nit, .aid = D_aid4800, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_060},
	{.br = 64, .cTbl = D_ctbl64nit, .aid = D_aid4438, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_064},
	{.br = 68, .cTbl = D_ctbl68nit, .aid = D_aid4062, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_068},
	{.br = 72, .cTbl = D_ctbl72nit, .aid = D_aid3662, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_072},
	{.br = 77, .cTbl = D_ctbl77nit, .aid = D_aid3662, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_077},
	{.br = 82, .cTbl = D_ctbl82nit, .aid = D_aid3662, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_082},
	{.br = 87, .cTbl = D_ctbl87nit, .aid = D_aid3662, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_087},
	{.br = 93, .cTbl = D_ctbl93nit, .aid = D_aid3662, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_093},
	{.br = 98, .cTbl = D_ctbl98nit, .aid = D_aid3662, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_098},
	{.br = 105, .cTbl = D_ctbl105nit, .aid = D_aid3662, .elvAcl = elv7, .elv = elv7, .m_gray = D_m_gray_105},

	{.br = 111, .cTbl = D_ctbl111nit, .aid = D_aid3662, .elvAcl = elv8, .elv = elv8, .m_gray = D_m_gray_111},
	{.br = 119, .cTbl = D_ctbl119nit, .aid = D_aid3662, .elvAcl = elv8, .elv = elv8, .m_gray = D_m_gray_119},

	{.br = 126, .cTbl = D_ctbl126nit, .aid = D_aid3662, .elvAcl = elv9, .elv = elv9, .m_gray = D_m_gray_126},
	{.br = 134, .cTbl = D_ctbl134nit, .aid = D_aid3662, .elvAcl = elv9, .elv = elv9, .m_gray = D_m_gray_134},
	{.br = 143, .cTbl = D_ctbl143nit, .aid = D_aid3662, .elvAcl = elv9, .elv = elv9, .m_gray = D_m_gray_143},
	{.br = 152, .cTbl = D_ctbl152nit, .aid = D_aid3662, .elvAcl = elv9, .elv = elv9, .m_gray = D_m_gray_152},
	{.br = 162, .cTbl = D_ctbl162nit, .aid = D_aid3662, .elvAcl = elv9, .elv = elv9, .m_gray = D_m_gray_162},
	{.br = 172, .cTbl = D_ctbl172nit, .aid = D_aid3662, .elvAcl = elv9, .elv = elv9, .m_gray = D_m_gray_172},
	{.br = 183, .cTbl = D_ctbl183nit, .aid = D_aid3200, .elvAcl = elv9, .elv = elv9, .m_gray = D_m_gray_183},

	{.br = 195, .cTbl = D_ctbl195nit, .aid = D_aid2708, .elvAcl = elv10, .elv = elv10, .m_gray = D_m_gray_195},
	{.br = 207, .cTbl = D_ctbl207nit, .aid = D_aid2185, .elvAcl = elv11, .elv = elv11, .m_gray = D_m_gray_207},
	{.br = 220, .cTbl = D_ctbl220nit, .aid = D_aid1654, .elvAcl = elv12, .elv = elv12, .m_gray = D_m_gray_220},
	{.br = 234, .cTbl = D_ctbl234nit, .aid = D_aid1038, .elvAcl = elv13, .elv = elv13, .m_gray = D_m_gray_234},
	{.br = 249, .cTbl = D_ctbl249nit, .aid = D_aid0369, .elvAcl = elv14, .elv = elv14, .m_gray = D_m_gray_249},		/* 249 ~ 360nit acl off */
	{.br = 265, .cTbl = D_ctbl265nit, .aid = D_aid0369, .elvAcl = elv15, .elv = elv15, .m_gray = D_m_gray_265},
	{.br = 282, .cTbl = D_ctbl282nit, .aid = D_aid0369, .elvAcl = elv16, .elv = elv16, .m_gray = D_m_gray_282},
	{.br = 300, .cTbl = D_ctbl300nit, .aid = D_aid0369, .elvAcl = elv17, .elv = elv17, .m_gray = D_m_gray_300},
	{.br = 316, .cTbl = D_ctbl316nit, .aid = D_aid0369, .elvAcl = elv18, .elv = elv18, .m_gray = D_m_gray_316},
	{.br = 333, .cTbl = D_ctbl333nit, .aid = D_aid0369, .elvAcl = elv19, .elv = elv19, .m_gray = D_m_gray_333},
	{.br = 350, .cTbl = D_ctbl350nit, .aid = D_aid0369, .elvAcl = elv20, .elv = elv20, .m_gray = D_m_gray_350},
	{.br = 350, .cTbl = D_ctbl350nit, .aid = D_aid0369, .elvAcl = elv20, .elv = elv20, .m_gray = D_m_gray_350},
};

struct SmtDimInfo ea8061_dimming_info[MAX_BR_INFO + 1] = {				/* add hbm array */
	{.br = 5, .cTbl = ctbl5nit, .aid = aid9685, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_005},
	{.br = 6, .cTbl = ctbl6nit, .aid = aid9592, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_006},
	{.br = 7, .cTbl = ctbl7nit, .aid = aid9523, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_007},
	{.br = 8, .cTbl = ctbl8nit, .aid = aid9446, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_008},
	{.br = 9, .cTbl = ctbl9nit, .aid = aid9354, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_009},
	{.br = 10, .cTbl = ctbl10nit, .aid = aid9285, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_010},
	{.br = 11, .cTbl = ctbl11nit, .aid = aid9215, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_011},
	{.br = 12, .cTbl = ctbl12nit, .aid = aid9123, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_012},
	{.br = 13, .cTbl = ctbl13nit, .aid = aid9069, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_013},
	{.br = 14, .cTbl = ctbl14nit, .aid = aid8985, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_014},
	{.br = 15, .cTbl = ctbl15nit, .aid = aid8923, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_015},
	{.br = 16, .cTbl = ctbl16nit, .aid = aid8831, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_016},
	{.br = 17, .cTbl = ctbl17nit, .aid = aid8746, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_017},
	{.br = 19, .cTbl = ctbl19nit, .aid = aid8592, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_019},
	{.br = 20, .cTbl = ctbl20nit, .aid = aid8500, .elvAcl = elv0, .elv = elv0, .m_gray = m_gray_020},

	{.br = 21, .cTbl = ctbl21nit, .aid = aid8423, .elvAcl = elv1, .elv = elv1, .m_gray = m_gray_021},
	{.br = 22, .cTbl = ctbl22nit, .aid = aid8331, .elvAcl = elv2, .elv = elv2, .m_gray = m_gray_022},
	{.br = 24, .cTbl = ctbl24nit, .aid = aid8138, .elvAcl = elv3, .elv = elv3, .m_gray = m_gray_024},
	{.br = 25, .cTbl = ctbl25nit, .aid = aid8054, .elvAcl = elv4, .elv = elv4, .m_gray = m_gray_025},
	{.br = 27, .cTbl = ctbl27nit, .aid = aid7885, .elvAcl = elv5, .elv = elv5, .m_gray = m_gray_027},
	{.br = 29, .cTbl = ctbl29nit, .aid = aid7692, .elvAcl = elv6, .elv = elv6, .m_gray = m_gray_029},

	{.br = 30, .cTbl = ctbl30nit, .aid = aid7562, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_030},
	{.br = 32, .cTbl = ctbl32nit, .aid = aid7362, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_032},
	{.br = 34, .cTbl = ctbl34nit, .aid = aid7192, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_034},
	{.br = 37, .cTbl = ctbl37nit, .aid = aid6931, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_037},
	{.br = 39, .cTbl = ctbl39nit, .aid = aid6746, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_039},
	{.br = 41, .cTbl = ctbl41nit, .aid = aid6577, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_041},
	{.br = 44, .cTbl = ctbl44nit, .aid = aid6292, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_044},
	{.br = 47, .cTbl = ctbl47nit, .aid = aid6015, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_047},
	{.br = 50, .cTbl = ctbl50nit, .aid = aid5762, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_050},
	{.br = 53, .cTbl = ctbl53nit, .aid = aid5515, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_053},
	{.br = 56, .cTbl = ctbl56nit, .aid = aid5254, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_056},
	{.br = 60, .cTbl = ctbl60nit, .aid = aid4846, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_060},
	{.br = 64, .cTbl = ctbl64nit, .aid = aid4515, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_064},
	{.br = 68, .cTbl = ctbl68nit, .aid = aid4146, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_068},

	{.br = 72, .cTbl = ctbl72nit, .aid = aid3754, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_072},
	{.br = 77, .cTbl = ctbl77nit, .aid = aid3754, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_077},
	{.br = 82, .cTbl = ctbl82nit, .aid = aid3754, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_082},
	{.br = 87, .cTbl = ctbl87nit, .aid = aid3754, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_087},
	{.br = 93, .cTbl = ctbl93nit, .aid = aid3754, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_093},
	{.br = 98, .cTbl = ctbl98nit, .aid = aid3754, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_098},
	{.br = 105, .cTbl = ctbl105nit, .aid = aid3754, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_105},


	{.br = 111, .cTbl = ctbl111nit, .aid = aid3754, .elvAcl = elv7, .elv = elv7, .m_gray = m_gray_111},
	{.br = 119, .cTbl = ctbl119nit, .aid = aid3754, .elvAcl = elv8, .elv = elv8, .m_gray = m_gray_119},
	{.br = 126, .cTbl = ctbl126nit, .aid = aid3754, .elvAcl = elv8, .elv = elv8, .m_gray = m_gray_126},
	{.br = 134, .cTbl = ctbl134nit, .aid = aid3754, .elvAcl = elv9, .elv = elv9, .m_gray = m_gray_134},

	{.br = 143, .cTbl = ctbl143nit, .aid = aid3754, .elvAcl = elv10, .elv = elv10, .m_gray = m_gray_143},
	{.br = 152, .cTbl = ctbl152nit, .aid = aid3754, .elvAcl = elv11, .elv = elv11, .m_gray = m_gray_152},
	{.br = 162, .cTbl = ctbl162nit, .aid = aid3754, .elvAcl = elv12, .elv = elv12, .m_gray = m_gray_162},

	{.br = 172, .cTbl = ctbl172nit, .aid = aid3754, .elvAcl = elv12, .elv = elv12, .m_gray = m_gray_172},
	{.br = 183, .cTbl = ctbl183nit, .aid = aid3292, .elvAcl = elv12, .elv = elv12, .m_gray = m_gray_183},
	{.br = 195, .cTbl = ctbl195nit, .aid = aid2800, .elvAcl = elv13, .elv = elv13, .m_gray = m_gray_195},
	{.br = 207, .cTbl = ctbl207nit, .aid = aid2277, .elvAcl = elv14, .elv = elv14, .m_gray = m_gray_207},
	{.br = 220, .cTbl = ctbl220nit, .aid = aid1708, .elvAcl = elv14, .elv = elv14, .m_gray = m_gray_220},
	{.br = 234, .cTbl = ctbl234nit, .aid = aid1077, .elvAcl = elv14, .elv = elv14, .m_gray = m_gray_234},

	{.br = 249, .cTbl = ctbl249nit, .aid = aid0369, .elvAcl = elv15, .elv = elv15, .m_gray = m_gray_249},		/* 249 ~ 360nit acl off */
	{.br = 265, .cTbl = ctbl265nit, .aid = aid0369, .elvAcl = elv15, .elv = elv15, .m_gray = m_gray_265},
	{.br = 282, .cTbl = ctbl282nit, .aid = aid0369, .elvAcl = elv16, .elv = elv16, .m_gray = m_gray_282},
	{.br = 300, .cTbl = ctbl300nit, .aid = aid0369, .elvAcl = elv17, .elv = elv17, .m_gray = m_gray_300},
	{.br = 316, .cTbl = ctbl316nit, .aid = aid0369, .elvAcl = elv18, .elv = elv18, .m_gray = m_gray_316},
	{.br = 333, .cTbl = ctbl333nit, .aid = aid0369, .elvAcl = elv19, .elv = elv19, .m_gray = m_gray_333},
	{.br = 350, .cTbl = ctbl350nit, .aid = aid0369, .elvAcl = elv20, .elv = elv20, .m_gray = m_gray_350},
	{.br = 350, .cTbl = ctbl350nit, .aid = aid0369, .elvAcl = elv20, .elv = elv20, .m_gray = m_gray_350},
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

	if (dsim->priv.id[2] >= 0x03) {
		diminfo = ea8061_dimming_info_D;
		dsim_info("%s : ID is over 0x03\n", __func__);
	} else {
		diminfo = ea8061_dimming_info;
	}

	panel->dim_data = (void *)dimming;
	panel->dim_info = (void *)diminfo;

	panel->br_tbl = (unsigned int *)br_tbl;

	if (panel->id[2] >=  0x02)
		panel->acl_cutoff_tbl = (unsigned char **)ACL_CUTOFF_TABLE_15;
	else
		panel->acl_cutoff_tbl = (unsigned char **)ACL_CUTOFF_TABLE_8;

	for (j = 0; j < CI_MAX; j++) {
		if (mtp[pos] & 0x01)
			temp = mtp[pos+1] - 256;
		else
			temp = mtp[pos+1];

		dimming->t_gamma[V255][j] = (int)center_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
		pos += 2;
	}

/* if ddi have V0 offset, plz modify to   for (i = V203; i >= V0; i--) {    */
	for (i = V203; i > V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((mtp[pos] & 0x80) ? -1 : 1) * ((mtp[pos] & 0x80) ? 128 - (mtp[pos] & 0x7f) : (mtp[pos] & 0x7f));
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


static int ea8061_read_init_info(struct dsim_device *dsim, unsigned char *mtp)
{
	int i = 0;
	int ret;
	struct panel_private *panel = &dsim->priv;
	unsigned char buf[60] = {0, };
	struct lcd_info *lcd = dsim->priv.par;

	dsim_info("MDD : %s was called\n", __func__);

	dsim_write_hl_data(dsim, SEQ_EA8061_READ_ID, ARRAY_SIZE(SEQ_EA8061_READ_ID));
	ret = dsim_read_hl_data(dsim, EA8061_READ_RX_REG, EA8061_ID_LEN, dsim->priv.id);
	if (ret != EA8061_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < EA8061_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0)
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);

	/* mtp SEQ_EA8061_READ_MTP */
	dsim_write_hl_data(dsim, SEQ_EA8061_READ_MTP, ARRAY_SIZE(SEQ_EA8061_READ_MTP));
	ret = dsim_read_hl_data(dsim, EA8061_READ_RX_REG, EA8061_MTP_DATE_SIZE, buf);
	if (ret != EA8061_MTP_DATE_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(mtp, buf, EA8061_MTP_SIZE);

	/* read DB */
	dsim_write_hl_data(dsim, SEQ_EA8061_READ_DB, ARRAY_SIZE(SEQ_EA8061_READ_DB));
	ret = dsim_read_hl_data(dsim, EA8061_READ_RX_REG, EA8061_MTP_DB_SIZE, buf);
	if (ret != EA8061_MTP_DB_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(lcd->DB, buf, EA8061_MTP_DB_SIZE);

	memcpy(dsim->priv.date, &buf[50], EA8061_DATE_SIZE);
	dsim->priv.coordinate[0] = buf[52] << 8 | buf[53];	/* X */
	dsim->priv.coordinate[1] = buf[54] << 8 | buf[55];

	/* read B2 */
	dsim_write_hl_data(dsim, SEQ_EA8061_READ_B2, ARRAY_SIZE(SEQ_EA8061_READ_B2));
	ret = dsim_read_hl_data(dsim, EA8061_READ_RX_REG, EA8061_MTP_B2_SIZE, buf);
	if (ret != EA8061_MTP_B2_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(lcd->B2, buf, EA8061_MTP_B2_SIZE);

	/* read D4 */
	dsim_write_hl_data(dsim, SEQ_EA8061_READ_D4, ARRAY_SIZE(SEQ_EA8061_READ_D4));
	ret = dsim_read_hl_data(dsim, EA8061_READ_RX_REG, EA8061_MTP_D4_SIZE, buf);
	if (ret != EA8061_MTP_D4_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(lcd->D4, buf, EA8061_MTP_D4_SIZE);

read_exit:
	return 0;

read_fail:
	return -ENODEV;
}

static int ea8061_displayon(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	goto displayon_err;


displayon_err:
	return ret;

}

static int ea8061_exit(struct dsim_device *dsim)
{
	int ret = 0;
	dsim_info("MDD : %s was called\n", __func__);
	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
		goto exit_err;
	}

	msleep(35);

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
		goto exit_err;
	}

	msleep(150);

exit_err:
	return ret;
}

static int ea8061_init(struct dsim_device *dsim)
{
	int ret;

	dsim_info("MDD : %s was called\n", __func__);

	usleep_range(5000, 6000);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_EA8061_LTPS_STOP, ARRAY_SIZE(SEQ_EA8061_LTPS_STOP));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_EA8061_LTPS_STOP\n", __func__);
		goto init_exit;
	}

	if (dsim->priv.id[2] >= 0x03) {
		ret = dsim_write_hl_data(dsim, SEQ_EA8061_LTPS_TIMING_REV_D, ARRAY_SIZE(SEQ_EA8061_LTPS_TIMING_REV_D));
		if (ret < 0) {
			dsim_err(":%s fail to write CMD : SEQ_EA8061_LTPS_TIMING_REV_D\n", __func__);
			goto init_exit;
		}
	} else {
		ret = dsim_write_hl_data(dsim, SEQ_EA8061_LTPS_TIMING, ARRAY_SIZE(SEQ_EA8061_LTPS_TIMING));
		if (ret < 0) {
			dsim_err(":%s fail to write CMD : SEQ_EA8061_LTPS_TIMING\n", __func__);
			goto init_exit;
		}
	}

	ret = dsim_write_hl_data(dsim, SEQ_EA8061_LTPS_UPDATE, ARRAY_SIZE(SEQ_EA8061_LTPS_UPDATE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_EA8061_LTPS_UPDATE\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_EA8061_SCAN_DIRECTION, ARRAY_SIZE(SEQ_EA8061_SCAN_DIRECTION));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_EA8061_SCAN_DIRECTION\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_EA8061_AID_SET_DEFAULT, ARRAY_SIZE(SEQ_EA8061_AID_SET_DEFAULT));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_EA8061_AID_SET_DEFAULT\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_EA8061_SLEW_CONTROL, ARRAY_SIZE(SEQ_EA8061_SLEW_CONTROL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_EA8061_SLEW_CONTROL\n", __func__);
		goto init_exit;
	}

#if 1
	/* control el_on by manual */

	/*Manual ON = 1*/
	ret = dsim_write_hl_data(dsim, SEQ_GP_01, ARRAY_SIZE(SEQ_GP_01));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GP_01\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_F3_ON, ARRAY_SIZE(SEQ_F3_ON));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_F3_ON\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}

	msleep(20);

	/*EL_ON=1*/
	ret = dsim_write_hl_data(dsim, SEQ_GP_02, ARRAY_SIZE(SEQ_GP_02));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GP_02\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_F3_08, ARRAY_SIZE(SEQ_F3_08));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_F3_08\n", __func__);
		goto init_exit;
	}

	usleep_range(150, 160);

	/*EL_ON=0*/
	ret = dsim_write_hl_data(dsim, SEQ_GP_02, ARRAY_SIZE(SEQ_GP_02));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GP_02\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_F3_OFF, ARRAY_SIZE(SEQ_F3_OFF));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_F3_OFF\n", __func__);
		goto init_exit;
	}


	usleep_range(100, 110);


	/*EL_ON=1*/
	ret = dsim_write_hl_data(dsim, SEQ_GP_02, ARRAY_SIZE(SEQ_GP_02));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GP_02\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_F3_08, ARRAY_SIZE(SEQ_F3_08));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_F3_08\n", __func__);
		goto init_exit;
	}

	usleep_range(150, 160);

	/*EL_ON=0*/
	ret = dsim_write_hl_data(dsim, SEQ_GP_02, ARRAY_SIZE(SEQ_GP_02));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GP_02\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_F3_OFF, ARRAY_SIZE(SEQ_F3_OFF));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_F3_OFF\n", __func__);
		goto init_exit;
	}


	usleep_range(100, 110);


	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_SLEEP_IN\n", __func__);
		goto init_exit;
	}

	/*Manual ON = 0*/
	ret = dsim_write_hl_data(dsim, SEQ_GP_01, ARRAY_SIZE(SEQ_GP_01));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GP_01\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_F3_OFF, ARRAY_SIZE(SEQ_F3_OFF));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_F3_OFF\n", __func__);
		goto init_exit;
	}

#endif

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}

	msleep(120);

	dsim_panel_set_brightness(dsim, 1);

#if 0
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_ON_F1\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_MONITOR_GLOBAL, ARRAY_SIZE(SEQ_MONITOR_GLOBAL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_MONITOR_GLOBAL\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_MONITOR_0, ARRAY_SIZE(SEQ_MONITOR_0));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_MONITOR_0\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_MONITOR_1, ARRAY_SIZE(SEQ_MONITOR_1));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_MONITOR_1\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F1, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F1));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_OFF_F1\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
		goto init_exit;
	}

#endif

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_EA8061_MPS_SET_MAX\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);

init_exit:
	return ret;
}

static int ea8061_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[EA8061_MTP_SIZE] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	panel->par = (void *)kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!panel->par) {
		pr_err("failed to allocate for panel par\n");
		ret = -ENOMEM;
		goto probe_exit;
	}

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;
	panel->weakness_hbm_comp = 0;

	ret = ea8061_read_init_info(dsim, mtp);
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

static int ea8061_early_probe(struct dsim_device *dsim)
{
	int ret = 0;

	return ret;
}

struct dsim_panel_ops ea8061_panel_ops = {
	.early_probe = ea8061_early_probe,
	.probe		= ea8061_probe,
	.displayon	= ea8061_displayon,
	.exit		= ea8061_exit,
	.init		= ea8061_init,
};


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

static ssize_t read_mtp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	unsigned int reg, pos, length, i;
	unsigned char readbuf[256] = {0xff, };
	unsigned char writebuf[2] = {0xB0, };
	unsigned char read_st[2] = {EA8061_READ_TX_REG,	0x00};

	dsim = container_of(priv, struct dsim_device, priv);

	sscanf(buf, "%x %d %d", &reg, &pos, &length);

	if (!reg || !length || reg > 0xff || length > 255 || pos > 255)
		return -EINVAL;

	if (priv->state == PANEL_STATE_RESUMED)
		return -EINVAL;

	writebuf[1] = pos;
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("fail to write F0 on command.\n");

	read_st[1] = reg;

	if (dsim_write_hl_data(dsim, read_st, ARRAY_SIZE(read_st)) < 0)
		dsim_err("fail to write global command.\n");

	if (dsim_read_hl_data(dsim, EA8061_READ_RX_REG, length, readbuf) < 0)
		dsim_err("fail to read id on command.\n");

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("fail to write F0 on command.\n");

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
	unsigned char read_reg[] = {
		EA8061_READ_TX_REG,
		0x00,
	};

	dsim = container_of(priv, struct dsim_device, priv);

	reg = priv->dump_info[0];
	len = priv->dump_info[1];

	read_reg[1] = reg;

	if (!reg || !len || reg > 0xff || len > 255)
		goto exit;

	dump = kzalloc(len * sizeof(u8), GFP_KERNEL);

	if (priv->state == PANEL_STATE_RESUMED) {
		if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
			dsim_err("fail to write test on f0 command.\n");

		dsim_write_hl_data(dsim, read_reg, ARRAY_SIZE(read_reg));
		ret = dsim_read_hl_data(dsim, EA8061_READ_RX_REG, len, dump);

		if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
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
		if (!reg || !len || reg > 0xff || len > 255)
			return -EINVAL;

		priv->dump_info[0] = reg;
		priv->dump_info[1] = len;
	}

	return size;
}

static ssize_t weakness_hbm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", priv->weakness_hbm_comp);

	return strlen(buf);
}

static ssize_t weakness_hbm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc, value;
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, (unsigned int)0, &value);

	if (rc < 0)
		return rc;
	else {
		if (priv->weakness_hbm_comp != value){
			if((priv->weakness_hbm_comp == 1) && (value == 2)) {
				pr_info("%s don't support color blind -> gallery\n", __func__);
				return size;
			}
			if((priv->weakness_hbm_comp == 2) || (value == 2)){
				priv->weakness_hbm_comp = value;
				dsim_panel_set_brightness(dsim, 1);
				dev_info(dev, "%s: %d, %d\n", __func__, priv->weakness_hbm_comp, value);
			}
		}
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
static DEVICE_ATTR(read_mtp, 0220, NULL, read_mtp_store);
#ifdef CONFIG_PANEL_AID_DIMMING
static DEVICE_ATTR(aid_log, 0444, aid_log_show, NULL);
#endif
static DEVICE_ATTR(dump_register, 0644, dump_register_show, dump_register_store);
static DEVICE_ATTR(weakness_hbm_comp, 0664, weakness_hbm_show, weakness_hbm_store);

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

	ret = device_create_file(&dsim->priv.bd->dev ,&dev_attr_weakness_hbm_comp);
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
	struct lcd_info *lcd = dsim->priv.par;

	dsim->priv.par = lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;
		goto probe_err;
	}

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
		if (panel->ops->init) {
			ret = panel->ops->init(dsim);
			panel->state = PANEL_STATE_RESUMED;
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

struct mipi_dsim_lcd_driver ea8061_mipi_lcd_driver = {
	.early_probe = dsim_panel_early_probe,
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
	.resume		= dsim_panel_resume,
};

