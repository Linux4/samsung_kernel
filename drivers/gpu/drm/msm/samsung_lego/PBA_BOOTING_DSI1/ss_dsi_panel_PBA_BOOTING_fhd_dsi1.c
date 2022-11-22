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
*/
#include "ss_dsi_panel_PBA_BOOTING_fhd_dsi1.h"

static void samsung_pba_config(struct samsung_display_driver_data *vdd, void *arg)
{
#if 1
	/* case 03756419: QC recommended not to change has_src_split setting here.
	 * No effect of has_src_split setting in PBA booting mode.
	 * After verifying it on picasso board, remove below depricated code..
	 */
	LCD_INFO("skip to disable source split for pba\n");
#else
	struct sde_mdss_cfg *sde_cfg = (struct sde_mdss_cfg *)arg;

	if (!IS_ERR_OR_NULL(sde_cfg)) {
		LCD_INFO("Disable source split\n");
		sde_cfg->has_src_split = false;
	}
#endif
}

void PBA_BOOTING_FHD_DSI_init(struct samsung_display_driver_data *vdd)
{
	LCD_INFO("%s\n", ss_get_panel_name(vdd));

	vdd->mdnie.support_mdnie = false;
	vdd->dtsi_data.tft_common_support = true;
	vdd->panel_func.samsung_pba_config = samsung_pba_config;
}
