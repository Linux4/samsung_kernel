/* 
 * Dummy LCD Panel
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
#include <linux/delay.h>
#include "../sprdfb_panel.h"

//#define  LCD_DEBUG
#ifdef LCD_DEBUG
#define LCD_PRINT printk
#else
#define LCD_PRINT(...)
#endif

static int32_t dummy_init(struct panel_spec *self)
{
	return 0;
}

static int32_t dummy_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	return 0;
}
static uint32_t dummy_read_id(struct panel_spec *self)
{

	return 0x57;
}
static uint32_t dummy_esd_check(struct panel_spec *self)
{
	return 0x57;
}

static struct panel_operations lcd_dummy_operations = {
	.panel_init = dummy_init,
	.panel_enter_sleep = dummy_enter_sleep,
	.panel_readid  = dummy_read_id,
	.panel_esd_check = dummy_esd_check,
};

static struct timing_rgb lcd_dummy_timing = {
	.hfp = 20,  /* unit: pixel */
	.hbp = 20,
	.hsync = 4,
	.vfp = 10, /*unit: line*/
	.vbp = 10,
	.vsync = 6,
};

static struct info_mipi lcd_dummy_info = {
	.work_mode  = SPRDFB_MIPI_MODE_CMD,//SPRDFB_MIPI_MODE_VIDEO,//SPRDFB_MIPI_MODE_CMD,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 2,
	.phy_feq = 500*1000,
	.h_sync_pol = SPRDFB_POLARITY_POS,
	.v_sync_pol = SPRDFB_POLARITY_POS,
	.de_pol = SPRDFB_POLARITY_POS,
	.te_pol = SPRDFB_POLARITY_POS,
	.color_mode_pol = SPRDFB_POLARITY_NEG,
	.shut_down_pol = SPRDFB_POLARITY_NEG,
	.timing = &lcd_dummy_timing,
	.ops = NULL,
};

struct panel_spec lcd_panel_dummy = {
	.width = 480,
	.height = 800,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mipi = &lcd_dummy_info
	},
	.ops = &lcd_dummy_operations,
};
struct panel_cfg lcd_dummy = {
	/* this panel may on both CS0/1 */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x57,
	.lcd_name = "lcd_dummy",
	.panel = &lcd_panel_dummy,
};

