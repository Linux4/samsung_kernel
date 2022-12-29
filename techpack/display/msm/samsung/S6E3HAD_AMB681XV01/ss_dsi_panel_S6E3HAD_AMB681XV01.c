/*
 * =================================================================
 *
 *	Description:  samsung display panel file
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2020, Samsung Electronics. All rights reserved.

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
#include "ss_dsi_panel_S6E3HAD_AMB681XV01.h"
#include "ss_dsi_mdnie_S6E3HAD_AMB681XV01.h"

static char VAINT_MTP[1];
static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	static bool is_first = true;
	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);

	if (is_first) {
		ss_panel_data_read(vdd, RX_VAINT_MTP, VAINT_MTP, LEVEL1_KEY);
		LCD_INFO(vdd, "is_first VAINT_MTP = 0x%x\n", (int)VAINT_MTP[0]);

		is_first = false;
	}

	ss_panel_attach_set(vdd, true);
	LCD_INFO(vdd, "-: ndx=%d \n", vdd->ndx);

	return true;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
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

	/* mafpc */
	if (vdd->mafpc.is_support) {
		vdd->mafpc.need_to_write = true;
		LCD_INFO(vdd, "Need to write mafpc image data to DDI\n");
	}

	return true;
}

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{
	char id;

	if (vdd->manufacture_id_dsi == PBA_ID)
		ss_panel_attach_set(vdd, false);
	else
		ss_panel_attach_set(vdd, true);

	id = ss_panel_id2_get(vdd);

	switch (id) {
	case 0xC0:
	case 0xE1:
		vdd->panel_revision = 'A'; /* 2nd */
		break;
	case 0xE2:
		vdd->panel_revision = 'B'; /* 3rd */
		break;
	case 0xE3:
		vdd->panel_revision = 'C'; /* 4th */
		break;
	case 0xE4:
		vdd->panel_revision = 'D'; /* 5th */
		break;
	case 0xE5:
		vdd->panel_revision = 'E'; /* 6th */
		break;
	case 0xE6:
		vdd->panel_revision = 'F'; /* 7th */
		break;
	case 0xE7:
		vdd->panel_revision = 'G'; /* 8th */
		break;
	case 0xE8:
		vdd->panel_revision = 'H'; /* 9th */
		break;
	default:
		vdd->panel_revision = 'H';
		LCD_ERR(vdd, "Invalid panel_rev(default rev : %c)\n",
				vdd->panel_revision);
		break;
	}

	vdd->panel_revision -= 'A';

	LCD_INFO_ONCE(vdd, "panel_revision = %c %d \n",
			vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

enum CURRENT_BR_MODE {
	DEFAULT_BR_MODE,
	HS96_48_NORMAL,
	HS96_48_HBM
};

static enum CURRENT_BR_MODE cur_br_mode;
static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2_normal(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int idx = 0;
	int cur_rr;

	if (IS_ERR_OR_NULL(vdd)) {
	        LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}
	cur_rr = vdd->vrr.cur_refresh_rate;

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_NORMAL);
	idx = ss_get_cmd_idx(pcmds, 0x00, 0x51);

	pcmds->cmds[idx].ss_txbuf[1] = (vdd->br_info.common_br.gm2_wrdisbv & 0xFF00) >> 8;
	pcmds->cmds[idx].ss_txbuf[2] = vdd->br_info.common_br.gm2_wrdisbv & 0xFF;

	/* IRC mode: 0x61: Moderato mode, 0x21: flat gamma mode */
	idx = ss_get_cmd_idx(pcmds, 0xC3, 0x91);
	if (vdd->br_info.common_br.irc_mode == IRC_MODERATO_MODE)
		pcmds->cmds[idx].ss_txbuf[1] = 0x61;
	else if (vdd->br_info.common_br.irc_mode == IRC_FLAT_GAMMA_MODE)
		pcmds->cmds[idx].ss_txbuf[1] = 0x21;
	else
		LCD_ERR(vdd, "invalid irc mode(%d)\n", vdd->br_info.common_br.irc_mode);

	/* As of E3 panel, additial cmds are required */
	if (vdd->panel_revision > 1) {
		/* Vaint */
		idx = ss_get_cmd_idx(pcmds, 0x50, 0xF4);
		pcmds->cmds[idx].ss_txbuf[1] = vdd->br_info.common_br.gm2_wrdisbv > 0xCD ? VAINT_MTP[0] : 0x00;
		LCD_INFO(vdd, "Vaint = 0x%x\n", pcmds->cmds[idx].ss_txbuf[1]);
		idx = ss_get_cmd_idx(pcmds, 0xC3, 0x91);
		if (vdd->br_info.common_br.irc_mode == IRC_MODERATO_MODE) {
			pcmds->cmds[idx].ss_txbuf[2] = 0xFF;
			pcmds->cmds[idx].ss_txbuf[3] = 0x31;
			pcmds->cmds[idx].ss_txbuf[4] = 0x00;
		} else if (vdd->br_info.common_br.irc_mode == IRC_FLAT_GAMMA_MODE) {
			pcmds->cmds[idx].ss_txbuf[2] = 0xF4;
			pcmds->cmds[idx].ss_txbuf[3] = 0x33;
			pcmds->cmds[idx].ss_txbuf[4] = 0x6B;
		}
	}

	/* TSET */
	idx = ss_get_cmd_idx(pcmds, 0x0D, 0xB1);
	pcmds->cmds[idx].ss_txbuf[1] = vdd->br_info.temperature > 0 ?	vdd->br_info.temperature : (char)(BIT(7) | (-1*vdd->br_info.temperature));

	return pcmds;
}

static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2_hbm(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;
	int idx = 0;
	int cur_rr;

	if (IS_ERR_OR_NULL(vdd)) {
	        LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}
	cur_rr = vdd->vrr.cur_refresh_rate;

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_HBM);

	idx = ss_get_cmd_idx(pcmds, 0x11, 0xC2);
	/* ELVSS Value for HBM brgihtness */
	pcmds->cmds[idx].ss_txbuf[1] = elvss_table_hbm_revA[vdd->br_info.common_br.cd_idx];

	idx = ss_get_cmd_idx(pcmds, 0x00, 0x51);
	pcmds->cmds[idx].ss_txbuf[1] = (vdd->br_info.common_br.gm2_wrdisbv & 0xFF00) >> 8;
	pcmds->cmds[idx].ss_txbuf[2] = vdd->br_info.common_br.gm2_wrdisbv & 0xFF;

	/* IRC mode: 0x61: Moderato mode, 0x21: flat gamma mode */
	idx = ss_get_cmd_idx(pcmds, 0xC3, 0x91);
	if (vdd->br_info.common_br.irc_mode == IRC_MODERATO_MODE)
		pcmds->cmds[idx].ss_txbuf[1] = 0x61;
	else if (vdd->br_info.common_br.irc_mode == IRC_FLAT_GAMMA_MODE)
		pcmds->cmds[idx].ss_txbuf[1] = 0x21;
	else
		LCD_ERR(vdd, "invalid irc mode(%d)\n", vdd->br_info.common_br.irc_mode);

	/* As of E3 panel, additial cmds are required */
	if (vdd->panel_revision > 1) {
		/* Vaint */
		idx = ss_get_cmd_idx(pcmds, 0x50, 0xF4);
		pcmds->cmds[idx].ss_txbuf[1] = VAINT_MTP[0];
		LCD_INFO(vdd, "Vaint = 0x%x\n", pcmds->cmds[idx].ss_txbuf[1]);
		idx = ss_get_cmd_idx(pcmds, 0xC3, 0x91);
		if (vdd->br_info.common_br.irc_mode == IRC_MODERATO_MODE) {
			pcmds->cmds[idx].ss_txbuf[2] = 0xFF;
			pcmds->cmds[idx].ss_txbuf[3] = 0x31;
			pcmds->cmds[idx].ss_txbuf[4] = 0x00;
		} else if (vdd->br_info.common_br.irc_mode == IRC_FLAT_GAMMA_MODE) {
			pcmds->cmds[idx].ss_txbuf[2] = 0xF4;
			pcmds->cmds[idx].ss_txbuf[3] = 0x33;
			pcmds->cmds[idx].ss_txbuf[4] = 0x6B;
		}
	}

	/* TSET */
	idx = ss_get_cmd_idx(pcmds, 0x0D, 0xB1);
	pcmds->cmds[idx].ss_txbuf[1] = vdd->br_info.temperature > 0 ?	vdd->br_info.temperature : (char)(BIT(7) | (-1*vdd->br_info.temperature));

	return pcmds;
}

static struct dsi_panel_cmd_set * ss_brightness_gamma_mode2_hmt(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;

	if (IS_ERR_OR_NULL(vdd)) {
	        LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_HMT);

	pcmds->cmds[0].ss_txbuf[1] = (vdd->br_info.common_br.gm2_wrdisbv & 0xFF00) >> 8;
	pcmds->cmds[0].ss_txbuf[2] = vdd->br_info.common_br.gm2_wrdisbv & 0xFF;

	LCD_INFO(vdd, "cd_idx: %d, cd_level: %d, WRDISBV: %x %x\n",
			vdd->hmt.cmd_idx,
			vdd->hmt.candela_level,
			pcmds->cmds[0].ss_txbuf[1],
			pcmds->cmds[0].ss_txbuf[2]);

	return pcmds;
}

enum VRR_CMD_RR {
	/* 1Hz is PSR mode in LPM (AOD) mode, 10Hz is PSR mode in 120HS mode */
	VRR_48NS = 0,
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

	if (scope == LFD_SCOPE_LPM) {
		base_rr = 30;
		max_div_def = 1;
		min_div_def = 30;
		min_div_lowest = 30;
		goto done;
	}

	vrr_id = ss_get_vrr_id(vdd);

	switch (vrr_id) {
	case VRR_48NS:
		base_rr = 48;
		max_div_def = min_div_def = min_div_lowest = 1; // 48hz
		break;
	case VRR_48HS:
		base_rr = 96;
		max_div_def = 2; // 48hz
		min_div_def = 8; // 12hz
		min_div_lowest = 96; // 1hz
		break;
	case VRR_60NS:
		base_rr = 60;
		max_div_def = 1; // 60hz
		min_div_def = min_div_lowest = 2; // 30hz
		break;
	case VRR_60HS:
		base_rr = 120;
		max_div_def = 2; // 60hz
		min_div_def = 12; // 10hz
		min_div_lowest = 120; // 1hz
		break;
	case VRR_96HS:
		base_rr = 96;
		max_div_def = 1; // 96hz
		min_div_def = 8; // 12hz
		min_div_lowest = 96; // 1hz
		break;
	case VRR_120HS:
		base_rr = 120;
		max_div_def = 1; // 120hz
		min_div_def = 12; // 10hz
		min_div_lowest = 120; // 1hz
		break;
	default:
		LCD_ERR(vdd, "invalid vrr_id\n");
		base_rr = 120;
		max_div_def = 1; // 120hz
		min_div_def = 12; // 10hz
		min_div_lowest = 120; // 1hz
		break;
	}

done:
	lfd_base->base_rr = base_rr;
	lfd_base->max_div_def = max_div_def;
	lfd_base->min_div_def = min_div_def;
	lfd_base->min_div_lowest = min_div_lowest;

	vrr->lfd.base_rr = base_rr;

	LCD_DEBUG(vdd, "LFD(%s): base_rr: %uhz, def: %uhz(%u)~%uhz(%u), lowest: %uhz(%u)\n",
			lfd_scope_name[scope],
			base_rr,
			DIV_ROUND_UP(base_rr, min_div_def), min_div_def,
			DIV_ROUND_UP(base_rr, max_div_def), max_div_def,
			DIV_ROUND_UP(base_rr, min_div_lowest), min_div_lowest);

	return 0;
}
/* 1. LFD min freq. and frame insertion (BDh 0x06th): BDh 0x6 offset, for LFD control
 * divider: 2nd param: (divder - 1)
 * frame insertion: 5th param: repeat count of current fps
 * 3rd param[7:4] : repeat count of half fps
 * 3rd param[3:0] : repeat count of quad fps
 *
 * ---------------------------------------------------------
 *     LFD_MIN  120HS~60HS  48HS~11HS  1HS       60NS~30NS
 * LFD_MAX
 * ---------------------------------------------------------
 * 120HS~96HS   10 00 01    14 00 01   EF 00 01
 *   60HS~1HS   10 00 01    00 00 02   EF 00 01
 *  60NS~30NS                                    00 00 01
 * ---------------------------------------------------------
 */
int ss_get_frame_insert_cmd(struct samsung_display_driver_data *vdd,
	u8 *out_cmd, u32 base_rr, u32 min_div, u32 max_div, bool cur_hs)
{
	u32 min_freq = DIV_ROUND_UP(base_rr, min_div);
	u32 max_freq = DIV_ROUND_UP(base_rr, max_div);

	out_cmd[1] = 0;
	if (cur_hs) {
		if (min_freq > 48) {
			/* LFD_MIN=120HS~60HS: 10 00 01 */
			out_cmd[0] = 0x10;
			out_cmd[2] = 0x01;
		} else if (min_freq == 1) {
			/* LFD_MIN=1HS: EF 00 01 */
			out_cmd[0] = 0xEF;
			out_cmd[2] = 0x01;
		} else if (max_freq > 60) {
			/* LFD_MIN=48HS~11HS, LFD_MAX=120HS~96HS: 14 00 01 */
			out_cmd[0] = 0x14;
			out_cmd[2] = 0x01;
		} else {
			/* LFD_MIN=48HS~11HS, LFD_MAX=60HS~1HS: 00 00 02 */
			out_cmd[0] = 0x00;
			out_cmd[2] = 0x02;
		}
	} else {
		/* NS mode: 00 00 01 */
		out_cmd[0] = 0x00;
		out_cmd[2] = 0x01;
	}
	LCD_INFO(vdd, "frame insert: %02X %02X %02X (LFD %uhz~%uhz, %s)\n",
			out_cmd[0], out_cmd[1], out_cmd[2], min_freq, max_freq,
			cur_hs ? "HS" : "NS");

	return 0;
}

#if 0 //for the reference keep me update
HS
	29 01 00 00 00 00 01 00 	/* 0. NULL packet to send vrr cmds at once, to prevent flicker during HS/NS transition */
	29 00 00 00 00 00 04 B0 00 AA C7			/* AOR_GLOBAL */
	29 00 00 00 00 00 07 C7 02 07 02 07 02 07 /* AOR */
	29 00 00 00 00 00 04 B0 00 06 BD
	29 00 00 00 00 00 06 BD 00 2C xx 00 01<<
	/* HS : 120Hz 2Frame, 60Hz 1Frame, 30Hz 4Frame (@120to10Hz) : 0x14(3th payload) */
	/* NS : 60Hz 2Frame, 30Hz 0Frame (@60to30Hz) : 0x00(3th payload) */
	29 00 00 00 00 00 02 BD 01
	29 00 00 00 00 00 04 B0 00 17 BD
	29 00 00 00 00 00 02 BD 40
	29 00 00 00 00 00 04 B0 00 03 F2
	29 00 00 00 00 00 02 F2 00			/* OSC_SETTING1 */<<
	29 00 00 00 00 00 04 B0 00 06 F2		/* VFP_GLOBAL1 */
	29 00 00 00 00 00 03 F2 00 10			/* VFP */
	29 00 00 00 00 00 04 B0 00 06 F2		/* VFP_GLOBAL2 */
	29 00 00 00 00 00 03 F2 00 10			/* VFP */
	29 00 00 00 00 00 04 B0 00 44 F2
	29 00 00 00 00 00 02 F2 00			/* OSC_SETTING2 */<<
	29 00 00 00 00 00 03 60 08 00		/* FREQ_SETTING1 */<<
	29 00 00 00 00 00 04 B0 00 34 6A
	29 00 00 00 00 00 02 6A 18			/* FREQ_SETTING2 */<<
	29 00 00 00 00 00 02 B9 01	/* TE modulation
					 * 00: default,
					 * 11: 60HS
					 * TE operates at every 2 frame
					 * TE_SEL & TE_FRAME_SEL = 1
					 */
	29 00 00 00 00 00 02 F7 0F		/* LTPS update */
NS
	29 01 00 00 00 00 01 00 	/* 0. NULL packet to send vrr cmds at once, to prevent flicker during HS/NS transition */
	29 00 00 00 00 00 04 B0 00 8A C7			/* AOR_GLOBAL */
	29 00 00 00 00 00 07 C7 02 07 02 07 02 07 /* AOR */
	29 00 00 00 00 00 04 B0 00 06 BD
	29 00 00 00 00 00 06 BD 00 04 xx 00 01 <<
	/* HS : 120Hz 2Frame, 60Hz 1Frame, 30Hz 4Frame (@120to10Hz) : 0x14(3th payload) */
	/* NS : 60Hz 2Frame, 30Hz 0Frame (@60to30Hz) : 0x00(3th payload) */
	29 00 00 00 00 00 02 BD 01
	29 00 00 00 00 00 04 B0 00 17 BD
	29 00 00 00 00 00 02 BD 40
	29 00 00 00 00 00 04 B0 00 03 F2
	29 00 00 00 00 00 02 F2 50			/* OSC_SETTING1 */<<
	29 00 00 00 00 00 04 B0 00 06 F2		/* VFP_GLOBAL1 */
	29 00 00 00 00 00 03 F2 00 10			/* VFP */
	29 00 00 00 00 00 04 B0 00 06 F2		/* VFP_GLOBAL2 */
	29 00 00 00 00 00 03 F2 00 10			/* VFP */
	29 00 00 00 00 00 04 B0 00 44 F2
	29 00 00 00 00 00 02 F2 80			/* OSC_SETTING2 */<<
	29 00 00 00 00 00 03 60 08 00		/* FREQ_SETTING1 */<<
	29 00 00 00 00 00 04 B0 00 34 6A
	29 00 00 00 00 00 02 6A 00			/* FREQ_SETTING2 */<<
	29 00 00 00 00 00 02 B9 01	/* TE modulation
					 * 00: default,
					 * 11: 60HS
					 * TE operates at every 2 frame
					 * TE_SEL & TE_FRAME_SEL = 1
					 */
	29 00 00 00 00 00 02 F7 0F		/* LTPS update */
#endif

enum VRR_CMD_ID {
	VRR_CMDID_AOR_GLOBAL = 0,
	VRR_CMDID_AOR = 1,
	VRR_CMDID_LFD_SET = 3,
	VRR_CMDID_OSC_SET1 = 8,
	VRR_CMDID_VFP_GLOBAL1 = 9,
	VRR_CMDID_VFP1 = 10,
	VRR_CMDID_VFP_GLOBAL2 = 11,
	VRR_CMDID_VFP2 = 12,
	VRR_CMDID_OSC_SET2 = 14,
	VRR_CMDID_FREQ_SET1 = 15,
	VRR_CMDID_FREQ_SET2 = 17,
	VRR_CMDID_TE_MOD = 18,
};

u8 cmdid_aor_global[VRR_MAX][3] = {
	{0x00, 0x8A, 0xC7},/*VRR_48NS*/
	{0x00, 0x8A, 0xC7},/*VRR_60NS*/
	{0x00, 0xAA, 0xC7},/*VRR_48HS*/
	{0x00, 0xAA, 0xC7},/*VRR_60HS*/
	{0x00, 0xAA, 0xC7},/*VRR_96HS*/
	{0x00, 0xAA, 0xC7},/*VRR_120HS*/
};

u8 cmdid_aor_normal[6] = {
	0x02, 0x07, 0x02, 0x07, 0x02, 0x07
};

u8 cmdid_aor_hmd[6] = {
	0x01, 0x0A, 0xA8, 0x00, 0x00, 0xC0
};

u8 cmdid_lfd_set[VRR_MAX][5] = {
	{0x00, 0x04, 0x00, 0x00, 0x01},/*VRR_48NS*/
	{0x00, 0x04, 0x00, 0x00, 0x01},/*VRR_60NS*/
	{0x00, 0x04, 0x14, 0x00, 0x01},/*VRR_48HS*/
	{0x00, 0x2C, 0x14, 0x00, 0x01},/*VRR_60HS*/
	{0x00, 0x00, 0x14, 0x00, 0x01},/*VRR_96HS*/
	{0x00, 0x2C, 0x14, 0x00, 0x01},/*VRR_120HS*/
};
u8 cmdid_osc_set1[VRR_MAX] = {
	0x50,/*VRR_48NS*/
	0x50,/*VRR_60NS*/
	0x00,/*VRR_48HS*/
	0x00,/*VRR_60HS*/
	0x00,/*VRR_96HS*/
	0x00,/*VRR_120HS*/
};

u8 cmdid_vfp[VRR_MAX][2] = {
	{0x03, 0x3C},/*VRR_48NS*/
	{0x00, 0x10},/*VRR_60NS*/
	{0x03, 0x3C},/*VRR_48HS*/
	{0x00, 0x10},/*VRR_60HS*/
	{0x03, 0x3C},/*VRR_96HS*/
	{0x00, 0x10},/*VRR_120HS*/
};
u8 cmdid_osc_set2[VRR_MAX] = {
	0x80,/*VRR_48NS*/
	0x80,/*VRR_60NS*/
	0x00,/*VRR_48HS*/
	0x00,/*VRR_60HS*/
	0x00,/*VRR_96HS*/
	0x00,/*VRR_120HS*/
};
u8 cmdid_freq_set1[VRR_MAX][2] = {
	{0x00, 0x00},/*VRR_48NS*/
	{0x00, 0x00},/*VRR_60NS*/
	{0x00, 0x04},/*VRR_48HS*/
	{0x00, 0x04},/*VRR_60HS*/
	{0x08, 0x00},/*VRR_96HS*/
	{0x08, 0x00},/*VRR_120HS*/
};
u8 cmdid_freq_set2[VRR_MAX] = {
	0x00,/*VRR_48NS*/
	0x00,/*VRR_60NS*/
	0x18,/*VRR_48HS*/
	0x18,/*VRR_60HS*/
	0x18,/*VRR_96HS*/
	0x18,/*VRR_120HS*/
};
u8 cmd_hw_te_mod[VRR_MAX] = {
	0x09,/*VRR_48NS*/
	0x09,/*VRR_60NS*/
	0x19,/*VRR_48HS*/
	0x19,/*VRR_60HS*/
	0x09,/*VRR_96HS*/
	0x09,/*VRR_120HS*/
};

static struct dsi_panel_cmd_set *__ss_vrr(struct samsung_display_driver_data *vdd,
					int *level_key, bool is_hbm, bool is_hmt)
{
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct dsi_panel_cmd_set  *vrr_cmds = ss_get_cmds(vdd, TX_VRR);

	struct vrr_info *vrr = &vdd->vrr;
	enum SS_BRR_MODE brr_mode = vrr->brr_mode;

	int cur_rr;
	bool cur_hs;
	enum VRR_CMD_RR vrr_id;
	u32 min_div, max_div;
	enum LFD_SCOPE_ID scope;
	u8 cmd_frame_insert[3];
	int cmd_idx = 0;

	if (SS_IS_CMDS_NULL(vrr_cmds)) {
		LCD_INFO(vdd, "no vrr cmds\n");
		return NULL;
	}

	if (panel && panel->cur_mode) {
		LCD_INFO(vdd, "VRR: cur_mode: %dx%d@%d%s, is_hbm: %d, is_hmt: %d, %s\n",
				panel->cur_mode->timing.h_active,
				panel->cur_mode->timing.v_active,
				panel->cur_mode->timing.refresh_rate,
				panel->cur_mode->timing.sot_hs_mode ? "HS" : "NM",
				is_hbm, is_hmt, ss_get_brr_mode_name(brr_mode));

		if (panel->cur_mode->timing.refresh_rate != vrr->adjusted_refresh_rate ||
				panel->cur_mode->timing.sot_hs_mode != vrr->adjusted_sot_hs_mode)
			LCD_DEBUG(vdd, "VRR: unmatched RR mode (%dhz%s / %dhz%s)\n",
					panel->cur_mode->timing.refresh_rate,
					panel->cur_mode->timing.sot_hs_mode ? "HS" : "NM",
					vrr->adjusted_refresh_rate,
					vrr->adjusted_sot_hs_mode ? "HS" : "NM");
	}

	cur_rr = vrr->cur_refresh_rate;
	cur_hs = vrr->cur_sot_hs_mode;

	if (is_hmt && !(cur_rr == 120 && cur_hs))
		LCD_ERR(vdd, "error: HMT not in 120HZ@HS mode, cur: %dhz%s\n",
				cur_rr, cur_hs ? "HS" : "NM");

	vrr_id = ss_get_vrr_id(vdd);

	scope = is_hmt ? LFD_SCOPE_HMD : LFD_SCOPE_NORMAL ;
	if (ss_get_lfd_div(vdd, scope, &min_div, &max_div)) {
		LCD_ERR(vdd, "fail to get LFD divider.. set default 1..\n");
		max_div = min_div = 1;
	}
	vrr->lfd.min_div = min_div;
	vrr->lfd.max_div = max_div;

	if (vdd->panel_revision > 0) {
		memcpy(&vrr_cmds->cmds[VRR_CMDID_AOR_GLOBAL].ss_txbuf[1],
				&cmdid_aor_global[vrr_id][0],
				vrr_cmds->cmds[VRR_CMDID_AOR_GLOBAL].msg.tx_len - 1);

		if (!is_hmt) {
			memcpy(&vrr_cmds->cmds[VRR_CMDID_AOR].ss_txbuf[1],
					&cmdid_aor_normal,
					vrr_cmds->cmds[VRR_CMDID_AOR].msg.tx_len - 1);
		} else {
			memcpy(&vrr_cmds->cmds[VRR_CMDID_AOR].ss_txbuf[1],
					&cmdid_aor_hmd,
					vrr_cmds->cmds[VRR_CMDID_AOR].msg.tx_len - 1);
		}

		vrr_cmds->cmds[VRR_CMDID_LFD_SET].ss_txbuf[2] = (min_div - 1) << 2;
		ss_get_frame_insert_cmd(vdd, cmd_frame_insert, vrr->lfd.base_rr, min_div, max_div, cur_hs);
		memcpy(&vrr_cmds->cmds[VRR_CMDID_LFD_SET].ss_txbuf[3], cmd_frame_insert, 3);

		memcpy(&vrr_cmds->cmds[VRR_CMDID_OSC_SET1].ss_txbuf[1],
				&cmdid_osc_set1[vrr_id],
				vrr_cmds->cmds[VRR_CMDID_OSC_SET1].msg.tx_len - 1);

		memcpy(&vrr_cmds->cmds[VRR_CMDID_VFP1].ss_txbuf[1],
				&cmdid_vfp[vrr_id],
				vrr_cmds->cmds[VRR_CMDID_VFP1].msg.tx_len - 1);

		memcpy(&vrr_cmds->cmds[VRR_CMDID_VFP2].ss_txbuf[1],
				&cmdid_vfp[vrr_id],
				vrr_cmds->cmds[VRR_CMDID_VFP2].msg.tx_len - 1);

		memcpy(&vrr_cmds->cmds[VRR_CMDID_OSC_SET2].ss_txbuf[1],
				&cmdid_osc_set2[vrr_id],
				vrr_cmds->cmds[VRR_CMDID_OSC_SET2].msg.tx_len - 1);

		memcpy(&vrr_cmds->cmds[VRR_CMDID_FREQ_SET1].ss_txbuf[1],
				&cmdid_freq_set1[vrr_id][0],
				vrr_cmds->cmds[VRR_CMDID_FREQ_SET1].msg.tx_len - 1);
		/* 8. Freq. (60h): frequency in image update case (non-LFD mode) */
		memcpy(&vrr_cmds->cmds[VRR_CMDID_FREQ_SET1].ss_txbuf[1],
				&cmdid_freq_set1[vrr_id][0],
				vrr_cmds->cmds[VRR_CMDID_FREQ_SET1].msg.tx_len - 1);

		vrr_cmds->cmds[VRR_CMDID_FREQ_SET1].ss_txbuf[2] = (max_div - 1) << 2;

		if (max_div == 1 && cur_hs)
			vrr_cmds->cmds[VRR_CMDID_FREQ_SET1].ss_txbuf[1] = 0x08;
		else
			vrr_cmds->cmds[VRR_CMDID_FREQ_SET1].ss_txbuf[1] = 0x00;

		if (vdd->panel_revision < 3) {
			memcpy(&vrr_cmds->cmds[VRR_CMDID_FREQ_SET2].ss_txbuf[1],
					&cmdid_freq_set2[vrr_id],
					vrr_cmds->cmds[VRR_CMDID_FREQ_SET2].msg.tx_len - 1);
		}
		/* 6. TE modulation (B9h) */
		memcpy(&vrr_cmds->cmds[VRR_CMDID_TE_MOD].ss_txbuf[1],
				&cmd_hw_te_mod[vrr_id],
				vrr_cmds->cmds[VRR_CMDID_TE_MOD].msg.tx_len - 1);
	}

	cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x3E3, 0xC6); /* gamma Enable Flag */
	if (cmd_idx > 0) {
		if (cur_rr == 96 || cur_rr == 48) {
			if (vdd->br_info.common_br.bl_level > 255 && vdd->br_info.common_br.bl_level < 261)
				vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x00; /* Gamma Disable */
			else
				vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x01; /* Gamma Enable */
		} else
			vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x00; /* Gamma Disable */
	}

	LCD_INFO(vdd, "VRR: (cur: %d%s, adj: %d%s, LFD: %uhz(%u)~%uhz(%u)\n",
			cur_rr,
			cur_hs ? "HS" : "NM",
			vrr->adjusted_refresh_rate,
			vrr->adjusted_sot_hs_mode ? "HS" : "NM",
			DIV_ROUND_UP(vrr->lfd.base_rr, min_div), min_div,
			DIV_ROUND_UP(vrr->lfd.base_rr, max_div), max_div);

	return vrr_cmds;
}

static struct dsi_panel_cmd_set *ss_vrr(struct samsung_display_driver_data *vdd, int *level_key)
{
	bool is_hbm = false;
	bool is_hmt = false;

	return __ss_vrr(vdd, level_key, is_hbm, is_hmt);
}

static struct dsi_panel_cmd_set *ss_vrr_hbm(struct samsung_display_driver_data *vdd, int *level_key)
{
	bool is_hbm = true;
	bool is_hmt = false;

	return __ss_vrr(vdd, level_key, is_hbm, is_hmt);
}

static struct dsi_panel_cmd_set *ss_vrr_hmt(struct samsung_display_driver_data *vdd, int *level_key)
{
	bool is_hbm = false;
	bool is_hmt = true;

	return __ss_vrr(vdd, level_key, is_hbm, is_hmt);
}

static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	char ddi_id[5];
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (D6h 1~5th) for ddi id */
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
	{0xff, 0x00, 0xf8, 0x00, 0xf0, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xf9, 0x00, 0xf6, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xfb, 0x00, 0xfd, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfb, 0x00, 0xf0, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xff, 0x00, 0xfd, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xfe, 0x00, 0xf0, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xff, 0x00, 0xf6, 0x00}, /* Tune_8 */
	{0xfc, 0x00, 0xff, 0x00, 0xfa, 0x00}, /* Tune_9 */
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
	unsigned char buf[11];
	int year, month, day;
	int hour, min;
	int x, y;
	int mdnie_tune_index = 0;
	int ret;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	if (ss_get_cmds(vdd, RX_MODULE_INFO)->count) {
		ret = ss_panel_data_read(vdd, RX_MODULE_INFO, buf, LEVEL1_KEY);
		if (ret) {
			LCD_ERR(vdd, "fail to read module ID, ret: %d", ret);
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

		LCD_INFO(vdd, "DSI%d : X-%d Y-%d \n", vdd->ndx,
			vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);

		/* CELL ID (manufacture date + white coordinates) */
		/* Manufacture Date */
		vdd->cell_id_dsi[0] = buf[4];
		vdd->cell_id_dsi[1] = buf[5];
		vdd->cell_id_dsi[2] = buf[6];
		vdd->cell_id_dsi[3] = buf[7];
		vdd->cell_id_dsi[4] = buf[8];
		vdd->cell_id_dsi[5] = buf[9];
		vdd->cell_id_dsi[6] = buf[10];
		/* White Coordinates */
		vdd->cell_id_dsi[7] = buf[0];
		vdd->cell_id_dsi[8] = buf[1];
		vdd->cell_id_dsi[9] = buf[2];
		vdd->cell_id_dsi[10] = buf[3];

		LCD_INFO(vdd, "DSI%d CELL ID : %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			vdd->ndx, vdd->cell_id_dsi[0],
			vdd->cell_id_dsi[1],	vdd->cell_id_dsi[2],
			vdd->cell_id_dsi[3],	vdd->cell_id_dsi[4],
			vdd->cell_id_dsi[5],	vdd->cell_id_dsi[6],
			vdd->cell_id_dsi[7],	vdd->cell_id_dsi[8],
			vdd->cell_id_dsi[9],	vdd->cell_id_dsi[10]);
	} else {
		LCD_ERR(vdd, "DSI%d no module_info_rx_cmds cmds(%d)", vdd->ndx, vdd->panel_revision);
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

static struct dsi_panel_cmd_set *ss_acl_on(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	pcmds = ss_get_cmds(vdd, TX_ACL_ON);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_ACL_ON..\n");
		return NULL;
	}
	if (vdd->br_info.common_br.bl_level <= MAX_BL_PF_LEVEL)
		pcmds->cmds[4].ss_txbuf[1] = 0x02;	/* ACL 15% in normal brightness */
	else
		pcmds->cmds[4].ss_txbuf[1] = 0x01;	/* ACL 8% in HBM */

	if (vdd->br_info.gradual_acl_val == 2)
		pcmds->cmds[4].ss_txbuf[1] = 0x03;	/* 0x03 : ACL 50% */

	LCD_INFO(vdd, "gradual_acl: %d, acl per: 0x%x",
			vdd->br_info.gradual_acl_val, pcmds->cmds[4].ss_txbuf[1]);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_acl_off(struct samsung_display_driver_data *vdd, int *level_key)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	LCD_INFO(vdd, "ACL off\n");
	return ss_get_cmds(vdd, TX_ACL_OFF);
}

static struct dsi_panel_cmd_set *ss_vint(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *vint_cmds = ss_get_cmds(vdd, TX_VINT);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd\n");
		return NULL;
	}

	if (IS_ERR_OR_NULL(vdd) || SS_IS_CMDS_NULL(vint_cmds)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)vint_cmds);
		return NULL;
	}

	/* VGH setting: 0x05: x-talk on, 0x0C: x-talk off */
	if (vdd->xtalk_mode)
		vint_cmds->cmds[1].ss_txbuf[1] = 0x05;
	else
		vint_cmds->cmds[1].ss_txbuf[1] = 0x0C;

	LCD_INFO(vdd, "xtalk_mode: %d\n", vdd->xtalk_mode);

	return vint_cmds;
}

static int ss_brightness_tot_level(struct samsung_display_driver_data *vdd)
{
	int tot_level_key = 0;

	/* HAB brightness setting requires F0h and FCh level key unlocked */
	/* DBV setting require TEST KEY3 (FCh) */
	tot_level_key = LEVEL1_KEY | LEVEL2_KEY;

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

#if 0 //for the reference keep me update
lpm_on_tx_cmds_revA
	29 00 00 00 00 00 03 F0 5A 5A
	29 00 00 00 00 00 02 BB 09			/* HLPM/ALPM Control: 0x09: HLPM, 0x0B: ALPM */
	29 00 00 00 00 00 04 B0 00 4E C7
	29 00 00 00 00 00 05 C7 FF FF FF FF /* HLPM/ALPM Control */
	29 00 00 00 00 00 04 B0 00 17 BD
	29 00 00 00 00 00 02 BD 61			/* LFD Setting */
	29 00 00 00 00 00 04 B0 00 53 C7
	29 00 00 00 00 00 07 C7 01 0C 4C 02 00 C0 /* HLPM/ALPM Control */
	29 00 00 00 00 00 04 B0 00 11 C2
	29 00 00 00 00 00 02 C2 2F
	29 00 00 00 00 00 04 B0 00 53 C7
	29 00 00 00 00 00 07 C7 01 00 C0 02 00 C0 /* 0x00, 0xC0 : 60nit *//* 0x06, 0xB8 : 30nit *//* 0x0A, 0xB4 : 10nit *//* 0x0C, 0x4C : 2nit */
	29 00 00 00 00 00 03 53 24
	29 00 00 00 00 00 02 F7 0F
	29 01 00 00 00 00 03 F0 A5 A5

lpm_off_tx_cmds_revA
	29 00 00 00 00 00 03 F0 5A 5A
	29 00 00 00 00 00 02 BB 01			/* HLPM/ALPM Control: 0x09: HLPM, 0x0B: ALPM */
	29 00 00 00 00 00 04 B0 00 11 C2
	29 00 00 00 00 00 C2 16 			/* ELVSS Control */
	29 00 00 00 00 00 04 B0 00 53 C7
	29 00 00 00 00 00 02 C7 00
	29 00 00 00 00 00 04 B0 00 17 BD
	29 00 00 00 00 00 02 BD 60			/* LFD Setting */
	29 00 00 00 00 00 02 F7 0F
	29 01 00 00 00 00 03 F0 A5 A5
#endif
#if 0
u8 cmd_aod_freq[LFD_MINFREQ_LPM_MAX][8] = {
	{0x12, 0x06, 0x1F, 0x00, 0x02, 0x00, 0x56, 0x02},/* AOD 30HZ */
	{0x01, 0x00, 0x1F, 0x00, 0x02, 0x00, 0x00, 0x00},/* AOD 1HZ */
};
#endif
static u8 hlpm_aor[4][32] = {
	{0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0,
	0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0, 0x00, 0xC0},
	{0x06, 0xB8, 0x06, 0xB8, 0x06, 0xB8, 0x06, 0xB8, 0x06, 0xB8, 0x06, 0xB8, 0x06, 0xB8, 0x06, 0xB8,
	 0x06, 0xB8, 0x06, 0xB8, 0x06, 0xB8, 0x06, 0xB8, 0x06, 0xB8, 0x06, 0xB8, 0x06, 0xB8, 0x06, 0xB8},
	{0x0A, 0xB4, 0x0A, 0xB4, 0x0A, 0xB4, 0x0A, 0xB4, 0x0A, 0xB4, 0x0A, 0xB4, 0x0A, 0xB4, 0x0A, 0xB4,
	 0x0A, 0xB4, 0x0A, 0xB4, 0x0A, 0xB4, 0x0A, 0xB4, 0x0A, 0xB4, 0x0A, 0xB4, 0x0A, 0xB4, 0x0A, 0xB4},
	{0x0C, 0x4C, 0x0C, 0x4C, 0x0C, 0x4C, 0x0C, 0x4C, 0x0C, 0x4C, 0x0C, 0x4C, 0x0C, 0x4C, 0x0C, 0x4C,
	 0x0C, 0x4C, 0x0C, 0x4C, 0x0C, 0x4C, 0x0C, 0x4C, 0x0C, 0x4C, 0x0C, 0x4C, 0x0C, 0x4C, 0x0C, 0x4C},
};

static void ss_set_panel_lpm_brightness(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *set = ss_get_cmds(vdd, TX_LPM_BL_CMD);
	struct dsi_panel_cmd_set *set_lpm_bl;
#if 0
	struct vrr_info *vrr = &vdd->vrr;
	u32 min_div, max_div;
#endif
	int cmd_idx;

	if (SS_IS_CMDS_NULL(set)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_BL_CMD\n");
		return;
	}

	cmd_idx = ss_get_cmd_idx(set, 0xE7, 0xC7);
	/* LPM_ON: 3. HLPM brightness */
	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_60NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
		memcpy(&set->cmds[cmd_idx].ss_txbuf[1], hlpm_aor[0], sizeof(hlpm_aor[0]));
		break;
	case LPM_30NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
		memcpy(&set->cmds[cmd_idx].ss_txbuf[1], hlpm_aor[1], sizeof(hlpm_aor[1]));
		break;
	case LPM_10NIT:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
		memcpy(&set->cmds[cmd_idx].ss_txbuf[1], hlpm_aor[2], sizeof(hlpm_aor[2]));
		break;
	case LPM_2NIT:
	default:
		set_lpm_bl = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
		memcpy(&set->cmds[cmd_idx].ss_txbuf[1], hlpm_aor[3], sizeof(hlpm_aor[3]));
		break;
	}

	if (SS_IS_CMDS_NULL(set_lpm_bl)) {
		LCD_ERR(vdd, "No cmds for alpm_ctrl..\n");
		return;
	}

	cmd_idx = ss_get_cmd_idx(set, 0x53, 0xC7);
	memcpy(&set->cmds[cmd_idx].ss_txbuf[1],
			&set_lpm_bl->cmds->ss_txbuf[1],
			sizeof(char) * (set->cmds[cmd_idx].msg.tx_len - 1));
#if 0
	ss_get_lfd_div(vdd, LFD_SCOPE_LPM, &min_div, &max_div);
	vrr->lfd.min_div = min_div;
	vrr->lfd.max_div = 1;

	if (vdd->panel_revision > 5) {
		cmd_idx = ss_get_cmd_idx(set, 0x04, 0xBD);/* LFD setting */
		if (min_div == 1) /* AOD 30HZ */
			set->cmds[cmd_idx].ss_txbuf[2] = 0x00;
		else
			set->cmds[cmd_idx].ss_txbuf[2] = 0x74;
	}
	cmd_idx = ss_get_cmd_idx(set, 0x161, 0xCB);
	if (cmd_idx < 0) {
		LCD_ERR(vdd, "No post-bias cmd at panel_revision = %c\n",
			vdd->panel_revision + 'A');
	} else {
		if (min_div == 1) /* AOD 30HZ */
			memcpy(&set->cmds[cmd_idx].ss_txbuf[1],
						&cmd_aod_freq[LFD_MINFREQ_LPM_30HZ],
						sizeof(char) * set->cmds[cmd_idx].msg.tx_len - 1);
		else /* AOD 1HZ */
			memcpy(&set->cmds[cmd_idx].ss_txbuf[1],
						&cmd_aod_freq[LFD_MINFREQ_LPM_1HZ],
						sizeof(char) * set->cmds[cmd_idx].msg.tx_len - 1);
	}
#endif
	/* send lpm bl cmd */
	ss_send_cmd(vdd, TX_LPM_BL_CMD);

	LCD_INFO(vdd, "[Panel LPM] bl_level : %s\n",
			/* Check current brightness level */
			vdd->panel_lpm.lpm_bl_level == LPM_2NIT ? "2NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_10NIT ? "10NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_30NIT ? "30NIT" :
			vdd->panel_lpm.lpm_bl_level == LPM_60NIT ? "60NIT" : "UNKNOWN");
}

/*
 * This function will update parameters for ALPM_REG/ALPM_CTRL_REG
 * Parameter for ALPM_REG : Control brightness for panel LPM
 * Parameter for ALPM_CTRL_REG : Change panel LPM mode for ALPM/HLPM
 * mode, brightness, hz are updated here.
 */
static void ss_update_panel_lpm_ctrl_cmd
				(struct samsung_display_driver_data *vdd, int enable)
{
	struct dsi_panel_cmd_set *set_lpm_on = ss_get_cmds(vdd, TX_LPM_ON);
	struct dsi_panel_cmd_set *set_lpm_off = ss_get_cmds(vdd, TX_LPM_OFF);
	struct dsi_panel_cmd_set *set_lpm_bl = NULL;
#if 0
	struct vrr_info *vrr = &vdd->vrr;
	u32 min_div, max_div;
#endif
	int cmd_idx;
	int gm2_wrdisbv;

	LCD_INFO(vdd, "%s++\n", __func__);

	if (SS_IS_CMDS_NULL(set_lpm_on) || SS_IS_CMDS_NULL(set_lpm_off)) {
		LCD_ERR(vdd, "No cmds for TX_LPM_ON/OFF\n");
		return;
	}

	/* LPM_ON: 1. HLPM/ALPM Control: 0x09: HLPM, 0x0B: ALPM */
	/* LPM_OFF: 1. Seamless Control For HLPM: 0x01: HLPM, 0x03: ALPM */
	switch (vdd->panel_lpm.mode) {
	case HLPM_MODE_ON:
		cmd_idx = ss_get_cmd_idx(set_lpm_on, 0x00, 0xBB);
		set_lpm_on->cmds[cmd_idx].ss_txbuf[1] = 0x29;
		cmd_idx = ss_get_cmd_idx(set_lpm_off, 0x00, 0xBB);
		set_lpm_off->cmds[cmd_idx].ss_txbuf[1] = 0x21;
		break;
	case ALPM_MODE_ON:
		cmd_idx = ss_get_cmd_idx(set_lpm_on, 0x00, 0xBB);
		set_lpm_on->cmds[cmd_idx].ss_txbuf[1] = 0x2B;
		cmd_idx = ss_get_cmd_idx(set_lpm_off, 0x00, 0xBB);
		set_lpm_off->cmds[cmd_idx].ss_txbuf[1] = 0x23;
		break;
	default:
		LCD_ERR(vdd, "invalid lpm mode: %d\n", vdd->panel_lpm.mode);
		break;
	}

	/* LPM_ON: 3. HLPM brightness */
	/* should restore normal brightness in LPM off sequence to prevent flicker.. */
	cmd_idx = ss_get_cmd_idx(set_lpm_on, 0xE7, 0xC7);
	if (cmd_idx >= 0) {
		switch (vdd->panel_lpm.lpm_bl_level) {
		case LPM_60NIT:
			set_lpm_bl = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);
			gm2_wrdisbv = 242;
			memcpy(&set_lpm_on->cmds[cmd_idx].ss_txbuf[1], hlpm_aor[0], sizeof(hlpm_aor[0]));
			break;
		case LPM_30NIT:
			set_lpm_bl = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
			gm2_wrdisbv = 124;
			memcpy(&set_lpm_on->cmds[cmd_idx].ss_txbuf[1], hlpm_aor[1], sizeof(hlpm_aor[1]));
			break;
		case LPM_10NIT:
			set_lpm_bl = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
			gm2_wrdisbv = 40;
			memcpy(&set_lpm_on->cmds[cmd_idx].ss_txbuf[1], hlpm_aor[2], sizeof(hlpm_aor[2]));
			break;
		case LPM_2NIT:
		default:
			set_lpm_bl = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
			gm2_wrdisbv = 8;
			memcpy(&set_lpm_on->cmds[cmd_idx].ss_txbuf[1], hlpm_aor[3], sizeof(hlpm_aor[3]));
			break;
		}
	}

	if (SS_IS_CMDS_NULL(set_lpm_bl)) {
		LCD_ERR(vdd, "No cmds for alpm_ctrl..\n");
		return;
	}

	cmd_idx = ss_get_cmd_idx(set_lpm_off, 0x00, 0x51);
	set_lpm_off->cmds[cmd_idx].ss_txbuf[1] = (gm2_wrdisbv & 0xFF00) >> 8;
	set_lpm_off->cmds[cmd_idx].ss_txbuf[2] = gm2_wrdisbv & 0xFF;

	/* there are two B0 00 53 C7 cmd & we need to ctrl second one */
	cmd_idx = ss_get_cmd_idx(set_lpm_on, 0x53, 0xC7) + 4;
	memcpy(&set_lpm_on->cmds[cmd_idx].ss_txbuf[1],
				&set_lpm_bl->cmds->ss_txbuf[1],
				sizeof(char) * set_lpm_on->cmds[cmd_idx].msg.tx_len - 1);
#if 0
	ss_get_lfd_div(vdd, LFD_SCOPE_LPM, &min_div, &max_div);
	vrr->lfd.min_div = min_div;
	vrr->lfd.max_div = 1;

	if (vdd->panel_revision > 5) {
		cmd_idx = ss_get_cmd_idx(set_lpm_on, 0x04, 0xBD);/* LFD setting */
		if (min_div == 1) /* AOD 30HZ */
			set_lpm_on->cmds[cmd_idx].ss_txbuf[2] = 0x00;
		else
			set_lpm_on->cmds[cmd_idx].ss_txbuf[2] = 0x74;
	}

	cmd_idx = ss_get_cmd_idx(set_lpm_on, 0x161, 0xCB);/* post-bias setting*/
	if (cmd_idx < 0) {
		LCD_ERR(vdd, "No cmds for post-bias setting panel_revision = %c\n",
			vdd->panel_revision + 'A');
	} else {
		if (min_div == 1) /* AOD 30HZ */
			memcpy(&set_lpm_on->cmds[cmd_idx].ss_txbuf[1],
						&cmd_aod_freq[LFD_MINFREQ_LPM_30HZ],
						sizeof(char) * set_lpm_on->cmds[cmd_idx].msg.tx_len - 1);
		else /* AOD 1HZ */
			memcpy(&set_lpm_on->cmds[cmd_idx].ss_txbuf[1],
						&cmd_aod_freq[LFD_MINFREQ_LPM_1HZ],
						sizeof(char) * set_lpm_on->cmds[cmd_idx].msg.tx_len - 1);
	}
#endif
	LCD_INFO(vdd, "%s--\n", __func__);
}
static int ss_elvss_read(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (C2h 22th) for grayspot */
	ss_panel_data_read(vdd, RX_ELVSS, vdd->br_info.common_br.elvss_value, LEVEL1_KEY);

	return true;
}

static void ss_gray_spot(struct samsung_display_driver_data *vdd, int enable)
{
	struct dsi_panel_cmd_set *pcmds;

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
		pcmds->cmds[8].ss_txbuf[1] = vdd->br_info.common_br.elvss_value[0];
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
	mdnie_data->DSI_COLOR_BLIND_MDNIE_1 = DSI_COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_1 = DSI_RGB_SENSOR_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_2 = DSI_RGB_SENSOR_MDNIE_2;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_3 = DSI_RGB_SENSOR_MDNIE_3;
	mdnie_data->DSI_TRANS_DIMMING_MDNIE = DSI_RGB_SENSOR_MDNIE_3;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE_2 = DSI_UI_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_UI_STANDARD_MDNIE_2 = DSI_UI_STANDARD_MDNIE_2;
	mdnie_data->DSI_UI_AUTO_MDNIE_2 = DSI_UI_AUTO_MDNIE_2;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE_2 = DSI_VIDEO_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE_2 = DSI_VIDEO_STANDARD_MDNIE_2;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE_2 = DSI_VIDEO_AUTO_MDNIE_2;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE_2 = DSI_CAMERA_AUTO_MDNIE_2;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE_2 = DSI_GALLERY_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE_2 = DSI_GALLERY_STANDARD_MDNIE_2;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE_2 = DSI_GALLERY_AUTO_MDNIE_2;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE_2 = DSI_BROWSER_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE_2 = DSI_BROWSER_STANDARD_MDNIE_2;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE_2 = DSI_BROWSER_AUTO_MDNIE_2;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE_2 = DSI_EBOOK_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE_2 = DSI_EBOOK_STANDARD_MDNIE_2;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE_2 = DSI_EBOOK_AUTO_MDNIE_2;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE_2 = DSI_TDMB_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE_2 = DSI_TDMB_STANDARD_MDNIE_2;
	mdnie_data->DSI_TDMB_AUTO_MDNIE_2 = DSI_TDMB_AUTO_MDNIE_2;

	mdnie_data->DSI_BYPASS_MDNIE = DSI_BYPASS_MDNIE;
	mdnie_data->DSI_NEGATIVE_MDNIE = DSI_NEGATIVE_MDNIE;
	mdnie_data->DSI_COLOR_BLIND_MDNIE = DSI_COLOR_BLIND_MDNIE;
	mdnie_data->DSI_HBM_CE_MDNIE = DSI_HBM_CE_MDNIE;
	mdnie_data->DSI_HBM_CE_D65_MDNIE = DSI_HBM_CE_D65_MDNIE;
	mdnie_data->DSI_RGB_SENSOR_MDNIE = DSI_RGB_SENSOR_MDNIE;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE = DSI_UI_DYNAMIC_MDNIE;
	mdnie_data->DSI_UI_STANDARD_MDNIE = DSI_UI_STANDARD_MDNIE;
	mdnie_data->DSI_UI_NATURAL_MDNIE = DSI_UI_NATURAL_MDNIE;
	mdnie_data->DSI_UI_AUTO_MDNIE = DSI_UI_AUTO_MDNIE;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE = DSI_VIDEO_DYNAMIC_MDNIE;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE = DSI_VIDEO_STANDARD_MDNIE;
	mdnie_data->DSI_VIDEO_NATURAL_MDNIE = DSI_VIDEO_NATURAL_MDNIE;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE = DSI_VIDEO_AUTO_MDNIE;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE = DSI_CAMERA_AUTO_MDNIE;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE = DSI_GALLERY_DYNAMIC_MDNIE;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE = DSI_GALLERY_STANDARD_MDNIE;
	mdnie_data->DSI_GALLERY_NATURAL_MDNIE = DSI_GALLERY_NATURAL_MDNIE;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE = DSI_GALLERY_AUTO_MDNIE;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE = DSI_BROWSER_DYNAMIC_MDNIE;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE = DSI_BROWSER_STANDARD_MDNIE;
	mdnie_data->DSI_BROWSER_NATURAL_MDNIE = DSI_BROWSER_NATURAL_MDNIE;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE = DSI_BROWSER_AUTO_MDNIE;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE = DSI_EBOOK_DYNAMIC_MDNIE;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE = DSI_EBOOK_STANDARD_MDNIE;
	mdnie_data->DSI_EBOOK_NATURAL_MDNIE = DSI_EBOOK_NATURAL_MDNIE;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE = DSI_EBOOK_AUTO_MDNIE;
	mdnie_data->DSI_EMAIL_AUTO_MDNIE = DSI_EMAIL_AUTO_MDNIE;
	mdnie_data->DSI_GAME_LOW_MDNIE = DSI_GAME_LOW_MDNIE;
	mdnie_data->DSI_GAME_MID_MDNIE = DSI_GAME_MID_MDNIE;
	mdnie_data->DSI_GAME_HIGH_MDNIE = DSI_GAME_HIGH_MDNIE;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE = DSI_TDMB_DYNAMIC_MDNIE;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE = DSI_TDMB_STANDARD_MDNIE;
	mdnie_data->DSI_TDMB_NATURAL_MDNIE = DSI_TDMB_NATURAL_MDNIE;
	mdnie_data->DSI_TDMB_AUTO_MDNIE = DSI_TDMB_AUTO_MDNIE;
	mdnie_data->DSI_GRAYSCALE_MDNIE = DSI_GRAYSCALE_MDNIE;
	mdnie_data->DSI_GRAYSCALE_NEGATIVE_MDNIE = DSI_GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data->DSI_CURTAIN = DSI_SCREEN_CURTAIN_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE = DSI_NIGHT_MODE_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE_SCR = DSI_NIGHT_MODE_MDNIE_1;
	mdnie_data->DSI_COLOR_LENS_MDNIE = DSI_COLOR_LENS_MDNIE;
	mdnie_data->DSI_COLOR_LENS_MDNIE_SCR = DSI_COLOR_LENS_MDNIE_1;
	mdnie_data->DSI_COLOR_BLIND_MDNIE_SCR = DSI_COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_SCR = DSI_RGB_SENSOR_MDNIE_1;
	mdnie_data->DSI_AFC = DSI_AFC;
	mdnie_data->DSI_AFC_ON = DSI_AFC_ON;
	mdnie_data->DSI_AFC_OFF = DSI_AFC_OFF;

	mdnie_data->mdnie_tune_value_dsi = mdnie_tune_value_dsi;
	mdnie_data->hmt_color_temperature_tune_value_dsi = hmt_color_temperature_tune_value_dsi;

	mdnie_data->hdr_tune_value_dsi = hdr_tune_value_dsi;

	mdnie_data->light_notification_tune_value_dsi = light_notification_tune_value_dsi;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data->dsi_bypass_mdnie_size = ARRAY_SIZE(DSI_BYPASS_MDNIE);
	mdnie_data->mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET;
	mdnie_data->mdnie_step_index[MDNIE_STEP1] = MDNIE_STEP1_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP2] = MDNIE_STEP2_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP3] = MDNIE_STEP3_INDEX;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_RED_OFFSET] = ADDRESS_SCR_WHITE_RED;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_GREEN_OFFSET] = ADDRESS_SCR_WHITE_GREEN;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_BLUE_OFFSET] = ADDRESS_SCR_WHITE_BLUE;
	mdnie_data->dsi_rgb_sensor_mdnie_1_size = DSI_RGB_SENSOR_MDNIE_1_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_2_size = DSI_RGB_SENSOR_MDNIE_2_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_3_size = DSI_RGB_SENSOR_MDNIE_3_SIZE;

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

static int ss_gct_read(struct samsung_display_driver_data *vdd)
{
	u8 valid_checksum[4] = {0x0F, 0x4F, 0x0F, 0x4F};
	int res;

	if (!vdd->gct.is_support)
		return GCT_RES_CHECKSUM_NOT_SUPPORT;

	if (!vdd->gct.on)
		return GCT_RES_CHECKSUM_OFF;

	if (!memcmp(vdd->gct.checksum, valid_checksum, 4))
		res = GCT_RES_CHECKSUM_PASS;
	else
		res = GCT_RES_CHECKSUM_NG;

	return res;
}

static int ss_gct_write(struct samsung_display_driver_data *vdd)
{
	u8 *checksum;
	int i;
	/* vddm set, 0x0: normal, 0x01: LV, 0x02: HV */
	u8 vddm_set[MAX_VDDM] = {0x0, 0x01, 0x02};
	int ret = 0;
	struct dsi_panel *panel = GET_DSI_PANEL(vdd);
	struct dsi_display *dsi_display = GET_DSI_DISPLAY(vdd);
	int wait_cnt = 1000; /* 1000 * 0.5ms = 500ms */

	LCD_INFO(vdd, "+\n");

	vdd->gct.is_running = true;

	/* prevent sw reset to trigger esd recovery */
	LCD_INFO(vdd, "disable esd interrupt\n");
	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd);

	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.enable = 1;
	while (!list_empty(&vdd->cmd_lock.wait_list) && --wait_cnt)
		usleep_range(500, 500);

	for (i = TX_GCT_ENTER; i <= TX_GCT_EXIT; i++)
		ss_set_exclusive_tx_packet(vdd, i, 1);
	ss_set_exclusive_tx_packet(vdd, RX_GCT_CHECKSUM, 1);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 1);

	usleep_range(10000, 11000);

	checksum = vdd->gct.checksum;
	for (i = VDDM_LV; i < MAX_VDDM; i++) {
		struct dsi_panel_cmd_set *set;

		LCD_INFO(vdd, "(%d) TX_GCT_ENTER\n", i);
		/* VDDM LV set (0x0: 1.0V, 0x10: 0.9V, 0x30: 1.1V) */
		set = ss_get_cmds(vdd, TX_GCT_ENTER);
		if (SS_IS_CMDS_NULL(set)) {
			LCD_ERR(vdd, "No cmds for TX_GCT_ENTER..\n");
			break;
		}
		set->cmds[15].ss_txbuf[1] = vddm_set[i];
		ss_send_cmd(vdd, TX_GCT_ENTER);

		msleep(150);

		ss_panel_data_read(vdd, RX_GCT_CHECKSUM, checksum++,
				LEVEL_KEY_NONE);
		LCD_INFO(vdd, "(%d) read checksum: %x\n", i, *(checksum - 1));

		LCD_INFO(vdd, "(%d) TX_GCT_MID\n", i);
		ss_send_cmd(vdd, TX_GCT_MID);

		msleep(150);

		ss_panel_data_read(vdd, RX_GCT_CHECKSUM, checksum++,
				LEVEL_KEY_NONE);

		LCD_INFO(vdd, "(%d) read checksum: %x\n", i, *(checksum - 1));

		LCD_INFO(vdd, "(%d) TX_GCT_EXIT\n", i);
		ss_send_cmd(vdd, TX_GCT_EXIT);
	}

	vdd->gct.on = 1;

	LCD_INFO(vdd, "checksum = {%x %x %x %x}\n",
			vdd->gct.checksum[0], vdd->gct.checksum[1],
			vdd->gct.checksum[2], vdd->gct.checksum[3]);

	if (dsi_display) {
		if (dsi_display->enabled == false) {
			LCD_ERR(vdd, "dsi_display is not enabled.. it may be turning off !!!\n");
			goto end;
		}
	}

	/* exit exclusive mode*/
	for (i = TX_GCT_ENTER; i <= TX_GCT_EXIT; i++)
		ss_set_exclusive_tx_packet(vdd, i, 0);
	ss_set_exclusive_tx_packet(vdd, RX_GCT_CHECKSUM, 0);
	ss_set_exclusive_tx_packet(vdd, TX_REG_READ_POS, 0);

	/* Reset panel to enter nornal panel mode.
	 * The on commands also should be sent before update new frame.
	 * Next new frame update is blocked by exclusive_tx.enable
	 * in ss_event_frame_update_pre(), and it will be released
	 * by wake_up exclusive_tx.ex_tx_waitq.
	 * So, on commands should be sent before wake up the waitq
	 * and set exclusive_tx.enable to false.
	 */
	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_OFF, 1);
	ss_send_cmd(vdd, DSI_CMD_SET_OFF);

	vdd->panel_state = PANEL_PWR_OFF;
	dsi_panel_power_off(panel);
	dsi_panel_power_on(panel);
	vdd->panel_state = PANEL_PWR_ON_READY;

	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_ON, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_ENABLE, 1);
	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_PPS, 1);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_DISABLE, 1);

	ss_send_cmd(vdd, DSI_CMD_SET_ON);
	dsi_panel_update_pps(panel);

	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_OFF, 0);
	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_ON, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_ENABLE, 0);
	ss_set_exclusive_tx_packet(vdd, DSI_CMD_SET_PPS, 0);
	ss_set_exclusive_tx_packet(vdd, TX_LEVEL0_KEY_DISABLE, 0);

	vdd->exclusive_tx.enable = 0;
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);

	vdd->mafpc.force_delay = true;
	ss_panel_on_post(vdd);

	vdd->gct.is_running = false;

	/* enable esd interrupt */
	LCD_INFO(vdd, "enable esd interrupt\n");
	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(true, true, (void *)vdd);

	return ret;

end:
	vdd->exclusive_tx.enable = 0;
	wake_up_all(&vdd->exclusive_tx.ex_tx_waitq);
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);

	vdd->gct.is_running = false;

	return ret;
}

static int ss_self_display_data_init(struct samsung_display_driver_data *vdd)
{
	u32 panel_type = 0;
	u32 panel_color = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	LCD_INFO(vdd, "Self Display Panel Data init\n");

	panel_type = (ss_panel_id0_get(vdd) & 0x30) >> 4;
	panel_color = ss_panel_id0_get(vdd) & 0xF;

	LCD_INFO(vdd, "Panel Type=0x%x, Panel Color=0x%x\n", panel_type, panel_color);

	vdd->self_disp.operation[FLAG_SELF_MASK].img_buf = self_mask_img_data;
	vdd->self_disp.operation[FLAG_SELF_MASK].img_size = ARRAY_SIZE(self_mask_img_data);
	vdd->self_disp.operation[FLAG_SELF_MASK].img_checksum = SELF_MASK_IMG_CHECKSUM;
	make_self_dispaly_img_cmds_HAD(vdd, TX_SELF_MASK_IMAGE, FLAG_SELF_MASK);

	vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_buf = self_mask_img_wqhd_crc_data;
	vdd->self_disp.operation[FLAG_SELF_MASK_CRC].img_size = ARRAY_SIZE(self_mask_img_wqhd_crc_data);
	make_mass_self_display_img_cmds_HAD(vdd, TX_SELF_MASK_IMAGE_CRC, FLAG_SELF_MASK_CRC);

	vdd->self_disp.operation[FLAG_SELF_ICON].img_buf = self_icon_img_data;
	vdd->self_disp.operation[FLAG_SELF_ICON].img_size = ARRAY_SIZE(self_icon_img_data);
	make_mass_self_display_img_cmds_HAD(vdd, TX_SELF_ICON_IMAGE, FLAG_SELF_ICON);

	vdd->self_disp.operation[FLAG_SELF_ACLK].img_buf = self_aclock_img_data;
	vdd->self_disp.operation[FLAG_SELF_ACLK].img_size = ARRAY_SIZE(self_aclock_img_data);
	make_mass_self_display_img_cmds_HAD(vdd, TX_SELF_ACLOCK_IMAGE, FLAG_SELF_ACLK);

	vdd->self_disp.operation[FLAG_SELF_DCLK].img_buf = self_dclock_img_data;
	vdd->self_disp.operation[FLAG_SELF_DCLK].img_size = ARRAY_SIZE(self_dclock_img_data);
	make_mass_self_display_img_cmds_HAD(vdd, TX_SELF_DCLOCK_IMAGE, FLAG_SELF_DCLK);

	vdd->self_disp.operation[FLAG_SELF_VIDEO].img_buf = self_video_img_data;
	vdd->self_disp.operation[FLAG_SELF_VIDEO].img_size = ARRAY_SIZE(self_video_img_data);
	make_mass_self_display_img_cmds_HAD(vdd, TX_SELF_VIDEO_IMAGE, FLAG_SELF_VIDEO);

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

	if (vdd->mafpc.make_img_mass_cmds)
		vdd->mafpc.make_img_mass_cmds(vdd, vdd->mafpc.img_buf, vdd->mafpc.img_size, TX_MAFPC_IMAGE); /* Image Data */
	else if (vdd->mafpc.make_img_cmds)
		vdd->mafpc.make_img_cmds(vdd, vdd->mafpc.img_buf, vdd->mafpc.img_size, TX_MAFPC_IMAGE); /* Image Data */
	else {
		LCD_ERR(vdd, "Can not make mafpc image commands\n");
		return -EINVAL;
	}

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

	return true;
}


static void ss_copr_panel_init(struct samsung_display_driver_data *vdd)
{
	vdd->copr.display_read = 0;
	vdd->copr.ver = COPR_VER_6P0;
	ss_copr_init(vdd);
}

static int poc_comp_table[][2] = {
					/*idx  cd */
	{0x53,	0x1A},	/*0 	2 */
	{0x53,	0x1A},	/*1 	3 */
	{0x53,	0x1A},	/*2 	4 */
	{0x53,	0x1A},	/*3 	5 */
	{0x53,	0x1A},	/*4 	6 */
	{0x53,	0x1A},	/*5 	7 */
	{0x53,	0x1A},	/*6 	8 */
	{0x53,	0x1A},	/*7 	9 */
	{0x53,	0x1A},	/*8 	10 */
	{0x53,	0x1A},	/*9 	11 */
	{0x53,	0x1A},	/*10	12 */
	{0x53,	0x1A},	/*11	13 */
	{0x53,	0x1A},	/*12	14 */
	{0x53,	0x1A},	/*13	15 */
	{0x53,	0x1B},	/*14	16 */
	{0x53,	0x1B},	/*15	17 */
	{0x53,	0x1C},	/*16	19 */
	{0x53,	0x1D},	/*17	20 */
	{0x53,	0x1D},	/*18	21 */
	{0x53,	0x1E},	/*19	22 */
	{0x53,	0x1F},	/*20	24 */
	{0x53,	0x1F},	/*21	25 */
	{0x53,	0x20},	/*22	27 */
	{0x53,	0x21},	/*23	29 */
	{0x53,	0x22},	/*24	30 */
	{0x53,	0x23},	/*25	32 */
	{0x53,	0x24},	/*26	34 */
	{0x53,	0x25},	/*27	37 */
	{0x53,	0x26},	/*28	39 */
	{0x53,	0x27},	/*29	41 */
	{0x53,	0x29},	/*30	44 */
	{0x53,	0x2A},	/*31	47 */
	{0x53,	0x2C},	/*32	50 */
	{0x53,	0x2D},	/*33	53 */
	{0x53,	0x2F},	/*34	56 */
	{0x53,	0x31},	/*35	60 */
	{0x53,	0x33},	/*36	64 */
	{0x53,	0x37},	/*37	68 */
	{0x53,	0x3C},	/*38	72 */
	{0x53,	0x42},	/*39	77 */
	{0x53,	0x48},	/*40	82 */
	{0x53,	0x4F},	/*41	87 */
	{0x53,	0x56},	/*42	93 */
	{0x53,	0x5C},	/*43	98 */
	{0x53,	0x61},	/*44	105*/
	{0x53,	0x64},	/*45	111*/
	{0x53,	0x69},	/*46	119*/
	{0x53,	0x6D},	/*47	126*/
	{0x53,	0x71},	/*48	134*/
	{0x53,	0x76},	/*49	143*/
	{0x53,	0x7B},	/*50	152*/
	{0x53,	0x80},	/*51	162*/
	{0x53,	0x85},	/*52	172*/
	{0x53,	0x8B},	/*53	183*/
	{0x53,	0x91},	/*54	195*/
	{0x53,	0x97},	/*55	207*/
	{0x53,	0x9D},	/*56	220*/
	{0x53,	0xA3},	/*57	234*/
	{0x53,	0xAA},	/*58	249*/
	{0x53,	0xB2},	/*59	265*/
	{0x53,	0xBB},	/*60	282*/
	{0x53,	0xC6},	/*61	300*/
	{0x53,	0xCF},	/*62	316*/
	{0x53,	0xD9},	/*63	333*/
	{0x53,	0xE1},	/*64	350*/
	{0x53,	0xE4},	/*65	357*/
	{0x53,	0xE7},	/*66	365*/
	{0x53,	0xEB},	/*67	372*/
	{0x53,	0xEE},	/*68	380*/
	{0x53,	0xF1},	/*69	387*/
	{0x53,	0xF5},	/*70	395*/
	{0x53,	0xF8},	/*71	403*/
	{0x53,	0xFC},	/*72	412*/
	{0x53,	0xFF},	/*73	420*/
	{0x53,	0xFF},	/*74	hbm*/
};

static void poc_comp(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *poc_comp_cmds = ss_get_cmds(vdd, TX_POC_COMP);
	int cd_idx;

	if (IS_ERR_OR_NULL(vdd) || SS_IS_CMDS_NULL(poc_comp_cmds)) {
		LCD_DEBUG(vdd, "Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)poc_comp_cmds);
		return;
	}

	if (is_hbm_level(vdd))
		cd_idx = ARRAY_SIZE(poc_comp_table) - 1;
	else
		cd_idx = vdd->br_info.common_br.cd_idx;

	LCD_DEBUG(vdd, "cd_idx (%d) val (%02x %02x)\n", cd_idx, poc_comp_table[cd_idx][0], poc_comp_table[cd_idx][1]);

	poc_comp_cmds->cmds[4].ss_txbuf[1] = poc_comp_table[cd_idx][0];
	poc_comp_cmds->cmds[4].ss_txbuf[2] = poc_comp_table[cd_idx][1];

	ss_send_cmd(vdd, TX_POC_COMP);

	return;
}

static int poc_spi_get_status(struct samsung_display_driver_data *vdd)
{
	u8 rxbuf[1];
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd\n");
		return -EINVAL;
	}

	ret = ss_spi_sync(vdd->spi_dev, rxbuf, RX_STATUS_REG1);
	if (ret) {
		LCD_ERR(vdd, "fail to spi read.. ret (%d) \n", ret);
		return ret;
	}

	LCD_INFO(vdd, "status : 0x%02x \n", rxbuf[0]);

	return rxbuf[0];
}

static int spi_write_enable_wait(struct samsung_display_driver_data *vdd)
{
	struct spi_device *spi_dev;
	int ret = 0;
	int status = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd\n");
		return -EINVAL;
	}

	spi_dev = vdd->spi_dev;

	if (IS_ERR_OR_NULL(spi_dev)) {
		LCD_ERR(vdd, "no spi_dev\n");
		return -EINVAL;
	}

	/* BUSY check */
	while ((status = poc_spi_get_status(vdd)) & 0x01) {
		usleep_range(20, 30);
		LCD_ERR(vdd, "status(0x%02x) is busy.. wait!!\n", status);
	}

	/* Write write enable */
	ss_spi_sync(spi_dev, NULL, TX_WRITE_ENABLE);

	/* WEL check */
	while (!((status = poc_spi_get_status(vdd)) & 0x02)) {
		LCD_ERR(vdd, "Not ready to write (0x%02x).. wait!!\n", status);
		ss_spi_sync(spi_dev, NULL, TX_WRITE_ENABLE);
		usleep_range(20, 30);
	}

	return ret;
}

static int poc_spi_erase(struct samsung_display_driver_data *vdd, u32 erase_pos, u32 erase_size, u32 target_pos)
{
	struct spi_device *spi_dev;
	struct ddi_spi_cmd_set *cmd_set = NULL;
	int image_size = 0;
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd\n");
		return -EINVAL;
	}

	spi_dev = vdd->spi_dev;

	if (IS_ERR_OR_NULL(spi_dev)) {
		LCD_ERR(vdd, "no spi_dev\n");
		return -EINVAL;
	}

	image_size = vdd->poc_driver.image_size;

	cmd_set = ss_get_spi_cmds(vdd, TX_ERASE);
	if (cmd_set == NULL) {
		LCD_ERR(vdd, "cmd_set is null..\n");
		return -EINVAL;
	}

	/* start */
	spi_write_enable_wait(vdd);
	ss_spi_sync(spi_dev, NULL, TX_WRITE_STATUS_REG1);
	spi_write_enable_wait(vdd);
	ss_spi_sync(spi_dev, NULL, TX_WRITE_STATUS_REG2);

	spi_write_enable_wait(vdd);

	/* spi erase */
	if (erase_size == POC_ERASE_64KB)
		cmd_set->tx_buf[0] = SPI_64K_ERASE_CMD;
	else if (erase_size == POC_ERASE_32KB)
		cmd_set->tx_buf[0] = SPI_32K_ERASE_CMD;
	else
		cmd_set->tx_buf[0] = SPI_SECTOR_ERASE_CMD;

	cmd_set->tx_addr = erase_pos;

	LCD_INFO(vdd, "[ERASE] (%6d / %6d), erase_size (%d) addr (%06x) %02x\n",
		erase_pos, target_pos, erase_size, cmd_set->tx_addr, cmd_set->tx_buf[0]);

	ss_spi_sync(spi_dev, NULL, TX_ERASE);

	/* end */
	spi_write_enable_wait(vdd);
	ss_spi_sync(spi_dev, NULL, TX_WRITE_STATUS_REG1_END);
	spi_write_enable_wait(vdd);
	ss_spi_sync(spi_dev, NULL, TX_WRITE_STATUS_REG2_END);

	return ret;
}

static int poc_spi_write(struct samsung_display_driver_data *vdd, u8 *data, u32 write_pos, u32 write_size)
{
	struct spi_device *spi_dev;
	struct ddi_spi_cmd_set *cmd_set = NULL;
	int pos, ret = 0;
	int last_pos, image_size, loop_cnt, poc_w_size;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd\n");
		return -EINVAL;
	}

	spi_dev = vdd->spi_dev;

	if (IS_ERR_OR_NULL(spi_dev)) {
		LCD_ERR(vdd, "no spi_dev\n");
		return -EINVAL;
	}

	image_size = vdd->poc_driver.image_size;
	last_pos = write_pos + write_size;
	poc_w_size = vdd->poc_driver.write_data_size;
	loop_cnt = vdd->poc_driver.write_loop_cnt;

	cmd_set = ss_get_spi_cmds(vdd, TX_WRITE_PAGE_PROGRAM);
	if (cmd_set == NULL) {
		LCD_ERR(vdd, "cmd_set is null..\n");
		return -EINVAL;
	}

	LCD_INFO(vdd, "[WRITE] write_pos : %6d, write_size : %6d, last_pos %6d, poc_w_size : %d\n",
		write_pos, write_size, last_pos, poc_w_size);

	/* start */
	spi_write_enable_wait(vdd);
	ss_spi_sync(spi_dev, NULL, TX_WRITE_STATUS_REG1);
	spi_write_enable_wait(vdd);
	ss_spi_sync(spi_dev, NULL, TX_WRITE_STATUS_REG2);

	for (pos = write_pos; pos < last_pos; ) {
		if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
			LCD_ERR(vdd, "cancel poc write by user\n");
			ret = -EIO;
			goto cancel_poc;
		}

		cmd_set->tx_addr = pos;

		memcpy(&cmd_set->tx_buf[4], &data[pos], cmd_set->tx_size - 3);

		spi_write_enable_wait(vdd);

		/* spi write */
		ss_spi_sync(spi_dev, NULL, TX_WRITE_PAGE_PROGRAM);

		pos += poc_w_size;
	}

cancel_poc:
	if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
		LCD_ERR(vdd, "cancel poc write by user\n");
		atomic_set(&vdd->poc_driver.cancel, 0);
		ret = -EIO;
	}

	/* end */
	spi_write_enable_wait(vdd);
	ss_spi_sync(spi_dev, NULL, TX_WRITE_STATUS_REG1_END);
	spi_write_enable_wait(vdd);
	ss_spi_sync(spi_dev, NULL, TX_WRITE_STATUS_REG2_END);

	return ret;
}

static int poc_spi_read(struct samsung_display_driver_data *vdd, u8 *buf, u32 read_pos, u32 read_size)
{
	struct spi_device *spi_dev;
	struct ddi_spi_cmd_set *cmd_set = NULL;
	u8 *rbuf;
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "no vdd\n");
		return -EINVAL;
	}

	spi_dev = vdd->spi_dev;

	if (IS_ERR_OR_NULL(spi_dev)) {
		LCD_ERR(vdd, "no spi_dev\n");
		return -EINVAL;
	}

	cmd_set = ss_get_spi_cmds(vdd, RX_DATA);
	if (cmd_set == NULL) {
		LCD_ERR(vdd, "cmd_set is null..\n");
		return -EINVAL;
	}

	LCD_INFO(vdd, "[READ] read_pos : %6d, read_size : %6d\n", read_pos, read_size);

	rbuf = kmalloc(read_size, GFP_KERNEL | GFP_DMA);
	if (rbuf == NULL) {
		LCD_ERR(vdd, "fail to alloc rbuf..\n");
		goto cancel_poc;
	}

	cmd_set->rx_size = read_size;
	cmd_set->rx_addr = read_pos;

	if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
		LCD_ERR(vdd, "cancel poc read by user\n");
		ret = -EIO;
		goto cancel_poc;
	}

	/* spi read */
	ret = ss_spi_sync(spi_dev, rbuf, RX_DATA);

	/* copy to buf */
	memcpy(&buf[read_pos], rbuf, read_size);

	/* rx_buf reset */
	memset(rbuf, 0, read_size);

	if (!(read_pos % DEBUG_POC_CNT))
		LCD_INFO(vdd, "buf[%d] = 0x%x\n", read_pos, buf[read_pos]);

cancel_poc:
	if (rbuf)
		kfree(rbuf);

	return ret;
}

static int ss_vrr_init(struct vrr_info *vrr)
{
	struct samsung_display_driver_data *vdd =
		container_of(vrr, struct samsung_display_driver_data, vrr);
	struct lfd_mngr *mngr;
	int i, scope;

	LCD_INFO(vdd, "+++\n");

	mutex_init(&vrr->vrr_lock);
	mutex_init(&vrr->brr_lock);

	vrr->running_vrr_mdp = false;
	vrr->running_vrr = false;

	/* default: WQHD@120hz HS mode */
	vrr->cur_refresh_rate = vrr->adjusted_refresh_rate = 120;
	vrr->cur_sot_hs_mode = vrr->adjusted_sot_hs_mode = true;

	vrr->hs_nm_seq = HS_NM_OFF;
	vrr->delayed_perf_normal = false;
	vrr->skip_vrr_in_brightness = false;

	vrr->vrr_workqueue = create_singlethread_workqueue("vrr_workqueue");
	INIT_WORK(&vrr->vrr_work, ss_panel_vrr_switch_work);

	vrr->brr_mode = BRR_OFF_MODE;

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
	case CHECK_SUPPORT_MCD:
		if (!cur_hs) {
			is_support = false;
			LCD_ERR(vdd, "MCD fail: supported on HS(cur: %dNS)\n",
					cur_rr);
		}
		break;

	case CHECK_SUPPORT_HMD:
		if (!(cur_rr == 60 && !cur_hs)) {
			is_support = false;
			LCD_ERR(vdd, "HMD fail: supported on 60NS(cur: %d%s)\n",
					cur_rr, cur_hs ? "HS" : "NS");
		}
		break;

	default:
		break;
	}

	return is_support;
}

static bool on_timimg_switch;
static int ss_timing_switch_pre(struct samsung_display_driver_data *vdd)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	LCD_INFO(vdd, "++\n");
	ss_send_cmd(vdd, TX_TIMING_SWITCH_PRE);
	on_timimg_switch = 1;
	LCD_INFO(vdd, "--\n");
	return ret;
}

static int ss_timing_switch_post(struct samsung_display_driver_data *vdd)
{
	struct vrr_info *vrr;
	int cur_hact = 0;
	int adjusted_hact = 0;
	int ret = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}
	vrr = &vdd->vrr;
	cur_hact = vrr->cur_h_active;
	adjusted_hact = vrr->adjusted_h_active;

	if (on_timimg_switch && (cur_hact < adjusted_hact)) {
		LCD_INFO(vdd, "++ DMS go to higer res\n");
		ss_send_cmd(vdd, TX_TIMING_SWITCH_POST);
		LCD_INFO(vdd, "-- DMS go to higer res\n");
	}
	on_timimg_switch = 0;

	return ret;
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

static void ss_read_flash(struct samsung_display_driver_data *vdd, u32 raddr, u32 rsize, u8 *rbuf)
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
		pos += snprintf(showbuf + pos, 256 - pos, "%02x ", rbuf[i]);
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

static void make_brightness_packet(struct samsung_display_driver_data *vdd,
	struct dsi_cmd_desc *packet, int *cmd_cnt, enum BR_TYPE br_type)
{
	if (br_type == BR_TYPE_NORMAL) {
			ss_add_brightness_packet(vdd, BR_FUNC_1, packet, cmd_cnt);

			/* ACL */
			if (vdd->br_info.acl_status || vdd->siop_status) {
				ss_add_brightness_packet(vdd, BR_FUNC_ACL_ON, packet, cmd_cnt);
				ss_add_brightness_packet(vdd, BR_FUNC_ACL_PERCENT_PRE, packet, cmd_cnt);
				ss_add_brightness_packet(vdd, BR_FUNC_ACL_PERCENT, packet, cmd_cnt);
			} else {
				ss_add_brightness_packet(vdd, BR_FUNC_ACL_OFF, packet, cmd_cnt);
			}

			/* TFM_PWM */
			ss_add_brightness_packet(vdd, BR_FUNC_TFT_PWM, packet, cmd_cnt);

			/* mAFPC */
			if (vdd->mafpc.is_support)
				ss_add_brightness_packet(vdd, BR_FUNC_MAFPC_SCALE, packet, cmd_cnt);

			/* gamma */
			ss_add_brightness_packet(vdd, BR_FUNC_GAMMA, packet, cmd_cnt);

			/* gamma compensation for gamma mode2 VRR modes */
			ss_add_brightness_packet(vdd, BR_FUNC_GAMMA_COMP, packet, cmd_cnt);

			/* vint */
			ss_add_brightness_packet(vdd, BR_FUNC_VINT, packet, cmd_cnt);

			/* VRR */
			ss_add_brightness_packet(vdd, BR_FUNC_VRR, packet, cmd_cnt);

	} else if (br_type == BR_TYPE_HBM) {
		/* acl */
		if (vdd->br_info.acl_status || vdd->siop_status) {
			ss_add_brightness_packet(vdd, BR_FUNC_HBM_ACL_ON, packet, cmd_cnt);
		} else {
			ss_add_brightness_packet(vdd, BR_FUNC_HBM_ACL_OFF, packet, cmd_cnt);
		}

		/* IRC */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_IRC, packet, cmd_cnt);

		/* mAFPC */
		if (vdd->mafpc.is_support)
			ss_add_brightness_packet(vdd, BR_FUNC_MAFPC_SCALE, packet, cmd_cnt);

		/* Gamma */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_GAMMA, packet, cmd_cnt);

		/* gamma compensation for gamma mode2 VRR modes */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_GAMMA_COMP, packet, cmd_cnt);

		/* vint */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_VINT, packet, cmd_cnt);

		/* LTPS: used for normal and HBM brightness */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_LTPS, packet, cmd_cnt);

		/* mAFPC */
		if (vdd->mafpc.is_support)
			ss_add_brightness_packet(vdd, BR_FUNC_MAFPC_SCALE, packet, cmd_cnt);

		/* hbm etc */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_ETC, packet, cmd_cnt);

		/* VRR */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_VRR, packet, cmd_cnt);
	} else if (br_type == BR_TYPE_HMT) {
		if (vdd->br_info.smart_dimming_hmt_loaded_dsi) {
			/* aid/aor B2 */
			ss_add_brightness_packet(vdd, BR_FUNC_HMT_AID, packet, cmd_cnt);

			/* elvss B5 */
			ss_add_brightness_packet(vdd, BR_FUNC_HMT_ELVSS, packet, cmd_cnt);

			/* vint F4 */
			ss_add_brightness_packet(vdd, BR_FUNC_HMT_VINT, packet, cmd_cnt);

			/* gamma CA */
			ss_add_brightness_packet(vdd, BR_FUNC_HMT_GAMMA, packet, cmd_cnt);

			/* VRR */
			ss_add_brightness_packet(vdd, BR_FUNC_HMT_VRR, packet, cmd_cnt);
		} else {
			ss_add_brightness_packet(vdd, BR_FUNC_HMT_GAMMA, packet, cmd_cnt);
		}
	} else {
		LCD_ERR(vdd, "undefined br_type (%d) \n", br_type);
	}

	return;
}

/* mtp original data */
u8 HS120_NORMAL_R_TYPE_BUF[GAMMA_R_SIZE];
/* mtp original data - V type */
int HS120_NORMAL_V_TYPE_BUF[GAMMA_V_SIZE];

/* mtp original data */
u8 HS120_HBM_R_TYPE_BUF[GAMMA_R_SIZE];
/* mtp original data - V type */
int HS120_HBM_V_TYPE_BUF[GAMMA_V_SIZE];


/* HS96 compensated data */
u8 HS96_R_TYPE_COMP[MTP_OFFSET_TAB_SIZE_96HS][70];
/* HS96 compensated data V type*/
int HS96_V_TYPE_COMP[MTP_OFFSET_TAB_SIZE_96HS][GAMMA_V_SIZE];

static void ss_print_gamma_comp(struct samsung_display_driver_data *vdd)
{
	char pBuffer[256];
	int i, j;

	LCD_INFO(vdd, "== HS120_HBM_R_TYPE_BUF ==\n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < GAMMA_R_SIZE; i++) {
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_HBM_R_TYPE_BUF[i]);
	}
	LCD_INFO(vdd, "SET : %s\n", pBuffer);

	LCD_INFO(vdd, "== HS120_NORMAL_R_TYPE_BUF ==\n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < GAMMA_R_SIZE; i++) {
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_NORMAL_R_TYPE_BUF[i]);
	}
	LCD_INFO(vdd, "SET : %s\n", pBuffer);

	LCD_DEBUG(vdd, "== HS120_HBM_V_TYPE_BUF ==\n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < GAMMA_V_SIZE; i++)
		snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_HBM_V_TYPE_BUF[i]);
	LCD_DEBUG(vdd, "SET : %s\n", pBuffer);

	LCD_DEBUG(vdd, "== HS120_NORMAL_V_TYPE_BUF ==\n");
	memset(pBuffer, 0x00, 256);
	for (i = 0; i < GAMMA_V_SIZE; i++)
		snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_NORMAL_V_TYPE_BUF[i]);
	LCD_DEBUG(vdd, "SET : %s\n", pBuffer);

	LCD_DEBUG(vdd, "== HS96_V_TYPE_COMP ==\n");
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		memset(pBuffer, 0x00, 256);
		LCD_DEBUG(vdd, "- COMP[%d]\n", i);
		for (j = 0; j < GAMMA_V_SIZE; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS96_V_TYPE_COMP[i][j]);
		LCD_DEBUG(vdd, "SET : %s\n", pBuffer);
	}

	LCD_DEBUG(vdd, "== HS96_R_TYPE_COMP ==\n");
	for (i = 0; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		memset(pBuffer, 0x00, 256);
		LCD_DEBUG(vdd, "- COMP[%d]\n", i);
		for (j = 0; j < 70; j++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS96_R_TYPE_COMP[i][j]);
		LCD_DEBUG(vdd, "SET : %s\n", pBuffer);
	}
}

static int ss_gm2_gamma_comp_init(struct samsung_display_driver_data *vdd)
{
	u8 readbuf[GAMMA_R_SIZE];
	int i, j, m;
	int val;
	int start_addr;
	struct flash_raw_table *gamma_tbl;
	u32 fsize = 0xC00000 | GAMMA_R_SIZE;

	gamma_tbl = vdd->br_info.br_tbl->gamma_tbl;

	LCD_INFO(vdd, "++\n");
	if (vdd->panel_revision > 6) {/* E8 */
		memcpy(MTP_OFFSET_96HS_VAL, MTP_OFFSET_96HS_VAL_revH, sizeof(MTP_OFFSET_96HS_VAL));
		memcpy(aor_table_96, aor_table_96_revH, sizeof(aor_table_96));
		memcpy(aor_table_120, aor_table_120_revH, sizeof(aor_table_120));
	} else if (vdd->panel_revision > 5) {/* E7 */
		memcpy(MTP_OFFSET_96HS_VAL, MTP_OFFSET_96HS_VAL_revG, sizeof(MTP_OFFSET_96HS_VAL));
		memcpy(aor_table_96, aor_table_96_revA, sizeof(aor_table_96));
		memcpy(aor_table_120, aor_table_120_revA, sizeof(aor_table_120));
	} else {
		memcpy(MTP_OFFSET_96HS_VAL, MTP_OFFSET_96HS_VAL_revA, sizeof(MTP_OFFSET_96HS_VAL));
		memcpy(aor_table_96, aor_table_96_revA, sizeof(aor_table_96));
		memcpy(aor_table_120, aor_table_120_revA, sizeof(aor_table_120));
	}

	/***********************************************************/
	/* 1-1. [HBM/ NORMAL] Read HS120 ORIGINAL GAMMA Flash      */
	/***********************************************************/

	ss_send_cmd(vdd, TX_FLASH_GAMMA_PRE1);
	if (vdd->panel_revision > 6)/* E8 ~ */
		start_addr = gamma_tbl->flash_table_hbm_gamma_offset;
	else if (vdd->panel_revision > 3)/* E5 ~ E7 */
		start_addr = 0xE15BE;/* 923070 */
	else if (vdd->panel_revision > 1)/* E3 ~ E4 */
		start_addr = 0xE1578;/* 923000 */
	else
		start_addr = 0xE1532;/* 922930 */
	flash_read_bytes(vdd, start_addr, fsize, GAMMA_R_SIZE, readbuf);
	memcpy(&HS120_HBM_R_TYPE_BUF, readbuf, GAMMA_R_SIZE);

	if (vdd->panel_revision > 6)/* E8 ~ */
		start_addr = gamma_tbl->flash_table_normal_gamma_offset;
	else if (vdd->panel_revision > 3)/* E5 ~ E7 */
		start_addr = 0xE1532;/* 922930 */
	else if (vdd->panel_revision > 1)/* E3 ~ E4 */
		start_addr = 0xE1532;/* 922930 */
	else
		start_addr = 0xE14EC;/* 922860 */

	flash_read_bytes(vdd, start_addr, fsize, GAMMA_R_SIZE, readbuf);
	memcpy(&HS120_NORMAL_R_TYPE_BUF, readbuf, GAMMA_R_SIZE);

	ss_send_cmd(vdd, TX_FLASH_GAMMA_POST);

	/***********************************************************/
	/* 1-2. [HBM] translate Register type to V type            */
	/***********************************************************/
	j = 0;
	for (i = 0; i < GAMMA_R_SIZE; ) {
		if (i == 0 || i == GAMMA_R_SIZE - 5) {
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i], 0, 3) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+2], 0, 7);
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i+1], 4, 7) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+3], 0, 7);
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i+1], 0, 3) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+4], 0, 7);
		} else {
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i], 0, 2) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+2], 0, 7);
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i+1], 4, 6) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+3], 0, 7);
			HS120_HBM_V_TYPE_BUF[j++] = (GET_BITS(HS120_HBM_R_TYPE_BUF[i+1], 0, 2) << 8)
									| GET_BITS(HS120_HBM_R_TYPE_BUF[i+4], 0, 7);
		}
		i = i + 5;
	}

	/***********************************************************/
	/* 1-3. [HBM] Make HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX]	   */
	/***********************************************************/

		for (j = 0; j < GAMMA_V_SIZE; j++) {
			/* check underflow & overflow */
			if (HS120_HBM_V_TYPE_BUF[j] + MTP_OFFSET_96HS_VAL[MTP_OFFSET_HBM_IDX][j] < 0) {
				HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = 0;
			} else {
				if (j <= 2 || j >= (GAMMA_V_SIZE - 3)) {/* 1th & 14th 12bit(0xFFF) */
					val = HS120_HBM_V_TYPE_BUF[j] + MTP_OFFSET_96HS_VAL[0][j];
					if (val > 0xFFF)	/* check overflow */
						HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = 0xFFF;
					else
						HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = val;
				} else {	/* 2th - 13th 11bit(0x7FF) */
					val = HS120_HBM_V_TYPE_BUF[j] + MTP_OFFSET_96HS_VAL[0][j];
					if (val > 0x7FF)	/* check overflow */
						HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = 0x7FF;
					else
						HS96_V_TYPE_COMP[MTP_OFFSET_HBM_IDX][j] = val;
				}
			}
		}

	/***********************************************************/
	/* 2-2. [NORMAL] translate Register type to V type 	       */
	/***********************************************************/

	/* HS120 */
	j = 0;
	for (i = 0; i < GAMMA_R_SIZE; ) {
		if (i == 0 || i == GAMMA_R_SIZE - 5) {
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i], 0, 3) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+2], 0, 7);
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+1], 4, 7) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+3], 0, 7);
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+1], 0, 3) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+4], 0, 7);
		} else {
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i], 0, 2) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+2], 0, 7);
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+1], 4, 6) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+3], 0, 7);
			HS120_NORMAL_V_TYPE_BUF[j++] = (GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+1], 0, 2) << 8)
									| GET_BITS(HS120_NORMAL_R_TYPE_BUF[i+4], 0, 7);
		}
		i = i + 5;
	}

	/*************************************************************/
	/* 2-3. [NORMAL] Make HS96_V_TYPE_COMP[MTP_OFFSET_NORMAL_IDX]*/
	/*************************************************************/

	/* 96HS */
	for (i = MTP_OFFSET_NORMAL_IDX; i < MTP_OFFSET_TAB_SIZE_96HS; i++) {
		for (j = 0; j < GAMMA_V_SIZE; j++) {
			/* check underflow & overflow */
			if (HS120_NORMAL_V_TYPE_BUF[j] + MTP_OFFSET_96HS_VAL[i][j] < 0) {
				HS96_V_TYPE_COMP[i][j] = 0;
			} else {
				if (j <= 2 || j >= (GAMMA_V_SIZE - 3)) {/* 1th & 14th 12bit(0xFFF) */
					val = HS120_NORMAL_V_TYPE_BUF[j] + MTP_OFFSET_96HS_VAL[i][j];
					if (val > 0xFFF)	/* check overflow */
						HS96_V_TYPE_COMP[i][j] = 0xFFF;
					else
						HS96_V_TYPE_COMP[i][j] = val;
				} else {	/* 2th - 13th 11bit(0x7FF) */
					val = HS120_NORMAL_V_TYPE_BUF[j] + MTP_OFFSET_96HS_VAL[i][j];
					if (val > 0x7FF)	/* check overflow */
						HS96_V_TYPE_COMP[i][j] = 0x7FF;
					else
						HS96_V_TYPE_COMP[i][j] = val;
				}
			}
		}
	}

	/******************************************************/
	/* 3. translate HS96_V_TYPE_COMP type to Register type*/
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
				HS96_R_TYPE_COMP[i][m++] = GET_BITS(HS96_V_TYPE_COMP[i][j+R], 8, 10);
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 8, 10) << 4)
											| (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 8, 10));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+R], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+G], 0, 7));
				HS96_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_V_TYPE_COMP[i][j+B], 0, 7));
			}
		}
	}
	/* print all results */
	ss_print_gamma_comp(vdd);

	LCD_INFO(vdd, " --\n");

	return 0;
}

struct dsi_panel_cmd_set *ss_brightness_gm2_gamma_comp(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *gamma_nm_cmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_NORMAL);
	struct dsi_panel_cmd_set *gamma_comp_cmds = ss_get_cmds(vdd, TX_VRR_GM2_GAMMA_COMP2);
	struct dsi_panel_cmd_set *vrr_cmds = ss_get_cmds(vdd, TX_VRR);

	int offset_idx = -1;
	int cmd_idx = 0;
	int cur_rr;

	if (SS_IS_CMDS_NULL(gamma_nm_cmds) || SS_IS_CMDS_NULL(gamma_comp_cmds)
		|| SS_IS_CMDS_NULL(vrr_cmds)) {
		LCD_INFO(vdd, "no gamma_nm_cmds/ vrr_cmds\n");
		return NULL;
	}

	cur_rr = vdd->vrr.cur_refresh_rate;
	if (cur_rr == 96 || cur_rr == 48) {
		/* 1. Edit TX_GAMMA_MODE2_NORMAL */
		cmd_idx = ss_get_cmd_idx(gamma_nm_cmds, 0x00, 0x53);
		gamma_nm_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x20;
		cmd_idx = ss_get_cmd_idx(gamma_nm_cmds, 0x0C, 0xC2);
		if (cur_br_mode == HS96_48_HBM)
			gamma_nm_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x60;
		else
			gamma_nm_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x20;
		/* 2. Edit TX_VRR_GM2_GAMMA_COMP2 & TX_VRR */
		offset_idx = mtp_offset_idx_table_96hs[vdd->br_info.common_br.cd_idx];
		if (offset_idx >= 0) {
			LCD_INFO(vdd, "COMP 96HS[%d] - %d\n", offset_idx, vdd->br_info.common_br.cd_idx);
			cmd_idx = ss_get_cmd_idx(gamma_comp_cmds, 0x53, 0xC7); /* AOR Enabler Flag */
			if (cur_br_mode == DEFAULT_BR_MODE)
				gamma_comp_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x00; /* AOR disable 120 -> 96 */
			else
				gamma_comp_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x01; /* AOR disable 96 <-> 96 */
			gamma_comp_cmds->cmds[cmd_idx].ss_txbuf[2] =
						(aor_table_96[vdd->br_info.common_br.cd_idx] & 0xFF00) >> 8;
			gamma_comp_cmds->cmds[cmd_idx].ss_txbuf[3] =
						(aor_table_96[vdd->br_info.common_br.cd_idx] & 0xFF);

			cmd_idx = ss_get_cmd_idx(gamma_comp_cmds, 0x3E3, 0xC6); /* gamma Enable Flag */
			gamma_comp_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x01; /* Gamma enabler */
			memcpy(&gamma_comp_cmds->cmds[cmd_idx].ss_txbuf[2], &HS96_R_TYPE_COMP[offset_idx][0], 25);
			memcpy(&gamma_comp_cmds->cmds[cmd_idx+1].ss_txbuf[1], &HS96_R_TYPE_COMP[offset_idx][25], 45);

			cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x3E3, 0xC6); /* gamma Enable Flag */
			vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x01;
			cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x53, 0xC7);
			vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x01; /* AOR Enable only for all 96/48 cases */
		} else {
			/* restore original register */
			LCD_ERR(vdd, "ERROR!! check idx[%d] - %d\n", offset_idx, vdd->br_info.common_br.cd_idx);
		}
		cur_br_mode = HS96_48_NORMAL;
	} else {
		LCD_INFO(vdd, "COMP NS/HS[%d] - %d\n", offset_idx, vdd->br_info.common_br.cd_idx);
		cmd_idx = ss_get_cmd_idx(gamma_nm_cmds, 0x00, 0x53);
		gamma_nm_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x28;
		cmd_idx = ss_get_cmd_idx(gamma_nm_cmds, 0x0C, 0xC2);
		gamma_nm_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x60;
		cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x3E3, 0xC6); /* gamma Enable Flag */
		vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x00;
		cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x53, 0xC7);
		vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x00; /* AOR Enable Flag */
		cur_br_mode = DEFAULT_BR_MODE;
		LCD_INFO(vdd, " --\n");
		return NULL;
	}

	LCD_INFO(vdd, " --\n");

	return gamma_comp_cmds;
}

struct dsi_panel_cmd_set *ss_brightness_gm2_hbm_gamma_comp(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *gamma_hbm_cmds = ss_get_cmds(vdd, TX_GAMMA_MODE2_HBM);
	struct dsi_panel_cmd_set *gamma_comp_cmds = ss_get_cmds(vdd, TX_VRR_GM2_GAMMA_COMP2);
	struct dsi_panel_cmd_set *vrr_cmds = ss_get_cmds(vdd, TX_VRR);

	int cmd_idx = 0;
	int cur_rr;

	if (SS_IS_CMDS_NULL(gamma_hbm_cmds) || SS_IS_CMDS_NULL(gamma_comp_cmds)
		|| SS_IS_CMDS_NULL(vrr_cmds)) {
		LCD_INFO(vdd, "no gamma_hbm_cmds/ gamma_comp_cmds/ vrr_cmds\n");
		return NULL;
	}

	LCD_INFO(vdd, " vdd->vrr.cur_refresh_rate %d ++\n", vdd->vrr.cur_refresh_rate);
	cur_rr = vdd->vrr.cur_refresh_rate;

	if (cur_rr == 96 || cur_rr == 48) {
		/* 1. Edit TX_GAMMA_MODE2_HBM */
		if (cur_br_mode == HS96_48_NORMAL) {
			cmd_idx = ss_get_cmd_idx(gamma_comp_cmds, 0x3E3, 0xC6); /* gamma Enable Flag */
			gamma_comp_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x00; /* Gamma enabler */
			cmd_idx = ss_get_cmd_idx(gamma_hbm_cmds, 0x00, 0x53);
			gamma_hbm_cmds->cmds[cmd_idx].ss_txbuf[1] = 0xE0;
			cmd_idx = ss_get_cmd_idx(gamma_hbm_cmds, 0x0C, 0xC2);
			gamma_hbm_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x20;
		} else {
			cmd_idx = ss_get_cmd_idx(gamma_comp_cmds, 0x3E3, 0xC6); /* gamma Enable Flag */
			gamma_comp_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x01; /* Gamma enabler */
			cmd_idx = ss_get_cmd_idx(gamma_hbm_cmds, 0x00, 0x53);
			gamma_hbm_cmds->cmds[cmd_idx].ss_txbuf[1] = 0xE8;
			cmd_idx = ss_get_cmd_idx(gamma_hbm_cmds, 0x0C, 0xC2);
			gamma_hbm_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x60;
		}

		if (vdd->br_info.common_br.bl_level < 261) {
			cmd_idx = ss_get_cmd_idx(gamma_comp_cmds, 0x3E3, 0xC6); /* gamma Enable Flag */
			gamma_comp_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x00; /* Gamma enabler */
		}

		/* 2. Edit TX_VRR_GM2_GAMMA_COMP2 & TX_VRR */
		LCD_INFO(vdd, "COMP 96HS[MTP_OFFSET_HBM_IDX] - %d\n", vdd->br_info.common_br.cd_idx);
		cmd_idx = ss_get_cmd_idx(gamma_comp_cmds, 0x53, 0xC7); /* AOR Enable Flag */
		gamma_comp_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x01; /* AOR Enable Flag */
		gamma_comp_cmds->cmds[cmd_idx].ss_txbuf[2] = 0x02;
		gamma_comp_cmds->cmds[cmd_idx].ss_txbuf[3] = 0x88;
		cmd_idx = ss_get_cmd_idx(gamma_comp_cmds, 0x3E3, 0xC6);
		memcpy(&gamma_comp_cmds->cmds[cmd_idx].ss_txbuf[2], &HS96_R_TYPE_COMP[MTP_OFFSET_HBM_IDX][0], 25);
		memcpy(&gamma_comp_cmds->cmds[cmd_idx+1].ss_txbuf[1], &HS96_R_TYPE_COMP[MTP_OFFSET_HBM_IDX][25], 45);
		cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x3E3, 0xC6); /* gamma Enable Flag */
		vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x01;
		cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x53, 0xC7);
		vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x01; /* AOR Enable Flag */
		cur_br_mode = HS96_48_HBM;
	} else {
		LCD_INFO(vdd, "COMP 120HS[MTP_OFFSET_HBM_IDX] - %d\n", vdd->br_info.common_br.cd_idx);
		cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x3E3, 0xC6); /* gamma Enable Flag */
		vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x00;
		cmd_idx = ss_get_cmd_idx(vrr_cmds, 0x53, 0xC7);
		vrr_cmds->cmds[cmd_idx].ss_txbuf[1] = 0x00; /* AOR Enable Flag */
		cur_br_mode = DEFAULT_BR_MODE;
		LCD_INFO(vdd, " --\n");
		return NULL;
	}
	LCD_INFO(vdd, " --\n");

	return gamma_comp_cmds;
}

void S6E3HAD_AMB681XV01_WQHD_init(struct samsung_display_driver_data *vdd)
{
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
	vdd->panel_func.br_func[BR_FUNC_VINT] = ss_vint;
	vdd->panel_func.br_func[BR_FUNC_VRR] = ss_vrr;
	vdd->panel_func.br_func[BR_FUNC_GAMMA_COMP] = ss_brightness_gm2_gamma_comp;
	vdd->panel_func.br_func[BR_FUNC_HBM_GAMMA_COMP] = ss_brightness_gm2_hbm_gamma_comp;

	vdd->br_info.smart_dimming_loaded_dsi = false;

	/* HBM */
	vdd->panel_func.br_func[BR_FUNC_HBM_GAMMA] = ss_brightness_gamma_mode2_hbm;
	vdd->panel_func.br_func[BR_FUNC_HBM_VRR] = ss_vrr_hbm;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_ON] = ss_acl_on;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_OFF] = ss_acl_off;
	vdd->panel_func.br_func[BR_FUNC_HBM_VINT] = ss_vint;

	/* HMT */
	vdd->panel_func.br_func[BR_FUNC_HMT_GAMMA] = ss_brightness_gamma_mode2_hmt;
	vdd->panel_func.br_func[BR_FUNC_HMT_VRR] = ss_vrr_hmt;

	/* Total level key in brightness */
	vdd->panel_func.samsung_brightness_tot_level = ss_brightness_tot_level;

	/* FFC */
	vdd->panel_func.set_ffc = ss_ffc;

	/* Panel LPM */
	vdd->panel_func.samsung_update_lpm_ctrl_cmd = ss_update_panel_lpm_ctrl_cmd;
	vdd->panel_func.samsung_set_lpm_brightness = ss_set_panel_lpm_brightness;

	/* Gray Spot Test */
	vdd->panel_func.samsung_gray_spot = ss_gray_spot;

	/* default brightness */
	vdd->br_info.common_br.bl_level = 128;

	/* mdnie */
	vdd->mdnie.support_mdnie = true;
	vdd->mdnie.support_trans_dimming = true;
	vdd->mdnie.mdnie_tune_size[0] = sizeof(DSI_BYPASS_MDNIE_1);
	vdd->mdnie.mdnie_tune_size[1] = sizeof(DSI_BYPASS_MDNIE_2);
	vdd->mdnie.mdnie_tune_size[2] = sizeof(DSI_BYPASS_MDNIE_3);

	dsi_update_mdnie_data(vdd);

	/* send recovery pck before sending image date (for ESD recovery) */
//	vdd->esd_recovery.send_esd_recovery = false;

	/* Enable panic on first pingpong timeout */
//	vdd->debug_data->panic_on_pptimeout = true;

	/* COLOR WEAKNESS */

	/* Support DDI HW CURSOR */

	/* COVER Open/Close */

	/* COPR */
	vdd->copr.panel_init = ss_copr_panel_init;

	/* ACL default ON */
	vdd->br_info.acl_status = 1;
	vdd->br_info.temperature = 20; // default temperature

	/* ACL default status in acl on */
	vdd->br_info.gradual_acl_val = 1;

	/* Gram Checksum Test */
	vdd->panel_func.samsung_gct_write = ss_gct_write;
	vdd->panel_func.samsung_gct_read = ss_gct_read;

	/* Self display */
	vdd->self_disp.is_support = true;
	vdd->self_disp.factory_support = true;
	vdd->self_disp.init = self_display_init_HAD;
	vdd->self_disp.data_init = ss_self_display_data_init;

	/* mAPFC */
	vdd->mafpc.init = ss_mafpc_init_HAD;
	vdd->mafpc.data_init = ss_mafpc_data_init;

	/* stm */
	vdd->stm.stm_on = 1;

	/* POC */
	vdd->poc_driver.poc_erase = poc_spi_erase;
	vdd->poc_driver.poc_write = poc_spi_write;
	vdd->poc_driver.poc_read = poc_spi_read;
	vdd->poc_driver.poc_comp = poc_comp;

	/* DDI flash test */
	vdd->panel_func.samsung_gm2_ddi_flash_prepare = NULL;
	vdd->panel_func.samsung_test_ddi_flash_check = ss_test_ddi_flash_check;


	/* gamma compensation for 48/96hz VRR mode in gamma mode2 */
	vdd->panel_func.samsung_gm2_gamma_comp_init = ss_gm2_gamma_comp_init;


	/* VRR */
	vdd->panel_func.samsung_lfd_get_base_val = ss_update_base_lfd_val;
	ss_vrr_init(&vdd->vrr);

	vdd->panel_func.samsung_check_support_mode = ss_check_support_mode;

	vdd->panel_func.samsung_timing_switch_pre = ss_timing_switch_pre;
	vdd->panel_func.samsung_timing_switch_post = ss_timing_switch_post;

	/* read flash */
	vdd->panel_func.read_flash = ss_read_flash;

}
