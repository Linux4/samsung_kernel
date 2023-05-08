/* drivers/video/sprdfb/lcd_st7789v.c
 *
 * Support for st7789v LCD device
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

#define ST7789V_BUS_WIDTH_8BIT  // bus_width = 8

static int32_t st7789v_init(struct panel_spec *self)
	{
		send_data_t send_cmd = self->info.mcu->ops->send_cmd;
		send_data_t send_data = self->info.mcu->ops->send_data;

		pr_debug("st7789v_init\n");
		//Power setting sequence
		send_cmd(0x11);
		mdelay(120);

#if defined(ST7789V_BUS_WIDTH_8BIT)
    send_cmd(0xDF);
    send_data(0x5A69);
    send_data(0x0201);

    send_cmd(0x36);
    send_data(0x0000);

    send_cmd(0x3a);
    send_data(0x0500);

    send_cmd(0x35);
    send_data(0x0000);

    send_cmd(0xd2);
    send_data(0x4c00);

    send_cmd(0xb2);
    send_data(0x0404);
    send_data(0x0033);
    send_data(0x3300);

    send_cmd(0xC6);
    send_data(0x0A00);

    send_cmd(0xb8);
    send_data(0x2A2B);
    send_data(0x0575);

    send_cmd(0xb7);
    send_data(0x7500);

    send_cmd(0xbb);
    send_data(0x1B00);

    send_cmd(0xC2);
    send_data(0x0100);

    send_cmd(0xC3);
    send_data(0x0b00);

    send_cmd(0xC4);
    send_data(0x2000);

    send_cmd(0xd0);
    send_data(0xa4a1);

    send_cmd(0xE8);
    send_data(0x8300);

    send_cmd(0xE9);
    send_data(0x0909);
    send_data(0x0500);

    send_cmd(0xC0);
    send_data(0x2500);

    send_cmd(0xb0);
    send_data(0x00d0);

    send_cmd(0xE0);
    send_data(0xd00E);
    send_data(0x1810);
    send_data(0x0E2A);
    send_data(0x3E54);
    send_data(0x4C2B);
    send_data(0x1716);
    send_data(0x1D21);

    send_cmd(0XE1);
    send_data(0xd00E);
    send_data(0x1710);
    send_data(0x0E29);
    send_data(0x3C44);
    send_data(0x4D2C);
    send_data(0x1615);
    send_data(0x1E22);

    send_cmd(0x29);
#else // 16/18/24bit
	send_cmd(0xB7); send_data(0x35);
	send_cmd(0xBB); send_data(0x1A);
	send_cmd(0xC2); send_data(0x01);
	send_cmd(0xC3); send_data(0x0B);
	send_cmd(0xC4); send_data(0x20);
	send_cmd(0xD0); send_data(0xA4);send_data(0xA1);
	send_cmd(0xE8); send_data(0x83);
	send_cmd(0xE9); send_data(0x09);send_data(0x09);send_data(0x08);

	//Init setting
	send_cmd(0x36); send_data(0x00);
	send_cmd(0x35); send_data(0x00);
	send_cmd(0x3A); send_data(0x05);//0x06: 262k 0x05:65k
	send_cmd(0xD2); send_data(0x4C);
	send_cmd(0xB2);send_data(0x0C);send_data(0x0C);send_data(0x00);send_data(0x33);send_data(0x33);
	send_cmd(0xC6); send_data(0x09);

	//Gamma setting
	send_cmd(0xE0);
	send_data(0xD0);
	send_data(0x02);
	send_data(0x11);
	send_data(0x12);
	send_data(0x12);
	send_data(0x2A);
	send_data(0x3D);
	send_data(0x44);
	send_data(0x4C);
	send_data(0x2B);
	send_data(0x16);
	send_data(0x14);
	send_data(0x1E);
	send_data(0x21);

	send_cmd(0xE1);
	send_data(0xD0);
	send_data(0x02);
	send_data(0x11);
	send_data(0x11);
	send_data(0x12);
	send_data(0x2A);
	send_data(0x3B);
	send_data(0x44);
	send_data(0x4C);
	send_data(0x2B);
	send_data(0x16);
	send_data(0x15);
	send_data(0x1E);
	send_data(0x21);

	send_cmd(0x29); //Display On
#endif
		return 0;
}


static int32_t st7789v_set_window(struct panel_spec *self,
		uint16_t left, uint16_t top, uint16_t right, uint16_t bottom)
{
	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	send_data_t send_data = self->info.mcu->ops->send_data;

	pr_debug("st7789v_set_window\n");

#if defined(ST7789V_BUS_WIDTH_8BIT)
	send_cmd(0x002A); // col
	send_data(left);
	send_data(right);

	send_cmd(0x002B); // row
	send_data(top);
	send_data(bottom);
	send_cmd(0x002C);
#else // 16/18/24bit
	send_cmd(0x2A); // col
	send_data((left >> 8));
	send_data((left & 0xFF));
	send_data((right >> 8));
	send_data((right & 0xFF));

	send_cmd(0x2B); // row
	send_data((top >> 8));
	send_data((top & 0xFF));
	send_data((bottom >> 8));
	send_data((bottom & 0xFF));
	send_cmd(0x2C);
#endif

	return 0;
}


static int32_t st7789v_invalidate(struct panel_spec *self)
{
	pr_debug("st7789v_invalidate\n");

	return self->ops->panel_set_window(self, 0, 0,
			self->width-1, self->height-1);
}

static int32_t st7789v_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom)
{

	pr_debug("st7789v_invalidate_rect : (%d, %d, %d, %d)\n",left, top, right, bottom);

	return self->ops->panel_set_window(self, left, top,
			right, bottom);
}

static int32_t st7789v_set_direction(struct panel_spec *self, uint16_t direction)
{

	pr_debug("st7789v_set_direction\n");
	return 0;
}

static int32_t st7789v_enter_sleep(struct panel_spec *self, uint8_t is_sleep)
{
	//send_data_t send_cmd = self->info.mcu->ops->send_cmd;

	if(is_sleep) {
		//send_cmd(0x10);
		mdelay(120);
	}
	else {
		//send_cmd(0x11);
		mdelay(120);
	}
	return 0;
}

#define LCD_MTP_ID 0x51a0f0
static uint32_t st7789v_read_id(struct panel_spec *self)
{
	int32_t read_value = 0;

	send_data_t send_cmd = self->info.mcu->ops->send_cmd;
	read_data_t read_data = self->info.mcu->ops->read_data;

	send_cmd(0x04);

#if defined(ST7789V_BUS_WIDTH_8BIT)
	read_value += (read_data() &0xff)<<16;
	read_value += read_data();
#else
	read_data();
	read_value += read_data()<< 16;
	read_value += read_data()<< 8;
	read_value += read_data();
#endif
	pr_debug("st7789v_read_id=%x\n",read_value);

	if(read_value == LCD_MTP_ID)
		read_value = 0x7789;

	return read_value;
}

static struct panel_operations lcd_st7789v_operations = {
	.panel_init            = st7789v_init,
	.panel_set_window      = st7789v_set_window,
	.panel_invalidate      = st7789v_invalidate,
	//.lcd_invalidate_rect = st7789v_invalidate_rect,
	.panel_set_direction   = st7789v_set_direction,
	.panel_enter_sleep     = st7789v_enter_sleep,
	.panel_readid         = st7789v_read_id,
};

static struct timing_mcu lcd_st7789v_timing[] = {
	[MCU_LCD_REGISTER_TIMING] = {
		.rcss = 45,
		.rlpw = 60,
		.rhpw = 100,
		.wcss = 10,
		.wlpw = 28,
		.whpw = 28,
	},
	[MCU_LCD_GRAM_TIMING] = {
		.rcss = 25,  /* 25 ns */
		.rlpw = 70,
		.rhpw = 70,
		.wcss = 10,
		.wlpw = 28,
		.whpw = 28,
	}
};


static struct info_mcu lcd_st7789v_info = {
	.bus_mode = LCD_BUS_8080,
	.bus_width = 8, //16,
	.bpp = 16, /*RGB565*/
	.timing = &lcd_st7789v_timing,
	.ops = NULL,
};

struct panel_spec lcd_panel_st7789v = {
	.width = 240,
	.height = 320,
	.fps = 60,
	.type = LCD_MODE_MCU,
	.direction = LCD_DIRECT_NORMAL,
	.info = {
                 .mcu = &lcd_st7789v_info
                },
	.ops = &lcd_st7789v_operations,
};

struct panel_cfg lcd_st7789v = {
	/* this panel may on both CS0/1 */
	.dev_id = SPRDFB_MAINLCD_ID,
	.lcd_id = 0x7789,
	.lcd_name = "lcd_st7789v",
	.panel = &lcd_panel_st7789v,
};

static int __init lcd_st7789v_init(void)
{
	return sprdfb_panel_register(&lcd_st7789v);
}

subsys_initcall(lcd_st7789v_init);