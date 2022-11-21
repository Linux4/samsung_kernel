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
#include <linux/cpufreq.h>
#include "ss_dsi_panel_S6E3HA6_AMB577MQ01.h"
#include "ss_dsi_mdnie_S6E3HA6_AMB577MQ01.h"
#include "gct_test_pattern_img.h"
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
/* Register to cnotrol POC */
#define POC_CTRL_REG	0xEB

#define LUMINANCE_MAX 74

static int mdss_panel_on_pre(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int on_reg_list[1][2] = {{POC_CTRL_REG, -EINVAL}};
	struct dsi_panel_cmds *on_cmd_list[1];
	char poc_buffer[4] = {0,};
	static unsigned int i_poc_buffer[4] = {0,};
	int MAX_POC = 4;
	int loop;

	on_cmd_list[0] = &ctrl->on_cmds;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	LCD_INFO("+: ndx=%d \n", ctrl->ndx);
	mdss_panel_attach_set(ctrl, true);

	if (!vdd->poc_driver.is_support) {
		LCD_DEBUG("Not Support POC Function \n");
		goto end;
	}

	/* Read Panel POC (EBh 1nd~4th) */
	if (get_panel_rx_cmds(ctrl, RX_POC_STATUS)->cmd_cnt) {
		memset(poc_buffer, 0x00, sizeof(poc_buffer[0]) * MAX_POC);

		if (unlikely(vdd->is_factory_mode) &&
				vdd->dtsi_data[ctrl->ndx].samsung_support_factory_panel_swap) {
			memset(i_poc_buffer, 0x00, sizeof(i_poc_buffer[0]) * MAX_POC);
		}

		if (i_poc_buffer[3] == 0) {
			mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_POC_STATUS),
					poc_buffer, LEVEL1_KEY);

			for(loop = 0; loop < MAX_POC; loop++)
				i_poc_buffer[loop] = (unsigned int)poc_buffer[loop];
		}

		LCD_DEBUG("[POC] DSI%d: %02x %02x %02x %02x\n",
				ctrl->ndx,
				i_poc_buffer[0],
				i_poc_buffer[1],
				i_poc_buffer[2],
				i_poc_buffer[3]);

		/*
		 * Update REBh 4th param to 0xFF or 0x64
		 */
		mdss_init_panel_lpm_reg_offset(ctrl, on_reg_list, on_cmd_list,
				sizeof(on_cmd_list) / sizeof(on_cmd_list[0]));

		if ((on_reg_list[0][1] != -EINVAL) &&\
			(vdd->octa_id_dsi[ctrl->ndx][1] == 0x1)) {	/* POC CHECK  C9 3th Para */
			if (i_poc_buffer[3] == 0x33)
				i_poc_buffer[3] = 0x64;

			on_cmd_list[0]->cmds[on_reg_list[0][1]].payload[4] =
				i_poc_buffer[3];

			LCD_DEBUG("Update POC register, 0x%02x\n",
					on_cmd_list[0]->cmds[on_reg_list[0][1]].payload[4]);

		}

		LCD_DEBUG("[POC] DSI%d: octa_id:%d, poc_buffer:%02x, index:%d\n",
				ctrl->ndx,
				vdd->octa_id_dsi[ctrl->ndx][1],
				i_poc_buffer[3],
				on_reg_list[0][1]);
	} else {
		LCD_ERR("DSI%d no poc_rx_cmds cmd\n", ctrl->ndx);
	}

end:
	LCD_INFO("-: ndx=%d \n", ctrl->ndx);

	return true;
}

static int mdss_panel_on_post(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	return true;
}

static char mdss_panel_revision(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	if (vdd->manufacture_id_dsi[ctrl->ndx] == PBA_ID)
		mdss_panel_attach_set(ctrl, false);
	else
		mdss_panel_attach_set(ctrl, true);

	switch (mdss_panel_rev_get(ctrl)) {
		case 0x00:
			/* Rev.Pre */
		case 0x01:
			/* Rev.A */
			vdd->panel_revision = 'A';
			break;
		case 0x02:
			/* Rev.B */
			vdd->panel_revision = 'B';
			break;
		case 0x03:
			/* Rev.C */
			vdd->panel_revision = 'C';
			break;
		case 0x04:
			/* Rev.D / E / F */
			/* vdd->panel_revision = 'D'; */
			/* vdd->panel_revision = 'E'; */
			vdd->panel_revision = 'F';
			break;
		case 0x05:
			/* Rev.G / H */
			/* vdd->panel_revision = 'G'; */
			vdd->panel_revision = 'H';
			break;
		default:
			vdd->panel_revision = 'H';
			LCD_ERR("Invalid panel_rev(default rev : %c)\n",
					vdd->panel_revision);
			break;
	}

	vdd->panel_revision -= 'A';

	LCD_INFO_ONCE("panel_revision = %c %d \n",
					vdd->panel_revision + 'A', vdd->panel_revision);

	return (vdd->panel_revision + 'A');
}

static int mdss_manufacture_date_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	unsigned char date[4];
	int year, month, day;
	int hour, min;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (A1h 5,6 th) for manufacture date */
	if (get_panel_rx_cmds(ctrl, RX_MANUFACTURE_DATE)->cmd_cnt) {
		mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_MANUFACTURE_DATE),
			date, LEVEL1_KEY);

		year = date[0] & 0xf0;
		year >>= 4;
		year += 2011; // 0 = 2011 year
		month = date[0] & 0x0f;
		day = date[1] & 0x1f;
		hour = date[2]& 0x0f;
		min = date[3] & 0x1f;

		vdd->manufacture_date_dsi[ctrl->ndx] = year * 10000 + month * 100 + day;
		vdd->manufacture_time_dsi[ctrl->ndx] = hour * 100 + min;

		LCD_ERR("manufacture_date DSI%d = (%d%04d) - year(%d) month(%d) day(%d) hour(%d) min(%d)\n",
			ctrl->ndx, vdd->manufacture_date_dsi[ctrl->ndx], vdd->manufacture_time_dsi[ctrl->ndx],
			year, month, day, hour, min);

	} else {
		LCD_ERR("DSI%d no manufacture_date_rx_cmds cmds(%d)",  ctrl->ndx,vdd->panel_revision);
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

	char elvss_3th_val, elvss_24th_val, elvss_25th_val;
	char elvss_443_offset, elvss_465_offset, elvss_488_offset, elvss_510_offset, elvss_533_offset;
	char elvss_555_offset, elvss_578_offset, elvss_600_offset;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_etc_cmds)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx cmd : 0x%zx", (size_t)ctrl, (size_t)vdd, (size_t)hbm_etc_cmds);
		return NULL;
	}

	elvss_3th_val = elvss_24th_val = elvss_25th_val = 0;

	/* OTP value - B5 25th */
	elvss_24th_val = vdd->display_status_dsi[ctrl->ndx].elvss_value1;
	elvss_25th_val = vdd->display_status_dsi[ctrl->ndx].elvss_value2;

	/* ELVSS 0xB5 3th*/
	elvss_443_offset = 0x15;
	elvss_465_offset = 0x13;
	elvss_488_offset = 0x12;
	elvss_510_offset = 0x10;
	elvss_533_offset = 0x0E;
	elvss_555_offset = 0x0D;
	elvss_578_offset = 0x0B;
	elvss_600_offset = 0x0A;

	/* ELVSS 0xB5 3th*/
	if (vdd->auto_brightness == HBM_MODE) /* 465CD */
		elvss_3th_val = elvss_465_offset;
	else if (vdd->auto_brightness == HBM_MODE + 1) /* 488CD */
		elvss_3th_val = elvss_488_offset;
	else if (vdd->auto_brightness == HBM_MODE + 2) /* 510CD */
		elvss_3th_val = elvss_510_offset;
	else if (vdd->auto_brightness == HBM_MODE + 3) /* 533CD */
		elvss_3th_val = elvss_533_offset;
	else if (vdd->auto_brightness == HBM_MODE + 4) /* 555CD */
		elvss_3th_val= elvss_555_offset;
	else if (vdd->auto_brightness == HBM_MODE + 5) /* 578CD */
		elvss_3th_val = elvss_578_offset;
	else if (vdd->auto_brightness == HBM_MODE + 6) /* 600CD */
		elvss_3th_val = elvss_600_offset;
	else if (vdd->auto_brightness == HBM_MODE + 7) /* 443CD */
		elvss_3th_val = elvss_443_offset;

	if (vdd->bl_level == 366) {
		elvss_3th_val = 0x00;
		LCD_INFO("bl_level is (%d), so elvss_3th : 0x00 for FingerPrint\n", vdd->bl_level);
	}

	/* 0xB5 2nd temperature */
	hbm_etc_cmds->cmds[1].payload[1] =
			vdd->temperature > 0 ? vdd->temperature : 0x80|(-1*vdd->temperature);

	/* ELVSS 0xB5 3th, elvss_24th_val */
	hbm_etc_cmds->cmds[1].payload[3] = elvss_3th_val;
	hbm_etc_cmds->cmds[3].payload[1] = elvss_25th_val;

	*level_key = LEVEL1_KEY;

	LCD_INFO("0xB5 3th: 0x%x 0xB5 elvss_25th_val(elvss val) 0x%x\n", elvss_3th_val, elvss_25th_val);

	return hbm_etc_cmds;
}

static int mdss_elvss_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char elvss_b5[2];
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (B5h 24th,25th) for elvss*/
	mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_ELVSS),
		elvss_b5, LEVEL1_KEY);
	vdd->display_status_dsi[ctrl->ndx].elvss_value1 = elvss_b5[0]; /*0xB5 24th OTP value*/
	vdd->display_status_dsi[ctrl->ndx].elvss_value2 = elvss_b5[1]; /*0xB5 25th */

	return true;
}

static int mdss_hbm_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char hbm_buffer1[33];
	struct dsi_panel_cmds *hbm_gamma_cmds = get_panel_tx_cmds(ctrl, TX_HBM_GAMMA);
	int loop;

	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_gamma_cmds)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx cmds : 0x%zx", (size_t)ctrl, (size_t)vdd, (size_t)hbm_gamma_cmds);
		return false;
	}

	/* Read mtp (B3h 3~35th) for hbm gamma */
	mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_HBM),
		hbm_buffer1, LEVEL1_KEY);

	for (loop = 0; loop < 33; loop++) {
		if (loop == 0) {
			/* V255 RGB MSB */
			hbm_gamma_cmds->cmds[0].payload[1] = (hbm_buffer1[loop] & 0x04) >> 2;
			hbm_gamma_cmds->cmds[0].payload[3] = (hbm_buffer1[loop] & 0x02) >> 1;
			hbm_gamma_cmds->cmds[0].payload[5] = hbm_buffer1[loop] & 0x01;
		} else if (loop == 1) {
			/* V255 R LSB */
			hbm_gamma_cmds->cmds[0].payload[2] = hbm_buffer1[loop];
		} else if (loop == 2) {
			/* V255 G LSB */
			hbm_gamma_cmds->cmds[0].payload[4] = hbm_buffer1[loop];
		} else if (loop == 3) {
			/* V255 B LSB */
			hbm_gamma_cmds->cmds[0].payload[6] = hbm_buffer1[loop];
		} else {
			/* +3 means V255 RGB MSB */
			hbm_gamma_cmds->cmds[0].payload[loop + 3] = hbm_buffer1[loop];
		}
	}

	return true;
}

#define COORDINATE_DATA_SIZE 6
#define MDNIE_SCR_WR_ADDR 	0x32
#define RGB_INDEX_SIZE 4
#define COEFFICIENT_DATA_SIZE 8

#define F1(x,y) (((y << 10) - (((x << 10) * 49) / 46) + (39 << 10)) >> 10)
#define F2(x,y) (((y << 10) + (((x << 10) * 104) / 29) - (13876 << 10)) >> 10)

static char coordinate_data_1[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xfe, 0x00, 0xfa, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_2 */
	{0xf9, 0x00, 0xfb, 0x00, 0xff, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xfe, 0x00, 0xfa, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_5 */
	{0xf9, 0x00, 0xfb, 0x00, 0xff, 0x00}, /* Tune_6 */
	{0xfb, 0x00, 0xff, 0x00, 0xf8, 0x00}, /* Tune_7 */
	{0xfa, 0x00, 0xff, 0x00, 0xfb, 0x00}, /* Tune_8 */
	{0xf8, 0x00, 0xff, 0x00, 0xff, 0x00}, /* Tune_9 */
};

static char coordinate_data_2[][COORDINATE_DATA_SIZE] = {
	{0xff, 0x00, 0xff, 0x00, 0xff, 0x00}, /* dummy */
	{0xff, 0x00, 0xf5, 0x00, 0xec, 0x00}, /* Tune_1 */
	{0xff, 0x00, 0xf6, 0x00, 0xef, 0x00}, /* Tune_2 */
	{0xff, 0x00, 0xf7, 0x00, 0xf4, 0x00}, /* Tune_3 */
	{0xff, 0x00, 0xf8, 0x00, 0xe9, 0x00}, /* Tune_4 */
	{0xff, 0x00, 0xfa, 0x00, 0xf1, 0x00}, /* Tune_5 */
	{0xff, 0x00, 0xfc, 0x00, 0xf5, 0x00}, /* Tune_6 */
	{0xff, 0x00, 0xfe, 0x00, 0xee, 0x00}, /* Tune_7 */
	{0xff, 0x00, 0xff, 0x00, 0xf0, 0x00}, /* Tune_8 */
	{0xfe, 0x00, 0xff, 0x00, 0xf4, 0x00}, /* Tune_9 */
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
	{       0,      0,      0,      0,      0,      0,      0,      0 }, /* dummy */
	{   95580,  78227, -25243, -30003,  29939,  59065, -14830, -13912 },
	{ -102381, -84518,  27387,  32046, -53136, -18756,  11837,  10913 },
	{  -95339, -79478,  25370,  30208, -48165, -77998,  21447,  19240 },
	{   91071,  75568, -24062, -28903,  18369, -13395,   -718,   -617 },
};

static int mdnie_coordinate_index(int x, int y)
{
	int tune_number = 0;

	if (F1(x,y) > 0) {
		if (F2(x,y) > 0) {
			tune_number = 1;
		} else {
			tune_number = 2;
		}
	} else {
		if (F2(x,y) > 0) {
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

	if(result < 0)
		result = 0;
	if(result > 1024)
		result = 1024;

	return result;
}

static int mdnie_coordinate_y(int x, int y, int index)
{
	int result = 0;

	result = (coefficient[index][4] * x) + (coefficient[index][5] * y) + (((coefficient[index][6] * x + 512) >> 10) * y) + (coefficient[index][7] * 10000);

	result = (result + 512) >> 10;

	if(result < 0)
		result = 0;
	if(result > 1024)
		result = 1024;

	return result;
}

static int mdss_mdnie_read(struct mdss_dsi_ctrl_pdata *ctrl)
{
	char x_y_location[4];
	int x, y;
	int mdnie_tune_index = 0;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	/* Read mtp (A1h 1~5th) for ddi id */
	if (get_panel_rx_cmds(ctrl, RX_MDNIE)->cmd_cnt) {
		mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_MDNIE),
			x_y_location, LEVEL1_KEY);

		vdd->mdnie_x[ctrl->ndx] = x_y_location[0] << 8 | x_y_location[1];	/* X */
		vdd->mdnie_y[ctrl->ndx] = x_y_location[2] << 8 | x_y_location[3];	/* Y */

		mdnie_tune_index = mdnie_coordinate_index(vdd->mdnie_x[ctrl->ndx], vdd->mdnie_y[ctrl->ndx]);

		if (((vdd->mdnie_x[ctrl->ndx] - 2991) * (vdd->mdnie_x[ctrl->ndx] - 2991) + (vdd->mdnie_y[ctrl->ndx] - 3148) * (vdd->mdnie_y[ctrl->ndx] - 3148)) <= 225) {
			x = 0;
			y = 0;
		}
		else {
			x = mdnie_coordinate_x(vdd->mdnie_x[ctrl->ndx], vdd->mdnie_y[ctrl->ndx],mdnie_tune_index );
			y = mdnie_coordinate_y(vdd->mdnie_x[ctrl->ndx], vdd->mdnie_y[ctrl->ndx],mdnie_tune_index );
		}

		coordinate_tunning_calculate(ctrl->ndx, x, y, coordinate_data, rgb_index[mdnie_tune_index],
			MDNIE_SCR_WR_ADDR, COORDINATE_DATA_SIZE);

		LCD_INFO("DSI%d : X-%d Y-%d \n", ctrl->ndx,
			vdd->mdnie_x[ctrl->ndx], vdd->mdnie_y[ctrl->ndx]);
	} else {
		LCD_ERR("DSI%d no mdnie_read_rx_cmds cmds", ctrl->ndx);
		return false;
	}

	return true;
}

static int mdss_samart_dimming_init(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	vdd->smart_dimming_dsi[ctrl->ndx] = vdd->panel_func.samsung_smart_get_conf();

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi[ctrl->ndx])) {
		LCD_ERR("DSI%d smart_dimming_dsi is null", ctrl->ndx);
		return false;
	} else {
		mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_SMART_DIM_MTP),
			vdd->smart_dimming_dsi[ctrl->ndx]->mtp_buffer, LEVEL1_KEY);

		/* Initialize smart dimming related things here */
		/* lux_tab setting for 350cd */
		vdd->smart_dimming_dsi[ctrl->ndx]->lux_tab = vdd->dtsi_data[ctrl->ndx].candela_map_table[vdd->panel_revision].cd;
		vdd->smart_dimming_dsi[ctrl->ndx]->lux_tabsize = vdd->dtsi_data[ctrl->ndx].candela_map_table[vdd->panel_revision].tab_size;
		vdd->smart_dimming_dsi[ctrl->ndx]->man_id = vdd->manufacture_id_dsi[ctrl->ndx];
		if (vdd->panel_func.samsung_panel_revision)
			vdd->smart_dimming_dsi[ctrl->ndx]->panel_revision = vdd->panel_func.samsung_panel_revision(ctrl);

		/* copy hbm gamma payload for hbm interpolation calc */
		vdd->smart_dimming_dsi[ctrl->ndx]->hbm_payload = &get_panel_tx_cmds(ctrl, TX_HBM_GAMMA)->cmds[0].payload[1];

		/* Just a safety check to ensure smart dimming data is initialised well */
		vdd->smart_dimming_dsi[ctrl->ndx]->init(vdd->smart_dimming_dsi[ctrl->ndx]);

		vdd->temperature = 20; // default temperature

		vdd->smart_dimming_loaded_dsi[ctrl->ndx] = true;
	}

	LCD_INFO("DSI%d : --\n", ctrl->ndx);

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

	/* Read Panel Unique OCTA ID (C9h 2nd~21th) */
	if (get_panel_rx_cmds(ctrl, RX_OCTA_ID)->cmd_cnt) {
		memset(octa_id_buffer, 0x00, MAX_OCTA_ID);

		mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_OCTA_ID),
			octa_id_buffer, LEVEL1_KEY);

		for(loop = 0; loop < MAX_OCTA_ID; loop++)
			vdd->octa_id_dsi[ctrl->ndx][loop] = octa_id_buffer[loop];

		LCD_INFO("DSI%d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			ctrl->ndx, vdd->octa_id_dsi[ctrl->ndx][0], vdd->octa_id_dsi[ctrl->ndx][1],
			vdd->octa_id_dsi[ctrl->ndx][2],	vdd->octa_id_dsi[ctrl->ndx][3],
			vdd->octa_id_dsi[ctrl->ndx][4],	vdd->octa_id_dsi[ctrl->ndx][5],
			vdd->octa_id_dsi[ctrl->ndx][6],	vdd->octa_id_dsi[ctrl->ndx][7],
			vdd->octa_id_dsi[ctrl->ndx][8],	vdd->octa_id_dsi[ctrl->ndx][9],
			vdd->octa_id_dsi[ctrl->ndx][10],vdd->octa_id_dsi[ctrl->ndx][11],
			vdd->octa_id_dsi[ctrl->ndx][12],vdd->octa_id_dsi[ctrl->ndx][13],
			vdd->octa_id_dsi[ctrl->ndx][14],vdd->octa_id_dsi[ctrl->ndx][15],
			vdd->octa_id_dsi[ctrl->ndx][16],vdd->octa_id_dsi[ctrl->ndx][17],
			vdd->octa_id_dsi[ctrl->ndx][18],vdd->octa_id_dsi[ctrl->ndx][19]);

	} else {
		LCD_ERR("DSI%d no octa_id_rx_cmds cmd\n", ctrl->ndx);
		return false;
	}

	return true;
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
	aid_cmd.cmds = &(get_panel_tx_cmds(ctrl, TX_AID_SUBDIVISION)->cmds[cd_index]);
	LCD_DEBUG("[%d] level(%d), aid(%x %x)\n", cd_index, vdd->bl_level, aid_cmd.cmds->payload[1], aid_cmd.cmds->payload[2]);

	*level_key = LEVEL1_KEY;

	return &aid_cmd;
}

#define GRADUAL_ACL_STEP 6 //refer config_display.xml
static struct dsi_panel_cmds *mdss_acl_on(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	u8 acl_value[GRADUAL_ACL_STEP] = {0x00, 0x02, 0x04, 0x07, 0x10, 0x14};
	int step;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	*level_key = LEVEL1_KEY;

	if (vdd->gradual_acl_val
			&& vdd->gradual_acl_val < GRADUAL_ACL_STEP)
		step = vdd->gradual_acl_val;
	else
		step = GRADUAL_ACL_STEP - 1; /* default value */

	if (vdd->gradual_acl_val)
		get_panel_tx_cmds(ctrl, TX_ACL_ON)->cmds[0].payload[7] = acl_value[step];

	return get_panel_tx_cmds(ctrl, TX_ACL_ON);
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

#if 0
static struct dsi_panel_cmds acl_percent_cmd;
static struct dsi_panel_cmds * mdss_acl_precent(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int cd_index = 0;
	int cmd_idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	cd_index = get_cmd_index(vdd, ctrl->ndx);

	if (!vdd->dtsi_data[ctrl->ndx].acl_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data[ctrl->ndx].acl_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data[ctrl->ndx].acl_map_table[vdd->panel_revision].cmd_idx[cd_index];

	acl_percent_cmd.cmds = &(vdd->dtsi_data[ctrl->ndx].acl_percent_tx_cmds[vdd->panel_revision].cmds[cmd_idx]);
	acl_percent_cmd.cmd_cnt = 1;

	*level_key = LEVEL1_KEY;

	return &acl_percent_cmd;

end :
	LCD_ERR("error");
	return NULL;

}
#endif

static struct dsi_panel_cmds elvss_cmd;
static struct dsi_panel_cmds * mdss_elvss(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *elvss_cmds = get_panel_tx_cmds(ctrl, TX_ELVSS);
	int cd_index = 0;
	int cmd_idx = 0;
	char elvss_3th_val;
	char elvss_3th_val_array[SUPPORT_LOWTEMP_ELVSS][MAX_TEMP];
	char panel_revision = 'A';

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(elvss_cmds)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx cmds : 0x%zx", (size_t)ctrl, (size_t)vdd, (size_t)elvss_cmds);
		return NULL;
	}

	panel_revision += vdd->panel_revision;

	switch (panel_revision) {
		case 'A' ... 'H':
		default:
			elvss_3th_val_array[14][HIGH_TEMP] = 0x15;
			elvss_3th_val_array[14][MID_TEMP] = 0x14;
			elvss_3th_val_array[14][LOW_TEMP] = 0x17;
			elvss_3th_val_array[13][HIGH_TEMP] = 0x13;
			elvss_3th_val_array[13][MID_TEMP] = 0x14;
			elvss_3th_val_array[13][LOW_TEMP] = 0x17;
			elvss_3th_val_array[12][HIGH_TEMP] = 0x11;
			elvss_3th_val_array[12][MID_TEMP] = 0x14;
			elvss_3th_val_array[12][LOW_TEMP] = 0x17;
			elvss_3th_val_array[11][HIGH_TEMP] = 0x0F;
			elvss_3th_val_array[11][MID_TEMP] = 0x14;
			elvss_3th_val_array[11][LOW_TEMP] = 0x17;
			elvss_3th_val_array[10][HIGH_TEMP] = 0x0D;
			elvss_3th_val_array[10][MID_TEMP] = 0x14;
			elvss_3th_val_array[10][LOW_TEMP] = 0x17;
			elvss_3th_val_array[9][HIGH_TEMP] = 0x0C;
			elvss_3th_val_array[9][MID_TEMP] = 0x14;
			elvss_3th_val_array[9][LOW_TEMP] = 0x17;
			elvss_3th_val_array[8][HIGH_TEMP] = 0x0B;
			elvss_3th_val_array[8][MID_TEMP] = 0x10;
			elvss_3th_val_array[8][LOW_TEMP] = 0x17;
			elvss_3th_val_array[7][HIGH_TEMP] = 0x0A;
			elvss_3th_val_array[7][MID_TEMP] = 0x10;
			elvss_3th_val_array[7][LOW_TEMP] = 0x17;
			elvss_3th_val_array[6][HIGH_TEMP] = 0x09;
			elvss_3th_val_array[6][MID_TEMP] = 0x10;
			elvss_3th_val_array[6][LOW_TEMP] = 0x17;
			elvss_3th_val_array[5][HIGH_TEMP] = 0x08;
			elvss_3th_val_array[5][MID_TEMP] = 0x10;
			elvss_3th_val_array[5][LOW_TEMP] = 0x17;
			elvss_3th_val_array[4][HIGH_TEMP] = 0x08;
			elvss_3th_val_array[4][MID_TEMP] = 0x10;
			elvss_3th_val_array[4][LOW_TEMP] = 0x17;
			elvss_3th_val_array[3][HIGH_TEMP] = 0x07;
			elvss_3th_val_array[3][MID_TEMP] = 0x10;
			elvss_3th_val_array[3][LOW_TEMP] = 0x17;
			elvss_3th_val_array[2][HIGH_TEMP] = 0x07;
			elvss_3th_val_array[2][MID_TEMP] = 0x10;
			elvss_3th_val_array[2][LOW_TEMP] = 0x17;

			/* 1CD and 0CD is not used */
			elvss_3th_val_array[1][HIGH_TEMP] = 0x07;
			elvss_3th_val_array[1][MID_TEMP] = 0x10;
			elvss_3th_val_array[1][LOW_TEMP] = 0x17;
			elvss_3th_val_array[0][HIGH_TEMP] = 0x07;
			elvss_3th_val_array[0][MID_TEMP] = 0x10;
			elvss_3th_val_array[0][LOW_TEMP] = 0x17;
			break;
	}

	cd_index  = vdd->cd_idx;
	LCD_DEBUG("cd_index (%d)\n", cd_index);

	if (!vdd->dtsi_data[ctrl->ndx].smart_acl_elvss_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data[ctrl->ndx].smart_acl_elvss_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data[ctrl->ndx].smart_acl_elvss_map_table[vdd->panel_revision].cmd_idx[cd_index];

	/* ELVSS Compensation for Low Temperature & Low Birghtness*/
	if ((vdd->temperature <= 0) && (vdd->candela_level <= 14)) {
		if (vdd->temperature > vdd->elvss_interpolation_temperature)
			elvss_3th_val =
				elvss_3th_val_array[vdd->candela_level][MID_TEMP];
		else
			elvss_3th_val =
				elvss_3th_val_array[vdd->candela_level][LOW_TEMP];

		LCD_DEBUG("temperature(%d) level(%d):B5 3th (0x%x)\n", vdd->temperature, vdd->candela_level, elvss_3th_val);
		elvss_cmds->cmds[cmd_idx].payload[3] = elvss_3th_val;
	} else if ((vdd->temperature > 0) && (vdd->candela_level <= 14)) {
		elvss_3th_val =
				elvss_3th_val_array[vdd->candela_level][HIGH_TEMP];
		LCD_DEBUG("temperature(%d) level(%d):B5 3th (0x%x)\n", vdd->temperature, vdd->candela_level, elvss_3th_val);
		elvss_cmds->cmds[cmd_idx].payload[3] = elvss_3th_val;
	}

	elvss_cmd.cmds = &(elvss_cmds->cmds[cmd_idx]);
	elvss_cmd.cmd_cnt = 1;
	*level_key = LEVEL1_KEY;

	return &elvss_cmd;

end :
	LCD_ERR("error");
	return NULL;
}

static struct dsi_panel_cmds * mdss_elvss_temperature1(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *cmds = get_panel_tx_cmds(ctrl, TX_ELVSS_LOWTEMP);
	char elvss_24th_val;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(cmds)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx cmds : 0x%zx", (size_t)ctrl, (size_t)vdd, (size_t)cmds);
		return NULL;
	}

	/* OTP value - B5 24th */
	elvss_24th_val = vdd->display_status_dsi[ctrl->ndx].elvss_value1;

	LCD_DEBUG("OTP val %x\n", elvss_24th_val);

	/* 0xB5 1th TSET */
	cmds->cmds[0].payload[1] =
		vdd->temperature > 0 ? vdd->temperature : 0x80|(-1*vdd->temperature);

	/* 0xB5 elvss_24th_val elvss_offset */
	cmds->cmds[2].payload[1] = elvss_24th_val;

	LCD_DEBUG("acl : %d, interpolation_temp : %d temp : %d, cd : %d, B5 1st :0x%x, B5 elvss_24th_val :0x%x\n",
		vdd->acl_status, vdd->elvss_interpolation_temperature, vdd->temperature, vdd->candela_level,
		cmds->cmds[0].payload[1],
		cmds->cmds[2].payload[1]);

	*level_key = LEVEL1_KEY;

	return cmds;
}

static struct dsi_panel_cmds vint_cmd;
static struct dsi_panel_cmds *mdss_vint(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *vint_cmds = get_panel_tx_cmds(ctrl, TX_VINT);
	int cd_index = 0;
	int cmd_idx = 0;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vint_cmds)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx cmds : 0x%zx", (size_t)ctrl, (size_t)vdd, (size_t)vint_cmds);
		return NULL;
	}

	cd_index = vdd->cd_idx;
	LCD_DEBUG("cd_index (%d)\n", cd_index);

	if (!vdd->dtsi_data[ctrl->ndx].vint_map_table[vdd->panel_revision].size ||
		cd_index > vdd->dtsi_data[ctrl->ndx].vint_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data[ctrl->ndx].vint_map_table[vdd->panel_revision].cmd_idx[cd_index];

	if (vdd->temperature > vdd->elvss_interpolation_temperature )
		vint_cmd.cmds = &vint_cmds->cmds[cmd_idx];
	else
		vint_cmd.cmds = &vint_cmds->cmds[0];

	if (vdd->xtalk_mode) {
		vint_cmd.cmds[0].payload[1]= 0x6B;	// VGH 6.2 V
	}
	else {
		vint_cmd.cmds[0].payload[1]= 0xEB;	// VGH 7.0 V
	}

	vint_cmd.cmd_cnt = 1;
	*level_key = LEVEL1_KEY;

	return &vint_cmd;

end :
	LCD_ERR("error");
	return NULL;
}

static struct dsi_panel_cmds irc_cmd;
static struct dsi_panel_cmds *mdss_irc(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds *irc_cmds = get_panel_tx_cmds(ctrl, TX_IRC_SUBDIVISION);
	int cd_index = 0;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(irc_cmds)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx cmds : 0x%zx", (size_t)ctrl, (size_t)vdd, (size_t)irc_cmds);
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
		cd_index = vdd->bl_level;

	LCD_DEBUG("irc idx (%d)\n", cd_index);

	irc_cmd.cmds = &(irc_cmds->cmds[cd_index]);
	irc_cmd.cmd_cnt = 1;
	*level_key = LEVEL1_KEY;

	return &irc_cmd;
}

static char irc_hbm_para_revD[8][20] = {
	{0x0D, 0xF0, 0x3B, 0x33, 0x5F, 0xD4, 0x33, 0x69, 0x12, 0x7A, 0xA5, 0x7F, 0x81, 0x7F, 0x2B, 0x2A, 0x2B, 0x29, 0x2B, 0x29}, // 600 ( 365 ~ 365 )
	{0x0D, 0xF0, 0x3B, 0x33, 0x5F, 0xD4, 0x33, 0x69, 0x12, 0x7A, 0xA5, 0x7A, 0x7C, 0x7A, 0x2A, 0x29, 0x2A, 0x28, 0x29, 0x28}, // 578 ( 351 ~ 364 )
	{0x0D, 0xF0, 0x3B, 0x33, 0x5F, 0xD4, 0x33, 0x69, 0x12, 0x7A, 0xA5, 0x76, 0x77, 0x76, 0x27, 0x28, 0x27, 0x27, 0x27, 0x27}, // 555 ( 337 ~ 350 )
	{0x0D, 0xF0, 0x3B, 0x33, 0x5F, 0xD4, 0x33, 0x69, 0x12, 0x7A, 0xA5, 0x71, 0x72, 0x71, 0x26, 0x26, 0x26, 0x25, 0x26, 0x25}, // 533 ( 324 ~ 336 )
	{0x0D, 0xF0, 0x3B, 0x33, 0x5F, 0xD4, 0x33, 0x69, 0x12, 0x7A, 0xA5, 0x6C, 0x6D, 0x6C, 0x25, 0x25, 0x25, 0x23, 0x24, 0x23}, // 510 ( 310 ~ 323 )
	{0x0D, 0xF0, 0x3B, 0x33, 0x5F, 0xD4, 0x33, 0x69, 0x12, 0x7A, 0xA5, 0x67, 0x69, 0x67, 0x23, 0x22, 0x23, 0x22, 0x23, 0x22}, // 488 ( 296 ~ 309 )
	{0x0D, 0xF0, 0x3B, 0x33, 0x5F, 0xD4, 0x33, 0x69, 0x12, 0x7A, 0xA5, 0x63, 0x64, 0x63, 0x21, 0x21, 0x21, 0x20, 0x21, 0x20}, // 465 ( 282 ~ 295 )
	{0x0D, 0xF0, 0x3B, 0x33, 0x5F, 0xD4, 0x33, 0x69, 0x12, 0x7A, 0xA5, 0x5E, 0x5F, 0x5E, 0x20, 0x20, 0x20, 0x1E, 0x1F, 0x1E}, // 443 ( 256 ~ 281 )
};

static struct dsi_panel_cmds *mdss_hbm_irc(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds * hbm_irc_cmds = get_panel_tx_cmds(ctrl, TX_HBM_IRC);
	int para_idx = 0;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hbm_irc_cmds)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx cmds : 0x%zx", (size_t)ctrl, (size_t)vdd, (size_t)hbm_irc_cmds);
		return NULL;
	}

	if (IS_ERR_OR_NULL(hbm_irc_cmds->cmds)) {
		LCD_ERR("No irc_tx_cmds\n");
		return NULL;
	}

	if (!vdd->samsung_support_irc)
		return NULL;

	*level_key = LEVEL1_KEY;

	/*auto_brightness is 13 to use 443cd of hbm step on color weakness mode*/
	if (vdd->auto_brightness == HBM_MODE + 7)
		para_idx = 7;
	else
		para_idx = vdd->auto_brightness_level - vdd->auto_brightness;

	/* copy irc default setting */
	memcpy(&hbm_irc_cmds->cmds[0].payload[1], irc_hbm_para_revD[para_idx], 20);

	return hbm_irc_cmds;
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

static int samsung_panel_off_pre(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int rc = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	return rc;
}

static int samsung_panel_off_post(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	int rc = 0;


	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	return rc;
}

// ========================
// 			HMT
// ========================
static struct dsi_panel_cmds * mdss_gamma_hmt(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds  *hmt_gamma_cmds = get_panel_tx_cmds(ctrl, TX_HMT_GAMMA);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(hmt_gamma_cmds)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx cmds : 0x%zx", (size_t)ctrl, (size_t)vdd, (size_t)hmt_gamma_cmds);
		return NULL;
	}

	LCD_DEBUG("hmt_bl_level : %d candela : %dCD\n", vdd->hmt_stat.hmt_bl_level, vdd->hmt_stat.candela_level_hmt);

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi_hmt[ctrl->ndx]->generate_gamma)) {
		LCD_ERR("generate_gamma is NULL");
		return NULL;
	} else {
		vdd->smart_dimming_dsi_hmt[ctrl->ndx]->generate_gamma(
			vdd->smart_dimming_dsi_hmt[ctrl->ndx],
			vdd->hmt_stat.candela_level_hmt,
			&hmt_gamma_cmds->cmds[0].payload[1]);

		*level_key = LEVEL1_KEY;

		return hmt_gamma_cmds;
	}
}

static struct dsi_panel_cmds hmt_aid_cmd;
static struct dsi_panel_cmds *mdss_aid_hmt(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct dsi_panel_cmds  *hmt_aid_cmds = get_panel_tx_cmds(ctrl, TX_HMT_AID);
	int cmd_idx = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	if (!vdd->dtsi_data[ctrl->ndx].hmt_reverse_aid_map_table[vdd->panel_revision].size ||
		vdd->hmt_stat.cmd_idx_hmt > vdd->dtsi_data[ctrl->ndx].hmt_reverse_aid_map_table[vdd->panel_revision].size)
		goto end;

	cmd_idx = vdd->dtsi_data[ctrl->ndx].hmt_reverse_aid_map_table[vdd->panel_revision].cmd_idx[vdd->hmt_stat.cmd_idx_hmt];

	hmt_aid_cmd.cmds = &hmt_aid_cmds->cmds[cmd_idx];
	hmt_aid_cmd.cmd_cnt = 1;

	*level_key = LEVEL1_KEY;

	return &hmt_aid_cmd;

end :
	LCD_ERR("error");
	return NULL;
}

static struct dsi_panel_cmds *mdss_elvss_hmt(struct mdss_dsi_ctrl_pdata *ctrl, int *level_key)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return NULL;
	}

	/* 0xB5 1th TSET */
	get_panel_tx_cmds(ctrl, TX_HMT_ELVSS)->cmds[0].payload[1] =
		vdd->temperature > 0 ? vdd->temperature : 0x80|(-1*vdd->temperature);

	/* ELVSS(MPS_CON) setting condition is equal to normal birghtness */ // B5 2nd para : MPS_CON
	if (vdd->hmt_stat.candela_level_hmt > 40) {
		get_panel_tx_cmds(ctrl, TX_HMT_ELVSS)->cmds[0].payload[2] = 0xDC;
	} else {
		get_panel_tx_cmds(ctrl, TX_HMT_ELVSS)->cmds[0].payload[2] = 0xCC;
	}

	*level_key = LEVEL1_KEY;

	return get_panel_tx_cmds(ctrl, TX_HMT_ELVSS);
}

static void mdss_make_sdimconf_hmt(struct mdss_dsi_ctrl_pdata *ctrl, struct samsung_display_driver_data *vdd) {
	/* Set the mtp read buffer pointer and read the NVM value*/
	mdss_samsung_panel_data_read(ctrl, get_panel_rx_cmds(ctrl, RX_SMART_DIM_MTP),
				vdd->smart_dimming_dsi_hmt[ctrl->ndx]->mtp_buffer, LEVEL1_KEY);

	/* Initialize smart dimming related things here */
	/* lux_tab setting for 350cd */
	vdd->smart_dimming_dsi_hmt[ctrl->ndx]->lux_tab = vdd->dtsi_data[ctrl->ndx].hmt_candela_map_table[vdd->panel_revision].cd;
	vdd->smart_dimming_dsi_hmt[ctrl->ndx]->lux_tabsize = vdd->dtsi_data[ctrl->ndx].hmt_candela_map_table[vdd->panel_revision].tab_size;
	vdd->smart_dimming_dsi_hmt[ctrl->ndx]->man_id = vdd->manufacture_id_dsi[ctrl->ndx];
	if (vdd->panel_func.samsung_panel_revision)
		vdd->smart_dimming_dsi_hmt[ctrl->ndx]->panel_revision = vdd->panel_func.samsung_panel_revision(ctrl);

	/* Just a safety check to ensure smart dimming data is initialised well */
	vdd->smart_dimming_dsi_hmt[ctrl->ndx]->init(vdd->smart_dimming_dsi_hmt[ctrl->ndx]);

	LCD_INFO("[HMT] smart dimming done!\n");
}

static int mdss_samart_dimming_init_hmt(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	LCD_INFO("DSI%d : ++\n", ctrl->ndx);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return false;
	}

	vdd->smart_dimming_dsi_hmt[ctrl->ndx] = vdd->panel_func.samsung_smart_get_conf_hmt();

	if (IS_ERR_OR_NULL(vdd->smart_dimming_dsi_hmt[ctrl->ndx])) {
		LCD_ERR("DSI%d error", ctrl->ndx);
		return false;
	} else {
		vdd->hmt_stat.hmt_on = 0;
		vdd->hmt_stat.hmt_bl_level = 0;
		vdd->hmt_stat.hmt_reverse = 0;
		vdd->hmt_stat.hmt_is_first = 1;

		mdss_make_sdimconf_hmt(ctrl, vdd);

		vdd->smart_dimming_hmt_loaded_dsi[ctrl->ndx] = true;
	}

	LCD_INFO("DSI%d : --\n", ctrl->ndx);

	return true;
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
	 * alpm_reg		 : Control register for ALPM/HLPM on/off
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
				(mode == HLPM_MODE_ON) ? HLPM_CTRL_2NIT : HLPM_CTRL_2NIT;
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

static int ddi_hw_cursor(struct mdss_dsi_ctrl_pdata *ctrl, int *input)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	char *payload;

	if (IS_ERR_OR_NULL(ctrl)) {
		LCD_ERR("dsi_ctrl is NULL\n");
		return 0;
	}

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("Invalid data ctrl : 0x%zx vdd : 0x%zx", (size_t)ctrl, (size_t)vdd);
		return 0;
	}

	if (IS_ERR_OR_NULL(input)) {
		LCD_ERR("input is NULL\n");
		return 0;
	}

	if (IS_ERR_OR_NULL(get_panel_tx_cmds(ctrl, TX_HW_CURSOR)->cmds[0].payload)) {
		LCD_ERR("hw_cursor_tx_cmds is NULL\n");
		return 0;
	}


	LCD_INFO("On/Off:(%d), Por/Land:(%d), On_Select:(%d), X:(%d), Y:(%d), Width:(%d), Length:(%d), Color:(0x%x), Period:(%x), TR_TIME(%x)\n",
		input[0], input[1], input[2], input[3], input[4], input[5],
		input[6], input[7], input[8], input[9]);

	payload = get_panel_tx_cmds(ctrl, TX_HW_CURSOR)->cmds[0].payload;

	/* Cursor On&Off (0:Off, 1:On) */
	payload[1] = input[0] & 0x1;

	/* 3rd bit : CURSOR_ON_SEL, 2nd bit : Port/Land, 1st bit : BLINK_ON(Default On)*/
	payload[2] = (input[2] & 0x1) << 2 | (input[1] & 0x1) << 1 | 0x1;

	/* Start X address */
	payload[3] = (input[3] & 0x700) >> 8;
	payload[4] = input[3] & 0xFF;

	/* Start Y address */
	payload[5] = (input[4] & 0x700) >> 8;
	payload[6] = input[4] & 0xFF;

	/* Width */
	payload[7] = input[5] & 0xF;

	/* Length */
	payload[8] = (input[6] & 0x100) >> 8;
	payload[9] = input[6] & 0xFF;

	/* Color */
	payload[10] = (input[7] & 0xFF0000) >> 16;
	payload[11] = (input[7] & 0xFF00) >> 8;
	payload[12] = input[7] & 0xFF;

	/* Period */
	payload[13] = input[8] & 0xFF;

	/* TR_TIME */
	payload[14] = input[9] & 0xFF;

	mdss_samsung_send_cmd(ctrl, TX_LEVEL1_KEY_ENABLE);
	mdss_samsung_send_cmd(ctrl, TX_HW_CURSOR);
	mdss_samsung_send_cmd(ctrl, TX_LEVEL1_KEY_DISABLE);

	return 1;
}

static void mdss_send_colorweakness_ccb_cmd(struct samsung_display_driver_data *vdd, int mode)
{
	struct mdss_dsi_ctrl_pdata *ctrl = samsung_get_dsi_ctrl(vdd);

	LCD_INFO("mode (%d) color_weakness_value (%x) \n", mode, vdd->color_weakness_value);

	if (mode) {
		get_panel_tx_cmds(ctrl, TX_COLOR_WEAKNESS_ENABLE)->cmds[1].payload[1] = vdd->color_weakness_value;
		mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], TX_COLOR_WEAKNESS_ENABLE);
	} else
		mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], TX_COLOR_WEAKNESS_DISABLE);
}

/*
	MULTI_RESOLUTION START
	Note : MULTI_RESOLUTION cmd & 2c image data has to be sent with exclusive_tx mode
		to prevent late 2C situation (2C should be sent in VFP period).
		MULTI_RESOLUTION END has to be invoked to terminate exclusive_tx mode later.
*/
static void mdss_panel_multires_start(struct samsung_display_driver_data *vdd)
{
	struct mdss_dsi_ctrl_pdata *ctrl;
	int i;
	ctrl = samsung_get_dsi_ctrl(vdd);

	LCD_INFO("++\n");

	if (vdd->multires_stat.prev_mode != vdd->multires_stat.curr_mode) {
		/* enter exclusive mode*/
		mutex_lock(&vdd->exclusive_tx.ex_tx_lock);

		vdd->exclusive_tx.enable = 1;

		for (i = TX_MULTIRES_FHD_TO_WQHD; i <= TX_MULTIRES_HD; i++)
			mdss_samsung_set_exclusive_tx_packet(ctrl, i, 1);

		LCD_INFO("vdd->multires_stat.prev_mode = %d, vdd-multires_stat.curr_mode = %d\n",
			vdd->multires_stat.prev_mode, vdd->multires_stat.curr_mode);
		if (vdd->multires_stat.curr_mode == MULTIRES_FHD)
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], TX_MULTIRES_FHD);
		else if (vdd->multires_stat.curr_mode == MULTIRES_HD)
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], TX_MULTIRES_HD);
		else if (vdd->multires_stat.curr_mode == MULTIRES_WQHD) {
			if(vdd->multires_stat.prev_mode == MULTIRES_FHD)
				mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], TX_MULTIRES_FHD_TO_WQHD);
			else
				mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], TX_MULTIRES_HD_TO_WQHD);
		}
	}
	LCD_INFO("--\n");
}

/*
	MULTI_RESOLUTION END
	Note : MULTI_RESOLUTION cmd & 2c image data has to be sent with exclusive_tx mode
		to prevent late 2C situation (2C should be sent in VFP period).
		MULTI_RESOLUTION END has to be invoked to terminate exclusive_tx mode after pp_done.
*/
static void mdss_panel_multires_end(struct samsung_display_driver_data *vdd)
{
	struct mdss_dsi_ctrl_pdata *ctrl;
	int i;
	ctrl = samsung_get_dsi_ctrl(vdd);

	LCD_INFO("++\n");

	/* exit exclusive mode*/
	for (i = TX_MULTIRES_FHD_TO_WQHD; i <= TX_MULTIRES_HD; i++)
		mdss_samsung_set_exclusive_tx_packet(ctrl, i, 0);

	vdd->multires_stat.prev_mode = vdd->multires_stat.curr_mode;
	vdd->exclusive_tx.enable = 0;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up(&vdd->exclusive_tx.ex_tx_waitq);
	LCD_INFO("--\n");
}

#if 0
static void mdss_panel_cover_control(struct mdss_dsi_ctrl_pdata *ctrl, struct samsung_display_driver_data *vdd)
{
	if (IS_ERR_OR_NULL(ctrl)) {
		LCD_ERR("dsi_ctrl is NULL\n");
		return;
	}

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("vdd is NULL\n");
		return;
	}

	if (vdd->cover_control) {
		mdss_samsung_send_cmd(ctrl, TX_COVER_CONTROL_ENABLE);
		LCD_INFO("Send Cover Contorl Enable CMD\n");
	} else {
		mdss_samsung_send_cmd(ctrl, TX_COVER_CONTROL_DISABLE);
		LCD_INFO("Send Cover Contorl Disable CMD\n");
	}
}
#endif

#if 0
static void samsung_copr_enable(struct samsung_display_driver_data *vdd, int enable)
{
	if (vdd->copr.copr_on == enable) {
		LCD_ERR("copr already %d..\n", vdd->copr.copr_on);
		return;
	}

	mutex_lock(&vdd->copr.copr_lock);

	if (enable) {
		if (gpio_direction_output(vdd->copr.copr_spi_gpio.clk, 0))
			LCD_ERR("failed to set copr spi (clk) to output \n");
		if (gpio_direction_output(vdd->copr.copr_spi_gpio.cs, 0))
			LCD_ERR("failed to set copr spi (cs) to output \n");
		if (gpio_direction_output(vdd->copr.copr_spi_gpio.mosi, 0))
			LCD_ERR("failed to set copr spi (mosi) to output \n");
		if (gpio_direction_input(vdd->copr.copr_spi_gpio.miso))
			LCD_ERR("failed to set copr spi (miso) to input \n");
	} else {
		if (gpio_direction_input(vdd->copr.copr_spi_gpio.clk))
			LCD_ERR("failed to set copr spi (clk) to input \n");
		if (gpio_direction_input(vdd->copr.copr_spi_gpio.cs))
			LCD_ERR("failed to set copr spi (cs) to input \n");
		if (gpio_direction_input(vdd->copr.copr_spi_gpio.mosi))
			LCD_ERR("failed to set copr spi (mosi) to input \n");
		if (gpio_direction_output(vdd->copr.copr_spi_gpio.miso, 0))
			LCD_ERR("failed to set copr spi (miso) to output \n");

		vdd->copr.copr_sum = 0;
		vdd->copr.cd_sum = 0;
		vdd->copr.total_t = 0;
	}

	vdd->copr.copr_on = enable;

	LCD_INFO("copr %s .. \n", vdd->copr.copr_on?"enabled..":"disabled..");

	mutex_unlock(&vdd->copr.copr_lock);

	return;
}

static void samsung_set_copr_sum(struct samsung_display_driver_data *vdd)
{
	s64 delta;

	vdd->copr.last_t = vdd->copr.cur_t;
	vdd->copr.cur_t = ktime_get();
	delta = ktime_ms_delta(vdd->copr.cur_t, vdd->copr.last_t);
	vdd->copr.total_t += delta;
	vdd->copr.copr_sum += (vdd->copr.copr_data * delta);
	vdd->copr.cd_sum += (vdd->interpolation_cd * delta);

	LCD_DEBUG("copr(%d) cd(%d) delta (%lld) copr_sum (%lld) cd_sum (%lld) total_t (%lld)\n",
			vdd->copr.copr_data, vdd->interpolation_cd, delta,
			vdd->copr.copr_sum, vdd->copr.cd_sum, vdd->copr.total_t);
}

static void samsung_copr_read(struct samsung_display_driver_data *vdd)
{
	int size;
	u8 read_addr;
	u8 rxbuf[1];
	int ret = 0;

	LCD_DEBUG("%s ++ \n", __func__);

	/* COPR read addr */
	read_addr = 0x5A;
	/* COPR read size */
	size = 1;

	ret = ss_spi_read(vdd->spi_dev, read_addr, rxbuf, size);
	if (!ret) {
		pr_err("[MDSS SPI] %s : spi read fail..(%x)\n", __func__, read_addr);
		return;
	}

	if (vdd->copr.copr_data && vdd->display_status_dsi[DISPLAY_1].wait_actual_disp_on) {
		LCD_INFO("ACTUAL_DISPLAY_ON\n");
		vdd->display_status_dsi[DISPLAY_1].wait_actual_disp_on = false;
	}

	vdd->copr.copr_data = rxbuf[0];

	LCD_DEBUG("%s -- data (%d)\n", __func__, vdd->copr.copr_data);

	return;
}

static void samsung_read_copr_work(struct work_struct *work)
{
	struct samsung_display_driver_data *vdd = NULL;
	struct COPR *copr;

	copr = container_of(work, struct COPR, read_copr_work);
	vdd = container_of(copr, struct samsung_display_driver_data, copr);

	LCD_DEBUG("copr_calc work!!\n");

	mutex_lock(&vdd->copr.copr_lock);

	if (vdd->panel_func.samsung_copr_read) {
		vdd->panel_func.samsung_set_copr_sum(vdd);
		vdd->panel_func.samsung_copr_read(vdd);
		LCD_DEBUG("COPR : %02x (%d) \n", vdd->copr.copr_data, vdd->copr.copr_data);
	}

	mutex_unlock(&vdd->copr.copr_lock);

	return;
}
#endif

static void dsi_update_mdnie_data(void)
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
	mdnie_data.DSI0_CAMERA_AUTO_MDNIE_2 = DSI0_CAMERA_AUTO_MDNIE_2;
	mdnie_data.DSI0_GALLERY_DYNAMIC_MDNIE_2 = DSI0_GALLERY_DYNAMIC_MDNIE_2;
	mdnie_data.DSI0_GALLERY_STANDARD_MDNIE_2 = DSI0_GALLERY_STANDARD_MDNIE_2;
	mdnie_data.DSI0_GALLERY_AUTO_MDNIE_2 = DSI0_GALLERY_AUTO_MDNIE_2;
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
	mdnie_data.DSI0_UI_AUTO_MDNIE = DSI0_UI_AUTO_MDNIE;
	mdnie_data.DSI0_VIDEO_DYNAMIC_MDNIE = DSI0_VIDEO_DYNAMIC_MDNIE;
	mdnie_data.DSI0_VIDEO_STANDARD_MDNIE = DSI0_VIDEO_STANDARD_MDNIE;
	mdnie_data.DSI0_VIDEO_NATURAL_MDNIE = DSI0_VIDEO_NATURAL_MDNIE;
	mdnie_data.DSI0_VIDEO_AUTO_MDNIE = DSI0_VIDEO_AUTO_MDNIE;
	mdnie_data.DSI0_CAMERA_AUTO_MDNIE = DSI0_CAMERA_AUTO_MDNIE;
	mdnie_data.DSI0_GALLERY_DYNAMIC_MDNIE = DSI0_GALLERY_DYNAMIC_MDNIE;
	mdnie_data.DSI0_GALLERY_STANDARD_MDNIE = DSI0_GALLERY_STANDARD_MDNIE;
	mdnie_data.DSI0_GALLERY_NATURAL_MDNIE = DSI0_GALLERY_NATURAL_MDNIE;
	mdnie_data.DSI0_GALLERY_AUTO_MDNIE = DSI0_GALLERY_AUTO_MDNIE;
	mdnie_data.DSI0_BROWSER_DYNAMIC_MDNIE = DSI0_BROWSER_DYNAMIC_MDNIE;
	mdnie_data.DSI0_BROWSER_STANDARD_MDNIE = DSI0_BROWSER_STANDARD_MDNIE;
	mdnie_data.DSI0_BROWSER_NATURAL_MDNIE = DSI0_BROWSER_NATURAL_MDNIE;
	mdnie_data.DSI0_BROWSER_AUTO_MDNIE = DSI0_BROWSER_AUTO_MDNIE;
	mdnie_data.DSI0_EBOOK_DYNAMIC_MDNIE = DSI0_EBOOK_DYNAMIC_MDNIE;
	mdnie_data.DSI0_EBOOK_STANDARD_MDNIE = DSI0_EBOOK_STANDARD_MDNIE;
	mdnie_data.DSI0_EBOOK_NATURAL_MDNIE = DSI0_EBOOK_NATURAL_MDNIE;
	mdnie_data.DSI0_EBOOK_AUTO_MDNIE = DSI0_EBOOK_AUTO_MDNIE;
	mdnie_data.DSI0_EMAIL_AUTO_MDNIE = DSI0_EMAIL_AUTO_MDNIE;
	mdnie_data.DSI0_GAME_LOW_MDNIE = DSI0_GAME_LOW_MDNIE;
	mdnie_data.DSI0_GAME_MID_MDNIE = DSI0_GAME_MID_MDNIE;
	mdnie_data.DSI0_GAME_HIGH_MDNIE = DSI0_GAME_HIGH_MDNIE;
	mdnie_data.DSI0_TDMB_DYNAMIC_MDNIE = DSI0_TDMB_DYNAMIC_MDNIE;
	mdnie_data.DSI0_TDMB_STANDARD_MDNIE = DSI0_TDMB_STANDARD_MDNIE;
	mdnie_data.DSI0_TDMB_NATURAL_MDNIE = DSI0_TDMB_NATURAL_MDNIE;
	mdnie_data.DSI0_TDMB_AUTO_MDNIE = DSI0_TDMB_AUTO_MDNIE;
	mdnie_data.DSI0_GRAYSCALE_MDNIE = DSI0_GRAYSCALE_MDNIE;
	mdnie_data.DSI0_GRAYSCALE_NEGATIVE_MDNIE= DSI0_GRAYSCALE_NEGATIVE_MDNIE;
	mdnie_data.DSI0_CURTAIN = DSI0_SCREEN_CURTAIN_MDNIE;
	mdnie_data.DSI0_NIGHT_MODE_MDNIE = DSI0_NIGHT_MODE_MDNIE;
	mdnie_data.DSI0_NIGHT_MODE_MDNIE_SCR = DSI0_NIGHT_MODE_MDNIE_1;
	mdnie_data.DSI0_COLOR_LENS_MDNIE = DSI0_COLOR_LENS_MDNIE;
	mdnie_data.DSI0_COLOR_LENS_MDNIE_SCR = DSI0_COLOR_LENS_MDNIE_1;
	mdnie_data.DSI0_COLOR_BLIND_MDNIE_SCR = DSI0_COLOR_BLIND_MDNIE_1;
	mdnie_data.DSI0_RGB_SENSOR_MDNIE_SCR = DSI0_RGB_SENSOR_MDNIE_1;

	mdnie_data.mdnie_tune_value_dsi0 = mdnie_tune_value_dsi0;
	mdnie_data.mdnie_tune_value_dsi1 = mdnie_tune_value_dsi0;
	mdnie_data.hmt_color_temperature_tune_value_dsi0 = hmt_color_temperature_tune_value_dsi0;
	mdnie_data.hmt_color_temperature_tune_value_dsi1 = hmt_color_temperature_tune_value_dsi0;

	mdnie_data.hdr_tune_value_dsi0 = hdr_tune_value_dsi0;
	mdnie_data.hdr_tune_value_dsi1 = hdr_tune_value_dsi0;

	mdnie_data.light_notification_tune_value_dsi0 = light_notification_tune_value_dsi0;
	mdnie_data.light_notification_tune_value_dsi1 = light_notification_tune_value_dsi0;

	/* Update MDNIE data related with size, offset or index */
	mdnie_data.dsi0_bypass_mdnie_size = ARRAY_SIZE(DSI0_BYPASS_MDNIE);
	mdnie_data.mdnie_color_blinde_cmd_offset = MDNIE_COLOR_BLINDE_CMD_OFFSET;
	mdnie_data.mdnie_step_index[MDNIE_STEP1] = MDNIE_STEP1_INDEX;
	mdnie_data.mdnie_step_index[MDNIE_STEP2] = MDNIE_STEP2_INDEX;
	mdnie_data.mdnie_step_index[MDNIE_STEP3] = MDNIE_STEP3_INDEX;
	mdnie_data.address_scr_white[ADDRESS_SCR_WHITE_RED_OFFSET] = ADDRESS_SCR_WHITE_RED;
	mdnie_data.address_scr_white[ADDRESS_SCR_WHITE_GREEN_OFFSET] = ADDRESS_SCR_WHITE_GREEN;
	mdnie_data.address_scr_white[ADDRESS_SCR_WHITE_BLUE_OFFSET] = ADDRESS_SCR_WHITE_BLUE;
	mdnie_data.dsi0_rgb_sensor_mdnie_1_size = DSI0_RGB_SENSOR_MDNIE_1_SIZE;
	mdnie_data.dsi0_rgb_sensor_mdnie_2_size = DSI0_RGB_SENSOR_MDNIE_2_SIZE;
	mdnie_data.dsi0_rgb_sensor_mdnie_3_size = DSI0_RGB_SENSOR_MDNIE_3_SIZE;

	mdnie_data.dsi0_trans_dimming_data_index = MDNIE_TRANS_DIMMING_DATA_INDEX;

	mdnie_data.dsi0_adjust_ldu_table = adjust_ldu_data;
	mdnie_data.dsi1_adjust_ldu_table = adjust_ldu_data;
	mdnie_data.dsi0_max_adjust_ldu = 6;
	mdnie_data.dsi1_max_adjust_ldu = 6;
	mdnie_data.dsi0_night_mode_table = night_mode_data;
	mdnie_data.dsi1_night_mode_table = night_mode_data;
	mdnie_data.dsi0_max_night_mode_index = 11;
	mdnie_data.dsi1_max_night_mode_index = 11;
	mdnie_data.dsi0_color_lens_table = color_lens_data;
	mdnie_data.dsi1_color_lens_table = color_lens_data;
	mdnie_data.dsi0_white_default_r = 0xff;
	mdnie_data.dsi0_white_default_g = 0xff;
	mdnie_data.dsi0_white_default_b = 0xff;
	mdnie_data.dsi1_white_default_r = 0xff;
	mdnie_data.dsi1_white_default_g = 0xff;
	mdnie_data.dsi1_white_default_b = 0xff;
	mdnie_data.dsi0_white_balanced_r = 0;
	mdnie_data.dsi0_white_balanced_g = 0;
	mdnie_data.dsi0_white_balanced_b = 0;
	mdnie_data.dsi1_white_balanced_r = 0;
	mdnie_data.dsi1_white_balanced_g = 0;
	mdnie_data.dsi1_white_balanced_b = 0;
	mdnie_data.dsi0_scr_step_index = MDNIE_STEP1_INDEX;
	mdnie_data.dsi1_scr_step_index = MDNIE_STEP1_INDEX;
}

static int mdss_gct_read(struct samsung_display_driver_data *vdd)
{
	u8 valid_checksum[4] = {0x8b, 0x8b, 0x8b, 0x8b};
	int res;

	if (vdd->gct.on) {
		if (!memcmp(vdd->gct.checksum, valid_checksum, 4))
			res = GCT_RES_CHECKSUM_PASS;
		else
			res = GCT_RES_CHECKSUM_NG;
	}
	else {
		res = GCT_RES_CHECKSUM_OFF;
	}

	return res;
}

#define PANEL_WIDTH	1440
#define PANEL_HEIGHT	2960
static int mdss_gct_init_pattern_buf(struct samsung_display_driver_data *vdd,
			struct gct_pattern *pat, u8 *slice_pat, int slice_size)
{
	int tot_size;
	int copy_size;
	int remain;
	int pos;

	tot_size = (PANEL_WIDTH	* PANEL_HEIGHT / 3) * 3;
	pat->buf = vmalloc(tot_size);
	if (unlikely(!pat->buf)) {
		LCD_ERR("%s: fail to allocate gct pattern1\n", __func__);
		return -ENOMEM;
	}

	pat->size = tot_size;
	remain = tot_size;
	pos = 0;
	while (remain) {
		if (remain > slice_size)
			copy_size = slice_size;
		else
			copy_size = remain;

		memcpy(pat->buf + pos, slice_pat, copy_size);

		pos += copy_size;
		remain -= copy_size;
	}

	LCD_INFO("tot_size=%d, slice_size=%d\n", tot_size, slice_size);
	return 0;
}

static int mdss_gct_write(struct samsung_display_driver_data *vdd)
{
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct mdss_overlay_private *mdp5_data;
	u8 *checksum;
	int i;
	/* vddm set, 0x0: 1.0V, 0x10: 0.9V, 0x30: 1.1V */
	u8 vddm_set[MAX_VDDM] = {0x0, 0x10, 0x30};
	int ret = 0;

	LCD_INFO("+\n");
	ctrl = samsung_get_dsi_ctrl(vdd);
	if (IS_ERR_OR_NULL(ctrl)) {
		LCD_ERR("ctrl is null..");
		return -ENODEV;;
	}

	/* alloc and copy repeated pattern */
	ret = mdss_gct_init_pattern_buf(vdd, &vdd->gct.pat1,
			pattern_line_1, sizeof(pattern_line_1));
	if (unlikely(ret))
		goto err_init_pat;

	ret = mdss_gct_init_pattern_buf(vdd, &vdd->gct.pat2,
			pattern_line_2, sizeof(pattern_line_2));
	if (unlikely(ret))
		goto err_init_pat;

	/* prevent sw reset to trigger esd recovery */
	LCD_INFO("disable esd interrupt\n");
	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd);

	/* block updating frame in DDI GRAM */
	mdp5_data = mfd_to_mdp5_data(vdd->mfd_dsi[DISPLAY_1]);
	mutex_lock(&mdp5_data->list_lock);
	msleep(17); /* commit flush time in commit buf list */

	/* enter exclusive mode*/
	mutex_lock(&vdd->exclusive_tx.ex_tx_lock);
	vdd->exclusive_tx.enable = 1;
	for (i = TX_GCT_ENTER; i <= TX_GCT_EXIT; i++)
		mdss_samsung_set_exclusive_tx_packet(ctrl, i, 1);
	mdss_samsung_set_exclusive_tx_packet(ctrl, TX_DDI_RAM_IMG_DATA, 1);
	mdss_samsung_set_exclusive_tx_packet(ctrl, TX_REG_READ_POS, 1);

	checksum = vdd->gct.checksum;
	for (i = VDDM_LV; i < MAX_VDDM; i++) {
		struct dsi_panel_cmds *vddm_cmds;

		LCD_INFO("TX_GCT_ENTER\n");
		mdss_samsung_send_cmd(ctrl, TX_GCT_ENTER);

		/* update vddm packet to control VDDM */
		vddm_cmds = mdss_samsung_cmds_select(ctrl, TX_GCT_VDDM_CTRL, NULL);
		vddm_cmds->cmds[1].payload[1] = vddm_set[i];
		LCD_INFO("TX_GCT_VDDM_CTRL: pac=%x\n", vddm_set[i]);
		mdss_samsung_send_cmd(ctrl, TX_GCT_VDDM_CTRL);

		LCD_INFO("TX_GCT_TEST_PATTERN_1\n");
		mdss_samsung_write_ddi_ram(ctrl, MIPI_TX_TYPE_GRAM,
				vdd->gct.pat1.buf, vdd->gct.pat1.size);
		msleep(300);

		mdss_samsung_panel_data_read(ctrl,
				get_panel_rx_cmds(ctrl, RX_GCT_CHECKSUM),
				checksum++, LEVEL_KEY_NONE);

		LCD_INFO("checksum %x\n", *(checksum - 1));

		LCD_INFO("TX_GCT_TEST_PATTERN_2\n");
		mdss_samsung_write_ddi_ram(ctrl, MIPI_TX_TYPE_GRAM,
				vdd->gct.pat2.buf, vdd->gct.pat2.size);

		msleep(300);

		mdss_samsung_panel_data_read(ctrl,
				get_panel_rx_cmds(ctrl, RX_GCT_CHECKSUM),
				checksum++, LEVEL_KEY_NONE);

		LCD_INFO("checksum =%x\n", *(checksum - 1));
		LCD_INFO("TX_GCT_EXIT\n");
		mdss_samsung_send_cmd(ctrl, TX_GCT_EXIT);
	}

	vdd->gct.on = 1;

	LCD_INFO("checksum = {%x %x %x %x}\n",
			vdd->gct.checksum[0], vdd->gct.checksum[1],
			vdd->gct.checksum[2], vdd->gct.checksum[3]);

	/* exit exclusive mode*/
	for (i = TX_GCT_ENTER; i <= TX_GCT_EXIT; i++)
		mdss_samsung_set_exclusive_tx_packet(ctrl, i, 0);
	mdss_samsung_set_exclusive_tx_packet(ctrl, TX_DDI_RAM_IMG_DATA, 0);
	mdss_samsung_set_exclusive_tx_packet(ctrl, TX_REG_READ_POS, 0);
	vdd->exclusive_tx.enable = 0;
	mutex_unlock(&vdd->exclusive_tx.ex_tx_lock);
	wake_up(&vdd->exclusive_tx.ex_tx_waitq);

	mutex_unlock(&mdp5_data->list_lock);

	/* enable esd interrupt */
	LCD_INFO("enable esd interrupt\n");
	if (vdd->esd_recovery.esd_irq_enable)
		vdd->esd_recovery.esd_irq_enable(true, true, (void *)vdd);

err_init_pat:
	if (vdd->gct.pat1.buf)
		vfree(vdd->gct.pat1.buf);
	if (vdd->gct.pat2.buf)
		vfree(vdd->gct.pat2.buf);
	vdd->gct.pat1.size = 0;
	vdd->gct.pat2.size = 0;

	return ret;
}

static int poc_erase(struct samsung_display_driver_data *vdd, u32 erase_pos, u32 erase_size, u32 target_pos)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int i = 0;
	int ret  = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("no vdd\n");
		return -EINVAL;
	}

	ctrl_pdata = vdd->ctrl_dsi[DISPLAY_1];
	if (IS_ERR_OR_NULL(ctrl_pdata)) {
		LCD_ERR("ctrl_pdata is null \n");
		return -EINVAL;
	}

	LCD_INFO("[AMB577] poc_erase !! \n");
	mdss_samsung_send_cmd(ctrl_pdata, TX_POC_ERASE);

	/* Delay 18 sec */
	for (i = 0; i < 180; i++) {
		msleep(100);
		if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
			LCD_ERR("cancel poc erase by user\n");
			ret = -EIO;
			goto cancel_poc;
		}
	}
	mdss_samsung_send_cmd(ctrl_pdata, TX_POC_ERASE1);
	LCD_INFO("[AMB577] poc_erase done!! \n");

cancel_poc:
	atomic_set(&vdd->poc_driver.cancel, 0);
	return ret;
}

static int poc_write(struct samsung_display_driver_data *vdd, u8 *data, u32 write_pos, u32 write_size)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_panel_cmds *poc_write_continue, *poc_write_1byte = NULL;
	int pos = 0;
	int ret = 0;
	int i = 0;
	int image_size = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("no vdd\n");
		return -EINVAL;
	}

	ctrl_pdata = vdd->ctrl_dsi[DISPLAY_1];
	if (IS_ERR_OR_NULL(ctrl_pdata)) {
		LCD_ERR("ctrl_pdata is null \n");
		return -EINVAL;
	}

	image_size = vdd->poc_driver.image_size;

	mutex_lock(&vdd->vdd_poc_operation_lock);
	vdd->poc_operation = true;
	mdss_dsi_samsung_poc_perf_mode_ctl(ctrl_pdata, true);

	if (write_pos == 0)	{
		LCD_INFO("1st, ss_poc_erase +++++ \n");
		mdss_samsung_send_cmd(ctrl_pdata, TX_POC_ERASE);
		/* Delay 18 sec */
		for (i = 0; i < 180; i++) {
			msleep(100);
			if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
				LCD_ERR("cancel poc erase by user\n");
				ret = -EIO;
				goto cancel_poc;
			}
		}
		mdss_samsung_send_cmd(ctrl_pdata, TX_POC_ERASE1);
		LCD_INFO("ss_poc_erase ----- \n");

		mdss_samsung_send_cmd(ctrl_pdata, TX_POC_PRE_WRITE);
	}

	LCD_DEBUG("[AMB577] ss_poc_write pos : %d size : %d +++++ \n", write_pos, write_size);

	poc_write_continue = get_panel_tx_cmds(ctrl_pdata, TX_POC_WRITE_CONTINUE3);
	poc_write_1byte = get_panel_tx_cmds(ctrl_pdata, TX_POC_WRITE_1BYTE);

	for (pos = write_pos; pos < (write_pos + write_size); pos++) {
		if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
			LCD_ERR("cancel poc write by user\n");
			ret = -EIO;
			goto cancel_poc;
		}

		if (pos > 1 && (pos % 256 == 0)) {
			mdss_samsung_send_cmd(ctrl_pdata, TX_POC_WRITE_CONTINUE);
			usleep_range(4000, 4000);
			mdss_samsung_send_cmd(ctrl_pdata, TX_POC_WRITE_CONTINUE2);
			udelay(2);
			poc_write_continue->cmds[0].payload[6] = (pos & 0xFF0000) >> 16;
			poc_write_continue->cmds[0].payload[7] = (pos & 0x00FF00) >> 8;
			poc_write_continue->cmds[0].payload[8] = 0;
			mdss_samsung_send_cmd(ctrl_pdata, TX_POC_WRITE_CONTINUE3);
		}

		poc_write_1byte->cmds[0].payload[1] = data[pos];
		mdss_samsung_send_cmd(ctrl_pdata, TX_POC_WRITE_1BYTE);
		udelay(2);

		if (!(pos % DEBUG_POC_CNT))
			LCD_INFO("[AMB577] cur_write_pos : %d data : 0x%x\n", pos, data[pos]);
	}

cancel_poc:
	if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
		LCD_ERR("cancel poc write by user\n");
		atomic_set(&vdd->poc_driver.cancel, 0);
		ret = -EIO;
	}

	if (pos == image_size || ret == -EIO) {
		LCD_INFO("image_size : %d cur_write_pos : %d ret : %d\n", image_size, pos, ret);
		mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], TX_POC_POST_WRITE);
	}

	mdss_dsi_samsung_poc_perf_mode_ctl(ctrl_pdata, false);
	vdd->poc_operation = false;
	mutex_unlock(&vdd->vdd_poc_operation_lock);

	LCD_DEBUG("ss_poc_write ----- \n");
	return ret;
}

static int poc_read(struct samsung_display_driver_data *vdd, u8 *buf, u32 read_pos, u32 read_size)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_panel_cmds *poc_tx_read, *poc_rx_read = NULL;
	struct cpufreq_policy *policy;
	int cpu;
	int pos = 0;
	int ret = 0;
	int image_size = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("no vdd\n");
		return -EINVAL;
	}

	LCD_DEBUG("[AMB577] ss_poc_read pos : %d size : %d+++++ \n", read_pos, read_size);

	ctrl_pdata = vdd->ctrl_dsi[DISPLAY_1];
	if (IS_ERR_OR_NULL(ctrl_pdata)) {
		LCD_ERR("ctrl_pdata is null \n");
		return -EINVAL;
	}

	image_size = vdd->poc_driver.image_size;

	/* MAX CPU ON */
	mutex_lock(&vdd->vdd_poc_operation_lock);
	vdd->poc_operation = true;
	get_online_cpus();
	for_each_online_cpu(cpu) {
		if (cpu < 4) {
			policy = cpufreq_cpu_get(cpu);
			if (policy) {
				policy->user_policy.min = 1900800;
				cpufreq_update_policy(cpu);
				cpufreq_cpu_put(policy);
			}
		}
	}

	put_online_cpus();
	mdss_dsi_samsung_poc_perf_mode_ctl(ctrl_pdata, true);

	poc_tx_read = get_panel_tx_cmds(ctrl_pdata, TX_POC_READ);
	poc_rx_read = get_panel_rx_cmds(ctrl_pdata, RX_POC_READ);

	mdss_samsung_send_cmd(ctrl_pdata, TX_POC_PRE_READ);
	for(pos = read_pos; pos < (read_pos + read_size) ; pos++) {
		if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
			LCD_ERR("cancel poc read by user\n");
			ret = -EIO;
			goto cancel_poc;
		}

		poc_tx_read->cmds[0].payload[6] = (pos & 0xFF0000) >> 16;
		poc_tx_read->cmds[0].payload[7] = (pos & 0x00FF00) >> 8;
		poc_tx_read->cmds[0].payload[8] = pos & 0x0000FF;

		mutex_lock(&vdd->vdd_mdss_direct_cmdlist_lock);
		mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], TX_POC_READ);
		usleep_range(200, 200);
		mdss_samsung_panel_data_read(vdd->ctrl_dsi[DISPLAY_1], poc_rx_read, buf + pos, LEVEL_KEY_NONE);
		mutex_unlock(&vdd->vdd_mdss_direct_cmdlist_lock);
		LCD_DEBUG("[MDSS] [IOCTRL] pos = %d, 0x%x\n", pos, buf[pos]);

		if (!(pos % DEBUG_POC_CNT))
			LCD_INFO("[AMB577] cur_read_pos : %d data : 0x%x\n", pos, buf[pos]);
	}

cancel_poc:
	if (unlikely(atomic_read(&vdd->poc_driver.cancel))) {
		LCD_ERR("cancel poc read by user\n");
		atomic_set(&vdd->poc_driver.cancel, 0);
		ret = -EIO;
	}

	if (pos == image_size || ret == -EIO) {
		LCD_INFO("image_size : %d cur_read_pos : %d ret : %d\n", image_size, pos, ret);
		mdss_samsung_send_cmd(ctrl_pdata, TX_POC_POST_READ);
	}

	/* MAX CPU OFF */
	mdss_dsi_samsung_poc_perf_mode_ctl(ctrl_pdata, false);
	get_online_cpus();
	for_each_online_cpu(cpu) {
		if (cpu < 4) {
			policy = cpufreq_cpu_get(cpu);
			if (policy) {
				policy->user_policy.min = 300000;
				cpufreq_update_policy(cpu);
				cpufreq_cpu_put(policy);
			}
		}
	}
	put_online_cpus();
	vdd->poc_operation = false;
	mutex_unlock(&vdd->vdd_poc_operation_lock);

	LCD_DEBUG("ss_poc_read ----- \n");
	return ret;
}

static void  mdss_panel_init(struct samsung_display_driver_data *vdd)
{
	LCD_ERR("%s", vdd->panel_name);

	/* ON/OFF */
	vdd->panel_func.samsung_panel_on_pre = mdss_panel_on_pre;
	vdd->panel_func.samsung_panel_on_post = mdss_panel_on_post;
	vdd->panel_func.samsung_panel_off_pre = samsung_panel_off_pre;
	vdd->panel_func.samsung_panel_off_post = samsung_panel_off_post;

	/* DDI RX */
	vdd->panel_func.samsung_panel_revision = mdss_panel_revision;
	vdd->panel_func.samsung_manufacture_date_read = mdss_manufacture_date_read;
	vdd->panel_func.samsung_ddi_id_read = mdss_ddi_id_read;
	vdd->panel_func.samsung_elvss_read = mdss_elvss_read;
	vdd->panel_func.samsung_hbm_read = mdss_hbm_read;
	vdd->panel_func.samsung_mdnie_read = mdss_mdnie_read;
	vdd->panel_func.samsung_smart_dimming_init = mdss_samart_dimming_init;
	vdd->panel_func.samsung_smart_get_conf = smart_get_conf_S6E3HA6_AMB577MQ01;
	vdd->panel_func.samsung_cell_id_read = mdss_cell_id_read;
	vdd->panel_func.samsung_octa_id_read = mdss_octa_id_read;

	/* Brightness */
	vdd->panel_func.samsung_brightness_hbm_off = NULL;
	vdd->panel_func.samsung_brightness_aid = mdss_aid;
	vdd->panel_func.samsung_brightness_acl_on = mdss_acl_on;
	vdd->panel_func.samsung_brightness_acl_percent = NULL;
	vdd->panel_func.samsung_brightness_acl_off = mdss_acl_off;
	vdd->panel_func.samsung_brightness_elvss = mdss_elvss;
	vdd->panel_func.samsung_brightness_elvss_temperature1 = mdss_elvss_temperature1;
	vdd->panel_func.samsung_brightness_elvss_temperature2 = NULL;
	vdd->panel_func.samsung_brightness_vint = mdss_vint;
	vdd->panel_func.samsung_brightness_irc = mdss_irc;
	vdd->panel_func.samsung_brightness_gamma = mdss_gamma;

	/* HBM */
	vdd->panel_func.samsung_hbm_gamma = mdss_hbm_gamma;
	vdd->panel_func.samsung_hbm_etc = mdss_hbm_etc;
	vdd->panel_func.samsung_hbm_irc = mdss_hbm_irc;
	vdd->panel_func.get_hbm_candela_value = get_hbm_candela_value;

	/* Event */
	vdd->panel_func.samsung_change_ldi_fps = NULL;

	/* HMT */
	vdd->panel_func.samsung_brightness_gamma_hmt = mdss_gamma_hmt;
	vdd->panel_func.samsung_brightness_aid_hmt = mdss_aid_hmt;
	vdd->panel_func.samsung_brightness_elvss_hmt = mdss_elvss_hmt;
	vdd->panel_func.samsung_brightness_vint_hmt = NULL;
	vdd->panel_func.samsung_smart_dimming_hmt_init = mdss_samart_dimming_init_hmt;
	vdd->panel_func.samsung_smart_get_conf_hmt = smart_get_conf_S6E3HA6_AMB577MQ01_hmt;

	/* Panel LPM */
	vdd->panel_func.samsung_get_panel_lpm_mode = mdss_get_panel_lpm_mode;

	/* default brightness */
	vdd->bl_level = 25500;

	/* mdnie */
	vdd->support_mdnie_lite = true;
	vdd->support_mdnie_trans_dimming = true;
	vdd->mdnie_tune_size[0] = sizeof(DSI0_BYPASS_MDNIE_1);
	vdd->mdnie_tune_size[1] = sizeof(DSI0_BYPASS_MDNIE_2);
	vdd->mdnie_tune_size[2] = sizeof(DSI0_BYPASS_MDNIE_3);
	dsi_update_mdnie_data();

	/* send recovery pck before sending image date (for ESD recovery) */
	vdd->send_esd_recovery = false;

	vdd->auto_brightness_level = 12;

	/* Enable panic on first pingpong timeout */
	vdd->debug_data->panic_on_pptimeout = true;

	/* Set IRC init value */
	vdd->irc_info.irc_enable_status = true;
	vdd->irc_info.irc_mode = IRC_MODERATO_MODE;

	/* COLOR WEAKNESS */
	vdd->panel_func.color_weakness_ccb_on_off =  mdss_send_colorweakness_ccb_cmd;

	/* Support DDI HW CURSOR */
	vdd->panel_func.ddi_hw_cursor = ddi_hw_cursor;

	/* MULTI_RESOLUTION */
	vdd->panel_func.samsung_multires_start = mdss_panel_multires_start;
	vdd->panel_func.samsung_multires_end = mdss_panel_multires_end;

	/* COVER Open/Close */
	vdd->panel_func.samsung_cover_control = NULL;

	/* ACL default ON */
	vdd->acl_status = 1;

	/* Gram Checksum Test */
	vdd->panel_func.samsung_gct_write = mdss_gct_write;
	vdd->panel_func.samsung_gct_read = mdss_gct_read;

	/* POC */
	vdd->poc_driver.poc_erase = poc_erase;
	vdd->poc_driver.poc_write = poc_write;
	vdd->poc_driver.poc_read = poc_read;
}

static int __init samsung_panel_init(void)
{
	struct samsung_display_driver_data *vdd = samsung_get_vdd();
	char panel_string[] = "ss_dsi_panel_S6E3HA6_AMB577MQ01_FHD";

	vdd->panel_name = mdss_mdp_panel + 8;

	LCD_INFO("MQ01 : %s / %s\n", vdd->panel_name, panel_string);

	if (!strncmp(vdd->panel_name, panel_string, strlen(panel_string))) {
		LCD_INFO("Found!\n");
		vdd->panel_func.samsung_panel_init = mdss_panel_init;
	}

	return 0;
}

early_initcall(samsung_panel_init);
