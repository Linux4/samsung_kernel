/* drivers/video/sprdfb/lcd_ssd2075_mipi.c
 *
 * Support for ssd2075 mipi LCD device
 *
 * Copyright (C) 2010 Spreadtrum
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

#include <linux/kernel.h>
#include "lcd_dummy.h"
//#define printk printf

/*mcu panel*/
static struct timing_mcu lcd_dummy_mcu_timing = {
	.rcss = 15,  // 15ns
	.rlpw = 60,
	.rhpw = 60,
	.wcss = 10,
	.wlpw = 35,
	.whpw = 35,
};

static struct info_mcu lcd_dummy_mcu_info = {
	.bus_mode = LCD_BUS_8080,
	.bus_width = 24,
	.bpp = 24, /*RGB88*/
	.te_pol = SPRDFB_POLARITY_POS,
	.te_sync_delay = 0,
	.timing = &lcd_dummy_mcu_timing,
};

struct panel_spec lcd_dummy_mcu_spec = {
	.width = 320,
	.height = 480,
	.type = LCD_MODE_MCU,
	.info = {
		.mcu = &lcd_dummy_mcu_info
	},
};

/*rgb panel*/
static struct timing_rgb lcd_dummy_rgb_timing = {
	.hfp = 30,  /* unit: pixel */
	.hbp = 16,
	.hsync = 1,
	.vfp = 16, /*unit: line*/
	.vbp = 16,
	.vsync = 1,
};

static struct info_rgb lcd_dummy_rgb_info = {
	.video_bus_width = 24, /*18,16*/
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.timing = &lcd_dummy_rgb_timing,
};

struct panel_spec lcd_dummy_rgb_spec = {
	.width = 320,
	.height = 480,
	.type = LCD_MODE_RGB,
	.info = {
		.rgb = &lcd_dummy_rgb_info
	},
};

/*mipi panel*/
static struct timing_rgb lcd_dummy_mipi_timing = {
	.hfp =50,
	.hbp = 36,
	.hsync =16,
	.vfp = 12,
	.vbp = 14,
	.vsync = 2,
};

static struct info_mipi lcd_dummy_mipi_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 	4,
	.phy_feq =100*1000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_dummy_mipi_timing,
};

struct panel_spec lcd_dummy_mipi_spec = {
	.width = 320,
	.height = 480,
	.type = LCD_MODE_DSI,
	.info = {
		.mipi = &lcd_dummy_mipi_info
	},
};


