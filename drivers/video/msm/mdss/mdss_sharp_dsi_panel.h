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

#ifndef MDSS_SHARP_DSI_PANEL_H
#define MDSS_SHARP_DSI_PANEL_H

#include "mdss_panel.h"
#include "mdss_fb.h"

enum {
	MIPI_RESUME_STATE,
	MIPI_SUSPEND_STATE,
};

enum CABC_MODE {
	CABC_MODE_OFF, /* 100% */
	CABC_MODE_82, /* 82.05% */
	CABC_MODE_78, /* 78.05% */
	CABC_MODE_74, /* 74.42% */
	CABC_MODE_72, /* 72.73% */
	MAX_CABC_MODE
};

struct display_status{
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
};

#endif	/* MDSS_SHARP_DSI_PANEL_H */
