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
#include "ss_dsi_panel_S6E8AA5X01_AMS604RM01.h"
#include "ss_dsi_mdnie_S6E8AA5X01_AMS604RM01.h"

/* The register to control the percentage for ACL */
#define ACL_CONTROL_REG	0xB5
/* The register to enable/disable ACL */
#define ACL_ENABLE_REG	0x55
/* The register for elvss */
#define ELVSS_REG	0xB6

static int mdss_panel_on_pre(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	LCD_INFO("+: ndx=%d \n", ctrl->ndx);
	mdss_panel_attach_set(ctrl, true);
	LCD_INFO("-: ndx=%d \n", ctrl->ndx);
	return true;
}

extern int poweroff_charging;

static int mdss_panel_on_post(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0]);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	pr_info("%s %d\n", __func__, ndx);

	if(poweroff_charging) {
		pr_info("%s LPM Mode Enable, Add More Delay\n", __func__);
		msleep(300);
	}

	return true;
}

static char mdss_panel_revision(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0]);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	if (vdd->manufacture_id_dsi[ndx] == PBA_ID)
		mdss_panel_attach_set(ctrl, false);
	else
		mdss_panel_attach_set(ctrl, true);

	vdd->aid_subdivision_enable = true;

	switch (mdss_panel_rev_get(ctrl)) {
	default:
		vdd->panel_revision = 'A';
		LCD_ERR("Invalid panel_rev(default rev : %c) rev %x\n",
				vdd->panel_revision, mdss_panel_rev_get(ctrl));
		break;
	}

	vdd->panel_revision -= 'A';

	LCD_INFO_ONCE("panel_revision = %c %d \n",
					vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

static int mdss_manufacture_date_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	unsigned char date[2];
	int year, month, day;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (C8h 41,42th) for manufacture date */
	if (get_panel_rx_cmds(ctrl, RX_MANUFACTURE_DATE)->cmd_cnt) {
		mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_MANUFACTURE_DATE),
			date, LEVEL1_KEY);

		year = date[0] & 0xf0;
		year >>= 4;
		year += 2011; /* 0 = 2011 year*/
		month = date[0] & 0x0f;
		day = date[1] & 0x1f;

		LCD_ERR("manufacture_date DSI%d = (%d%04d) - year(%d) month(%d) day(%d)\n",
			ctrl->ndx, vdd->manufacture_date_dsi[ctrl->ndx], vdd->manufacture_time_dsi[ctrl->ndx],
			year, month, day);
		vdd->manufacture_date_dsi[ctrl->ndx]  =   year * 10000 + month * 100 + day;
	} else {
		LCD_ERR("DSI%d no manufacture_date_rx_cmds cmds(%d)", ctrl->ndx, vdd->panel_revision);
		return false;
	}

	return true;
}

static int mdss_ddi_id_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char ddi_id[5];
	int loop;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (D6h 1~5th) for ddi id */
	if (get_panel_rx_cmds(ctrl, RX_DDI_ID)->cmd_cnt) {
		mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_DDI_ID),
			ddi_id, LEVEL1_KEY);

		for(loop = 0; loop < 5; loop++)
			vdd->ddi_id_dsi[ctrl->ndx][loop] = ddi_id[loop];

		LCD_INFO("DSI%d : %02x %02x %02x %02x %02x\n", ctrl->ndx,
			vdd->ddi_id_dsi[ctrl->ndx][0], vdd->ddi_id_dsi[ctrl->ndx][1],
			vdd->ddi_id_dsi[ctrl->ndx][2], vdd->ddi_id_dsi[ctrl->ndx][3],
			vdd->ddi_id_dsi[ctrl->ndx][4]);
	} else {
		LCD_ERR("DSI%d no ddi_id_rx_cmds cmds", ctrl->ndx);
		return false;
	}

	return true;
}

static int mdss_cell_id_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char cell_id_buffer[MAX_CELL_ID] = {0,};
	int loop;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read Panel Unique Cell ID (C8h 41~51th) */
	if (get_panel_rx_cmds(ctrl, RX_CELL_ID)->cmd_cnt) {
		memset(cell_id_buffer, 0x00, MAX_CELL_ID);

		mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_CELL_ID),
			cell_id_buffer, LEVEL1_KEY);

		for(loop = 0; loop < MAX_CELL_ID; loop++)
			vdd->cell_id_dsi[ctrl->ndx][loop] = cell_id_buffer[loop];

		LCD_INFO("DSI%d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			ctrl->ndx, vdd->cell_id_dsi[ctrl->ndx][0],
			vdd->cell_id_dsi[ctrl->ndx][1],	vdd->cell_id_dsi[ctrl->ndx][2],
			vdd->cell_id_dsi[ctrl->ndx][3],	vdd->cell_id_dsi[ctrl->ndx][4],
			vdd->cell_id_dsi[ctrl->ndx][5],	vdd->cell_id_dsi[ctrl->ndx][6],
			vdd->cell_id_dsi[ctrl->ndx][7],	vdd->cell_id_dsi[ctrl->ndx][8],
			vdd->cell_id_dsi[ctrl->ndx][9],	vdd->cell_id_dsi[ctrl->ndx][10]);

	} else {
		LCD_ERR("DSI%d no cell_id_rx_cmds cmd\n", ctrl->ndx);
		return false;
	}

	return true;
}

static int mdss_octa_id_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char octa_id_buffer[MAX_OCTA_ID] = {0,};
	int loop;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read Panel Unique OCTA ID (D6h 1st~5th) */
	if (get_panel_rx_cmds(ctrl, RX_OCTA_ID)->cmd_cnt) {
		memset(octa_id_buffer, 0x00, MAX_OCTA_ID);

		mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_OCTA_ID),
			octa_id_buffer, LEVEL1_KEY);

		for (loop = 0; loop < MAX_OCTA_ID; loop++)
			vdd->octa_id_dsi[ctrl->ndx][loop] = octa_id_buffer[loop];

		LCD_INFO("DSI%d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			ctrl->ndx, vdd->octa_id_dsi[ctrl->ndx][0], vdd->octa_id_dsi[ctrl->ndx][1],
			vdd->octa_id_dsi[ctrl->ndx][2],	vdd->octa_id_dsi[ctrl->ndx][3],
			vdd->octa_id_dsi[ctrl->ndx][4],	vdd->octa_id_dsi[ctrl->ndx][5],
			vdd->octa_id_dsi[ctrl->ndx][6],	vdd->octa_id_dsi[ctrl->ndx][7],
			vdd->octa_id_dsi[ctrl->ndx][8],	vdd->octa_id_dsi[ctrl->ndx][9],
			vdd->octa_id_dsi[ctrl->ndx][10], vdd->octa_id_dsi[ctrl->ndx][11],
			vdd->octa_id_dsi[ctrl->ndx][12], vdd->octa_id_dsi[ctrl->ndx][13],
			vdd->octa_id_dsi[ctrl->ndx][14], vdd->octa_id_dsi[ctrl->ndx][15],
			vdd->octa_id_dsi[ctrl->ndx][16], vdd->octa_id_dsi[ctrl->ndx][17],
			vdd->octa_id_dsi[ctrl->ndx][18], vdd->octa_id_dsi[ctrl->ndx][19]);

	} else {
		LCD_ERR("DSI%d no octa_id_rx_cmds cmd\n", ctrl->ndx);
		return false;
	}

	return true;
}

static int mdss_elvss_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char elvss_b6[2];
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (B6h 22th,23th) for elvss*/
	mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_ELVSS),
		elvss_b6, LEVEL1_KEY);

	/*0xB6 22nd OTP value*/
	vdd->display_status_dsi[ctrl->ndx].elvss_value1 = elvss_b6[0];
	/*0xB6 23th OTP Value */
	vdd->display_status_dsi[ctrl->ndx].elvss_value2 = elvss_b6[1];

	return true;
}
static int mdss_hbm_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int i, j;
	char hbm_buffer1[31];
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *hbm_gamma_cmds = get_panel_tx_cmds(ctrl, TX_HBM_GAMMA);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_gamma_cmds)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx cmds : 0x%zx", (size_t)ctrl, (size_t)vdd, (size_t)hbm_gamma_cmds);
		return false;
	}

	/* Read mtp (B4h 1~33th) for hbm gamma */
	mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_HBM),
		hbm_buffer1, LEVEL1_KEY);

	/* V255 ~ V1 */
	for (i = 1, j = 1; i < 31; i++, j++)
		hbm_gamma_cmds->cmds[0].payload[i] = hbm_buffer1[j];

	/* VT RGB */
	hbm_gamma_cmds->cmds[0].payload[34] = 0;
	hbm_gamma_cmds->cmds[0].payload[34] = 0;
	hbm_gamma_cmds->cmds[0].payload[35] = 0;

	return true;
}

static int get_hbm_candela_value(int level)
{
	if (level == 13)
		return 500;
	else if (level == 6)
		return 378;
	else if (level == 7)
		return 395;
	else if (level == 8)
		return 413;
	else if (level == 9)
		return 430;
	else if (level == 10)
		return 338;
	else if (level == 11)
		return 465;
	else if (level == 12)
		return 483;
	else
		return 500;
}

static struct dsi_panel_cmds *mdss_hbm_gamma(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *hbm_gamma_cmds = get_panel_tx_cmds(ctrl, TX_HBM_GAMMA);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_gamma_cmds)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx cmd : 0x%zx", (size_t)ctrl, (size_t)vdd, (size_t)hbm_gamma_cmds);
		return NULL;
	}

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi[ctrl->ndx]->generate_hbm_gamma)) {
		LCD_ERR("generate_hbm_gamma is NULL error");
		return NULL;
	} else {
		vdd->smart_dimming_dsi[ctrl->ndx]->generate_hbm_gamma(
			vdd->smart_dimming_dsi[ctrl->ndx],
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
	struct dsi_panel_cmds *cmd_list[3];
	/*
	 * Init reg_list and cmd list
	 * reg_list[X][0] is reg value
	 * reg_list[X][1] is offset for reg value
	 * cmd_list is the target cmds for searching reg value
	 */
	static int reg_list[3][2] = {
		{ELVSS_REG, -EINVAL},
		{ACL_CONTROL_REG, -EINVAL},
		{ACL_ENABLE_REG, -EINVAL}};
	/* RB6h 2nd parameter */
	char hbm_elvss_offset[] = {
		0x10,	/* 378CD */
		0x0F,	/* 395CD */
		0x0D,	/* 413CD */
		0x0B,	/* 430CD */
		0x09,	/* 448CD */
		0x07,	/* 465CD */
		0x05,	/* 483CD */
		0x04,	/* 500CD */
	};

	cmd_list[0] = hbm_etc_cmds;
	cmd_list[1] = hbm_etc_cmds;
	cmd_list[2] = hbm_etc_cmds;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	mdss_init_panel_reg_offset(ctrl, reg_list, cmd_list,
			sizeof(cmd_list) / sizeof(cmd_list[0]));

	/* MPS/ELVSS : 0xB6 2rd val */
	hbm_etc_cmds->cmds[reg_list[0][1]].payload[2] = hbm_elvss_offset[vdd->auto_brightness - HBM_MODE];

	if (vdd->acl_status == 1) {  /*acl on : 8%*/
		hbm_etc_cmds->cmds[reg_list[1][1]].payload[1] = 0x50; /*B5 1st*/
		hbm_etc_cmds->cmds[reg_list[2][1]].payload[1] = 0x02; /*55 1st*/
	} else {  /*acl off*/
		hbm_etc_cmds->cmds[reg_list[1][1]].payload[1] = 0x40; /*B5 1st*/
		hbm_etc_cmds->cmds[reg_list[2][1]].payload[1] = 0x00; /*55 1st*/
	}
	/* Read B6h 23th para -> Write to B6h 22th para */
	hbm_etc_cmds->cmds[reg_list[0][1]].payload[22] = vdd->display_status_dsi[ctrl->ndx].elvss_value2;

	*level_key = LEVEL1_KEY;

	LCD_INFO("B5_3rd_PARA = 0x%x, TSET = 0x%x, 23th = 0x%x \n",
		hbm_etc_cmds->cmds[reg_list[1][1]].payload[3],
		hbm_etc_cmds->cmds[reg_list[1][1]].payload[1],
		hbm_etc_cmds->cmds[reg_list[0][1]].payload[22]);

	return hbm_etc_cmds;

}

#if 0
static struct dsi_panel_cmds *mdss_hbm_off(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = PANEL_LEVE2_KEY;

	pr_info("[MDSS] %s\n", __func__);
	return &vdd->dtsi_data[ctrl->ndx].hbm_off_tx_cmds[vdd->panel_revision];
}
#endif

#define COORDINATE_DATA_SIZE 6

#define F1(x,y) ((y)-((547*(x))/503)+31)
#define F2(x,y) ((y)-((467*(x))/447)-25)
#define F3(x,y) ((y)+((201*(x))/39)-18718)
#define F4(x,y) ((y)+((523*(x))/173)-12111)

/* Normal Mode */
static char coordinate_data_1[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xf9, 0x00, 0xf9, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfb, 0x00, 0xff, 0x00}, /* Tune_2 */
	{0xf9, 0x00, 0xf7, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfd, 0x00, 0xfa, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xf9, 0x00, 0xfb, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfc, 0x00, 0xff, 0x00, 0xf8, 0x00}, /* Tune_7 */
	{0xfb, 0x00, 0xff, 0x00, 0xfb, 0x00}, /* Tune_8 */
	{0xf9, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_9 */
};

/* sRGB/Adobe RGB Mode */
static char coordinate_data_2[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xf7, 0x00, 0xef, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xf7, 0x00, 0xef, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xf7, 0x00, 0xef, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xf7, 0x00, 0xef, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xf7, 0x00, 0xef, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xf7, 0x00, 0xef, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xf7, 0x00, 0xef, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xf7, 0x00, 0xef, 0x00}, /* Tune_8 */
	{0xff, 0x00, 0xf7, 0x00, 0xef, 0x00}, /* Tune_9 */
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

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (D6h 1~5th) for ddi id */
	if (get_panel_rx_cmds(ctrl, RX_MDNIE)->cmd_cnt) {
		mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_MDNIE),
			x_y_location, LEVEL1_KEY);

		vdd->mdnie_x[ctrl->ndx] = x_y_location[0] << 8 | x_y_location[1];	/* X */
		vdd->mdnie_y[ctrl->ndx] = x_y_location[2] << 8 | x_y_location[3];	/* Y */

		mdnie_tune_index = mdnie_coordinate_index(vdd->mdnie_x[ctrl->ndx], vdd->mdnie_y[ctrl->ndx]);

		coordinate_tunning_multi(ctrl->ndx, coordinate_data_multi, mdnie_tune_index,
			ADDRESS_SCR_WHITE_RED, COORDINATE_DATA_SIZE);

		LCD_INFO("PANEL%d : X-%d Y-%d \n", ctrl->ndx,
			vdd->mdnie_x[ctrl->ndx], vdd->mdnie_y[ctrl->ndx]);
	} else {
		LCD_ERR("PANEL%d error\n", ctrl->ndx);
		return false;
	}

	return true;
}

static int mdss_smart_dimming_init(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0]);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	vdd->smart_dimming_dsi[ndx] = vdd->panel_func.samsung_smart_get_conf();

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi[ndx])) {
		LCD_ERR("DSI%d error\n", ndx);
		return false;
	} else {
		if (get_panel_rx_cmds(ctrl, RX_SMART_DIM_MTP)->cmd_cnt) {
			mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_SMART_DIM_MTP),
			vdd->smart_dimming_dsi[ctrl->ndx]->mtp_buffer, LEVEL1_KEY);

			/* Initialize smart dimming related things here */
			vdd->smart_dimming_dsi[ctrl->ndx]->lux_tab = vdd->dtsi_data[ctrl->ndx].candela_map_table[vdd->panel_revision].cd;
			vdd->smart_dimming_dsi[ctrl->ndx]->lux_tabsize = vdd->dtsi_data[ctrl->ndx].candela_map_table[vdd->panel_revision].tab_size;
			vdd->smart_dimming_dsi[ctrl->ndx]->man_id = vdd->manufacture_id_dsi[ctrl->ndx];

			if (vdd->panel_func.samsung_panel_revision)
				vdd->smart_dimming_dsi[ctrl->ndx]->panel_revision = vdd->panel_func.samsung_panel_revision(ctrl);

			/* copy hbm gamma payload for hbm interpolation calc */
			vdd->smart_dimming_dsi[ctrl->ndx]->hbm_payload = &get_panel_tx_cmds(ctrl, TX_HBM_GAMMA)->cmds[0].payload[1];

			/* Just a safety check to ensure smart dimming data is initialised well */
			vdd->smart_dimming_dsi[ndx]->init(vdd->smart_dimming_dsi[ndx]);

			vdd->temperature = 20; // default temperature

			vdd->smart_dimming_loaded_dsi[ndx] = true;
		} else {
			LCD_ERR("DSI%d error\n", ndx);
			return false;
		}
	}

	LCD_INFO("DSI%d : --\n", ndx);

	return true;
}

static int get_cd_index(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int cd_index = 0;
	int i = 0;

	for (i = 0; i <vdd->dtsi_data[0].aid_map_table[vdd->panel_revision].size; i++) {
		if (vdd->bl_level <= vdd->dtsi_data[0].aid_map_table[vdd->panel_revision].bl_level[i])
			break;
	}

	cd_index = vdd->dtsi_data[0].aid_map_table[vdd->panel_revision].cmd_idx[i];

	LCD_DEBUG("cd_index %d\n",cd_index );

	return cd_index;
}

static int get_cmd_index(struct mdss_dsi_ctrl_pdata *ctrl, struct cmd_map *map)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int cmd_index = 0;
	int i = 0;

	for (i = 0; i < map[vdd->panel_revision].size; i++) {
		if (vdd->candela_level <= map[vdd->panel_revision].bl_level[i])
			break;
	}

	cmd_index = map[vdd->panel_revision].cmd_idx[i];

	LCD_DEBUG("cmd_index %d\n",cmd_index );

	return cmd_index;
}


static struct dsi_panel_cmds aid_cmd;
static struct dsi_panel_cmds *mdss_aid(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	int cd_index = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	if (vdd->pac)
		cd_index = vdd->pac_cd_idx;
	else
		cd_index = vdd->bl_level;

	aid_cmd.cmd_cnt = 1;
	if (initial_dimming[vdd->panel_revision]) {
		cd_index = get_cd_index(ctrl);
		aid_cmd.cmds = &(get_panel_tx_cmds(ctrl, TX_AID)->cmds[cd_index]);
	} else {
		aid_cmd.cmds = &(get_panel_tx_cmds(ctrl, TX_AID_SUBDIVISION)->cmds[cd_index]);
	}

	LCD_DEBUG("[%d] level(%d), aid(%x %x)\n", cd_index, vdd->bl_level, aid_cmd.cmds->payload[1], aid_cmd.cmds->payload[2]);

	*level_key = LEVEL1_KEY;

	return &aid_cmd;
}

#define GRADUAL_ACL_STEP_JP2 2 //refer config_display.xml
/*
 *   <!-- [adaptive Control Parameter] -->
 *   <add-resource type="array" name="config_adaptiveControlValues"></add-resource>
 *   <string-array translatable="false" name="config_adaptiveControlValues">
 *       <item>0</item>
 *       <item>1</item>
 *   </string-array>
 *
 *   <!-- Using Adaptive Display (EAD) -->
 *   <add-resource type="bool" name="config_environment_adaptive_display_enabled"></add-resource>
 *   <bool name="config_environment_adaptive_display_enabled">true</bool>
 */
static struct dsi_panel_cmds * mdss_acl_on(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;

	return get_panel_tx_cmds(ctrl, TX_ACL_ON);
}

/* ACL Concept
 * ACL Start Point
 *		50% : Brightness level(0~255)
 *		60% : HBM Interpolation, HBM(256 ~ 365)
 * ACL Ratio
 *		OFF : Max, HBM Interpolation, HBM(255~365) &&
 *			Gallery, *#0*# receiver, Color blanace app)
 *		 8% : HBM Interpolation, HBM(256~265) && Other apps
 *		15% : MAX(255) && Other Apps
 *		15% : Brightness level(0~254)
 *		OFF : HMD
 *
 * Combination of ACL commands
 *		1. Start Point 60%, ACL  8% -> HBM, HBM Interpolation level 256~365
 *		2. Start Point 50%, ACL 15% -> level 0 ~ 255
 */
static struct dsi_panel_cmds acl_percent_cmd;
static struct dsi_panel_cmds * mdss_acl_percent(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0]);
	int step;
	int cmd_idx;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;

	if (vdd->gradual_acl_val
			&& vdd->gradual_acl_val < GRADUAL_ACL_STEP_JP2)
		step = vdd->gradual_acl_val;
	else
		step = GRADUAL_ACL_STEP_JP2 - 1; /* default value */

	cmd_idx = get_cmd_index(ctrl, vdd->dtsi_data[ndx].acl_map_table);

	LCD_DEBUG("cmd_idx: %d\n", cmd_idx);

	acl_percent_cmd.cmds = &get_panel_tx_cmds(ctrl, TX_ACL_PERCENT)->cmds[cmd_idx];
	acl_percent_cmd.cmd_cnt = 1;

	return &acl_percent_cmd;
}

static struct dsi_panel_cmds * mdss_acl_off(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;

	return get_panel_tx_cmds(ctrl, TX_ACL_OFF);
}

static struct dsi_panel_cmds elvss_cmd;
static struct dsi_panel_cmds *mdss_elvss(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *elvss_cmds = get_panel_tx_cmds(ctrl, TX_ELVSS);
	int ndx = display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0]);
	int cd_index = 0;
	int cmd_idx = 0;
	int candela_value = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx \n", (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	if (vdd->temperature > 0) // high temp
		elvss_cmds = get_panel_tx_cmds(ctrl, TX_ELVSS_HIGH);
	else if (vdd->temperature <= -15) // low temp
		elvss_cmds = get_panel_tx_cmds(ctrl, TX_ELVSS_LOW);
	else // mid temp
		elvss_cmds = get_panel_tx_cmds(ctrl, TX_ELVSS_MID);

	if (IS_ERR_OR_NULL(elvss_cmds)) {
		LCD_ERR("Invalid data ctrl : cmds : 0x%zx \n", (size_t)elvss_cmds);
		return NULL;
	}

	cd_index = vdd->cd_idx;
	candela_value = vdd->candela_level;

	if (!vdd->dtsi_data[ndx].smart_acl_elvss_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data[ndx].smart_acl_elvss_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = get_cmd_index(ctrl, vdd->dtsi_data[ndx].smart_acl_elvss_map_table);

	LCD_DEBUG("cd_index (%d) cmd_idx(%d) candela_value(%d) B6 1st(%x) 2nd (%x) 3rd (%x)\n",
		cd_index, cmd_idx, candela_value,
		elvss_cmds->cmds[cmd_idx].payload[1],
		elvss_cmds->cmds[cmd_idx].payload[2],
		elvss_cmds->cmds[cmd_idx].payload[3]);

	elvss_cmd.cmds = &(elvss_cmds->cmds[cmd_idx]);
	elvss_cmd.cmd_cnt = 1;

	/* Read B6h 22th para -> Write to B6h 22th para */
	elvss_cmd.cmds->payload[22] = vdd->display_status_dsi[ctrl->ndx].elvss_value1;

	*level_key = LEVEL1_KEY;

	return &elvss_cmd;
end :
	LCD_ERR("error");
	return NULL;
}

static struct dsi_panel_cmds temperature_cmd;
static struct dsi_panel_cmds *mdss_elvss_temperature1(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *cmds = get_panel_tx_cmds(ctrl, TX_ELVSS_LOWTEMP);
	char temperature = 0;
	int b8_8th_para = 8;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx cmds : 0x%zx", (size_t)ctrl, (size_t)vdd, (size_t)cmds);
		return NULL;
	}

	/*
	 * The temperature cound note be bigger than 127 and
	 * lower than -127
	 */
	if (vdd->temperature >= 0)
		temperature = vdd->temperature > 127 ? 127 : vdd->temperature;
	else if (vdd->temperature < 0)
		temperature =
			BIT(7) | (-1 * (vdd->temperature < -127 ? -127 : vdd->temperature));

	/* 0xB8 8th TSET */
	cmds->cmds[0].payload[b8_8th_para] = temperature;

	LCD_DEBUG("acl : %d, interpolation_temp : %d temp : %d, cd : %d, B8 %dst :0x%x\n",
		vdd->acl_status, vdd->elvss_interpolation_temperature,
		vdd->temperature, vdd->candela_level,
		b8_8th_para, cmds->cmds[0].payload[b8_8th_para]);

	temperature_cmd.cmds = &(cmds->cmds[0]);
	temperature_cmd.cmd_cnt = 1;

	*level_key = LEVEL1_KEY;

	return &temperature_cmd;
}

static struct dsi_panel_cmds * mdss_gamma(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds  *gamma_cmds = get_panel_tx_cmds(ctrl, TX_GAMMA);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(gamma_cmds)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx cmds : 0x%zx", (size_t)ctrl, (size_t)vdd, (size_t)gamma_cmds);
		return NULL;
	}

	LCD_DEBUG("bl_level : %d candela : %dCD\n", vdd->bl_level, vdd->candela_level);

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi[ctrl->ndx]->generate_gamma)) {
		LCD_ERR("generate_gamma is NULL error");
		return NULL;
	} else {
		vdd->smart_dimming_dsi[ctrl->ndx]->generate_gamma(
			vdd->smart_dimming_dsi[ctrl->ndx],
			vdd->candela_level,
			&gamma_cmds->cmds[0].payload[1]);

		*level_key = LEVEL1_KEY;

		return gamma_cmds;
	}
}

static void dsi_update_mdnie_data(void)
{
	/* Update mdnie command */
	mdnie_data.DSI0_COLOR_BLIND_MDNIE_1 = DSI0_COLOR_BLIND_MDNIE_1;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE_1 = DSI0_RGB_SENSOR_MDNIE_1;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE_2 = DSI0_RGB_SENSOR_MDNIE_2;
//	mdnie_data.DSI0_RGB_SENSOR_MDNIE_3 = DSI0_RGB_SENSOR_MDNIE_3;

	mdnie_data.DSI0_BYPASS_MDNIE = DSI0_BYPASS_MDNIE;
	mdnie_data.DSI0_NEGATIVE_MDNIE = DSI0_NEGATIVE_MDNIE;
	mdnie_data.DSI0_COLOR_BLIND_MDNIE = DSI0_COLOR_BLIND_MDNIE;
	mdnie_data.DSI0_HBM_CE_MDNIE = DSI0_HBM_CE_MDNIE;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE = DSI0_RGB_SENSOR_MDNIE;
	mdnie_data.DSI0_GRAYSCALE_MDNIE = DSI0_GRAYSCALE_MDNIE;
	mdnie_data.DSI0_GRAYSCALE_NEGATIVE_MDNIE = DSI0_GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data.DSI0_CURTAIN = DSI0_SCREEN_CURTAIN_MDNIE;
	mdnie_data.DSI0_NIGHT_MODE_MDNIE = DSI0_NIGHT_MODE_MDNIE;
	mdnie_data.DSI0_NIGHT_MODE_MDNIE_SCR = DSI0_NIGHT_MODE_MDNIE_2;
	mdnie_data.DSI0_COLOR_BLIND_MDNIE_SCR = DSI0_COLOR_BLIND_MDNIE_2;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE_SCR = DSI0_RGB_SENSOR_MDNIE_2;
	mdnie_data.DSI0_COLOR_LENS_MDNIE = DSI0_COLOR_LENS_MDNIE;
	mdnie_data.DSI0_COLOR_LENS_MDNIE_SCR = DSI0_COLOR_LENS_MDNIE_2;

	mdnie_data.mdnie_tune_value_dsi0 = mdnie_tune_value_dsi0;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data.dsi0_bypass_mdnie_size = ARRAY_SIZE(DSI0_BYPASS_MDNIE);
	mdnie_data.mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET;
	mdnie_data.mdnie_step_index[MDNIE_STEP1] = MDNIE_STEP1_INDEX;
	mdnie_data.mdnie_step_index[MDNIE_STEP2] = MDNIE_STEP2_INDEX;
	mdnie_data.address_scr_white[ADDRESS_SCR_WHITE_RED_OFFSET] = ADDRESS_SCR_WHITE_RED;
	mdnie_data.address_scr_white[ADDRESS_SCR_WHITE_GREEN_OFFSET] = ADDRESS_SCR_WHITE_GREEN;
	mdnie_data.address_scr_white[ADDRESS_SCR_WHITE_BLUE_OFFSET] = ADDRESS_SCR_WHITE_BLUE;
	mdnie_data.DSI0_NIGHT_MODE_MDNIE = DSI0_NIGHT_MODE_MDNIE;
//	mdnie_data.DSI0_NIGHT_MODE_MDNIE_SCR = DSI0_NIGHT_MODE_MDNIE_2;
	mdnie_data.dsi0_rgb_sensor_mdnie_1_size = DSI0_RGB_SENSOR_MDNIE_1_SIZE;
	mdnie_data.dsi0_rgb_sensor_mdnie_2_size = DSI0_RGB_SENSOR_MDNIE_2_SIZE;
//	mdnie_data.dsi0_rgb_sensor_mdnie_3_size = DSI0_RGB_SENSOR_MDNIE_3_SIZE;
//	mdnie_data.dsi0_rgb_sensor_mdnie_index = MDNIE_RGB_SENSOR_INDEX;
	mdnie_data.dsi0_adjust_ldu_table = adjust_ldu_data;
	mdnie_data.dsi1_adjust_ldu_table = NULL;
	mdnie_data.dsi0_max_adjust_ldu = 6;
	mdnie_data.dsi1_max_adjust_ldu = 6;
	mdnie_data.dsi0_night_mode_table = night_mode_data;
	mdnie_data.dsi1_night_mode_table = NULL;
	mdnie_data.dsi0_color_lens_table = color_lens_data;
	mdnie_data.dsi1_color_lens_table = NULL;
	mdnie_data.dsi0_max_night_mode_index = 11;
	mdnie_data.dsi1_max_night_mode_index = 11;
	mdnie_data.dsi0_white_default_r = 0xff;
	mdnie_data.dsi0_white_default_g = 0xff;
	mdnie_data.dsi0_white_default_b = 0xff;
	mdnie_data.dsi0_white_rgb_enabled = 0;
	mdnie_data.dsi1_white_default_r = 0xff;
	mdnie_data.dsi1_white_default_g = 0xff;
	mdnie_data.dsi1_white_default_b = 0xff;
	mdnie_data.dsi1_white_rgb_enabled = 0;
	mdnie_data.dsi0_scr_step_index = MDNIE_STEP2_INDEX;
	mdnie_data.dsi1_scr_step_index = MDNIE_STEP2_INDEX;
	mdnie_data.light_notification_tune_value_dsi0 = light_notification_tune_value_dsi0;
	mdnie_data.light_notification_tune_value_dsi1 = light_notification_tune_value_dsi0;
}

static void mdss_panel_init(struct samsung_display_driver_data *vdd)
{
	pr_info("S6E8AA5X01_AMS604RM01 : %s\n", __func__);

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
	vdd->panel_func.samsung_smart_get_conf = smart_get_conf_S6E8AA5X01_AMS604RM01;

	/* Brightness */
	vdd->panel_func.samsung_brightness_hbm_off = NULL/*mdss_hbm_off*/;
	vdd->panel_func.samsung_brightness_aid = mdss_aid;
	vdd->panel_func.samsung_brightness_acl_on = mdss_acl_on;
	vdd->panel_func.samsung_brightness_acl_percent = mdss_acl_percent;
	vdd->panel_func.samsung_brightness_acl_off = mdss_acl_off;
	vdd->panel_func.samsung_brightness_elvss = mdss_elvss;
	vdd->panel_func.samsung_brightness_elvss_temperature1 = mdss_elvss_temperature1;
	vdd->panel_func.samsung_brightness_elvss_temperature2 = NULL;
	vdd->panel_func.samsung_brightness_vint = NULL;
	vdd->panel_func.samsung_brightness_gamma = mdss_gamma;

	/* HBM */
	vdd->panel_func.samsung_hbm_gamma = mdss_hbm_gamma;
	vdd->panel_func.samsung_hbm_etc = mdss_hbm_etc;
	vdd->panel_func.get_hbm_candela_value = get_hbm_candela_value;

	/* default brightness */
	vdd->bl_level = 255;

	/* mdnie */
	vdd->support_mdnie_lite = true;
	vdd->support_mdnie_trans_dimming = false;
	/* for mdnie tuning */
	vdd->mdnie_tune_size[0] = sizeof(DSI0_BYPASS_MDNIE_1);
	vdd->mdnie_tune_size[1] = sizeof(DSI0_BYPASS_MDNIE_2);
//	vdd->mdnie_tune_size[2] = sizeof(DSI0_BYPASS_MDNIE_3);
	dsi_update_mdnie_data();

	/* send recovery pck before sending image date (for ESD recovery) */
	vdd->send_esd_recovery = false;

	vdd->auto_brightness_level = 13;

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = NULL;

	/* Enable panic on first pingpong timeout */
	vdd->debug_data->panic_on_pptimeout = true;

	/* COLOR WEAKNESS */
	vdd->panel_func.color_weakness_ccb_on_off = NULL;

	/* ACL default ON */
	vdd->acl_status = 1;

	/* Initialize the panel before first commit on bootup */
	vdd->init_panel_before_commit = true;
}

static int __init samsung_panel_init(void)
{
	struct samsung_display_driver_data *vdd = samsung_get_vdd();
	char panel_string[] = "ss_dsi_panel_S6E8AA5X01_AMS604RM01_HD";

	vdd->panel_name = mdss_mdp_panel + 8;

	LCD_INFO("%s / %s\n", vdd->panel_name, panel_string);

	if (!strncmp(vdd->panel_name, panel_string, strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = mdss_panel_init;

	return 0;
}
early_initcall(samsung_panel_init);
