/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2022, Samsung Electronics. All rights reserved.

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
#include "DM3_S6E3HAE_AMB681AZ01_panel.h"
#include "DM3_S6E3HAE_AMB681AZ01_mdnie.h"
#include "hwparam/DM3_S6E3HAE_AMB681AZ01.hw_param.h"

#define MAX_READ_BUF_SIZE	(20)
static u8 read_buf[MAX_READ_BUF_SIZE];

/* mtp original data R type */
static u8 HS120_R_TYPE_BUF[GAMMA_SET_MAX][GAMMA_R_SIZE];

/* mtp original data V type */
static int HS120_V_TYPE_BUF[GAMMA_SET_MAX][GAMMA_V_SIZE];

/* compensated data R type*/
static u8 HS120_R_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_R_SIZE];

/* compensated data V type*/
static int HS120_V_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_V_SIZE];

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);

	ss_panel_attach_set(vdd, true);

	return 0;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
	int ret = 0;
	/*
	 * self mask is enabled from bootloader.
	 * so skip self mask setting during splash booting.
	 */
	if (!vdd->samsung_splash_enabled) {
		if (vdd->self_disp.self_mask_img_write)
			vdd->self_disp.self_mask_img_write(vdd);
	} else {
		LCD_INFO(vdd, "samsung splash enabled.. skip image write\n");
	}

	/* self mask checksum */
	if (vdd->self_disp.self_display_debug)
		ret = vdd->self_disp.self_display_debug(vdd);

	/* self mask is turned on only when data checksum matches. */
	if (vdd->self_disp.self_mask_on && !ret)
		vdd->self_disp.self_mask_on(vdd, true);

	/* mafpc */
	if (vdd->mafpc.is_support) {
		vdd->mafpc.need_to_write = true;
		LCD_INFO(vdd, "Need to write mafpc image data to DDI\n");
	}

	return 0;
}

static int samsung_display_on_post(struct samsung_display_driver_data *vdd)
{
	/* HAE ddi (B0) only issue */
	/* P220307-02604, P211207-05270 : Do not write AOD 60nit before AOD display on + 1vsync (34ms) */
	if (vdd->panel_lpm.need_br_update) {
		vdd->panel_lpm.need_br_update = false;
		ss_brightness_dcs(vdd, USE_CURRENT_BL_LEVEL, BACKLIGHT_NORMAL);
	}

	return 0;
}

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{
	if (vdd->manufacture_id_dsi == PBA_ID)
		ss_panel_attach_set(vdd, false);
	else
		ss_panel_attach_set(vdd, true);

	switch (ss_panel_rev_get(vdd)) {
	case 0x00:
	case 0x01:
	case 0x02:
		vdd->panel_revision = 'A';
		break;
	case 0x03:
	case 0x04:
		vdd->panel_revision = 'B';
		break;
	case 0x05:
		vdd->panel_revision = 'C';
		break;
	default:
		vdd->panel_revision = 'C';
		LCD_ERR(vdd, "Invalid panel_rev(default rev : %c)\n", vdd->panel_revision);
		break;
	}

	vdd->panel_revision -= 'A';
	LCD_INFO_ONCE(vdd, "panel_revision = %c %d \n", vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

enum VRR_CMD_RR {
	/* 1Hz is PSR mode in LPM (AOD) mode, 10Hz is PSR mode in 120HS mode */
	VRR_10HS = 0,
	VRR_24HS,
	VRR_30HS,
	VRR_48NS,
	VRR_60NS,
	VRR_48HS,
	VRR_60HS,
	VRR_96HS,
	VRR_120HS,
	VRR_MAX
};

static enum VRR_CMD_RR ss_get_vrr_id(struct samsung_display_driver_data *vdd)
{
	enum VRR_CMD_RR vrr_id;
	int cur_rr = vdd->vrr.cur_refresh_rate;
	int cur_hs = vdd->vrr.cur_sot_hs_mode;

	switch (cur_rr) {
	case 10:
		vrr_id = VRR_10HS;
		break;
	case 24:
		vrr_id = VRR_24HS;
		break;
	case 30:
		vrr_id = VRR_30HS;
		break;
	case 48:
		vrr_id = (cur_hs) ? VRR_48HS : VRR_48NS ;
		break;
	case 60:
		vrr_id = (cur_hs) ? VRR_60HS : VRR_60NS ;
		break;
	case 96:
		vrr_id = VRR_96HS;
		break;
	case 120:
		vrr_id = VRR_120HS;
		break;
	default:
		LCD_ERR(vdd, "invalid refresh rate (%d, %d), set default 120HS..\n", cur_rr, cur_hs);
		vrr_id = VRR_120HS;
		break;
	}

	return vrr_id;
}

static int ss_update_base_lfd_val(struct vrr_info *vrr,
			enum LFD_SCOPE_ID scope, struct lfd_base_str *lfd_base)
{
	u32 base_rr, max_div_def, min_div_def, min_div_lowest;
	enum VRR_CMD_RR vrr_id;
	struct samsung_display_driver_data *vdd =
		container_of(vrr, struct samsung_display_driver_data, vrr);
	struct lfd_mngr *mngr;

	if (scope == LFD_SCOPE_LPM) {
		base_rr = 30;
		max_div_def = 1;	/* default 1HZ */
		min_div_def = 30;
		min_div_lowest = 30;
		goto done;
	}

	mngr = &vrr->lfd.lfd_mngr[LFD_CLIENT_DISP];

	vrr_id = ss_get_vrr_id(vdd);

	switch (vrr_id) {
	case VRR_10HS:
		base_rr = 120;
		max_div_def = 12; /* 10hz */
		min_div_def = min_div_lowest = 120; /* 1hz */
		break;
	case VRR_24HS:
		base_rr = 120;
		max_div_def = 5; /* 24hz */
		min_div_def = min_div_lowest = 120; /* 1hz */
		break;
	case VRR_30HS:
		base_rr = 120;
		max_div_def = 4; /* 30hz */
		min_div_def = min_div_lowest = 120; /* 1hz */
		break;
	case VRR_48NS:
		base_rr = 48;
		max_div_def = min_div_def = min_div_lowest = 1; /* 48hz */
		break;
	case VRR_48HS:
		base_rr = 96;
		max_div_def = 2; /* 48hz */
		min_div_def = min_div_lowest = 96; /* 1hz */
		break;
	case VRR_60NS:
		base_rr = 60;
		max_div_def = 1; /* 60hz */
		min_div_def = min_div_lowest = 2; /* 30hz */
		break;
	case VRR_60HS:
		base_rr = 120;
		max_div_def = 2; /* 60hz */
		min_div_def = min_div_lowest = 120; /* 1hz */
		break;
	case VRR_96HS:
		base_rr = 96;
		max_div_def = 1; /* 96hz */
		min_div_def = min_div_lowest = 96; /* 1hz */
		break;
	case VRR_120HS:
		base_rr = 120;
		max_div_def = 1; /* 120hz */
		min_div_def = min_div_lowest = 120; /* 1hz */
		break;
	default:
		LCD_ERR(vdd, "invalid vrr_id\n");
		base_rr = 120;
		max_div_def = 1; /* 120hz */
		min_div_def = min_div_lowest = 120; /* 1hz */
		break;
	}

	/* HBM 1Hz */
	if (mngr->scalability[LFD_SCOPE_NORMAL] == LFD_FUNC_SCALABILITY6)
		min_div_def = min_div_lowest;

done:
	lfd_base->base_rr = base_rr;
	lfd_base->max_div_def = max_div_def;
	lfd_base->min_div_def = min_div_def;
	lfd_base->min_div_lowest = min_div_lowest;
	lfd_base->fix_div_def = 1; // LFD MAX/MIN 120hz fix
	lfd_base->highdot_div_def = 120 * 2; // 120hz % 240 = 0.5hz for highdot test (120hz base)

	vrr->lfd.base_rr = base_rr;

	LCD_DEBUG(vdd, "LFD(%s): base_rr: %uhz, def: %uhz(%u)~%uhz(%u), lowest: %uhz(%u), highdot_div: %u\n",
			lfd_scope_name[scope], base_rr,
			DIV_ROUND_UP(base_rr, min_div_def), min_div_def,
			DIV_ROUND_UP(base_rr, max_div_def), max_div_def,
			DIV_ROUND_UP(base_rr, min_div_lowest), min_div_lowest,
			lfd_base->highdot_div_def);

	return 0;
}

static int ss_pre_hmt_brightness(struct samsung_display_driver_data *vdd)
{
	vdd->br_info.last_br_is_hbm = false;

	return 0;
}

static int ss_pre_brightness(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "invalid vdd\n");
		return -ENODEV;
	}

	if (vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL) {
		/* HBM -> Normal Case */
		if (vdd->br_info.last_br_is_hbm) {
			LCD_INFO(vdd, "HBM -> Normal Case, Disable ESD\n");

			/* If there is a pending ESD enable work, cancel that first */
			cancel_delayed_work(&vdd->esd_enable_event_work);

			/* To avoid unexpected ESD detction, Disable ESD irq before cmd tx related with 51h */
			if (vdd->esd_recovery.esd_irq_enable)
				vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd, ESD_MASK_WORK);

			/* Enable ESD after (ESD_WORK_DELAY)ms */
			schedule_delayed_work(&vdd->esd_enable_event_work,
				msecs_to_jiffies(ESD_WORK_DELAY));
		}

		vdd->br_info.last_br_is_hbm = false;
	} else {
		/* Normal -> HBM Case */
		if (!vdd->br_info.last_br_is_hbm) {
			LCD_INFO(vdd, "Normal -> HBM Case, Disable ESD\n");

			/* If there is a pending ESD enable work, cancel that first */
			cancel_delayed_work(&vdd->esd_enable_event_work);

			/* To avoid unexpected ESD detction, Disable ESD irq before cmd tx related with 51h */
			if (vdd->esd_recovery.esd_irq_enable)
				vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd, ESD_MASK_WORK);

			/* Enable ESD after (ESD_WORK_DELAY)ms */
			schedule_delayed_work(&vdd->esd_enable_event_work,
				msecs_to_jiffies(ESD_WORK_DELAY));
		}

		vdd->br_info.last_br_is_hbm = true;
	}

	return 0;
}

static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	int i, len = 0;
	u8 temp[20];
	int rx_len;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	pcmds = ss_get_cmds(vdd, RX_DDI_ID);
	if (pcmds->count <= 0) {
		LCD_ERR(vdd, "DSI%d no ddi_id_rx_cmds cmds", vdd->ndx);
		return false;
	}

	/* Read mtp (D6h 1~5th) for CHIP ID */
	rx_len = ss_send_cmd_get_rx(vdd, RX_DDI_ID, read_buf);
	if (rx_len < 0 || rx_len > MAX_READ_BUF_SIZE) {
		LCD_ERR(vdd, "invalid rx_len(%d)\n", rx_len);
		return false;
	}

	for (i = 0; i < rx_len; i++)
		len += sprintf(temp + len, "%02x", read_buf[i]);
	len += sprintf(temp + len, "\n");

	vdd->ddi_id_dsi = kzalloc(len, GFP_KERNEL);
	if (!vdd->ddi_id_dsi) {
		LCD_ERR(vdd, "fail to kzalloc for ddi_id_dsi\n");
		return false;
	}

	vdd->ddi_id_len = len;
	strlcat(vdd->ddi_id_dsi, temp, len);

	LCD_INFO(vdd, "[%d] %s\n", vdd->ddi_id_len, vdd->ddi_id_dsi);

	return true;
}

#define COORDINATE_DATA_SIZE 6
#define MDNIE_SCR_WR_ADDR	0x32
#define RGB_INDEX_SIZE 4
#define COEFFICIENT_DATA_SIZE 8

#define F1(x, y) (((y << 10) - (((x << 10) * 56) / 55) - (102 << 10)) >> 10)
#define F2(x, y) (((y << 10) + (((x << 10) * 5) / 1) - (18483 << 10)) >> 10)

static char coordinate_data_1[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_8 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_9 */
};

static char coordinate_data_2[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xfc, 0x00, 0xf4, 0x00}, /* dummy */
	{0xff, 0x00, 0xfc, 0x00, 0xf4, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfc, 0x00, 0xf4, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xfc, 0x00, 0xf4, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfc, 0x00, 0xf4, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xfc, 0x00, 0xf4, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xfc, 0x00, 0xf4, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xfc, 0x00, 0xf4, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xfc, 0x00, 0xf4, 0x00}, /* Tune_8 */
	{0xff, 0x00, 0xfc, 0x00, 0xf4, 0x00}, /* Tune_9 */
};

static char (*coordinate_data[MAX_MODE])[COORDINATE_DATA_SIZE] = {
	coordinate_data_2,
	coordinate_data_2,
	coordinate_data_2,
	coordinate_data_1,
	coordinate_data_1,
	coordinate_data_1
};

static int rgb_index[][RGB_INDEX_SIZE] = {
	{ 5, 5, 5, 5 }, /* dummy */
	{ 5, 2, 6, 3 },
	{ 5, 2, 4, 1 },
	{ 5, 8, 4, 7 },
	{ 5, 8, 6, 9 },
};

static int coefficient[][COEFFICIENT_DATA_SIZE] = {
	{       0,        0,      0,      0,      0,       0,      0,      0 }, /* dummy */
	{  -52615,   -61905,  21249,  15603,  40775,   80902, -19651, -19618 },
	{ -212096,  -186041,  61987,  65143, -75083,  -27237,  16637,  15737 },
	{   69454,    77493, -27852, -19429, -93856, -133061,  37638,  35353 },
	{  192949,   174780, -56853, -60597,  57592,   13018, -11491, -10757 },
};

static int mdnie_coordinate_index(int x, int y)
{
	int tune_number = 0;

	if (F1(x, y) > 0) {
		if (F2(x, y) > 0)
			tune_number = 1;
		else
			tune_number = 2;
	} else {
		if (F2(x, y) > 0)
			tune_number = 4;
		else
			tune_number = 3;
	}

	return tune_number;
}

static int mdnie_coordinate_x(int x, int y, int index)
{
	int result = 0;

	result = (coefficient[index][0] * x) + (coefficient[index][1] * y) +
			(((coefficient[index][2] * x + 512) >> 10) * y) +
			(coefficient[index][3] * 10000);

	result = (result + 512) >> 10;

	if (result < 0)
		result = 0;
	if (result > 1024)
		result = 1024;

	return result;
}

static int mdnie_coordinate_y(int x, int y, int index)
{
	int result = 0;

	result = (coefficient[index][4] * x) + (coefficient[index][5] * y) +
			(((coefficient[index][6] * x + 512) >> 10) * y) +
			(coefficient[index][7] * 10000);

	result = (result + 512) >> 10;

	if (result < 0)
		result = 0;
	if (result > 1024)
		result = 1024;

	return result;
}

static int dsi_update_mdnie_data(struct samsung_display_driver_data *vdd)
{
	struct mdnie_lite_tune_data *mdnie_data;

	mdnie_data = kzalloc(sizeof(struct mdnie_lite_tune_data), GFP_KERNEL);
	if (!mdnie_data) {
		LCD_ERR(vdd, "fail to allocate mdnie_data memory\n");
		return -ENOMEM;
	}

	/* Update mdnie command */
	mdnie_data->DSI_COLOR_BLIND_MDNIE_1 = COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_1 = RGB_SENSOR_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_2 = RGB_SENSOR_MDNIE_2;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_3 = RGB_SENSOR_MDNIE_3;
	mdnie_data->DSI_TRANS_DIMMING_MDNIE = RGB_SENSOR_MDNIE_3;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE_2 = UI_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_UI_STANDARD_MDNIE_2 = UI_STANDARD_MDNIE_2;
	mdnie_data->DSI_UI_AUTO_MDNIE_2 = UI_AUTO_MDNIE_2;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE_2 = VIDEO_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE_2 = VIDEO_STANDARD_MDNIE_2;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE_2 = VIDEO_AUTO_MDNIE_2;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE_2 = CAMERA_AUTO_MDNIE_2;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE_2 = GALLERY_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE_2 = GALLERY_STANDARD_MDNIE_2;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE_2 = GALLERY_AUTO_MDNIE_2;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE_2 = BROWSER_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE_2 = BROWSER_STANDARD_MDNIE_2;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE_2 = BROWSER_AUTO_MDNIE_2;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE_2 = EBOOK_AUTO_MDNIE_2;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE_2 = EBOOK_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE_2 = EBOOK_STANDARD_MDNIE_2;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE_2 = EBOOK_AUTO_MDNIE_2;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE_2 = TDMB_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE_2 = TDMB_STANDARD_MDNIE_2;
	mdnie_data->DSI_TDMB_AUTO_MDNIE_2 = TDMB_AUTO_MDNIE_2;

	mdnie_data->DSI_BYPASS_MDNIE = BYPASS_MDNIE;
	mdnie_data->DSI_NEGATIVE_MDNIE = NEGATIVE_MDNIE;
	mdnie_data->DSI_COLOR_BLIND_MDNIE = COLOR_BLIND_MDNIE;
	mdnie_data->DSI_HBM_CE_MDNIE = HBM_CE_MDNIE;
	mdnie_data->DSI_HBM_CE_D65_MDNIE = HBM_CE_D65_MDNIE;
	mdnie_data->DSI_RGB_SENSOR_MDNIE = RGB_SENSOR_MDNIE;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE = UI_DYNAMIC_MDNIE;
	mdnie_data->DSI_UI_STANDARD_MDNIE = UI_STANDARD_MDNIE;
	mdnie_data->DSI_UI_NATURAL_MDNIE = UI_NATURAL_MDNIE;
	mdnie_data->DSI_UI_AUTO_MDNIE = UI_AUTO_MDNIE;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE = VIDEO_DYNAMIC_MDNIE;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE = VIDEO_STANDARD_MDNIE;
	mdnie_data->DSI_VIDEO_NATURAL_MDNIE = VIDEO_NATURAL_MDNIE;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE = VIDEO_AUTO_MDNIE;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE = CAMERA_AUTO_MDNIE;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE = GALLERY_DYNAMIC_MDNIE;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE = GALLERY_STANDARD_MDNIE;
	mdnie_data->DSI_GALLERY_NATURAL_MDNIE = GALLERY_NATURAL_MDNIE;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE = GALLERY_AUTO_MDNIE;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE = BROWSER_DYNAMIC_MDNIE;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE = BROWSER_STANDARD_MDNIE;
	mdnie_data->DSI_BROWSER_NATURAL_MDNIE = BROWSER_NATURAL_MDNIE;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE = BROWSER_AUTO_MDNIE;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE = EBOOK_DYNAMIC_MDNIE;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE = EBOOK_STANDARD_MDNIE;
	mdnie_data->DSI_EBOOK_NATURAL_MDNIE = EBOOK_NATURAL_MDNIE;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE = EBOOK_AUTO_MDNIE;
	mdnie_data->DSI_EMAIL_AUTO_MDNIE = EMAIL_AUTO_MDNIE;
	mdnie_data->DSI_GAME_LOW_MDNIE = BYPASS_MDNIE;
	mdnie_data->DSI_GAME_MID_MDNIE = BYPASS_MDNIE;
	mdnie_data->DSI_GAME_HIGH_MDNIE = BYPASS_MDNIE;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE = TDMB_DYNAMIC_MDNIE;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE = TDMB_STANDARD_MDNIE;
	mdnie_data->DSI_TDMB_NATURAL_MDNIE = TDMB_NATURAL_MDNIE;
	mdnie_data->DSI_TDMB_AUTO_MDNIE = TDMB_AUTO_MDNIE;
	mdnie_data->DSI_GRAYSCALE_MDNIE = GRAYSCALE_MDNIE;
	mdnie_data->DSI_GRAYSCALE_NEGATIVE_MDNIE = GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data->DSI_CURTAIN = SCREEN_CURTAIN_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE = NIGHT_MODE_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE_SCR = NIGHT_MODE_MDNIE_1;
	mdnie_data->DSI_COLOR_LENS_MDNIE = COLOR_LENS_MDNIE;
	mdnie_data->DSI_COLOR_LENS_MDNIE_SCR = COLOR_LENS_MDNIE_1;
	mdnie_data->DSI_COLOR_BLIND_MDNIE_SCR = COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_SCR = RGB_SENSOR_MDNIE_1;
	mdnie_data->DSI_HBM_CE_MDNIE_SCR_1 = HBM_CE_MDNIE1_1;
	mdnie_data->DSI_HBM_CE_MDNIE_SCR_2 = HBM_CE_MDNIE2_1;
	mdnie_data->DSI_HBM_CE_MDNIE_SCR_3 = HBM_CE_MDNIE3_1;
	mdnie_data->DSI_HBM_CE_MDNIE_DIMMING_1 = HBM_CE_MDNIE1_3;
	mdnie_data->DSI_HBM_CE_MDNIE_DIMMING_2 = HBM_CE_MDNIE2_3;
	mdnie_data->DSI_HBM_CE_MDNIE_DIMMING_3 = HBM_CE_MDNIE3_3;

	mdnie_data->mdnie_tune_value_dsi = mdnie_tune_value_dsi0;
	mdnie_data->hmt_color_temperature_tune_value_dsi = hmt_color_temperature_tune_value_dsi0;
	mdnie_data->light_notification_tune_value_dsi = light_notification_tune_value_dsi0;
	mdnie_data->hdr_tune_value_dsi = hdr_tune_value_dsi0;
	mdnie_data->hbm_ce_data = hbm_ce_data;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data->dsi_bypass_mdnie_size = ARRAY_SIZE(BYPASS_MDNIE);
	mdnie_data->mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET;
	mdnie_data->mdnie_scr_cmd_offset = MDNIE_SCR_CMD_OFFSET;
	mdnie_data->mdnie_step_index[MDNIE_STEP1] = MDNIE_STEP1_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP2] = MDNIE_STEP2_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP3] = MDNIE_STEP3_INDEX;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_RED_OFFSET] = ADDRESS_SCR_WHITE_RED;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_GREEN_OFFSET] = ADDRESS_SCR_WHITE_GREEN;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_BLUE_OFFSET] = ADDRESS_SCR_WHITE_BLUE;
	mdnie_data->dsi_rgb_sensor_mdnie_1_size = RGB_SENSOR_MDNIE_1_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_2_size = RGB_SENSOR_MDNIE_2_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_3_size = RGB_SENSOR_MDNIE_3_SIZE;

	mdnie_data->dsi_trans_dimming_data_index = MDNIE_TRANS_DIMMING_DATA_INDEX;
	mdnie_data->dsi_trans_dimming_slope_index = MDNIE_TRANS_DIMMING_SLOPE_INDEX;

	mdnie_data->dsi_adjust_ldu_table = adjust_ldu_data;
	mdnie_data->dsi_max_adjust_ldu = 6;
	mdnie_data->dsi_night_mode_table = night_mode_data;
	mdnie_data->dsi_max_night_mode_index = 306;
	mdnie_data->dsi_hbm_scr_table = hbm_scr_data;
	mdnie_data->dsi_color_lens_table = color_lens_data;
	mdnie_data->dsi_white_default_r = 0xff;
	mdnie_data->dsi_white_default_g = 0xff;
	mdnie_data->dsi_white_default_b = 0xff;
	mdnie_data->dsi_white_balanced_r = 0;
	mdnie_data->dsi_white_balanced_g = 0;
	mdnie_data->dsi_white_balanced_b = 0;
	mdnie_data->dsi_scr_step_index = MDNIE_STEP1_INDEX;
	mdnie_data->dsi_afc_size = 71;
	mdnie_data->dsi_afc_index = 56;

	vdd->mdnie.mdnie_data = mdnie_data;

	return 0;
}

static int ss_module_info_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	int year, month, day;
	int hour, min;
	int x, y;
	int mdnie_tune_index = 0;
	char temp[50];
	int rx_len, len = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	pcmds = ss_get_cmds(vdd, RX_MODULE_INFO);
	if (pcmds->count <= 0) {
		LCD_ERR(vdd, "no module_info_rx_cmds cmds(%d)", vdd->panel_revision);
		return false;
	}

	rx_len = ss_send_cmd_get_rx(vdd, RX_MODULE_INFO, read_buf);
	if (rx_len < 0 || rx_len > MAX_READ_BUF_SIZE) {
		LCD_ERR(vdd, "invalid rx_len(%d)\n", rx_len);
		return false;
	}

	/* Manufacture Date */
	year = read_buf[4] & 0xf0;
	year >>= 4;
	year += 2011; /* 0 = 2011 year */
	month = read_buf[4] & 0x0f;
	day = read_buf[5] & 0x1f;
	hour = read_buf[6] & 0x0f;
	min = read_buf[7] & 0x1f;

	vdd->manufacture_date_dsi = year * 10000 + month * 100 + day;
	vdd->manufacture_time_dsi = hour * 100 + min;

	LCD_INFO(vdd, "manufacture_date (%d%04d), y:m:d=%d:%d:%d, h:m=%d:%d\n",
		vdd->manufacture_date_dsi, vdd->manufacture_time_dsi,
		year, month, day, hour, min);

	/* While Coordinates */

	vdd->mdnie.mdnie_x = read_buf[0] << 8 | read_buf[1];	/* X */
	vdd->mdnie.mdnie_y = read_buf[2] << 8 | read_buf[3];	/* Y */

	mdnie_tune_index = mdnie_coordinate_index(vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);

	if (((vdd->mdnie.mdnie_x - 3050) * (vdd->mdnie.mdnie_x - 3050) + (vdd->mdnie.mdnie_y - 3210) * (vdd->mdnie.mdnie_y - 3210)) <= 225) {
		x = 0;
		y = 0;
	} else {
		x = mdnie_coordinate_x(vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y, mdnie_tune_index);
		y = mdnie_coordinate_y(vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y, mdnie_tune_index);
	}

	coordinate_tunning_calculate(vdd, x, y, coordinate_data,
			rgb_index[mdnie_tune_index],
			MDNIE_SCR_WR_ADDR, COORDINATE_DATA_SIZE);

	LCD_INFO(vdd, "X-%d Y-%d \n", vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);

	/* CELL ID (manufacture date + white coordinates) */
	/* Manufacture Date */
	len += sprintf(temp + len, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
			read_buf[4], read_buf[5], read_buf[6], read_buf[7],
			read_buf[8], read_buf[9], read_buf[10], read_buf[0],
			read_buf[1], read_buf[2], read_buf[3]);

	vdd->cell_id_dsi = kzalloc(len, GFP_KERNEL);
	if (!vdd->cell_id_dsi) {
		LCD_ERR(vdd, "fail to kzalloc for cell_id_dsi\n");
		return false;
	}

	vdd->cell_id_len = len;
	strlcat(vdd->cell_id_dsi, temp, vdd->cell_id_len);
	LCD_INFO(vdd, "CELL ID: [%d] %s\n", vdd->cell_id_len, vdd->cell_id_dsi);

	return true;
}

static int ss_octa_id_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	char temp[50];
	int len = 0;
	int rx_len;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	pcmds = ss_get_cmds(vdd, RX_OCTA_ID);
	if (pcmds->count <= 0) {
		LCD_ERR(vdd, "DSI%d no octa_id_rx_cmds cmd\n", vdd->ndx);
		return false;
	}

	/* Read Panel Unique OCTA ID (C9h 2nd~21th) */
	rx_len = ss_send_cmd_get_rx(vdd, RX_OCTA_ID, read_buf);
	if (rx_len < 0 || rx_len > MAX_READ_BUF_SIZE) {
		LCD_ERR(vdd, "invalid rx_len(%d)\n", rx_len);
		return false;
	}

	len += sprintf(temp + len, "%d", (read_buf[0] & 0xF0) >> 4);
	len += sprintf(temp + len, "%d", (read_buf[0] & 0x0F));
	len += sprintf(temp + len, "%d", (read_buf[1] & 0x0F));
	len += sprintf(temp + len, "%02x", read_buf[2]);
	len += sprintf(temp + len, "%02x", read_buf[3]);

	len += sprintf(temp + len, "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
			read_buf[4], read_buf[5], read_buf[6], read_buf[7],
			read_buf[8], read_buf[9], read_buf[10], read_buf[11],
			read_buf[12], read_buf[13], read_buf[14], read_buf[15],
			read_buf[16], read_buf[17], read_buf[18], read_buf[19]);

	vdd->octa_id_dsi = kzalloc(len, GFP_KERNEL);
	if (!vdd->octa_id_dsi) {
		LCD_ERR(vdd, "fail to kzalloc for octa_id_dsi\n");
		return false;
	}

	vdd->octa_id_len = len;
	strlcat(vdd->octa_id_dsi, temp, vdd->octa_id_len);
	LCD_INFO(vdd, "octa id : [%d] %s \n", vdd->octa_id_len, vdd->octa_id_dsi);

	return true;
}

static int ss_pre_lpm_brightness(struct samsung_display_driver_data *vdd)
{
	vdd->br_info.last_br_is_hbm = false;

	return 0;
}

static int ss_ecc_read(struct samsung_display_driver_data *vdd)
{
	u8 ecc[3] = {0, };
	bool pass = false;

	ss_send_cmd_get_rx(vdd, RX_GCT_ECC, ecc);

	if ((ecc[0] == vdd->ecc_check[0]) && (ecc[1] == vdd->ecc_check[1]) && (ecc[2] == vdd->ecc_check[2]))
		pass = true;
	else
		pass = false;

	LCD_INFO(vdd, "ECC = 0x%02X 0x%02X 0x%02X -> %s\n", ecc[0], ecc[1], ecc[2], pass ? "ECC_PASS" : "ECC_FAIL");

	return pass;
}

static int ss_ssr_read(struct samsung_display_driver_data *vdd)
{
	u8 ssr[2] = {0, };
	bool pass = false;

	ss_send_cmd_get_rx(vdd, RX_SSR, ssr);

	if ((ssr[0] == 0xC4) && ((ssr[1] & 0xF) == 0x0))
		pass = true;
	else
		pass = false;

	LCD_INFO(vdd, "SSR = 0x%02X 0x%02X -> %s\n", ssr[0], ssr[1], pass ? "SSR_PASS" : "SSR_FAIL");

	return pass;
}

static int ss_self_display_data_init(struct samsung_display_driver_data *vdd)
{
	u32 panel_type = 0;
	u32 panel_color = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->self_disp.is_support) {
		LCD_ERR(vdd, "Self Display is not supported\n");
		return -EINVAL;
	}

	LCD_INFO(vdd, "Self Display Panel Data init\n");

	panel_type = (ss_panel_id0_get(vdd) & 0x30) >> 4;
	panel_color = ss_panel_id0_get(vdd) & 0xF;

	LCD_INFO(vdd, "Panel Type=0x%x, Panel Color=0x%x\n", panel_type, panel_color);

	vdd->self_disp.operation[FLAG_SELF_MASK].img_buf = self_mask_img_data;
	vdd->self_disp.operation[FLAG_SELF_MASK].img_size = ARRAY_SIZE(self_mask_img_data);
	vdd->self_disp.operation[FLAG_SELF_MASK].img_checksum = SELF_MASK_IMG_CHECKSUM;
	make_mass_self_display_img_cmds_HAE(vdd, TX_SELF_MASK_IMAGE, FLAG_SELF_MASK);

	if (vdd->is_factory_mode) {
		vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_buf = self_mask_img_crc_data;
		vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_size = ARRAY_SIZE(self_mask_img_crc_data);
		make_mass_self_display_img_cmds_HAE(vdd, TX_SELF_MASK_IMAGE_CRC, FLAG_SELF_MASK_CRC);
	}

	return 1;
}


static int ss_mafpc_data_init(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	LCD_INFO(vdd, "mAFPC Panel Data init\n");

	vdd->mafpc.img_buf = mafpc_img_data;
	vdd->mafpc.img_size = ARRAY_SIZE(mafpc_img_data);

	if (vdd->mafpc.make_img_mass_cmds) {
		/* Image Data */
		vdd->mafpc.make_img_mass_cmds(vdd, vdd->mafpc.img_buf, vdd->mafpc.img_size, TX_MAFPC_IMAGE);
	} else if (vdd->mafpc.make_img_cmds) {
		/* Image Data */
		vdd->mafpc.make_img_cmds(vdd, vdd->mafpc.img_buf, vdd->mafpc.img_size, TX_MAFPC_IMAGE);
	} else {
		LCD_ERR(vdd, "Can not make mafpc image commands\n");
		return -EINVAL;
	}

	if (vdd->is_factory_mode) {
		/* CRC Check For Factory Mode */
		vdd->mafpc.crc_img_buf = mafpc_img_data_crc_check;
		vdd->mafpc.crc_img_size = ARRAY_SIZE(mafpc_img_data_crc_check);

		if (vdd->mafpc.make_img_mass_cmds) {
			/* CRC Check Image Data */
			vdd->mafpc.make_img_mass_cmds(vdd, vdd->mafpc.crc_img_buf,
					vdd->mafpc.crc_img_size, TX_MAFPC_CRC_CHECK_IMAGE);
		} else if (vdd->mafpc.make_img_cmds) {
			/* CRC Check Image Data */
			vdd->mafpc.make_img_cmds(vdd, vdd->mafpc.crc_img_buf,
					vdd->mafpc.crc_img_size, TX_MAFPC_CRC_CHECK_IMAGE);
		} else {
			LCD_ERR(vdd, "Can not make mafpc image commands\n");
			return -EINVAL;
		}
	}

	return true;
}

static void ss_copr_panel_init(struct samsung_display_driver_data *vdd)
{
	vdd->copr.ver = COPR_VER_5P0;
	vdd->copr.display_read = 0;
	ss_copr_init(vdd);
}

static int ss_post_vrr(struct samsung_display_driver_data *vdd,
			int old_rr, bool old_hs, bool old_phs,
			int new_rr, bool new_hs, bool new_phs)
{
	if (new_rr == 120 || new_rr == 96)
		vdd->dbg_tear_info.early_te_delay_us = 2000; /* 2ms */
	else
		vdd->dbg_tear_info.early_te_delay_us = 0;

	return 0;
}

static int ss_vrr_init(struct vrr_info *vrr)
{
	struct lfd_mngr *mngr;
	int i, scope;
	struct samsung_display_driver_data *vdd =
		container_of(vrr, struct samsung_display_driver_data, vrr);

	LCD_INFO(vdd, "+++\n");

	mutex_init(&vrr->vrr_lock);
	mutex_init(&vrr->brr_lock);

	vrr->running_vrr_mdp = false;
	vrr->running_vrr = false;

	/* initial value : Bootloader: 120HS */
	vrr->prev_refresh_rate = vrr->cur_refresh_rate = vrr->adjusted_refresh_rate = 120;
	vrr->prev_sot_hs_mode = vrr->cur_sot_hs_mode = vrr->adjusted_sot_hs_mode = true;
	vrr->prev_phs_mode = vrr->cur_phs_mode = vrr->adjusted_phs_mode = false;

	vrr->hs_nm_seq = HS_NM_OFF;
	vrr->delayed_perf_normal = false;
	vrr->skip_vrr_in_brightness = false;

	vrr->brr_mode = BRR_OFF_MODE;
	vrr->brr_rewind_on = false;

	vrr->vrr_workqueue = create_singlethread_workqueue("vrr_workqueue");
	INIT_WORK(&vrr->vrr_work, ss_panel_vrr_switch_work);

	/* LFD mode */
	for (i = 0, mngr = &vrr->lfd.lfd_mngr[i]; i < LFD_CLIENT_MAX; i++, mngr++) {
		for (scope = 0; scope < LFD_SCOPE_MAX; scope++) {
			mngr->fix[scope] = LFD_FUNC_FIX_OFF;
			mngr->scalability[scope] = LFD_FUNC_SCALABILITY0;
			mngr->min[scope] = LFD_FUNC_MIN_CLEAR;
			mngr->max[scope] = LFD_FUNC_MAX_CLEAR;
		}
	}

#if IS_ENABLED(CONFIG_SEC_FACTORY)
	mngr = &vrr->lfd.lfd_mngr[LFD_CLIENT_FAC];
	mngr->fix[LFD_SCOPE_NORMAL] = LFD_FUNC_FIX_HIGH;
	mngr->fix[LFD_SCOPE_LPM] = LFD_FUNC_FIX_HIGH;
	mngr->fix[LFD_SCOPE_HMD] = LFD_FUNC_FIX_HIGH;
#endif

	/* TE modulation */
	vrr->te_mod_on = 0;
	vrr->te_mod_divider = 0;
	vrr->te_mod_cnt = 0;

	LCD_INFO(vdd, "---\n");
	return 0;
}

static bool ss_check_support_mode(struct samsung_display_driver_data *vdd, enum CHECK_SUPPORT_MODE mode)
{
	bool is_support = true;
	int cur_rr = vdd->vrr.cur_refresh_rate;
	bool cur_hs = vdd->vrr.cur_sot_hs_mode;

	switch (mode) {

	case CHECK_SUPPORT_BRIGHTDOT:
		if (!(cur_rr == 120 && cur_hs)) {
			is_support = false;
			LCD_ERR(vdd, "BRIGHT DOT fail: supported on 120HS(cur: %d%s)\n",
					cur_rr, cur_hs ? "HS" : "NS");
		}
		break;

	default:
		break;
	}

	return is_support;
}

static void ss_read_flash(struct samsung_display_driver_data *vdd, u32 raddr, u32 rsize, u8 *rbuf)
{
	char showbuf[256];
	int i, pos = 0;

	if (rsize > 0x100) {
		LCD_ERR(vdd, "rsize(%x) is not supported..\n", rsize);
		return;
	}

	ss_send_cmd(vdd, TX_POC_ENABLE);
	spsram_read_bytes(vdd, raddr, rsize, rbuf);
	ss_send_cmd(vdd, TX_POC_DISABLE);

	for (i = 0; i < rsize; i++)
		pos += scnprintf(showbuf + pos, 256 - pos, "%02x ", rbuf[i]);
	LCD_INFO(vdd, "buf : %s\n", showbuf);
}

/* mtp original data */
static u8 HS120_R_TYPE_BUF[GAMMA_SET_MAX][GAMMA_R_SIZE];
/* mtp original data - V type */
static int HS120_V_TYPE_BUF[GAMMA_SET_MAX][GAMMA_V_SIZE];

/* HS96 compensated data */
//static u8 HS96_R_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_R_SIZE];
/* HS96 compensated data V type*/
//static int HS96_V_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_V_SIZE];

static void ss_print_gamma_comp(struct samsung_display_driver_data *vdd)
{
	char pBuffer[256];
	int i, j;

	LCD_INFO(vdd, "== HS120_R_TYPE_BUF ==\n");
	for (i = 0; i < GAMMA_SET_MAX; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_R_TYPE_BUF[i][j]);
		LCD_INFO(vdd, "READ_R SET[%2d] : %s\n", GAMMA_SET_MAX - 1 - i, pBuffer);
	}

	LCD_INFO(vdd, "== HS120_V_TYPE_BUF ==\n");
	for (i = 0; i < GAMMA_SET_MAX; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_V_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_V_TYPE_BUF[i][j]);
		LCD_INFO(vdd, "READ_V SET[%2d] : %s\n", GAMMA_SET_MAX - 1 - i, pBuffer);
	}

	LCD_INFO(vdd, "== HS120_V_TYPE_COMP ==\n");
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_V_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_V_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP_V 120 LV[%3d] : %s\n", i, pBuffer);
	}

	LCD_INFO(vdd, "== HS120_R_TYPE_COMP ==\n");
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_R_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP_R 120 LV[%3d] : %s\n", i, pBuffer);
	}
}

#define FLASH_READ_SIZE (GAMMA_R_SIZE * 2)

static void ss_read_gamma(struct samsung_display_driver_data *vdd)
{
	u8 readbuf[FLASH_READ_SIZE];
	char pBuffer[256];
	int i, j;

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.permit_frame_update = 1;
	vdd->exclusive_tx.enable = 1;

	ss_set_exclusive_tx_packet(vdd, TX_SPSRAM_DATA_READ, 1);
	ss_set_exclusive_tx_packet(vdd, RX_FLASH_GAMMA, 1);
	ss_set_exclusive_tx_packet(vdd, TX_POC_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, TX_POC_DISABLE, 1);

	ss_send_cmd(vdd, TX_POC_ENABLE);

	LCD_INFO(vdd, "READ_R start\n");

	/* total 840 = 70bytes * 12set
	 * 120HS : 0x0690 ~ 0x09D7
	 */
	for (i = 0; i < GAMMA_SET_MAX; i+=2) {
		LCD_INFO(vdd, "[120] start_addr : %X, size : %d\n", GAMMA_SET_ADDR_120_TABLE[i], FLASH_READ_SIZE);
		spsram_read_bytes(vdd, GAMMA_SET_ADDR_120_TABLE[i], FLASH_READ_SIZE, readbuf);
		memcpy(HS120_R_TYPE_BUF[i], readbuf, GAMMA_R_SIZE);
		memcpy(HS120_R_TYPE_BUF[i+1], readbuf + (GAMMA_R_SIZE), GAMMA_R_SIZE);
	}

	ss_send_cmd(vdd, TX_POC_DISABLE);

	LCD_INFO(vdd, "== HS120_R_TYPE_BUF ==\n");
	for (i = 0; i < GAMMA_SET_MAX; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_R_TYPE_BUF[i][j]);
		LCD_INFO(vdd, "READ_R 120 SET[%2d] : %s\n", GAMMA_SET_MAX - 1 - i, pBuffer);
	}

	ss_set_exclusive_tx_packet(vdd, TX_SPSRAM_DATA_READ, 0);
	ss_set_exclusive_tx_packet(vdd, RX_FLASH_GAMMA, 0);
	ss_set_exclusive_tx_packet(vdd, TX_POC_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, TX_POC_DISABLE, 0);

	vdd->exclusive_tx.enable = 0;
	vdd->exclusive_tx.permit_frame_update = 0;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);

	return;
}

static int ss_gm2_gamma_comp_init(struct samsung_display_driver_data *vdd)
{
	int i, j, m, offset;
	int val, *v_val;
	struct cmd_legoop_map *analog_map;

	LCD_INFO(vdd, "++\n");

	/********************************************/
	/* 1.  Read HS120 ORIGINAL GAMMA Flash		*/
	/********************************************/
	ss_read_gamma(vdd);

	/***********************************************************/
	/* 2. translate Register type to V type 		   */
	/***********************************************************/
	for (i = 0; i < GAMMA_SET_MAX; i++) {
		m = 0;
		for (j = 0; j < GAMMA_R_SIZE; ) {
			HS120_V_TYPE_BUF[i][m++] = (GET_BITS(HS120_R_TYPE_BUF[i][j], 0, 3) << 8)
									| GET_BITS(HS120_R_TYPE_BUF[i][j+2], 0, 7);
			HS120_V_TYPE_BUF[i][m++] = (GET_BITS(HS120_R_TYPE_BUF[i][j+1], 4, 7) << 8)
									| GET_BITS(HS120_R_TYPE_BUF[i][j+3], 0, 7);
			HS120_V_TYPE_BUF[i][m++] = (GET_BITS(HS120_R_TYPE_BUF[i][j+1], 0, 3) << 8)
									| GET_BITS(HS120_R_TYPE_BUF[i][j+4], 0, 7);
			j += 5;
		}
	}

	/*************************************************************/
	/* 3. [ALL] Make HSXX_V_TYPE_COMP (NORMAL + HBM) */
	/*************************************************************/

	analog_map = &vdd->br_info.analog_offset_120hs[0];

	/* 120HS - from 120HS mtp gamma */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		v_val = HS120_V_TYPE_BUF[GAMMA_SET_REGION_TABLE[i]];
		for (j = 0; j < GAMMA_V_SIZE; j++) {
			if (!analog_map->cmds)
				offset = 0;
			else
				offset = analog_map->cmds[i][j];

			/* check underflow & overflow */
			if (v_val[j] + offset < 0) {
				HS120_V_TYPE_COMP[i][j] = 0;
			} else {
				if (j <= 5 || j >= (GAMMA_V_SIZE - 3)) {/* 1/13/14 th is 12bit(0xFFF) */
					val = v_val[j] + offset;
					if (val > 0xFFF)	/* check overflow */
						HS120_V_TYPE_COMP[i][j] = 0xFFF;
					else
						HS120_V_TYPE_COMP[i][j] = val;
				} else {	/* 2 ~ 12th 11bit(0x7FF) */
					val = v_val[j] + offset;
					if (val > 0x7FF)	/* check overflow */
						HS120_V_TYPE_COMP[i][j] = 0x7FF;
					else
						HS120_V_TYPE_COMP[i][j] = val;
				}
			}
		}
	}

#if 0
	analog_map = &vdd->br_info.analog_offset_96hs[0];

	/* 96HS - from 120HS mtp gamma */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		v_val = HS120_V_TYPE_BUF[GAMMA_SET_REGION_TABLE[i]];
		for (j = 0; j < GAMMA_V_SIZE; j++) {
			if (!analog_map->cmds)
				offset = 0;
			else
				offset = analog_map->cmds[i][j];

			/* check underflow & overflow */
			if (v_val[j] + offset < 0) {
				HS96_V_TYPE_COMP[i][j] = 0;
			} else {
				if (j <= 5 || j >= (GAMMA_V_SIZE - 3)) {/* 1/13/14 th is 12bit(0xFFF) */
					val = v_val[j] + offset;
					if (val > 0xFFF)	/* check overflow */
						HS96_V_TYPE_COMP[i][j] = 0xFFF;
					else
						HS96_V_TYPE_COMP[i][j] = val;
				} else {	/* 2 ~ 12th 11bit(0x7FF) */
					val = v_val[j] + offset;
					if (val > 0x7FF)	/* check overflow */
						HS96_V_TYPE_COMP[i][j] = 0x7FF;
					else
						HS96_V_TYPE_COMP[i][j] = val;
				}
			}
		}
	}

	analog_map = &vdd->br_info.analog_offset_60hs[0];

	/* 60HS - from 60HS mtp gamma */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		v_val = HS60_V_TYPE_BUF[GAMMA_SET_REGION_TABLE[i]];
		for (j = 0; j < GAMMA_V_SIZE; j++) {
			if (!analog_map->cmds)
				offset = 0;
			else
				offset = analog_map->cmds[i][j];

			/* check underflow & overflow */
			if (v_val[j] + offset < 0) {
				HS60_V_TYPE_COMP[i][j] = 0;
			} else {
				if (j <= 5 || j >= (GAMMA_V_SIZE - 3)) {/* 1/13/14 th is 12bit(0xFFF) */
					val = v_val[j] + offset;
					if (val > 0xFFF)	/* check overflow */
						HS60_V_TYPE_COMP[i][j] = 0xFFF;
					else
						HS60_V_TYPE_COMP[i][j] = val;
				} else {	/* 2 ~ 12th 11bit(0x7FF) */
					val = v_val[j] + offset;
					if (val > 0x7FF)	/* check overflow */
						HS60_V_TYPE_COMP[i][j] = 0x7FF;
					else
						HS60_V_TYPE_COMP[i][j] = val;
				}
			}
		}
	}

	analog_map = &vdd->br_info.analog_offset_48hs[0];

	/* 48HS - from 60HS mtp gamma */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		v_val = HS60_V_TYPE_BUF[GAMMA_SET_REGION_TABLE[i]];
		for (j = 0; j < GAMMA_V_SIZE; j++) {
			if (!analog_map->cmds)
				offset = 0;
			else
				offset = analog_map->cmds[i][j];

			/* check underflow & overflow */
			if (v_val[j] + offset < 0) {
				HS48_V_TYPE_COMP[i][j] = 0;
			} else {
				if (j <= 5 || j >= (GAMMA_V_SIZE - 3)) {/* 1/13/14 th is 12bit(0xFFF) */
					val = v_val[j] + offset;
					if (val > 0xFFF)	/* check overflow */
						HS48_V_TYPE_COMP[i][j] = 0xFFF;
					else
						HS48_V_TYPE_COMP[i][j] = val;
				} else {	/* 2 ~ 12th 11bit(0x7FF) */
					val = v_val[j] + offset;
					if (val > 0x7FF)	/* check overflow */
						HS48_V_TYPE_COMP[i][j] = 0x7FF;
					else
						HS48_V_TYPE_COMP[i][j] = val;
				}
			}
		}
	}
#endif

	/******************************************************/
	/* 4. translate HSXX_V_TYPE_COMP type to Register type*/
	/******************************************************/

	/* 120HS */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		m = 0; // GAMMA SET size
		for (j = 0; j < GAMMA_V_SIZE; j += RGB_MAX) {
			if (j == 0 || j == 3 || j == GAMMA_V_SIZE - 3) {
				HS120_R_TYPE_COMP[i][m++] = GET_BITS(HS120_V_TYPE_COMP[i][j+R], 8, 11);
				HS120_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_V_TYPE_COMP[i][j+G], 8, 11) << 4)
											| (GET_BITS(HS120_V_TYPE_COMP[i][j+B], 8, 11));
				HS120_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_V_TYPE_COMP[i][j+R], 0, 7));
				HS120_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_V_TYPE_COMP[i][j+G], 0, 7));
				HS120_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_V_TYPE_COMP[i][j+B], 0, 7));
			} else {	/* 1st ~ 10th */
				HS120_R_TYPE_COMP[i][m++] = GET_BITS(HS120_V_TYPE_COMP[i][j+R], 8, 10);
				HS120_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_V_TYPE_COMP[i][j+G], 8, 10) << 4)
											| (GET_BITS(HS120_V_TYPE_COMP[i][j+B], 8, 10));
				HS120_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_V_TYPE_COMP[i][j+R], 0, 7));
				HS120_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_V_TYPE_COMP[i][j+G], 0, 7));
				HS120_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_V_TYPE_COMP[i][j+B], 0, 7));
			}
		}
	}

#if 0
	/* 96HS */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		m = 0; // GAMMA SET size
		for (j = 0; j < GAMMA_V_SIZE; j += RGB_MAX) {
			if (j == 0 || j == 3 || j == GAMMA_V_SIZE - 3) {
				HS96_R_TYPE_COMP[i][m++] = GET_BITS(HS96_V_TYPE_COMP[i][j+R], 8, 11);
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 8, 11) << 4)
											| (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 8, 11));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+R], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 0, 7));
			} else {	/* 1st ~ 10th */
				HS96_R_TYPE_COMP[i][m++] = GET_BITS(HS96_V_TYPE_COMP[i][j+R], 8, 10);
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 8, 10) << 4)
											| (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 8, 10));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+R], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 0, 7));
			}
		}
	}

	/* 60HS */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		m = 0; // GAMMA SET size
		for (j = 0; j < GAMMA_V_SIZE; j += RGB_MAX) {
			if (j == 0 || j == 3 || j == GAMMA_V_SIZE - 3) {
				HS60_R_TYPE_COMP[i][m++] = GET_BITS(HS60_V_TYPE_COMP[i][j+R], 8, 11);
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+G], 8, 11) << 4)
											| (GET_BITS(HS60_V_TYPE_COMP[i][j+B], 8, 11));
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+R], 0, 7));
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+G], 0, 7));
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+B], 0, 7));
			} else {	/* 1st ~ 10th */
				HS60_R_TYPE_COMP[i][m++] = GET_BITS(HS60_V_TYPE_COMP[i][j+R], 8, 10);
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+G], 8, 10) << 4)
											| (GET_BITS(HS60_V_TYPE_COMP[i][j+B], 8, 10));
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+R], 0, 7));
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+G], 0, 7));
				HS60_R_TYPE_COMP[i][m++] = (GET_BITS(HS60_V_TYPE_COMP[i][j+B], 0, 7));
			}
		}
	}

	/* 48HS */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		m = 0; // GAMMA SET size
		for (j = 0; j < GAMMA_V_SIZE; j += RGB_MAX) {
			if (j == 0 || j == 3 || j == GAMMA_V_SIZE - 3) {
				HS48_R_TYPE_COMP[i][m++] = GET_BITS(HS48_V_TYPE_COMP[i][j+R], 8, 11);
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+G], 8, 11) << 4)
											| (GET_BITS(HS48_V_TYPE_COMP[i][j+B], 8, 11));
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+R], 0, 7));
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+G], 0, 7));
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+B], 0, 7));
			} else {	/* 1st ~ 10th */
				HS48_R_TYPE_COMP[i][m++] = GET_BITS(HS48_V_TYPE_COMP[i][j+R], 8, 10);
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+G], 8, 10) << 4)
											| (GET_BITS(HS48_V_TYPE_COMP[i][j+B], 8, 10));
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+R], 0, 7));
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+G], 0, 7));
				HS48_R_TYPE_COMP[i][m++] = (GET_BITS(HS48_V_TYPE_COMP[i][j+B], 0, 7));
			}
		}
	}
#endif

	LCD_INFO(vdd, " --\n");

	return 0;
}

/* UPDATE ANALOG_OFFSET_1 */
static int update_analog1_DM3_S6E3HAE_AMB681AZ01(
			struct samsung_display_driver_data *vdd,
			char *val, struct ss_cmd_desc *cmd)
{
	struct cmd_ref_state *state = &vdd->cmd_ref_state;
	int bl_lvl = state->bl_level;
	struct cmd_legoop_map *analog_map = &vdd->br_info.analog_offset_120hs[0];
	int i = -1;

	if (GAMMA_SET_REGION_TABLE[bl_lvl] != GAMMA_SET_0)
		return 0;

	LCD_ERR(vdd, "++ %d\n", bl_lvl);

	while (!cmd->pos_0xXX[++i] && i < cmd->tx_len);

	if (i + 1 >= cmd->tx_len) {
		LCD_ERR(vdd, "fail to find proper 0xXX position\n");
		return -EINVAL;
	}

	if (!analog_map->cmds) {
		LCD_ERR(vdd, "No offset data for analog 120HS\n");
		return -EINVAL;
	}

	/* write analog offset for G9,G10,G11 during on time. */
	/* analog offset is always same for all levels in each G region. */
	/* SET 0 : 0992(G11) -> 70+2 = 72 byte */

	memcpy(&cmd->txbuf[i], HS120_R_TYPE_COMP[bl_lvl], GAMMA_R_SIZE);

	vdd->br_info.last_tx_time = ktime_get();

	LCD_ERR(vdd, "--\n");

	return 0;
}

static int update_analog2_DM3_S6E3HAE_AMB681AZ01(
			struct samsung_display_driver_data *vdd,
			char *val, struct ss_cmd_desc *cmd)
{
	int i = -1;
	struct cmd_legoop_map *analog_map;

	analog_map = &vdd->br_info.analog_offset_120hs[0];

	while (!cmd->pos_0xXX[++i] && i < cmd->tx_len);

	if (i + 1 >= cmd->tx_len) {
		LCD_ERR(vdd, "fail to find proper 0xXX position\n");
		return -EINVAL;
	}

	if (!analog_map->cmds) {
		LCD_ERR(vdd, "No offset data for analog 120HS\n");
		return -EINVAL;
	}

	return 0;
}

static int update_analog3_DM3_S6E3HAE_AMB681AZ01(
			struct samsung_display_driver_data *vdd,
			char *val, struct ss_cmd_desc *cmd)
{
	int i = -1;
	struct cmd_legoop_map *analog_map;

	analog_map = &vdd->br_info.analog_offset_120hs[0];

	while (!cmd->pos_0xXX[++i] && i < cmd->tx_len);

	if (i + 1 >= cmd->tx_len) {
		LCD_ERR(vdd, "fail to find proper 0xXX position\n");
		return -EINVAL;
	}

	if (!analog_map->cmds) {
		LCD_ERR(vdd, "No offset data for analog 120HS\n");
		return -EINVAL;
	}

	vdd->br_info.last_tx_time = ktime_get();

	return 0;
}

static int update_aor_S6E3HAE_AMB681AZ01(struct samsung_display_driver_data *vdd,
			char *val, struct ss_cmd_desc *cmd)
{
	struct cmd_ref_state *state = &vdd->cmd_ref_state;
	int cur_rr = state->cur_refresh_rate;
	int bl_lvl = state->bl_level;
	struct cmd_legoop_map *manual_aor_map = NULL;
	int i = -1;

	while (!cmd->pos_0xXX[++i] && i < cmd->tx_len);

	if (i + 1 >= cmd->tx_len) {
		LCD_ERR(vdd, "fail to find proper 0xXX position\n");
		return -EINVAL;
	}

	if (cur_rr == 48 || cur_rr == 96)
		manual_aor_map = &vdd->br_info.manual_aor_96hs[vdd->panel_revision];
	else
		manual_aor_map = &vdd->br_info.manual_aor_120hs[vdd->panel_revision];

	if (manual_aor_map) {
		cmd->txbuf[i] = manual_aor_map->cmds[bl_lvl][0];
		cmd->txbuf[i + 1] = manual_aor_map->cmds[bl_lvl][1];

		LCD_DEBUG(vdd, "bl_lvl: %d, cur_rr: %d, i: %d, aor: 0x%X%X, tx_len: %d\n",
				bl_lvl, cur_rr, i, manual_aor_map->cmds[bl_lvl][0], manual_aor_map->cmds[bl_lvl][1], cmd->tx_len);
	}

	return 0;
}

static int ss_parse_panel_glut_table(struct samsung_display_driver_data *vdd,
		void *tbl, int *org_tbl)
{
	struct cmd_legoop_map *table = (struct cmd_legoop_map *) tbl;
	int i;

	table->col_size = GLUT_SIZE;
	table->row_size = GAMMA_OFFSET_SIZE;
	table->cmds = kvzalloc((sizeof(int *) * table->row_size), GFP_KERNEL);
	if (!table->cmds) {
		LCD_ERR(vdd, "fail to allocate cmds(%d)\n", table->row_size);
		return -ENOMEM;
	}

	LCD_DEBUG(vdd, "%dx%d\n", table->row_size, table->col_size);

	for (i = 0 ; i < table->row_size; i++)
		table->cmds[i] = &org_tbl[i * GLUT_SIZE];

	return 0;
}

static void update_glut_map(struct samsung_display_driver_data *vdd)
{
	ss_parse_panel_glut_table(vdd, &vdd->br_info.glut_offset_48hs, GLUT_OFFSET_48_96HS_V_revA);
	ss_parse_panel_glut_table(vdd, &vdd->br_info.glut_offset_night_dim, GLUT_OFFSET_night_dim_V_revA);
}

static int update_glut_enable(struct samsung_display_driver_data *vdd,
			char *val, struct ss_cmd_desc *cmd)
{
	struct cmd_ref_state *state = &vdd->cmd_ref_state;
	int cur_rr = state->cur_refresh_rate;
	int bl_lvl = state->bl_level;
	int i = -1;
	bool glut_enable = true;

	while (!cmd->pos_0xXX[++i] && i < cmd->tx_len);

	if (i + 1 > cmd->tx_len) {
		LCD_ERR(vdd, "fail to find proper 0xXX position(%d, %d)\n",
				i, cmd->tx_len);
		goto err_skip;
	}

	if (vdd->br_info.glut_skip) {
		LCD_ERR(vdd, "skip GLUT\n");
		goto err_skip;
	}

	if (vdd->night_dim && (bl_lvl <= 30)) /* night_dim */
		glut_enable = true;
	else if (cur_rr == 96 || cur_rr == 48)
		glut_enable = true;
	else
		glut_enable = false;

	cmd->txbuf[i] = glut_enable ? 0x02 : 0x00;

	LCD_INFO(vdd, "enable[%d] bl_lvl: %d, cur_rr: %d, i: %d, night_dim : %d\n",
		glut_enable, bl_lvl, cur_rr, i, vdd->night_dim);

	return 0;

err_skip:
	cmd->skip_by_cond = true;
	return -EINVAL;
}

static int update_glut(struct samsung_display_driver_data *vdd,
			char *val, struct ss_cmd_desc *cmd)
{
	struct cmd_ref_state *state = &vdd->cmd_ref_state;
	int cur_rr = state->cur_refresh_rate;
	int bl_lvl = state->bl_level;
	struct cmd_legoop_map *glut_map = NULL;
	int i = -1, j;

	while (!cmd->pos_0xXX[++i] && i < cmd->tx_len);

	if (i + GLUT_SIZE > cmd->tx_len) {
		LCD_ERR(vdd, "fail to find proper 0xXX position(%d, %d)\n",
				i, cmd->tx_len);
		goto err_skip;
	}

	if (vdd->br_info.glut_skip) {
		LCD_ERR(vdd, "skip GLUT\n");
		goto err_skip;
	}

	if (vdd->night_dim && (bl_lvl <= 30)) /* night_dim */
		glut_map = &vdd->br_info.glut_offset_night_dim;
	else if (cur_rr == 96 || cur_rr == 48)
		glut_map = &vdd->br_info.glut_offset_48hs;
	else
		glut_map = NULL;

	if (glut_map) {
		if (vdd->br_info.glut_00_val) {
			memset(&cmd->txbuf[i], 0x00, glut_map->col_size);
		} else {
			for (j = 0; j < glut_map->col_size; j++)
				cmd->txbuf[i + j] = glut_map->cmds[bl_lvl][j];
		}

		LCD_DEBUG(vdd, "bl_lvl: %d, cur_rr: %d, i: %d col_size : %d, night_dim : %d\n", bl_lvl, cur_rr, i, glut_map->col_size, vdd->night_dim);
	} else {
		cmd->skip_by_cond = true;
	}

	return 0;

err_skip:
	cmd->skip_by_cond = true;
	return -EINVAL;
}

static void ss_set_night_dim(struct samsung_display_driver_data *vdd, int val)
{
	ss_brightness_dcs(vdd, USE_CURRENT_BL_LEVEL, BACKLIGHT_NORMAL);
	return;
}

void DM3_S6E3HAE_AMB681AZ01_WQHD_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "%s\n", ss_get_panel_name(vdd));

	vdd->panel_state = PANEL_PWR_OFF;	/* default OFF */

	vdd->panel_func.samsung_panel_on_pre = samsung_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = samsung_panel_on_post;
	vdd->panel_func.samsung_display_on_post = samsung_display_on_post;

	vdd->panel_func.samsung_panel_revision = ss_panel_revision;
	vdd->panel_func.samsung_module_info_read = ss_module_info_read;
	vdd->panel_func.samsung_ddi_id_read = ss_ddi_id_read;
	vdd->panel_func.samsung_octa_id_read = ss_octa_id_read;

	vdd->copr.panel_init = ss_copr_panel_init;

	/* Brightness */
	vdd->panel_func.pre_brightness = ss_pre_brightness;
	vdd->panel_func.pre_hmt_brightness = ss_pre_hmt_brightness;
	vdd->panel_func.pre_lpm_brightness = ss_pre_lpm_brightness;
	vdd->br_info.common_br.bl_level = MAX_BL_PF_LEVEL;	/* default brightness */
	vdd->panel_lpm.lpm_bl_level = LPM_2NIT;

	vdd->br_info.acl_status = 1; /* ACL default ON */
	vdd->br_info.gradual_acl_val = 1; /* ACL default status in acl on */
	vdd->br_info.temperature = 20;

	/* mdnie */
	vdd->mdnie.support_mdnie = true;
	vdd->mdnie.support_trans_dimming = true;
	vdd->mdnie.mdnie_tune_size[0] = sizeof(BYPASS_MDNIE_1);
	vdd->mdnie.mdnie_tune_size[1] = sizeof(BYPASS_MDNIE_2);
	vdd->mdnie.mdnie_tune_size[2] = sizeof(BYPASS_MDNIE_3);
	dsi_update_mdnie_data(vdd);

	vdd->panel_func.ecc_read = ss_ecc_read;
	vdd->panel_func.ssr_read = ss_ssr_read;

	vdd->self_disp.factory_support = true;
	vdd->self_disp.init = self_display_init_HAE;
	vdd->self_disp.data_init = ss_self_display_data_init;

	vdd->mafpc.init = ss_mafpc_init_HAE;
	vdd->mafpc.data_init = ss_mafpc_data_init;

	/* Gamma compensation (Gamma Offset) */
	vdd->panel_func.samsung_gm2_gamma_comp_init = ss_gm2_gamma_comp_init;
	vdd->panel_func.samsung_print_gamma_comp = ss_print_gamma_comp;
	vdd->panel_func.read_flash = ss_read_flash;

	/* VRR */
	vdd->panel_func.samsung_lfd_get_base_val = ss_update_base_lfd_val;
	vdd->panel_func.post_vrr = ss_post_vrr;
	ss_vrr_init(&vdd->vrr);

	/* early te*/
	vdd->early_te = false;
	vdd->check_early_te = 0;

	vdd->panel_func.samsung_check_support_mode = ss_check_support_mode;

	/* night dim */
	vdd->panel_func.set_night_dim = ss_set_night_dim;

	register_op_sym_cb(vdd, "GLUT", update_glut, true);
	register_op_sym_cb(vdd, "GLUT_ENABLE", update_glut_enable, true);
	register_op_sym_cb(vdd, "AOR", update_aor_S6E3HAE_AMB681AZ01, true);
	register_op_sym_cb(vdd, "ANALOG_OFFSET_1",
			update_analog1_DM3_S6E3HAE_AMB681AZ01, true);
	register_op_sym_cb(vdd, "ANALOG_OFFSET_2",
			update_analog2_DM3_S6E3HAE_AMB681AZ01, true);
	register_op_sym_cb(vdd, "ANALOG_OFFSET_3",
			update_analog3_DM3_S6E3HAE_AMB681AZ01, true);

	update_glut_map(vdd);
}
