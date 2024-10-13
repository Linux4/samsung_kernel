/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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
#include "lcdpanel.h"

static int32_t hx8369_init(struct panel_spec *self)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	/* SET password */
	send_cmd(0xB9);
	send_data(0xFF);
	send_data(0x83);
	send_data(0x69);

	/* Set Power */
	send_cmd(0xB1);
	send_data(0x9D);
	send_data(0x00);
	send_data(0x34);
	send_data(0x07);
	send_data(0x00);
	send_data(0x0B);
	send_data(0x0B);
	send_data(0x1A);
	send_data(0x22);
	send_data(0x3F);
	send_data(0x3F);
	send_data(0x01);
	send_data(0x23);
	send_data(0x01);
	send_data(0xE6);
	send_data(0xE6);
	send_data(0xE6);
	send_data(0xE6);
	send_data(0xE6);

	/* SET Display  480x800 */
	send_cmd(0xB2);
	send_data(0x00);
	send_data(0x20);
	send_data(0x03);
	send_data(0x03);
	send_data(0x70);
	send_data(0x00);
	send_data(0xFF);
	send_data(0x00);
	send_data(0x00);
	send_data(0x00);
	send_data(0x00);
	send_data(0x03);
	send_data(0x03);
	send_data(0x00);
	send_data(0x01);

	/* SET Display 480x800 */
	send_cmd(0xB4);
	send_data(0x00);
	send_data(0x18);
	send_data(0x80);
	send_data(0x06);
	send_data(0x02);

	/* OSC */
	send_cmd(0xB0);
	send_data(0x00);
	send_data(0x09); /*05   42HZ  07 50HZ  0B 100% 67HZ */

	/* SET VCOM */
	send_cmd(0xB6);
	send_data(0x4A);
	send_data(0x4A);

	/* SET GIP */
	send_cmd(0xD5);
	send_data(0x00);
	send_data(0x03);
	send_data(0x03);
	send_data(0x00);
	send_data(0x01);
	send_data(0x02);

	send_data(0x28);
	send_data(0x70);
	send_data(0x11);
	send_data(0x13);
	send_data(0x00);
	send_data(0x00);
	send_data(0x40);
	send_data(0x06);
	send_data(0x51);
	send_data(0x07);
	send_data(0x00);
	send_data(0x00);
	send_data(0x41);
	send_data(0x06);
	send_data(0x50);
	send_data(0x07);
	send_data(0x07);
	send_data(0x0F);
	send_data(0x04);
	send_data(0x00);

	/* Set Gamma */
	send_cmd(0xE0);
	send_data(0x00);
	send_data(0x01);
	send_data(0x04);
	send_data(0x23);
	send_data(0x22);
	send_data(0x3F);
	send_data(0x13);
	send_data(0x39);
	send_data(0x06);
	send_data(0x0B);
	send_data(0x0E);
	send_data(0x12);
	send_data(0x15);
	send_data(0x13);
	send_data(0x15);
	send_data(0x13);
	send_data(0x1B);
	send_data(0x00);
	send_data(0x01);
	send_data(0x04);
	send_data(0x23);
	send_data(0x22);
	send_data(0x3F);
	send_data(0x13);
	send_data(0x39);
	send_data(0x06);
	send_data(0x0B);
	send_data(0x0E);
	send_data(0x12);
	send_data(0x15);
	send_data(0x13);
	send_data(0x15);
	send_data(0x13);
	send_data(0x1B);
	send_cmd(0x35);   /* TE on*/
	send_data(0x00);

	/* set CSEL */
	send_cmd(0x3A);
	send_data(0x07);  /* CSEL=0x06, 16bit-color CSEL=0x06, 18bit-color CSEL=0x07, 24bit-color */

	/*24 bit don't need to set 2DH*/

	/* Sleep Out */
	send_cmd(0x11);

	LCD_DelayMS(120);

	#if 0
	{ /* for test the lcd */
		int i;

		send_cmd(0x2C); //Write data
		for (i = 0; i < 480*800/3; i++)
			send_data(0xff);
		for (i = 0; i < 480*800/3; i++)
			send_data(0xff00);
		for (i = 0; i < 480*800/3; i++)
			send_data(0xff0000);
	}
	#endif
	/* Display On */
	send_cmd(0x29);

	LCD_DelayMS (120);

	/* Write data */
	send_cmd(0x2C);

	return 0;
}

static int32_t hx8369_set_window(struct panel_spec *self,
		uint16_t left, uint16_t top, uint16_t right, uint16_t bottom)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	pr_debug("hx8369_set_window:%d, %d, %d, %d\n", left, top, right, bottom);

	/* col */
	send_cmd(0x2A);
	send_data((left >> 8));
	send_data((left & 0xFF));
	send_data((right >> 8));
	send_data((right & 0xFF));

	/* row */
	send_cmd(0x2B);
	send_data((top >> 8));
	send_data((top & 0xFF));
	send_data((bottom >> 8));
	send_data((bottom & 0xFF));

	/* Write data */
	send_cmd(0x2C);

	return 0;
}


static int32_t hx8369_invalidate(struct panel_spec *self)
{

	return self->ops->panel_set_window(self, 0, 0,
			self->width-1, self->height-1);

}

static int32_t hx8369_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	pr_debug("hx8369_invalidate_rect\n");

	/* TE scanline */
	send_cmd(0x44);
	send_data((top >> 8));
	send_data((top & 0xFF));

	return self->ops->panel_set_window(self, left, top,
			right, bottom);
}

static int32_t hx8369_set_direction(struct panel_spec *self, uint16_t direction)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	pr_debug("hx8369_set_direction\n");
	send_cmd(0x36);

	switch (direction) {
	case LCD_DIRECT_NORMAL:
		send_data(0);
		break;
	case LCD_DIRECT_ROT_90:
		send_data(0xA0);
		break;
	case LCD_DIRECT_ROT_180:
		send_data(0x60);
		break;
	case LCD_DIRECT_ROT_270:
		send_data(0xB0);
		break;
	case LCD_DIRECT_MIR_H:
		send_data(0x40);
		break;
	case LCD_DIRECT_MIR_V:
		send_data(0x10);
		break;
	case LCD_DIRECT_MIR_HV:
		send_data(0xE0);
		break;
	default:
		pr_debug("unknown lcd direction!\n");
		send_data(0x0);
		direction = LCD_DIRECT_NORMAL;
		break;
	}

	self->direction = direction;

	return 0;
}

static int32_t hx8369_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;

	if(is_sleep) {
		/* Sleep In */
		send_cmd(0x28);
		LCD_DelayMS(120);
		send_cmd(0x10);
		LCD_DelayMS(120);
	}
	else {
#if 1
		/* Sleep Out */
		send_cmd(0x11);
		LCD_DelayMS(120);
		send_cmd(0x29);
		LCD_DelayMS(120);
#else
		/* re init */
		se1f->ops->reset(self);
		se1f->ops->lcd_init(self);
#endif
	}
	return 0;
}

static uint32_t hx8369_read_id(struct panel_spec *self)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;
	read_data_t read_data = self->info.mcu->ops->read_data;

	send_cmd(0xB9);
	send_data(0xFF);
	send_data(0x83);
	send_data(0x69);

	send_cmd(0xF4);
	read_data();
	return read_data();
}

static struct panel_operations lcd_hx8369_operations = {
	.panel_init            = hx8369_init,
	.panel_set_window      = hx8369_set_window,
	.panel_invalidate      = hx8369_invalidate,
	.panel_invalidate_rect = hx8369_invalidate_rect,
	.panel_set_direction   = hx8369_set_direction,
	.panel_enter_sleep     = hx8369_enter_sleep,
	.panel_readid          = hx8369_read_id,
};

static struct timing_mcu lcd_hx8369_timing[] = {
[LCD_REGISTER_TIMING] = {    /* read/write register timing (ns) */
		.rcss = 25,  /* 25 ns */
		.rlpw = 70,
		.rhpw = 70,
		.wcss = 10,
		.wlpw = 50,
		.whpw = 50,
	},
[LCD_GRAM_TIMING] = {        /* read/write gram timing (ns) */
		.rcss = 25,
		.rlpw = 70,
		.rhpw = 70,
		.wcss = 0,
		.wlpw = 15,
		.whpw = 24,
	}
};

static struct info_mcu lcd_hx8369_info = {
	.bus_mode = LCD_BUS_8080,
	.bus_width = 24,
	.timing = lcd_hx8369_timing,
	.ops = NULL,
};

struct panel_spec lcd_hx8369_spec = {
	.width = 480,
	.height = 800,
	.mode = LCD_MODE_MCU,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
		.mcu = &lcd_hx8369_info,
	},
	.ops = &lcd_hx8369_operations,
};

struct panel_cfg lcd_hx8369 = {
	/* this panel may on both CS0/1 */
	.lcd_cs = -1,
	.lcd_id = 0x69,
	.lcd_name = "lcd_hx8369",
	.panel = &lcd_hx8369_spec,
};

static int __init lcd_hx8369_init(void)
{
	return sprd_register_panel(&lcd_hx8369);
}

subsys_initcall(lcd_hx8369_init);
