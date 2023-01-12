/* SPDX-License-Identifier: GPL-2.0-only
 *
 * linux/drivers/gpu/drm/samsung/mcd_cal/mcd_drm_dsim.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Samsung MCD MIPI DSI Master driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MCD_DRM_DSI_H__
#define __MCD_DRM_DSI_H__

/* Add header */
#include <drm/drm_encoder.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_property.h>
#include <drm/drm_panel.h>
#include <video/videomode.h>

#include <exynos_drm_drv.h>
#include <exynos_drm_crtc.h>
//#include <exynos_drm_dsim.h>
#include <dt-bindings/display/rf-band-id.h>
#include <dsim_cal.h>

#define MAX_MODIFIABLE_CLK_CNT 10


struct mcd_dsim_device {
	struct notifier_block radio_noti;
	struct list_head channel_head[MAX_BAND_ID];

	/* modifiable clock count */
	u32 hs_clk_cnt;
	u32 hs_clk_list[MAX_MODIFIABLE_CLK_CNT];

	u32 dsi_freq;
	u32 osc_freq;
};


#define MAX_HS_CLOCK			25000000 /*2.5Ghz*/
#define MAX_HS_CLOCK_STR_LEN		9

#define PMSK_VALUE_COUNT		14

#define DT_NAME_HS_TIMING		"hs_pll_timing"


int mcd_dsim_of_get_pll_param(struct device_node *np, u32 khz, struct stdphy_pms *pms);
#endif /* __MCD_DRM_DSI_H__ */
