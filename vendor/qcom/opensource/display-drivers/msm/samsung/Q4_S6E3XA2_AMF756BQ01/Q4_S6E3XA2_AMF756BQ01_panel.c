/*
 * =================================================================
 *
 *
 *	Description:  samsung display panel file
 *
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
#include "Q4_S6E3XA2_AMF756BQ01_panel.h"
#include "Q4_S6E3XA2_AMF756BQ01_mdnie.h"

static bool is_umc_panel;
static void ss_read_gamma(struct samsung_display_driver_data *vdd);

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx", __func__, (size_t)vdd);
		return false;
	}

	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);
	ss_panel_attach_set(vdd, true);

	return true;
}

static int samsung_display_on_pre(struct samsung_display_driver_data *vdd)
{
	/*
	 * self mask is enabled from bootloader.
	 * so skip self mask setting during splash booting.
	 */
	if (vdd->self_disp.need_to_enable) {
		if (!vdd->samsung_splash_enabled) {
			if (vdd->self_disp.self_mask_img_write)
				vdd->self_disp.self_mask_img_write(vdd);
		} else {
			LCD_INFO(vdd, "samsung splash enabled.. skip image write\n");
		}

		if (vdd->self_disp.self_mask_on)
			vdd->self_disp.self_mask_on(vdd, true);

		vdd->self_disp.need_to_enable = false;
	}

	return 0;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
#if 1
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

	if (vdd->self_disp.self_mask_on)
		vdd->self_disp.self_mask_on(vdd, true);

	if (is_umc_panel)
		ss_send_cmd(vdd, TX_UMC_IP_OFF_TIMING);
#else
	if (!vdd->samsung_splash_enabled && vdd->self_disp.is_support) {
		vdd->self_disp.need_to_enable = true;
		LCD_INFO(vdd, "Need to enable self mask\n");
	}
#endif
	/* mafpc */
	if (vdd->mafpc.is_support) {
		// delay the mafpc operation to frame pre to reduce display on time.
		vdd->mafpc.need_to_write = true;
		LCD_INFO(vdd, "Need to write mafpc image data to DDI\n");

	}

	return true;
}

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *cmds = NULL;
	char d_ic = (ss_panel_id2_get(vdd) & 0xC0);
	int idx;

	if (vdd->manufacture_id_dsi == PBA_ID)
		ss_panel_attach_set(vdd, false);
	else
		ss_panel_attach_set(vdd, true);

	switch (ss_panel_rev_get(vdd)) {
	case 0x00:
	case 0x01:
		vdd->panel_revision = 'A';	/* 0x01 */
		break;
	case 0x02:
		vdd->panel_revision = 'B';	/* 0x02 */
		break;
	case 0x03:
		vdd->panel_revision = 'C';	/* 0x02 */
		break;
	default:
		vdd->panel_revision = 'C';
		LCD_ERR(vdd, "Invalid panel_rev(default rev : %c)\n",
				vdd->panel_revision);
		break;
	}

	vdd->panel_revision -= 'A';

	/* change some values for UMC D-IC panel */
	if (d_ic == 0x00) {	/* 0x00 : S.LSI*/
		vdd->gct.valid_checksum[0] = 0x72;
		vdd->gct.valid_checksum[1] = 0x0A;
		vdd->gct.valid_checksum[2] = vdd->gct.valid_checksum[0];
		vdd->gct.valid_checksum[3] = vdd->gct.valid_checksum[1];

		vdd->dsc_crc_pass_val[0] = 0x48;
		vdd->dsc_crc_pass_val[1] = 0x65;

		is_umc_panel = false;
	} else {			/* 0x40 : UMC */
		vdd->gct.valid_checksum[0] = 0x6E;
		vdd->gct.valid_checksum[1] = 0x0E;
		vdd->gct.valid_checksum[2] = vdd->gct.valid_checksum[0];
		vdd->gct.valid_checksum[3] = vdd->gct.valid_checksum[1];

		vdd->dsc_crc_pass_val[0] = 0x2A;
		vdd->dsc_crc_pass_val[1] = 0x66;

		/* GCT */
		cmds = ss_get_cmds(vdd, TX_GCT_HV);
		if (IS_ERR_OR_NULL(cmds)) {
	        LCD_ERR(vdd, "Invalid cmds HV\n");
	    } else {
			/* delete B0 26 F4 cmds */
			idx = ss_get_cmd_idx(cmds, 0x26, 0xF4);
			if (idx > 0) {
				cmds->cmds[idx].ss_txbuf[0] = 0x00;
				cmds->cmds[idx].ss_txbuf[1] = 0x00;
				cmds->cmds[idx-1].ss_txbuf[0] = 0x00;
				cmds->cmds[idx-1].ss_txbuf[1] = 0x00;
				cmds->cmds[idx-1].ss_txbuf[2] = 0x00;
				cmds->cmds[idx-1].ss_txbuf[3] = 0x00;

			}
	    }

		cmds = ss_get_cmds(vdd, TX_GCT_LV);
		if (IS_ERR_OR_NULL(cmds)) {
	        LCD_ERR(vdd, "Invalid cmds LV\n");
	    } else {
			/* delete B0 26 F4 cmds */
			idx = ss_get_cmd_idx(cmds, 0x26, 0xF4);
			if (idx > 0) {
				cmds->cmds[idx].ss_txbuf[0] = 0x00;
				cmds->cmds[idx].ss_txbuf[1] = 0x00;
				cmds->cmds[idx-1].ss_txbuf[0] = 0x00;
				cmds->cmds[idx-1].ss_txbuf[1] = 0x00;
				cmds->cmds[idx-1].ss_txbuf[2] = 0x00;
				cmds->cmds[idx-1].ss_txbuf[3] = 0x00;

			}
	    }

		/* ABC */
		cmds = ss_get_cmds(vdd, TX_MAFPC_SET_PRE2);
		if (IS_ERR_OR_NULL(cmds)) {
	        LCD_ERR(vdd, "Invalid cmds mafpc pre2\n");
	    } else {
			idx = ss_get_cmd_idx(cmds, 0x64, 0xFE);
			if (idx > 0) {
				cmds->cmds[idx-1].ss_txbuf[2] = 0x65;
			}
	    }

		is_umc_panel = true;
	}

	LCD_INFO(vdd, "panel_revision = %c %d , d_ic (0x%X) is_umc_panel (%d)\n",
					vdd->panel_revision + 'A', vdd->panel_revision, d_ic, is_umc_panel);

	return (vdd->panel_revision + 'A');
}

enum VRR_CMD_RR {
	VRR_10HS = 0,
	VRR_24HS,
	VRR_30HS,
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
		vrr_id = VRR_48HS;
		break;
	case 60:
		vrr_id = VRR_60HS;
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

static enum VRR_CMD_RR ss_get_vrr_mode_base(struct samsung_display_driver_data *vdd)
{
	enum VRR_CMD_RR vrr_base;
	int cur_rr = vdd->vrr.cur_refresh_rate;

	switch (cur_rr) {
	case 96:
	case 48:
		vrr_base = VRR_96HS;
		break;
	case 120:
	case 60:
	case 30:
	case 24:
	case 10:
		vrr_base = VRR_120HS;
		break;
	default:
		LCD_ERR(vdd, "invalid refresh rate (%d), set default 120HS..\n", cur_rr);
		vrr_base = VRR_120HS;
		break;
	}

	return vrr_base;
}

static u8 IRC_VAL[IRC_MAX_MODE][40] = {
	{0x27, 0xE3, 0xFC, 0x03, 0x00, 0x80, 0x41, 0xFF, 0x10, 0xF2,
	 0xFF, 0x10, 0xFF, 0x37, 0x5E, 0x74, 0x04, 0x04, 0x04, 0x13,
	 0x13, 0x13, 0x1A, 0x1A, 0x1A, 0x1F, 0x1F, 0x1F, 0x21, 0x21,
	 0x21, 0x36, 0x98, 0x22, 0x48, 0x04, 0x80, 0x00, 0x00, 0x22
	}, /* Moderato */
	{0x27, 0xA3, 0xFC, 0x03, 0xD4, 0x80, 0x41, 0xFF, 0x10, 0x80,
	 0xFF, 0x10, 0xFF, 0x37, 0x5E, 0x74, 0x04, 0x04, 0x04, 0x13,
	 0x13, 0x13, 0x1A, 0x1A, 0x1A, 0x1F, 0x1F, 0x1F, 0x21, 0x21,
	 0x21, 0x36, 0x98, 0x22, 0x48, 0x04, 0x80, 0x00, 0x00, 0x22
	}, /* Flat */
};

static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2_normal(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	struct vrr_info *vrr = &vdd->vrr;
	int idx = 0;
	int smooth_frame, smooth_idx = 0;
	int tset;
	bool dim_off = false;
	int irc_mode = 0;
	bool first_high_level = false;
	int base_rr;
	bool normal_hbm_change = false;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_NORMAL);
	base_rr = ss_get_vrr_mode_base(vdd);

	/* ELVSS Temp comp */
	tset =  vdd->br_info.temperature > 0 ?  vdd->br_info.temperature : (char)(BIT(7) | (-1 * vdd->br_info.temperature));
	idx = ss_get_cmd_idx(pcmds, 0x09, 0xB1);
	pcmds->cmds[idx].ss_txbuf[1] = tset;

	/* WRDISBV */
	idx = ss_get_cmd_idx(pcmds, 0x00, 0x51);
	pcmds->cmds[idx].ss_txbuf[1] = (vdd->br_info.common_br.gm2_wrdisbv & 0xFF00) >> 8;
	pcmds->cmds[idx].ss_txbuf[2] = vdd->br_info.common_br.gm2_wrdisbv & 0xFF;

	if ((vdd->br_info.common_br.prev_bl_level <= MAX_BL_PF_LEVEL && vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL) ||
		(vdd->br_info.common_br.prev_bl_level > MAX_BL_PF_LEVEL && vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL)) {
		normal_hbm_change = true;
		LCD_DEBUG(vdd, "NORMAL<>HBM change, %d -> %d\n", vdd->br_info.common_br.prev_bl_level, vdd->br_info.common_br.bl_level);
	}

	if (!vdd->br_info.common_br.prev_bl_level && vdd->br_info.common_br.bl_level && vdd->display_on)
		first_high_level = true;

	/* smooth */
	if (vrr->cur_refresh_rate == 48 || vrr->cur_refresh_rate == 96 || first_high_level || !vdd->display_on) {
		dim_off = true;
		smooth_frame = 0;
	} else {
		dim_off = false;
		smooth_frame = 15;
	}

	smooth_idx = ss_get_cmd_idx(pcmds, 0x0D, 0x66);
	pcmds->cmds[smooth_idx].ss_txbuf[1] = dim_off ? (normal_hbm_change ? 0x20 : 0x20) : 0x60;

	smooth_idx = ss_get_cmd_idx(pcmds, 0x0E, 0x66);
	pcmds->cmds[smooth_idx].ss_txbuf[1] = smooth_frame;

	smooth_idx = ss_get_cmd_idx(pcmds, 0x00, 0x53);
	pcmds->cmds[smooth_idx].ss_txbuf[1] = dim_off ? 0x20 : 0x28;

	/* IRC mode*/
	irc_mode = vdd->br_info.common_br.irc_mode;

	idx = ss_get_cmd_idx(pcmds, 0x156, 0x63);
	if (idx > 0) {
		if (irc_mode < IRC_MAX_MODE) {
			memcpy(&pcmds->cmds[idx].ss_txbuf[1], IRC_VAL[irc_mode], sizeof(IRC_VAL[0]) / sizeof(u8));
		}
	}

//	LCD_INFO(vdd, "NORMAL : cd_idx [%d] cur_md [%d] base [%d] smooth_frame [%d]\n",
//		vdd->br_info.common_br.cd_idx, cur_md, cur_md_base, smooth_frame);

	return pcmds;
}

static void ss_read_flash_sysfs(struct samsung_display_driver_data *vdd, u32 raddr, u32 rsize, u8 *rbuf)
{
	char showbuf[256];
	int i, pos = 0;
	u32 fsize = 0xC00000;

	if (rsize > 0x100) {
		LCD_ERR(vdd, "rsize(%x) is not supported..\n", rsize);
		return;
	}

	ss_send_cmd(vdd, TX_POC_ENABLE);
	ss_send_cmd(vdd, TX_FLASH_GAMMA_PRE1);

	fsize |= rsize;
	flash_read_bytes(vdd, raddr, fsize, rsize, rbuf);

	ss_send_cmd(vdd, TX_FLASH_GAMMA_POST);
	ss_send_cmd(vdd, TX_POC_DISABLE);

	for (i = 0; i < rsize; i++)
		pos += scnprintf(showbuf + pos, 256 - pos, "%02x ", rbuf[i]);
	LCD_INFO(vdd, "buf : %s\n", showbuf);

	return;
}


#define FLASH_TEST_MODE_NUM     (1)
static int ss_test_ddi_flash_check(struct samsung_display_driver_data *vdd, char *buf)
{
	int len = 0;
	int ret = 0;
	char FLASH_LOADING[1];

	ss_panel_data_read(vdd, RX_FLASH_LOADING_CHECK, FLASH_LOADING, LEVEL1_KEY);

	if ((int)FLASH_LOADING[0] == 0)
		ret = 1;

	len += sprintf(buf + len, "%d\n", FLASH_TEST_MODE_NUM);
	len += sprintf(buf + len, "%d %02x000000\n",
			ret,
			(int)FLASH_LOADING[0]);

	return len;
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
		max_div_def = 1;
		min_div_def = 30;
		min_div_lowest = 30;
		goto done;
	}

	mngr = &vrr->lfd.lfd_mngr[LFD_CLIENT_DISP];

	vrr_id = ss_get_vrr_id(vdd);

	switch (vrr_id) {
	case VRR_10HS:
		base_rr = 120;
		max_div_def = 12; // 10hz
		min_div_def = min_div_lowest = 120; // 1hz
		break;
	case VRR_24HS:
		base_rr = 120;
		max_div_def = 5; // 24hz
		min_div_def = min_div_lowest = 120; // 1hz
		break;
	case VRR_30HS:
		base_rr = 120;
		max_div_def = 4; // 30hz
		min_div_def = min_div_lowest = 120; // 1hz
		break;
	case VRR_48HS:
		base_rr = 96;
		max_div_def = 2; // 48hz
		min_div_def = 96; // 1hz
		min_div_lowest = 96; // 1hz
		break;
	case VRR_60HS:
		base_rr = 120;
		max_div_def = 2; // 60hz
		min_div_def = 120; // 1hz
		min_div_lowest = 120; // 1hz
		break;
	case VRR_96HS:
		base_rr = 96;
		max_div_def = 1; // 96hz
		min_div_def = min_div_lowest = 96; // 1hz
		break;
	case VRR_120HS:
		base_rr = 120;
		max_div_def = 1; // 120hz
		min_div_def = min_div_lowest = 120; // 1hz
		break;
	default:
		LCD_ERR(vdd, "invalid vrr_id\n");
		base_rr = 120;
		max_div_def = 1; // 120hz
		min_div_def = min_div_lowest = 120; // 1hz
		break;
	}

	/* HBM 1Hz */
	if (mngr->scalability[LFD_SCOPE_NORMAL] == LFD_FUNC_SCALABILITY6) {
		min_div_def = min_div_lowest;
	}

done:
	lfd_base->base_rr = base_rr;
	lfd_base->max_div_def = max_div_def;
	lfd_base->min_div_def = min_div_def;
	lfd_base->min_div_lowest = min_div_lowest;
	lfd_base->fix_div_def = 1; // LFD MAX/MIN 120hz fix

	vrr->lfd.base_rr = base_rr;

	LCD_DEBUG(vdd, "LFD(%s): base_rr: %uhz, def: %uhz(%u)~%uhz(%u), lowest: %uhz(%u)\n",
			lfd_scope_name[scope],
			base_rr,
			DIV_ROUND_UP(base_rr, min_div_def), min_div_def,
			DIV_ROUND_UP(base_rr, max_div_def), max_div_def,
			DIV_ROUND_UP(base_rr, min_div_lowest), min_div_lowest);

	return 0;
}


/* FRAME INSERSION : BDh 9th ~ 11th */
enum FRAME_INSERTIOIN_CASE {
	CASE1 = 0,
	CASE2,
	CASE3,
	CASE4,
	CASE5,
	CASE6,
	CASE7,
	CASE8,
	CASE_MAX,
};

static u8 FRAME_INSERT_CMD[CASE_MAX][3] = {
	{0x10, 0x00, 0x01},	/* CASE1 */
	{0x14, 0x00, 0x01},	/* CASE2 */
	{0x00, 0x00, 0x02},	/* CASE3 */
	{0xEF, 0x00, 0x01},	/* CASE4 */
	{0x00, 0x00, 0x10},	/* CASE5 */
	{0x00, 0x00, 0x1E},	/* CASE6 */
	{0x00, 0x00, 0x26},	/* CASE7 */
	{0x00, 0x00, 0x1E},	/* CASE8 */
};

static int ss_get_frame_insert_cmd(struct samsung_display_driver_data *vdd,
	u8 *out_cmd, struct vrr_info *vrr, u32 min_div, u32 max_div)
{
	u32 min_freq;
	u32 max_freq;
	int case_n = 0;
	int base_rr = vrr->lfd.base_rr;
	int phs = vrr->cur_phs_mode;

	min_freq = DIV_ROUND_UP(base_rr, min_div);
	max_freq = DIV_ROUND_UP(base_rr, max_div);

	if (phs) {
		case_n = CASE2;
	} else {
		if (min_freq > 48)
			case_n = CASE1;
		else if ((max_freq > 60) && (min_freq > 1))
			case_n = CASE2;
		else if ((max_freq > 30) && (min_freq > 1))
			case_n = CASE3;
		else if ((max_freq > 60) && (min_freq == 1))
			case_n = CASE4;
		else if ((max_freq > 30) && (min_freq == 1))
			case_n = CASE5;
		else if (max_freq == 30)
			case_n = CASE6;
		else if (max_freq == 24)
			case_n = CASE7;
		else if (max_freq <= 10)
			case_n = CASE8;
		else
			case_n = CASE1;
	}

	memcpy(out_cmd, FRAME_INSERT_CMD[case_n], sizeof(FRAME_INSERT_CMD[0]) / sizeof(u8));

	LCD_DEBUG(vdd, "frame insert [case %d]: %02X %02X %02X (LFD %uhz~%uhz)\n", case_n + 1,
			out_cmd[0], out_cmd[1], out_cmd[2], min_freq, max_freq);

	return 0;
}

static void get_lfd_cmds(struct samsung_display_driver_data *vdd, int fps, u8 *out_cmd)
{
	if (fps >= 96) {
		out_cmd[0] = 0x00;
	} else if (fps >= 48) {
		out_cmd[0] = 0x01;
	} else if (fps >= 32) {
		out_cmd[0] = 0x02;
	} else if (fps == 30) {
		out_cmd[0] = 0x03;
	} else if (fps == 24) {
		out_cmd[0] = 0x04;
	} else if (fps == 10) {
		out_cmd[0] = 0x0B;
	} else if (fps == 1) {
		out_cmd[0] = 0x77;
	} else {
		LCD_ERR(vdd, "No lfd case..\n");
		out_cmd[0] = 0x00;
	}

	LCD_DEBUG(vdd, "fps (%d), 0x%02X 0x%02X\n", fps, out_cmd[0], out_cmd[1]);
	return;
}

static u8 AOR_96_48HZ[6] = {0x01, 0x90, 0x01, 0x90, 0x01, 0x90};
static u8 AOR_120HZ[6] = {0x01, 0x40, 0x01, 0x40, 0x01, 0x40};

static u8 VFP_96_48HZ[2] = {0x02, 0x90};
static u8 VFP_120HZ[2] = {0x00, 0x9C};

static struct dsi_panel_cmd_set *ss_vrr(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct dsi_panel_cmd_set  *vrr_cmds = ss_get_cmds(vdd, TX_VRR);
	bool is_hbm = vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL ? true : false;
	struct vrr_info *vrr = &vdd->vrr;
	int cur_rr, base_rr;
	bool cur_hs;
	enum VRR_CMD_RR vrr_id;
	u32 min_div, max_div;
	enum LFD_SCOPE_ID scope;
	u8 cmd_frame_insert[3];
	u8 cmd_lfd[1];
	int idx;

	if (SS_IS_CMDS_NULL(vrr_cmds)) {
		LCD_INFO(vdd, "no vrr cmds\n");
		return NULL;
	}

	if (panel && panel->cur_mode) {
		LCD_DEBUG(vdd, "VRR: cur_mode: %dx%d@%d%s, is_hbm: %d rev: %d\n",
				panel->cur_mode->timing.h_active,
				panel->cur_mode->timing.v_active,
				panel->cur_mode->timing.refresh_rate,
				panel->cur_mode->timing.sot_hs_mode ? "HS" : "NM",
				is_hbm, vdd->panel_revision);

		if (panel->cur_mode->timing.refresh_rate != vdd->vrr.adjusted_refresh_rate ||
				panel->cur_mode->timing.sot_hs_mode != vdd->vrr.adjusted_sot_hs_mode)
			LCD_ERR(vdd, "VRR: unmatched RR mode (%dhz%s / %dhz%s)\n",
					panel->cur_mode->timing.refresh_rate,
					panel->cur_mode->timing.sot_hs_mode ? "HS" : "NM",
					vdd->vrr.adjusted_refresh_rate,
					vdd->vrr.adjusted_sot_hs_mode ? "HS" : "NM");
	}

	cur_rr = vrr->cur_refresh_rate;
	cur_hs = vrr->cur_sot_hs_mode;

	vrr_id = ss_get_vrr_id(vdd);
	base_rr = ss_get_vrr_mode_base(vdd);

	scope = LFD_SCOPE_NORMAL;
	if (ss_get_lfd_div(vdd, scope, &min_div, &max_div)) {
		LCD_ERR(vdd, "fail to get LFD divider.. set default 1..\n");
		max_div = min_div = 1;
	}
	vrr->lfd.min_div = min_div;
	vrr->lfd.max_div = max_div;

	/* AOR Setting */
	idx = ss_get_cmd_idx(vrr_cmds, 0xAE, 0x68);
	if (cur_rr == 96 || cur_rr == 48)
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], AOR_96_48HZ, sizeof(AOR_96_48HZ) / sizeof(u8));
	else
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], AOR_120HZ, sizeof(AOR_120HZ) / sizeof(u8));

	/* LFD MIN Setting */
	if (vrr->cur_phs_mode)
		get_lfd_cmds(vdd, 120, cmd_lfd);
	else
		get_lfd_cmds(vdd, DIV_ROUND_UP(vrr->lfd.base_rr, min_div), cmd_lfd);
	idx = ss_get_cmd_idx(vrr_cmds, 0x06, 0xBD);
	if (idx >= 0) {
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[2], cmd_lfd, sizeof(cmd_lfd));

		ss_get_frame_insert_cmd(vdd, cmd_frame_insert, vrr, min_div, max_div);
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[3], cmd_frame_insert, sizeof(cmd_frame_insert));
	}

	/* VFP Setting */
	idx = ss_get_cmd_idx(vrr_cmds, 0x06, 0xF2);
	if (cur_rr == 96 || cur_rr == 48)
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_96_48HZ, sizeof(VFP_96_48HZ) / sizeof(u8));
	else
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_120HZ, sizeof(VFP_120HZ) / sizeof(u8));

	idx = ss_get_cmd_idx(vrr_cmds, 0x0D, 0xF2);
	if (cur_rr == 96 || cur_rr == 48)
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_96_48HZ, sizeof(VFP_96_48HZ) / sizeof(u8));
	else
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[1], VFP_120HZ, sizeof(VFP_120HZ) / sizeof(u8));

	/* TE_FRAME_SEL */
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0xB9);
	if (cur_rr == 60 || cur_rr == 48) {
		vrr_cmds->cmds[idx].ss_txbuf[2] = 0x01;
	} else if (cur_rr == 30) {
		vrr_cmds->cmds[idx].ss_txbuf[2] = 0x03;
	} else if (cur_rr == 24) {
		vrr_cmds->cmds[idx].ss_txbuf[2] = 0x04;
	} else if (cur_rr == 10) {
		vrr_cmds->cmds[idx].ss_txbuf[2] = 0x0B;
	} else { // 120, 96
		vrr_cmds->cmds[idx].ss_txbuf[2] = 0x00;
	}

	/* LFD MAX Setting */
	if (vrr->cur_phs_mode)
		get_lfd_cmds(vdd, 120, cmd_lfd);
	else
		get_lfd_cmds(vdd, DIV_ROUND_UP(vrr->lfd.base_rr, max_div), cmd_lfd);
	idx = ss_get_cmd_idx(vrr_cmds, 0x00, 0x60);
	if (idx >= 0)
		memcpy(&vrr_cmds->cmds[idx].ss_txbuf[2], cmd_lfd, sizeof(cmd_lfd));

	LCD_INFO(vdd, "cur:%dx%d@%d%s, adj:%d%s, is_hbm:%d, LFD:%uhz(%u)~%uhz(%u), running_lfd(%d), base_rr(%d) rev:%c\n",
			panel->cur_mode->timing.h_active,
			panel->cur_mode->timing.v_active,
			cur_rr,
			cur_hs ? "HS" : "NM",
			vrr->adjusted_refresh_rate,
			vrr->adjusted_sot_hs_mode ? "HS" : "NM", is_hbm,
			DIV_ROUND_UP(vrr->lfd.base_rr, min_div), min_div,
			DIV_ROUND_UP(vrr->lfd.base_rr, max_div), max_div,
			vrr->lfd.running_lfd, base_rr, vdd->panel_revision + 'A');

	return vrr_cmds;
}

static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	char *read_buf;
	int i, len = 0;
	u8 temp[20];
	int ddi_id_len;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	pcmds = ss_get_cmds(vdd, RX_DDI_ID);

	/* Read mtp (D6h 1~5th) for CHIP ID */
	if (pcmds->count) {
		ddi_id_len = pcmds->cmds[0].msg.rx_len;
		read_buf = kzalloc(ddi_id_len, GFP_KERNEL);
		if (!read_buf) {
			LCD_ERR(vdd, "fail to kzalloc for read_buf\n");
			return false;
		}

		ss_panel_data_read(vdd, RX_DDI_ID, read_buf, LEVEL1_KEY);

		for (i = 0; i < ddi_id_len; i++)
			len += sprintf(temp + len, "%02x", read_buf[i]);
		len += sprintf(temp + len, "\n");

		vdd->ddi_id_dsi = kzalloc(len, GFP_KERNEL);
		if (!vdd->ddi_id_dsi)
			LCD_ERR(vdd, "fail to kzalloc for ddi_id_dsi\n");
		else {
			vdd->ddi_id_len = len;
			strlcat(vdd->ddi_id_dsi, temp, len);
			LCD_INFO(vdd, "[%d] %s\n", vdd->ddi_id_len, vdd->ddi_id_dsi);
		}
	} else {
		LCD_ERR(vdd, "DSI%d no ddi_id_rx_cmds cmds", vdd->ndx);
		return false;
	}

	kfree(read_buf);

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
	{0xff, 0x00, 0xfa, 0x00, 0xf9, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfc, 0x00, 0xff, 0x00}, /* Tune_2 */
	{0xf8, 0x00, 0xf7, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfd, 0x00, 0xf9, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xf8, 0x00, 0xfa, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfd, 0x00, 0xff, 0x00, 0xf8, 0x00}, /* Tune_7 */
	{0xfb, 0x00, 0xff, 0x00, 0xfc, 0x00}, /* Tune_8 */
	{0xf8, 0x00, 0xfd, 0x00, 0xff, 0x00}, /* Tune_9 */
};

static char coordinate_data_2[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xfb, 0x00, 0xf5, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfb, 0x00, 0xf5, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xfb, 0x00, 0xf5, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfb, 0x00, 0xf5, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xfb, 0x00, 0xf5, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xfb, 0x00, 0xf5, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xfb, 0x00, 0xf5, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xfb, 0x00, 0xf5, 0x00}, /* Tune_8 */
	{0xfc, 0x00, 0xfb, 0x00, 0xf5, 0x00}, /* Tune_9 */
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
		if (F2(x, y) > 0) {
			tune_number = 1;
		} else {
			tune_number = 2;
		}
	} else {
		if (F2(x, y) > 0) {
			tune_number = 4;
		} else {
			tune_number = 3;
		}
	}

	return tune_number;
}

static int mdnie_coordinate_x(int x, int y, int index)
{
	int result = 0;

	result = (coefficient[index][0] * x) + (coefficient[index][1] * y) + (((coefficient[index][2] * x + 512) >> 10) * y) + (coefficient[index][3] * 10000);

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

	result = (coefficient[index][4] * x) + (coefficient[index][5] * y) + (((coefficient[index][6] * x + 512) >> 10) * y) + (coefficient[index][7] * 10000);

	result = (result + 512) >> 10;

	if (result < 0)
		result = 0;
	if (result > 1024)
		result = 1024;

	return result;
}

static int ss_module_info_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	char *buf;
	int year, month, day;
	int hour, min;
	int x, y;
	int mdnie_tune_index = 0;
	int ret;
	char temp[50];
	int buf_len, len = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	pcmds = ss_get_cmds(vdd, RX_MODULE_INFO);

	if (pcmds->count) {
		buf_len = pcmds->cmds[0].msg.rx_len;

		buf = kzalloc(buf_len, GFP_KERNEL);
		if (!buf) {
			LCD_ERR(vdd, "fail to kzalloc for buf\n");
			return false;
		}

		ret = ss_panel_data_read(vdd, RX_MODULE_INFO, buf, LEVEL1_KEY);
		if (ret) {
			LCD_ERR(vdd, "fail to read module ID, ret: %d", ret);
			kfree(buf);
			return false;
		}

		/* Manufacture Date */

		year = buf[4] & 0xf0;
		year >>= 4;
		year += 2011; // 0 = 2011 year
		month = buf[4] & 0x0f;
		day = buf[5] & 0x1f;
		hour = buf[6] & 0x0f;
		min = buf[7] & 0x1f;

		vdd->manufacture_date_dsi = year * 10000 + month * 100 + day;
		vdd->manufacture_time_dsi = hour * 100 + min;

		LCD_INFO(vdd, "manufacture_date DSI%d = (%d%04d) - year(%d) month(%d) day(%d) hour(%d) min(%d)\n",
			vdd->ndx, vdd->manufacture_date_dsi, vdd->manufacture_time_dsi,
			year, month, day, hour, min);

		/* While Coordinates */

		vdd->mdnie.mdnie_x = buf[0] << 8 | buf[1];	/* X */
		vdd->mdnie.mdnie_y = buf[2] << 8 | buf[3];	/* Y */

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
			buf[4], buf[5], buf[6], buf[7], buf[8], buf[9], buf[10],
			buf[0], buf[1], buf[2], buf[3]);

		vdd->cell_id_dsi = kzalloc(len, GFP_KERNEL);
		if (!vdd->cell_id_dsi)
			LCD_ERR(vdd, "fail to kzalloc for cell_id_dsi\n");
		else {
			vdd->cell_id_len = len;
			strlcat(vdd->cell_id_dsi, temp, vdd->cell_id_len);
			LCD_INFO(vdd, "CELL ID : [%d] %s\n", vdd->cell_id_len, vdd->cell_id_dsi);
		}
	} else {
		LCD_ERR(vdd, "no module_info_rx_cmds cmds(%d)", vdd->panel_revision);
		return false;
	}

	kfree(buf);

	return true;
}


static int ss_elvss_read(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (C66h 0x17th) for grayspot */
	ss_panel_data_read(vdd, RX_ELVSS, vdd->br_info.common_br.elvss_value, LEVEL1_KEY);

	LCD_INFO(vdd, "elvss mtp : %0x\n", vdd->br_info.common_br.elvss_value[0]);

	return true;
}

static void ss_gray_spot(struct samsung_display_driver_data *vdd, int enable)
{
	struct dsi_panel_cmd_set *pcmds;
	int idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return;
	}

	pcmds = ss_get_cmds(vdd, TX_GRAY_SPOT_TEST_OFF);
	if (IS_ERR_OR_NULL(pcmds)) {
        LCD_ERR(vdd, "Invalid pcmds\n");
        return;
    }

	if (!enable) { /* grayspot off */
		/* restore ELVSS_CAL_OFFSET */
		idx = ss_get_cmd_idx(pcmds, 0x17, 0x66);
		pcmds->cmds[idx].ss_txbuf[1] = vdd->br_info.common_br.elvss_value[0];
	}
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
	mdnie_data->DSI_AFC = DSI_AFC;
	mdnie_data->DSI_AFC_ON = DSI_AFC_ON;
	mdnie_data->DSI_AFC_OFF = DSI_AFC_OFF;

	mdnie_data->mdnie_tune_value_dsi = mdnie_tune_value_dsi;
	mdnie_data->hmt_color_temperature_tune_value_dsi = hmt_color_temperature_tune_value_dsi;

	mdnie_data->hdr_tune_value_dsi = hdr_tune_value_dsi;

	mdnie_data->light_notification_tune_value_dsi = light_notification_tune_value_dsi;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data->dsi_bypass_mdnie_size = ARRAY_SIZE(BYPASS_MDNIE);
	mdnie_data->mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET;
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

	mdnie_data->dsi_adjust_ldu_table = adjust_ldu_data;
	mdnie_data->dsi_max_adjust_ldu = 6;
	mdnie_data->dsi_night_mode_table = night_mode_data;
	mdnie_data->dsi_max_night_mode_index = 102;
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

static int ss_ecc_read(struct samsung_display_driver_data *vdd)
{
	u8 ecc = 0;
	int enable = 0;

	ss_send_cmd_get_rx(vdd, RX_GCT_ECC, &ecc);
	LCD_INFO(vdd, "GCT ECC = 0x%02X\n", ecc);

	if (ecc == 0x00)
		enable = 1;
	else
		enable = 0;

	return enable;
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
	make_mass_self_display_img_cmds_XA2(vdd, TX_SELF_MASK_IMAGE, FLAG_SELF_MASK);

	if (vdd->is_factory_mode) {
		vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_buf = self_mask_img_crc_data;
		vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_size = ARRAY_SIZE(self_mask_img_crc_data);
		make_mass_self_display_img_cmds_XA2(vdd, TX_SELF_MASK_IMAGE_CRC, FLAG_SELF_MASK_CRC);
	}

	vdd->self_disp.is_initialized = true;

	return 1;
}

static int ss_mafpc_data_init(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->mafpc.is_support) {
		LCD_ERR(vdd, "mAFPC is not supported\n");
		return -EINVAL;
	}

	LCD_INFO(vdd, "mAFPC Panel Data init\n");

	vdd->mafpc.img_buf = mafpc_img_data;
	vdd->mafpc.img_size = ARRAY_SIZE(mafpc_img_data);

	if (vdd->mafpc.make_img_mass_cmds)
		vdd->mafpc.make_img_mass_cmds(vdd, vdd->mafpc.img_buf, vdd->mafpc.img_size, TX_MAFPC_IMAGE); /* Image Data */
	else if (vdd->mafpc.make_img_cmds)
		vdd->mafpc.make_img_cmds(vdd, vdd->mafpc.img_buf, vdd->mafpc.img_size, TX_MAFPC_IMAGE); /* Image Data */
	else {
		LCD_ERR(vdd, "Can not make mafpc image commands\n");
		return -EINVAL;
	}

	if (vdd->is_factory_mode) {
		/* CRC Check For Factory Mode */
		vdd->mafpc.crc_img_buf = mafpc_img_data_crc_check;
		vdd->mafpc.crc_img_size = ARRAY_SIZE(mafpc_img_data_crc_check);

		if (vdd->mafpc.make_img_mass_cmds)
			vdd->mafpc.make_img_mass_cmds(vdd, vdd->mafpc.crc_img_buf, vdd->mafpc.crc_img_size, TX_MAFPC_CRC_CHECK_IMAGE); /* CRC Check Image Data */
		else if (vdd->mafpc.make_img_cmds)
			vdd->mafpc.make_img_cmds(vdd, vdd->mafpc.crc_img_buf, vdd->mafpc.crc_img_size, TX_MAFPC_CRC_CHECK_IMAGE); /* CRC Check Image Data */
		else {
			LCD_ERR(vdd, "Can not make mafpc image commands\n");
			return -EINVAL;
		}
	}

	return true;
}

/* Please enable 'ss_copr_panel_init' in panel.c file to cal cd_avr for ABC(mafpc)*/
static void ss_copr_panel_init(struct samsung_display_driver_data *vdd)
{
	vdd->copr.ver = COPR_VER_5P0;
	vdd->copr.display_read = 0;
	ss_copr_init(vdd);
}

static int ss_octa_id_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	char *read_buf;
	int read_len;
	char temp[50];
	int len = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	pcmds = ss_get_cmds(vdd, RX_OCTA_ID);

	/* Read Panel Unique OCTA ID (C9h 2nd~21th) */
	if (pcmds->count) {
		read_len = pcmds->cmds[0].msg.rx_len;
		read_buf = kzalloc(read_len, GFP_KERNEL);
		if (!read_buf) {
			LCD_ERR(vdd, "fail to kzalloc for buf\n");
			return false;
		}

		ss_panel_data_read(vdd, RX_OCTA_ID, read_buf, LEVEL1_KEY);

		LCD_INFO(vdd, "octa id (read buf): %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			read_buf[0], read_buf[1], read_buf[2], read_buf[3],
			read_buf[4], read_buf[5], read_buf[6], read_buf[7],
			read_buf[8], read_buf[9], read_buf[10], read_buf[11],
			read_buf[12], read_buf[13], read_buf[14], read_buf[15],
			read_buf[16], read_buf[17],	read_buf[18], read_buf[19]);

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
		if (!vdd->octa_id_dsi)
			LCD_ERR(vdd, "fail to kzalloc for octa_id_dsi\n");
		else {
			vdd->octa_id_len = len;
			strlcat(vdd->octa_id_dsi, temp, vdd->octa_id_len);
			LCD_INFO(vdd, "octa id : [%d] %s \n", vdd->octa_id_len, vdd->octa_id_dsi);
		}
	} else {
		LCD_ERR(vdd, "DSI%d no octa_id_rx_cmds cmd\n", vdd->ndx);
		return false;
	}

	kfree(read_buf);

	return true;
}

static struct dsi_panel_cmd_set *ss_acl_on(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int idx;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_ACL_ON);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_ACL_ON..\n");
		return NULL;
	}

	idx = ss_get_cmd_idx(pcmds, 0x108, 0x68);
	if (vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL) {
		pcmds->cmds[idx].ss_txbuf[1] = 0x55;	/* 0x44 : 16Frame ACL OFF, 0x55 : 32Frame ACL ON */
		pcmds->cmds[idx].ss_txbuf[2] = 0x40;
		pcmds->cmds[idx].ss_txbuf[3] = 0xFF;	/* 0x80 0x67 : 60%, 0x40 0xFF : 50% */
	} else {
		pcmds->cmds[idx].ss_txbuf[1] = 0x55;	/* 0x44 : 16Frame ACL OFF, 0x55 : 32Frame ACL ON */
		pcmds->cmds[idx].ss_txbuf[2] = 0x40;
		pcmds->cmds[idx].ss_txbuf[3] = 0xFF;	/* 0x80 0x67 : 60%, 0x40 0xFF : 50% */
	}

	/* ACL DIM control */
	idx = ss_get_cmd_idx(pcmds, 0x116, 0x68);
	pcmds->cmds[idx].ss_txbuf[3] = 0x1F;		/* 0x00 0x00 : OFF, 0x00 0x1F : DIM 32frame */

	/* ACL 30% */
	idx = ss_get_cmd_idx(pcmds, 0x00, 0x55);
	if (vdd->br_info.gradual_acl_val == 2) {
		pcmds->cmds[idx].ss_txbuf[1] = 0x03;	/* 0x03 : ACL 30% */
	} else {
		if (vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL)
			if (vdd->br_info.gradual_acl_val == 3)
				pcmds->cmds[idx].ss_txbuf[1] = 0x02;	/* 0x02 : ACL 15% */
			else
				pcmds->cmds[idx].ss_txbuf[1] = 0x01;	/* 0x01 : ACL 8%, D.Lab Request(21.10.13) */
		else
			pcmds->cmds[idx].ss_txbuf[1] = 0x01;	/* 0x01 : ACL 8% */
	}

	LCD_DEBUG(vdd, "bl_level: %d, gradual_acl: %d, acl per: 0x%x", vdd->br_info.common_br.bl_level,
			vdd->br_info.gradual_acl_val, pcmds->cmds[idx].ss_txbuf[1]);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_acl_off(struct samsung_display_driver_data *vdd, int *level_key)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	LCD_DEBUG(vdd, "off\n");
	return ss_get_cmds(vdd, TX_ACL_OFF);
}

enum LPM_FPS {
	LPM_30HZ = 0,
	LPM_10HZ,
	LPM_1HZ,
	LPM_FPS_MAX,
};

static u8 LPM_FREQ[LPM_FPS_MAX][2] = {
	{0x00, 0x00},	/* 30hz */
	{0x00, 0x02},	/* 10hz */
	{0x00, 0x1D},	/* 1hz */
};

static u8 hlpm_aor[5][30] = {
	{0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40,
	 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40, 0x01, 0x40},
	{0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E,
	 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E, 0x04, 0x7E},
	{0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3,
	 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3, 0x06, 0xB3},
	{0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95,
	 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95, 0x07, 0x95},
	{0x07, 0xBB, 0x07, 0xBB, 0x07, 0xBB, 0x07, 0xBB, 0x07, 0xBB, 0x07, 0xBB, 0x07, 0xBB,
	 0x07, 0xBB, 0x07, 0xBB, 0x07, 0xBB, 0x07, 0xBB, 0x07, 0xBB, 0x07, 0xBB, 0x07, 0xBB, 0x07, 0xBB},
};

static void ss_set_panel_lpm_brightness(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *set = ss_get_cmds(vdd, TX_LPM_BL_CMD);
	struct dsi_panel_cmd_set *set_lpm_bl;
	int idx, hlpm_aor_idx;
	u32 min_div, max_div;
	int min_fps, max_fps;
	struct vrr_info *vrr = &vdd->vrr;

	if (SS_IS_CMDS_NULL(set)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_BL_CMD\n");
		return;
	}

	min_fps = max_fps = min_div = max_div = 0;

	if (ss_get_lfd_div(vdd, LFD_SCOPE_LPM, &min_div, &max_div)) {
		LCD_ERR(vdd, "fail to get LFD divider.. set default 1..\n");
		max_div = min_div = 1;
	}

	min_fps = DIV_ROUND_UP(vrr->lfd.base_rr, min_div);
	max_fps = DIV_ROUND_UP(vrr->lfd.base_rr, max_div);

	/* LFD */
	idx = ss_get_cmd_idx(set, 0x04, 0xBD);
	if (min_fps == 30)
		memcpy(&set->cmds[idx].ss_txbuf[1], LPM_FREQ[LPM_30HZ], sizeof(LPM_FREQ[0]) / sizeof(u8));
	else if (min_fps == 10)
		memcpy(&set->cmds[idx].ss_txbuf[1], LPM_FREQ[LPM_10HZ], sizeof(LPM_FREQ[0]) / sizeof(u8));
	else
		memcpy(&set->cmds[idx].ss_txbuf[1], LPM_FREQ[LPM_1HZ], sizeof(LPM_FREQ[0]) / sizeof(u8));

	/* HLPM AOR */
	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_60NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
		hlpm_aor_idx = 0;
		break;
	case LPM_30NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
		hlpm_aor_idx = 1;
		break;
	case LPM_10NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
		hlpm_aor_idx = 2;
		break;
	case LPM_2NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
		hlpm_aor_idx = 3;
	case LPM_1NIT:
	default:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_1NIT_CMD);
		hlpm_aor_idx = 4;
		break;
	}

	if (SS_IS_CMDS_NULL(set_lpm_bl)) {
		LCD_ERR(vdd, "No cmds for alpm_ctrl..\n");
		return;
	}

	if (vdd->panel_revision + 'A' <= 'A') {
		/* HLPM AOR setting */
		idx = ss_get_cmd_idx(set, 0xCD, 0x68);
		memcpy(&set->cmds[idx].ss_txbuf[1], hlpm_aor[hlpm_aor_idx], sizeof(hlpm_aor[hlpm_aor_idx]));

		/* HLPM Control */
		idx = ss_get_cmd_idx(set, 0x37, 0x68);
		memcpy(&set->cmds[idx].ss_txbuf[1], &set_lpm_bl->cmds->ss_txbuf[1],
				sizeof(char) * (set->cmds[idx].msg.tx_len - 1));
	} else {
		/* HLPM AOR setting : AOR fix */
		idx = ss_get_cmd_idx(set, 0xCD, 0x68);
		memcpy(&set->cmds[idx].ss_txbuf[1], hlpm_aor[0], sizeof(hlpm_aor[0]));

		/* DBV control */
		idx = ss_get_cmd_idx(set, 0x00, 0x51);
		memcpy(&set->cmds[idx].ss_txbuf[1], &set_lpm_bl->cmds->ss_txbuf[1],
				sizeof(char) * (set->cmds[idx].msg.tx_len - 1));
	}

	/* send lpm bl cmd */
	ss_send_cmd(vdd, TX_LPM_BL_CMD);

	LCD_INFO(vdd, "[Panel LPM] bl_level : %s\n",
			/* Check current brightness level */
			vdd->panel_lpm.lpm_bl_level == LPM_1NIT ? "1NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_2NIT ? "2NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_10NIT ? "10NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_30NIT ? "30NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_60NIT ? "60NIT" : "UNKNOWN");
}

enum LPMON_CMD_ID {
	LPMON_CMDID_CTRL = 1,
	LPMON_CMDID_LFD = 2,
	LPMON_CMDID_LPM_ON = 5,
};
/*
 * This function will update parameters for LPM
 */
static void ss_update_panel_lpm_ctrl_cmd(struct samsung_display_driver_data *vdd, int enable)
{
	struct dsi_panel_cmd_set *set_lpm = NULL;
	struct dsi_panel_cmd_set *set_lpm_bl = NULL;
	int idx, hlpm_aor_idx;
	int gm2_wrdisbv;
	u32 min_div, max_div;
	int min_fps, max_fps;
	struct vrr_info *vrr = &vdd->vrr;

	if (enable) {
		set_lpm = ss_get_cmds(vdd, TX_LPM_ON);
	} else {
		set_lpm = ss_get_cmds(vdd, TX_LPM_OFF);
	}

	if (SS_IS_CMDS_NULL(set_lpm)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_ON/OFF\n");
		return;
	}

	/* HLPM AOR */
	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_60NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
		hlpm_aor_idx = 0;
		gm2_wrdisbv = 243;
		break;
	case LPM_30NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
		hlpm_aor_idx = 1;
		gm2_wrdisbv = 120;
		break;
	case LPM_10NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
		hlpm_aor_idx = 2;
		gm2_wrdisbv = 42;
		break;
	case LPM_2NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
		hlpm_aor_idx = 3;
		gm2_wrdisbv = 8;
	case LPM_1NIT:
	default:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_1NIT_CMD);
		hlpm_aor_idx = 4;
		gm2_wrdisbv = 4;
		break;
	}

	if (enable) {
		min_fps = max_fps = min_div = max_div = 0;

		if (ss_get_lfd_div(vdd, LFD_SCOPE_LPM, &min_div, &max_div)) {
			LCD_ERR(vdd, "fail to get LFD divider.. set default 1..\n");
			max_div = min_div = 1;
		}

		min_fps = DIV_ROUND_UP(vrr->lfd.base_rr, min_div);
		max_fps = DIV_ROUND_UP(vrr->lfd.base_rr, max_div);

		/* LFD */
		idx = ss_get_cmd_idx(set_lpm, 0x04, 0xBD);
		if (min_fps == 30)
			memcpy(&set_lpm->cmds[idx].ss_txbuf[1], LPM_FREQ[LPM_30HZ], sizeof(LPM_FREQ[0]) / sizeof(u8));
		else if (min_fps == 10)
			memcpy(&set_lpm->cmds[idx].ss_txbuf[1], LPM_FREQ[LPM_10HZ], sizeof(LPM_FREQ[0]) / sizeof(u8));
		else
			memcpy(&set_lpm->cmds[idx].ss_txbuf[1], LPM_FREQ[LPM_1HZ], sizeof(LPM_FREQ[0]) / sizeof(u8));

		if (SS_IS_CMDS_NULL(set_lpm_bl)) {
			LCD_ERR(vdd, "No cmds for lpm_bl..\n");
			return;
		}

		if (vdd->panel_revision + 'A' <= 'A') {
			/* HLPM AOR setting */
			idx = ss_get_cmd_idx(set_lpm, 0xCD, 0x68);
			memcpy(&set_lpm->cmds[idx].ss_txbuf[1], hlpm_aor[hlpm_aor_idx], sizeof(hlpm_aor[hlpm_aor_idx]));

			/* HLPM Control */
			idx = ss_get_cmd_idx(set_lpm, 0x37, 0x68);
			memcpy(&set_lpm->cmds[idx].ss_txbuf[1], &set_lpm_bl->cmds->ss_txbuf[1],
					sizeof(char) * (set_lpm->cmds[idx].msg.tx_len - 1));
		} else {
			/* HLPM AOR setting : AOR fix */
			idx = ss_get_cmd_idx(set_lpm, 0xCD, 0x68);
			memcpy(&set_lpm->cmds[idx].ss_txbuf[1], hlpm_aor[0], sizeof(hlpm_aor[0]));

			/* DBV control */
			idx = ss_get_cmd_idx(set_lpm, 0x00, 0x51);
			memcpy(&set_lpm->cmds[idx].ss_txbuf[1], &set_lpm_bl->cmds->ss_txbuf[1],
					sizeof(char) * (set_lpm->cmds[idx].msg.tx_len - 1));
		}
	} else {
		idx = ss_get_cmd_idx(set_lpm, 0x00, 0x51);
		set_lpm->cmds[idx].ss_txbuf[1] = (gm2_wrdisbv & 0xFF00) >> 8;
		set_lpm->cmds[idx].ss_txbuf[2] = gm2_wrdisbv & 0xFF;
	}

	return;
}

static int ss_brightness_tot_level(struct samsung_display_driver_data *vdd)
{
	int tot_level_key = 0;

	/* HAB brightness setting requires F0h and FCh level key unlocked */
	/* DBV setting require TEST KEY3 (FCh) */
	tot_level_key = LEVEL0_KEY | LEVEL1_KEY | LEVEL2_KEY;

	return tot_level_key;
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

	/* Bootloader: 120HS */
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

#define FFC_REG	(0xC5)
static int ss_ffc(struct samsung_display_driver_data *vdd, int idx)
{
	struct dsi_panel_cmd_set *ffc_set;
	struct dsi_panel_cmd_set *dyn_ffc_set;
	struct dsi_panel_cmd_set *cmd_list[1];
	static int reg_list[1][2] = { {FFC_REG, -EINVAL} };
	int pos_ffc;

	LCD_INFO(vdd, "[DISPLAY_%d] +++ clk idx: %d, tx FFC\n", vdd->ndx, idx);

	ffc_set = ss_get_cmds(vdd, TX_FFC);
	dyn_ffc_set = ss_get_cmds(vdd, TX_DYNAMIC_FFC_SET);

	if (SS_IS_CMDS_NULL(ffc_set) || SS_IS_CMDS_NULL(dyn_ffc_set)) {
		LCD_ERR(vdd, "No cmds for TX_FFC..\n");
		return -EINVAL;
	}

	if (unlikely((reg_list[0][1] == -EINVAL))) {
		cmd_list[0] = ffc_set;
		ss_find_reg_offset(reg_list, cmd_list, ARRAY_SIZE(cmd_list));
	}

	pos_ffc = reg_list[0][1];
	if (pos_ffc == -EINVAL) {
		LCD_ERR(vdd, "fail to find FFC(C5h) offset in set\n");
		return -EINVAL;
	}

	memcpy(ffc_set->cmds[pos_ffc].ss_txbuf,
			dyn_ffc_set->cmds[idx].ss_txbuf,
			ffc_set->cmds[pos_ffc].msg.tx_len);

	ss_send_cmd(vdd, TX_FFC);

	LCD_INFO(vdd, "[DISPLAY_%d] --- clk idx: %d, tx FFC\n", vdd->ndx, idx);

	return 0;
}

#define FLASH_SECTOR_SIZE		4096

static int flash_sector_erase(struct samsung_display_driver_data *vdd, u32 start_addr, int sector_n)
{
	struct dsi_display *display = NULL;
	struct dsi_panel_cmd_set *poc_erase_sector_tx_cmds = NULL;
	int delay_us = 0;
	int image_size = 0;
	int type;
	int ret = 0;
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */
	int erase_size = FLASH_SECTOR_SIZE * sector_n;
	int i;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd\n");
		return -EINVAL;
	}

	display = GET_DSI_DISPLAY(vdd);
	if (IS_ERR_OR_NULL(display)) {
		LCD_ERR(vdd, "no display");
		return -EINVAL;
	}

	if (ss_is_panel_off(vdd)) {
		LCD_ERR(vdd, "Panel is not ready. Panel State(%d)\n", vdd->panel_state);
		return -EBUSY;
	}

	if (vdd->poc_driver.erase_sector_addr_idx[0] < 0) {
		LCD_ERR(vdd, "sector addr index is not implemented.. %d\n",
			vdd->poc_driver.erase_sector_addr_idx[0]);
		return -EINVAL;
	}

	poc_erase_sector_tx_cmds = ss_get_cmds(vdd, TX_POC_ERASE_SECTOR);
	if (SS_IS_CMDS_NULL(poc_erase_sector_tx_cmds)) {
		LCD_ERR(vdd, "No cmds for TX_POC_ERASE_SECTOR..\n");
		return -ENODEV;
	}

	image_size = vdd->poc_driver.image_size;
	delay_us = vdd->poc_driver.erase_delay_us;

	LCD_INFO(vdd, "[ERASE] 0x%X, %d sectoc, erase_size (%d), delay %dus\n",
		start_addr, sector_n, erase_size, delay_us);

	/* Enter exclusive mode */
	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.permit_frame_update = 1;
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 1);

	/* POC MODE ENABLE */
	ss_send_cmd(vdd, TX_POC_ENABLE);
	ss_send_cmd(vdd, TX_POC_PRE_ERASE_SECTOR);

	for (i = 0; i < sector_n; i++) {
		LCD_INFO(vdd, "[ERASE] 0x%X\n", start_addr);

		poc_erase_sector_tx_cmds->cmds[2].ss_txbuf[vdd->poc_driver.erase_sector_addr_idx[0]]
												= (start_addr & 0xFF0000) >> 16;
		poc_erase_sector_tx_cmds->cmds[2].ss_txbuf[vdd->poc_driver.erase_sector_addr_idx[1]]
												= (start_addr & 0x00FF00) >> 8;
		poc_erase_sector_tx_cmds->cmds[2].ss_txbuf[vdd->poc_driver.erase_sector_addr_idx[2]]
												= start_addr & 0x0000FF;
		vdd->debug_data->print_cmds = true;
		ss_send_cmd(vdd, TX_POC_ERASE_SECTOR);
		vdd->debug_data->print_cmds = false;

		usleep_range(delay_us, delay_us);

		start_addr += FLASH_SECTOR_SIZE;
	}

	ss_send_cmd(vdd, TX_POC_POST_ERASE_SECTOR);
	ss_send_cmd(vdd, TX_POC_DISABLE);

	/* exit exclusive mode*/
	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 0);
	vdd->exclusive_tx.permit_frame_update = 0;
	vdd->exclusive_tx.enable = 0;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);

	return ret;
}

static int poc_write(struct samsung_display_driver_data *vdd, u8 *data, u32 write_pos, u32 write_size)
{
	struct dsi_display *display = NULL;
	struct dsi_panel_cmd_set *write_cmd = NULL;
	struct dsi_panel_cmd_set *write_data_add = NULL;
	int pos, type, ret = 0;
	int last_pos, delay_us, image_size, loop_cnt, poc_w_size, start_addr;
	/*int tx_size, tx_size1, tx_size2;*/
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd\n");
		return -EINVAL;
	}

	display = GET_DSI_DISPLAY(vdd);
	if (IS_ERR_OR_NULL(display)) {
		LCD_ERR(vdd, "no display");
		return -EINVAL;
	}

	write_cmd = ss_get_cmds(vdd, TX_POC_WRITE_LOOP_256BYTE);
	if (SS_IS_CMDS_NULL(write_cmd)) {
		LCD_ERR(vdd, "no cmds for TX_POC_WRITE_LOOP_256BYTE..\n");
		return -EINVAL;
	}

	write_data_add = ss_get_cmds(vdd, TX_POC_WRITE_LOOP_DATA_ADD);
	if (SS_IS_CMDS_NULL(write_data_add)) {
		LCD_ERR(vdd, "no cmds for TX_POC_WRITE_LOOP_DATA_ADD..\n");
		return -EINVAL;
	}

	if (vdd->poc_driver.write_addr_idx[0] < 0) {
		LCD_ERR(vdd, "write addr index is not implemented.. %d\n",
			vdd->poc_driver.write_addr_idx[0]);
		return -EINVAL;
	}

	delay_us = vdd->poc_driver.write_delay_us; /* Panel dtsi set */
	image_size = vdd->poc_driver.image_size;
	last_pos = write_pos + write_size;
	poc_w_size = vdd->poc_driver.write_data_size;
	loop_cnt = vdd->poc_driver.write_loop_cnt;
	start_addr = vdd->poc_driver.start_addr;

	LCD_INFO(vdd, "[WRITE++] start_addr : %6d(%X) : write_pos : %6d, write_size : %6d, last_pos %6d, poc_w_sise : %d delay %dus\n",
		start_addr, start_addr, write_pos, write_size, last_pos, poc_w_size, delay_us);

	/* Enter exclusive mode */
	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.permit_frame_update = 1;
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 1);

	/* POC MODE ENABLE */
	ss_send_cmd(vdd, TX_POC_ENABLE);

	if (write_pos == start_addr) {
		LCD_INFO(vdd, "WRITE [TX_POC_PRE_WRITE]");
		ss_send_cmd(vdd, TX_POC_PRE_WRITE);
	}

	for (pos = write_pos; pos < last_pos; pos += poc_w_size) {
		if (!(pos % DEBUG_POC_CNT))
			LCD_INFO(vdd, "cur_write_pos : %d data : 0x%x\n", pos, data[pos - write_pos]);

		if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
			LCD_ERR(vdd, "cancel poc write by user\n");
			ret = -EIO;
			goto cancel_poc;
		}

		/* 128 Byte Write*/
		LCD_INFO(vdd, "pos[%X] buf_pos[%X] poc_w_size (%d)\n", pos, pos - write_pos, poc_w_size);

		/* data copy */
		memcpy(&write_cmd->cmds[0].ss_txbuf[1], &data[pos - write_pos], poc_w_size);

		ss_send_cmd(vdd, TX_POC_WRITE_LOOP_256BYTE);

		if (pos % loop_cnt == 0) {
			/*	Multi Data Address */
			write_data_add->cmds[2].ss_txbuf[vdd->poc_driver.write_addr_idx[0]]
											= (pos & 0xFF0000) >> 16;
			write_data_add->cmds[2].ss_txbuf[vdd->poc_driver.write_addr_idx[1]]
											= (pos & 0x00FF00) >> 8;
			write_data_add->cmds[2].ss_txbuf[vdd->poc_driver.write_addr_idx[2]]
											= (pos & 0x0000FF);
			ss_send_cmd(vdd, TX_POC_WRITE_LOOP_DATA_ADD);
		}

		usleep_range(delay_us, delay_us);
	}

cancel_poc:
	if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
		LCD_ERR(vdd, "cancel poc write by user\n");
		atomic_set(&vdd->poc_driver.cancel, 0);
		ret = -EIO;
	}

	if (pos == (start_addr + image_size) || ret == -EIO) {
		LCD_DEBUG(vdd, "WRITE_LOOP_END pos : %d \n", pos);
		ss_send_cmd(vdd, TX_POC_WRITE_LOOP_END);

		LCD_INFO(vdd, "WRITE [TX_POC_POST_WRITE] - image_size(%d) cur_write_pos(%d) ret(%d)\n", image_size, pos, ret);
		ss_send_cmd(vdd, TX_POC_POST_WRITE);
	}

	/* POC MODE DISABLE */
	ss_send_cmd(vdd, TX_POC_DISABLE);

	/* exit exclusive mode*/
	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 0);
	vdd->exclusive_tx.permit_frame_update = 0;
	vdd->exclusive_tx.enable = 0;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);

	LCD_INFO(vdd, "[WRITE--] start_addr : %6d(%X) : write_pos : %6d, write_size : %6d, last_pos %6d, poc_w_sise : %d delay %dus\n",
			start_addr, start_addr, write_pos, write_size, last_pos, poc_w_size, delay_us);

	return ret;
}

#define FLASH_READ_SIZE 128
static int poc_read(struct samsung_display_driver_data *vdd, u8 *buf, u32 read_pos, u32 read_size) /* read_size = 256 */
{
	struct dsi_display *display = NULL;
	struct dsi_panel_cmd_set *poc_read_tx_cmds = NULL;
	struct dsi_panel_cmd_set *poc_read_rx_cmds = NULL;
	int delay_us;
	int image_size, start_addr;
	u8 rx_buf[FLASH_READ_SIZE];
	int pos;
	int type;
	int ret = 0;
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */
	int temp;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd\n");
		return -EINVAL;
	}

	display = GET_DSI_DISPLAY(vdd);
	if (IS_ERR_OR_NULL(display)) {
		LCD_ERR(vdd, "no display");
		return -EINVAL;
	}

	poc_read_tx_cmds = ss_get_cmds(vdd, TX_POC_READ);
	if (SS_IS_CMDS_NULL(poc_read_tx_cmds)) {
		LCD_ERR(vdd, "no cmds for TX_POC_READ..\n");
		return -EINVAL;
	}

	poc_read_rx_cmds = ss_get_cmds(vdd, RX_POC_READ);
	if (SS_IS_CMDS_NULL(poc_read_rx_cmds)) {
		LCD_ERR(vdd, "no cmds for RX_POC_READ..\n");
		return -EINVAL;
	}

	if (vdd->poc_driver.read_addr_idx[0] < 0) {
		LCD_ERR(vdd, "read addr index is not implemented.. %d\n",
			vdd->poc_driver.read_addr_idx[0]);
		return -EINVAL;
	}

	delay_us = vdd->poc_driver.read_delay_us; /* Panel dtsi set */
	image_size = vdd->poc_driver.image_size;
	start_addr = vdd->poc_driver.start_addr;

	LCD_INFO(vdd, "[READ++] read_pos : start : %6d(%X), pos : %6d(%X), read_size : %6d, delay %dus\n",
		start_addr, start_addr, read_pos, read_pos, read_size, delay_us);

	/* Enter exclusive mode */
	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.permit_frame_update = 1;
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 1);
	ss_set_exclusive_tx_packet(vdd, RX_POC_READ, 1);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 1);

	/* For sending direct rx cmd  */
	poc_read_rx_cmds->state = DSI_CMD_SET_STATE_HS;

	/* POC MODE ENABLE */
	ss_send_cmd(vdd, TX_POC_ENABLE);

	/* Before read poc data, need to read mca (only for B0) */
	if (read_pos == vdd->poc_driver.start_addr) {
		LCD_INFO(vdd, "WRITE [TX_POC_PRE_READ]");
		ss_send_cmd(vdd, TX_POC_PRE_READ);
	}

	LCD_INFO(vdd, "%d(%x) ~ %d(%x) \n", read_pos, read_pos, read_pos + read_size, read_pos + read_size);

	for (pos = read_pos; pos < (read_pos + read_size); pos += FLASH_READ_SIZE) {
		if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
			LCD_ERR(vdd, "cancel poc read by user\n");
			ret = -EIO;
			goto cancel_poc;
		}

		poc_read_tx_cmds->cmds[0].ss_txbuf[vdd->poc_driver.read_addr_idx[0]]
									= (pos & 0xFF0000) >> 16;
		poc_read_tx_cmds->cmds[0].ss_txbuf[vdd->poc_driver.read_addr_idx[1]]
									= (pos & 0x00FF00) >> 8;
		poc_read_tx_cmds->cmds[0].ss_txbuf[vdd->poc_driver.read_addr_idx[2]]
									= pos & 0x0000FF;

		ss_send_cmd(vdd, TX_POC_READ);

		usleep_range(delay_us, delay_us);

		// gara is not supported.
		temp = vdd->gpara;
		vdd->gpara = false;
		ss_panel_data_read(vdd, RX_POC_READ, rx_buf, LEVEL_KEY_NONE);
		vdd->gpara = temp;

		LCD_INFO(vdd, "buf[%d][%x] copy\n", pos - read_pos, pos - read_pos);
		memcpy(&buf[pos - read_pos], rx_buf, FLASH_READ_SIZE);
	}

cancel_poc:
	if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
		LCD_ERR(vdd, "cancel poc read by user\n");
		atomic_set(&vdd->poc_driver.cancel, 0);
		ret = -EIO;
	}

	if (pos == (start_addr + image_size) || ret == -EIO) {
		LCD_INFO(vdd, "WRITE [TX_POC_POST_READ] - image_size(%d) cur_read_pos(%d) ret(%d)\n", image_size, pos, ret);
		ss_send_cmd(vdd, TX_POC_POST_READ);
	}

	/* POC MODE DISABLE */
	ss_send_cmd(vdd, TX_POC_DISABLE);

	/* Exit exclusive mode*/
	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 0);
	ss_set_exclusive_tx_packet(vdd, RX_POC_READ, 0);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 0);

	vdd->exclusive_tx.permit_frame_update = 0;
	vdd->exclusive_tx.enable = 0;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);

	LCD_INFO(vdd, "[READ--] read_pos : %6d, read_size : %6d, delay %dus\n", read_pos, read_size, delay_us);

	return ret;
}

static int ss_read_udc_transmittance_data(struct samsung_display_driver_data *vdd)
{
	int i = 0;
	u8 *read_buf;
	u32 fsize = 0xC00000 | vdd->udc.size;

	vdd->debug_data->print_cmds = true;

	read_buf = kzalloc(vdd->udc.size, GFP_KERNEL);

	ss_send_cmd(vdd, TX_POC_ENABLE);
	ss_send_cmd(vdd, TX_FLASH_GAMMA_PRE1);

	LCD_INFO(vdd, "[UDC transmittance] start_addr : %X, size : %X\n", vdd->udc.start_addr, vdd->udc.size);

	flash_read_bytes(vdd, vdd->udc.start_addr, fsize, vdd->udc.size, read_buf);
	memcpy(vdd->udc.data, read_buf, vdd->udc.size);

	ss_send_cmd(vdd, TX_FLASH_GAMMA_POST);
	ss_send_cmd(vdd, TX_POC_DISABLE);

	for (i = 0; i < vdd->udc.size/2; i++) {
		LCD_INFO(vdd, "[%d] %X.%X\n", i, vdd->udc.data[(i*2)+0], vdd->udc.data[(i*2)+1]);
	}

	vdd->udc.read_done = true;

	vdd->debug_data->print_cmds = false;

	return 0;
}

static int ss_flash_write(struct samsung_display_driver_data *vdd, int start_addr, int write_size, int total_size, u8 *buf)
{
	int i, r_size;
	struct dsi_panel_cmd_set *write_cmd = NULL;
	struct dsi_panel_cmd_set *write_data_add = NULL;
	int type;
	int wait_cnt = 1000;

	write_cmd = ss_get_cmds(vdd, TX_POC_WRITE_LOOP_256BYTE);
	if (SS_IS_CMDS_NULL(write_cmd)) {
		LCD_ERR(vdd, "no cmds for TX_POC_WRITE_LOOP_256BYTE..\n");
		return -EINVAL;
	}

	write_data_add = ss_get_cmds(vdd, TX_POC_WRITE_LOOP_DATA_ADD);
	if (SS_IS_CMDS_NULL(write_data_add)) {
		LCD_ERR(vdd, "no cmds for TX_POC_WRITE_LOOP_DATA_ADD..\n");
		return -EINVAL;
	}

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.permit_frame_update = 1;
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 1);
	for (type = TX_FLASH_GAMMA_PRE1; type < TX_FLASH_GAMMA_POST + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 1);

	ss_send_cmd(vdd, TX_POC_ENABLE);
	ss_send_cmd(vdd, TX_FLASH_GAMMA_PRE1);

	r_size = write_size;
	for (i = 0; i < total_size; i += r_size) {

		/* last trim */
		if (total_size - i < write_size)
			r_size = total_size - i;

		LCD_INFO(vdd, "[%X] r_addr [%X] r_size [%d][0x%X] \n", i, start_addr + i, r_size, r_size);

		/* set write size */
		write_cmd->cmds[0].msg.tx_len = r_size + 1;

		/* data copy */
		memcpy(&write_cmd->cmds[0].ss_txbuf[1], &buf[i], r_size);

		if (total_size < 10)
			LCD_INFO(vdd, "[0x%X] %02X %02X \n", start_addr + i, write_cmd->cmds[0].ss_txbuf[1], write_cmd->cmds[0].ss_txbuf[2]);
		else
			LCD_INFO(vdd, "[0x%X] %02X %02X %02X %02X %02X .. %02X %02X %02X %02X\n", start_addr + i,
				write_cmd->cmds[0].ss_txbuf[1], write_cmd->cmds[0].ss_txbuf[2], write_cmd->cmds[0].ss_txbuf[3], write_cmd->cmds[0].ss_txbuf[4], write_cmd->cmds[0].ss_txbuf[5],
				write_cmd->cmds[0].ss_txbuf[r_size-3], write_cmd->cmds[0].ss_txbuf[r_size-2], write_cmd->cmds[0].ss_txbuf[r_size-1], write_cmd->cmds[0].ss_txbuf[r_size]);

		vdd->debug_data->print_cmds = true;

		ss_send_cmd(vdd, TX_POC_WRITE_LOOP_256BYTE);

		/*	Multi Data Address */
		write_data_add->cmds[2].ss_txbuf[vdd->poc_driver.write_addr_idx[0]]
										= ((start_addr + i) & 0xFF0000) >> 16;
		write_data_add->cmds[2].ss_txbuf[vdd->poc_driver.write_addr_idx[1]]
										= ((start_addr + i) & 0x00FF00) >> 8;
		write_data_add->cmds[2].ss_txbuf[vdd->poc_driver.write_addr_idx[2]]
										= ((start_addr + i) & 0x0000FF);

		write_data_add->cmds[2].ss_txbuf[vdd->poc_driver.write_size_idx[0]]
										= ((r_size | 0xC000) & 0xFF00) >> 8;
		write_data_add->cmds[2].ss_txbuf[vdd->poc_driver.write_size_idx[1]]
										= ((r_size | 0xC000) & 0x00FF);

		ss_send_cmd(vdd, TX_POC_WRITE_LOOP_DATA_ADD);
		vdd->debug_data->print_cmds = false;

		usleep_range(40000, 40000);

	}

	ss_send_cmd(vdd, TX_FLASH_GAMMA_POST);
	ss_send_cmd(vdd, TX_POC_DISABLE);

	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 0);
	for (type = TX_FLASH_GAMMA_PRE1; type < TX_FLASH_GAMMA_POST + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 0);
	vdd->exclusive_tx.enable = 0;
	vdd->exclusive_tx.permit_frame_update = 0;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);

	return 0;
}

static void ss_flash_read(struct samsung_display_driver_data *vdd, u32 addr, int total_size, int read_size, u8* buf)
{
	int i = 0;
	u32 read_addr, target_addr, fsize, rsize;
	u8 *read_buf;
	int type;
	int wait_cnt = 1000;

	target_addr = addr + total_size;

	LCD_INFO(vdd, "addr : 0x%X ~ 0x%X, total_read_size %d ++\n", addr, target_addr - 1, total_size);

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.permit_frame_update = 1;
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);
	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 1);
	for (type = TX_FLASH_GAMMA_PRE1; type < TX_FLASH_GAMMA_POST + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 1);
	ss_set_exclusive_tx_packet(vdd, RX_POC_READ, 1);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 1);
	ss_set_exclusive_tx_packet(vdd, RX_FLASH_GAMMA, 1);

	/* 1. Read UDC Flash */
	ss_send_cmd(vdd, TX_POC_ENABLE);
	ss_send_cmd(vdd, TX_FLASH_GAMMA_PRE1);

	for (i = 0; i < total_size; i+= rsize) {
		read_addr = addr + i;

		if (read_addr + rsize >= target_addr)
			rsize = target_addr - read_addr;
		else
			rsize = read_size;

		fsize = 0xC00000 | rsize;
		read_buf = kzalloc(rsize, GFP_KERNEL);

		LCD_INFO(vdd, "[%d] addr : 0x%X, rsize : %d, fsize : 0x%X\n", i, read_addr, rsize, fsize);

		flash_read_bytes(vdd, read_addr, fsize, rsize, read_buf);

		/* keep the original flash data */
		memcpy(&buf[i], read_buf, rsize);

		if (rsize > 10)
			LCD_INFO(vdd, "[0x%X] %02X %02X %02X %02X %02X .. %02X %02X %02X\n", read_addr,
				read_buf[0], read_buf[1], read_buf[2], read_buf[3], read_buf[4], read_buf[rsize-3], read_buf[rsize-2], read_buf[rsize-1]);
		else
			LCD_INFO(vdd, "[0x%X] %02X  .. %02X\n", read_addr, read_buf[0], read_buf[rsize-1]);

	}

	ss_send_cmd(vdd, TX_FLASH_GAMMA_POST);
	ss_send_cmd(vdd, TX_POC_DISABLE);

	for (type = TX_POC_CMD_START; type < TX_POC_CMD_END + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 0);
	for (type = TX_FLASH_GAMMA_PRE1; type < TX_FLASH_GAMMA_POST + 1 ; type++)
		ss_set_exclusive_tx_packet(vdd, type, 0);
	ss_set_exclusive_tx_packet(vdd, RX_POC_READ, 0);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 0);
	ss_set_exclusive_tx_packet(vdd, RX_FLASH_GAMMA, 0);
	vdd->exclusive_tx.enable = 0;
	vdd->exclusive_tx.permit_frame_update = 0;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);

	LCD_INFO(vdd, "-- \n");
	return;
}

#define UDC_GAMMA_READ_SIZE		128
#define UDC_GAMMA_WRITE_SIZE	128
#define UDC_CS_ADDR1		0xCF244
#define UDC_CS_ADDR2		0xCF245
#define COMP_HS1_ADDR		0xCE110
#define COMP_HS2_ADDR		0xCEAA0

/*
 * 1. [READ] Read the UDC gamma data from 0xCD000 ~ 0xCF245 (8774 bytes)
 * 2. [C/S] Check CHECKSUM data
 * 3. [BACKUP] If it's never backed it up, backup the original UDC gamma data to the backup address
 */
static void ss_read_udc_gamma_data(struct samsung_display_driver_data *vdd)
{
	int target_addr;
	u8* backup_buf;
	int orig_data_sum = 0;
	int i = 0;

	target_addr = vdd->udc.gamma_start_addr + vdd->udc.gamma_size;

	/* 1. [READ] Read the UDC gamma data from 0xCD000 ~ 0xCF245 (8774 bytes) */
	LCD_INFO(vdd, "addr : 0x%X ~ 0x%X, total_read_size %d ++\n", vdd->udc.gamma_start_addr, target_addr, vdd->udc.gamma_size);
	ss_flash_read(vdd, vdd->udc.gamma_start_addr, vdd->udc.gamma_size, UDC_GAMMA_READ_SIZE, vdd->udc.gamma_data);

	/* 2. [C/S] Check CHECKSUM data */
	for (i = 0; i < vdd->udc.gamma_size - 2; i++)
		orig_data_sum += vdd->udc.gamma_data[i];

	LCD_INFO(vdd, "ORIG_DATA_SUM : [%X]\n", orig_data_sum);
	LCD_INFO(vdd, "ORIG_CHECKSUM : [%X] %02X, [%X] %02X\n",
		UDC_CS_ADDR1, vdd->udc.gamma_data[UDC_CS_ADDR1 - vdd->udc.gamma_start_addr],
		UDC_CS_ADDR2, vdd->udc.gamma_data[UDC_CS_ADDR2 - vdd->udc.gamma_start_addr]);

	vdd->udc.checksum = true;

	if (((orig_data_sum >> 8) & 0xFF) != vdd->udc.gamma_data[UDC_CS_ADDR1 - vdd->udc.gamma_start_addr]) {
		LCD_ERR(vdd, "ORIG_CHECKSUM1 fail!\n");
		vdd->udc.checksum = false;
	}

	if ((orig_data_sum & 0xFF) != vdd->udc.gamma_data[UDC_CS_ADDR2 - vdd->udc.gamma_start_addr]) {
		LCD_ERR(vdd, "ORIG_CHECKSUM2 fail!\n");
		vdd->udc.checksum = false;
	}

	/* 3. [BACKUP] If it's never backed it up, backup the original UDC gamma data to the backup address */

	/* Read only 2 bytes in the backup address. */
	backup_buf = kzalloc(2, GFP_KERNEL);
	ss_flash_read(vdd, vdd->udc.gamma_backup_addr, 2, 2, backup_buf);

	LCD_INFO(vdd, "[0x%X] %X, [0x%X] %X\n",
		vdd->udc.gamma_backup_addr, backup_buf[0], vdd->udc.gamma_backup_addr + 1, backup_buf[1]);

	/* The value 0xFF means that it has never been backed up. */
	if (backup_buf[0] == 0xFF && backup_buf[1] == 0xFF) {
		LCD_INFO(vdd, "Do backup..\n");
		flash_sector_erase(vdd, vdd->udc.gamma_backup_addr, 3);
		ss_flash_write(vdd, vdd->udc.gamma_backup_addr, UDC_GAMMA_WRITE_SIZE, vdd->udc.gamma_size, &vdd->udc.gamma_data[0]);
	} else {
		LCD_INFO(vdd, "already backed up..\n");
	}

	LCD_INFO(vdd, "-- checksum result: %d\n", vdd->udc.checksum);

	return;
}

#define UDC_CONVERT1_SIZE	19
#define UDC_COMP1_SIZE		(UDC_CONVERT1_SIZE * 2)

static int JNCD_VAL1[3][UDC_CONVERT1_SIZE] = {
	{-1, -3, -6, -9, -11, -14, -17, -20, -20, -20, -18, -16, -13, -11, -9, -7, -4, -2, -1},
	{-1, -7, -14, -21, -29, -36, -43, -50, -50, -50, -44, -39, -33, -28, -22, -17, -11, -6, -1},
	{-1, -11, -23, -34, -46, -57, -69, -80, -80, -80, -71, -62, -53, -44, -36, -27, -18, -9, -1},
};

static int comp1_idx1[4] = {
	6816,
	6612,
	4368,
	4164
};

static int comp1_idx2[4] = {
	7224,
	7020,
	4776,
	4572
};

#define UDC_CONVERT2_SIZE	15
#define UDC_COMP2_SIZE		(UDC_CONVERT2_SIZE * 2)

static int JNCD_VAL2[3][UDC_CONVERT2_SIZE] = {
	{-2, -5, -7, -10, -12, -15, -17, -20, -20, -20, -20, -20, -20, -20, -20},
	{-6, -12, -18, -25, -31, -37, -43, -50, -50, -50, -50, -50, -50, -50, -50},
	{-10, -20, -30, -40, -50, -60, -70, -80, -80, -80, -80, -80, -80, -80, -80},
};

static int JNCD_VAL3[3][UDC_CONVERT2_SIZE] = {
	{0, -3, -6, -10, -13, -16, -20, -20, -20, -20, -20, -20, -20, -20, 0},
	{0, -8, -16, -25, -33, -41, -50, -50, -50, -50, -50, -50, -50, -50, 0},
	{0, -13, -26, -40, -53, -66, -80, -80, -80, -80, -80, -80, -80, -80, 0},
};

static int comp2_idx1[2] = {
	5794,
	3346,
};
static int comp2_idx2[2] = {
	5998,
	3550,
};
static int comp2_idx3[2] = {
	6202,
	3754,
};
static int comp2_idx4[2] = {
	6406,
	3958,
};

static int ss_udc_gamma_comp(struct samsung_display_driver_data *vdd)
{
	struct UDC *udc = &vdd->udc;
	int convert1_data[UDC_CONVERT1_SIZE];
	int new_convert_data1[UDC_CONVERT1_SIZE];
	int new_convert_data2[UDC_CONVERT1_SIZE];
	int convert2_data[UDC_CONVERT2_SIZE];
	int new_convert_data3[UDC_CONVERT2_SIZE];
	int new_convert_data4[UDC_CONVERT2_SIZE];
	int new_convert_data5[UDC_CONVERT2_SIZE];
	int new_convert_data6[UDC_CONVERT2_SIZE];
	char pBuf[256];
	u8 comp_buf1[UDC_COMP1_SIZE];
	u8 comp_buf2[UDC_COMP2_SIZE];
	int i, j, comp_idx;
	u8 val1, val2;
	int new_data_sum = 0;
	int JNCD_idx = vdd->udc.JNCD_idx;
	int CS1_idx, CS2_idx;
	int convert_idx, convert_base;
	int ret = 0;

	LCD_INFO(vdd, "++\n");

	if (!vdd->udc.gamma_data) {
		LCD_ERR(vdd, "No udc gamma data..\n");
		return -1;
	}

	CS1_idx = UDC_CS_ADDR1 - udc->gamma_start_addr;
	CS2_idx = UDC_CS_ADDR2 - udc->gamma_start_addr;

	/******************************************************************/
	/* 2. Convert *****************************************************/
	/******************************************************************/

	/* 2. Convert 1-1 (6816, 6612, 4368, 4164) */

	convert_base = comp1_idx1[0];

	LCD_INFO(vdd, "[UDC_GAMMA_COMP] comp idx [%d] [0x%X] ============================== \n", convert_base, convert_base);

	memset(pBuf, 0x00, 256);
	for (i = 0; i < UDC_CONVERT1_SIZE; i++) {
		convert1_data[i] = (udc->gamma_data[convert_base+2*i] << 8) | (udc->gamma_data[convert_base+2*i+1]);
		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %04X", convert1_data[i]);
	}

	LCD_INFO(vdd, "convert_data[%d] : %s\n", UDC_CONVERT1_SIZE, pBuf);

	LCD_INFO(vdd, "OFFSET[%d] : %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", JNCD_idx,
			JNCD_VAL1[JNCD_idx][0], JNCD_VAL1[JNCD_idx][1], JNCD_VAL1[JNCD_idx][2], JNCD_VAL1[JNCD_idx][3], JNCD_VAL1[JNCD_idx][4], JNCD_VAL1[JNCD_idx][5],
			JNCD_VAL1[JNCD_idx][6], JNCD_VAL1[JNCD_idx][7], JNCD_VAL1[JNCD_idx][8], JNCD_VAL1[JNCD_idx][9], JNCD_VAL1[JNCD_idx][10], JNCD_VAL1[JNCD_idx][11],
			JNCD_VAL1[JNCD_idx][12], JNCD_VAL1[JNCD_idx][13], JNCD_VAL1[JNCD_idx][14], JNCD_VAL1[JNCD_idx][15], JNCD_VAL1[JNCD_idx][16], JNCD_VAL1[JNCD_idx][17],
			JNCD_VAL1[JNCD_idx][18]);

	/* UDC Offset & CS Caluculation */
	memset(pBuf, 0x00, 256);
	for (i = 0; i < UDC_CONVERT1_SIZE; i++) {
		if (convert1_data[i] + JNCD_VAL1[JNCD_idx][i] < 0)
			new_convert_data1[i] = 0x00;
		else if (convert1_data[i] + JNCD_VAL1[JNCD_idx][i] > 0xFFFF)
			new_convert_data1[i] = 0xFFFF;
		else
			new_convert_data1[i] = convert1_data[i] + JNCD_VAL1[JNCD_idx][i];

		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %04X", new_convert_data1[i]);
	}

	LCD_INFO(vdd, "new_convert_data1[%d] : %s\n", UDC_CONVERT1_SIZE, pBuf);

	/* Convert Format */
	for (i = 0; i < sizeof(comp1_idx1) / sizeof(int); i++) {
		convert_idx = comp1_idx1[i];

		for (j = 0; j < UDC_CONVERT1_SIZE; j++) {
			val1 = udc->gamma_data[convert_idx+2*j];
			val2 = udc->gamma_data[convert_idx+2*j+1];

			udc->gamma_data[convert_idx+2*j] = (new_convert_data1[j] >> 8) & 0xFF;
			udc->gamma_data[convert_idx+2*j+1] = new_convert_data1[j] & 0xFF;

			LCD_INFO(vdd, "Convert [%d] %02X -> %02X \n", convert_idx+2*j, val1, udc->gamma_data[convert_idx+2*j]);
			LCD_INFO(vdd, "Convert [%d] %02X -> %02X \n", convert_idx+2*j+1, val2, udc->gamma_data[convert_idx+2*j+1]);
		}
	}

	/* 2. Convert 1-2 (7224, 7020, 4776, 4572) */

	convert_base = comp1_idx2[0];

	LCD_INFO(vdd, "[UDC_GAMMA_COMP] comp idx [%d] [0x%X] ============================== \n", convert_base, convert_base);

	memset(pBuf, 0x00, 256);
	for (i = 0; i < UDC_CONVERT1_SIZE; i++) {
		convert1_data[i] = (udc->gamma_data[convert_base+2*i] << 8) | (udc->gamma_data[convert_base+2*i+1]);
		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %04X", convert1_data[i]);
	}

	LCD_INFO(vdd, "convert_data[%d] : %s\n", UDC_CONVERT1_SIZE, pBuf);

	LCD_INFO(vdd, "OFFSET[%d] : %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", JNCD_idx,
			JNCD_VAL1[JNCD_idx][0], JNCD_VAL1[JNCD_idx][1], JNCD_VAL1[JNCD_idx][2], JNCD_VAL1[JNCD_idx][3], JNCD_VAL1[JNCD_idx][4], JNCD_VAL1[JNCD_idx][5],
			JNCD_VAL1[JNCD_idx][6], JNCD_VAL1[JNCD_idx][7], JNCD_VAL1[JNCD_idx][8], JNCD_VAL1[JNCD_idx][9], JNCD_VAL1[JNCD_idx][10], JNCD_VAL1[JNCD_idx][11],
			JNCD_VAL1[JNCD_idx][12], JNCD_VAL1[JNCD_idx][13], JNCD_VAL1[JNCD_idx][14], JNCD_VAL1[JNCD_idx][15], JNCD_VAL1[JNCD_idx][16], JNCD_VAL1[JNCD_idx][17],
			JNCD_VAL1[JNCD_idx][18]);

	/* UDC Offset & CS Caluculation */
	memset(pBuf, 0x00, 256);
	for (i = 0; i < UDC_CONVERT1_SIZE; i++) {
		if (convert1_data[i] + JNCD_VAL1[JNCD_idx][i] < 0)
			new_convert_data2[i] = 0x00;
		else if (convert1_data[i] + JNCD_VAL1[JNCD_idx][i] > 0xFFFF)
			new_convert_data2[i] = 0xFFFF;
		else
			new_convert_data2[i] = convert1_data[i] + JNCD_VAL1[JNCD_idx][i];

		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %04X", new_convert_data2[i]);
	}

	LCD_INFO(vdd, "new_convert_data2[%d] : %s\n", UDC_CONVERT1_SIZE, pBuf);

	/* Convert Format */
	for (i = 0; i < sizeof(comp1_idx2) / sizeof(int); i++) {
		convert_idx = comp1_idx2[i];

		for (j = 0; j < UDC_CONVERT1_SIZE; j++) {
			val1 = udc->gamma_data[convert_idx+2*j];
			val2 = udc->gamma_data[convert_idx+2*j+1];

			udc->gamma_data[convert_idx+2*j] = (new_convert_data2[j] >> 8) & 0xFF;
			udc->gamma_data[convert_idx+2*j+1] = new_convert_data2[j] & 0xFF;

			LCD_INFO(vdd, "Convert [%d] %02X -> %02X \n", convert_idx+2*j, val1, udc->gamma_data[convert_idx+2*i]);
			LCD_INFO(vdd, "Convert [%d] %02X -> %02X \n", convert_idx+2*j+1, val2, udc->gamma_data[convert_idx+2*i+1]);
		}
	}

	/* 2. Convert 2-1 (5794, 3346) */

	convert_base = comp2_idx1[0];

	LCD_INFO(vdd, "[UDC_GAMMA_COMP] comp idx [%d] [0x%X] ============================== \n", convert_base, convert_base);

	memset(pBuf, 0x00, 256);
	for (i = 0; i < UDC_CONVERT2_SIZE; i++) {
		convert2_data[i] = (udc->gamma_data[convert_base+2*i] << 8) | (udc->gamma_data[convert_base+2*i+1]);
		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %04X", convert2_data[i]);
	}

	LCD_INFO(vdd, "convert_data[%d] : %s\n", UDC_CONVERT2_SIZE, pBuf);

	LCD_INFO(vdd, "OFFSET[%d] : %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", JNCD_idx,
			JNCD_VAL2[JNCD_idx][0], JNCD_VAL2[JNCD_idx][1], JNCD_VAL2[JNCD_idx][2], JNCD_VAL2[JNCD_idx][3], JNCD_VAL2[JNCD_idx][4], JNCD_VAL2[JNCD_idx][5],
			JNCD_VAL2[JNCD_idx][6], JNCD_VAL2[JNCD_idx][7], JNCD_VAL2[JNCD_idx][8], JNCD_VAL2[JNCD_idx][9], JNCD_VAL2[JNCD_idx][10], JNCD_VAL2[JNCD_idx][11],
			JNCD_VAL2[JNCD_idx][12], JNCD_VAL2[JNCD_idx][13], JNCD_VAL2[JNCD_idx][14]);

	/* UDC Offset & CS Caluculation */
	memset(pBuf, 0x00, 256);
	for (i = 0; i < UDC_CONVERT2_SIZE; i++) {
		if (convert2_data[i] + JNCD_VAL2[JNCD_idx][i] < 0)
			new_convert_data3[i] = 0x00;
		else if (convert2_data[i] + JNCD_VAL2[JNCD_idx][i] > 0xFFFF)
			new_convert_data3[i] = 0xFFFF;
		else
			new_convert_data3[i] = convert2_data[i] + JNCD_VAL2[JNCD_idx][i];

		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %04X", new_convert_data3[i]);
	}

	LCD_INFO(vdd, "new_convert_data3[%d] : %s\n", UDC_CONVERT2_SIZE, pBuf);

	/* Convert Format */
	for (i = 0; i < sizeof(comp2_idx1) / sizeof(int); i++) {
		convert_idx = comp2_idx1[i];

		for (j = 0; j < UDC_CONVERT2_SIZE; j++) {
			val1 = udc->gamma_data[convert_idx+2*j];
			val2 = udc->gamma_data[convert_idx+2*j+1];

			udc->gamma_data[convert_idx+2*j] = (new_convert_data3[j] >> 8) & 0xFF;
			udc->gamma_data[convert_idx+2*j+1] = new_convert_data3[j] & 0xFF;

			LCD_INFO(vdd, "Convert [%d] %02X -> %02X \n", convert_idx+2*j, val1, udc->gamma_data[convert_idx+2*i]);
			LCD_INFO(vdd, "Convert [%d] %02X -> %02X \n", convert_idx+2*j+1, val2, udc->gamma_data[convert_idx+2*i+1]);
		}
	}

	/* 2. Convert 2-2 (5998, 3550) */

	convert_base = comp2_idx2[0];

	LCD_INFO(vdd, "[UDC_GAMMA_COMP] comp idx [%d] [0x%X] ============================== \n", convert_base, convert_base);

	memset(pBuf, 0x00, 256);
	for (i = 0; i < UDC_CONVERT2_SIZE; i++) {
		convert2_data[i] = (udc->gamma_data[convert_base+2*i] << 8) | (udc->gamma_data[convert_base+2*i+1]);
		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %04X", convert2_data[i]);
	}

	LCD_INFO(vdd, "convert_data[%d] : %s\n", UDC_CONVERT2_SIZE, pBuf);

	LCD_INFO(vdd, "OFFSET[%d] : %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", JNCD_idx,
			JNCD_VAL2[JNCD_idx][0], JNCD_VAL2[JNCD_idx][1], JNCD_VAL2[JNCD_idx][2], JNCD_VAL2[JNCD_idx][3], JNCD_VAL2[JNCD_idx][4], JNCD_VAL2[JNCD_idx][5],
			JNCD_VAL2[JNCD_idx][6], JNCD_VAL2[JNCD_idx][7], JNCD_VAL2[JNCD_idx][8], JNCD_VAL2[JNCD_idx][9], JNCD_VAL2[JNCD_idx][10], JNCD_VAL2[JNCD_idx][11],
			JNCD_VAL2[JNCD_idx][12], JNCD_VAL2[JNCD_idx][13], JNCD_VAL2[JNCD_idx][14]);

	/* UDC Offset & CS Caluculation */
	memset(pBuf, 0x00, 256);
	for (i = 0; i < UDC_CONVERT2_SIZE; i++) {
		if (convert2_data[i] + JNCD_VAL2[JNCD_idx][i] < 0)
			new_convert_data4[i] = 0x00;
		else if (convert2_data[i] + JNCD_VAL2[JNCD_idx][i] > 0xFFFF)
			new_convert_data4[i] = 0xFFFF;
		else
			new_convert_data4[i] = convert2_data[i] + JNCD_VAL2[JNCD_idx][i];

		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %04X", new_convert_data4[i]);
	}

	LCD_INFO(vdd, "new_convert_data4[%d] : %s\n", UDC_CONVERT2_SIZE, pBuf);

	/* Convert Format */
	for (i = 0; i < sizeof(comp2_idx2) / sizeof(int); i++) {
		convert_idx = comp2_idx2[i];

		for (j = 0; j < UDC_CONVERT2_SIZE; j++) {
			val1 = udc->gamma_data[convert_idx+2*j];
			val2 = udc->gamma_data[convert_idx+2*j+1];

			udc->gamma_data[convert_idx+2*j] = (new_convert_data4[j] >> 8) & 0xFF;
			udc->gamma_data[convert_idx+2*j+1] = new_convert_data4[j] & 0xFF;

			LCD_INFO(vdd, "Convert [%d] %02X -> %02X \n", convert_idx+2*j, val1, udc->gamma_data[convert_idx+2*i]);
			LCD_INFO(vdd, "Convert [%d] %02X -> %02X \n", convert_idx+2*j+1, val2, udc->gamma_data[convert_idx+2*i+1]);
		}
	}

	/* 2. Convert 2-3 (6202, 3754) */

	convert_base = comp2_idx3[0];

	LCD_INFO(vdd, "[UDC_GAMMA_COMP] comp idx [%d] [0x%X] ============================== \n", convert_base, convert_base);

	memset(pBuf, 0x00, 256);
	for (i = 0; i < UDC_CONVERT2_SIZE; i++) {
		convert2_data[i] = (udc->gamma_data[convert_base+2*i] << 8) | (udc->gamma_data[convert_base+2*i+1]);
		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %04X", convert2_data[i]);
	}

	LCD_INFO(vdd, "convert_data[%d] : %s\n", UDC_CONVERT2_SIZE, pBuf);

	LCD_INFO(vdd, "OFFSET[%d] : %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", JNCD_idx,
			JNCD_VAL2[JNCD_idx][0], JNCD_VAL2[JNCD_idx][1], JNCD_VAL2[JNCD_idx][2], JNCD_VAL2[JNCD_idx][3], JNCD_VAL2[JNCD_idx][4], JNCD_VAL2[JNCD_idx][5],
			JNCD_VAL2[JNCD_idx][6], JNCD_VAL2[JNCD_idx][7], JNCD_VAL2[JNCD_idx][8], JNCD_VAL2[JNCD_idx][9], JNCD_VAL2[JNCD_idx][10], JNCD_VAL2[JNCD_idx][11],
			JNCD_VAL2[JNCD_idx][12], JNCD_VAL2[JNCD_idx][13], JNCD_VAL2[JNCD_idx][14]);

	/* UDC Offset & CS Caluculation */
	memset(pBuf, 0x00, 256);
	for (i = 0; i < UDC_CONVERT2_SIZE; i++) {
		if (convert2_data[i] + JNCD_VAL2[JNCD_idx][i] < 0)
			new_convert_data5[i] = 0x00;
		else if (convert2_data[i] + JNCD_VAL2[JNCD_idx][i] > 0xFFFF)
			new_convert_data5[i] = 0xFFFF;
		else
			new_convert_data5[i] = convert2_data[i] + JNCD_VAL2[JNCD_idx][i];

		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %04X", new_convert_data5[i]);
	}

	LCD_INFO(vdd, "new_convert_data5[%d] : %s\n", UDC_CONVERT2_SIZE, pBuf);

	/* Convert Format */
	for (i = 0; i < sizeof(comp2_idx3) / sizeof(int); i++) {
		convert_idx = comp2_idx3[i];

		for (j = 0; j < UDC_CONVERT2_SIZE; j++) {
			val1 = udc->gamma_data[convert_idx+2*j];
			val2 = udc->gamma_data[convert_idx+2*j+1];

			udc->gamma_data[convert_idx+2*j] = (new_convert_data5[j] >> 8) & 0xFF;
			udc->gamma_data[convert_idx+2*j+1] = new_convert_data5[j] & 0xFF;

			LCD_INFO(vdd, "Convert [%d] %02X -> %02X \n", convert_idx+2*j, val1, udc->gamma_data[convert_idx+2*i]);
			LCD_INFO(vdd, "Convert [%d] %02X -> %02X \n", convert_idx+2*j+1, val2, udc->gamma_data[convert_idx+2*i+1]);
		}
	}

	/* 2. Convert 2-4 (6406, 3958) */

	convert_base = comp2_idx4[0];

	LCD_INFO(vdd, "[UDC_GAMMA_COMP] comp idx [%d] [0x%X] ============================== \n", convert_base, convert_base);

	memset(pBuf, 0x00, 256);
	for (i = 0; i < UDC_CONVERT2_SIZE; i++) {
		convert2_data[i] = (udc->gamma_data[convert_base+2*i] << 8) | (udc->gamma_data[convert_base+2*i+1]);
		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %04X", convert2_data[i]);
	}

	LCD_INFO(vdd, "convert_data[%d] : %s\n", UDC_CONVERT2_SIZE, pBuf);

	LCD_INFO(vdd, "OFFSET[%d] : %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", JNCD_idx,
			JNCD_VAL3[JNCD_idx][0], JNCD_VAL3[JNCD_idx][1], JNCD_VAL3[JNCD_idx][2], JNCD_VAL3[JNCD_idx][3], JNCD_VAL3[JNCD_idx][4], JNCD_VAL3[JNCD_idx][5],
			JNCD_VAL3[JNCD_idx][6], JNCD_VAL3[JNCD_idx][7], JNCD_VAL3[JNCD_idx][8], JNCD_VAL3[JNCD_idx][9], JNCD_VAL3[JNCD_idx][10], JNCD_VAL3[JNCD_idx][11],
			JNCD_VAL3[JNCD_idx][12], JNCD_VAL3[JNCD_idx][13], JNCD_VAL3[JNCD_idx][14]);

	/* UDC Offset & CS Caluculation */
	memset(pBuf, 0x00, 256);
	for (i = 0; i < UDC_CONVERT2_SIZE; i++) {
		if (convert2_data[i] + JNCD_VAL3[JNCD_idx][i] < 0)
			new_convert_data6[i] = 0x00;
		else if (convert2_data[i] + JNCD_VAL3[JNCD_idx][i] > 0xFFFF)
			new_convert_data6[i] = 0xFFFF;
		else
			new_convert_data6[i] = convert2_data[i] + JNCD_VAL3[JNCD_idx][i];

		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %04X", new_convert_data6[i]);
	}

	LCD_INFO(vdd, "new_convert_data6[%d] : %s\n", UDC_CONVERT2_SIZE, pBuf);

	/* Convert Format */
	for (i = 0; i < sizeof(comp2_idx4) / sizeof(int); i++) {
		convert_idx = comp2_idx4[i];

		for (j = 0; j < UDC_CONVERT2_SIZE; j++) {
			val1 = udc->gamma_data[convert_idx+2*j];
			val2 = udc->gamma_data[convert_idx+2*j+1];

			udc->gamma_data[convert_idx+2*j] = (new_convert_data6[j] >> 8) & 0xFF;
			udc->gamma_data[convert_idx+2*j+1] = new_convert_data6[j] & 0xFF;

			LCD_INFO(vdd, "Convert [%d] %02X -> %02X \n", convert_idx+2*j, val1, udc->gamma_data[convert_idx+2*i]);
			LCD_INFO(vdd, "Convert [%d] %02X -> %02X \n", convert_idx+2*j+1, val2, udc->gamma_data[convert_idx+2*i+1]);
		}
	}

	/***************************************/
	/* check/set NEW CHECKSUM **************/
	/***************************************/

	for (i = 0; i < udc->gamma_size - 2; i++)
		new_data_sum += udc->gamma_data[i];

	udc->gamma_data[CS1_idx] = (new_data_sum >> 8) & 0xFF;
	udc->gamma_data[CS2_idx] = new_data_sum & 0xFF;

	LCD_INFO(vdd, "NEW_DATA_SUM : [%X]\n", new_data_sum);
	LCD_INFO(vdd, "NEW_CHECKSUM : [%X] %02X, [%X] %02X\n",
		UDC_CS_ADDR1, udc->gamma_data[CS1_idx],	UDC_CS_ADDR2, udc->gamma_data[CS2_idx]);

	/******************************************************************/
	/* 4. Sector Erase (it must be performed before writing) **********/
	/******************************************************************/

	flash_sector_erase(vdd, udc->gamma_start_addr, 3);

	/******************************************************************/
	/* 5. write comp data *********************************************/
	/******************************************************************/

	ss_flash_write(vdd, udc->gamma_start_addr, UDC_GAMMA_WRITE_SIZE, udc->gamma_size, &udc->gamma_data[0]);


	/******************************************************************/
	/* 6. check written values ****************************************/
	/******************************************************************/

	/* read a written value and check it. (38byte) */
	LCD_INFO(vdd, "check written values...\n");

	comp_idx = comp1_idx1[0];
	memset(pBuf, 0x00, 256);
	ss_flash_read(vdd, udc->gamma_start_addr + comp_idx, UDC_COMP1_SIZE, UDC_COMP1_SIZE, comp_buf1);
	for (i = 0; i < UDC_COMP1_SIZE; i++) {
		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %02X", comp_buf1[i]);

		if (udc->gamma_data[comp_idx+i] != comp_buf1[i]) {
			LCD_ERR(vdd, "[%d][%X] calc data [%02X] != read data [%02X] ..\n", i, comp_idx+i, udc->gamma_data[comp_idx+i], comp_buf1[i]);
			ret = -1;
			break;
		}
	}
	LCD_INFO(vdd, "[%d] read_data[%d] : %s\n", comp_idx, UDC_COMP1_SIZE, pBuf);

	/* read a written value and check it. (38byte) */
	comp_idx = comp1_idx2[0];
	memset(pBuf, 0x00, 256);
	ss_flash_read(vdd, udc->gamma_start_addr + comp_idx, UDC_COMP1_SIZE, UDC_COMP1_SIZE, comp_buf1);
	for (i = 0; i < UDC_COMP1_SIZE; i++) {
		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %02X", comp_buf1[i]);

		if (udc->gamma_data[comp_idx+i] != comp_buf1[i]) {
			LCD_ERR(vdd, "[%d][%X] calc data [%02X] != read data [%02X] ..\n", i, comp_idx+i, udc->gamma_data[comp_idx+i], comp_buf1[i]);
			ret = -1;
			break;
		}
	}
	LCD_INFO(vdd, "[%d] read_data[%d] : %s\n", comp_idx, UDC_COMP1_SIZE, pBuf);

	/* read a written value and check it. (30byte) */
	comp_idx = comp2_idx1[0];
	memset(pBuf, 0x00, 256);
	ss_flash_read(vdd, udc->gamma_start_addr + comp_idx, UDC_COMP2_SIZE, UDC_COMP2_SIZE, comp_buf2);
	for (i = 0; i < UDC_COMP2_SIZE; i++) {
		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %02X", comp_buf2[i]);

		if (udc->gamma_data[comp_idx+i] != comp_buf2[i]) {
			LCD_ERR(vdd, "[%d][%X] calc data [%02X] != read data [%02X] ..\n", i, comp_idx+i, udc->gamma_data[comp_idx+i], comp_buf2[i]);
			ret = -1;
			break;
		}
	}
	LCD_INFO(vdd, "[%d] read_data[%d] : %s\n", comp_idx, UDC_COMP2_SIZE, pBuf);

	/* read a written value and check it. (30byte) */
	comp_idx = comp2_idx2[0];
	memset(pBuf, 0x00, 256);
	ss_flash_read(vdd, udc->gamma_start_addr + comp_idx, UDC_COMP2_SIZE, UDC_COMP2_SIZE, comp_buf2);
	for (i = 0; i < UDC_COMP2_SIZE; i++) {
		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %02X", comp_buf2[i]);

		if (udc->gamma_data[comp_idx+i] != comp_buf2[i]) {
			LCD_ERR(vdd, "[%d][%X] calc data [%02X] != read data [%02X] ..\n", i, comp_idx+i, udc->gamma_data[comp_idx+i], comp_buf2[i]);
			ret = -1;
			break;
		}
	}
	LCD_INFO(vdd, "[%d] read_data[%d] : %s\n", comp_idx, UDC_COMP2_SIZE, pBuf);

	/* read a written value and check it. (30byte) */
	comp_idx = comp2_idx3[0];
	memset(pBuf, 0x00, 256);
	ss_flash_read(vdd, udc->gamma_start_addr + comp_idx, UDC_COMP2_SIZE, UDC_COMP2_SIZE, comp_buf2);
	for (i = 0; i < UDC_COMP2_SIZE; i++) {
		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %02X", comp_buf2[i]);

		if (udc->gamma_data[comp_idx+i] != comp_buf2[i]) {
			LCD_ERR(vdd, "[%d][%X] calc data [%02X] != read data [%02X] ..\n", i, comp_idx+i, udc->gamma_data[comp_idx+i], comp_buf2[i]);
			ret = -1;
			break;
		}
	}
	LCD_INFO(vdd, "[%d] read_data[%d] : %s\n", comp_idx, UDC_COMP2_SIZE, pBuf);

	/* read a written value and check it. (30byte) */
	comp_idx = comp2_idx4[0];
	memset(pBuf, 0x00, 256);
	ss_flash_read(vdd, udc->gamma_start_addr + comp_idx, UDC_COMP2_SIZE, UDC_COMP2_SIZE, comp_buf2);
	for (i = 0; i < UDC_COMP2_SIZE; i++) {
		scnprintf(pBuf + strnlen(pBuf, 256), 256, " %02X", comp_buf2[i]);

		if (udc->gamma_data[comp_idx+i] != comp_buf2[i]) {
			LCD_ERR(vdd, "[%d][%X] calc data [%02X] != read data [%02X] ..\n", i, comp_idx+i, udc->gamma_data[comp_idx+i], comp_buf2[i]);
			ret = -1;
			break;
		}
	}
	LCD_INFO(vdd, "[%d] read_data[%d] : %s\n", comp_idx, UDC_COMP2_SIZE, pBuf);

	if (ret)
		udc->udc_comp_done = false;
	else
		udc->udc_comp_done = true;

	LCD_INFO(vdd, "-- ret:%d comp_done:%d\n", ret, udc->udc_comp_done);

	return ret;
}

static int ss_udc_restore_orig_gamma(struct samsung_display_driver_data *vdd)
{
	struct UDC *udc = &vdd->udc;
	u32 target_addr;
	int ret = 0;
	char pBuf[256];
	int i, comp_idx;
	u8 comp_buf1[UDC_COMP1_SIZE];
	u8 comp_buf2[UDC_COMP2_SIZE];

	target_addr = udc->gamma_backup_addr + udc->gamma_size;

	LCD_INFO(vdd, "backup addr : 0x%X ~ 0x%X, total_read_size %d ++\n",
		udc->gamma_backup_addr, target_addr, udc->gamma_size);

	/******************************************************************/
	/* Read flash gamma from backup addr ******************************/
	/******************************************************************/

	ss_flash_read(vdd, udc->gamma_backup_addr, udc->gamma_size, UDC_GAMMA_READ_SIZE, udc->gamma_data_backup);
	memcpy(udc->gamma_data, udc->gamma_data_backup, udc->gamma_size);

	if (udc->gamma_data_backup[0] == 0xFF && udc->gamma_data_backup[1] == 0xFF) {
		LCD_ERR(vdd, "not backed up!\n");
		ret = -1;
	} else {
		LCD_INFO(vdd, "Do restore\n");

		/******************************************************************/
		/* Erase UDC gamma addr *******************************************/
		/******************************************************************/

		flash_sector_erase(vdd, udc->gamma_start_addr, 3);

		/******************************************************************/
		/* Write Backup data to gamma flash addr **************************/
		/******************************************************************/

		ss_flash_write(vdd, udc->gamma_start_addr, UDC_GAMMA_WRITE_SIZE, udc->gamma_size, &udc->gamma_data_backup[0]);

		/******************************************************************/
		/* check written values *******************************************/
		/******************************************************************/

		/* read a written value and check it. (38byte) */
		LCD_INFO(vdd, "check written values...\n");

		comp_idx = comp1_idx1[0];
		memset(pBuf, 0x00, 256);
		ss_flash_read(vdd, udc->gamma_start_addr + comp_idx, UDC_COMP1_SIZE, UDC_COMP1_SIZE, comp_buf1);
		for (i = 0; i < UDC_COMP1_SIZE; i++) {
			scnprintf(pBuf + strnlen(pBuf, 256), 256, " %02X", comp_buf1[i]);

			if (udc->gamma_data[comp_idx+i] != comp_buf1[i]) {
				LCD_ERR(vdd, "[%d][%X] calc data [%02X] != read data [%02X] ..\n", i, comp_idx+i, udc->gamma_data[comp_idx+i], comp_buf1[i]);
				ret = -1;
				break;
			}
		}
		LCD_INFO(vdd, "[%d] read_data[%d] : %s\n", comp_idx, UDC_COMP1_SIZE, pBuf);

		/* read a written value and check it. (38byte) */
		comp_idx = comp1_idx2[0];
		memset(pBuf, 0x00, 256);
		ss_flash_read(vdd, udc->gamma_start_addr + comp_idx, UDC_COMP1_SIZE, UDC_COMP1_SIZE, comp_buf1);
		for (i = 0; i < UDC_COMP1_SIZE; i++) {
			scnprintf(pBuf + strnlen(pBuf, 256), 256, " %02X", comp_buf1[i]);

			if (udc->gamma_data[comp_idx+i] != comp_buf1[i]) {
				LCD_ERR(vdd, "[%d][%X] calc data [%02X] != read data [%02X] ..\n", i, comp_idx+i, udc->gamma_data[comp_idx+i], comp_buf1[i]);
				ret = -1;
				break;
			}
		}
		LCD_INFO(vdd, "[%d] read_data[%d] : %s\n", comp_idx, UDC_COMP1_SIZE, pBuf);

		/* read a written value and check it. (30byte) */
		comp_idx = comp2_idx1[0];
		memset(pBuf, 0x00, 256);
		ss_flash_read(vdd, udc->gamma_start_addr + comp_idx, UDC_COMP2_SIZE, UDC_COMP2_SIZE, comp_buf2);
		for (i = 0; i < UDC_COMP2_SIZE; i++) {
			scnprintf(pBuf + strnlen(pBuf, 256), 256, " %02X", comp_buf2[i]);

			if (udc->gamma_data[comp_idx+i] != comp_buf2[i]) {
				LCD_ERR(vdd, "[%d][%X] calc data [%02X] != read data [%02X] ..\n", i, comp_idx+i, udc->gamma_data[comp_idx+i], comp_buf2[i]);
				ret = -1;
				break;
			}
		}
		LCD_INFO(vdd, "[%d] read_data[%d] : %s\n", comp_idx, UDC_COMP2_SIZE, pBuf);

		/* read a written value and check it. (30byte) */
		comp_idx = comp2_idx2[0];
		memset(pBuf, 0x00, 256);
		ss_flash_read(vdd, udc->gamma_start_addr + comp_idx, UDC_COMP2_SIZE, UDC_COMP2_SIZE, comp_buf2);
		for (i = 0; i < UDC_COMP2_SIZE; i++) {
			scnprintf(pBuf + strnlen(pBuf, 256), 256, " %02X", comp_buf2[i]);

			if (udc->gamma_data[comp_idx+i] != comp_buf2[i]) {
				LCD_ERR(vdd, "[%d][%X] calc data [%02X] != read data [%02X] ..\n", i, comp_idx+i, udc->gamma_data[comp_idx+i], comp_buf2[i]);
				ret = -1;
				break;
			}
		}
		LCD_INFO(vdd, "[%d] read_data[%d] : %s\n", comp_idx, UDC_COMP2_SIZE, pBuf);

		/* read a written value and check it. (30byte) */
		comp_idx = comp2_idx3[0];
		memset(pBuf, 0x00, 256);
		ss_flash_read(vdd, udc->gamma_start_addr + comp_idx, UDC_COMP2_SIZE, UDC_COMP2_SIZE, comp_buf2);
		for (i = 0; i < UDC_COMP2_SIZE; i++) {
			scnprintf(pBuf + strnlen(pBuf, 256), 256, " %02X", comp_buf2[i]);

			if (udc->gamma_data[comp_idx+i] != comp_buf2[i]) {
				LCD_ERR(vdd, "[%d][%X] calc data [%02X] != read data [%02X] ..\n", i, comp_idx+i, udc->gamma_data[comp_idx+i], comp_buf2[i]);
				ret = -1;
				break;
			}
		}
		LCD_INFO(vdd, "[%d] read_data[%d] : %s\n", comp_idx, UDC_COMP2_SIZE, pBuf);

		/* read a written value and check it. (30byte) */
		comp_idx = comp2_idx4[0];
		memset(pBuf, 0x00, 256);
		ss_flash_read(vdd, udc->gamma_start_addr + comp_idx, UDC_COMP2_SIZE, UDC_COMP2_SIZE, comp_buf2);
		for (i = 0; i < UDC_COMP2_SIZE; i++) {
			scnprintf(pBuf + strnlen(pBuf, 256), 256, " %02X", comp_buf2[i]);

			if (udc->gamma_data[comp_idx+i] != comp_buf2[i]) {
				LCD_ERR(vdd, "[%d][%X] calc data [%02X] != read data [%02X] ..\n", i, comp_idx+i, udc->gamma_data[comp_idx+i], comp_buf2[i]);
				ret = -1;
				break;
			}
		}
		LCD_INFO(vdd, "[%d] read_data[%d] : %s\n", comp_idx, UDC_COMP2_SIZE, pBuf);
	}

	if (ret)
		udc->udc_restore_done = false;
	else
		udc->udc_restore_done = true;

	LCD_INFO(vdd, "-- ret:%d udc_restore_done:%d\n", ret, udc->udc_restore_done);

	return ret;
}

/* mtp original data */
static u8 HS120_NORMAL_R_TYPE_BUF[GAMMA_R_SIZE];
/* mtp original data - V type */
static int HS120_NORMAL_V_TYPE_BUF[GAMMA_V_SIZE];

/* mtp original data */
static u8 HS120_HBM_R_TYPE_BUF[GAMMA_R_SIZE];
/* mtp original data - V type */
static int HS120_HBM_V_TYPE_BUF[GAMMA_V_SIZE];

/* HS96 compensated data */
static u8 HS96_R_TYPE_COMP[MTP_OFFSET_TAB_SIZE_96HS][GAMMA_R_SIZE];
/* HS96 compensated data V type*/
static int HS96_V_TYPE_COMP[MTP_OFFSET_TAB_SIZE_96HS][GAMMA_V_SIZE];

static void ss_print_gamma_comp(struct samsung_display_driver_data *vdd)
{
	char pBuffer[256];
	int i, j;

	LCD_INFO(vdd, "== HS120_NORMAL_R_TYPE_BUF ==\n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < GAMMA_R_SIZE; i++)
		scnprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_NORMAL_R_TYPE_BUF[i]);
	LCD_INFO(vdd, "SET : %s\n", pBuffer);

	LCD_INFO(vdd, "== HS120_NORMAL_V_TYPE_BUF ==\n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < GAMMA_V_SIZE; i++)
		scnprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_NORMAL_V_TYPE_BUF[i]);
	LCD_INFO(vdd, "SET : %s\n", pBuffer);


	LCD_INFO(vdd, "== HS120_HBM_R_TYPE_BUF ==\n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < GAMMA_R_SIZE; i++)
		scnprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_HBM_R_TYPE_BUF[i]);
	LCD_INFO(vdd, "SET : %s\n", pBuffer);

	LCD_INFO(vdd, "== HS120_HBM_V_TYPE_BUF ==\n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < GAMMA_V_SIZE; i++)
		scnprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_HBM_V_TYPE_BUF[i]);
	LCD_INFO(vdd, "SET : %s\n", pBuffer);

	LCD_INFO(vdd, "== HS96_V_TYPE_COMP ==\n");
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_V_SIZE; j++)
			scnprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS96_V_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP[%d] : %s\n", i, pBuffer);
	}

	LCD_INFO(vdd, "== HS96_R_TYPE_COMP ==\n");
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			scnprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS96_R_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP[%d] : %s\n", i, pBuffer);
	}
}

static void ss_read_gamma(struct samsung_display_driver_data *vdd)
{
	u8 readbuf[GAMMA_R_SIZE];
	int start_addr;
	struct flash_raw_table *gamma_tbl;
	u32 fsize = 0xC00000 | GAMMA_R_SIZE;

	gamma_tbl = vdd->br_info.br_tbl->gamma_tbl;

	vdd->debug_data->print_cmds = true;

	ss_send_cmd(vdd, TX_POC_ENABLE);
	ss_send_cmd(vdd, TX_FLASH_GAMMA_PRE1);

	start_addr = gamma_tbl->flash_table_hbm_gamma_offset;
	LCD_INFO(vdd, "[HBM] start_addr : %X\n", start_addr);
	flash_read_bytes(vdd, start_addr, fsize, GAMMA_R_SIZE, readbuf);
	memcpy(&HS120_HBM_R_TYPE_BUF, readbuf, GAMMA_R_SIZE);

	start_addr = gamma_tbl->flash_table_normal_gamma_offset;
	LCD_INFO(vdd, "[NORMAL] start_addr : %X\n", start_addr);
	flash_read_bytes(vdd, start_addr, fsize, GAMMA_R_SIZE, readbuf);
	memcpy(&HS120_NORMAL_R_TYPE_BUF, readbuf, GAMMA_R_SIZE);

	ss_send_cmd(vdd, TX_FLASH_GAMMA_POST);
	ss_send_cmd(vdd, TX_POC_DISABLE);

	vdd->debug_data->print_cmds = false;
}

static int ss_gm2_gamma_comp_init(struct samsung_display_driver_data *vdd)
{
	int i, j, m;
	int val, *v_val_120;

	LCD_INFO(vdd, "++\n");

	memcpy(MTP_OFFSET_96HS_VAL, MTP_OFFSET_96HS_VAL_revA, sizeof(MTP_OFFSET_96HS_VAL));

	if (vdd->panel_revision + 'A' <= 'A') {
		memcpy(aor_table_96, aor_table_96_revA, sizeof(aor_table_96));
		memcpy(aor_table_120, aor_table_120_revA, sizeof(aor_table_120));
	} else {
		memcpy(aor_table_96, aor_table_96_revA, sizeof(aor_table_96));
		memcpy(aor_table_120, aor_table_120_revA, sizeof(aor_table_120));
	}

	/***********************************************************/
	/* 1. [HBM/ NORMAL] Read HS120 ORIGINAL GAMMA Flash      */
	/***********************************************************/
	ss_read_gamma(vdd);

	/***********************************************************/
	/* 2-1. [HBM] translate Register type to V type            */
	/***********************************************************/
	j = 0;
	for (i = 0; i < GAMMA_R_SIZE; ) {
		if (i == 0 || i == GAMMA_R_SIZE - 5) { /* R0, R12 */
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i], 0, 3) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+2], 0, 7);
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i+1], 4, 7) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+3], 0, 7);
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i+1], 0, 3) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+4], 0, 7);
			i += 5;
		} else {	/* R1 ~ R11 */
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i], 4, 5) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+1], 0, 7);
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i], 2, 3) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+2], 0, 7);
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i], 0, 1) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+3], 0, 7);
			i += 4;
		}
	}

#if 0
	/***********************************************************/
	/* 1-3. [HBM] Make HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX]	   */
	/***********************************************************/

	for (j = 0; j < GAMMA_V_SIZE; j++) {
		/* check underflow & overflow */
		if (HS120_HBM_V_TYPE_BUF[j] + MTP_OFFSET_96HS_VAL[MTP_OFFSET_HBM_IDX][j] < 0) {
			HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = 0;
		} else {
			if (j <= 2 || j >= (GAMMA_V_SIZE - 3)) {	/* 1th & 14th 12bit(0xFFF) */
				val = HS120_HBM_V_TYPE_BUF[j] + MTP_OFFSET_96HS_VAL[i][j];
				if (val > 0xFFF)	/* check overflow */
					HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = 0xFFF;
				else
					HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = val;
			} else {	/* 2th - 13th 11bit(0x7FF) */
				val = HS120_HBM_V_TYPE_BUF[j] + MTP_OFFSET_96HS_VAL[0][j];
				if (val > 0x3FF)	/* check overflow */
					HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = 0x3FF;
				else
					HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = val;
			}
		}
	}
#endif

	/***********************************************************/
	/* 2-2. [NORMAL] translate Register type to V type 	       */
	/***********************************************************/

	/* HS120 */
	j = 0;
	for (i = 0; i < GAMMA_R_SIZE; ) {
		if (i == 0 || i == GAMMA_R_SIZE - 5) { /* R0, R12 */
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i], 0, 3) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+2], 0, 7);
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+1], 4, 7) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+3], 0, 7);
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+1], 0, 3) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+4], 0, 7);
			i += 5;
		} else {	/* R1 ~ R11 */
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i], 4, 5) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+1], 0, 7);
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i], 2, 3) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+2], 0, 7);
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i], 0, 1) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+3], 0, 7);
			i += 4;
		}
	}

	/*************************************************************/
	/* 3. [ALL] Make HS96_V_TYPE_COMP (NORMAL + HBM) */
	/*************************************************************/

	/* 96HS */
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		if (i <= MAX_BL_PF_LEVEL)
			v_val_120 = HS120_NORMAL_V_TYPE_BUF;
		else
			v_val_120 = HS120_HBM_V_TYPE_BUF;

		for (j = 0; j < GAMMA_V_SIZE; j++) {
			/* check underflow & overflow */
			if (v_val_120[j] + MTP_OFFSET_96HS_VAL[i][j] < 0) {
				HS96_V_TYPE_COMP[i][j] = 0;
			} else {
				if (j <= 2 || j >= (GAMMA_V_SIZE - 3)) {/* 1th & 14th 12bit(0xFFF) */
					val = v_val_120[j] + MTP_OFFSET_96HS_VAL[i][j];
					if (val > 0xFFF)	/* check overflow */
						HS96_V_TYPE_COMP[i][j] = 0xFFF;
					else
						HS96_V_TYPE_COMP[i][j] = val;
				} else {	/* 2th - 13th 11bit(0x7FF) */
					val = v_val_120[j] + MTP_OFFSET_96HS_VAL[i][j];
					if (val > 0x3FF)	/* check overflow */
						HS96_V_TYPE_COMP[i][j] = 0x3FF;
					else
						HS96_V_TYPE_COMP[i][j] = val;
				}
			}
		}
	}

	/******************************************************/
	/* 4. translate HS96_V_TYPE_COMP type to Register type*/
	/******************************************************/
	/* 96HS */
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		m = 0; // GAMMA SET size
		for (j = 0; j < GAMMA_V_SIZE; j += RGB_MAX) {
			if (j == 0 || j == GAMMA_V_SIZE - 3) {
				HS96_R_TYPE_COMP[i][m++] = GET_BITS(HS96_V_TYPE_COMP[i][j+R], 8, 11);
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 8, 11) << 4)
											| (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 8, 11));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+R], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 0, 7));
			} else {	/* 1st ~ 10th */
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+R], 8, 9) << 4)
											| (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 8, 9) << 2)
											| GET_BITS(HS96_V_TYPE_COMP[i][j+B], 8, 9);
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+R], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 0, 7));
			}
		}
	}

	/* print all results */
//	ss_print_gamma_comp(vdd);

	LCD_INFO(vdd, " --\n");

	return 0;
}

static struct dsi_panel_cmd_set *ss_analog_offset(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds = ss_get_cmds(vdd, TX_VRR_GM2_GAMMA_COMP);
	int cd_idx, cmd_idx = 0;

	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_VRR_GM2_GAMMA_COMP.. \n");
		return NULL;
	}

	if (vdd->br_info.common_br.bl_level > MAX_BL_PF_LEVEL)
		cd_idx = MAX_BL_PF_LEVEL + 1;
	else
		cd_idx = vdd->br_info.common_br.cd_idx;

	LCD_DEBUG(vdd, "vdd->vrr.cur_refresh_rate [%d], cd_idx [%d] \n",
		vdd->vrr.cur_refresh_rate, cd_idx);

	if (vdd->vrr.cur_refresh_rate == 96 || vdd->vrr.cur_refresh_rate == 48) {
		if (cd_idx >= 0) {
			cmd_idx = ss_get_cmd_idx(pcmds, 0x3DB, 0x67);
			if (cmd_idx >= 0)
				memcpy(&pcmds->cmds[cmd_idx].ss_txbuf[1], &HS96_R_TYPE_COMP[cd_idx][0], 37);

			cmd_idx = ss_get_cmd_idx(pcmds, 0x00, 0x68);
			if (cmd_idx >= 0)
				memcpy(&pcmds->cmds[cmd_idx].ss_txbuf[1], &HS96_R_TYPE_COMP[cd_idx][37], 17);

		} else {
			/* restore original register */
			LCD_ERR(vdd, "ERROR!! check idx[%d]\n", cd_idx);
		}

		return pcmds;
	} else {
		return NULL;
	}

	return NULL;
}

static struct dsi_panel_cmd_set *ss_manual_aor(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds = ss_get_cmds(vdd, TX_MANUAL_AOR);
	int base_rr, cd_idx, idx = 0;

	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_MANUAL_AOR.. \n");
		return NULL;
	}

	base_rr = ss_get_vrr_mode_base(vdd);

	/* AOR Value */
	cd_idx = vdd->br_info.common_br.bl_level;
	if (cd_idx >= 0) {
		idx = ss_get_cmd_idx(pcmds, 0x37, 0x68);
		if (idx >= 0) {
			if (base_rr == VRR_96HS) {
				pcmds->cmds[idx].ss_txbuf[2] = (aor_table_96[cd_idx] & 0xFF00) >> 8;
				pcmds->cmds[idx].ss_txbuf[3] = (aor_table_96[cd_idx] & 0xFF);
			} else {
				pcmds->cmds[idx].ss_txbuf[2] = (aor_table_120[cd_idx] & 0xFF00) >> 8;
				pcmds->cmds[idx].ss_txbuf[3] = (aor_table_120[cd_idx] & 0xFF);
			}
		}
	} else {
		/* restore original register */
		LCD_ERR(vdd, "ERROR!! check idx[%d]\n", cd_idx);
	}

	idx = ss_get_cmd_idx(pcmds, 0x37, 0x68);	/* AOR Enable Flag */
	if (idx >= 0)
		pcmds->cmds[idx].ss_txbuf[1] = (base_rr == VRR_96HS) ? 0x01 : 0x00;

	idx = ss_get_cmd_idx(pcmds, 0x3DA, 0x67); /* Gamma Enable Flag */
	if (idx >= 0)
		pcmds->cmds[idx].ss_txbuf[1] = (base_rr == VRR_96HS) ? 0x01 : 0x00;

	if (base_rr == VRR_96HS) {
		idx = ss_get_cmd_idx(pcmds, 0x0D, 0x66); /* Gamma Enable Flag */
		if (idx >= 0)
			pcmds->cmds[idx].ss_txbuf[1] = (base_rr == VRR_96HS) ? 0x40 : 0x20;

		pcmds->count = 6;
	} else {
		pcmds->count = 4;
	}

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_panel_update(struct samsung_display_driver_data *vdd, int *level_key)
{
	return ss_get_cmds(vdd, TX_PANEL_UPDATE);
}

static void make_brightness_packet(struct samsung_display_driver_data *vdd,
	struct dsi_cmd_desc *packet, int *cmd_cnt, enum BR_TYPE br_type)
{
	if (br_type == BR_TYPE_NORMAL || br_type == BR_TYPE_HBM) {
		/* ACL */
		if (vdd->br_info.acl_status || vdd->siop_status)
			ss_add_brightness_packet(vdd, BR_FUNC_ACL_ON, packet, cmd_cnt);
		else
			ss_add_brightness_packet(vdd, BR_FUNC_ACL_OFF, packet, cmd_cnt);

		/* mAFPC */
		if (vdd->mafpc.is_support)
			ss_add_brightness_packet(vdd, BR_FUNC_MAFPC_SCALE, packet, cmd_cnt);

		/* gamma */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA, packet, cmd_cnt);

		/* gamma compensation for gamma mode2 VRR modes */
		ss_add_brightness_packet(vdd, BR_FUNC_GAMMA_COMP, packet, cmd_cnt);

		/* VRR */
		ss_add_brightness_packet(vdd, BR_FUNC_VRR, packet, cmd_cnt);

		/* MANUAL AOR */
		ss_add_brightness_packet(vdd, BR_FUNC_MANUAL_AOR, packet, cmd_cnt);

		/* PANEL UPDATE */
		ss_add_brightness_packet(vdd, BR_FUNC_PANEL_UPDATE, packet, cmd_cnt);
	} else {
		LCD_ERR(vdd, "undefined br_type (%d) \n", br_type);
	}

	return;
}

void Q4_S6E3XA2_AMF756BQ01_QXGA_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "%s\n", ss_get_panel_name(vdd));

	/* Default Panel Power Status is OFF */
	vdd->panel_state = PANEL_PWR_OFF;

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = samsung_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = samsung_panel_on_post;
	vdd->panel_func.samsung_panel_off_pre = samsung_panel_off_pre;
	vdd->panel_func.samsung_panel_off_post = samsung_panel_off_post;
	vdd->panel_func.samsung_display_on_pre = samsung_display_on_pre;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = ss_panel_revision;
	vdd->panel_func.samsung_module_info_read = ss_module_info_read;
	vdd->panel_func.samsung_ddi_id_read = ss_ddi_id_read;
	vdd->panel_func.samsung_octa_id_read = ss_octa_id_read;
	vdd->panel_func.samsung_elvss_read = ss_elvss_read;

	/* Make brightness packer */
	vdd->panel_func.make_brightness_packet = make_brightness_packet;

	/* Brightness */
	vdd->panel_func.br_func[BR_FUNC_GAMMA] = ss_brightness_gamma_mode2_normal;
	vdd->panel_func.br_func[BR_FUNC_ACL_ON] = ss_acl_on;
	vdd->panel_func.br_func[BR_FUNC_ACL_OFF] = ss_acl_off;
	vdd->panel_func.br_func[BR_FUNC_VRR] = ss_vrr;
	vdd->panel_func.br_func[BR_FUNC_GAMMA_COMP] = ss_analog_offset;
	vdd->panel_func.br_func[BR_FUNC_MANUAL_AOR] = ss_manual_aor;
	vdd->panel_func.br_func[BR_FUNC_PANEL_UPDATE] = ss_panel_update;

	/* Total level key in brightness */
	vdd->panel_func.samsung_brightness_tot_level = ss_brightness_tot_level;

	/* Panel LPM */
	vdd->panel_func.samsung_update_lpm_ctrl_cmd = ss_update_panel_lpm_ctrl_cmd;
	vdd->panel_func.samsung_set_lpm_brightness = ss_set_panel_lpm_brightness;

	/* Grayspot test */
	vdd->panel_func.samsung_gray_spot = ss_gray_spot;

	/* default brightness */
	vdd->br_info.common_br.bl_level = 255;

	/* mdnie */
	vdd->mdnie.support_mdnie = true;

	vdd->mdnie.support_trans_dimming = true;
	vdd->mdnie.mdnie_tune_size[0] = sizeof(BYPASS_MDNIE_1);
	vdd->mdnie.mdnie_tune_size[1] = sizeof(BYPASS_MDNIE_2);
	vdd->mdnie.mdnie_tune_size[2] = sizeof(BYPASS_MDNIE_3);

	dsi_update_mdnie_data(vdd);

	/* COLOR WEAKNESS */
	vdd->panel_func.color_weakness_ccb_on_off =  NULL;

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = NULL;

	/* COPR */
	vdd->copr.panel_init = ss_copr_panel_init;

	/* ACL default ON */
	vdd->br_info.acl_status = 1;
	vdd->br_info.temperature = 20; // default temperature

	/* ACL default status in acl on */
	vdd->br_info.gradual_acl_val = 1;

	vdd->panel_func.ecc_read = ss_ecc_read;

	/* Self display */
	vdd->self_disp.factory_support = true;
	vdd->self_disp.init = self_display_init_XA2;
	vdd->self_disp.data_init = ss_self_display_data_init;

	/* mAPFC */
	vdd->mafpc.init = ss_mafpc_init_XA2;
	vdd->mafpc.data_init = ss_mafpc_data_init;

	/* FFC */
	vdd->panel_func.set_ffc = ss_ffc;

	/* DDI flash test */
	vdd->panel_func.samsung_gm2_ddi_flash_prepare = NULL;
	vdd->panel_func.samsung_test_ddi_flash_check = ss_test_ddi_flash_check;

	/* gamma compensation for 48/96hz VRR mode in gamma mode2 */
	vdd->panel_func.samsung_gm2_gamma_comp_init = ss_gm2_gamma_comp_init;
	vdd->panel_func.samsung_print_gamma_comp = ss_print_gamma_comp;
	vdd->panel_func.samsung_read_gamma = ss_read_gamma;
	vdd->panel_func.read_flash = ss_read_flash_sysfs;

	/* UDC */
	vdd->panel_func.read_udc_data = ss_read_udc_transmittance_data;
	vdd->panel_func.read_udc_gamma_data = ss_read_udc_gamma_data;
	vdd->panel_func.udc_gamma_comp = ss_udc_gamma_comp;
	vdd->panel_func.restore_udc_orig_gamma = ss_udc_restore_orig_gamma;

	/* VRR */
	vdd->panel_func.samsung_lfd_get_base_val = ss_update_base_lfd_val;
	ss_vrr_init(&vdd->vrr);

	vdd->panel_func.samsung_check_support_mode = ss_check_support_mode;

	/* change  MIPI Drive strength values for this panel - request by HW group */
	vdd->motto_info.motto_swing = 0xEE;
	vdd->motto_info.vreg_ctrl_0 = 0x47;

	/* FLSH */
	vdd->poc_driver.poc_write = poc_write;
	vdd->poc_driver.poc_read = poc_read;
}
