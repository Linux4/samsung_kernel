/* drivers/video/sc8800g/sc8800g_lcd_s6d0139.c
 *
 * Support for s6d0139 LCD device
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

static int32_t s6d0139_init(struct panel_spec *self)
{
	send_cmd_data_t send_cmd_data = self->info.mcu->ops->send_cmd_data;
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;
	pr_debug(KERN_DEBUG "s6d0139_init\n");

	/* reset the lcd */
	self->ops->panel_reset(self);
	mdelay(100);
	/* start init sequence */
	send_cmd_data(0x0000, 0x0001);
	mdelay(10);

	send_cmd_data(0x0007, 0x0000);
	send_cmd_data(0x0013, 0x0000);
	send_cmd_data(0x0011, 0x2604);
	send_cmd_data(0x0014, 0x1212);
	send_cmd_data(0x0010, 0x3C00);
	send_cmd_data(0x0013, 0x0040);
	mdelay(10);
	send_cmd_data(0x0013, 0x0060);
	mdelay(50);
	send_cmd_data(0x0013, 0x0070);
	mdelay(40);
	send_cmd_data(0x0001, 0x0127);
	send_cmd_data(0x0002, 0x0700);
	send_cmd_data(0x0003, 0x1030);
	send_cmd_data(0x0007, 0x0000);
	send_cmd_data(0x0008, 0x0404);
	send_cmd_data(0x000B, 0x0200);
	send_cmd_data(0x000C, 0x0000);
	send_cmd_data(0x0015, 0x0000);

	//gamma setting
	send_cmd_data(0x0030, 0x0000);
	send_cmd_data(0x0031, 0x0606);
	send_cmd_data(0x0032, 0x0006);
	send_cmd_data(0x0033, 0x0403);
	send_cmd_data(0x0034, 0x0107);
	send_cmd_data(0x0035, 0x0101);
	send_cmd_data(0x0036, 0x0707);
	send_cmd_data(0x0037, 0x0304);
	send_cmd_data(0x0038, 0x0A00);
	send_cmd_data(0x0039, 0x0706);
	send_cmd_data(0x0040, 0x0000);
	send_cmd_data(0x0041, 0x0000);
	send_cmd_data(0x0042, 0x013F);
	send_cmd_data(0x0043, 0x0000);
	send_cmd_data(0x0044, 0x0000);
	send_cmd_data(0x0045, 0x0000);
	send_cmd_data(0x0046, 0xEF00);
	send_cmd_data(0x0047, 0x013F);
	send_cmd_data(0x0048, 0x0000);
	send_cmd_data(0x0007, 0x0011);
	mdelay(40);
	send_cmd_data(0x0007, 0x0017);
	send_cmd_data(0x0020, 0x0000);
	send_cmd_data(0x0021, 0x0000);
	mdelay(40);

	send_cmd(0x0022);


	if(1){
		int i = 0;
		for (i=0; i<320*240/3; i++)
			send_data(0xf800);
		for (i=0; i< 320*240/3; i++)
			send_data(0x07e0);
		for (i=0; i< 320*240/3; i++)
			send_data(0x001f);
	}

	return 0;
}

static int32_t s6d0139_set_window(struct panel_spec *self,
		uint16_t left, uint16_t top, uint16_t right, uint16_t bottom)
{
	send_cmd_data_t send_cmd_data = self->info.mcu->ops->send_cmd_data;
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	pr_debug(KERN_DEBUG "s6d0139_set_window\n");

	send_cmd_data(0x46, ((right << 8) & 0xff00)|(left&0xff));
	send_cmd_data(0x47, bottom);
	send_cmd_data(0x48, top);

	switch (self->direction) {
	case LCD_DIRECT_NORMAL:
	case LCD_DIRECT_ROT_270:
		send_cmd_data(0x0020, left);
		send_cmd_data(0x0021, top);
		break;
	case LCD_DIRECT_ROT_90:
	case LCD_DIRECT_MIR_H:
		send_cmd_data(0x0020, right);
		send_cmd_data(0x0021, top);
		break;
	case LCD_DIRECT_ROT_180:
	case LCD_DIRECT_MIR_HV:
		send_cmd_data(0x0020, right);
		send_cmd_data(0x0021, bottom);
		break;
	case LCD_DIRECT_MIR_V:
		send_cmd_data(0x0020, left);
		send_cmd_data(0x0021, bottom);
		break;
	default:
		pr_debug(KERN_ERR "unknown lcd direction!\n");
		break;
	}

	send_cmd(0x0022);

	return 0;
}

static int32_t s6d0139_invalidate(struct panel_spec *self)
{
	pr_debug(KERN_DEBUG "s6d0139_invalidate\n");

	return self->ops->panel_set_window(self, 0, 0,
			self->width-1, self->height-1);

}

static int32_t s6d0139_set_direction(struct panel_spec *self, uint16_t direction)
{
	send_cmd_data_t send_cmd_data = self->info.mcu->ops->send_cmd_data;
	pr_debug(KERN_DEBUG "s6d0139_set_direction\n");

	switch (direction) {
	case LCD_DIRECT_NORMAL:
		send_cmd_data(0x0003, 0x1030);
		break;
	case LCD_DIRECT_ROT_90:
		send_cmd_data(0x0003, 0x1028);
		break;
	case LCD_DIRECT_ROT_180:
	case LCD_DIRECT_MIR_HV:
		send_cmd_data(0x0003, 0x1000);
		break;
	case LCD_DIRECT_ROT_270:
		send_cmd_data(0x0003, 0x1018);
		break;
	case LCD_DIRECT_MIR_H:
		send_cmd_data(0x0003, 0x1020);
		break;
	case LCD_DIRECT_MIR_V:
		send_cmd_data(0x0003, 0x1010);
		break;
	default:
		pr_debug(KERN_ERR "unknown lcd direction!\n");
		send_cmd_data(0x0003, 0x1030);
		direction = LCD_DIRECT_NORMAL;
		break;
	}

	self->direction = direction;

	return 0;
}

static struct panel_operations lcd_s6d0139_operations = {
	.panel_init = s6d0139_init,
	.panel_set_window = s6d0139_set_window,
	.panel_invalidate = s6d0139_invalidate,
	.panel_set_direction = s6d0139_set_direction,
};


static struct timing_mcu lcd_s6d0139_timing[] = {
[MCU_LCD_REGISTER_TIMING] = {    /* read/write register timing (ns) */
	.rcss = 15,
	.rlpw = 250,
	.rhpw = 250,
	.wcss = 10,
	.wlpw = 28,
	.whpw = 28,
	},
[MCU_LCD_GRAM_TIMING] = {        /* read/write gram timing (ns) */
		.rcss = 25,  /* 25 ns */
		.rlpw = 70,
		.rhpw = 70,
		.wcss = 10,
		.wlpw = 50,
		.whpw = 50,
	}
};

static struct info_mcu lcd_s6d0139_info = {
	.bus_mode = LCD_BUS_8080,
	.bus_width = 16,
	.bpp = 16, /*RGB565*/
	.timing = lcd_s6d0139_timing,
	.ops = NULL,
};

struct panel_spec lcd_s6d0139_spec = {
	.width = 240,
	.height = 320,
	.type = LCD_MODE_MCU,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mcu = &lcd_s6d0139_info
	},
	.ops = &lcd_s6d0139_operations,
};

struct panel_cfg lcd_s6d0139 = {
	/* this panel may on both CS0/1 */
	.dev_id = SPRDFB_UNDEFINELCD_ID,
	.lcd_id = 0x139,
	.lcd_name = "lcd_s6d0139",
	.panel = &lcd_s6d0139_spec,
};

static int __init lcd_s6d0139_init(void)
{
	return sprdfb_panel_register(&lcd_s6d0139);
}

subsys_initcall(lcd_s6d0139_init);
