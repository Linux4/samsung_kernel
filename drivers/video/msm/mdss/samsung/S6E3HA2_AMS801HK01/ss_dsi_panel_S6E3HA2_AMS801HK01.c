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
#include "ss_dsi_panel_S6E3HA2_AMS801HK01.h"
#include "ss_dsi_mdnie_S6E3HA2_AMS801HK01.h"

static int init_ldi_fps(struct mdss_dsi_ctrl_pdata *ctrl);

static int mdss_panel_on_pre(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	pr_info("%s+: ndx=%d \n", __func__, ctrl->ndx);
	mdss_panel_attach_set(ctrl, true);
	pr_info("%s-: ndx=%d \n", __func__, ctrl->ndx);

	init_ldi_fps(ctrl);

	return true;
}

static int mdss_panel_on_post(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	return true;
}

static int mdss_panel_revision(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	if (vdd->manufacture_id_dsi[ctrl->ndx] == 0)
		mdss_panel_attach_set(ctrl, false);
	else
		mdss_panel_attach_set(ctrl, true);

	if (mdss_panel_id2_get(ctrl) == 0x00)
		vdd->panel_revision = 'A';
	else if (mdss_panel_id2_get(ctrl) == 0x01)
		vdd->panel_revision = 'A';
	else
		vdd->panel_revision = 'A';

	vdd->panel_revision -= 'A';

	pr_debug("%s : panel_revision = %c %d \n", __func__,
					vdd->panel_revision + 'A', vdd->panel_revision);

	return true;
}

static int mdss_manufacture_date_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	unsigned char date[4];
	int year, month, day;
	int hour, min;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (C8h 41,42th) for manufacture date */
	if (vdd->dtsi_data[ctrl->ndx].manufacture_date_rx_cmds[vdd->panel_revision].cmd_cnt) {
		mdss_samsung_panel_data_read(ctrl,
			&vdd->dtsi_data[ctrl->ndx].manufacture_date_rx_cmds[vdd->panel_revision],
			date, PANEL_LEVE1_KEY);

		year = date[0] & 0xf0;
		year >>= 4;
		year += 2011; // 0 = 2011 year
		month = date[0] & 0x0f;
		day = date[1] & 0x1f;
		hour = date[2]& 0x0f;
		min = date[3] & 0x1f;

		vdd->manufacture_date_dsi[ctrl->ndx] = year * 10000 + month * 100 + day;
		vdd->manufacture_time_dsi[ctrl->ndx] = hour * 100 + min;

		pr_err("manufacture_date DSI%d = (%d%04d) - year(%d) month(%d) day(%d) hour(%d) min(%d)\n",
			ctrl->ndx, vdd->manufacture_date_dsi[ctrl->ndx], vdd->manufacture_time_dsi[ctrl->ndx],
			year, month, day, hour, min);

	} else {
		pr_err("%s DSI%d error (%d)", __func__, ctrl->ndx,vdd->panel_revision);
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
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (D6h 1~5th) for ddi id */
	if (vdd->dtsi_data[ctrl->ndx].ddi_id_rx_cmds[vdd->panel_revision].cmd_cnt) {
		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ctrl->ndx].ddi_id_rx_cmds[vdd->panel_revision]),
			ddi_id, PANEL_LEVE1_KEY);

		for(loop = 0; loop < 5; loop++)
			vdd->ddi_id_dsi[ctrl->ndx][loop] = ddi_id[loop];

		pr_info("%s DSI%d : %02x %02x %02x %02x %02x\n", __func__, ctrl->ndx,
			vdd->ddi_id_dsi[ctrl->ndx][0], vdd->ddi_id_dsi[ctrl->ndx][1],
			vdd->ddi_id_dsi[ctrl->ndx][2], vdd->ddi_id_dsi[ctrl->ndx][3],
			vdd->ddi_id_dsi[ctrl->ndx][4]);
	} else {
		pr_err("%s DSI%d error", __func__, ctrl->ndx);
		return false;
	}

	return true;
}

static struct dsi_panel_cmds *mdss_hbm_gamma(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = PANEL_LEVE1_KEY;

	return &vdd->dtsi_data[ctrl->ndx].hbm_gamma_tx_cmds[vdd->panel_revision];
}

static struct dsi_panel_cmds *mdss_hbm_etc(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = PANEL_LEVE1_KEY;

	return &vdd->dtsi_data[ctrl->ndx].hbm_etc_tx_cmds[vdd->panel_revision];
}

static int mdss_hbm_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char hbm_buffer[20];
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (B6h 21th) for elvss*/
	mdss_samsung_panel_data_read(ctrl,
		&(vdd->dtsi_data[ctrl->ndx].elvss_rx_cmds[vdd->panel_revision]),
		hbm_buffer, PANEL_LEVE1_KEY);
	vdd->display_status_dsi[ctrl->ndx].elvss_value = hbm_buffer[0];

	return true;
}

static int mdss_cell_id_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char cell_id_buffer[MAX_CELL_ID] = {0,};
	int loop;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read Panel Unique Cell ID (C8h 41~51th) */
	if (vdd->dtsi_data[ctrl->ndx].cell_id_rx_cmds[vdd->panel_revision].cmd_cnt) {
		memset(cell_id_buffer, 0x00, MAX_CELL_ID);

		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ctrl->ndx].cell_id_rx_cmds[vdd->panel_revision]),
			cell_id_buffer, PANEL_LEVE1_KEY);

		for(loop = 0; loop < MAX_CELL_ID; loop++)
			vdd->cell_id_dsi[ctrl->ndx][loop] = cell_id_buffer[loop];

		pr_info("%s DSI%d: %02x %02x %02x %02x %02x %02x %02x\n",
			__func__, ctrl->ndx, vdd->cell_id_dsi[ctrl->ndx][0],
			vdd->cell_id_dsi[ctrl->ndx][1],	vdd->cell_id_dsi[ctrl->ndx][2],
			vdd->cell_id_dsi[ctrl->ndx][3],	vdd->cell_id_dsi[ctrl->ndx][4],
			vdd->cell_id_dsi[ctrl->ndx][5],	vdd->cell_id_dsi[ctrl->ndx][6]);
	} else {
		pr_err("%s DSI%d error", __func__, ctrl->ndx);
		//return false;	//cell ID read has been removed for this panel. But, return true so that system does not keep checking repeatedly
	}

	return true;
}

static struct dsi_panel_cmds *mdss_hbm_off(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = PANEL_LEVE1_KEY;

	return &vdd->dtsi_data[ctrl->ndx].hbm_off_tx_cmds[vdd->panel_revision];
}

#define COORDINATE_DATA_SIZE 6
#define MDNIE_SCR_WR_ADDR 122

#define F1(x,y) ((y)-((164*(x))/151)+8)
#define F2(x,y) ((y)-((70*(x))/67)-7)
#define F3(x,y) ((y)+((181*(x))/35)-18852)
#define F4(x,y) ((y)+((157*(x))/52)-12055)

static char coordinate_data[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xfa, 0x00, 0xfa, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xfb, 0x00, 0xfe, 0x00}, /* Tune_2 */
	{0xfc, 0x00, 0xfb, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfe, 0x00, 0xfb, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xfb, 0x00, 0xfc, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfc, 0x00, 0xff, 0x00, 0xfa, 0x00}, /* Tune_7 */
	{0xfb, 0x00, 0xff, 0x00, 0xfb, 0x00}, /* Tune_8 */
	{0xfb, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_9 */
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
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (D6h 1~5th) for ddi id */
	if (vdd->dtsi_data[ctrl->ndx].mdnie_read_rx_cmds[vdd->panel_revision].cmd_cnt) {
		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ctrl->ndx].mdnie_read_rx_cmds[vdd->panel_revision]),
			x_y_location, PANEL_LEVE1_KEY);

		vdd->mdnie_x[ctrl->ndx] = x_y_location[0] << 8 | x_y_location[1];	/* X */
		vdd->mdnie_y[ctrl->ndx] = x_y_location[2] << 8 | x_y_location[3];	/* Y */

		mdnie_tune_index = mdnie_coordinate_index(vdd->mdnie_x[ctrl->ndx], vdd->mdnie_y[ctrl->ndx]);
		coordinate_tunning(ctrl->ndx, coordinate_data[mdnie_tune_index],
			MDNIE_SCR_WR_ADDR, COORDINATE_DATA_SIZE);

		/* Need to make cell_id sysfs output as all zero. Not removing rx_cmds becuase tunning is required*/
		vdd->mdnie_x[ctrl->ndx] = 0;
		vdd->mdnie_y[ctrl->ndx] = 0;
		
		pr_info("%s DSI%d : X-%d Y-%d \n", __func__, ctrl->ndx,
			vdd->mdnie_x[ctrl->ndx], vdd->mdnie_y[ctrl->ndx]);
	} else {
		pr_err("%s DSI%d error", __func__, ctrl->ndx);
		return false;
	}

	return true;
}

static int mdss_samart_dimming_init(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	vdd->smart_dimming_dsi[ctrl->ndx] = vdd->panel_func.samsung_smart_get_conf();

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi[ctrl->ndx])) {
		pr_err("%s DSI%d error", __func__, ctrl->ndx);
		return false;
	} else {
		mdss_samsung_panel_data_read(ctrl,
			&(vdd->dtsi_data[ctrl->ndx].smart_dimming_mtp_rx_cmds[vdd->panel_revision]),
			vdd->smart_dimming_dsi[ctrl->ndx]->mtp_buffer, PANEL_LEVE1_KEY);

		/* Initialize smart dimming related things here */
		/* lux_tab setting for 350cd */
		vdd->smart_dimming_dsi[ctrl->ndx]->lux_tab = vdd->dtsi_data[ctrl->ndx].candela_map_table[vdd->panel_revision].lux_tab;
		vdd->smart_dimming_dsi[ctrl->ndx]->lux_tabsize = vdd->dtsi_data[ctrl->ndx].candela_map_table[vdd->panel_revision].lux_tab_size;
		vdd->smart_dimming_dsi[ctrl->ndx]->man_id = vdd->manufacture_id_dsi[ctrl->ndx];

		/* Just a safety check to ensure smart dimming data is initialised well */
		vdd->smart_dimming_dsi[ctrl->ndx]->init(vdd->smart_dimming_dsi[ctrl->ndx]);

		vdd->temperature = 20; // default temperature

		vdd->smart_dimming_loaded_dsi[ctrl->ndx] = true;
	}

	pr_info("%s DSI%d : --\n",__func__, ctrl->ndx);

	return true;
}

static struct dsi_panel_cmds aid_cmd;
static struct dsi_panel_cmds *mdss_aid(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int cd_index = 0;
	int cmd_idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	cd_index = get_cmd_index(vdd, ctrl->ndx);

	if (!vdd->dtsi_data[ctrl->ndx].aid_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data[ctrl->ndx].aid_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data[ctrl->ndx].aid_map_table[vdd->panel_revision].cmd_idx[cd_index];

	aid_cmd.cmds = &(vdd->dtsi_data[ctrl->ndx].aid_tx_cmds[vdd->panel_revision].cmds[cmd_idx]);
	aid_cmd.cmd_cnt = 1;

	*level_key = PANEL_LEVE1_KEY;

	return &aid_cmd;

end :
	pr_err("%s error", __func__);
	return NULL;
}

static int mdss_force_acl_on(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int acl_on;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* acl is always on */
	acl_on = 1;

	/* Gallery & Max brightness */
	if ((vdd->weakness_hbm_comp == 2) && (vdd->bl_level == 255))
		acl_on = 0;

	pr_info("%s : acl_on(%d)\n", __func__, acl_on);

	return acl_on;
}

static struct dsi_panel_cmds * mdss_acl_on(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = PANEL_LEVE1_KEY;
	pr_err("%s : acl_on\n", __func__);

	return &(vdd->dtsi_data[ctrl->ndx].acl_on_tx_cmds[vdd->panel_revision]);
}

static struct dsi_panel_cmds * mdss_acl_off(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = PANEL_LEVE1_KEY;
	pr_err("%s : mdss_acl_off\n", __func__);

	return &(vdd->dtsi_data[ctrl->ndx].acl_off_tx_cmds[vdd->panel_revision]);
}

static struct dsi_panel_cmds acl_percent_cmd;
static struct dsi_panel_cmds * mdss_acl_precent(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int cd_index = 0;
	int cmd_idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	cd_index = get_cmd_index(vdd, ctrl->ndx);

	if (!vdd->dtsi_data[ctrl->ndx].acl_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data[ctrl->ndx].acl_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data[ctrl->ndx].acl_map_table[vdd->panel_revision].cmd_idx[cd_index];

	acl_percent_cmd.cmds = &(vdd->dtsi_data[ctrl->ndx].acl_percent_tx_cmds[vdd->panel_revision].cmds[cmd_idx]);
	acl_percent_cmd.cmd_cnt = 1;

	*level_key = PANEL_LEVE1_KEY;

	return &acl_percent_cmd;

end :
	pr_err("%s error", __func__);
	return NULL;

}

static struct dsi_panel_cmds elvss_cmd;
static struct dsi_panel_cmds * mdss_elvss(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int cd_index = 0;
	int cmd_idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	cd_index = get_cmd_index(vdd, ctrl->ndx);

	if (!vdd->dtsi_data[ctrl->ndx].smart_acl_elvss_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data[ctrl->ndx].smart_acl_elvss_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data[ctrl->ndx].smart_acl_elvss_map_table[vdd->panel_revision].cmd_idx[cd_index];

	if (vdd->acl_status || vdd->siop_status)
		elvss_cmd.cmds = &(vdd->dtsi_data[ctrl->ndx].smart_acl_elvss_tx_cmds[vdd->panel_revision].cmds[cmd_idx]);
	else
		elvss_cmd.cmds = &(vdd->dtsi_data[ctrl->ndx].elvss_tx_cmds[vdd->panel_revision].cmds[cmd_idx]);

	elvss_cmd.cmd_cnt = 1;

	*level_key = PANEL_LEVE1_KEY;

	return &elvss_cmd;

end :
	pr_err("%s error", __func__);
	return NULL;
}

static struct dsi_panel_cmds * mdss_elvss_temperature1(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	if (vdd->temperature > 0)
		vdd->dtsi_data[ctrl->ndx].elvss_lowtemp_tx_cmds[vdd->panel_revision].cmds[1].payload[1] = 0x19; //0xB8
	else if (vdd->temperature > -20)
		vdd->dtsi_data[ctrl->ndx].elvss_lowtemp_tx_cmds[vdd->panel_revision].cmds[1].payload[1] = 0x00; //0xB8
	else
		vdd->dtsi_data[ctrl->ndx].elvss_lowtemp_tx_cmds[vdd->panel_revision].cmds[1].payload[1] = 0x94; //0xB8

	pr_debug("%s acl : %d temp : %d 0xB8 :0x%x\n", __func__, vdd->acl_status, vdd->temperature,
		vdd->dtsi_data[ctrl->ndx].elvss_lowtemp_tx_cmds[vdd->panel_revision].cmds[1].payload[1] );

	*level_key = PANEL_LEVE1_KEY;

	return &(vdd->dtsi_data[ctrl->ndx].elvss_lowtemp_tx_cmds[vdd->panel_revision]);
}

static struct dsi_panel_cmds * mdss_elvss_temperature2(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	vdd->dtsi_data[ctrl->ndx].elvss_lowtemp2_tx_cmds[vdd->panel_revision].cmds[1].payload[1] =
		vdd->display_status_dsi[ctrl->ndx].elvss_value;

	pr_debug("%s acl : %d temp : %d 0xB0 : 0x%x 0xB6 : 0x%x\n", __func__, vdd->acl_status, vdd->temperature,
		vdd->dtsi_data[ctrl->ndx].elvss_lowtemp2_tx_cmds[vdd->panel_revision].cmds[0].payload[1],
		vdd->dtsi_data[ctrl->ndx].elvss_lowtemp2_tx_cmds[vdd->panel_revision].cmds[1].payload[1]);

	*level_key = PANEL_LEVE1_KEY;

	return &(vdd->dtsi_data[ctrl->ndx].elvss_lowtemp2_tx_cmds[vdd->panel_revision]);
}

static struct dsi_panel_cmds * mdss_gamma(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	vdd->candela_level = get_candela_value(vdd, ctrl->ndx);

	pr_debug("%s bl_level : %d candela : %dCD\n", __func__, vdd->bl_level, vdd->candela_level);

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi[ctrl->ndx]->generate_gamma)) {
		pr_err("%s generate_gamma is NULL error", __func__);
		return NULL;
	} else {
		vdd->smart_dimming_dsi[ctrl->ndx]->generate_gamma(
			vdd->smart_dimming_dsi[ctrl->ndx],
			vdd->candela_level,
			&vdd->dtsi_data[ctrl->ndx].gamma_tx_cmds[vdd->panel_revision].cmds[0].payload[1]);

		*level_key = PANEL_LEVE1_KEY;

		return &vdd->dtsi_data[ctrl->ndx].gamma_tx_cmds[vdd->panel_revision];
	}
}

#define LUT_SIZE (sizeof(freq_cal_lut_offset) / sizeof(int)) / 3

static int freq_cal_lut_offset[][3] = {
	{56568, 56686,	-25},
	{56686, 56805,	-24},
	{56805, 56924,	-23},
	{56924, 57044,	-22},
	{57044, 57165,	-21},
	{57165, 57286,	-20},
	{57286, 57407,	-19},
	{57407, 57529,	-18},
	{57529, 57651,	-17},
	{57651, 57774,	-16},
	{57774, 57898,	-15},
	{57898, 58022,	-14},
	{58022, 58146,	-13},
	{58146, 58272,	-12},
	{58272, 58397,	-11},
	{58397, 58523,	-10},
	{58523, 58650,	-9},
	{58650, 58777,	-8},
	{58777, 58905,	-7},
	{58905, 59034,	-6},
	{59034, 59163,	-5},
	{59163, 59292,	-4},
	{59292, 59422,	-3},
	{59422, 59553,	-2},
	{59553, 59684,	-1},
	{59684, 59816,	0},
	{59816, 59948,	1},
	{59948, 60081,	2},
	{60081, 60215,	3},
	{60215, 60349,	4},
	{60349, 60484,	5},
	{60484, 60619,	6},
	{60619, 60755,	7},
	{60755, 60892,	8},
	{60892, 61029,	9},
	{61029, 61167,	10},
	{61167, 61305,	11},
	{61305, 61444,	12},
	{61444, 61584,	13},
	{61584, 61724,	14},
	{61724, 61865,	15},
	{61865, 62007,	16},
	{62007, 62149,	17},
	{62149, 62292,	18},
	{62292, 62436,	19},
	{62436, 62580,	20},
	{62580, 62725,	21},
	{62725, 62871,	22},
	{62871, 63017,	23},
	{63017, 63164,	24},
	{63164, 63312,	25},
	{63312, 63460,	26},
	{63460, 63609,	27},
	{63609, 63759,	28},
	{63759, 63909,	29},
	{63909, 64010,	30},
};

#define FPS_CMD_INDEX 2

static int mdss_samsung_change_ldi_fps(struct mdss_dsi_ctrl_pdata *ctrl, unsigned int input_fps)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	int offset = 0;
	int i;
	int dest_cal_val;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return 0;
	}

	pr_debug("%s :input_fps (%d), lut size (%d)\n", __func__, input_fps, (int)LUT_SIZE);

	if (mdss_panel_id2_get(ctrl) <= 0x01) {
		pr_err("%s :LDI EVT0 Not Support. Skip!! \n",__func__);
		return 0;
	}

	for (i = 0; i < LUT_SIZE; i++) {
		if (input_fps >= freq_cal_lut_offset[i][0] &&
			input_fps < freq_cal_lut_offset[i][1]) {
			offset = freq_cal_lut_offset[i][2];
			break;
		}
	}

	if (i == LUT_SIZE) {
		pr_err("%s :can not find offset !!\n", __func__);
		return 0;
	}

	pr_debug("%s :current comp value(0x%x),offset(%d)\n", __func__,
		vdd->dtsi_data[ctrl->ndx].ldi_fps_change_tx_cmds[vdd->panel_revision].cmds[FPS_CMD_INDEX].payload[3], offset);

	dest_cal_val = vdd->dtsi_data[ctrl->ndx].ldi_fps_change_tx_cmds[vdd->panel_revision].cmds[FPS_CMD_INDEX].payload[3] + offset;

	if((dest_cal_val < 0xAC) || (dest_cal_val > 0xE3)) {
		pr_err("%s Invalid cal value(0x%x)", __func__, dest_cal_val);
		return 0;
	} else
		pr_info("%s : input : %d dest write value (0x%x)\n", __func__, input_fps, dest_cal_val);

	vdd->dtsi_data[ctrl->ndx].ldi_fps_change_tx_cmds[vdd->panel_revision].cmds[FPS_CMD_INDEX].payload[3] = dest_cal_val;

	return 1;
}

static int init_ldi_fps(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return 0;
	}

	if (ctrl->cmd_sync_wait_broadcast)
		if (!ctrl->cmd_sync_wait_trigger)
			return 0;

	vdd->dtsi_data[ctrl->ndx].ldi_fps_change_tx_cmds[vdd->panel_revision].cmds[FPS_CMD_INDEX].payload[3] = 0xC5;
	pr_err("%s : (0x%x)\n", __func__, vdd->dtsi_data[ctrl->ndx].ldi_fps_change_tx_cmds[vdd->panel_revision].cmds[FPS_CMD_INDEX].payload[3]);

	return 1;
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

static struct dsi_panel_cmds * mdss_gamma_hmt(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	pr_info("%s hmt_bl_level : %d candela : %dCD\n", __func__, vdd->hmt_stat.hmt_bl_level, vdd->hmt_stat.candela_level_hmt);

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi_hmt[ctrl->ndx]->generate_gamma)) {
		pr_err("%s generate_gamma is NULL error", __func__);
		return NULL;
	} else {
		vdd->smart_dimming_dsi_hmt[ctrl->ndx]->generate_gamma(
			vdd->smart_dimming_dsi_hmt[ctrl->ndx],
			vdd->hmt_stat.candela_level_hmt,
			&vdd->dtsi_data[ctrl->ndx].hmt_gamma_tx_cmds[vdd->panel_revision].cmds[0].payload[1]);

		*level_key = PANEL_LEVE1_KEY;

		return &vdd->dtsi_data[ctrl->ndx].hmt_gamma_tx_cmds[vdd->panel_revision];
	}
}

static struct dsi_panel_cmds hmt_aid_cmd;
static struct dsi_panel_cmds *mdss_aid_hmt(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int cmd_idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	if (!vdd->dtsi_data[ctrl->ndx].hmt_reverse_aid_map_table[vdd->panel_revision].size ||
		vdd->hmt_stat.cmd_idx_hmt > vdd->dtsi_data[ctrl->ndx].hmt_reverse_aid_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data[ctrl->ndx].hmt_reverse_aid_map_table[vdd->panel_revision].cmd_idx[vdd->hmt_stat.cmd_idx_hmt];

	hmt_aid_cmd.cmds = &(vdd->dtsi_data[ctrl->ndx].hmt_aid_tx_cmds[vdd->panel_revision].cmds[cmd_idx]);
	hmt_aid_cmd.cmd_cnt = 1;

	*level_key = PANEL_LEVE1_KEY;

	return &hmt_aid_cmd;

end :
	pr_err("%s error", __func__);
	return NULL;
}

static struct dsi_panel_cmds *mdss_elvss_hmt(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	if (vdd->acl_status || vdd->siop_status) {
		vdd->dtsi_data[ctrl->ndx].hmt_elvss_tx_cmds[vdd->panel_revision].cmds[0].payload[1] = 0x8C;
	} else {
		vdd->dtsi_data[ctrl->ndx].hmt_elvss_tx_cmds[vdd->panel_revision].cmds[0].payload[1] = 0x9C;
	}

	if (vdd->bl_level < 0 || vdd->bl_level > 255)
		vdd->dtsi_data[ctrl->ndx].hmt_elvss_tx_cmds[vdd->panel_revision].cmds[0].payload[1] = 0x00;

	*level_key = PANEL_LEVE1_KEY;

	return &vdd->dtsi_data[ctrl->ndx].hmt_elvss_tx_cmds[vdd->panel_revision];
}

static struct dsi_panel_cmds *mdss_vint_hmt(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = PANEL_LEVE1_KEY;

	return &vdd->dtsi_data[ctrl->ndx].hmt_vint_tx_cmds[vdd->panel_revision];
}

static void mdss_make_sdimconf_hmt(struct mdss_dsi_ctrl_pdata *ctrl, struct samsung_display_driver_data *vdd) {
	/* Set the mtp read buffer pointer and read the NVM value*/
	mdss_samsung_panel_data_read(ctrl,
				&(vdd->dtsi_data[ctrl->ndx].smart_dimming_mtp_rx_cmds[vdd->panel_revision]),
				vdd->smart_dimming_dsi_hmt[ctrl->ndx]->mtp_buffer, PANEL_LEVE1_KEY);


	/* Initialize smart dimming related things here */
	/* lux_tab setting for 350cd */
	vdd->smart_dimming_dsi_hmt[ctrl->ndx]->lux_tab = vdd->dtsi_data[ctrl->ndx].hmt_candela_map_table[vdd->panel_revision].lux_tab;
	vdd->smart_dimming_dsi_hmt[ctrl->ndx]->lux_tabsize = vdd->dtsi_data[ctrl->ndx].hmt_candela_map_table[vdd->panel_revision].lux_tab_size;
	vdd->smart_dimming_dsi_hmt[ctrl->ndx]->man_id = vdd->manufacture_id_dsi[ctrl->ndx];

	/* Just a safety check to ensure smart dimming data is initialised well */
	vdd->smart_dimming_dsi_hmt[ctrl->ndx]->init(vdd->smart_dimming_dsi_hmt[ctrl->ndx]);

	pr_info("[HMT] smart dimming done!\n");
}


static int mdss_samart_dimming_init_hmt(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	pr_info("%s DSI%d : ++\n",__func__, ctrl->ndx);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	vdd->smart_dimming_dsi_hmt[ctrl->ndx] = vdd->panel_func.samsung_smart_get_conf_hmt();

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi_hmt[ctrl->ndx])) {
		pr_err("%s DSI%d error", __func__, ctrl->ndx);
		return false;
	} else {
		vdd->hmt_stat.hmt_on = 0;
		vdd->hmt_stat.hmt_bl_level = 0;
		vdd->hmt_stat.hmt_reverse = 0;
		vdd->hmt_stat.hmt_is_first = 1;

		mdss_make_sdimconf_hmt(ctrl, vdd);

		vdd->smart_dimming_hmt_loaded_dsi[ctrl->ndx] = true;
	}

	pr_info("%s DSI%d : --\n",__func__, ctrl->ndx);

	return true;
}

static void dsi_update_mdnie_data(void)
{
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
	mdnie_data.DSI0_GRAYSCALE_MDNIE = DSI0_GRAYSCALE_MDNIE;
	mdnie_data.DSI0_GRAYSCALE_NEGATIVE_MDNIE = DSI0_GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data.DSI0_COLOR_BLIND_MDNIE = DSI0_COLOR_BLIND_MDNIE;
	mdnie_data.DSI0_HBM_CE_MDNIE = DSI0_HBM_CE_MDNIE;
	mdnie_data.DSI0_HBM_CE_TEXT_MDNIE = DSI0_HBM_CE_TEXT_MDNIE;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE = DSI0_RGB_SENSOR_MDNIE;
	mdnie_data.DSI0_CURTAIN = DSI0_CURTAIN;
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
	mdnie_data.DSI0_TDMB_DYNAMIC_MDNIE = DSI0_TDMB_DYNAMIC_MDNIE;
	mdnie_data.DSI0_TDMB_STANDARD_MDNIE = DSI0_TDMB_STANDARD_MDNIE;
	mdnie_data.DSI0_TDMB_NATURAL_MDNIE = DSI0_TDMB_NATURAL_MDNIE;
	mdnie_data.DSI0_TDMB_MOVIE_MDNIE = DSI0_TDMB_MOVIE_MDNIE;
	mdnie_data.DSI0_TDMB_AUTO_MDNIE = DSI0_TDMB_AUTO_MDNIE;

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
	mdnie_data.dsi0_rgb_sensor_mdnie_index = MDNIE_RGB_SENSOR_INDEX;
}

static void  mdss_panel_init(struct samsung_display_driver_data *vdd)
{
	pr_err("%s : %s", __func__, vdd->panel_name);

	vdd->support_mdnie_lite = true;
	vdd->mdnie_tune_size1 = 157;
	vdd->mdnie_tune_size2 = 4;

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = mdss_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = mdss_panel_on_post;
	vdd->panel_func.samsung_panel_off_post = samsung_panel_off_post;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = mdss_panel_revision;
	vdd->panel_func.samsung_manufacture_date_read = mdss_manufacture_date_read;
	vdd->panel_func.samsung_ddi_id_read = mdss_ddi_id_read;
	vdd->panel_func.samsung_hbm_read = mdss_hbm_read;
	vdd->panel_func.samsung_mdnie_read = mdss_mdnie_read;
	vdd->panel_func.samsung_smart_dimming_init = mdss_samart_dimming_init;
	vdd->panel_func.samsung_smart_get_conf = smart_get_conf_S6E3HA2_AMS801HK01;
	vdd->panel_func.samsung_cell_id_read = mdss_cell_id_read;

	/* Brightness */
	vdd->panel_func.samsung_brightness_hbm_off = mdss_hbm_off;
	vdd->panel_func.samsung_brightness_aid = mdss_aid;
	vdd->panel_func.samsung_brightness_acl_on = mdss_acl_on;
	vdd->panel_func.samsung_brightness_acl_percent = mdss_acl_precent;
	vdd->panel_func.samsung_brightness_acl_off = mdss_acl_off;
	vdd->panel_func.samsung_brightness_elvss = mdss_elvss;
	vdd->panel_func.samsung_brightness_elvss_temperature1 = mdss_elvss_temperature1;
	vdd->panel_func.samsung_brightness_elvss_temperature2 = mdss_elvss_temperature2;
	vdd->panel_func.samsung_brightness_vint = NULL;
	vdd->panel_func.samsung_brightness_gamma = mdss_gamma;

	/* HBM */
	vdd->panel_func.samsung_hbm_gamma = mdss_hbm_gamma;
	vdd->panel_func.samsung_hbm_etc = mdss_hbm_etc;

	/* Event */
	vdd->panel_func.samsung_change_ldi_fps = mdss_samsung_change_ldi_fps;

	/* HMT */
	vdd->panel_func.samsung_brightness_gamma_hmt = mdss_gamma_hmt;
	vdd->panel_func.samsung_brightness_aid_hmt = mdss_aid_hmt;
	vdd->panel_func.samsung_brightness_elvss_hmt = mdss_elvss_hmt;
	vdd->panel_func.samsung_brightness_vint_hmt = mdss_vint_hmt;
	vdd->panel_func.samsung_smart_dimming_hmt_init = mdss_samart_dimming_init_hmt;
	vdd->panel_func.samsung_smart_get_conf_hmt = smart_get_conf_S6E3HA2_AMS801HK01_hmt;

	vdd->manufacture_id_dsi[0] = PBA_ID;

	/* If HBM interpolation is used, set auto_brightness_level to 12
	If HBM interpolation is not used, set auto_brightness_level to 6
	*/
	vdd->auto_brightness_level = 6;

	dsi_update_mdnie_data();

	/* force ACL */
	vdd->panel_func.samsung_force_acl_on = mdss_force_acl_on;

	/* ACL default ON */
	vdd->acl_status = 1;
}

static int __init samsung_panel_init(void)
{
	struct samsung_display_driver_data *vdd = samsung_get_vdd();
	char panel_string[] = "ss_dsi_panel_S6E3HA2_AMS801HK01_QXGA";

	vdd->panel_name = mdss_mdp_panel + 8;
	pr_info("%s : %s\n", __func__, vdd->panel_name);

	if (!strncmp(vdd->panel_name, panel_string, strlen(panel_string)))
		vdd->panel_func.samsung_panel_init = mdss_panel_init;

	return 0;
}
early_initcall(samsung_panel_init);
