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
#include "ss_dsi_panel_ANA6705_AMS667UK01.h"

/* AOD Mode status on AOD Service */

enum {
	HLPM_CTRL_2NIT,
	HLPM_CTRL_10NIT,
	HLPM_CTRL_30NIT,
	HLPM_CTRL_60NIT,
	MAX_LPM_CTRL,
};


/* Register to control brightness level */
#define ALPM_REG	0x53
/* Register to cnotrol ALPM/HLPM mode */
#define ALPM_CTRL_REG	0xBB

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx", __func__, (size_t)vdd);
		return false;
	}

	LCD_INFO("+: ndx=%d\n", vdd->ndx);
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
		LCD_ERR("Invalid panel_rev(default rev : %c)\n",
				vdd->panel_revision);
		break;
	}

	vdd->panel_revision -= 'A';

	LCD_INFO_ONCE("panel_revision = %c %d \n",
					vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}


#define get_bit(value, shift, width)	((value >> shift) & (GENMASK(width - 1, 0)))
static struct dsi_panel_cmd_set * mdss_brightness_gamma_mode2(struct samsung_display_driver_data *vdd, int *level_key)
{
	//struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmd_set *pcmds;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx", __func__,  (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2);
	if(vdd->br.cd_idx <= MAX_BL_PF_LEVEL){
		pcmds->cmds[0].msg.tx_buf[1] = vdd->finger_mask_updated? 0x20 : 0x28;  /* Normal Smooth transition : 0x28 */
		pcmds->cmds[1].msg.tx_buf[6] = 0x16;  /* ELVSS Value for normal brgihtness */
	}
	else{
		pcmds->cmds[0].msg.tx_buf[1] = vdd->finger_mask_updated? 0xE0 : 0xE8;	/* HBM Smooth transition : 0xE8 */
		pcmds->cmds[1].msg.tx_buf[6] = elvss_table[vdd->br.cd_level]; /* ELVSS Value for HBM brgihtness */
	}	


	/* B7 8th para:(Normal Elvss Offset),9th para:(HBM Elvss Offset) OTP, 46th para:TSET */
	pcmds->cmds[1].msg.tx_buf[8] = vdd->br.elvss_value1;
	pcmds->cmds[1].msg.tx_buf[9] = vdd->br.elvss_value2;
	pcmds->cmds[1].msg.tx_buf[46] = vdd->temperature > 0 ?
			vdd->temperature : (char)(BIT(7) | (-1*vdd->temperature));

	pcmds->cmds[2].msg.tx_buf[1] = get_bit(vdd->br.cd_level, 8, 2);
	pcmds->cmds[2].msg.tx_buf[2] = get_bit(vdd->br.cd_level, 0, 8);

	*level_key = LEVEL1_KEY;

	return pcmds;
}

static int ss_manufacture_date_read(struct samsung_display_driver_data *vdd)
{
	unsigned char date[8];
	int year, month, day;
	int hour, min;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (A1h 5~8th) for manufacture date */
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

static int ss_elvss_read(struct samsung_display_driver_data *vdd)
{
	char elvss_b7[2];

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (B7h 8th,9th) for elvss*/
	ss_panel_data_read(vdd, RX_ELVSS, elvss_b7, LEVEL1_KEY);

	vdd->br.elvss_value1 = elvss_b7[0]; /*0xB7 8th para OTP value*/
	vdd->br.elvss_value2 = elvss_b7[1]; /*0xB7 9th para OTP value */

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
		pcmds->cmds[0].msg.tx_buf[1] = 0X03;	/* ACL 15% */
	else
		pcmds->cmds[0].msg.tx_buf[1] = 0X01;	/* ACL 8% */

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
		{ALPM_CTRL_REG, -EINVAL} };

	LCD_INFO("%s++\n", __func__);

	cmd_list[0] = ss_get_cmds(vdd, TX_LPM_BL_CMD);
	cmd_list[1] = ss_get_cmds(vdd, TX_LPM_BL_CMD);
	if (SS_IS_CMDS_NULL(cmd_list[0]) || SS_IS_CMDS_NULL(cmd_list[1])) {
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

	alpm_ctrl[HLPM_CTRL_2NIT] = ss_get_cmds(vdd, TX_HLPM_2NIT_CMD);
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
	if (unlikely((reg_list[0][1] == -EINVAL) ||
				(reg_list[1][1] == -EINVAL)))
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

	LCD_DEBUG("%s--\n", __func__);
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
		{ALPM_CTRL_REG, -EINVAL} };

	static int off_reg_list[1][2] = {
		{ALPM_CTRL_REG, -EINVAL} };

	LCD_ERR("%s++\n", __func__);

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
	alpm_brightness[LPM_2NIT_IDX] = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
	alpm_brightness[LPM_10NIT_IDX] = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
	alpm_brightness[LPM_30NIT_IDX] = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
	alpm_brightness[LPM_60NIT_IDX] = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
	if (SS_IS_CMDS_NULL(alpm_brightness[LPM_2NIT_IDX]) || SS_IS_CMDS_NULL(alpm_brightness[LPM_10NIT_IDX]) ||
		SS_IS_CMDS_NULL(alpm_brightness[LPM_30NIT_IDX]) || SS_IS_CMDS_NULL(alpm_brightness[LPM_60NIT_IDX])) {
		LCD_ERR("No cmds for alpm_brightness..\n");
		return;
	}

	alpm_ctrl[HLPM_CTRL_2NIT] = ss_get_cmds(vdd, TX_HLPM_2NIT_CMD);
	alpm_ctrl[HLPM_CTRL_10NIT] = ss_get_cmds(vdd, TX_HLPM_10NIT_CMD);
	alpm_ctrl[HLPM_CTRL_30NIT] = ss_get_cmds(vdd, TX_HLPM_30NIT_CMD);
	alpm_ctrl[HLPM_CTRL_60NIT] = ss_get_cmds(vdd, TX_HLPM_60NIT_CMD);
	if (SS_IS_CMDS_NULL(alpm_ctrl[HLPM_CTRL_2NIT]) || SS_IS_CMDS_NULL(alpm_ctrl[HLPM_CTRL_10NIT]) ||
		SS_IS_CMDS_NULL(alpm_ctrl[HLPM_CTRL_30NIT]) || SS_IS_CMDS_NULL(alpm_ctrl[HLPM_CTRL_60NIT])) {
		LCD_ERR("No cmds for hlpm_brightness..\n");
		return;
	}

/*	alpm_off_ctrl[HLPM_MODE_ON] = ss_get_cmds(vdd, TX_HLPM_OFF);
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
	if (unlikely((reg_list[0][1] == -EINVAL) ||
				(reg_list[1][1] == -EINVAL)))
		ss_find_reg_offset(reg_list, cmd_list, ARRAY_SIZE(cmd_list));

	if (unlikely(off_reg_list[0][1] == -EINVAL))
		ss_find_reg_offset(off_reg_list, off_cmd_list,
						ARRAY_SIZE(off_cmd_list));

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
		/* Update parameter for ALPM_CTRL_REG 0xB9*/
		memcpy(cmd_list[1]->cmds[reg_list[1][1]].msg.tx_buf,
				alpm_ctrl[ctrl_index]->cmds[0].msg.tx_buf,
				sizeof(char) * cmd_list[1]->cmds[reg_list[1][1]].msg.tx_len);

		LCD_INFO("[Panel LPM] update alpm ctrl reg\n");
	}

	if ((off_reg_list[0][1] != -EINVAL) &&\
			(mode != LPM_MODE_OFF)) {
		/* Update parameter for ALPM_CTRL_REG */
		memcpy(off_cmd_list[0]->cmds[off_reg_list[0][1]].msg.tx_buf,
				alpm_off_ctrl[mode]->cmds[0].msg.tx_buf,
				sizeof(char) * off_cmd_list[0]->cmds[off_reg_list[0][1]].msg.tx_len);
	}

	LCD_ERR("%s--\n", __func__);
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

static void samsung_panel_init(struct samsung_display_driver_data *vdd)
{
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
	vdd->panel_func.samsung_ddi_id_read = NULL;
	vdd->panel_func.samsung_smart_dimming_init = NULL;
	vdd->panel_func.samsung_cell_id_read = ss_cell_id_read;
	vdd->panel_func.samsung_octa_id_read = ss_octa_id_read;
	vdd->panel_func.samsung_elvss_read = ss_elvss_read;
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

	/* HMT */
	vdd->panel_func.samsung_brightness_gamma_hmt = NULL;
	vdd->panel_func.samsung_brightness_aid_hmt = NULL;
	vdd->panel_func.samsung_brightness_elvss_hmt = NULL;
	vdd->panel_func.samsung_brightness_vint_hmt = NULL;
	vdd->panel_func.samsung_smart_dimming_hmt_init = NULL;
	vdd->panel_func.samsung_smart_get_conf_hmt = NULL;
	vdd->panel_func.samsung_brightness_hmt = NULL;

	/* Panel LPM */
	vdd->panel_func.samsung_update_lpm_ctrl_cmd = ss_update_panel_lpm_ctrl_cmd;
	vdd->panel_func.samsung_set_lpm_brightness = ss_set_panel_lpm_brightness;

	/* default brightness */
	vdd->br.bl_level = 255;

	/* mdnie */
	vdd->mdnie.support_mdnie = false;

	/* send recovery pck before sending image date (for ESD recovery) */
	vdd->esd_recovery.send_esd_recovery = false;

	/* Enable panic on first pingpong timeout */
//	vdd->debug_data->panic_on_pptimeout = true;

	/* COLOR WEAKNESS */
	vdd->panel_func.color_weakness_ccb_on_off =  NULL;

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = NULL;

	/* COVER Open/Close */
	vdd->panel_func.samsung_cover_control = NULL;

	/* COPR */
	vdd->copr.panel_init = NULL;

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
	vdd->self_disp.is_support = false;
	vdd->self_disp.factory_support = true;
	//vdd->self_disp.init = self_display_init_FA7;
	//vdd->self_disp.data_init = NULL;

	/* SAMSUNG_FINGERPRINT */
	vdd->panel_hbm_entry_delay = 2;

}

static int __init samsung_panel_initialize(void)
{
	struct samsung_display_driver_data *vdd;
	enum ss_display_ndx ndx;
	char panel_string[] = "ss_dsi_panel_ANA6705_AMS667UK01_FHD";
	char panel_name[MAX_CMDLINE_PARAM_LEN];
	char panel_secondary_name[MAX_CMDLINE_PARAM_LEN];

	ss_get_primary_panel_name_cmdline(panel_name);
	ss_get_secondary_panel_name_cmdline(panel_secondary_name);

	/* TODO: use component_bind with panel_func
	 * and match by panel_string, instead.
	 */
	if (!strncmp(panel_string, panel_name, strlen(panel_string)))
		ndx = PRIMARY_DISPLAY_NDX;
	else if (!strncmp(panel_string, panel_secondary_name,
				strlen(panel_string)))
		ndx = SECONDARY_DISPLAY_NDX;
	else {
		LCD_ERR("can not find panel_name (%s) / (%s)\n", panel_string, panel_name);
		return 0;
	}

	vdd = ss_get_vdd(ndx);
	vdd->panel_func.samsung_panel_init = samsung_panel_init;

	LCD_INFO("%s done.. \n", panel_name);

	return 0;
}

early_initcall(samsung_panel_initialize);
