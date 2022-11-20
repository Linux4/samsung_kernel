/*
 * drivers/video/decon_3475/panels/S6E88A0_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Jiun Yu, <jiun.yu@samsung.com>
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

#include "../dsim.h"

#include "panel_info.h"

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#include "dimming_core.h"
#include "s6e8aa0_aid_dimming.h"
#include "dsim_backlight.h"
#endif

#ifdef CONFIG_PANEL_AID_DIMMING
static const unsigned char *HBM_TABLE[HBM_STATUS_MAX] = {SEQ_HBM_OFF, SEQ_HBM_ON};
static const unsigned char *ACL_CUTOFF_TABLE[ACL_STATUS_MAX] = {SEQ_ACL_OFF, SEQ_ACL_15};
static const unsigned char *ACL_OPR_TABLE[ACL_OPR_MAX] = {SEQ_ACL_OFF_OPR_AVR, SEQ_ACL_ON_OPR_AVR};

static const unsigned int br_tbl [256] = {
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

struct SmtDimInfo daisy_dimming_info[MAX_BR_INFO + 1] = {				// add hbm array
	{.br = 5, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl5nit, .cTbl = ctbl5nit, .aid = aid5, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 6, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl6nit, .cTbl = ctbl6nit, .aid = aid6, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 7, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl7nit, .cTbl = ctbl7nit, .aid = aid7, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 8, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl8nit, .cTbl = ctbl8nit, .aid = aid8, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 9, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl9nit, .cTbl = ctbl9nit, .aid = aid9, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 10, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl10nit, .cTbl = ctbl10nit, .aid = aid10, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 11, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl11nit, .cTbl = ctbl11nit, .aid = aid11, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 12, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl12nit, .cTbl = ctbl12nit, .aid = aid12, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 13, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl13nit, .cTbl = ctbl13nit, .aid = aid13, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 14, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl14nit, .cTbl = ctbl14nit, .aid = aid14, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 15, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl15nit, .cTbl = ctbl15nit, .aid = aid15, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 16, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl16nit, .cTbl = ctbl16nit, .aid = aid16, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 17, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl17nit, .cTbl = ctbl17nit, .aid = aid17, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 19, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl19nit, .cTbl = ctbl19nit, .aid = aid19, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 20, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl20nit, .cTbl = ctbl20nit, .aid = aid20, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 21, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl21nit, .cTbl = ctbl21nit, .aid = aid21, .elvAcl = elv_0D, .elv = elv_0D},
	{.br = 22, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl22nit, .cTbl = ctbl22nit, .aid = aid22, .elvAcl = elv_0F, .elv = elv_0F},
	{.br = 24, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl24nit, .cTbl = ctbl24nit, .aid = aid24, .elvAcl = elv_11, .elv = elv_11},
	{.br = 25, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl25nit, .cTbl = ctbl25nit, .aid = aid25, .elvAcl = elv_13, .elv = elv_13},
	{.br = 27, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl27nit, .cTbl = ctbl27nit, .aid = aid27, .elvAcl = elv_15, .elv = elv_15},
	{.br = 29, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl29nit, .cTbl = ctbl29nit, .aid = aid29, .elvAcl = elv_17, .elv = elv_17},
	{.br = 30, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl30nit, .cTbl = ctbl30nit, .aid = aid30, .elvAcl = elv_18, .elv = elv_18},
	{.br = 32, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl32nit, .cTbl = ctbl32nit, .aid = aid32, .elvAcl = elv_18, .elv = elv_18},
	{.br = 34, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl34nit, .cTbl = ctbl34nit, .aid = aid34, .elvAcl = elv_18, .elv = elv_18},
	{.br = 37, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl37nit, .cTbl = ctbl37nit, .aid = aid37, .elvAcl = elv_18, .elv = elv_18},
	{.br = 39, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl39nit, .cTbl = ctbl39nit, .aid = aid39, .elvAcl = elv_18, .elv = elv_18},
	{.br = 41, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl41nit, .cTbl = ctbl41nit, .aid = aid41, .elvAcl = elv_18, .elv = elv_18},
	{.br = 44, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl44nit, .cTbl = ctbl44nit, .aid = aid44, .elvAcl = elv_18, .elv = elv_18},
	{.br = 47, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl47nit, .cTbl = ctbl47nit, .aid = aid47, .elvAcl = elv_18, .elv = elv_18},
	{.br = 50, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl50nit, .cTbl = ctbl50nit, .aid = aid50, .elvAcl = elv_18, .elv = elv_18},
	{.br = 53, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl53nit, .cTbl = ctbl53nit, .aid = aid53, .elvAcl = elv_18, .elv = elv_18},
	{.br = 56, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl56nit, .cTbl = ctbl56nit, .aid = aid56, .elvAcl = elv_18, .elv = elv_18},
	{.br = 60, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl60nit, .cTbl = ctbl60nit, .aid = aid60, .elvAcl = elv_18, .elv = elv_18},
	{.br = 64, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl64nit, .cTbl = ctbl64nit, .aid = aid64, .elvAcl = elv_18, .elv = elv_18},
	{.br = 68, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl68nit, .cTbl = ctbl68nit, .aid = aid68, .elvAcl = elv_18, .elv = elv_18},
	{.br = 72, .refBr = 129, .cGma = gma2p15, .rTbl = rtbl72nit, .cTbl = ctbl72nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18},
	{.br = 77, .refBr = 136, .cGma = gma2p15, .rTbl = rtbl77nit, .cTbl = ctbl77nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18},
	{.br = 82, .refBr = 146, .cGma = gma2p15, .rTbl = rtbl82nit, .cTbl = ctbl82nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18},
	{.br = 87, .refBr = 153, .cGma = gma2p15, .rTbl = rtbl87nit, .cTbl = ctbl87nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18},
	{.br = 93, .refBr = 163, .cGma = gma2p15, .rTbl = rtbl93nit, .cTbl = ctbl93nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18},
	{.br = 98, .refBr = 169, .cGma = gma2p15, .rTbl = rtbl98nit, .cTbl = ctbl98nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18},
	{.br = 105, .refBr = 182, .cGma = gma2p15, .rTbl = rtbl105nit, .cTbl = ctbl105nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18},
	{.br = 111, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl111nit, .cTbl = ctbl111nit, .aid = aid72, .elvAcl = elv_18, .elv = elv_18},
	{.br = 119, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl119nit, .cTbl = ctbl119nit, .aid = aid119, .elvAcl = elv_18, .elv = elv_18},
	{.br = 126, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl126nit, .cTbl = ctbl126nit, .aid = aid126, .elvAcl = elv_18, .elv = elv_18},
	{.br = 134, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl134nit, .cTbl = ctbl134nit, .aid = aid134, .elvAcl = elv_17, .elv = elv_17},
	{.br = 143, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl143nit, .cTbl = ctbl143nit, .aid = aid143, .elvAcl = elv_17, .elv = elv_17},
	{.br = 152, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl152nit, .cTbl = ctbl152nit, .aid = aid152, .elvAcl = elv_16, .elv = elv_16},
	{.br = 162, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl162nit, .cTbl = ctbl162nit, .aid = aid162, .elvAcl = elv_16, .elv = elv_16},
	{.br = 172, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl172nit, .cTbl = ctbl172nit, .aid = aid172, .elvAcl = elv_15, .elv = elv_15},
	{.br = 183, .refBr = 191, .cGma = gma2p15, .rTbl = rtbl183nit, .cTbl = ctbl183nit, .aid = aid183, .elvAcl = elv_15, .elv = elv_15},
	{.br = 195, .refBr = 204, .cGma = gma2p15, .rTbl = rtbl195nit, .cTbl = ctbl195nit, .aid = aid183, .elvAcl = elv_14, .elv = elv_14},
	{.br = 207, .refBr = 216, .cGma = gma2p15, .rTbl = rtbl207nit, .cTbl = ctbl207nit, .aid = aid183, .elvAcl = elv_13, .elv = elv_13},
	{.br = 220, .refBr = 229, .cGma = gma2p15, .rTbl = rtbl220nit, .cTbl = ctbl220nit, .aid = aid183, .elvAcl = elv_12, .elv = elv_12},
	{.br = 234, .refBr = 243, .cGma = gma2p15, .rTbl = rtbl234nit, .cTbl = ctbl234nit, .aid = aid183, .elvAcl = elv_12, .elv = elv_12},
	{.br = 249, .refBr = 258, .cGma = gma2p15, .rTbl = rtbl249nit, .cTbl = ctbl249nit, .aid = aid183, .elvAcl = elv_11, .elv = elv_11},
	{.br = 265, .refBr = 273, .cGma = gma2p15, .rTbl = rtbl265nit, .cTbl = ctbl265nit, .aid = aid183, .elvAcl = elv_10, .elv = elv_10},
	{.br = 282, .refBr = 289, .cGma = gma2p15, .rTbl = rtbl282nit, .cTbl = ctbl282nit, .aid = aid183, .elvAcl = elv_0F, .elv = elv_0F},
	{.br = 300, .refBr = 305, .cGma = gma2p15, .rTbl = rtbl300nit, .cTbl = ctbl300nit, .aid = aid183, .elvAcl = elv_0E, .elv = elv_0E},		// 249 ~ 360nit acl off
	{.br = 316, .refBr = 320, .cGma = gma2p15, .rTbl = rtbl316nit, .cTbl = ctbl316nit, .aid = aid183, .elvAcl = elv_0D, .elv = elv_0D},
	{.br = 333, .refBr = 336, .cGma = gma2p15, .rTbl = rtbl333nit, .cTbl = ctbl333nit, .aid = aid183, .elvAcl = elv_0C, .elv = elv_0C},
	{.br = 350, .refBr = 350, .cGma = gma2p15, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid183, .elvAcl = elv_0B, .elv = elv_0B},
	{.br = 350, .refBr = 350, .cGma = gma2p15, .rTbl = rtbl350nit, .cTbl = ctbl350nit, .aid = aid183, .elvAcl = elv_0B, .elv = elv_0B},
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

	dimming = (struct dim_data *)kmalloc(sizeof(struct dim_data), GFP_KERNEL);
	if (!dimming) {
		dsim_err("failed to allocate memory for dim data\n");
		ret = -ENOMEM;
		goto error;
	}

	diminfo = daisy_dimming_info;

	panel->dim_data= (void *)dimming;
	panel->dim_info = (void *)diminfo; // dimming info

	panel->br_tbl = (unsigned int *)br_tbl; // backlight table
	panel->hbm_tbl = (unsigned char **)HBM_TABLE; // command hbm on and off
	panel->acl_cutoff_tbl = (unsigned char **)ACL_CUTOFF_TABLE; // ACL on and off command
	panel->acl_opr_tbl = (unsigned char **)ACL_OPR_TABLE; //

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
	dimm_info("Center Gamma Info : \n");
	for(i=0;i<VMAX;i++) {
		dsim_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE],
			dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE] );
	}


	dimm_info("VT MTP : \n");
	dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE],
			dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE] );

	/* MTP value get from ddi */
	dimm_info("MTP Info from ddi: \n");
	for(i=0;i<VMAX;i++) {
		dimm_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE],
			dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE] );
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


static int S6E88A0_read_init_info(struct dsim_device *dsim, unsigned char* mtp)
{
	int i = 0;
	int ret;
	struct panel_private *panel = &dsim->priv;
	unsigned char bufForCoordi[S6E88A0_COORDINATE_LEN] = {0,};
	unsigned char bufForDate[S6E88A0_DATE_LEN] = {0,};
	unsigned char buf[S6E88A0_MTP_DATE_SIZE] = {0, };
	unsigned char buf_HBM_1[HBM_GAMMA_READ_SIZE_1] = {0, };
	unsigned char buf_HBM_2[HBM_GAMMA_READ_SIZE_2] = {0, };

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	// id
	ret = dsim_read_hl_data(dsim, S6E88A0_ID_REG, S6E88A0_ID_LEN, dsim->priv.id);
	if (ret != S6E88A0_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n",__func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for(i = 0; i < S6E88A0_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	// mtp
	ret = dsim_read_hl_data(dsim, S6E88A0_MTP_ADDR, S6E88A0_MTP_DATE_SIZE, buf);
	if (ret != S6E88A0_MTP_DATE_SIZE) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(mtp, buf, S6E88A0_MTP_SIZE);
	dsim_info("READ MTP SIZE : %d\n", S6E88A0_MTP_SIZE);
	dsim_info("=========== MTP INFO =========== \n");
	for (i = 0; i < S6E88A0_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);

	//elvss

	ret = dsim_read_hl_data(dsim, ELVSS_REG, ELVSS_LEN, dsim->priv.elvss);
	if (ret != ELVSS_LEN) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}

	// date
	ret = dsim_read_hl_data(dsim, S6E88A0_DATE_REG, S6E88A0_DATE_LEN, bufForDate);
	if (ret != S6E88A0_DATE_LEN) {
		dsim_err("fail to read date on command.\n");
		goto read_fail;
	}
	dsim->priv.date[0] = bufForDate[23] >> 4;
	dsim->priv.date[1] = bufForDate[23] & 0x0f;
	dsim->priv.date[2] = bufForDate[24];

	// coordinate
	ret = dsim_read_hl_data(dsim, S6E88A0_COORDINATE_REG, S6E88A0_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E88A0_COORDINATE_LEN) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[20] << 8 | bufForCoordi[21];	/* X */
	dsim->priv.coordinate[1] = bufForCoordi[22] << 8 | bufForCoordi[23];	/* Y */
	dsim_info("READ coordi : ");
	for(i = 0; i < 2; i++)
		dsim_info("%d, ", dsim->priv.coordinate[i]);
	dsim_info("\n");

	// Read HBM from B5h
	ret = dsim_read_hl_data(dsim, HBM_GAMMA_READ_ADD_1, HBM_GAMMA_READ_SIZE_1, buf_HBM_1);
	if (ret != HBM_GAMMA_READ_SIZE_1) {
		dsim_err("fail to read HBM from B5h.\n");
		goto read_fail;
	}
	for(i = 0; i < 6; i++)
		dsim->priv.DB[i] = buf_HBM_1[i+12];
	for(i = 6; i < 9; i++)
		dsim->priv.DB[i] = buf_HBM_1[i+19];

	// Read elvss setting HBM
		dsim->priv.elvss_hbm = buf_HBM_1[18];

	// Read HBM from B6h
	ret = dsim_read_hl_data(dsim, HBM_GAMMA_READ_ADD_2, HBM_GAMMA_READ_SIZE_2, buf_HBM_2);
	if (ret != HBM_GAMMA_READ_SIZE_2) {
		dsim_err("fail to read HBM from B6h.\n");
		goto read_fail;
	}
	for(i = 0; i < 12; i++)
		dsim->priv.DB[i+9] = buf_HBM_2[i+17];

	// Read default 0xB6 17th value.
	dsim->priv.elvss_hbm_default = buf_HBM_2[16];

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

static int S6E88A0_displayon(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
		goto displayon_err;
	}

	msleep(12);

displayon_err:
	return ret;

}

static int S6E88A0_exit(struct dsim_device *dsim)
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


static int S6E88A0_init(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	msleep(5);

	ret = dsim_write_hl_data(dsim, SEQ_S_WIRE, ARRAY_SIZE(SEQ_S_WIRE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_S_WIRE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}

	msleep(20);

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

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_AID_SET, ARRAY_SIZE(SEQ_AID_SET));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_AID_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_ACL_MTP, ARRAY_SIZE(SEQ_ACL_MTP));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ACL_MTP\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_ELVSS_SET_S6E88A0, ARRAY_SIZE(SEQ_ELVSS_SET_S6E88A0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ELVSS_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_S6E88A0, ARRAY_SIZE(SEQ_GAMMA_UPDATE_S6E88A0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ACL_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_ACL_SET_S6E88A0, ARRAY_SIZE(SEQ_ACL_SET_S6E88A0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ACL_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSET\n", __func__);
		goto init_exit;
	}

	msleep(120);

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

static int S6E88A0_probe(struct dsim_device *dsim)
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

	S6E88A0_read_init_info(dsim, mtp);
	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}
#ifdef CONFIG_PANEL_AID_DIMMING
		ret = init_dimming(dsim, mtp);
		if (ret) {
			dsim_err("%s : failed to generate gamma tablen\n", __func__);
		}
#endif

probe_exit:
	return ret;
}

struct dsim_panel_ops S6E88A0_panel_ops = {
	.early_probe = NULL,
	.probe		= S6E88A0_probe,
	.displayon	= S6E88A0_displayon,
	.exit		= S6E88A0_exit,
	.init		= S6E88A0_init,
};
