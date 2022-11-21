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
#include "ss_dsi_mdnie_NT36672C_PM6585JB3_M23.h"
#include "ss_dsi_panel_NT36672C_PM6585JB3_M23.h"
//#include "ss_dsi_reading_mode_NT36672C_PM6585JB3_M23.h"

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return false;
	}

	LCD_INFO(vdd, "+++\n");

	ss_panel_attach_set(vdd, true);

	LCD_INFO(vdd, "---\n");

	return true;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return -EINVAL;
	}

	LCD_INFO(vdd, "+++\n");

	return 0;
}

static int samsung_panel_off_pre(struct samsung_display_driver_data *vdd)
{
	return 0;
}

static int samsung_panel_off_post(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return -EINVAL;
	}

	LCD_INFO(vdd, "---\n");

	return 0;
}

#if 0
static void backlight_tft_late_on(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return;
	}

	/* DDI spec.: control brightness PWM (51h) 40ms delay after
	 * display_on(29h) be sent. 40ms delay included in display on command.
	 */
	LCD_INFO(vdd, "re-conf bl_level=%d\n", vdd->br.bl_level);
	ss_brightness_dcs(vdd, USE_CURRENT_BL_LEVEL, BACKLIGHT_NORMAL);
}
#endif

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid vdd\n", __func__);
		return false;
	}

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

static struct dsi_panel_cmd_set *tft_pwm(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *set = ss_get_cmds(vdd, TX_BRIGHT_CTRL);
	int pwm = vdd->br_info.common_br.cd_level;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(set)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	/* 12 bits PWM: 000h = off, 001h = 2/4096, FFFh = 4096/4096
	 * 8 bits PWM: 00=off, 01=2/256, FF=256/256
	 * config_screenBrightnessSettingMaximum : bl_level = 255 : (pwm + 1)
	 */
	set->cmds->ss_txbuf[1] = pwm & 0xFF;

	LCD_INFO(vdd, "bl: %d, cd: %d, pwm: 0x%x, cmd: 0x%x\n",
			vdd->br_info.common_br.bl_level,
			vdd->br_info.common_br.cd_level,
			pwm, set->cmds->ss_txbuf[1]);

	*level_key = LEVEL_KEY_NONE;

	return set;
}

static void make_brightness_packet(struct samsung_display_driver_data *vdd,
	struct dsi_cmd_desc *packet, int *cmd_cnt, enum BR_TYPE br_type)
{
	if (br_type == BR_TYPE_NORMAL) { /* TFT:  ~255, & 256 here*/
		if (vdd->dtsi_data.tft_common_support) { /* TFT PANEL */

			/* TFM_PWM */
			ss_add_brightness_packet(vdd, BR_FUNC_TFT_PWM, packet, cmd_cnt);

		}
	} else {
		LCD_ERR(vdd, "undefined br_type (%d) \n", br_type);
	}

	return;
}


#if 0
static int dsi_update_mdnie_data(struct samsung_display_driver_data *vdd)
{
	struct mdnie_lite_tune_data *mdnie_data;

	mdnie_data = kzalloc(sizeof(struct mdnie_lite_tune_data), GFP_KERNEL);
	if (!mdnie_data) {
		LCD_ERR(vdd, "fail to allocate mdnie_data memory\n");
		return -ENOMEM;
	}
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
#endif

void NT36672C_PM6585JB3_M23_FHD_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "%s\n", ss_get_panel_name(vdd));

	/* Default Panel Power Status is OFF */
	vdd->panel_state = PANEL_PWR_OFF;

	vdd->support_cabc = false;

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
	vdd->panel_func.br_func[BR_FUNC_TFT_PWM] = tft_pwm;
	/* Make brightness packet */
	vdd->panel_func.make_brightness_packet = make_brightness_packet;

	/* default brightness */
	vdd->br_info.common_br.bl_level = LCD_DEFAUL_BL_LEVEL;

	/* MDNIE */
	vdd->mdnie.support_mdnie = false;
	vdd->mdnie.support_trans_dimming = false;
	//dsi_update_mdnie_data(vdd);

#if 0
	/* Reading mode */
	vdd->reading_mode.support_reading_mode = true;
	vdd->reading_mode.cur_level = READING_MODE_MAX_LEVEL_NT36672C_PM6585JB3_M23;
	vdd->reading_mode.max_level = READING_MODE_MAX_LEVEL_NT36672C_PM6585JB3_M23;
	vdd->reading_mode.dsi_tune_size = READING_MODE_TUNE_SIZE_NT36672C_PM6585JB3_M23;
	vdd->reading_mode.reading_mode_tune_dsi = reading_mode_tune_value_dsi;
#endif

	/* Enable panic on first pingpong timeout */
	//vdd->debug_data->panic_on_pptimeout = true;

	//ss_panel_attach_set(vdd, true); // ksj..
}
