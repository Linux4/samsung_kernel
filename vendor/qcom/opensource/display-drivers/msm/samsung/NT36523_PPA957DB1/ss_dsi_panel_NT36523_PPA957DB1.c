/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
 *
 *	Author: wu.deng
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
#include "ss_dsi_panel_NT36523_PPA957DB1.h"

static int ss_buck_isl98608_control_ex(struct samsung_display_driver_data *vdd)
{
	u8 buf[2] = {0, 0};
	u8 value = 0x02; //VN & VP 5.5V

	if (unlikely(vdd->is_factory_mode)) {
		if (ub_con_det_status(PRIMARY_DISPLAY_NDX)) {
			LCD_ERR(vdd, "Do not panel power on..\n");
			return 0;
		}
	}

	//ss_buck_i2c_write_ex(0x06, 0x0D);
	ss_buck_i2c_write_ex(0x08, value);
	ss_buck_i2c_write_ex(0x09, value);

	ss_buck_i2c_read_ex(0x08, &buf[0]);
	ss_buck_i2c_read_ex(0x09, &buf[1]);

	LCD_ERR(vdd, "08:0x%x 09:0x%x\n", buf[0], buf[1]);

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

	/* enable backlight_tft_gpio1 : LCD_SW_EN */
	ss_backlight_tft_gpio_config(vdd, 1);

	if (vdd->panel_func.samsung_buck_control)
		vdd->panel_func.samsung_buck_control(vdd);

	return true;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
	ss_send_cmd(vdd, TX_DFPS);

	return true;
}

static int samsung_panel_off_post(struct samsung_display_driver_data *vdd)
{
	/* disable backlight_tft_gpio1 : LCD_SW_EN */
	ss_backlight_tft_gpio_config(vdd, 0);

	return true;
}

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{
	if (vdd->manufacture_id_dsi == PBA_ID)
		ss_panel_attach_set(vdd, true);
	else
		ss_panel_attach_set(vdd, true);

	switch (ss_panel_rev_get(vdd)) {
	case 0x00:
		vdd->panel_revision = 'A';
		break;
	default:
		vdd->panel_revision = 'A';
		LCD_ERR(vdd, "Invalid panel_rev(default rev : %c)\n",
				vdd->panel_revision);
		break;
	}

	vdd->panel_revision -= 'A';
	LCD_INFO_ONCE(vdd, "panel_revision = %c %d \n", vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

/* Below value is config_display.xml data */
#define config_screenBrightnessSettingMaximum (255)
#define config_screenBrightnessSettingDefault (125)
#define config_screenBrightnessSettingMinimum (2)
#define config_screenBrightnessSettingzero (0)

/* Below value is pwm value & candela matching data at the 9bit 20khz PWM*/
#define PWM_Outdoor (462) //600CD
#define PWM_Maximum (382) //0x017E, 500CD
#define PWM_Default (133) //0x0085, 190CD
#define PWM_Minimum (3) //4CD
#define PWM_ZERO (0) //0CD

#define BIT_SHIFT 10

static struct dsi_panel_cmd_set *tft_pwm_ldi(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pwm_cmds = ss_get_cmds(vdd, TX_TFT_PWM);

	int bl_level = vdd->br_info.common_br.bl_level;
	unsigned long long result;
	unsigned long long multiple;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(pwm_cmds)) {
		LCD_ERR(vdd, "Invalid vdd or invalid cmd\n");
		return NULL;
	}

	/* 9bit PWM */
	if (bl_level > config_screenBrightnessSettingMaximum) {
		result = PWM_Outdoor;
	} else if ((bl_level > config_screenBrightnessSettingDefault) && (bl_level <= config_screenBrightnessSettingMaximum)) {
		/*
			(((382 - 139 ) / (255 - 125)) * (bl_level - 125)) + 139
		*/
		multiple = (PWM_Maximum - PWM_Default);
		multiple <<= BIT_SHIFT;
		do_div(multiple, config_screenBrightnessSettingMaximum - config_screenBrightnessSettingDefault);

		result = (bl_level - config_screenBrightnessSettingDefault) * multiple;
		result >>= BIT_SHIFT;
		result += PWM_Default;
	} else if ((bl_level > config_screenBrightnessSettingMinimum) && (bl_level <= config_screenBrightnessSettingDefault)) {
		/*
			(139/125) * bl_level
		*/
		result = bl_level * PWM_Default;
		result <<=  BIT_SHIFT;
		do_div(result, config_screenBrightnessSettingDefault);
		result >>= BIT_SHIFT;
	} else if ((bl_level > config_screenBrightnessSettingzero) && (bl_level <= config_screenBrightnessSettingMinimum)){
		result = PWM_Minimum;
	} else
		result = PWM_ZERO;

	pwm_cmds->cmds->ss_txbuf[1] = (u8)(result >> 8);
	pwm_cmds->cmds->ss_txbuf[2] = (u8)(result & 0xFF);

	LCD_ERR(vdd, "bl_level : %d %lld", vdd->br_info.common_br.bl_level, result);

	*level_key = LEVEL_KEY_NONE;

	return pwm_cmds;
}

static void make_brightness_packet(struct samsung_display_driver_data *vdd,
	struct dsi_cmd_desc *packet, int *cmd_cnt, enum BR_TYPE br_type)
{
	if ((br_type == BR_TYPE_NORMAL)  || (br_type == BR_TYPE_HBM)) { /* TFT:  ~255, & 256 here*/
		if (vdd->dtsi_data.tft_common_support) { /* TFT PANEL */
			/* TFM_PWM */
			ss_add_brightness_packet(vdd, BR_FUNC_TFT_PWM, packet, cmd_cnt);
		}
	} else {
		LCD_ERR(vdd, "undefined br_type (%d) \n", br_type);
	}

	return;
}

static void dfps_update(struct samsung_display_driver_data *vdd, int fps)
{
	struct dsi_panel_cmd_set *dpfs = ss_get_cmds(vdd, TX_DFPS);
	u8 data = 0;

	if (fps == 120)
		data = 0x0D;
	else if (fps == 96)
		data = 0x0E;
	else if (fps == 60)
		data = 0x0C;
	else
		data = 0x0F;

	dpfs->cmds[2].ss_txbuf[1] = data;
	ss_send_cmd(vdd, TX_DFPS);

	LCD_INFO(vdd, "fps : %d data : 0x%x\n", fps, data);
}

void NT36523_PPA957DB1_WQXGA_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "++\n");
	LCD_ERR(vdd, "%s\n", ss_get_panel_name(vdd));

	/* Default Panel Power Status is OFF */
	vdd->panel_state = PANEL_PWR_OFF;

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = samsung_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = samsung_panel_on_post;
	vdd->panel_func.samsung_panel_off_post = samsung_panel_off_post;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = ss_panel_revision;
	vdd->panel_func.samsung_manufacture_date_read = NULL;

	/* default brightness */
	vdd->br_info.common_br.bl_level = 255;//pac?

	/* mdnie */
	vdd->mdnie.support_mdnie = false;

	/* Brightness PWM */
	vdd->panel_func.br_func[BR_FUNC_TFT_PWM] = tft_pwm_ldi;
	/* Make brightness packet */
	vdd->panel_func.make_brightness_packet = make_brightness_packet;
	vdd->panel_func.samsung_dfps_panel_update = dfps_update;

	/* BLIC, BUCK control */
	vdd->panel_func.samsung_buck_control = ss_buck_isl98608_control_ex;

	/* send recovery pck before sending image date (for ESD recovery) */
	//vdd->esd_recovery.send_esd_recovery = false;
	//vdd->br_info.common_br.auto_level = 12;

	/* Enable panic on first pingpong timeout */
	if (vdd->debug_data)
		vdd->debug_data->panic_on_pptimeout = true;

	ss_buck_isl98608_init();

	//if (vdd->debug_data)
	//	vdd->debug_data->print_cmds = true;

	LCD_INFO(vdd, "--\n");
}



