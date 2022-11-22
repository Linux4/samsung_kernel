/*
 * =================================================================
 *	Description:  samsung display panel file
 *	Company:  Samsung Electronics
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
*/
#include "ss_dsi_mdnie_NT36672C_TL066FVMC02.h"
#include "ss_dsi_panel_NT36672C_TL066FVMC02.h"

enum {
	BLIC_STATE_OFF = 0,
	BLIC_STATE_ON,
	BLIC_STATE_UNKNOWN,
};

static void ss_blic_ctrl(struct samsung_display_driver_data *vdd, bool on)
{
	/* W/A: PWM floating causes brightness on in LCD off case.
	 * Change brightness mode: pwm -> i2c mode.
	 * Default i2c mode brightness is zero.
	 */
	u8 on_data[][2] = {
		/* addr value */
		{0x0C, 0x24},
		{0x0D, 0x1E},
		{0x0E, 0x1E},
		{0x09, 0x99},
		{0x02, 0x6B},
		{0x03, 0x0D},
		{0x11, 0x74},
		{0x04, 0x03},
		{0x05, 0xC2},
		{0x10, 0x66},
		{0x08, 0x13},
	};
	int on_size = ARRAY_SIZE(on_data);

	u8 off_data[][2] = {
		/* addr value */
		{0x08, 0x00},
	};
	int off_size = ARRAY_SIZE(off_data);

	static int blic_state = BLIC_STATE_UNKNOWN;

	LCD_INFO(vdd, "+++ state: %s, on: %d, data size: %d\n",
			(blic_state == BLIC_STATE_OFF) ? "off" :
			(blic_state == BLIC_STATE_ON) ? "on" : "unknown",
			on, on ? on_size : off_size);

	if (on && blic_state != BLIC_STATE_ON) {
		ss_blic_ktz8864b_configure(on_data, on_size);
		blic_state = BLIC_STATE_ON;
	} else if (!on && blic_state != BLIC_STATE_OFF) {
		ss_blic_ktz8864b_configure(off_data, off_size);
		blic_state = BLIC_STATE_OFF;
	} else {
		LCD_INFO(vdd, "skip\n");
	}

	LCD_INFO(vdd, "--- state: %s, on: %d\n",
			(blic_state == BLIC_STATE_OFF) ? "off" :
			(blic_state == BLIC_STATE_ON) ? "on" : "unknown",
			on);
}

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd 0x%zx", __func__, (size_t)vdd);
		return -EINVAL;
	}

	LCD_INFO(vdd, "+++\n");
	ss_panel_attach_set(vdd, true);
	ss_blic_ctrl(vdd, true); /* turn on blic */
	LCD_INFO(vdd, "---\n");

	return 0;
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

	LCD_INFO(vdd, "+++\n");
	ss_blic_ctrl(vdd, false); /* turn off blic */
	LCD_INFO(vdd, "---\n");

	return 0;
}

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

void NT36672C_TL066FVMC02_FHD_init(struct samsung_display_driver_data *vdd)
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

	ss_blic_ktz8864b_init();
}
