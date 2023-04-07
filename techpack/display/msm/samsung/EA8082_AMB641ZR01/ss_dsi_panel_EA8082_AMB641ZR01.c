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
#include "ss_dsi_panel_EA8082_AMB641ZR01.h"
#include "ss_dsi_mdnie_EA8082_AMB641ZR01.h"

#define IRC_MODERATO_MODE_VAL	0x61
#define IRC_FLAT_GAMMA_MODE_VAL	0x21

static struct dsi_panel_cmd_set *__ss_vrr(struct samsung_display_driver_data *vdd,
					int *level_key, bool is_hbm, bool is_hmt)
{
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct dsi_panel_cmd_set  *vrr_cmds = ss_get_cmds(vdd, TX_VRR);

	struct vrr_info *vrr = &vdd->vrr;
	enum SS_BRR_MODE brr_mode = vrr->brr_mode;

	int cur_rr;
	bool cur_hs;

	if (SS_IS_CMDS_NULL(vrr_cmds)) {
		LCD_INFO(vdd, "no vrr cmds\n");
		return NULL;
	}

	if (panel && panel->cur_mode) {
		LCD_INFO(vdd, "VRR: cur_mode: %dx%d@%d%s, is_hbm: %d, is_hmt: %d, %s\n",
				panel->cur_mode->timing.h_active,
				panel->cur_mode->timing.v_active,
				panel->cur_mode->timing.refresh_rate,
				panel->cur_mode->timing.sot_hs_mode ? "HS" : "NM",
				is_hbm, is_hmt, ss_get_brr_mode_name(brr_mode));

		if (panel->cur_mode->timing.refresh_rate != vdd->vrr.adjusted_refresh_rate ||
				panel->cur_mode->timing.sot_hs_mode != vdd->vrr.adjusted_sot_hs_mode)
			LCD_ERR(vdd, "VRR: unmatched RR mode (%dHz%s / %dHz%s)\n",
					panel->cur_mode->timing.refresh_rate,
					panel->cur_mode->timing.sot_hs_mode ? "HS" : "NM",
					vdd->vrr.adjusted_refresh_rate,
					vdd->vrr.adjusted_sot_hs_mode ? "HS" : "NM");
	}

	cur_rr = vrr->cur_refresh_rate;
	cur_hs = vrr->cur_sot_hs_mode;

	if (vdd->vrr.adjusted_refresh_rate == 120)
		vrr_cmds->cmds[1].ss_txbuf[1] = 0x08;
	else
		vrr_cmds->cmds[1].ss_txbuf[1] = 0x00;

	LCD_INFO(vdd, "VRR: %s, FPS: %dHz%s (cur: %d%s, target: %d%s)\n",
			ss_get_brr_mode_name(brr_mode),
			cur_rr,
			cur_hs ? "HS" : "NM",
			vrr->cur_refresh_rate,
			vrr->cur_sot_hs_mode ? "HS" : "NM",
			vrr->adjusted_refresh_rate,
			vrr->adjusted_sot_hs_mode ? "HS" : "NM");

	return vrr_cmds;
}

static struct dsi_panel_cmd_set *ss_vrr(struct samsung_display_driver_data *vdd, int *level_key)
{
	bool is_hbm = false;
	bool is_hmt = false;

	return __ss_vrr(vdd, level_key, is_hbm, is_hmt);
}

static struct dsi_panel_cmd_set *ss_vrr_hbm(struct samsung_display_driver_data *vdd, int *level_key)
{
	bool is_hbm = true;
	bool is_hmt = false;

	return __ss_vrr(vdd, level_key, is_hbm, is_hmt);
}

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
	return true;
}

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{
	if (vdd->manufacture_id_dsi == PBA_ID)
		ss_panel_attach_set(vdd, false);
	else
		ss_panel_attach_set(vdd, true);

	switch (ss_panel_rev_get(vdd)) {
	case 0x00:
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

#define FRAME_WAIT_60FPS (17)
#define FRAME_WAIT_120FPS (9)
#define NORMAL_HBM_DELAY_120FPS (9)
#define NORMAL_HBM_DELAY_60FPS (9)
#define HBM_NORMAL_DELAY_60FPS (14)
#define HBM_NORMAL_DELAY_120FPS (14)

static bool last_br_hbm;
static bool finger_exit_cnt;

#define get_bit(value, shift, width)	((value >> shift) & (GENMASK(width - 1, 0)))
static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2_normal(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int cmd_idx;

	if (IS_ERR_OR_NULL(vdd)) {
	        LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}
	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_NORMAL);
	LCD_INFO(vdd, "Normal : cd_idx [%d] \n", vdd->br_info.common_br.cd_idx);

	cmd_idx = ss_get_cmd_idx(pcmds, 0x00, 0x53);
	pcmds->cmds[cmd_idx].ss_txbuf[1] = (vdd->finger_mask_updated || finger_exit_cnt) ? 0x20 : 0x28;	/* Normal (0x20) / Smooth (0x28) */
	cmd_idx = ss_get_cmd_idx(pcmds, 0x00, 0x51);
	pcmds->cmds[cmd_idx].ss_txbuf[1] = get_bit(vdd->br_info.common_br.gm2_wrdisbv, 8, 2);
	pcmds->cmds[cmd_idx].ss_txbuf[2] = get_bit(vdd->br_info.common_br.gm2_wrdisbv, 0, 8);

	/* ELVSS TSET */
	cmd_idx = ss_get_cmd_idx(pcmds, 0x99, 0xDF);
	pcmds->cmds[cmd_idx].ss_txbuf[1] = vdd->br_info.temperature > 0 ? vdd->br_info.temperature : (char)(BIT(7) | (-1*vdd->br_info.temperature));

	if (vdd->finger_mask_updated) {
		/* There is panel limitation for HBM setting. */
		cmd_idx = ss_get_cmd_idx(pcmds, 0x00, 0x53);
		pcmds->cmds[cmd_idx].last_command = 1;

		if (vdd->vrr.cur_refresh_rate > 60)
			pcmds->cmds[cmd_idx].post_wait_ms = HBM_NORMAL_DELAY_120FPS;
		else
			pcmds->cmds[cmd_idx].post_wait_ms = HBM_NORMAL_DELAY_60FPS;
		finger_exit_cnt = 1;
	} else {
		cmd_idx = ss_get_cmd_idx(pcmds, 0x00, 0x53);
		pcmds->cmds[cmd_idx].last_command = 0;
		pcmds->cmds[cmd_idx].post_wait_ms = 0;
		finger_exit_cnt = 0;
	}
	last_br_hbm = false;
	*level_key = LEVEL1_KEY;
	return pcmds;
}

static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2_hbm(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	struct dsi_panel_cmd_set *pcmds_smooth_off;
	int cmd_idx;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_HBM);
	pcmds_smooth_off = ss_get_cmds(vdd, TX_SMOOTH_DIMMING_OFF);

	LCD_INFO(vdd, "HBM : cd_idx [%d]\n", vdd->br_info.common_br.cd_idx);

	cmd_idx = ss_get_cmd_idx(pcmds, 0x00, 0x53);
	pcmds->cmds[cmd_idx].ss_txbuf[1] = vdd->finger_mask_updated? 0xE0 : 0xE8;	/* Normal (0x20) / Smooth (0x28) */
	cmd_idx = ss_get_cmd_idx(pcmds, 0x00, 0x51);
	pcmds->cmds[cmd_idx].ss_txbuf[1] = get_bit(vdd->br_info.common_br.gm2_wrdisbv, 8, 2);
	pcmds->cmds[cmd_idx].ss_txbuf[2] = get_bit(vdd->br_info.common_br.gm2_wrdisbv, 0, 8);

	/* ELVSS TSET */
	cmd_idx = ss_get_cmd_idx(pcmds, 0x99, 0xDF);
	pcmds->cmds[cmd_idx].ss_txbuf[1] = vdd->br_info.temperature > 0 ? vdd->br_info.temperature : (char)(BIT(7) | (-1*vdd->br_info.temperature));

	if (vdd->finger_mask_updated) {
			if (last_br_hbm == false) { /* Normal -> HBM Case Only */
				cmd_idx = ss_get_cmd_idx(pcmds_smooth_off, 0x00, 0x53);
				/* Smooth Dimming Off First */
				if (vdd->vrr.cur_refresh_rate > 60)
					pcmds_smooth_off->cmds[cmd_idx].post_wait_ms = FRAME_WAIT_120FPS;
				else
					pcmds_smooth_off->cmds[cmd_idx].post_wait_ms = FRAME_WAIT_60FPS;
				ss_send_cmd(vdd, TX_SMOOTH_DIMMING_OFF);
			}
		cmd_idx = ss_get_cmd_idx(pcmds, 0x99, 0xDF);

		pcmds->cmds[cmd_idx].last_command = 1;
		if (vdd->vrr.cur_refresh_rate > 60)
			pcmds->cmds[cmd_idx].post_wait_ms = NORMAL_HBM_DELAY_120FPS;
		else
			pcmds->cmds[cmd_idx].post_wait_ms = NORMAL_HBM_DELAY_60FPS;
	} else {
		cmd_idx = ss_get_cmd_idx(pcmds, 0x99, 0xDF);
		pcmds->cmds[cmd_idx].last_command = 0;
		pcmds->cmds[cmd_idx].post_wait_ms = 0;
	}
	*level_key = LEVEL1_KEY;
	last_br_hbm = true;

	return pcmds;
}

static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2_hmt(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int cmd_idx;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_HMT);

	LCD_INFO(vdd, "HBM : cd_idx [%d]\n", vdd->br_info.common_br.cd_idx);

	cmd_idx = ss_get_cmd_idx(pcmds, 0x00, 0x51);
	pcmds->cmds[cmd_idx].ss_txbuf[1] = get_bit(vdd->br_info.common_br.gm2_wrdisbv, 8, 2);
	pcmds->cmds[cmd_idx].ss_txbuf[2] = get_bit(vdd->br_info.common_br.gm2_wrdisbv, 0, 8);

	LCD_INFO(vdd, "cd_idx: %d, cd_level: %d, WRDISBV: %x %x\n",
			vdd->hmt.cmd_idx,
			vdd->hmt.candela_level,
			pcmds->cmds[cmd_idx].ss_txbuf[1],
			pcmds->cmds[cmd_idx].ss_txbuf[2]);

	return pcmds;
}

static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	char ddi_id[6];
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (D1h 96th~101st) for CHIP ID */
	if (ss_get_cmds(vdd, RX_DDI_ID)->count) {
		ss_panel_data_read(vdd, RX_DDI_ID, ddi_id, LEVEL1_KEY);

		for (loop = 0; loop < 6; loop++)
			vdd->ddi_id_dsi[loop] = ddi_id[loop];

		LCD_INFO(vdd, "DSI%d : %02x %02x %02x %02x %02x %02x\n", vdd->ndx,
			vdd->ddi_id_dsi[0], vdd->ddi_id_dsi[1],
			vdd->ddi_id_dsi[2], vdd->ddi_id_dsi[3],
			vdd->ddi_id_dsi[4], vdd->ddi_id_dsi[5]);
	} else {
		LCD_INFO(vdd, "DSI%d no ddi_id_rx_cmds cmds", vdd->ndx);
		return false;
	}

	return true;
}

static int ss_module_info_read(struct samsung_display_driver_data *vdd)
{
	unsigned char buf[11];
	int year, month, day;
	int hour, min;
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

	/* Read Panel Unique OCTA ID (EAh 15th ~ 34th) */
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

static int ss_elvss_read(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (B7h 8th,9th) for elvss*/
	ss_panel_data_read(vdd, RX_ELVSS, vdd->br_info.common_br.elvss_value, LEVEL1_KEY);

	return true;
}

static void ss_gray_spot(struct samsung_display_driver_data *vdd, int enable)
{
	char gray_spot_buf[3];
	struct dsi_panel_cmd_set *pcmds_on, *pcmds_off;
	int idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return;
	}

	if (enable) {
		pcmds_on = ss_get_cmds(vdd, TX_GRAY_SPOT_TEST_ON);
		pcmds_off = ss_get_cmds(vdd, TX_GRAY_SPOT_TEST_OFF);

		ss_panel_data_read(vdd, RX_GRAYSPOT_RESTORE_VALUE, gray_spot_buf, LEVEL1_KEY);
		idx = ss_get_cmd_idx(pcmds_on, 0x11, 0xB4);
		/* TX_GRAY_SPOT_TEST_ON : replace 0x35, 0xXX, 0xXX, 0x00 */
		memcpy(&pcmds_on->cmds[idx].ss_txbuf[2],
				&gray_spot_buf[1],
				sizeof(char) * (pcmds_on->cmds[idx].msg.tx_len - 3));
		idx = ss_get_cmd_idx(pcmds_off, 0x11, 0xB4);
		/* TX_GRAY_SPOT_TEST_OFF : replace 0xXX, 0xXX, 0xXX, 0xXX */
		memcpy(&pcmds_off->cmds[idx].ss_txbuf[1],
				gray_spot_buf,
				sizeof(char) * (pcmds_off->cmds[idx].msg.tx_len - 1));
	} else {
		/* Nothing to do */
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
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE_2 = NULL;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE_2 = NULL;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE_2 = NULL;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE_2 = DSI0_CAMERA_AUTO_MDNIE_2;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE_2 = NULL;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE_2 = NULL;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE_2 = DSI0_GALLERY_AUTO_MDNIE_2;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE_2 = NULL;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE_2 = NULL;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE_2 = NULL;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE_2 = NULL;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE_2 = NULL;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE_2 = DSI0_EBOOK_AUTO_MDNIE_2;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE_2 = NULL;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE_2 = NULL;
	mdnie_data->DSI_TDMB_AUTO_MDNIE_2 = NULL;

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
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE = NULL;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE = NULL;
	mdnie_data->DSI_VIDEO_NATURAL_MDNIE = NULL;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE = NULL;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE = DSI0_CAMERA_AUTO_MDNIE;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE = NULL;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE = NULL;
	mdnie_data->DSI_GALLERY_NATURAL_MDNIE = NULL;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE = DSI0_GALLERY_AUTO_MDNIE;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE = NULL;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE = NULL;
	mdnie_data->DSI_BROWSER_NATURAL_MDNIE = NULL;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE = NULL;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE = NULL;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE = NULL;
	mdnie_data->DSI_EBOOK_NATURAL_MDNIE = NULL;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE = DSI0_EBOOK_AUTO_MDNIE;
	mdnie_data->DSI_EMAIL_AUTO_MDNIE = DSI0_EMAIL_AUTO_MDNIE;
	mdnie_data->DSI_GAME_LOW_MDNIE = NULL;
	mdnie_data->DSI_GAME_MID_MDNIE = NULL;
	mdnie_data->DSI_GAME_HIGH_MDNIE = NULL;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE = NULL;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE = NULL;
	mdnie_data->DSI_TDMB_NATURAL_MDNIE = NULL;
	mdnie_data->DSI_TDMB_AUTO_MDNIE = NULL;
	mdnie_data->DSI_GRAYSCALE_MDNIE = DSI0_GRAYSCALE_MDNIE;
	mdnie_data->DSI_GRAYSCALE_NEGATIVE_MDNIE = DSI0_GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data->DSI_CURTAIN = DSI0_SCREEN_CURTAIN_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE = DSI0_NIGHT_MODE_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE_SCR = DSI0_NIGHT_MODE_MDNIE_2;
	mdnie_data->DSI_COLOR_LENS_MDNIE = DSI0_COLOR_LENS_MDNIE;
	mdnie_data->DSI_COLOR_LENS_MDNIE_SCR = DSI0_COLOR_LENS_MDNIE_2;
	mdnie_data->DSI_COLOR_BLIND_MDNIE_SCR = DSI0_COLOR_BLIND_MDNIE_2;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_SCR = DSI0_RGB_SENSOR_MDNIE_2;

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
	mdnie_data->dsi_scr_step_index = MDNIE_STEP2_INDEX;
	mdnie_data->dsi_afc_size = 45;
	mdnie_data->dsi_afc_index = 33;

	vdd->mdnie.mdnie_data = mdnie_data;

	return 0;
}

static struct dsi_panel_cmd_set *ss_acl_on_hbm(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;

	pcmds = ss_get_cmds(vdd, TX_ACL_ON);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_ACL_ON..\n");
		return NULL;
	}
	pcmds->cmds[0].ss_txbuf[1] = 0x01;	/* 55h 0x01 ACL 8% */

	LCD_INFO(vdd, "HBM: gradual_acl: %d, acl per: 0x%x",
			vdd->br_info.gradual_acl_val, pcmds->cmds[0].ss_txbuf[1]);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_acl_on(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;

	pcmds = ss_get_cmds(vdd, TX_ACL_ON);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_ACL_ON..\n");
		return NULL;
	}

	pcmds->cmds[0].ss_txbuf[1] = 0x01;	/* ACL 8% */

	LCD_INFO(vdd, "gradual_acl: %d, acl per: 0x%x",
			vdd->br_info.gradual_acl_val, pcmds->cmds[0].ss_txbuf[1]);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_acl_off(struct samsung_display_driver_data *vdd, int *level_key)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;
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

	idx = ss_get_cmd_idx(vint_cmds, 0x0C, 0xD3);

	if (vdd->xtalk_mode)
		vint_cmds->cmds[idx].ss_txbuf[1] = 0x0D; // ON
	else
		vint_cmds->cmds[idx].ss_txbuf[1] = 0x13; // OFF

	return vint_cmds;
}

static void ss_set_panel_lpm_brightness(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *set = ss_get_cmds(vdd, TX_LPM_BL_CMD);
	struct dsi_panel_cmd_set *set_lpm_bl;
	int cmd_idx;

	if (SS_IS_CMDS_NULL(set)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_BL_CMD\n");
		return;
	}

	/* LPM_ON: 3. HLPM brightness */
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
		LCD_ERR(vdd, "No cmds for alpm_ctrl..\n");
		return;
	}
	cmd_idx = ss_get_cmd_idx(set, 0x00, 0x53);
	memcpy(&set->cmds[cmd_idx].ss_txbuf[1],
			&set_lpm_bl->cmds->ss_txbuf[1],
			sizeof(char) * (set->cmds[cmd_idx].msg.tx_len - 1));
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
 * This function will update parameters for ALPM_REG/ALPM_CTRL_REG
 * Parameter for ALPM_REG : Control brightness for panel LPM
 * Parameter for ALPM_CTRL_REG : Change panel LPM mode for ALPM/HLPM
 * mode, brightness, hz are updated here.
 */
static void ss_update_panel_lpm_ctrl_cmd
				(struct samsung_display_driver_data *vdd, int enable)
{
	struct dsi_panel_cmd_set *set_lpm_on = ss_get_cmds(vdd, TX_LPM_ON);
	struct dsi_panel_cmd_set *set_lpm_off = ss_get_cmds(vdd, TX_LPM_OFF);
	struct dsi_panel_cmd_set *set_lpm_bl;
	int cmd_idx;

	LCD_INFO(vdd, "%s++\n", __func__);

	if (SS_IS_CMDS_NULL(set_lpm_on) || SS_IS_CMDS_NULL(set_lpm_off)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_ON/OFF\n");
		goto start_lpm_bl;
	}

start_lpm_bl:
	/* LPM_ON: 3. HLPM brightness */
	/* should restore normal brightness in LPM off sequence to prevent flicker.. */
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
		LCD_ERR(vdd, "No cmds for alpm_ctrl..\n");
		return;
	}
	cmd_idx = ss_get_cmd_idx(set_lpm_on, 0x00, 0x53);
	memcpy(&set_lpm_on->cmds[cmd_idx].ss_txbuf[1],
				&set_lpm_bl->cmds->ss_txbuf[1],
				sizeof(char) * set_lpm_on->cmds[cmd_idx].msg.tx_len - 1);
	LCD_INFO(vdd, "%s--\n", __func__);
}

static void ss_copr_panel_init(struct samsung_display_driver_data *vdd)
{
	ss_copr_init(vdd);
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

	LCD_INFO(vdd, "EA8082_AMB641ZR01 +++\n");

	mutex_init(&vrr->vrr_lock);
	mutex_init(&vrr->brr_lock);

	vrr->running_vrr_mdp = false;
	vrr->running_vrr = false;

	/* Bootloader: FHD@120hz HS mode */
	vrr->cur_refresh_rate = vrr->adjusted_refresh_rate = 120;
	vrr->cur_sot_hs_mode = vrr->adjusted_sot_hs_mode = true;
	vrr->max_h_active_support_120hs = 1080; /* supports 120hz until FHD 1080 */

	vrr->hs_nm_seq = HS_NM_OFF;
	vrr->delayed_perf_normal = false;
	vrr->skip_vrr_in_brightness = false;

	vrr->brr_mode = BRR_OFF_MODE;
	vrr->brr_rewind_on = false;

	vrr->vrr_workqueue = create_singlethread_workqueue("vrr_workqueue");
	INIT_WORK(&vrr->vrr_work, ss_panel_vrr_switch_work);

	LCD_INFO(vdd, "EA8082_AMB641ZR01 ---\n");

	return 0;
}

static bool ss_check_support_mode(struct samsung_display_driver_data *vdd, enum CHECK_SUPPORT_MODE mode)
{
	bool is_support = true;
	int cur_rr = vdd->vrr.cur_refresh_rate;
	bool cur_hs = vdd->vrr.cur_sot_hs_mode;

	switch (mode) {
	case CHECK_SUPPORT_HMD:
		if (!(cur_rr == 60)) {
			is_support = false;
			LCD_ERR(vdd, "HMD fail: supported on 60HS(cur: %d%s)\n",
					cur_rr, cur_hs ? "HS" : "NS");
		}
		break;

	default:
		break;
	}

	return is_support;
}

#define FFC_REG	(0xDE)
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

static void make_brightness_packet(struct samsung_display_driver_data *vdd,
	struct dsi_cmd_desc *packet, int *cmd_cnt, enum BR_TYPE br_type)
{
	if (br_type == BR_TYPE_NORMAL) {
			ss_add_brightness_packet(vdd, BR_FUNC_1, packet, cmd_cnt);

			/* ACL */
			if (vdd->br_info.acl_status || vdd->siop_status) {
				ss_add_brightness_packet(vdd, BR_FUNC_ACL_ON, packet, cmd_cnt);
				ss_add_brightness_packet(vdd, BR_FUNC_ACL_PERCENT_PRE, packet, cmd_cnt);
				ss_add_brightness_packet(vdd, BR_FUNC_ACL_PERCENT, packet, cmd_cnt);
			} else {
				ss_add_brightness_packet(vdd, BR_FUNC_ACL_OFF, packet, cmd_cnt);
			}

			/* gamma */
			ss_add_brightness_packet(vdd, BR_FUNC_GAMMA, packet, cmd_cnt);

			/* vint */
			ss_add_brightness_packet(vdd, BR_FUNC_VINT, packet, cmd_cnt);

			/* VRR */
			ss_add_brightness_packet(vdd, BR_FUNC_VRR, packet, cmd_cnt);

	} else if (br_type == BR_TYPE_HBM) {
		/* acl */
		if (vdd->br_info.acl_status || vdd->siop_status) {
			ss_add_brightness_packet(vdd, BR_FUNC_HBM_ACL_ON, packet, cmd_cnt);
		} else {
			ss_add_brightness_packet(vdd, BR_FUNC_HBM_ACL_OFF, packet, cmd_cnt);
		}

		/* IRC */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_IRC, packet, cmd_cnt);

		/* mAFPC */
		if (vdd->mafpc.is_support)
			ss_add_brightness_packet(vdd, BR_FUNC_MAFPC_SCALE, packet, cmd_cnt);

		/* Gamma */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_GAMMA, packet, cmd_cnt);

		/* LTPS: used for normal and HBM brightness */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_LTPS, packet, cmd_cnt);

		/* hbm etc */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_ETC, packet, cmd_cnt);

		/* vint */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_VINT, packet, cmd_cnt);

		/* VRR */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_VRR, packet, cmd_cnt);
	} else if (br_type == BR_TYPE_HMT) {
			ss_add_brightness_packet(vdd, BR_FUNC_HMT_GAMMA, packet, cmd_cnt);
	}
	 else {
		LCD_ERR(vdd, "undefined br_type (%d) \n", br_type);
	}

	return;
}

void EA8082_AMB641ZR01_FHD_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "EA8082_AMB641ZR01 : +++ \n");
	LCD_ERR(vdd, "%s\n", ss_get_panel_name(vdd));

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
	vdd->panel_func.br_func[BR_FUNC_VRR] = ss_vrr;
	vdd->panel_func.br_func[BR_FUNC_VINT] = ss_vint;

	vdd->br_info.smart_dimming_loaded_dsi = false;

	/* HBM */
	vdd->panel_func.br_func[BR_FUNC_HBM_GAMMA] = ss_brightness_gamma_mode2_hbm;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_ON] = ss_acl_on_hbm;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_OFF] = ss_acl_off;
	vdd->panel_func.br_func[BR_FUNC_HBM_VRR] = ss_vrr_hbm;
	vdd->panel_func.br_func[BR_FUNC_HBM_VINT] = ss_vint;

	/* HMT */
	vdd->panel_func.br_func[BR_FUNC_HMT_GAMMA] = ss_brightness_gamma_mode2_hmt;

	/* Panel LPM */
	vdd->panel_func.samsung_update_lpm_ctrl_cmd = ss_update_panel_lpm_ctrl_cmd;
	vdd->panel_func.samsung_set_lpm_brightness = ss_set_panel_lpm_brightness;

	/* Gray Spot Test */
	vdd->panel_func.samsung_gray_spot = ss_gray_spot;

	/* default brightness */
	vdd->br_info.common_br.bl_level = 128;

	/* mdnie */
	vdd->mdnie.support_mdnie = true;
	vdd->no_qcom_pps = true;

	vdd->mdnie.support_trans_dimming = false;
	vdd->mdnie.mdnie_tune_size[0] = sizeof(DSI0_BYPASS_MDNIE_1);
	vdd->mdnie.mdnie_tune_size[1] = sizeof(DSI0_BYPASS_MDNIE_2);
	vdd->mdnie.mdnie_tune_size[2] = sizeof(DSI0_BYPASS_MDNIE_3);
	vdd->mdnie.mdnie_tune_size[3] = sizeof(DSI0_BYPASS_MDNIE_4);
	vdd->mdnie.mdnie_tune_size[4] = sizeof(DSI0_BYPASS_MDNIE_5);
	//vdd->mdnie.mdnie_tune_size[5] = sizeof(DSI0_BYPASS_MDNIE_6);
	dsi_update_mdnie_data(vdd);

	/* COPR */
	vdd->copr.panel_init = ss_copr_panel_init;

	/* FFC */
	vdd->panel_func.set_ffc = ss_ffc;

	/* ACL default ON */
	vdd->br_info.acl_status = 1;

	/*Default br_info.temperature*/
	vdd->br_info.temperature = 20;

	/* ACL default status in acl on */
	vdd->br_info.gradual_acl_val = 1;

	/* Gram Checksum Test */
	vdd->panel_func.samsung_gct_write = NULL;
	vdd->panel_func.samsung_gct_read = NULL;

	/* SAMSUNG_FINGERPRINT */
	vdd->panel_hbm_entry_delay = 0;
	vdd->panel_hbm_entry_after_te = 60;
	vdd->panel_hbm_exit_delay = 0;

	vdd->panel_func.samsung_check_support_mode = ss_check_support_mode;

	/* VRR */
	ss_vrr_init(&vdd->vrr);

	LCD_INFO(vdd, "EA8082_AMB641ZR01 : --- \n");
}

