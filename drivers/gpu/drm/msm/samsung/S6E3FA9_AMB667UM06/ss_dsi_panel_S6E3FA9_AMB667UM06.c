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
Copyright (C) 2017, Samsung Electronics. All rights reserved.

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
#include "ss_dsi_panel_S6E3FA9_AMB667UM06.h"
#include "ss_dsi_mdnie_S6E3FA9_AMB667UM06.h"

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

#define IRC_MODERATO_MODE_VAL	0x61
#define IRC_FLAT_GAMMA_MODE_VAL	0x21

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
	        LCD_ERR(": Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	LCD_INFO("+: ndx=%d\n", vdd->ndx);
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
		LCD_INFO("samsung splash enabled.. skip image write\n");
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
		vdd->panel_revision = 'A';
		break;
	default:
		vdd->panel_revision = 'A';
		LCD_ERR("Invalid panel_rev(default rev : %c)\n", vdd->panel_revision);
		break;
	}

	vdd->panel_revision -= 'A';
	LCD_INFO_ONCE("panel_revision = %c %d \n", vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}


#define get_bit(value, shift, width)	((value >> shift) & (GENMASK(width - 1, 0)))

static struct dsi_panel_cmd_set * mdss_brightness_gamma_mode2(struct samsung_display_driver_data *vdd, int *level_key)
{
	//struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmd_set *pcmds;

	if (IS_ERR_OR_NULL(vdd)) {
	        LCD_ERR(": Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2);

	if(vdd->br.cd_idx <= MAX_BL_PF_LEVEL){
		LCD_INFO("Normal : cd_idx [%d] \n", vdd->br.cd_idx);
		pcmds->cmds[0].msg.tx_buf[1] = vdd->finger_mask_updated? 0x20 : 0x28;	/* Normal Smooth transition : 0x28 */
		pcmds->cmds[1].msg.tx_buf[1] = vdd->temperature > 0 ?	vdd->temperature : (char)(BIT(7) | (-1*vdd->temperature));
		pcmds->cmds[1].msg.tx_buf[3] = 0x10; 									/* ELVSS Value for Normal Mode brightness */				
		pcmds->cmds[2].msg.tx_buf[1] = get_bit(vdd->br.cd_level, 8, 2);
		pcmds->cmds[2].msg.tx_buf[2] = get_bit(vdd->br.cd_level, 0, 8);
	}
	else{
		LCD_INFO("HBM : cd_idx [%d] \n", vdd->br.cd_idx);
		pcmds->cmds[0].msg.tx_buf[1] = vdd->finger_mask_updated? 0xE0 : 0xE8;	/* HBM Smooth transition : 0xE8 */
		pcmds->cmds[1].msg.tx_buf[1] = vdd->temperature > 0 ?	vdd->temperature : (char)(BIT(7) | (-1*vdd->temperature));
		pcmds->cmds[1].msg.tx_buf[3] = elvss_table[vdd->br.cd_level]; /* ELVSS Value for HBM brightness */
		pcmds->cmds[2].msg.tx_buf[1] = get_bit(vdd->br.cd_level, 8, 2);
		pcmds->cmds[2].msg.tx_buf[2] = get_bit(vdd->br.cd_level, 0, 8);
	}

	*level_key = LEVEL1_KEY;

	return pcmds;
}

/**static struct dsi_panel_cmd_set * mdss_brightness_hmt(struct samsung_display_driver_data *vdd, int *level_key)
{
    struct dsi_panel_cmd_set *pcmds;

    if (IS_ERR_OR_NULL(vdd)) {
        LCD_ERR(": Invalid data vdd : 0x%zx", (size_t)vdd);
        return NULL;
    }

    LCD_INFO("hmt_bl_level : %d candela : %dCD\n", vdd->hmt_stat.hmt_bl_level, vdd->hmt_stat.candela_level_hmt);

    pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2);

    pcmds->cmds[0].msg.tx_buf[1] = 0x28;  // Smooth transition 
    pcmds->cmds[1].msg.tx_buf[7] = 0x90;  // ELVSS Value for normal brgihtness 
    pcmds->cmds[2].msg.tx_buf[1] = get_bit(vdd->hmt_stat.candela_level_hmt, 8, 2);
    pcmds->cmds[2].msg.tx_buf[2] = get_bit(vdd->hmt_stat.candela_level_hmt, 0, 8);
    *level_key = LEVEL1_KEY;

    return pcmds;
}*/

static int ss_manufacture_date_read(struct samsung_display_driver_data *vdd)
{
	unsigned char date[8];
	int year, month, day;
	int hour, min;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (EAh 8~14th) for manufacture date */
	if (ss_get_cmds(vdd, RX_MANUFACTURE_DATE)->count) {
		ss_panel_data_read(vdd, RX_MANUFACTURE_DATE, date, LEVEL1_KEY);

		year = date[0] & 0xf0;
		year >>= 4;
		year += 2011; // 0 = 2011 year
		month = date[0] & 0x0f;
		day = date[1] & 0x1f;
		hour = date[2] & 0x1f;
		min = date[3] & 0x3f;

		vdd->manufacture_date_dsi = year * 10000 + month * 100 + day;
		vdd->manufacture_time_dsi = hour * 100 + min;

		LCD_ERR("manufacture_date DSI%d = (%d%04d) - year(%d) month(%d) day(%d) hour(%d) min(%d)\n",
			vdd->ndx, vdd->manufacture_date_dsi, vdd->manufacture_time_dsi,
			year, month, day, hour, min);

	} else {
		LCD_ERR("DSI%d no manufacture_date_rx_cmds cmds(%d)", vdd->ndx, vdd->panel_revision);
		return false;
	}

	return true;
}

static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	char ddi_id[5];
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (D6h 1~5th) for CHIP ID */
	if (ss_get_cmds(vdd, RX_DDI_ID)->count) {
		ss_panel_data_read(vdd, RX_DDI_ID, ddi_id, LEVEL1_KEY);

		for (loop = 0; loop < 5; loop++)
			vdd->ddi_id_dsi[loop] = ddi_id[loop];

		LCD_INFO("DSI%d : %02x %02x %02x %02x %02x\n", vdd->ndx,
			vdd->ddi_id_dsi[0], vdd->ddi_id_dsi[1],
			vdd->ddi_id_dsi[2], vdd->ddi_id_dsi[3],
			vdd->ddi_id_dsi[4]);
	} else {
		LCD_ERR("DSI%d no ddi_id_rx_cmds cmds", vdd->ndx);
		return false;
	}

	return true;
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

/*static int ss_elvss_read(struct samsung_display_driver_data *vdd)
{
	char elvss_b7[2];

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	// Read mtp (B7h 9th,10th) for elvss
	ss_panel_data_read(vdd, RX_ELVSS, elvss_b7, LEVEL1_KEY);

	vdd->br.elvss_value1 = elvss_b7[0]; //0xB7 9th para OTP value
	vdd->br.elvss_value2 = elvss_b7[1]; //0xB7 10th para OTP value

	return true;
}*/

static int ss_mdnie_read(struct samsung_display_driver_data *vdd)
{
	char x_y_location[4];
	int mdnie_tune_index = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (A1h 1~5th) for ddi id */
	if (ss_get_cmds(vdd, RX_MDNIE)->count) {
		ss_panel_data_read(vdd, RX_MDNIE, x_y_location, LEVEL1_KEY);

		vdd->mdnie.mdnie_x = x_y_location[0] << 8 | x_y_location[1];	/* X */
		vdd->mdnie.mdnie_y = x_y_location[2] << 8 | x_y_location[3];	/* Y */

		mdnie_tune_index = mdnie_coordinate_index(vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);

		coordinate_tunning_multi(vdd, coordinate_data_multi, mdnie_tune_index,
				ADDRESS_SCR_WHITE_RED, COORDINATE_DATA_SIZE);

		LCD_INFO("DSI%d : X-%d Y-%d \n", vdd->ndx, vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);
	} else {
		LCD_ERR("DSI%d no mdnie_read_rx_cmds cmds", vdd->ndx);
		return false;
	}

	return true;
}

static int dsi_update_mdnie_data(struct samsung_display_driver_data *vdd)
{
	struct mdnie_lite_tune_data *mdnie_data;

	mdnie_data = kzalloc(sizeof(struct mdnie_lite_tune_data), GFP_KERNEL);
	if (!mdnie_data) {
		LCD_ERR("fail to allocate mdnie_data memory\n");
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
	mdnie_data->dsi_max_night_mode_index = 11;
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
	
	mdnie_data->support_mode = 1;
	mdnie_data->support_scenario = 1;
	mdnie_data->support_outdoor = 1;
	mdnie_data->support_bypass = 1;
	mdnie_data->support_accessibility = 1;
	mdnie_data->support_sensorRGB = 1;
	mdnie_data->support_whiteRGB = 1;
	mdnie_data->support_mdnie_ldu = 1;
	mdnie_data->support_night_mode = 1;
	mdnie_data->support_color_lens = 1;
	mdnie_data->support_hdr = 1;
	mdnie_data->support_light_notification = 1;
	mdnie_data->support_afc = 1;
	mdnie_data->support_cabc = 0;
	mdnie_data->support_hmt_color_temperature = 1;
	mdnie_data->support_siliconworks = 0;

	vdd->mdnie.mdnie_data = mdnie_data;

	return 0;
}

static int ss_cell_id_read(struct samsung_display_driver_data *vdd)
{
	char cell_id_buffer[MAX_CELL_ID] = {0,};
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read Panel Unique Cell ID (C9h 19~34th) */
	if (ss_get_cmds(vdd, RX_CELL_ID)->count) {
		memset(cell_id_buffer, 0x00, MAX_CELL_ID);

		ss_panel_data_read(vdd, RX_CELL_ID, cell_id_buffer, LEVEL1_KEY);

		for (loop = 0; loop < MAX_CELL_ID; loop++)
			vdd->cell_id_dsi[loop] = cell_id_buffer[loop];

		LCD_INFO("DSI%d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			vdd->ndx, vdd->cell_id_dsi[0],
			vdd->cell_id_dsi[1],	vdd->cell_id_dsi[2],
			vdd->cell_id_dsi[3],	vdd->cell_id_dsi[4],
			vdd->cell_id_dsi[5],	vdd->cell_id_dsi[6],
			vdd->cell_id_dsi[7],	vdd->cell_id_dsi[8],
			vdd->cell_id_dsi[9],	vdd->cell_id_dsi[10]);

	} else {
		LCD_ERR("DSI%d no cell_id_rx_cmds cmd\n", vdd->ndx);
		return false;
	}

	return true;
}

static int ss_octa_id_read(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read Panel Unique OCTA ID (C9h 2nd~21th) */
	if (ss_get_cmds(vdd, RX_OCTA_ID)->count) {
		memset(vdd->octa_id_dsi, 0x00, MAX_OCTA_ID);

		ss_panel_data_read(vdd, RX_OCTA_ID,
				vdd->octa_id_dsi, LEVEL1_KEY);

		LCD_INFO("octa id: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
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
		LCD_ERR("DSI%d no octa_id_rx_cmds cmd\n", vdd->ndx);
		return false;
	}

	return true;
}

static struct dsi_panel_cmd_set *ss_acl_on(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;

	pcmds = ss_get_cmds(vdd, TX_ACL_ON);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR("No cmds for TX_ACL_ON..\n");
		return NULL;
	}

	if(vdd->br.cd_idx <= MAX_BL_PF_LEVEL)
		pcmds->cmds[0].msg.tx_buf[1] = 0x02;	/* ACL 15% */
	else
		pcmds->cmds[0].msg.tx_buf[1] = 0x01;	/* ACL 8% */

	LCD_INFO("gradual_acl: %d, acl per: 0x%x",
			vdd->gradual_acl_val, pcmds->cmds[0].msg.tx_buf[1]);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_acl_off(struct samsung_display_driver_data *vdd, int *level_key)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;
	LCD_INFO("off\n");
	return ss_get_cmds(vdd, TX_ACL_OFF);
}

static struct dsi_panel_cmd_set *ss_vint(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *vint_cmds = ss_get_cmds(vdd, TX_VINT);

	if (IS_ERR_OR_NULL(vdd) || SS_IS_CMDS_NULL(vint_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)vint_cmds);
		return NULL;
	}

	if (vdd->xtalk_mode)
		vint_cmds->cmds->msg.tx_buf[1] = 0xC4; // ON : VGH 6.2 V
	else
		vint_cmds->cmds->msg.tx_buf[1] = 0xCC; // OFF

	*level_key = LEVEL1_KEY;

	return vint_cmds;
}

static void ss_set_panel_lpm_brightness(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *alpm_brightness[LPM_BRIGHTNESS_MAX_IDX] = {NULL, };
	struct dsi_panel_cmd_set *alpm_ctrl[MAX_LPM_CTRL] = {NULL, };
	struct dsi_panel_cmd_set *cmd_list[2];

	/*default*/
	int mode = ALPM_MODE_ON;
	int ctrl_index = HLPM_CTRL_2NIT;
	int bl_index = LPM_2NIT_IDX;

	/*
	 * Init reg_list and cmd list
	 * reg_list[X][0] is reg value
	 * reg_list[X][1] is offset for reg value
	 * cmd_list is the target cmds for searching reg value
	 */
	static int reg_list[2][2] = {
		{ALPM_REG, -EINVAL},
		{ALPM_CTRL_REG, -EINVAL}
	};

	LCD_INFO("%s++\n", __func__);

	cmd_list[0] = ss_get_cmds(vdd, TX_LPM_BL_CMD);	// JUN_TEMP
	cmd_list[1] = ss_get_cmds(vdd, TX_LPM_BL_CMD);	// JUN_TEMP
	if (SS_IS_CMDS_NULL(cmd_list[0]) || SS_IS_CMDS_NULL(cmd_list[1])) {
		LCD_ERR("No cmds for TX_LPM_BL_CMD..\n");
		return;
	}

	/* Init alpm_brightness and alpm_ctrl cmds */
	alpm_brightness[LPM_2NIT_IDX]  = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
	alpm_brightness[LPM_10NIT_IDX] = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
	alpm_brightness[LPM_30NIT_IDX] = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
	alpm_brightness[LPM_60NIT_IDX] = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
	if (SS_IS_CMDS_NULL(alpm_brightness[LPM_2NIT_IDX]) || SS_IS_CMDS_NULL(alpm_brightness[LPM_10NIT_IDX]) ||
		SS_IS_CMDS_NULL(alpm_brightness[LPM_30NIT_IDX]) || SS_IS_CMDS_NULL(alpm_brightness[LPM_60NIT_IDX])) {
		LCD_ERR("No cmds for alpm_brightness..\n");
		return;
	}

	alpm_ctrl[HLPM_CTRL_2NIT]  = ss_get_cmds(vdd, TX_HLPM_2NIT_CMD);
	alpm_ctrl[HLPM_CTRL_10NIT] = ss_get_cmds(vdd, TX_HLPM_10NIT_CMD);
	alpm_ctrl[HLPM_CTRL_30NIT] = ss_get_cmds(vdd, TX_HLPM_30NIT_CMD);
	alpm_ctrl[HLPM_CTRL_60NIT] = ss_get_cmds(vdd, TX_HLPM_60NIT_CMD);
	if (SS_IS_CMDS_NULL(alpm_ctrl[HLPM_CTRL_2NIT]) || SS_IS_CMDS_NULL(alpm_ctrl[HLPM_CTRL_10NIT]) ||
		SS_IS_CMDS_NULL(alpm_ctrl[HLPM_CTRL_30NIT]) || SS_IS_CMDS_NULL(alpm_ctrl[HLPM_CTRL_60NIT])) {
		LCD_ERR("No cmds for hlpm_brightness..\n");
		return;
	}

	mode = vdd->panel_lpm.mode;

	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_10NIT:
		ctrl_index = HLPM_CTRL_10NIT;
		bl_index = LPM_10NIT_IDX;
		break;
	case LPM_30NIT:
		ctrl_index = HLPM_CTRL_30NIT;
		bl_index = LPM_30NIT_IDX;
		break;
	case LPM_60NIT:
		ctrl_index = HLPM_CTRL_60NIT;
		bl_index = LPM_60NIT_IDX;
		break;
	case LPM_2NIT:
	default:
		ctrl_index = HLPM_CTRL_2NIT;
		bl_index = LPM_2NIT_IDX;
		break;
	}

	LCD_INFO("[Panel LPM] lpm_bl_level = %d bl_index %d, ctrl_index %d, mode %d\n",
			 vdd->panel_lpm.lpm_bl_level, bl_index, ctrl_index, mode);

	/*
	 * Find offset for alpm_reg and alpm_ctrl_reg
	 * alpm_reg  : Control register for ALPM/HLPM on/off
	 * alpm_ctrl_reg : Control register for changing ALPM/HLPM mode
	 */
	if (unlikely((reg_list[0][1] == -EINVAL) || (reg_list[1][1] == -EINVAL)))
		ss_find_reg_offset(reg_list, cmd_list, ARRAY_SIZE(cmd_list));

	if (reg_list[0][1] != -EINVAL) {
		/* Update parameter for ALPM_REG */
		memcpy(cmd_list[0]->cmds[reg_list[0][1]].msg.tx_buf,
				alpm_brightness[bl_index]->cmds[0].msg.tx_buf,
				sizeof(char) * cmd_list[0]->cmds[reg_list[0][1]].msg.tx_len);

		LCD_INFO("[Panel LPM] change brightness cmd : %x, %x\n",
				cmd_list[0]->cmds[reg_list[0][1]].msg.tx_buf[1],
				alpm_brightness[bl_index]->cmds[0].msg.tx_buf[1]);
	}

	if (reg_list[1][1] != -EINVAL) {
		/* Initialize ALPM/HLPM cmds */
		/* Update parameter for ALPM_CTRL_REG */
		memcpy(cmd_list[1]->cmds[reg_list[1][1]].msg.tx_buf,
				alpm_ctrl[ctrl_index]->cmds[0].msg.tx_buf,
				sizeof(char) * cmd_list[1]->cmds[reg_list[1][1]].msg.tx_len);

		LCD_INFO("[Panel LPM] update alpm ctrl reg\n");
	}

	//send lpm bl cmd
	ss_send_cmd(vdd, TX_LPM_BL_CMD);

	LCD_INFO("[Panel LPM] bl_level : %s\n",
			/* Check current brightness level */
			vdd->panel_lpm.lpm_bl_level == LPM_2NIT ? "2NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_10NIT ? "10NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_30NIT ? "30NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_60NIT ? "60NIT" : "UNKNOWN");

	LCD_INFO("%s--\n", __func__);
}

/*
 * This function will update parameters for ALPM_REG/ALPM_CTRL_REG
 * Parameter for ALPM_REG : Control brightness for panel LPM
 * Parameter for ALPM_CTRL_REG : Change panel LPM mode for ALPM/HLPM
 * mode, brightness, hz are updated here.
 */
static void ss_update_panel_lpm_ctrl_cmd(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *alpm_brightness[LPM_BRIGHTNESS_MAX_IDX] = {NULL, };
	struct dsi_panel_cmd_set *alpm_ctrl[MAX_LPM_CTRL] = {NULL, };
	struct dsi_panel_cmd_set *alpm_off_ctrl[MAX_LPM_MODE] = {NULL, };
	struct dsi_panel_cmd_set *cmd_list[2];
	struct dsi_panel_cmd_set *off_cmd_list[1];

	/*default*/
	int mode = ALPM_MODE_ON;
	int ctrl_index = HLPM_CTRL_2NIT;
	int bl_index = LPM_2NIT_IDX;

	/*
	 * Init reg_list and cmd list
	 * reg_list[X][0] is reg value
	 * reg_list[X][1] is offset for reg value
	 * cmd_list is the target cmds for searching reg value
	 */
	static int reg_list[2][2] = {
		{ALPM_REG, -EINVAL},
		{ALPM_CTRL_REG, -EINVAL}
	};

	static int off_reg_list[1][2] = { {ALPM_CTRL_REG, -EINVAL} };

	LCD_INFO("%s++\n", __func__);

	cmd_list[0] = ss_get_cmds(vdd, TX_LPM_ON);
	cmd_list[1] = ss_get_cmds(vdd, TX_LPM_ON);
	if (SS_IS_CMDS_NULL(cmd_list[0]) || SS_IS_CMDS_NULL(cmd_list[1])) {
		LCD_ERR("No cmds for TX_LPM_ON..\n");
		return;
	}

	off_cmd_list[0] = ss_get_cmds(vdd, TX_LPM_OFF);
	if (SS_IS_CMDS_NULL(off_cmd_list[0])) {
		LCD_ERR("No cmds for TX_LPM_OFF..\n");
		return;
	}

	/* Init alpm_brightness and alpm_ctrl cmds */
	alpm_brightness[LPM_2NIT_IDX]  = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
	alpm_brightness[LPM_10NIT_IDX] = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
	alpm_brightness[LPM_30NIT_IDX] = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
	alpm_brightness[LPM_60NIT_IDX] = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
	if (SS_IS_CMDS_NULL(alpm_brightness[LPM_2NIT_IDX]) || SS_IS_CMDS_NULL(alpm_brightness[LPM_10NIT_IDX]) ||
		SS_IS_CMDS_NULL(alpm_brightness[LPM_30NIT_IDX]) || SS_IS_CMDS_NULL(alpm_brightness[LPM_60NIT_IDX])) {
		LCD_ERR("No cmds for alpm_brightness..\n");
		return;
	}

	alpm_ctrl[HLPM_CTRL_2NIT]  = ss_get_cmds(vdd, TX_HLPM_2NIT_CMD);
	alpm_ctrl[HLPM_CTRL_10NIT] = ss_get_cmds(vdd, TX_HLPM_10NIT_CMD);
	alpm_ctrl[HLPM_CTRL_30NIT] = ss_get_cmds(vdd, TX_HLPM_30NIT_CMD);
	alpm_ctrl[HLPM_CTRL_60NIT] = ss_get_cmds(vdd, TX_HLPM_60NIT_CMD);
	if (SS_IS_CMDS_NULL(alpm_ctrl[HLPM_CTRL_2NIT]) || SS_IS_CMDS_NULL(alpm_ctrl[HLPM_CTRL_10NIT]) ||
		SS_IS_CMDS_NULL(alpm_ctrl[HLPM_CTRL_30NIT]) || SS_IS_CMDS_NULL(alpm_ctrl[HLPM_CTRL_60NIT])) {
		LCD_ERR("No cmds for hlpm_brightness..\n");
		return;
	}

/*
	alpm_off_ctrl[HLPM_MODE_ON] = ss_get_cmds(vdd, TX_HLPM_OFF);
	if (SS_IS_CMDS_NULL(alpm_off_ctrl[HLPM_MODE_ON])) {
		LCD_ERR("No cmds for TX_HLPM_OFF..\n");
		return;
	}
*/
	mode = vdd->panel_lpm.mode;

	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_10NIT:
		ctrl_index = HLPM_CTRL_10NIT;
		bl_index = LPM_10NIT_IDX;
		break;
	case LPM_30NIT:
		ctrl_index = HLPM_CTRL_30NIT;
		bl_index = LPM_30NIT_IDX;
		break;
	case LPM_60NIT:
		ctrl_index = HLPM_CTRL_60NIT;
		bl_index = LPM_60NIT_IDX;
		break;
	case LPM_2NIT:
	default:
		ctrl_index = HLPM_CTRL_2NIT;
		bl_index = LPM_2NIT_IDX;
		break;
	}

	LCD_INFO("[Panel LPM] change brightness cmd bl_index :%d, ctrl_index : %d, mode : %d\n",
			 bl_index, ctrl_index, mode);

	/*
	 * Find offset for alpm_reg and alpm_ctrl_reg
	 * alpm_reg  : Control register for ALPM/HLPM on/off
	 * alpm_ctrl_reg : Control register for changing ALPM/HLPM mode
	 */
	if (unlikely((reg_list[0][1] == -EINVAL) || (reg_list[1][1] == -EINVAL)))
		ss_find_reg_offset(reg_list, cmd_list, ARRAY_SIZE(cmd_list));

	if (unlikely(off_reg_list[0][1] == -EINVAL))
		ss_find_reg_offset(off_reg_list, off_cmd_list, ARRAY_SIZE(off_cmd_list));

	if (reg_list[0][1] != -EINVAL) {
		/* Update parameter for ALPM_REG 0x53*/
		memcpy(cmd_list[0]->cmds[reg_list[0][1]].msg.tx_buf,
				alpm_brightness[bl_index]->cmds[0].msg.tx_buf,
				sizeof(char) * cmd_list[0]->cmds[reg_list[0][1]].msg.tx_len);

		LCD_INFO("[Panel LPM] change brightness cmd : %x, %x\n",
				cmd_list[0]->cmds[reg_list[0][1]].msg.tx_buf[1],
				alpm_brightness[bl_index]->cmds[0].msg.tx_buf[1]);
	}

	if (reg_list[1][1] != -EINVAL) {
		/* Initialize ALPM/HLPM cmds */
		/* Update parameter for ALPM_CTRL_REG 0xBB*/
		memcpy(cmd_list[1]->cmds[reg_list[1][1]].msg.tx_buf,
				alpm_ctrl[ctrl_index]->cmds[0].msg.tx_buf,
				sizeof(char) * cmd_list[1]->cmds[reg_list[1][1]].msg.tx_len);

		LCD_INFO("[Panel LPM] update alpm ctrl reg\n");
	}

	if ((off_reg_list[0][1] != -EINVAL) && (mode != LPM_MODE_OFF)) {
		/* Update parameter for ALPM_CTRL_REG */
		memcpy(off_cmd_list[0]->cmds[off_reg_list[0][1]].msg.tx_buf,
				alpm_off_ctrl[mode]->cmds[0].msg.tx_buf,
				sizeof(char) * off_cmd_list[0]->cmds[off_reg_list[0][1]].msg.tx_len);
	}

	LCD_INFO("%s--\n", __func__);
}

static int ss_self_display_data_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO("++\n");

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->self_disp.is_support) {
		LCD_ERR("Self Display is not supported\n");
		return -EINVAL;
	}

	vdd->self_disp.operation[FLAG_SELF_MASK].img_buf = self_mask_img_data;
	vdd->self_disp.operation[FLAG_SELF_MASK].img_size = ARRAY_SIZE(self_mask_img_data);
	vdd->self_disp.operation[FLAG_SELF_MASK].img_checksum = SELF_MASK_IMG_CHECKSUM;
	make_self_dispaly_img_cmds_FA9(vdd, TX_SELF_MASK_IMAGE, FLAG_SELF_MASK);

	vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_buf = self_mask_crc_img_data;
	vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_size = ARRAY_SIZE(self_mask_crc_img_data);
	make_self_dispaly_img_cmds_FA9(vdd, TX_SELF_MASK_IMAGE_CRC, FLAG_SELF_MASK_CRC);

	LCD_INFO("--\n");
	return 1;
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

static int poc_erase(struct samsung_display_driver_data *vdd, u32 erase_pos, u32 erase_size, u32 target_pos)
{
	struct dsi_display *display = NULL;
	struct dsi_panel_cmd_set *poc_erase_sector_tx_cmds = NULL;
	int delay_us = 0;
	int image_size = 0;
	int type;
	int ret = 0;

	struct msm_drm_private *priv = NULL;
	struct sde_kms *sde_kms = NULL;
	u64 sde_mnoc_ab, sde_mnoc_ib;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("no vdd\n");
		return -EINVAL;
	}

	display = GET_DSI_DISPLAY(vdd);
	if (IS_ERR_OR_NULL(display)) {
		LCD_ERR("no display");
		return -EINVAL;
	}

	if (vdd->poc_driver.erase_sector_addr_idx[0] < 0) {
		LCD_ERR("sector addr index is not implemented.. %d\n",
			vdd->poc_driver.erase_sector_addr_idx[0]);
		return -EINVAL;
	}

	if (!ss_is_ready_to_send_cmd(vdd)) {
		LCD_ERR("Panel is not ready. Panel State(%d)\n", vdd->panel_state);
		return -EBUSY;
	}
	
	poc_erase_sector_tx_cmds = ss_get_cmds(vdd, TX_POC_ERASE_SECTOR);
	if (SS_IS_CMDS_NULL(poc_erase_sector_tx_cmds)) {
		LCD_ERR("No cmds for TX_POC_ERASE_SECTOR..\n");
		return -ENODEV;
	}

	image_size = vdd->poc_driver.image_size;
	delay_us = vdd->poc_driver.erase_delay_us;

	if (erase_size == POC_ERASE_64KB) {
		delay_us = 2000000; /* 2000ms */
		poc_erase_sector_tx_cmds->cmds[2].msg.tx_buf[3] = 0xD8;
	} else if (erase_size == POC_ERASE_32KB) {
		delay_us = 1600000; /* 1600ms */
		poc_erase_sector_tx_cmds->cmds[2].msg.tx_buf[3] = 0x52;
	} else {
		delay_us = 400000; /* 400ms */
		poc_erase_sector_tx_cmds->cmds[2].msg.tx_buf[3] = 0x20;
	} 

	LCD_INFO("[ERASE] (%6d / %6d), erase_size (%d), delay %dus\n",
		erase_pos, target_pos, erase_size, delay_us);

	/* MAX CPU ON */
	priv = display->drm_dev->dev_private;
	sde_kms = (struct sde_kms *)priv->kms;
	sde_mnoc_ab = sde_kms->core_client->ab[SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT];
	sde_mnoc_ib = sde_kms->core_client->ib[SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT];

	ss_set_max_cpufreq(vdd, true, CPUFREQ_CLUSTER_ALL);
	sde_power_data_bus_set_quota(&priv->phandle,
			sde_kms->core_client,
			SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT,
			SDE_POWER_HANDLE_DBUS_ID_MNOC,
			SDE_POWER_HANDLE_CONT_SPLASH_BUS_AB_QUOTA,
			SDE_POWER_HANDLE_CONT_SPLASH_BUS_IB_QUOTA);
	dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_ON);

	/* Enter exclusive mode */
	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.permit_frame_update = 1;
	vdd->exclusive_tx.enable = 1;
	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 1);

	LCD_INFO("WRITE [TX_POC_PRE_ERASE_SECTOR]");
	ss_send_cmd(vdd, TX_POC_PRE_ERASE_SECTOR);

	poc_erase_sector_tx_cmds->cmds[2].msg.tx_buf[vdd->poc_driver.erase_sector_addr_idx[0]]
											= (erase_pos & 0xFF0000) >> 16;
	poc_erase_sector_tx_cmds->cmds[2].msg.tx_buf[vdd->poc_driver.erase_sector_addr_idx[1]]
											= (erase_pos & 0x00FF00) >> 8;
	poc_erase_sector_tx_cmds->cmds[2].msg.tx_buf[vdd->poc_driver.erase_sector_addr_idx[2]]
											= erase_pos & 0x0000FF;

	ss_send_cmd(vdd, TX_POC_ERASE_SECTOR);

	usleep_range(delay_us, delay_us);

	/* PGM Disable*/
	LCD_INFO("WRITE [TX_POC_POST_ERASE_SECTOR]");
	ss_send_cmd(vdd, TX_POC_POST_ERASE_SECTOR);

	/* exit exclusive mode*/
	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 0);
	vdd->exclusive_tx.permit_frame_update = 0;
	vdd->exclusive_tx.enable = 0;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);

	/* MAX CPU OFF */
	dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_OFF);
	sde_power_data_bus_set_quota(&priv->phandle,
			sde_kms->core_client,
			SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT,
			SDE_POWER_HANDLE_DBUS_ID_MNOC,
			sde_mnoc_ab,
			sde_mnoc_ib);
	ss_set_max_cpufreq(vdd, false, CPUFREQ_CLUSTER_ALL);

	return ret;
}

static int poc_write(struct samsung_display_driver_data *vdd, u8 *data, u32 write_pos, u32 write_size)
{
	struct dsi_panel_cmd_set *write_cmd = NULL;
	struct dsi_panel_cmd_set *write_data_add = NULL;

	int pos, type, ret = 0;
	int last_pos, delay_us, image_size, loop_cnt, poc_w_size;
	int tx_size, tx_size1, tx_size2;

	struct msm_drm_private *priv = NULL;
	struct sde_kms *sde_kms = NULL;
	u64 sde_mnoc_ab, sde_mnoc_ib;
	struct dsi_display *display = NULL; 

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("no vdd\n");
		return -EINVAL;
	}

	display = GET_DSI_DISPLAY(vdd);
	if (IS_ERR_OR_NULL(display)) {
		LCD_ERR("no display");
		return -EINVAL;
	}

	write_cmd = ss_get_cmds(vdd, TX_POC_WRITE_LOOP_256BYTE);
	if (SS_IS_CMDS_NULL(write_cmd)) {
		LCD_ERR("no cmds for TX_POC_WRITE_LOOP_256BYTE..\n");
		return -EINVAL;
	}

	write_data_add = ss_get_cmds(vdd, TX_POC_WRITE_LOOP_DATA_ADD);
	if (SS_IS_CMDS_NULL(write_data_add)) {
		LCD_ERR("no cmds for TX_POC_WRITE_LOOP_DATA_ADD..\n");
		return -EINVAL;
	}

	if (vdd->poc_driver.write_addr_idx[0] < 0) {
		LCD_ERR("write addr index is not implemented.. %d\n",
			vdd->poc_driver.write_addr_idx[0]);
		return -EINVAL;
	}

	delay_us = vdd->poc_driver.write_delay_us; /* Panel dtsi set */
	image_size = vdd->poc_driver.image_size;
	last_pos = write_pos + write_size;
	poc_w_size = vdd->poc_driver.write_data_size;
	loop_cnt = vdd->poc_driver.write_loop_cnt;


	LCD_INFO("[WRITE] write_pos : %6d, write_size : %6d, last_pos %6d, poc_w_size : %6d delay %dus\n",
		write_pos, write_size, last_pos, poc_w_size, delay_us);

	/* MAX CPU ON */
	priv = display->drm_dev->dev_private;
	sde_kms = (struct sde_kms *)priv->kms;
	sde_mnoc_ab = sde_kms->core_client->ab[SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT];
	sde_mnoc_ib = sde_kms->core_client->ib[SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT];

	ss_set_max_cpufreq(vdd, true, CPUFREQ_CLUSTER_ALL);

	sde_power_data_bus_set_quota(&priv->phandle,
			sde_kms->core_client,
			SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT,
			SDE_POWER_HANDLE_DBUS_ID_MNOC,
			SDE_POWER_HANDLE_CONT_SPLASH_BUS_AB_QUOTA,
			SDE_POWER_HANDLE_CONT_SPLASH_BUS_IB_QUOTA);

	dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_ON);

	/* Enter exclusive mode */
	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.permit_frame_update = 1;	/* 1->frame will update  0->frame update blocked */
	vdd->exclusive_tx.enable = 1;
	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 1);

	/* POC MODE ENABLE */
	ss_send_cmd(vdd, TX_POC_ENABLE);
	ss_send_cmd(vdd, TX_POC_PRE_WRITE);

	for (pos = write_pos; pos < last_pos; ) {
		if (!(pos % DEBUG_POC_CNT))
			LCD_INFO("cur_write_pos : %d data : 0x%x\n", pos, data[pos]);

		if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
			LCD_ERR("cancel poc write by user\n");
			ret = -EIO;
			goto cancel_poc;
		}
		
		/* 256 Byte Write (128 and 128) */
		if (last_pos - pos >= poc_w_size) {
			tx_size = poc_w_size;
			tx_size1 = tx_size / 2;
			tx_size2 = tx_size / 2;
		} else {
			tx_size = last_pos - pos;
			tx_size1 = (tx_size / (poc_w_size / 2)) ? poc_w_size / 2 : tx_size;
			tx_size2 = (tx_size / (poc_w_size / 2)) ? tx_size - tx_size1 : 0;

			/* set as dummy data */
			memset(&write_cmd->cmds[1].msg.tx_buf[1], 0xFF, poc_w_size / 2);
			memset(&write_cmd->cmds[3].msg.tx_buf[1], 0xFF, poc_w_size / 2);
		}

		/* data copy */
		if (tx_size1)
			memcpy(&write_cmd->cmds[1].msg.tx_buf[1], &data[pos], tx_size1);
		if (tx_size2)
			memcpy(&write_cmd->cmds[3].msg.tx_buf[1], &data[pos + tx_size1], tx_size2);

		ss_send_cmd(vdd, TX_POC_WRITE_LOOP_256BYTE);

		if (pos % loop_cnt == 0) {
			LCD_DEBUG("WRITE_LOOP_START pos : %d \n", pos);
			ss_send_cmd(vdd, TX_POC_WRITE_LOOP_START);

			/*	Multi Data Address */
			write_data_add->cmds[0].msg.tx_buf[vdd->poc_driver.write_addr_idx[0]]
											= (pos & 0xFF0000) >> 16;
			write_data_add->cmds[0].msg.tx_buf[vdd->poc_driver.write_addr_idx[1]]
											= (pos & 0x00FF00) >> 8;
			write_data_add->cmds[0].msg.tx_buf[vdd->poc_driver.write_addr_idx[2]]
											= (pos & 0x0000FF);

		ss_send_cmd(vdd, TX_POC_WRITE_LOOP_DATA_ADD);
		}
		pos += tx_size;
	}

cancel_poc:
	if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
		LCD_ERR("cancel poc write by user\n");
		atomic_set(&vdd->poc_driver.cancel, 0);
		ret = -EIO;
	}

	if (pos == image_size || ret == -EIO) {
		LCD_INFO("WRITE [TX_POC_POST_WRITE] - image_size(%d) cur_write_pos(%d) ret(%d)\n", image_size, pos, ret);
		ss_send_cmd(vdd, TX_POC_POST_WRITE);
	}

	if(pos < image_size)
		ss_send_cmd(vdd, TX_POC_POST_WRITE);
	/* POC MODE DISABLE */
	ss_send_cmd(vdd, TX_POC_DISABLE);

	/* exit exclusive mode*/
	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 0);
	vdd->exclusive_tx.permit_frame_update = 0;
	vdd->exclusive_tx.enable = 0;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);

	/* MAX CPU OFF */
	dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_OFF);

	sde_power_data_bus_set_quota(&priv->phandle,
			sde_kms->core_client,
			SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT,
			SDE_POWER_HANDLE_DBUS_ID_MNOC,
			sde_mnoc_ab,
			sde_mnoc_ib);
	ss_set_max_cpufreq(vdd, false, CPUFREQ_CLUSTER_ALL);

	return ret;
}

#define read_buf_size 256
#define read_length 128
static int poc_read(struct samsung_display_driver_data *vdd, u8 *buf, u32 read_pos, u32 read_size)
{
	struct msm_drm_private *priv = NULL;
	struct sde_kms *sde_kms = NULL;
	u64 sde_mnoc_ab, sde_mnoc_ib;

	struct dsi_display *display = NULL;
	struct dsi_panel_cmd_set *poc_read_tx_cmds = NULL;
	struct dsi_panel_cmd_set *poc_read_rx_cmds = NULL;

	int delay_us;
	int image_size;
	int read_len = 0;
	u8 rx_buf[read_buf_size] = {0, };
	int pos;
	int type;
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("no vdd\n");
		return -EINVAL;
	}

	display = GET_DSI_DISPLAY(vdd);
	if (IS_ERR_OR_NULL(display)) {
		LCD_ERR("no display");
		return -EINVAL;
	}

	if (!ss_is_ready_to_send_cmd(vdd)) {
		LCD_ERR("Panel is not ready. Panel State(%d)\n", vdd->panel_state);
		return -EBUSY;
	}

	poc_read_tx_cmds = ss_get_cmds(vdd, TX_POC_READ);
	if (SS_IS_CMDS_NULL(poc_read_tx_cmds)) {
		LCD_ERR("no cmds for TX_POC_READ..\n");
		return -EINVAL;
	}

	poc_read_rx_cmds = ss_get_cmds(vdd, RX_POC_READ);
	if (SS_IS_CMDS_NULL(poc_read_rx_cmds)) {
		LCD_ERR("no cmds for RX_POC_READ..\n");
		return -EINVAL;
	}

	if (vdd->poc_driver.read_addr_idx[0] < 0) {
		LCD_ERR("read addr index is not implemented.. %d\n",
			vdd->poc_driver.read_addr_idx[0]);
		return -EINVAL;
	}

	delay_us = vdd->poc_driver.read_delay_us; /* Panel dtsi set */
	image_size = vdd->poc_driver.image_size;
	
	LCD_INFO("[READ] read_pos : %6d, read_size : %6d\n", read_pos, read_size);


	/* MAX CPU ON */
	priv = display->drm_dev->dev_private;
	sde_kms = (struct sde_kms *)priv->kms;
	sde_mnoc_ab = sde_kms->core_client->ab[SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT];
	sde_mnoc_ib = sde_kms->core_client->ib[SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT];

	ss_set_max_cpufreq(vdd, true, CPUFREQ_CLUSTER_ALL);
	sde_power_data_bus_set_quota(&priv->phandle,
			sde_kms->core_client,
			SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT,
			SDE_POWER_HANDLE_DBUS_ID_MNOC,
			SDE_POWER_HANDLE_CONT_SPLASH_BUS_AB_QUOTA,
			SDE_POWER_HANDLE_CONT_SPLASH_BUS_IB_QUOTA);
	dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_ON);

	/* Enter exclusive mode */
	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.permit_frame_update = 1;
	vdd->exclusive_tx.enable = 1;
	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 1);
	ss_set_exclusive_tx_packet(vdd, RX_POC_READ, 1);

	/* For sending direct rx cmd  */
	poc_read_rx_cmds->cmds[0].msg.rx_buf = rx_buf;
	poc_read_rx_cmds->state = DSI_CMD_SET_STATE_HS;

	ss_send_cmd(vdd, TX_POC_PRE_READ);

	for (pos = read_pos; pos < (read_pos + read_size);) {
		if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
			LCD_ERR("cancel poc read by user\n");
			ret = -EIO;
			goto cancel_poc;
		}

		poc_read_tx_cmds->cmds[0].msg.tx_buf[vdd->poc_driver.read_addr_idx[0]]
									= (pos & 0xFF0000) >> 16;
		poc_read_tx_cmds->cmds[0].msg.tx_buf[vdd->poc_driver.read_addr_idx[1]]
									= (pos & 0x00FF00) >> 8;
		poc_read_tx_cmds->cmds[0].msg.tx_buf[vdd->poc_driver.read_addr_idx[2]]
									= pos & 0x0000FF;

		ss_send_cmd(vdd, TX_POC_READ);

		ss_send_cmd(vdd, RX_POC_READ);

		read_len = (read_pos + read_size) - pos;	//read length remaining
		if(read_len > read_length)
			read_len = read_length;
		memcpy(&buf[pos], rx_buf, read_len);

		if (!(pos % DEBUG_POC_CNT))
			LCD_INFO("[POC]buf[%d] = 0x%06x\n", pos, buf[pos]);

		pos += read_len;
	}
	if(pos < image_size)
		ss_send_cmd(vdd, TX_POC_POST_READ);

cancel_poc:
	if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
		LCD_ERR("cancel poc read by user\n");
		atomic_set(&vdd->poc_driver.cancel, 0);
		ret = -EIO;
	}

	if (pos == image_size || ret == -EIO) {
		LCD_INFO("WRITE [TX_POC_POST_READ] - image_size(%d) cur_read_pos(%d) ret(%d)\n", image_size, pos, ret);
		ss_send_cmd(vdd, TX_POC_POST_READ);
	}


	/* MAX CPU OFF */
	dsi_display_clk_ctrl(display->dsi_clk_handle, DSI_ALL_CLKS, DSI_CLK_OFF);
	sde_power_data_bus_set_quota(&priv->phandle,
			sde_kms->core_client,
			SDE_POWER_HANDLE_DATA_BUS_CLIENT_RT,
			SDE_POWER_HANDLE_DBUS_ID_MNOC,
			sde_mnoc_ab,
			sde_mnoc_ib);
	ss_set_max_cpufreq(vdd, false, CPUFREQ_CLUSTER_ALL);

	/* Exit exclusive mode*/
	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 0);
	ss_set_exclusive_tx_packet(vdd, RX_POC_READ, 0);
	vdd->exclusive_tx.permit_frame_update = 0;
	vdd->exclusive_tx.enable = 0;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);

	return ret;
}

static void samsung_panel_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO("S6E3FA9_AMB667UM06 : ++ \n");
	LCD_ERR("%s\n", ss_get_panel_name(vdd));

	/* Default Panel Power Status is OFF */
	vdd->panel_state = PANEL_PWR_OFF;

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = samsung_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = samsung_panel_on_post;
	vdd->panel_func.samsung_panel_off_pre = samsung_panel_off_pre;
	vdd->panel_func.samsung_panel_off_post = samsung_panel_off_post;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = ss_panel_revision;
	vdd->panel_func.samsung_manufacture_date_read = ss_manufacture_date_read;
	vdd->panel_func.samsung_ddi_id_read = ss_ddi_id_read;
	vdd->panel_func.samsung_smart_dimming_init = NULL;
	vdd->panel_func.samsung_cell_id_read = ss_cell_id_read;
	vdd->panel_func.samsung_octa_id_read = ss_octa_id_read;
	vdd->panel_func.samsung_elvss_read = NULL;
	vdd->panel_func.samsung_mdnie_read = ss_mdnie_read;

	/* Brightness */
	vdd->panel_func.samsung_brightness_gamma_mode2 = mdss_brightness_gamma_mode2;
	vdd->panel_func.samsung_brightness_acl_on = ss_acl_on;
	vdd->panel_func.samsung_brightness_acl_off = ss_acl_off;
	vdd->panel_func.samsung_brightness_hbm_off = NULL;
	vdd->panel_func.samsung_brightness_acl_percent = NULL;
	vdd->panel_func.samsung_brightness_elvss = NULL;
	vdd->panel_func.samsung_brightness_elvss_temperature1 = NULL;
	vdd->panel_func.samsung_brightness_elvss_temperature2 = NULL;
	vdd->panel_func.samsung_brightness_vint = ss_vint;
	vdd->panel_func.samsung_brightness_gamma = NULL;

	vdd->smart_dimming_loaded_dsi = false;

	/* Event */
	vdd->panel_func.samsung_change_ldi_fps = NULL;

	/* HMT */
	vdd->panel_func.samsung_brightness_gamma_hmt = NULL;
	vdd->panel_func.samsung_brightness_aid_hmt = NULL;
	vdd->panel_func.samsung_brightness_elvss_hmt = NULL;
	vdd->panel_func.samsung_brightness_vint_hmt = NULL;
	vdd->panel_func.samsung_smart_dimming_hmt_init = NULL;
	vdd->panel_func.samsung_smart_get_conf_hmt = NULL;
	//vdd->panel_func.samsung_brightness_hmt = NULL;

	/* Panel LPM */
	vdd->panel_func.samsung_update_lpm_ctrl_cmd = ss_update_panel_lpm_ctrl_cmd;
	vdd->panel_func.samsung_set_lpm_brightness = ss_set_panel_lpm_brightness;

	/* default brightness */
	vdd->br.bl_level = 255;

	/* mdnie */
	vdd->mdnie.support_mdnie = true;

	vdd->mdnie.support_trans_dimming = true;
	vdd->mdnie.mdnie_tune_size[0] = sizeof(DSI0_BYPASS_MDNIE_1);
	vdd->mdnie.mdnie_tune_size[1] = sizeof(DSI0_BYPASS_MDNIE_2);
	vdd->mdnie.mdnie_tune_size[2] = sizeof(DSI0_BYPASS_MDNIE_3);
	/*vdd->mdnie.mdnie_tune_size[3] = sizeof(DSI0_BYPASS_MDNIE_4);
	vdd->mdnie.mdnie_tune_size[4] = sizeof(DSI0_BYPASS_MDNIE_5);
	vdd->mdnie.mdnie_tune_size[5] = sizeof(DSI0_BYPASS_MDNIE_6);*/

	dsi_update_mdnie_data(vdd);

	/* send recovery pck before sending image date (for ESD recovery) */
	vdd->esd_recovery.send_esd_recovery = false;

	/* Enable panic on first pingpong timeout */
	//vdd->debug_data->panic_on_pptimeout = true;

	/* COLOR WEAKNESS */
	vdd->panel_func.color_weakness_ccb_on_off =  NULL;

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = NULL;

	/* COVER Open/Close */
	vdd->panel_func.samsung_cover_control = NULL;

	/* COPR */
	vdd->copr.panel_init = ss_copr_panel_init;

	/* ACL default ON */
	vdd->acl_status = 1;

	/*Default Temperature*/
	vdd->temperature = 20;

	/* ACL default status in acl on */
	vdd->gradual_acl_val = 1;

	/* Gram Checksum Test */
	vdd->panel_func.samsung_gct_write = NULL;
	vdd->panel_func.samsung_gct_read = NULL;

	/* Self display */
	vdd->self_disp.is_support = true;
	vdd->self_disp.factory_support = true;
	vdd->self_disp.init = self_display_init_FA9;
	vdd->self_disp.data_init = ss_self_display_data_init;

	/* POC */
	vdd->poc_driver.poc_erase = poc_erase;
	vdd->poc_driver.poc_write = poc_write;
	vdd->poc_driver.poc_read = poc_read;

	/* SAMSUNG_FINGERPRINT */
	vdd->panel_hbm_entry_delay = 2;

	LCD_INFO("S6E3FA9_AMB667UM06 : -- \n");
}

static int __init samsung_panel_initialize(void)
{
	struct samsung_display_driver_data *vdd;
	enum ss_display_ndx ndx;
	char panel_string[] = "ss_dsi_panel_S6E3FA9_AMB667UM06_FHD";
	char panel_name[MAX_CMDLINE_PARAM_LEN];
	char panel_secondary_name[MAX_CMDLINE_PARAM_LEN];

	LCD_INFO("S6E3FA9_AMB667UM06 : ++ \n");

	ss_get_primary_panel_name_cmdline(panel_name);
	ss_get_secondary_panel_name_cmdline(panel_secondary_name);

	/* TODO: use component_bind with panel_func
	 * and match by panel_string, instead.
	 */
	if (!strncmp(panel_string, panel_name, strlen(panel_string)))
		ndx = PRIMARY_DISPLAY_NDX;
	else if (!strncmp(panel_string, panel_secondary_name, strlen(panel_string)))
		ndx = SECONDARY_DISPLAY_NDX;
	else {
		LCD_ERR("can not find panel_name (%s) / (%s)\n", panel_string, panel_name);
		return 0;
	}
	
	vdd = ss_get_vdd(ndx);
	vdd->panel_func.samsung_panel_init = samsung_panel_init;

	LCD_INFO("%s done.. \n", panel_name);
	LCD_INFO("S6E3FA9_AMB667UM06 : -- \n");

	return 0;
}

early_initcall(samsung_panel_initialize);
