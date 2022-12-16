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

#ifndef MDSS_NT35521_DSI_PANEL_H

#define MDSS_NT35521_DSI_PANEL_H


#include "mdss_panel.h"
#include "mdss_fb.h"

enum {
	MIPI_RESUME_STATE,
	MIPI_SUSPEND_STATE,
};

struct mdss_samsung_driver_data{
	struct msm_fb_data_type *mfd;
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;
	struct mutex lock;

	unsigned int manufacture_id;
};

#endif	/* MDSS_HX8394C_DSI_PANEL_H */
