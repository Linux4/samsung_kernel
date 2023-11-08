/* Copyright (c) 2022, Samsung Electronics Corporation. All rights reserved.
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

#include "GTACT4PRO_HX8279_TV101WUM_panel.h"

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return -EINVAL;
	}

	ss_panel_attach_set(vdd, true);

	return 0;
}

static int ss_buck_isl98608_control_ex(struct samsung_display_driver_data *vdd)
{
	u8 buf[2] = {0, 0};
	u8 value = 0x08; // VN & VP 5.8V

	if (unlikely(vdd->is_factory_mode)) {
		if (ub_con_det_status(PRIMARY_DISPLAY_NDX)) {
			LCD_ERR(vdd, "Do not panel power on..\n");
			return 0;
		}
	}

	ss_buck_i2c_write_ex(0x06, 0x08);
	ss_buck_i2c_write_ex(0x08, value);
	ss_buck_i2c_write_ex(0x09, value);

	ss_buck_i2c_read_ex(0x08, &buf[0]);
	ss_buck_i2c_read_ex(0x09, &buf[1]);

	if ((value != buf[0]) || (value != buf[1]))
		LCD_INFO(vdd, "0x%x 0x%x fail\n", buf[0], buf[1]);
	else
		LCD_INFO(vdd, "0x%x 0x%x success\n", buf[0], buf[1]);

	return 0;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return -EINVAL;
	}

	if (vdd->panel_func.samsung_buck_control)
		vdd->panel_func.samsung_buck_control(vdd);

	return 0;
}

static int samsung_panel_off_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return -EINVAL;
	}

	return true;
}

static int samsung_panel_off_post(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return -EINVAL;
	}

	return true;
}

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{

	if (IS_ERR_OR_NULL(vdd)) {
	        LCD_ERR(vdd, ": Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return false;
	}

	if (vdd->manufacture_id_dsi == PBA_ID)
		ss_panel_attach_set(vdd, false);
	else
		ss_panel_attach_set(vdd, true);

	switch (ss_panel_rev_get(vdd)) {
	default:
		vdd->panel_revision = 'A';
		LCD_ERR(vdd, "Invalid panel_rev(default rev : %c)\n", vdd->panel_revision);
		break;
	}

	vdd->panel_revision -= 'A';
	LCD_INFO_ONCE(vdd, "panel_revision = %c %d\n", vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

static struct dsi_panel_cmd_set *tft_pwm(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pwm_cmd = ss_get_cmds(vdd, TX_TFT_PWM);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(pwm_cmd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return NULL;
	}

	pwm_cmd->cmds[1].ss_txbuf[1] = vdd->br_info.common_br.cd_level;

	LCD_INFO(vdd, "cd_level : %d, tx_buf %x\n", vdd->br_info.common_br.cd_level, pwm_cmd->cmds[1].ss_txbuf[1]);

	*level_key = LEVEL_KEY_NONE;
	return pwm_cmd;
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

void GTACT4PRO_HX8279_TV101WUM_WUXGA_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "++\n");
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
	vdd->panel_func.samsung_manufacture_date_read = NULL;
	vdd->panel_func.samsung_ddi_id_read = NULL;
	vdd->panel_func.samsung_mdnie_read = NULL;

	/* Brightness */
	//vdd->panel_func.samsung_brightness_tft_pwm_ldi = samsung_brightness_tft_pwm;

	/* Brightness PWM */
	vdd->panel_func.br_func[BR_FUNC_TFT_PWM] = tft_pwm;
	//vdd->panel_func.samsung_bl_ic_en = isl98608_panel_power;

	/* Make brightness packet */
	vdd->panel_func.make_brightness_packet = make_brightness_packet;

	/* BLIC, BUCK control */
	vdd->panel_func.samsung_buck_control = ss_buck_isl98608_control_ex;

	/* Default br_info.temperature */
	vdd->br_info.temperature = 20;

	/* init motto values for MIPI drving strength */
	vdd->motto_info.motto_swing = 0xFF;

	vdd->debug_data->print_cmds = false;

	/* Call Buck init */
	ss_buck_isl98608_init();

	LCD_INFO(vdd, "--\n");
}
