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
Copyright (C) 2017, Samsung Electronics. All rights reserved.

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
#include "ss_dsi_panel_ANA38401_AMSA05RB01.h"
#include "ss_dsi_mdnie_ANA38401_AMSA05RB01.h"

static char hbm_buffer1[33] = {0,};

static int samsung_panel_on_pre(struct samsung_display_driver_data *vdd)
{
 	struct dsi_panel_cmd_set  *on_cmds = ss_get_cmds(vdd, DSI_CMD_SET_ON); /* mdss-dsi-on-command */
	static bool check_first = true;

	LCD_INFO("++\n");
 	if (check_first == true) {
		LCD_INFO("Generate_gamma for max Candela\n");

		/* BL Gamma OFFSET is Default, Kernel Gamma OFFSET is 4, so first On_PRE makes abnormal Color. */
		/* ANA38401_AMSA05RB01 must use Gamma Value after SmartDimming instead of mdss-dsi-on-command's Default Brigtness setting. */
		vdd->smart_dimming_dsi->generate_gamma(
			vdd->smart_dimming_dsi,
			360,	/* MAX_CANDELA */
			&on_cmds->cmds[4].msg.tx_buf[1]);
		check_first = false;
	}
	ss_panel_attach_set(vdd, true);
	LCD_INFO("--\n");

	return true;
}

static int samsung_panel_on_post(struct samsung_display_driver_data *vdd)
{
	return true;
}

static char ss_panel_revision(struct samsung_display_driver_data *vdd)
{
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
		LCD_ERR("Invalid panel_rev(default rev : %c)\n",
				vdd->panel_revision);
		break;
	}

	vdd->panel_revision -= 'A';

	LCD_INFO_ONCE("panel_revision = %c %d \n",
		vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

static int ss_manufacture_date_read(struct samsung_display_driver_data *vdd)
{
	unsigned char date[8] = {0,};
	int year, month, day;
	int hour, min;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (D8h 134 ~ 140th Para) for manufacture date */
	if (ss_get_cmds(vdd, RX_MANUFACTURE_DATE)->count) {
		ss_panel_data_read(vdd, RX_MANUFACTURE_DATE, date, LEVEL_KEY_NONE);

		year = date[0] & 0xf0;
		year >>= 4;
		year += 2011; // 0 = 2011 year
		month = date[0] & 0x0f;
		day = date[1] & 0x1f;
		hour = date[2]& 0x0f;
		min = date[3] & 0x1f;

		vdd->manufacture_date_dsi = year * 10000 + month * 100 + day;
		vdd->manufacture_time_dsi = hour * 100 + min;

		LCD_ERR("manufacture_date DSI%d = (%d%04d) - year(%d) month(%d) day(%d) hour(%d) min(%d)\n",
			vdd->ndx, vdd->manufacture_date_dsi, vdd->manufacture_time_dsi,
			year, month, day, hour, min);
	} else {
		LCD_ERR("DSI%d no manufacture_date_rx_cmds cmds(%d)", vdd->ndx, vdd->panel_revision);
		return false;
	}

	return true;
}

static int ss_ddi_id_read(struct samsung_display_driver_data *vdd)
{
	char ddi_id[5];
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx\n", (size_t)vdd);
		return false;
	}

	/* Read mtp (D6h 1~5th) for ddi id */
	if (ss_get_cmds(vdd, RX_DDI_ID)->count) {
		ss_panel_data_read(vdd, RX_DDI_ID, ddi_id, LEVEL_KEY_NONE);

		for(loop = 0; loop < 5; loop++)
			vdd->ddi_id_dsi[loop] = ddi_id[loop];

		LCD_INFO("DSI%d : %02x %02x %02x %02x %02x\n", vdd->ndx,
			vdd->ddi_id_dsi[0], vdd->ddi_id_dsi[1],
			vdd->ddi_id_dsi[2], vdd->ddi_id_dsi[3],
			vdd->ddi_id_dsi[4]);
	} else {
		LCD_ERR("DSI%d no ddi_id_rx_cmds cmds\n", vdd->ndx);
		return false;
	}

	return true;
}

static int ss_cell_id_read(struct samsung_display_driver_data *vdd)
{
	char cell_id_buffer[MAX_CELL_ID] = {0,};
	int loop;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read Panel Unique Cell ID (0xD8 : 134th para ~ 140th para) */
	if (ss_get_cmds(vdd, RX_CELL_ID)->count) {
		memset(cell_id_buffer, 0x00, MAX_CELL_ID);

		ss_panel_data_read(vdd, RX_CELL_ID, cell_id_buffer, LEVEL_KEY_NONE);

		for(loop = 0; loop < MAX_CELL_ID; loop++)
			vdd->cell_id_dsi[loop] = cell_id_buffer[loop];

		LCD_INFO("DSI%d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			vdd->ndx, vdd->cell_id_dsi[0],
			vdd->cell_id_dsi[1],	vdd->cell_id_dsi[2],
			vdd->cell_id_dsi[3],	vdd->cell_id_dsi[4],
			vdd->cell_id_dsi[5],	vdd->cell_id_dsi[6],
			vdd->cell_id_dsi[7],	vdd->cell_id_dsi[8],
			vdd->cell_id_dsi[9],	vdd->cell_id_dsi[10]);
	} else {
		LCD_ERR("DSI%d no cell_id_rx_cmds cmd\n", vdd->ndx);
		return false;
	}

	return true;
}

#if 0  // From : ss_dsi_panel_S6E3FA7_AMS628RF01.c
static int ss_octa_id_read(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read Panel Unique OCTA ID (C9h 2nd~21th) */
	if (ss_get_cmds(vdd, RX_OCTA_ID)->count) {
		memset(vdd->octa_id_dsi, 0x00, MAX_OCTA_ID);

		ss_panel_data_read(vdd, RX_OCTA_ID,
				vdd->octa_id_dsi, LEVEL1_KEY);

		LCD_INFO("octa id: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
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
		LCD_ERR("DSI%d no octa_id_rx_cmds cmd\n", vdd->ndx);
		return false;
	}

	return true;
}

static int ss_elvss_read(struct samsung_display_driver_data *vdd)
{
	char elvss_b5[2];

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (B5h 23th,24th) for elvss*/
	ss_panel_data_read(vdd, RX_ELVSS, elvss_b5, LEVEL1_KEY);

	vdd->display_status_dsi.elvss_value1 = elvss_b5[0]; /*0xB5 23th OTP value*/
	vdd->display_status_dsi.elvss_value2 = elvss_b5[1]; /*0xB5 24th */

	return true;
}

static struct dsi_panel_cmd_set *ss_vint(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *vint_cmds = ss_get_cmds(vdd, TX_VINT);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vint_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)vint_cmds);
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
#endif

static int ss_hbm_read(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *hbm_rx_cmds = ss_get_cmds(vdd, RX_HBM);
	struct dsi_panel_cmd_set *hbm_etc_cmds = ss_get_cmds(vdd, TX_HBM_ETC);
	struct dsi_panel_cmd_set *hbm_off_cmds = ss_get_cmds(vdd, TX_HBM_OFF);

	char hbm_buffer2[1]    = {0,};
	char hbm_off_buffer[1] = {0,};
	char log_buf[256]      = {0,};
	u32 i;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_rx_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)hbm_rx_cmds);
		return false;
	}

	/* Read mtp (D4h 91~123th) for hbm gamma */
	ss_panel_data_read(vdd, RX_HBM, hbm_buffer1, LEVEL_KEY_NONE);

	hbm_rx_cmds->read_startoffset = 0x63; // 99
	ss_panel_data_read(vdd, RX_HBM, hbm_buffer1 + 8, LEVEL_KEY_NONE);

	hbm_rx_cmds->read_startoffset =  0x6B; // 107
	ss_panel_data_read(vdd, RX_HBM, hbm_buffer1 + 16, LEVEL_KEY_NONE);

	hbm_rx_cmds->read_startoffset =  0x73; // 115
	ss_panel_data_read(vdd, RX_HBM, hbm_buffer1 + 24, LEVEL_KEY_NONE);

	hbm_rx_cmds->read_startoffset =  0x7B; // 123
	hbm_rx_cmds->cmds[0].msg.rx_len = 1;
	hbm_rx_cmds->cmds[0].msg.tx_buf[1] = 1;
	ss_panel_data_read(vdd, RX_HBM, hbm_buffer1 + 32, LEVEL_KEY_NONE);

	memset(log_buf, 0x00, 256);
	for(i = 0; i < 33; i++)
		snprintf(log_buf + strnlen(log_buf, 256), 256, " %02x", hbm_buffer1[i]);
	LCD_INFO("MTP data for HBM Read From D4 =%s\n", log_buf);

	/* Read mtp (D8h 129th) for HBM On elvss*/
	ss_panel_data_read(vdd, RX_HBM2, hbm_buffer2, LEVEL_KEY_NONE);
	memcpy(&hbm_etc_cmds->cmds[2].msg.tx_buf[1], hbm_buffer2, 1);

	/* Read mtp (D2h 112th) for HBM Off elvss*/
	ss_panel_data_read(vdd, RX_ELVSS, hbm_off_buffer, LEVEL_KEY_NONE);
	memcpy(&hbm_off_cmds->cmds[1].msg.tx_buf[1], hbm_off_buffer, 1);

	return true;
}

static struct dsi_panel_cmd_set *ss_hbm_gamma(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *hbm_gamma_cmds = ss_get_cmds(vdd, TX_HBM_GAMMA);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_gamma_cmds)) {
		LCD_ERR("Invalid data  vdd : 0x%zx cmd : 0x%zx", (size_t)vdd, (size_t)hbm_gamma_cmds);
		return NULL;
	}

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi->generate_hbm_gamma)) {
		LCD_ERR("generate_hbm_gamma is NULL error");
		return NULL;
	} else {
		vdd->smart_dimming_dsi->generate_hbm_gamma(
			vdd->smart_dimming_dsi,
			vdd->br.auto_level,
			&hbm_gamma_cmds->cmds[0].msg.tx_buf[1]);

		*level_key = LEVEL_KEY_NONE;
		return hbm_gamma_cmds;
	}
}

static struct dsi_panel_cmd_set *ss_hbm_etc(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *hbm_etc_cmds = ss_get_cmds(vdd, TX_HBM_ETC);
	int elvss_dim_off;
	int elvss_comp;
	int acl_opr;
	int acl_start;
	int acl_percent;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_etc_cmds)) {
		LCD_ERR("Invalid data  vdd : 0x%zx cmd : 0x%zx", (size_t)vdd, (size_t)hbm_etc_cmds);
		return NULL;
	}

	*level_key = LEVEL_KEY_NONE;

	/* HBM ELVSS */
	/*
	 * To Do : In the Rev A OP manual there are no elvss dim offset
	 * settings for -20 < T <= 0 and T <= -20 . In the future when new OP manual
	 * is released with these settings then ,
	 * this code needs to be updated for -20 < T <= 0  and T <= -20  cases
	 */

	switch(vdd->br.auto_level) {
	case 6: /*378*/
		if (vdd->temperature > 0)		elvss_dim_off = 0x1A;
		else if (vdd->temperature > -20)	elvss_dim_off = 0x0E;
		else					elvss_dim_off = 0x0C;
		break;
	case 7:	/*395*/
		if (vdd->temperature > 0)		elvss_dim_off = 0x18;
		else if (vdd->temperature > -20)	elvss_dim_off = 0x0D;
		else					elvss_dim_off = 0x0C;
		break;
	case 8:	/*413*/
		if (vdd->temperature > 0)		elvss_dim_off = 0x16;
		else if (vdd->temperature > -20)	elvss_dim_off = 0x0C;
		else					elvss_dim_off = 0x0C;
		break;
	case 9:	/*430*/
		if (vdd->temperature > 0)		elvss_dim_off = 0x15;
		else if (vdd->temperature > -20)	elvss_dim_off = 0x0C;
		else					elvss_dim_off = 0x0C;
		break;
	case 10: /*448*/
		if (vdd->temperature > 0)		elvss_dim_off = 0x13;
		else if (vdd->temperature > -20)	elvss_dim_off = 0x0C;
		else					elvss_dim_off = 0x0C;
		break;
	case 11: /*465*/
		if (vdd->temperature > 0)		elvss_dim_off = 0x12;
		else if (vdd->temperature > -20)	elvss_dim_off = 0x0C;
		else					elvss_dim_off = 0x0C;
		break;
	case 12: /*483*/
		if (vdd->temperature > 0)		elvss_dim_off = 0x10;
		else if (vdd->temperature > -20)	elvss_dim_off = 0x0C;
		else					elvss_dim_off = 0x0C;
		break;
	case 13: /*500, HBM*/
		if (vdd->temperature > 0)		elvss_dim_off = 0x0F;
		else if (vdd->temperature > -20)	elvss_dim_off = 0x0C;
		else					elvss_dim_off = 0x0C;
		break;

	default:
		LCD_INFO("err: auto_brightness=%d\n", vdd->br.auto_level);
		elvss_dim_off = 0x0F;
		break;
	}

	hbm_etc_cmds->cmds[4].msg.tx_buf[1] = elvss_dim_off;

	/* ACL */
	if (!vdd->gradual_acl_val) {	/* gallery app */
		acl_opr = 0x4;		/* 16 Frame Avg at ACL off */
		acl_start = 0x99;	/* Start setting: 60% start */
		acl_percent = 0x10;	/* ACL off */
	}
	else {				/* not gallery app */
		acl_opr = 0x5;		/* 32 Frame Avg at ACL on */
		acl_start = 0x99;	/* Start setting: 60% start */
		acl_percent = 0x11;	/* ACL 8% on */
	}

	hbm_etc_cmds->cmds[6].msg.tx_buf[1] = acl_opr;
	hbm_etc_cmds->cmds[8].msg.tx_buf[1] = acl_start;
	hbm_etc_cmds->cmds[10].msg.tx_buf[1] = acl_percent;

	if (vdd->dtsi_data.samsung_elvss_compensation) {
		elvss_comp = hbm_etc_cmds->cmds[2].msg.tx_buf[1] + hbm_etc_cmds->cmds[4].msg.tx_buf[1];
		
		hbm_etc_cmds->cmds[2].msg.tx_buf[1] = 0x00;       /* B2h 0x70th */
		hbm_etc_cmds->cmds[4].msg.tx_buf[1] = elvss_comp; /* B2h 0x67th */
	}

	LCD_INFO("bl:%d can:%d elv:%x temp:%d opr:%x start:%x acl:%x\n",
			vdd->br.bl_level, vdd->br.cd_level,
			elvss_dim_off, vdd->temperature, acl_opr, acl_start,
			acl_percent);

	return hbm_etc_cmds;
}

static struct dsi_panel_cmd_set *ss_hbm_off(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *hbm_off_cmds = ss_get_cmds(vdd, TX_HBM_OFF);
	
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	if (vdd->dtsi_data.samsung_elvss_compensation) {
		hbm_off_cmds->cmds[1].msg.tx_buf[1] = 0x00;
	}

	*level_key = LEVEL_KEY_NONE;

	return ss_get_cmds(vdd, TX_HBM_OFF);
}


#define COORDINATE_DATA_SIZE 6
#define MDNIE_SCR_WR_ADDR 50

#define F1(x,y) ((y)-((353*(x))/326)+30)
#define F2(x,y) ((y)-((20*(x))/19)-14)
#define F3(x,y) ((y)+((185*(x))/42)-16412)
#define F4(x,y) ((y)+((337*(x))/106)-12601)

/* Normal Mode */
static char coordinate_data_1[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xfb, 0x00, 0xfb, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfc, 0x00, 0xff, 0x00}, /* Tune_2 */
	{0xfb, 0x00, 0xf9, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfe, 0x00, 0xfc, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xfb, 0x00, 0xfc, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfd, 0x00, 0xff, 0x00, 0xfa, 0x00}, /* Tune_7 */
	{0xfc, 0x00, 0xff, 0x00, 0xfc, 0x00}, /* Tune_8 */
	{0xfb, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_9 */
};

/* sRGB/Adobe RGB Mode */
static char coordinate_data_2[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xf7, 0x00, 0xef, 0x00}, /* dummy */
	{0xff, 0x00, 0xf4, 0x00, 0xec, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xf5, 0x00, 0xef, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xf6, 0x00, 0xf2, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xf6, 0x00, 0xed, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xf7, 0x00, 0xef, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xf8, 0x00, 0xf2, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xf9, 0x00, 0xed, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xf9, 0x00, 0xef, 0x00}, /* Tune_8 */
	{0xff, 0x00, 0xfa, 0x00, 0xf2, 0x00}, /* Tune_9 */
};

static char (*coordinate_data_multi[MAX_MODE])[COORDINATE_DATA_SIZE] = {
	coordinate_data_2, /* DYNAMIC - Normal */
	coordinate_data_2, /* STANDARD - sRGB/Adobe RGB */
	coordinate_data_2, /* NATURAL - sRGB/Adobe RGB */
	coordinate_data_1, /* MOVIE - Normal */
	coordinate_data_1, /* AUTO - Normal */
	coordinate_data_1, /* READING - Normal */
};

static int mdnie_coordinate_index(int x, int y)
{
	int tune_number = 0;

	if (F1(x,y) > 0) {
		if (F3(x,y) > 0) {
			tune_number = 3;
		} else {
			if (F4(x,y) < 0)
				tune_number = 1;
			else
				tune_number = 2;
		}
	} else {
		if (F2(x,y) < 0) {
			if (F3(x,y) > 0) {
				tune_number = 9;
			} else {
				if (F4(x,y) < 0)
					tune_number = 7;
				else
					tune_number = 8;
			}
		} else {
			if (F3(x,y) > 0)
				tune_number = 6;
			else {
				if (F4(x,y) < 0)
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
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	/* Read mtp (D8h 123~126th) for ddi id */
	if (ss_get_cmds(vdd, RX_MDNIE)->count) {
		ss_panel_data_read(vdd, RX_MDNIE, x_y_location, LEVEL_KEY_NONE);

		vdd->mdnie.mdnie_x = x_y_location[0] << 8 | x_y_location[1];	/* X */
		vdd->mdnie.mdnie_y = x_y_location[2] << 8 | x_y_location[3];	/* Y */

		mdnie_tune_index = mdnie_coordinate_index(vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);

		coordinate_tunning_multi(vdd, coordinate_data_multi, mdnie_tune_index,
				MDNIE_SCR_WR_ADDR, COORDINATE_DATA_SIZE);  // JUN_TEMP : From TAB_S4 : MDNIE_SCR_WR_ADDR

		LCD_INFO("DSI%d : X-%d Y-%d \n", vdd->ndx,
			vdd->mdnie.mdnie_x, vdd->mdnie.mdnie_y);
	} else {
		LCD_ERR("DSI%d no mdnie_read_rx_cmds cmds", vdd->ndx);
		return false;
	}

	return true;
}

static int ss_smart_dimming_init(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *pcmds;
	struct dsi_panel_cmd_set *hbm_gamma_cmds = ss_get_cmds(vdd, TX_HBM_GAMMA);
	struct dsi_panel_cmd_set *rx_smart_dim_mtp_cmds = ss_get_cmds(vdd, RX_SMART_DIM_MTP);

	LCD_INFO("++\n");
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	vdd->smart_dimming_dsi = vdd->panel_func.samsung_smart_get_conf();
	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi)) {
		LCD_ERR("DSI%d smart_dimming_dsi is null", vdd->ndx);
		return false;
	}

	ss_panel_data_read(vdd, RX_SMART_DIM_MTP, vdd->smart_dimming_dsi->mtp_buffer, LEVEL_KEY_NONE);

	rx_smart_dim_mtp_cmds->read_startoffset = 0x62;
	ss_panel_data_read(vdd, RX_SMART_DIM_MTP, vdd->smart_dimming_dsi->mtp_buffer + 8, LEVEL_KEY_NONE);

	rx_smart_dim_mtp_cmds->read_startoffset = 0x6A;
	ss_panel_data_read(vdd, RX_SMART_DIM_MTP, vdd->smart_dimming_dsi->mtp_buffer + 16, LEVEL_KEY_NONE);

	rx_smart_dim_mtp_cmds->read_startoffset = 0x72;
	ss_panel_data_read(vdd, RX_SMART_DIM_MTP, vdd->smart_dimming_dsi->mtp_buffer + 24, LEVEL_KEY_NONE);

	rx_smart_dim_mtp_cmds->read_startoffset = 0x7A;
	rx_smart_dim_mtp_cmds->cmds[0].msg.rx_len = 1;
	rx_smart_dim_mtp_cmds->cmds[0].msg.tx_buf[1] = 1;
	ss_panel_data_read(vdd, RX_SMART_DIM_MTP, vdd->smart_dimming_dsi->mtp_buffer + 32, LEVEL_KEY_NONE);

	/* Modifying hbm gamma tx command for Gamma Offset Index 4 */
	memcpy(&hbm_gamma_cmds->cmds[0].msg.tx_buf[1], hbm_buffer1, 33);

	/* Initialize smart dimming related things here */
	/* lux_tab setting for 350cd */
	vdd->smart_dimming_dsi->lux_tab = vdd->dtsi_data.candela_map_table[NORMAL][vdd->panel_revision].cd;
	vdd->smart_dimming_dsi->lux_tabsize = vdd->dtsi_data.candela_map_table[NORMAL][vdd->panel_revision].tab_size;
	vdd->smart_dimming_dsi->man_id = vdd->manufacture_id_dsi;

	/* copy hbm gamma payload for hbm interpolation calc */
	pcmds = ss_get_cmds(vdd, TX_HBM_GAMMA);
	vdd->smart_dimming_dsi->hbm_payload = &pcmds->cmds[0].msg.tx_buf[1];

	/* Just a safety check to ensure smart dimming data is initialised well */
	vdd->smart_dimming_dsi->init(vdd->smart_dimming_dsi);

	vdd->temperature = 20; // default temperature

	vdd->smart_dimming_loaded_dsi = true;

	LCD_INFO("DSI%d : --\n", vdd->ndx);
	LCD_INFO("--\n");

	return true;
}

static struct dsi_panel_cmd_set aid_cmd;
static struct dsi_panel_cmd_set *ss_aid(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds = ss_get_cmds(vdd, TX_AID_SUBDIVISION);
	int cd_index = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	cd_index = vdd->br.bl_level;

	aid_cmd.count = 1;
	aid_cmd.cmds = &pcmds->cmds[cd_index];
	LCD_DEBUG("[%d] level(%d), aid(%x %x)\n",
			cd_index, vdd->br.bl_level,
			aid_cmd.cmds->msg.tx_buf[1],
			aid_cmd.cmds->msg.tx_buf[2]);
	*level_key = LEVEL_KEY_NONE;

	return &aid_cmd;
}

static struct dsi_panel_cmd_set *ss_acl_on(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *pcmds;

	LCD_INFO("++\n");
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL_KEY_NONE;

	if (vdd->gradual_acl_val) {
		pcmds = ss_get_cmds(vdd, TX_ACL_ON);
		pcmds->cmds[5].msg.tx_buf[1] = vdd->gradual_acl_val;
	}

	LCD_INFO("--\n");
	return ss_get_cmds(vdd, TX_ACL_ON);

}

static struct dsi_panel_cmd_set *ss_acl_off(struct samsung_display_driver_data *vdd, int *level_key)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL_KEY_NONE;

	return ss_get_cmds(vdd, TX_ACL_OFF);
}

static int ss_elvss_read(struct samsung_display_driver_data *vdd)
{
	char elvss_read_CAL_OFFSET[1] = {0,};

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	if (!vdd->dtsi_data.samsung_elvss_compensation) {
		LCD_INFO("Not Supported \n");
		return false;
	}
	
	/* Read CAL_OFFSET[0x70] & Save to elvss_value2 */
	ss_panel_data_read(vdd, RX_ELVSS, elvss_read_CAL_OFFSET, LEVEL_KEY_NONE);
	vdd->br.elvss_value2 = elvss_read_CAL_OFFSET[0];

	return true;
}

static struct dsi_panel_cmd_set *ss_pre_elvss(struct samsung_display_driver_data *vdd, int *level_key)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL_KEY_NONE;
	return ss_get_cmds(vdd, TX_ELVSS_PRE);
}

static struct dsi_panel_cmd_set elvss_cmd;
static struct dsi_panel_cmd_set *ss_elvss(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *elvss_cmds;
	int cd_index = 0;
	int cmd_idx = 0;
	int candela_value = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return NULL;
	}

	cd_index = vdd->br.cd_idx;
	candela_value = vdd->br.cd_level;
	//LCD_INFO("cd_index (%d) candela_value(%d)\n", cd_index, candela_value);

	// JUN_TEMP : To do
	if (vdd->acl_status || vdd->siop_status) {
		if (!vdd->dtsi_data.smart_acl_elvss_map_table[vdd->panel_revision].size ||
			cd_index > vdd->dtsi_data.smart_acl_elvss_map_table[vdd->panel_revision].size)
			goto end;

		cmd_idx = vdd->dtsi_data.smart_acl_elvss_map_table[vdd->panel_revision].cmd_idx[cd_index];
		//LCD_INFO("ACL ELVSS : cd_index[%d] cmd_idx[%d]\n", cd_index, cmd_idx);

		if (vdd->temperature > 0)
			elvss_cmds = ss_get_cmds(vdd, TX_SMART_ACL_ELVSS);
		else if (vdd->temperature > -20)
			elvss_cmds = ss_get_cmds(vdd, TX_SMART_ACL_ELVSS_LOWTEMP);
		else
			elvss_cmds = ss_get_cmds(vdd, TX_SMART_ACL_ELVSS_LOWTEMP2);

		if (IS_ERR_OR_NULL(elvss_cmds)) {
			LCD_ERR("Invalid data cmds : 0x%zx \n", (size_t)elvss_cmds);
			return NULL;
		}
	}
	else {
		if (!vdd->dtsi_data.elvss_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data.elvss_map_table[vdd->panel_revision].size)
		goto end;

		cmd_idx = vdd->dtsi_data.elvss_map_table[vdd->panel_revision].cmd_idx[cd_index];
		//LCD_INFO("ELVSS : cd_index[%d] cmd_idx[%d]\n", cd_index, cmd_idx);

		if (vdd->temperature > 0)
			elvss_cmds = ss_get_cmds(vdd, TX_ELVSS);
		else if (vdd->temperature > -20)
			elvss_cmds = ss_get_cmds(vdd, TX_ELVSS_LOWTEMP);
		else
			elvss_cmds = ss_get_cmds(vdd, TX_ELVSS_LOWTEMP2);

		if (IS_ERR_OR_NULL(elvss_cmds)) {
			LCD_ERR("Invalid data cmds : 0x%zx \n", (size_t)elvss_cmds);
			return NULL;
		}
	}

	elvss_cmd.cmds = &(elvss_cmds->cmds[cmd_idx]);
	elvss_cmd.count = 1;

	if (vdd->dtsi_data.samsung_elvss_compensation) {
		/* Save elvss_value1 to send elvss2 cmd */
		vdd->br.elvss_value1 = elvss_cmd.cmds[0].msg.tx_buf[1]; // 0x67 Value Save 
	}
	
	*level_key = LEVEL_KEY_NONE;

	return &elvss_cmd;
end :
	LCD_ERR("error");
	return NULL;
}

static struct dsi_panel_cmd_set *ss_elvss_2(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *elvss_2_cmds = ss_get_cmds(vdd, TX_ELVSS_2);
	int elvss_compensation = 0;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(elvss_2_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)elvss_2_cmds);
		return NULL;
	}

	if (!vdd->dtsi_data.samsung_elvss_compensation) {
		LCD_INFO("Not Supported !\n");
		return NULL;
	}

	elvss_compensation = vdd->br.elvss_value1 + vdd->br.elvss_value2;
	elvss_2_cmds->cmds[1].msg.tx_buf[1] = elvss_compensation;
	LCD_INFO("elvss_compensation [0x%x]+[0x%x]=[0x%x]\n", vdd->br.elvss_value1, vdd->br.elvss_value2, elvss_compensation);

	*level_key = LEVEL_KEY_NONE;
	return elvss_2_cmds;
}

static struct dsi_panel_cmd_set *ss_gamma(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set  *gamma_cmds = ss_get_cmds(vdd, TX_GAMMA);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(gamma_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)gamma_cmds);
		return NULL;
	}

	LCD_INFO("bl_level : %d candela : %dCD\n", vdd->br.bl_level, vdd->br.cd_level);

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi->generate_gamma)) {
		LCD_ERR("generate_gamma is NULL error");
		return NULL;
	} else {
		vdd->smart_dimming_dsi->generate_gamma(
			vdd->smart_dimming_dsi,
			vdd->br.cd_level,
			&gamma_cmds->cmds[0].msg.tx_buf[1]);

		*level_key = LEVEL_KEY_NONE;

		return gamma_cmds;
	}
}

#if 0
/* IRC */
#define NUM_IRC_OTP	2
static int ss_irc_read(struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	if (!vdd->display_status_dsi.irc_otp) {
		vdd->display_status_dsi.irc_otp	= kzalloc(NUM_IRC_OTP, GFP_KERNEL);
		if (!vdd->display_status_dsi.irc_otp) {
			LCD_ERR("fail to allocate irc_otp memory\n");
			return false;
		}
	}

	/* Read mtp (B5h 23th,24th) for elvss*/
	ss_panel_data_read(vdd, RX_IRC, vdd->display_status_dsi.irc_otp,
							LEVEL1_KEY);


	return true;
}

static struct dsi_panel_cmd_set irc_cmd;
static struct dsi_panel_cmd_set *ss_irc(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *irc_cmds = ss_get_cmds(vdd, TX_IRC_SUBDIVISION);
	int cd_index = 0;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(irc_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx",
				(size_t)vdd, (size_t)irc_cmds);
		return NULL;
	}

	if (IS_ERR_OR_NULL(irc_cmds->cmds)) {
		LCD_ERR("No irc_subdivision_tx_cmds\n");
		return NULL;
	}

	if (!vdd->samsung_support_irc)
		return NULL;

	/* IRC Subdivision works like as AID Subdivision */
	if (vdd->pac)
		cd_index = vdd->pac_cd_idx;
	else
		cd_index = vdd->br.bl_level;

	LCD_DEBUG("irc idx (%d)\n", cd_index);

	irc_cmd.cmds = &(irc_cmds->cmds[cd_index]);
	irc_cmd.count = 1;
	*level_key = LEVEL1_KEY;

	/* read B8 1st,2nd from MTP and write to B8 1st,2nd */
	irc_cmd.cmds->msg.tx_buf[1] = vdd->display_status_dsi.irc_otp[0];
	irc_cmd.cmds->msg.tx_buf[2] = vdd->display_status_dsi.irc_otp[1];

	return &irc_cmd;
}

static struct dsi_panel_cmd_set hbm_irc_cmd;
static struct dsi_panel_cmd_set *ss_hbm_irc(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *hbm_irc_cmds = ss_get_cmds(vdd, TX_HBM_IRC);
	int para_idx = 0;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_irc_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)hbm_irc_cmds);
		return NULL;
	}

	if (IS_ERR_OR_NULL(hbm_irc_cmds->cmds)) {
		LCD_ERR("No irc_tx_cmds\n");
		return NULL;
	}

	if (!vdd->samsung_support_irc)
		return NULL;

	*level_key = LEVEL1_KEY;

	para_idx = vdd->auto_brightness_level - vdd->auto_brightness;

	hbm_irc_cmd.cmds = &(hbm_irc_cmds->cmds[para_idx]);
	hbm_irc_cmd.count = 1;

	/* read B8 1st,2nd from MTP and write to B8 1st,2nd */
	hbm_irc_cmd.cmds->msg.tx_buf[1] = vdd->display_status_dsi.irc_otp[0];
	hbm_irc_cmd.cmds->msg.tx_buf[2] = vdd->display_status_dsi.irc_otp[1];

	return &hbm_irc_cmd;
}

// ========================
//			HMT
// ========================
static struct dsi_panel_cmd_set *ss_gamma_hmt(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set  *hmt_gamma_cmds = ss_get_cmds(vdd, TX_HMT_GAMMA);

	if (IS_ERR_OR_NULL(hmt_gamma_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)hmt_gamma_cmds);
		return NULL;
	}

	LCD_DEBUG("hmt_bl_level : %d candela : %dCD\n", vdd->hmt_stat.hmt_bl_level, vdd->hmt_stat.candela_level_hmt);

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi_hmt->generate_gamma)) {
		LCD_ERR("generate_gamma is NULL");
		return NULL;
	} else {
		vdd->smart_dimming_dsi_hmt->generate_gamma(
			vdd->smart_dimming_dsi_hmt,
			vdd->hmt_stat.candela_level_hmt,
			&hmt_gamma_cmds->cmds[0].msg.tx_buf[1]);

		*level_key = LEVEL1_KEY;

		return hmt_gamma_cmds;
	}
}

static struct dsi_panel_cmd_set hmt_aid_cmd;
static struct dsi_panel_cmd_set *ss_aid_hmt(
		struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set  *hmt_aid_cmds = ss_get_cmds(vdd, TX_HMT_AID);
	int cmd_idx = 0;

	if (!vdd->dtsi_data.hmt_reverse_aid_map_table[vdd->panel_revision].size ||
		vdd->hmt_stat.cmd_idx_hmt > vdd->dtsi_data.hmt_reverse_aid_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data.hmt_reverse_aid_map_table[vdd->panel_revision].cmd_idx[vdd->hmt_stat.cmd_idx_hmt];

	hmt_aid_cmd.cmds = &hmt_aid_cmds->cmds[cmd_idx];
	hmt_aid_cmd.count = 1;

	*level_key = LEVEL1_KEY;

	return &hmt_aid_cmd;

end:
	LCD_ERR("error");
	return NULL;
}

static struct dsi_panel_cmd_set *ss_elvss_hmt(struct samsung_display_driver_data *vdd, int *level_key)
{
	struct dsi_panel_cmd_set *elvss_cmds = ss_get_cmds(vdd, TX_HMT_ELVSS);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(elvss_cmds)) {
		LCD_ERR("Invalid data vdd : 0x%zx cmds : 0x%zx", (size_t)vdd, (size_t)elvss_cmds);
		return NULL;
	}

	/* 0xB5 1th TSET */
	elvss_cmds->cmds[0].msg.tx_buf[1] =
		vdd->temperature > 0 ? vdd->temperature : 0x80|(-1*vdd->temperature);

	/* ELVSS(MPS_CON) setting condition is equal to normal birghtness */ // B5 2nd para : MPS_CON
	if (vdd->hmt_stat.candela_level_hmt > 40)
		elvss_cmds->cmds[0].msg.tx_buf[2] = 0xDC;
	else
		elvss_cmds->cmds[0].msg.tx_buf[2] = 0xCC;

	*level_key = LEVEL1_KEY;

	return elvss_cmds;
}

static void ss_make_sdimconf_hmt(struct samsung_display_driver_data *vdd)
{
	/* Set the mtp read buffer pointer and read the NVM value*/
	ss_panel_data_read(vdd, RX_SMART_DIM_MTP,
			vdd->smart_dimming_dsi_hmt->mtp_buffer, LEVEL1_KEY);

	/* Initialize smart dimming related things here */
	/* lux_tab setting for 350cd */
	vdd->smart_dimming_dsi_hmt->lux_tab = vdd->dtsi_data.hmt_candela_map_table[vdd->panel_revision].cd;
	vdd->smart_dimming_dsi_hmt->lux_tabsize = vdd->dtsi_data.hmt_candela_map_table[vdd->panel_revision].tab_size;
	vdd->smart_dimming_dsi_hmt->man_id = vdd->manufacture_id_dsi;
	if (vdd->panel_func.samsung_panel_revision)
			vdd->smart_dimming_dsi_hmt->panel_revision = vdd->panel_func.samsung_panel_revision(vdd);

	/* Just a safety check to ensure smart dimming data is initialised well */
	vdd->smart_dimming_dsi_hmt->init(vdd->smart_dimming_dsi_hmt);

	LCD_INFO("[HMT] smart dimming done!\n");
}

static int ss_samart_dimming_init_hmt(struct samsung_display_driver_data *vdd)
{
	LCD_INFO("DSI%d : ++\n", vdd->ndx);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data vdd : 0x%zx", (size_t)vdd);
		return false;
	}

	vdd->smart_dimming_dsi_hmt = vdd->panel_func.samsung_smart_get_conf_hmt();

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi_hmt)) {
		LCD_ERR("DSI%d error", vdd->ndx);
		return false;
	} else {
		vdd->hmt_stat.hmt_on = 0;
		vdd->hmt_stat.hmt_bl_level = 0;
		vdd->hmt_stat.hmt_reverse = 0;
		vdd->hmt_stat.hmt_is_first = 1;

		ss_make_sdimconf_hmt(vdd);

		vdd->smart_dimming_hmt_loaded_dsi = true;
	}

	LCD_INFO("DSI%d : --\n", vdd->ndx);

	return true;
}

static void ss_set_panel_lpm_brightness(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *alpm_brightness[LPM_BRIGHTNESS_MAX_IDX] = {NULL, };
	struct dsi_panel_cmd_set *alpm_ctrl[MAX_LPM_CTRL] = {NULL, };
	struct dsi_panel_cmd_set *cmd_list[2];

	/*default*/
	int mode = ALPM_MODE_ON;
	int ctrl_index = ALPM_CTRL_2NIT;
	int bl_index = LPM_2NIT_IDX;

	/*
	 * Init reg_list and cmd list
	 * reg_list[X][0] is reg value
	 * reg_list[X][1] is offset for reg value
	 * cmd_list is the target cmds for searching reg value
	 */
	static int reg_list[2][2] = {
		{ALPM_REG, -EINVAL},
		{ALPM_CTRL_REG, -EINVAL} };

	LCD_DEBUG("%s++\n", __func__);

	cmd_list[0] = ss_get_cmds(vdd, TX_LPM_BL_CMD);
	cmd_list[1] = ss_get_cmds(vdd, TX_LPM_BL_CMD);

	/* Init alpm_brightness and alpm_ctrl cmds */
	alpm_brightness[LPM_2NIT_IDX] = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
	alpm_brightness[LPM_10NIT_IDX] = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
	alpm_brightness[LPM_30NIT_IDX] = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
	alpm_brightness[LPM_60NIT_IDX] = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);

	alpm_ctrl[ALPM_CTRL_2NIT] = ss_get_cmds(vdd, TX_ALPM_2NIT_CMD);
	alpm_ctrl[ALPM_CTRL_10NIT] = ss_get_cmds(vdd, TX_ALPM_10NIT_CMD);
	alpm_ctrl[ALPM_CTRL_30NIT] = ss_get_cmds(vdd, TX_ALPM_30NIT_CMD);
	alpm_ctrl[ALPM_CTRL_60NIT] = ss_get_cmds(vdd, TX_ALPM_60NIT_CMD);
	alpm_ctrl[HLPM_CTRL_2NIT] = ss_get_cmds(vdd, TX_HLPM_2NIT_CMD);
	alpm_ctrl[HLPM_CTRL_10NIT] = ss_get_cmds(vdd, TX_HLPM_10NIT_CMD);
	alpm_ctrl[HLPM_CTRL_30NIT] = ss_get_cmds(vdd, TX_HLPM_30NIT_CMD);
	alpm_ctrl[HLPM_CTRL_60NIT] = ss_get_cmds(vdd, TX_HLPM_60NIT_CMD);

	mode = vdd->panel_lpm.mode;

	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_10NIT:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_10NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_10NIT : ALPM_CTRL_10NIT;
		bl_index = LPM_10NIT_IDX;
		break;
	case LPM_30NIT:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_30NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_30NIT : ALPM_CTRL_30NIT;
		bl_index = LPM_30NIT_IDX;
		break;
	case LPM_60NIT:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_60NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_60NIT : ALPM_CTRL_60NIT;
		bl_index = LPM_60NIT_IDX;
		break;
	case LPM_2NIT:
	default:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_2NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_2NIT : ALPM_CTRL_2NIT;
		bl_index = LPM_2NIT_IDX;
		break;
	}

	LCD_DEBUG("[Panel LPM]bl_index %d, ctrl_index %d, mode %d\n",
			 bl_index, ctrl_index, mode);

	/*
	 * Find offset for alpm_reg and alpm_ctrl_reg
	 * alpm_reg  : Control register for ALPM/HLPM on/off
	 * alpm_ctrl_reg : Control register for changing ALPM/HLPM mode
	 */
	if (unlikely((reg_list[0][1] == -EINVAL) ||
				(reg_list[1][1] == -EINVAL)))
		ss_find_reg_offset(reg_list, cmd_list, ARRAY_SIZE(cmd_list));

	if (reg_list[0][1] != -EINVAL) {
		/* Update parameter for ALPM_REG */
		memcpy(cmd_list[0]->cmds[reg_list[0][1]].msg.tx_buf,
				alpm_brightness[bl_index]->cmds[0].msg.tx_buf,
				sizeof(char) * cmd_list[0]->cmds[reg_list[0][1]].msg.tx_len);

		LCD_DEBUG("[Panel LPM] change brightness cmd : %x, %x\n",
				cmd_list[0]->cmds[reg_list[0][1]].msg.tx_buf[1],
				alpm_brightness[bl_index]->cmds[0].msg.tx_buf[1]);
	}

	if (reg_list[1][1] != -EINVAL) {
		/* Initialize ALPM/HLPM cmds */
		/* Update parameter for ALPM_CTRL_REG */
		memcpy(cmd_list[1]->cmds[reg_list[1][1]].msg.tx_buf,
				alpm_ctrl[ctrl_index]->cmds[0].msg.tx_buf,
				sizeof(char) * cmd_list[1]->cmds[reg_list[1][1]].msg.tx_len);

		LCD_DEBUG("[Panel LPM] update alpm ctrl reg\n");
	}

	//send lpm bl cmd
	ss_send_cmd(vdd, TX_LPM_BL_CMD);

	LCD_INFO("[Panel LPM] bl_level : %s\n",
				/* Check current brightness level */
				vdd->panel_lpm.lpm_bl_level == LPM_2NIT ? "2NIT" :
				vdd->panel_lpm.lpm_bl_level == LPM_10NIT ? "10NIT" :
				vdd->panel_lpm.lpm_bl_level == LPM_30NIT ? "30NIT" :
				vdd->panel_lpm.lpm_bl_level == LPM_60NIT ? "60NIT" : "UNKNOWN");

	LCD_DEBUG("%s--\n", __func__);
}

/*
 * This function will update parameters for ALPM_REG/ALPM_CTRL_REG
 * Parameter for ALPM_REG : Control brightness for panel LPM
 * Parameter for ALPM_CTRL_REG : Change panel LPM mode for ALPM/HLPM
 * mode, brightness, hz are updated here.
 */
static void ss_update_panel_lpm_ctrl_cmd(struct samsung_display_driver_data *vdd)
{
	struct dsi_panel_cmd_set *alpm_brightness[LPM_BRIGHTNESS_MAX_IDX] = {NULL, };
	struct dsi_panel_cmd_set *alpm_ctrl[MAX_LPM_CTRL] = {NULL, };
	struct dsi_panel_cmd_set *alpm_off_ctrl[MAX_LPM_MODE] = {NULL, };
	struct dsi_panel_cmd_set *cmd_list[2];
	struct dsi_panel_cmd_set *off_cmd_list[1];

	/*default*/
	int mode = ALPM_MODE_ON;
	int ctrl_index = ALPM_CTRL_2NIT;
	int bl_index = LPM_2NIT_IDX;

	/*
	 * Init reg_list and cmd list
	 * reg_list[X][0] is reg value
	 * reg_list[X][1] is offset for reg value
	 * cmd_list is the target cmds for searching reg value
	 */
	static int reg_list[2][2] = {
		{ALPM_REG, -EINVAL},
		{ALPM_CTRL_REG, -EINVAL} };

	static int off_reg_list[1][2] = {
		{ALPM_CTRL_REG, -EINVAL} };

	LCD_ERR("%s++\n", __func__);

	cmd_list[0] = ss_get_cmds(vdd, TX_LPM_ON);
	cmd_list[1] = ss_get_cmds(vdd, TX_LPM_ON);
	off_cmd_list[0] = ss_get_cmds(vdd, TX_LPM_OFF);

	/* Init alpm_brightness and alpm_ctrl cmds */
	alpm_brightness[LPM_2NIT_IDX] = ss_get_cmds(vdd, TX_LPM_2NIT_CMD);
	alpm_brightness[LPM_10NIT_IDX] = ss_get_cmds(vdd, TX_LPM_10NIT_CMD);
	alpm_brightness[LPM_30NIT_IDX] = ss_get_cmds(vdd, TX_LPM_30NIT_CMD);
	alpm_brightness[LPM_60NIT_IDX] = ss_get_cmds(vdd, TX_LPM_60NIT_CMD);

	alpm_ctrl[ALPM_CTRL_2NIT] = ss_get_cmds(vdd, TX_ALPM_2NIT_CMD);
	alpm_ctrl[ALPM_CTRL_10NIT] = ss_get_cmds(vdd, TX_ALPM_10NIT_CMD);
	alpm_ctrl[ALPM_CTRL_30NIT] = ss_get_cmds(vdd, TX_ALPM_30NIT_CMD);
	alpm_ctrl[ALPM_CTRL_60NIT] = ss_get_cmds(vdd, TX_ALPM_60NIT_CMD);
	alpm_ctrl[HLPM_CTRL_2NIT] = ss_get_cmds(vdd, TX_HLPM_2NIT_CMD);
	alpm_ctrl[HLPM_CTRL_10NIT] = ss_get_cmds(vdd, TX_HLPM_10NIT_CMD);
	alpm_ctrl[HLPM_CTRL_30NIT] = ss_get_cmds(vdd, TX_HLPM_30NIT_CMD);
	alpm_ctrl[HLPM_CTRL_60NIT] = ss_get_cmds(vdd, TX_HLPM_60NIT_CMD);
	alpm_off_ctrl[ALPM_MODE_ON] = ss_get_cmds(vdd, TX_ALPM_OFF);
	alpm_off_ctrl[HLPM_MODE_ON] = ss_get_cmds(vdd, TX_HLPM_OFF);

	mode = vdd->panel_lpm.mode;

	switch (vdd->panel_lpm.lpm_bl_level) {
	case LPM_10NIT:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_10NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_10NIT : ALPM_CTRL_10NIT;
		bl_index = LPM_10NIT_IDX;
		break;
	case LPM_30NIT:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_30NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_30NIT : ALPM_CTRL_30NIT;
		bl_index = LPM_30NIT_IDX;
		break;
	case LPM_60NIT:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_60NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_60NIT : ALPM_CTRL_60NIT;
		bl_index = LPM_60NIT_IDX;
		break;
	case LPM_2NIT:
	default:
		ctrl_index = (mode == ALPM_MODE_ON) ? ALPM_CTRL_2NIT :
			(mode == HLPM_MODE_ON) ? HLPM_CTRL_2NIT : ALPM_CTRL_2NIT;
		bl_index = LPM_2NIT_IDX;
		break;
	}

	LCD_DEBUG("[Panel LPM] change brightness cmd :%d, %d, %d\n",
			 bl_index, ctrl_index, mode);

	/*
	 * Find offset for alpm_reg and alpm_ctrl_reg
	 * alpm_reg	 : Control register for ALPM/HLPM on/off
	 * alpm_ctrl_reg : Control register for changing ALPM/HLPM mode
	 */
	if (unlikely((reg_list[0][1] == -EINVAL) ||
				(reg_list[1][1] == -EINVAL)))
		ss_find_reg_offset(reg_list, cmd_list, ARRAY_SIZE(cmd_list));

	if (unlikely(off_reg_list[0][1] == -EINVAL))
		ss_find_reg_offset(off_reg_list, off_cmd_list,
						ARRAY_SIZE(off_cmd_list));

	if (reg_list[0][1] != -EINVAL) {
		/* Update parameter for ALPM_REG */
		memcpy(cmd_list[0]->cmds[reg_list[0][1]].msg.tx_buf,
				alpm_brightness[bl_index]->cmds[0].msg.tx_buf,
				sizeof(char) * cmd_list[0]->cmds[reg_list[0][1]].msg.tx_len);

		LCD_DEBUG("[Panel LPM] change brightness cmd : %x, %x\n",
				cmd_list[0]->cmds[reg_list[0][1]].msg.tx_buf[1],
				alpm_brightness[bl_index]->cmds[0].msg.tx_buf[1]);
	}

	if (reg_list[1][1] != -EINVAL) {
		/* Initialize ALPM/HLPM cmds */
		/* Update parameter for ALPM_CTRL_REG */
		memcpy(cmd_list[1]->cmds[reg_list[1][1]].msg.tx_buf,
				alpm_ctrl[ctrl_index]->cmds[0].msg.tx_buf,
				sizeof(char) * cmd_list[1]->cmds[reg_list[1][1]].msg.tx_len);

		LCD_DEBUG("[Panel LPM] update alpm ctrl reg\n");
	}

	if ((off_reg_list[0][1] != -EINVAL) &&\
			(mode != LPM_MODE_OFF)) {
		/* Update parameter for ALPM_CTRL_REG */
		memcpy(off_cmd_list[0]->cmds[off_reg_list[0][1]].msg.tx_buf,
				alpm_off_ctrl[mode]->cmds[0].msg.tx_buf,
				sizeof(char) * off_cmd_list[0]->cmds[off_reg_list[0][1]].msg.tx_len);
	}

	LCD_ERR("%s--\n", __func__);
}
#endif

static int dsi_update_mdnie_data(struct samsung_display_driver_data *vdd)
{
	struct mdnie_lite_tune_data *mdnie_data;

	LCD_INFO("++\n");

	mdnie_data = kzalloc(sizeof(struct mdnie_lite_tune_data), GFP_KERNEL);
	if (!mdnie_data) {
		LCD_ERR("fail to allocate mdnie_data memory\n");
		return -ENOMEM;
	}

	/* Update mdnie command */
	mdnie_data->DSI_COLOR_BLIND_MDNIE_1 = DSI0_COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_COLOR_BLIND_MDNIE_2 = DSI0_COLOR_BLIND_MDNIE_2;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_1 = DSI0_RGB_SENSOR_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_2 = DSI0_RGB_SENSOR_MDNIE_2;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE_2 = DSI0_UI_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_UI_STANDARD_MDNIE_2 = DSI0_UI_STANDARD_MDNIE_2;
	mdnie_data->DSI_UI_AUTO_MDNIE_2 = DSI0_UI_AUTO_MDNIE_2;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE_2 = DSI0_VIDEO_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE_2 = DSI0_VIDEO_STANDARD_MDNIE_2;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE_2 = DSI0_VIDEO_AUTO_MDNIE_2;
	mdnie_data->DSI_CAMERA_MDNIE_2 = DSI0_CAMERA_AUTO_MDNIE_2;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE_2 = DSI0_CAMERA_AUTO_MDNIE_2;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE_2 = DSI0_GALLERY_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE_2 = DSI0_GALLERY_STANDARD_MDNIE_2;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE_2 = DSI0_GALLERY_AUTO_MDNIE_2;
	mdnie_data->DSI_VT_DYNAMIC_MDNIE_2 = DSI0_VT_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_VT_STANDARD_MDNIE_2 = DSI0_VT_STANDARD_MDNIE_2;
	mdnie_data->DSI_VT_AUTO_MDNIE_2 = DSI0_VT_AUTO_MDNIE_2;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE_2 = DSI0_BROWSER_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE_2 = DSI0_BROWSER_STANDARD_MDNIE_2;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE_2 = DSI0_BROWSER_AUTO_MDNIE_2;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE_2 = DSI0_EBOOK_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE_2 = DSI0_EBOOK_STANDARD_MDNIE_2;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE_2 = DSI0_EBOOK_AUTO_MDNIE_2;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE_2 = DSI0_TDMB_DYNAMIC_MDNIE_2;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE_2 = DSI0_TDMB_STANDARD_MDNIE_2;
	mdnie_data->DSI_TDMB_AUTO_MDNIE_2 = DSI0_TDMB_AUTO_MDNIE_2;

	mdnie_data->DSI_BYPASS_MDNIE = DSI0_BYPASS_MDNIE;
	mdnie_data->DSI_NEGATIVE_MDNIE = DSI0_NEGATIVE_MDNIE;
	mdnie_data->DSI_COLOR_BLIND_MDNIE = DSI0_COLOR_BLIND_MDNIE;
	mdnie_data->DSI_HBM_CE_MDNIE = DSI0_HBM_CE_MDNIE;
	mdnie_data->DSI_HBM_CE_TEXT_MDNIE = DSI0_HBM_CE_TEXT_MDNIE;
	mdnie_data->DSI_RGB_SENSOR_MDNIE = DSI0_RGB_SENSOR_MDNIE;
	mdnie_data->DSI_CURTAIN = DSI0_CURTAIN;
	mdnie_data->DSI_GRAYSCALE_MDNIE = DSI0_GRAYSCALE_MDNIE;
	mdnie_data->DSI_GRAYSCALE_NEGATIVE_MDNIE = DSI0_GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data->DSI_UI_DYNAMIC_MDNIE = DSI0_UI_DYNAMIC_MDNIE;
	mdnie_data->DSI_UI_STANDARD_MDNIE = DSI0_UI_STANDARD_MDNIE;
	mdnie_data->DSI_UI_NATURAL_MDNIE = DSI0_UI_NATURAL_MDNIE;
	mdnie_data->DSI_UI_MOVIE_MDNIE = DSI0_UI_MOVIE_MDNIE;
	mdnie_data->DSI_UI_AUTO_MDNIE = DSI0_UI_AUTO_MDNIE;
	mdnie_data->DSI_VIDEO_OUTDOOR_MDNIE = DSI0_VIDEO_OUTDOOR_MDNIE;
	mdnie_data->DSI_VIDEO_DYNAMIC_MDNIE = DSI0_VIDEO_DYNAMIC_MDNIE;
	mdnie_data->DSI_VIDEO_STANDARD_MDNIE = DSI0_VIDEO_STANDARD_MDNIE;
	mdnie_data->DSI_VIDEO_NATURAL_MDNIE = DSI0_VIDEO_NATURAL_MDNIE;
	mdnie_data->DSI_VIDEO_MOVIE_MDNIE = DSI0_VIDEO_MOVIE_MDNIE;
	mdnie_data->DSI_VIDEO_AUTO_MDNIE = DSI0_VIDEO_AUTO_MDNIE;
	mdnie_data->DSI_VIDEO_WARM_OUTDOOR_MDNIE = DSI0_VIDEO_OUTDOOR_MDNIE;
	mdnie_data->DSI_VIDEO_WARM_MDNIE = DSI0_VIDEO_OUTDOOR_MDNIE;
	mdnie_data->DSI_VIDEO_COLD_OUTDOOR_MDNIE = DSI0_VIDEO_OUTDOOR_MDNIE;
	mdnie_data->DSI_VIDEO_COLD_MDNIE = DSI0_VIDEO_OUTDOOR_MDNIE;
	mdnie_data->DSI_CAMERA_OUTDOOR_MDNIE = DSI0_CAMERA_OUTDOOR_MDNIE;
	mdnie_data->DSI_CAMERA_MDNIE = DSI0_CAMERA_AUTO_MDNIE;
	mdnie_data->DSI_CAMERA_AUTO_MDNIE = DSI0_CAMERA_AUTO_MDNIE;
	mdnie_data->DSI_GALLERY_DYNAMIC_MDNIE = DSI0_GALLERY_DYNAMIC_MDNIE;
	mdnie_data->DSI_GALLERY_STANDARD_MDNIE = DSI0_GALLERY_STANDARD_MDNIE;
	mdnie_data->DSI_GALLERY_NATURAL_MDNIE = DSI0_GALLERY_NATURAL_MDNIE;
	mdnie_data->DSI_GALLERY_MOVIE_MDNIE = DSI0_GALLERY_MOVIE_MDNIE;
	mdnie_data->DSI_GALLERY_AUTO_MDNIE = DSI0_GALLERY_AUTO_MDNIE;
	mdnie_data->DSI_VT_DYNAMIC_MDNIE = DSI0_VT_DYNAMIC_MDNIE;
	mdnie_data->DSI_VT_STANDARD_MDNIE = DSI0_VT_STANDARD_MDNIE;
	mdnie_data->DSI_VT_NATURAL_MDNIE = DSI0_VT_NATURAL_MDNIE;
	mdnie_data->DSI_VT_MOVIE_MDNIE = DSI0_VT_MOVIE_MDNIE;
	mdnie_data->DSI_VT_AUTO_MDNIE = DSI0_VT_AUTO_MDNIE;
	mdnie_data->DSI_BROWSER_DYNAMIC_MDNIE = DSI0_BROWSER_DYNAMIC_MDNIE;
	mdnie_data->DSI_BROWSER_STANDARD_MDNIE = DSI0_BROWSER_STANDARD_MDNIE;
	mdnie_data->DSI_BROWSER_NATURAL_MDNIE = DSI0_BROWSER_NATURAL_MDNIE;
	mdnie_data->DSI_BROWSER_MOVIE_MDNIE = DSI0_BROWSER_MOVIE_MDNIE;
	mdnie_data->DSI_BROWSER_AUTO_MDNIE = DSI0_BROWSER_AUTO_MDNIE;
	mdnie_data->DSI_EBOOK_DYNAMIC_MDNIE = DSI0_EBOOK_DYNAMIC_MDNIE;
	mdnie_data->DSI_EBOOK_STANDARD_MDNIE = DSI0_EBOOK_STANDARD_MDNIE;
	mdnie_data->DSI_EBOOK_NATURAL_MDNIE = DSI0_EBOOK_NATURAL_MDNIE;
	mdnie_data->DSI_EBOOK_MOVIE_MDNIE = DSI0_EBOOK_MOVIE_MDNIE;
	mdnie_data->DSI_EBOOK_AUTO_MDNIE = DSI0_EBOOK_AUTO_MDNIE;
	mdnie_data->DSI_EMAIL_AUTO_MDNIE = DSI0_EMAIL_AUTO_MDNIE;
	mdnie_data->DSI_TDMB_DYNAMIC_MDNIE = DSI0_TDMB_DYNAMIC_MDNIE;
	mdnie_data->DSI_TDMB_STANDARD_MDNIE = DSI0_TDMB_STANDARD_MDNIE;
	mdnie_data->DSI_TDMB_NATURAL_MDNIE = DSI0_TDMB_NATURAL_MDNIE;
	mdnie_data->DSI_TDMB_MOVIE_MDNIE = DSI0_TDMB_NATURAL_MDNIE;
	mdnie_data->DSI_TDMB_AUTO_MDNIE = DSI0_TDMB_AUTO_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE = DSI0_NIGHT_MODE_MDNIE;
	mdnie_data->DSI_NIGHT_MODE_MDNIE_SCR = DSI0_NIGHT_MODE_MDNIE_1;
	mdnie_data->DSI_COLOR_LENS_MDNIE = DSI0_COLOR_LENS_MDNIE;
	mdnie_data->DSI_COLOR_LENS_MDNIE_SCR = DSI0_COLOR_LENS_MDNIE_1;
	mdnie_data->DSI_COLOR_BLIND_MDNIE_SCR = DSI0_COLOR_BLIND_MDNIE_1;
	mdnie_data->DSI_RGB_SENSOR_MDNIE_SCR = DSI0_RGB_SENSOR_MDNIE_1;

	mdnie_data->mdnie_tune_value_dsi = mdnie_tune_value_dsi0;
	//mdnie_data.mdnie_tune_value_dsi1 = mdnie_tune_value_dsi0;

	mdnie_data->light_notification_tune_value_dsi = light_notification_tune_value_dsi0;
	//mdnie_data.light_notification_tune_value_dsi1 = light_notification_tune_value_dsi0;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data->dsi_bypass_mdnie_size = ARRAY_SIZE(DSI0_BYPASS_MDNIE);
	mdnie_data->mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET;
	mdnie_data->mdnie_step_index[MDNIE_STEP1] = MDNIE_STEP1_INDEX;
	mdnie_data->mdnie_step_index[MDNIE_STEP2] = MDNIE_STEP2_INDEX;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_RED_OFFSET] = ADDRESS_SCR_WHITE_RED;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_GREEN_OFFSET] = ADDRESS_SCR_WHITE_GREEN;
	mdnie_data->address_scr_white[ADDRESS_SCR_WHITE_BLUE_OFFSET] = ADDRESS_SCR_WHITE_BLUE;
	mdnie_data->dsi_rgb_sensor_mdnie_1_size = DSI0_RGB_SENSOR_MDNIE_1_SIZE;
	mdnie_data->dsi_rgb_sensor_mdnie_2_size = DSI0_RGB_SENSOR_MDNIE_2_SIZE;
	mdnie_data->hdr_tune_value_dsi = hdr_tune_value_dsi0;
	//mdnie_data->hdr_tune_value_dsi1 = hdr_tune_value_dsi0;
	mdnie_data->dsi_adjust_ldu_table = adjust_ldu_data;
	//mdnie_data.dsi1_adjust_ldu_table = adjust_ldu_data;
	mdnie_data->dsi_max_adjust_ldu = 6;
	//mdnie_data.dsi1_max_adjust_ldu = 6;
	mdnie_data->dsi_night_mode_table = night_mode_data;
	//mdnie_data.dsi1_night_mode_table = night_mode_data;
	mdnie_data->dsi_max_night_mode_index = 11;
	//mdnie_data.dsi1_max_night_mode_index = 11;
	mdnie_data->dsi_color_lens_table = color_lens_data;
	//mdnie_data.dsi1_color_lens_table = color_lens_data;
	mdnie_data->dsi_scr_step_index = MDNIE_STEP1_INDEX;
	//mdnie_data.dsi1_scr_step_index = MDNIE_STEP1_INDEX;
	mdnie_data->dsi_white_default_r = 0xff;
	mdnie_data->dsi_white_default_g = 0xff;
	mdnie_data->dsi_white_default_b = 0xff;
	//mdnie_data->dsi1_white_default_r = 0xff;
	//mdnie_data->dsi1_white_default_g = 0xff;
	//mdnie_data->dsi1_white_default_b = 0xff;

	vdd->mdnie.mdnie_data = mdnie_data;

	LCD_INFO("--\n");
	return 0;
}

static void samsung_panel_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO("++\n");
	LCD_ERR("%s\n", ss_get_panel_name(vdd));

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
	vdd->panel_func.samsung_octa_id_read = NULL;
	vdd->panel_func.samsung_elvss_read = ss_elvss_read;
	vdd->panel_func.samsung_hbm_read = ss_hbm_read;
	vdd->panel_func.samsung_mdnie_read = ss_mdnie_read;

	vdd->panel_func.samsung_smart_dimming_init = ss_smart_dimming_init;

	vdd->panel_func.samsung_smart_get_conf = smart_get_conf_ANA38401_AMSA05RB01;

	/* Brightness */
	vdd->panel_func.samsung_brightness_hbm_off = ss_hbm_off;
	vdd->panel_func.samsung_brightness_aid = ss_aid;
	vdd->panel_func.samsung_brightness_acl_on = ss_acl_on;
	vdd->panel_func.samsung_brightness_pre_acl_percent = NULL;
	vdd->panel_func.samsung_brightness_acl_percent = NULL;
	vdd->panel_func.samsung_brightness_acl_off = ss_acl_off;
	vdd->panel_func.samsung_brightness_pre_elvss = ss_pre_elvss;
	vdd->panel_func.samsung_brightness_elvss = ss_elvss;
	vdd->panel_func.samsung_brightness_elvss_2 = ss_elvss_2;
	vdd->panel_func.samsung_brightness_elvss_temperature1 = NULL;
	vdd->panel_func.samsung_brightness_elvss_temperature2 = NULL;
	vdd->panel_func.samsung_brightness_vint = NULL;
	vdd->panel_func.samsung_brightness_gamma = ss_gamma;

	/* HBM */
	vdd->panel_func.samsung_hbm_gamma = ss_hbm_gamma;
	vdd->panel_func.samsung_hbm_etc = ss_hbm_etc;
	vdd->panel_func.samsung_hbm_irc = NULL;
	vdd->panel_func.get_hbm_candela_value = NULL;

	/* Panel LPM */
	//vdd->panel_func.samsung_get_panel_lpm_mode = NULL;

	/* default brightness */
	vdd->br.bl_level = 255;

	/* mdnie */
	vdd->mdnie.support_mdnie = true;
	vdd->mdnie.mdnie_tune_size[1]= 2;
	vdd->mdnie.mdnie_tune_size[2]= 56;
	vdd->mdnie.mdnie_tune_size[3]= 2;
	vdd->mdnie.mdnie_tune_size[4]= 103;

	dsi_update_mdnie_data(vdd);

	/* send recovery pck before sending image date (for ESD recovery) */
	vdd->esd_recovery.send_esd_recovery = false;
	vdd->br.auto_level = 12;

	/* Enable panic on first pingpong timeout */
	if (vdd->debug_data)
		vdd->debug_data->panic_on_pptimeout = true;

	/* Set IRC init value */
	vdd->br.irc_mode = IRC_MODERATO_MODE;

	/* COLOR WEAKNESS */
	vdd->panel_func.color_weakness_ccb_on_off = NULL;

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = NULL;

	/* ACL default ON */
	vdd->acl_status = 1;

	LCD_INFO("--\n");
}

static int __init samsung_panel_initialize(void)
{
	struct samsung_display_driver_data *vdd;
	enum ss_display_ndx ndx = PRIMARY_DISPLAY_NDX;
	char panel_string[] = "ss_dsi_panel_ANA38401_AMSA05RB01_WQXGA";
	char panel_name[MAX_CMDLINE_PARAM_LEN];
	char panel_secondary_name[MAX_CMDLINE_PARAM_LEN];

	LCD_INFO("++\n");

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
	LCD_INFO("--\n");

	return 0;
}

early_initcall(samsung_panel_initialize);
