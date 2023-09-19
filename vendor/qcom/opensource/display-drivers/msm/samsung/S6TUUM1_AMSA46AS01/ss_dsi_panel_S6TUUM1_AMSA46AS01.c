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
#include "ss_dsi_panel_S6TUUM1_AMSA46AS01.h"
#include "ss_dsi_mdnie_S6TUUM1_AMSA46AS01.h"

#define get_bit(value, shift, width)	((value >> shift) & (GENMASK(width - 1, 0)))

/* Add finger_exit_cnt flag to keep smooth off
 * until 2nd brightness event comes from HBM->Normal
 * after finger_mask_updated.
 */
static int finger_exit_cnt;

static struct dsi_panel_cmd_set *__ss_vrr(struct samsung_display_driver_data *vdd,
					int *level_key, bool is_hbm, bool is_hmt)
{
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct dsi_panel_cmd_set  *vrr_cmds;

	struct vrr_info *vrr = &vdd->vrr;
	enum SS_BRR_MODE brr_mode = vrr->brr_mode;
	int cur_rr;
	bool cur_hs;
	int cmd_idx = 0;

	vrr_cmds = ss_get_cmds(vdd, TX_VRR);

	if (SS_IS_CMDS_NULL(vrr_cmds)) {
		LCD_INFO(vdd, "no vrr cmds\n");
		return NULL;
	}

	if (panel && panel->cur_mode) {
		LCD_INFO(vdd, "VRR: cur_mode: %dx%d@%d%s, %s, running:%d\n",
				panel->cur_mode->timing.h_active,
				panel->cur_mode->timing.v_active,
				panel->cur_mode->timing.refresh_rate,
				panel->cur_mode->timing.sot_hs_mode ? "HS" : "NM",
				ss_get_brr_mode_name(brr_mode), vrr->running_vrr);

		if (panel->cur_mode->timing.refresh_rate != vdd->vrr.adjusted_refresh_rate ||
				panel->cur_mode->timing.sot_hs_mode != vdd->vrr.adjusted_sot_hs_mode)
			LCD_ERR(vdd, "VRR: unmatched RR mode (%dhz%s / %dhz%s)\n",
					panel->cur_mode->timing.refresh_rate,
					panel->cur_mode->timing.sot_hs_mode ? "HS" : "NM",
					vdd->vrr.adjusted_refresh_rate,
					vdd->vrr.adjusted_sot_hs_mode ? "HS" : "NM");
	}

	cur_rr = vrr->cur_refresh_rate;
	cur_hs = vrr->cur_sot_hs_mode;
	cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0x60);
	switch (cur_rr) {
	case 120:
		vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x20;
		break;
	case 60:
		vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x00;
		break;
	}

	LCD_INFO(vdd, "VRR: %s, FPS:%dhz-> %dhz%s (cur: %d%s, target: %d%s)\n",
			ss_get_brr_mode_name(brr_mode),
			vrr->prev_refresh_rate, cur_rr,
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

static int ss_boost_control(struct samsung_display_driver_data *vdd)
{

	char max77816_data1[2] = {0x03, 0x70}; /* BB_EN=1, GPIO_CFG = disable */
	char max77816_data2[2] = {0x02, 0x8E}; /* Set vol 3.1A */
	char read_buf[2];
	int ret = 0;

	ret = ss_boost_i2c_write_ex(max77816_data1[0], max77816_data1[1]);
	if (ret > -1) {
		ss_boost_i2c_read_ex(max77816_data1[0], &read_buf[0]);
		if (read_buf[0] != max77816_data1[1])
			LCD_INFO(vdd, "0x%x not 0x%x fail\n", read_buf[0], max77816_data1[1]);
		else
			LCD_INFO(vdd, "Success to write addr 0x%x\n", max77816_data1[0]);
	} else {
		LCD_ERR(vdd, "boost write failed...%d\n", ret);
		goto err;
	}
	ret = ss_boost_i2c_write_ex(max77816_data2[0], max77816_data2[1]);
	if (ret > -1) {
		ss_boost_i2c_read_ex(max77816_data2[0], &read_buf[1]);
		if (read_buf[1] != max77816_data2[1])
			LCD_INFO(vdd, "0x%x not 0x%x fail\n", read_buf[1], max77816_data2[1]);
		else
			LCD_INFO(vdd, "Success to write addr 0x%x\n", max77816_data2[0]);
	} else {
		LCD_ERR(vdd, "boost write failed...%d\n", ret);
		goto err;
	}
err:
	return 0;
}

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);
	ss_panel_attach_set(vdd, true);

	finger_exit_cnt = 0;

	return true;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
	/* Check if support selfmask */
	if (!vdd->self_disp.is_support)
		return false;

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
	case 0x01:
	case 0x02:
		vdd->panel_revision = 'A';
		break;
	case 0x03:
		vdd->panel_revision = 'B';
		break;
	default:
		vdd->panel_revision = 'B';
		LCD_ERR(vdd, "Invalid panel_rev(default rev : %c)\n", vdd->panel_revision);
		break;
	}

	vdd->panel_revision -= 'A';
	LCD_INFO_ONCE(vdd, "panel_revision = %c %d\n", vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

enum CURRENT_BR_MODE {
	DEFAULT_BR_MODE,
	HS_NORMAL,
	HS_HBM
};

static enum CURRENT_BR_MODE cur_br_mode;

static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2_normal(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *normal_pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_NORMAL);
	int cmd_idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}
	if (SS_IS_CMDS_NULL(normal_pcmds)) {
		LCD_INFO(vdd, "no normal_pcmds\n");
		return NULL;
	}

	cmd_idx = ss_get_cmd_idx(normal_pcmds, 0x00, 0x51);

	normal_pcmds->cmds[cmd_idx].ss_txbuf[1] = get_bit(vdd->br_info.common_br.gm2_wrdisbv, 0, 8); /* DBV [7:0] */
	normal_pcmds->cmds[cmd_idx].ss_txbuf[2] = get_bit(vdd->br_info.common_br.gm2_wrdisbv, 8, 3); /* DBV [10:8] */

	cmd_idx = ss_get_cmd_idx(normal_pcmds, 0x00, 0x53);
	normal_pcmds->cmds[cmd_idx].post_wait_ms = 0x00;
	if (vdd->finger_mask_updated || finger_exit_cnt > 0) {
		if (finger_exit_cnt > 0)
			finger_exit_cnt--;
		normal_pcmds->cmds[cmd_idx].ss_txbuf[1] = 0x20;
	}
	else if (unlikely(vdd->is_factory_mode))
		normal_pcmds->cmds[cmd_idx].ss_txbuf[1] = 0x20;
	else if (cur_br_mode == HS_HBM) { /* smooth off from HBM*/
		normal_pcmds->cmds[cmd_idx].ss_txbuf[1] = 0x20;
		normal_pcmds->cmds[cmd_idx].post_wait_ms = 0x32;
	}
	else if (vdd->display_status_dsi.wait_disp_on) /* smooth off before display on*/
		normal_pcmds->cmds[cmd_idx].ss_txbuf[1] = 0x20;
	else
		normal_pcmds->cmds[cmd_idx].ss_txbuf[1] = 0x28;

	LCD_INFO(vdd, "Normal : bl_level:%d, finger:%d exit_cnt:%d Pstate:%d(2:on),%d\n",
		vdd->br_info.common_br.bl_level, vdd->finger_mask_updated, finger_exit_cnt,
		vdd->panel_state, vdd->display_status_dsi.wait_disp_on);

	*level_key = LEVEL_KEY_NONE;
	cur_br_mode = HS_NORMAL;

	return normal_pcmds;
}

static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2_hbm(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *hbm_cmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_HBM);
	struct dsi_panel_cmd_set *normal_pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_NORMAL);

	int cmd_idx = 0;
	int hbm_tb_idx = 0;
    if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx\n", (size_t)vdd);
        return NULL;
    }
	if (SS_IS_CMDS_NULL(hbm_cmds)) {
		LCD_INFO(vdd, "no hbm_cmds\n");
		return NULL;
	}

	LCD_INFO(vdd, "HBM : cd:%d  refresh_rate:%d bl_level:%d finger:%d\n",
		vdd->br_info.common_br.cd_level, vdd->vrr.adjusted_refresh_rate, vdd->br_info.common_br.bl_level,
		vdd->finger_mask_updated);
	hbm_tb_idx = vdd->br_info.common_br.bl_level - MAX_BL_PF_LEVEL - 1;
	if (hbm_tb_idx >= 0) {
		cmd_idx = ss_get_cmd_idx(hbm_cmds, 0x02, 0xB8);
		hbm_cmds->cmds[cmd_idx].ss_txbuf[1] = get_bit(hbm_60hz_wrdisbv_tb[hbm_tb_idx], 8, 3); /* [10:8] */
		hbm_cmds->cmds[cmd_idx].ss_txbuf[2] = get_bit(hbm_60hz_wrdisbv_tb[hbm_tb_idx], 0, 8); /* [7:0] */
		cmd_idx = ss_get_cmd_idx(hbm_cmds, 0x79, 0xC7);
		hbm_cmds->cmds[cmd_idx].ss_txbuf[1] = get_bit(hbm_120hz_wrdisbv_tb[hbm_tb_idx], 8, 3); /* [10:8] */
		hbm_cmds->cmds[cmd_idx].ss_txbuf[2] = get_bit(hbm_120hz_wrdisbv_tb[hbm_tb_idx], 0, 8); /* [7:0] */
	} else {
		LCD_ERR(vdd, "Invalid bl_level\n");
	}

	if (cur_br_mode == HS_NORMAL) { /* smooth off from normal*/
		cmd_idx = ss_get_cmd_idx(normal_pcmds, 0x00, 0x53);
		normal_pcmds->cmds[cmd_idx].post_wait_ms = 0x00;
			normal_pcmds->cmds[cmd_idx].ss_txbuf[1] = 0x20;
			normal_pcmds->cmds[cmd_idx].post_wait_ms = 0x32;
		ss_send_cmd(vdd, TX_GAMMA_MODE2_NORMAL);
	}

	if (vdd->finger_mask_updated)
		finger_exit_cnt = 3;

	*level_key = LEVEL_KEY_NONE;
	cur_br_mode = HS_HBM;

	return hbm_cmds;
}

#undef COORDINATE_DATA_SIZE
#define COORDINATE_DATA_SIZE 6

#define F1(x, y) ((y)-((39*(x))/38)-95)
#define F2(x, y) ((y)-((36*(x))/35)-56)
#define F3(x, y) ((y)+((7*(x))/1)-24728)
#define F4(x, y) ((y)+((25*(x))/7)-14031)

/* Normal Mode */
static char coordinate_data_1[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xfb, 0x00, 0xfb, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfc, 0x00, 0xff, 0x00}, /* Tune_2 */
	{0xfa, 0x00, 0xf9, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfd, 0x00, 0xfb, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xfa, 0x00, 0xfb, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfd, 0x00, 0xff, 0x00, 0xfa, 0x00}, /* Tune_7 */
	{0xfc, 0x00, 0xff, 0x00, 0xfc, 0x00}, /* Tune_8 */
	{0xfa, 0x00, 0xfd, 0x00, 0xff, 0x00}, /* Tune_9 */
};

/* sRGB/Adobe RGB Mode */
static char coordinate_data_2[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* dummy */
	{0xff, 0x00, 0xfa, 0x00, 0xf3, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfa, 0x00, 0xf6, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xfb, 0x00, 0xfa, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfc, 0x00, 0xf3, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xfe, 0x00, 0xfb, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xfd, 0x00, 0xf3, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xff, 0x00, 0xf7, 0x00}, /* Tune_8 */
	{0xfd, 0x00, 0xff, 0x00, 0xf9, 0x00}, /* Tune_9 */
};

static char (*coordinate_data_multi[MAX_MODE])[COORDINATE_DATA_SIZE] = {
	coordinate_data_2, /* DYNAMIC - DCI */
	coordinate_data_2, /* STANDARD - sRGB/Adobe RGB */
	coordinate_data_2, /* NATURAL - sRGB/Adobe RGB */
	coordinate_data_1, /* MOVIE - Normal */
	coordinate_data_1, /* AUTO - Normal */
	coordinate_data_1, /* READING - Normal */
};

static int mdnie_coordinate_index(int x, int y)
{
	int tune_number = 0;

	if (F1(x, y) > 0) {
		if (F3(x, y) > 0) {
			tune_number = 3;
		} else {
			if (F4(x, y) < 0)
				tune_number = 1;
			else
				tune_number = 2;
		}
	} else {
		if (F2(x, y) < 0) {
			if (F3(x, y) > 0) {
				tune_number = 9;
			} else {
				if (F4(x, y) < 0)
					tune_number = 7;
				else
					tune_number = 8;
			}
		} else {
			if (F3(x, y) > 0)
				tune_number = 6;
			else {
				if (F4(x, y) < 0)
					tune_number = 4;
				else
					tune_number = 5;
			}
		}
	}

	return tune_number;
}

static struct dsi_panel_cmd_set *ss_elvss(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *elvss_cmds = ss_get_cmds(vdd, TX_ELVSS);
	int cmd_idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	if (SS_IS_CMDS_NULL(elvss_cmds)) {
		LCD_ERR(vdd, "No elvss_tx_cmds\n");
		return NULL;
	}

	LCD_DEBUG(vdd, "temperature:%d\n", vdd->br_info.temperature);
	/* 	0x68 1th TSET
		BIT(7) is signed bit.
		BIT(6) ~ BIT(0) is real br_info.temperature.
	*/
	cmd_idx = ss_get_cmd_idx(elvss_cmds, 0x00, 0x68);
	elvss_cmds->cmds[cmd_idx].ss_txbuf[1] = vdd->br_info.temperature > 0 ?
			vdd->br_info.temperature : (char)(BIT(7) | (-1*vdd->br_info.temperature));

	return elvss_cmds;
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
	int mdnie_tune_index = 0;
	int ret;
	char temp[50];
	int buf_len, len = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* A1h 5th~11th para + A1h 1st~4th para */
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

		coordinate_tunning_multi(vdd, coordinate_data_multi, mdnie_tune_index,
						ADDRESS_SCR_WHITE_RED, COORDINATE_DATA_SIZE);

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

	/* A1h 12th para~31th para  */
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
			read_buf[16], read_buf[17], read_buf[18], read_buf[19]);

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

static struct dsi_panel_cmd_set *ss_acl_on_hbm(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL_KEY_NONE;

	pcmds = ss_get_cmds(vdd, TX_ACL_ON);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_ACL_ON..\n");
		return NULL;
	}
	if (vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL) {
		LCD_ERR(vdd, "Not a hbm level..:&d\n", vdd->br_info.common_br.bl_level);
		return NULL;
 	}

	pcmds->cmds[0].ss_txbuf[1] = 0x01;	/* 55h 0x01 ACL 8% */
	LCD_INFO(vdd, "HBM: acl per: 0x%x", pcmds->cmds[0].ss_txbuf[1]);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_acl_on(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL_KEY_NONE;

	pcmds = ss_get_cmds(vdd, TX_ACL_ON);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_ACL_ON..\n");
		return NULL;
	}

	if (vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL) {
		if (vdd->br_info.gradual_acl_val > 1)
			pcmds->cmds[0].ss_txbuf[1] = 0x02;	/* 0x02 : ACL 15% */
		else
			pcmds->cmds[0].ss_txbuf[1] = 0x01;	/* 0x01 : ACL 8%, D.Lab Request(22.02.15) */
	} else {
		LCD_INFO(vdd, "not normal bl_level for acl :%d\n");
	}

	LCD_DEBUG(vdd, "acl per: 0x%x\n", pcmds->cmds[0].ss_txbuf[1]);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_acl_off(struct samsung_display_driver_data *vdd, int *level_key)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL_KEY_NONE;
	LCD_DEBUG(vdd, "off\n");
	return ss_get_cmds(vdd, TX_ACL_OFF);
}

static struct dsi_panel_cmd_set *ss_irc(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *irc_on_cmds = ss_get_cmds(vdd, TX_IRC);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx",	(size_t)vdd);
		return NULL;
	}

	if (SS_IS_CMDS_NULL(irc_on_cmds)) {
		LCD_ERR(vdd, "No irc_tx_cmds_revA\n");
		return NULL;
	}

	if (!vdd->br_info.common_br.support_irc)
		return NULL;
	if (vdd->br_info.common_br.irc_mode == IRC_MODERATO_MODE) /* Vivid */
		memcpy(&irc_on_cmds->cmds->ss_txbuf[1], IRC_MODERATO, sizeof(IRC_MODERATO));
	else if (vdd->br_info.common_br.irc_mode == IRC_FLAT_GAMMA_MODE) /* Natural */
		memcpy(&irc_on_cmds->cmds->ss_txbuf[1], IRC_FLAT, sizeof(IRC_FLAT));
	else
		LCD_ERR(vdd, "invalid irc mode(%d)\n", vdd->br_info.common_br.irc_mode);
	*level_key = LEVEL_KEY_NONE;

	return irc_on_cmds;
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
	make_self_display_img_cmds_UM1(vdd, TX_SELF_MASK_IMAGE, FLAG_SELF_MASK);

	LCD_INFO(vdd, "--\n");
	return 1;
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

	vrr->prev_refresh_rate = vrr->cur_refresh_rate = vrr->adjusted_refresh_rate = 120;
	vrr->prev_sot_hs_mode = vrr->cur_sot_hs_mode = vrr->adjusted_sot_hs_mode = true;
	vrr->prev_phs_mode = vrr->cur_phs_mode = vrr->adjusted_phs_mode = false;

	vrr->hs_nm_seq = HS_NM_OFF;
	vrr->delayed_perf_normal = false;
	vrr->skip_vrr_in_brightness = false;

	vrr->brr_mode = BRR_OFF_MODE;

	vrr->vrr_workqueue = create_singlethread_workqueue("vrr_workqueue");
	INIT_WORK(&vrr->vrr_work, ss_panel_vrr_switch_work);

	LCD_INFO(vdd, "---\n");

	return 0;
}

static int ss_brightness_tot_level(struct samsung_display_driver_data *vdd)
{
	int tot_level_key = 0;

	/* S6TUUM1 does not use level Key */
	tot_level_key = LEVEL_KEY_NONE;

	return tot_level_key;
}

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

		ss_add_brightness_packet(vdd, BR_FUNC_ELVSS, packet, cmd_cnt);
		ss_add_brightness_packet(vdd, BR_FUNC_IRC, packet, cmd_cnt);

		/* gamma */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA, packet, cmd_cnt);
	} else if (br_type == BR_TYPE_HBM) {
		/* gamma compensation for gamma mode2 VRR modes */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA_COMP, packet, cmd_cnt);

		/* VRR */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_VRR, packet, cmd_cnt);

		/* acl */
		if (vdd->br_info.acl_status || vdd->siop_status)
			ss_add_brightness_packet(vdd, BR_FUNC_HBM_ACL_ON, packet, cmd_cnt);
		else
			ss_add_brightness_packet(vdd, BR_FUNC_HBM_ACL_OFF, packet, cmd_cnt);

		/* Gamma */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_GAMMA, packet, cmd_cnt);
	} else {
		LCD_ERR(vdd, "undefined br_type (%d)\n", br_type);
	}
}

void S6TUUM1_AMSA46AS01_WQXGA_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "S6TUUM1_AMSA46AS01 : ++\n");
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
	vdd->panel_func.samsung_octa_id_read = ss_octa_id_read;

	/* Make brightness packer */
	vdd->panel_func.make_brightness_packet = make_brightness_packet;

	/* Brightness */
	vdd->panel_func.br_func[BR_FUNC_GAMMA] = ss_brightness_gamma_mode2_normal;
	vdd->panel_func.br_func[BR_FUNC_ACL_ON] = ss_acl_on;
	vdd->panel_func.br_func[BR_FUNC_ACL_OFF] = ss_acl_off;
	vdd->panel_func.br_func[BR_FUNC_ELVSS] = ss_elvss;
	vdd->panel_func.br_func[BR_FUNC_IRC] = ss_irc;
	vdd->panel_func.br_func[BR_FUNC_VRR] = ss_vrr;

	/* Total level key in brightness */
	vdd->panel_func.samsung_brightness_tot_level = ss_brightness_tot_level;

	/* HBM */
	vdd->panel_func.br_func[BR_FUNC_HBM_GAMMA] = ss_brightness_gamma_mode2_hbm;
	vdd->panel_func.br_func[BR_FUNC_HBM_VRR] = ss_vrr_hbm;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_ON] = ss_acl_on_hbm;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_OFF] = ss_acl_off;
	vdd->br_info.smart_dimming_loaded_dsi = false;

	/* Panel LPM */
	vdd->panel_func.samsung_update_lpm_ctrl_cmd = NULL;
	vdd->panel_func.samsung_set_lpm_brightness = NULL;


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

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = NULL;

	/* COPR */
	//vdd->copr.panel_init = ss_copr_panel_init;

	/* ACL default ON */
	vdd->br_info.acl_status = 1;
	vdd->br_info.temperature = 20; // default temperature

	/*Default Temperature*/
	vdd->br_info.temperature = 20;

	/* Gram Checksum Test */
	vdd->panel_func.samsung_gct_write = NULL;
	vdd->panel_func.samsung_gct_read = NULL;

	/* Self display */
	vdd->self_disp.is_support = true;
	vdd->self_disp.factory_support = true;
	vdd->self_disp.init = self_display_init_UM1;
	vdd->self_disp.data_init = ss_self_display_data_init;

	/* Boost control */
	vdd->panel_func.samsung_boost_control = ss_boost_control;

	/* SAMSUNG_FINGERPRINT */
	vdd->panel_hbm_entry_delay = 1;
	vdd->panel_hbm_exit_delay = 1;
	vdd->panel_hbm_entry_after_te = 1000;
	/* VRR */
	ss_vrr_init(&vdd->vrr);

	/* Disp pmic init */
	ss_boost_max77816_init();

	LCD_INFO(vdd, "S6TUUM1_AMSA46AS01 : --\n");
}
