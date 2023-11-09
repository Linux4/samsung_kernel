/*
 * drivers/video/decon_3475/panels/s6e8aa5x01_lcd_ctrl.c
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

#include "s6e8aa5x01_param.h"

#define LEVEL_IS_ACL_OFF(brightness)	(brightness == UI_MAX_BRIGHTNESS)

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#include "mdnie.h"
#include "mdnie_lite_table_j3x.h"
#endif

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#include "dimming_core.h"
#include "s6e8aa5x01_aid_dimming.h"

static const unsigned char *HBM_TABLE[HBM_STATUS_MAX] = {SEQ_HBM_OFF, SEQ_HBM_ON};
static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {SEQ_ACL_OFF, SEQ_ACL_15};
static const unsigned char *ACL_OPR_TABLE[ACL_OPR_MAX] = {SEQ_ACL_OFF_OPR_AVR, SEQ_ACL_ON_OPR_AVR};

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

static struct SmtDimInfo dimming_info[MAX_BR_INFO + 1] = {				/* add hbm array */
	{.br = 5, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl5nit, .cTbl = ctbl5nit, .aid = aid5, .elvAcl = elv_5, .elv = elv_5},
	{.br = 6, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl6nit, .cTbl = ctbl6nit, .aid = aid6, .elvAcl = elv_5, .elv = elv_5},
	{.br = 7, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl7nit, .cTbl = ctbl7nit, .aid = aid7, .elvAcl = elv_5, .elv = elv_5},
	{.br = 8, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl8nit, .cTbl = ctbl8nit, .aid = aid8, .elvAcl = elv_5, .elv = elv_5},
	{.br = 9, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl9nit, .cTbl = ctbl9nit, .aid = aid9, .elvAcl = elv_5, .elv = elv_5},
	{.br = 10, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl10nit, .cTbl = ctbl10nit, .aid = aid10, .elvAcl = elv_5, .elv = elv_5},
	{.br = 11, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl11nit, .cTbl = ctbl11nit, .aid = aid11, .elvAcl = elv_5, .elv = elv_5},
	{.br = 12, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl12nit, .cTbl = ctbl12nit, .aid = aid12, .elvAcl = elv_5, .elv = elv_5},
	{.br = 13, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl13nit, .cTbl = ctbl13nit, .aid = aid13, .elvAcl = elv_5, .elv = elv_5},
	{.br = 14, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl14nit, .cTbl = ctbl14nit, .aid = aid14, .elvAcl = elv_5, .elv = elv_5},
	{.br = 15, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl15nit, .cTbl = ctbl15nit, .aid = aid15, .elvAcl = elv_5, .elv = elv_5},
	{.br = 16, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl16nit, .cTbl = ctbl16nit, .aid = aid16, .elvAcl = elv_5, .elv = elv_5},
	{.br = 17, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl17nit, .cTbl = ctbl17nit, .aid = aid17, .elvAcl = elv_5, .elv = elv_5},
	{.br = 19, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl19nit, .cTbl = ctbl19nit, .aid = aid19, .elvAcl = elv_5, .elv = elv_5},
	{.br = 20, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl20nit, .cTbl = ctbl20nit, .aid = aid20, .elvAcl = elv_5, .elv = elv_5},
	{.br = 21, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl21nit, .cTbl = ctbl21nit, .aid = aid21, .elvAcl = elv_21, .elv = elv_21},
	{.br = 22, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl22nit, .cTbl = ctbl22nit, .aid = aid22, .elvAcl = elv_22, .elv = elv_22},
	{.br = 24, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl24nit, .cTbl = ctbl24nit, .aid = aid24, .elvAcl = elv_24, .elv = elv_24},
	{.br = 25, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl25nit, .cTbl = ctbl25nit, .aid = aid25, .elvAcl = elv_25, .elv = elv_25},
	{.br = 27, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl27nit, .cTbl = ctbl27nit, .aid = aid27, .elvAcl = elv_27, .elv = elv_27},
	{.br = 29, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl29nit, .cTbl = ctbl29nit, .aid = aid29, .elvAcl = elv_29, .elv = elv_29},
	{.br = 30, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl30nit, .cTbl = ctbl30nit, .aid = aid30, .elvAcl = elv_30, .elv = elv_30},
	{.br = 32, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl32nit, .cTbl = ctbl32nit, .aid = aid32, .elvAcl = elv_30, .elv = elv_30},
	{.br = 34, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl34nit, .cTbl = ctbl34nit, .aid = aid34, .elvAcl = elv_30, .elv = elv_30},
	{.br = 37, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl37nit, .cTbl = ctbl37nit, .aid = aid37, .elvAcl = elv_30, .elv = elv_30},
	{.br = 39, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl39nit, .cTbl = ctbl39nit, .aid = aid39, .elvAcl = elv_30, .elv = elv_30},
	{.br = 41, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl41nit, .cTbl = ctbl41nit, .aid = aid41, .elvAcl = elv_41, .elv = elv_41},
	{.br = 44, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl44nit, .cTbl = ctbl44nit, .aid = aid44, .elvAcl = elv_41, .elv = elv_41},
	{.br = 47, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl47nit, .cTbl = ctbl47nit, .aid = aid47, .elvAcl = elv_41, .elv = elv_41},
	{.br = 50, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl50nit, .cTbl = ctbl50nit, .aid = aid50, .elvAcl = elv_41, .elv = elv_41},
	{.br = 53, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl53nit, .cTbl = ctbl53nit, .aid = aid53, .elvAcl = elv_41, .elv = elv_41},
	{.br = 56, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl56nit, .cTbl = ctbl56nit, .aid = aid56, .elvAcl = elv_41, .elv = elv_41},
	{.br = 60, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl60nit, .cTbl = ctbl60nit, .aid = aid60, .elvAcl = elv_41, .elv = elv_41},
	{.br = 64, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl64nit, .cTbl = ctbl64nit, .aid = aid64, .elvAcl = elv_41, .elv = elv_41},
	{.br = 68, .refBr = 113, .cGma = gma2p00, .rTbl = rtbl68nit, .cTbl = ctbl68nit, .aid = aid68, .elvAcl = elv_41, .elv = elv_41},
	{.br = 72, .refBr = 118, .cGma = gma2p00, .rTbl = rtbl72nit, .cTbl = ctbl72nit, .aid = aid72, .elvAcl = elv_41, .elv = elv_41},
	{.br = 77, .refBr = 127, .cGma = gma2p00, .rTbl = rtbl77nit, .cTbl = ctbl77nit, .aid = aid77, .elvAcl = elv_41, .elv = elv_41},
	{.br = 82, .refBr = 135, .cGma = gma2p00, .rTbl = rtbl82nit, .cTbl = ctbl82nit, .aid = aid82, .elvAcl = elv_41, .elv = elv_41},
	{.br = 87, .refBr = 142, .cGma = gma2p00, .rTbl = rtbl87nit, .cTbl = ctbl87nit, .aid = aid87, .elvAcl = elv_41, .elv = elv_41},
	{.br = 93, .refBr = 151, .cGma = gma2p00, .rTbl = rtbl93nit, .cTbl = ctbl93nit, .aid = aid93, .elvAcl = elv_41, .elv = elv_41},
	{.br = 98, .refBr = 160, .cGma = gma2p00, .rTbl = rtbl98nit, .cTbl = ctbl98nit, .aid = aid98, .elvAcl = elv_41, .elv = elv_41},
	{.br = 105, .refBr = 172, .cGma = gma2p00, .rTbl = rtbl105nit, .cTbl = ctbl105nit, .aid = aid105, .elvAcl = elv_41, .elv = elv_41},
	{.br = 111, .refBr = 180, .cGma = gma2p00, .rTbl = rtbl111nit, .cTbl = ctbl111nit, .aid = aid110, .elvAcl = elv_111, .elv = elv_111},
	{.br = 119, .refBr = 192, .cGma = gma2p00, .rTbl = rtbl119nit, .cTbl = ctbl119nit, .aid = aid119, .elvAcl = elv_111, .elv = elv_111},
	{.br = 126, .refBr = 192, .cGma = gma2p00, .rTbl = rtbl126nit, .cTbl = ctbl126nit, .aid = aid126, .elvAcl = elv_111, .elv = elv_111},
	{.br = 134, .refBr = 192, .cGma = gma2p00, .rTbl = rtbl134nit, .cTbl = ctbl134nit, .aid = aid134, .elvAcl = elv_111, .elv = elv_111},
	{.br = 143, .refBr = 192, .cGma = gma2p00, .rTbl = rtbl143nit, .cTbl = ctbl143nit, .aid = aid143, .elvAcl = elv_143, .elv = elv_143},
	{.br = 152, .refBr = 192, .cGma = gma2p00, .rTbl = rtbl152nit, .cTbl = ctbl152nit, .aid = aid152, .elvAcl = elv_143, .elv = elv_143},
	{.br = 162, .refBr = 192, .cGma = gma2p00, .rTbl = rtbl162nit, .cTbl = ctbl162nit, .aid = aid162, .elvAcl = elv_143, .elv = elv_143},
	{.br = 172, .refBr = 192, .cGma = gma2p00, .rTbl = rtbl172nit, .cTbl = ctbl172nit, .aid = aid172, .elvAcl = elv_143, .elv = elv_143},
	{.br = 183, .refBr = 192, .cGma = gma2p00, .rTbl = rtbl183nit, .cTbl = ctbl183nit, .aid = aid183, .elvAcl = elv_183, .elv = elv_183},
	{.br = 195, .refBr = 204, .cGma = gma2p00, .rTbl = rtbl195nit, .cTbl = ctbl195nit, .aid = aid195, .elvAcl = elv_195, .elv = elv_195},
	{.br = 207, .refBr = 215, .cGma = gma2p00, .rTbl = rtbl207nit, .cTbl = ctbl207nit, .aid = aid207, .elvAcl = elv_207, .elv = elv_207},
	{.br = 220, .refBr = 228, .cGma = gma2p00, .rTbl = rtbl220nit, .cTbl = ctbl220nit, .aid = aid220, .elvAcl = elv_220, .elv = elv_220},
	{.br = 234, .refBr = 240, .cGma = gma2p00, .rTbl = rtbl234nit, .cTbl = ctbl234nit, .aid = aid234, .elvAcl = elv_234, .elv = elv_234},
	{.br = 249, .refBr = 255, .cGma = gma2p00, .rTbl = rtbl249nit, .cTbl = ctbl249nit, .aid = aid249, .elvAcl = elv_249, .elv = elv_249},
	{.br = 265, .refBr = 271, .cGma = gma2p00, .rTbl = rtbl265nit, .cTbl = ctbl265nit, .aid = aid265, .elvAcl = elv_265, .elv = elv_265},
	{.br = 282, .refBr = 286, .cGma = gma2p00, .rTbl = rtbl282nit, .cTbl = ctbl282nit, .aid = aid282, .elvAcl = elv_282, .elv = elv_282},
	{.br = 300, .refBr = 303, .cGma = gma2p00, .rTbl = rtbl300nit, .cTbl = ctbl300nit, .aid = aid300, .elvAcl = elv_300, .elv = elv_300},
	{.br = 316, .refBr = 318, .cGma = gma2p00, .rTbl = rtbl316nit, .cTbl = ctbl316nit, .aid = aid316, .elvAcl = elv_316, .elv = elv_316},
	{.br = 333, .refBr = 336, .cGma = gma2p00, .rTbl = rtbl333nit, .cTbl = ctbl333nit, .aid = aid333, .elvAcl = elv_333, .elv = elv_333},
	{.br = 360, .refBr = 360, .cGma = gma2p00, .rTbl = rtbl360nit, .cTbl = ctbl360nit, .aid = aid360, .elvAcl = elv_360, .elv = elv_360},
	{.br = 360, .refBr = 360, .cGma = gma2p00, .rTbl = rtbl360nit, .cTbl = ctbl360nit, .aid = aid360, .elvAcl = elv_HBM, .elv = elv_HBM},
};

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
	u8 elvss_val[3] = {0, };
	u8 B6_GW[2] = {0xB6, };
	int level = LEVEL_IS_HBM(dsim->priv.auto_brightness);
	int real_br = 0;

	real_br = get_actual_br_value(dsim, dsim->priv.br_index);
	elvss = get_elvss_from_index(dsim, dsim->priv.br_index, dsim->priv.acl_enable);

	if (elvss == NULL) {
		dsim_err("%s : failed to get elvss value\n", __func__);
		return;
	}

	elvss_val[0] = elvss[0];
	elvss_val[1] = level ? 0xAC : elvss[1];
	elvss_val[2] = elvss[2];

	B6_GW[1] = dsim->priv.elvss22th;

	if (real_br <= 29) {
		if (dsim->priv.temperature <= -20)
			B6_GW[1] = elvss[5];
		else if (dsim->priv.temperature > -20 && dsim->priv.temperature <= 0)
			B6_GW[1] = elvss[4];
		else
			B6_GW[1] = elvss[3];
	}


	if (dsim_write_hl_data(dsim, elvss_val, ELVSS_LEN) < 0)
		dsim_err("%s : failed to write elvss\n", __func__);

	if (dsim_write_hl_data(dsim, SEQ_ELVSS_GLOBAL, ARRAY_SIZE(SEQ_ELVSS_GLOBAL)) < 0)
		dsim_err("%s : failed to SEQ_ELVSS_GLOBAL\n", __func__);

	if (dsim_write_hl_data(dsim, B6_GW, 2) < 0)
		dsim_err("%s : failed to write elvss\n", __func__);

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

		if (dsim_write_hl_data(dsim, panel->acl_opr_tbl[level], ARRAY_SIZE(SEQ_ACL_ON_OPR_AVR)) < 0) {
			dsim_err("fail to write SEQ_ACL_ON/OFF_OPR_AVR command.\n");
			ret = -EPERM;
		}

		if (level) {
			if (dsim_write_hl_data(dsim, dsim->priv.hbm_gamma, HBM_GAMMA_CMD_CNT) < 0) {
				dsim_err("fail to write hbm gamma  command.\n");
				ret = -EPERM;
			}
		}

		dsim_info("hbm: %d, auto_brightness: %d\n", dsim->priv.current_hbm, dsim->priv.auto_brightness);
	}
exit:
	return ret;
}

static int dsim_panel_set_acl(struct dsim_device *dsim, int force)
{
	int ret = 0, level = ACL_STATUS_15P;
	struct panel_private *panel = &dsim->priv;
	int brightness = panel->bd->props.brightness;

	if (panel == NULL) {
		dsim_err("%s : panel is NULL\n", __func__);
		goto exit;
	}

	if (dsim->priv.siop_enable || LEVEL_IS_HBM(dsim->priv.auto_brightness))  /* auto acl or hbm is acl on */
		goto acl_update;

	if (!dsim->priv.acl_enable && LEVEL_IS_ACL_OFF(brightness) && dsim->priv.weakness_hbm_comp == 2)
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

static int dsim_panel_set_tset(struct dsim_device *dsim, int force)
{
	int ret = 0;
	int tset = 0;
	unsigned char SEQ_TSET[TSET_LEN] = {TSET_REG, 0x00};

	tset = ((dsim->priv.temperature >= 0) ? 0 : BIT(7)) | abs(dsim->priv.temperature);

	if (force || (dsim->priv.tset[0] != tset)) {
		dsim->priv.tset[0] = SEQ_TSET[1] = tset;

		if (dsim_write_hl_data(dsim, SEQ_TSET_GP, ARRAY_SIZE(SEQ_TSET_GP)) < 0) {
			dsim_err("fail to write SEQ_TSET_GP command.\n");
			ret = -EPERM;
		}

		if (dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET)) < 0) {
			dsim_err("fail to write SEQ_TSET command.\n");
			ret = -EPERM;
		}

		dsim_info("temperature: %d, tset: %d\n", dsim->priv.temperature, SEQ_TSET[1]);
	}
	return ret;
}

static int low_level_set_brightness(struct dsim_device *dsim, int force)
{
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);

	dsim_panel_gamma_ctrl(dsim);

	dsim_panel_aid_ctrl(dsim);

	dsim_panel_set_elvss(dsim);

	dsim_panel_set_hbm(dsim, force);

	dsim_panel_set_acl(dsim, force);

	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
		dsim_err("%s : fail to write SEQ_GAMMA_UPDATE on command.\n", __func__);

	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L)) < 0)
		dsim_err("%s : fail to write SEQ_GAMMA_UPDATE_L on command.\n", __func__);

	dsim_panel_set_tset(dsim, force);

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
	min = priv->date[4];

	sprintf(buf, "%d, %d, %d, %d:%d\n", year, month, day, hour, min);
	return strlen(buf);
}

static ssize_t cell_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		priv->date[0] , priv->date[1], priv->date[2], priv->date[3], priv->date[4],
		priv->date[5],priv->date[6], (priv->coordinate[0] &0xFF00)>>8,priv->coordinate[0] &0x00FF,
		(priv->coordinate[1]&0xFF00)>>8,priv->coordinate[1]&0x00FF);

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
static DEVICE_ATTR(weakness_hbm_comp, 0664, weakness_hbm_show, weakness_hbm_store);

static void lcd_init_sysfs(struct dsim_device *dsim)
{
	int ret = -1;
	struct panel_private *panel = &dsim->priv;

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

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_cell_id);
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

	ret = device_create_file(&panel->bd->dev ,&dev_attr_weakness_hbm_comp);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
}


#ifdef CONFIG_PANEL_AID_DIMMING

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

	diminfo = dimming_info;

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

static int s6e8aa5x01_read_init_info(struct dsim_device *dsim, unsigned char *mtp)
{
	int i = 0;
	int ret;
	struct panel_private *panel = &dsim->priv;
	unsigned char bufForCoordi[S6E8AA5X01_COORDINATE_LEN] = {0,};
	unsigned char buf[S6E8AA5X01_MTP_DATE_SIZE] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);

	/* id */
	ret = dsim_read_hl_data(dsim, S6E8AA5X01_ID_REG, S6E8AA5X01_ID_LEN, dsim->priv.id);
	if (ret != S6E8AA5X01_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < S6E8AA5X01_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	/* chip id */
	ret = dsim_read_hl_data(dsim, S6E8AA5X01_CHIP_ID_REG, S6E8AA5X01_CHIP_ID_LEN, dsim->priv.chip_id);
	if (ret != S6E8AA5X01_CHIP_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

// 0xC8h Info
// 01~39th : gamma mtp
// 41~45th : manufacture date
// 73~87th : HBM gamma

#if 0
	/* mtp */
	ret = dsim_read_hl_data(dsim, S6E8AA5X01_MTP_ADDR, S6E8AA5X01_MTP_DATE_SIZE, buf);
	if (ret != S6E8AA5X01_MTP_DATE_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
#else
	/* mtp */
	ret = dsim_read_hl_data(dsim, S6E8AA5X01_MTP_ADDR, S6E8AA5X01_MTP_SIZE_GAMMA, buf);
	if (ret != S6E8AA5X01_MTP_SIZE_GAMMA) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}

	ret = dsim_write_hl_data(dsim, SEQ_MTP_READ_DATE_GP, ARRAY_SIZE(SEQ_MTP_READ_DATE_GP));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_MTP_READ_DATE_GP\n", __func__);
		goto read_fail;
	}

	/* mtp */
	ret = dsim_read_hl_data(dsim, S6E8AA5X01_MTP_ADDR, S6E8AA5X01_MTP_SIZE_DATE, buf + S6E8AA5X01_MTP_OFFSET_DATE);
	if (ret != S6E8AA5X01_MTP_SIZE_DATE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}

	ret = dsim_write_hl_data(dsim, SEQ_MTP_READ_HBM_GP, ARRAY_SIZE(SEQ_MTP_READ_HBM_GP));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_MTP_READ_HBM_GP\n", __func__);
		goto read_fail;
	}

	/* mtp */
	ret = dsim_read_hl_data(dsim, S6E8AA5X01_MTP_ADDR, S6E8AA5X01_MTP_SIZE_HBM, buf + S6E8AA5X01_MTP_OFFSET_HBM);
	if (ret != S6E8AA5X01_MTP_SIZE_HBM) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}

#endif

	memcpy(mtp, buf, S6E8AA5X01_MTP_SIZE);

	dsim_info("READ MTP SIZE : %d\n", S6E8AA5X01_MTP_SIZE);
	dsim_info("=========== MTP INFO ===========\n");
	for (i = 0; i < S6E8AA5X01_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

	/* hbm gamma 34~39 and 73~87 */
	dsim->priv.hbm_gamma[0] = 0xB4;
	dsim->priv.hbm_gamma[1] = 0x0B;
	for (i = 0; i < 6; i++)
		dsim->priv.hbm_gamma[i+2] = buf[i+33];
	for (i = 0; i < 15; i++)
		dsim->priv.hbm_gamma[i+8] = buf[i+72];
	for (i = 23; i < 32; i++)
		dsim->priv.hbm_gamma[i] = 0x80;
	for (i = 32; i < 35; i++)
		dsim->priv.hbm_gamma[i] = 0x00;


	dsim_info("READ MTP SIZE : %d\n", S6E8AA5X01_MTP_SIZE);
	dsim_info("=========== HBM GAMMA INFO ===========\n");
	for (i = 0; i < S6E8AA5X01_MTP_HBM_GAMMA_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, dsim->priv.hbm_gamma[i+2], dsim->priv.hbm_gamma[i+2]);

	/* date */
 	dsim->priv.date[0] = buf[40] >> 4; /*year*/
	dsim->priv.date[1] = buf[40] & 0x0f; /*month*/
	dsim->priv.date[2] = buf[41]; /*day*/
	dsim->priv.date[4] = buf[42]; /*hour*/
	dsim->priv.date[5] = buf[43]; /*minute*/
	dsim->priv.date[6] = buf[44]; /*second*/


	/* elvss */
	ret = dsim_read_hl_data(dsim, ELVSS_REG, ELVSS_MTP_LEN, buf);
	if (ret != ELVSS_MTP_LEN) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}

	dsim->priv.elvss22th = buf[21]; /*reserve for turn off HBM */

	/* coordinate */
	ret = dsim_read_hl_data(dsim, S6E8AA5X01_COORDINATE_REG, S6E8AA5X01_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E8AA5X01_COORDINATE_LEN) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[3] << 8 | bufForCoordi[4];	/* X */
	dsim->priv.coordinate[1] = bufForCoordi[5] << 8 | bufForCoordi[6];	/* Y */
	dsim_info("READ coordi : ");
	for (i = 0; i < 2; i++)
		dsim_info("%d, ", dsim->priv.coordinate[i]);
	dsim_info("\n");

	/* chip id */
	ret = dsim_read_hl_data(dsim, S6E8AA5X01_CODE_REG, S6E8AA5X01_CODE_LEN, dsim->priv.code);
	if (ret != S6E8AA5X01_CODE_LEN) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("READ code : ");
	for(i = 0; i < S6E8AA5X01_CODE_LEN; i++)
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

static int s6e8aa5x01_displayon(struct dsim_device *dsim)
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

static int s6e8aa5x01_exit(struct dsim_device *dsim)
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

static int s6e8aa5x01_init(struct dsim_device *dsim)
{
	int ret = 0;
	int i = 0;

	dsim_info("MDD : %s was called\n", __func__);

	usleep_range(5000, 6000);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}

	msleep(20);

	ret = dsim_read_hl_data(dsim, S6E8AA5X01_ID_REG, S6E8AA5X01_ID_LEN, dsim->priv.id);
	if (ret != S6E8AA5X01_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
	}

	dsim_info("READ ID : ");
	for (i = 0; i < S6E8AA5X01_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	/* 2. Brightness Setting */
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_AID_360NIT, ARRAY_SIZE(SEQ_AID_360NIT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_AID_360NIT\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_ELVSS_360NIT, ARRAY_SIZE(SEQ_ELVSS_360NIT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ELVSS_360NIT\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_ACL_OFF_OPR, ARRAY_SIZE(SEQ_ACL_OFF_OPR));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ACL_OFF_OPR\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_ACL_OFF, ARRAY_SIZE(SEQ_ACL_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ACL_OFF\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TSET_GP, ARRAY_SIZE(SEQ_TSET_GP));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSET_GP\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSET\n", __func__);
		goto init_exit;
	}


	/* 3. Common Setting */
	ret = dsim_write_hl_data(dsim, SEQ_PENTILE_SETTING, ARRAY_SIZE(SEQ_PENTILE_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PENTILE_SETTING\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_DE_DIM_GP, ARRAY_SIZE(SEQ_DE_DIM_GP));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_DE_DIM_GP\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_DE_DIM_SETTING, ARRAY_SIZE(SEQ_DE_DIM_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_DE_DIM_SETTING\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

	msleep(120);

init_exit:
	return ret;
}

static int s6e8aa5x01_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[S6E8AA5X01_MTP_DATE_SIZE] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;

	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}

	s6e8aa5x01_read_init_info(dsim, mtp);

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

struct dsim_panel_ops s6e8aa5x01_panel_ops = {
	.early_probe = NULL,
	.probe		= s6e8aa5x01_probe,
	.displayon	= s6e8aa5x01_displayon,
	.exit		= s6e8aa5x01_exit,
	.init		= s6e8aa5x01_init,
};

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

static int mdnie_lite_send_seq(struct dsim_device *dsim, struct lcd_seq_info *seq, u32 num)
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

static int mdnie_lite_read(struct dsim_device *dsim, u8 addr, u8 *buf, u32 size)
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
	panel->weakness_hbm_comp = 0;
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

struct mipi_dsim_lcd_driver s6e8aa5x01_mipi_lcd_driver = {
	.early_probe = dsim_panel_early_probe,
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
	.resume		= dsim_panel_resume,
};

