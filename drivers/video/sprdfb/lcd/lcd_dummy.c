/* drivers/video/sc8825/lcd_dummy.c
 *
 * Support for dummy LCD device
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
#include <linux/bug.h>
#include <linux/delay.h>
#include "../sprdfb_panel.h"

static int32_t dummy_mipi_init(struct panel_spec *self)
{
	pr_debug(KERN_DEBUG "dummy_init\n");
	return 0;
}

static uint32_t dummy_readid(struct panel_spec *self)
{
	printk("lcd_dummy read id!\n");
	printk("lcd_dummy read id success!\n");
	return 0xFFFFFFFF;
}

static int32_t dummy_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	printk(KERN_DEBUG "dummy_enter_sleep, is_sleep = %d\n", is_sleep);
	return 0;
}

static uint32_t dummy_readpowermode(struct panel_spec *self)
{
	pr_debug("lcd_dummy read power mode!\n");
	return 0x9c;
}

static int32_t dummy_check_esd(struct panel_spec *self)
{
	pr_debug("dummy_check_esd!\n");
	pr_debug("dummy_check_esd OK!\n");
	return 1;	
}

static struct panel_operations lcd_dummy_operations = {
	.panel_init = dummy_mipi_init,
	.panel_readid = dummy_readid,
	.panel_enter_sleep = dummy_enter_sleep,
	.panel_esd_check = dummy_check_esd,
};

static struct timing_rgb lcd_dummy_timing = {
	.hfp = 20,  /* unit: pixel */
	.hbp = 20,
	.hsync = 20,//4,
	.vfp = 10, /*unit: line*/
	.vbp = 10,
	.vsync = 6,
};

static struct info_mipi lcd_dummy_info = {
	.work_mode  = SPRDFB_MIPI_MODE_VIDEO,
	.video_bus_width = 24, /*18,16*/
	.lan_number = 3,
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

struct panel_spec lcd_dummy_spec = {
	.width = 320,
	.height = 480,
	.fps = 60,
	.type = LCD_MODE_DSI,
	.direction = LCD_DIRECT_NORMAL,
	.is_clean_lcd = true,
	.info = {
		.mipi = &lcd_dummy_info
	},
	.ops = &lcd_dummy_operations,
};

struct panel_cfg lcd_dummy = {
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0xFFFFFFFF,
	.lcd_name = "lcd_dummy",
	.panel = &lcd_dummy_spec,
};

static int __init lcd_dummy_init(void)
{
	return sprdfb_panel_register(&lcd_dummy);
}

subsys_initcall(lcd_dummy_init);
