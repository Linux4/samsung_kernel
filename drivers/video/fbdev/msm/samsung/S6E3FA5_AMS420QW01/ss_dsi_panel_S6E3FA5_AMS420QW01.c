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
#include "ss_dsi_panel_S6E3FA5_AMS420QW01.h"
#include "ss_dsi_mdnie_S6E3FA5_AMS420QW01.h"
#include "../../mdss_dsi.h"

/* AOD Mode status on AOD Service */
enum {
	AOD_MODE_ALPM_2NIT_ON = MAX_LPM_MODE + 1,
	AOD_MODE_HLPM_2NIT_ON,
	AOD_MODE_ALPM_60NIT_ON,
	AOD_MODE_HLPM_60NIT_ON,
};

enum {
	ALPM_CTRL_2NIT,
	ALPM_CTRL_60NIT,
	HLPM_CTRL_2NIT,
	HLPM_CTRL_60NIT,
	MAX_LPM_CTRL,
};

/* Register to control brightness level */
#define ALPM_REG	0x53
/* Register to cnotrol ALPM/HLPM mode */
#define ALPM_CTRL_REG	0xBB

static int mdss_panel_on_pre(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	pr_info("%s %d\n", __func__, ndx);

	mdss_panel_attach_set(ctrl, true);

	return true;
}


static int mdss_panel_on_post(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	pr_info("%s %d\n", __func__, ndx);

	return true;
}

static char mdss_panel_revision(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	if (vdd->manufacture_id_dsi[ndx] == PBA_ID)
		mdss_panel_attach_set(ctrl, false);
	else
		mdss_panel_attach_set(ctrl, true);

	vdd->panel_revision = (int)mdss_panel_rev_get(ctrl);

	LCD_DEBUG("panel_revision = %c %d \n",
					vdd->panel_revision + 'A', vdd->panel_revision);

	return vdd->panel_revision + 'A';
}

static int mdss_manufacture_date_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	unsigned char date[4];
	int year, month, day;
	int hour, min;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (C8h 41,42th) for manufacture date */
	if (get_panel_rx_cmds(ctrl, RX_MANUFACTURE_DATE)->cmd_cnt) {
		mdss_samsung_panel_data_read(ctrl,
			get_panel_rx_cmds(ctrl, RX_MANUFACTURE_DATE),
			date, LEVEL1_KEY);

		year = date[0] & 0xf0;
		year >>= 4;
		year += 2011; // 0 = 2011 year
		month = date[0] & 0x0f;
		day = date[1] & 0x1f;
		hour = date[2]& 0x0f;
		min = date[3] & 0x1f;

		vdd->manufacture_date_dsi[ndx] = year * 10000 + month * 100 + day;
		vdd->manufacture_time_dsi[ndx] = hour * 100 + min;

		LCD_ERR("manufacture_date PANEL%d = (%d%04d) - year(%d) month(%d) day(%d) hour(%d) min(%d)\n",
			ndx, vdd->manufacture_date_dsi[ndx], vdd->manufacture_time_dsi[ndx],
			year, month, day, hour, min);

	} else {
		LCD_ERR("PANEL%d no manufacture_date_rx_cmds cmds(%d)",  ndx, vdd->panel_revision);
		return false;
	}

	return true;
}

static int mdss_ddi_id_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char ddi_id[5];
	int loop;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (D6h 1~5th) for ddi id */
	if (get_panel_rx_cmds(ctrl, RX_DDI_ID)->cmd_cnt) {
		mdss_samsung_panel_data_read(ctrl,
			get_panel_rx_cmds(ctrl, RX_DDI_ID),
			ddi_id, LEVEL1_KEY);

		for(loop = 0; loop < 5; loop++)
			vdd->ddi_id_dsi[ndx][loop] = ddi_id[loop];

		LCD_INFO("PANEL%d : %02x %02x %02x %02x %02x\n", ndx,
			vdd->ddi_id_dsi[ndx][0], vdd->ddi_id_dsi[ndx][1],
			vdd->ddi_id_dsi[ndx][2], vdd->ddi_id_dsi[ndx][3],
			vdd->ddi_id_dsi[ndx][4]);
	} else {
		LCD_ERR("PANEL%d no ddi_id_rx_cmds cmds", ndx);
		return false;
	}

	return true;
}

static int mdss_cell_id_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char cell_id_buffer[MAX_CELL_ID] = {0,};
	int loop;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read Panel Unique Cell ID (C8h 41~51th) */
	if (get_panel_rx_cmds(ctrl, RX_CELL_ID)->cmd_cnt) {
		memset(cell_id_buffer, 0x00, MAX_CELL_ID);

		mdss_samsung_panel_data_read(ctrl,
			get_panel_rx_cmds(ctrl, RX_CELL_ID),
			cell_id_buffer, LEVEL1_KEY);

		for(loop = 0; loop < MAX_CELL_ID; loop++)
			vdd->cell_id_dsi[ndx][loop] = cell_id_buffer[loop];

		LCD_INFO("PANEL%d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			ndx, vdd->cell_id_dsi[ndx][0],
			vdd->cell_id_dsi[ndx][1],	vdd->cell_id_dsi[ndx][2],
			vdd->cell_id_dsi[ndx][3],	vdd->cell_id_dsi[ndx][4],
			vdd->cell_id_dsi[ndx][5],	vdd->cell_id_dsi[ndx][6],
			vdd->cell_id_dsi[ndx][7],	vdd->cell_id_dsi[ndx][8],
			vdd->cell_id_dsi[ndx][9],	vdd->cell_id_dsi[ndx][10]);

	} else {
		LCD_ERR("PANEL%d no cell_id_rx_cmds cmd\n", ndx);
		return false;
	}

	return true;
}

static int mdss_octa_id_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char octa_id_buffer[MAX_OCTA_ID] = {0,};
	int loop;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read Panel Unique OCTA ID (C9h 2nd~21th) */
	if (get_panel_rx_cmds(ctrl, RX_OCTA_ID)->cmd_cnt) {
		memset(octa_id_buffer, 0x00, MAX_OCTA_ID);

		mdss_samsung_panel_data_read(ctrl,
			get_panel_rx_cmds(ctrl, RX_OCTA_ID),
			octa_id_buffer, LEVEL1_KEY);

		for(loop = 0; loop < MAX_OCTA_ID; loop++)
			vdd->octa_id_dsi[ndx][loop] = octa_id_buffer[loop];

		LCD_INFO("PANEL%d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			ndx, vdd->octa_id_dsi[ndx][0], vdd->octa_id_dsi[ndx][1],
			vdd->octa_id_dsi[ndx][2],	vdd->octa_id_dsi[ndx][3],
			vdd->octa_id_dsi[ndx][4],	vdd->octa_id_dsi[ndx][5],
			vdd->octa_id_dsi[ndx][6],	vdd->octa_id_dsi[ndx][7],
			vdd->octa_id_dsi[ndx][8],	vdd->octa_id_dsi[ndx][9],
			vdd->octa_id_dsi[ndx][10],vdd->octa_id_dsi[ndx][11],
			vdd->octa_id_dsi[ndx][12],vdd->octa_id_dsi[ndx][13],
			vdd->octa_id_dsi[ndx][14],vdd->octa_id_dsi[ndx][15],
			vdd->octa_id_dsi[ndx][16],vdd->octa_id_dsi[ndx][17],
			vdd->octa_id_dsi[ndx][18],vdd->octa_id_dsi[ndx][19]);

	} else {
		LCD_ERR("PANEL%d no octa_id_rx_cmds cmd\n", ndx);
		return false;
	}

	return true;
}

static int mdss_elvss_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char elvss_b5[2];
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (B5h 24th,25th) for elvss*/
	mdss_samsung_panel_data_read(ctrl,
		get_panel_rx_cmds(ctrl, RX_ELVSS),
		elvss_b5, LEVEL1_KEY);
	vdd->display_status_dsi[ndx].elvss_value1 = elvss_b5[0]; /*0xB5 24th OTP value */
	vdd->display_status_dsi[ndx].elvss_value2 = elvss_b5[1]; /*0xB5 25th OTP value */

	return true;
}

static int mdss_hbm_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char hbm_buffer1[6] = {0,};
	char hbm_buffer2[29] = {0,};

	char V255R_0, V255R_1, V255G_0, V255G_1, V255B_0, V255B_1;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *hbm_gamma_cmds = get_panel_tx_cmds(ctrl, TX_HBM_GAMMA);
	int ndx = display_ndx_check(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	if (get_panel_rx_cmds(ctrl, RX_HBM)->cmd_cnt) {
		mdss_samsung_panel_data_read(ctrl,
			get_panel_rx_cmds(ctrl, RX_HBM), // B3 3rd~6th
			hbm_buffer1, LEVEL1_KEY);

		V255R_0 = (hbm_buffer1[0] & BIT(2))>>2;
		V255R_1 = hbm_buffer1[1];
		V255G_0 = (hbm_buffer1[0] & BIT(1))>>1;
		V255G_1 = hbm_buffer1[2];
		V255B_0 = hbm_buffer1[0] & BIT(0);
		V255B_1 = hbm_buffer1[3];
		hbm_buffer1[0] = V255R_0;
		hbm_buffer1[1] = V255R_1;
		hbm_buffer1[2] = V255G_0;
		hbm_buffer1[3] = V255G_1;
		hbm_buffer1[4] = V255B_0;
		hbm_buffer1[5] = V255B_1;

		pr_info("mdss_hbm_read = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
			hbm_buffer1[0], hbm_buffer1[1], hbm_buffer1[2], hbm_buffer1[3], hbm_buffer1[4], hbm_buffer1[5]);
		memcpy(&hbm_gamma_cmds->cmds[0].payload[1], hbm_buffer1, 6);

	} else {
		pr_err("%s PANEL%d error", __func__, ndx);
		return false;
	}

	if (get_panel_rx_cmds(ctrl, RX_HBM2)->cmd_cnt) {
		mdss_samsung_panel_data_read(ctrl,
			get_panel_rx_cmds(ctrl, RX_HBM2), // B3 7th~35th
			hbm_buffer2, LEVEL1_KEY);

		memcpy(&hbm_gamma_cmds->cmds[0].payload[7], hbm_buffer2, 29);
	} else {
		pr_err("%s PANEL%d error\n", __func__, ndx);
		return false;
	}

	return true;
}

static int get_hbm_candela_value(int level)
{
	if (level == 6)
		return 443;
	else if (level == 7)
		return 465;
	else if (level == 8)
		return 488;
	else if (level == 9)
		return 510;
	else if (level == 10)
		return 533;
	else if (level == 11)
		return 555;
	else if (level == 12)
		return 578;
	else if (level == 13)
		return 600;
	else
		return 600;
}

static struct dsi_panel_cmds *mdss_hbm_gamma(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(ctrl);
	struct dsi_panel_cmds *hbm_gamma_cmds = get_panel_tx_cmds(ctrl, TX_HBM_GAMMA);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi[ndx]->generate_gamma)) {
		pr_err("%s generate_gamma is NULL error", __func__);
		return NULL;
	} else {
		vdd->smart_dimming_dsi[ndx]->generate_hbm_gamma(
			vdd->smart_dimming_dsi[ndx],
			vdd->auto_brightness,
			&hbm_gamma_cmds->cmds[0].payload[1]);

		*level_key = LEVEL1_KEY;
		return hbm_gamma_cmds;
	}
}

static struct dsi_panel_cmds *mdss_hbm_etc(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *hbm_etc_cmds = get_panel_tx_cmds(ctrl, TX_HBM_ETC);
	char elvss_3th_val, elvss_25th_val;
	char elvss_443_offset, elvss_465_offset, elvss_488_offset, elvss_510_offset, elvss_533_offset;
	char elvss_555_offset, elvss_578_offset, elvss_600_offset;
	int ndx = display_ndx_check(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	elvss_3th_val = elvss_25th_val = 0;

	/* OTP value - B5 25 th */
	elvss_25th_val = vdd->display_status_dsi[ndx].elvss_value2;

	/* ELVSS 0xB5 3th*/
	elvss_443_offset = 0x12;
	elvss_465_offset = 0x11;
	elvss_488_offset = 0x0F;
	elvss_510_offset = 0x0E;
	elvss_533_offset = 0x0C;
	elvss_555_offset = 0x0B;
	elvss_578_offset = 0x0B;
	elvss_600_offset = 0x0A;

	/* ELVSS 0xB5 3th*/
	if (vdd->auto_brightness == HBM_MODE) /* 443CD */
		elvss_3th_val = elvss_443_offset;
	else if (vdd->auto_brightness == HBM_MODE + 1) /* 465CD */
		elvss_3th_val = elvss_465_offset;
	else if (vdd->auto_brightness == HBM_MODE + 2) /* 488CD */
		elvss_3th_val = elvss_488_offset;
	else if (vdd->auto_brightness == HBM_MODE + 3) /* 510CD */
		elvss_3th_val = elvss_510_offset;
	else if (vdd->auto_brightness == HBM_MODE + 4) /* 533CD */
		elvss_3th_val = elvss_533_offset;
	else if (vdd->auto_brightness == HBM_MODE + 5) /* 555CD */
		elvss_3th_val= elvss_555_offset;
	else if (vdd->auto_brightness == HBM_MODE + 6) /* 578CD */
		elvss_3th_val = elvss_578_offset;
	else/* 600CD */
		elvss_3th_val = elvss_600_offset;

	/* 0xB5 1st para (temperature) */
	hbm_etc_cmds->cmds[1].payload[1] =
			vdd->temperature > 0 ? vdd->temperature : 0x80|(-1*vdd->temperature);

	/* ELVSS 0xB5 3th, elvss_24th_val */
	hbm_etc_cmds->cmds[1].payload[3] = elvss_3th_val;

	/* 0xB5 24th para */
	hbm_etc_cmds->cmds[1].payload[24] = elvss_25th_val;

	/* ACL on/off */
	hbm_etc_cmds->cmds[2].payload[2] = vdd->acl_status ? 0x50 : 0x40;
	hbm_etc_cmds->cmds[3].payload[1] = vdd->acl_status ? 0x02 : 0x00;

	*level_key = LEVEL1_KEY;

	LCD_INFO("0xB5 3th: 0x%x 0xB5 elvss_25th_val(elvss val) 0x%x\n", elvss_3th_val, elvss_25th_val);

	return hbm_etc_cmds;
}

static struct dsi_panel_cmds *mdss_hbm_off(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;
	return get_panel_tx_cmds(ctrl, TX_HBM_OFF);
}


#define COORDINATE_DATA_SIZE 6

#define F1(x,y) ((y)-((353*(x))/326)+30)
#define F2(x,y) ((y)-((20*(x))/19)-14)
#define F3(x,y) ((y)+((185*(x))/42)-16412)
#define F4(x,y) ((y)+((337*(x))/106)-12601)

/* Normal Mode */
static char coordinate_data_1[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xfb, 0x00, 0xfb, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfd, 0x00, 0xff, 0x00}, /* Tune_2 */
	{0xfb, 0x00, 0xfa, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfe, 0x00, 0xfc, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xfb, 0x00, 0xfc, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfd, 0x00, 0xff, 0x00, 0xfa, 0x00}, /* Tune_7 */
	{0xfd, 0x00, 0xff, 0x00, 0xfd, 0x00}, /* Tune_8 */
	{0xfb, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_9 */
};

/* sRGB/Adobe RGB Mode */
static char coordinate_data_2[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xf3, 0x00, 0xeb, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xf4, 0x00, 0xef, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xf6, 0x00, 0xf3, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xf7, 0x00, 0xec, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xf7, 0x00, 0xef, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xf9, 0x00, 0xf3, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xf9, 0x00, 0xec, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xfa, 0x00, 0xef, 0x00}, /* Tune_8 */
	{0xff, 0x00, 0xfc, 0x00, 0xf4, 0x00}, /* Tune_9 */
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

static int mdss_mdnie_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char x_y_location[4];
	int mdnie_tune_index = 0;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (D6h 1~5th) for ddi id */
	if (get_panel_rx_cmds(ctrl, RX_MDNIE)->cmd_cnt) {
		mdss_samsung_panel_data_read(ctrl,
			get_panel_rx_cmds(ctrl, RX_MDNIE),
			x_y_location, LEVEL1_KEY);

		vdd->mdnie_x[ndx] = x_y_location[0] << 8 | x_y_location[1];	/* X */
		vdd->mdnie_y[ndx] = x_y_location[2] << 8 | x_y_location[3];	/* Y */

		mdnie_tune_index = mdnie_coordinate_index(vdd->mdnie_x[ndx], vdd->mdnie_y[ndx]);
		coordinate_tunning_multi(ndx, coordinate_data_multi, mdnie_tune_index,
			ADDRESS_SCR_WHITE_RED, COORDINATE_DATA_SIZE);

		pr_info("%s PANEL%d : X-%d Y-%d \n", __func__, ndx,
			vdd->mdnie_x[ndx], vdd->mdnie_y[ndx]);
	} else {
		pr_err("%s PANEL%d error", __func__, ndx);
		return false;
	}

	return true;
}

static int mdss_smart_dimming_init(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	vdd->smart_dimming_dsi[ndx] = vdd->panel_func.samsung_smart_get_conf();

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi[ndx])) {
		LCD_ERR("PANEL%d smart_dimming_dsi is null", ndx);
		return false;
	} else {
		mdss_samsung_panel_data_read(ctrl,
			get_panel_rx_cmds(ctrl, RX_SMART_DIM_MTP),
			vdd->smart_dimming_dsi[ndx]->mtp_buffer, LEVEL1_KEY);

		/* Initialize smart dimming related things here */
		/* lux_tab setting for 420cd */
		vdd->smart_dimming_dsi[ndx]->lux_tab = vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].cd;
		vdd->smart_dimming_dsi[ndx]->lux_tabsize = vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].tab_size;
		vdd->smart_dimming_dsi[ndx]->man_id = vdd->manufacture_id_dsi[ndx];

		/* copy hbm gamma payload for hbm interpolation calc */
		vdd->smart_dimming_dsi[ndx]->hbm_payload = &get_panel_tx_cmds(ctrl, TX_HBM_GAMMA)->cmds[0].payload[1];

		/* Just a safety check to ensure smart dimming data is initialised well */
		vdd->smart_dimming_dsi[ndx]->init(vdd->smart_dimming_dsi[ndx]);

		vdd->temperature = 20; // default temperature

		vdd->smart_dimming_loaded_dsi[ndx] = true;
	}

	LCD_INFO("PANEL%d : --\n", ndx);

	return true;
}

static struct dsi_panel_cmds aid_cmd;
static struct dsi_panel_cmds *mdss_aid(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int cd_index = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	if (vdd->pac)
		cd_index = vdd->pac_cd_idx;
	else
		cd_index = vdd->bl_level;

	aid_cmd.cmds = &(get_panel_tx_cmds(ctrl, TX_AID_SUBDIVISION)->cmds[cd_index]);
	aid_cmd.cmd_cnt = 1;
	pr_info("%s : level(%d), aid(%x %x)\n", __func__, cd_index, aid_cmd.cmds->payload[1], aid_cmd.cmds->payload[2]);
	*level_key = LEVEL1_KEY;

	return &aid_cmd;
}

static struct dsi_panel_cmds * mdss_acl_on(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *acl_on_cmds = get_panel_tx_cmds(ctrl, TX_ACL_ON);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;
	if (vdd->gradual_acl_val)
		acl_on_cmds->cmds[0].payload[6] = vdd->gradual_acl_val;

	return acl_on_cmds;
}

static struct dsi_panel_cmds * mdss_acl_off(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;

	return get_panel_tx_cmds(ctrl, TX_ACL_OFF);
}


static struct dsi_panel_cmds elvss_cmd;
static struct dsi_panel_cmds * mdss_elvss(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *smart_acl_elvss_cmds = get_panel_tx_cmds(ctrl, TX_SMART_ACL_ELVSS);
	struct dsi_panel_cmds *smart_acl_elvss_lowtemp_cmds = get_panel_tx_cmds(ctrl, TX_SMART_ACL_ELVSS_LOWTEMP);
	int ndx = display_ndx_check(ctrl);
	int cd_index = 0;
	int cmd_idx = 0;
	char elvss_24th_val;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	/* OTP value - B5 24th */
	elvss_24th_val = vdd->display_status_dsi[ndx].elvss_value1;
	LCD_DEBUG("OTP val %x\n", elvss_24th_val);

	cd_index = vdd->cd_idx;

	if (!vdd->dtsi_data[ndx].smart_acl_elvss_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data[ndx].smart_acl_elvss_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data[ndx].smart_acl_elvss_map_table[vdd->panel_revision].cmd_idx[cd_index];
	if(vdd->temperature > 0)
		elvss_cmd.cmds = &(smart_acl_elvss_cmds->cmds[cmd_idx]);
	else
		elvss_cmd.cmds = &(smart_acl_elvss_lowtemp_cmds->cmds[cmd_idx]);
	elvss_cmd.cmd_cnt = 1;

	/* 0xB5 1th TSET */
	elvss_cmd.cmds[0].payload[1] = vdd->temperature > 0 ? vdd->temperature : 0x80|(-1*vdd->temperature);
	/* 0xB5 elvss_24th_val elvss_offset */
	elvss_cmd.cmds[0].payload[24] = elvss_24th_val;

	*level_key = LEVEL1_KEY;

	return &elvss_cmd;
end :
	pr_err("%s error", __func__);
	return NULL;
}


static struct dsi_panel_cmds vint_cmd;
static struct dsi_panel_cmds *mdss_vint(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *vint_cmds = get_panel_tx_cmds(ctrl, TX_VINT);
	int ndx = display_ndx_check(ctrl);
	int cd_index = 0;
	int cmd_idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	cd_index = vdd->cd_idx;

	if (!vdd->dtsi_data[ndx].vint_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data[ndx].vint_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data[ndx].vint_map_table[vdd->panel_revision].cmd_idx[cd_index];

	if (vdd->temperature > vdd->elvss_interpolation_temperature )
		vint_cmd.cmds = &(vint_cmds->cmds[cmd_idx]);
	else
		vint_cmd.cmds = &(vint_cmds->cmds[0]);
	vint_cmd.cmd_cnt = 1;

	*level_key = LEVEL1_KEY;

	return &vint_cmd;

end :
	LCD_ERR("error");
	return NULL;
}

static struct dsi_panel_cmds * mdss_gamma(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds  *gamma_cmds = get_panel_tx_cmds(ctrl, TX_GAMMA);
	int ndx = display_ndx_check(ctrl);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(gamma_cmds)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx cmds : 0x%zx", (size_t)ctrl, (size_t)vdd, (size_t)gamma_cmds);
		return NULL;
	}

	pr_debug("%s bl_level : %d candela : %dCD\n", __func__, vdd->bl_level, vdd->candela_level);

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi[ndx]->generate_gamma)) {
		pr_err("%s generate_gamma is NULL error", __func__);
		return NULL;
	} else {
		vdd->smart_dimming_dsi[ndx]->generate_gamma(
			vdd->smart_dimming_dsi[ndx],
			vdd->candela_level,
			&gamma_cmds->cmds[0].payload[1]);

		*level_key = LEVEL1_KEY;

		return gamma_cmds;
	}
}

/*
 * This function will update parameters for ALPM_REG/ALPM_CTRL_REG
 * Parameter for ALPM_REG : Control brightness for panel LPM
 * Parameter for ALPM_CTRL_REG : Change panel LPM mode for ALPM/HLPM
 */
static int mdss_update_panel_lpm_cmds(struct mdss_dsi_ctrl_pdata *ctrl, int bl_level, int mode)
{
	struct samsung_display_driver_data *vdd = NULL;
	struct dsi_panel_cmds *alpm_brightness[PANEL_LPM_BRIGHTNESS_MAX] = {NULL, };
	struct dsi_panel_cmds *alpm_ctrl[MAX_LPM_CTRL] = {NULL, };
	struct dsi_panel_cmds *alpm_off_ctrl[MAX_LPM_CTRL] = {NULL, };
	struct dsi_panel_cmds *cmd_list[2];
	struct dsi_panel_cmds *off_cmd_list[1];

	/*
	 * Init reg_list and cmd list
	 * reg_list[X][0] is reg value
	 * reg_list[X][1] is offset for reg value
	 * cmd_list is the target cmds for searching reg value
	 */
	static int reg_list[2][2] = {
		{ALPM_REG, -EINVAL},
		{ALPM_CTRL_REG, -EINVAL}};

	static int off_reg_list[1][2] = {
		{ALPM_CTRL_REG, -EINVAL}};

	if (IS_ERR_OR_NULL(ctrl))
		goto end;

	vdd = check_valid_ctrl(ctrl);

	cmd_list[0] = get_panel_tx_cmds(ctrl, TX_LPM_ON);
	cmd_list[1] = get_panel_tx_cmds(ctrl, TX_LPM_ON);
	off_cmd_list[0] = get_panel_tx_cmds(ctrl, TX_LPM_OFF);

	/* Init alpm_brightness and alpm_ctrl cmds */
	alpm_brightness[PANEL_LPM_2NIT] = get_panel_tx_cmds(ctrl, TX_LPM_2NIT_CMD);
	alpm_brightness[PANEL_LPM_40NIT] = get_panel_tx_cmds(ctrl, TX_LPM_40NIT_CMD);
	alpm_brightness[PANEL_LPM_60NIT] = get_panel_tx_cmds(ctrl, TX_LPM_60NIT_CMD);

	alpm_ctrl[ALPM_CTRL_2NIT] = get_panel_tx_cmds(ctrl, TX_ALPM_2NIT_CMD);
	alpm_ctrl[ALPM_CTRL_60NIT] = get_panel_tx_cmds(ctrl, TX_ALPM_60NIT_CMD);
	alpm_ctrl[HLPM_CTRL_2NIT] = get_panel_tx_cmds(ctrl, TX_HLPM_2NIT_CMD);
	alpm_ctrl[HLPM_CTRL_60NIT] = get_panel_tx_cmds(ctrl, TX_HLPM_60NIT_CMD);

	alpm_off_ctrl[ALPM_CTRL_2NIT] = get_panel_tx_cmds(ctrl, TX_ALPM_2NIT_OFF);
	alpm_off_ctrl[ALPM_CTRL_60NIT] = get_panel_tx_cmds(ctrl, TX_ALPM_60NIT_OFF);
	alpm_off_ctrl[HLPM_CTRL_2NIT] = get_panel_tx_cmds(ctrl, TX_HLPM_2NIT_OFF);
	alpm_off_ctrl[HLPM_CTRL_60NIT] = get_panel_tx_cmds(ctrl, TX_HLPM_60NIT_OFF);
	/*
	 * Find offset for alpm_reg and alpm_ctrl_reg
	 * alpm_reg 	 : Control register for ALPM/HLPM on/off
	 * alpm_ctrl_reg : Control register for changing ALPM/HLPM mode
	 */
	if (unlikely((reg_list[0][1] == -EINVAL) ||\
				(reg_list[1][1] == -EINVAL)))
		mdss_init_panel_lpm_reg_offset(ctrl, reg_list, cmd_list,
				sizeof(cmd_list) / sizeof(cmd_list[0]));

	if (unlikely(off_reg_list[0][1] == -EINVAL))
		mdss_init_panel_lpm_reg_offset(ctrl, off_reg_list, off_cmd_list,
				sizeof(off_cmd_list) / sizeof(off_cmd_list[0]));

	if (reg_list[0][1] != -EINVAL) {
		/* Update parameter for ALPM_REG */
		memcpy(cmd_list[0]->cmds[reg_list[0][1]].payload,
				alpm_brightness[bl_level]->cmds[0].payload,
				sizeof(char) * cmd_list[0]->cmds[reg_list[0][1]].dchdr.dlen);

		LCD_DEBUG("[Panel LPM] change brightness cmd : %x, %x\n",
				cmd_list[0]->cmds[reg_list[0][1]].payload[1],
				alpm_brightness[bl_level]->cmds[0].payload[1]);
	}

	switch (bl_level) {
		case PANEL_LPM_40NIT:
		case PANEL_LPM_60NIT:
			mode = (mode == ALPM_MODE_ON) ? ALPM_CTRL_60NIT :
				(mode == HLPM_MODE_ON) ? HLPM_CTRL_60NIT : ALPM_CTRL_60NIT;
			break;
		case PANEL_LPM_2NIT:
		default:
			mode = (mode == ALPM_MODE_ON) ? ALPM_CTRL_2NIT :
				(mode == HLPM_MODE_ON) ? HLPM_CTRL_2NIT : ALPM_CTRL_2NIT;
			break;
	}

	if (reg_list[1][1] != -EINVAL) {
		/* Initialize ALPM/HLPM cmds */
		/* Update parameter for ALPM_CTRL_REG */
		memcpy(cmd_list[1]->cmds[reg_list[1][1]].payload,
				alpm_ctrl[mode]->cmds[0].payload,
				sizeof(char) * cmd_list[1]->cmds[reg_list[1][1]].dchdr.dlen);
		LCD_DEBUG("[Panel LPM] update alpm ctrl reg(%d)\n", mode);
	}

	if ((off_reg_list[0][1] != -EINVAL) &&\
			(mode != TX_LPM_OFF)) {
		/* Update parameter for ALPM_CTRL_REG */
		memcpy(off_cmd_list[0]->cmds[off_reg_list[0][1]].payload,
				alpm_off_ctrl[mode]->cmds[0].payload,
				sizeof(char) * off_cmd_list[0]->cmds[off_reg_list[0][1]].dchdr.dlen);
	}
end:
	return 0;
}


static void mdss_get_panel_lpm_mode(struct mdss_dsi_ctrl_pdata *ctrl, u8 *mode)
{
	struct samsung_display_driver_data *vdd = NULL;
	int panel_lpm_mode = 0, lpm_bl_level = 0;

	if (IS_ERR_OR_NULL(ctrl))
		return;

	/*
	 * if the mode value is lower than MAX_LPM_MODE
	 * this function was not called by mdss_samsung_alpm_store()
	 * so the mode will not be changed
	 */
	if (*mode < MAX_LPM_MODE)
		return;

	vdd = check_valid_ctrl(ctrl);

	/* default Hz is 30Hz */
	vdd->panel_lpm.hz = TX_LPM_30HZ;

	/* Check mode and bl_level */
	switch (*mode) {
		case AOD_MODE_ALPM_2NIT_ON:
			panel_lpm_mode = ALPM_MODE_ON;
			lpm_bl_level = PANEL_LPM_2NIT;
			break;
		case AOD_MODE_HLPM_2NIT_ON:
			panel_lpm_mode = HLPM_MODE_ON;
			lpm_bl_level = PANEL_LPM_2NIT;
			break;
		case AOD_MODE_ALPM_60NIT_ON:
			panel_lpm_mode = ALPM_MODE_ON;
			lpm_bl_level = PANEL_LPM_60NIT;
			break;
		case AOD_MODE_HLPM_60NIT_ON:
			panel_lpm_mode = HLPM_MODE_ON;
			lpm_bl_level = PANEL_LPM_60NIT;
			break;
		default:
			panel_lpm_mode = MODE_OFF;
			break;
	}

	*mode = panel_lpm_mode;

	/* Save mode and bl_level */
	vdd->panel_lpm.lpm_bl_level = lpm_bl_level;

	mdss_update_panel_lpm_cmds(ctrl, lpm_bl_level, panel_lpm_mode);
}


static void dsi_update_mdnie_data_dsi0(void)
{
	/* Update mdnie command */
	mdnie_data.DSI0_COLOR_BLIND_MDNIE_1 = DSI0_COLOR_BLIND_MDNIE_1;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE_1 = DSI0_RGB_SENSOR_MDNIE_1;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE_2 = DSI0_RGB_SENSOR_MDNIE_2;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE_3 = DSI0_RGB_SENSOR_MDNIE_3;
	mdnie_data.DSI0_TRANS_DIMMING_MDNIE = DSI0_RGB_SENSOR_MDNIE_3;
	mdnie_data.DSI0_UI_DYNAMIC_MDNIE_2 = DSI0_UI_DYNAMIC_MDNIE_2;
	mdnie_data.DSI0_UI_STANDARD_MDNIE_2 = DSI0_UI_STANDARD_MDNIE_2;
	mdnie_data.DSI0_UI_AUTO_MDNIE_2 = DSI0_UI_AUTO_MDNIE_2;
	mdnie_data.DSI0_VIDEO_DYNAMIC_MDNIE_2 = DSI0_VIDEO_DYNAMIC_MDNIE_2;
	mdnie_data.DSI0_VIDEO_STANDARD_MDNIE_2 = DSI0_VIDEO_STANDARD_MDNIE_2;
	mdnie_data.DSI0_VIDEO_AUTO_MDNIE_2 = DSI0_VIDEO_AUTO_MDNIE_2;
	mdnie_data.DSI0_CAMERA_MDNIE_2 = DSI0_CAMERA_MDNIE_2;
	mdnie_data.DSI0_CAMERA_AUTO_MDNIE_2 = DSI0_CAMERA_AUTO_MDNIE_2;
	mdnie_data.DSI0_GALLERY_DYNAMIC_MDNIE_2 = DSI0_GALLERY_DYNAMIC_MDNIE_2;
	mdnie_data.DSI0_GALLERY_STANDARD_MDNIE_2 = DSI0_GALLERY_STANDARD_MDNIE_2;
	mdnie_data.DSI0_GALLERY_AUTO_MDNIE_2 = DSI0_GALLERY_AUTO_MDNIE_2;
	mdnie_data.DSI0_VT_DYNAMIC_MDNIE_2 = DSI0_VT_DYNAMIC_MDNIE_2;
	mdnie_data.DSI0_VT_STANDARD_MDNIE_2 = DSI0_VT_STANDARD_MDNIE_2;
	mdnie_data.DSI0_VT_AUTO_MDNIE_2 = DSI0_VT_AUTO_MDNIE_2;
	mdnie_data.DSI0_BROWSER_DYNAMIC_MDNIE_2 = DSI0_BROWSER_DYNAMIC_MDNIE_2;
	mdnie_data.DSI0_BROWSER_STANDARD_MDNIE_2 = DSI0_BROWSER_STANDARD_MDNIE_2;
	mdnie_data.DSI0_BROWSER_AUTO_MDNIE_2 = DSI0_BROWSER_AUTO_MDNIE_2;
	mdnie_data.DSI0_EBOOK_DYNAMIC_MDNIE_2 = DSI0_EBOOK_DYNAMIC_MDNIE_2;
	mdnie_data.DSI0_EBOOK_STANDARD_MDNIE_2 = DSI0_EBOOK_STANDARD_MDNIE_2;
	mdnie_data.DSI0_EBOOK_AUTO_MDNIE_2 = DSI0_EBOOK_AUTO_MDNIE_2;
	mdnie_data.DSI0_TDMB_DYNAMIC_MDNIE_2 = DSI0_TDMB_DYNAMIC_MDNIE_2;
	mdnie_data.DSI0_TDMB_STANDARD_MDNIE_2 = DSI0_TDMB_STANDARD_MDNIE_2;
	mdnie_data.DSI0_TDMB_AUTO_MDNIE_2 = DSI0_TDMB_AUTO_MDNIE_2;

	mdnie_data.DSI0_BYPASS_MDNIE = DSI0_BYPASS_MDNIE;
	mdnie_data.DSI0_NEGATIVE_MDNIE = DSI0_NEGATIVE_MDNIE;
	mdnie_data.DSI0_COLOR_BLIND_MDNIE = DSI0_COLOR_BLIND_MDNIE;
	mdnie_data.DSI0_HBM_CE_MDNIE = DSI0_HBM_CE_MDNIE;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE = DSI0_RGB_SENSOR_MDNIE;
	mdnie_data.DSI0_UI_DYNAMIC_MDNIE = DSI0_UI_DYNAMIC_MDNIE;
	mdnie_data.DSI0_UI_STANDARD_MDNIE = DSI0_UI_STANDARD_MDNIE;
	mdnie_data.DSI0_UI_NATURAL_MDNIE = DSI0_UI_NATURAL_MDNIE;
	mdnie_data.DSI0_UI_MOVIE_MDNIE = DSI0_UI_MOVIE_MDNIE;
	mdnie_data.DSI0_UI_AUTO_MDNIE = DSI0_UI_AUTO_MDNIE;
	mdnie_data.DSI0_VIDEO_OUTDOOR_MDNIE = NULL;
	mdnie_data.DSI0_VIDEO_DYNAMIC_MDNIE = DSI0_VIDEO_DYNAMIC_MDNIE;
	mdnie_data.DSI0_VIDEO_STANDARD_MDNIE = DSI0_VIDEO_STANDARD_MDNIE;
	mdnie_data.DSI0_VIDEO_NATURAL_MDNIE = DSI0_VIDEO_NATURAL_MDNIE;
	mdnie_data.DSI0_VIDEO_MOVIE_MDNIE = DSI0_VIDEO_MOVIE_MDNIE;
	mdnie_data.DSI0_VIDEO_AUTO_MDNIE = DSI0_VIDEO_AUTO_MDNIE;
	mdnie_data.DSI0_VIDEO_WARM_OUTDOOR_MDNIE = DSI0_VIDEO_WARM_OUTDOOR_MDNIE;
	mdnie_data.DSI0_VIDEO_WARM_MDNIE = DSI0_VIDEO_WARM_MDNIE;
	mdnie_data.DSI0_VIDEO_COLD_OUTDOOR_MDNIE = DSI0_VIDEO_COLD_OUTDOOR_MDNIE;
	mdnie_data.DSI0_VIDEO_COLD_MDNIE = DSI0_VIDEO_COLD_MDNIE;
	mdnie_data.DSI0_CAMERA_OUTDOOR_MDNIE = DSI0_CAMERA_OUTDOOR_MDNIE;
	mdnie_data.DSI0_CAMERA_MDNIE = DSI0_CAMERA_MDNIE;
	mdnie_data.DSI0_CAMERA_AUTO_MDNIE = DSI0_CAMERA_AUTO_MDNIE;
	mdnie_data.DSI0_GALLERY_DYNAMIC_MDNIE = DSI0_GALLERY_DYNAMIC_MDNIE;
	mdnie_data.DSI0_GALLERY_STANDARD_MDNIE = DSI0_GALLERY_STANDARD_MDNIE;
	mdnie_data.DSI0_GALLERY_NATURAL_MDNIE = DSI0_GALLERY_NATURAL_MDNIE;
	mdnie_data.DSI0_GALLERY_MOVIE_MDNIE = DSI0_GALLERY_MOVIE_MDNIE;
	mdnie_data.DSI0_GALLERY_AUTO_MDNIE = DSI0_GALLERY_AUTO_MDNIE;
	mdnie_data.DSI0_VT_DYNAMIC_MDNIE = DSI0_VT_DYNAMIC_MDNIE;
	mdnie_data.DSI0_VT_STANDARD_MDNIE = DSI0_VT_STANDARD_MDNIE;
	mdnie_data.DSI0_VT_NATURAL_MDNIE = DSI0_VT_NATURAL_MDNIE;
	mdnie_data.DSI0_VT_MOVIE_MDNIE = DSI0_VT_MOVIE_MDNIE;
	mdnie_data.DSI0_VT_AUTO_MDNIE = DSI0_VT_AUTO_MDNIE;
	mdnie_data.DSI0_BROWSER_DYNAMIC_MDNIE = DSI0_BROWSER_DYNAMIC_MDNIE;
	mdnie_data.DSI0_BROWSER_STANDARD_MDNIE = DSI0_BROWSER_STANDARD_MDNIE;
	mdnie_data.DSI0_BROWSER_NATURAL_MDNIE = DSI0_BROWSER_NATURAL_MDNIE;
	mdnie_data.DSI0_BROWSER_MOVIE_MDNIE = DSI0_BROWSER_MOVIE_MDNIE;
	mdnie_data.DSI0_BROWSER_AUTO_MDNIE = DSI0_BROWSER_AUTO_MDNIE;
	mdnie_data.DSI0_EBOOK_DYNAMIC_MDNIE = DSI0_EBOOK_DYNAMIC_MDNIE;
	mdnie_data.DSI0_EBOOK_STANDARD_MDNIE = DSI0_EBOOK_STANDARD_MDNIE;
	mdnie_data.DSI0_EBOOK_NATURAL_MDNIE = DSI0_EBOOK_NATURAL_MDNIE;
	mdnie_data.DSI0_EBOOK_MOVIE_MDNIE = DSI0_EBOOK_MOVIE_MDNIE;
	mdnie_data.DSI0_EBOOK_AUTO_MDNIE = DSI0_EBOOK_AUTO_MDNIE;
	mdnie_data.DSI0_EMAIL_AUTO_MDNIE = DSI0_EMAIL_AUTO_MDNIE;
	mdnie_data.DSI0_GAME_LOW_MDNIE = DSI0_GAME_LOW_MDNIE;
	mdnie_data.DSI0_GAME_MID_MDNIE = DSI0_GAME_MID_MDNIE;
	mdnie_data.DSI0_GAME_HIGH_MDNIE = DSI0_GAME_HIGH_MDNIE;
	mdnie_data.DSI0_TDMB_DYNAMIC_MDNIE = DSI0_TDMB_DYNAMIC_MDNIE;
	mdnie_data.DSI0_TDMB_STANDARD_MDNIE = DSI0_TDMB_STANDARD_MDNIE;
	mdnie_data.DSI0_TDMB_NATURAL_MDNIE = DSI0_TDMB_NATURAL_MDNIE;
	mdnie_data.DSI0_TDMB_MOVIE_MDNIE = DSI0_TDMB_MOVIE_MDNIE;
	mdnie_data.DSI0_TDMB_AUTO_MDNIE = DSI0_TDMB_AUTO_MDNIE;
	mdnie_data.DSI0_GRAYSCALE_MDNIE = DSI0_GRAYSCALE_MDNIE;
	mdnie_data.DSI0_GRAYSCALE_NEGATIVE_MDNIE= DSI0_GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data.DSI0_CURTAIN = DSI0_SCREEN_CURTAIN_MDNIE;
	mdnie_data.DSI0_NIGHT_MODE_MDNIE = DSI0_NIGHT_MODE_MDNIE;
	mdnie_data.DSI0_NIGHT_MODE_MDNIE_1 = DSI0_NIGHT_MODE_MDNIE_1;
	mdnie_data.DSI0_COLOR_BLIND_MDNIE_SCR = DSI0_COLOR_BLIND_MDNIE_1;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE_SCR = DSI0_RGB_SENSOR_MDNIE_1;

	mdnie_data.mdnie_tune_value_dsi0 = mdnie_tune_value_dsi0;
	mdnie_data.hmt_color_temperature_tune_value_dsi0 = hmt_color_temperature_tune_value_dsi0;
	mdnie_data.hdr_tune_value_dsi0 = hdr_tune_value_dsi0;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data.dsi0_bypass_mdnie_size = ARRAY_SIZE(DSI0_BYPASS_MDNIE);

	mdnie_data.dsi0_rgb_sensor_mdnie_1_size = DSI0_RGB_SENSOR_MDNIE_1_SIZE;
	mdnie_data.dsi0_rgb_sensor_mdnie_2_size = DSI0_RGB_SENSOR_MDNIE_2_SIZE;
	mdnie_data.dsi0_rgb_sensor_mdnie_3_size = DSI0_RGB_SENSOR_MDNIE_3_SIZE;
	mdnie_data.dsi0_trans_dimming_data_index = MDNIE_TRANS_DIMMING_DATA_INDEX;

	mdnie_data.dsi0_adjust_ldu_table = adjust_ldu_data;
	mdnie_data.dsi0_max_adjust_ldu = 6;
	mdnie_data.dsi0_night_mode_table = night_mode_data;
	mdnie_data.dsi0_max_night_mode_index = 12;
	mdnie_data.dsi0_scr_step_index = MDNIE_STEP1_INDEX;

	mdnie_data.dsi0_white_default_r = 0xff;
	mdnie_data.dsi0_white_default_g = 0xff;
	mdnie_data.dsi0_white_default_b = 0xff;
	mdnie_data.dsi0_white_balanced_r = 0;
	mdnie_data.dsi0_white_balanced_g = 0;
	mdnie_data.dsi0_white_balanced_b = 0;
}


static void dsi_update_mdnie_data_dsi1(void)
{
	/* Update mdnie command */
	mdnie_data.DSI1_COLOR_BLIND_MDNIE_1 = DSI0_COLOR_BLIND_MDNIE_1;
	mdnie_data.DSI1_RGB_SENSOR_MDNIE_1 = DSI0_RGB_SENSOR_MDNIE_1;
	mdnie_data.DSI1_RGB_SENSOR_MDNIE_2 = DSI0_RGB_SENSOR_MDNIE_2;
	mdnie_data.DSI1_RGB_SENSOR_MDNIE_3 = DSI0_RGB_SENSOR_MDNIE_3;
	mdnie_data.DSI1_TRANS_DIMMING_MDNIE = DSI0_RGB_SENSOR_MDNIE_3;
	mdnie_data.DSI1_UI_DYNAMIC_MDNIE_2 = DSI0_UI_DYNAMIC_MDNIE_2;
	mdnie_data.DSI1_UI_STANDARD_MDNIE_2 = DSI0_UI_STANDARD_MDNIE_2;
	mdnie_data.DSI1_UI_AUTO_MDNIE_2 = DSI0_UI_AUTO_MDNIE_2;
	mdnie_data.DSI1_VIDEO_DYNAMIC_MDNIE_2 = DSI0_VIDEO_DYNAMIC_MDNIE_2;
	mdnie_data.DSI1_VIDEO_STANDARD_MDNIE_2 = DSI0_VIDEO_STANDARD_MDNIE_2;
	mdnie_data.DSI1_VIDEO_AUTO_MDNIE_2 = DSI0_VIDEO_AUTO_MDNIE_2;
	mdnie_data.DSI1_CAMERA_MDNIE_2 = DSI0_CAMERA_MDNIE_2;
	mdnie_data.DSI1_CAMERA_AUTO_MDNIE_2 = DSI0_CAMERA_AUTO_MDNIE_2;
	mdnie_data.DSI1_GALLERY_DYNAMIC_MDNIE_2 = DSI0_GALLERY_DYNAMIC_MDNIE_2;
	mdnie_data.DSI1_GALLERY_STANDARD_MDNIE_2 = DSI0_GALLERY_STANDARD_MDNIE_2;
	mdnie_data.DSI1_GALLERY_AUTO_MDNIE_2 = DSI0_GALLERY_AUTO_MDNIE_2;
	mdnie_data.DSI1_VT_DYNAMIC_MDNIE_2 = DSI0_VT_DYNAMIC_MDNIE_2;
	mdnie_data.DSI1_VT_STANDARD_MDNIE_2 = DSI0_VT_STANDARD_MDNIE_2;
	mdnie_data.DSI1_VT_AUTO_MDNIE_2 = DSI0_VT_AUTO_MDNIE_2;
	mdnie_data.DSI1_BROWSER_DYNAMIC_MDNIE_2 = DSI0_BROWSER_DYNAMIC_MDNIE_2;
	mdnie_data.DSI1_BROWSER_STANDARD_MDNIE_2 = DSI0_BROWSER_STANDARD_MDNIE_2;
	mdnie_data.DSI1_BROWSER_AUTO_MDNIE_2 = DSI0_BROWSER_AUTO_MDNIE_2;
	mdnie_data.DSI1_EBOOK_DYNAMIC_MDNIE_2 = DSI0_EBOOK_DYNAMIC_MDNIE_2;
	mdnie_data.DSI1_EBOOK_STANDARD_MDNIE_2 = DSI0_EBOOK_STANDARD_MDNIE_2;
	mdnie_data.DSI1_EBOOK_AUTO_MDNIE_2 = DSI0_EBOOK_AUTO_MDNIE_2;
	mdnie_data.DSI1_TDMB_DYNAMIC_MDNIE_2 = DSI0_TDMB_DYNAMIC_MDNIE_2;
	mdnie_data.DSI1_TDMB_STANDARD_MDNIE_2 = DSI0_TDMB_STANDARD_MDNIE_2;
	mdnie_data.DSI1_TDMB_AUTO_MDNIE_2 = DSI0_TDMB_AUTO_MDNIE_2;

	mdnie_data.DSI1_BYPASS_MDNIE = DSI0_BYPASS_MDNIE;
	mdnie_data.DSI1_NEGATIVE_MDNIE = DSI0_NEGATIVE_MDNIE;
	mdnie_data.DSI1_COLOR_BLIND_MDNIE = DSI0_COLOR_BLIND_MDNIE;
	mdnie_data.DSI1_HBM_CE_MDNIE = DSI0_HBM_CE_MDNIE;
	mdnie_data.DSI1_RGB_SENSOR_MDNIE = DSI0_RGB_SENSOR_MDNIE;
	mdnie_data.DSI1_UI_DYNAMIC_MDNIE = DSI0_UI_DYNAMIC_MDNIE;
	mdnie_data.DSI1_UI_STANDARD_MDNIE = DSI0_UI_STANDARD_MDNIE;
	mdnie_data.DSI1_UI_NATURAL_MDNIE = DSI0_UI_NATURAL_MDNIE;
	mdnie_data.DSI1_UI_MOVIE_MDNIE = DSI0_UI_MOVIE_MDNIE;
	mdnie_data.DSI1_UI_AUTO_MDNIE = DSI0_UI_AUTO_MDNIE;
	mdnie_data.DSI1_VIDEO_OUTDOOR_MDNIE = NULL;
	mdnie_data.DSI1_VIDEO_DYNAMIC_MDNIE = DSI0_VIDEO_DYNAMIC_MDNIE;
	mdnie_data.DSI1_VIDEO_STANDARD_MDNIE = DSI0_VIDEO_STANDARD_MDNIE;
	mdnie_data.DSI1_VIDEO_NATURAL_MDNIE = DSI0_VIDEO_NATURAL_MDNIE;
	mdnie_data.DSI1_VIDEO_MOVIE_MDNIE = DSI0_VIDEO_MOVIE_MDNIE;
	mdnie_data.DSI1_VIDEO_AUTO_MDNIE = DSI0_VIDEO_AUTO_MDNIE;
	mdnie_data.DSI1_VIDEO_WARM_OUTDOOR_MDNIE = DSI0_VIDEO_WARM_OUTDOOR_MDNIE;
	mdnie_data.DSI1_VIDEO_WARM_MDNIE = DSI0_VIDEO_WARM_MDNIE;
	mdnie_data.DSI1_VIDEO_COLD_OUTDOOR_MDNIE = DSI0_VIDEO_COLD_OUTDOOR_MDNIE;
	mdnie_data.DSI1_VIDEO_COLD_MDNIE = DSI0_VIDEO_COLD_MDNIE;
	mdnie_data.DSI1_CAMERA_OUTDOOR_MDNIE = DSI0_CAMERA_OUTDOOR_MDNIE;
	mdnie_data.DSI1_CAMERA_MDNIE = DSI0_CAMERA_MDNIE;
	mdnie_data.DSI1_CAMERA_AUTO_MDNIE = DSI0_CAMERA_AUTO_MDNIE;
	mdnie_data.DSI1_GALLERY_DYNAMIC_MDNIE = DSI0_GALLERY_DYNAMIC_MDNIE;
	mdnie_data.DSI1_GALLERY_STANDARD_MDNIE = DSI0_GALLERY_STANDARD_MDNIE;
	mdnie_data.DSI1_GALLERY_NATURAL_MDNIE = DSI0_GALLERY_NATURAL_MDNIE;
	mdnie_data.DSI1_GALLERY_MOVIE_MDNIE = DSI0_GALLERY_MOVIE_MDNIE;
	mdnie_data.DSI1_GALLERY_AUTO_MDNIE = DSI0_GALLERY_AUTO_MDNIE;
	mdnie_data.DSI1_VT_DYNAMIC_MDNIE = DSI0_VT_DYNAMIC_MDNIE;
	mdnie_data.DSI1_VT_STANDARD_MDNIE = DSI0_VT_STANDARD_MDNIE;
	mdnie_data.DSI1_VT_NATURAL_MDNIE = DSI0_VT_NATURAL_MDNIE;
	mdnie_data.DSI1_VT_MOVIE_MDNIE = DSI0_VT_MOVIE_MDNIE;
	mdnie_data.DSI1_VT_AUTO_MDNIE = DSI0_VT_AUTO_MDNIE;
	mdnie_data.DSI1_BROWSER_DYNAMIC_MDNIE = DSI0_BROWSER_DYNAMIC_MDNIE;
	mdnie_data.DSI1_BROWSER_STANDARD_MDNIE = DSI0_BROWSER_STANDARD_MDNIE;
	mdnie_data.DSI1_BROWSER_NATURAL_MDNIE = DSI0_BROWSER_NATURAL_MDNIE;
	mdnie_data.DSI1_BROWSER_MOVIE_MDNIE = DSI0_BROWSER_MOVIE_MDNIE;
	mdnie_data.DSI1_BROWSER_AUTO_MDNIE = DSI0_BROWSER_AUTO_MDNIE;
	mdnie_data.DSI1_EBOOK_DYNAMIC_MDNIE = DSI0_EBOOK_DYNAMIC_MDNIE;
	mdnie_data.DSI1_EBOOK_STANDARD_MDNIE = DSI0_EBOOK_STANDARD_MDNIE;
	mdnie_data.DSI1_EBOOK_NATURAL_MDNIE = DSI0_EBOOK_NATURAL_MDNIE;
	mdnie_data.DSI1_EBOOK_MOVIE_MDNIE = DSI0_EBOOK_MOVIE_MDNIE;
	mdnie_data.DSI1_EBOOK_AUTO_MDNIE = DSI0_EBOOK_AUTO_MDNIE;
	mdnie_data.DSI1_EMAIL_AUTO_MDNIE = DSI0_EMAIL_AUTO_MDNIE;
	mdnie_data.DSI1_GAME_LOW_MDNIE = DSI0_GAME_LOW_MDNIE;
	mdnie_data.DSI1_GAME_MID_MDNIE = DSI0_GAME_MID_MDNIE;
	mdnie_data.DSI1_GAME_HIGH_MDNIE = DSI0_GAME_HIGH_MDNIE;
	mdnie_data.DSI1_TDMB_DYNAMIC_MDNIE = DSI0_TDMB_DYNAMIC_MDNIE;
	mdnie_data.DSI1_TDMB_STANDARD_MDNIE = DSI0_TDMB_STANDARD_MDNIE;
	mdnie_data.DSI1_TDMB_NATURAL_MDNIE = DSI0_TDMB_NATURAL_MDNIE;
	mdnie_data.DSI1_TDMB_MOVIE_MDNIE = DSI0_TDMB_MOVIE_MDNIE;
	mdnie_data.DSI1_TDMB_AUTO_MDNIE = DSI0_TDMB_AUTO_MDNIE;
	mdnie_data.DSI1_GRAYSCALE_MDNIE = DSI0_GRAYSCALE_MDNIE;
	mdnie_data.DSI1_GRAYSCALE_NEGATIVE_MDNIE= DSI0_GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data.DSI1_CURTAIN = DSI0_SCREEN_CURTAIN_MDNIE;
	mdnie_data.DSI1_NIGHT_MODE_MDNIE = DSI0_NIGHT_MODE_MDNIE;
	mdnie_data.DSI1_NIGHT_MODE_MDNIE_1 = DSI0_NIGHT_MODE_MDNIE_1;
	mdnie_data.DSI1_COLOR_BLIND_MDNIE_SCR = DSI0_COLOR_BLIND_MDNIE_1;
	mdnie_data.DSI1_RGB_SENSOR_MDNIE_SCR = DSI0_RGB_SENSOR_MDNIE_1;

	mdnie_data.mdnie_tune_value_dsi1 = mdnie_tune_value_dsi0;
	mdnie_data.hmt_color_temperature_tune_value_dsi1 = hmt_color_temperature_tune_value_dsi0;
	mdnie_data.hdr_tune_value_dsi1 = hdr_tune_value_dsi0;
	
	mdnie_data.light_notification_tune_value_dsi0 = light_notification_tune_value_dsi0;
	mdnie_data.light_notification_tune_value_dsi1 = light_notification_tune_value_dsi0;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data.dsi1_bypass_mdnie_size = ARRAY_SIZE(DSI0_BYPASS_MDNIE);

	mdnie_data.dsi1_rgb_sensor_mdnie_1_size = DSI0_RGB_SENSOR_MDNIE_1_SIZE;
	mdnie_data.dsi1_rgb_sensor_mdnie_2_size = DSI0_RGB_SENSOR_MDNIE_2_SIZE;
	mdnie_data.dsi1_rgb_sensor_mdnie_3_size = DSI0_RGB_SENSOR_MDNIE_3_SIZE;
	mdnie_data.dsi1_trans_dimming_data_index = MDNIE_TRANS_DIMMING_DATA_INDEX;

	mdnie_data.dsi1_adjust_ldu_table = adjust_ldu_data;
	mdnie_data.dsi1_max_adjust_ldu = 6;
	mdnie_data.dsi1_night_mode_table = night_mode_data;
	mdnie_data.dsi1_max_night_mode_index = 12;
	mdnie_data.dsi1_scr_step_index = MDNIE_STEP1_INDEX;

	mdnie_data.dsi1_white_default_r = 0xff;
	mdnie_data.dsi1_white_default_g = 0xff;
	mdnie_data.dsi1_white_default_b = 0xff;
	mdnie_data.dsi1_white_balanced_r = 0;
	mdnie_data.dsi1_white_balanced_g = 0;
	mdnie_data.dsi1_white_balanced_b = 0;
}


static void dsi_update_mdnie_data(void)
{
	dsi_update_mdnie_data_dsi0();
	dsi_update_mdnie_data_dsi1();
	mdnie_data.mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET;
	mdnie_data.mdnie_step_index[MDNIE_STEP1] = MDNIE_STEP1_INDEX;
	mdnie_data.mdnie_step_index[MDNIE_STEP2] = MDNIE_STEP2_INDEX;
	mdnie_data.mdnie_step_index[MDNIE_STEP3] = MDNIE_STEP3_INDEX;
	mdnie_data.address_scr_white[ADDRESS_SCR_WHITE_RED_OFFSET] = ADDRESS_SCR_WHITE_RED;
	mdnie_data.address_scr_white[ADDRESS_SCR_WHITE_GREEN_OFFSET] = ADDRESS_SCR_WHITE_GREEN;
	mdnie_data.address_scr_white[ADDRESS_SCR_WHITE_BLUE_OFFSET] = ADDRESS_SCR_WHITE_BLUE;
}

static void mdss_panel_init(struct samsung_display_driver_data *vdd)
{
	pr_info("%s", __func__);

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = mdss_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = mdss_panel_on_post;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = mdss_panel_revision;
	vdd->panel_func.samsung_manufacture_date_read = mdss_manufacture_date_read;
	vdd->panel_func.samsung_ddi_id_read = mdss_ddi_id_read;
	vdd->panel_func.samsung_cell_id_read = mdss_cell_id_read;
	vdd->panel_func.samsung_octa_id_read = mdss_octa_id_read;
	vdd->panel_func.samsung_elvss_read = mdss_elvss_read;
	vdd->panel_func.samsung_hbm_read = mdss_hbm_read;
	vdd->panel_func.samsung_mdnie_read = mdss_mdnie_read;
	vdd->panel_func.samsung_smart_dimming_init = mdss_smart_dimming_init;
	vdd->panel_func.samsung_smart_get_conf = smart_get_conf_S6E3FA5_AMS420QW01;

	/* Brightness */
	vdd->panel_func.samsung_brightness_aid = mdss_aid;
	vdd->panel_func.samsung_brightness_acl_on = mdss_acl_on;
	vdd->panel_func.samsung_brightness_acl_percent = NULL;
	vdd->panel_func.samsung_brightness_acl_off = mdss_acl_off;
	vdd->panel_func.samsung_brightness_elvss = mdss_elvss;
	vdd->panel_func.samsung_brightness_elvss_temperature1 = NULL;
	vdd->panel_func.samsung_brightness_elvss_temperature2 = NULL;
	vdd->panel_func.samsung_brightness_vint = mdss_vint;
	vdd->panel_func.samsung_brightness_gamma = mdss_gamma;

	/* HBM */
	vdd->panel_func.samsung_hbm_gamma = mdss_hbm_gamma;
	vdd->panel_func.samsung_hbm_etc = mdss_hbm_etc;
	vdd->panel_func.samsung_brightness_hbm_off = mdss_hbm_off;
	vdd->panel_func.get_hbm_candela_value = get_hbm_candela_value;

	/* MDNIE */
	vdd->support_mdnie_lite = true;
	vdd->support_mdnie_trans_dimming = true;
	vdd->mdnie_tune_size1 = sizeof(DSI0_BYPASS_MDNIE_1);
	vdd->mdnie_tune_size2 = sizeof(DSI0_BYPASS_MDNIE_2);
	vdd->mdnie_tune_size3 = sizeof(DSI0_BYPASS_MDNIE_3);
	dsi_update_mdnie_data();


	/* Panel LPM */
	vdd->panel_func.samsung_get_panel_lpm_mode = mdss_get_panel_lpm_mode;

	/* default brightness */
	vdd->bl_level = 255;

	/* send recovery pck before sending image date (for ESD recovery) */
	vdd->send_esd_recovery = false;

	vdd->auto_brightness_level = 12;

	/* Enable panic on first pingpong timeout */
	vdd->debug_data->panic_on_pptimeout = true;

	/* Set IRC init value */
	vdd->irc_info.irc_enable_status = false;
	vdd->irc_info.irc_mode = IRC_MODERATO_MODE;

	/* COLOR WEAKNESS */
	vdd->panel_func.color_weakness_ccb_on_off = NULL;

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = NULL;

	/* MULTI_RESOLUTION */
	vdd->panel_func.samsung_multires_start = NULL;
	vdd->panel_func.samsung_multires_end = NULL;

	/* ACL default ON */
	vdd->acl_status = 1;
}

static int __init samsung_panel_init(void)
{
	struct samsung_display_driver_data *vdd = samsung_get_vdd();
	char panel_string[] = "ss_dsi_panel_S6E3FA5_AMS420QW01_FHD";

	vdd->panel_name = mdss_mdp_panel + 8;

	pr_info("%s : %s\n", __func__, panel_string);

	if (!strncmp(vdd->panel_name, panel_string, strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = mdss_panel_init;
	return 0;
}
early_initcall(samsung_panel_init);
