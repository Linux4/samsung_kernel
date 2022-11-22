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
Copyright (C) 2012, Samsung Electronics. All rights reserved.

*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
*/
#include "ss_dsi_panel_HX8279_TV101WUM.h"

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return false;
	}

	pr_info("%s\n", __func__);

	ss_panel_attach_set(vdd, true);
	return true;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return false;
	}

	pr_info("%s\n", __func__);

	return true;
}

static int samsung_panel_off_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return false;
	}

	pr_info("%s\n", __func__);

	return true;
}

static int samsung_panel_off_post(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return false;
	}

	pr_info("%s\n", __func__);

	return true;
}

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return false;
	}

	if (vdd->manufacture_id_dsi == PBA_ID)
		ss_panel_attach_set(vdd, false);
	else
		ss_panel_attach_set(vdd, true);

	switch (ss_panel_rev_get(vdd)) {

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

static struct dsi_panel_cmd_set * samsung_brightness_tft_pwm(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pwm_cmd = ss_get_cmds(vdd, TX_TFT_PWM);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return NULL;
	}

	pwm_cmd->cmds[1].msg.tx_buf[1] = vdd->br.cd_level;

	*level_key = LEVEL_KEY_NONE;

	return pwm_cmd;
}

static void samsung_panel_init(struct samsung_display_driver_data *vdd)
{
	LCD_ERR("%s\n", ss_get_panel_name(vdd));

	/* Default Panel Power Status is OFF */
	vdd->panel_state = PANEL_PWR_OFF;

	vdd->support_cabc = false;

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = samsung_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = samsung_panel_on_post;
	vdd->panel_func.samsung_panel_off_pre = samsung_panel_off_pre;
	vdd->panel_func.samsung_panel_off_post = samsung_panel_off_post;
	vdd->panel_func.samsung_backlight_late_on = NULL;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = ss_panel_revision;
	vdd->panel_func.samsung_manufacture_date_read = NULL;
	vdd->panel_func.samsung_ddi_id_read = NULL;
	vdd->panel_func.samsung_hbm_read = NULL;
	vdd->panel_func.samsung_mdnie_read = NULL;
	vdd->panel_func.samsung_smart_dimming_init = NULL;

	/* Brightness */
	vdd->panel_func.samsung_brightness_tft_pwm_ldi = samsung_brightness_tft_pwm;
	vdd->panel_func.samsung_brightness_hbm_off = NULL;
	vdd->panel_func.samsung_brightness_aid = NULL;
	vdd->panel_func.samsung_brightness_acl_on = NULL;
	vdd->panel_func.samsung_brightness_acl_percent = NULL;
	vdd->panel_func.samsung_brightness_acl_off = NULL;
	vdd->panel_func.samsung_brightness_elvss = NULL;
	vdd->panel_func.samsung_brightness_elvss_temperature1 = NULL;
	vdd->panel_func.samsung_brightness_elvss_temperature2 = NULL;
	vdd->panel_func.samsung_brightness_vint = NULL;
	vdd->panel_func.samsung_brightness_gamma = NULL;
	//vdd->brightness[0].brightness_packet_tx_cmds_dsi.link_state = DSI_HS_MODE;

	vdd->panel_func.samsung_bl_ic_en = isl98608_panel_power;

	/* default brightness */
	vdd->br.bl_level = LCD_DEFAUL_BL_LEVEL;

	ss_panel_attach_set(vdd, true);
}

static int __init samsung_panel_initialize(void)
{
	struct samsung_display_driver_data *vdd;
	enum ss_display_ndx ndx = PRIMARY_DISPLAY_NDX;
	char panel_string[] = "ss_dsi_panel_HX8279_TV101WUM_WUXGA";
	char panel_name[MAX_CMDLINE_PARAM_LEN];
	char panel_secondary_name[MAX_CMDLINE_PARAM_LEN];

	LCD_INFO("++++++++++ \n");

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
	else{
		LCD_ERR("string (%s) is not same with given panel_name (%s, %s)\n", panel_string, panel_name, panel_secondary_name);
		return 0;
	}

	vdd = ss_get_vdd(ndx);
	vdd->panel_func.samsung_panel_init = samsung_panel_init;

	LCD_INFO("%s done.. \n", panel_string);
	LCD_INFO("---------- \n");

	return 0;
}
early_initcall(samsung_panel_initialize);
