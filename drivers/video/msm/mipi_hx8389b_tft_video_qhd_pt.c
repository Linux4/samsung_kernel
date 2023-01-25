/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include "mipi_hx8389b.h"

static struct msm_panel_info pinfo;
static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db ={
	/* DSI_BIT_CLK at 311.86MHz, 3 lane, RGB888 */
	/* regulator */
	{0x09, 0x08, 0x05, 0x00, 0x20},
	/* timing */
	{0x60, 0x13, 0xC, 0x00, 0x35, 0x3F, 0x11, 0x17,
	0x16, 0x03, 0x04, 0xa0},
	/* phy ctrl */
	{0x5f, 0x00, 0x00, 0x10},
	/* strength */
	{0xee, 0x02, 0x86, 0x00},
	/* pll control */
	{0x0, 0x2B, 0x31, 0xda, 0x00, 0x20, 0x07, 0x62,
	0x41, 0x0f, 0x01,
	0x00, 0x14, 0x03, 0x00, 0x02, 0x00, 0x20, 0x00, 0x01 },
};

static int mipi_video_hx8389b_qhd_pt_init(void)
{
	int ret;

	printk(KERN_ERR "mipi_video_hx8389b_qhd_pt_init\n");

	pinfo.xres = 540;
	pinfo.yres = 960;
	pinfo.type = MIPI_VIDEO_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;

	pinfo.lcdc.h_back_porch = 40;
	pinfo.lcdc.h_front_porch = 53;
	pinfo.lcdc.h_pulse_width = 20;
	pinfo.lcdc.v_back_porch = 6;
	pinfo.lcdc.v_front_porch = 45;
	pinfo.lcdc.v_pulse_width = 1;
	pinfo.clk_rate = 311860000;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	pinfo.mipi.data_lane2 = TRUE;

	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	/* number of dot_clk cycles HSYNC active edge is
	delayed from VSYNC active edge */
	pinfo.lcdc.hsync_skew = 0;

	pinfo.bl_max = 255;
	pinfo.bl_min = 1;
	pinfo.fb_num = 2;

	pinfo.mipi.mode = DSI_VIDEO_MODE;
	/* send HSA and HE following VS/VE packet */
	pinfo.mipi.pulse_mode_hsa_he = TRUE;
	pinfo.mipi.hfp_power_stop = FALSE; /* LP-11 during the HFP period */
	pinfo.mipi.hbp_power_stop = FALSE; /* LP-11 during the HBP period */
	pinfo.mipi.hsa_power_stop = FALSE; /* LP-11 during the HSA period */
	/* LP-11 or let Command Mode Engine send packets in
	HS or LP mode for the BLLP of the last line of a frame */
	pinfo.mipi.eof_bllp_power_stop = TRUE;
	/* LP-11 or let Command Mode Engine send packets in
	HS or LP mode for packets sent during BLLP period */
	pinfo.mipi.bllp_power_stop = TRUE;

	pinfo.mipi.traffic_mode = DSI_BURST_MODE;
	pinfo.mipi.dst_format =  DSI_VIDEO_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_RGB; /* RGB */
	pinfo.mipi.dlane_swap = 0x01;

	pinfo.mipi.t_clk_post = 0x20;
	pinfo.mipi.t_clk_pre = 0x2f;

	pinfo.mipi.stream = 0; /* dma_p */
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_NONE;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.frame_rate = 60; /* FIX ME */

	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db;
	/* append EOT at the end of data burst */
	pinfo.mipi.tx_eot_append = 0x01;
	pinfo.mipi.esc_byte_ratio = 2;

	ret = mipi_hx8389b_device_register(&pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_QHD_PT);

	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	return ret;
}

module_init(mipi_video_hx8389b_qhd_pt_init);

