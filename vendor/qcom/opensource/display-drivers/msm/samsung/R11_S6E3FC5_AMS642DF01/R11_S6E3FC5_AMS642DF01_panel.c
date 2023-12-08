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
#include "R11_S6E3FC5_AMS642DF01_panel.h"
#include "R11_S6E3FC5_AMS642DF01_mdnie.h"

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
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x08;
		else
			vrr_cmds->cmds[idx].ss_txbuf[1] = 0x00;
	}

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
	int cur_rr;
	bool cur_hs, cur_phs, dim_off = false;

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
		dim_off = true;
	}

	LCD_INFO(vdd, "NORMAL : cd_idx [%d] cur_md [%d] base [%d]\n",
		vdd->br_info.common_br.cd_idx, cur_md, cur_md_base);

	/* TSET */
	idx = ss_get_cmd_idx(pcmds, 0x01, 0xB5);
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

	/* Smooth dim */
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
			pcmds->cmds[idx].ss_txbuf[1] = 0x03;	/* 0x03 : ACL 15% */
		else
			pcmds->cmds[idx].ss_txbuf[1] = 0x01;	/* 0x01 : ACL 8%, D.Lab Request(21.10.13) */
	else
		pcmds->cmds[idx].ss_txbuf[1] = 0x01;		/* 0x01 : ACL 8% */

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

void R11_S6E3FC5_AMS642DF01_FHD_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "R11_S6E3FC5_AMS642DF01 : ++\n");
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
	vdd->panel_func.samsung_octa_id_read = NULL;
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

	/* HBM */
	vdd->panel_func.br_func[BR_FUNC_HBM_GAMMA] = ss_brightness_gamma;
	vdd->panel_func.br_func[BR_FUNC_HBM_VRR] = ss_vrr;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_ON] = ss_acl_on;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_OFF] = ss_acl_off;

	/* default brightness */
	vdd->br_info.common_br.bl_level = 255;

	/* mdnie */
	vdd->mdnie.support_mdnie = false;		// JUN_TEMP
	vdd->mdnie.support_trans_dimming = false;	// JUN_TEMP
	vdd->mdnie.mdnie_tune_size[0] = sizeof(BYPASS_MDNIE_1);
	vdd->mdnie.mdnie_tune_size[1] = sizeof(BYPASS_MDNIE_2);
	vdd->mdnie.mdnie_tune_size[2] = sizeof(BYPASS_MDNIE_3);

	dsi_update_mdnie_data(vdd);

	/* ACL default ON */
	vdd->br_info.acl_status = 1;

	/* Default br_info.temperature */
	vdd->br_info.temperature = 20;

	/* ACL default status in acl on */
	vdd->br_info.gradual_acl_val = 1;

	/* VRR */
	ss_vrr_init(&vdd->vrr);

	vdd->panel_func.samsung_check_support_mode = ss_check_support_mode;

	vdd->debug_data->print_cmds = true;
	LCD_INFO(vdd, "S6E3FAC_AMB606AW01 : --\n");
}
