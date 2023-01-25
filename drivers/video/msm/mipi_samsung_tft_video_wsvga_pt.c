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
#include "mipi_tc358764_dsi2lvds.h"

static struct msm_panel_info pinfo;

static struct dsi2lvds_panel_data dsi2lvds_pd = {
	.panel_name = "SAMSUNG-LTL070NL01\n",
};


static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
	/* DSI_BIT_CLK at 450MHz, 4 lane, RGB888 */
	{0x09, 0x08, 0x05, 0x00, 0x20},    /* regulator */
	/* timing */
	{0xb7, 0x2b, 0x1d, 0x00, 0x57, 0x62, 0x22, 0x2f,
	0x32, 0x03, 0x04, 0xa0},
    /* phy ctrl */
	{0x5f, 0x00, 0x00, 0x10},
    /* strength */
	{0xff, 0x00, 0x06, 0x00},
	/* pll control */
	{0x0, 0x7f, 0x30, 0xda, 0x00, 0x50, 0x48, 0x63,
	0x41, 0x0f, 0x01,
	0x00, 0x14, 0x03, 0x00, 0x02, 0x00, 0x20, 0x00, 0x01 },
};
extern unsigned int system_rev;

static int __init mipi_video_samsung_tft_wsvga_pt_init(void)
{
	int ret;

	if(system_rev) {
		printk(KERN_ERR"%s:system_rev:%d..Initialize Quick Logic  MIPI2LVDS Converter\n", __func__,system_rev);
		return 0;
	}

	/* Landscape */
	pinfo.xres = 1024;
	pinfo.yres = 600;
	pinfo.mode2_xres = 0;
	pinfo.mode2_yres = 0;
	pinfo.mode2_bpp = 0;
	pinfo.type =  MIPI_VIDEO_PANEL;
	pinfo.pdest = DISPLAY_1; /* Primary Display */
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24; /* RGB888 = 24 bits-per-pixel */
	pinfo.fb_num = 2; /* using two frame buffers */

	/* bitclk */
	pinfo.clk_rate = 380000000;

	/*
	 * this panel is operated by DE,
	 * vsycn and hsync are ignored
	 */

	pinfo.lcdc.h_front_porch = 25;/* thfp */
	pinfo.lcdc.h_back_porch = 25;	/* thb */
	pinfo.lcdc.h_pulse_width = 470;	/* thpw */

	pinfo.lcdc.v_front_porch = 58;	/* tvfp */
	pinfo.lcdc.v_back_porch = 7;	/* tvb */
	pinfo.lcdc.v_pulse_width = 30;	/* tvpw */

	pinfo.lcdc.border_clr = 0;		/* black */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */

	pinfo.lcdc.hsync_skew = 0;

	/* Backlight levels - controled via PMIC pwm gpio */
	pinfo.bl_max = 255;
	pinfo.bl_min = 1;

	/* mipi - general */
	pinfo.mipi.vc = 0; /* virtual channel */
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo.mipi.tx_eot_append = false;//true
	pinfo.mipi.t_clk_post = 4;		/* Calculated */
	pinfo.mipi.t_clk_pre = 16;		/* Calculated */

	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db;
	pinfo.mipi.esc_byte_ratio = 4;

	/* Four lanes are recomended for 1366x768 at 60 frames per second */
	pinfo.mipi.frame_rate = 60; /* 60 frames per second */
	pinfo.mipi.data_lane0 = true;
	pinfo.mipi.data_lane1 = true;
	pinfo.mipi.data_lane2 = true;
	pinfo.mipi.data_lane3 = true;
	pinfo.mipi.dlane_swap = 0x00;

	pinfo.mipi.mode = DSI_VIDEO_MODE;
	/*
	 * Note: The CMI panel input is RGB888,
	 * thus the DSI-to-LVDS bridge output is RGB888.
	 * This parameter selects the DSI-Core output to the bridge.
	 */
	pinfo.mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;

	/* mipi - video mode */
	pinfo.mipi.traffic_mode = DSI_NON_BURST_SYNCH_EVENT;
	pinfo.mipi.pulse_mode_hsa_he = TRUE; /* sync mode */

	pinfo.mipi.hfp_power_stop = TRUE;
	pinfo.mipi.hbp_power_stop = TRUE;
	pinfo.mipi.hsa_power_stop = TRUE;
	pinfo.mipi.eof_bllp_power_stop = TRUE;
	pinfo.mipi.bllp_power_stop = TRUE;
	pinfo.mipi.vc = 0;

	/* mipi - command mode */
	pinfo.mipi.te_sel = 0;
	pinfo.mipi.interleave_max = 1;
	/* The bridge supports only Generic Read/Write commands */
	pinfo.mipi.insert_dcs_cmd = false;
	pinfo.mipi.wr_mem_continue = 0;
	pinfo.mipi.wr_mem_start = 0;
	pinfo.mipi.stream = false; /* dma_p */
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	/*
	 * toshiba d2l chip does not need max_pkt_szie dcs cmd
	 * client reply len is directly configure through
	 * RDPKTLN register (0x0404)
	 */
	pinfo.mipi.tx_eot_append = FALSE;
	pinfo.mipi.t_clk_post = 0x20;
	pinfo.mipi.t_clk_pre = 0x2D;
	pinfo.mipi.no_max_pkt_size = 1;
	pinfo.mipi.force_clk_lane_hs = 1;

	ret = mipi_tc358764_dsi2lvds_register(&pinfo, MIPI_DSI_PRIM,
				MIPI_DSI_PANEL_QHD_PT, &dsi2lvds_pd);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	return ret;
}

module_init(mipi_video_samsung_tft_wsvga_pt_init);
