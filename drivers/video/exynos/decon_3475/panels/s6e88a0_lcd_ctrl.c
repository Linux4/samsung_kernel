/*
 * drivers/video/decon_3475/panels/s6e88a0_lcd_ctrl.c
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
#include "s6e88a0_aid_dimming.h"
#endif

#include "s6e88a0_param.h"


struct lcd_info {
	struct lcd_device *ld;
	struct backlight_device *bd;
	unsigned char id[3];
	unsigned char code[5];
	unsigned char tset[8];
	unsigned char elvss[4];

	unsigned char DB[33];
	unsigned char elvss_hbm;
	unsigned char elvss_hbm_default;

	int	temperature;
	unsigned int coordinate[2];
	unsigned char date[7];
	unsigned int state;
	unsigned int auto_brightness;
	unsigned int br_index;
	unsigned int acl_enable;
	unsigned int acl_enable_force;
	unsigned int current_acl;
	unsigned int current_hbm;
	unsigned int current_acl_opr;
	unsigned int siop_enable;
	unsigned char dump_info[3];

	void *dim_data;
	void *dim_info;
	unsigned int *br_tbl;
	unsigned char **hbm_tbl;
	unsigned char **acl_cutoff_tbl;
	unsigned char **acl_opr_tbl;
	struct mutex lock;
	struct dsim_device *dsim;
};


#ifdef CONFIG_PANEL_AID_DIMMING
static unsigned int get_actual_br_value(struct lcd_info *lcd, int index)
{
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)lcd->dim_info;

	if (dimming_info == NULL) {
		dev_err(&lcd->ld->dev, "%s : dimming info is NULL\n", __func__);
		goto get_br_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return dimming_info[index].br;

get_br_err:
	return 0;
}
static unsigned char *get_gamma_from_index(struct lcd_info *lcd, int index)
{
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)lcd->dim_info;

	if (dimming_info == NULL) {
		dev_err(&lcd->ld->dev, "%s : dimming info is NULL\n", __func__);
		goto get_gamma_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return (unsigned char *)dimming_info[index].gamma;

get_gamma_err:
	return NULL;
}

static unsigned char *get_aid_from_index(struct lcd_info *lcd, int index)
{
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)lcd->dim_info;

	if (dimming_info == NULL) {
		dev_err(&lcd->ld->dev, "%s : dimming info is NULL\n", __func__);
		goto get_aid_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return (u8 *)dimming_info[index].aid;

get_aid_err:
	return NULL;
}

static unsigned char *get_elvss_from_index(struct lcd_info *lcd, int index, int acl)
{
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)lcd->dim_info;

	if (dimming_info == NULL) {
		dev_err(&lcd->ld->dev, "%s : dimming info is NULL\n", __func__);
		goto get_elvess_err;
	}

	if (acl)
		return (unsigned char *)dimming_info[index].elvAcl;
	else
		return (unsigned char *)dimming_info[index].elv;

get_elvess_err:
	return NULL;
}

static void dsim_panel_gamma_ctrl(struct lcd_info *lcd)
{
	int level = LEVEL_IS_HBM(lcd->auto_brightness), i = 0;
	u8 HBM_W[34] = {0xCA, };
	u8 *gamma = NULL;

	if (level) {
		memcpy(&HBM_W[1], lcd->DB, 21);
		for (i = 22; i <= 30; i++)
			HBM_W[i] = 0x80;
		HBM_W[31] = 0x00;
		HBM_W[32] = 0x00;
		HBM_W[33] = 0x00;

		if (dsim_write_hl_data(lcd->dsim, HBM_W, ARRAY_SIZE(HBM_W)) < 0)
			dev_err(&lcd->ld->dev, "%s : failed to write gamma\n", __func__);

	} else {
		gamma = get_gamma_from_index(lcd, lcd->br_index);
		if (gamma == NULL) {
			dev_err(&lcd->ld->dev, "%s : failed to get gamma\n", __func__);
			return;
		}
		if (dsim_write_hl_data(lcd->dsim, gamma, GAMMA_CMD_CNT) < 0)
			dev_err(&lcd->ld->dev, "%s : failed to write gamma\n", __func__);
	}
}

static void dsim_panel_aid_ctrl(struct lcd_info *lcd)
{
	u8 *aid = NULL;

	aid = get_aid_from_index(lcd, lcd->br_index);
	if (aid == NULL) {
		dev_err(&lcd->ld->dev, "%s : failed to get aid value\n", __func__);
		return;
	}
	if (dsim_write_hl_data(lcd->dsim, aid, AID_CMD_CNT) < 0)
		dev_err(&lcd->ld->dev, "%s : failed to write gamma\n", __func__);
}

static void dsim_panel_set_elvss(struct lcd_info *lcd)
{
	u8 *elvss = NULL;
	u8 elvss_val[3] = {0, };
	u8 B6_W[18] = {0xB6,
					0x2C, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x0A, 0xAA, 0xAF, 0x0F, 0x00};
	u8 B6_GW[2] = {0xB6, };
	int level = LEVEL_IS_HBM(lcd->auto_brightness);
	int real_br = 0;

	real_br = get_actual_br_value(lcd, lcd->br_index);
	elvss = get_elvss_from_index(lcd, lcd->br_index, lcd->acl_enable);

	if (elvss == NULL) {
		dev_err(&lcd->ld->dev, "%s : failed to get elvss value\n", __func__);
		return;
	}

	if (level) {
		B6_W[17] = lcd->elvss_hbm;

		if (dsim_write_hl_data(lcd->dsim, B6_W, ARRAY_SIZE(B6_W)) < 0)
			dev_err(&lcd->ld->dev, "%s : failed to write elvss\n", __func__);

	} else {

		B6_GW[1] = lcd->elvss_hbm_default;

		if (real_br <= 29) {
			if (lcd->temperature <= -20)
				B6_GW[1] = elvss[5];
			else if (lcd->temperature > -20 && lcd->temperature <= 0)
				B6_GW[1] = elvss[4];
			else
				B6_GW[1] = elvss[3];
		}

		elvss_val[0] = elvss[0];
		elvss_val[1] = elvss[1];
		elvss_val[2] = elvss[2];

		if (dsim_write_hl_data(lcd->dsim, elvss_val, ELVSS_LEN) < 0)
			dev_err(&lcd->ld->dev, "%s : failed to write elvss\n", __func__);

		if (dsim_write_hl_data(lcd->dsim, SEQ_ELVSS_GLOBAL, ARRAY_SIZE(SEQ_ELVSS_GLOBAL)) < 0)
			dev_err(&lcd->ld->dev, "%s : failed to SEQ_ELVSS_GLOBAL\n", __func__);

		if (dsim_write_hl_data(lcd->dsim, B6_GW, 2) < 0)
			dev_err(&lcd->ld->dev, "%s : failed to write elvss\n", __func__);

	}
}

static int dsim_panel_set_acl(struct lcd_info *lcd, int force)
{
	int ret = 0, level = ACL_STATUS_15P;

	if (lcd->siop_enable || LEVEL_IS_HBM(lcd->auto_brightness))  /* auto acl or hbm is acl on */
		goto acl_update;

	if (!lcd->acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	if (force || lcd->current_acl != lcd->acl_cutoff_tbl[level][1]) {
		if (dsim_write_hl_data(lcd->dsim, lcd->acl_cutoff_tbl[level], 2) < 0) {
			dev_err(&lcd->ld->dev, "failed to write acl command.\n");
			ret = -EPERM;
			goto exit;
		}
		lcd->current_acl = lcd->acl_cutoff_tbl[level][1];
		dev_info(&lcd->ld->dev, "acl: %d, auto_brightness: %d\n", lcd->current_acl, lcd->auto_brightness);
	}
exit:
	if (!ret)
		ret = -EPERM;
	return ret;
}

static int dsim_panel_set_tset(struct lcd_info *lcd, int force)
{
	int ret = 0;
	int tset = 0;
	unsigned char SEQ_TSET[TSET_LEN] = {TSET_REG, 0x38, 0x0B, 0x40, 0x00, 0xA8, 0x00};

	tset = ((lcd->temperature >= 0) ? 0 : BIT(7)) | abs(lcd->temperature);

	if (force || lcd->tset[0] != tset) {
		lcd->tset[0] = SEQ_TSET[6] = tset;

		if (dsim_write_hl_data(lcd->dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET)) < 0) {
			dev_err(&lcd->ld->dev, "failed to write tset command.\n");
			ret = -EPERM;
		}

		dev_info(&lcd->ld->dev, "temperature: %d, tset: %d\n", lcd->temperature, SEQ_TSET[6]);
	}
	return ret;
}

static int low_level_set_brightness(struct lcd_info *lcd, int force)
{

	if (dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dev_err(&lcd->ld->dev, "%s : failed to write F0 on command.\n", __func__);

	dsim_panel_gamma_ctrl(lcd);

	dsim_panel_aid_ctrl(lcd);

	dsim_panel_set_elvss(lcd);

	dsim_panel_set_acl(lcd, force);

	if (dsim_write_hl_data(lcd->dsim, SEQ_GAMMA_UPDATE_S6E88A0, ARRAY_SIZE(SEQ_GAMMA_UPDATE_S6E88A0)) < 0)
		dev_err(&lcd->ld->dev, "%s : failed to write SEQ_GAMMA_UPDATE_S6E88A0 on command.\n", __func__);

	dsim_panel_set_tset(lcd, force);

	if (dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dev_err(&lcd->ld->dev, "%s : failed to write F0 on command\n", __func__);

	return 0;
}

static int get_acutal_br_index(struct lcd_info *lcd, int br)
{
	int i;
	int min;
	int gap;
	int index = 0;
	struct SmtDimInfo *dimming_info = lcd->dim_info;

	if (dimming_info == NULL) {
		dev_err(&lcd->ld->dev, "%s : dimming_info is NULL\n", __func__);
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

static int dsim_panel_set_brightness(struct lcd_info *lcd, int force)
{
	int ret = 0;

	struct dim_data *dimming;
	int p_br = lcd->bd->props.brightness;
	int acutal_br = 0;
	int real_br = 0;
	int prev_index = lcd->br_index;

	dimming = (struct dim_data *)lcd->dim_data;
	if ((dimming == NULL) || (lcd->br_tbl == NULL)) {
		dev_info(&lcd->ld->dev, "%s : this panel does not support dimming\n", __func__);
		return ret;
	}

	acutal_br = lcd->br_tbl[p_br];
	lcd->br_index = get_acutal_br_index(lcd, acutal_br);
	real_br = get_actual_br_value(lcd, lcd->br_index);
	lcd->acl_enable = ACL_IS_ON(real_br);

	if (LEVEL_IS_HBM(lcd->auto_brightness) && (p_br == lcd->bd->props.max_brightness)) {
		lcd->br_index = HBM_INDEX;
		lcd->acl_enable = 1;				/* hbm is acl on */
	}
	if (lcd->siop_enable)					/* check auto acl */
		lcd->acl_enable = 1;

	if (lcd->state != PANEL_STATE_RESUMED) {
		dev_info(&lcd->ld->dev, "%s : panel is not active state..\n", __func__);
		goto set_br_exit;
	}

	dev_info(&lcd->ld->dev, "%s : platform : %d, : mapping : %d, real : %d, index : %d\n",
		__func__, p_br, acutal_br, real_br, lcd->br_index);

	if (!force && lcd->br_index == prev_index)
		goto set_br_exit;

	if ((acutal_br == 0) || (real_br == 0))
		goto set_br_exit;

	mutex_lock(&lcd->lock);

	ret = low_level_set_brightness(lcd, force);
	if (ret)
		dev_err(&lcd->ld->dev, "%s failed to set brightness : %d\n", __func__, acutal_br);

	mutex_unlock(&lcd->lock);

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
	struct lcd_info *lcd = bl_get_data(bd);

	if (brightness < UI_MIN_BRIGHTNESS || brightness > UI_MAX_BRIGHTNESS) {
		pr_alert("Brightness should be in the range of 0 ~ 255\n");
		ret = -EINVAL;
		goto exit_set;
	}

	ret = dsim_panel_set_brightness(lcd, 0);
	if (ret) {
		dev_err(&lcd->ld->dev, "%s : failed to set brightness\n", __func__);
		goto exit_set;
	}
exit_set:
	return ret;
}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
};


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

static struct SmtDimInfo daisy_dimming_info[MAX_BR_INFO + 1] = {				/* add hbm array */
	{.br = 5, .cTbl = ctbl5nit, .aid = aid5, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_005},
	{.br = 6, .cTbl = ctbl6nit, .aid = aid6, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_006},
	{.br = 7, .cTbl = ctbl7nit, .aid = aid7, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_007},
	{.br = 8, .cTbl = ctbl8nit, .aid = aid8, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_008},
	{.br = 9, .cTbl = ctbl9nit, .aid = aid9, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_009},
	{.br = 10, .cTbl = ctbl10nit, .aid = aid10, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_010},
	{.br = 11, .cTbl = ctbl11nit, .aid = aid11, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_011},
	{.br = 12, .cTbl = ctbl12nit, .aid = aid12, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_012},
	{.br = 13, .cTbl = ctbl13nit, .aid = aid13, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_013},
	{.br = 14, .cTbl = ctbl14nit, .aid = aid14, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_014},
	{.br = 15, .cTbl = ctbl15nit, .aid = aid15, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_015},
	{.br = 16, .cTbl = ctbl16nit, .aid = aid16, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_016},
	{.br = 17, .cTbl = ctbl17nit, .aid = aid17, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_017},
	{.br = 19, .cTbl = ctbl19nit, .aid = aid19, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_019},
	{.br = 20, .cTbl = ctbl20nit, .aid = aid20, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_020},
	{.br = 21, .cTbl = ctbl21nit, .aid = aid21, .elvAcl = elv_0D, .elv = elv_0D, .m_gray = m_gray_021},
	{.br = 22, .cTbl = ctbl22nit, .aid = aid22, .elvAcl = elv_0F, .elv = elv_0F, .m_gray = m_gray_022},
	{.br = 24, .cTbl = ctbl24nit, .aid = aid24, .elvAcl = elv_11, .elv = elv_11, .m_gray = m_gray_024},
	{.br = 25, .cTbl = ctbl25nit, .aid = aid25, .elvAcl = elv_13, .elv = elv_13, .m_gray = m_gray_025},
	{.br = 27, .cTbl = ctbl27nit, .aid = aid27, .elvAcl = elv_15, .elv = elv_15, .m_gray = m_gray_027},
	{.br = 29, .cTbl = ctbl29nit, .aid = aid29, .elvAcl = elv_17, .elv = elv_17, .m_gray = m_gray_029},
	{.br = 30, .cTbl = ctbl30nit, .aid = aid30, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_030},
	{.br = 32, .cTbl = ctbl32nit, .aid = aid32, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_032},
	{.br = 34, .cTbl = ctbl34nit, .aid = aid34, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_034},
	{.br = 37, .cTbl = ctbl37nit, .aid = aid37, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_037},
	{.br = 39, .cTbl = ctbl39nit, .aid = aid39, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_039},
	{.br = 41, .cTbl = ctbl41nit, .aid = aid41, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_041},
	{.br = 44, .cTbl = ctbl44nit, .aid = aid44, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_044},
	{.br = 47, .cTbl = ctbl47nit, .aid = aid47, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_047},
	{.br = 50, .cTbl = ctbl50nit, .aid = aid50, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_050},
	{.br = 53, .cTbl = ctbl53nit, .aid = aid53, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_053},
	{.br = 56, .cTbl = ctbl56nit, .aid = aid56, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_056},
	{.br = 60, .cTbl = ctbl60nit, .aid = aid60, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_060},
	{.br = 64, .cTbl = ctbl64nit, .aid = aid64, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_064},
	{.br = 68, .cTbl = ctbl68nit, .aid = aid68, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_068},
	{.br = 72, .cTbl = ctbl72nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_072},
	{.br = 77, .cTbl = ctbl77nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_077},
	{.br = 82, .cTbl = ctbl82nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_082},
	{.br = 87, .cTbl = ctbl87nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_087},
	{.br = 93, .cTbl = ctbl93nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_093},
	{.br = 98, .cTbl = ctbl98nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_098},
	{.br = 105, .cTbl = ctbl105nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18, .m_gray =  m_gray_105},
	{.br = 111, .cTbl = ctbl111nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18, .m_gray =  m_gray_111},
	{.br = 119, .cTbl = ctbl119nit, .aid = aid119, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_119},
	{.br = 126, .cTbl = ctbl126nit, .aid = aid126, .elvAcl = elv_18, .elv = elv_18, .m_gray = m_gray_126},
	{.br = 134, .cTbl = ctbl134nit, .aid = aid134, .elvAcl = elv_17, .elv = elv_17, .m_gray = m_gray_134},
	{.br = 143, .cTbl = ctbl143nit, .aid = aid143, .elvAcl = elv_17, .elv = elv_17, .m_gray = m_gray_143},
	{.br = 152, .cTbl = ctbl152nit, .aid = aid152, .elvAcl = elv_16, .elv = elv_16, .m_gray = m_gray_152},
	{.br = 162, .cTbl = ctbl162nit, .aid = aid162, .elvAcl = elv_16, .elv = elv_16, .m_gray = m_gray_162},
	{.br = 172, .cTbl = ctbl172nit, .aid = aid172, .elvAcl = elv_15, .elv = elv_15, .m_gray = m_gray_172},
	{.br = 183, .cTbl = ctbl183nit, .aid = aid183, .elvAcl = elv_15, .elv = elv_15, .m_gray = m_gray_183},
	{.br = 195, .cTbl = ctbl195nit, .aid = aid183, .elvAcl = elv_14, .elv = elv_14, .m_gray = m_gray_195},
	{.br = 207, .cTbl = ctbl207nit, .aid = aid183, .elvAcl = elv_13, .elv = elv_13, .m_gray = m_gray_207},
	{.br = 220, .cTbl = ctbl220nit, .aid = aid183, .elvAcl = elv_12, .elv = elv_12, .m_gray = m_gray_220},
	{.br = 234, .cTbl = ctbl234nit, .aid = aid183, .elvAcl = elv_12, .elv = elv_12, .m_gray = m_gray_234},
	{.br = 249, .cTbl = ctbl249nit, .aid = aid183, .elvAcl = elv_11, .elv = elv_11, .m_gray = m_gray_249},
	{.br = 265, .cTbl = ctbl265nit, .aid = aid183, .elvAcl = elv_10, .elv = elv_10, .m_gray = m_gray_265},
	{.br = 282, .cTbl = ctbl282nit, .aid = aid183, .elvAcl = elv_0F, .elv = elv_0F, .m_gray = m_gray_282},
	{.br = 300, .cTbl = ctbl300nit, .aid = aid183, .elvAcl = elv_0E, .elv = elv_0E, .m_gray = m_gray_300},
	{.br = 316, .cTbl = ctbl316nit, .aid = aid183, .elvAcl = elv_0D, .elv = elv_0D, .m_gray = m_gray_316},
	{.br = 333, .cTbl = ctbl333nit, .aid = aid183, .elvAcl = elv_0C, .elv = elv_0C, .m_gray = m_gray_333},
	{.br = 350, .cTbl = ctbl350nit, .aid = aid183, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_350},
	{.br = 350, .cTbl = ctbl350nit, .aid = aid183, .elvAcl = elv_0B, .elv = elv_0B, .m_gray = m_gray_350},
};

static int init_dimming(struct lcd_info *lcd, u8 *mtp)
{
	int i, j;
	int pos = 0;
	int ret = 0;
	short temp;
	struct dim_data *dimming;
	struct SmtDimInfo *diminfo = NULL;

	dimming = kzalloc(sizeof(struct dim_data), GFP_KERNEL);
	if (!dimming) {
		dev_err(&lcd->ld->dev, "failed to allocate memory for dim data\n");
		ret = -ENOMEM;
		goto error;
	}

	diminfo = daisy_dimming_info;
	dev_info(&lcd->ld->dev, "%s : s6e88a0\n", __func__);

	lcd->dim_data = (void *)dimming;
	lcd->dim_info = (void *)diminfo; /* dimming info */

	lcd->br_tbl = (unsigned int *)br_tbl; /* backlight table */
	lcd->hbm_tbl = (unsigned char **)HBM_TABLE; /* command hbm on and off */
	lcd->acl_cutoff_tbl = (unsigned char **)ACL_CUTOFF_TABLE; /* ACL on and off command */
	lcd->acl_opr_tbl = (unsigned char **)ACL_OPR_TABLE;

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
		dev_info(&lcd->ld->dev, "Gamma : %3d %3d %3d : %3x %3x %3x\n",
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
			dev_err(&lcd->ld->dev, "failed to calculate gamma : index : %d\n", i);
			goto error;
		}
	}
	memcpy(diminfo[i].gamma, SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET));
error:
	return ret;
}
#endif

static int s6e88a0_read_init_info(struct lcd_info *lcd, unsigned char *mtp)
{
	int i = 0;
	int ret;
	struct panel_private *priv = &lcd->dsim->priv;
	unsigned char bufForCoordi[S6E88A0_COORDINATE_LEN] = {0,};
	unsigned char bufForDate[S6E88A0_DATE_LEN] = {0,};
	unsigned char buf[S6E88A0_MTP_DATE_SIZE] = {0, };
	unsigned char buf_HBM_1[HBM_GAMMA_READ_SIZE_1] = {0, };
	unsigned char buf_HBM_2[HBM_GAMMA_READ_SIZE_2] = {0, };

	dev_info(&lcd->ld->dev, "MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0)
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);

	/* id */
	ret = dsim_read_hl_data(lcd->dsim, S6E88A0_ID_REG, S6E88A0_ID_LEN, lcd->id);
	if (ret != S6E88A0_ID_LEN) {
		dev_err(&lcd->ld->dev, "%s : can't find connected panel. check panel connection\n", __func__);
		priv->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	dev_info(&lcd->ld->dev, "READ ID : ");
	for (i = 0; i < S6E88A0_ID_LEN; i++)
		dev_info(&lcd->ld->dev, "%02x, ", lcd->id[i]);
	dev_info(&lcd->ld->dev, "\n");

	/* mtp */
	ret = dsim_read_hl_data(lcd->dsim, S6E88A0_MTP_ADDR, S6E88A0_MTP_DATE_SIZE, buf);
	if (ret != S6E88A0_MTP_DATE_SIZE) {
		dev_err(&lcd->ld->dev, "failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(mtp, buf, S6E88A0_MTP_SIZE);
	dev_info(&lcd->ld->dev, "READ MTP SIZE : %d\n", S6E88A0_MTP_SIZE);
	dev_info(&lcd->ld->dev, "=========== MTP INFO ===========\n");
	for (i = 0; i < S6E88A0_MTP_SIZE; i++)
		dev_info(&lcd->ld->dev, "MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

	/* elvss */
	ret = dsim_read_hl_data(lcd->dsim, ELVSS_REG, ELVSS_LEN, lcd->elvss);
	if (ret != ELVSS_LEN) {
		dev_err(&lcd->ld->dev, "failed to read mtp, check panel connection\n");
		goto read_fail;
	}

	/* date */
	ret = dsim_read_hl_data(lcd->dsim, S6E88A0_DATE_REG, S6E88A0_DATE_LEN, bufForDate);
	if (ret != S6E88A0_DATE_LEN) {
		dev_err(&lcd->ld->dev, "failed to read date on command.\n");
		goto read_fail;
	}
	lcd->date[0] = bufForDate[23] >> 4;
	lcd->date[1] = bufForDate[23] & 0x0f;
	lcd->date[2] = bufForDate[24];

	/* coordinate */
	ret = dsim_read_hl_data(lcd->dsim, S6E88A0_COORDINATE_REG, S6E88A0_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E88A0_COORDINATE_LEN) {
		dev_err(&lcd->ld->dev, "failed to read coordinate on command.\n");
		goto read_fail;
	}
	lcd->coordinate[0] = bufForCoordi[20] << 8 | bufForCoordi[21];	/* X */
	lcd->coordinate[1] = bufForCoordi[22] << 8 | bufForCoordi[23];	/* Y */
	dev_info(&lcd->ld->dev, "READ coordi : ");
	for (i = 0; i < 2; i++)
		dev_info(&lcd->ld->dev, "%d, ", lcd->coordinate[i]);
	dev_info(&lcd->ld->dev, "\n");

	/* Read HBM from B5h */
	ret = dsim_read_hl_data(lcd->dsim, HBM_GAMMA_READ_ADD_1, HBM_GAMMA_READ_SIZE_1, buf_HBM_1);
	if (ret != HBM_GAMMA_READ_SIZE_1) {
		dev_err(&lcd->ld->dev, "failed to read HBM from B5h.\n");
		goto read_fail;
	}
	for (i = 0; i < 6; i++)
		lcd->DB[i] = buf_HBM_1[i+12];
	for (i = 6; i < 9; i++)
		lcd->DB[i] = buf_HBM_1[i+19];

	/* Read elvss setting HBM */
		lcd->elvss_hbm = buf_HBM_1[18];

	/* Read HBM from B6h */
	ret = dsim_read_hl_data(lcd->dsim, HBM_GAMMA_READ_ADD_2, HBM_GAMMA_READ_SIZE_2, buf_HBM_2);
	if (ret != HBM_GAMMA_READ_SIZE_2) {
		dev_err(&lcd->ld->dev, "failed to read HBM from B6h.\n");
		goto read_fail;
	}
	for (i = 0; i < 12; i++)
		lcd->DB[i+9] = buf_HBM_2[i+17];

	/* Read default 0xB6 17th value. */
	lcd->elvss_hbm_default = buf_HBM_2[16];

	ret = dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto read_fail;
	}
read_exit:
	return 0;

read_fail:
	return -ENODEV;
}

static int s6e88a0_displayon(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(lcd->dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : DISPLAY_ON\n", __func__);
		goto displayon_err;
	}

	usleep_range(12000, 13000);

displayon_err:
	return ret;
}

static int s6e88a0_exit(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "MDD : %s was called\n", __func__);
	ret = dsim_write_hl_data(lcd->dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : DISPLAY_OFF\n", __func__);
		goto exit_err;
	}

	if (lcd->id)
		msleep(20);

	ret = dsim_write_hl_data(lcd->dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SLEEP_IN\n", __func__);
		goto exit_err;
	}

	msleep(120);

exit_err:
	return ret;
}

static int s6e88a0_init(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "MDD : %s was called : s6e88a0\n", __func__);

	usleep_range(5000, 6000);

	ret = dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_TEST_KEY_ON_FC\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_S_WIRE, ARRAY_SIZE(SEQ_S_WIRE));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_S_WIRE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}

	msleep(20);

	ret = dsim_write_hl_data(lcd->dsim, SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_AID_SET, ARRAY_SIZE(SEQ_AID_SET));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_AID_SET\n", __func__);
		goto init_exit;
	}

	/* rev.A panel need ACL MTP re-write */

	if (lcd->id[2] == 0x00) {
		ret = dsim_write_hl_data(lcd->dsim, SEQ_ACL_MTP, ARRAY_SIZE(SEQ_ACL_MTP));
		if (ret < 0) {
			dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_ACL_MTP\n", __func__);
			goto init_exit;
		}
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_ELVSS_SET_S6E88A0, ARRAY_SIZE(SEQ_ELVSS_SET_S6E88A0));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_ELVSS_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_GAMMA_UPDATE_S6E88A0, ARRAY_SIZE(SEQ_GAMMA_UPDATE_S6E88A0));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_ACL_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_ACL_SET_S6E88A0, ARRAY_SIZE(SEQ_ACL_SET_S6E88A0));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_ACL_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_TSET\n", __func__);
		goto init_exit;
	}

	msleep(120);

	ret = dsim_write_hl_data(lcd->dsim, SEQ_MDNIE_1, ARRAY_SIZE(SEQ_MDNIE_1));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_MDNIE_1\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_MDNIE_2, ARRAY_SIZE(SEQ_MDNIE_2));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_MDNIE_2\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dev_err(&lcd->ld->dev, "%s : failed to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

init_exit:
	return ret;
}

static int s6e88a0_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *priv = &dsim->priv;
	struct lcd_info *lcd = dsim->priv.par;
	unsigned char mtp[S6E88A0_MTP_SIZE] = {0, };

	dev_info(&lcd->ld->dev, "MDD : %s was called\n", __func__);

	priv->lcdConnected = PANEL_CONNECTED;
	lcd->bd->props.max_brightness = UI_MAX_BRIGHTNESS;
	lcd->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;
	lcd->dsim = dsim;
	lcd->state = PANEL_STATE_RESUMED;
	lcd->temperature = NORMAL_TEMPERATURE;
	lcd->acl_enable = 0;
	lcd->current_acl = 0;
	lcd->auto_brightness = 0;
	lcd->siop_enable = 0;
	lcd->current_hbm = 0;

	s6e88a0_read_init_info(lcd, mtp);
	if (priv->lcdConnected == PANEL_DISCONNEDTED) {
		dev_err(&lcd->ld->dev, "dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}

#ifdef CONFIG_PANEL_AID_DIMMING
	ret = init_dimming(lcd, mtp);
	if (ret)
		dev_err(&lcd->ld->dev, "%s : failed to generate gamma tablen\n", __func__);
#endif

	dsim_panel_set_brightness(lcd, 1);

probe_exit:
	return ret;
}


static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "SDC_%02X%02X%02X\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%x %x %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);

	return strlen(buf);
}

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	char *pos = buf;
	int nit, i, br_index;

	if (IS_ERR_OR_NULL(lcd->br_tbl))
		return strlen(buf);

	for (i = 0; i <= UI_MAX_BRIGHTNESS; i++) {
		nit = lcd->br_tbl[i];
		br_index = get_acutal_br_index(lcd, nit);
		nit = get_actual_br_value(lcd, br_index);
		pos += sprintf(pos, "%3d %3d\n", i, nit);
	}
	return pos - buf;
}

static ssize_t auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->auto_brightness);

	return strlen(buf);
}

static ssize_t auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->auto_brightness != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->auto_brightness, value);
			mutex_lock(&lcd->lock);
			lcd->auto_brightness = value;
			mutex_unlock(&lcd->lock);
			dsim_panel_set_brightness(lcd, 0);
		}
	}
	return size;
}

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->siop_enable);

	return strlen(buf);
}

static ssize_t siop_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->siop_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->siop_enable, value);
			mutex_lock(&lcd->lock);
			lcd->siop_enable = value;
			mutex_unlock(&lcd->lock);

			dsim_panel_set_brightness(lcd, 1);
		}
	}
	return size;
}

static ssize_t power_reduce_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", lcd->acl_enable);

	return strlen(buf);
}

static ssize_t power_reduce_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);

	if (rc < 0)
		return rc;
	else {
		if (lcd->acl_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->acl_enable, value);
			mutex_lock(&lcd->lock);
			lcd->acl_enable = value;
			mutex_unlock(&lcd->lock);
			dsim_panel_set_brightness(lcd, 1);
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
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoint(buf, 10, &value);

	if (rc < 0)
		return rc;
	else {
		mutex_lock(&lcd->lock);
		lcd->temperature = value;
		mutex_unlock(&lcd->lock);

		dsim_panel_set_brightness(lcd, 1);

		dev_info(dev, "%s: %d, %d\n", __func__, value, lcd->temperature);
	}

	return size;
}

static ssize_t color_coordinate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%u, %u\n", lcd->coordinate[0], lcd->coordinate[1]);
	return strlen(buf);
}

static ssize_t manufacture_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	u16 year;
	u8 month, day, hour, min;

	year = lcd->date[0] + 2011;
	month = lcd->date[1];
	day = lcd->date[2];
	hour = lcd->date[3];
	min = lcd->date[3];

	sprintf(buf, "%d, %d, %d, %d:%d\n", year, month, day, hour, min);
	return strlen(buf);
}


#ifdef CONFIG_PANEL_AID_DIMMING
static void show_aid_log(struct lcd_info *lcd)
{
	int i, j, k;
	struct dim_data *dim_data = NULL;
	struct SmtDimInfo *dimming_info = NULL;
	u8 temp[256];


	dim_data = (struct dim_data *)(lcd->dim_data);
	if (dim_data == NULL) {
		dev_info(&lcd->ld->dev, "%s:dimming is NULL\n", __func__);
		return;
	}

	dimming_info = (struct SmtDimInfo *)(lcd->dim_info);
	if (dimming_info == NULL) {
		dev_info(&lcd->ld->dev, "%s:dimming is NULL\n", __func__);
		return;
	}

	dev_info(&lcd->ld->dev, "MTP VT : %d %d %d\n",
			dim_data->vt_mtp[CI_RED], dim_data->vt_mtp[CI_GREEN], dim_data->vt_mtp[CI_BLUE]);

	for (i = 0; i < VMAX; i++) {
		dev_info(&lcd->ld->dev, "MTP V%d : %4d %4d %4d\n",
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
		dev_info(&lcd->ld->dev, "nit :%3d %s\n", dimming_info[i].br, temp);
	}
}


static ssize_t aid_log_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	show_aid_log(lcd);
	return strlen(buf);
}
#endif

static ssize_t manufacture_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X\n",
		lcd->code[0], lcd->code[1], lcd->code[2], lcd->code[3], lcd->code[4]);

	return strlen(buf);
}

static int dsim_read_hl_data_offset(struct lcd_info *lcd, u8 addr, u32 size, u8 *buf, u32 offset)
{
	unsigned char wbuf[] = {0xB0, 0};
	int ret = 0;

	wbuf[1] = offset;

	ret = dsim_write_hl_data(lcd->dsim, wbuf, ARRAY_SIZE(wbuf));
	if (ret < 0)
		dev_err(&lcd->ld->dev, "%s: failed to write wbuf\n", __func__);

	ret = dsim_read_hl_data(lcd->dsim, addr, size, buf);
	if (ret < 1)
		dev_err(&lcd->ld->dev, "%s: fail\n", __func__);

	return ret;
}

static ssize_t dump_register_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char *pos = buf;
	u8 reg, len, offset;
	int ret, i;
	u8 *dump = NULL;

	reg = lcd->dump_info[0];
	len = lcd->dump_info[1];
	offset = lcd->dump_info[2];

	if (!reg || !len || reg > 0xff || len > 255 || offset > 255)
		goto exit;

	dump = kcalloc(len, sizeof(u8), GFP_KERNEL);

	if (lcd->state == PANEL_STATE_RESUMED) {
		if (dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
			dev_err(&lcd->ld->dev, "failed to write test on f0 command.\n");
		if (dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
			dev_err(&lcd->ld->dev, "failed to write test on fc command.\n");

		if (offset)
			ret = dsim_read_hl_data_offset(lcd, reg, len, dump, offset);
		else
			ret = dsim_read_hl_data(lcd->dsim, reg, len, dump);

		if (dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
			dev_err(&lcd->ld->dev, "failed to write test off fc command.\n");
		if (dsim_write_hl_data(lcd->dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
			dev_err(&lcd->ld->dev, "failed to write test off f0 command.\n");
	}

	pos += sprintf(pos, "+ [%02X]\n", reg);
	for (i = 0; i < len; i++)
		pos += sprintf(pos, "%2d: %02x\n", i + offset + 1, dump[i]);
	pos += sprintf(pos, "- [%02X]\n", reg);

	dev_info(&lcd->ld->dev, "+ [%02X]\n", reg);
	for (i = 0; i < len; i++)
		dev_info(&lcd->ld->dev, "%2d: %02x\n", i + offset + 1, dump[i]);
	dev_info(&lcd->ld->dev, "- [%02X]\n", reg);

	kfree(dump);
exit:
	return pos - buf;
}

static ssize_t dump_register_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	unsigned int reg, len, offset;
	int ret;

	ret = sscanf(buf, "%x %d %d", &reg, &len, &offset);

	if (ret == 2)
		offset = 0;

	dev_info(dev, "%s: %x %d %d\n", __func__, reg, len, offset);

	if (ret < 0)
		return ret;
	else {
		if (!reg || !len || reg > 0xff || len > 255 || offset > 255)
			return -EINVAL;

		lcd->dump_info[0] = reg;
		lcd->dump_info[1] = len;
		lcd->dump_info[2] = offset;
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
#ifdef CONFIG_PANEL_AID_DIMMING
static DEVICE_ATTR(aid_log, 0444, aid_log_show, NULL);
#endif
static DEVICE_ATTR(dump_register, 0664, dump_register_show, dump_register_store);

static struct attribute *lcd_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_window_type.attr,
	&dev_attr_manufacture_code.attr,
	&dev_attr_brightness_table.attr,
	&dev_attr_siop_enable.attr,
	&dev_attr_power_reduce.attr,
	&dev_attr_temperature.attr,
	&dev_attr_color_coordinate.attr,
	&dev_attr_manufacture_date.attr,
	&dev_attr_aid_log.attr,
	&dev_attr_dump_register.attr,
	NULL,
};

static struct attribute *backlight_sysfs_attributes[] = {
	&dev_attr_auto_brightness.attr,
	NULL,
};

static const struct attribute_group lcd_sysfs_attr_group = {
	.attrs = lcd_sysfs_attributes,
};

static const struct attribute_group backlight_sysfs_attr_group = {
	.attrs = backlight_sysfs_attributes,
};

static void lcd_init_sysfs(struct lcd_info *lcd)
{
	int ret = 0;

	ret = sysfs_create_group(&lcd->ld->dev.kobj, &lcd_sysfs_attr_group);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add lcd sysfs\n");

	ret = sysfs_create_group(&lcd->bd->dev.kobj, &backlight_sysfs_attr_group);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add backlight sysfs\n");
}


static int dsim_panel_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct lcd_info *lcd;

	dsim->priv.par = lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	if (!lcd) {
		pr_err("%s : failed to allocate for lcd\n", __func__);
		ret = -ENOMEM;
		goto probe_err;
	}

	dsim->lcd = lcd->ld = lcd_device_register("panel", dsim->dev, lcd, NULL);
	if (IS_ERR(lcd->ld)) {
		pr_err("%s : failed to register lcd device\n", __func__);
		ret = PTR_ERR(lcd->ld);
		goto probe_err;
	}

	lcd->bd = backlight_device_register("panel", dsim->dev, lcd, &panel_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("%s : failed to register backlight device\n", __func__);
		ret = PTR_ERR(lcd->bd);
		goto probe_err;
	}

	mutex_init(&lcd->lock);

	ret = s6e88a0_probe(dsim);
	if (ret) {
		dev_err(&lcd->ld->dev, "%s : failed to probe panel\n", __func__);
		goto probe_err;
	}

#if defined(CONFIG_EXYNOS_DECON_LCD_SYSFS)
	lcd_init_sysfs(lcd);
#endif

	dev_info(&lcd->ld->dev, "%s: %s: done\n", kbasename(__FILE__), __func__);
probe_err:
	return ret;
}

static int dsim_panel_displayon(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;
	int ret = 0;

	if (lcd->state == PANEL_STATE_SUSPENED) {
		ret = s6e88a0_init(lcd);
		if (ret) {
			dev_err(&lcd->ld->dev, "%s : failed to panel init\n", __func__);
			lcd->state = PANEL_STATE_SUSPENED;
			goto displayon_err;
		}
	}

	ret = s6e88a0_displayon(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "%s : failed to panel display on\n", __func__);
		goto displayon_err;
	}

	lcd->state = PANEL_STATE_RESUMED;
	dsim_panel_set_brightness(lcd, 1);

displayon_err:
	return ret;
}

static int dsim_panel_suspend(struct dsim_device *dsim)
{
	struct lcd_info *lcd = dsim->priv.par;
	int ret = 0;

	mutex_lock(&lcd->lock);

	if (lcd->state == PANEL_STATE_SUSPENED)
		goto suspend_err;

	lcd->state = PANEL_STATE_SUSPENDING;

	ret = s6e88a0_exit(lcd);
	if (ret) {
		dev_err(&lcd->ld->dev, "%s : failed to panel exit\n", __func__);
		goto suspend_err;
	}

	lcd->state = PANEL_STATE_SUSPENED;

suspend_err:
	mutex_unlock(&lcd->lock);

	return ret;
}

struct mipi_dsim_lcd_driver s6e88a0_mipi_lcd_driver = {
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
};

