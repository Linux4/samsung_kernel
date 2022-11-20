/*
 * drivers/video/decon_7580/panels/ea8064g_lcd_ctrl.c
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

#include "ea8064g_param.h"

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#include "dimming_core.h"
#include "ea8064g_aid_dimming.h"
#endif

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#include "mdnie.h"
#include "mdnie_lite_table_s5neo.h"
#endif



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
	int real_br = 0;

	real_br = get_actual_br_value(dsim, dsim->priv.br_index);

	elvss = get_elvss_from_index(dsim, dsim->priv.br_index, dsim->priv.acl_enable);
	if (elvss == NULL) {
		dsim_err("%s : failed to get elvss value\n", __func__);
		return;
	}

	elvss_val[0] = elvss[0];

	if (real_br >= 41)
		elvss_val[1] = 0x5C;
	else
		elvss_val[1] = 0x4C;

	elvss_val[2] = elvss[2];
	elvss_val[3] = dsim->priv.elvss[2];
	elvss_val[4] = dsim->priv.elvss[3];

	if (real_br <= 29) {
		if (dsim->priv.temperature <= -20)
			elvss_val[4] = elvss[5];
		else if (dsim->priv.temperature > -20 && dsim->priv.temperature <= 0)
			elvss_val[4] = elvss[4];
		else
			elvss_val[4] = elvss[3];
	}

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

	if (force || dsim->priv.tset[0] != tset)
		dsim->priv.tset[0] = SEQ_TSET[1] = tset;
	else
		return ret;

	if (dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET)) < 0) {
		dsim_err("fail to write tset command.\n");
		ret = -EPERM;

	}
	dsim_info("temperature: %d, tset: %d\n", dsim->priv.temperature, SEQ_TSET[1]);

	return ret;
}

static int dsim_panel_set_hbm(struct dsim_device *dsim, int force)
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
		dsim_info("hbm: %d, auto_brightness: %d\n", dsim->priv.current_hbm, dsim->priv.auto_brightness);
	}
exit:
	return ret;
}

static int low_level_set_brightness(struct dsim_device *dsim, int force)
{
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1)) < 0)
		dsim_err("%s : fail to write F1 on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
		dsim_err("%s : fail to write FC on command.\n", __func__);

	dsim_panel_gamma_ctrl(dsim);

	dsim_panel_aid_ctrl(dsim);

	dsim_panel_set_elvss(dsim);

	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_EA8064G, ARRAY_SIZE(SEQ_GAMMA_UPDATE_EA8064G)) < 0)
		dsim_err("%s : failed to write gamma\n", __func__);

	dsim_panel_set_acl(dsim, force);

	dsim_panel_set_tset(dsim, force);

	dsim_panel_set_hbm(dsim, force);

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
			dsim_err("%s : fail to write FC on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F1, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F1)) < 0)
			dsim_err("%s : fail to write F1 on command.\n", __func__);
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

	dsim_info("%s : platform : %d, : mapping : %d, real : %d, index : %d force : %d\n",
		__func__, p_br, acutal_br, real_br, panel->br_index, force);

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
	316, 316, 316, 316, 333, 333, 333, 333, 333, 333, 333, 333, 333, 333, 333, 333, 360,
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

struct SmtDimInfo lily_dimming_info[MAX_BR_INFO + 1] = {				/* add hbm array */
	{.br = 5, .cTbl = ctbl5nit_B, .aid = aid5_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray005_B},
	{.br = 6, .cTbl = ctbl6nit_B, .aid = aid6_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray006_B},
	{.br = 7, .cTbl = ctbl7nit_B, .aid = aid7_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray007_B},
	{.br = 8, .cTbl = ctbl8nit_B, .aid = aid8_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray008_B},
	{.br = 9, .cTbl = ctbl9nit_B, .aid = aid9_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray009_B},
	{.br = 10, .cTbl = ctbl10nit_B, .aid = aid10_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray010_B},
	{.br = 11, .cTbl = ctbl11nit_B, .aid = aid11_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray011_B},
	{.br = 12, .cTbl = ctbl12nit_B, .aid = aid12_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray012_B},
	{.br = 13, .cTbl = ctbl13nit_B, .aid = aid13_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray013_B},
	{.br = 14, .cTbl = ctbl14nit_B, .aid = aid14_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray014_B},
	{.br = 15, .cTbl = ctbl15nit_B, .aid = aid15_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray015_B},
	{.br = 16, .cTbl = ctbl16nit_B, .aid = aid16_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray016_B},
	{.br = 17, .cTbl = ctbl17nit_B, .aid = aid17_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray017_B},
	{.br = 19, .cTbl = ctbl19nit_B, .aid = aid19_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray019_B},
	{.br = 20, .cTbl = ctbl20nit_B, .aid = aid20_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray020_B},
	{.br = 21, .cTbl = ctbl21nit_B, .aid = aid21_B, .elvAcl = elv86, .elv = elv86, .m_gray = m_gray021_B},
	{.br = 22, .cTbl = ctbl22nit_B, .aid = aid22_B, .elvAcl = elv88, .elv = elv88, .m_gray = m_gray022_B},
	{.br = 24, .cTbl = ctbl24nit_B, .aid = aid24_B, .elvAcl = elv8a, .elv = elv8a, .m_gray = m_gray024_B},
	{.br = 25, .cTbl = ctbl25nit_B, .aid = aid25_B, .elvAcl = elv8c, .elv = elv8c, .m_gray = m_gray025_B},
	{.br = 27, .cTbl = ctbl27nit_B, .aid = aid27_B, .elvAcl = elv8e, .elv = elv8e, .m_gray = m_gray027_B},
	{.br = 29, .cTbl = ctbl29nit_B, .aid = aid29_B, .elvAcl = elv90, .elv = elv90, .m_gray = m_gray029_B},
	{.br = 30, .cTbl = ctbl30nit_B, .aid = aid30_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray030_B},
	{.br = 32, .cTbl = ctbl32nit_B, .aid = aid32_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray032_B},
	{.br = 34, .cTbl = ctbl34nit_B, .aid = aid34_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray034_B},
	{.br = 37, .cTbl = ctbl37nit_B, .aid = aid37_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray037_B},
	{.br = 39, .cTbl = ctbl39nit_B, .aid = aid39_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray039_B},
	{.br = 41, .cTbl = ctbl41nit_B, .aid = aid41_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray041_B},
	{.br = 44, .cTbl = ctbl44nit_B, .aid = aid44_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray044_B},
	{.br = 47, .cTbl = ctbl47nit_B, .aid = aid47_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray047_B},
	{.br = 50, .cTbl = ctbl50nit_B, .aid = aid50_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray050_B},
	{.br = 53, .cTbl = ctbl53nit_B, .aid = aid53_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray053_B},
	{.br = 56, .cTbl = ctbl56nit_B, .aid = aid56_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray056_B},
	{.br = 60, .cTbl = ctbl60nit_B, .aid = aid60_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray060_B},
	{.br = 64, .cTbl = ctbl64nit_B, .aid = aid64_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray064_B},
	{.br = 68, .cTbl = ctbl68nit_B, .aid = aid68_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray068_B},
	{.br = 72, .cTbl = ctbl72nit_B, .aid = aid68_B, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray072_B},
	{.br = 77, .cTbl = ctbl77nit_B, .aid = aid68_B, .elvAcl = elv91, .elv = elv91, .m_gray = m_gray077_B},
	{.br = 82, .cTbl = ctbl82nit_B, .aid = aid68_B, .elvAcl = elv91, .elv = elv91, .m_gray = m_gray082_B},
	{.br = 87, .cTbl = ctbl87nit_B, .aid = aid68_B, .elvAcl = elv91, .elv = elv91, .m_gray = m_gray087_B},
	{.br = 93, .cTbl = ctbl93nit_B, .aid = aid68_B, .elvAcl = elv90, .elv = elv90, .m_gray = m_gray093_B},
	{.br = 98, .cTbl = ctbl98nit_B, .aid = aid68_B, .elvAcl = elv90, .elv = elv90, .m_gray = m_gray098_B},
	{.br = 105, .cTbl = ctbl105nit_B, .aid = aid68_B, .elvAcl = elv90, .elv = elv90, .m_gray = m_gray105_B},
	{.br = 111, .cTbl = ctbl111nit_B, .aid = aid68_B, .elvAcl = elv8f, .elv = elv8f, .m_gray = m_gray111_B},
	{.br = 119, .cTbl = ctbl119nit_B, .aid = aid68_B, .elvAcl = elv8f, .elv = elv8f, .m_gray = m_gray119_B},
	{.br = 126, .cTbl = ctbl126nit_B, .aid = aid68_B, .elvAcl = elv8e, .elv = elv8e, .m_gray = m_gray126_B},
	{.br = 134, .cTbl = ctbl134nit_B, .aid = aid68_B, .elvAcl = elv8e, .elv = elv8e, .m_gray = m_gray134_B},
	{.br = 143, .cTbl = ctbl143nit_B, .aid = aid68_B, .elvAcl = elv8d, .elv = elv8d, .m_gray = m_gray143_B},
	{.br = 152, .cTbl = ctbl152nit_B, .aid = aid68_B, .elvAcl = elv8c, .elv = elv8c, .m_gray = m_gray152_B},
	{.br = 162, .cTbl = ctbl162nit_B, .aid = aid68_B, .elvAcl = elv8c, .elv = elv8c, .m_gray = m_gray162_B},
	{.br = 172, .cTbl = ctbl172nit_B, .aid = aid172_B, .elvAcl = elv8c, .elv = elv8c, .m_gray = m_gray172_B},
	{.br = 183, .cTbl = ctbl183nit_B, .aid = aid183_B, .elvAcl = elv8c, .elv = elv8c, .m_gray = m_gray183_B},
	{.br = 195, .cTbl = ctbl195nit_B, .aid = aid195_B, .elvAcl = elv8b, .elv = elv8b, .m_gray = m_gray195_B},
	{.br = 207, .cTbl = ctbl207nit_B, .aid = aid207_B, .elvAcl = elv8a, .elv = elv8a, .m_gray = m_gray207_B},
	{.br = 220, .cTbl = ctbl220nit_B, .aid = aid220_B, .elvAcl = elv8a, .elv = elv8a, .m_gray = m_gray220_B},
	{.br = 234, .cTbl = ctbl234nit_B, .aid = aid234_B, .elvAcl = elv8a, .elv = elv8a, .m_gray = m_gray234_B},
	{.br = 249, .cTbl = ctbl249nit_B, .aid = aid249_B, .elvAcl = elv89, .elv = elv89, .m_gray = m_gray249_B},
	{.br = 265, .cTbl = ctbl265nit_B, .aid = aid249_B, .elvAcl = elv88, .elv = elv88, .m_gray = m_gray265_B},
	{.br = 282, .cTbl = ctbl282nit_B, .aid = aid249_B, .elvAcl = elv87, .elv = elv87, .m_gray = m_gray282_B},
	{.br = 300, .cTbl = ctbl300nit_B, .aid = aid249_B, .elvAcl = elv86, .elv = elv86, .m_gray = m_gray300_B},
	{.br = 316, .cTbl = ctbl316nit_B, .aid = aid249_B, .elvAcl = elv86, .elv = elv86, .m_gray = m_gray316_B},
	{.br = 333, .cTbl = ctbl333nit_B, .aid = aid249_B, .elvAcl = elv85, .elv = elv85, .m_gray = m_gray333_B},
	{.br = 360, .cTbl = ctbl360nit_B, .aid = aid249_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray360_B},
	{.br = 360, .cTbl = ctbl360nit_B, .aid = aid249_B, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray360_B},
};

struct SmtDimInfo daisy_dimming_info[MAX_BR_INFO + 1] = {				/* add hbm array */
	{.br = 5, .cTbl = ctbl5nit, .aid = aid5, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_005},
	{.br = 6, .cTbl = ctbl6nit, .aid = aid6, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_006},
	{.br = 7, .cTbl = ctbl7nit, .aid = aid7, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_007},
	{.br = 8, .cTbl = ctbl8nit, .aid = aid8, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_008},
	{.br = 9, .cTbl = ctbl9nit, .aid = aid9, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_009},
	{.br = 10, .cTbl = ctbl10nit, .aid = aid10, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_010},
	{.br = 11, .cTbl = ctbl11nit, .aid = aid11, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_011},
	{.br = 12, .cTbl = ctbl12nit, .aid = aid12, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_012},
	{.br = 13, .cTbl = ctbl13nit, .aid = aid13, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_013},
	{.br = 14, .cTbl = ctbl14nit, .aid = aid14, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_014},
	{.br = 15, .cTbl = ctbl15nit, .aid = aid15, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_015},
	{.br = 16, .cTbl = ctbl16nit, .aid = aid16, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_016},
	{.br = 17, .cTbl = ctbl17nit, .aid = aid17, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_017},
	{.br = 19, .cTbl = ctbl19nit, .aid = aid19, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_019},
	{.br = 20, .cTbl = ctbl20nit, .aid = aid20, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_020},
	{.br = 21, .cTbl = ctbl21nit, .aid = aid21, .elvAcl = elv86, .elv = elv86, .m_gray = m_gray_021},
	{.br = 22, .cTbl = ctbl22nit, .aid = aid22, .elvAcl = elv88, .elv = elv88, .m_gray = m_gray_022},
	{.br = 24, .cTbl = ctbl24nit, .aid = aid24, .elvAcl = elv8a, .elv = elv8a, .m_gray = m_gray_024},
	{.br = 25, .cTbl = ctbl25nit, .aid = aid25, .elvAcl = elv8c, .elv = elv8c, .m_gray = m_gray_025},
	{.br = 27, .cTbl = ctbl27nit, .aid = aid27, .elvAcl = elv8e, .elv = elv8e, .m_gray = m_gray_027},
	{.br = 29, .cTbl = ctbl29nit, .aid = aid29, .elvAcl = elv90, .elv = elv90, .m_gray = m_gray_029},
	{.br = 30, .cTbl = ctbl30nit, .aid = aid30, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_030},
	{.br = 32, .cTbl = ctbl32nit, .aid = aid32, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_032},
	{.br = 34, .cTbl = ctbl34nit, .aid = aid34, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_034},
	{.br = 37, .cTbl = ctbl37nit, .aid = aid37, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_037},
	{.br = 39, .cTbl = ctbl39nit, .aid = aid39, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_039},
	{.br = 41, .cTbl = ctbl41nit, .aid = aid41, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_041},
	{.br = 44, .cTbl = ctbl44nit, .aid = aid44, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_044},
	{.br = 47, .cTbl = ctbl47nit, .aid = aid47, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_047},
	{.br = 50, .cTbl = ctbl50nit, .aid = aid50, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_050},
	{.br = 53, .cTbl = ctbl53nit, .aid = aid53, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_053},
	{.br = 56, .cTbl = ctbl56nit, .aid = aid56, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_056},
	{.br = 60, .cTbl = ctbl60nit, .aid = aid60, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_060},
	{.br = 64, .cTbl = ctbl64nit, .aid = aid64, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_064},
	{.br = 68, .cTbl = ctbl68nit, .aid = aid68, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_068},
	{.br = 72, .cTbl = ctbl72nit, .aid = aid72, .elvAcl = elv92, .elv = elv92, .m_gray = m_gray_072},
	{.br = 77, .cTbl = ctbl77nit, .aid = aid77, .elvAcl = elv91, .elv = elv91, .m_gray = m_gray_077},
	{.br = 82, .cTbl = ctbl82nit, .aid = aid77, .elvAcl = elv91, .elv = elv91, .m_gray = m_gray_082},
	{.br = 87, .cTbl = ctbl87nit, .aid = aid77, .elvAcl = elv91, .elv = elv91, .m_gray = m_gray_087},
	{.br = 93, .cTbl = ctbl93nit, .aid = aid77, .elvAcl = elv90, .elv = elv90, .m_gray = m_gray_093},
	{.br = 98, .cTbl = ctbl98nit, .aid = aid77, .elvAcl = elv90, .elv = elv90, .m_gray = m_gray_098},
	{.br = 105, .cTbl = ctbl105nit, .aid = aid77, .elvAcl = elv90, .elv = elv90, .m_gray = m_gray_105},
	{.br = 111, .cTbl = ctbl111nit, .aid = aid77, .elvAcl = elv8f, .elv = elv8f, .m_gray = m_gray_111},
	{.br = 119, .cTbl = ctbl119nit, .aid = aid77, .elvAcl = elv8f, .elv = elv8f, .m_gray = m_gray_119},
	{.br = 126, .cTbl = ctbl126nit, .aid = aid77, .elvAcl = elv8e, .elv = elv8e, .m_gray = m_gray_126},
	{.br = 134, .cTbl = ctbl134nit, .aid = aid77, .elvAcl = elv8e, .elv = elv8e, .m_gray = m_gray_134},
	{.br = 143, .cTbl = ctbl143nit, .aid = aid77, .elvAcl = elv8d, .elv = elv8d, .m_gray = m_gray_143},
	{.br = 152, .cTbl = ctbl152nit, .aid = aid77, .elvAcl = elv8c, .elv = elv8c, .m_gray = m_gray_152},
	{.br = 162, .cTbl = ctbl162nit, .aid = aid77, .elvAcl = elv8c, .elv = elv8c, .m_gray = m_gray_162},
	{.br = 172, .cTbl = ctbl172nit, .aid = aid77, .elvAcl = elv8c, .elv = elv8c, .m_gray = m_gray_172},
	{.br = 183, .cTbl = ctbl183nit, .aid = aid183, .elvAcl = elv8c, .elv = elv8c, .m_gray = m_gray_183},
	{.br = 195, .cTbl = ctbl195nit, .aid = aid195, .elvAcl = elv8b, .elv = elv8b, .m_gray = m_gray_195},
	{.br = 207, .cTbl = ctbl207nit, .aid = aid207, .elvAcl = elv8a, .elv = elv8a, .m_gray = m_gray_207},
	{.br = 220, .cTbl = ctbl220nit, .aid = aid220, .elvAcl = elv8a, .elv = elv8a, .m_gray = m_gray_220},
	{.br = 234, .cTbl = ctbl234nit, .aid = aid234, .elvAcl = elv8a, .elv = elv8a, .m_gray = m_gray_234},
	{.br = 249, .cTbl = ctbl249nit, .aid = aid249, .elvAcl = elv89, .elv = elv89, .m_gray = m_gray_249},
	{.br = 265, .cTbl = ctbl265nit, .aid = aid249, .elvAcl = elv88, .elv = elv88, .m_gray = m_gray_265},
	{.br = 282, .cTbl = ctbl282nit, .aid = aid249, .elvAcl = elv87, .elv = elv87, .m_gray = m_gray_282},
	{.br = 300, .cTbl = ctbl300nit, .aid = aid249, .elvAcl = elv86, .elv = elv86, .m_gray = m_gray_300},
	{.br = 316, .cTbl = ctbl316nit, .aid = aid249, .elvAcl = elv86, .elv = elv86, .m_gray = m_gray_316},
	{.br = 333, .cTbl = ctbl333nit, .aid = aid249, .elvAcl = elv85, .elv = elv85, .m_gray = m_gray_333},
	{.br = 360, .cTbl = ctbl350nit, .aid = aid249, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_360},
	{.br = 360, .cTbl = ctbl350nit, .aid = aid249, .elvAcl = elv84, .elv = elv84, .m_gray = m_gray_360},
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

	if (dsim->priv.id[2] >= 0x02)
		diminfo = lily_dimming_info;
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

	for (i = V203; i > V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((mtp[pos] & 0x80) ? -1 : 1) * (mtp[pos] & 0x7f);
			dimming->t_gamma[i][j] = (int)center_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
		}
	}

	for (j = 0; j < CI_MAX; j++) {
		dimming->t_gamma[V0][j] = (int)center_gamma[V0][j] + temp;
		dimming->mtp[V0][j] = 0;
	}

	for (j = 0; j < CI_MAX; j++) {
		dimming->vt_mtp[j] = mtp[pos];
		pos++;
	}

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

static int ea8064g_read_init_info(struct dsim_device *dsim, unsigned char *mtp)
{
	int i = 0;
	int ret;
	struct panel_private *panel = &dsim->priv;
	unsigned char bufForCoordi[EA8064G_COORDINATE_LEN] = {0,};
	unsigned char buf[EA8064G_MTP_DATE_SIZE] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);

	/* id */
	ret = dsim_read_hl_data(dsim, EA8064G_ID_REG, EA8064G_ID_LEN, dsim->priv.id);
	if (ret != EA8064G_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < EA8064G_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	/* mtp */
	ret = dsim_read_hl_data(dsim, EA8064G_MTP_ADDR, EA8064G_MTP_DATE_SIZE, buf);
	if (ret != EA8064G_MTP_DATE_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(mtp, buf, EA8064G_MTP_SIZE);
	dsim_info("READ MTP SIZE : %d\n", EA8064G_MTP_SIZE);
	dsim_info("=========== MTP INFO ===========\n");
	for (i = 0; i < EA8064G_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

	/* elvss */
	ret = dsim_read_hl_data(dsim, ELVSS_REG, ELVSS_LEN, dsim->priv.elvss);
	if (ret != ELVSS_LEN) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}

	/* coordinate */
	ret = dsim_read_hl_data(dsim, EA8064G_COORDINATE_REG, EA8064G_COORDINATE_LEN, bufForCoordi);
	if (ret != EA8064G_COORDINATE_LEN) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];	/* X */
	dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];	/* Y */
	dsim->priv.date[0] = bufForCoordi[4];  /* year, month */
	dsim->priv.date[1] = bufForCoordi[5];  /* day */

	dsim_info("READ coordi : ");
	for (i = 0; i < 2; i++)
		dsim_info("%d, ", dsim->priv.coordinate[i]);
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

static int ea8064g_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[EA8064G_MTP_SIZE] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;

	ret = ea8064g_read_init_info(dsim, mtp);
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


static int ea8064g_displayon(struct dsim_device *dsim)
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

static int ea8064g_exit(struct dsim_device *dsim)
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

	msleep(130);

exit_err:
	return ret;
}

static int ea8064g_init(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_panel_set_brightness(dsim, 1);
	if (ret)
		dsim_err("%s : fail to set brightness\n", __func__);

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

	msleep(25);

	ret = dsim_write_hl_data(dsim, SEQ_DCDC1_GP, ARRAY_SIZE(SEQ_DCDC1_GP));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_SOURCE_CONTROL\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_DCDC1_SET, ARRAY_SIZE(SEQ_DCDC1_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_SOURCE_CONTROL\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SOURCE_CONTROL, ARRAY_SIZE(SEQ_SOURCE_CONTROL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_SOURCE_CONTROL\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_PCD_CONTROL, ARRAY_SIZE(SEQ_PCD_CONTROL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_PCD_CONTROL\n", __func__);
		goto init_exit;
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


struct dsim_panel_ops ea8064g_panel_ops = {
	.early_probe = NULL,
	.probe		= ea8064g_probe,
	.displayon	= ea8064g_displayon,
	.exit		= ea8064g_exit,
	.init		= ea8064g_init,
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

	if (!reg || !length || reg > 0xff || length > 255 || pos > 255)
		return -EINVAL;

	if (priv->state == PANEL_STATE_RESUMED)
		return -EINVAL;

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

	if (!reg || !len || reg > 0xff || len > 255)
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
		if (!reg || !len || reg > 0xff || len > 255)
			return -EINVAL;

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
static DEVICE_ATTR(read_mtp, 0220, NULL, read_mtp_store);
#ifdef CONFIG_PANEL_AID_DIMMING
static DEVICE_ATTR(aid_log, 0444, aid_log_show, NULL);
#endif
static DEVICE_ATTR(dump_register, 0644, dump_register_show, dump_register_store);

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


struct mipi_dsim_lcd_driver ea8064g_mipi_lcd_driver = {
	.early_probe = dsim_panel_early_probe,
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
	.resume		= dsim_panel_resume,
};

