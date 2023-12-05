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
#include "ss_dsi_panel_SW83106.h"
#include "ss_dsi_mdnie_SW83106.h"


static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
	        LCD_ERR(": Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	LCD_INFO("+: ndx=%d\n", vdd->ndx);
	ss_panel_attach_set(vdd, true);

	if (gpio_is_valid(vdd->ub_fd.gpio)) {
		gpio_set_value(vdd->ub_fd.gpio, 1);
		LCD_ERR("FD gpio set to high\n");
	} else {
		LCD_ERR("FD gpio is not valid\n");
		return -ENODEV;
	}

	return true;
}

static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	char ddi_id_buffer[MAX_CHIP_ID] = {0,};
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read Panel Unique chip/ddi id (F1h 1~10th) for CHIP ID */
	if (ss_get_cmds(vdd, RX_DDI_ID)->count) {
		memset(ddi_id_buffer, 0x00, MAX_CHIP_ID);

		ss_panel_data_read(vdd, RX_DDI_ID, ddi_id_buffer, LEVEL2_KEY);

		for (loop = 0; loop < 10; loop++){
			vdd->ddi_id_dsi[loop] = ddi_id_buffer[loop];
		}

		LCD_INFO("DSI%d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			vdd->ndx, vdd->ddi_id_dsi[0],
			vdd->ddi_id_dsi[1],	vdd->ddi_id_dsi[2],
			vdd->ddi_id_dsi[3],	vdd->ddi_id_dsi[4],
			vdd->ddi_id_dsi[5],	vdd->ddi_id_dsi[6],
			vdd->ddi_id_dsi[7],	vdd->ddi_id_dsi[8],
			vdd->ddi_id_dsi[9]);

	} else {
		LCD_ERR("DSI%d no ddi_id_rx_cmds cmds", vdd->ndx);
		return false;
	}

	return true;
}

static int ss_cell_id_read(struct samsung_display_driver_data *vdd)
{
	char cell_id_buffer[MAX_CELL_ID] = {0,};
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read Panel Unique Cell ID (DDh 1~13th) */
	if (ss_get_cmds(vdd, RX_CELL_ID)->count) {
		memset(cell_id_buffer, 0x00, MAX_CELL_ID);

		ss_panel_data_read(vdd, RX_CELL_ID, cell_id_buffer, LEVEL1_KEY);

		for (loop = 0; loop < 13; loop++)
			vdd->cell_id_dsi[loop] = cell_id_buffer[loop];

		LCD_INFO("DSI%d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			vdd->ndx, vdd->cell_id_dsi[0],
			vdd->cell_id_dsi[1],	vdd->cell_id_dsi[2],
			vdd->cell_id_dsi[3],	vdd->cell_id_dsi[4],
			vdd->cell_id_dsi[5],	vdd->cell_id_dsi[6],
			vdd->cell_id_dsi[7],	vdd->cell_id_dsi[8],
			vdd->cell_id_dsi[9],	vdd->cell_id_dsi[10],
			vdd->cell_id_dsi[11],	vdd->cell_id_dsi[12]);

	} else {
		LCD_ERR("DSI%d no cell_id_rx_cmds cmd\n", vdd->ndx);
		return false;
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

	LCD_INFO("%s : cd_idx [%d] \n",(vdd->br.cd_idx <= MAX_BL_PF_LEVEL ? "Normal" : "HBM"), vdd->br.cd_idx);
	pcmds->cmds[0].msg.tx_buf[1] = get_bit(vdd->br.cd_level, 8, 3);
	pcmds->cmds[0].msg.tx_buf[2] = get_bit(vdd->br.cd_level, 0, 8);
	pcmds->cmds[1].msg.tx_buf[1] = vdd->temperature + 0x80;

	return pcmds;
}

static int ss_ub_fd_control(struct samsung_display_driver_data *vdd, int enable)
{
	int rc = 0;
	int loop = 0;

	LCD_INFO("Fast Discharging Contorl %s\n", enable?"Enable":"Disable");

	if (enable) { /* Enable */
		if (gpio_is_valid(vdd->ub_fd.gpio)) {
			rc = gpio_direction_output(vdd->ub_fd.gpio, 1);
			if (rc) {
				LCD_ERR("unable to set dir for fd gpio rc=%d\n", rc);
				return rc;
			}

			for (loop = 0; loop < vdd->ub_fd.fd_on_count ; loop++) {
				gpio_set_value(vdd->ub_fd.gpio, 0);
				udelay(5);
				gpio_set_value(vdd->ub_fd.gpio, 1);
				udelay(5);
			}

			usleep_range(250, 300);
		} else {
			LCD_ERR("FD gpio is not valid\n");
			return -ENODEV;
		}
	} else { /* Disable */
		if (gpio_is_valid(vdd->ub_fd.gpio)) {
			rc = gpio_direction_output(vdd->ub_fd.gpio, 1);
			if (rc) {
				LCD_ERR("unable to set dir for fd gpio rc=%d\n", rc);
				return rc;
			}

			for (loop = 0; loop < vdd->ub_fd.fd_off_count ; loop++) {
				gpio_set_value(vdd->ub_fd.gpio, 0);
				udelay(5);
				gpio_set_value(vdd->ub_fd.gpio, 1);
				udelay(5);
			}

			usleep_range(250, 300);
		} else {
			LCD_ERR("FD gpio is not valid\n");
			return -ENODEV;
		}
	}

	return rc;
}

static struct dsi_panel_cmd_set *ss_acl_on(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_ACL_ON);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR("No cmds for TX_ACL_ON..\n");
		return NULL;
	}
	if(vdd->br.cd_idx <= MAX_BL_PF_LEVEL){	/* ACL 15% */
		pcmds->cmds[3].msg.tx_buf[7] = 0x6A;
		pcmds->cmds[3].msg.tx_buf[8] = 0xDE;
		pcmds->cmds[3].msg.tx_buf[9] = 0x4C;
		pcmds->cmds[3].msg.tx_buf[10] = 0xBE;
	}
	else{		/* ACL 8% */
		pcmds->cmds[3].msg.tx_buf[7] = 0x7F;
		pcmds->cmds[3].msg.tx_buf[8] = 0xFF;
		pcmds->cmds[3].msg.tx_buf[9] = 0x6D;
		pcmds->cmds[3].msg.tx_buf[10] = 0xDC;
	}

	LCD_INFO("acl per: 0x%x", pcmds->cmds[0].msg.tx_buf[1]);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_acl_off(struct samsung_display_driver_data *vdd, int *level_key)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	LCD_INFO("off\n");
	return ss_get_cmds(vdd, TX_ACL_OFF);
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
	struct dsi_panel_cmd_set *cmd_list;
	int mode = ALPM_MODE_ON;
	int bl_index = LPM_2NIT_IDX;

	LCD_INFO("%s++\n", __func__);

	cmd_list = ss_get_cmds(vdd, TX_LPM_ON);
	if (SS_IS_CMDS_NULL(cmd_list)) {
		LCD_ERR("No cmds for TX_LPM_BL_CMD..\n");
		return;
	}
	/* Init alpm_brightness and alpm_ctrl cmds */
	alpm_brightness[LPM_2NIT_IDX] = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
	alpm_brightness[LPM_10NIT_IDX] = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
	alpm_brightness[LPM_30NIT_IDX] = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
	alpm_brightness[LPM_60NIT_IDX] = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
	if (SS_IS_CMDS_NULL(alpm_brightness[LPM_2NIT_IDX]) || SS_IS_CMDS_NULL(alpm_brightness[LPM_10NIT_IDX]) ||
		SS_IS_CMDS_NULL(alpm_brightness[LPM_30NIT_IDX]) || SS_IS_CMDS_NULL(alpm_brightness[LPM_60NIT_IDX])) {
		LCD_ERR("No cmds for alpm_brightness..\n");
		return;
	}

	mode = vdd->panel_lpm.mode;

	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_10NIT:
		bl_index = LPM_10NIT_IDX;
		break;
	case LPM_30NIT:
		bl_index = LPM_30NIT_IDX;
		break;
	case LPM_60NIT:
		bl_index = LPM_60NIT_IDX;
		break;
	case LPM_2NIT:
	default:
		bl_index = LPM_2NIT_IDX;
		break;
	}

	LCD_INFO("[Panel LPM] change brightness cmd bl_index :%d, mode : %d\n",
			 bl_index, mode);

	memcpy(cmd_list->cmds[1].msg.tx_buf,
			alpm_brightness[bl_index]->cmds[0].msg.tx_buf,
			sizeof(char) * cmd_list->cmds[1].msg.tx_len);

	LCD_INFO("[Panel LPM] change brightness cmd : %x, %x\n",
			cmd_list->cmds[0].msg.tx_buf[1],
			alpm_brightness[bl_index]->cmds[0].msg.tx_buf[1]);

	LCD_ERR("%s--\n", __func__);
}

static void ss_set_panel_lpm_brightness(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *alpm_brightness[LPM_BRIGHTNESS_MAX_IDX] = {NULL, };
	struct dsi_panel_cmd_set *cmd_list;
	int mode = ALPM_MODE_ON;
	int bl_index = LPM_2NIT_IDX;

	LCD_INFO("%s++\n", __func__);

	cmd_list = ss_get_cmds(vdd, TX_LPM_BL_CMD);
	if (SS_IS_CMDS_NULL(cmd_list)) {
		LCD_ERR("No cmds for TX_LPM_BL_CMD..\n");
		return;
	}

	/* Init alpm_brightness and alpm_ctrl cmds */
	alpm_brightness[LPM_2NIT_IDX] = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
	alpm_brightness[LPM_10NIT_IDX] = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
	alpm_brightness[LPM_30NIT_IDX] = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
	alpm_brightness[LPM_60NIT_IDX] = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
	if (SS_IS_CMDS_NULL(alpm_brightness[LPM_2NIT_IDX]) || SS_IS_CMDS_NULL(alpm_brightness[LPM_10NIT_IDX]) ||
		SS_IS_CMDS_NULL(alpm_brightness[LPM_30NIT_IDX]) || SS_IS_CMDS_NULL(alpm_brightness[LPM_60NIT_IDX])) {
		LCD_ERR("No cmds for alpm_brightness..\n");
		return;
	}

	mode = vdd->panel_lpm.mode;

	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_10NIT:
		bl_index = LPM_10NIT_IDX;
		break;
	case LPM_30NIT:
		bl_index = LPM_30NIT_IDX;
		break;
	case LPM_60NIT:
		bl_index = LPM_60NIT_IDX;
		break;
	case LPM_2NIT:
	default:
		bl_index = LPM_2NIT_IDX;
		break;
	}

	LCD_INFO("[Panel LPM] lpm_bl_level = %d bl_index %d, mode %d\n",
			 vdd->panel_lpm.lpm_bl_level, bl_index, mode);


	memcpy(cmd_list->cmds[0].msg.tx_buf,
			alpm_brightness[bl_index]->cmds[0].msg.tx_buf,
			sizeof(char) * cmd_list->cmds[0].msg.tx_len);

	LCD_INFO("[Panel LPM] change brightness cmd : %x, %x\n",
			cmd_list->cmds[0].msg.tx_buf[1],
			alpm_brightness[bl_index]->cmds[0].msg.tx_buf[1]);

	//send lpm bl cmd
	ss_send_cmd(vdd, TX_LPM_BL_CMD);

	LCD_INFO("[Panel LPM] bl_level : %s\n",
				/* Check current brightness level */
				vdd->panel_lpm.lpm_bl_level == LPM_2NIT ? "2NIT" :
				vdd->panel_lpm.lpm_bl_level == LPM_10NIT ? "10NIT" :
				vdd->panel_lpm.lpm_bl_level == LPM_30NIT ? "30NIT" :
				vdd->panel_lpm.lpm_bl_level == LPM_60NIT ? "60NIT" : "UNKNOWN");

	LCD_DEBUG("%s--\n", __func__);
}

static int samsung_panel_off_pre(struct samsung_display_driver_data *vdd)
{
	int rc = 0;
	return rc;
}

static int samsung_panel_off_post(struct samsung_display_driver_data *vdd)
{
	int rc = 0;

	/* Fast Discharging Control */
	if (unlikely(vdd->is_factory_mode)) {
		rc = ss_ub_fd_control(vdd, true);
		if (rc) {
			LCD_ERR("Failed to UB_FD control, rc=%d\n", rc);
			return rc;
		}
	}

	if (gpio_is_valid(vdd->ub_fd.gpio)) {
		gpio_set_value(vdd->ub_fd.gpio, 0);
		LCD_ERR("FD gpio set to low\n");
	} else {
		LCD_ERR("FD gpio is not valid\n");
		return -ENODEV;
	}

	return rc;
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
	mdnie_data->DSI_RGB_SENSOR_MDNIE_1 = NULL;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_2 = NULL;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_3 = NULL;
	mdnie_data->DSI_TRANS_DIMMING_MDNIE = NULL;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE_2 = NULL;
	mdnie_data->DSI_UI_STANDARD_MDNIE_2 = NULL;
	mdnie_data->DSI_UI_AUTO_MDNIE_2 = DSI0_UI_AUTO_MDNIE_2;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE_2 = NULL;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE_2 = NULL;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE_2 = DSI0_VIDEO_AUTO_MDNIE_2;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE_2 = DSI0_CAMERA_AUTO_MDNIE_2;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE_2 = NULL;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE_2 = NULL;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE_2 = DSI0_GALLERY_AUTO_MDNIE_2;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE_2 = NULL;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE_2 = NULL;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE_2 = DSI0_BROWSER_AUTO_MDNIE_2;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE_2 = DSI0_EBOOK_AUTO_MDNIE_2;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE_2 = NULL;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE_2 = NULL;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE_2 = NULL;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE_2 = NULL;
	mdnie_data->DSI_TDMB_AUTO_MDNIE_2 = DSI0_TDMB_AUTO_MDNIE_2;

	mdnie_data->DSI_BYPASS_MDNIE = DSI0_BYPASS_MDNIE;
	mdnie_data->DSI_NEGATIVE_MDNIE = DSI0_NEGATIVE_MDNIE;
	mdnie_data->DSI_GRAYSCALE_MDNIE = DSI0_GRAYSCALE_MDNIE;
	mdnie_data->DSI_GRAYSCALE_NEGATIVE_MDNIE = DSI0_GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data->DSI_COLOR_BLIND_MDNIE = DSI0_COLOR_BLIND_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE = DSI0_NIGHT_MODE_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE_SCR = DSI0_NIGHT_MODE_MDNIE_7;
	mdnie_data->DSI_NIGHT_MODE_MDNIE_SCR_EXTRA = DSI0_NIGHT_MODE_MDNIE_6;	/*this is special case for SiliconWorks DDI SW83106*/
	mdnie_data->DSI_HBM_CE_MDNIE = DSI0_HBM_CE_MDNIE;
	mdnie_data->DSI_HBM_CE_D65_MDNIE = DSI0_HBM_CE_D65_MDNIE;
	mdnie_data->DSI_CURTAIN = DSI0_SCREEN_CURTAIN_MDNIE;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE = DSI0_UI_DYNAMIC_MDNIE;
	mdnie_data->DSI_UI_AUTO_MDNIE = DSI0_UI_AUTO_MDNIE;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE = DSI0_VIDEO_DYNAMIC_MDNIE;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE = DSI0_VIDEO_AUTO_MDNIE;

	mdnie_data->DSI_CAMERA_AUTO_MDNIE = DSI0_CAMERA_AUTO_MDNIE;

	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE = DSI0_GALLERY_DYNAMIC_MDNIE;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE = DSI0_GALLERY_AUTO_MDNIE;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE = DSI0_BROWSER_DYNAMIC_MDNIE;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE = DSI0_BROWSER_AUTO_MDNIE;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE = DSI0_EBOOK_DYNAMIC_MDNIE;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE = DSI0_EBOOK_AUTO_MDNIE;
	mdnie_data->DSI_EMAIL_AUTO_MDNIE = DSI0_EMAIL_AUTO_MDNIE;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE = DSI0_TDMB_DYNAMIC_MDNIE;
	mdnie_data->DSI_TDMB_AUTO_MDNIE = DSI0_TDMB_AUTO_MDNIE;

	mdnie_data->mdnie_tune_value_dsi = mdnie_tune_value_dsi0;
	mdnie_data->hmt_color_temperature_tune_value_dsi = hmt_color_temperature_tune_value_dsi0;
	mdnie_data->light_notification_tune_value_dsi = light_notification_tune_value_dsi0;
	mdnie_data->hdr_tune_value_dsi = hdr_tune_value_dsi0;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data->dsi_bypass_mdnie_size = ARRAY_SIZE(DSI0_BYPASS_MDNIE);

	mdnie_data->mdnie_step_index[MDNIE_STEP1] = MDNIE_STEP1_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP2] = MDNIE_STEP2_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP3] = MDNIE_STEP3_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP4] = MDNIE_STEP4_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP5] = MDNIE_STEP5_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP6] = MDNIE_STEP6_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP7] = MDNIE_STEP7_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP8] = MDNIE_STEP8_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP9] = MDNIE_STEP9_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP10] = MDNIE_STEP10_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP11] = MDNIE_STEP11_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP12] = MDNIE_STEP12_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP13] = MDNIE_STEP13_INDEX;

	mdnie_data->mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET_CUSTOM_1;
	mdnie_data->mdnie_color_blinde_cmd_offset_extra = MDNIE_COLOR_BLINDE_CMD_OFFSET_CUSTOM_2;
	mdnie_data->dsi_night_mode_table = night_mode_data;
	mdnie_data->dsi_max_night_mode_index = 11;
	mdnie_data->dsi_night_mode_table_extra = night_mode_data_extra;
	mdnie_data->dsi_max_night_mode_index_extra =11;
	mdnie_data->dsi_scr_step_index = MDNIE_STEP5_INDEX;

	mdnie_data->support_mode = 1;
	mdnie_data->support_scenario = 1;
	mdnie_data->support_outdoor = 0;
	mdnie_data->support_bypass = 1;
	mdnie_data->support_accessibility = 0;
	mdnie_data->support_sensorRGB = 0;
	mdnie_data->support_whiteRGB = 1;
	mdnie_data->support_mdnie_ldu = 0;
	mdnie_data->support_night_mode = 1;
	mdnie_data->support_color_lens = 0;
	mdnie_data->support_hdr = 1;
	mdnie_data->support_light_notification = 1;
	mdnie_data->support_afc = 0;
	mdnie_data->support_cabc = 0;
	mdnie_data->support_hmt_color_temperature = 0;
	mdnie_data->support_siliconworks = 1;

/*
	mdnie_data->dsi_afc_size = 45;
	mdnie_data->dsi_afc_index = 33;
*/
	vdd->mdnie.mdnie_data = mdnie_data;

	return 0;
}

static int ss_gct_read(struct samsung_display_driver_data *vdd)
{
	int res;

	if (!vdd->gct.is_support)
		return GCT_RES_CHECKSUM_NOT_SUPPORT;

	if (!vdd->gct.on)
		return GCT_RES_CHECKSUM_OFF;

	if (vdd->gct.checksum[2] ==  vdd->gct.checksum[3])
		res = GCT_RES_CHECKSUM_PASS;
	else
		res = GCT_RES_CHECKSUM_NG;

	return res;
}

static int ss_gct_write(struct samsung_display_driver_data *vdd)
{
	u8 *checksum;

	int ret = 0;
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct dsi_panel_cmd_set *set;

	LCD_INFO("+\n");

	/* prevent sw reset to trigger esd recovery */
	LCD_INFO("disable esd interrupt\n");
	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd);

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.enable = 1;

mdelay(10);

	checksum = vdd->gct.checksum;
	vdd->panel_state = PANEL_PWR_OFF;
	dsi_panel_power_off(panel);
	dsi_panel_power_on(panel);
	vdd->panel_state = PANEL_PWR_ON_READY;

	ss_set_exclusive_tx_packet(vdd, TX_GCT_ENTER, 1);
	ss_set_exclusive_tx_packet(vdd, TX_GCT_EXIT, 1);
	ss_set_exclusive_tx_packet(vdd, RX_GCT_CHECKSUM, 1);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 1);

	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_ON, 1);
	ss_set_exclusive_tx_packet(vdd, TX_DISPLAY_ON, 1);

	ss_send_cmd(vdd, DSI_CMD_SET_ON);

	LCD_INFO("TX_GCT_ENTER\n");
	set = ss_get_cmds(vdd, TX_GCT_ENTER);
	ss_send_cmd(vdd, TX_GCT_ENTER);


	set = ss_get_cmds(vdd, TX_DISPLAY_ON);
	ss_send_cmd(vdd, TX_DISPLAY_ON);

	mdelay(160);

	ss_panel_data_read(vdd, RX_GCT_CHECKSUM, checksum++,
				LEVEL_KEY_NONE);


	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_ON, 0);
	ss_set_exclusive_tx_packet(vdd, TX_DISPLAY_ON, 0);

	/* exit exclusive mode*/
	ss_set_exclusive_tx_packet(vdd, TX_GCT_ENTER, 0);
	ss_set_exclusive_tx_packet(vdd, TX_GCT_EXIT, 0);
	ss_set_exclusive_tx_packet(vdd, RX_GCT_CHECKSUM, 0);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 0);

	vdd->gct.on = 1;

	LCD_INFO("checksum = {%x %x %x %x}\n",
			vdd->gct.checksum[0], vdd->gct.checksum[1],
			vdd->gct.checksum[2], vdd->gct.checksum[3]);

	/* Reset panel to enter nornal panel mode.
	 * The on commands also should be sent before update new frame.
	 * Next new frame update is blocked by exclusive_tx.enable
	 * in ss_event_frame_update_pre(), and it will be released
	 * by wake_up exclusive_tx.ex_tx_waitq.
	 * So, on commands should be sent before wake up the waitq
	 * and set exclusive_tx.enable to false.
	 */
	vdd->panel_state = PANEL_PWR_OFF;
	dsi_panel_power_off(panel);
	dsi_panel_power_on(panel);
	vdd->panel_state = PANEL_PWR_ON_READY;

	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_ON, 1);
	ss_set_exclusive_tx_packet(vdd, TX_GCT_EXIT, 1);

	ss_send_cmd(vdd, DSI_CMD_SET_ON);
	ss_send_cmd(vdd, TX_GCT_EXIT);

	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_ON, 0);
	ss_set_exclusive_tx_packet(vdd, TX_GCT_EXIT, 0);

	vdd->exclusive_tx.enable = 0;
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);

	ss_panel_on_post(vdd);

	/* enable esd interrupt */
	LCD_INFO("enable esd interrupt\n");
	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(true, true, (void *)vdd);

	return ret;
}

static void samsung_panel_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO("SW83106 : ++ \n");
	LCD_ERR("%s\n", ss_get_panel_name(vdd));

	/* Default Panel Power Status is OFF */
	vdd->panel_state = PANEL_PWR_OFF;

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = samsung_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = NULL;
	vdd->panel_func.samsung_panel_off_pre = samsung_panel_off_pre;
	vdd->panel_func.samsung_panel_off_post = samsung_panel_off_post;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = ss_panel_revision;
	vdd->panel_func.samsung_manufacture_date_read = NULL;
	vdd->panel_func.samsung_ddi_id_read = ss_ddi_id_read;
	vdd->panel_func.samsung_smart_dimming_init = NULL;
	vdd->panel_func.samsung_cell_id_read = ss_cell_id_read;
	vdd->panel_func.samsung_octa_id_read = NULL;
	vdd->panel_func.samsung_elvss_read = NULL;
	vdd->panel_func.samsung_mdnie_read = NULL;

	/* Brightness */
	vdd->panel_func.samsung_brightness_gamma_mode2 = mdss_brightness_gamma_mode2;
	vdd->panel_func.samsung_brightness_acl_on = ss_acl_on;
	vdd->panel_func.samsung_brightness_acl_off = ss_acl_off;
	vdd->panel_func.samsung_brightness_hbm_off = NULL;
	vdd->panel_func.samsung_brightness_acl_percent = NULL;
	vdd->panel_func.samsung_brightness_elvss = NULL;
	vdd->panel_func.samsung_brightness_elvss_temperature1 = NULL;
	vdd->panel_func.samsung_brightness_elvss_temperature2 = NULL;
	vdd->panel_func.samsung_brightness_vint = NULL;
	vdd->panel_func.samsung_brightness_gamma = NULL;

	vdd->smart_dimming_loaded_dsi = false;

	/* Event */
	vdd->panel_func.samsung_change_ldi_fps = NULL;

	/* Fast Discharging (FD) */
	vdd->panel_func.ub_fd_control = ss_ub_fd_control;

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

	vdd->mdnie.mdnie_tune_size[0] = sizeof(DSI0_BYPASS_MDNIE_1);
	vdd->mdnie.mdnie_tune_size[1] = sizeof(DSI0_BYPASS_MDNIE_2);
	vdd->mdnie.mdnie_tune_size[2] = sizeof(DSI0_BYPASS_MDNIE_3);
	vdd->mdnie.mdnie_tune_size[3] = sizeof(DSI0_BYPASS_MDNIE_4);
	vdd->mdnie.mdnie_tune_size[4] = sizeof(DSI0_BYPASS_MDNIE_5);
	vdd->mdnie.mdnie_tune_size[5] = sizeof(DSI0_BYPASS_MDNIE_6);
	vdd->mdnie.mdnie_tune_size[6] = sizeof(DSI0_BYPASS_MDNIE_7);
	vdd->mdnie.mdnie_tune_size[7] = sizeof(DSI0_BYPASS_MDNIE_8);
	vdd->mdnie.mdnie_tune_size[8] = sizeof(DSI0_BYPASS_MDNIE_9);
	vdd->mdnie.mdnie_tune_size[9] = sizeof(DSI0_BYPASS_MDNIE_10);
	vdd->mdnie.mdnie_tune_size[10] = sizeof(DSI0_BYPASS_MDNIE_11);
	vdd->mdnie.mdnie_tune_size[11] = sizeof(DSI0_BYPASS_MDNIE_12);
	vdd->mdnie.mdnie_tune_size[12] = sizeof(DSI0_BYPASS_MDNIE_13);
	vdd->mdnie.mdnie_tune_size[13] = sizeof(DSI0_BYPASS_MDNIE_14);
	vdd->mdnie.mdnie_tune_size[14] = sizeof(DSI0_BYPASS_MDNIE_15);

	dsi_update_mdnie_data(vdd);

	/* send recovery pck before sending image date (for ESD recovery) */
	vdd->esd_recovery.send_esd_recovery = false;

	/* COLOR WEAKNESS */
	vdd->panel_func.color_weakness_ccb_on_off =  NULL;

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = NULL;

	/* COVER Open/Close */
	vdd->panel_func.samsung_cover_control = NULL;

	/* COPR */
	vdd->copr.panel_init = NULL;

	/*Default Temperature*/
	vdd->temperature = 20;

	/* Gram Checksum Test */
	vdd->panel_func.samsung_gct_write = ss_gct_write;
	vdd->panel_func.samsung_gct_read = ss_gct_read;

	/* Self display */
	vdd->self_disp.is_support = false;
	vdd->self_disp.factory_support = true;
	vdd->self_disp.init = NULL;
	vdd->self_disp.data_init = NULL;

	/*siliconworks ddi*/
	vdd->siliconworks_ddi  = 1;

	LCD_INFO("SW83106 : -- \n");
}

static int __init samsung_panel_initialize(void)
{
	struct samsung_display_driver_data *vdd;
	enum ss_display_ndx ndx;
	char panel_string[] = "ss_dsi_panel_SW83106_FHD";
	char panel_name[MAX_CMDLINE_PARAM_LEN];
	char panel_secondary_name[MAX_CMDLINE_PARAM_LEN];

	LCD_INFO("SW83106 : ++ \n");

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
	LCD_INFO("SW83106 : -- \n");

	return 0;
}

early_initcall(samsung_panel_initialize);
