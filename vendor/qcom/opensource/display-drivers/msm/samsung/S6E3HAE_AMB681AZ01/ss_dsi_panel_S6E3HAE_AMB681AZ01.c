/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
 *
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2021, Samsung Electronics. All rights reserved.

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
#include "ss_dsi_panel_S6E3HAE_AMB681AZ01.h"
#include "ss_dsi_mdnie_S6E3HAE_AMB681AZ01.h"

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

static int samsung_display_on_post(struct samsung_display_driver_data *vdd)
{
	/* HAE ddi (B0) only issue */
	/* P220307-02604, P211207-05270 : Do not write AOD 60nit before AOD display on + 1vsync (34ms) */
	if (vdd->panel_lpm.need_br_update) {
		vdd->panel_lpm.need_br_update = false;
		ss_brightness_dcs(vdd, USE_CURRENT_BL_LEVEL, BACKLIGHT_NORMAL);
	}

	return 0;
}

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
	case 0x04:
		vdd->panel_revision = 'B';
		break;
	case 0x05:
		vdd->panel_revision = 'C';
		break;
	default:
		vdd->panel_revision = 'C';
		LCD_ERR(vdd, "Invalid panel_rev(default rev : %c)\n", vdd->panel_revision);
		break;
	}

	vdd->panel_revision -= 'A';
	LCD_INFO_ONCE(vdd, "panel_revision = %c %d \n", vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}


/*
 * VRR related cmds
 */
enum VRR_CMD_RR {
	/* 1Hz is PSR mode in LPM (AOD) mode, 10Hz is PSR mode in 120HS mode */
	VRR_10HS = 0,
	VRR_24HS,
	VRR_30HS,
	VRR_48NS,
	VRR_60NS,
	VRR_48HS,
	VRR_60HS,
	VRR_96HS,
	VRR_120HS,
	VRR_MAX
};

static enum VRR_CMD_RR ss_get_vrr_id(struct samsung_display_driver_data *vdd)
{
	enum VRR_CMD_RR vrr_id;
	int cur_rr = vdd->vrr.cur_refresh_rate;
	int cur_hs = vdd->vrr.cur_sot_hs_mode;

	switch (cur_rr) {
	case 10:
		vrr_id = VRR_10HS;
		break;
	case 24:
		vrr_id = VRR_24HS;
		break;
	case 30:
		vrr_id = VRR_30HS;
		break;
	case 48:
		vrr_id = (cur_hs) ? VRR_48HS : VRR_48NS ;
		break;
	case 60:
		vrr_id = (cur_hs) ? VRR_60HS : VRR_60NS ;
		break;
	case 96:
		vrr_id = VRR_96HS;
		break;
	case 120:
		vrr_id = VRR_120HS;
		break;
	default:
		LCD_ERR(vdd, "invalid refresh rate (%d, %d), set default 120HS..\n", cur_rr, cur_hs);
		vrr_id = VRR_120HS;
		break;
	}

	return vrr_id;
}

static int ss_update_base_lfd_val(struct vrr_info *vrr,
			enum LFD_SCOPE_ID scope, struct lfd_base_str *lfd_base)
{
	u32 base_rr, max_div_def, min_div_def, min_div_lowest;
	enum VRR_CMD_RR vrr_id;
	struct samsung_display_driver_data *vdd =
		container_of(vrr, struct samsung_display_driver_data, vrr);
	struct lfd_mngr *mngr;

	if (scope == LFD_SCOPE_LPM) {
		base_rr = 30;
		max_div_def = 1;	/* default 1HZ */
		min_div_def = 30;
		min_div_lowest = 30;
		goto done;
	}

	mngr = &vrr->lfd.lfd_mngr[LFD_CLIENT_DISP];

	vrr_id = ss_get_vrr_id(vdd);

	switch (vrr_id) {
	case VRR_10HS:
		base_rr = 120;
		max_div_def = 12; // 10hz
		min_div_def = min_div_lowest = 120; // 1hz
		break;
	case VRR_24HS:
		base_rr = 120;
		max_div_def = 5; // 24hz
		min_div_def = min_div_lowest = 120; // 1hz
		break;
	case VRR_30HS:
		base_rr = 120;
		max_div_def = 4; // 30hz
		min_div_def = min_div_lowest = 120; // 1hz
		break;
	case VRR_48NS:
		base_rr = 48;
		max_div_def = min_div_def = min_div_lowest = 1; // 48hz
		break;
	case VRR_48HS:
		base_rr = 96;
		max_div_def = 2; // 48hz
		min_div_def = 3; // 32hz
		min_div_lowest = 96; // 1hz
		break;
	case VRR_60NS:
		base_rr = 60;
		max_div_def = 1; // 60hz
		min_div_def = min_div_lowest = 2; // 30hz
		break;
	case VRR_60HS:
		base_rr = 120;
		max_div_def = 2; // 60hz
		min_div_def = 3; // 40hz
		min_div_lowest = 120; // 1hz
		break;
	case VRR_96HS:
		base_rr = 96;
		max_div_def = 1; // 96hz
		min_div_def = min_div_lowest = 96; // 1hz
		break;
	case VRR_120HS:
		base_rr = 120;
		max_div_def = 1; // 120hz
		min_div_def = min_div_lowest = 120; // 1hz
		break;
	default:
		LCD_ERR(vdd, "invalid vrr_id\n");
		base_rr = 120;
		max_div_def = 1; // 120hz
		min_div_def = min_div_lowest = 120; // 1hz
		break;
	}

	/* HBM 1Hz */
	if (mngr->scalability[LFD_SCOPE_NORMAL] == LFD_FUNC_SCALABILITY6) {
		min_div_def = min_div_lowest;
	}

done:
	lfd_base->base_rr = base_rr;
	lfd_base->max_div_def = max_div_def;
	lfd_base->min_div_def = min_div_def;
	lfd_base->min_div_lowest = min_div_lowest;

	vrr->lfd.base_rr = base_rr;

	LCD_INFO(vdd, "LFD(%s): base_rr: %uhz, def: %uhz(%u)~%uhz(%u), lowest: %uhz(%u)\n",
			lfd_scope_name[scope],
			base_rr,
			DIV_ROUND_UP(base_rr, min_div_def), min_div_def,
			DIV_ROUND_UP(base_rr, max_div_def), max_div_def,
			DIV_ROUND_UP(base_rr, min_div_lowest), min_div_lowest);

	return 0;
}

/* 1. LFD min freq. and frame insertion (BDh 0x06th): BDh 0x6 offset, for LFD control
 * divider: 2nd param: (divder - 1)
 * frame insertion: 5th param: repeat count of current fps
 * 3rd param[7:4] : repeat count of half fps
 * 3rd param[3:0] : repeat count of quad fps
 *
 * ---------------------------------------------------------
 *	 LFD_MIN	120HS~60HS	48HS~10HS  1HS		 60NS~30NS
 *   LFD_MAX
 * ---------------------------------------------------------
 * 120HS~96HS
 *	60HS~10HS
 *	60NS~30NS
 * ---------------------------------------------------------
 */

enum FRAME_INSERTIOIN_CASE {
	CASE1 = 0,
	CASE2,
	CASE3,
	CASE4,
	CASE5,
	CASE6,
	CASE7,
	CASE8,
	CASE9,
	CASE_MAX,
};

/* BDh : 99th ~ 107th + 17th + 114th ~ 117th */
static u8 FRAME_INSERT_CMD[CASE_MAX][14] = {
	{0x00, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00},	/* CASE1 */
	{0x00, 0x00, 0x02, 0x00, 0x06, 0x00, 0x16, 0x00, 0x16, 0x00, 0x01, 0x00, 0x03, 0x00},	/* CASE2 */
	{0x02, 0x00, 0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x00},	/* CASE3 */
	{0x00, 0x00, 0x02, 0x00, 0x06, 0x00, 0x16, 0x00, 0xEE, 0x01, 0x00, 0x00, 0x03, 0x05},	/* CASE4 */
	{0x02, 0x00, 0x02, 0x00, 0x16, 0x00, 0x16, 0x00, 0xEE, 0x00, 0x01, 0x00, 0x00, 0x01},	/* CASE5 */
	{0x04, 0x00, 0x06, 0x00, 0x16, 0x00, 0xEE, 0x00, 0xEE, 0x00, 0x00, 0x01, 0x03, 0x00},	/* CASE6 */
	{0x06, 0x00, 0x08, 0x00, 0x1C, 0x00, 0xEE, 0x00, 0xEE, 0x00, 0x00, 0x01, 0x03, 0x00},	/* CASE7 */
	{0x08, 0x00, 0x0A, 0x00, 0x16, 0x00, 0xEE, 0x00, 0xEE, 0x00, 0x00, 0x02, 0x02, 0x00},	/* CASE8 */
	{0x00, 0x00, 0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00},	/* CASE9 */
};

static int ss_get_frame_insert_cmd(struct samsung_display_driver_data *vdd,
	u8 *out_cmd, u32 base_rr, u32 min_div, u32 max_div, bool cur_hs)
{
	u32 min_freq = DIV_ROUND_UP(base_rr, min_div);
	u32 max_freq = DIV_ROUND_UP(base_rr, max_div);
	int case_n = 0;

	if (cur_hs) {
		if (min_freq > 48) {
			/* CASE1 : LFD_MIN=120HS~60HS */
			case_n = CASE1;
		} else if (max_freq == 30) {
			case_n = CASE6;
		} else if (max_freq == 24) {
			case_n = CASE7;
		} else if (max_freq <= 10) {
			case_n = CASE8;
		} else if (min_freq == 1) {
			if (max_freq > 60)
				case_n = CASE4;	/* CASE4 : 120HS ~ 1HS */
			else if (max_freq > 30)
				case_n = CASE5; /* CASE5 : 60HS ~ 1HS */
		} else if (max_freq > 60) {
			/* CASE2 : LFD_MIN=48HS~10HS, LFD_MAX=120HS~96HS */
			case_n = CASE2;
		} else {
			/* CASE3 : LFD_MIN=48HS~10HS, LFD_MAX=60HS~48HS */
			case_n = CASE3;
		}
	} else {
		/* CASE5 : NS mode */
		case_n = CASE9;
	}

	memcpy(out_cmd, FRAME_INSERT_CMD[case_n], sizeof(FRAME_INSERT_CMD[0]) / sizeof(u8));

	LCD_INFO(vdd, "frame insert[case %d]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X(LFD %uhz~%uhz, %s)\n",
			case_n + 1,
			out_cmd[0], out_cmd[1], out_cmd[2], out_cmd[3], out_cmd[4],
			out_cmd[5], out_cmd[6], out_cmd[7], out_cmd[8], out_cmd[9],
			out_cmd[10], out_cmd[11], out_cmd[12], out_cmd[13],
			min_freq, max_freq, cur_hs ? "HS" : "NS");

	return 0;
}

static void get_lfd_cmds(struct samsung_display_driver_data *vdd, int cur_hs, int fps, u8 *out_cmd)
{
	if (fps >= 96) {
		out_cmd[0] = 0x00;
		out_cmd[1] = 0x00;
	} else if (fps >= 48) {
		if (cur_hs) {
			out_cmd[0] = 0x00;
			out_cmd[1] = 0x02;
		} else {
			out_cmd[0] = 0x00;
			out_cmd[1] = 0x00;
		}
	} else if (fps >= 32) {
		out_cmd[0] = 0x00;
		out_cmd[1] = 0x04;
	} else if (fps == 30) {
		if (cur_hs) {
			out_cmd[0] = 0x00;
			out_cmd[1] = 0x06;
		} else {
			out_cmd[0] = 0x00;
			out_cmd[1] = 0x04;
		}
	} else if (fps == 30) {
		out_cmd[0] = 0x00;
		out_cmd[1] = 0x08;
	} else if (fps == 24) {
		out_cmd[0] = 0x00;
		out_cmd[1] = 0x08;
	} else if (fps == 10 || fps == 12) {
		out_cmd[0] = 0x00;
		out_cmd[1] = 0x16;
	} else if (fps == 1) {
		out_cmd[0] = 0x00;
		out_cmd[1] = 0xEE;
	} else {
		LCD_ERR(vdd, "No lfd case..\n");
		out_cmd[0] = 0x00;
		out_cmd[1] = 0x00;
	}

	LCD_DEBUG(vdd, "hs(%d) fps (%d), 0x%02X 0x%02X\n", cur_hs, fps, out_cmd[0], out_cmd[1]);
	return;
}

static int ss_get_base_vrr(struct samsung_display_driver_data *vdd, int rr, bool hs)
{
	int vrr_base;

	switch (rr) {
	case 120:
		vrr_base = 120;
		break;
	case 96:
		vrr_base = 96;
		break;
	case 60:
		if (hs)
			vrr_base = 120;
		else
			vrr_base = 60;
		break;
	case 48:
		vrr_base = 96;
		break;
	case 30:
		vrr_base = 120;
		break;
	case 24:
		vrr_base = 120;
		break;
	case 10:
		vrr_base = 120;
		break;
	default:
		LCD_ERR(vdd, "invalid refresh rate (%d, %d), set default 120HS..\n", rr, hs);
		vrr_base = 120;
		break;
	}

	return vrr_base;
}

static struct dsi_panel_cmd_set *ss_vrr(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct dsi_panel_cmd_set  *vrr_cmds = ss_get_cmds(vdd, TX_VRR);
	bool is_hbm = vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL ? true : false;
	struct vrr_info *vrr = &vdd->vrr;
	int cur_rr, base_rr, prev_rr;
	bool cur_hs, prev_hs;
	u32 min_div, max_div;
	enum LFD_SCOPE_ID scope;
	enum VRR_CMD_RR vrr_id;
	u8 cmd_frame_insert[14];
	u8 cmd_lfd_max[2];
	u8 cmd_lfd_min[2];
	int idx, cd_idx;
	int delay;

	if (SS_IS_CMDS_NULL(vrr_cmds)) {
		LCD_INFO(vdd, "no vrr cmds\n");
		return NULL;
	}

	cur_rr = vrr->cur_refresh_rate;
	cur_hs = vrr->cur_sot_hs_mode;
	prev_rr = vrr->prev_refresh_rate;
	prev_hs = vrr->prev_sot_hs_mode;
	base_rr = ss_get_base_vrr(vdd, prev_rr, prev_hs);

	if (!vrr->lfd.running_lfd && !vrr->running_vrr && !vdd->vrr.need_vrr_update) {
		LCD_INFO(vdd, "No vrr/lfd update lfd(%d) vrr(%d) need(%d)\n",
			vrr->lfd.running_lfd, vrr->running_vrr, vdd->vrr.need_vrr_update);
		return NULL;
	}

	if (panel && panel->cur_mode) {
		LCD_DEBUG(vdd, "VRR: cur_mode: %dx%d@%d%s, is_hbm: %d, runnig_lfd : %d, rev: %c\n",
				panel->cur_mode->timing.h_active,
				panel->cur_mode->timing.v_active,
				panel->cur_mode->timing.refresh_rate,
				panel->cur_mode->timing.sot_hs_mode ? "HS" : "NM",
				is_hbm, vrr->lfd.running_lfd, vdd->panel_revision + 'A');

		if (panel->cur_mode->timing.refresh_rate != vdd->vrr.adjusted_refresh_rate ||
				panel->cur_mode->timing.sot_hs_mode != vdd->vrr.adjusted_sot_hs_mode)
			LCD_ERR(vdd, "VRR: unmatched RR mode (%dhz%s / %dhz%s)\n",
					panel->cur_mode->timing.refresh_rate,
					panel->cur_mode->timing.sot_hs_mode ? "HS" : "NM",
					vdd->vrr.adjusted_refresh_rate,
					vdd->vrr.adjusted_sot_hs_mode ? "HS" : "NM");
	}

	min_div = max_div = 0;
	scope = LFD_SCOPE_NORMAL;
	vrr_id = ss_get_vrr_id(vdd);

	if (vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL)
		cd_idx = MAX_BL_PF_LEVEL + 1;
	else
		cd_idx = vdd->br_info.common_br.cd_idx;

	/* LPTS update will apply after 1 vsync */
	delay = ss_frame_delay(base_rr, 1);
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0xF7);
	if (idx >= 0)
		vrr_cmds->cmds[idx].post_wait_ms = delay / 1000;

	/* TSP/Wacom Sync */
	idx = ss_get_cmd_idx(vrr_cmds, 0x38, 0xB9);
	if (idx >= 0)
		vrr_cmds->cmds[idx].ss_txbuf[1] = cur_hs ? 0x00 : 0x80;

	/* 3Ch */
	idx = ss_get_cmd_idx(vrr_cmds, 0xA7, 0xBD);
	if (idx >= 0) {
		if (!cur_hs)
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x16; // OFF
		else if (cur_rr == 60 || cur_rr == 48)
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x16;
		else
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x17; // ON (120, 96, 30, 24, 10)
	}

	/* Freq Setting */
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0x60);
	if (idx >= 0) {
		if (vdd->panel_revision + 'A' < 'B')
			vrr_cmds->cmds[idx].ss_txbuf[1] = cur_hs ? 0x00 : 0x10;
		else
			vrr_cmds->cmds[idx].ss_txbuf[1] = cur_hs ? 0x01 : 0x11;
	}

	if (ss_get_lfd_div(vdd, scope, &min_div, &max_div)) {
		LCD_ERR(vdd, "fail to get LFD divider.. set default 1..\n");
		max_div = min_div = 1;
	}
	vrr->lfd.min_div = min_div;
	vrr->lfd.max_div = max_div;

	/* LFD MAX Setting */
	get_lfd_cmds(vdd, cur_hs, DIV_ROUND_UP(vrr->lfd.base_rr, max_div), cmd_lfd_max);
	idx = ss_get_cmd_idx(vrr_cmds, 0x1E, 0xBD);
	if (idx < 0) {
		idx = ss_get_cmd_idx(vrr_cmds, 0x3E, 0xBD);
	}

	if (idx >= 0) {
		if (cur_hs) {
			vrr_cmds->cmds[idx - 1].ss_txbuf[1] = 0x00;
			vrr_cmds->cmds[idx - 1].ss_txbuf[2] = 0x1E;
		} else {
			vrr_cmds->cmds[idx - 1].ss_txbuf[1] = 0x00;
			vrr_cmds->cmds[idx - 1].ss_txbuf[2] = 0x3E;
		}
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], cmd_lfd_max, sizeof(cmd_lfd_max));
	}

	/***************************************************************************************/
	/********************************* Frame insertion cmds ********************************/

	ss_get_frame_insert_cmd(vdd, cmd_frame_insert, vrr->lfd.base_rr, min_div, max_div, cur_hs);

	/* BDh 99 ~ 107th */
	idx = ss_get_cmd_idx(vrr_cmds, 0x62, 0xBD);
	if (idx >= 0)
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], &cmd_frame_insert[0], 9);

	/* LFD (1) + 00 (1) + 1st Frame insertion (1) +  LFD Min (2) */

	/* BDh 15th : LFD setting */
	idx = ss_get_cmd_idx(vrr_cmds, 0x0E, 0xBD);
	if (idx >= 0) {
		if ((cur_rr == 60 && cur_hs) || cur_rr == 48 || cur_rr == 30 || cur_rr == 24 || cur_rr == 10)
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x00;	// OFF
		else
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x10; // ON

		/* DBh 17th : 1st frmae insertion */
		//idx = ss_get_cmd_idx(vrr_cmds, 0x10, 0xBD);
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[3], &cmd_frame_insert[9], 1);

		/* DBh 18 ~ 19th : LFD MIN Setting */
		get_lfd_cmds(vdd, cur_hs, DIV_ROUND_UP(vrr->lfd.base_rr, min_div), cmd_lfd_min);
		//idx = ss_get_cmd_idx(vrr_cmds, 0x11, 0xBD);
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[4], cmd_lfd_min, sizeof(cmd_lfd_min));
	}

	/* BDh 114 ~ 117th */
	idx = ss_get_cmd_idx(vrr_cmds, 0x71, 0xBD);
	if (idx >= 0)
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], &cmd_frame_insert[10], 4);

	/***************************************************************************************/

	/* AOR Setting */
	idx = ss_get_cmd_idx(vrr_cmds, 0x216, 0x98);
	if (idx < 0) {
		idx = ss_get_cmd_idx(vrr_cmds, 0x1D6, 0x98);
	}

	if (idx >= 0) {
		if (cur_hs) {
			vrr_cmds->cmds[idx - 1].ss_txbuf[1] = 0x02;
			vrr_cmds->cmds[idx - 1].ss_txbuf[2] = 0x16;
		} else {
			vrr_cmds->cmds[idx - 1].ss_txbuf[1] = 0x01;
			vrr_cmds->cmds[idx - 1].ss_txbuf[2] = 0xD6;
		}
	}

#if 0
	/* Manual AOR */
	idx = ss_get_cmd_idx(vrr_cmds, 0x19F, 0x98);
	if (cur_rr == 60 || cur_rr == 120 || !cur_hs) {
		vrr_cmds->cmds[idx].ss_txbuf[1] = 0x00;	/* off */
	} else {
		vrr_cmds->cmds[idx].ss_txbuf[1] = 0x01; /* on */
	}

	if (cur_rr == 48 || cur_rr == 96) {
		vrr_cmds->cmds[idx].ss_txbuf[2] = (aor_table_96[cd_idx] & 0xFF00) >> 8;
		vrr_cmds->cmds[idx].ss_txbuf[3] = aor_table_96[cd_idx] & 0xFF;
	} else {
		vrr_cmds->cmds[idx].ss_txbuf[2] = (aor_table_120[cd_idx] & 0xFF00) >> 8;
		vrr_cmds->cmds[idx].ss_txbuf[3] = aor_table_120[cd_idx] & 0xFF;
	}
#endif

	/* VFP */
	idx = ss_get_cmd_idx(vrr_cmds, 0x09, 0xF2);
	if (idx < 0)
		idx = ss_get_cmd_idx(vrr_cmds, 0x0F, 0xF2);

	if (idx >= 0) {
		if (cur_hs) {
			vrr_cmds->cmds[idx - 1].ss_txbuf[1] = 0x00;
			vrr_cmds->cmds[idx - 1].ss_txbuf[2] = 0x09;
		} else {
			vrr_cmds->cmds[idx - 1].ss_txbuf[1] = 0x00;
			vrr_cmds->cmds[idx - 1].ss_txbuf[2] = 0x0F;
		}

		if (cur_rr == 120 || cur_rr == 60 || cur_rr == 30 || cur_rr == 24 || cur_rr == 10) {
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x00;
			vrr_cmds->cmds[idx].ss_txbuf[2] = 0x10;
		} else {
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x03;
			vrr_cmds->cmds[idx].ss_txbuf[2] = 0x24;
		}
	}

	/* TE_FRAME_SEL */
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0xB9);
	if (idx >= 0) {
		if (cur_rr == 120) {
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[17] = 0x06;
			vrr_cmds->cmds[idx].ss_txbuf[18] = 0x18;
		} else if (cur_rr == 96) {
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[17] = 0x04;
			vrr_cmds->cmds[idx].ss_txbuf[18] = 0x8E;
		} else if (cur_rr == 60 && cur_hs) {
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x03;
			vrr_cmds->cmds[idx].ss_txbuf[17] = 0x06;
			vrr_cmds->cmds[idx].ss_txbuf[18] = 0x18;
		} else if (cur_rr == 60 && !cur_hs) {
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x03;
			vrr_cmds->cmds[idx].ss_txbuf[17] = 0x03;
			vrr_cmds->cmds[idx].ss_txbuf[18] = 0x04;
		} else if (cur_rr == 48) {
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x03;
			vrr_cmds->cmds[idx].ss_txbuf[17] = 0x04;
			vrr_cmds->cmds[idx].ss_txbuf[18] = 0x8E;
		} else if (cur_rr == 30) {
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x07;
			vrr_cmds->cmds[idx].ss_txbuf[17] = 0x06;
			vrr_cmds->cmds[idx].ss_txbuf[18] = 0x18;
		} else if (cur_rr == 24) {
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x09;
			vrr_cmds->cmds[idx].ss_txbuf[17] = 0x06;
			vrr_cmds->cmds[idx].ss_txbuf[18] = 0x18;
		} else if (cur_rr == 10) {
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x17;
			vrr_cmds->cmds[idx].ss_txbuf[17] = 0x06;
			vrr_cmds->cmds[idx].ss_txbuf[18] = 0x18;
		}
	}
	vdd->vrr.need_vrr_update = false;

	LCD_INFO(vdd, "cur:%dx%d@%d%s, adj:%d%s, cd_idx:%d, is_hbm:%d, LFD:%uhz(%u)~%uhz(%u), running_lfd(%d), Delay(%d), prev_rr(%d) base_rr(%d) rev:%c\n",
			panel->cur_mode->timing.h_active,
			panel->cur_mode->timing.v_active,
			cur_rr,
			cur_hs ? "HS" : "NM",
			vrr->adjusted_refresh_rate,
			vrr->adjusted_sot_hs_mode ? "HS" : "NM", cd_idx, is_hbm,
			DIV_ROUND_UP(vrr->lfd.base_rr, min_div), min_div,
			DIV_ROUND_UP(vrr->lfd.base_rr, max_div), max_div,
			vrr->lfd.running_lfd, delay, prev_rr, base_rr, vdd->panel_revision + 'A');

	return vrr_cmds;
}

static u8 IRC_MEDERATO_VAL1[IRC_MAX_MODE][4] = {
	{0x61, 0xFF, 0x01, 0x00},
	{0x21, 0xFA, 0X03, 0X77},
};

static u8 IRC_MEDERATO_VAL2[IRC_MAX_MODE][3] = {
	{0x6C, 0x9A, 0x32},
	{0x67, 0x93, 0X25},
};

static struct dsi_panel_cmd_set * ss_brightness_gamma(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	struct vrr_info *vrr;
	int idx = 0;
	int irc_mode = 0;
	int smooth_frame = 0;
	int cur_rr, bl_lv, cur_base;
	bool cur_hs, dim_off = true;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	vrr = &vdd->vrr;
	cur_rr = vrr->cur_refresh_rate;
	cur_hs = vrr->cur_sot_hs_mode;
	cur_base = ss_get_base_vrr(vdd, cur_rr, cur_hs);

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_NORMAL);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_INFO(vdd, "no gamma cmds\n");
		return NULL;
	}

	irc_mode = vdd->br_info.common_br.irc_mode;
	bl_lv = vdd->br_info.common_br.bl_level;

	LCD_DEBUG(vdd, "NORMAL : bl_lv [%d] irc [%d]\n", bl_lv, irc_mode);

	/* IRC mode */
	idx = ss_get_cmd_idx(pcmds, 0x152, 0x92);
	if (idx >= 0) {
		if (irc_mode < IRC_MAX_MODE)
			memcpy(&pcmds->cmds[idx].ss_txbuf[1], IRC_MEDERATO_VAL1[irc_mode], sizeof(IRC_MEDERATO_VAL1[0]) / sizeof(u8));
	}

	idx = ss_get_cmd_idx(pcmds, 0x15E, 0x92);
	if (idx >= 0) {
		if (irc_mode < IRC_MAX_MODE)
			memcpy(&pcmds->cmds[idx].ss_txbuf[1], IRC_MEDERATO_VAL2[irc_mode], sizeof(IRC_MEDERATO_VAL2[0]) / sizeof(u8));
	}
	/* TSET */
	idx = ss_get_cmd_idx(pcmds, 0x01, 0xB1);
	if (idx >= 0) {
		pcmds->cmds[idx].ss_txbuf[1] = vdd->br_info.temperature > 0 ?
			vdd->br_info.temperature : (char)(BIT(7) | (-1*vdd->br_info.temperature));
	}

	/* WRDISDV */
	idx = ss_get_cmd_idx(pcmds, 0x00, 0x51);
	if (idx >= 0) {
		pcmds->cmds[idx].ss_txbuf[1] = (vdd->br_info.common_br.gm2_wrdisbv & 0xFF00) >> 8;
		pcmds->cmds[idx].ss_txbuf[2] = vdd->br_info.common_br.gm2_wrdisbv & 0xFF;
	}

	/* smooth - 120HS base + 60NS base is ON */
	if (cur_base == 120 || cur_base == 60) {
		dim_off = false;
		smooth_frame = 15;
	}

	if (vrr->running_vrr) {
		dim_off = true;
		smooth_frame = 0;
	}

	idx = ss_get_cmd_idx(pcmds, 0x0D, 0x94);
	if (idx >= 0) {
		pcmds->cmds[idx].ss_txbuf[1] = dim_off ? 0x20 : 0x60;
		pcmds->cmds[idx].ss_txbuf[2] = vdd->display_on ? smooth_frame : (cur_hs ? 0x01 : 0x02);
	}

	idx = ss_get_cmd_idx(pcmds, 0x00, 0x53);
	if (idx >= 0) {
		pcmds->cmds[idx].ss_txbuf[1] = dim_off ? 0x20 : 0x28;
	}

	/* Manual AOR enable */
	idx = ss_get_cmd_idx(pcmds, 0x19F, 0x98);
	if (idx >= 0) {
		if (cur_rr == 48 || cur_rr == 96) {
			pcmds->cmds[idx].ss_txbuf[1] = 0x01; /* on */
		} else {
			pcmds->cmds[idx].ss_txbuf[1] = 0x00; /* off */
		}

		if (cur_rr == 48 || cur_rr == 96) {
			pcmds->cmds[idx].ss_txbuf[2] = (aor_table_96[bl_lv] & 0xFF00) >> 8;
			pcmds->cmds[idx].ss_txbuf[3] = aor_table_96[bl_lv] & 0xFF;
		} else {
			pcmds->cmds[idx].ss_txbuf[2] = (aor_table_120[bl_lv] & 0xFF00) >> 8;
			pcmds->cmds[idx].ss_txbuf[3] = aor_table_120[bl_lv] & 0xFF;
		}
	}

	/* GLUT Offset Enable */
	if (vdd->panel_revision + 'A' >= 'B') {
		idx = ss_get_cmd_idx(pcmds, 0x2ED, 0x92);
		if (idx >= 0) {
			if (cur_rr == 48 || cur_rr == 96)
				pcmds->cmds[idx].ss_txbuf[1] = 0x02;	/* Enable */
			else
				pcmds->cmds[idx].ss_txbuf[1] = 0x00;	/* Disable */
		}
	}

	if (vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL) {
		/* HBM -> Normal Case */
		if (vdd->br_info.last_br_is_hbm) {
			LCD_INFO(vdd, "HBM -> Normal Case, Disable ESD\n");

			/* If there is a pending ESD enable work, cancel that first */
			cancel_delayed_work(&vdd->esd_enable_event_work);

			/* To avoid unexpected ESD detction, Disable ESD irq before cmd tx related with 51h */
			if (vdd->esd_recovery.esd_irq_enable)
				vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd, ESD_MASK_WORK);

			/* Enable ESD after (ESD_WORK_DELAY)ms */
			schedule_delayed_work(&vdd->esd_enable_event_work,
				msecs_to_jiffies(ESD_WORK_DELAY));
		}

		vdd->br_info.last_br_is_hbm = false;
	} else {
		/* Normal -> HBM Case */
		if (!vdd->br_info.last_br_is_hbm) {
			LCD_INFO(vdd, "Normal -> HBM Case, Disable ESD\n");

			/* If there is a pending ESD enable work, cancel that first */
			cancel_delayed_work(&vdd->esd_enable_event_work);

			/* To avoid unexpected ESD detction, Disable ESD irq before cmd tx related with 51h */
			if (vdd->esd_recovery.esd_irq_enable)
				vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd, ESD_MASK_WORK);

			/* Enable ESD after (ESD_WORK_DELAY)ms */
			schedule_delayed_work(&vdd->esd_enable_event_work,
				msecs_to_jiffies(ESD_WORK_DELAY));
		}

		vdd->br_info.last_br_is_hbm = true;
	}

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

	vdd->br_info.last_br_is_hbm = false;

	return pcmds;
}


static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	char *read_buf;
	int i, len = 0;
	u8 temp[20];
	int ddi_id_len;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	pcmds = ss_get_cmds(vdd, RX_DDI_ID);

	/* Read mtp (D6h 1~5th) for CHIP ID */
	if (pcmds->count) {
		ddi_id_len = pcmds->cmds[0].msg.rx_len;
		read_buf = kzalloc(ddi_id_len, GFP_KERNEL);
		if (!read_buf) {
			LCD_ERR(vdd, "fail to kzalloc for read_buf\n");
			return false;
		}

		ss_panel_data_read(vdd, RX_DDI_ID, read_buf, LEVEL1_KEY);

		for (i = 0; i < ddi_id_len; i++)
			len += sprintf(temp + len, "%02x", read_buf[i]);
		len += sprintf(temp + len, "\n");

		vdd->ddi_id_dsi = kzalloc(len, GFP_KERNEL);
		if (!vdd->ddi_id_dsi)
			LCD_ERR(vdd, "fail to kzalloc for ddi_id_dsi\n");
		else {
			vdd->ddi_id_len = len;
			strlcat(vdd->ddi_id_dsi, temp, len);
			LCD_INFO(vdd, "[%d] %s\n", vdd->ddi_id_len, vdd->ddi_id_dsi);
		}
	} else {
		LCD_ERR(vdd, "DSI%d no ddi_id_rx_cmds cmds", vdd->ndx);
		return false;
	}

	kfree(read_buf);

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
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_8 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_9 */
};

static char coordinate_data_2[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xfa, 0x00, 0xf4, 0x00}, /* dummy */
	{0xff, 0x00, 0xfa, 0x00, 0xf4, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfa, 0x00, 0xf4, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xfa, 0x00, 0xf4, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfa, 0x00, 0xf4, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xfa, 0x00, 0xf4, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xfa, 0x00, 0xf4, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xfa, 0x00, 0xf4, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xfa, 0x00, 0xf4, 0x00}, /* Tune_8 */
	{0xff, 0x00, 0xfa, 0x00, 0xf4, 0x00}, /* Tune_9 */
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
	u8 elvss_buf[3];

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (94h 18 ~ 1Ath) for grayspot */
	ss_panel_data_read(vdd, RX_ELVSS, elvss_buf, LEVEL1_KEY);

	vdd->br_info.common_br.elvss_value[0] = elvss_buf[0]; /* 94h 18th */
	vdd->br_info.common_br.elvss_value[1] = elvss_buf[2]; /* 94h 1Ath */

	return true;
}

static void ss_gray_spot(struct samsung_display_driver_data *vdd, int enable)
{
	struct dsi_panel_cmd_set *pcmds;
#if 0 // remove updatable feature
	int idx = 0;
#endif

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return;
	}

	pcmds = ss_get_cmds(vdd, TX_GRAY_SPOT_TEST_OFF);
	if (IS_ERR_OR_NULL(pcmds)) {
        LCD_ERR(vdd, "Invalid pcmds\n");
        return;
    }

#if 0 // remove updatable feature
	if (!enable) { /* grayspot off */
		if (ss_wrapper_restore_backup_cmd(vdd, TX_GRAY_SPOT_TEST_OFF,
					0x18, 0x94)) {
			LCD_ERR(vdd, "fail to restore backup cmd\n");
			return;
		}

		/* restore ELVSS_CAL_OFFSET (94h 0x18th) */
		idx = ss_get_cmd_idx(pcmds, 0x18, 0x94);
		if (idx < 0) {
			LCD_ERR(vdd, "fail to find C2h (off=0x14) cmd\n");
			return;
		}

		pcmds->cmds[idx].ss_txbuf[1] = vdd->br_info.common_br.elvss_value[0];

		/* restore ELVSS_CAL_OFFSET (94h 0x1Ath) */
		idx = ss_get_cmd_idx(pcmds, 0x1A, 0x94);
		if (idx < 0) {
			LCD_ERR(vdd, "fail to find C2h (off=0x14) cmd\n");
			return;
		}

		pcmds->cmds[idx].ss_txbuf[1] = vdd->br_info.common_br.elvss_value[1];

		LCD_INFO(vdd, "update 94h elvss(0x%x 0x%x)\n",
				vdd->br_info.common_br.elvss_value[0],
				vdd->br_info.common_br.elvss_value[1]);
	}
#endif
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
	mdnie_data->DSI_COLOR_BLIND_MDNIE_1 = COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_1 = RGB_SENSOR_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_2 = RGB_SENSOR_MDNIE_2;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_3 = RGB_SENSOR_MDNIE_3;
	mdnie_data->DSI_TRANS_DIMMING_MDNIE = RGB_SENSOR_MDNIE_3;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE_2 = UI_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_UI_STANDARD_MDNIE_2 = UI_STANDARD_MDNIE_2;
	mdnie_data->DSI_UI_AUTO_MDNIE_2 = UI_AUTO_MDNIE_2;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE_2 = VIDEO_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE_2 = VIDEO_STANDARD_MDNIE_2;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE_2 = VIDEO_AUTO_MDNIE_2;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE_2 = CAMERA_AUTO_MDNIE_2;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE_2 = GALLERY_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE_2 = GALLERY_STANDARD_MDNIE_2;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE_2 = GALLERY_AUTO_MDNIE_2;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE_2 = BROWSER_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE_2 = BROWSER_STANDARD_MDNIE_2;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE_2 = BROWSER_AUTO_MDNIE_2;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE_2 = EBOOK_AUTO_MDNIE_2;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE_2 = EBOOK_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE_2 = EBOOK_STANDARD_MDNIE_2;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE_2 = EBOOK_AUTO_MDNIE_2;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE_2 = TDMB_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE_2 = TDMB_STANDARD_MDNIE_2;
	mdnie_data->DSI_TDMB_AUTO_MDNIE_2 = TDMB_AUTO_MDNIE_2;

	mdnie_data->DSI_BYPASS_MDNIE = BYPASS_MDNIE;
	mdnie_data->DSI_NEGATIVE_MDNIE = NEGATIVE_MDNIE;
	mdnie_data->DSI_COLOR_BLIND_MDNIE = COLOR_BLIND_MDNIE;
	mdnie_data->DSI_HBM_CE_MDNIE = HBM_CE_MDNIE;
	mdnie_data->DSI_HBM_CE_D65_MDNIE = HBM_CE_D65_MDNIE;
	mdnie_data->DSI_RGB_SENSOR_MDNIE = RGB_SENSOR_MDNIE;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE = UI_DYNAMIC_MDNIE;
	mdnie_data->DSI_UI_STANDARD_MDNIE = UI_STANDARD_MDNIE;
	mdnie_data->DSI_UI_NATURAL_MDNIE = UI_NATURAL_MDNIE;
	mdnie_data->DSI_UI_AUTO_MDNIE = UI_AUTO_MDNIE;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE = VIDEO_DYNAMIC_MDNIE;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE = VIDEO_STANDARD_MDNIE;
	mdnie_data->DSI_VIDEO_NATURAL_MDNIE = VIDEO_NATURAL_MDNIE;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE = VIDEO_AUTO_MDNIE;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE = CAMERA_AUTO_MDNIE;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE = GALLERY_DYNAMIC_MDNIE;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE = GALLERY_STANDARD_MDNIE;
	mdnie_data->DSI_GALLERY_NATURAL_MDNIE = GALLERY_NATURAL_MDNIE;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE = GALLERY_AUTO_MDNIE;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE = BROWSER_DYNAMIC_MDNIE;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE = BROWSER_STANDARD_MDNIE;
	mdnie_data->DSI_BROWSER_NATURAL_MDNIE = BROWSER_NATURAL_MDNIE;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE = BROWSER_AUTO_MDNIE;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE = EBOOK_DYNAMIC_MDNIE;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE = EBOOK_STANDARD_MDNIE;
	mdnie_data->DSI_EBOOK_NATURAL_MDNIE = EBOOK_NATURAL_MDNIE;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE = EBOOK_AUTO_MDNIE;
	mdnie_data->DSI_EMAIL_AUTO_MDNIE = EMAIL_AUTO_MDNIE;
	mdnie_data->DSI_GAME_LOW_MDNIE = BYPASS_MDNIE;
	mdnie_data->DSI_GAME_MID_MDNIE = BYPASS_MDNIE;
	mdnie_data->DSI_GAME_HIGH_MDNIE = BYPASS_MDNIE;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE = TDMB_DYNAMIC_MDNIE;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE = TDMB_STANDARD_MDNIE;
	mdnie_data->DSI_TDMB_NATURAL_MDNIE = TDMB_NATURAL_MDNIE;
	mdnie_data->DSI_TDMB_AUTO_MDNIE = TDMB_AUTO_MDNIE;
	mdnie_data->DSI_GRAYSCALE_MDNIE = GRAYSCALE_MDNIE;
	mdnie_data->DSI_GRAYSCALE_NEGATIVE_MDNIE = GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data->DSI_CURTAIN = SCREEN_CURTAIN_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE = NIGHT_MODE_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE_SCR = NIGHT_MODE_MDNIE_1;
	mdnie_data->DSI_COLOR_LENS_MDNIE = COLOR_LENS_MDNIE;
	mdnie_data->DSI_COLOR_LENS_MDNIE_SCR = COLOR_LENS_MDNIE_1;
	mdnie_data->DSI_COLOR_BLIND_MDNIE_SCR = COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_SCR = RGB_SENSOR_MDNIE_1;

	mdnie_data->mdnie_tune_value_dsi = mdnie_tune_value_dsi0;
	mdnie_data->hmt_color_temperature_tune_value_dsi = hmt_color_temperature_tune_value_dsi0;
	mdnie_data->light_notification_tune_value_dsi = light_notification_tune_value_dsi0;
	mdnie_data->hdr_tune_value_dsi = hdr_tune_value_dsi0;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data->dsi_bypass_mdnie_size = ARRAY_SIZE(BYPASS_MDNIE);
	mdnie_data->mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET;
	mdnie_data->mdnie_step_index[MDNIE_STEP1] = MDNIE_STEP1_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP2] = MDNIE_STEP2_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP3] = MDNIE_STEP3_INDEX;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_RED_OFFSET] = ADDRESS_SCR_WHITE_RED;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_GREEN_OFFSET] = ADDRESS_SCR_WHITE_GREEN;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_BLUE_OFFSET] = ADDRESS_SCR_WHITE_BLUE;
	mdnie_data->dsi_rgb_sensor_mdnie_1_size = RGB_SENSOR_MDNIE_1_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_2_size = RGB_SENSOR_MDNIE_2_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_3_size = RGB_SENSOR_MDNIE_3_SIZE;

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
	mdnie_data->dsi_afc_size = 71;
	mdnie_data->dsi_afc_index = 56;

	vdd->mdnie.mdnie_data = mdnie_data;

	return 0;
}

static int ss_module_info_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	char *buf;
	int year, month, day;
	int hour, min;
	int x, y;
	int mdnie_tune_index = 0;
	int ret;
	char temp[50];
	int buf_len, len = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	pcmds = ss_get_cmds(vdd, RX_MODULE_INFO);

	if (pcmds->count) {
		buf_len = pcmds->cmds[0].msg.rx_len;

		buf = kzalloc(buf_len, GFP_KERNEL);
		if (!buf) {
			LCD_ERR(vdd, "fail to kzalloc for buf\n");
			return false;
		}

		ret = ss_panel_data_read(vdd, RX_MODULE_INFO, buf, LEVEL1_KEY);
		if (ret) {
			LCD_ERR(vdd, "fail to read module ID, ret: %d", ret);
			kfree(buf);
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

		LCD_INFO(vdd, "X-%d Y-%d \n", vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);

		/* CELL ID (manufacture date + white coordinates) */
		/* Manufacture Date */
		len += sprintf(temp + len, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
			buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10],
			buf[0], buf[1], buf[2], buf[3]);

		vdd->cell_id_dsi = kzalloc(len, GFP_KERNEL);
		if (!vdd->cell_id_dsi)
			LCD_ERR(vdd, "fail to kzalloc for cell_id_dsi\n");
		else {
			vdd->cell_id_len = len;
			strlcat(vdd->cell_id_dsi, temp, vdd->cell_id_len);
			LCD_INFO(vdd, "CELL ID : [%d] %s\n", vdd->cell_id_len, vdd->cell_id_dsi);
		}
	} else {
		LCD_ERR(vdd, "no module_info_rx_cmds cmds(%d)", vdd->panel_revision);
		return false;
	}

	kfree(buf);

	return true;
}

static int ss_octa_id_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	char *read_buf;
	int read_len;
	char temp[50];
	int len = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	pcmds = ss_get_cmds(vdd, RX_OCTA_ID);

	/* Read Panel Unique OCTA ID (C9h 2nd~21th) */
	if (pcmds->count) {
		read_len = pcmds->cmds[0].msg.rx_len;
		read_buf = kzalloc(read_len, GFP_KERNEL);
		if (!read_buf) {
			LCD_ERR(vdd, "fail to kzalloc for buf\n");
			return false;
		}

		ss_panel_data_read(vdd, RX_OCTA_ID, read_buf, LEVEL1_KEY);

		LCD_INFO(vdd, "octa id (read buf): %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			read_buf[0], read_buf[1], read_buf[2], read_buf[3],
			read_buf[4], read_buf[5], read_buf[6], read_buf[7],
			read_buf[8], read_buf[9], read_buf[10], read_buf[11],
			read_buf[12], read_buf[13], read_buf[14], read_buf[15],
			read_buf[16], read_buf[17],	read_buf[18], read_buf[19]);

		len += sprintf(temp + len, "%d", (read_buf[0] & 0xF0) >> 4);
		len += sprintf(temp + len, "%d", (read_buf[0] & 0x0F));
		len += sprintf(temp + len, "%d", (read_buf[1] & 0x0F));
		len += sprintf(temp + len, "%02x", read_buf[2]);
		len += sprintf(temp + len, "%02x", read_buf[3]);

		len += sprintf(temp + len, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
			read_buf[4], read_buf[5], read_buf[6], read_buf[7],
			read_buf[8], read_buf[9], read_buf[10], read_buf[11],
			read_buf[12], read_buf[13], read_buf[14], read_buf[15],
			read_buf[16], read_buf[17], read_buf[18], read_buf[19]);

		vdd->octa_id_dsi = kzalloc(len, GFP_KERNEL);
		if (!vdd->octa_id_dsi)
			LCD_ERR(vdd, "fail to kzalloc for octa_id_dsi\n");
		else {
			vdd->octa_id_len = len;
			strlcat(vdd->octa_id_dsi, temp, vdd->octa_id_len);
			LCD_INFO(vdd, "octa id : [%d] %s \n", vdd->octa_id_len, vdd->octa_id_dsi);
		}
	} else {
		LCD_ERR(vdd, "DSI%d no octa_id_rx_cmds cmd\n", vdd->ndx);
		return false;
	}

	kfree(read_buf);

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

	idx = ss_get_cmd_idx(pcmds, 0x272, 0x98);
	if (vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL) {
		pcmds->cmds[idx].ss_txbuf[1] = 0x55;	/* 0x44 : 16Frame ACL OFF, 0x55 : 32Frame ACL ON */
		pcmds->cmds[idx].ss_txbuf[2] = 0x40;
		pcmds->cmds[idx].ss_txbuf[3] = 0xFF;	/* 0x80 0x67 : 60%, 0x40 0xFF : 50% */
	} else {
		pcmds->cmds[idx].ss_txbuf[1] = 0x55;	/* 0x44 : 16Frame ACL OFF, 0x55 : 32Frame ACL ON */
		pcmds->cmds[idx].ss_txbuf[2] = 0x40;
		pcmds->cmds[idx].ss_txbuf[3] = 0xFF;	/* 0x80 0x67 : 60%, 0x40 0xFF : 50% */
	}

	/* ACL DIM control */
	idx = ss_get_cmd_idx(pcmds, 0x281, 0x98);
	pcmds->cmds[idx].ss_txbuf[1] = 0x00;
	pcmds->cmds[idx].ss_txbuf[2] = 0x1F;		/* 0x00 0x00 : OFF, 0x00 0x1F : DIM 32frame */

	/* ACL 30% */
	idx = ss_get_cmd_idx(pcmds, 0x00, 0x55);
	if (vdd->br_info.gradual_acl_val == 2) {
		pcmds->cmds[idx].ss_txbuf[1] = 0x03;	/* 0x03 : ACL 30% */
	} else {
		if (vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL)
			if (vdd->br_info.gradual_acl_val == 3)
				pcmds->cmds[idx].ss_txbuf[1] = 0x02;	/* 0x02 : ACL 15% */
			else
				pcmds->cmds[idx].ss_txbuf[1] = 0x01;	/* 0x01 : ACL 8%, D.Lab Request(21.10.13) */
		else
			pcmds->cmds[idx].ss_txbuf[1] = 0x01;	/* 0x01 : ACL 8% */
	}

	LCD_DEBUG(vdd, "bl_level: %d, gradual_acl: %d, acl per: 0x%x", vdd->br_info.common_br.bl_level,
			vdd->br_info.gradual_acl_val, pcmds->cmds[idx].ss_txbuf[1]);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_acl_off(struct samsung_display_driver_data *vdd, int *level_key)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	LCD_DEBUG(vdd, "off\n");
	return ss_get_cmds(vdd, TX_ACL_OFF);
}

enum LPM_FPS {
	LPM_30HZ = 0,
	LPM_1HZ,
	LPM_FPS_MAX,
};

static u8 LPM_LTPS[LPM_FPS_MAX][7] = {
	{0x06, 0x30, 0x00, 0x02, 0x00, 0x26, 0x02},	/* 30hz */
	{0x00, 0x30, 0x00, 0x02, 0x00, 0x00, 0x00},	/* 1hz */
};

static u8 LPM_FREQ[LPM_FPS_MAX][2] = {
	{0x00, 0x00},	/* 30hz */
	{0x00, 0x74},	/* 1hz */
};

static void ss_set_panel_lpm_brightness(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *set = ss_get_cmds(vdd, TX_LPM_BL_CMD);
	struct dsi_panel_cmd_set *set_lpm_bl;
	int bl_idx, aor_idx, idx;
	u32 min_div, max_div;
	int min_fps, max_fps;
	struct vrr_info *vrr = &vdd->vrr;

	if (SS_IS_CMDS_NULL(set)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_BL_CMD\n");
		return;
	}

	mutex_lock(&vdd->display_on_lock);

	/* HAE ddi (B0) only issue */
	/* P220307-02604, P211207-05270 : Do not write AOD 60nit before AOD display on + 1vsync (34ms) */
	if ((vdd->panel_state == PANEL_PWR_LPM) &&
		(vdd->display_on == false) &&
		(vdd->panel_lpm.lpm_bl_level == LPM_60NIT)) {
		LCD_ERR(vdd, "Do not set AOD(%d) 60nit before display on(%d)\n", vdd->panel_state, vdd->display_on);
		vdd->panel_lpm.need_br_update = true;
		mutex_unlock(&vdd->display_on_lock);
		return;
	}

	mutex_unlock(&vdd->display_on_lock);

	min_fps = max_fps = min_div = max_div = 0;

	if (ss_get_lfd_div(vdd, LFD_SCOPE_LPM, &min_div, &max_div)) {
		LCD_ERR(vdd, "fail to get LFD divider.. set default 1..\n");
		max_div = min_div = 1;
	}

	min_fps = DIV_ROUND_UP(vrr->lfd.base_rr, min_div);
	max_fps = DIV_ROUND_UP(vrr->lfd.base_rr, max_div);

	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_60NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
		break;
	case LPM_30NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
		break;
	case LPM_10NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
		break;
	case LPM_2NIT:
	default:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
		break;
	}

	if (SS_IS_CMDS_NULL(set_lpm_bl)) {
		LCD_ERR(vdd, "No lpm bl cmd ..\n");
		return;
	}

	if (vdd->panel_revision + 'A' >= 'B') {
		idx = ss_get_cmd_idx(set, 0x20F, 0xCB);
		if (min_fps == 30)
			memcpy(&set->cmds[idx].ss_txbuf[1], LPM_LTPS[LPM_30HZ], sizeof(LPM_LTPS[0]) / sizeof(u8));
		else
			memcpy(&set->cmds[idx].ss_txbuf[1], LPM_LTPS[LPM_1HZ], sizeof(LPM_LTPS[0]) / sizeof(u8));

		idx = ss_get_cmd_idx(set, 0x4E, 0xBD);
		if (min_fps == 30)
			memcpy(&set->cmds[idx].ss_txbuf[1], LPM_FREQ[LPM_30HZ], sizeof(LPM_FREQ[0]) / sizeof(u8));
		else
			memcpy(&set->cmds[idx].ss_txbuf[1], LPM_FREQ[LPM_1HZ], sizeof(LPM_FREQ[0]) / sizeof(u8));
	}

	if (vdd->panel_revision + 'A' >= 'C') {
		idx = ss_get_cmd_idx(set, 0x17, 0xBD);
		if (min_fps == 30)
			memcpy(&set->cmds[idx].ss_txbuf[1], LPM_FREQ[LPM_30HZ], sizeof(LPM_FREQ[0]) / sizeof(u8));
		else
			memcpy(&set->cmds[idx].ss_txbuf[1], LPM_FREQ[LPM_1HZ], sizeof(LPM_FREQ[0]) / sizeof(u8));

		idx = ss_get_cmd_idx(set, 0x00, 0x51);
		if (min_fps == 1) {
			set->cmds[idx].ss_txbuf[1] = 0x07;
			set->cmds[idx].ss_txbuf[2] = 0xFF;
		} else {	/* 30Hz */
			switch (vdd->panel_lpm.lpm_bl_level) {
			case LPM_60NIT:
				set->cmds[idx].ss_txbuf[1] = 0x07;
				set->cmds[idx].ss_txbuf[2] = 0xFF;
				break;
			case LPM_30NIT:
				set->cmds[idx].ss_txbuf[1] = 0x03;
				set->cmds[idx].ss_txbuf[2] = 0xFF;
				break;
			case LPM_10NIT:
				set->cmds[idx].ss_txbuf[1] = 0x01;
				set->cmds[idx].ss_txbuf[2] = 0x55;
				break;
			case LPM_2NIT:
			default:
				set->cmds[idx].ss_txbuf[1] = 0x00;
				set->cmds[idx].ss_txbuf[2] = 0x44;
				break;
			}
		}

		idx = ss_get_cmd_idx(set, 0x1A0, 0x98);
		if (min_fps == 30) {
			set->cmds[idx].ss_txbuf[1] = 0x01;
			set->cmds[idx].ss_txbuf[2] = 0x00;
		} else { /* 1hz */
			switch (vdd->panel_lpm.lpm_bl_level) {
			case LPM_60NIT:
				set->cmds[idx].ss_txbuf[1] = 0x01;
				set->cmds[idx].ss_txbuf[2] = 0x00;
				break;
			case LPM_30NIT:
				set->cmds[idx].ss_txbuf[1] = 0x06;
				set->cmds[idx].ss_txbuf[2] = 0xA0;
				break;
			case LPM_10NIT:
				set->cmds[idx].ss_txbuf[1] = 0x0A;
				set->cmds[idx].ss_txbuf[2] = 0x60;
				break;
			case LPM_2NIT:
			default:
				set->cmds[idx].ss_txbuf[1] = 0x0B;
				set->cmds[idx].ss_txbuf[2] = 0xE0;
				break;
			}
		}

		idx = ss_get_cmd_idx(set, 0x19F, 0x98);
		if (min_fps == 30)
			set->cmds[idx].ss_txbuf[1] = 0x00;
		else	/* 1Hz */
			set->cmds[idx].ss_txbuf[1] = 0x01;
	} else {
		/* 0x98 - LPM BL Contrl */
		bl_idx = ss_get_cmd_idx(set, 0x19F, 0x98);
		if (bl_idx > 0) {
			memcpy(&set->cmds[bl_idx].ss_txbuf[2],
					&set_lpm_bl->cmds->ss_txbuf[1],
					sizeof(char) * (set_lpm_bl->cmds->msg.tx_len - 1));
		}

		aor_idx = ss_get_cmd_idx(set, 0x251, 0x98);
		if (aor_idx > 0) {
			memcpy(&set->cmds[aor_idx].ss_txbuf[1],
					&set_lpm_bl->cmds->ss_txbuf[1],
					sizeof(char) * (set_lpm_bl->cmds->msg.tx_len - 1));
		}
	}

	/* send lpm bl cmd */
	ss_send_cmd(vdd, TX_LPM_BL_CMD);

	vdd->br_info.last_br_is_hbm = false;

	LCD_INFO(vdd, "[Panel LPM] bl_level : %s, LFD: %uhz(%u)~%uhz(%u)\n",
			/* Check current brightness level */
			vdd->panel_lpm.lpm_bl_level == LPM_2NIT ? "2NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_10NIT ? "10NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_30NIT ? "30NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_60NIT ? "60NIT" : "UNKNOWN",
			min_fps, min_div, max_fps, max_div);
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
	int idx;
	int gm2_wrdisbv;
	u32 min_div, max_div;
	int min_fps, max_fps;
	struct vrr_info *vrr = &vdd->vrr;

	if (enable) {
		set_lpm = ss_get_cmds(vdd, TX_LPM_ON);
	} else {
		set_lpm = ss_get_cmds(vdd, TX_LPM_OFF);
	}

	if (SS_IS_CMDS_NULL(set_lpm)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_ON/OFF\n");
		return;
	}

	min_fps = max_fps = min_div = max_div = 0;

	if (ss_get_lfd_div(vdd, LFD_SCOPE_LPM, &min_div, &max_div)) {
		LCD_ERR(vdd, "fail to get LFD divider.. set default 1..\n");
		max_div = min_div = 1;
	}

	min_fps = DIV_ROUND_UP(vrr->lfd.base_rr, min_div);
	max_fps = DIV_ROUND_UP(vrr->lfd.base_rr, max_div);

	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_60NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
		gm2_wrdisbv = 247;
		break;
	case LPM_30NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
		gm2_wrdisbv = 123;
		break;
	case LPM_10NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
		gm2_wrdisbv = 42;
		break;
	case LPM_2NIT:
	default:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
		gm2_wrdisbv = 8;
		break;
	}

	if (enable) {
		/* HAE W/A : Do not set 60nit before lpm display on */
		if (vdd->panel_state != PANEL_PWR_LPM) {
			LCD_ERR(vdd, "Do not set AOD 60nit for LPM ON seq..\n");
			set_lpm_bl = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
			gm2_wrdisbv = 8;
		}

		if (SS_IS_CMDS_NULL(set_lpm_bl)) {
			LCD_ERR(vdd, "No cmds for lpm_bl..\n");
			return;
		}

		if (vdd->panel_revision + 'A' >= 'C') {
			idx = ss_get_cmd_idx(set_lpm, 0x4E, 0xBD);
			if (min_fps == 30)
				memcpy(&set_lpm->cmds[idx].ss_txbuf[1], LPM_FREQ[LPM_30HZ], sizeof(LPM_FREQ[0]) / sizeof(u8));
			else
				memcpy(&set_lpm->cmds[idx].ss_txbuf[1], LPM_FREQ[LPM_1HZ], sizeof(LPM_FREQ[0]) / sizeof(u8));

			idx = ss_get_cmd_idx(set_lpm, 0x17, 0xBD);
			if (min_fps == 30)
				memcpy(&set_lpm->cmds[idx].ss_txbuf[1], LPM_FREQ[LPM_30HZ], sizeof(LPM_FREQ[0]) / sizeof(u8));
			else
				memcpy(&set_lpm->cmds[idx].ss_txbuf[1], LPM_FREQ[LPM_1HZ], sizeof(LPM_FREQ[0]) / sizeof(u8));

			idx = ss_get_cmd_idx(set_lpm, 0x20F, 0xCB);
			if (min_fps == 30)
				memcpy(&set_lpm->cmds[idx].ss_txbuf[1], LPM_LTPS[LPM_30HZ], sizeof(LPM_LTPS[0]) / sizeof(u8));
			else
				memcpy(&set_lpm->cmds[idx].ss_txbuf[1], LPM_LTPS[LPM_1HZ], sizeof(LPM_LTPS[0]) / sizeof(u8));

			idx = ss_get_cmd_idx(set_lpm, 0x00, 0x51);
			if (min_fps == 1) {
				set_lpm->cmds[idx].ss_txbuf[1] = 0x07;
				set_lpm->cmds[idx].ss_txbuf[2] = 0xFF;
			} else {	/* 30Hz */
				switch (vdd->panel_lpm.lpm_bl_level) {
				case LPM_60NIT:
					set_lpm->cmds[idx].ss_txbuf[1] = 0x07;
					set_lpm->cmds[idx].ss_txbuf[2] = 0xFF;
					break;
				case LPM_30NIT:
					set_lpm->cmds[idx].ss_txbuf[1] = 0x03;
					set_lpm->cmds[idx].ss_txbuf[2] = 0xFF;
					break;
				case LPM_10NIT:
					set_lpm->cmds[idx].ss_txbuf[1] = 0x01;
					set_lpm->cmds[idx].ss_txbuf[2] = 0x55;
					break;
				case LPM_2NIT:
				default:
					set_lpm->cmds[idx].ss_txbuf[1] = 0x00;
					set_lpm->cmds[idx].ss_txbuf[2] = 0x44;
					break;
				}
			}

			idx = ss_get_cmd_idx(set_lpm, 0x1A0, 0x98);
			if (min_fps == 30) {
				set_lpm->cmds[idx].ss_txbuf[1] = 0x01;
				set_lpm->cmds[idx].ss_txbuf[2] = 0x00;
			} else { /* 1hz */
				switch (vdd->panel_lpm.lpm_bl_level) {
				case LPM_60NIT:
					set_lpm->cmds[idx].ss_txbuf[1] = 0x01;
					set_lpm->cmds[idx].ss_txbuf[2] = 0x00;
					break;
				case LPM_30NIT:
					set_lpm->cmds[idx].ss_txbuf[1] = 0x06;
					set_lpm->cmds[idx].ss_txbuf[2] = 0xA0;
					break;
				case LPM_10NIT:
					set_lpm->cmds[idx].ss_txbuf[1] = 0x0A;
					set_lpm->cmds[idx].ss_txbuf[2] = 0x60;
					break;
				case LPM_2NIT:
				default:
					set_lpm->cmds[idx].ss_txbuf[1] = 0x0B;
					set_lpm->cmds[idx].ss_txbuf[2] = 0xE0;
					break;
				}
			}

			idx = ss_get_cmd_idx(set_lpm, 0x19F, 0x98);
			if (min_fps == 30)
				set_lpm->cmds[idx].ss_txbuf[1] = 0x00;
			else	/* 1Hz */
				set_lpm->cmds[idx].ss_txbuf[1] = 0x01;
		} else {
			idx = ss_get_cmd_idx(set_lpm, 0x19F, 0x98);
			if (idx > 0)
				memcpy(&set_lpm->cmds[idx].ss_txbuf[2],
						&set_lpm_bl->cmds->ss_txbuf[1],
						sizeof(char) * (set_lpm_bl->cmds->msg.tx_len - 1));

			idx = ss_get_cmd_idx(set_lpm, 0x251, 0x98);
			if (idx > 0)
				memcpy(&set_lpm->cmds[idx].ss_txbuf[1],
						&set_lpm_bl->cmds->ss_txbuf[1],
						sizeof(char) * (set_lpm_bl->cmds->msg.tx_len - 1));
		}
	} else {
		idx = ss_get_cmd_idx(set_lpm, 0x00, 0x51);
		set_lpm->cmds[idx].ss_txbuf[1] = (gm2_wrdisbv & 0xFF00) >> 8;
		set_lpm->cmds[idx].ss_txbuf[2] = gm2_wrdisbv & 0xFF;
	}

	/* 1frame delay is needed after display_on(29h) (P211207-05270) */
	vdd->display_on_post_delay = 34;

	LCD_INFO(vdd, "[Panel LPM] [%s] : %s, LFD: %uhz(%u)~%uhz(%u)\n",
			enable ? "ON" : "OFF",
			vdd->panel_lpm.lpm_bl_level == LPM_2NIT ? "2NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_10NIT ? "10NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_30NIT ? "30NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_60NIT ? "60NIT" : "UNKNOWN",
			min_fps, min_div, max_fps, max_div);

	return;
}

static int ss_ecc_read(struct samsung_display_driver_data *vdd)
{
	u8 ecc[3] = {0, };
	bool pass = false;

	ss_send_cmd_get_rx(vdd, RX_GCT_ECC, ecc);

	if ((ecc[0] == 0x00) && (ecc[1] == 0x00) && (ecc[2] == 0x00))
		pass = true;
	else
		pass = false;

	LCD_INFO(vdd, "ECC = 0x%02X 0x%02X 0x%02X -> %s\n", ecc[0], ecc[1], ecc[2], pass ? "ECC_PASS" : "ECC_FAIL");

	return pass;
}

static int ss_ssr_read(struct samsung_display_driver_data *vdd)
{
	u8 ssr[2] = {0, };
	bool pass = false;

	ss_send_cmd_get_rx(vdd, RX_SSR, ssr);

	if ((ssr[0] == 0xC4) && ((ssr[1] & 0xF) == 0x0))
		pass = true;
	else
		pass = false;

	LCD_INFO(vdd, "SSR = 0x%02X 0x%02X -> %s\n", ssr[0], ssr[1], pass ? "SSR_PASS" : "SSR_FAIL");

	return pass;
}

static int ss_self_display_data_init(struct samsung_display_driver_data *vdd)
{
	u32 panel_type = 0;
	u32 panel_color = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->self_disp.is_support) {
		LCD_ERR(vdd, "Self Display is not supported\n");
		return -EINVAL;
	}

	LCD_INFO(vdd, "Self Display Panel Data init\n");

	panel_type = (ss_panel_id0_get(vdd) & 0x30) >> 4;
	panel_color = ss_panel_id0_get(vdd) & 0xF;

	LCD_INFO(vdd, "Panel Type=0x%x, Panel Color=0x%x\n", panel_type, panel_color);

	vdd->self_disp.operation[FLAG_SELF_MASK].img_buf = self_mask_img_data;
	vdd->self_disp.operation[FLAG_SELF_MASK].img_size = ARRAY_SIZE(self_mask_img_data);
	vdd->self_disp.operation[FLAG_SELF_MASK].img_checksum = SELF_MASK_IMG_CHECKSUM;
	make_mass_self_display_img_cmds_HAE(vdd, TX_SELF_MASK_IMAGE, FLAG_SELF_MASK);

	if (vdd->is_factory_mode) {
		vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_buf = self_mask_img_crc_data;
		vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_size = ARRAY_SIZE(self_mask_img_crc_data);
		make_mass_self_display_img_cmds_HAE(vdd, TX_SELF_MASK_IMAGE_CRC, FLAG_SELF_MASK_CRC);
	}

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

	if (vdd->is_factory_mode) {
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
	}

	return true;
}

static void ss_copr_panel_init(struct samsung_display_driver_data *vdd)
{
	vdd->copr.ver = COPR_VER_5P0;
	vdd->copr.display_read = 0;
	ss_copr_init(vdd);
}

static int ss_brightness_tot_level(struct samsung_display_driver_data *vdd)
{
	int tot_level_key = LEVEL0_KEY | LEVEL1_KEY | LEVEL2_KEY;
	return tot_level_key;
}

static int samsung_panel_off_pre(struct samsung_display_driver_data *vdd)
{
	int rc = 0;
	return rc;
}

static int samsung_panel_off_post(struct samsung_display_driver_data *vdd)
{
	int rc = 0;
	return rc;
}

static int ss_vrr_init(struct vrr_info *vrr)
{
	struct lfd_mngr *mngr;
	int i, scope;
	struct samsung_display_driver_data *vdd =
		container_of(vrr, struct samsung_display_driver_data, vrr);

	LCD_INFO(vdd, "+++\n");

//	mutex_init(&vrr->lfd.lfd_lock);
	mutex_init(&vrr->vrr_lock);
	mutex_init(&vrr->brr_lock);

	vrr->running_vrr_mdp = false;
	vrr->running_vrr = false;

	/* initial value : Bootloader: 120HS */
	vrr->prev_refresh_rate = vrr->cur_refresh_rate = vrr->adjusted_refresh_rate = 120;
	vrr->prev_sot_hs_mode = vrr->cur_sot_hs_mode = vrr->adjusted_sot_hs_mode = true;
	vrr->prev_phs_mode = vrr->cur_phs_mode = vrr->adjusted_phs_mode = false;

	vrr->hs_nm_seq = HS_NM_OFF;
	vrr->delayed_perf_normal = false;
	vrr->skip_vrr_in_brightness = false;

	vrr->brr_mode = BRR_OFF_MODE;
	vrr->brr_rewind_on = false;

	vrr->vrr_workqueue = create_singlethread_workqueue("vrr_workqueue");
	INIT_WORK(&vrr->vrr_work, ss_panel_vrr_switch_work);

	/* LFD mode */
	for (i = 0, mngr = &vrr->lfd.lfd_mngr[i]; i < LFD_CLIENT_MAX; i++, mngr++) {
		for (scope = 0; scope < LFD_SCOPE_MAX; scope++) {
			mngr->fix[scope] = LFD_FUNC_FIX_OFF;
			mngr->scalability[scope] = LFD_FUNC_SCALABILITY0;
			mngr->min[scope] = LFD_FUNC_MIN_CLEAR;
			mngr->max[scope] = LFD_FUNC_MAX_CLEAR;
		}
	}

#if IS_ENABLED(CONFIG_SEC_FACTORY)
	mngr = &vrr->lfd.lfd_mngr[LFD_CLIENT_FAC];
	mngr->fix[LFD_SCOPE_NORMAL] = LFD_FUNC_FIX_HIGH;
	mngr->fix[LFD_SCOPE_LPM] = LFD_FUNC_FIX_HIGH;
	mngr->fix[LFD_SCOPE_HMD] = LFD_FUNC_FIX_HIGH;
#endif

	/* TE modulation */
	vrr->te_mod_on = 0;
	vrr->te_mod_divider = 0;
	vrr->te_mod_cnt = 0;

	LCD_INFO(vdd, "---\n");
	return 0;
}


static bool ss_check_support_mode(struct samsung_display_driver_data *vdd, enum CHECK_SUPPORT_MODE mode)
{
	bool is_support = true;
	int cur_rr = vdd->vrr.cur_refresh_rate;
	bool cur_hs = vdd->vrr.cur_sot_hs_mode;

	switch (mode) {

	case CHECK_SUPPORT_BRIGHTDOT:
		if (!(cur_rr == 120 && cur_hs)) {
			is_support = false;
			LCD_ERR(vdd, "BRIGHT DOT fail: supported on 120HS(cur: %d%s)\n",
					cur_rr, cur_hs ? "HS" : "NS");
		}
		break;

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

#if 0
static int ss_read_udc_data(struct samsung_display_driver_data *vdd)
{
	int i = 0;
	u8 *read_buf;
	u32 fsize = 0xC00000 | vdd->udc.size;

	read_buf = kzalloc(vdd->udc.size, GFP_KERNEL);

	ss_send_cmd(vdd, TX_POC_ENABLE);

	ss_send_cmd(vdd, TX_FLASH_GAMMA_PRE1);

	LCD_INFO(vdd, "[UDC] start_addr : %X, size : %X\n", vdd->udc.start_addr, vdd->udc.size);

	flash_read_bytes(vdd, vdd->udc.start_addr, fsize, vdd->udc.size, read_buf);
	memcpy(vdd->udc.data, read_buf, vdd->udc.size);

	ss_send_cmd(vdd, TX_FLASH_GAMMA_POST);

	ss_send_cmd(vdd, TX_POC_DISABLE);

	for (i = 0; i < vdd->udc.size/2; i++) {
		LCD_INFO(vdd, "[%d] %X.%X\n", i, vdd->udc.data[(i*2)+0], vdd->udc.data[(i*2)+1]);
	}

	vdd->udc.read_done = true;

	return 0;
}
#endif

static void ss_read_flash(struct samsung_display_driver_data *vdd, u32 raddr, u32 rsize, u8 *rbuf)
{
	char showbuf[256];
	int i, pos = 0;

	if (rsize > 0x100) {
		LCD_ERR(vdd, "rsize(%x) is not supported..\n", rsize);
		return;
	}

	ss_send_cmd(vdd, TX_POC_ENABLE);
	spsram_read_bytes(vdd, raddr, rsize, rbuf);
	ss_send_cmd(vdd, TX_POC_DISABLE);

	for (i = 0; i < rsize; i++)
		pos += scnprintf(showbuf + pos, 256 - pos, "%02x ", rbuf[i]);
	LCD_INFO(vdd, "buf : %s\n", showbuf);

	return;
}

/* mtp original data */
static u8 HS120_R_TYPE_BUF[GAMMA_SET_MAX][GAMMA_R_SIZE];
/* mtp original data - V type */
static int HS120_V_TYPE_BUF[GAMMA_SET_MAX][GAMMA_V_SIZE];

/* HS96 compensated data */
static u8 HS96_R_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_R_SIZE];
/* HS96 compensated data V type*/
static int HS96_V_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_V_SIZE];

/* HS48 compensated data */
//static u8 HS48_R_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_R_SIZE];
/* HS48 compensated data V type*/
//static int HS48_V_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_V_SIZE];

static void ss_print_gamma_comp(struct samsung_display_driver_data *vdd)
{
	char pBuffer[256];
	int i, j;

	LCD_INFO(vdd, "== HS120_R_TYPE_BUF ==\n");
	for (i = 0; i < GAMMA_SET_MAX; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_R_TYPE_BUF[i][j]);
		LCD_INFO(vdd, "READ_R SET[%2d] : %s\n", GAMMA_SET_MAX - 1 - i, pBuffer);
	}

	LCD_INFO(vdd, "== HS120_V_TYPE_BUF ==\n");
	for (i = 0; i < GAMMA_SET_MAX; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_V_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_V_TYPE_BUF[i][j]);
		LCD_INFO(vdd, "READ_V SET[%2d] : %s\n", GAMMA_SET_MAX - 1 - i, pBuffer);
	}

	LCD_INFO(vdd, "== HS96_V_TYPE_COMP ==\n");
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_V_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS96_V_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP_V LV[%3d] : %s\n", i, pBuffer);
	}

	LCD_INFO(vdd, "== HS96_R_TYPE_COMP ==\n");
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS96_R_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP_R LV[%3d] : %s\n", i, pBuffer);
	}
}

#define FLASH_READ_SIZE (GAMMA_R_SIZE * 3)

static int ss_gm2_gamma_comp_init(struct samsung_display_driver_data *vdd)
{
	u8 readbuf[FLASH_READ_SIZE];
	int i, j, m;
	int val, *v_val;

	LCD_INFO(vdd, "++\n");

	memcpy(GLUT_OFFSET_48_96HS_V, GLUT_OFFSET_48_96HS_V_revA, sizeof(GLUT_OFFSET_48_96HS_V));

	memcpy(aor_table_96, aor_table_96_revA, sizeof(aor_table_96));
	memcpy(aor_table_120, aor_table_120_revA, sizeof(aor_table_120));

	// use GLUT offset only now.
	return 0;

	vdd->debug_data->print_cmds = true;

	// need to off self mask before read 6Eh
	ss_send_cmd(vdd, TX_SELF_MASK_OFF);
	usleep_range(10000, 10000);

	/********************************************/
	/* 1.  Read HS120 ORIGINAL GAMMA Flash      */
	/********************************************/
	ss_send_cmd(vdd, TX_POC_ENABLE);

	// total 70byte x 12set = 840 byte read 0x0690 (1680) ~ 0x09D7 (2519) */
	for (i = 0; i < GAMMA_SET_MAX; i+=3) {
		LCD_INFO(vdd, "start_addr : %X, size : %d\n", GAMMA_SET_ADDR_TABLE[i], FLASH_READ_SIZE);
		spsram_read_bytes(vdd, GAMMA_SET_ADDR_TABLE[i], FLASH_READ_SIZE, readbuf);
		memcpy(HS120_R_TYPE_BUF[i], readbuf, GAMMA_R_SIZE);
		memcpy(HS120_R_TYPE_BUF[i+1], readbuf + (GAMMA_R_SIZE), GAMMA_R_SIZE);
		memcpy(HS120_R_TYPE_BUF[i+2], readbuf + (GAMMA_R_SIZE * 2), GAMMA_R_SIZE);
	}

	ss_send_cmd(vdd, TX_POC_DISABLE);

	ss_send_cmd(vdd, TX_SELF_MASK_ON);

	vdd->debug_data->print_cmds = false;

	/***********************************************************/
	/* 2. translate Register type to V type            */
	/***********************************************************/
	for (i = 0; i < GAMMA_SET_MAX; i++) {
		m = 0;
		for (j = 0; j < GAMMA_R_SIZE; ) {
			HS120_V_TYPE_BUF[i][m++] = (GET_BITS(HS120_R_TYPE_BUF[i][j], 0, 3) << 8)
									| GET_BITS(HS120_R_TYPE_BUF[i][j+2], 0, 7);
			HS120_V_TYPE_BUF[i][m++] = (GET_BITS(HS120_R_TYPE_BUF[i][j+1], 4, 7) << 8)
									| GET_BITS(HS120_R_TYPE_BUF[i][j+3], 0, 7);
			HS120_V_TYPE_BUF[i][m++] = (GET_BITS(HS120_R_TYPE_BUF[i][j+1], 0, 3) << 8)
									| GET_BITS(HS120_R_TYPE_BUF[i][j+4], 0, 7);
			j += 5;
		}
	}

	/*************************************************************/
	/* 3. [ALL] Make HS96_V_TYPE_COMP (NORMAL + HBM) */
	/*************************************************************/

	/* 96HS */

	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		v_val = HS120_V_TYPE_BUF[GAMMA_SET_REGION_TABLE[i]];
		for (j = 0; j < GAMMA_V_SIZE; j++) {
			/* check underflow & overflow */
			if (v_val[j] + GAMMA_OFFSET_96HS_V[i][j] < 0) {
				HS96_V_TYPE_COMP[i][j] = 0;
			} else {
				if (j <= 5 || j >= (GAMMA_V_SIZE - 3)) {/* 1/13/14 th is 12bit(0xFFF) */
					val = v_val[j] + GAMMA_OFFSET_96HS_V[i][j];
					if (val > 0xFFF)	/* check overflow */
						HS96_V_TYPE_COMP[i][j] = 0xFFF;
					else
						HS96_V_TYPE_COMP[i][j] = val;
				} else {	/* 2 ~ 12th 11bit(0x7FF) */
					val = v_val[j] + GAMMA_OFFSET_96HS_V[i][j];
					if (val > 0x7FF)	/* check overflow */
						HS96_V_TYPE_COMP[i][j] = 0x7FF;
					else
						HS96_V_TYPE_COMP[i][j] = val;
				}
			}
		}
	}

	/******************************************************/
	/* 4. translate HS96_V_TYPE_COMP type to Register type*/
	/******************************************************/
	/* 96HS */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		m = 0; // GAMMA SET size
		for (j = 0; j < GAMMA_V_SIZE; j += RGB_MAX) {
			if (j == 0 || j == 3 || j == GAMMA_V_SIZE - 3) {
				HS96_R_TYPE_COMP[i][m++] = GET_BITS(HS96_V_TYPE_COMP[i][j+R], 8, 11);
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 8, 11) << 4)
											| (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 8, 11));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+R], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 0, 7));
			} else {	/* 1st ~ 10th */
				HS96_R_TYPE_COMP[i][m++] = GET_BITS(HS96_V_TYPE_COMP[i][j+R], 8, 10);
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 8, 10) << 4)
											| (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 8, 10));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+R], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 0, 7));
			}
		}
	}

	/* print all results */
//	ss_print_gamma_comp(vdd);

	LCD_INFO(vdd, " --\n");

	return 0;
}

/* add (GLUT / Analog Gamma) offset */
#if 0
static struct dsi_panel_cmd_set *ss_brightness_gm2_gamma_comp(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds = ss_get_cmds(vdd, TX_VRR_GM2_GAMMA_COMP);
	int idx, level, cur_rr, set;

	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_VRR_GM2_GAMMA_COMP.. \n");
		return NULL;
	}

	cur_rr = vdd->vrr.cur_refresh_rate;

	if (cur_rr == 120 || cur_rr == 96 || cur_rr == 48) {
		level = vdd->br_info.common_br.bl_level;
		set = GAMMA_SET_REGION_TABLE[level];

		idx = ss_get_cmd_idx(pcmds, 0x00, 0x71);
		pcmds->cmds[idx].ss_txbuf[2] = (GAMMA_SET_ADDR_TABLE[set] & 0xFF00) >> 8;
		pcmds->cmds[idx].ss_txbuf[3] = (GAMMA_SET_ADDR_TABLE[set] & 0xFF);

		idx = ss_get_cmd_idx(pcmds, 0x00, 0x6C);
		if (cur_rr == 120)
			memcpy(&pcmds->cmds[idx].ss_txbuf[1], &HS120_R_TYPE_BUF[set][0], GAMMA_R_SIZE);
		else
			memcpy(&pcmds->cmds[idx].ss_txbuf[1], &HS96_R_TYPE_COMP[level][0], GAMMA_R_SIZE);

		LCD_INFO(vdd, "cur_rr [%d], level [%d] set[%d] offset [%02x%02x]\n",
			cur_rr, level, set, pcmds->cmds[0].ss_txbuf[2], pcmds->cmds[0].ss_txbuf[3]);
		return pcmds;
	}

	return NULL;
}
#else
static struct dsi_panel_cmd_set *ss_brightness_gm2_gamma_comp(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds = ss_get_cmds(vdd, TX_VRR_GM2_GAMMA_COMP);
	int idx, level, cur_rr;

	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_VRR_GM2_GAMMA_COMP.. \n");
		return NULL;
	}

	cur_rr = vdd->vrr.cur_refresh_rate;
	level = vdd->br_info.common_br.bl_level;

	idx = ss_get_cmd_idx(pcmds, 0x309, 0x92);
	if (cur_rr == 96 || cur_rr == 48)
		memcpy(&pcmds->cmds[idx].ss_txbuf[1], GLUT_OFFSET_48_96HS_V[level], GLUT_SIZE);
	else
		return NULL;

	LCD_INFO(vdd, "cur_rr [%d], level [%d] \n",	cur_rr, level);
	return pcmds;
}
#endif

static void make_brightness_packet(struct samsung_display_driver_data *vdd,
	struct dsi_cmd_desc *packet, int *cmd_cnt, enum BR_TYPE br_type)
{
	int loop;
	int start = *cmd_cnt;

	if (br_type == BR_TYPE_NORMAL || br_type == BR_TYPE_HBM) {
		/* ACL */
		if (vdd->br_info.acl_status || vdd->siop_status)
			ss_add_brightness_packet(vdd, BR_FUNC_ACL_ON, packet, cmd_cnt);
		else
			ss_add_brightness_packet(vdd, BR_FUNC_ACL_OFF, packet, cmd_cnt);

		/* mAFPC */
		if (vdd->mafpc.is_support)
			ss_add_brightness_packet(vdd, BR_FUNC_MAFPC_SCALE, packet, cmd_cnt);

		/* gamma compensation (glut) */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA_COMP, packet, cmd_cnt);

		/* VRR */
		ss_add_brightness_packet(vdd, BR_FUNC_VRR, packet, cmd_cnt);

		/* gamma */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA, packet, cmd_cnt);
	} else if (br_type == BR_TYPE_HMT) {
		ss_add_brightness_packet(vdd, BR_FUNC_HMT_GAMMA, packet, cmd_cnt);
	} else {
		LCD_ERR(vdd, "undefined br_type (%d) \n", br_type);
	}

	for (loop = start ; loop < *cmd_cnt ; loop++) {
		if (packet[loop].ss_txbuf[0] == LEVEL_KEY_1 || packet[loop].ss_txbuf[0] == LEVEL_KEY_2 ||
			packet[loop].ss_txbuf[0] == LEVEL_KEY_3) {
			LCD_ERR(vdd, "Unexpected Level Key is added into Brightness Command[%d]\n", loop);
			*cmd_cnt = start;
		}
	}

	return;
}

void S6E3HAE_AMB681AZ01_WQHD_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "%s\n", ss_get_panel_name(vdd));

	/* Default Panel Power Status is OFF */
	vdd->panel_state = PANEL_PWR_OFF;

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = samsung_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = samsung_panel_on_post;
	vdd->panel_func.samsung_panel_off_pre = samsung_panel_off_pre;
	vdd->panel_func.samsung_panel_off_post = samsung_panel_off_post;
	vdd->panel_func.samsung_display_on_post = samsung_display_on_post;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = ss_panel_revision;
	vdd->panel_func.samsung_module_info_read = ss_module_info_read;
	vdd->panel_func.samsung_ddi_id_read = ss_ddi_id_read;
	vdd->panel_func.samsung_octa_id_read = ss_octa_id_read;
	vdd->panel_func.samsung_elvss_read = ss_elvss_read;

	/* Make brightness packer */
	vdd->panel_func.make_brightness_packet = make_brightness_packet;

	/* Brightness */
	vdd->panel_func.br_func[BR_FUNC_GAMMA] = ss_brightness_gamma;
	vdd->panel_func.br_func[BR_FUNC_ACL_ON] = ss_acl_on;
	vdd->panel_func.br_func[BR_FUNC_ACL_OFF] = ss_acl_off;
	vdd->panel_func.br_func[BR_FUNC_VRR] = ss_vrr;
	vdd->panel_func.br_func[BR_FUNC_GAMMA_COMP] = ss_brightness_gm2_gamma_comp;

	/* HMT */
	vdd->panel_func.br_func[BR_FUNC_HMT_GAMMA] = ss_brightness_gamma_mode2_hmt;

	/* Total level key in brightness */
	vdd->panel_func.samsung_brightness_tot_level = ss_brightness_tot_level;

	/* Panel LPM */
	vdd->panel_func.samsung_update_lpm_ctrl_cmd = ss_update_panel_lpm_ctrl_cmd;
	vdd->panel_func.samsung_set_lpm_brightness = ss_set_panel_lpm_brightness;

	/* Gray Spot Test */
	vdd->panel_func.samsung_gray_spot = ss_gray_spot;

	/* default brightness */
	vdd->br_info.common_br.bl_level = MAX_BL_PF_LEVEL;

	/* mdnie */
	vdd->mdnie.support_mdnie = true;
	vdd->mdnie.support_trans_dimming = true;
	vdd->mdnie.mdnie_tune_size[0] = sizeof(BYPASS_MDNIE_1);
	vdd->mdnie.mdnie_tune_size[1] = sizeof(BYPASS_MDNIE_2);
	vdd->mdnie.mdnie_tune_size[2] = sizeof(BYPASS_MDNIE_3);

	dsi_update_mdnie_data(vdd);

	/* COPR */
	vdd->copr.panel_init = ss_copr_panel_init;

	/* ACL default ON */
	vdd->br_info.acl_status = 1;

	/* Default br_info.temperature */
	vdd->br_info.temperature = 20;

	/* ACL default status in acl on */
	vdd->br_info.gradual_acl_val = 1;

	/* Gram Checksum Test */
	vdd->panel_func.samsung_gct_write = NULL;
	vdd->panel_func.samsung_gct_read = NULL;
	vdd->panel_func.ecc_read = ss_ecc_read;

	/* SSR Test */
	vdd->panel_func.ssr_read = ss_ssr_read;

	/* Self display */
	vdd->self_disp.factory_support = true;
	vdd->self_disp.init = self_display_init_HAE;
	vdd->self_disp.data_init = ss_self_display_data_init;

	/* mAPFC */
	vdd->mafpc.init = ss_mafpc_init_HAE;
	vdd->mafpc.data_init = ss_mafpc_data_init;

	/* FFC */
	vdd->panel_func.set_ffc = ss_ffc;

	/* DDI flash read for GM2 */
	vdd->panel_func.samsung_test_ddi_flash_check = NULL;

	/* Gamma compensation (Gamma Offset) */
	vdd->panel_func.samsung_gm2_gamma_comp_init = ss_gm2_gamma_comp_init;
	vdd->panel_func.samsung_print_gamma_comp = ss_print_gamma_comp;
	vdd->panel_func.read_flash = ss_read_flash;

	/* VRR */
	vdd->panel_func.samsung_lfd_get_base_val = ss_update_base_lfd_val;
	ss_vrr_init(&vdd->vrr);

	vdd->panel_func.samsung_check_support_mode = ss_check_support_mode;

	/* change  MIPI Drive strength values for this panel - request by HW group */
//	vdd->motto_info.motto_swing = 0xFF;

	LCD_INFO(vdd, "S6E3HAE_AMB681AZ01 : -- \n");
}
