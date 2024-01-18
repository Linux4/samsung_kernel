/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
 *
 *	Author: jb09.kim
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2020, Samsung Electronics. All rights reserved.

*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/
#include "ss_dsi_panel_S6E3FAB_AMB624XT01.h"
#include "ss_dsi_mdnie_S6E3FAB_AMB624XT01.h"

/* AOD Mode status on AOD Service */

enum {
	HLPM_CTRL_2NIT,
	HLPM_CTRL_10NIT,
	HLPM_CTRL_30NIT,
	HLPM_CTRL_60NIT,
	MAX_LPM_CTRL,
};

#define ALPM_REG	0x53	/* Register to control brightness level */
#define ALPM_CTRL_REG	0xBB	/* Register to cnotrol ALPM/HLPM mode */

#define IRC_MODERATO_MODE_VAL	0x6F
#define IRC_FLAT_GAMMA_MODE_VAL	0x2F

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
	        LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);
	ss_panel_attach_set(vdd, true);

	return true;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
	/*
	 * self mask is enabled from bootloader.
	 * so skip self mask setting during splash booting.
	 */
	if (!vdd->samsung_splash_enabled) {
		if (vdd->self_disp.self_mask_img_write)
			vdd->self_disp.self_mask_img_write(vdd);
	} else {
		LCD_INFO(vdd, "samsung splash enabled.. skip image write\n");
	}

	if (vdd->self_disp.self_mask_on)
		vdd->self_disp.self_mask_on(vdd, true);

	/* mafpc */
	if (vdd->mafpc.is_support) {
		vdd->mafpc.need_to_write = true;
		LCD_INFO(vdd, "Need to write mafpc image data to DDI\n");
	}

	return true;
}

bool apply_flash_gamma = false;

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{
	if (vdd->manufacture_id_dsi == PBA_ID)
		ss_panel_attach_set(vdd, false);
	else
		ss_panel_attach_set(vdd, true);

	switch (ss_panel_rev_get(vdd)) {
	case 0x00:
	case 0x01:
	case 0x02:
		vdd->panel_revision = 'A';
		break;
	case 0x03:
		vdd->panel_revision = 'D';
		break;
	default:
		vdd->panel_revision = 'D';
		LCD_ERR(vdd, "Invalid panel_rev(default rev : %c)\n", vdd->panel_revision);
		break;
	}

	if (vdd->panel_revision >= 'D') {
		LCD_ERR(vdd, "apply flash_gamma.\n");
		apply_flash_gamma = true;
	}

	vdd->panel_revision -= 'A';
	LCD_INFO_ONCE(vdd, "panel_revision = %c %d \n", vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

/*
 * VRR related cmds
 */

static u8 LTPS_SETTING1[VRR_MODE_MAX][8] = {
	{0x11, 0x89, 0x11, 0x89, 0x11, 0x89, 0x11, 0x89},
	{0x16, 0x77, 0x16, 0x77, 0x16, 0x77, 0x16, 0x77},
};

static u8 LTPS_SETTING2[VRR_MODE_MAX][8] = {
	{0x0C, 0x75, 0x0C, 0x75, 0x0C, 0x75, 0x0C, 0x75},
	{0x18, 0x45, 0x18, 0x45, 0x18, 0x45, 0x18, 0x45},
};

static u8 OSC_SETTING[VRR_MODE_MAX][1] = {
	{0xB2},
	{0xB0},
};

static u8 VFP_48HZ_SETTING[2] = {0x0E, 0x50};
static u8 VFP_96HZ_SETTING[2] = {0x02, 0x70};
static u8 VFP_120HZ_SETTING[2] = {0x00, 0x10};

static u8 VFP_60HZ_SETTING[VRR_MODE_MAX][2] = {
	{0x00, 0x10},	/* 120hs, 60ns */
	{0x09, 0x90},	/* 60hs */
};

static u8 AID_SETTING[VRR_MODE_MAX][1] = {
	{0x41},
	{0x21},
};

static u8 SOURCE_SETTING[VRR_MODE_MAX][1] = {
	{0x01},
	{0x81},
};

static struct dsi_panel_cmd_set *ss_vrr(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set  *vrr_cmds = ss_get_cmds(vdd, TX_VRR);
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct vrr_info *vrr = &vdd->vrr;
	enum SS_BRR_MODE brr_mode = vrr->brr_mode;
	bool is_hbm = vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL ? true : false;
	int cur_rr, prev_rr;
	bool cur_hs, cur_phs, prev_hs, prev_phs;
	int idx = 0;

	if (SS_IS_CMDS_NULL(vrr_cmds)) {
		LCD_INFO(vdd, "no vrr cmds\n");
		return NULL;
	}

	if (panel && panel->cur_mode) {
		LCD_INFO(vdd, "VRR(%d): cur_mode: %dx%d@%d%s, is_hbm: %d, is_hmt: %d, %s\n",
				vrr->running_vrr,
				panel->cur_mode->timing.h_active,
				panel->cur_mode->timing.v_active,
				panel->cur_mode->timing.refresh_rate,
				panel->cur_mode->timing.sot_hs_mode ?
				(panel->cur_mode->timing.phs_mode ? "PHS" : "HS") : "NM",
				is_hbm, 0, ss_get_brr_mode_name(brr_mode));

		if (panel->cur_mode->timing.refresh_rate != vrr->adjusted_refresh_rate ||
				panel->cur_mode->timing.sot_hs_mode != vrr->adjusted_sot_hs_mode ||
				panel->cur_mode->timing.phs_mode != vrr->adjusted_phs_mode)
			LCD_DEBUG(vdd, "VRR: unmatched RR mode (%dhz%s / %dhz%s)\n",
					panel->cur_mode->timing.refresh_rate,
					panel->cur_mode->timing.sot_hs_mode ?
					(panel->cur_mode->timing.phs_mode ? "PHS" : "HS") : "NM",
					vrr->adjusted_refresh_rate,
					vrr->adjusted_sot_hs_mode ?
					(vrr->adjusted_phs_mode ? "PHS" : "HS") : "NM");
	}

	if (vdd->panel_revision < 3) {	/* panel rev.A ~ rev.C */
		vrr->cur_sot_hs_mode = true;
		LCD_INFO(vdd, "Do not support 60NS (rev %d), set HS\n", vdd->panel_revision);
	}

	if (vrr->running_vrr) {
		prev_rr = vrr->prev_refresh_rate;
		prev_hs = vrr->prev_sot_hs_mode;
		prev_phs = vrr->prev_phs_mode;
	} else {
		prev_rr = vrr->cur_refresh_rate;
		prev_hs = vrr->cur_sot_hs_mode;
		prev_phs = vrr->cur_phs_mode;
	}

	cur_rr = vrr->cur_refresh_rate;
	cur_hs = vrr->cur_sot_hs_mode;
	cur_phs = vrr->cur_phs_mode;

	if (vdd->panel_revision >= 3) {
		/* LTPS1 */
		idx = ss_get_cmd_idx(vrr_cmds, 0x42, 0xCB);
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], LTPS_SETTING1[cur_hs], sizeof(LTPS_SETTING1[0]) / sizeof(u8));

		/* LTPS2 */
		idx = ss_get_cmd_idx(vrr_cmds, 0x62, 0xCB);
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], LTPS_SETTING2[cur_hs], sizeof(LTPS_SETTING2[0]) / sizeof(u8));
	}

	/* OSC */
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0xF2);
	memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], OSC_SETTING[cur_hs], sizeof(OSC_SETTING[0]) / sizeof(u8));

	/* VFP 120 <-> 96 */
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0x60);
	if (cur_rr == 120 || cur_rr == 96 ||
		(cur_phs && cur_rr == 60) || (cur_phs && cur_rr == 48))
		vrr_cmds->cmds[idx].ss_txbuf[1] = 0x20;
	else
		vrr_cmds->cmds[idx].ss_txbuf[1] = 0x00;

	/* AOR */
	idx = ss_get_cmd_idx(vrr_cmds, 0x01, 0xF2);
	if (cur_rr == 60 || cur_rr == 120 || (cur_phs && cur_rr == 60))
		vrr_cmds->cmds[idx].ss_txbuf[1] = 0x00;
	else	/* 48hs, 48phs, 96hs */
		vrr_cmds->cmds[idx].ss_txbuf[1] = 0x03;

	/* AOR */
	idx = ss_get_cmd_idx(vrr_cmds, 0xC5, 0x9A);
	if ((cur_rr == 96 || (cur_rr == 48 && cur_phs))
		&& vdd->br_info.common_br.bl_level <= 1)
		vrr_cmds->cmds[idx].ss_txbuf[2] = 0x4F;
	else
		vrr_cmds->cmds[idx].ss_txbuf[2] = 0x50;

	/* VFP - 120, 96 */
	idx = ss_get_cmd_idx(vrr_cmds, 0x06, 0xF2);
	if (cur_rr == 96 || (cur_rr == 48 && cur_phs))
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_96HZ_SETTING, sizeof(VFP_96HZ_SETTING) / sizeof(u8));
	else	/* 120hz */
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_120HZ_SETTING, sizeof(VFP_120HZ_SETTING) / sizeof(u8));

	/* VFP - 60, 48 */
	idx = ss_get_cmd_idx(vrr_cmds, 0x0B, 0xF2);
	if (cur_rr == 48 && !cur_phs)
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_48HZ_SETTING, sizeof(VFP_48HZ_SETTING) / sizeof(u8));
	else	/* 60hz */
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_60HZ_SETTING[cur_hs], sizeof(VFP_60HZ_SETTING[0]) / sizeof(u8));

	/* AID offset */
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0x9A);
	if (idx < 0) {
		idx = ss_get_cmd_idx(vrr_cmds, 0xCD, 0x9A);
		if (idx < 0)
			idx = ss_get_cmd_idx(vrr_cmds, 0xC9, 0x9A);
	}
	if (is_hbm)
		vrr_cmds->cmds[idx - 1].ss_txbuf[1] = 0xCD;
	else
		vrr_cmds->cmds[idx - 1].ss_txbuf[1] = 0xC9;

	/* AID */
	memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], AID_SETTING[cur_hs], sizeof(AID_SETTING[0]) / sizeof(u8));

	/* Source */
	idx = ss_get_cmd_idx(vrr_cmds, 0x16, 0xF6);
	memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], SOURCE_SETTING[cur_hs], sizeof(SOURCE_SETTING[0]) / sizeof(u8));

	/* TE moduration */
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0xB9);
	if (cur_phs) {
		vrr_cmds->cmds[idx].ss_txbuf[1] = 0x11;
		vrr_cmds->cmds[idx].ss_txbuf[2] = 0x90;
		vrr_cmds->cmds[idx].ss_txbuf[3] = 0x6F;
		vrr_cmds->cmds[idx].ss_txbuf[4] = 0x0F;
	} else {
		vrr_cmds->cmds[idx].ss_txbuf[1] = 0x00;
		vrr_cmds->cmds[idx].ss_txbuf[2] = 0x00;
		vrr_cmds->cmds[idx].ss_txbuf[3] = 0x00;
		vrr_cmds->cmds[idx].ss_txbuf[4] = 0x00;

	}

	/* HS <-> NS case during vrr changing */
	if (((prev_hs && !cur_hs) || (!prev_hs && cur_hs)) && vrr->running_vrr)
		vdd->need_brightness_lock = 1;
	else
		vdd->need_brightness_lock = 0;

	LCD_INFO(vdd, "VRR: (prev: %d%s -> cur: %d%s) brightness_lock (%d)\n",
			prev_rr, prev_hs ? (prev_phs ? "PHS" : "HS") : "NM",
			cur_rr, cur_hs ? (cur_phs ? "PHS" : "HS") : "NM",
			vdd->need_brightness_lock);

	return vrr_cmds;
}

static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2_normal(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
	        LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_NORMAL);

	LCD_INFO(vdd, "NORMAL : cd_idx [%d] \n", vdd->br_info.common_br.cd_idx);

	/* Smooth transition : 0x28 */
	idx = ss_get_cmd_idx(pcmds, 0x00, 0x53);
	pcmds->cmds[idx].ss_txbuf[1] = vdd->finger_mask_updated ? 0x20 : 0x28;

	/* smooth transition frame */
	idx = ss_get_cmd_idx(pcmds, 0x05, 0xC6);
#if 0
	if (vdd->panel_lpm.during_ctrl)
		pcmds->cmds[idx].ss_txbuf[2] = 0x01;	// 1-frame
	else
#endif
		pcmds->cmds[idx].ss_txbuf[2] = 0x18;	// 24-frame

	/* IRC mode - 0x6F: Moderato mode, 0x2F: flat gamma mode */
	idx = ss_get_cmd_idx(pcmds, 0x0B, 0x92);
	if (vdd->br_info.common_br.irc_mode == IRC_MODERATO_MODE)
		pcmds->cmds[idx].ss_txbuf[1] = IRC_MODERATO_MODE_VAL;
	else if (vdd->br_info.common_br.irc_mode == IRC_FLAT_GAMMA_MODE)
		pcmds->cmds[idx].ss_txbuf[1] = IRC_FLAT_GAMMA_MODE_VAL;

	/* TSET */
	idx = ss_get_cmd_idx(pcmds, 0x2D, 0xC6);
	pcmds->cmds[idx].ss_txbuf[1] = vdd->br_info.temperature > 0 ?
		vdd->br_info.temperature : (char)(BIT(7) | (-1*vdd->br_info.temperature));

	/* WRDISDV */
	idx = ss_get_cmd_idx(pcmds, 0x00, 0x51);
	pcmds->cmds[idx].ss_txbuf[1] = (vdd->br_info.common_br.gm2_wrdisbv & 0xFF00) >> 8;
	pcmds->cmds[idx].ss_txbuf[2] = vdd->br_info.common_br.gm2_wrdisbv & 0xFF;

	return pcmds;
}

static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2_hbm(struct samsung_display_driver_data *vdd, int *level_key)
{
    struct dsi_panel_cmd_set *pcmds;
	int idx = 0;

    if (IS_ERR_OR_NULL(vdd)) {
        LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
        return NULL;
    }

    pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_HBM);

	LCD_INFO(vdd, "HBM : cd_idx [%d] \n", vdd->br_info.common_br.cd_idx);

	/* HBM Smooth transition : 0xE8 */
	idx = ss_get_cmd_idx(pcmds, 0x00, 0x53);
	pcmds->cmds[idx].ss_txbuf[1] = vdd->finger_mask_updated ? 0xE0 : 0xE0;

	/* ELVSS */
	idx = ss_get_cmd_idx(pcmds, 0x05, 0xC6);
	pcmds->cmds[idx].ss_txbuf[1] = elvss_table_hbm_revA[vdd->br_info.common_br.cd_idx][ELVSS_DIM_OFF]; 	/* ELVSS Value for HBM brgihtness */
	if (vdd->panel_lpm.during_ctrl)
		pcmds->cmds[idx].ss_txbuf[2] = 0x01;	// 1-frame
	else
		pcmds->cmds[idx].ss_txbuf[2] = 0x18;	// 24-frame

	/* TSET */
	idx = ss_get_cmd_idx(pcmds, 0x2D, 0xC6);
	pcmds->cmds[idx].ss_txbuf[1] = vdd->br_info.temperature > 0 ?
		vdd->br_info.temperature : (char)(BIT(7) | (-1*vdd->br_info.temperature));

	/* WRDISBV_HBM */
	idx = ss_get_cmd_idx(pcmds, 0x00, 0x51);
	pcmds->cmds[idx].ss_txbuf[1] = (vdd->br_info.common_br.gm2_wrdisbv & 0xFF00) >> 8;
	pcmds->cmds[idx].ss_txbuf[2] = vdd->br_info.common_br.gm2_wrdisbv & 0xFF;

    return pcmds;
}

static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2_hmt(struct samsung_display_driver_data *vdd, int *level_key)
{
 	struct dsi_panel_cmd_set *pcmds;

	if (IS_ERR_OR_NULL(vdd)) {
	        LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_HMT);

	pcmds->cmds[0].ss_txbuf[1] = (vdd->br_info.common_br.gm2_wrdisbv & 0xFF00) >> 8;
	pcmds->cmds[0].ss_txbuf[2] = vdd->br_info.common_br.gm2_wrdisbv & 0xFF;

	LCD_INFO(vdd, "cd_idx: %d, cd_level: %d, WRDISBV: %x %x\n",
			vdd->br_info.common_br.cd_idx,
			vdd->br_info.common_br.cd_level,
			pcmds->cmds[0].ss_txbuf[1],
			pcmds->cmds[0].ss_txbuf[2]);

	return pcmds;
}

static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	char ddi_id[5];
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (D6h 1~5th) for CHIP ID */
	if (ss_get_cmds(vdd, RX_DDI_ID)->count) {
		ss_panel_data_read(vdd, RX_DDI_ID, ddi_id, LEVEL1_KEY);

		for (loop = 0; loop < 5; loop++)
			vdd->ddi_id_dsi[loop] = ddi_id[loop];

		LCD_INFO(vdd, "DSI%d : %02x %02x %02x %02x %02x\n", vdd->ndx,
			vdd->ddi_id_dsi[0], vdd->ddi_id_dsi[1],
			vdd->ddi_id_dsi[2], vdd->ddi_id_dsi[3],
			vdd->ddi_id_dsi[4]);
	} else {
		LCD_ERR(vdd, "DSI%d no ddi_id_rx_cmds cmds", vdd->ndx);
		return false;
	}

	return true;
}

#define COORDINATE_DATA_SIZE 6
#define MDNIE_SCR_WR_ADDR	0x32
#define RGB_INDEX_SIZE 4
#define COEFFICIENT_DATA_SIZE 8

#define F1(x, y) (((y << 10) - (((x << 10) * 56) / 55) - (102 << 10)) >> 10)
#define F2(x, y) (((y << 10) + (((x << 10) * 5) / 1) - (18483 << 10)) >> 10)

static char coordinate_data_1[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xfa, 0x00, 0xf9, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfc, 0x00, 0xff, 0x00}, /* Tune_2 */
	{0xf8, 0x00, 0xf7, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfd, 0x00, 0xf9, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xf8, 0x00, 0xfa, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfd, 0x00, 0xff, 0x00, 0xf8, 0x00}, /* Tune_7 */
	{0xfb, 0x00, 0xff, 0x00, 0xfc, 0x00}, /* Tune_8 */
	{0xf8, 0x00, 0xfd, 0x00, 0xff, 0x00}, /* Tune_9 */
};

static char coordinate_data_2[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xf8, 0x00, 0xf0, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xf9, 0x00, 0xf6, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xfb, 0x00, 0xfd, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfb, 0x00, 0xf0, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xff, 0x00, 0xfd, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xfe, 0x00, 0xf0, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xff, 0x00, 0xf6, 0x00}, /* Tune_8 */
	{0xfc, 0x00, 0xff, 0x00, 0xfa, 0x00}, /* Tune_9 */
};

static char (*coordinate_data[MAX_MODE])[COORDINATE_DATA_SIZE] = {
	coordinate_data_2,
	coordinate_data_2,
	coordinate_data_2,
	coordinate_data_1,
	coordinate_data_1,
	coordinate_data_1
};

static int rgb_index[][RGB_INDEX_SIZE] = {
	{ 5, 5, 5, 5 }, /* dummy */
	{ 5, 2, 6, 3 },
	{ 5, 2, 4, 1 },
	{ 5, 8, 4, 7 },
	{ 5, 8, 6, 9 },
};

static int coefficient[][COEFFICIENT_DATA_SIZE] = {
	{       0,        0,      0,      0,      0,       0,      0,      0 }, /* dummy */
	{  -52615,   -61905,  21249,  15603,  40775,   80902, -19651, -19618 },
	{ -212096,  -186041,  61987,  65143, -75083,  -27237,  16637,  15737 },
	{   69454,    77493, -27852, -19429, -93856, -133061,  37638,  35353 },
	{  192949,   174780, -56853, -60597,  57592,   13018, -11491, -10757 },
};


static int mdnie_coordinate_index(int x, int y)
{
	int tune_number = 0;

	if (F1(x, y) > 0) {
		if (F2(x, y) > 0) {
			tune_number = 1;
		} else {
			tune_number = 2;
		}
	} else {
		if (F2(x, y) > 0) {
			tune_number = 4;
		} else {
			tune_number = 3;
		}
	}

	return tune_number;
}

static int mdnie_coordinate_x(int x, int y, int index)
{
	int result = 0;

	result = (coefficient[index][0] * x) + (coefficient[index][1] * y) + (((coefficient[index][2] * x + 512) >> 10) * y) + (coefficient[index][3] * 10000);

	result = (result + 512) >> 10;

	if (result < 0)
		result = 0;
	if (result > 1024)
		result = 1024;

	return result;
}

static int mdnie_coordinate_y(int x, int y, int index)
{
	int result = 0;

	result = (coefficient[index][4] * x) + (coefficient[index][5] * y) + (((coefficient[index][6] * x + 512) >> 10) * y) + (coefficient[index][7] * 10000);

	result = (result + 512) >> 10;

	if (result < 0)
		result = 0;
	if (result > 1024)
		result = 1024;

	return result;
}


static int ss_elvss_read(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (C6h 8th) for grayspot */
	ss_panel_data_read(vdd, RX_ELVSS, vdd->br_info.common_br.elvss_value, LEVEL1_KEY);

	return true;
}

static void ss_gray_spot(struct samsung_display_driver_data *vdd, int enable)
{
	struct dsi_panel_cmd_set *pcmds;
	int idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return;
	}

	pcmds = ss_get_cmds(vdd, TX_GRAY_SPOT_TEST_OFF);
	if (IS_ERR_OR_NULL(pcmds)) {
        LCD_ERR(vdd, "Invalid pcmds\n");
        return;
    }

	if (!enable) { /* grayspot off */
		/* restore ELVSS_CAL_OFFSET (C6h 8th) */
		idx = ss_get_cmd_idx(pcmds, 0x00, 0xC6);
		pcmds->cmds[idx].ss_txbuf[8] = vdd->br_info.common_br.elvss_value[0];
	}
}

static int dsi_update_mdnie_data(struct samsung_display_driver_data *vdd)
{
	struct mdnie_lite_tune_data *mdnie_data;

	mdnie_data = kzalloc(sizeof(struct mdnie_lite_tune_data), GFP_KERNEL);
	if (!mdnie_data) {
		LCD_ERR(vdd, "fail to allocate mdnie_data memory\n");
		return -ENOMEM;
	}

	/* Update mdnie command */
	mdnie_data->DSI_COLOR_BLIND_MDNIE_1 = DSI0_COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_1 = DSI0_RGB_SENSOR_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_2 = DSI0_RGB_SENSOR_MDNIE_2;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_3 = DSI0_RGB_SENSOR_MDNIE_3;
	mdnie_data->DSI_TRANS_DIMMING_MDNIE = DSI0_RGB_SENSOR_MDNIE_3;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE_2 = DSI0_UI_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_UI_STANDARD_MDNIE_2 = DSI0_UI_STANDARD_MDNIE_2;
	mdnie_data->DSI_UI_AUTO_MDNIE_2 = DSI0_UI_AUTO_MDNIE_2;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE_2 = DSI0_VIDEO_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE_2 = DSI0_VIDEO_STANDARD_MDNIE_2;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE_2 = DSI0_VIDEO_AUTO_MDNIE_2;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE_2 = DSI0_CAMERA_AUTO_MDNIE_2;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE_2 = DSI0_GALLERY_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE_2 = DSI0_GALLERY_STANDARD_MDNIE_2;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE_2 = DSI0_GALLERY_AUTO_MDNIE_2;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE_2 = DSI0_BROWSER_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE_2 = DSI0_BROWSER_STANDARD_MDNIE_2;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE_2 = DSI0_BROWSER_AUTO_MDNIE_2;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE_2 = DSI0_EBOOK_AUTO_MDNIE_2;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE_2 = DSI0_EBOOK_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE_2 = DSI0_EBOOK_STANDARD_MDNIE_2;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE_2 = DSI0_EBOOK_AUTO_MDNIE_2;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE_2 = DSI0_TDMB_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE_2 = DSI0_TDMB_STANDARD_MDNIE_2;
	mdnie_data->DSI_TDMB_AUTO_MDNIE_2 = DSI0_TDMB_AUTO_MDNIE_2;

	mdnie_data->DSI_BYPASS_MDNIE = DSI0_BYPASS_MDNIE;
	mdnie_data->DSI_NEGATIVE_MDNIE = DSI0_NEGATIVE_MDNIE;
	mdnie_data->DSI_COLOR_BLIND_MDNIE = DSI0_COLOR_BLIND_MDNIE;
	mdnie_data->DSI_HBM_CE_MDNIE = DSI0_HBM_CE_MDNIE;
	mdnie_data->DSI_HBM_CE_D65_MDNIE = DSI0_HBM_CE_D65_MDNIE;
	mdnie_data->DSI_RGB_SENSOR_MDNIE = DSI0_RGB_SENSOR_MDNIE;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE = DSI0_UI_DYNAMIC_MDNIE;
	mdnie_data->DSI_UI_STANDARD_MDNIE = DSI0_UI_STANDARD_MDNIE;
	mdnie_data->DSI_UI_NATURAL_MDNIE = DSI0_UI_NATURAL_MDNIE;
	mdnie_data->DSI_UI_AUTO_MDNIE = DSI0_UI_AUTO_MDNIE;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE = DSI0_VIDEO_DYNAMIC_MDNIE;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE = DSI0_VIDEO_STANDARD_MDNIE;
	mdnie_data->DSI_VIDEO_NATURAL_MDNIE = DSI0_VIDEO_NATURAL_MDNIE;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE = DSI0_VIDEO_AUTO_MDNIE;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE = DSI0_CAMERA_AUTO_MDNIE;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE = DSI0_GALLERY_DYNAMIC_MDNIE;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE = DSI0_GALLERY_STANDARD_MDNIE;
	mdnie_data->DSI_GALLERY_NATURAL_MDNIE = DSI0_GALLERY_NATURAL_MDNIE;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE = DSI0_GALLERY_AUTO_MDNIE;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE = DSI0_BROWSER_DYNAMIC_MDNIE;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE = DSI0_BROWSER_STANDARD_MDNIE;
	mdnie_data->DSI_BROWSER_NATURAL_MDNIE = DSI0_BROWSER_NATURAL_MDNIE;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE = DSI0_BROWSER_AUTO_MDNIE;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE = DSI0_EBOOK_DYNAMIC_MDNIE;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE = DSI0_EBOOK_STANDARD_MDNIE;
	mdnie_data->DSI_EBOOK_NATURAL_MDNIE = DSI0_EBOOK_NATURAL_MDNIE;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE = DSI0_EBOOK_AUTO_MDNIE;
	mdnie_data->DSI_EMAIL_AUTO_MDNIE = DSI0_EMAIL_AUTO_MDNIE;
	mdnie_data->DSI_GAME_LOW_MDNIE = DSI0_GAME_LOW_MDNIE;
	mdnie_data->DSI_GAME_MID_MDNIE = DSI0_GAME_MID_MDNIE;
	mdnie_data->DSI_GAME_HIGH_MDNIE = DSI0_GAME_HIGH_MDNIE;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE = DSI0_TDMB_DYNAMIC_MDNIE;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE = DSI0_TDMB_STANDARD_MDNIE;
	mdnie_data->DSI_TDMB_NATURAL_MDNIE = DSI0_TDMB_NATURAL_MDNIE;
	mdnie_data->DSI_TDMB_AUTO_MDNIE = DSI0_TDMB_AUTO_MDNIE;
	mdnie_data->DSI_GRAYSCALE_MDNIE = DSI0_GRAYSCALE_MDNIE;
	mdnie_data->DSI_GRAYSCALE_NEGATIVE_MDNIE = DSI0_GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data->DSI_CURTAIN = DSI0_SCREEN_CURTAIN_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE = DSI0_NIGHT_MODE_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE_SCR = DSI0_NIGHT_MODE_MDNIE_1;
	mdnie_data->DSI_COLOR_LENS_MDNIE = DSI0_COLOR_LENS_MDNIE;
	mdnie_data->DSI_COLOR_LENS_MDNIE_SCR = DSI0_COLOR_LENS_MDNIE_1;
	mdnie_data->DSI_COLOR_BLIND_MDNIE_SCR = DSI0_COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_SCR = DSI0_RGB_SENSOR_MDNIE_1;

	mdnie_data->mdnie_tune_value_dsi = mdnie_tune_value_dsi0;
	mdnie_data->hmt_color_temperature_tune_value_dsi = hmt_color_temperature_tune_value_dsi0;
	mdnie_data->light_notification_tune_value_dsi = light_notification_tune_value_dsi0;
	mdnie_data->hdr_tune_value_dsi = hdr_tune_value_dsi0;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data->dsi_bypass_mdnie_size = ARRAY_SIZE(DSI0_BYPASS_MDNIE);
	mdnie_data->mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET;
	mdnie_data->mdnie_step_index[MDNIE_STEP1] = MDNIE_STEP1_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP2] = MDNIE_STEP2_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP3] = MDNIE_STEP3_INDEX;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_RED_OFFSET] = ADDRESS_SCR_WHITE_RED;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_GREEN_OFFSET] = ADDRESS_SCR_WHITE_GREEN;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_BLUE_OFFSET] = ADDRESS_SCR_WHITE_BLUE;
	mdnie_data->dsi_rgb_sensor_mdnie_1_size = DSI0_RGB_SENSOR_MDNIE_1_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_2_size = DSI0_RGB_SENSOR_MDNIE_2_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_3_size = DSI0_RGB_SENSOR_MDNIE_3_SIZE;

	mdnie_data->dsi_trans_dimming_data_index = MDNIE_TRANS_DIMMING_DATA_INDEX;

	mdnie_data->dsi_adjust_ldu_table = adjust_ldu_data;
	mdnie_data->dsi_max_adjust_ldu = 6;
	mdnie_data->dsi_night_mode_table = night_mode_data;
	mdnie_data->dsi_max_night_mode_index = 102;
	mdnie_data->dsi_color_lens_table = color_lens_data;
	mdnie_data->dsi_white_default_r = 0xff;
	mdnie_data->dsi_white_default_g = 0xff;
	mdnie_data->dsi_white_default_b = 0xff;
	mdnie_data->dsi_white_balanced_r = 0;
	mdnie_data->dsi_white_balanced_g = 0;
	mdnie_data->dsi_white_balanced_b = 0;
	mdnie_data->dsi_scr_step_index = MDNIE_STEP1_INDEX;
	mdnie_data->dsi_afc_size = 45;
	mdnie_data->dsi_afc_index = 33;

	vdd->mdnie.mdnie_data = mdnie_data;

	return 0;
}

static int ss_module_info_read(struct samsung_display_driver_data *vdd)
{
	unsigned char buf[11];
	int year, month, day;
	int hour, min;
	int x, y;
	int mdnie_tune_index = 0;
	int ret;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	if (ss_get_cmds(vdd, RX_MODULE_INFO)->count) {
		ret = ss_panel_data_read(vdd, RX_MODULE_INFO, buf, LEVEL1_KEY);
		if (ret) {
			LCD_ERR(vdd, "fail to read module ID, ret: %d", ret);
			return false;
		}

		/* Manufacture Date */

		year = buf[4] & 0xf0;
		year >>= 4;
		year += 2011; // 0 = 2011 year
		month = buf[4] & 0x0f;
		day = buf[5] & 0x1f;
		hour = buf[6] & 0x0f;
		min = buf[7] & 0x1f;

		vdd->manufacture_date_dsi = year * 10000 + month * 100 + day;
		vdd->manufacture_time_dsi = hour * 100 + min;

		LCD_INFO(vdd, "manufacture_date DSI%d = (%d%04d) - year(%d) month(%d) day(%d) hour(%d) min(%d)\n",
			vdd->ndx, vdd->manufacture_date_dsi, vdd->manufacture_time_dsi,
			year, month, day, hour, min);

		/* While Coordinates */

		vdd->mdnie.mdnie_x = buf[0] << 8 | buf[1];	/* X */
		vdd->mdnie.mdnie_y = buf[2] << 8 | buf[3];	/* Y */

		mdnie_tune_index = mdnie_coordinate_index(vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);

		if (((vdd->mdnie.mdnie_x - 3050) * (vdd->mdnie.mdnie_x - 3050) + (vdd->mdnie.mdnie_y - 3210) * (vdd->mdnie.mdnie_y - 3210)) <= 225) {
			x = 0;
			y = 0;
		} else {
			x = mdnie_coordinate_x(vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y, mdnie_tune_index);
			y = mdnie_coordinate_y(vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y, mdnie_tune_index);
		}

		coordinate_tunning_calculate(vdd, x, y, coordinate_data,
				rgb_index[mdnie_tune_index],
				MDNIE_SCR_WR_ADDR, COORDINATE_DATA_SIZE);

		LCD_INFO(vdd, "DSI%d : X-%d Y-%d \n", vdd->ndx,
			vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);

		/* CELL ID (manufacture date + white coordinates) */
		/* Manufacture Date */
		vdd->cell_id_dsi[0] = buf[4];
		vdd->cell_id_dsi[1] = buf[5];
		vdd->cell_id_dsi[2] = buf[6];
		vdd->cell_id_dsi[3] = buf[7];
		vdd->cell_id_dsi[4] = buf[8];
		vdd->cell_id_dsi[5] = buf[9];
		vdd->cell_id_dsi[6] = buf[10];
		/* White Coordinates */
		vdd->cell_id_dsi[7] = buf[0];
		vdd->cell_id_dsi[8] = buf[1];
		vdd->cell_id_dsi[9] = buf[2];
		vdd->cell_id_dsi[10] = buf[3];

		LCD_INFO(vdd, "DSI%d CELL ID : %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			vdd->ndx, vdd->cell_id_dsi[0],
			vdd->cell_id_dsi[1],	vdd->cell_id_dsi[2],
			vdd->cell_id_dsi[3],	vdd->cell_id_dsi[4],
			vdd->cell_id_dsi[5],	vdd->cell_id_dsi[6],
			vdd->cell_id_dsi[7],	vdd->cell_id_dsi[8],
			vdd->cell_id_dsi[9],	vdd->cell_id_dsi[10]);
	} else {
		LCD_ERR(vdd, "DSI%d no module_info_rx_cmds cmds(%d)", vdd->ndx, vdd->panel_revision);
		return false;
	}

	return true;
}

static int ss_octa_id_read(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read Panel Unique OCTA ID (C9h 2nd~21th) */
	if (ss_get_cmds(vdd, RX_OCTA_ID)->count) {
		memset(vdd->octa_id_dsi, 0x00, MAX_OCTA_ID);

		ss_panel_data_read(vdd, RX_OCTA_ID,
				vdd->octa_id_dsi, LEVEL1_KEY);

		LCD_INFO(vdd, "octa id: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			vdd->octa_id_dsi[0], vdd->octa_id_dsi[1],
			vdd->octa_id_dsi[2], vdd->octa_id_dsi[3],
			vdd->octa_id_dsi[4], vdd->octa_id_dsi[5],
			vdd->octa_id_dsi[6], vdd->octa_id_dsi[7],
			vdd->octa_id_dsi[8], vdd->octa_id_dsi[9],
			vdd->octa_id_dsi[10], vdd->octa_id_dsi[11],
			vdd->octa_id_dsi[12], vdd->octa_id_dsi[13],
			vdd->octa_id_dsi[14], vdd->octa_id_dsi[15],
			vdd->octa_id_dsi[16], vdd->octa_id_dsi[17],
			vdd->octa_id_dsi[18], vdd->octa_id_dsi[19]);

	} else {
		LCD_ERR(vdd, "DSI%d no octa_id_rx_cmds cmd\n", vdd->ndx);
		return false;
	}

	return true;
}

static struct dsi_panel_cmd_set *ss_acl_on(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_ACL_ON);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_ACL_ON..\n");
		return NULL;
	}

	idx = ss_get_cmd_idx(pcmds, 0x90, 0x9B);
	if (vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL) {
		pcmds->cmds[idx].ss_txbuf[1] = 0x0B;	/* 0x09 : 16Frame ACL OFF, 0x0B : 32Frame ACL ON */
		pcmds->cmds[idx].ss_txbuf[2] = 0x04;
		pcmds->cmds[idx].ss_txbuf[3] = 0x91;	/* 0x02 0x61 : ACL 8%, 0x04 0x91 : ACL 15% */
		pcmds->cmds[idx].ss_txbuf[6] = 0x41;
		pcmds->cmds[idx].ss_txbuf[7] = 0xFF;	/* 0x42 0x65 : 60%, 0x41 0xFF : 50% */
	} else {
		pcmds->cmds[idx].ss_txbuf[1] = 0x0B;	/* 0x09 : 16Frame ACL OFF, 0x0B : 32Frame ACL ON */
		pcmds->cmds[idx].ss_txbuf[2] = 0x02;
		pcmds->cmds[idx].ss_txbuf[3] = 0x61;	/* 0x02 0x61 : ACL 8%, 0x04 0x91 : ACL 15% */
		pcmds->cmds[idx].ss_txbuf[6] = 0x41;
		pcmds->cmds[idx].ss_txbuf[7] = 0xFF;	/* 0x42 0x65 : 60%, 0x41 0xFF : 50% */
	}

	/* ACL 30% */
	if (vdd->br_info.gradual_acl_val == 2) {
		pcmds->cmds[idx].ss_txbuf[2] = 0x09;
		pcmds->cmds[idx].ss_txbuf[3] = 0x91;	/* 0x09 0x91 : ACL 30% */
	}

	idx = ss_get_cmd_idx(pcmds, 0x9F, 0x9B);
	pcmds->cmds[idx].ss_txbuf[1] = 0x20;	/* 0x20 : ACL DIM 32Frame, 0x00 : ACL DIM OFF */

	LCD_INFO(vdd, "bl_level: %d, gradual_acl: %d, acl per: 0x%x", vdd->br_info.common_br.bl_level,
			vdd->br_info.gradual_acl_val, pcmds->cmds[0].ss_txbuf[1]);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_acl_off(struct samsung_display_driver_data *vdd, int *level_key)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	LCD_INFO(vdd, "off\n");
	return ss_get_cmds(vdd, TX_ACL_OFF);
}

static struct dsi_panel_cmd_set *ss_vint(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *vint_cmds = ss_get_cmds(vdd, TX_VINT);
	int idx = 0;

	if (IS_ERR_OR_NULL(vdd) || SS_IS_CMDS_NULL(vint_cmds)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)vint_cmds);
		return NULL;
	}

	idx = ss_get_cmd_idx(vint_cmds, 0x19, 0xF4);

	if (vdd->xtalk_mode)
		vint_cmds->cmds[idx].ss_txbuf[1] = 0x05; // ON
	else
		vint_cmds->cmds[idx].ss_txbuf[1] = 0x0C; // OFF

	return vint_cmds;
}

static void ss_set_panel_lpm_brightness(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *set = ss_get_cmds(vdd, TX_LPM_BL_CMD);
	struct dsi_panel_cmd_set *set_lpm_bl;
	int lpm_on_idx, lpm_bl_ctrl_idx;

	if (SS_IS_CMDS_NULL(set)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_BL_CMD\n");
		return;
	}

	lpm_bl_ctrl_idx = ss_get_cmd_idx(set, 0xD1, 0x9A);
	lpm_on_idx = ss_get_cmd_idx(set, 0x00, 0x53);

	/* 0x53 - LPM ON */
	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_60NIT:
		set->cmds[lpm_on_idx].ss_txbuf[1] = 0x22;
		set_lpm_bl = ss_get_cmds(vdd, TX_HLPM_60NIT_CMD);
		break;
	case LPM_30NIT:
		set->cmds[lpm_on_idx].ss_txbuf[1] = 0x22;
		set_lpm_bl = ss_get_cmds(vdd, TX_HLPM_30NIT_CMD);
		break;
	case LPM_10NIT:
		set->cmds[lpm_on_idx].ss_txbuf[1] = 0x22;
		set_lpm_bl = ss_get_cmds(vdd, TX_HLPM_10NIT_CMD);
		break;
	case LPM_2NIT:
	default:
		set->cmds[lpm_on_idx].ss_txbuf[1] = 0x23;
		set_lpm_bl = ss_get_cmds(vdd, TX_HLPM_2NIT_CMD);
		break;
	}

	if (SS_IS_CMDS_NULL(set_lpm_bl)) {
		LCD_ERR(vdd, "No cmds for alpm_ctrl..\n");
		return;
	}

	/* 0x9A - LPM BL Contrl */
	memcpy(&set->cmds[lpm_bl_ctrl_idx].ss_txbuf[1],
			&set_lpm_bl->cmds->ss_txbuf[1],
			sizeof(char) * (set->cmds[lpm_bl_ctrl_idx].msg.tx_len - 1));

	/* send lpm bl cmd */
	ss_send_cmd(vdd, TX_LPM_BL_CMD);

	LCD_INFO(vdd, "[Panel LPM] bl_level : %s\n",
			/* Check current brightness level */
			vdd->panel_lpm.lpm_bl_level == LPM_2NIT ? "2NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_10NIT ? "10NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_30NIT ? "30NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_60NIT ? "60NIT" : "UNKNOWN");
}

/*
 * This function will update parameters for LPM cmds
 * mode, brightness are updated here.
 */
static void ss_update_panel_lpm_ctrl_cmd
			(struct samsung_display_driver_data *vdd, int enable)
{
	struct dsi_panel_cmd_set *set_lpm;
	struct dsi_panel_cmd_set *set_lpm_bl;
	int lpm_on_idx, lpm_bl_ctrl_idx, lpm_ctrl_idx, wrdisvb_idx;
	int gm2_wrdisbv;

	if (enable) {
		set_lpm = ss_get_cmds(vdd, TX_LPM_ON);
	} else {
		set_lpm = ss_get_cmds(vdd, TX_LPM_OFF);
	}

	if (SS_IS_CMDS_NULL(set_lpm)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_ON/OFF\n");
		return;
	}

	lpm_bl_ctrl_idx = ss_get_cmd_idx(set_lpm, 0xD1, 0x9A);
	lpm_ctrl_idx = ss_get_cmd_idx(set_lpm, 0x00, 0x69);
	lpm_on_idx = ss_get_cmd_idx(set_lpm, 0x00, 0x53);

	/* 0x69 - LPM mode Contrl */
	if (lpm_ctrl_idx >= 0) {
		if (vdd->panel_lpm.mode == HLPM_MODE_ON)
			set_lpm->cmds[lpm_ctrl_idx].ss_txbuf[1] = 0x00;
		else
			set_lpm->cmds[lpm_ctrl_idx].ss_txbuf[1] = 0x20;
	}

	/* 0x53 - LPM ON/OFF */
	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_60NIT:
		set_lpm->cmds[lpm_on_idx].ss_txbuf[1] = enable ? 0x22 : 0x20;
		set_lpm_bl = ss_get_cmds(vdd, TX_HLPM_60NIT_CMD);
		gm2_wrdisbv = 245;
		break;
	case LPM_30NIT:
		set_lpm->cmds[lpm_on_idx].ss_txbuf[1] = enable ? 0x22 : 0x20;
		set_lpm_bl = ss_get_cmds(vdd, TX_HLPM_30NIT_CMD);
		gm2_wrdisbv = 125;
		break;
	case LPM_10NIT:
		set_lpm->cmds[lpm_on_idx].ss_txbuf[1] = enable ? 0x22 : 0x20;
		set_lpm_bl = ss_get_cmds(vdd, TX_HLPM_10NIT_CMD);
		gm2_wrdisbv = 44;
		break;
	case LPM_2NIT:
	default:
		set_lpm->cmds[lpm_on_idx].ss_txbuf[1] = enable ? 0x23 : 0x21;
		set_lpm_bl = ss_get_cmds(vdd, TX_HLPM_2NIT_CMD);
		gm2_wrdisbv = 8;
		break;
	}

	if (!enable) {
		wrdisvb_idx = ss_get_cmd_idx(set_lpm, 0x00, 0x51);
		set_lpm->cmds[wrdisvb_idx].ss_txbuf[1] = (gm2_wrdisbv & 0xFF00) >> 8;
		set_lpm->cmds[wrdisvb_idx].ss_txbuf[2] = gm2_wrdisbv & 0xFF;
	}

	if (SS_IS_CMDS_NULL(set_lpm_bl)) {
		LCD_ERR(vdd, "No cmds for alpm_ctrl..\n");
		return;
	}

	/* 0x9A - LPM BL Contrl */
	if (lpm_bl_ctrl_idx >= 0) {
		if (enable) {
			memcpy(&set_lpm->cmds[lpm_bl_ctrl_idx].ss_txbuf[1],
					&set_lpm_bl->cmds->ss_txbuf[1],
					sizeof(char) * set_lpm->cmds[lpm_bl_ctrl_idx].msg.tx_len - 1);
		}
	}
}

static int ss_gct_read(struct samsung_display_driver_data *vdd)
{
	u8 valid_checksum[4] = {0x69, 0x69, 0x69, 0x69};
	int res;

	if (!vdd->gct.is_support) {
		LCD_ERR(vdd, "GCT is not supported\n");
		return GCT_RES_CHECKSUM_NOT_SUPPORT;
	}

	if (!vdd->gct.on) {
		LCD_ERR(vdd, "GCT is not ON\n");
		return GCT_RES_CHECKSUM_OFF;
	}

	if (!memcmp(vdd->gct.checksum, valid_checksum, 4))
		res = GCT_RES_CHECKSUM_PASS;
	else
		res = GCT_RES_CHECKSUM_NG;

	return res;
}

static int ss_gct_write(struct samsung_display_driver_data *vdd)
{
	u8 *checksum;
	int i, idx;
	/* vddm set, 0x0: normal, 0x0F: LV, 0x2D: HV */
	u8 vddm_set[MAX_VDDM] = {0x0, 0x0F, 0x2D};
	int ret = 0;
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct dsi_display *dsi_display = GET_DSI_DISPLAY(vdd);
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */
	struct dsi_panel_cmd_set *set;

	LCD_INFO(vdd, "+\n");

	set = ss_get_cmds(vdd, TX_GCT_ENTER);
	if (SS_IS_CMDS_NULL(set)) {
		LCD_ERR(vdd, "No cmds for TX_GCT_ENTER..\n");
		return ret;
	}

	vdd->gct.is_running = true;

	/* prevent sw reset to trigger esd recovery */
	LCD_INFO(vdd, "disable esd interrupt\n");

	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd);

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	for (i = TX_GCT_ENTER; i <= TX_GCT_EXIT; i++)
		ss_set_exclusive_tx_packet(vdd, i, 1);
	ss_set_exclusive_tx_packet(vdd, RX_GCT_CHECKSUM, 1);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 1);

	usleep_range(10000, 11000);

	idx = ss_get_cmd_idx(set, 0x0E, 0xF4);

	checksum = vdd->gct.checksum;
	for (i = VDDM_LV; i < MAX_VDDM; i++) {
		LCD_INFO(vdd, "(%d) TX_GCT_ENTER\n", i);
		/* VDDM LV set (0x0: 1.0V, 0x10: 0.9V, 0x30: 1.1V) */
		set->cmds[idx].ss_txbuf[1] = vddm_set[i];

		ss_send_cmd(vdd, TX_GCT_ENTER);

		msleep(150);

		ss_panel_data_read(vdd, RX_GCT_CHECKSUM, checksum++,
				LEVEL_KEY_NONE);
		LCD_INFO(vdd, "(%d) read checksum: %x\n", i, *(checksum - 1));

		LCD_INFO(vdd, "(%d) TX_GCT_MID\n", i);
		ss_send_cmd(vdd, TX_GCT_MID);

		msleep(150);

		ss_panel_data_read(vdd, RX_GCT_CHECKSUM, checksum++,
				LEVEL_KEY_NONE);

		LCD_INFO(vdd, "(%d) read checksum: %x\n", i, *(checksum - 1));

		LCD_INFO(vdd, "(%d) TX_GCT_EXIT\n", i);
		ss_send_cmd(vdd, TX_GCT_EXIT);
	}

	vdd->gct.on = 1;

	LCD_INFO(vdd, "checksum = {%x %x %x %x}\n",
			vdd->gct.checksum[0], vdd->gct.checksum[1],
			vdd->gct.checksum[2], vdd->gct.checksum[3]);

	if (dsi_display) {
		if (dsi_display->enabled == false) {
			LCD_ERR(vdd, "dsi_display is not enabled.. it may be turning off.\n");
			goto end;
		}
	}

	/* exit exclusive mode*/
	for (i = TX_GCT_ENTER; i <= TX_GCT_EXIT; i++)
		ss_set_exclusive_tx_packet(vdd, i, 0);
	ss_set_exclusive_tx_packet(vdd, RX_GCT_CHECKSUM, 0);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 0);

	/* Reset panel to enter nornal panel mode.
	 * The on commands also should be sent before update new frame.
	 * Next new frame update is blocked by exclusive_tx.enable
	 * in ss_event_frame_update_pre(), and it will be released
	 * by wake_up exclusive_tx.ex_tx_waitq.
	 * So, on commands should be sent before wake up the waitq
	 * and set exclusive_tx.enable to false.
	 */
	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_OFF, 1);
	ss_send_cmd(vdd, DSI_CMD_SET_OFF);

	vdd->panel_state = PANEL_PWR_OFF;
	dsi_panel_power_off(panel);
	dsi_panel_power_on(panel);
	vdd->panel_state = PANEL_PWR_ON_READY;

	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_ON, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_PPS, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_DISABLE, 1);

	ss_send_cmd(vdd, DSI_CMD_SET_ON);
	dsi_panel_update_pps(panel);

	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_OFF, 0);
	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_ON, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_PPS, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_DISABLE, 0);

	vdd->exclusive_tx.enable = 0;
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);

	vdd->mafpc.force_delay = true;
	ss_panel_on_post(vdd);

	vdd->gct.is_running = false;

	/* enable esd interrupt */
	LCD_INFO(vdd, "enable esd interrupt\n");

	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(true, true, (void *)vdd);

	return ret;

end:
	vdd->exclusive_tx.enable = 0;
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);

	vdd->gct.is_running = false;

	return ret;
}

static int ss_self_display_data_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "++\n");

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->self_disp.is_support) {
		LCD_ERR(vdd, "Self Display is not supported\n");
		return -EINVAL;
	}

	vdd->self_disp.operation[FLAG_SELF_MASK].img_buf = self_mask_img_data;
	vdd->self_disp.operation[FLAG_SELF_MASK].img_size = ARRAY_SIZE(self_mask_img_data);
	vdd->self_disp.operation[FLAG_SELF_MASK].img_checksum = SELF_MASK_IMG_CHECKSUM;
	make_self_dispaly_img_cmds_FAB(vdd, TX_SELF_MASK_IMAGE, FLAG_SELF_MASK);

	vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_buf = self_mask_img_fhd_crc_data;
	vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_size = ARRAY_SIZE(self_mask_img_fhd_crc_data);
	make_mass_self_display_img_cmds_FAB(vdd, TX_SELF_MASK_IMAGE_CRC, FLAG_SELF_MASK_CRC);

	vdd->self_disp.operation[FLAG_SELF_ICON].img_buf = self_icon_img_data;
	vdd->self_disp.operation[FLAG_SELF_ICON].img_size = ARRAY_SIZE(self_icon_img_data);
	make_mass_self_display_img_cmds_FAB(vdd, TX_SELF_ICON_IMAGE, FLAG_SELF_ICON);

	vdd->self_disp.operation[FLAG_SELF_ACLK].img_buf = self_aclock_img_data;
	vdd->self_disp.operation[FLAG_SELF_ACLK].img_size = ARRAY_SIZE(self_aclock_img_data);
	make_mass_self_display_img_cmds_FAB(vdd, TX_SELF_ACLOCK_IMAGE, FLAG_SELF_ACLK);

	vdd->self_disp.operation[FLAG_SELF_DCLK].img_buf = self_dclock_img_data;
	vdd->self_disp.operation[FLAG_SELF_DCLK].img_size = ARRAY_SIZE(self_dclock_img_data);
	make_mass_self_display_img_cmds_FAB(vdd, TX_SELF_DCLOCK_IMAGE, FLAG_SELF_DCLK);

	LCD_INFO(vdd, "--\n");
	return 1;
}

static int ss_mafpc_data_init(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	LCD_INFO(vdd, "mAFPC Panel Data init\n");

	vdd->mafpc.img_buf = mafpc_img_data;
	vdd->mafpc.img_size = ARRAY_SIZE(mafpc_img_data);

	if (vdd->mafpc.make_img_mass_cmds)
		vdd->mafpc.make_img_mass_cmds(vdd, vdd->mafpc.img_buf, vdd->mafpc.img_size, TX_MAFPC_IMAGE); /* Image Data */
	else if (vdd->mafpc.make_img_cmds)
		vdd->mafpc.make_img_cmds(vdd, vdd->mafpc.img_buf, vdd->mafpc.img_size, TX_MAFPC_IMAGE); /* Image Data */
	else {
		LCD_ERR(vdd, "Can not make mafpc image commands\n");
		return -EINVAL;
	}

	/* CRC Check For Factory Mode */
	vdd->mafpc.crc_img_buf = mafpc_img_data_crc_check;
	vdd->mafpc.crc_img_size = ARRAY_SIZE(mafpc_img_data_crc_check);

	if (vdd->mafpc.make_img_mass_cmds)
		vdd->mafpc.make_img_mass_cmds(vdd, vdd->mafpc.crc_img_buf, vdd->mafpc.crc_img_size, TX_MAFPC_CRC_CHECK_IMAGE); /* CRC Check Image Data */
	else if (vdd->mafpc.make_img_cmds)
		vdd->mafpc.make_img_cmds(vdd, vdd->mafpc.crc_img_buf, vdd->mafpc.crc_img_size, TX_MAFPC_CRC_CHECK_IMAGE); /* CRC Check Image Data */
	else {
		LCD_ERR(vdd, "Can not make mafpc image commands\n");
		return -EINVAL;
	}

	return true;
}

static void ss_copr_panel_init(struct samsung_display_driver_data *vdd)
{
	ss_copr_init(vdd);
}

static int ss_brightness_tot_level(struct samsung_display_driver_data *vdd)
{
	int tot_level_key = 0;

	/* HAB brightness setting requires F0h and FCh level key unlocked */
	/* DBV setting require TEST KEY3 (FCh) */
	tot_level_key = LEVEL1_KEY | LEVEL2_KEY;

	return tot_level_key;
}

static void ss_read_dsc_1(struct samsung_display_driver_data *vdd)
{
	u8 rbuf;
	int ret;

	ret = ss_panel_data_read(vdd, RX_LDI_DEBUG7, &rbuf, LEVEL_KEY_NONE);
	if (ret) {
		LCD_ERR(vdd, "fail to read RX_LDI_DEBUG7(ret=%d)\n", ret);
		return;
	}

	return;
}

static void ss_read_dsc_2(struct samsung_display_driver_data *vdd)
{
	u8 rbuf[89];
	int ret;
	char pBuffer[512];
	int j;

	ret = ss_panel_data_read(vdd, RX_LDI_DEBUG8, rbuf, LEVEL_KEY_NONE);
	if (ret) {
		LCD_ERR(vdd, "fail to read RX_LDI_DEBUG8(ret=%d)\n", ret);
		return;
	}

	memset(pBuffer, 0x00, 512);
	for (j = 0; j < 89; j++) {
		snprintf(pBuffer + strnlen(pBuffer, 512), 512, " %02x", rbuf[j]);
	}
	LCD_INFO(vdd, "PPS(read) : %s\n", pBuffer);

	for (j = 0; j < 89; j++) {
		if (vdd->qc_pps_cmd[j] != rbuf[j]) {
			LCD_ERR(vdd, "[%d] pps different!! %02x %02x\n", vdd->qc_pps_cmd[j], rbuf[j]);
			//panic("PPS");
		}
	}

	return;
}

static int samsung_panel_off_pre(struct samsung_display_driver_data *vdd)
{
	int rc = 0;

	/* DSC read test */
	ss_read_dsc_1(vdd);
	ss_read_dsc_2(vdd);

	return rc;
}

static int samsung_panel_off_post(struct samsung_display_driver_data *vdd)
{
	int rc = 0;
	return rc;
}

static int ss_vrr_init(struct vrr_info *vrr)
{
	struct samsung_display_driver_data *vdd =
		container_of(vrr, struct samsung_display_driver_data, vrr);

	LCD_INFO(vdd, "+++\n");

	mutex_init(&vrr->vrr_lock);
	mutex_init(&vrr->brr_lock);

	vrr->running_vrr_mdp = false;
	vrr->running_vrr = false;

	/* defult: FHD@120hz HS mode */
	vrr->cur_refresh_rate = vrr->adjusted_refresh_rate = 120;
	vrr->cur_sot_hs_mode = vrr->adjusted_sot_hs_mode = true;
	vrr->cur_phs_mode = vrr->adjusted_phs_mode = false;
	vrr->max_h_active_support_120hs = 1080; /* supports 120hz until FHD 1080 */

	vrr->hs_nm_seq = HS_NM_OFF;
	vrr->delayed_perf_normal = false;
	vrr->skip_vrr_in_brightness = false;

	vrr->vrr_workqueue = create_singlethread_workqueue("vrr_workqueue");
	INIT_WORK(&vrr->vrr_work, ss_panel_vrr_switch_work);

	vrr->brr_mode = BRR_OFF_MODE;

	LCD_INFO(vdd, "---\n");

	return 0;
}

static bool ss_check_support_mode(struct samsung_display_driver_data *vdd, enum CHECK_SUPPORT_MODE mode)
{
	bool is_support = true;
	int cur_rr = vdd->vrr.cur_refresh_rate;
	bool cur_hs = vdd->vrr.cur_sot_hs_mode;

	switch (mode) {
	case CHECK_SUPPORT_HMD:
		if (!(cur_rr == 60 && !cur_hs)) {
			is_support = false;
			LCD_ERR(vdd, "HMD fail: supported on 60NS(cur: %d%s)\n",
					cur_rr, cur_hs ? "HS" : "NS");
		}

		break;
#if 0
	case CHECK_SUPPORT_GCT:
		/* in ACT, force return to PASS when vrr is not 120hz */
		if (cur_rr != 120 && !(vdd->is_factory_mode)) {
			is_support = false;
			LCD_ERR(vdd, "GCT fail: supported on 120HS(cur: %d%s)\n",
					cur_rr, cur_hs ? "HS" : "NS");
		}
		break;

	default:
		if (cur_rr != 120) {
			is_support = false;
			LCD_ERR(vdd, "TEST fail: supported on 120HS(cur: %d)\n",
					cur_rr);
		}
		break;
#endif
	default:
		break;
	}

	return is_support;
}

#define FFC_REG	(0xC5)
static int ss_ffc(struct samsung_display_driver_data *vdd, int idx)
{
	struct dsi_panel_cmd_set *ffc_set;
	struct dsi_panel_cmd_set *dyn_ffc_set;
	struct dsi_panel_cmd_set *cmd_list[1];
	static int reg_list[1][2] = { {FFC_REG, -EINVAL} };
	int pos_ffc;

	LCD_INFO(vdd, "[DISPLAY_%d] +++ clk idx: %d, tx FFC\n", vdd->ndx, idx);

	ffc_set = ss_get_cmds(vdd, TX_FFC);
	dyn_ffc_set = ss_get_cmds(vdd, TX_DYNAMIC_FFC_SET);

	if (SS_IS_CMDS_NULL(ffc_set) || SS_IS_CMDS_NULL(dyn_ffc_set)) {
		LCD_ERR(vdd, "No cmds for TX_FFC..\n");
		return -EINVAL;
	}

	if (unlikely((reg_list[0][1] == -EINVAL))) {
		cmd_list[0] = ffc_set;
		ss_find_reg_offset(reg_list, cmd_list, ARRAY_SIZE(cmd_list));
	}

	pos_ffc = reg_list[0][1];
	if (pos_ffc == -EINVAL) {
		LCD_ERR(vdd, "fail to find FFC(C5h) offset in set\n");
		return -EINVAL;
	}

	memcpy(ffc_set->cmds[pos_ffc].ss_txbuf,
			dyn_ffc_set->cmds[idx].ss_txbuf,
			ffc_set->cmds[pos_ffc].msg.tx_len);

	ss_send_cmd(vdd, TX_FFC);

	LCD_INFO(vdd, "[DISPLAY_%d] --- clk idx: %d, tx FFC\n", vdd->ndx, idx);

	return 0;
}

/* mtp original data */
static int HS_V_TYPE_BUF[GAMMA_ROOM_MAX][GAMMA_SET_MAX][GAMMA_V_SIZE]; // 33 bytes (10bit)
static u8 HS_R_TYPE_BUF[GAMMA_ROOM_MAX][GAMMA_SET_MAX][GAMMA_R_SIZE]; // read_buf, 43 bytes

//int NS60_V_TYPE_BUF[GAMMA_SET_MAX][GAMMA_V_SIZE]; // 33 bytes (10bit)
static u8  NS60_R_TYPE_BUF[GAMMA_SET_MAX][GAMMA_R_SIZE]; // read_buf, 43 bytes

/* comp result data */
static int HS60_V_TYPE_COMP[MTP_OFFSET_TAB_SIZE_60HS][GAMMA_SET_MAX][GAMMA_V_SIZE]; // comp data  V type
static u8 HS60_R_TYPE_COMP[MTP_OFFSET_TAB_SIZE_60HS][GAMMA_SET_MAX][GAMMA_R_SIZE]; // comp data  Register type

static int HS96_V_TYPE_COMP[MTP_OFFSET_TAB_SIZE_96HS][GAMMA_SET_MAX][GAMMA_V_SIZE]; // comp data  V type
static u8 HS96_R_TYPE_COMP[MTP_OFFSET_TAB_SIZE_96HS][GAMMA_SET_MAX][GAMMA_R_SIZE]; // comp data  Register type

static int HS48_V_TYPE_COMP[MTP_OFFSET_TAB_SIZE_48HS][GAMMA_SET_MAX][GAMMA_V_SIZE]; // comp data  V type
static u8 HS48_R_TYPE_COMP[MTP_OFFSET_TAB_SIZE_48HS][GAMMA_SET_MAX][GAMMA_R_SIZE]; // comp data  Register type

/* read 60 HS/NS GAMMA value from flash */
static int ss_gm2_ddi_flash_prepare(struct samsung_display_driver_data *vdd)
{
	struct spi_device *spi_dev;
	struct ddi_spi_cmd_set *cmd_set = NULL;
	struct gm2_flash_table *gm2_flash_tbl;
	int i, j, r, ret;
	u32 checksum = 0;

	if (!apply_flash_gamma) {
		LCD_ERR(vdd, "Do not support flash gamma for gm2. force return to PASS\n");
		vdd->br_info.gm2_flash_checksum_cal = 0;
		vdd->br_info.gm2_flash_checksum_raw = 0;
		vdd->br_info.gm2_flash_write_check = 1;
		return 0;
	}

	/* read ddi flash data via SPI */
	spi_dev = vdd->spi_dev;
	if (IS_ERR_OR_NULL(spi_dev)) {
		LCD_ERR(vdd, "no spi_dev\n");
		ret = -ENODEV;
		goto err;
	}

	cmd_set = ss_get_spi_cmds(vdd, RX_DATA);
	if (cmd_set == NULL) {
		LCD_ERR(vdd, "cmd_set is null..\n");
		ret = -EINVAL;
		goto err;
	}

	gm2_flash_tbl = vdd->br_info.gm2_flash_tbl;
	if (gm2_flash_tbl == NULL) {
		LCD_ERR(vdd, "gm2_flash_tbl is null..\n");
		ret = -EINVAL;
		goto err;
	}

	LCD_INFO(vdd, "++ (%d)\n", vdd->br_info.gm2_flash_tbl_cnt);

	/* 1. Read raw data from flash */
	for (i = 0; i < vdd->br_info.gm2_flash_tbl_cnt; i++) {

		cmd_set->rx_size = gm2_flash_tbl[i].buf_len;
		cmd_set->rx_addr = gm2_flash_tbl[i].start_addr;

		ret = ss_spi_sync(spi_dev, gm2_flash_tbl[i].raw_buf, RX_DATA);
		if (ret) {
			LCD_ERR(vdd, "fail to spi read.. ret (%d) \n", ret);
			goto err;
		}

		/* checksum 16bit */
		if (i < 2) {	/* FLASH MAP is tbl 0,1 */
			for (j = 0; j < cmd_set->rx_size; j++)
				checksum += gm2_flash_tbl[i].raw_buf[j];
			checksum &= 0x0000FFFF;
		}

		if (i == 2) {	/* FLASH CHECKSUM is tbl 2 */
			vdd->br_info.gm2_flash_checksum_cal = checksum;
			vdd->br_info.gm2_flash_checksum_raw = (gm2_flash_tbl[i].raw_buf[0] << 8)
												| gm2_flash_tbl[i].raw_buf[1];

			LCD_INFO(vdd, "checksum(cal) 0x%lx, checksum(raw) 0x%x\n",
						vdd->br_info.gm2_flash_checksum_cal,
						vdd->br_info.gm2_flash_checksum_raw);
		}

		if (i == 3) {	/* GAMMA FLASH Write Check is tbl 3 */
			vdd->br_info.gm2_flash_write_check = gm2_flash_tbl[i].raw_buf[0];
			LCD_INFO(vdd, "write_check = %d\n", vdd->br_info.gm2_flash_write_check);
		}

		/* debug */
		if (gm2_flash_tbl[i].buf_len > 1)
			LCD_INFO(vdd, "0x%x 0x%x \n", gm2_flash_tbl[i].raw_buf[0], gm2_flash_tbl[i].raw_buf[1]);
		else
			LCD_INFO(vdd, "0x%x \n", gm2_flash_tbl[i].raw_buf[0]);
	}

	/* save NS60 GAMMA */
	for (i = GAMMA_SET_MAX - 1, r = 0; i >= GAMMA_SET_0; i--) {
		memcpy(NS60_R_TYPE_BUF[i], &gm2_flash_tbl[NS].raw_buf[r], GAMMA_R_SIZE);
		r += GAMMA_R_SIZE;
	}
err:
	LCD_INFO(vdd, "--\n");
	return 0;
}

#define FLASH_TEST_MODE_NUM     (1)

static int ss_test_ddi_flash_check(struct samsung_display_driver_data *vdd, char *buf)
{
	int len = 0;
	int ret = 0;

	ss_gm2_ddi_flash_prepare(vdd);

	if (vdd->br_info.gm2_flash_write_check) {
		if (vdd->br_info.gm2_flash_checksum_raw ==
			vdd->br_info.gm2_flash_checksum_cal)
			ret = 1;
	}

	len += sprintf(buf + len, "%d\n", FLASH_TEST_MODE_NUM);
	len += sprintf(buf + len, "%d %08x %08x %08d\n",
			ret,
			vdd->br_info.gm2_flash_checksum_raw,
			vdd->br_info.gm2_flash_checksum_cal,
			vdd->br_info.gm2_flash_write_check);
	return len;
}

/* result for gamma max check (DBV_G0 ~ G6, HBM) */
static u8 gamma_max_check_res[GAMMA_ROOM_MAX][GAMMA_SET_MAX];
static int ss_gamma_max_check(struct samsung_display_driver_data *vdd, char *buf)
{
	int len = 0;
	int ret = 1;
	int i, j;

	for (i = GAMMA_ROOM_120; i < GAMMA_ROOM_MAX; i++) {
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			if (gamma_max_check_res[i][j]) {
				LCD_INFO(vdd, "[%d] - [%d] %d\n", i == GAMMA_ROOM_120 ? 120 : 60,
					gamma_max_check_res[i][j]);
				ret = 0;
			}
		}
	}

	len += sprintf(buf + len, "%d\n", ret);

	return len;
}

static void ss_print_gamma_comp(struct samsung_display_driver_data *vdd)
{
	char pBuffer[256];
	int i, j, r, v;
	u8 *r_buf;
	int *v_buf;

	/* 120/60 HS MTP GAMMA print */
	memset(pBuffer, 0x00, 256);
	for (i = GAMMA_ROOM_120; i < GAMMA_ROOM_MAX; i++) {
		LCD_DEBUG(vdd, "=== [%dHS_MTP_GAMMA] ===\n", i == GAMMA_ROOM_120 ? 120 : 60);

		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			/* pointer of 120/60 HS R type buf */
			r_buf = HS_R_TYPE_BUF[i][j];

			memset(pBuffer, 0x00, 256);

			for (r = 0; r < GAMMA_R_SIZE; r++)
				snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", r_buf[r]);

			LCD_DEBUG(vdd, "SET[%d] : %s\n", j, pBuffer);
		}
	}

	/* 60 NS MTP GAMMA print */
	LCD_DEBUG(vdd, "=== [60NS_MTP_GAMMA - FLASH] ===\n");
	for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
		/* pointer of 120/60 HS R type buf */
		r_buf = NS60_R_TYPE_BUF[j];

		memset(pBuffer, 0x00, 256);

		for (r = 0; r < GAMMA_R_SIZE; r++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", r_buf[r]);

		LCD_DEBUG(vdd, "SET[%d] : %s\n", j, pBuffer);
	}

	/* debug print */
	memset(pBuffer, 0x00, 256);
	for (i = GAMMA_ROOM_120; i < GAMMA_ROOM_MAX; i++) {
		LCD_DEBUG(vdd, "=== %dHS_V_TYPE_BUF ===\n", i == GAMMA_ROOM_120 ? 120 : 60);
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			v_buf = HS_V_TYPE_BUF[i][j];
			for (v = 0; v < GAMMA_V_SIZE; v++)
				snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", v_buf[v]);
			LCD_DEBUG(vdd, "SET[%d] : %s\n", j, pBuffer);
			memset(pBuffer, 0x00, 256);
		}
	}

	LCD_DEBUG(vdd, " == HS48_V_TYPE_COMP == \n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_48HS; i++) {
		LCD_DEBUG(vdd, "- COMP[%d]\n", i);
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			for (v = 0; v < GAMMA_V_SIZE; v++) {
				snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS48_V_TYPE_COMP[i][j][v]);
			}
			LCD_DEBUG(vdd, "SET[%d] : %s\n", j, pBuffer);
			memset(pBuffer, 0x00, 256);
		}
	}

	/* debug print */
	LCD_DEBUG(vdd, " == HS60_V_TYPE_COMP == \n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_60HS; i++) {
		LCD_DEBUG(vdd, "- COMP[%d]\n", i);
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			for (v = 0; v < GAMMA_V_SIZE; v++) {
				snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS60_V_TYPE_COMP[i][j][v]);
			}
			LCD_DEBUG(vdd, "SET[%d] : %s\n", j, pBuffer);
			memset(pBuffer, 0x00, 256);
		}
	}

	LCD_DEBUG(vdd, " == HS96_V_TYPE_COMP == \n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		LCD_DEBUG(vdd, "- COMP[%d]\n", i);
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			for (v = 0; v < GAMMA_V_SIZE; v++) {
				snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS96_V_TYPE_COMP[i][j][v]);
			}
			LCD_DEBUG(vdd, "SET[%d] : %s\n", j, pBuffer);
			memset(pBuffer, 0x00, 256);
		}
	}

	LCD_DEBUG(vdd, " == HS48_R_TYPE_COMP == \n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_48HS; i++) {
		LCD_DEBUG(vdd, "- COMP[%d]\n", i);
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			for (r = 0; r < GAMMA_R_SIZE; r++) {
				snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS48_R_TYPE_COMP[i][j][r]);
			}
			LCD_DEBUG(vdd, "SET[%d] : %s\n", j, pBuffer);
			memset(pBuffer, 0x00, 256);
		}
	}

	/* debug print */
	LCD_DEBUG(vdd, " == HS60_R_TYPE_COMP == \n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_60HS; i++) {
		LCD_DEBUG(vdd, "- COMP[%d]\n", i);
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			for (r = 0; r < GAMMA_R_SIZE; r++) {
				snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS60_R_TYPE_COMP[i][j][r]);
			}
			LCD_DEBUG(vdd, "SET[%d] : %s\n", j, pBuffer);
			memset(pBuffer, 0x00, 256);
		}
	}

	LCD_DEBUG(vdd, " == HS96_R_TYPE_COMP == \n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		LCD_DEBUG(vdd, "- COMP[%d]\n", i);
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			for (r = 0; r < GAMMA_R_SIZE; r++) {
				snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS96_R_TYPE_COMP[i][j][r]);
			}
			LCD_DEBUG(vdd, "SET[%d] : %s\n", j, pBuffer);
			memset(pBuffer, 0x00, 256);
		}
	}

	return;
}

static int ss_gm2_gamma_comp_init(struct samsung_display_driver_data *vdd)
{
	u8 readbuf[GAMMA_R_SIZE];
	struct dsi_panel_cmd_set *rx_cmds;
	int i, j, r, v;
	int val;
	u8 *r_buf;
	int *v_buf;

	LCD_INFO(vdd, " ++\n");

	rx_cmds = ss_get_cmds(vdd, RX_SMART_DIM_MTP);
	if (SS_IS_CMDS_NULL(rx_cmds)) {
		LCD_ERR(vdd, "No cmds for RX_SMART_DIM_MTP.. \n");
		return -ENODEV;
	}

	/*****************************************/
	/* 1. read HS120/HS60 ORIGINAL MTP GAMMA */
	/*    NS60 MTP GAMMA is from flash       */
	/*****************************************/

	for (i = GAMMA_ROOM_120; i < GAMMA_ROOM_MAX; i++) {
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			/* pointer of 120/60 HS R type buf */
			r_buf = HS_R_TYPE_BUF[i][j];

			rx_cmds->read_startoffset = GAMMA_SET_ADDR_TABLE[i][j][0];
			rx_cmds->cmds->ss_txbuf[0] = GAMMA_SET_ADDR_TABLE[i][j][1];
			rx_cmds->cmds->msg.rx_len = GAMMA_R_SIZE;

			ss_panel_data_read(vdd, RX_SMART_DIM_MTP, readbuf, LEVEL1_KEY);
			memcpy(r_buf, readbuf, GAMMA_R_SIZE);

			/* 120 SET0 and SET6 have last 4byte from different address */
			if (i == GAMMA_ROOM_120 && j == GAMMA_SET_0) {
				rx_cmds->cmds->ss_txbuf[0] = 0x9A;
				rx_cmds->cmds->msg.rx_len = 4;
				rx_cmds->read_startoffset = 0x00;

				ss_panel_data_read(vdd, RX_SMART_DIM_MTP, readbuf, LEVEL1_KEY);
				memcpy(&r_buf[GAMMA_R_SIZE - 4], readbuf, 4);
			} else if (i == GAMMA_ROOM_120 && j == GAMMA_SET_6) {
				rx_cmds->cmds->ss_txbuf[0] = 0xC9;
				rx_cmds->cmds->msg.rx_len = 4;
				rx_cmds->read_startoffset = 0x00;

				ss_panel_data_read(vdd, RX_SMART_DIM_MTP, readbuf, LEVEL1_KEY);
				memcpy(&r_buf[GAMMA_R_SIZE - 4], readbuf, 4);
			}
		}
	}

	/******************************************************/
	/* 2. translate Register type to V(R/G/B) type */
	/******************************************************/
	for (i = GAMMA_ROOM_120; i < GAMMA_ROOM_MAX; i++) {
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			/* pointer of 120/60 HS R/V type buf */
			r_buf = HS_R_TYPE_BUF[i][j];
			v_buf = HS_V_TYPE_BUF[i][j];

			v = 0; // V index (33bytes)

			for (r = 0; r < GAMMA_R_SIZE; ) {
				if (r == 0) { /* 11th - R,G,B */
					v_buf[v++] = r_buf[r++];
					v_buf[v++] = r_buf[r++];
					v_buf[v++] = r_buf[r++];
				} else { /* 1st ~ 10th - R,G,B */
					v_buf[v++] = (GET_BITS(r_buf[r], 0, 5) << 4)
												 | (GET_BITS(r_buf[r+1], 4, 7));
					v_buf[v++] = (GET_BITS(r_buf[r+1], 0, 3) << 6)
												 | (GET_BITS(r_buf[r+2], 2, 7));
					v_buf[v++] = (GET_BITS(r_buf[r+2], 0, 1) << 8)
												 | (GET_BITS(r_buf[r+3], 0, 7));
					r += 4;
				}
			}
		}
	}

	/******************************************************/
	/* Before making comp value, check gamma max first..  */
	/******************************************************/
	for (i = GAMMA_ROOM_120; i < GAMMA_ROOM_MAX; i++) {
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			v_buf = HS_V_TYPE_BUF[i][j];

			/* V1 ~ V255 */
			for (v = 3; v < GAMMA_V_SIZE; v++) {
				if (v_buf[v] == 0x3FF) {
					gamma_max_check_res[i][j] = 1;
					LCD_ERR(vdd, "gamma check error! %dhz set[%d] vidx[%d] = 0x%x \n",
						i == GAMMA_ROOM_120 ? 120 : 60, j, v, v_buf[v]);
				}
			}
		}
	}

	/********************************************/
	/* 3. Apply GAMMA OFFSET (48HS, 60HS, 96HS) */
	/********************************************/

	/* 48HS COMP VALUE - from 120HS MTP GAMMA */
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_48HS; i++) {
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			v_buf = HS_V_TYPE_BUF[GAMMA_ROOM_120][j];
			for (v = 0; v < GAMMA_V_SIZE; v++) {
				/* check underflow & overflow */
				if (v_buf[v] + MTP_OFFSET_48HS_VAL[i][j][v] < 0) {
					HS48_V_TYPE_COMP[i][j][v] = 0;
				} else {
					if (v <= 2) {	/* 11th - 8bit(0xFF) */
						val = v_buf[v] + MTP_OFFSET_48HS_VAL[i][j][v];
						if (val > 0xFF)	/* check ovefflow */
							HS48_V_TYPE_COMP[i][j][v] = 0xFF;
						else
							HS48_V_TYPE_COMP[i][j][v] = val;
					} else {	/* 1~10th - 10bit(0x3FF) */
						val = v_buf[v] + MTP_OFFSET_48HS_VAL[i][j][v];
						if (val > 0x3FF)	/* check ovefflow */
							HS48_V_TYPE_COMP[i][j][v] = 0x3FF;
						else
							HS48_V_TYPE_COMP[i][j][v] = val;
					}
				}
			}
		}
	}

	/* 60HS COMP VALUE - from 120HS MTP GAMMA */
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_60HS; i++) {
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			v_buf = HS_V_TYPE_BUF[GAMMA_ROOM_120][j];
			for (v = 0; v < GAMMA_V_SIZE; v++) {
				/* check underflow & overflow */
				if (v_buf[v] + MTP_OFFSET_60HS_VAL[i][j][v] < 0) {
					HS60_V_TYPE_COMP[i][j][v] = 0;
				} else {
					if (v <= 2) {	/* 11th - 8bit(0xFF) */
						val = v_buf[v] + MTP_OFFSET_60HS_VAL[i][j][v];
						if (val > 0xFF)	/* check ovefflow */
							HS60_V_TYPE_COMP[i][j][v] = 0xFF;
						else
							HS60_V_TYPE_COMP[i][j][v] = val;
					} else {	/* 1~10th - 10bit(0x3FF) */
						val = v_buf[v] + MTP_OFFSET_60HS_VAL[i][j][v];
						if (val > 0x3FF)	/* check ovefflow */
							HS60_V_TYPE_COMP[i][j][v] = 0x3FF;
						else
							HS60_V_TYPE_COMP[i][j][v] = val;
					}
				}
			}
		}
	}

	/* 96HS COMP VALUE - from 120HS MTP GAMMA */
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			v_buf = HS_V_TYPE_BUF[GAMMA_ROOM_120][j];
			for (v = 0; v < GAMMA_V_SIZE; v++) {
				/* check underflow & overflow */
				if (v_buf[v] + MTP_OFFSET_96HS_VAL[i][j][v] < 0) {
					HS96_V_TYPE_COMP[i][j][v] = 0;
				} else {
					if (v <= 2) {	/* 11th - 8bit(0xFF) */
						val = v_buf[v] + MTP_OFFSET_96HS_VAL[i][j][v];
						if (val > 0xFF)	/* check ovefflow */
							HS96_V_TYPE_COMP[i][j][v] = 0xFF;
						else
							HS96_V_TYPE_COMP[i][j][v] = val;
					} else {	/* 1~10th - 10bit(0x3FF) */
						val = v_buf[v] + MTP_OFFSET_96HS_VAL[i][j][v];
						if (val > 0x3FF)	/* check ovefflow */
							HS96_V_TYPE_COMP[i][j][v] = 0x3FF;
						else
							HS96_V_TYPE_COMP[i][j][v] = val;
					}
				}
			}
		}
	}

	/******************************************************************/
	/* 4. translate V(R/G/B) type to Register type (48HS, 60HS, 96HS) */
	/******************************************************************/

	/* 48HS */
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_48HS; i++) {
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			r = 0; // GAMMA SET size
			for (v = 0; v < GAMMA_V_SIZE; v += RGB_MAX) {
				if (v == 0) {	/* 11th */
					HS48_R_TYPE_COMP[i][j][r++] = HS48_V_TYPE_COMP[i][j][v+R];
					HS48_R_TYPE_COMP[i][j][r++] = HS48_V_TYPE_COMP[i][j][v+G];
					HS48_R_TYPE_COMP[i][j][r++] = HS48_V_TYPE_COMP[i][j][v+B];
				} else {	/* 1st ~ 10th */

					HS48_R_TYPE_COMP[i][j][r++] = GET_BITS(HS48_V_TYPE_COMP[i][j][v+R], 4, 9);
					HS48_R_TYPE_COMP[i][j][r++] = (GET_BITS(HS48_V_TYPE_COMP[i][j][v+R], 0, 3) << 4)
												| (GET_BITS(HS48_V_TYPE_COMP[i][j][v+G], 6, 9));
					HS48_R_TYPE_COMP[i][j][r++]	= (GET_BITS(HS48_V_TYPE_COMP[i][j][v+G], 0, 5) << 2)
												| (GET_BITS(HS48_V_TYPE_COMP[i][j][v+B], 8, 9));
					HS48_R_TYPE_COMP[i][j][r++]	= GET_BITS(HS48_V_TYPE_COMP[i][j][v+B], 0, 7);
				}
			}
		}
	}

	/* 60HS */
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_60HS; i++) {
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			r = 0; // GAMMA SET size
			for (v = 0; v < GAMMA_V_SIZE; v += RGB_MAX) {
				if (v == 0) {	/* 11th */
					HS60_R_TYPE_COMP[i][j][r++] = HS60_V_TYPE_COMP[i][j][v+R];
					HS60_R_TYPE_COMP[i][j][r++] = HS60_V_TYPE_COMP[i][j][v+G];
					HS60_R_TYPE_COMP[i][j][r++] = HS60_V_TYPE_COMP[i][j][v+B];
				} else {	/* 1st ~ 10th */

					HS60_R_TYPE_COMP[i][j][r++] = GET_BITS(HS60_V_TYPE_COMP[i][j][v+R], 4, 9);
					HS60_R_TYPE_COMP[i][j][r++] = (GET_BITS(HS60_V_TYPE_COMP[i][j][v+R], 0, 3) << 4)
												| (GET_BITS(HS60_V_TYPE_COMP[i][j][v+G], 6, 9));
					HS60_R_TYPE_COMP[i][j][r++] = (GET_BITS(HS60_V_TYPE_COMP[i][j][v+G], 0, 5) << 2)
												| (GET_BITS(HS60_V_TYPE_COMP[i][j][v+B], 8, 9));
					HS60_R_TYPE_COMP[i][j][r++] = GET_BITS(HS60_V_TYPE_COMP[i][j][v+B], 0, 7);
				}
			}
		}
	}

	/* 96HS */
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		for (j = GAMMA_SET_0; j < GAMMA_SET_MAX; j++) {
			r = 0; // GAMMA SET size
			for (v = 0; v < GAMMA_V_SIZE; v += RGB_MAX) {
				if (v == 0) {	/* 11th */
					HS96_R_TYPE_COMP[i][j][r++] = HS96_V_TYPE_COMP[i][j][v+R];
					HS96_R_TYPE_COMP[i][j][r++] = HS96_V_TYPE_COMP[i][j][v+G];
					HS96_R_TYPE_COMP[i][j][r++] = HS96_V_TYPE_COMP[i][j][v+B];
				} else {	/* 1st ~ 10th */

					HS96_R_TYPE_COMP[i][j][r++] = GET_BITS(HS96_V_TYPE_COMP[i][j][v+R], 4, 9);
					HS96_R_TYPE_COMP[i][j][r++] = (GET_BITS(HS96_V_TYPE_COMP[i][j][v+R], 0, 3) << 4)
												| (GET_BITS(HS96_V_TYPE_COMP[i][j][v+G], 6, 9));
					HS96_R_TYPE_COMP[i][j][r++]	= (GET_BITS(HS96_V_TYPE_COMP[i][j][v+G], 0, 5) << 2)
												| (GET_BITS(HS96_V_TYPE_COMP[i][j][v+B], 8, 9));
					HS96_R_TYPE_COMP[i][j][r++]	= GET_BITS(HS96_V_TYPE_COMP[i][j][v+B], 0, 7);
				}
			}
		}
	}

	/* 5. write adjusted GAMMA values at ss_brightness_gm2_gamma_comp() */

	/* print all results */
	ss_print_gamma_comp(vdd);

	LCD_INFO(vdd, " --\n");

	return 0;
}

#if 0
static bool IS_120HS_GAMMA_CHANGED;

struct dsi_panel_cmd_set *ss_brightness_gm2_gamma_comp(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds = NULL;
	int idx = -1;

	if (vdd->vrr.cur_refresh_rate == 60 && vdd->vrr.cur_sot_hs_mode && !vdd->vrr.cur_phs_mode) {
		pcmds = ss_get_cmds(vdd, TX_VRR_GM2_GAMMA_COMP);
		if (SS_IS_CMDS_NULL(pcmds)) {
			LCD_ERR(vdd, "No cmds for TX_VRR_GM2_GAMMA_COMP.. \n");
			return NULL;
		}

		idx = mtp_offset_idx_table_60hs[vdd->br_info.common_br.cd_idx];

		/* Now only SET1 exist. */
		if (idx >= 0) {
			LCD_ERR(vdd, "COMP 60HS[%d] - %d\n", idx, vdd->br_info.common_br.cd_idx);
			memcpy(&pcmds->cmds[1].ss_txbuf[1], HS60_R_TYPE_COMP[idx][GAMMA_SET_1], GAMMA_R_SIZE);
		} else {
			/* restore original register */
			LCD_ERR(vdd, "60HS original restore..\n");
			memcpy(&pcmds->cmds[1].ss_txbuf[1], HS60_R_TYPE_BUF[GAMMA_SET_1], GAMMA_R_SIZE);
		}
	} else if (vdd->vrr.cur_refresh_rate == 96 ||
				(vdd->vrr.cur_refresh_rate == 48 && vdd->vrr.cur_phs_mode)) {
		pcmds = ss_get_cmds(vdd, TX_VRR_GM2_GAMMA_COMP2);
		if (SS_IS_CMDS_NULL(pcmds)) {
			LCD_ERR(vdd, "No cmds for TX_VRR_GM2_GAMMA_COMP2.. \n");
			return NULL;
		}

		idx = mtp_offset_idx_table_96hs[vdd->br_info.common_br.cd_idx];

		if (idx >= 0) {
			LCD_ERR(vdd, "COMP 96HS[%d] - %d\n", idx, vdd->br_info.common_br.cd_idx);

			/* copy HS96 R type comp data to 120HS addr : C9h (0x00 ~ 0xDA) and C7h (0xAC ~ 0xFD) */
			/* C9h (0x00 ~ 0x59) -  */
			memcpy(&pcmds->cmds[0].ss_txbuf[1], &HS96_R_TYPE_COMP[idx][GAMMA_SET_6][GAMMA_R_SIZE-4], 4);
			memcpy(&pcmds->cmds[0].ss_txbuf[1+4], HS96_R_TYPE_COMP[idx][GAMMA_SET_5], GAMMA_R_SIZE);

			memcpy(&pcmds->cmds[0].ss_txbuf[1+4+(GAMMA_R_SIZE*1)], HS96_R_TYPE_COMP[idx][GAMMA_SET_4], GAMMA_R_SIZE);

			/* C9h (0x5A ~ 0xDA) */
			memcpy(&pcmds->cmds[2].ss_txbuf[1], HS96_R_TYPE_COMP[idx][GAMMA_SET_3], GAMMA_R_SIZE);
			memcpy(&pcmds->cmds[2].ss_txbuf[1+(GAMMA_R_SIZE*1)], HS96_R_TYPE_COMP[idx][GAMMA_SET_2], GAMMA_R_SIZE);
			memcpy(&pcmds->cmds[2].ss_txbuf[1+(GAMMA_R_SIZE*2)], HS96_R_TYPE_COMP[idx][GAMMA_SET_1], GAMMA_R_SIZE);

			/* C7h (0xAC ~ 0xFD) */
			memcpy(&pcmds->cmds[4].ss_txbuf[1], HS96_R_TYPE_COMP[idx][GAMMA_SET_7], GAMMA_R_SIZE);
			memcpy(&pcmds->cmds[4].ss_txbuf[1+GAMMA_R_SIZE], HS96_R_TYPE_COMP[idx][GAMMA_SET_6], GAMMA_R_SIZE-4);
			IS_120HS_GAMMA_CHANGED = true;
		} else {
			/* restore original register */
			LCD_ERR(vdd, "ERROR!! check idx[%d] - %d\n", idx, vdd->br_info.common_br.cd_idx);
		}
	} else if (vdd->vrr.cur_refresh_rate == 48 && !vdd->vrr.cur_phs_mode) {
		pcmds = ss_get_cmds(vdd, TX_VRR_GM2_GAMMA_COMP);
		if (SS_IS_CMDS_NULL(pcmds)) {
			LCD_ERR(vdd, "No cmds for TX_VRR_GM2_GAMMA_COMP.. \n");
			return NULL;
		}

		idx = mtp_offset_idx_table_48hs[vdd->br_info.common_br.cd_idx];

		/* Now only SET1 exist. */
		if (idx >= 0) {
			LCD_ERR(vdd, "COMP 48HS[%d] - %d\n", idx, vdd->br_info.common_br.cd_idx);
			memcpy(&pcmds->cmds[1].ss_txbuf[1], HS48_R_TYPE_COMP[idx][GAMMA_SET_1], GAMMA_R_SIZE);
			IS_120HS_GAMMA_CHANGED = true;
		} else {
			/* restore original register */
			LCD_ERR(vdd, "48HS(120hs) original restore..\n");
			memcpy(&pcmds->cmds[1].ss_txbuf[1], HS120_R_TYPE_BUF[GAMMA_SET_1], GAMMA_R_SIZE);
			IS_120HS_GAMMA_CHANGED = false;
		}
	} else if (vdd->vrr.cur_refresh_rate == 120 ||
			(vdd->vrr.cur_refresh_rate == 60 && vdd->vrr.cur_phs_mode)) {
		/* 120HS gamma is changed. need to resotore original 120HS gamma in case 120hz mode. */
		if (IS_120HS_GAMMA_CHANGED) {
			pcmds = ss_get_cmds(vdd, TX_VRR_GM2_GAMMA_COMP2);
			if (SS_IS_CMDS_NULL(pcmds)) {
				LCD_ERR(vdd, "No cmds for TX_VRR_GM2_GAMMA_COMP2.. \n");
				return NULL;
			}

			LCD_ERR(vdd, "120HS original restore.. \n");
			/* C9h (0x00 ~ 0x59) -  */
			memcpy(&pcmds->cmds[0].ss_txbuf[1], &HS120_R_TYPE_BUF[GAMMA_SET_6][GAMMA_R_SIZE-4], 4);
			memcpy(&pcmds->cmds[0].ss_txbuf[1+4], HS120_R_TYPE_BUF[GAMMA_SET_5], GAMMA_R_SIZE);
			memcpy(&pcmds->cmds[0].ss_txbuf[1+4+(GAMMA_R_SIZE*1)], HS120_R_TYPE_BUF[GAMMA_SET_4], GAMMA_R_SIZE);

			/* C9h (0x5A ~ 0xDA) */
			memcpy(&pcmds->cmds[2].ss_txbuf[1], HS120_R_TYPE_BUF[GAMMA_SET_3], GAMMA_R_SIZE);
			memcpy(&pcmds->cmds[2].ss_txbuf[1+(GAMMA_R_SIZE*1)], HS120_R_TYPE_BUF[GAMMA_SET_2], GAMMA_R_SIZE);
			memcpy(&pcmds->cmds[2].ss_txbuf[1+(GAMMA_R_SIZE*2)], HS120_R_TYPE_BUF[GAMMA_SET_1], GAMMA_R_SIZE);

			/* C7h (0xAC ~ 0xFD) */
			memcpy(&pcmds->cmds[4].ss_txbuf[1], HS120_R_TYPE_BUF[GAMMA_SET_7], GAMMA_R_SIZE);
			memcpy(&pcmds->cmds[4].ss_txbuf[1+GAMMA_R_SIZE], HS120_R_TYPE_BUF[GAMMA_SET_6], GAMMA_R_SIZE-4);
			IS_120HS_GAMMA_CHANGED = false;
		}
	} else {
		return NULL;
	}

	return pcmds;
};
#else

static u8 GLOABL_PARA_1[3] = {0xB0, 0x00, 0x00};
static u8 GLOABL_PARA_2[3] = {0xB0, 0x00, 0x00};
static u8 GAMMA_1[1+(GAMMA_R_SIZE*2)] = {0x00, };
static u8 GAMMA_2[1+(GAMMA_R_SIZE*2)] = {0x00, };

#if IS_ENABLED(CONFIG_ARCH_WAIPIO) /* From SM8450(WAIPIO) */
/* only one register comp */
static struct dsi_cmd_desc GAMMA_SET_REGION_TYPE1[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(GLOABL_PARA_1), GLOABL_PARA_1, 0, NULL}, false, 0, 0, 0, GLOABL_PARA_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(GAMMA_1), GAMMA_1, 0, NULL}, false, 0, 0, 0, GAMMA_1},
};

/* two register comp */
static struct dsi_cmd_desc GAMMA_SET_REGION_TYPE2[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(GLOABL_PARA_1), GLOABL_PARA_1, 0, NULL}, false, 0, 0, 0, GLOABL_PARA_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(GAMMA_1), GAMMA_1, 0, NULL}, false, 0, 0, 0, GAMMA_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(GLOABL_PARA_2), GLOABL_PARA_2, 0, NULL}, false, 0, 0, 0, GLOABL_PARA_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(GAMMA_2), GAMMA_2, 0, NULL}, false, 0, 0, 0, GAMMA_2},
};
#else
/* only one register comp */
static struct dsi_cmd_desc GAMMA_SET_REGION_TYPE1[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(GLOABL_PARA_1), GLOABL_PARA_1, 0, NULL}, false, 0, GLOABL_PARA_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(GAMMA_1), GAMMA_1, 0, NULL}, false, 0, GAMMA_1},
};

/* two register comp */
static struct dsi_cmd_desc GAMMA_SET_REGION_TYPE2[] = {
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(GLOABL_PARA_1), GLOABL_PARA_1, 0, NULL}, false, 0, GLOABL_PARA_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(GAMMA_1), GAMMA_1, 0, NULL}, false, 0, GAMMA_1},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(GLOABL_PARA_2), GLOABL_PARA_2, 0, NULL}, false, 0, GLOABL_PARA_2},
	{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, sizeof(GAMMA_2), GAMMA_2, 0, NULL}, false, 0, GAMMA_2},
};
#endif

static struct dsi_panel_cmd_set *ss_brightness_gm2_gamma_comp(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	struct dsi_cmd_desc *cmd_desc;
	int cmd_count;

	struct vrr_info *vrr = &vdd->vrr;
	int level = vdd->br_info.common_br.bl_level;
	enum GAMMA_ROOM gamma_room = GAMMA_ROOM_120;
	int cur_rr;
	bool cur_hs, cur_phs;
	int set1, set2;
	int idx = 0;
	u8 *set1_data, *set2_data;

#if IS_ENABLED(CONFIG_ARCH_WAIPIO)
	// Temporary Block for SM8450
	return NULL;
#endif

	if (vdd->panel_revision < 3) {	/* panel rev.A ~ rev.C */
		vrr->cur_sot_hs_mode = true;
		LCD_ERR(vdd, "Do not support 60NS (rev %d), set HS\n", vdd->panel_revision);
	}

	cur_rr = vrr->cur_refresh_rate;
	cur_hs = vrr->cur_sot_hs_mode;
	cur_phs = vrr->cur_phs_mode;

	/* 1. Get gamma_room to be applied
	 * 120hs room : 120hs 60phs 96hs 48phs
	 * 60hs room : 60hs 60nm 48hs
	 */
	if (cur_rr == 120 || (cur_rr == 60 && cur_phs) ||
		cur_rr == 96 || (cur_rr == 48 && cur_phs))
		gamma_room = GAMMA_ROOM_120;
	else if ((cur_rr == 60 && cur_hs && !cur_phs) ||
		(cur_rr == 60 && !cur_hs) ||
		(cur_rr == 48 && cur_hs && !cur_phs))
		gamma_room = GAMMA_ROOM_60;
	else {
		LCD_ERR(vdd, "fail to get proper gamma_room\n");
		return NULL;
	}

	/* 2. Get gamma_set region to be applied */
	set1 = GAMMA_SET_REGION_TABLE[level][0];
	set2 = GAMMA_SET_REGION_TABLE[level][1];

	/* 3. Make gamma cmd */

	LCD_INFO(vdd, "COMP romm[%d] - set[%d %d] level [%d], cur_rr=%d, cur_hs=%d, cur_phs=%d\n",
		gamma_room == GAMMA_ROOM_120 ? 120 : 60, set1, set2, level, cur_rr, cur_hs, cur_phs);

	if (cur_rr == 120 || (cur_rr == 60 && cur_phs)) {
		set1_data = HS_R_TYPE_BUF[gamma_room][set1];
		set2_data = HS_R_TYPE_BUF[gamma_room][set2];
		LCD_DEBUG(vdd, "ORIGINAL 120HS gamma\n");
	} else if (cur_rr == 96 || (cur_rr == 48 && cur_phs)) {
		idx = mtp_offset_idx_table_96hs[level];
		set1_data = HS96_R_TYPE_COMP[idx][set1];
		set2_data = HS96_R_TYPE_COMP[idx][set2];
		LCD_DEBUG(vdd, "COMP 96HS gamma [%d]\n", idx);
	} else if (cur_rr == 60 && cur_hs && !cur_phs) {
		idx = mtp_offset_idx_table_60hs[level];
		if (idx < 0)
			goto unexpected_combination;

		set1_data = HS60_R_TYPE_COMP[idx][set1];
		set2_data = HS60_R_TYPE_COMP[idx][set2];
		LCD_DEBUG(vdd, "COMP 60HS gamma [%d]\n", idx);
	} else if (cur_rr == 48 && cur_hs && !cur_phs) {
		idx = mtp_offset_idx_table_48hs[level];
		if (idx < 0)
			goto unexpected_combination;

		set1_data = HS48_R_TYPE_COMP[idx][set1];
		set2_data = HS48_R_TYPE_COMP[idx][set2];
		LCD_DEBUG(vdd, "COMP 48HS gamma [%d]\n", idx);
	} else {	/* 60NS */
		set1_data = NS60_R_TYPE_BUF[set1];
		set2_data = NS60_R_TYPE_BUF[set2];
		LCD_DEBUG(vdd, "ORIGINAL 60NS gamma\n");
	}

	/* 120HS (60PHS) original */
	/* 96HS 48PHS offset */
	if (gamma_room == GAMMA_ROOM_120) {
		if (set2 == GAMMA_SET_6) {
			/* 2 register : SET5 + SET6 case */

			/* C7h : 0xD7 ~ 0xFD */
			memcpy(&GLOABL_PARA_1[1], GAMMA_SET_ADDR_TABLE[gamma_room][set2], 2);

			GAMMA_1[0] = GAMMA_SET_ADDR_TABLE[gamma_room][set2][1];
			memcpy(&GAMMA_1[1], set2_data, GAMMA_R_SIZE - 4);
			GAMMA_SET_REGION_TYPE2[1].msg.tx_len = 1 + GAMMA_R_SIZE - 4;

			/* C9h : 0x00 ~ 0x2E */
			memcpy(&GLOABL_PARA_2[1], GAMMA_SET_ADDR_TABLE[gamma_room][set1], 2);
			GLOABL_PARA_2[1] = 0x00;

			GAMMA_2[0] = GAMMA_SET_ADDR_TABLE[gamma_room][set1][1];
			memcpy(&GAMMA_2[1], &set2_data[GAMMA_R_SIZE - 4], 4);
			memcpy(&GAMMA_2[1 + 4], set1_data, GAMMA_R_SIZE);
			GAMMA_SET_REGION_TYPE2[3].msg.tx_len = 1 + GAMMA_R_SIZE + 4;

			cmd_desc = GAMMA_SET_REGION_TYPE2;
			cmd_count = ARRAY_SIZE(GAMMA_SET_REGION_TYPE2);
		} else if (set1 == GAMMA_SET_6) {
			/* 2 register : SET6 + SET7 case */

			/* C7h : 0xAC ~ 0xFD */
			memcpy(&GLOABL_PARA_1[1], GAMMA_SET_ADDR_TABLE[gamma_room][set2], 2);

			GAMMA_1[0] = GAMMA_SET_ADDR_TABLE[gamma_room][set2][1];
			memcpy(&GAMMA_1[1], set2_data, GAMMA_R_SIZE);
			memcpy(&GAMMA_1[1 + GAMMA_R_SIZE], set1_data, GAMMA_R_SIZE - 4);
			GAMMA_SET_REGION_TYPE2[1].msg.tx_len = 1 + GAMMA_R_SIZE + (GAMMA_R_SIZE - 4);

			/* C9h : 0x00 ~ 0x04 */
			memcpy(&GLOABL_PARA_2[1], GAMMA_SET_ADDR_TABLE[gamma_room][set1 - 1], 2);
			GLOABL_PARA_2[1] = 0x00;

			GAMMA_2[0] = GAMMA_SET_ADDR_TABLE[gamma_room][set1 - 1][1];
			memcpy(&GAMMA_2[1], &set1_data[GAMMA_R_SIZE - 4], 4);
			GAMMA_SET_REGION_TYPE2[3].msg.tx_len = 1 + 4;

			cmd_desc = GAMMA_SET_REGION_TYPE2;
			cmd_count = ARRAY_SIZE(GAMMA_SET_REGION_TYPE2);
		} else if (set1 == GAMMA_SET_0) {
			/* 2 register : SET0 + SET1 case */

			/* C9h : 0xB0 ~ 0x101 */
			memcpy(&GLOABL_PARA_1[1], GAMMA_SET_ADDR_TABLE[gamma_room][set2], 2);

			GAMMA_1[0] = GAMMA_SET_ADDR_TABLE[gamma_room][set2][1];
			memcpy(&GAMMA_1[1], set2_data, GAMMA_R_SIZE);
			memcpy(&GAMMA_1[1 + GAMMA_R_SIZE], set1_data, GAMMA_R_SIZE - 4);
			GAMMA_SET_REGION_TYPE2[1].msg.tx_len = 1 + GAMMA_R_SIZE + (GAMMA_R_SIZE - 4);

			/* 9Ah : 0x00 ~ 0x04 */
			GLOABL_PARA_2[1] = 0x00;
			GLOABL_PARA_2[2] = 0x9A;
			GAMMA_2[0] = 0x9A;

			memcpy(&GAMMA_2[1], &set1_data[GAMMA_R_SIZE - 4], 4);
			GAMMA_SET_REGION_TYPE2[3].msg.tx_len = 1 + 4;

			cmd_desc = GAMMA_SET_REGION_TYPE2;
			cmd_count = ARRAY_SIZE(GAMMA_SET_REGION_TYPE2);
		} else {
			/* only 1 register comp case */
			memcpy(&GLOABL_PARA_1[1], GAMMA_SET_ADDR_TABLE[gamma_room][set2], 2);
			GAMMA_1[0] = GAMMA_SET_ADDR_TABLE[gamma_room][set2][1];
			memcpy(&GAMMA_1[1], set2_data, GAMMA_R_SIZE);
			memcpy(&GAMMA_1[1 + GAMMA_R_SIZE], set1_data, GAMMA_R_SIZE);
			GAMMA_SET_REGION_TYPE1[1].msg.tx_len = 1 + GAMMA_R_SIZE + GAMMA_R_SIZE;

			cmd_desc = GAMMA_SET_REGION_TYPE1;
			cmd_count = ARRAY_SIZE(GAMMA_SET_REGION_TYPE1);
		}
	}

	/* 60NS original */
	/* 60HS 48HS offset */
	if (gamma_room == GAMMA_ROOM_60) {
		if (cur_rr == 60 && !cur_hs) {
			/* restore 60NS gamma */
			memcpy(&GLOABL_PARA_1[1], GAMMA_SET_ADDR_TABLE[gamma_room][set2], 2);
			GAMMA_1[0] = GAMMA_SET_ADDR_TABLE[gamma_room][set2][1];
			memcpy(&GAMMA_1[1], set2_data, GAMMA_R_SIZE);
			GAMMA_SET_REGION_TYPE2[1].msg.tx_len = 1 + GAMMA_R_SIZE;

			memcpy(&GLOABL_PARA_2[1], GAMMA_SET_ADDR_TABLE[gamma_room][set1], 2);
			GAMMA_2[0] = GAMMA_SET_ADDR_TABLE[gamma_room][set1][1];
			memcpy(&GAMMA_2[1], set1_data, GAMMA_R_SIZE);
			GAMMA_SET_REGION_TYPE2[3].msg.tx_len = 1 + GAMMA_R_SIZE;

			cmd_desc = GAMMA_SET_REGION_TYPE2;
			cmd_count = ARRAY_SIZE(GAMMA_SET_REGION_TYPE2);
		} else {
			/* 48HS 60HS - only comp GAMMA_SET_0, GAMMA_SET_1(set2) */
			memcpy(&GLOABL_PARA_1[1], GAMMA_SET_ADDR_TABLE[gamma_room][set2], 2);
			GAMMA_1[0] = GAMMA_SET_ADDR_TABLE[gamma_room][set2][1];
			memcpy(&GAMMA_1[1], set2_data, GAMMA_R_SIZE);
			memcpy(&GAMMA_1[1 + GAMMA_R_SIZE], set1_data, GAMMA_R_SIZE);
			GAMMA_SET_REGION_TYPE1[1].msg.tx_len = 1 + GAMMA_R_SIZE + GAMMA_R_SIZE;

			cmd_desc = GAMMA_SET_REGION_TYPE1;
			cmd_count = ARRAY_SIZE(GAMMA_SET_REGION_TYPE1);
		}
	}

	pcmds = ss_get_cmds(vdd, TX_GM2_GAMMA_COMP);
	pcmds->cmds = cmd_desc;
	pcmds->count = cmd_count;
	pcmds->state = DSI_CMD_SET_STATE_HS;

	return pcmds;
unexpected_combination:
	LCD_ERR(vdd, "Unexpected Combination. cur_rr=%d, cur_hs=%d, cur_phs=%d, level=%d\n",
				cur_rr, cur_hs, cur_phs, level);
	return NULL;
}
#endif

static void make_brightness_packet(struct samsung_display_driver_data *vdd,
	struct dsi_cmd_desc *packet, int *cmd_cnt, enum BR_TYPE br_type)
{
	if (br_type == BR_TYPE_NORMAL) {
		/* gamma compensation for gamma mode2 VRR modes */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA_COMP, packet, cmd_cnt);

		/* VRR */
		ss_add_brightness_packet(vdd, BR_FUNC_VRR, packet, cmd_cnt);

		/* ACL */
		if (vdd->br_info.acl_status || vdd->siop_status)
			ss_add_brightness_packet(vdd, BR_FUNC_ACL_ON, packet, cmd_cnt);
		else
			ss_add_brightness_packet(vdd, BR_FUNC_ACL_OFF, packet, cmd_cnt);

		/* vint */
		ss_add_brightness_packet(vdd, BR_FUNC_VINT, packet, cmd_cnt);

		/* mAFPC */
		if (vdd->mafpc.is_support)
			ss_add_brightness_packet(vdd, BR_FUNC_MAFPC_SCALE, packet, cmd_cnt);

		/* gamma */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA, packet, cmd_cnt);
	} else if (br_type == BR_TYPE_HBM) {
		/* gamma compensation for gamma mode2 VRR modes */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA_COMP, packet, cmd_cnt);

		/* VRR */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_VRR, packet, cmd_cnt);

		/* acl */
		if (vdd->br_info.acl_status || vdd->siop_status) {
			ss_add_brightness_packet(vdd, BR_FUNC_HBM_ACL_ON, packet, cmd_cnt);
		} else {
			ss_add_brightness_packet(vdd, BR_FUNC_HBM_ACL_OFF, packet, cmd_cnt);
		}

		/* vint */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_VINT, packet, cmd_cnt);

		/* mAFPC */
		if (vdd->mafpc.is_support)
			ss_add_brightness_packet(vdd, BR_FUNC_MAFPC_SCALE, packet, cmd_cnt);

		/* Gamma */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_GAMMA, packet, cmd_cnt);
	} else if (br_type == BR_TYPE_HMT) {
		ss_add_brightness_packet(vdd, BR_FUNC_HMT_GAMMA, packet, cmd_cnt);
	} else {
		LCD_ERR(vdd, "undefined br_type (%d) \n", br_type);
	}

	return;
}

void S6E3FAB_AMB624XT01_FHD_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "S6E3FAB_AMB624XT01 : ++ \n");
	LCD_INFO(vdd, "%s\n", ss_get_panel_name(vdd));

	/* Default Panel Power Status is OFF */
	vdd->panel_state = PANEL_PWR_OFF;

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = samsung_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = samsung_panel_on_post;
	vdd->panel_func.samsung_panel_off_pre = samsung_panel_off_pre;
	vdd->panel_func.samsung_panel_off_post = samsung_panel_off_post;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = ss_panel_revision;
	vdd->panel_func.samsung_module_info_read = ss_module_info_read;
	vdd->panel_func.samsung_ddi_id_read = ss_ddi_id_read;
	vdd->panel_func.samsung_octa_id_read = ss_octa_id_read;
	vdd->panel_func.samsung_elvss_read = ss_elvss_read;

	/* Make brightness packer */
	vdd->panel_func.make_brightness_packet = make_brightness_packet;

	/* Brightness */
	vdd->panel_func.br_func[BR_FUNC_GAMMA] = ss_brightness_gamma_mode2_normal;
	vdd->panel_func.br_func[BR_FUNC_ACL_ON] = ss_acl_on;
	vdd->panel_func.br_func[BR_FUNC_ACL_OFF] = ss_acl_off;
	vdd->panel_func.br_func[BR_FUNC_VINT] = ss_vint;
	vdd->panel_func.br_func[BR_FUNC_VRR] = ss_vrr;
	vdd->panel_func.br_func[BR_FUNC_GAMMA_COMP] = ss_brightness_gm2_gamma_comp;

	/* Total level key in brightness */
	vdd->panel_func.samsung_brightness_tot_level = ss_brightness_tot_level;

	/* HBM */
	vdd->panel_func.br_func[BR_FUNC_HBM_GAMMA] = ss_brightness_gamma_mode2_hbm;
	vdd->panel_func.br_func[BR_FUNC_HBM_VRR] = ss_vrr;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_ON] = ss_acl_on;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_OFF] = ss_acl_off;
	vdd->panel_func.br_func[BR_FUNC_HBM_VINT] = ss_vint;

	/* HMT */
	vdd->panel_func.br_func[BR_FUNC_HMT_GAMMA] = ss_brightness_gamma_mode2_hmt;
//	vdd->panel_func.br_func[BR_FUNC_HMT_VRR] = ss_vrr_hmt;

	/* Panel LPM */
	vdd->panel_func.samsung_update_lpm_ctrl_cmd = ss_update_panel_lpm_ctrl_cmd;
	vdd->panel_func.samsung_set_lpm_brightness = ss_set_panel_lpm_brightness;

	/* Gray Spot Test */
	vdd->panel_func.samsung_gray_spot = ss_gray_spot;

	/* default brightness */
	vdd->br_info.common_br.bl_level = 255;

	/* mdnie */
	vdd->mdnie.support_mdnie = true;
	vdd->mdnie.support_trans_dimming = true;
	vdd->mdnie.mdnie_tune_size[0] = sizeof(DSI0_BYPASS_MDNIE_1);
	vdd->mdnie.mdnie_tune_size[1] = sizeof(DSI0_BYPASS_MDNIE_2);
	vdd->mdnie.mdnie_tune_size[2] = sizeof(DSI0_BYPASS_MDNIE_3);

	dsi_update_mdnie_data(vdd);

	/* Enable panic on first pingpong timeout */
	//vdd->debug_data->panic_on_pptimeout = true;

	/* COLOR WEAKNESS */
	vdd->panel_func.color_weakness_ccb_on_off =  NULL;

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = NULL;

	/* COPR */
	vdd->copr.panel_init = ss_copr_panel_init;

	/* ACL default ON */
	vdd->br_info.acl_status = 1;

	/* Default br_info.temperature */
	vdd->br_info.temperature = 20;

	/* ACL default status in acl on */
	vdd->br_info.gradual_acl_val = 1;

	/* Gram Checksum Test */
	vdd->panel_func.samsung_gct_write = ss_gct_write;
	vdd->panel_func.samsung_gct_read = ss_gct_read;

	/* Self display */
	vdd->self_disp.is_support = true;
	vdd->self_disp.factory_support = true;
	vdd->self_disp.init = self_display_init_FAB;
	vdd->self_disp.data_init = ss_self_display_data_init;

	/* mAPFC */
	vdd->mafpc.init = ss_mafpc_init_FAB;
	vdd->mafpc.data_init = ss_mafpc_data_init;

	/* FFC */
	vdd->panel_func.set_ffc = ss_ffc;

	/* SAMSUNG_FINGERPRINT */
	//vdd->panel_hbm_entry_delay = 2;

	/* DDI flash read for GM2 */
	vdd->panel_func.samsung_gm2_ddi_flash_prepare = ss_gm2_ddi_flash_prepare;
	vdd->panel_func.samsung_test_ddi_flash_check = ss_test_ddi_flash_check;

	/* Gamma compensation (Gamma Offset) */
	vdd->panel_func.samsung_gm2_gamma_comp_init = ss_gm2_gamma_comp_init;

	/* VRR */
	ss_vrr_init(&vdd->vrr);

	vdd->panel_func.samsung_check_support_mode = ss_check_support_mode;

	vdd->panel_func.samsung_gamma_check = ss_gamma_max_check;

	vdd->panel_func.samsung_print_gamma_comp = ss_print_gamma_comp;
	LCD_INFO(vdd, "S6E3FAB_AMB624XT01 : -- \n");
}
