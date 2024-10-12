/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
 *
 *	Author: cj1225.jang
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
#include "ss_dsi_panel_S6E3FAC_AMB606AW01.h"
#include "ss_dsi_mdnie_S6E3FAC_AMB606AW01.h"

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

/* mtp original data R type */
static u8 HS120_R_TYPE_BUF[GAMMA_SET_MAX][GAMMA_R_SIZE];
static u8 HS60_R_TYPE_BUF[GAMMA_SET_MAX][GAMMA_R_SIZE];

/* mtp original data V type */
static int HS120_V_TYPE_BUF[GAMMA_SET_MAX][GAMMA_V_SIZE];
static int HS60_V_TYPE_BUF[GAMMA_SET_MAX][GAMMA_V_SIZE];

/* compensated data R type*/
static u8 HS96_R_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_R_SIZE];
static u8 HS60_R_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_R_SIZE];
static u8 HS48_R_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_R_SIZE];

/* compensated data V type*/
static int HS96_V_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_V_SIZE];
static int HS60_V_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_V_SIZE];
static int HS48_V_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_V_SIZE];

static void ss_read_gamma(struct samsung_display_driver_data *vdd);

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
	struct dsi_panel_cmd_set *pcmds;
	int idx;

	/* write analog offset for G10,G11 during on time. */
	/* analog offset is always same for all levels in each G region. */
	/* SET0,1 : 08A2(G10) + 08E3(G11) -> 65+65+1 byte */
	pcmds = ss_get_cmds(vdd, TX_VRR_GM2_GAMMA_COMP3);
	if (!SS_IS_CMDS_NULL(pcmds)) {
		idx = ss_get_cmd_idx(pcmds, 0x00, 0x6C);
		memcpy(&pcmds->cmds[idx].ss_txbuf[1], HS60_R_TYPE_COMP[255], GAMMA_R_SIZE);
		memcpy(&pcmds->cmds[idx].ss_txbuf[1 + GAMMA_R_SIZE], HS60_R_TYPE_COMP[510], GAMMA_R_SIZE);
		ss_send_cmd(vdd, TX_VRR_GM2_GAMMA_COMP3);
		LCD_INFO(vdd, "write 60HS gamma offset (73~510lv) \n");
		vdd->br_info.last_tx_time = ktime_get();
	}

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

static enum VRR_CMD_RR ss_get_vrr_mode(struct samsung_display_driver_data *vdd, int rr, bool hs, bool phs)
{
	enum VRR_CMD_RR vrr_id;
	switch (rr) {
	case 120:
		vrr_id = VRR_120HS;
		break;
	case 96:
		vrr_id = VRR_96HS;
		break;
	case 60:
		if (phs)
			vrr_id = VRR_60PHS;
		else if (hs)
			vrr_id = VRR_60HS;
		else
			vrr_id = VRR_60NS;
		break;
	case 48:
		vrr_id = (phs) ? VRR_48PHS : VRR_48HS;
		break;
	case 30:
		vrr_id = (phs) ? VRR_30PHS : VRR_30HS;
		break;
	case 24:
		vrr_id = (phs) ? VRR_24PHS : VRR_24HS;
		break;
	case 10:
		vrr_id = (phs) ? VRR_10PHS : VRR_10HS;
		break;
	default:
		LCD_ERR(vdd, "invalid refresh rate (%d, %d), set default 120HS..\n", rr, hs);
		vrr_id = VRR_120HS;
		break;
	}

	return vrr_id;
}

static enum VRR_CMD_RR ss_get_vrr_mode_base(struct samsung_display_driver_data *vdd, int rr, bool hs, bool phs)
{
	enum VRR_CMD_RR vrr_base;

	switch (rr) {
	case 120:
		vrr_base = VRR_120HS;
		break;
	case 96:
		vrr_base = VRR_96HS;
		break;
	case 60:
		if (phs)
			vrr_base = VRR_120HS;
		else if (hs)
			vrr_base = VRR_60HS;
		else
			vrr_base = VRR_60NS;
		break;
	case 48:
		vrr_base = (phs) ? VRR_96HS : VRR_48HS;
		break;
	case 30:
		vrr_base = (phs) ? VRR_120HS : VRR_60HS;
		break;
	case 24:
		vrr_base = (phs) ? VRR_120HS : VRR_48HS;
		break;
	case 10:
		vrr_base = (phs) ? VRR_120HS : VRR_48HS;
		break;
	default:
		LCD_ERR(vdd, "invalid refresh rate (%d, %d), set default 120HS..\n", rr, hs);
		vrr_base = VRR_120HS;
		break;
	}

	return vrr_base;
}

static struct dsi_panel_cmd_set *ss_vrr(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set  *vrr_cmds = ss_get_cmds(vdd, TX_VRR);
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct vrr_info *vrr = &vdd->vrr;
	enum VRR_CMD_RR cur_md;
	enum VRR_CMD_RR cur_md_base;
	bool is_hbm = vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL ? true : false;
	bool is_hmt = vdd->hmt.hmt_on ? true : false;
	int cur_rr, prev_rr;
	bool cur_hs, cur_phs, prev_hs, prev_phs;
	int idx = 0;
	int bl_level = vdd->br_info.common_br.bl_level;

	if (SS_IS_CMDS_NULL(vrr_cmds)) {
		LCD_INFO(vdd, "no vrr cmds\n");
		return NULL;
	}

	if (panel && panel->cur_mode) {
		LCD_DEBUG(vdd, "VRR(%d): cur_mode: %dx%d@%d%s, bl_level: %d, hbm: %d, hmt: %d\n",
				vrr->running_vrr,
				panel->cur_mode->timing.h_active,
				panel->cur_mode->timing.v_active,
				panel->cur_mode->timing.refresh_rate,
				panel->cur_mode->timing.sot_hs_mode ?
				(panel->cur_mode->timing.phs_mode ? "PHS" : "HS") : "NM",
				bl_level, is_hbm, is_hmt);

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

	cur_md = ss_get_vrr_mode(vdd, cur_rr, cur_hs, cur_phs);
	cur_md_base = ss_get_vrr_mode_base(vdd, cur_rr, cur_hs, cur_phs);

	LCD_DEBUG(vdd, "cur_md = %d, cur_md_base = %d\n", cur_md, cur_md_base);

	/* send vrr cmd in 1vsync when the image is not being transmitted */
	/* wait TE + pass image update (9ms), then send cmd */
	if (vdd->vrr.running_vrr) {
		if (cur_rr == 30) {
			ss_wait_for_te_gpio(vdd, 1, 0, false);
			usleep_range(9000, 9000);
			LCD_ERR(vdd, "DELAY 9ms!!\n");
		}
	}

	/* GLUT Offset */
	idx = ss_get_cmd_idx(vrr_cmds, 0x85, 0x93);
	if (idx >= 0) {
		if (cur_md_base == VRR_96HS || cur_md_base == VRR_48HS || cur_md_base == VRR_60HS)
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x02;	/* Enable */
		else
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x00;	/* Disable */
	}

	/* OSC */
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0xF2);
	if (idx >= 0) {
		if (cur_md_base == VRR_60HS || cur_md_base == VRR_48HS)
			vrr_cmds->cmds[idx].ss_txbuf[1] = 00;
		else
			vrr_cmds->cmds[idx].ss_txbuf[1] = 01;
	}

	/* FREQ */
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0x60);
	if (idx >= 0) {
		if (cur_md_base == VRR_60NS)
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x18;
		else if (cur_md_base == VRR_60HS || cur_md_base == VRR_48HS)
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x08;
		else
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x00;
	}

	/* TE_SEL */
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0xB9);
	if (idx >= 0) {
		if (cur_md == VRR_60HS || cur_md == VRR_48HS) {
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x04;
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[5] = 0x09;
			vrr_cmds->cmds[idx].ss_txbuf[6] = 0x23;
			vrr_cmds->cmds[idx].ss_txbuf[7] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[8] = 0x0F;
			vrr_cmds->cmds[idx].ss_txbuf[13] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[14] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[20] = 0;
		}  else if (cur_md == VRR_60PHS) {
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x61;
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x03;
			vrr_cmds->cmds[idx].ss_txbuf[5] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[6] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[7] = vrr_cmds->cmds[idx].ss_txbuf[8] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[13] = 0x04;
			vrr_cmds->cmds[idx].ss_txbuf[14] = 0x94;
			vrr_cmds->cmds[idx].ss_txbuf[20] = 0x10;
		} else if (cur_md == VRR_48PHS) {
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x61;
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x03;
			vrr_cmds->cmds[idx].ss_txbuf[5] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[6] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[7] = vrr_cmds->cmds[idx].ss_txbuf[8] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[13] = 0x03;
			vrr_cmds->cmds[idx].ss_txbuf[14] = 0x6C;
			vrr_cmds->cmds[idx].ss_txbuf[20] = 0x10;
		} else if (cur_md == VRR_30PHS) {
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x61;
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x07;
			vrr_cmds->cmds[idx].ss_txbuf[5] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[6] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[7] = vrr_cmds->cmds[idx].ss_txbuf[8] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[13] = 0x04;
			vrr_cmds->cmds[idx].ss_txbuf[14] = 0x94;
			vrr_cmds->cmds[idx].ss_txbuf[20] = 0x10;
		} else if (cur_md == VRR_30HS || cur_md == VRR_24HS) {
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x61;
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x07;
			vrr_cmds->cmds[idx].ss_txbuf[5] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[6] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[7] = vrr_cmds->cmds[idx].ss_txbuf[8] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[13] = 0x00;
			vrr_cmds->cmds[idx].ss_txbuf[14] = 0x11;
			vrr_cmds->cmds[idx].ss_txbuf[20] = 0x10;
		} else if (cur_md == VRR_24PHS) {
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x61;
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x09;
			vrr_cmds->cmds[idx].ss_txbuf[5] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[6] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[7] = vrr_cmds->cmds[idx].ss_txbuf[8] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[13] = 0x04;
			vrr_cmds->cmds[idx].ss_txbuf[14] = 0x94;
			vrr_cmds->cmds[idx].ss_txbuf[20] = 0x10;
		} else if (cur_md == VRR_10HS) {
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x61;
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x13;
			vrr_cmds->cmds[idx].ss_txbuf[5] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[6] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[7] = vrr_cmds->cmds[idx].ss_txbuf[8] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[13] = 0x00;
			vrr_cmds->cmds[idx].ss_txbuf[14] = 0x11;
			vrr_cmds->cmds[idx].ss_txbuf[20] = 0x10;
		} else if (cur_md == VRR_10PHS) {
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x61;
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x17;
			vrr_cmds->cmds[idx].ss_txbuf[5] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[6] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[7] = vrr_cmds->cmds[idx].ss_txbuf[8] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[13] = 0x04;
			vrr_cmds->cmds[idx].ss_txbuf[14] = 0x94;
			vrr_cmds->cmds[idx].ss_txbuf[20] = 0x10;
		} else if (cur_md == VRR_60NS) {
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x61;
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x03;
			vrr_cmds->cmds[idx].ss_txbuf[5] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[6] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[7] = vrr_cmds->cmds[idx].ss_txbuf[8] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[13] = 0x02;
			vrr_cmds->cmds[idx].ss_txbuf[14] = 0x44;
			vrr_cmds->cmds[idx].ss_txbuf[20] = 0x10;
		} else if (cur_md == VRR_96HS) {
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x61;
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[5] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[6] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[7] = vrr_cmds->cmds[idx].ss_txbuf[8] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[13] = 0x03;
			vrr_cmds->cmds[idx].ss_txbuf[14] = 0x6C;
			vrr_cmds->cmds[idx].ss_txbuf[20] = 0x10;
		} else {
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x61;
			vrr_cmds->cmds[idx].ss_txbuf[3] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[5] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[6] = 0x01;
			vrr_cmds->cmds[idx].ss_txbuf[7] = vrr_cmds->cmds[idx].ss_txbuf[8] = 0;
			vrr_cmds->cmds[idx].ss_txbuf[13] = 0x04;
			vrr_cmds->cmds[idx].ss_txbuf[14] = 0x94;
			vrr_cmds->cmds[idx].ss_txbuf[20] = 0x10;
		}
	}

	/* VFP Setting */
	idx = ss_get_cmd_idx(vrr_cmds, 0x08, 0xF2);
	if (idx < 0)
		idx = ss_get_cmd_idx(vrr_cmds, 0x11, 0xF2);

	if (idx >= 0) {
		if (cur_md_base == VRR_120HS) {
			memcpy(&vrr_cmds->cmds[idx-1].ss_txbuf[1], VFP_SETTING_GPARA_120HS,
										sizeof(VFP_SETTING_GPARA_120HS));
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_SETTING_120HS, sizeof(VFP_SETTING_120HS));
		} else if (cur_md_base == VRR_96HS) {
			memcpy(&vrr_cmds->cmds[idx-1].ss_txbuf[1], VFP_SETTING_GPARA_96HS,
										sizeof(VFP_SETTING_GPARA_96HS));
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_SETTING_96HS, sizeof(VFP_SETTING_96HS));
		} else if (cur_md_base == VRR_60HS) {
			memcpy(&vrr_cmds->cmds[idx-1].ss_txbuf[1], VFP_SETTING_GPARA_60HS,
										sizeof(VFP_SETTING_GPARA_60HS));
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_SETTING_60HS, sizeof(VFP_SETTING_60HS));
		} else if (cur_md_base == VRR_60NS) {
			memcpy(&vrr_cmds->cmds[idx-1].ss_txbuf[1], VFP_SETTING_GPARA_60NS,
										sizeof(VFP_SETTING_GPARA_60NS));
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_SETTING_60NS, sizeof(VFP_SETTING_60NS));
		} else if (cur_md_base == VRR_48HS) {
			memcpy(&vrr_cmds->cmds[idx-1].ss_txbuf[1], VFP_SETTING_GPARA_48HS,
										sizeof(VFP_SETTING_GPARA_48HS));
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_SETTING_48HS, sizeof(VFP_SETTING_48HS));
		} else
			LCD_ERR(vdd, "Invalid rr or hs, cur_rr=%d, cur_hs=%d\n", cur_rr, cur_hs);
	}

	/* AOR Setting */
	idx = ss_get_cmd_idx(vrr_cmds, 0x8B, 0x98);
	if (idx < 0) {
		idx = ss_get_cmd_idx(vrr_cmds, 0xAB, 0x98);
		if (idx < 0)
			idx = ss_get_cmd_idx(vrr_cmds, 0x6B, 0x98);
	}

	if (idx >= 0) {
		if (cur_md_base == VRR_120HS) {
			memcpy(&vrr_cmds->cmds[idx-1].ss_txbuf[1], AOR_SETTING_GPARA_120HS,
										sizeof(AOR_SETTING_GPARA_120HS));
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], AOR_SETTING_120HS, sizeof(AOR_SETTING_120HS));
		} else if (cur_md_base == VRR_96HS) {
			memcpy(&vrr_cmds->cmds[idx-1].ss_txbuf[1], AOR_SETTING_GPARA_96HS,
										sizeof(AOR_SETTING_GPARA_96HS));
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], AOR_SETTING_96HS, sizeof(AOR_SETTING_96HS));
		} else if (cur_md_base == VRR_60HS) {
			memcpy(&vrr_cmds->cmds[idx-1].ss_txbuf[1], AOR_SETTING_GPARA_60HS,
										sizeof(AOR_SETTING_GPARA_60HS));
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], AOR_SETTING_60HS, sizeof(AOR_SETTING_60HS));
		} else if (cur_md_base == VRR_60NS) {
			memcpy(&vrr_cmds->cmds[idx-1].ss_txbuf[1], AOR_SETTING_GPARA_60NS,
										sizeof(AOR_SETTING_GPARA_60NS));
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], AOR_SETTING_60NS, sizeof(AOR_SETTING_60NS));
		} else if (cur_md_base == VRR_48HS) {
			memcpy(&vrr_cmds->cmds[idx-1].ss_txbuf[1], AOR_SETTING_GPARA_48HS,
										sizeof(AOR_SETTING_GPARA_48HS));
			memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], AOR_SETTING_48HS, sizeof(AOR_SETTING_48HS));
		} else
			LCD_ERR(vdd, "Invalid rr or hs, cur_rr=%d, cur_hs=%d\n", cur_rr, cur_hs);
	}

	/* HS <-> NS case during vrr changing */
	if (((prev_hs && !cur_hs) || (!prev_hs && cur_hs)) && vrr->running_vrr)
		vdd->need_brightness_lock = 1;
	else
		vdd->need_brightness_lock = 0;

	LCD_INFO(vdd, "VRR: (prev: %d%s -> cur: %dx%d@%d%s) brightness_lock (%d)\n",
			prev_rr, prev_hs ? (prev_phs ? "PHS" : "HS") : "NM",
			panel->cur_mode->timing.h_active,
			panel->cur_mode->timing.v_active,
			cur_rr, cur_hs ? (cur_phs ? "PHS" : "HS") : "NM",
			vdd->need_brightness_lock);

	return vrr_cmds;
}

static struct dsi_panel_cmd_set * ss_brightness_gamma(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	enum VRR_CMD_RR cur_md, cur_md_base;
	int idx = 0;
	int cur_rr, smooth_frame;
	bool cur_hs, cur_phs, dim_off = false;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	cur_rr = vdd->vrr.cur_refresh_rate;
	cur_hs = vdd->vrr.cur_sot_hs_mode;
	cur_phs = vdd->vrr.cur_phs_mode;

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_NORMAL);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_INFO(vdd, "no gamma cmds\n");
		return NULL;
	}

	cur_md = ss_get_vrr_mode(vdd, cur_rr, cur_hs, cur_phs);
	cur_md_base = ss_get_vrr_mode_base(vdd, cur_rr, cur_hs, cur_phs);

	if (cur_md_base == VRR_120HS || cur_md_base == VRR_60NS) {
		dim_off = false;
		smooth_frame = 15;
	} else {
		dim_off = true;
		smooth_frame = 0;
	}

	if (vdd->vrr.running_vrr || !vdd->display_on) {
		dim_off = true;
		smooth_frame = 0;
	}

	LCD_INFO(vdd, "NORMAL : cd_idx [%d] cur_md [%d] base [%d] smooth_frame [%d]\n",
		vdd->br_info.common_br.cd_idx, cur_md, cur_md_base, smooth_frame);

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
	if (idx >= 0) {
		if (vdd->br_info.common_br.irc_mode == IRC_MODERATO_MODE) /* Vivid */
			memcpy(&pcmds->cmds[idx].ss_txbuf[1], IRC_MODERATO, sizeof(IRC_MODERATO));
		else if (vdd->br_info.common_br.irc_mode == IRC_FLAT_GAMMA_MODE) /* Natural */
			memcpy(&pcmds->cmds[idx].ss_txbuf[1], IRC_FLAT, sizeof(IRC_FLAT));
	}

	/* smooth dim */
	idx = ss_get_cmd_idx(pcmds, 0x0D, 0x94);
	pcmds->cmds[idx].ss_txbuf[1] = dim_off ? 0x20 : 0x70;
	pcmds->cmds[idx].ss_txbuf[2] = smooth_frame;
	idx = ss_get_cmd_idx(pcmds, 0x00, 0x53);
	pcmds->cmds[idx].ss_txbuf[1] = dim_off ? 0x20 : 0x28;

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
#if 0 // remove updatable feature
	int idx = 0;
	u8 evlss_addr = 0x94;
	int elvss_off = 0x18;
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
					elvss_off, evlss_addr)) {
			LCD_ERR(vdd, "fail to restore backup cmd\n");
			return;
		}

		/* restore ELVSS_CAL_OFFSET (94h 0x18th) */
		idx = ss_get_cmd_idx(pcmds, elvss_off, evlss_addr);
		if (idx < 0) {
			LCD_ERR(vdd, "fail to find C2h (off=0x14) cmd\n");
			return;
		}

		pcmds->cmds[idx].ss_txbuf[1] = vdd->br_info.common_br.elvss_value[0];
		LCD_INFO(vdd, "update C2h elvss(0x%x), idx: %d\n",
				vdd->br_info.common_br.elvss_value[0], idx);
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
	mdnie_data->dsi_afc_size = 45;
	mdnie_data->dsi_afc_index = 33;

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

/* Don't have to care xtalk_mode flag with lego-opcode concept */
static struct dsi_panel_cmd_set *ss_vint(struct samsung_display_driver_data *vdd, int *level_key)
{
	return NULL;
}

static void ss_set_panel_lpm_brightness(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *set = ss_get_cmds(vdd, TX_LPM_BL_CMD);
	struct dsi_panel_cmd_set *set_lpm_bl;
	int lpm_ctrl_idx, lpm_aor_idx, aor_pload_0, aor_pload_1;

	if (SS_IS_CMDS_NULL(set)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_BL_CMD\n");
		return;
	}

	lpm_ctrl_idx = ss_get_cmd_idx(set, 0x4E, 0x98);
	lpm_aor_idx = ss_get_cmd_idx(set, 0x13B, 0x98);


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

	if (lpm_ctrl_idx < 0 || lpm_aor_idx < 0) {
		LCD_ERR(vdd, "Invalid AOD cmd idx, lpm_ctrl(%d), lpm_aor(%d)\n", lpm_ctrl_idx, lpm_aor_idx);
		return;
	}

	if (SS_IS_CMDS_NULL(set_lpm_bl)) {
		LCD_ERR(vdd, "No cmds for HLPM Brightness..\n");
		return;
	}

	aor_pload_0 = set_lpm_bl->cmds->ss_txbuf[1];
	aor_pload_1 = set_lpm_bl->cmds->ss_txbuf[2];

	set->cmds[lpm_ctrl_idx].ss_txbuf[2] = set->cmds[lpm_aor_idx].ss_txbuf[1] = aor_pload_0;
	set->cmds[lpm_ctrl_idx].ss_txbuf[3] = set->cmds[lpm_aor_idx].ss_txbuf[2] = aor_pload_1;

	/* send lpm bl cmd */
	ss_send_cmd(vdd, TX_LPM_BL_CMD);

	vdd->br_info.last_br_is_hbm = false;

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
	struct dsi_panel_cmd_set *lpm_cmd;
	struct dsi_panel_cmd_set *lpm_bl_cmd;
	int lpm_aor_idx, lpm_ctrl_idx, aor_pload_0, aor_pload_1;


	if (enable) {
		lpm_cmd = ss_get_cmds(vdd, TX_LPM_ON);
	} else {
		/* Don't need to update */
		return;
	}

	if (SS_IS_CMDS_NULL(lpm_cmd)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_ON/OFF\n");
		return;
	}

	lpm_ctrl_idx = ss_get_cmd_idx(lpm_cmd, 0x4E, 0x98);
	lpm_aor_idx = ss_get_cmd_idx(lpm_cmd, 0x13B, 0x98);

	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_60NIT:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
		break;
	case LPM_30NIT:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
		break;
	case LPM_10NIT:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
		break;
	case LPM_2NIT:
	default:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
		break;
	}

	if (lpm_ctrl_idx < 0 || lpm_aor_idx < 0) {
		LCD_ERR(vdd, "Invalid AOD cmd idx, lpm_ctrl(%d), lpm_aor(%d)\n", lpm_ctrl_idx, lpm_aor_idx);
		return;
	}

	if (SS_IS_CMDS_NULL(lpm_bl_cmd)) {
		LCD_ERR(vdd, "No cmds for HLPM control brightness..\n");
		return;
	}

	aor_pload_0 = lpm_bl_cmd->cmds->ss_txbuf[1];
	aor_pload_1 = lpm_bl_cmd->cmds->ss_txbuf[2];

	lpm_cmd->cmds[lpm_ctrl_idx].ss_txbuf[2] = lpm_cmd->cmds[lpm_aor_idx].ss_txbuf[1] = aor_pload_0;
	lpm_cmd->cmds[lpm_ctrl_idx].ss_txbuf[3] = lpm_cmd->cmds[lpm_aor_idx].ss_txbuf[2] = aor_pload_1;
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

	LCD_INFO(vdd, "ECC = 0x%02X 0x%02X 0x%02X -> %d\n", ecc[0], ecc[1], ecc[2], pass);

	return pass;
}

static int ss_ssr_read(struct samsung_display_driver_data *vdd)
{
	u8 ssr[2] = {0, };
	bool pass = false;

	ss_send_cmd_get_rx(vdd, RX_SSR, ssr);

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

static void ss_print_gamma_comp(struct samsung_display_driver_data *vdd)
{
	char pBuffer[256];
	int i, j;

	LCD_INFO(vdd, "== HS120_R_TYPE_BUF ==\n");
	for (i = 0; i < GAMMA_SET_MAX; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_R_TYPE_BUF[i][j]);
		LCD_INFO(vdd, "READ_R 120 SET[%2d] : %s\n", GAMMA_SET_MAX - 1 - i, pBuffer);
	}

	LCD_INFO(vdd, "== HS60_R_TYPE_BUF ==\n");
	for (i = 0; i < GAMMA_SET_MAX; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS60_R_TYPE_BUF[i][j]);
		LCD_INFO(vdd, "READ_R  60 SET[%2d] : %s\n", GAMMA_SET_MAX - 1 - i, pBuffer);
	}

	LCD_INFO(vdd, "== HS120_V_TYPE_BUF ==\n");
	for (i = 0; i < GAMMA_SET_MAX; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_V_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_V_TYPE_BUF[i][j]);
		LCD_INFO(vdd, "READ_V 120 SET[%2d] : %s\n", GAMMA_SET_MAX - 1 - i, pBuffer);
	}

	LCD_INFO(vdd, "== HS60_V_TYPE_BUF ==\n");
	for (i = 0; i < GAMMA_SET_MAX; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_V_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS60_V_TYPE_BUF[i][j]);
		LCD_INFO(vdd, "READ_V  60 SET[%2d] : %s\n", GAMMA_SET_MAX - 1 - i, pBuffer);
	}

	LCD_INFO(vdd, "== HS96_V_TYPE_COMP ==\n");
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_V_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS96_V_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP_V 96 LV[%3d] : %s\n", i, pBuffer);
	}

	LCD_INFO(vdd, "== HS60_V_TYPE_COMP ==\n");
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_V_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS60_V_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP_V 60 LV[%3d] : %s\n", i, pBuffer);
	}

	LCD_INFO(vdd, "== HS48_V_TYPE_COMP ==\n");
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_V_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS48_V_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP_V 48 LV[%3d] : %s\n", i, pBuffer);
	}

	LCD_INFO(vdd, "== HS96_R_TYPE_COMP ==\n");
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS96_R_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP_R 96 LV[%3d] : %s\n", i, pBuffer);
	}

	LCD_INFO(vdd, "== HS60_R_TYPE_COMP ==\n");
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS60_R_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP_R 60 LV[%3d] : %s\n", i, pBuffer);
	}

	LCD_INFO(vdd, "== HS48_R_TYPE_COMP ==\n");
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS48_R_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP_R 48 LV[%3d] : %s\n", i, pBuffer);
	}
}

#define FLASH_READ_SIZE (GAMMA_R_SIZE * 3)

static void ss_read_gamma(struct samsung_display_driver_data *vdd)
{
	u8 readbuf[FLASH_READ_SIZE];
	char pBuffer[256];
	int i, j;

	ss_send_cmd(vdd, TX_POC_ENABLE);

	LCD_INFO(vdd, "READ_R start\n");

	for (i = 0; i < GAMMA_SET_MAX; i+=3) {
		LCD_INFO(vdd, "[120] start_addr : %X, size : %d\n", GAMMA_SET_ADDR_120_TABLE[i], FLASH_READ_SIZE);
		spsram_read_bytes(vdd, GAMMA_SET_ADDR_120_TABLE[i], FLASH_READ_SIZE, readbuf);
		memcpy(HS120_R_TYPE_BUF[i], readbuf, GAMMA_R_SIZE);
		memcpy(HS120_R_TYPE_BUF[i+1], readbuf + (GAMMA_R_SIZE), GAMMA_R_SIZE);
		memcpy(HS120_R_TYPE_BUF[i+2], readbuf + (GAMMA_R_SIZE * 2), GAMMA_R_SIZE);
	}

	for (i = 0; i < GAMMA_SET_MAX; i+=3) {
		LCD_INFO(vdd, "[60] start_addr : %X, size : %d\n", GAMMA_SET_ADDR_60_TABLE[i], FLASH_READ_SIZE);
		spsram_read_bytes(vdd, GAMMA_SET_ADDR_60_TABLE[i], FLASH_READ_SIZE, readbuf);
		memcpy(HS60_R_TYPE_BUF[i], readbuf, GAMMA_R_SIZE);
		memcpy(HS60_R_TYPE_BUF[i+1], readbuf + (GAMMA_R_SIZE), GAMMA_R_SIZE);
		memcpy(HS60_R_TYPE_BUF[i+2], readbuf + (GAMMA_R_SIZE * 2), GAMMA_R_SIZE);
	}

	ss_send_cmd(vdd, TX_POC_DISABLE);


	LCD_INFO(vdd, "== HS120_R_TYPE_BUF ==\n");
	for (i = 0; i < GAMMA_SET_MAX; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_R_TYPE_BUF[i][j]);
		LCD_INFO(vdd, "READ_R 120 SET[%2d] : %s\n", GAMMA_SET_MAX - 1 - i, pBuffer);
	}

	LCD_INFO(vdd, "== HS60_R_TYPE_BUF ==\n");
	for (i = 0; i < GAMMA_SET_MAX; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS60_R_TYPE_BUF[i][j]);
		LCD_INFO(vdd, "READ_R  60 SET[%2d] : %s\n", GAMMA_SET_MAX - 1 - i, pBuffer);
	}

	return;
}

static int ss_gm2_gamma_comp_init(struct samsung_display_driver_data *vdd)
{
//	u8 readbuf[FLASH_READ_SIZE];
	int i, j, m;
	int val, *v_val;

	LCD_INFO(vdd, "++\n");

	memcpy(GLUT_OFFSET_96HS_V, GLUT_OFFSET_96HS_V_revA, sizeof(GLUT_OFFSET_96HS_V));
	memcpy(GLUT_OFFSET_60HS_V, GLUT_OFFSET_60HS_V_revA, sizeof(GLUT_OFFSET_60HS_V));
	memcpy(GLUT_OFFSET_48HS_V, GLUT_OFFSET_48HS_V_revA, sizeof(GLUT_OFFSET_48HS_V));

	memcpy(GAMMA_OFFSET_96HS_V, GAMMA_OFFSET_96HS_V_revA, sizeof(GAMMA_OFFSET_96HS_V));
	memcpy(GAMMA_OFFSET_60HS_V, GAMMA_OFFSET_60HS_V_revA, sizeof(GAMMA_OFFSET_60HS_V));
	memcpy(GAMMA_OFFSET_48HS_V, GAMMA_OFFSET_48HS_V_revA, sizeof(GAMMA_OFFSET_48HS_V));

	/********************************************/
	/* 1.  Read HS120 ORIGINAL GAMMA Flash      */
	/********************************************/
	ss_read_gamma(vdd);

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

	for (i = 0; i < GAMMA_SET_MAX; i++) {
		m = 0;
		for (j = 0; j < GAMMA_R_SIZE; ) {
			HS60_V_TYPE_BUF[i][m++] = (GET_BITS(HS60_R_TYPE_BUF[i][j], 0, 3) << 8)
									| GET_BITS(HS60_R_TYPE_BUF[i][j+2], 0, 7);
			HS60_V_TYPE_BUF[i][m++] = (GET_BITS(HS60_R_TYPE_BUF[i][j+1], 4, 7) << 8)
									| GET_BITS(HS60_R_TYPE_BUF[i][j+3], 0, 7);
			HS60_V_TYPE_BUF[i][m++] = (GET_BITS(HS60_R_TYPE_BUF[i][j+1], 0, 3) << 8)
									| GET_BITS(HS60_R_TYPE_BUF[i][j+4], 0, 7);
			j += 5;
		}
	}

	/*************************************************************/
	/* 3. [ALL] Make HS96_V_TYPE_COMP (NORMAL + HBM) */
	/*************************************************************/

	/* 96HS - from 120HS mtp gamma */
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

	/* 60HS - from 60HS mtp gamma */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		v_val = HS60_V_TYPE_BUF[GAMMA_SET_REGION_TABLE[i]];
		for (j = 0; j < GAMMA_V_SIZE; j++) {
			/* check underflow & overflow */
			if (v_val[j] + GAMMA_OFFSET_60HS_V[i][j] < 0) {
				HS60_V_TYPE_COMP[i][j] = 0;
			} else {
				if (j <= 5 || j >= (GAMMA_V_SIZE - 3)) {/* 1/13/14 th is 12bit(0xFFF) */
					val = v_val[j] + GAMMA_OFFSET_60HS_V[i][j];
					if (val > 0xFFF)	/* check overflow */
						HS60_V_TYPE_COMP[i][j] = 0xFFF;
					else
						HS60_V_TYPE_COMP[i][j] = val;
				} else {	/* 2 ~ 12th 11bit(0x7FF) */
					val = v_val[j] + GAMMA_OFFSET_60HS_V[i][j];
					if (val > 0x7FF)	/* check overflow */
						HS60_V_TYPE_COMP[i][j] = 0x7FF;
					else
						HS60_V_TYPE_COMP[i][j] = val;
				}
			}
		}
	}

	/* 48HS - from 60HS mtp gamma */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		v_val = HS60_V_TYPE_BUF[GAMMA_SET_REGION_TABLE[i]];
		for (j = 0; j < GAMMA_V_SIZE; j++) {
			/* check underflow & overflow */
			if (v_val[j] + GAMMA_OFFSET_48HS_V[i][j] < 0) {
				HS48_V_TYPE_COMP[i][j] = 0;
			} else {
				if (j <= 5 || j >= (GAMMA_V_SIZE - 3)) {/* 1/13/14 th is 12bit(0xFFF) */
					val = v_val[j] + GAMMA_OFFSET_48HS_V[i][j];
					if (val > 0xFFF)	/* check overflow */
						HS48_V_TYPE_COMP[i][j] = 0xFFF;
					else
						HS48_V_TYPE_COMP[i][j] = val;
				} else {	/* 2 ~ 12th 11bit(0x7FF) */
					val = v_val[j] + GAMMA_OFFSET_48HS_V[i][j];
					if (val > 0x7FF)	/* check overflow */
						HS48_V_TYPE_COMP[i][j] = 0x7FF;
					else
						HS48_V_TYPE_COMP[i][j] = val;
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

	/* 60HS */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		m = 0; // GAMMA SET size
		for (j = 0; j < GAMMA_V_SIZE; j += RGB_MAX) {
			if (j == 0 || j == 3 || j == GAMMA_V_SIZE - 3) {
				HS60_R_TYPE_COMP[i][m++] = GET_BITS(HS60_V_TYPE_COMP[i][j+R], 8, 11);
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+G], 8, 11) << 4)
											| (GET_BITS(HS60_V_TYPE_COMP[i][j+B], 8, 11));
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+R], 0, 7));
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+G], 0, 7));
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+B], 0, 7));
			} else {	/* 1st ~ 10th */
				HS60_R_TYPE_COMP[i][m++] = GET_BITS(HS60_V_TYPE_COMP[i][j+R], 8, 10);
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+G], 8, 10) << 4)
											| (GET_BITS(HS60_V_TYPE_COMP[i][j+B], 8, 10));
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+R], 0, 7));
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+G], 0, 7));
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+B], 0, 7));
			}
		}
	}

	/* 48HS */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		m = 0; // GAMMA SET size
		for (j = 0; j < GAMMA_V_SIZE; j += RGB_MAX) {
			if (j == 0 || j == 3 || j == GAMMA_V_SIZE - 3) {
				HS48_R_TYPE_COMP[i][m++] = GET_BITS(HS48_V_TYPE_COMP[i][j+R], 8, 11);
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+G], 8, 11) << 4)
											| (GET_BITS(HS48_V_TYPE_COMP[i][j+B], 8, 11));
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+R], 0, 7));
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+G], 0, 7));
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+B], 0, 7));
			} else {	/* 1st ~ 10th */
				HS48_R_TYPE_COMP[i][m++] = GET_BITS(HS48_V_TYPE_COMP[i][j+R], 8, 10);
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+G], 8, 10) << 4)
											| (GET_BITS(HS48_V_TYPE_COMP[i][j+B], 8, 10));
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+R], 0, 7));
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+G], 0, 7));
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+B], 0, 7));
			}
		}
	}

	/* print all results */
//	ss_print_gamma_comp(vdd);

	LCD_INFO(vdd, " --\n");

	return 0;
}

static struct dsi_panel_cmd_set *ss_glut_offset(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds = ss_get_cmds(vdd, TX_GLUT_OFFSET);
	int idx, level;
	enum VRR_CMD_RR cur_md_base;
	int cur_rr;
	bool cur_hs, cur_phs;

	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_GLUT_OFFSET.. \n");
		return NULL;
	}

	cur_rr = vdd->vrr.cur_refresh_rate;
	cur_hs = vdd->vrr.cur_sot_hs_mode;
	cur_phs = vdd->vrr.cur_phs_mode;

	cur_md_base = ss_get_vrr_mode_base(vdd, cur_rr, cur_hs, cur_phs);
	level = vdd->br_info.common_br.bl_level;

	idx = ss_get_cmd_idx(pcmds, 0xA1, 0x93);
	if (cur_md_base == VRR_60HS)
		memcpy(&pcmds->cmds[idx].ss_txbuf[1], GLUT_OFFSET_60HS_V[level], GLUT_SIZE);
	else if (cur_md_base == VRR_48HS)
		memcpy(&pcmds->cmds[idx].ss_txbuf[1], GLUT_OFFSET_48HS_V[level], GLUT_SIZE);
	else if (cur_md_base == VRR_96HS)
		memcpy(&pcmds->cmds[idx].ss_txbuf[1], GLUT_OFFSET_96HS_V[level], GLUT_SIZE);
	else
		return NULL;

	LCD_INFO(vdd, "cur_md_base [%d], level[%d]\n", cur_md_base, level);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_manual_aor(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	enum VRR_CMD_RR cur_md_base;
	int cur_rr, idx;
	bool cur_hs, cur_phs;
	int bl_level = vdd->br_info.common_br.bl_level;

	pcmds = ss_get_cmds(vdd, TX_MANUAL_AOR);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_MANUAL_AOR.. \n");
		return NULL;
	}

	cur_rr = vdd->vrr.cur_refresh_rate;
	cur_hs = vdd->vrr.cur_sot_hs_mode;
	cur_phs = vdd->vrr.cur_phs_mode;
	cur_md_base = ss_get_vrr_mode_base(vdd, cur_rr, cur_hs, cur_phs);

	/* Manual AOR Enable */
	idx = ss_get_cmd_idx(pcmds, 0x4E, 0x98);
	if (idx >= 0) {
		if (cur_md_base == VRR_120HS || cur_md_base == VRR_60NS) {
			pcmds->cmds[idx].ss_txbuf[1] = 0x00;
		} else {
			pcmds->cmds[idx].ss_txbuf[1] = 0x01;
		}
	}

	if (cur_md_base != VRR_120HS) {
		pcmds->count = 8;

		/* Manual AOR Value */
		idx = ss_get_cmd_idx(pcmds, 0x4F, 0x98);
		if (idx >= 0) {
			if (bl_level <= MAX_HBM_PF_LEVEL) {
				if (cur_md_base == VRR_96HS) {
					pcmds->cmds[idx].ss_txbuf[1] = MANUAL_AOR_TABLE[bl_level][MANUAL_AOR_96HS_1ST];
					pcmds->cmds[idx].ss_txbuf[2] = MANUAL_AOR_TABLE[bl_level][MANUAL_AOR_96HS_2ND];
				} else if (cur_md_base == VRR_60HS) {
					pcmds->cmds[idx].ss_txbuf[1] = MANUAL_AOR_TABLE[bl_level][MANUAL_AOR_60HS_1ST];
					pcmds->cmds[idx].ss_txbuf[2] = MANUAL_AOR_TABLE[bl_level][MANUAL_AOR_60HS_2ND];
				} else if (cur_md_base == VRR_48HS) {
					pcmds->cmds[idx].ss_txbuf[1] = MANUAL_AOR_TABLE[bl_level][MANUAL_AOR_48HS_1ST];
					pcmds->cmds[idx].ss_txbuf[2] = MANUAL_AOR_TABLE[bl_level][MANUAL_AOR_48HS_2ND];
				} else if (cur_md_base == VRR_60NS) {
					/* Incase of 60NS, Set Value to prevent horizontal line during display off */
					pcmds->cmds[idx].ss_txbuf[1] = 0x00;
					pcmds->cmds[idx].ss_txbuf[2] = 0x13;
				} else {
					/* Incase of 120HS, Disable Manual AOR(Don't Care) */
					pcmds->cmds[idx].ss_txbuf[1] = 0x00;
					pcmds->cmds[idx].ss_txbuf[2] = 0x00;
				}
			}
		}
	} else {
		pcmds->count = 2;
	}

	return pcmds;
}

static bool offset_restored[GAMMA_SET_MAX] = {0,};
static struct dsi_panel_cmd_set *ss_brightness_gm2_gamma_comp(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int idx, bl_level, prev_bl_level, set, dset, seto, use_comp2, fdelay, dummy_offset;
	enum VRR_CMD_RR cur_md_base, prev_md_base;
	u8 *comp_reg;
	s64 t_delta;
	int cur_rr, prev_rr, dummy_size;
	bool cur_hs, cur_phs, prev_hs, prev_phs;

	cur_rr = vdd->vrr.cur_refresh_rate;
	cur_hs = vdd->vrr.cur_sot_hs_mode;
	cur_phs = vdd->vrr.cur_phs_mode;

	prev_rr = vdd->vrr.prev_refresh_rate;
	prev_hs = vdd->vrr.prev_sot_hs_mode;
	prev_phs = vdd->vrr.prev_phs_mode;

	bl_level = vdd->br_info.common_br.bl_level;
	prev_bl_level = vdd->br_info.common_br.prev_bl_level;
	cur_md_base = ss_get_vrr_mode_base(vdd, cur_rr, cur_hs, cur_phs);
	prev_md_base = ss_get_vrr_mode_base(vdd, prev_rr, prev_hs, prev_phs);

	set = GAMMA_SET_REGION_TABLE[bl_level];
	dset = set + 1;

	return NULL;

	if (set == GAMMA_SET_11) {
		pcmds = ss_get_cmds(vdd, TX_VRR_GM2_GAMMA_COMP); // 65 + 3 bytes
		seto = 0; // set offset
		use_comp2 = false;
		dummy_size = 3;
		dummy_offset = 1 + GAMMA_R_SIZE;
	} else {
		pcmds = ss_get_cmds(vdd, TX_VRR_GM2_GAMMA_COMP2); // 65 + 65 + 2 bytes
		seto = 1; // set offset
		use_comp2 = true;
		dummy_size = 2;
		dummy_offset = 1 + GAMMA_R_SIZE + GAMMA_R_SIZE;
	}

	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_VRR_GM2_GAMMA_COMP.. \n");
		return NULL;
	}

	/* alread restored -> null return */
	if (cur_md_base == VRR_120HS) {
		if (offset_restored[set]) {
			LCD_ERR(vdd, "already restored .. set %d\n", set);
			return NULL;
		}
	}

	if (cur_md_base == VRR_120HS || cur_md_base == VRR_96HS) {
		LCD_DEBUG(vdd, "Do not write 120/96 ananlog gamma..\n");
		return NULL;
	}

	if (cur_md_base == VRR_120HS || cur_md_base == VRR_96HS ||
			cur_md_base == VRR_60HS || cur_md_base == VRR_48HS) {

		/* HBM -> Normal BL case */
		if ((bl_level <= MAX_BL_PF_LEVEL) && (prev_bl_level > MAX_BL_PF_LEVEL)) {
			LCD_INFO(vdd, "need to update ananlog offset for 48/60hs when hbm(%d) -> normal(%d) case\n",
				prev_bl_level, bl_level);
		} else if ((prev_md_base == VRR_120HS || prev_md_base == VRR_96HS) && (cur_md_base == VRR_60HS || cur_md_base == VRR_48HS)) {
			LCD_INFO(vdd, "need to update ananlog offset for 48/60hs when 120addr -> 60addr case\n");
		} else {
			if ((cur_md_base == VRR_60HS || cur_md_base == VRR_48HS) && (set != GAMMA_SET_0)) {
				LCD_INFO(vdd, "Do not write ananlog offset for 48/60 base + 0~255lv\n");
				return NULL;
			}
		}

		if (cur_md_base == VRR_120HS) {
			fdelay = ss_frame_delay(120, 1);
		} else if (cur_md_base == VRR_96HS) {
			fdelay = ss_frame_delay(96, 1);
		} else if (cur_md_base == VRR_60HS) {
			fdelay = ss_frame_delay(60, 1);
			fdelay = fdelay / 2;
		} else if (cur_md_base == VRR_48HS) {
			fdelay = ss_frame_delay(48, 1);
		}

		if (vdd->br_info.last_tx_time) {
			t_delta = ktime_us_delta(ktime_get(), vdd->br_info.last_tx_time);
			if (t_delta < fdelay) {
				usleep_range(fdelay - t_delta, fdelay - t_delta);
				LCD_INFO(vdd, "delay %d (%d-%d)\n", fdelay - t_delta, fdelay, t_delta);
			}
		}

		idx = ss_get_cmd_idx(pcmds, 0x00, 0x71);
		if (cur_md_base == VRR_120HS || cur_md_base == VRR_96HS) {
			pcmds->cmds[idx].ss_txbuf[2] = (GAMMA_SET_ADDR_120_TABLE[set - seto] & 0xFF00) >> 8;
			pcmds->cmds[idx].ss_txbuf[3] = (GAMMA_SET_ADDR_120_TABLE[set- seto] & 0xFF);

			if (cur_md_base == VRR_120HS)
				offset_restored[set] = true;
			else
				offset_restored[set] = false;
		} else {
			pcmds->cmds[idx].ss_txbuf[2] = (GAMMA_SET_ADDR_60_TABLE[set - seto] & 0xFF00) >> 8;
			pcmds->cmds[idx].ss_txbuf[3] = (GAMMA_SET_ADDR_60_TABLE[set - seto] & 0xFF);
		}

		idx = ss_get_cmd_idx(pcmds, 0x00, 0x6C);
		if (idx >= 0) {
			if (use_comp2) {
				if (cur_md_base == VRR_120HS || cur_md_base == VRR_96HS)
					comp_reg = HS120_R_TYPE_BUF[set - seto];
				else
					comp_reg = HS60_R_TYPE_BUF[set - seto];

				memcpy(&pcmds->cmds[idx].ss_txbuf[1], comp_reg, GAMMA_R_SIZE);

				if (cur_md_base == VRR_120HS)
					comp_reg = HS120_R_TYPE_BUF[set];
		 		else if (cur_md_base == VRR_96HS)
					comp_reg = HS96_R_TYPE_COMP[bl_level];
				else if (cur_md_base == VRR_60HS)
					comp_reg = HS60_R_TYPE_COMP[bl_level];
				else
					comp_reg = HS48_R_TYPE_COMP[bl_level];

				memcpy(&pcmds->cmds[idx].ss_txbuf[1 + GAMMA_R_SIZE], comp_reg, GAMMA_R_SIZE);
			} else {
				if (cur_md_base == VRR_120HS)
					comp_reg = HS120_R_TYPE_BUF[set];
		 		else if (cur_md_base == VRR_96HS)
					comp_reg = HS96_R_TYPE_COMP[bl_level];
				else if (cur_md_base == VRR_60HS)
					comp_reg = HS60_R_TYPE_COMP[bl_level];
				else
					comp_reg = HS48_R_TYPE_COMP[bl_level];

				memcpy(&pcmds->cmds[idx].ss_txbuf[1], comp_reg, GAMMA_R_SIZE);
			}

			/* fill dummpy bytes (next addr original gamma) */
			if (dset < GAMMA_SET_MAX) {
				if (cur_md_base == VRR_120HS || cur_md_base == VRR_96HS)
					memcpy(&pcmds->cmds[idx].ss_txbuf[dummy_offset], HS120_R_TYPE_BUF[dset], dummy_size);
				else
					memcpy(&pcmds->cmds[idx].ss_txbuf[dummy_offset], HS60_R_TYPE_BUF[dset], dummy_size);
			} else {
				memset(&pcmds->cmds[idx].ss_txbuf[dummy_offset], 0x00, dummy_size);
			}
		}

		LCD_INFO(vdd, "cur_md_base[%d], bl_level[%d] set[%d] offset [%02x%02x] \n",
			cur_md_base, bl_level, set, pcmds->cmds[0].ss_txbuf[2], pcmds->cmds[0].ss_txbuf[3]);
		return pcmds;
	}

	return NULL;
}

static void make_brightness_packet(struct samsung_display_driver_data *vdd,
	struct dsi_cmd_desc *packet, int *cmd_cnt, enum BR_TYPE br_type)
{
	int loop;
	int start = *cmd_cnt;

	if (br_type == BR_TYPE_NORMAL || br_type == BR_TYPE_HBM) {
		/* vint */
		ss_add_brightness_packet(vdd, BR_FUNC_VINT, packet, cmd_cnt);

		/* mAFPC */
		if (vdd->mafpc.is_support)
			ss_add_brightness_packet(vdd, BR_FUNC_MAFPC_SCALE, packet, cmd_cnt);

		/* VRR */
		ss_add_brightness_packet(vdd, BR_FUNC_VRR, packet, cmd_cnt);

		/* MANUAL AOR */
		ss_add_brightness_packet(vdd, BR_FUNC_MANUAL_AOR, packet, cmd_cnt);

		/* ACL */
		if (vdd->br_info.acl_status || vdd->siop_status)
			ss_add_brightness_packet(vdd, BR_FUNC_ACL_ON, packet, cmd_cnt);
		else
			ss_add_brightness_packet(vdd, BR_FUNC_ACL_OFF, packet, cmd_cnt);

		/* Analog OFFSET */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA_COMP, packet, cmd_cnt);

		/* GLUT OFFSET */
		ss_add_brightness_packet(vdd, BR_FUNC_GLUT_OFFSET, packet, cmd_cnt);

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

void S6E3FAC_AMB606AW01_FHD_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "S6E3FAC_AMB606AW01 : ++ \n");
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
	vdd->panel_func.br_func[BR_FUNC_VINT] = ss_vint;
	vdd->panel_func.br_func[BR_FUNC_VRR] = ss_vrr;
	vdd->panel_func.br_func[BR_FUNC_MANUAL_AOR] = ss_manual_aor;
	vdd->panel_func.br_func[BR_FUNC_GAMMA_COMP] = ss_brightness_gm2_gamma_comp;
	vdd->panel_func.br_func[BR_FUNC_GLUT_OFFSET] = ss_glut_offset;

	/* Total level key in brightness */
	vdd->panel_func.samsung_brightness_tot_level = ss_brightness_tot_level;

	/* HBM */
	vdd->panel_func.br_func[BR_FUNC_HBM_GAMMA] = ss_brightness_gamma;
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
	vdd->panel_func.samsung_gm2_gamma_comp_init = ss_gm2_gamma_comp_init;
	vdd->panel_func.samsung_read_gamma = ss_read_gamma;
	vdd->panel_func.samsung_print_gamma_comp = ss_print_gamma_comp;
	vdd->panel_func.read_flash = ss_read_flash;

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
	vdd->panel_func.samsung_gct_write = NULL;
	vdd->panel_func.samsung_gct_read = NULL;
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

	/* FFC */
	vdd->panel_func.set_ffc = ss_set_ffc;

	/* OSC */
	vdd->panel_func.update_osc = ss_update_osc;

	/* VRR */
	ss_vrr_init(&vdd->vrr);

	vdd->panel_func.samsung_check_support_mode = ss_check_support_mode;

//	vdd->debug_data->print_cmds = true;

	LCD_INFO(vdd, "S6E3FAC_AMB606AW01 : -- \n");
}
