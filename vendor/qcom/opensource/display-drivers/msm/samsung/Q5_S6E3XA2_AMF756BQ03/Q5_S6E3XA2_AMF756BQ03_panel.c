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
#include "Q5_S6E3XA2_AMF756BQ03_panel.h"
#include "Q5_S6E3XA2_AMF756BQ03_mdnie.h"

#define MAX_READ_BUF_SIZE	(20)
static u8 read_buf[MAX_READ_BUF_SIZE];

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	char d_ic = (ss_panel_id2_get(vdd) & 0xC0); /* 0x40:UMC, 0x00:SF */
	struct dsi_panel_cmd_set *cmds = NULL;
	int idx;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, ": Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	LCD_INFO(vdd, "+: ndx=%d panel_revision:%c d_ic:0x%x\n", vdd->ndx, vdd->panel_revision + 'A', d_ic);
	ss_panel_attach_set(vdd, true);

	if (vdd->display_status_dsi.first_commit_disp_on || vdd->is_factory_mode) {
		/* Diverge for UMC Test code & UMC old panel */
		if (d_ic == 0x40) {
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

			if (vdd->panel_revision == 0x00) { /* Support Old UMC version */
				vdd->gct.valid_checksum[0] = 0x6E;
				vdd->gct.valid_checksum[1] = 0x0E;
				vdd->gct.valid_checksum[2] = vdd->gct.valid_checksum[0];
				vdd->gct.valid_checksum[3] = vdd->gct.valid_checksum[1];
			} else { /* panel_revision > 'A' */
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
					} //else : already changed
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

				vdd->gct.valid_checksum[0] = 0x6C;
				vdd->gct.valid_checksum[1] = 0x0C;
				vdd->gct.valid_checksum[2] = vdd->gct.valid_checksum[0];
				vdd->gct.valid_checksum[3] = vdd->gct.valid_checksum[1];

				LCD_INFO(vdd, "gct valid_chksum for UMC: %02X %02X\n",
									vdd->gct.valid_checksum[0], vdd->gct.valid_checksum[1]);

				vdd->dsc_crc_pass_val[0] = 0x2A;
				vdd->dsc_crc_pass_val[1] = 0x66;

				LCD_INFO(vdd, "dsc_crc valid_chksum for UMC: %02X %02X\n",
						vdd->dsc_crc_pass_val[0], vdd->dsc_crc_pass_val[1]);

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
			}
		}
		else if (d_ic == 0x00) {
			/* 0x00 : Diverge for Samsung.Foundary */

			/* GCT */
			cmds = ss_get_cmds(vdd, TX_GCT_HV);
			if (IS_ERR_OR_NULL(cmds)) {
				LCD_ERR(vdd, "Invalid cmds HV\n");
			} else {
				/* restore B0 26 F4 cmds */
				idx = ss_get_cmd_idx(cmds, 0x02, 0xD7);
				if (idx > 0) {
					cmds->cmds[idx+1].ss_txbuf[0] = 0xB0;
					cmds->cmds[idx+1].ss_txbuf[1] = 0x00;
					cmds->cmds[idx+1].ss_txbuf[2] = 0x26;
					cmds->cmds[idx+1].ss_txbuf[3] = 0xF4;
					cmds->cmds[idx+2].ss_txbuf[0] = 0xF4;
					cmds->cmds[idx+2].ss_txbuf[1] = 0x25;

				}
			}
			cmds = ss_get_cmds(vdd, TX_GCT_LV);
			if (IS_ERR_OR_NULL(cmds)) {
				LCD_ERR(vdd, "Invalid cmds LV\n");
			} else {
				/* restore B0 26 F4 cmds */
				idx = ss_get_cmd_idx(cmds, 0x02, 0xD7);
				if (idx > 0) {
					cmds->cmds[idx+1].ss_txbuf[0] = 0xB0;
					cmds->cmds[idx+1].ss_txbuf[1] = 0x00;
					cmds->cmds[idx+1].ss_txbuf[2] = 0x26;
					cmds->cmds[idx+1].ss_txbuf[3] = 0xF4;
					cmds->cmds[idx+2].ss_txbuf[0] = 0xF4;
					cmds->cmds[idx+2].ss_txbuf[1] = 0x05;

				}
			}

			vdd->gct.valid_checksum[0] = 0x70;
			vdd->gct.valid_checksum[1] = 0x08;
			vdd->gct.valid_checksum[2] = vdd->gct.valid_checksum[0];
			vdd->gct.valid_checksum[3] = vdd->gct.valid_checksum[1];

			LCD_INFO(vdd, "gct valid_chksum for SF: %02X %02X\n",
								vdd->gct.valid_checksum[0], vdd->gct.valid_checksum[1]);

			vdd->dsc_crc_pass_val[0] = 0x48;
			vdd->dsc_crc_pass_val[1] = 0x65;

			LCD_INFO(vdd, "dsc_crc valid_chksum for SF: %02X %02X\n",
					vdd->dsc_crc_pass_val[0], vdd->dsc_crc_pass_val[1]);

			/* ABC */
			cmds = ss_get_cmds(vdd, TX_MAFPC_SET_PRE2);
			if (IS_ERR_OR_NULL(cmds)) {
				LCD_ERR(vdd, "Invalid cmds mafpc pre2\n");
			} else {
				idx = ss_get_cmd_idx(cmds, 0x65, 0xFE);
				if (idx > 0) {
					cmds->cmds[idx-1].ss_txbuf[2] = 0x64;
				}
			}
		}
	}

	LCD_INFO(vdd, "-: ndx=%d\n", vdd->ndx);

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

static bool panel_vddr_vol_changed = false;
static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel *panel = NULL;
	struct dsi_vreg *target_vreg = NULL;
	struct dsi_regulator_info regs;
	int i, get_voltage, rc = 0;
//	char d_ic = (ss_panel_id2_get(vdd) & 0xC0); /* 0x40:UMC, 0x00:SF */

	panel = GET_DSI_PANEL(vdd);
	if (IS_ERR_OR_NULL(panel)) {
		LCD_ERR(vdd, "No Panel Data\n");
		goto err;
	}

	if (vdd->manufacture_id_dsi == PBA_ID)
		ss_panel_attach_set(vdd, false);
	else
		ss_panel_attach_set(vdd, true);

	switch (ss_panel_rev_get(vdd)) {
	case 0x00:
		vdd->panel_revision = 'A';
		break;
	case 0x01:
	case 0x02:
		vdd->panel_revision = 'B';
		break;
	case 0x03:
		vdd->panel_revision = 'C';
		break;
	default:
		vdd->panel_revision = 'C';
		LCD_ERR(vdd, "Invalid panel_rev(default rev : %c)\n", vdd->panel_revision);
		break;
	}

	vdd->panel_revision -= 'A';
	LCD_INFO_ONCE(vdd, "panel_revision = %c %d\n", vdd->panel_revision + 'A', vdd->panel_revision);
	
	/* Find panel_vddr regulator and Set the voltage differently depending on the type of ic (SF, UMC) */
	/* UMC = 1.06875v */
	/* SF  = 1.08750v (dtsi default setting) */
	if (!panel_vddr_vol_changed) {
		//if (d_ic != 0x40) {
		if (0) {
			regs = panel->power_info;

			for (i = 0; i < regs.count; i++) {
				target_vreg = &regs.vregs[i];
				if (!strcmp(target_vreg->vreg_name, "panel_vddr")) {
					LCD_INFO(vdd, "Found Voltage(%d) [%s]\n", i, target_vreg->vreg_name);
					break;
				}
			}

			/* SF */		
			LCD_INFO(vdd, "SF IC, change panel_vddr voltage to 1.06875v\n");
	
			//target_vreg->max_voltage = 1068750;
			//target_vreg->min_voltage = 1068750;
			target_vreg->max_voltage = 1087500;
			target_vreg->min_voltage = 1087500;
			rc = ss_wrapper_regulator_set_voltage(vdd,
					target_vreg->vreg, target_vreg->min_voltage, target_vreg->max_voltage);

			if (rc < 0) {
				LCD_ERR(vdd, "Voltage Set Fail voltage : %d rc : %d\n",
							target_vreg->min_voltage, rc);
			} else {
				get_voltage = ss_wrapper_regulator_get_voltage(vdd, target_vreg->vreg);
				LCD_INFO(vdd, "Changed get_voltage = %d\n", get_voltage);
				if (get_voltage != target_vreg->min_voltage)
					panic("Voltage Set Fail to %d\n", target_vreg->min_voltage);
				panel_vddr_vol_changed = true;
			}
		}
	}

err:
	return (vdd->panel_revision + 'A');
}

/*
 * VRR related cmds
 */
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

static enum VRR_CMD_RR ss_get_vrr_mode_base(struct samsung_display_driver_data *vdd, int cur_rr)
{
	enum VRR_CMD_RR vrr_base;

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
		min_div_def = min_div_lowest = 96; // 1hz
		break;
	case VRR_60HS:
		base_rr = 120;
		max_div_def = 2; // 60hz
		min_div_def = min_div_lowest = 120; // 1hz
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
	lfd_base->highdot_div_def = 120 * 2; // 120hz % 240 = 0.5hz for highdot test (120hz base)

	vrr->lfd.base_rr = base_rr;

	LCD_DEBUG(vdd, "LFD(%s): base_rr: %uhz, def: %uhz(%u)~%uhz(%u), lowest: %uhz(%u) hightdot_div: %u\n",
			lfd_scope_name[scope], base_rr,
			DIV_ROUND_UP(base_rr, min_div_def), min_div_def,
			DIV_ROUND_UP(base_rr, max_div_def), max_div_def,
			DIV_ROUND_UP(base_rr, min_div_lowest), min_div_lowest,
			lfd_base->highdot_div_def);

	return 0;
}

#if 0
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

	vdd->vrr.need_vrr_update = false;

	return 0;
}
#endif

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
	mdnie_data->DSI_HBM_CE_MDNIE_SCR_1 = HBM_CE_MDNIE1_1;
	mdnie_data->DSI_HBM_CE_MDNIE_SCR_2 = HBM_CE_MDNIE2_1;
	mdnie_data->DSI_HBM_CE_MDNIE_SCR_3 = HBM_CE_MDNIE3_1;
	mdnie_data->DSI_HBM_CE_MDNIE_DIMMING_1 = HBM_CE_MDNIE1_3;
	mdnie_data->DSI_HBM_CE_MDNIE_DIMMING_2 = HBM_CE_MDNIE2_3;
	mdnie_data->DSI_HBM_CE_MDNIE_DIMMING_3 = HBM_CE_MDNIE3_3;

	mdnie_data->mdnie_tune_value_dsi = mdnie_tune_value_dsi;
	mdnie_data->hmt_color_temperature_tune_value_dsi = hmt_color_temperature_tune_value_dsi;

	mdnie_data->hdr_tune_value_dsi = hdr_tune_value_dsi;

	mdnie_data->light_notification_tune_value_dsi = light_notification_tune_value_dsi;

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

static void ss_copr_panel_init(struct samsung_display_driver_data *vdd)
{
	vdd->copr.ver = COPR_VER_5P0;
	vdd->copr.display_read = 0;
	ss_copr_init(vdd);
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

	return;
}

static int ss_read_udc_transmittance_data(struct samsung_display_driver_data *vdd)
{
	int ret, i = 0;
	u8 *read_buf;

	read_buf = kzalloc(vdd->udc.size, GFP_KERNEL);
	if (!read_buf) {
		LCD_ERR(vdd, "fail to kzalloc for udc read_buf\n");
		return 0;
	}

	ret = ss_send_cmd_get_rx(vdd, RX_UDC_DATA, read_buf);
	if (ret <= 0) {
		LCD_ERR(vdd, "fail to read udc data..\n");
		goto err;
	}

	memcpy(vdd->udc.data, read_buf, vdd->udc.size);

	for (i = 0; i < vdd->udc.size/2; i++)
		LCD_INFO(vdd, "[%d] %X.%X\n", i, vdd->udc.data[(i*2)+0], vdd->udc.data[(i*2)+1]);

	vdd->udc.read_done = true;
err:
	kfree(read_buf);
	return 0;
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
static u8 HS120_NIGHT_DIM_R_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_R_SIZE];
static u8 HS96_NIGHT_DIM_R_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_R_SIZE];
static u8 HS96_R_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_R_SIZE];

/* HS96 compensated data V type*/
static int HS120_NIGHT_DIM_V_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_V_SIZE];
static int HS96_NIGHT_DIM_V_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_V_SIZE];
static int HS96_V_TYPE_COMP[GAMMA_OFFSET_SIZE][GAMMA_V_SIZE];

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

	LCD_INFO(vdd, "== HS120_NIGHT_DIM_R_TYPE_COMP ==\n");
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			scnprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS120_NIGHT_DIM_R_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP[%d] : %s\n", i, pBuffer);
	}

	LCD_INFO(vdd, "== HS96_NIGHT_DIM_R_TYPE_COMP ==\n");
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_R_SIZE; j++)
			scnprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS96_NIGHT_DIM_R_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP[%d] : %s\n", i, pBuffer);
	}

	LCD_INFO(vdd, "== HS96_V_TYPE_COMP ==\n");
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		memset(pBuffer, 0x00, 256);
		for (j = 0; j < GAMMA_V_SIZE; j++)
			scnprintf(pBuffer + strnlen(pBuffer, 256), 256, " %02x", HS96_V_TYPE_COMP[i][j]);
		LCD_INFO(vdd, "COMP[%d] : %s\n", i, pBuffer);
	}

	LCD_INFO(vdd, "== HS96_R_TYPE_COMP ==\n");
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
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
}

static int ss_gm2_gamma_comp_init(struct samsung_display_driver_data *vdd)
{
	int i, j, m, offset;
	int val, *v_val_120;
	struct cmd_legoop_map *analog_map;

	LCD_INFO(vdd, "++\n");

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
	/* 3. [ALL] Make HSXX_V_TYPE_COMP (NORMAL + HBM) */
	/*************************************************************/

	/* 96HS NIGHT_DIM */
	analog_map = &vdd->br_info.analog_offset_96hs_night_dim[0];
	if (!analog_map->cmds)
		LCD_ERR(vdd, "No analog_offset_120hs...\n");

	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		if (i <= MAX_BL_PF_LEVEL)
			v_val_120 = HS120_NORMAL_V_TYPE_BUF;
		else
			v_val_120 = HS120_HBM_V_TYPE_BUF;

		for (j = 0; j < GAMMA_V_SIZE; j++) {
			if (!analog_map->cmds)
				offset = 0;
			else
				offset = analog_map->cmds[i][j];

			/* check underflow & overflow */
			if (v_val_120[j] + offset < 0) {
				HS96_NIGHT_DIM_V_TYPE_COMP[i][j] = 0;
			} else {
				if (j <= 2 || j >= (GAMMA_V_SIZE - 3)) {/* 1th & 14th 12bit(0xFFF) */
					val = v_val_120[j] + offset;
					if (val > 0xFFF)	/* check overflow */
						HS96_NIGHT_DIM_V_TYPE_COMP[i][j] = 0xFFF;
					else
						HS96_NIGHT_DIM_V_TYPE_COMP[i][j] = val;
				} else {	/* 2th - 13th 11bit(0x7FF) */
					val = v_val_120[j] + offset;
					if (val > 0x3FF)	/* check overflow */
						HS96_NIGHT_DIM_V_TYPE_COMP[i][j] = 0x3FF;
					else
						HS96_NIGHT_DIM_V_TYPE_COMP[i][j] = val;
				}
			}
		}
	}

	/* 120HS NIGHT_DIM */
	analog_map = &vdd->br_info.analog_offset_120hs_night_dim[0];
	if (!analog_map->cmds)
		LCD_ERR(vdd, "No analog_offset_120hs...\n");

	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		if (i <= MAX_BL_PF_LEVEL)
			v_val_120 = HS120_NORMAL_V_TYPE_BUF;
		else
			v_val_120 = HS120_HBM_V_TYPE_BUF;

		for (j = 0; j < GAMMA_V_SIZE; j++) {
			if (!analog_map->cmds)
				offset = 0;
			else
				offset = analog_map->cmds[i][j];

			/* check underflow & overflow */
			if (v_val_120[j] + offset < 0) {
				HS120_NIGHT_DIM_V_TYPE_COMP[i][j] = 0;
			} else {
				if (j <= 2 || j >= (GAMMA_V_SIZE - 3)) {/* 1th & 14th 12bit(0xFFF) */
					val = v_val_120[j] + offset;
					if (val > 0xFFF)	/* check overflow */
						HS120_NIGHT_DIM_V_TYPE_COMP[i][j] = 0xFFF;
					else
						HS120_NIGHT_DIM_V_TYPE_COMP[i][j] = val;
				} else {	/* 2th - 13th 11bit(0x7FF) */
					val = v_val_120[j] + offset;
					if (val > 0x3FF)	/* check overflow */
						HS120_NIGHT_DIM_V_TYPE_COMP[i][j] = 0x3FF;
					else
						HS120_NIGHT_DIM_V_TYPE_COMP[i][j] = val;
				}
			}
		}
	}

	/* 96HS */
	analog_map = &vdd->br_info.analog_offset_96hs[0];
	if (!analog_map->cmds)
		LCD_ERR(vdd, "No analog_offset_96hs...\n");

	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		if (i <= MAX_BL_PF_LEVEL)
			v_val_120 = HS120_NORMAL_V_TYPE_BUF;
		else
			v_val_120 = HS120_HBM_V_TYPE_BUF;

		for (j = 0; j < GAMMA_V_SIZE; j++) {
			if (!analog_map->cmds)
				offset = 0;
			else
				offset = analog_map->cmds[i][j];

			/* check underflow & overflow */
			if (v_val_120[j] + offset < 0) {
				HS96_V_TYPE_COMP[i][j] = 0;
			} else {
				if (j <= 2 || j >= (GAMMA_V_SIZE - 3)) {/* 1th & 14th 12bit(0xFFF) */
					val = v_val_120[j] + offset;
					if (val > 0xFFF)	/* check overflow */
						HS96_V_TYPE_COMP[i][j] = 0xFFF;
					else
						HS96_V_TYPE_COMP[i][j] = val;
				} else {	/* 2th - 13th 11bit(0x7FF) */
					val = v_val_120[j] + offset;
					if (val > 0x3FF)	/* check overflow */
						HS96_V_TYPE_COMP[i][j] = 0x3FF;
					else
						HS96_V_TYPE_COMP[i][j] = val;
				}
			}
		}
	}

	/******************************************************/
	/* 4. translate HSXX_V_TYPE_COMP type to Register type*/
	/******************************************************/

	/* 96HS NIGHT_DIM */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		m = 0; // GAMMA SET size
		for (j = 0; j < GAMMA_V_SIZE; j += RGB_MAX) {
			if (j == 0 || j == GAMMA_V_SIZE - 3) {
				HS96_NIGHT_DIM_R_TYPE_COMP[i][m++] = GET_BITS(HS96_NIGHT_DIM_V_TYPE_COMP[i][j+R], 8, 11);
				HS96_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_NIGHT_DIM_V_TYPE_COMP[i][j+G], 8, 11) << 4)
											| (GET_BITS(HS96_NIGHT_DIM_V_TYPE_COMP[i][j+B], 8, 11));
				HS96_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_NIGHT_DIM_V_TYPE_COMP[i][j+R], 0, 7));
				HS96_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_NIGHT_DIM_V_TYPE_COMP[i][j+G], 0, 7));
				HS96_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_NIGHT_DIM_V_TYPE_COMP[i][j+B], 0, 7));
			} else {	/* 1st ~ 10th */
				HS96_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_NIGHT_DIM_V_TYPE_COMP[i][j+R], 8, 9) << 4)
											| (GET_BITS(HS96_NIGHT_DIM_V_TYPE_COMP[i][j+G], 8, 9) << 2)
											| GET_BITS(HS96_NIGHT_DIM_V_TYPE_COMP[i][j+B], 8, 9);
				HS96_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_NIGHT_DIM_V_TYPE_COMP[i][j+R], 0, 7));
				HS96_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_NIGHT_DIM_V_TYPE_COMP[i][j+G], 0, 7));
				HS96_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS96_NIGHT_DIM_V_TYPE_COMP[i][j+B], 0, 7));
			}
		}
	}

	/* 120HS NIGHT_DIM */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
		m = 0; // GAMMA SET size
		for (j = 0; j < GAMMA_V_SIZE; j += RGB_MAX) {
			if (j == 0 || j == GAMMA_V_SIZE - 3) {
				HS120_NIGHT_DIM_R_TYPE_COMP[i][m++] = GET_BITS(HS120_NIGHT_DIM_V_TYPE_COMP[i][j+R], 8, 11);
				HS120_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_NIGHT_DIM_V_TYPE_COMP[i][j+G], 8, 11) << 4)
											| (GET_BITS(HS120_NIGHT_DIM_V_TYPE_COMP[i][j+B], 8, 11));
				HS120_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_NIGHT_DIM_V_TYPE_COMP[i][j+R], 0, 7));
				HS120_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_NIGHT_DIM_V_TYPE_COMP[i][j+G], 0, 7));
				HS120_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_NIGHT_DIM_V_TYPE_COMP[i][j+B], 0, 7));
			} else {	/* 1st ~ 10th */
				HS120_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_NIGHT_DIM_V_TYPE_COMP[i][j+R], 8, 9) << 4)
											| (GET_BITS(HS120_NIGHT_DIM_V_TYPE_COMP[i][j+G], 8, 9) << 2)
											| GET_BITS(HS120_NIGHT_DIM_V_TYPE_COMP[i][j+B], 8, 9);
				HS120_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_NIGHT_DIM_V_TYPE_COMP[i][j+R], 0, 7));
				HS120_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_NIGHT_DIM_V_TYPE_COMP[i][j+G], 0, 7));
				HS120_NIGHT_DIM_R_TYPE_COMP[i][m++] = (GET_BITS(HS120_NIGHT_DIM_V_TYPE_COMP[i][j+B], 0, 7));
			}
		}
	}

	/* 96HS */
	for (i = 0; i < GAMMA_OFFSET_SIZE; i++) {
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

static bool analog_offset_on(struct samsung_display_driver_data *vdd)
{	
	struct cmd_ref_state *state = &vdd->cmd_ref_state;
	int cur_rr = state->cur_refresh_rate;
	int base_rr = ss_get_vrr_mode_base(vdd, cur_rr);
	int bl_lvl = state->bl_level;
	bool analog_enable = false;

	if (base_rr == VRR_120HS) {
		if (bl_lvl <= 30) {
			analog_enable = true;
		} else {
			analog_enable = false;
		}
	} else {
		analog_enable = true;	/* 48/96 base */
	}

	LCD_DEBUG(vdd, "analog_enable = %d\n", analog_enable);

	return analog_enable;
}

static int update_analog_enable(struct samsung_display_driver_data *vdd,
			char *val, struct ss_cmd_desc *cmd)
{
	struct cmd_ref_state *state = &vdd->cmd_ref_state;
	int cur_rr = state->cur_refresh_rate;
	int base_rr = ss_get_vrr_mode_base(vdd, cur_rr);
	int bl_lvl = state->bl_level;
	int i = -1;
	bool analog_enable = false;

	while (!cmd->pos_0xXX[++i] && i < cmd->tx_len);

	if (i + 1 > cmd->tx_len) {
		LCD_ERR(vdd, "fail to find proper 0xXX position(%d, %d)\n",
				i, cmd->tx_len);
		goto err_skip;
	}

	/* Analog enabler is enabled when bl_lv is under 30lv 
	 * bl_lvl <= 30 && night_dim on -> 120 night dim analog offset gamma (enabler ON)
	 * bl_lvl <= 30 && night_dim off -> 120 mtp gamma (enabler ON)
	 */
	if (base_rr == VRR_120HS) {
		if (bl_lvl <= 30) {
			analog_enable = true;
		} else {
			analog_enable = false;
		}
	} else {
		analog_enable = true;	/* 48/96 base */
	}

	cmd->txbuf[i] = analog_enable ? 0x01 : 0x00;

	LCD_INFO_IF(vdd, "enable[%d] bl_lvl: %d, cur_rr: %d, night_dim : %d\n",
		analog_enable, bl_lvl, cur_rr, vdd->night_dim);

	return 0;

err_skip:
	cmd->skip_by_cond = true;
	return -EINVAL;
}

/* UPDATE ANALOG_OFFSET_2
 * Split from Q4's analog_offset 1st line
 */
static int update_analog1_Q5_S6E3XA2_AMF756BQ03(
			struct samsung_display_driver_data *vdd,
			char *val, struct ss_cmd_desc *cmd)
{
	struct cmd_ref_state *state = &vdd->cmd_ref_state;
	int cur_rr = state->cur_refresh_rate;
	int base_rr = ss_get_vrr_mode_base(vdd, cur_rr);
	int bl_lvl;
	int i = -1;

	bl_lvl = state->bl_level;

	while (!cmd->pos_0xXX[++i] && i < cmd->tx_len);

	if (i + 1 >= cmd->tx_len) {
		LCD_ERR(vdd, "fail to find proper 0xXX position\n");
		return -EINVAL;
	}

	if (base_rr == VRR_120HS) {
		if (bl_lvl <= 30) {
			if (vdd->night_dim)
				memcpy(&cmd->txbuf[i], &HS120_NIGHT_DIM_R_TYPE_COMP[bl_lvl][0], 37);
			else
				memcpy(&cmd->txbuf[i], &HS120_NORMAL_R_TYPE_BUF[0], 37);
		}
	} else {
		if (vdd->night_dim && (bl_lvl <= 30)) {
			memcpy(&cmd->txbuf[i], &HS96_NIGHT_DIM_R_TYPE_COMP[bl_lvl][0], 37);
		} else {
			memcpy(&cmd->txbuf[i], &HS96_R_TYPE_COMP[bl_lvl][0], 37);
		}
	}

	vdd->br_info.last_tx_time = ktime_get();

	return 0;
}

/* UPDATE ANALOG_OFFSET_2
 * Split from Q4's analog_offset 2nd line
 */
static int update_analog2_Q5_S6E3XA2_AMF756BQ03(
			struct samsung_display_driver_data *vdd,
			char *val, struct ss_cmd_desc *cmd)
{
	struct cmd_ref_state *state = &vdd->cmd_ref_state;
	int cur_rr = state->cur_refresh_rate;
	int base_rr = ss_get_vrr_mode_base(vdd, cur_rr);
	int bl_lvl;
	int i = -1;

	bl_lvl = state->bl_level;

	while (!cmd->pos_0xXX[++i] && i < cmd->tx_len);

	if (i + 1 >= cmd->tx_len) {
		LCD_ERR(vdd, "fail to find proper 0xXX position\n");
		return -EINVAL;
	}

	if (base_rr == VRR_120HS) {
		if (bl_lvl <= 30) {
			if (vdd->night_dim)
				memcpy(&cmd->txbuf[i], &HS120_NIGHT_DIM_R_TYPE_COMP[bl_lvl][37], 17);
			else
				memcpy(&cmd->txbuf[i], &HS120_NORMAL_R_TYPE_BUF[37], 17);
		}
	} else {
		if (vdd->night_dim && (bl_lvl <= 30)) {
			memcpy(&cmd->txbuf[i], &HS96_NIGHT_DIM_R_TYPE_COMP[bl_lvl][37], 17);
		} else {
			memcpy(&cmd->txbuf[i], &HS96_R_TYPE_COMP[bl_lvl][37], 17);
		}
	}

	vdd->br_info.last_tx_time = ktime_get();

	return 0;
}

static int update_aor_Q5_S6E3XA2_AMF756BQ03(struct samsung_display_driver_data *vdd,
			char *val, struct ss_cmd_desc *cmd)
{

	struct cmd_ref_state *state = &vdd->cmd_ref_state;
	int cur_rr = state->cur_refresh_rate;
	int bl_lvl = state->bl_level;
	int base_rr = ss_get_vrr_mode_base(vdd, cur_rr);
	struct cmd_legoop_map *manual_aor_map = NULL;
	int i = -1;

	while (!cmd->pos_0xXX[++i] && i < cmd->tx_len);

	if (i + 1 >= cmd->tx_len) {
		LCD_ERR(vdd, "fail to find proper 0xXX position\n");
		return -EINVAL;
	}

	if (base_rr == VRR_96HS) {
		manual_aor_map = &vdd->br_info.manual_aor_96hs[vdd->panel_revision];
		cmd->txbuf[i] = 0x01;
	} else {
		manual_aor_map = &vdd->br_info.manual_aor_120hs[vdd->panel_revision];
		cmd->txbuf[i] = 0x00;
	}

	if (manual_aor_map) {
		if (bl_lvl > manual_aor_map->row_size) {
			LCD_ERR(vdd, "invalid bl [%d]/[%d]\n", bl_lvl, manual_aor_map->row_size);
			return 0;
		}

		cmd->txbuf[i + 1] = manual_aor_map->cmds[bl_lvl][0];
		cmd->txbuf[i + 2] = manual_aor_map->cmds[bl_lvl][1];

		LCD_INFO(vdd, "bl_lvl: %d, cur_rr: %d, i: %d, aor: 0x%X%X, tx_len: %d\n",
				bl_lvl, cur_rr, i, manual_aor_map->cmds[bl_lvl][0], manual_aor_map->cmds[bl_lvl][1], cmd->tx_len);
	}

	return 0;
}

static void ss_set_night_dim(struct samsung_display_driver_data *vdd, int val)
{
	ss_brightness_dcs(vdd, USE_CURRENT_BL_LEVEL, BACKLIGHT_NORMAL);
	return;
}

void Q5_S6E3XA2_AMF756BQ03_QXGA_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "%s\n", ss_get_panel_name(vdd));

	vdd->panel_state = PANEL_PWR_OFF;	/* default OFF */

	vdd->panel_func.samsung_panel_on_pre = samsung_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = samsung_panel_on_post;

	vdd->panel_func.samsung_panel_revision = ss_panel_revision;
	vdd->panel_func.samsung_module_info_read = ss_module_info_read;
	vdd->panel_func.samsung_ddi_id_read = ss_ddi_id_read;
	vdd->panel_func.samsung_octa_id_read = ss_octa_id_read;

	vdd->copr.panel_init = ss_copr_panel_init;

	/* Brightness */
//	vdd->panel_func.pre_brightness = ss_pre_brightness;
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

	vdd->self_disp.factory_support = true;
	vdd->self_disp.init = self_display_init_XA2;
	vdd->self_disp.data_init = ss_self_display_data_init;

	vdd->mafpc.init = ss_mafpc_init_XA2;
	vdd->mafpc.data_init = ss_mafpc_data_init;

	/* Gamma compensation (Gamma Offset) */
	vdd->panel_func.samsung_gm2_gamma_comp_init = ss_gm2_gamma_comp_init;
	vdd->panel_func.samsung_print_gamma_comp = ss_print_gamma_comp;
	vdd->panel_func.read_flash = ss_read_flash;


	/* UDC */
	vdd->panel_func.read_udc_data = ss_read_udc_transmittance_data;
#if 0
	vdd->panel_func.read_udc_gamma_data = ss_read_udc_gamma_data;
	vdd->panel_func.udc_gamma_comp = ss_udc_gamma_comp;
	vdd->panel_func.restore_udc_orig_gamma = ss_udc_restore_orig_gamma;
#endif

	/* VRR */
	vdd->panel_func.samsung_lfd_get_base_val = ss_update_base_lfd_val;
	ss_vrr_init(&vdd->vrr);

	/* early te */
	vdd->early_te = false;
	vdd->check_early_te = 0;

	vdd->panel_func.samsung_check_support_mode = ss_check_support_mode;

	/* change  MIPI Drive strength values for this panel - request by HW group */
	vdd->motto_info.motto_swing = 0xEE;
	vdd->motto_info.vreg_ctrl_0 = 0x47;

	/* Below data will be genarated by script in Kbuild file */
	vdd->h_buf = Q5_S6E3XA2_AMF756BQ03_PDF_DATA;
	vdd->h_size = sizeof(Q5_S6E3XA2_AMF756BQ03_PDF_DATA);

	/* Get f_buf from header file data to cover recovery mode
	 * Below code should be called before any PDF parsing code such as update_glut_map
	 */
	if (!vdd->file_loading && vdd->h_buf) {
		LCD_INFO(vdd, "Get f_buf from header file data(%llu)\n", vdd->h_size);
		vdd->f_buf = vdd->h_buf;
		vdd->f_size = vdd->h_size;
	}

	register_op_sym_cb(vdd, "AOR", update_aor_Q5_S6E3XA2_AMF756BQ03, true);
	register_op_sym_cb(vdd, "ANALOG_OFFSET_1",
			update_analog1_Q5_S6E3XA2_AMF756BQ03, true);
	register_op_sym_cb(vdd, "ANALOG_OFFSET_2",
			update_analog2_Q5_S6E3XA2_AMF756BQ03, true);
	register_op_sym_cb(vdd, "ANALOG_ENABLE", update_analog_enable, true);

	vdd->panel_func.analog_offset_on = analog_offset_on;

	/* night dim */
	vdd->panel_func.set_night_dim = ss_set_night_dim;
}
