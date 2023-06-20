/* Copyright (c) 2008-2013, The Linux Foundation. All rights reserved.
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

 #ifndef MDSS_SDC_PANEL_H
 #define MDSS_SDC_PANEL_H

 #include "mdss_panel.h"
 #include <mach/msm_iomap.h>
 #include <linux/io.h>
 
 struct display_status{
 	unsigned char auto_brightness;
//	unsigned char siop_status;
	int bright_level;
	int siop_status;
 };

 struct mdss_sdc_driver_data{
 	struct dsi_buf sdc_tx_buf;
	struct msm_fb_data_type *mfd;
	struct dsi_buf sdc_rx_buf;
	struct mdss_panel_common_pdata *mdss_sdc_disp_pdata;
	struct mdss_panel_data *mpd;

#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct platform_device *msm_pdev;
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	struct display_status dstat;
	struct mutex lock;
 };

#endif


void mdss_dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *cmds, int cnt, int flag);
