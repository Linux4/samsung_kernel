/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
 *
 *	Author: js46.jeong
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
#include "B4_S6E3FAC_AMF670BS01_panel.h"
#include "B4_S6E3FAC_AMF670BS01_mdnie.h"

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

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{
	if (vdd->manufacture_id_dsi == PBA_ID)
		ss_panel_attach_set(vdd, false);
	else
		ss_panel_attach_set(vdd, true);

	switch (ss_panel_rev_get(vdd)) {
	case 0x0:
	case 0x1:
		vdd->panel_revision = 'A';
		break;
	case 0x2:
		vdd->panel_revision = 'B';
		break;
	case 0x3:
		vdd->panel_revision = 'D';
		break;
	case 0x4:
	case 0x5:
		vdd->panel_revision = 'E';
		break;
	default:
		vdd->panel_revision = 'E';
		LCD_ERR(vdd, "Invalid panel_rev(default rev : %c)\n", vdd->panel_revision);
		break;
	}

	vdd->panel_revision -= 'A';
	LCD_INFO_ONCE(vdd, "panel_revision = %c %d \n", vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

#if 0
29 00 00 00 00 00 04 B0 00 08 F2
29 00 00 00 00 00 03 F2 00 10		/* vfp normal 0x00 0x10 */
									/* wide porch 0x2 B0 */
29 00 00 00 00 00 02 60 00			/* OSC setting */
29 00 00 00 00 00 04 B0 00 1F BD
29 00 00 00 00 00 03 BD 00 00		/* HS LFD MAX(freq.) setting 60Hz */
									/* 120Hz 0x00 0x00, 60Hz 0x00 0x01, 40Hz 0x00 0x02, 30Hz 0x00 0x03 */
									/* 24Hz 0x00 0x04, 20Hz 0x00 0x05, 15Hz 0x00 0x07, 10Hz 0x00 0x0B, 5Hz 0x00 0x17, 1Hz 0x00 0x77 */
29 00 00 00 00 00 04 B0 00 4F BD
29 00 00 00 00 00 03 BD 00 00		/* NS LFD(freq.) setting 60Hz */
									/* 60Hz 0x00 0x00, 30Hz 0x00 0x01, 20Hz 0x00 0x02 */
									/* 15Hz 0x00 0x03, 10Hz 0x00 0x05, 5Hz 0x00 0x0B, 1Hz 0x00 0x3B */
29 00 00 00 00 00 04 B0 00 11 BD
29 00 00 00 00 00 02 B0 00
29 00 00 00 00 00 04 B0 00 14 BD	/* LFD MIN setting */
29 00 00 00 00 00 03 BD 00 00		/* 0x00, 0x00: 120Hz */
									/* 0x00, 0x01: 60Hz */
									/* 0x00, 0x03: 30Hz */
									/* 0x00, 0x0B: 10Hz */
									/* 0x00, 0x77: 1Hz */
29 00 00 00 00 00 04 B0 00 7B BD	/* VRR_LFD_FRAME_INSERT */
29 00 00 00 00 00 0E BD 00 00 01 00 03 00 0B 00 77 00 01 04 00

									/* HS : 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x0B, 0x00, 0x77 */
									/* NS : 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 */

29 00 00 00 00 00 04 B0 00 84 BD	/* VRR_LFD_FRAME_INSERT */
29 00 00 00 00 00 05 BD 00 01 04 00 /* 120Hz 2Frame, 60Hz 1Frame, 30Hz 4Frame (@120to10Hz) : 0x01, 0x04, 0x00 */
									/* 120Hz 2Frame, 60Hz 1Frame, 30Hz 2Frame, 10Hz 4Frame (@120to1Hz) : 0x01, 0x02, 0x04 */
									/* 60Hz 2Frame, 30Hz 0Frame : 0x02 0x00 */
29 00 00 00 00 00 02 F7 0F
29 00 00 00 00 00 02 BD E3
#endif
enum VRR_CMD_RR {
	VRR_10HS = 0,
	VRR_24HS,
	VRR_30HS,
	VRR_60NS,
	VRR_48HS,
	VRR_60HS,
	VRR_96HS,
	VRR_120HS,
	VRR_MAX
};
enum FRAME_INSERTION {
	CASE1 = 0,
	CASE2,
	CASE3,
	CASE4,
	CASE5,
	CASE6,
	CASE7,
	CASE8,
	CASE_NS,
	CASE_MAX
};
enum LFD_SET {
	LFD_120,
	LFD_96,
	LFD_60,
	LFD_48,
	LFD_30,
	LFD_24,
	LFD_10,
	LFD_1,
	LFD_MAX
};
static u8 FRAME_INSERT_TABLE[LFD_MAX][LFD_MAX] = { /* B0 00 08 F2 */
	{1, 1, 1, 2, 2, 2, 2, 4},
	{0, 1, 1, 2, 2, 2, 2, 4},
	{0, 0, 1, 3, 3, 3, 3, 5},
	{0, 0, 0, 3, 3, 3, 3, 5},
	{0, 0, 0, 0, 6, 6, 6, 6},
	{0, 0, 0, 0, 0, 7, 7, 7},
	{0, 0, 0, 0, 0, 0, 8, 8},
	{0, 0, 0, 0, 0, 0, 0, 8},
};

static u8 VRR_VFP[VRR_MAX][2] = { /* B0 00 08 F2 */
	{0x00, 0x20},/*VRR_10HS*/
	{0x00, 0x20},/*VRR_24HS*/
	{0x00, 0x20},/*VRR_30HS*/
	{0x00, 0x20},/*VRR_60NS*/
	{0x02, 0xC4},/*VRR_48HS*/
	{0x00, 0x20},/*VRR_60HS*/
	{0x02, 0xC4},/*VRR_96HS*/
	{0x00, 0x20},/*VRR_120HS*/
};

static u8 VRR_LFD_MAX[VRR_MAX][2] = { /* HS B0 00 1F BD, NS B0 00 4F BD */
	{0x00, 0x0B},/*VRR_10HS*/
	{0x00, 0x04},/*VRR_24HS*/
	{0x00, 0x03},/*VRR_30HS*/
	{0x00, 0x00},/*VRR_60NS*/
	{0x00, 0x01},/*VRR_48HS*/
	{0x00, 0x01},/*VRR_60HS*/
	{0x00, 0x00},/*VRR_96HS*/
	{0x00, 0x00},/*VRR_120HS*/
};

static u8 VRR_LFD_MIN[VRR_MAX][2] = { /* B0 00 14 BD */
	{0x00, 0x0B},/*VRR_10HS*/
	{0x00, 0x04},/*VRR_24HS*/
	{0x00, 0x03},/*VRR_30HS*/
	{0x00, 0x00},/*VRR_60NS*/
	{0x00, 0x01},/*VRR_48HS*/
	{0x00, 0x01},/*VRR_60HS*/
	{0x00, 0x00},/*VRR_96HS*/
	{0x00, 0x00},/*VRR_120HS*/
};

static u8 VRR_FRAME_INSERT[CASE_MAX][13] = { /* B0 00 7B BD */
	{0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x0B, 0x00, 0x0B, 0x02, 0x01, 0x04, 0x00},
	{0x01, 0x00, 0x0B, 0x00, 0x0B, 0x00, 0x0B, 0x00, 0x0B, 0x02, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x0B, 0x00, 0x77, 0x02, 0x01, 0x04, 0x06},
	{0x01, 0x00, 0x03, 0x00, 0x0B, 0x00, 0x77, 0x00, 0x77, 0x02, 0x01, 0x02, 0x00},
	{0x03, 0x00, 0x0B, 0x00, 0x77, 0x00, 0x77, 0x00, 0x77, 0x02, 0x04, 0x00, 0x00},
	{0x04, 0x00, 0x0E, 0x00, 0x77, 0x00, 0x77, 0x00, 0x77, 0x02, 0x04, 0x00, 0x00},
	{0x0B, 0x00, 0x77, 0x00, 0x77, 0x00, 0x77, 0x00, 0x77, 0x03, 0x00, 0x00, 0x00},
	{0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x02, 0x01, 0x00, 0x00},
};

static u8 VRR_FREQ[VRR_MAX][1] = {
	{0x00},/*VRR_10HS*/
	{0x00},/*VRR_24HS*/
	{0x00},/*VRR_30HS*/
	{0x18},/*VRR_60NS*/
	{0x00},/*VRR_48HS*/
	{0x00},/*VRR_60HS*/
	{0x00},/*VRR_96HS*/
	{0x00},/*VRR_120HS*/
};

static u8 TE_FRAME_SEL[VRR_MAX][7] = {
	{0x61, 0x00, 0x17, 0x05, 0x28, 0x00, 0x00},/*VRR_10HS*/
	{0x61, 0x00, 0x09, 0x05, 0x28, 0x00, 0x00},/*VRR_24HS*/
	{0x61, 0x00, 0x07, 0x05, 0x28, 0x00, 0x00},/*VRR_30HS*/
	{0x61, 0x00, 0x03, 0x02, 0x84, 0x00, 0x00},/*VRR_60NS*/
	{0x61, 0x00, 0x03, 0x03, 0xD6, 0x00, 0x00},/*VRR_48HS*/
	{0x61, 0x00, 0x03, 0x05, 0x28, 0x00, 0x00},/*VRR_60HS*/
	{0x61, 0x00, 0x01, 0x03, 0xD6, 0x00, 0x00},/*VRR_96HS*/
	{0x61, 0x00, 0x01, 0x05, 0x28, 0x00, 0x00},/*VRR_120HS*/
};

static u8 GLUT_EN_SETTING[VRR_MAX][1] = {
	{0x00},/*VRR_10HS*/
	{0x00},/*VRR_24HS*/
	{0x00},/*VRR_30HS*/
	{0x00},/*VRR_60NS*/
	{0x02},/*VRR_48HS*/
	{0x00},/*VRR_60HS*/
	{0x02},/*VRR_96HS*/
	{0x00},/*VRR_120HS*/
};

static u8 AOR_EN_SETTING[VRR_MAX][1] = {
	{0x00},/*VRR_10HS*/
	{0x00},/*VRR_24HS*/
	{0x00},/*VRR_30HS*/
	{0x00},/*VRR_60NS*/
	{0x01},/*VRR_48HS*/
	{0x00},/*VRR_60HS*/
	{0x01},/*VRR_96HS*/
	{0x00},/*VRR_120HS*/
};

static u8 SMOOTH_DIM_SETTING[VRR_MAX][1] = {
	{0x0F},/*VRR_10HS*/
	{0x0F},/*VRR_24HS*/
	{0x0F},/*VRR_30HS*/
	{0x0F},/*VRR_60NS*/
	{0x01},/*VRR_48HS*/
	{0x0F},/*VRR_60HS*/
	{0x01},/*VRR_96HS*/
	{0x0F},/*VRR_120HS*/
};

static enum VRR_CMD_RR ss_get_vrr_mode(struct samsung_display_driver_data *vdd)
{
	enum VRR_CMD_RR vrr_id;
	int cur_rr = vdd->vrr.cur_refresh_rate;
	int cur_hs = vdd->vrr.cur_sot_hs_mode;

	switch (cur_rr) {
	case 120:
		vrr_id = VRR_120HS;
		break;
	case 96:
		vrr_id = VRR_96HS;
		break;
	case 60:
		if (cur_hs)
			vrr_id = VRR_60HS;
		else
			vrr_id = VRR_60NS;
		break;
	case 48:
		vrr_id = VRR_48HS;
		break;
	case 30:
		vrr_id = VRR_30HS;
		break;
	case 24:
		vrr_id = VRR_24HS;
		break;
	case 10:
		vrr_id = VRR_10HS;
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

	vrr_id = ss_get_vrr_mode(vdd);

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
	case VRR_48HS:
		base_rr = 96;
		max_div_def = 2; // 48hz
		min_div_def = min_div_lowest = 96; // 1hz
		break;
	case VRR_60NS:
		base_rr = 60;
		max_div_def = 1; // 60hz
		min_div_def = min_div_lowest = 2; // 30hz
		break;
	case VRR_60HS:
		base_rr = 120;
		max_div_def = 2; // 60hz
		min_div_def = min_div_lowest = 120; // 1hz
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
	lfd_base->fix_div_def = 1; // LFD MAX/MIN 120hz fix

	vrr->lfd.base_rr = base_rr;

	LCD_DEBUG(vdd, "LFD(%s): base_rr: %uhz, def: %uhz(%u)~%uhz(%u), lowest: %uhz(%u)\n",
			lfd_scope_name[scope],
			base_rr,
			DIV_ROUND_UP(base_rr, min_div_def), min_div_def,
			DIV_ROUND_UP(base_rr, max_div_def), max_div_def,
			DIV_ROUND_UP(base_rr, min_div_lowest), min_div_lowest);

	return 0;
}

static enum LFD_SET ss_get_lfd_idx(struct samsung_display_driver_data *vdd, u32 freq)
{
	enum LFD_SET lfd_idx = 0;

		switch (freq) {
		case 120:
			lfd_idx = LFD_120;
			break;
		case 96:
			lfd_idx = LFD_96;
			break;
		case 60:
			lfd_idx = LFD_60;
			break;
		case 48:
		case 40:
			lfd_idx = LFD_48;
			break;
		case 30:
			lfd_idx = LFD_30;
			break;
		case 24:
		case 20:
			lfd_idx = LFD_24;
			break;
		case 10:
		case 5:
			lfd_idx = LFD_10;
			break;
		case 1:
			lfd_idx = LFD_1;
			break;
		default:
			LCD_ERR(vdd, "lfd_idx set default lfd 120HS..\n");
			lfd_idx = LFD_120;
			break;
		}
		return lfd_idx;
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

static void ss_get_frame_insert_cmd(struct samsung_display_driver_data *vdd,
	u8 *out_cmd, u32 base_rr, u32 min_div, u32 max_div)
{
	u32 min_freq = DIV_ROUND_UP(base_rr, min_div);
	u32 max_freq = DIV_ROUND_UP(base_rr, max_div);
	enum LFD_SET max_lfd_idx;
	enum LFD_SET min_lfd_idx;
	int frame_insert_idx;

	max_lfd_idx = ss_get_lfd_idx(vdd, max_freq);
	min_lfd_idx = ss_get_lfd_idx(vdd, min_freq);
	frame_insert_idx = FRAME_INSERT_TABLE[max_lfd_idx][min_lfd_idx] - 1;

	if (frame_insert_idx < 0)
		frame_insert_idx = 0;

	memcpy(out_cmd, &VRR_FRAME_INSERT[frame_insert_idx][0],
		sizeof(VRR_FRAME_INSERT[0]) / sizeof(u8));

	LCD_DEBUG(vdd, "min_freq = %d, max_freq = %d, frame_inser_idx = %d\n",
		min_freq, max_freq, frame_insert_idx);
}

static struct dsi_panel_cmd_set *ss_vrr(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct dsi_panel_cmd_set  *vrr_cmds = ss_get_cmds(vdd, TX_VRR);
	bool is_hbm = vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL ? true : false;
	struct vrr_info *vrr = &vdd->vrr;
	int cur_rr, prev_rr, base_rr;
	bool cur_hs, prev_hs;
	u32 min_div, max_div;
	enum LFD_SCOPE_ID scope;
	enum VRR_CMD_RR vrr_id;
	u8 cmd_frame_insert[13];
	int cmd_idx;
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

	cur_rr = vrr->cur_refresh_rate;
	cur_hs = vrr->cur_sot_hs_mode;
	min_div = max_div = 0;
	scope = LFD_SCOPE_NORMAL;
	vrr_id = ss_get_vrr_mode(vdd);

	if (!vrr->lfd.running_lfd && !vrr->running_vrr && !vdd->vrr.need_vrr_update) {
		LCD_INFO(vdd, "No vrr/lfd update lfd(%d) vrr(%d) need(%d)\n",
			vrr->lfd.running_lfd, vrr->running_vrr, vdd->vrr.need_vrr_update);
		return NULL;
	}

	/* LPTS update will apply after 1 vsync */
	delay = ss_frame_delay(base_rr, 1);
	cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0xF7);
	if (cmd_idx >= 0)
		vrr_cmds->cmds[cmd_idx].post_wait_ms = delay / 1000;
	LCD_DEBUG(vdd, "VRR delay %d\n", delay);

	/* VFP setting */
	cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x08, 0xF2);
	memcpy(&vrr_cmds->cmds[cmd_idx].ss_txbuf[1], &VRR_VFP[vrr_id][0],
		vrr_cmds->cmds[cmd_idx].msg.tx_len - 1);

	/* FREQ setting */
	cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0x60);
	memcpy(&vrr_cmds->cmds[cmd_idx].ss_txbuf[1], &VRR_FREQ[vrr_id][0],
		vrr_cmds->cmds[cmd_idx].msg.tx_len - 1);

	if (ss_get_lfd_div(vdd, scope, &min_div, &max_div)) {
		LCD_ERR(vdd, "fail to get LFD divider.. set default 1..\n");
		max_div = min_div = 1;
	}
	vrr->lfd.min_div = min_div;
	vrr->lfd.max_div = max_div;

	/* LFD MAX setting */
	if (cur_hs)
		cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x1F, 0xBD);
	else
		cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x4F, 0xBD);

	if (!vrr->cur_phs_mode) {/* set min/ max 120 for phs mode  */
		memcpy(&vrr_cmds->cmds[cmd_idx].ss_txbuf[1], &VRR_LFD_MAX[vrr_id][0],
			vrr_cmds->cmds[cmd_idx].msg.tx_len - 1);
		vrr_cmds->cmds[cmd_idx].ss_txbuf[2] = (max_div - 1);
	} else {
		memcpy(&vrr_cmds->cmds[cmd_idx].ss_txbuf[1], &VRR_LFD_MAX[VRR_120HS][0],
			vrr_cmds->cmds[cmd_idx].msg.tx_len - 1);
	}

	/* LFD MIN setting */
	cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x14, 0xBD);

	if (!vrr->cur_phs_mode) {/* set min/ max 120 for phs mode  */
		memcpy(&vrr_cmds->cmds[cmd_idx].ss_txbuf[1], &VRR_LFD_MIN[vrr_id][0],
			vrr_cmds->cmds[cmd_idx].msg.tx_len - 1);
		vrr_cmds->cmds[cmd_idx].ss_txbuf[2] = (min_div - 1);
	} else {
		memcpy(&vrr_cmds->cmds[cmd_idx].ss_txbuf[1], &VRR_LFD_MIN[VRR_120HS][0],
			vrr_cmds->cmds[cmd_idx].msg.tx_len - 1);
	}

	/* Frame insertion setting */
	if (cur_hs) {
		ss_get_frame_insert_cmd(vdd, cmd_frame_insert, vrr->lfd.base_rr, min_div, max_div);
	} else {
		memcpy(cmd_frame_insert, &VRR_FRAME_INSERT[CASE_NS][0], sizeof(VRR_FRAME_INSERT[0]) / sizeof(u8));
	}
	cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x7B, 0xBD);
	memcpy(&vrr_cmds->cmds[cmd_idx].ss_txbuf[1], &cmd_frame_insert[0],
		vrr_cmds->cmds[cmd_idx].msg.tx_len - 1);

	/* TE_FRAME_SEL */
	cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0xB9);
	memcpy(&vrr_cmds->cmds[cmd_idx].ss_txbuf[1], &TE_FRAME_SEL[vrr_id][0],
		vrr_cmds->cmds[cmd_idx].msg.tx_len - 1);
	cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x0C, 0xB9);
	memcpy(&vrr_cmds->cmds[cmd_idx].ss_txbuf[1], &TE_FRAME_SEL[vrr_id][3],
		vrr_cmds->cmds[cmd_idx].msg.tx_len - 1);

	/* GLUT Offset enable */
	cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x85, 0x93);
	memcpy(&vrr_cmds->cmds[cmd_idx].ss_txbuf[1], &GLUT_EN_SETTING[vrr_id][0],
		vrr_cmds->cmds[cmd_idx].msg.tx_len - 1);

	/* manual AOR enable */
	cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x4E, 0x98);
	memcpy(&vrr_cmds->cmds[cmd_idx].ss_txbuf[1], &AOR_EN_SETTING[vrr_id][0],
		vrr_cmds->cmds[cmd_idx].msg.tx_len - 1);

	/* SMOOTH_DIM enable */
	cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x0E, 0x94);
	memcpy(&vrr_cmds->cmds[cmd_idx].ss_txbuf[1], &SMOOTH_DIM_SETTING[vrr_id][0],
		vrr_cmds->cmds[cmd_idx].msg.tx_len - 1);


	LCD_INFO(vdd, "VRR: (prev: %d%s -> cur: %dx%d@%d%s) is_hbm:%d\n",
			prev_rr, prev_hs ? "HS" : "NM",
			panel->cur_mode->timing.h_active,
			panel->cur_mode->timing.v_active,
			cur_rr, cur_hs ? "HS" : "NM", is_hbm);

	return vrr_cmds;
}

static struct dsi_panel_cmd_set * ss_brightness_gamma(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int idx = 0;
	int cur_rr;
	bool dim_off = false;
	u8 irc_mode[2] = {0x65, 0x25};
	bool first_high_level = false;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	cur_rr = vdd->vrr.cur_refresh_rate;

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_NORMAL);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_INFO(vdd, "no gamma cmds\n");
		return NULL;
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

	/* IRC */
	idx = ss_get_cmd_idx(pcmds, 0x279, 0x92);
	memcpy(&pcmds->cmds[idx].ss_txbuf[1], &irc_mode[vdd->br_info.common_br.irc_mode], 1);


	if (!vdd->br_info.common_br.prev_bl_level && vdd->br_info.common_br.bl_level && vdd->display_on)
		first_high_level = true;

	/* smooth dim */
	if (!vdd->display_on || first_high_level)
		dim_off = true;
	else
		dim_off = false;

	/* smooth dim */
	idx = ss_get_cmd_idx(pcmds, 0x0D, 0x94);
	pcmds->cmds[idx].ss_txbuf[1] = dim_off ? 0x20 : 0x70;
	idx = ss_get_cmd_idx(pcmds, 0x00, 0x53);
	pcmds->cmds[idx].ss_txbuf[1] = dim_off ? 0x20 : 0x28;

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
#ifdef PROJECT_B2
static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	char ddi_id[5];
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (D6h 1~5th) for ddi id */
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
#else
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
#endif
#define FLASH_TEST_MODE_NUM     (1)
static int ss_test_ddi_flash_check(struct samsung_display_driver_data *vdd, char *buf)
{
	int len = 0;
	int ret = 0;
	char FLASH_LOADING[1];

	ss_panel_data_read(vdd, RX_FLASH_LOADING_CHECK, FLASH_LOADING, LEVEL1_KEY);

	if ((int)FLASH_LOADING[0] == 0)
		ret = 1;

	len += sprintf(buf + len, "%d\n", FLASH_TEST_MODE_NUM);
	len += sprintf(buf + len, "%d %02x000000\n",
			ret,
			(int)FLASH_LOADING[0]);

	return len;
}

#define COORDINATE_DATA_SIZE 6
#define MDNIE_SCR_WR_ADDR	0x32
#define RGB_INDEX_SIZE 4
#define COEFFICIENT_DATA_SIZE 8

#define F1(x, y) (((y << 10) - (((x << 10) * 56) / 55) - (102 << 10)) >> 10)
#define F2(x, y) (((y << 10) + (((x << 10) * 5) / 1) - (18483 << 10)) >> 10)

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
		idx = ss_get_cmd_idx(pcmds, 0x18, 0x94);
		if (idx < 0) {
			LCD_ERR(vdd, "fail to find C2h (off=0x14) cmd\n");
			return;
		}

		pcmds->cmds[idx].ss_txbuf[1] = vdd->br_info.common_br.elvss_value[0];
		LCD_INFO(vdd, "update C2h elvss(0x%x), idx: %d\n",
				vdd->br_info.common_br.elvss_value[0], idx);
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
	mdnie_data->dsi_afc_size = 45;
	mdnie_data->dsi_afc_index = 33;

	vdd->mdnie.mdnie_data = mdnie_data;

	return 0;
}
#ifdef PROJECT_B2
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
#else
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
#endif
#ifdef PROJECT_B2
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
#else
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
#endif
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

	idx = ss_get_cmd_idx(pcmds, 0x00, 0x55);
	if (vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL)
		if (vdd->br_info.gradual_acl_val == 3)
			pcmds->cmds[idx].ss_txbuf[1] = 0x02;	/* 0x03 : ACL 15% */
		else
			pcmds->cmds[idx].ss_txbuf[1] = 0x01;	/* 0x01 : ACL 8%, D.Lab Request(21.10.13) */
	else
		pcmds->cmds[idx].ss_txbuf[1] = 0x01;	/* 0x01 : ACL 8% */

	/* ACL 30% */
	if (vdd->br_info.gradual_acl_val == 2)
		pcmds->cmds[idx].ss_txbuf[1] = 0x03;	/* 0x03 : ACL 30% */

	LCD_DEBUG(vdd, "bl_level: %d, gradual_acl: %d, acl per: 0x%x", vdd->br_info.common_br.bl_level,
			vdd->br_info.gradual_acl_val, pcmds->cmds[0].ss_txbuf[1]);

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

static void ss_set_panel_lpm_brightness(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *set = ss_get_cmds(vdd, TX_LPM_BL_CMD);
	struct dsi_panel_cmd_set *set_lpm_bl;
	int set_idx, aor_pload_0, aor_pload_1;
	u32 min_div, max_div;
	int min_fps, max_fps;
	struct vrr_info *vrr = &vdd->vrr;

	if (SS_IS_CMDS_NULL(set)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_BL_CMD\n");
		return;
	}

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
	case LPM_1NIT:
	default:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_1NIT_CMD);
		break;
	}

	if (SS_IS_CMDS_NULL(set_lpm_bl)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_XXNIT_CMD\n");
		return;
	}

	/* 1. SET LFD */
	min_fps = max_fps = min_div = max_div = 0;

	if (ss_get_lfd_div(vdd, LFD_SCOPE_LPM, &min_div, &max_div)) {
		LCD_ERR(vdd, "fail to get LFD divider.. set default 1..\n");
		max_div = min_div = 1;
	}
	min_fps = DIV_ROUND_UP(vrr->lfd.base_rr, min_div);
	max_fps = DIV_ROUND_UP(vrr->lfd.base_rr, max_div);

	set_idx = ss_get_cmd_idx(set, 0x5F, 0xBD);
	if (min_fps == 30)
		set->cmds[set_idx].ss_txbuf[2] = 0x00;
	else
		set->cmds[set_idx].ss_txbuf[2] = 0x1D;

	set_idx = ss_get_cmd_idx(set, 0x1A, 0xBD);
	if (min_fps == 30)
		set->cmds[set_idx].ss_txbuf[2] = 0x00;
	else
		set->cmds[set_idx].ss_txbuf[2] = 0x1D;

	/* 2. SET AOR */
	aor_pload_0 = set_lpm_bl->cmds->ss_txbuf[1];
	aor_pload_1 = set_lpm_bl->cmds->ss_txbuf[2];

	set_idx = ss_get_cmd_idx(set, 0x4E, 0x98);
	set->cmds[set_idx].ss_txbuf[2] = aor_pload_0;
	set->cmds[set_idx].ss_txbuf[3] = aor_pload_1;

	set_idx = ss_get_cmd_idx(set, 0x13B, 0x98);
	set->cmds[set_idx].ss_txbuf[1] = aor_pload_0;
	set->cmds[set_idx].ss_txbuf[2] = aor_pload_1;

	/* send lpm bl cmd */
	ss_send_cmd(vdd, TX_LPM_BL_CMD);

	LCD_INFO(vdd, "[Panel LPM] bl_level : %s, min_fps = %d\n",
			/* Check current brightness level */
			vdd->panel_lpm.lpm_bl_level == LPM_2NIT ? "2NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_10NIT ? "10NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_30NIT ? "30NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_60NIT ? "60NIT" : "UNKNOWN", min_fps);
}

/*
 * This function will update parameters for LPM cmds
 * mode, brightness are updated here.
 */
static void ss_update_panel_lpm_ctrl_cmd
			(struct samsung_display_driver_data *vdd, int enable)
{
	struct dsi_panel_cmd_set *lpm_cmd;
	struct dsi_panel_cmd_set *lpm_bl_cmd;
	int lpm_ctrl_idx, aor_pload_0, aor_pload_1;
	int gm2_wrdisbv;
	u32 min_div, max_div;
	int min_fps, max_fps;
	struct vrr_info *vrr = &vdd->vrr;


	if (enable) {
		lpm_cmd = ss_get_cmds(vdd, TX_LPM_ON);
	} else {
		lpm_cmd = ss_get_cmds(vdd, TX_LPM_OFF);
	}

	if (SS_IS_CMDS_NULL(lpm_cmd)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_ON/OFF\n");
		return;
	}

	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_60NIT:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
		gm2_wrdisbv = 250;
		break;
	case LPM_30NIT:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
		gm2_wrdisbv = 125;
		break;
	case LPM_10NIT:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
		gm2_wrdisbv = 42;
		break;
	case LPM_1NIT:
	default:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_1NIT_CMD);
		gm2_wrdisbv = 6;
		break;
	}

	if (SS_IS_CMDS_NULL(lpm_bl_cmd)) {
		LCD_ERR(vdd, "No cmds for HLPM control brightness..\n");
		return;
	}

	if (enable) {
		/* 1. SET LFD */
		min_fps = max_fps = min_div = max_div = 0;

		if (ss_get_lfd_div(vdd, LFD_SCOPE_LPM, &min_div, &max_div)) {
			LCD_ERR(vdd, "fail to get LFD divider.. set default 1..\n");
			max_div = min_div = 1;
		}
		min_fps = DIV_ROUND_UP(vrr->lfd.base_rr, min_div);
		max_fps = DIV_ROUND_UP(vrr->lfd.base_rr, max_div);

		lpm_ctrl_idx = ss_get_cmd_idx(lpm_cmd, 0x1A, 0xBD);

		if (min_fps == 30)
			lpm_cmd->cmds[lpm_ctrl_idx].ss_txbuf[2] = 0x00;
		else
			lpm_cmd->cmds[lpm_ctrl_idx].ss_txbuf[2] = 0x1D;

		aor_pload_0 = lpm_bl_cmd->cmds->ss_txbuf[1];
		aor_pload_1 = lpm_bl_cmd->cmds->ss_txbuf[2];

		lpm_ctrl_idx = ss_get_cmd_idx(lpm_cmd, 0x4E, 0x98);
		lpm_cmd->cmds[lpm_ctrl_idx].ss_txbuf[2] = aor_pload_0;
		lpm_cmd->cmds[lpm_ctrl_idx].ss_txbuf[3] = aor_pload_1;

		lpm_ctrl_idx = ss_get_cmd_idx(lpm_cmd, 0x13B, 0x98);
		lpm_cmd->cmds[lpm_ctrl_idx].ss_txbuf[1] = aor_pload_0;
		lpm_cmd->cmds[lpm_ctrl_idx].ss_txbuf[2] = aor_pload_1;

	} else {
		lpm_ctrl_idx = ss_get_cmd_idx(lpm_cmd, 0x00, 0x51);
		lpm_cmd->cmds[lpm_ctrl_idx].ss_txbuf[1] = (gm2_wrdisbv & 0xFF00) >> 8;
		lpm_cmd->cmds[lpm_ctrl_idx].ss_txbuf[2] = gm2_wrdisbv & 0xFF;
	}
}

static int ss_gct_read(struct samsung_display_driver_data *vdd)
{
	u8 valid_checksum[4] = {0x5E, 0x5E, 0x5E, 0x5E};
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
	/* vddm set, normal : 0x00 / HV : 0x08 */
	u8 vddm_set[MAX_VDDM] = {0x00, 0x04, 0x08};
	int ret = 0;
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */
	struct dsi_panel_cmd_set *set;

	LCD_INFO(vdd, "+\n");

	set = ss_get_cmds(vdd, TX_GCT_ENTER);
	if (SS_IS_CMDS_NULL(set)) {
		LCD_ERR(vdd, "No cmds for TX_GCT_ENTER..\n");
		return ret;
	}

	vdd->gct.is_running = true;

#ifndef PROJECT_B2
	/* prevent sw reset to trigger esd recovery */
	LCD_INFO(vdd, "disable esd interrupt\n");

	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd, ESD_MASK_GCT_TEST);
#endif

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	for (i = TX_GCT_ENTER; i <= TX_GCT_EXIT; i++)
		ss_set_exclusive_tx_packet(vdd, i, 1);
	ss_set_exclusive_tx_packet(vdd, RX_GCT_CHECKSUM, 1);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 1);

	usleep_range(10000, 11000);

	idx = ss_get_cmd_idx(set, 0x02, 0xD7);

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
#ifndef PROJECT_B2
	/* enable esd interrupt */
	LCD_INFO(vdd, "enable esd interrupt\n");

	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(true, true, (void *)vdd, ESD_MASK_GCT_TEST);
#endif
	return ret;
}

static int ss_ecc_read(struct samsung_display_driver_data *vdd)
{
	u8 ecc[3] = {0, };
	bool pass = false;

	ss_panel_data_read(vdd, RX_GCT_ECC, ecc, LEVEL1_KEY);

	if ((ecc[0] == 0x00) && (ecc[1] == 0x00) && (ecc[2] == 0x00))
		pass = true;
	else
		pass = false;

	LCD_INFO(vdd, "ECC = 0x%02X 0x%02X 0x%02X -> %d\n", ecc[0], ecc[1], ecc[2], pass);

	return pass;
}

static int ss_ssr_read(struct samsung_display_driver_data *vdd)
{
	u8 ssr[2] = {0, };
	bool pass = false;

	ss_panel_data_read(vdd, RX_SSR_ON, &ssr[0], LEVEL1_KEY|LEVEL2_KEY);
	ss_panel_data_read(vdd, RX_SSR_CHECK, &ssr[1], LEVEL1_KEY|LEVEL2_KEY);

	if ((ssr[0] == 0x22) && ((ssr[1] & 0xF) == 0x0))
		pass = true;
	else
		pass = false;

	LCD_INFO(vdd, "SSR = 0x%02X 0x%02X -> %d\n", ssr[0], ssr[1], pass);

	return pass;
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
	make_self_dispaly_img_cmds_FAC(vdd, TX_SELF_MASK_IMAGE, FLAG_SELF_MASK);

	if (vdd->is_factory_mode) {
		vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_buf = self_mask_img_fhd_crc_data;
		vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_size = ARRAY_SIZE(self_mask_img_fhd_crc_data);
		make_mass_self_display_img_cmds_FAC(vdd, TX_SELF_MASK_IMAGE_CRC, FLAG_SELF_MASK_CRC);
	}

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
	ss_copr_init(vdd);
}

static int ss_brightness_tot_level(struct samsung_display_driver_data *vdd)
{
	int tot_level_key = 0;

	/* 3FAC VRR & brightness requires 9Fh and F0h and FCh level key unlocked */
	tot_level_key = LEVEL0_KEY | LEVEL1_KEY | LEVEL2_KEY;

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

	mutex_init(&vrr->vrr_lock);
	mutex_init(&vrr->brr_lock);

	vrr->running_vrr_mdp = false;
	vrr->running_vrr = false;

	/* defult: FHD@120hz HS mode */
	vrr->prev_refresh_rate = vrr->cur_refresh_rate = vrr->adjusted_refresh_rate = 120;
	vrr->prev_sot_hs_mode = vrr->cur_sot_hs_mode = vrr->adjusted_sot_hs_mode = true;
	vrr->prev_phs_mode = vrr->cur_phs_mode = vrr->adjusted_phs_mode = false;
	vrr->max_h_active_support_120hs = 1080; /* supports 120hz until FHD 1080 */

	vrr->hs_nm_seq = HS_NM_OFF;
	vrr->delayed_perf_normal = false;
	vrr->skip_vrr_in_brightness = false;

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

#define FFC_CMD_LEN (4)
#define FFC_START_IDX (3)
static int ss_update_ffc(struct samsung_display_driver_data *vdd, int idx)
{
	struct dsi_panel_cmd_set *ffc_set;
	struct dsi_panel_cmd_set *dyn_ffc_set;
	int loop, ffc_idx, dyn_ffc_idx;

	if (!vdd->dyn_mipi_clk.is_support) {
		LCD_INFO(vdd, "Dynamic MIPI Clock does not support\n");
		return -ENODEV;
	}

	ffc_set = ss_get_cmds(vdd, TX_FFC);
	dyn_ffc_set = ss_get_cmds(vdd, TX_DYNAMIC_FFC_SET);

	if (SS_IS_CMDS_NULL(ffc_set) || SS_IS_CMDS_NULL(dyn_ffc_set)) {
		LCD_ERR(vdd, "No cmds for TX_FFC..\n");
		return -EINVAL;
	}

	for (loop = 0; loop < FFC_CMD_LEN; loop++) {
		ffc_idx = FFC_START_IDX + loop;
		dyn_ffc_idx = (FFC_CMD_LEN * idx) + loop;

		if (ffc_set->cmds[ffc_idx].msg.tx_len ==
			dyn_ffc_set->cmds[dyn_ffc_idx].msg.tx_len) {
			memcpy(ffc_set->cmds[ffc_idx].ss_txbuf,
					dyn_ffc_set->cmds[dyn_ffc_idx].ss_txbuf,
					ffc_set->cmds[ffc_idx].msg.tx_len);
		} else {
			LCD_INFO(vdd, "[DISPLAY_%d] ffc cmd(%d) doesn't match\n", vdd->ndx, loop);
		}
	}

	LCD_INFO(vdd, "CLK IDX : %d\n", idx);

	return 0;
}

static int ss_set_ffc(struct samsung_display_driver_data *vdd, int idx)
{
	if (!vdd->dyn_mipi_clk.is_support) {
		LCD_INFO(vdd, "Dynamic MIPI Clock does not support\n");
		return -ENODEV;
	}

	LCD_INFO(vdd, "CLK IDX : %d ++\n", idx);

	ss_update_ffc(vdd, idx);

	ss_send_cmd(vdd, TX_FFC);

	LCD_INFO(vdd, "CLK IDX : %d --\n", idx);

	return 0;
}

static int ss_update_osc(struct samsung_display_driver_data *vdd, int idx)
{
	struct dsi_panel_cmd_set *osc_cmd;
	struct dsi_panel_cmd_set *dyn_osc_cmd;
	int loop, osc_idx, dyn_osc_idx;

	if (!vdd->dyn_mipi_clk.is_support) {
		LCD_INFO(vdd, "Dynamic MIPI Clock does not support\n");
		return -ENODEV;
	}

	if (!vdd->dyn_mipi_clk.osc_support) {
		LCD_INFO(vdd, "Dynamic OSC Clock does not support\n");
		return -ENODEV;
	}

	osc_cmd = ss_get_cmds(vdd, TX_OSC);
	dyn_osc_cmd = ss_get_cmds(vdd, TX_DYNAMIC_OSC_SET);

	if (SS_IS_CMDS_NULL(osc_cmd) || SS_IS_CMDS_NULL(dyn_osc_cmd)) {
		LCD_ERR(vdd, "No cmds for TX_OSC..\n");
		return -EINVAL;
	}

	for (loop = 0; loop < osc_cmd->count ; loop++) {
		osc_idx = loop;
		dyn_osc_idx = (osc_cmd->count * idx) + loop;

		if (osc_cmd->cmds[osc_idx].msg.tx_len ==
			dyn_osc_cmd->cmds[dyn_osc_idx].msg.tx_len) {
			memcpy(osc_cmd->cmds[osc_idx].ss_txbuf,
					dyn_osc_cmd->cmds[dyn_osc_idx].ss_txbuf,
					osc_cmd->cmds[osc_idx].msg.tx_len);
		} else {
			LCD_INFO(vdd, "[DISPLAY_%d] osc cmd(%d) doesn't match\n", vdd->ndx, loop);
		}
	}

	LCD_INFO(vdd, "OSC IDX : %d\n", idx);

	return 0;
}

static int ss_set_osc(struct samsung_display_driver_data *vdd, int idx)
{
	if (!vdd->dyn_mipi_clk.is_support) {
		LCD_INFO(vdd, "Dynamic MIPI Clock does not support\n");
		return -ENODEV;
	}

	if (!vdd->dyn_mipi_clk.osc_support) {
		LCD_INFO(vdd, "Dynamic OSC Clock does not support\n");
		return -ENODEV;
	}

	ss_update_osc(vdd, idx);

	/* OSC Cmd will be sent next panel OFF->ON in ss_panel_on_post() function */
	/*ss_send_cmd(vdd, TX_OSC);*/

	LCD_INFO(vdd, "OSC IDX : %d\n", idx);

	return 0;
}

static int ss_gm2_gamma_comp_init(struct samsung_display_driver_data *vdd)
{
	memcpy(GLUT_OFFSET_48_96HS_V, GLUT_OFFSET_48_96HS_V_revA, sizeof(GLUT_OFFSET_48_96HS_V));
	return 0;
}

static struct dsi_panel_cmd_set *ss_glut_offset(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int cur_rr, idx;
	int bl_level = vdd->br_info.common_br.bl_level;

	pcmds = ss_get_cmds(vdd, TX_GLUT_OFFSET);

	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_GLUT_OFFSET\n");
		return NULL;
	}

	cur_rr = vdd->vrr.cur_refresh_rate;

	/* Glut offset */
	if (cur_rr == 48 || cur_rr == 96) {
		idx = ss_get_cmd_idx(pcmds, 0xA1, 0x93);
		LCD_INFO(vdd, "cur_rr [%d], bl_level [%d]\n", cur_rr, bl_level);
		memcpy(&pcmds->cmds[idx].ss_txbuf[1], GLUT_OFFSET_48_96HS_V[bl_level], GLUT_SIZE);
	} else {
		return NULL;
	}

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_manual_aor(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int cur_rr, idx;
	int bl_level = vdd->br_info.common_br.bl_level;

	pcmds = ss_get_cmds(vdd, TX_MANUAL_AOR);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_MANUAL_AOR\n");
		return NULL;
	}

	cur_rr = vdd->vrr.cur_refresh_rate;

	/* Manual AOR enable */
	if (cur_rr == 48 || cur_rr == 96) {
		idx = ss_get_cmd_idx(pcmds, 0x4F, 0x98);
		LCD_INFO(vdd, "cur_rr [%d], bl_level [%d]\n", cur_rr, bl_level);
		pcmds->cmds[idx].ss_txbuf[1] = (aor_table_96_revA[bl_level] & 0xFF00) >> 8;
		pcmds->cmds[idx].ss_txbuf[2] = aor_table_96_revA[bl_level] & 0xFF;
	}

	return pcmds;
}

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

static void make_brightness_packet(struct samsung_display_driver_data *vdd,
	struct dsi_cmd_desc *packet, int *cmd_cnt, enum BR_TYPE br_type)
{
	int loop;
	int start = *cmd_cnt;

	if ((br_type == BR_TYPE_NORMAL) || (br_type == BR_TYPE_HBM)) {

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

		ss_add_brightness_packet(vdd, BR_FUNC_MANUAL_AOR, packet, cmd_cnt);

		ss_add_brightness_packet(vdd, BR_FUNC_GLUT_OFFSET, packet, cmd_cnt);

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

void B4_S6E3FAC_AMF670BS01_FHD_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "B4_S6E3FAC_AMF670BS01 : ++ \n");
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
	vdd->panel_func.br_func[BR_FUNC_GAMMA] = ss_brightness_gamma;
	vdd->panel_func.br_func[BR_FUNC_ACL_ON] = ss_acl_on;
	vdd->panel_func.br_func[BR_FUNC_ACL_OFF] = ss_acl_off;
	vdd->panel_func.br_func[BR_FUNC_VRR] = ss_vrr;
	vdd->panel_func.br_func[BR_FUNC_GLUT_OFFSET] = ss_glut_offset;
	vdd->panel_func.br_func[BR_FUNC_MANUAL_AOR] = ss_manual_aor;

	/* Total level key in brightness */
	vdd->panel_func.samsung_brightness_tot_level = ss_brightness_tot_level;

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
	vdd->mdnie.mdnie_tune_size[0] = sizeof(BYPASS_MDNIE_1);
	vdd->mdnie.mdnie_tune_size[1] = sizeof(BYPASS_MDNIE_2);
	vdd->mdnie.mdnie_tune_size[2] = sizeof(BYPASS_MDNIE_3);

	dsi_update_mdnie_data(vdd);
	/* Enable panic on first pingpong timeout */
	//vdd->debug_data->panic_on_pptimeout = true;

	/* COLOR WEAKNESS */
	vdd->panel_func.color_weakness_ccb_on_off =  NULL;

	/* COPR */
	vdd->copr.panel_init = ss_copr_panel_init;

	/* Gamma compensation (Gamma Offset) */
	vdd->panel_func.read_flash = ss_read_flash;
	vdd->panel_func.samsung_gm2_gamma_comp_init = ss_gm2_gamma_comp_init;

	/* ACL default ON */
	vdd->br_info.acl_status = 1;

	/* Default br_info.temperature */
	vdd->br_info.temperature = 20;

	/* ACL default status in acl on */
	vdd->br_info.gradual_acl_val = 1;

	/* LPM(AOD) Related Delay */
	vdd->panel_lpm.entry_frame = 2;
	vdd->panel_lpm.exit_frame = 2;

	/* Gram Checksum Test */
	vdd->panel_func.samsung_gct_write = ss_gct_write;
	vdd->panel_func.samsung_gct_read = ss_gct_read;
	vdd->panel_func.ecc_read = ss_ecc_read;

	/* SSR Test */
	vdd->panel_func.ssr_read = ss_ssr_read;

	/* Self display */
	vdd->self_disp.factory_support = true;
	vdd->self_disp.init = self_display_init_FAC;
	vdd->self_disp.data_init = ss_self_display_data_init;

	/* mAPFC */
	vdd->mafpc.init = ss_mafpc_init_FAC;
	vdd->mafpc.data_init = ss_mafpc_data_init;

	/* OSC */
	vdd->panel_func.update_osc = ss_update_osc;
	vdd->panel_func.set_osc = ss_set_osc;

	/* FFC */
	vdd->panel_func.set_ffc = ss_set_ffc;

	/* DDI flash read for GM2 */
	vdd->panel_func.samsung_test_ddi_flash_check = ss_test_ddi_flash_check;

	/* VRR */
	vdd->panel_func.samsung_lfd_get_base_val = ss_update_base_lfd_val;
	ss_vrr_init(&vdd->vrr);

	vdd->panel_func.samsung_check_support_mode = ss_check_support_mode;

	/* change  MIPI Drive strength values for this panel - request by HW group */
	vdd->motto_info.motto_swing = 0xFF;

	LCD_INFO(vdd, "B4_S6E3FAC_AMF670BS01 : -- \n");
}
