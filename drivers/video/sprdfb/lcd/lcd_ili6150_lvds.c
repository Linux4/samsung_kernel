/*
 * drivers/video/sprdfb/lcd/lcd_ili6150_lvds.c
 *
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 *
 * Author: Haibing.Yang <haibing.yang@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include "../sprdfb.h"
#include "../sprdfb_panel.h"

static uint32_t ili6150_lvds_readid(struct panel_spec *self)
{
        return 0x1806;
}

static struct panel_operations ili6150_lvds_ops = {
	.panel_init = sprdchip_lvds_init,
	.panel_readid = ili6150_lvds_readid,
};

static struct timing_rgb ili6150_lvds_timing = {
	.hfp = 150,  /* unit: pixel */
	.hbp = 150,
	.hsync = 1,//4
	.vfp = 16, /*unit: line*/
	.vbp = 16,
	.vsync = 1,
};

static struct info_rgb ili6150_lvds_info = {
	.cmd_bus_mode  = SPRDFB_RGB_BUS_TYPE_LVDS,
	.video_bus_width = 24, /*18,16*/
#if 0
	.h_sync_pol = SPRDFB_POLARITY_NEG,
	.v_sync_pol = SPRDFB_POLARITY_NEG,
	.de_pol = SPRDFB_POLARITY_POS,
#endif
	.timing = &ili6150_lvds_timing,
	.bus_info = {
		.spi = NULL,
	}
};

static struct panel_spec ili6150_lvds_spec = {
	//.cap = PANEL_CAP_NOT_TEAR_SYNC,
	.width = 1024,
	.height = 600,
	.fps = 60,
	.type = SPRDFB_PANEL_TYPE_LVDS,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.rgb = &ili6150_lvds_info
	},
	.ops = &ili6150_lvds_ops,
};

struct panel_cfg lcd_ili6150_lvds = {
	/* this panel can only be main lcd */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x1806,
	.lcd_name = "lcd_ili6150_lvds",
	.panel = &ili6150_lvds_spec,
};

static int __init lcd_ili6150_lvds_init(void)
{
	return sprdfb_panel_register(&lcd_ili6150_lvds);
}

subsys_initcall(lcd_ili6150_lvds_init);
