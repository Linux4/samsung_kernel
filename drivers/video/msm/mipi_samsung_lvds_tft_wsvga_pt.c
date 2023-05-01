/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_ql_dsi2lvds.h"


static struct msm_panel_info pinfo;

static struct mipi_panel_data mipi_pd = {
	.panel_name = "SDC_LTL070NL01\n",
	};

static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
	/* DSI_BIT_CLK at 450MHz, 4 lane, RGB888 */
	{0x0F, 0x0a, 0x04, 0x00, 0x20}, /* regulator */
	/* timing   */
	{0x5E, 0x29, 0x0C, 0x00, 0x34, 0x38, 0x11, 0x2D,
	 0x15, 0x03, 0x04, 0xa0},
	/* phy ctrl */
	{0x5f, 0x00, 0x00, 0x10},
	/* strength */
	{0xff, 0x00, 0x06, 0x00},
	/* pll control */
	{0x0, 0x7f, 0x31, 0xda, 0x00, 0x50, 0x48, 0x63,
	 0x31, 0x0F, 0x03,/* 4 lane */
	 0x00, 0x14, 0x03, 0x00, 0x02, 0x00, 0x20, 0x00, 0x01},
};

static int __init mipi2lvds_vx5b3d_wsvga_pt_init(void)
{
	int ret;

	printk("%s: start!\n", __func__);
	
	if(!system_rev) {
		printk(KERN_ERR"%s:system_rev:%d..Initialize TC358764 MIPI2LVDS Converter\n", __func__,system_rev);
		return 0;
	}

	/* Landscape */
	pinfo.xres = 1024;
	pinfo.yres = 600;
	pinfo.mode2_xres = 0;
	pinfo.mode2_yres = 0;
	pinfo.mode2_bpp = 0;
	pinfo.type = MIPI_VIDEO_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24; /* RGB888 = 24 bits-per-pixel */
	pinfo.fb_num = 2; /* using two frame buffers */

	pinfo.height =1024;
	pinfo.width = 600;

	/* bitclk */
	pinfo.clk_rate = 350000000;

	pinfo.lcdc.h_back_porch = 170;/* thfp */
	pinfo.lcdc.h_front_porch = 145; /* thb */
	pinfo.lcdc.h_pulse_width = 7;	/* thpw */
	pinfo.lcdc.v_back_porch = 58;	/* tvfp */
	pinfo.lcdc.v_front_porch = 58;	/* tvb */
	pinfo.lcdc.v_pulse_width = 2;	/* tvpw */

	pinfo.lcdc.border_clr = 0;		/* black */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */

	pinfo.lcdc.hsync_skew = 0;

	/* Backlight levels - controled via PMIC pwm gpio */
	pinfo.bl_max = 255;
	pinfo.bl_min = 1;

	/* mipi - general */
	pinfo.mipi.mode = DSI_VIDEO_MODE;
	pinfo.mipi.pulse_mode_hsa_he = TRUE;
	pinfo.mipi.hfp_power_stop = FALSE;
	pinfo.mipi.hbp_power_stop = FALSE;
	pinfo.mipi.hsa_power_stop = FALSE;
	pinfo.mipi.eof_bllp_power_stop = TRUE;
	pinfo.mipi.bllp_power_stop = TRUE;
	pinfo.mipi.traffic_mode = DSI_BURST_MODE;
	pinfo.mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;

	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo.mipi.dlane_swap = 0x00;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	pinfo.mipi.data_lane2 = TRUE;
	pinfo.mipi.data_lane3 = TRUE;

	pinfo.mipi.force_clk_lane_hs = 1;
	pinfo.mipi.no_max_pkt_size = 0;

	pinfo.mipi.tx_eot_append = FALSE;
	pinfo.mipi.t_clk_post = 0x19;
	pinfo.mipi.t_clk_pre = 0x2D;
	pinfo.mipi.stream = 0; /* dma_p */
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.frame_rate = 60;

	pinfo.mipi.esc_byte_ratio = 4;
	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db;


	ret = mipi2lvds_vx5b3d_disp_device_register(&pinfo, MIPI_DSI_PRIM,
						4, &mipi_pd);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	printk("%s: end!\n", __func__);

	return ret;
}

module_init(mipi2lvds_vx5b3d_wsvga_pt_init);

