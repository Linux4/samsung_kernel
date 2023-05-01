/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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
 */

#ifndef MIPI_TC358764_DSI2LVDS_H
#define MIPI_TC358764_DSI2LVDS_H

enum {
	MIPI_RESUME_STATE,
	MIPI_SUSPEND_STATE,
};

struct dsi2lvds_panel_data {
	const char panel_name[30];
};
struct dsi2lvds_driver_data {
	struct dsi2lvds_panel_data *dpd;
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
};
extern int poweroff_charging;

int mipi_tc358764_dsi2lvds_register(struct msm_panel_info *pinfo,
	u32 channel_id, u32 panel_id, struct dsi2lvds_panel_data *dpd);
#endif  /* MIPI_TC358764_DSI2LVDS_H */
