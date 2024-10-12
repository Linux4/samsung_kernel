/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
 *
 *	Company:  Samsung Electronics Display Team
 *
 * ================================================================
 */

/*
 *Copyright (C) 2021, Samsung Electronics. All rights reserved.
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
#include "ss_dsi_panel_FT8203_TS124QDM.h"

int prev_bl_ft8203; /* To save previous brightness level */

static int ss_boost_control(struct samsung_display_driver_data *vdd)
{
	u8 read_data;
	int ret;

	/* For voltage set when control */
	ret = ss_boost_i2c_write_ex(0x04, 0x78);
	if (ret > -1) {
		ss_boost_i2c_read_ex(0x04, &read_data);
		if (read_data != 0x78)
			LCD_INFO(vdd, "0x%x not 0x78 fail\n", read_data);
		else
			LCD_INFO(vdd, "0x%x same as 0x78 success\n", read_data);
	} else {
		LCD_ERR(vdd, "boost write failed...%d\n", ret);
	}

	return 0;
}

static int ss_boost_en_control(struct samsung_display_driver_data *vdd, bool enable)
{
	int bl_level = vdd->br_info.common_br.bl_level;
	struct dsi_panel *panel =  GET_DSI_PANEL(vdd);

	dsi_panel_boost_regulator(panel, enable);

	LCD_INFO(vdd, "boost_en %s, bl_level:%d, prev bl:%d\n",
		enable ? "ON" : "OFF", bl_level, prev_bl_ft8203);

	/* Set this to recover boost_en if bl_level goes up before turned off */
	if (enable) {
		vdd->boost_off_while_working = false;
		ss_boost_control(vdd);
	} else {
		vdd->boost_off_while_working = true;
	}
	return 0;
}

static int ss_buck_isl98608_control_ex(struct samsung_display_driver_data *vdd)
{
	u8 buf[2] = {0, 0};
	u8 value = 0x04; //VN & VP 5.6V

	if (unlikely(vdd->is_factory_mode)) {
		if (ub_con_det_status(PRIMARY_DISPLAY_NDX)) {
			LCD_ERR(vdd, "Do not panel power on..\n");
			return 0;
		}
	}

	ss_buck_i2c_write_ex(0x06, 0x0D);
	ss_buck_i2c_write_ex(0x08, value);
	ss_buck_i2c_write_ex(0x09, value);

	ss_buck_i2c_read_ex(0x08, &buf[0]);
	ss_buck_i2c_read_ex(0x05, &buf[1]);

	LCD_INFO(vdd, "08:0x%x 05:0x%x\n", buf[0], buf[1]);

	//Write to prevent error.
	ss_buck_i2c_write_ex(0x05, 0x27);

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

	if (vdd->panel_func.samsung_buck_control)
		vdd->panel_func.samsung_buck_control(vdd);

	return true;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
	/* Boost for backlight ic's 5V input */
	if (vdd->panel_func.samsung_boost_control)
		vdd->panel_func.samsung_boost_control(vdd);

	if (vdd->panel_func.samsung_blic_control)
		vdd->panel_func.samsung_blic_control(vdd);

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
	LCD_INFO_ONCE(vdd, "panel_revision = %c %d\n", vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

/* Below value is config_display.xml data */
#define config_screenBrightnessSettingMaximum (255)
#define config_screenBrightnessSettingDefault (125) /* TFT default level */
#define config_screenBrightnessSettingMinimum (2)
#define config_screenBrightnessSettingzero (1) /*0~1: 00 00h*/

/* Below value is pwm value & candela matching data at the 9bit 20khz PWM*/
#define PWM_Outdoor (3976) //0x0F88 256~ 600CD
#define PWM_Maximum (3231) //0x0C9F 255 500CD
#define PWM_Default (1200) //0x4B0  190nit
#define PWM_Minimum (32) //0x28 //4CD
#define PWM_ZERO (0) //0CD

#define BIT_SHIFT 10
static struct dsi_panel_cmd_set *ss_tft_pwm(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pwm_cmds = ss_get_cmds(vdd, TX_TFT_PWM);
	int bl_level = vdd->br_info.common_br.bl_level;
	unsigned long long result;
	unsigned long long multiple;

	if (IS_ERR_OR_NULL(vdd) || (SS_IS_CMDS_NULL(pwm_cmds))) {
		LCD_ERR(vdd, "Invalid data, vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	if (bl_level > config_screenBrightnessSettingMaximum) {
		result = PWM_Outdoor;
	} else if ((bl_level > config_screenBrightnessSettingDefault)
			&& (bl_level <= config_screenBrightnessSettingMaximum)) {
		/*	(((3231 - 1200 ) / (255 - 125)) * (bl_level - 125)) + 1200	*/
		multiple = (PWM_Maximum - PWM_Default);
		multiple <<= BIT_SHIFT;
		do_div(multiple, config_screenBrightnessSettingMaximum - config_screenBrightnessSettingDefault);

		result = (bl_level - config_screenBrightnessSettingDefault) * multiple;
		result >>= BIT_SHIFT;
		result += PWM_Default;
	} else if ((bl_level > config_screenBrightnessSettingMinimum)
			&& (bl_level <= config_screenBrightnessSettingDefault)) {
		/*	(((1200 - 32) / (125 - 2)) * (bl_level - 2)) + 32	*/
		multiple = (PWM_Default - PWM_Minimum);
		multiple <<= BIT_SHIFT;
		do_div(multiple, config_screenBrightnessSettingDefault - config_screenBrightnessSettingMinimum);

		result = (bl_level - config_screenBrightnessSettingMinimum) * multiple;
		result >>= BIT_SHIFT;
		result += PWM_Minimum;
	} else if ((bl_level > config_screenBrightnessSettingzero)
			&& (bl_level <= config_screenBrightnessSettingMinimum)) {
		result = PWM_Minimum; /*platform level 2 : min level */
	} else
		result = PWM_ZERO;

	pwm_cmds->cmds->ss_txbuf[1] = (u8)(result >> 4);
	pwm_cmds->cmds->ss_txbuf[2] = (u8)(result & 0x000F);

	if (bl_level > 14)
		pwm_cmds->cmds[1].ss_txbuf[1] = 0x2C;	/* PWM dimming ON */
	else
		pwm_cmds->cmds[1].ss_txbuf[1] = 0x24;	/* PWM dimming OFF */

	LCD_INFO(vdd, "bl_level:%d, tx_buf:%x, %x, prev_bl:%d, pwm_dimming:%x\n",
		vdd->br_info.common_br.bl_level, pwm_cmds->cmds->ss_txbuf[1], pwm_cmds->cmds->ss_txbuf[2],
		prev_bl_ft8203, pwm_cmds->cmds[1].ss_txbuf[1]);

	/* Boost_en early off when pwm 0 bl_level
	 * to prevent reverse-voltage loading at boost IC output
	 */
	if (!bl_level && !vdd->display_status_dsi.wait_disp_on) {
		/* Connect only when previous level is bigger than 0 */
		if (prev_bl_ft8203) {
			LCD_INFO(vdd, "Connect function for boost_en early off.\n");
			vdd->panel_func.samsung_boost_en_control = ss_boost_en_control;
		} else {
			vdd->panel_func.samsung_boost_en_control = NULL;
		}
	}

	/* Boost_en recover if bl_level goes up with off state previously */
	if (bl_level && !prev_bl_ft8203 && vdd->boost_off_while_working) {
		if (vdd->panel_func.samsung_boost_en_control)
			vdd->panel_func.samsung_boost_en_control(vdd, true);

		LCD_INFO(vdd, "Recover boost_en ON state\n");
	}

	*level_key = LEVEL_KEY_NONE;
	pwm_cmds->state = DSI_CMD_SET_STATE_LP;
	prev_bl_ft8203 = bl_level;

	return pwm_cmds;
}

static struct dsi_panel_cmd_set *ss_tft_cabc(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int bl_level = vdd->br_info.common_br.bl_level;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL_KEY_NONE;

	pcmds = ss_get_cmds(vdd, TX_NORMAL_BRIGHTNESS_ETC);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_ACL_ON..\n");
		return NULL;
	}

	if (bl_level > 60) {
		pcmds->cmds[0].ss_txbuf[1] = 0x01;	/* CABC ON */
		LCD_INFO(vdd, "cabc enabled, bl_level[%d] > 60, count:%d\n", bl_level, pcmds->count);
	} else {
		pcmds->cmds[0].ss_txbuf[1] = 0x00;	/* CABC OFF */
		LCD_INFO(vdd, "cabc disabled, bl_level[%d] <= 60, count:%d\n", bl_level, pcmds->count);
	}

	return pcmds;
}

static void make_brightness_packet(struct samsung_display_driver_data *vdd,
	struct dsi_cmd_desc *packet, int *cmd_cnt, enum BR_TYPE br_type)
{
	if ((br_type == BR_TYPE_NORMAL)  || (br_type == BR_TYPE_HBM)) { /* TFT:  ~255, & 256 here*/
		if (vdd->dtsi_data.tft_common_support) { /* TFT PANEL */
			/* TFM_PWM */
			ss_add_brightness_packet(vdd, BR_FUNC_TFT_PWM, packet, cmd_cnt);

			/* PANEL SPECIFIC SETTINGS : CABC */
			ss_add_brightness_packet(vdd, BR_FUNC_ETC, packet, cmd_cnt);
		}
	} else {
		LCD_ERR(vdd, "undefined br_type (%d)\n", br_type);
	}

	return;
}

static int ss_manufacture_date_read(struct samsung_display_driver_data *vdd)
{
	unsigned char date[8];
	int year, month, day;
	int hour, min;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
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

		LCD_ERR(vdd, "manufacture_date DSI%d = (%d%04d) - year(%d) month(%d) day(%d) hour(%d) min(%d)\n",
			vdd->ndx, vdd->manufacture_date_dsi, vdd->manufacture_time_dsi,
			year, month, day, hour, min);

	} else {
		LCD_ERR(vdd, "DSI%d no manufacture_date_rx_cmds cmds(%d)", vdd->ndx, vdd->panel_revision);
		return false;
	}

	return true;
}

static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	char ddi_id[5];
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (D6h 1~5th) for CHIP ID */
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

static int ss_cell_id_read(struct samsung_display_driver_data *vdd)
{
	char cell_id_buffer[MAX_CELL_ID] = {0,};
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read Panel Unique Cell ID (C9h 19~34th) */
	if (ss_get_cmds(vdd, RX_CELL_ID)->count) {
		memset(cell_id_buffer, 0x00, MAX_CELL_ID);

		ss_panel_data_read(vdd, RX_CELL_ID, cell_id_buffer, LEVEL1_KEY);

		for (loop = 0; loop < MAX_CELL_ID; loop++)
			vdd->cell_id_dsi[loop] = cell_id_buffer[loop];

		LCD_INFO(vdd, "DSI%d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			vdd->ndx, vdd->cell_id_dsi[0],
			vdd->cell_id_dsi[1],	vdd->cell_id_dsi[2],
			vdd->cell_id_dsi[3],	vdd->cell_id_dsi[4],
			vdd->cell_id_dsi[5],	vdd->cell_id_dsi[6],
			vdd->cell_id_dsi[7],	vdd->cell_id_dsi[8],
			vdd->cell_id_dsi[9],	vdd->cell_id_dsi[10]);

	} else {
		LCD_ERR(vdd, "DSI%d no cell_id_rx_cmds cmd\n", vdd->ndx);
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

static int samsung_panel_off_pre(struct samsung_display_driver_data *vdd)
{
	int rc = 0;

	return rc;
}

static int samsung_panel_off_post(struct samsung_display_driver_data *vdd)
{
	int rc = 0;

	prev_bl_ft8203 = 0;
	vdd->boost_off_while_working = false;

	return rc;
}

void FT8203_TS124QDM_WQXGA_init(struct samsung_display_driver_data *vdd)
{
	struct dsi_display *display = NULL;
	struct dsi_display_ctrl *m_ctrl = NULL;
	int phy_ver;

	LCD_INFO(vdd, "FT8203_TS124QDM : ++\n");
	LCD_INFO(vdd, "%s\n", ss_get_panel_name(vdd));

	display = GET_DSI_DISPLAY(vdd);
	if (IS_ERR_OR_NULL(display)) {
		LCD_ERR(vdd, "no display");
		return;
	}
	m_ctrl = &display->ctrl[display->clk_master_idx];
	if (IS_ERR_OR_NULL(m_ctrl)) {
		LCD_ERR(vdd, "no m_ctrl");
		return;
	}

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
	//vdd->panel_func.samsung_mdnie_read = ss_mdnie_read;

	/* Brightness PWM */
	vdd->panel_func.br_func[BR_FUNC_TFT_PWM] = ss_tft_pwm;
	/* Brightness CABC */
	vdd->panel_func.br_func[BR_FUNC_ETC] = ss_tft_cabc;
	/* Make brightness packet */
	vdd->panel_func.make_brightness_packet = make_brightness_packet;
	/* BLIC, BUCK control */
	vdd->panel_func.samsung_buck_control = ss_buck_isl98608_control_ex;
	vdd->panel_func.samsung_blic_control = ss_blic_lp8558_control;
	vdd->panel_func.samsung_boost_control = ss_boost_control;

	/* default brightness */
	//vdd->br_info.common_br.bl_level = 25500;

	/* mdnie */
	vdd->mdnie.support_mdnie = false;

	/* Do not support single tx*/
	vdd->not_support_single_tx = true;

	/* Default br_info.temperature */
	vdd->br_info.temperature = 20;

	/* init motto values for MIPI drving strength */
	phy_ver = dsi_phy_get_version(m_ctrl->phy);
	if (phy_ver <= DSI_PHY_VERSION_3_0)
		vdd->motto_info.motto_swing = 0x1F;
	else
		vdd->motto_info.motto_swing = 0xFF;

	/* To make boost_en off right after pwm 0*/
	vdd->boost_early_off = true;
	vdd->panel_func.samsung_boost_en_control = ss_boost_en_control;

	vdd->debug_data->print_cmds = false;

	/* Call blic, buck, boost init */
	ss_blic_lp8558_init();
	ss_boost_max77816_init();
	ss_buck_isl98608_init();

	LCD_INFO(vdd, "FT8203_TS124QDM : --\n");
}
