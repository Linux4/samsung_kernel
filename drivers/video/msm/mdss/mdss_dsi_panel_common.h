/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#ifndef MDSS_HX8394C_DSI_PANEL_H
#define MDSS_HX8394C_DSI_PANEL_H

#include "mdss_panel.h"
#include "mdss_fb.h"

#define MAX_PANEL_NAME_SIZE 100

enum {
	MIPI_RESUME_STATE,
	MIPI_SUSPEND_STATE,
};

enum mdss_cmd_list {
#if defined(CONFIG_CABC_TUNING)
	PANEL_BRIGHT_CTRL,
	PANEL_CABC_DISABLE,
	PANEL_CABC_ENABLE,
#endif
};

struct display_status{
	unsigned char cabc_on;
 	unsigned char auto_brightness;
	unsigned char siop_status;
	unsigned char bright_level;
};

struct mdss_samsung_driver_data{
	struct display_status dstat;

	struct msm_fb_data_type *mfd;
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;
	struct mutex lock;

	unsigned int manufacture_id;

	char panel_name[MAX_PANEL_NAME_SIZE];
	int panel;
};

struct panel_hrev {
	char *name;
	int panel_code;
};

enum {
	PANEL_HX8394C_720P_VIDEO,
	PANEL_NT35592_720P_VIDEO,
	MAX_PANEL_LIST,
};

#endif	/* MDSS_HX8394C_DSI_PANEL_H */
