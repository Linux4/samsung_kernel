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
Copyright (C) 2023, Samsung Electronics. All rights reserved.

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
#include "R11_S6E3FC3_AMS642DF03_panel.h"
#include "R11_S6E3FC3_AMS642DF03_mdnie.h"

/* AOD Mode status on AOD Service */

enum {
	HLPM_CTRL_2NIT,
	HLPM_CTRL_10NIT,
	HLPM_CTRL_30NIT,
	HLPM_CTRL_60NIT,
	MAX_LPM_CTRL,
};

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
	        LCD_ERR(vdd, ": Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return false;
	}

	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);
	ss_panel_attach_set(vdd, true);

	return true;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{

	/* Module info */
	if (!vdd->module_info_loaded_dsi && vdd->panel_func.samsung_module_info_read) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_module_info_read))
			LCD_INFO(vdd, "no samsung_module_info_read function\n");
		else
			vdd->module_info_loaded_dsi = vdd->panel_func.samsung_module_info_read(vdd);
	}

	/* Manufacture date */
	if (!vdd->manufacture_date_loaded_dsi && vdd->panel_func.samsung_manufacture_date_read) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_manufacture_date_read))
			LCD_INFO(vdd, "no samsung_manufacture_date_read function\n");
		else
			vdd->manufacture_date_loaded_dsi = vdd->panel_func.samsung_manufacture_date_read(vdd);
	}

	/* DDI ID */
	if (!vdd->ddi_id_loaded_dsi && vdd->panel_func.samsung_ddi_id_read) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_ddi_id_read))
			LCD_INFO(vdd, "no samsung_ddi_id_read function\n");
		else
			vdd->ddi_id_loaded_dsi = vdd->panel_func.samsung_ddi_id_read(vdd);
	}

	/* MDNIE X,Y (1.Manufacture Date -> 2.MDNIE X,Y -> 3.Cell ID -> 4.OCTA ID) */
	if (!vdd->mdnie_loaded_dsi && vdd->mdnie.support_mdnie && vdd->panel_func.samsung_mdnie_read) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_mdnie_read))
			LCD_INFO(vdd, "no samsung_mdnie_read function\n");
		else
			vdd->mdnie_loaded_dsi = vdd->panel_func.samsung_mdnie_read(vdd);
	}

	/* Panel Unique Cell ID (1.Manufacture Date -> 2.MDNIE X,Y -> 3.Cell ID -> 4.OCTA ID) */
	if (!vdd->cell_id_loaded_dsi && vdd->panel_func.samsung_cell_id_read) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_cell_id_read))
			LCD_INFO(vdd, "no samsung_cell_id_read function\n");
		else
			vdd->cell_id_loaded_dsi = vdd->panel_func.samsung_cell_id_read(vdd);
	}

	/* Panel Unique OCTA ID (1.Manufacture Date -> 2.MDNIE X,Y -> 3.Cell ID -> 4.OCTA ID) */
	if (!vdd->octa_id_loaded_dsi && vdd->panel_func.samsung_octa_id_read) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_octa_id_read))
			LCD_INFO(vdd, "no samsung_octa_id_read function\n");
		else
			vdd->octa_id_loaded_dsi = vdd->panel_func.samsung_octa_id_read(vdd);
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
	case 0x2:
	case 0x3:
	case 0x4:
	case 0x5:
		vdd->panel_revision = 'A';
		break;
	default:
		vdd->panel_revision = 'A';
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
	case 60:
		if (phs)
			vrr_id = VRR_60PHS;
		else
			vrr_id = VRR_60HS;
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
	case 60:
		if (phs)
			vrr_base = VRR_120HS;
		else
			vrr_base = VRR_60HS;
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

	/* FREQ */
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0x60);
	if (idx >= 0) {
		if (cur_md_base == VRR_60HS)
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x00;
		else
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x08;
	}

	/* TE_SEL */
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0xB9);
	if (idx >= 0) {
		if (cur_md == VRR_60PHS)
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x11;
		else
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x00;
	}

	/* Intermediate delay should not proceed when fingerprint situation*/
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0xF7);
	if (idx >= 0) {
		if (vdd->finger_mask_updated) {
			LCD_DEBUG(vdd, " Previous F7 post delay:%d ->0\n", vrr_cmds->cmds[idx].post_wait_ms);
			vrr_cmds->cmds[idx].post_wait_ms = 0x00;
		}
		else
			vrr_cmds->cmds[idx].post_wait_ms = 0x04;
	}

	LCD_INFO(vdd, "VRR: (prev: %d%s -> cur: %dx%d@%d%s) brightness_lock (%d)\n",
			prev_rr, prev_hs ? (prev_phs ? "PHS" : "HS") : "NM",
			panel->cur_mode->timing.h_active,
			panel->cur_mode->timing.v_active,
			cur_rr, cur_hs ? (cur_phs ? "PHS" : "HS") : "NM",
			vdd->need_brightness_lock);

	return vrr_cmds;
}

#define HBM_NORMAL_DELAY_60FPS (34)
#define HBM_NORMAL_DELAY_120FPS (17)
static bool last_br_hbm;

static struct dsi_panel_cmd_set * ss_brightness_gamma(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	enum VRR_CMD_RR cur_md, cur_md_base;
	int idx = 0;
	int cur_rr;
	bool cur_hs, cur_phs;
	bool smooth_trans = true;
	bool is_hbm = vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL ? true : false;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx\n", (size_t)vdd);
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

	if (vdd->vrr.running_vrr || !vdd->display_on) {
		smooth_trans = false;
	}

	LCD_INFO(vdd, "%s : cd_idx [%d] cur_md [%d] base [%d]\n",
		is_hbm ? "HBM" : "NORMAL", vdd->br_info.common_br.cd_idx, cur_md, cur_md_base);

	/* TSET */
	idx = ss_get_cmd_idx(pcmds, 0x00, 0xB5);
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
	idx = ss_get_cmd_idx(pcmds, 0x03, 0x8F);
	if (idx >= 0) {
		if (vdd->br_info.common_br.irc_mode == IRC_MODERATO_MODE)
			memcpy(&pcmds->cmds[idx].ss_txbuf[1], IRC_MODERATO, sizeof(IRC_MODERATO));
		else if (vdd->br_info.common_br.irc_mode == IRC_FLAT_GAMMA_MODE)
			memcpy(&pcmds->cmds[idx].ss_txbuf[1], IRC_FLAT, sizeof(IRC_FLAT));
	}

	if ((vdd->br_info.common_br.prev_bl_level <= MAX_BL_PF_LEVEL
			&& vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL) ||
			(vdd->br_info.common_br.prev_bl_level > MAX_BL_PF_LEVEL
			&& vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL)) {
		smooth_trans = false;
		LCD_DEBUG(vdd, "NORMAL<>HBM change, %d -> %d\n",
			vdd->br_info.common_br.prev_bl_level, vdd->br_info.common_br.bl_level);
	}

	if (vdd->panel_hbm_exit_frame_wait) {
		LCD_INFO(vdd, "[FINGER_MASK]BACKLIGHT_FINGERMASK_OFF is not finished yet, wait vsync\n");
		ss_wait_for_te_gpio(vdd, 1, vdd->panel_hbm_entry_after_te, false);
	}

	/* Fingerprint HBM Timing Tunning */
	switch (cur_md) {
	case VRR_120HS:
		/* TE Duration : 230us / 1 Frame = 8300us */
		vdd->panel_hbm_entry_after_te = 300;
		vdd->panel_hbm_delay_after_tx = 8500;
		break;
	case VRR_60HS:
		/* TE Duration : 8300us / 1 Frame = 16660us */
		vdd->panel_hbm_entry_after_te = 8600;
		vdd->panel_hbm_delay_after_tx = 18000;
		break;
	case VRR_60PHS:
		/* TE Duration : 230us / 1 Frame = 8300us */
		vdd->panel_hbm_entry_after_te = 300;
		vdd->panel_hbm_delay_after_tx = 8500;
		break;
	default:
		LCD_ERR(vdd, "Invalid mode value(%d)\n", cur_md);
		break;
	}

	/* Smooth dim */
	idx = ss_get_cmd_idx(pcmds, 0x091, 0x63);
	pcmds->cmds[idx].ss_txbuf[1] = smooth_trans ? 0x60 : 0x20;
	idx = ss_get_cmd_idx(pcmds, 0x092, 0x63);
	pcmds->cmds[idx].ss_txbuf[1] = vdd->display_on ? 0x0F : 0x01;

	idx = ss_get_cmd_idx(pcmds, 0x00, 0x53);
	if (vdd->finger_mask_updated) {
		pcmds->cmds[idx].ss_txbuf[1] = 0x20;
		LCD_INFO(vdd, "%s - FINGER UPDATED\n", is_hbm ? "HBM" : "NORMAL");
	} else {
		if (!smooth_trans)
			pcmds->cmds[idx].ss_txbuf[1] = 0x20;
		else if (vdd->br_info.common_br.prev_bl_level > MAX_BL_PF_LEVEL
			&& vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL)
			pcmds->cmds[idx].ss_txbuf[1] = 0x20;
		else
			pcmds->cmds[idx].ss_txbuf[1] = 0x28;
		LCD_INFO(vdd, "%s - NO FINGER\n", is_hbm ? "HBM" : "NORMAL");
	}

	LCD_DEBUG(vdd, "panel_hbm_entry_after_te = %d, panel_hbm_delay_after_tx = %d\n",
		vdd->panel_hbm_entry_after_te, vdd->panel_hbm_delay_after_tx);

	/* Intermediate delays should not proceed when fingerprint situation*/
	if (last_br_hbm == true && !vdd->finger_mask_updated) {
		LCD_INFO(vdd, "HBM_NORMAL_DELAY add\n");
		pcmds->cmds[6].last_command = 1;
		if (cur_md == VRR_60HS)
			pcmds->cmds[6].post_wait_ms = HBM_NORMAL_DELAY_60FPS;
		else
			pcmds->cmds[6].post_wait_ms = HBM_NORMAL_DELAY_120FPS;
	} else {
		pcmds->cmds[6].last_command = 0;
		pcmds->cmds[6].post_wait_ms = 0;
	}

	last_br_hbm = false;
	return pcmds;
}

static struct dsi_panel_cmd_set *ss_brightness_gamma_hbm(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	enum VRR_CMD_RR cur_md, cur_md_base;
	int idx = 0;
	int cur_rr;
	bool cur_hs, cur_phs;
	bool smooth_trans = true;
	bool is_hbm = vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL ? true : false;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return NULL;
	}

	cur_rr = vdd->vrr.cur_refresh_rate;
	cur_hs = vdd->vrr.cur_sot_hs_mode;
	cur_phs = vdd->vrr.cur_phs_mode;

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_HBM);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_INFO(vdd, "no gamma cmds\n");
		return NULL;
	}

	cur_md = ss_get_vrr_mode(vdd, cur_rr, cur_hs, cur_phs);
	cur_md_base = ss_get_vrr_mode_base(vdd, cur_rr, cur_hs, cur_phs);

	if (vdd->vrr.running_vrr || !vdd->display_on)
		smooth_trans = false;

	LCD_INFO(vdd, "%s : cd_idx [%d] cur_md [%d] base [%d]\n",
		is_hbm ? "HBM" : "NORMAL", vdd->br_info.common_br.cd_idx, cur_md, cur_md_base);

	/* TSET */
	idx = ss_get_cmd_idx(pcmds, 0x00, 0xB5);
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
	idx = ss_get_cmd_idx(pcmds, 0x03, 0x8F);
	if (idx >= 0) {
		if (vdd->br_info.common_br.irc_mode == IRC_MODERATO_MODE)
			memcpy(&pcmds->cmds[idx].ss_txbuf[1], IRC_MODERATO, sizeof(IRC_MODERATO));
		else if (vdd->br_info.common_br.irc_mode == IRC_FLAT_GAMMA_MODE)
			memcpy(&pcmds->cmds[idx].ss_txbuf[1], IRC_FLAT, sizeof(IRC_FLAT));
	}

	if ((vdd->br_info.common_br.prev_bl_level <= MAX_BL_PF_LEVEL
			&& vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL) ||
			(vdd->br_info.common_br.prev_bl_level > MAX_BL_PF_LEVEL
			&& vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL)) {
		smooth_trans = false;
		LCD_DEBUG(vdd, "NORMAL<>HBM change, %d -> %d\n",
			vdd->br_info.common_br.prev_bl_level, vdd->br_info.common_br.bl_level);
	}

	if (vdd->panel_hbm_exit_frame_wait) {
		LCD_INFO(vdd, "[FINGER_MASK]BACKLIGHT_FINGERMASK_OFF is not finished yet, wait vsync\n");
		ss_wait_for_te_gpio(vdd, 1, vdd->panel_hbm_entry_after_te, false);
	}

	/* Fingerprint HBM Timing Tunning */
	switch (cur_md) {
	case VRR_120HS:
		/* TE Duration : 230us / 1 Frame = 8300us */
		vdd->panel_hbm_entry_after_te = 300;
		vdd->panel_hbm_delay_after_tx = 8500;
		break;
	case VRR_60HS:
		/* TE Duration : 8300us / 1 Frame = 16660us */
		vdd->panel_hbm_entry_after_te = 8600;
		vdd->panel_hbm_delay_after_tx = 18000;
		break;
	case VRR_60PHS:
		/* TE Duration : 230us / 1 Frame = 8300us */
		vdd->panel_hbm_entry_after_te = 300;
		vdd->panel_hbm_delay_after_tx = 8500;
		break;
	default:
		LCD_ERR(vdd, "Invalid mode value(%d)\n", cur_md);
		break;
	}

	/* Smooth dim */
	idx = ss_get_cmd_idx(pcmds, 0x091, 0x63);
	pcmds->cmds[idx].ss_txbuf[1] = smooth_trans ? 0x60 : 0x20;
	idx = ss_get_cmd_idx(pcmds, 0x092, 0x63);
	pcmds->cmds[idx].ss_txbuf[1] = vdd->display_on ? 0x0F : 0x01;

	idx = ss_get_cmd_idx(pcmds, 0x00, 0x53);
	if (vdd->finger_mask_updated) {
		pcmds->cmds[idx].ss_txbuf[1] = 0x20;
		LCD_INFO(vdd, "%s - FINGER UPDATED\n", is_hbm ? "HBM" : "NORMAL");
	} else {
		if (!smooth_trans)
			pcmds->cmds[idx].ss_txbuf[1] = 0x20;
		else if (vdd->br_info.common_br.prev_bl_level > MAX_BL_PF_LEVEL
			&& vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL)
			pcmds->cmds[idx].ss_txbuf[1] = 0x20;
		else
			pcmds->cmds[idx].ss_txbuf[1] = 0x28;
		LCD_INFO(vdd, "%s - NO FINGER\n", is_hbm ? "HBM" : "NORMAL");
	}

	LCD_DEBUG(vdd, "panel_hbm_entry_after_te = %d, panel_hbm_delay_after_tx = %d\n",
		vdd->panel_hbm_entry_after_te, vdd->panel_hbm_delay_after_tx);

	last_br_hbm = true;
	return pcmds;
}

static void ss_set_panel_lpm_brightness(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *lpm_brt_cmd = ss_get_cmds(vdd, TX_LPM_BL_CMD);
	struct dsi_panel_cmd_set *lpm_bl_cmd;
	int lpm_aor_idx, gm2_wrdisbv, wrdisbv_idx;

	if (SS_IS_CMDS_NULL(lpm_brt_cmd)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_BL_CMD\n");
		return;
	}

	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_60NIT:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
		gm2_wrdisbv = 255;
		break;
	case LPM_30NIT:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
		gm2_wrdisbv = 133;
		break;
	case LPM_10NIT:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
		gm2_wrdisbv = 43;
		break;
	case LPM_2NIT:
	default:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
		gm2_wrdisbv = 8;
		break;
	}

	if (SS_IS_CMDS_NULL(lpm_bl_cmd)) {
		LCD_ERR(vdd, "No cmds for HLPM/ALPM control brightness..\n");
		return;
	}

	lpm_aor_idx = ss_get_cmd_idx(lpm_brt_cmd, 0x77, 0x63);

	lpm_brt_cmd->cmds[lpm_aor_idx].ss_txbuf[1] = lpm_bl_cmd->cmds->ss_txbuf[1];
	lpm_brt_cmd->cmds[lpm_aor_idx].ss_txbuf[2] = lpm_bl_cmd->cmds->ss_txbuf[2];

	wrdisbv_idx = ss_get_cmd_idx(lpm_brt_cmd, 0x00, 0x51);

	lpm_brt_cmd->cmds[wrdisbv_idx].ss_txbuf[1] = (gm2_wrdisbv & 0xFF00) >> 8;
	lpm_brt_cmd->cmds[wrdisbv_idx].ss_txbuf[2] = gm2_wrdisbv & 0xFF;

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
	int lpm_aor_idx, gm2_wrdisbv, wrdisbv_idx;

	if (enable)
		lpm_cmd = ss_get_cmds(vdd, TX_LPM_ON);
	else
		return;

	if (SS_IS_CMDS_NULL(lpm_cmd)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_ON\n");
		return;
	}

	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_60NIT:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
		gm2_wrdisbv = 255;
		break;
	case LPM_30NIT:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
		gm2_wrdisbv = 133;
		break;
	case LPM_10NIT:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
		gm2_wrdisbv = 43;
		break;
	case LPM_2NIT:
	default:
		lpm_bl_cmd = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
		gm2_wrdisbv = 8;
		break;
	}

	if (SS_IS_CMDS_NULL(lpm_bl_cmd)) {
		LCD_ERR(vdd, "No cmds for HLPM/ALPM control brightness..\n");
		return;
	}

	lpm_aor_idx = ss_get_cmd_idx(lpm_cmd, 0x77, 0x63);

	lpm_cmd->cmds[lpm_aor_idx].ss_txbuf[1] = lpm_bl_cmd->cmds->ss_txbuf[1];
	lpm_cmd->cmds[lpm_aor_idx].ss_txbuf[2] = lpm_bl_cmd->cmds->ss_txbuf[2];

	wrdisbv_idx = ss_get_cmd_idx(lpm_cmd, 0x00, 0x51);
	lpm_cmd->cmds[wrdisbv_idx].ss_txbuf[1] = (gm2_wrdisbv & 0xFF00) >> 8;
	lpm_cmd->cmds[wrdisbv_idx].ss_txbuf[2] = gm2_wrdisbv & 0xFF;
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
	{0xff, 0x00, 0xf8, 0x00, 0xe9, 0x00}, /* Tune_5 */
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
	mdnie_data->DSI_NIGHT_MODE_MDNIE_SCR = NIGHT_MODE_MDNIE_2;
	mdnie_data->DSI_COLOR_LENS_MDNIE = COLOR_LENS_MDNIE;
	mdnie_data->DSI_COLOR_LENS_MDNIE_SCR = COLOR_LENS_MDNIE_2;
	mdnie_data->DSI_COLOR_BLIND_MDNIE_SCR = COLOR_BLIND_MDNIE_2;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_SCR = RGB_SENSOR_MDNIE_2;

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
	mdnie_data->dsi_scr_step_index = MDNIE_STEP2_INDEX;
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
	struct dsi_panel_cmd_set *pcmds_octaid;
	struct dsi_panel_cmd_set *pcmds_cellid;
	char *read_buf;
	int len = 0;
	u8 temp[50];
	int ocat_id_len;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}
	/* Read Panel Unique OCTA ID (A1 12th ~ 15th(4bytes) + Cell ID(16bytes)) */
	pcmds_octaid = ss_get_cmds(vdd, RX_OCTA_ID);
	pcmds_cellid = ss_get_cmds(vdd, RX_CELL_ID);

	if (pcmds_octaid->count && pcmds_cellid->count) {
		ocat_id_len = pcmds_octaid->cmds[0].msg.rx_len + pcmds_cellid->cmds[0].msg.rx_len;
		read_buf = kzalloc(ocat_id_len, GFP_KERNEL);
		if (!read_buf) {
			LCD_ERR(vdd, "fail to kzalloc for buf\n");
			return false;
		}

		ss_panel_data_read(vdd, RX_OCTA_ID, read_buf, LEVEL1_KEY);	/* Read A1h 12th ~ 15th */
		ss_panel_data_read(vdd, RX_CELL_ID, read_buf + 4, LEVEL1_KEY);	/* Read 92h 3rd ~ 18th */

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
			strlcat(vdd->octa_id_dsi, temp, len);
			LCD_INFO(vdd, "[%d] %s\n", vdd->octa_id_len, vdd->octa_id_dsi);
		}
	} else {
		LCD_ERR(vdd, "DSI%d no octa_id_rx_cmds cmd\n", vdd->ndx);
		return false;
	}

	kfree(read_buf);

	return true;
}

static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	char *read_buf;
	int i, len = 0;
	u8 temp[20];
	int ddi_id_len;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx\n", (size_t)vdd);
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
		LCD_ERR(vdd, "DSI%d no ddi_id_rx_cmds cmds\n", vdd->ndx);
		return false;
	}

	kfree(read_buf);
	return true;
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
	make_mass_self_display_img_cmds_FC3(vdd, TX_SELF_MASK_IMAGE, FLAG_SELF_MASK);

	if (vdd->is_factory_mode) {
		vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_buf = self_mask_img_crc_data;
		vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_size = ARRAY_SIZE(self_mask_img_crc_data);
		make_mass_self_display_img_cmds_FC3(vdd, TX_SELF_MASK_IMAGE_CRC, FLAG_SELF_MASK_CRC);
	}

	LCD_INFO(vdd, "--\n");
	return 1;
}


static struct dsi_panel_cmd_set *ss_acl_on(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	struct vrr_info *vrr = &vdd->vrr;
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
		if (vdd->br_info.gradual_acl_val == 2 || vdd->br_info.gradual_acl_val == 3)
			pcmds->cmds[idx].ss_txbuf[1] = 0x02;	/* 0x02 : ACL 15% */
		else
			pcmds->cmds[idx].ss_txbuf[1] = 0x01;	/* 0x01 : ACL 8% */
	else if (vdd->finger_mask_updated)
		pcmds->cmds[idx].ss_txbuf[1] = 0x00;		/* 0x00 : ACL OFF */
	else
		pcmds->cmds[idx].ss_txbuf[1] = 0x01;		/* 0x01 : ACL 8% */

	if (vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL
			&& vdd->br_info.common_br.prev_bl_level > MAX_BL_PF_LEVEL
			&& vdd->finger_mask_updated
			&& vrr->cur_refresh_rate > 60)
		pcmds->cmds[idx].post_wait_ms = 3;
	else
		pcmds->cmds[idx].post_wait_ms = 0;

	LCD_DEBUG(vdd, "bl_level: %d, gradual_acl: %d, acl per: 0x%x", vdd->br_info.common_br.bl_level,
			vdd->br_info.gradual_acl_val, pcmds->cmds[0].ss_txbuf[1]);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_acl_off(struct samsung_display_driver_data *vdd, int *level_key)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return NULL;
	}

	LCD_DEBUG(vdd, "off\n");
	return ss_get_cmds(vdd, TX_ACL_OFF);
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
	default:
		break;
	}
	return is_support;
}

#define FFC_CMD_LEN (4)
#define FFC_START_IDX (2)
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

static void make_brightness_packet(struct samsung_display_driver_data *vdd,
	struct dsi_cmd_desc *packet, int *cmd_cnt, enum BR_TYPE br_type)
{
	int loop;
	int start = *cmd_cnt;

	if (br_type == BR_TYPE_NORMAL || br_type == BR_TYPE_HBM) {
		/* VRR */
		ss_add_brightness_packet(vdd, BR_FUNC_VRR, packet, cmd_cnt);

		/* ACL */
		if (vdd->br_info.acl_status || vdd->siop_status)
			ss_add_brightness_packet(vdd, BR_FUNC_ACL_ON, packet, cmd_cnt);
		else
			ss_add_brightness_packet(vdd, BR_FUNC_ACL_OFF, packet, cmd_cnt);

		/* gamma */
		if (br_type == BR_TYPE_HBM)
			ss_add_brightness_packet(vdd, BR_FUNC_HBM_GAMMA, packet, cmd_cnt);
		else
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

void R11_S6E3FC3_AMS642DF03_FHD_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "R11_S6E3FC3_AMS642DF03 : ++\n");
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
	vdd->panel_func.samsung_elvss_read = NULL;

	/* Make brightness packer */
	vdd->panel_func.make_brightness_packet = make_brightness_packet;

	/* Brightness */
	vdd->panel_func.br_func[BR_FUNC_GAMMA] = ss_brightness_gamma;
	vdd->panel_func.br_func[BR_FUNC_ACL_ON] = ss_acl_on;
	vdd->panel_func.br_func[BR_FUNC_ACL_OFF] = ss_acl_off;
	vdd->panel_func.br_func[BR_FUNC_VINT] = NULL;
	vdd->panel_func.br_func[BR_FUNC_VRR] = ss_vrr;

	/* Total level key in brightness */
	vdd->panel_func.samsung_brightness_tot_level = ss_brightness_tot_level;

	/* Panel LPM */
	vdd->panel_func.samsung_update_lpm_ctrl_cmd = ss_update_panel_lpm_ctrl_cmd;
	vdd->panel_func.samsung_set_lpm_brightness = ss_set_panel_lpm_brightness;

	/* HBM */
	vdd->panel_func.br_func[BR_FUNC_HBM_GAMMA] = ss_brightness_gamma_hbm;
	vdd->panel_func.br_func[BR_FUNC_HBM_VRR] = ss_vrr;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_ON] = ss_acl_on;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_OFF] = ss_acl_off;

	/* default brightness */
	vdd->br_info.common_br.bl_level = 255;

	/* SAMSUNG_FINGERPRINT */
	vdd->panel_hbm_entry_delay = 0;
	vdd->panel_hbm_exit_delay = 0;

	/* mdnie */
	vdd->mdnie.support_mdnie = true;
	vdd->mdnie.support_trans_dimming = false;
	vdd->mdnie.mdnie_tune_size[0] = sizeof(BYPASS_MDNIE_1);
	vdd->mdnie.mdnie_tune_size[1] = sizeof(BYPASS_MDNIE_2);
	vdd->mdnie.mdnie_tune_size[2] = sizeof(BYPASS_MDNIE_3);
	vdd->mdnie.mdnie_tune_size[3] = sizeof(BYPASS_MDNIE_4);

	dsi_update_mdnie_data(vdd);

	/* ACL default ON */
	vdd->br_info.acl_status = 1;

	/* Default br_info.temperature */
	vdd->br_info.temperature = 20;

	/* Self display */
	vdd->self_disp.is_support = true;
	vdd->self_disp.factory_support = true;
	vdd->self_disp.init = self_display_init_FC3;
	vdd->self_disp.data_init = ss_self_display_data_init;

	/* ACL default status in acl on */
	vdd->br_info.gradual_acl_val = 1;

	/* VRR */
	ss_vrr_init(&vdd->vrr);

	/* FFC */
	vdd->panel_func.set_ffc = ss_set_ffc;

	vdd->panel_func.samsung_check_support_mode = ss_check_support_mode;

	vdd->debug_data->print_cmds = false;
	LCD_INFO(vdd, "R11_S6E3FC3_AMS642DF03 : --\n");
}
