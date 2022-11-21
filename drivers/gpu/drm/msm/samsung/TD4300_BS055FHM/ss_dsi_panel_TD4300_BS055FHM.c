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
#include "ss_dsi_mdnie_TD4300_BS055FHM.h"
#include "ss_dsi_panel_TD4300_BS055FHM.h"

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return false;
	}

#if defined(CONFIG_SEC_INCELL)
	if (incell_data.tsp_enable)
		incell_data.tsp_enable(NULL);
#endif
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
	if(ss_panel_attach_get(vdd))
		/* Disable PWM_EN */
		isl98611_pwm_en(false);
	pr_info("%s\n", __func__);


	return true;
}

static int samsung_panel_off_post(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return false;
	}

#if defined(CONFIG_SEC_INCELL)
	if (incell_data.tsp_disable)
		incell_data.tsp_disable(NULL);
#endif
	pr_info("%s\n", __func__);

	return true;
}

static void backlight_tft_late_on(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return;
	}
	if(ss_panel_attach_get(vdd))
		isl98611_pwm_en(true);
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

	pwm_cmd->cmds[0].msg.tx_buf[1] = vdd->br.cd_level;
	*level_key = LEVEL1_KEY;

	return pwm_cmd;
}

#if 0
static void samsung_panel_tft_outdoormode_update(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return;
	}
	pr_info("%s: tft panel autobrightness update\n", __func__);

}
#endif

static int dsi_update_mdnie_data(struct samsung_display_driver_data *vdd)
{
	struct mdnie_lite_tune_data *mdnie_data;

	mdnie_data = kzalloc(sizeof(struct mdnie_lite_tune_data), GFP_KERNEL);
	if (!mdnie_data) {
		LCD_ERR("fail to allocate mdnie_data memory\n");
		return -ENOMEM;
	}
#if 0
	vdd->mdnie_tune_size1 = 47;
	vdd->mdnie_tune_size2 = 56;
	vdd->mdnie_tune_size3 = 2;
#endif
	pr_info("dsi_update_mdnie_data\n");

	/* Update mdnie command */

	mdnie_data->DSI_BYPASS_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_NEGATIVE_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_GRAYSCALE_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_GRAYSCALE_NEGATIVE_MDNIE = DSI0_UI_MDNIE;
//	mdnie_data->DSI_COLOR_BLIND_MDNIE_1 = DSI0_UI_MDNIE;
	mdnie_data->DSI_HBM_CE_MDNIE = DSI0_HBM_CE_MDNIE;
	mdnie_data->DSI_RGB_SENSOR_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_CURTAIN = NULL;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_UI_STANDARD_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_UI_NATURAL_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_UI_MOVIE_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_UI_AUTO_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_VIDEO_OUTDOOR_MDNIE = NULL;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE = DSI0_VIDEO_MDNIE;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE = DSI0_VIDEO_MDNIE;
	mdnie_data->DSI_VIDEO_NATURAL_MDNIE = DSI0_VIDEO_MDNIE;
	mdnie_data->DSI_VIDEO_MOVIE_MDNIE = DSI0_VIDEO_MDNIE;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE = DSI0_VIDEO_MDNIE;
	mdnie_data->DSI_VIDEO_WARM_OUTDOOR_MDNIE = NULL;
	mdnie_data->DSI_VIDEO_WARM_MDNIE = NULL;
	mdnie_data->DSI_VIDEO_COLD_OUTDOOR_MDNIE = NULL;
	mdnie_data->DSI_VIDEO_COLD_MDNIE = NULL;
	mdnie_data->DSI_CAMERA_OUTDOOR_MDNIE = NULL;
	mdnie_data->DSI_CAMERA_MDNIE = DSI0_CAMERA_MDNIE;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE = DSI0_CAMERA_MDNIE;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_GALLERY_NATURAL_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_GALLERY_MOVIE_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_VT_DYNAMIC_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_VT_STANDARD_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_VT_NATURAL_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_VT_MOVIE_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_VT_AUTO_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_BROWSER_NATURAL_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_BROWSER_MOVIE_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE = DSI0_EBOOK_MDNIE;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE = DSI0_EBOOK_MDNIE;
	mdnie_data->DSI_EBOOK_NATURAL_MDNIE = DSI0_EBOOK_MDNIE;
	mdnie_data->DSI_EBOOK_MOVIE_MDNIE = DSI0_EBOOK_MDNIE;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE = DSI0_EBOOK_MDNIE;
	mdnie_data->DSI_EMAIL_AUTO_MDNIE = DSI0_UI_MDNIE;
	mdnie_data->mdnie_tune_value_dsi = mdnie_tune_value_dsi0;
	mdnie_data->light_notification_tune_value_dsi = light_notification_tune_value;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data->dsi_bypass_mdnie_size = ARRAY_SIZE(DSI0_UI_MDNIE);
	mdnie_data->mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET;
	mdnie_data->mdnie_step_index[MDNIE_STEP1] = MDNIE_STEP1_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP2] = MDNIE_STEP2_INDEX;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_RED_OFFSET] = 0;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_GREEN_OFFSET] = 0;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_BLUE_OFFSET] = 0;
	mdnie_data->dsi_rgb_sensor_mdnie_1_size = 0;
	mdnie_data->dsi_rgb_sensor_mdnie_2_size = 0;
	mdnie_data->dsi_white_default_r = 0xff;
	mdnie_data->dsi_white_default_g = 0xff;
	mdnie_data->dsi_white_default_b = 0xff;
	mdnie_data->dsi_white_balanced_r = 0;
	mdnie_data->dsi_white_balanced_g = 0;
	mdnie_data->dsi_white_balanced_b = 0;

	vdd->mdnie.mdnie_data = mdnie_data;
	return 0;
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
	vdd->panel_func.samsung_backlight_late_on = backlight_tft_late_on;

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
	vdd->panel_func.samsung_brightness_acl_on = NULL;
	vdd->panel_func.samsung_brightness_acl_percent = NULL;
	vdd->panel_func.samsung_brightness_acl_off = NULL;
	vdd->panel_func.samsung_brightness_elvss = NULL;
	vdd->panel_func.samsung_brightness_elvss_temperature1 = NULL;
	vdd->panel_func.samsung_brightness_elvss_temperature2 = NULL;
	vdd->panel_func.samsung_brightness_vint = NULL;
	vdd->panel_func.samsung_brightness_gamma = NULL;
//	vdd->brightness[0].brightness_packet_tx_cmds_dsi.link_state = DSI_HS_MODE;
//	vdd->mdss_panel_tft_outdoormode_update=ss_panel_tft_outdoormode_update;
	vdd->panel_func.samsung_bl_ic_en = isl98611_bl_ic_en;

	/* default brightness */
	vdd->bl_level = LCD_DEFAUL_BL_LEVEL;

	/* default brightness */
	vdd->br.bl_level = LCD_DEFAUL_BL_LEVEL;

	/* MDNIE */
	vdd->mdnie.support_mdnie = false;
	vdd->mdnie.support_trans_dimming = false;
	dsi_update_mdnie_data(vdd);

	/* send recovery pck before sending image date (for ESD recovery) */
	vdd->esd_recovery.send_esd_recovery = false;


	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = NULL;

	/* Enable panic on first pingpong timeout */
	if (vdd->debug_data)
		vdd->debug_data->panic_on_pptimeout = true;

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = NULL;

	ss_panel_attach_set(vdd, true);
}

static int __init samsung_panel_initialize(void)
{
	struct samsung_display_driver_data *vdd;
	enum ss_display_ndx ndx = PRIMARY_DISPLAY_NDX;
	char panel_string[] = "ss_dsi_panel_TD4300_BS055FHM_FHD";
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
		LCD_ERR("string (%s) can not find given panel_name (%s, %s)\n", panel_string, panel_name, panel_secondary_name);
		return 0;
	}

	vdd = ss_get_vdd(ndx);
	vdd->panel_func.samsung_panel_init = samsung_panel_init;

	LCD_INFO("%s done.. \n", panel_string);
	LCD_INFO("---------- \n");

	return 0;
}
early_initcall(samsung_panel_initialize);
