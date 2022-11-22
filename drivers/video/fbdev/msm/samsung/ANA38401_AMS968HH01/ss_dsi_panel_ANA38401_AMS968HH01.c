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
#include "ss_dsi_panel_ANA38401_AMS968HH01.h"
#include "ss_dsi_mdnie_ANA38401_AMS968HH01.h"
#include "../../mdss_dsi.h"

static char hbm_buffer1[33] = {0,};
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

	if (mdss_panel_rev_get(ctrl) == 0x01)
		vdd->panel_revision = 'A';
	else
		vdd->panel_revision = 'A';
	vdd->panel_revision -= 'A';

	LCD_INFO("panel_revision = %c %d \n",
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
			date, LEVEL_KEY_NONE);

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
			ddi_id, LEVEL_KEY_NONE);

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
			cell_id_buffer, LEVEL_KEY_NONE);

		for(loop = 0; loop < MAX_CELL_ID; loop++)
			vdd->cell_id_dsi[ndx][loop] = cell_id_buffer[loop];

		LCD_INFO("PANEL%d: %02x %02x %02x %02x %02x %02x %02x\n",
			ndx, vdd->cell_id_dsi[ndx][0],
			vdd->cell_id_dsi[ndx][1],	vdd->cell_id_dsi[ndx][2],
			vdd->cell_id_dsi[ndx][3],	vdd->cell_id_dsi[ndx][4],
			vdd->cell_id_dsi[ndx][5],	vdd->cell_id_dsi[ndx][6]);
	} else {
		LCD_ERR("PANEL%d no cell_id_rx_cmds cmd\n", ndx);
		// Cell ID read has been removed for this panel. 
		// But, return true so that system does not keep checking repeatedly
		//return false;
	}

	return true;
}

static int mdss_hbm_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char hbm_buffer2[1] ={0,};
	char hbm_off_buffer[1] ={0,};
	u32 i;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int ndx = display_ndx_check(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}
	if (vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_HBM][vdd->panel_revision].cmd_cnt) {
		/* Read mtp (D4h 91~123th) for hbm gamma */
		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_HBM][vdd->panel_revision]),
			hbm_buffer1, LEVEL_KEY_NONE);
		vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_HBM][vdd->panel_revision].read_startoffset[0] =  0x63;
		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_HBM][vdd->panel_revision]),
				hbm_buffer1+8, LEVEL_KEY_NONE);
		vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_HBM][vdd->panel_revision].read_startoffset[0] = 0x6B;
		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_HBM][vdd->panel_revision]),
				hbm_buffer1+16, LEVEL_KEY_NONE);
		vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_HBM][vdd->panel_revision].read_startoffset[0] = 0x73;
		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_HBM][vdd->panel_revision]),
				hbm_buffer1+24, LEVEL_KEY_NONE);
		vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_HBM][vdd->panel_revision].read_startoffset[0] = 0x7B;
		vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_HBM][vdd->panel_revision].read_size[0] = 1;
		vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_HBM][vdd->panel_revision].cmds->payload[1] = 1;
		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_HBM][vdd->panel_revision]),
				hbm_buffer1+32, LEVEL_KEY_NONE);

		for(i = 0; i < 33; i++)
			pr_debug("%s MTP data for HBM = %02x\n", __func__, hbm_buffer1[i]);

	}

	if (vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_HBM2][vdd->panel_revision].cmd_cnt) {
		/* Read mtp (D8h 129th) for HBM On elvss*/
		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_HBM2][vdd->panel_revision]),
			hbm_buffer2, LEVEL_KEY_NONE);
		memcpy(&vdd->dtsi_data[ndx].panel_tx_cmd_list[TX_HBM_ETC][vdd->panel_revision].cmds[4].payload[1], hbm_buffer2, 1);
	}

	if (vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_ELVSS][vdd->panel_revision].cmd_cnt) {
		/* Read mtp (D2h 112th) for HBM Off elvss*/
		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_ELVSS][vdd->panel_revision]),
			hbm_off_buffer, LEVEL_KEY_NONE);
		memcpy(&vdd->dtsi_data[ndx].panel_tx_cmd_list[TX_HBM_OFF][vdd->panel_revision].cmds[1].payload[1], hbm_off_buffer, 1);
	}

	return true;
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

		*level_key = LEVEL_KEY_NONE;
		return hbm_gamma_cmds;
	}
}

static struct dsi_panel_cmds *mdss_hbm_etc(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *hbm_etc;
	int elvss_dim_off;
	int acl_opr;
	int acl_start;
	int acl_percent;


	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	hbm_etc = get_panel_tx_cmds(ctrl, TX_HBM_ETC);;

	*level_key = LEVEL_KEY_NONE;

	/* HBM ELVSS */
	switch(vdd->auto_brightness) {
	case 6: /*378*/
		if (vdd->temperature > 0)
			elvss_dim_off = 0x25;
		else if (vdd->temperature > -20)
			elvss_dim_off = 0x1B;
		else
			elvss_dim_off = 0x17;
		break;
	case 7:	/*395*/
		if (vdd->temperature > 0)
			elvss_dim_off = 0x23;
		else if (vdd->temperature > -20)
			elvss_dim_off = 0x19;
		else
			elvss_dim_off = 0x17;
		break;
	case 8:	/*413*/
		if (vdd->temperature > 0)
			elvss_dim_off = 0x21;
		else if (vdd->temperature > -20)
			elvss_dim_off = 0x17;
		else
			elvss_dim_off = 0x17;
		break;
	case 9:	/*430*/
		if (vdd->temperature > 0)
			elvss_dim_off = 0x1F;
		else if (vdd->temperature > -20)
			elvss_dim_off = 0x17;
		else
			elvss_dim_off = 0x17;
		break;
	case 10: /*448*/
		if (vdd->temperature > 0)
			elvss_dim_off = 0x1E;
		else if (vdd->temperature > -20)
			elvss_dim_off = 0x17;
		else
			elvss_dim_off = 0x17;
		break;
	case 11: /*465*/
		if (vdd->temperature > 0)
			elvss_dim_off = 0x1C;
		else if (vdd->temperature > -20)
			elvss_dim_off = 0x17;
		else
			elvss_dim_off = 0x17;
		break;
	case 12: /*483, HBM*/
		if (vdd->temperature > 0)
			elvss_dim_off = 0x1A;
		else if (vdd->temperature > -20)
			elvss_dim_off = 0x17;
		else
			elvss_dim_off = 0x17;
		break;
	case 13: /*500, HBM*/
		if (vdd->temperature > 0)
			elvss_dim_off = 0x17;
		else if (vdd->temperature > -20)
			elvss_dim_off = 0x17;
		else
			elvss_dim_off = 0x17;
		break;

	default:
		pr_err("%s: err: auto_brightness=%d\n",
				__func__, vdd->auto_brightness);
		elvss_dim_off = 0x17;
		break;
	}

	hbm_etc->cmds[2].payload[1] = elvss_dim_off;

	/* ACL */
	if (!vdd->gradual_acl_val) {
		/* gallery app */
		acl_opr = 0x4; /* 16 Frame Avg at ACL off */
		acl_start = 0x99; /* Start setting: 60% start */
		acl_percent = 0x10; /* ACL off */
	} else {
		/* not gallery app */
		acl_opr = 0x5; /* 32 Frame Avg at ACL on */
		acl_start = 0x99; /* Start setting: 60% start */
		acl_percent = 0x11; /* ACL 8% on */
	}

	hbm_etc->cmds[6].payload[1] = acl_opr;
	hbm_etc->cmds[8].payload[1] = acl_start;
	hbm_etc->cmds[10].payload[1] = acl_percent;

	LCD_INFO("%s bl:%d can:%d elv:%x temp:%d opr:%x start:%x acl:%x\n",
			__func__, vdd->bl_level, vdd->candela_level,
			elvss_dim_off, vdd->temperature, acl_opr, acl_start,
			acl_percent);

	return hbm_etc;
}

static struct dsi_panel_cmds *mdss_hbm_off(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL_KEY_NONE;
	return get_panel_tx_cmds(ctrl, TX_HBM_OFF);
}


#define COORDINATE_DATA_SIZE 6
#define MDNIE_SCR_WR_ADDR 50

#define F1(x,y) ((y)-((43*(x))/40)+45)
#define F2(x,y) ((y)-((310*(x))/297)-3)
#define F3(x,y) ((y)+((367*(x))/84)-16305)
#define F4(x,y) ((y)+((333*(x))/107)-12396)

static char coordinate_data_1[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xfc, 0x00, 0xfc, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfd, 0x00, 0xff, 0x00}, /* Tune_2 */
	{0xfc, 0x00, 0xfb, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfe, 0x00, 0xfc, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xfc, 0x00, 0xfd, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfd, 0x00, 0xff, 0x00, 0xfb, 0x00}, /* Tune_7 */
	{0xfc, 0x00, 0xff, 0x00, 0xfd, 0x00}, /* Tune_8 */
	{0xfc, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_9 */
};


static char coordinate_data_2[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xf7, 0x00, 0xee, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xf8, 0x00, 0xf1, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xf9, 0x00, 0xf4, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfa, 0x00, 0xef, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xfa, 0x00, 0xf1, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xfb, 0x00, 0xf4, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xfc, 0x00, 0xf0, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xfc, 0x00, 0xf2, 0x00}, /* Tune_8 */
	{0xff, 0x00, 0xfd, 0x00, 0xf5, 0x00}, /* Tune_9 */
};

static char (*coordinate_data[MAX_MODE])[COORDINATE_DATA_SIZE] = {
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

	/* Read mtp (D8h 123~126th) for ddi id */
	if (get_panel_rx_cmds(ctrl, RX_MDNIE)->cmd_cnt) {
		mdss_samsung_panel_data_read(ctrl,
			get_panel_rx_cmds(ctrl, RX_MDNIE),
			x_y_location, LEVEL_KEY_NONE);

		vdd->mdnie_x[ndx] = x_y_location[0] << 8 | x_y_location[1];	/* X */
		vdd->mdnie_y[ndx] = x_y_location[2] << 8 | x_y_location[3];	/* Y */

		mdnie_tune_index = mdnie_coordinate_index(vdd->mdnie_x[ndx], vdd->mdnie_y[ndx]);
		coordinate_tunning_multi(ndx, coordinate_data, mdnie_tune_index,
			MDNIE_SCR_WR_ADDR, COORDINATE_DATA_SIZE);

		/* Need to make cell_id sysfs output as all zero. Not removing rx_cmds becuase tunning is required*/
		vdd->mdnie_x[ctrl->ndx] = 0;
		vdd->mdnie_y[ctrl->ndx] = 0;

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
	u32 i;

	pr_info("%s DSI%d : ++\n",__func__, ndx);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	vdd->smart_dimming_dsi[ndx] = vdd->panel_func.samsung_smart_get_conf();

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi[ndx])) {
		pr_err("%s DSI%d error", __func__, ndx);
		return false;
	} else {
		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_SMART_DIM_MTP][vdd->panel_revision]),
			vdd->smart_dimming_dsi[ndx]->mtp_buffer, LEVEL_KEY_NONE);

		vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_SMART_DIM_MTP][vdd->panel_revision].read_startoffset[0] =  0x62;
		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_SMART_DIM_MTP][vdd->panel_revision]),
			vdd->smart_dimming_dsi[ndx]->mtp_buffer+8, LEVEL_KEY_NONE);

		vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_SMART_DIM_MTP][vdd->panel_revision].read_startoffset[0] = 0x6A;
		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_SMART_DIM_MTP][vdd->panel_revision]),
			vdd->smart_dimming_dsi[ndx]->mtp_buffer+16, LEVEL_KEY_NONE);

		vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_SMART_DIM_MTP][vdd->panel_revision].read_startoffset[0] = 0x72;
		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_SMART_DIM_MTP][vdd->panel_revision]),
			vdd->smart_dimming_dsi[ndx]->mtp_buffer+24, LEVEL_KEY_NONE);

		vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_SMART_DIM_MTP][vdd->panel_revision].read_startoffset[0] = 0x7A;
		vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_SMART_DIM_MTP][vdd->panel_revision].read_size[0] = 1;
		vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_SMART_DIM_MTP][vdd->panel_revision].cmds->payload[1] = 1;

		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ndx].panel_rx_cmd_list[RX_SMART_DIM_MTP][vdd->panel_revision]),
			vdd->smart_dimming_dsi[ndx]->mtp_buffer+32, LEVEL_KEY_NONE);

		for(i = 0; i < 33; i++) {
			pr_debug("%s MTP data = %02x\n", __func__, vdd->smart_dimming_dsi[ndx]->mtp_buffer[i]);
			/* Modifying hbm parameters for Gamma Offset Index 4 */
			hbm_buffer1[i] += vdd->smart_dimming_dsi[ndx]->mtp_buffer[i];
		}

		/* Modifying hbm gamma tx command for Gamma Offset Index 4 */
		memcpy(&get_panel_tx_cmds(ctrl, TX_HBM_GAMMA)->cmds[0].payload[1], hbm_buffer1, 33);

		/* Initialize smart dimming related things here */
		/* lux_tab setting for 350cd */
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

	pr_info("%s DSI%d : --\n",__func__, ndx);

	return true;
}

static struct dsi_panel_cmds aid_cmd;
static struct dsi_panel_cmds *mdss_aid(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	aid_cmd.cmds = &(get_panel_tx_cmds(ctrl, TX_AID_SUBDIVISION)->cmds[vdd->bl_level]);
	LCD_ERR("level(%d), aid(%x, %x)\n", vdd->bl_level,
			aid_cmd.cmds->payload[0],
			aid_cmd.cmds->payload[3]);

	aid_cmd.cmd_cnt = 1;
	*level_key = LEVEL_KEY_NONE;

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

	*level_key = LEVEL_KEY_NONE;
	if (vdd->gradual_acl_val)
		acl_on_cmds->cmds[5].payload[1] = vdd->gradual_acl_val;

	return acl_on_cmds;
}

static struct dsi_panel_cmds * mdss_acl_off(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL_KEY_NONE;

	return get_panel_tx_cmds(ctrl, TX_ACL_OFF);
}

static struct dsi_panel_cmds *  mdss_pre_elvss(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL_KEY_NONE;

	return get_panel_tx_cmds(ctrl, TX_ELVSS_PRE);
}

static struct dsi_panel_cmds elvss_cmd;
static struct dsi_panel_cmds * mdss_elvss(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *smart_acl_elvss_cmds = get_panel_tx_cmds(ctrl, TX_SMART_ACL_ELVSS);
	struct dsi_panel_cmds *smart_acl_elvss_lowtemp_cmds = get_panel_tx_cmds(ctrl, TX_SMART_ACL_ELVSS_LOWTEMP);
	struct dsi_panel_cmds *smart_acl_elvss_lowtemp2_cmds = get_panel_tx_cmds(ctrl, TX_SMART_ACL_ELVSS_LOWTEMP2);
	struct dsi_panel_cmds *elvss_cmds = get_panel_tx_cmds(ctrl, TX_ELVSS);
	struct dsi_panel_cmds *elvss_lowtemp_cmds = get_panel_tx_cmds(ctrl, TX_ELVSS_LOWTEMP);
	struct dsi_panel_cmds *elvss_lowtemp2_cmds = get_panel_tx_cmds(ctrl, TX_ELVSS_LOWTEMP2);

	int cd_index = 0;
	int cmd_idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}
	cd_index = vdd->cd_idx;


	if (vdd->acl_status || vdd->siop_status) {
		if (!vdd->dtsi_data[ctrl->ndx].smart_acl_elvss_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data[ctrl->ndx].smart_acl_elvss_map_table[vdd->panel_revision].size)
		goto end;

		cmd_idx = vdd->dtsi_data[ctrl->ndx].smart_acl_elvss_map_table[vdd->panel_revision].cmd_idx[cd_index];
		if (vdd->temperature > 0)
			elvss_cmd.cmds = &(smart_acl_elvss_cmds->cmds[cmd_idx]);
		else if (vdd->temperature > -20)
			elvss_cmd.cmds = &(smart_acl_elvss_lowtemp_cmds->cmds[cmd_idx]);
		else
			elvss_cmd.cmds = &(smart_acl_elvss_lowtemp2_cmds->cmds[cmd_idx]);
	} else {
		if (!vdd->dtsi_data[ctrl->ndx].elvss_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data[ctrl->ndx].elvss_map_table[vdd->panel_revision].size)
		goto end;

		cmd_idx = vdd->dtsi_data[ctrl->ndx].elvss_map_table[vdd->panel_revision].cmd_idx[cd_index];
		if (vdd->temperature > 0)
			elvss_cmd.cmds = &(elvss_cmds->cmds[cmd_idx]);
		else if (vdd->temperature > -20)
			elvss_cmd.cmds = &(elvss_lowtemp_cmds->cmds[cmd_idx]);
		else
			elvss_cmd.cmds = &(elvss_lowtemp2_cmds->cmds[cmd_idx]);
	}

	elvss_cmd.cmd_cnt = 1;

	*level_key = LEVEL_KEY_NONE;

	return &elvss_cmd;

end :
	pr_err("%s error", __func__);
	return NULL;
}

static struct dsi_panel_cmds * mdss_pre_caps_setting(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL_KEY_NONE;

	return get_panel_tx_cmds(ctrl, TX_CAPS_PRE);
}

static struct dsi_panel_cmds caps_setting_cmd;
static struct dsi_panel_cmds * mdss_caps_setting(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int cd_index = 0;
	int cmd_idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	cd_index = vdd->cd_idx;

	if (!vdd->dtsi_data[ctrl->ndx].caps_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data[ctrl->ndx].caps_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data[ctrl->ndx].caps_map_table[vdd->panel_revision].cmd_idx[cd_index];
	caps_setting_cmd.cmds = &(get_panel_tx_cmds(ctrl, TX_CAPS)->cmds[cmd_idx]);
	caps_setting_cmd.cmd_cnt = 1;

	*level_key = LEVEL_KEY_NONE;

	return &caps_setting_cmd;

end :
	pr_err("%s error", __func__);
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

		*level_key = LEVEL_KEY_NONE;

		return gamma_cmds;
	}
}

static int samsung_panel_off_post(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int rc = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}
	return rc;
}

static void dsi_update_mdnie_data(void)
{
	pr_info("%s\n", __func__);
	/* Update mdnie command */
	mdnie_data.DSI0_COLOR_BLIND_MDNIE_1 = DSI0_COLOR_BLIND_MDNIE_1;
	mdnie_data.DSI0_COLOR_BLIND_MDNIE_2 = DSI0_COLOR_BLIND_MDNIE_2;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE_1 = DSI0_RGB_SENSOR_MDNIE_1;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE_2 = DSI0_RGB_SENSOR_MDNIE_2;
	mdnie_data.DSI0_UI_DYNAMIC_MDNIE_2 = DSI0_UI_DYNAMIC_MDNIE_2;
	mdnie_data.DSI0_UI_STANDARD_MDNIE_2 = DSI0_UI_STANDARD_MDNIE_2;
	mdnie_data.DSI0_UI_AUTO_MDNIE_2 = DSI0_UI_AUTO_MDNIE_2;
	mdnie_data.DSI0_VIDEO_DYNAMIC_MDNIE_2 = DSI0_VIDEO_DYNAMIC_MDNIE_2;
	mdnie_data.DSI0_VIDEO_STANDARD_MDNIE_2 = DSI0_VIDEO_STANDARD_MDNIE_2;
	mdnie_data.DSI0_VIDEO_AUTO_MDNIE_2 = DSI0_VIDEO_AUTO_MDNIE_2;
	mdnie_data.DSI0_CAMERA_MDNIE_2 = DSI0_CAMERA_AUTO_MDNIE_2;
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
	mdnie_data.DSI0_HBM_CE_TEXT_MDNIE = DSI0_HBM_CE_TEXT_MDNIE;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE = DSI0_RGB_SENSOR_MDNIE;
	mdnie_data.DSI0_CURTAIN = DSI0_CURTAIN;
	mdnie_data.DSI0_GRAYSCALE_MDNIE = DSI0_GRAYSCALE_MDNIE;
	mdnie_data.DSI0_GRAYSCALE_NEGATIVE_MDNIE = DSI0_GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data.DSI0_UI_DYNAMIC_MDNIE = DSI0_UI_DYNAMIC_MDNIE;
	mdnie_data.DSI0_UI_STANDARD_MDNIE = DSI0_UI_STANDARD_MDNIE;
	mdnie_data.DSI0_UI_NATURAL_MDNIE = DSI0_UI_NATURAL_MDNIE;
	mdnie_data.DSI0_UI_MOVIE_MDNIE = DSI0_UI_MOVIE_MDNIE;
	mdnie_data.DSI0_UI_AUTO_MDNIE = DSI0_UI_AUTO_MDNIE;
	mdnie_data.DSI0_VIDEO_OUTDOOR_MDNIE = DSI0_VIDEO_OUTDOOR_MDNIE;
	mdnie_data.DSI0_VIDEO_DYNAMIC_MDNIE = DSI0_VIDEO_DYNAMIC_MDNIE;
	mdnie_data.DSI0_VIDEO_STANDARD_MDNIE = DSI0_VIDEO_STANDARD_MDNIE;
	mdnie_data.DSI0_VIDEO_NATURAL_MDNIE = DSI0_VIDEO_NATURAL_MDNIE;
	mdnie_data.DSI0_VIDEO_MOVIE_MDNIE = DSI0_VIDEO_MOVIE_MDNIE;
	mdnie_data.DSI0_VIDEO_AUTO_MDNIE = DSI0_VIDEO_AUTO_MDNIE;
	mdnie_data.DSI0_VIDEO_WARM_OUTDOOR_MDNIE = DSI0_VIDEO_OUTDOOR_MDNIE;
	mdnie_data.DSI0_VIDEO_WARM_MDNIE = DSI0_VIDEO_OUTDOOR_MDNIE;
	mdnie_data.DSI0_VIDEO_COLD_OUTDOOR_MDNIE = DSI0_VIDEO_OUTDOOR_MDNIE;
	mdnie_data.DSI0_VIDEO_COLD_MDNIE = DSI0_VIDEO_OUTDOOR_MDNIE;
	mdnie_data.DSI0_CAMERA_OUTDOOR_MDNIE = DSI0_CAMERA_OUTDOOR_MDNIE;
	mdnie_data.DSI0_CAMERA_MDNIE = DSI0_CAMERA_AUTO_MDNIE;
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
	mdnie_data.DSI0_TDMB_DYNAMIC_MDNIE = DSI0_TDMB_DYNAMIC_MDNIE;
	mdnie_data.DSI0_TDMB_STANDARD_MDNIE = DSI0_TDMB_STANDARD_MDNIE;
	mdnie_data.DSI0_TDMB_NATURAL_MDNIE = DSI0_TDMB_NATURAL_MDNIE;
	mdnie_data.DSI0_TDMB_MOVIE_MDNIE = DSI0_TDMB_NATURAL_MDNIE;
	mdnie_data.DSI0_TDMB_AUTO_MDNIE = DSI0_TDMB_AUTO_MDNIE;
	mdnie_data.DSI0_NIGHT_MODE_MDNIE = DSI0_NIGHT_MODE_MDNIE;
	mdnie_data.DSI0_NIGHT_MODE_MDNIE_SCR = DSI0_NIGHT_MODE_MDNIE_1;
	mdnie_data.DSI0_COLOR_BLIND_MDNIE_SCR = DSI0_COLOR_BLIND_MDNIE_1;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE_SCR = DSI0_RGB_SENSOR_MDNIE_1;

	mdnie_data.mdnie_tune_value_dsi0 = mdnie_tune_value_dsi0;
	mdnie_data.mdnie_tune_value_dsi1 = mdnie_tune_value_dsi0;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data.dsi0_bypass_mdnie_size = ARRAY_SIZE(DSI0_BYPASS_MDNIE);
	mdnie_data.mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET;
	mdnie_data.mdnie_step_index[MDNIE_STEP1] = MDNIE_STEP1_INDEX;
	mdnie_data.mdnie_step_index[MDNIE_STEP2] = MDNIE_STEP2_INDEX;
	mdnie_data.address_scr_white[ADDRESS_SCR_WHITE_RED_OFFSET] = ADDRESS_SCR_WHITE_RED;
	mdnie_data.address_scr_white[ADDRESS_SCR_WHITE_GREEN_OFFSET] = ADDRESS_SCR_WHITE_GREEN;
	mdnie_data.address_scr_white[ADDRESS_SCR_WHITE_BLUE_OFFSET] = ADDRESS_SCR_WHITE_BLUE;
	mdnie_data.dsi0_rgb_sensor_mdnie_1_size = DSI0_RGB_SENSOR_MDNIE_1_SIZE;
	mdnie_data.dsi0_rgb_sensor_mdnie_2_size = DSI0_RGB_SENSOR_MDNIE_2_SIZE;
	mdnie_data.hdr_tune_value_dsi0 = hdr_tune_value_dsi0;
	mdnie_data.hdr_tune_value_dsi1 = hdr_tune_value_dsi0;
	mdnie_data.dsi0_adjust_ldu_table = adjust_ldu_data;
	mdnie_data.dsi1_adjust_ldu_table = adjust_ldu_data;
	mdnie_data.dsi0_max_adjust_ldu = 6;
	mdnie_data.dsi1_max_adjust_ldu = 6;
	mdnie_data.dsi0_night_mode_table = night_mode_data;
	mdnie_data.dsi1_night_mode_table = night_mode_data;
	mdnie_data.dsi0_max_night_mode_index = 12;
	mdnie_data.dsi1_max_night_mode_index = 12;
	mdnie_data.dsi0_scr_step_index = MDNIE_STEP1_INDEX;
	mdnie_data.dsi1_scr_step_index = MDNIE_STEP1_INDEX;
	mdnie_data.dsi0_white_default_r = 0xff;
	mdnie_data.dsi0_white_default_g = 0xff;
	mdnie_data.dsi0_white_default_b = 0xff;
	//mdnie_data.dsi0_white_rgb_enabled = 0;
	mdnie_data.dsi1_white_default_r = 0xff;
	mdnie_data.dsi1_white_default_g = 0xff;
	mdnie_data.dsi1_white_default_b = 0xff;
	//mdnie_data.dsi1_white_rgb_enabled = 0;
}

static void mdss_panel_init(struct samsung_display_driver_data *vdd)
{
	pr_info("%s\n", __func__);

	vdd->support_mdnie_lite = true;
	vdd->mdnie_tune_size1 = 2;
	vdd->mdnie_tune_size2 = 56;
	vdd->mdnie_tune_size3 = 2;
	vdd->mdnie_tune_size4 = 103;

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = mdss_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = mdss_panel_on_post;
	vdd->panel_func.samsung_panel_off_post = samsung_panel_off_post;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = mdss_panel_revision;
	vdd->panel_func.samsung_manufacture_date_read = mdss_manufacture_date_read;
	vdd->panel_func.samsung_ddi_id_read = mdss_ddi_id_read;
	vdd->panel_func.samsung_cell_id_read = mdss_cell_id_read;
	vdd->panel_func.samsung_octa_id_read = NULL;
	vdd->panel_func.samsung_elvss_read = NULL;
	vdd->panel_func.samsung_hbm_read = mdss_hbm_read;
	vdd->panel_func.samsung_mdnie_read = mdss_mdnie_read;
	vdd->panel_func.samsung_smart_dimming_init = mdss_smart_dimming_init;
	vdd->panel_func.samsung_smart_get_conf = smart_get_conf_ANA38401_AMS968HH01;

	/* Brightness */
	vdd->panel_func.samsung_brightness_hbm_off = mdss_hbm_off;
	vdd->panel_func.samsung_brightness_aid = mdss_aid;
	vdd->panel_func.samsung_brightness_acl_on = mdss_acl_on;
	vdd->panel_func.samsung_brightness_pre_acl_percent = NULL;
	vdd->panel_func.samsung_brightness_acl_percent = NULL;
	vdd->panel_func.samsung_brightness_acl_off = mdss_acl_off;
	vdd->panel_func.samsung_brightness_pre_elvss = mdss_pre_elvss;
	vdd->panel_func.samsung_brightness_pre_caps = mdss_pre_caps_setting;
	vdd->panel_func.samsung_brightness_caps = mdss_caps_setting;
	vdd->panel_func.samsung_brightness_elvss = mdss_elvss;
	vdd->panel_func.samsung_brightness_elvss_temperature1 = NULL;
	vdd->panel_func.samsung_brightness_elvss_temperature2 = NULL;
	vdd->panel_func.samsung_brightness_vint = NULL;
	vdd->panel_func.samsung_brightness_gamma = mdss_gamma;

	/* HBM */
	vdd->panel_func.samsung_hbm_gamma = mdss_hbm_gamma;
	vdd->panel_func.samsung_hbm_etc = mdss_hbm_etc;
	vdd->panel_func.get_hbm_candela_value = NULL;
	vdd->panel_func.samsung_hbm_irc = NULL;

	dsi_update_mdnie_data();

	/* Panel LPM */
	vdd->panel_func.samsung_get_panel_lpm_mode = NULL;

	/* default brightness */
	vdd->bl_level = 255;

	/* send recovery pck before sending image date (for ESD recovery) */
	vdd->send_esd_recovery = false;

	/* If HBM interpolation is used, set auto_brightness_level to 12
	If HBM interpolation is not used, set auto_brightness_level to 6
	*/
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
	char panel_string[] = "ss_dsi_panel_ANA38401_AMS968HH01_QXGA";

	vdd->panel_name = mdss_mdp_panel + 8;
	LCD_INFO("%s : %s\n", __func__, panel_string);

	if (!strncmp(vdd->panel_name, panel_string, strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = mdss_panel_init;
	return 0;
}
early_initcall(samsung_panel_init);
