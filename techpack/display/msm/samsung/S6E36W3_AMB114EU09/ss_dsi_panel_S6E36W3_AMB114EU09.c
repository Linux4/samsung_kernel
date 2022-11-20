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
 *
*/
#include "ss_dsi_panel_S6E36W3_AMB114EU09.h"
#include "ss_dsi_mdnie_S6E36W3_AMB114EU09.h"
#include "ss_dsi_interpolation_S6E36W3_AMB114EU09.h"

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);
	ss_panel_attach_set(vdd, true);
	LCD_INFO(vdd, "-: ndx=%d \n", vdd->ndx);

	return true;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "+: ndx=%d\n", vdd->ndx);
	if (vdd->self_disp.self_mask_img_write)
		vdd->self_disp.self_mask_img_write(vdd);
	if (vdd->self_disp.self_mask_on)
		vdd->self_disp.self_mask_on(vdd, true);
	LCD_INFO(vdd, "-: ndx=%d\n", vdd->ndx);

	return true;
}

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{
	if (vdd->manufacture_id_dsi == PBA_ID)
		ss_panel_attach_set(vdd, false);
	else
		ss_panel_attach_set(vdd, true);

	/* Specific colors 
	 * ZW : WHITE => transmittance 90%, use rev K 
	 * ZA : GRAY => transmittance 60%, use rev A
	 * ZN : BRONZE => transmittance 60%, use rev A
	 * color rev is from rev 'K'
	 */

	LCD_INFO(vdd, "window_color : %s\n", vdd->window_color);

	if (!strncmp(vdd->window_color, "ZW", 2)) {	/* WHITE */
		vdd->panel_revision = 'K';
	} else {
		switch (ss_panel_rev_get(vdd)) {
		case 0x00:
			vdd->panel_revision = 'A';
		default:
			vdd->panel_revision = 'A';
			LCD_ERR(vdd, "Invalid panel_rev(default rev : %c)\n",
					vdd->panel_revision);
			break;
		}
	}

	vdd->panel_revision -= 'A';

	LCD_INFO_ONCE(vdd, "panel_revision = %c %d \n",
					vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

static int ss_manufacture_date_read(struct samsung_display_driver_data *vdd)
{
	unsigned char date[4];
	int year, month, day;
	int hour, min;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (A1h 5,6 th) for manufacture date */
	if (ss_get_cmds(vdd, RX_MANUFACTURE_DATE)->count) {
		ss_panel_data_read(vdd, RX_MANUFACTURE_DATE, date, LEVEL1_KEY);

		year = date[0] & 0xf0;
		year >>= 4;
		year += 2011; // 0 = 2011 year
		month = date[0] & 0x0f;
		day = date[1] & 0x1f;
		hour = date[2] & 0x0f;
		min = date[3] & 0x1f;

		vdd->manufacture_date_dsi = year * 10000 + month * 100 + day;
		vdd->manufacture_time_dsi = hour * 100 + min;

		LCD_INFO(vdd, "manufacture_date DSI%d = (%d%04d) - year(%d) month(%d) day(%d) hour(%d) min(%d)\n",
			vdd->ndx, vdd->manufacture_date_dsi, vdd->manufacture_time_dsi,
			year, month, day, hour, min);

	} else {
		LCD_ERR(vdd, "DSI%d no manufacture_date_rx_cmds cmds(%d)", vdd->ndx, vdd->panel_revision);
		return false;
	}

	return true;
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

static int ss_cell_id_read(struct samsung_display_driver_data *vdd)
{
	char cell_id_buffer[MAX_CELL_ID] = {0,};
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read Panel Unique Cell ID (C8h 41~51th) */
	if (ss_get_cmds(vdd, RX_CELL_ID)->count) {
		memset(cell_id_buffer, 0x00, MAX_CELL_ID);

		ss_panel_data_read(vdd, RX_CELL_ID, cell_id_buffer, LEVEL1_KEY);

		for (loop = 0; loop < MAX_CELL_ID; loop++)
			vdd->cell_id_dsi[loop] = cell_id_buffer[loop];

		LCD_INFO(vdd, "DSI%d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			vdd->ndx, vdd->cell_id_dsi[0],
			vdd->cell_id_dsi[1],	vdd->cell_id_dsi[2],
			vdd->cell_id_dsi[3],	vdd->cell_id_dsi[4],
			vdd->cell_id_dsi[5],	vdd->cell_id_dsi[6],
			vdd->cell_id_dsi[7],	vdd->cell_id_dsi[8],
			vdd->cell_id_dsi[9],	vdd->cell_id_dsi[10]);

	} else {
		LCD_ERR(vdd, "DSI%d no cell_id_rx_cmds cmd\n", vdd->ndx);
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

#if 0
static struct dsi_panel_cmd_set *ss_vint(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *vint_cmds = ss_get_cmds(vdd, TX_VINT);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vint_cmds)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)vint_cmds);
		return NULL;
	}

	/* TODO: implement xtalk_mode
	if (vdd->xtalk_mode)
		cmd_index = 1;	// VGH 6.2 V
	else
		cmd_index = 0;	// VGH 7.0 V
	 */

	*level_key = LEVEL1_KEY;

	return vint_cmds;
}

static int ss_hbm_read(struct samsung_display_driver_data *vdd)
{
	int i, j;
	char hbm_buffer1[33];
	struct dsi_panel_cmd_set *hbm_gamma_cmds = ss_get_cmds(vdd, TX_HBM_GAMMA);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_gamma_cmds)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)hbm_gamma_cmds);
		return false;
	}

	/* Read mtp (B3h 3~34th) for hbm gamma */
	ss_panel_data_read(vdd, RX_HBM, hbm_buffer1, LEVEL1_KEY);

	/* V255 RGB */
	hbm_gamma_cmds->cmds[0].ss_txbuf[1] = (hbm_buffer1[0] & 0x04) >> 2;
	hbm_gamma_cmds->cmds[0].ss_txbuf[2] = hbm_buffer1[2];
	hbm_gamma_cmds->cmds[0].ss_txbuf[3] = (hbm_buffer1[0] & 0x02) >> 1;
	hbm_gamma_cmds->cmds[0].ss_txbuf[4] = hbm_buffer1[3];
	hbm_gamma_cmds->cmds[0].ss_txbuf[5] = (hbm_buffer1[0] & 0x01) >> 0;
	hbm_gamma_cmds->cmds[0].ss_txbuf[6] = hbm_buffer1[4];

	/* V203 ~ V1 */
	for (i = 7, j = 5; i <= 33; i++, j++)
		hbm_gamma_cmds->cmds[0].ss_txbuf[i] = hbm_buffer1[j];

	/* VT RGB */
	hbm_gamma_cmds->cmds[0].ss_txbuf[34] = hbm_buffer1[0] & 0xF0;
	hbm_gamma_cmds->cmds[0].ss_txbuf[34] |= (hbm_buffer1[1] & 0xF0) >> 4;
	hbm_gamma_cmds->cmds[0].ss_txbuf[35] = hbm_buffer1[1] & 0x0F;

	return true;
}

static int get_hbm_candela_value(int level)
{
	if (level == 13)
		return 443;
	else if (level == 6)
		return 465;
	else if (level == 7)
		return 488;
	else if (level == 8)
		return 510;
	else if (level == 9)
		return 533;
	else if (level == 10)
		return 555;
	else if (level == 11)
		return 578;
	else if (level == 12)
		return 600;
	else
		return 600;
}

static struct dsi_panel_cmd_set *ss_hbm_gamma(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *hbm_gamma_cmds = ss_get_cmds(vdd, TX_HBM_GAMMA);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_gamma_cmds)) {
		LCD_ERR(vdd, "Invalid data  vdd : 0x%zx cmd : 0x%zx", (size_t)vdd, (size_t)hbm_gamma_cmds);
		return NULL;
	}

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi->generate_hbm_gamma)) {
		LCD_ERR(vdd, "generate_hbm_gamma is NULL error");
		return NULL;
	} else {
		vdd->smart_dimming_dsi->generate_hbm_gamma(
			vdd->smart_dimming_dsi,
			vdd->br.auto_level,
			&hbm_gamma_cmds->cmds[0].ss_txbuf[1]);

		*level_key = LEVEL1_KEY;
		return hbm_gamma_cmds;
	}
}
#endif

/*
*samsung,hbm_etc_tx_cmds_revA = [
*	29 00 00 00 00 00 03 F0 5A 5A		 TEST KEY Enable
*	29 00 00 00 00 00 04 B5 19 DC 0A	 MPS/ELVSS Setting
*	29 00 00 00 00 00 02 B3 21			 HBM ACL ON
*	29 00 00 00 00 00 02 55 00			 ACL OFF
*	29 00 00 00 00 00 04 B1 00 20 20	 AID Setting 1000nit
*or	29 00 00 00 00 00 04 B1 00 20 82	 AID Setting 670nit
*	29 00 00 00 00 00 02 53 C0			 HBM ON 
*	29 00 00 00 00 00 02 F7 03			 Gamma & AID update
*	29 01 00 00 00 00 03 F0 5A 5A		 TEST KEY Disable
*];
*/

static struct dsi_panel_cmd_set *ss_hbm_etc(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *hbm_etc_cmds = ss_get_cmds(vdd, TX_HBM_ETC);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_etc_cmds)) {
		LCD_ERR(vdd, "Invalid data  vdd : 0x%zx cmd : 0x%zx", (size_t)vdd, (size_t)hbm_etc_cmds);
		return NULL;
	}

	/* MPS/ELVSS : 0xB5 1th TSET */
	hbm_etc_cmds->cmds[1].ss_txbuf[1] =
		vdd->br_info.temperature > 0 ? vdd->br_info.temperature : 0x80|(-1*vdd->br_info.temperature);

	if (vdd->br_info.common_br.cd_level == 670) {
		hbm_etc_cmds->cmds[4].ss_txbuf[3] = 0x82;
	}
	else if (vdd->br_info.common_br.cd_level == 1000) {
		hbm_etc_cmds->cmds[4].ss_txbuf[3] = 0x20;
	}
	else if (vdd->br_info.common_br.cd_level == 440) { /* BLOOMX_TODO : need to add proper aor value for WHITE */
		hbm_etc_cmds->cmds[4].ss_txbuf[3] = 0xA0;
	}
	else if (vdd->br_info.common_br.cd_level == 666) {
		hbm_etc_cmds->cmds[4].ss_txbuf[3] = 0x30;
	}

	LCD_INFO(vdd, "vdd->br_info.common_br.cd_level = %d\n", vdd->br_info.common_br.cd_level);

	return hbm_etc_cmds;
}

#define COORDINATE_DATA_SIZE 6

#define F1(x, y) ((y)-((39*(x))/38)-95)
#define F2(x, y) ((y)-((36*(x))/35)-56)
#define F3(x, y) ((y)+((7*(x))/1)-24728)
#define F4(x, y) ((y)+((25*(x))/7)-14031)

/* Normal Mode */
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

/* sRGB/Adobe RGB Mode */
static char coordinate_data_2[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* dummy */
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* Tune_8 */
	{0xff, 0x00, 0xfc, 0x00, 0xf6, 0x00}, /* Tune_9 */
};

static char (*coordinate_data_multi[MAX_MODE])[COORDINATE_DATA_SIZE] = {
	coordinate_data_2, /* DYNAMIC - DCI */
	coordinate_data_2, /* STANDARD - sRGB/Adobe RGB */
	coordinate_data_2, /* NATURAL - sRGB/Adobe RGB */
	coordinate_data_1, /* MOVIE - Normal */
	coordinate_data_1, /* AUTO - Normal */
	coordinate_data_1, /* READING - Normal */
};

static int mdnie_coordinate_index(int x, int y)
{
	int tune_number = 0;

	if (F1(x, y) > 0) {
		if (F3(x, y) > 0) {
			tune_number = 3;
		} else {
			if (F4(x, y) < 0)
				tune_number = 1;
			else
				tune_number = 2;
		}
	} else {
		if (F2(x, y) < 0) {
			if (F3(x, y) > 0) {
				tune_number = 9;
			} else {
				if (F4(x, y) < 0)
					tune_number = 7;
				else
					tune_number = 8;
			}
		} else {
			if (F3(x, y) > 0)
				tune_number = 6;
			else {
				if (F4(x, y) < 0)
					tune_number = 4;
				else
					tune_number = 5;
			}
		}
	}

	return tune_number;
}

static int ss_mdnie_read(struct samsung_display_driver_data *vdd)
{
	char x_y_location[4];
	int mdnie_tune_index = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (A1h 1~5th) for ddi id */
	if (ss_get_cmds(vdd, RX_MDNIE)->count) {
		ss_panel_data_read(vdd, RX_MDNIE, x_y_location, LEVEL1_KEY);

		vdd->mdnie.mdnie_x = x_y_location[0] << 8 | x_y_location[1];	/* X */
		vdd->mdnie.mdnie_y = x_y_location[2] << 8 | x_y_location[3];	/* Y */

		mdnie_tune_index = mdnie_coordinate_index(vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);

		coordinate_tunning_multi(vdd, coordinate_data_multi, mdnie_tune_index,
				ADDRESS_SCR_WHITE_RED, COORDINATE_DATA_SIZE);

		LCD_INFO(vdd, "DSI%d : X-%d Y-%d \n", vdd->ndx,
			vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);
	} else {
		LCD_ERR(vdd, "DSI%d no mdnie_read_rx_cmds cmds", vdd->ndx);
		return false;
	}

	return true;
}

static int ss_smart_dimming_init(struct samsung_display_driver_data *vdd,
		struct brightness_table *br_tbl)
{
	struct smartdim_conf *sconf;
	struct dsi_panel_cmd_set *pcmds;

	sconf = vdd->panel_func.samsung_smart_get_conf();
	if (IS_ERR_OR_NULL(sconf)) {
		LCD_ERR(vdd, "fail to get smartdim_conf (ndx: %d)\n", vdd->ndx);
		return false;
	}

	br_tbl->smart_dimming_dsi = sconf;

	if (IS_ERR_OR_NULL(sconf)) {
		LCD_ERR(vdd, "DSI%d smart_dimming_dsi is null", vdd->ndx);
		return false;
	}

	ss_panel_data_read(vdd, RX_SMART_DIM_MTP,
			sconf->mtp_buffer, LEVEL1_KEY);

	/* Initialize smart dimming related things here */
	sconf->lux_tab = vdd->br_info.candela_map_table[NORMAL][vdd->panel_revision].cd;
	sconf->lux_tabsize = vdd->br_info.candela_map_table[NORMAL][vdd->panel_revision].tab_size;
	sconf->man_id = vdd->manufacture_id_dsi;
	if (vdd->panel_func.samsung_panel_revision)
		sconf->panel_revision = vdd->panel_func.samsung_panel_revision(vdd);

	/* copy hbm gamma payload for hbm interpolation calc */
	pcmds = ss_get_cmds(vdd, TX_HBM_GAMMA);
	if (pcmds->count)
		sconf->hbm_payload = &pcmds->cmds[0].ss_txbuf[1];
	else
		LCD_ERR(vdd, "No hbm gamma cmds..\n");
	
	/* Just a safety check to ensure smart dimming data is initialised well */
	sconf->init(sconf);

	vdd->br_info.temperature = 20; // default temperature

	vdd->br_info.smart_dimming_loaded_dsi = true;
	LCD_INFO(vdd, "DSI%d : --\n", vdd->ndx);

	return true;
}

static struct dsi_panel_cmd_set aid_cmd;
static struct dsi_panel_cmd_set *ss_aid(struct samsung_display_driver_data *vdd, int *level_key)
{
	int cd_index = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	cd_index = vdd->br_info.common_br.cd_idx;

	aid_cmd.count = 1;
	aid_cmd.cmds = &(ss_get_cmds(vdd, TX_AID_SUBDIVISION)->cmds[cd_index]);
	LCD_INFO(vdd, "[%d] level(%d), aid(%x %x)\n",
			cd_index, vdd->br_info.common_br.bl_level,
			aid_cmd.cmds->ss_txbuf[1],
			aid_cmd.cmds->ss_txbuf[2]);

	*level_key = LEVEL1_KEY;

	return &aid_cmd;
}

#define GRADUAL_ACL_STEP_JP2 6 //refer config_display.xml
static struct dsi_panel_cmd_set *ss_acl_on(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;

	pcmds = ss_get_cmds(vdd, TX_ACL_ON);
	if (SS_IS_CMDS_NULL(pcmds)) {
		LCD_ERR(vdd, "No cmds for TX_ACL_ON..\n");
		return NULL;
	}

	LCD_INFO(vdd, "gradual_acl: %d, acl per: 0x%x",
			vdd->br_info.gradual_acl_val, pcmds->cmds[0].ss_txbuf[6]);

	return pcmds;
}

static struct dsi_panel_cmd_set *ss_acl_off(struct samsung_display_driver_data *vdd, int *level_key)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;

	return ss_get_cmds(vdd, TX_ACL_OFF);
}

static struct dsi_panel_cmd_set elvss_cmd;
static struct dsi_panel_cmd_set *ss_elvss(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *elvss_cmds;
	int cd_index = 0;
	int cmd_idx = 0;
	int candela_value = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}
	
	//SUB_DISPLAY_TBD
	elvss_cmds = ss_get_cmds(vdd, TX_ELVSS);
	cd_index = vdd->br_info.common_br.cd_idx;

	if (!vdd->br_info.smart_acl_elvss_map_table[vdd->panel_revision].size ||
		cd_index > vdd->br_info.smart_acl_elvss_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->br_info.smart_acl_elvss_map_table[vdd->panel_revision].cmd_idx[cd_index];

	/* 	cmds[0].ss_txbuf[1] = TSET | cmds[0].ss_txbuf[2] = MPS_CON | ss_txbuf[3] = ELVSS_Dim_offset */
	elvss_cmds->cmds[cmd_idx].ss_txbuf[1] =
		vdd->br_info.temperature > 0 ? vdd->br_info.temperature : 0x80|(-1*vdd->br_info.temperature);
	/* 0xB5 2th MSP */
	if (vdd->br_info.common_br.cd_level > 39)
		elvss_cmds->cmds[cmd_idx].ss_txbuf[2] = 0xDC;
	else
		elvss_cmds->cmds[cmd_idx].ss_txbuf[2] = 0xCC;

	/* ELVSS Compensation for Low Temperature & Low Birghtness*/
	br_interpolation_generate_event(vdd, GEN_NORMAL_INTERPOLATION_ELVSS, &elvss_cmds->cmds->ss_txbuf[3]);


	LCD_INFO(vdd, "cd_index (%d) cmd_idx(%d) candela_value(%d) B5 1st(%x) 2nd (%x) 3rd (%x)\n",
		cd_index, cmd_idx, candela_value,
		elvss_cmds->cmds[cmd_idx].ss_txbuf[1],
		elvss_cmds->cmds[cmd_idx].ss_txbuf[2],
		elvss_cmds->cmds[cmd_idx].ss_txbuf[3]);

	elvss_cmd.cmds = &(elvss_cmds->cmds[cmd_idx]);
	elvss_cmd.count = 1;

	*level_key = LEVEL1_KEY;

	return &elvss_cmd;
end:
	LCD_ERR(vdd, "error");
	return NULL;
}

static struct dsi_panel_cmd_set *ss_gamma(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set  *gamma_cmds = ss_get_cmds(vdd, TX_GAMMA);

	if (IS_ERR_OR_NULL(vdd) || SS_IS_CMDS_NULL(gamma_cmds)) {
		LCD_ERR(vdd, "Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)gamma_cmds);
		return NULL;
	}

	LCD_DEBUG(vdd, "bl_level : %d candela : %dCD\n", vdd->br_info.common_br.bl_level, vdd->br_info.common_br.cd_level);

	*level_key = LEVEL1_KEY;

	br_interpolation_generate_event(vdd, GEN_NORMAL_GAMMA, &gamma_cmds->cmds[0].ss_txbuf[1]);

	return gamma_cmds;
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
	mdnie_data->DSI_COLOR_BLIND_MDNIE_1 = DSI0_COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_1 = DSI0_RGB_SENSOR_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_2 = DSI0_RGB_SENSOR_MDNIE_2;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_3 = DSI0_RGB_SENSOR_MDNIE_2;
	mdnie_data->DSI_TRANS_DIMMING_MDNIE = DSI0_RGB_SENSOR_MDNIE_2;

	mdnie_data->DSI_BYPASS_MDNIE = DSI0_BYPASS_MDNIE;
	mdnie_data->DSI_NEGATIVE_MDNIE = DSI0_NEGATIVE_MDNIE;
	mdnie_data->DSI_COLOR_BLIND_MDNIE = DSI0_COLOR_BLIND_MDNIE;
	mdnie_data->DSI_HBM_CE_MDNIE = DSI0_HBM_CE_MDNIE;
	mdnie_data->DSI_RGB_SENSOR_MDNIE = DSI0_RGB_SENSOR_MDNIE;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE = DSI0_UI_DYNAMIC_MDNIE;
	mdnie_data->DSI_UI_AUTO_MDNIE = DSI0_UI_AUTO_MDNIE;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE = DSI0_CAMERA_AUTO_MDNIE;	
	mdnie_data->DSI_GRAYSCALE_MDNIE = DSI0_GRAYSCALE_MDNIE;
	mdnie_data->DSI_GRAYSCALE_NEGATIVE_MDNIE = DSI0_GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data->DSI_CURTAIN = DSI0_SCREEN_CURTAIN_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE = DSI0_NIGHT_MODE_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE_SCR = DSI0_NIGHT_MODE_MDNIE_1;
	mdnie_data->DSI_COLOR_BLIND_MDNIE_SCR = DSI0_COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_SCR = DSI0_RGB_SENSOR_MDNIE_1;
	mdnie_data->DSI_COLOR_LENS_MDNIE = DSI0_COLOR_LENS_MDNIE;
	mdnie_data->DSI_COLOR_LENS_MDNIE_SCR = DSI0_COLOR_LENS_MDNIE_1;

	mdnie_data->mdnie_tune_value_dsi = mdnie_tune_value_dsi0;
	mdnie_data->hmt_color_temperature_tune_value_dsi = hmt_color_temperature_tune_value_dsi0;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data->dsi_bypass_mdnie_size = ARRAY_SIZE(DSI0_BYPASS_MDNIE);
	mdnie_data->mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET;
	mdnie_data->mdnie_step_index[MDNIE_STEP1] = MDNIE_STEP1_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP2] = MDNIE_STEP2_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP3] = MDNIE_STEP3_INDEX;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_RED_OFFSET] = ADDRESS_SCR_WHITE_RED;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_GREEN_OFFSET] = ADDRESS_SCR_WHITE_GREEN;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_BLUE_OFFSET] = ADDRESS_SCR_WHITE_BLUE;
	mdnie_data->dsi_rgb_sensor_mdnie_1_size = DSI0_RGB_SENSOR_MDNIE_1_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_2_size = DSI0_RGB_SENSOR_MDNIE_2_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_3_size = 0;
	mdnie_data->dsi_trans_dimming_data_index = MDNIE_TRANS_DIMMING_DATA_INDEX;
	mdnie_data->dsi_adjust_ldu_table = adjust_ldu_data;
	mdnie_data->dsi_max_adjust_ldu = 6;
	mdnie_data->dsi_night_mode_table = night_mode_data;
	mdnie_data->dsi_max_night_mode_index = 12;
	mdnie_data->dsi_color_lens_table = color_lens_data;
	mdnie_data->dsi_white_default_r = 0xff;
	mdnie_data->dsi_white_default_g = 0xff;
	mdnie_data->dsi_white_default_b = 0xff;
	mdnie_data->dsi_white_balanced_r = 0;
	mdnie_data->dsi_white_balanced_g = 0;
	mdnie_data->dsi_white_balanced_b = 0;
	mdnie_data->dsi_scr_step_index = MDNIE_STEP1_INDEX;
	mdnie_data->light_notification_tune_value_dsi = light_notification_tune_value_dsi0;	
	mdnie_data->hdr_tune_value_dsi = hdr_tune_value_dsi0;

	vdd->mdnie.mdnie_data = mdnie_data;

	return 0;
}

static int ss_self_display_data_init(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR(vdd, "vdd is null or error\n");
		return -ENODEV;
	}

	if (!vdd->self_disp.is_support) {
		LCD_ERR(vdd, "Self Display is not supported\n");
		return -EINVAL;
	}

	LCD_INFO(vdd, "Self Display Panel Data init\n");

	/* SELF DISPLAY */
	if (unlikely(vdd->is_factory_mode)) {
		vdd->self_disp.operation[FLAG_SELF_MASK].img_buf = self_mask_img_data_factory;
		vdd->self_disp.operation[FLAG_SELF_MASK].img_size = ARRAY_SIZE(self_mask_img_data_factory);
	}
	else {
		vdd->self_disp.operation[FLAG_SELF_MASK].img_buf = self_mask_img_data;
		vdd->self_disp.operation[FLAG_SELF_MASK].img_size = ARRAY_SIZE(self_mask_img_data);
	}
	make_self_display_img_cmds_6W3(vdd, TX_SELF_MASK_IMAGE, FLAG_SELF_MASK);
	vdd->self_disp.operation[FLAG_SELF_MASK].img_checksum = SELF_MASK_IMG_CHECKSUM;

	vdd->self_disp.operation[FLAG_SELF_ICON].img_buf = self_icon_img_data;
	vdd->self_disp.operation[FLAG_SELF_ICON].img_size = ARRAY_SIZE(self_icon_img_data);
	make_self_display_img_cmds_6W3(vdd, TX_SELF_ICON_IMAGE, FLAG_SELF_ICON);

	vdd->self_disp.operation[FLAG_SELF_ACLK].img_buf = self_aclock_img_data;
	vdd->self_disp.operation[FLAG_SELF_ACLK].img_size = ARRAY_SIZE(self_aclock_img_data);
	make_self_display_img_cmds_6W3(vdd, TX_SELF_ACLOCK_IMAGE, FLAG_SELF_ACLK);

	vdd->self_disp.operation[FLAG_SELF_DCLK].img_buf = self_dclock_img_data;
	vdd->self_disp.operation[FLAG_SELF_DCLK].img_size = ARRAY_SIZE(self_dclock_img_data);
	make_self_display_img_cmds_6W3(vdd, TX_SELF_DCLOCK_IMAGE, FLAG_SELF_DCLK);


	return 1;
}

static void make_brightness_packet(struct samsung_display_driver_data *vdd,
	struct dsi_cmd_desc *packet, int *cmd_cnt, enum BR_TYPE br_type)
{
	if (br_type == BR_TYPE_NORMAL) {
		if (vdd->br_info.smart_dimming_loaded_dsi) { /* OCTA PANEL */
			/* hbm off */
			if (vdd->br_info.is_hbm)
				ss_add_brightness_packet(vdd, BR_FUNC_HBM_OFF, packet, cmd_cnt);

			/* aid/aor */
			ss_add_brightness_packet(vdd, BR_FUNC_AID, packet, cmd_cnt);

			/* acl */
			if (vdd->br_info.acl_status || vdd->siop_status) {
				ss_add_brightness_packet(vdd, BR_FUNC_ACL_ON, packet, cmd_cnt);
				ss_add_brightness_packet(vdd, BR_FUNC_ACL_PERCENT_PRE, packet, cmd_cnt);
				ss_add_brightness_packet(vdd, BR_FUNC_ACL_PERCENT, packet, cmd_cnt);
			} else {
				ss_add_brightness_packet(vdd, BR_FUNC_ACL_OFF, packet, cmd_cnt);
			}

			/* elvss */
			ss_add_brightness_packet(vdd, BR_FUNC_ELVSS_PRE, packet, cmd_cnt);
			ss_add_brightness_packet(vdd, BR_FUNC_ELVSS, packet, cmd_cnt);

			/* temperature elvss */
			ss_add_brightness_packet(vdd, BR_FUNC_ELVSS_TEMP1, packet, cmd_cnt);
			ss_add_brightness_packet(vdd, BR_FUNC_ELVSS_TEMP2, packet, cmd_cnt);

			/* caps*/
			ss_add_brightness_packet(vdd, BR_FUNC_CAPS_PRE, packet, cmd_cnt);
			ss_add_brightness_packet(vdd, BR_FUNC_CAPS, packet, cmd_cnt);

			/* Manual DBV (for DIA setting) */
			ss_add_brightness_packet(vdd, BR_FUNC_DBV, packet, cmd_cnt);

			/* vint */
			ss_add_brightness_packet(vdd, BR_FUNC_VINT, packet, cmd_cnt);

			/* IRC */
			ss_add_brightness_packet(vdd, BR_FUNC_IRC, packet, cmd_cnt);

			/* LTPS: used for normal and HBM brightness */
			ss_add_brightness_packet(vdd, BR_FUNC_LTPS, packet, cmd_cnt);

			/* PANEL SPECIFIC SETTINGS */
			ss_add_brightness_packet(vdd, BR_FUNC_ETC, packet, cmd_cnt);

			/* mAFPC */
			if (vdd->mafpc.is_support)
				ss_add_brightness_packet(vdd, BR_FUNC_MAFPC_SCALE, packet, cmd_cnt);

			/* gamma */
			ss_add_brightness_packet(vdd, BR_FUNC_GAMMA, packet, cmd_cnt);

			/* VRR */
			ss_add_brightness_packet(vdd, BR_FUNC_VRR, packet, cmd_cnt);
		} else { /* Gamma Mode2 or TFT PANEL */

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

		}
	} else if (br_type == BR_TYPE_HBM) {
		/* acl */
		if (vdd->br_info.acl_status || vdd->siop_status) {
			ss_add_brightness_packet(vdd, BR_FUNC_HBM_ACL_ON, packet, cmd_cnt);
		} else {
			ss_add_brightness_packet(vdd, BR_FUNC_HBM_ACL_OFF, packet, cmd_cnt);
		}

		/* IRC */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_IRC, packet, cmd_cnt);

		/* Gamma */
		ss_add_brightness_packet(vdd, BR_FUNC_HBM_GAMMA, packet, cmd_cnt);

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

void S6E36W3_AMB114EU09_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO(vdd, "%s\n", ss_get_panel_name(vdd));

	/* Default Panel Power Status is OFF */
	vdd->panel_state = PANEL_PWR_OFF;

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = samsung_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = samsung_panel_on_post;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = ss_panel_revision;
	vdd->panel_func.samsung_manufacture_date_read = ss_manufacture_date_read;
	vdd->panel_func.samsung_ddi_id_read = ss_ddi_id_read;
	vdd->panel_func.samsung_cell_id_read = ss_cell_id_read;
	vdd->panel_func.samsung_octa_id_read = ss_octa_id_read;
	vdd->panel_func.samsung_elvss_read = NULL;
	vdd->panel_func.samsung_hbm_read = NULL;
	vdd->panel_func.samsung_mdnie_read = ss_mdnie_read;
	vdd->panel_func.samsung_irc_read = NULL;

	/* Smart Dimming */
	vdd->panel_func.samsung_smart_dimming_init = ss_smart_dimming_init;
	vdd->panel_func.samsung_smart_get_conf = smart_get_conf_S6E36W3_AMB114EU09;

	/* GAMMA FLASH */
	vdd->panel_func.samsung_flash_gamma_support = flash_gamma_support_S6E36W3_AMB114EU09;
	vdd->panel_func.samsung_interpolation_init = init_interpolation_S6E36W3_AMB114EU09;

	/* Make brightness packer */
	vdd->panel_func.make_brightness_packet = make_brightness_packet;

	/* Brightness */
	vdd->panel_func.br_func[BR_FUNC_AID] = ss_aid;
	vdd->panel_func.br_func[BR_FUNC_ACL_ON] = ss_acl_on;
	vdd->panel_func.br_func[BR_FUNC_ACL_OFF] = ss_acl_off;
	vdd->panel_func.br_func[BR_FUNC_ELVSS] = ss_elvss;
	vdd->panel_func.br_func[BR_FUNC_DBV] = NULL;
	vdd->panel_func.br_func[BR_FUNC_VINT] = NULL;
	vdd->panel_func.br_func[BR_FUNC_IRC] = NULL;
	vdd->panel_func.br_func[BR_FUNC_GAMMA] = ss_gamma;
	vdd->panel_func.br_func[BR_FUNC_LTPS] = NULL;
	vdd->panel_func.br_func[BR_FUNC_VRR] = NULL;

	/* HBM */
	vdd->panel_func.br_func[BR_FUNC_HBM_GAMMA] = NULL;
	vdd->panel_func.br_func[BR_FUNC_HBM_ETC] = ss_hbm_etc;
	vdd->panel_func.br_func[BR_FUNC_HBM_IRC] = NULL;
	vdd->panel_func.br_func[BR_FUNC_HBM_VRR] = NULL;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_ON] = NULL;
	vdd->panel_func.br_func[BR_FUNC_HBM_ACL_OFF] = NULL;
	vdd->panel_func.br_func[BR_FUNC_HBM_VINT] = NULL;

	/* HMT */
	vdd->panel_func.br_func[BR_FUNC_HMT_GAMMA] = NULL;
	vdd->panel_func.br_func[BR_FUNC_HMT_AID] = NULL;
	vdd->panel_func.br_func[BR_FUNC_HMT_ELVSS] = NULL;
	vdd->panel_func.br_func[BR_FUNC_HMT_VRR] = NULL;

	/* Panel LPM */
	vdd->panel_func.samsung_update_lpm_ctrl_cmd = NULL;
	vdd->panel_func.samsung_set_lpm_brightness = NULL;

	/* default brightness */
	vdd->br_info.common_br.bl_level = 25500;

	/* mdnie */
	vdd->mdnie.support_mdnie = false;
	vdd->mdnie.support_trans_dimming = true;
	/* for mdnie tuning */
#if 0
	vdd->mdnie_tune_size1 = sizeof(DSI0_BYPASS_MDNIE_1);
	vdd->mdnie_tune_size2 = sizeof(DSI0_BYPASS_MDNIE_2);
	vdd->mdnie_tune_size3 = sizeof(DSI0_BYPASS_MDNIE_2);
#endif

	dsi_update_mdnie_data(vdd);

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = NULL;

	/* Enable panic on first pingpong timeout */
	if (vdd->debug_data)
		vdd->debug_data->panic_on_pptimeout = true;

	/* COLOR WEAKNESS */
	vdd->panel_func.color_weakness_ccb_on_off = NULL;

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = NULL;

	/* ACL default ON */
	vdd->br_info.acl_status = 1;

	/* Self display */
	vdd->self_disp.is_support = true;
	vdd->self_disp.init = self_display_init_6W3;
	vdd->self_disp.data_init = ss_self_display_data_init;
}
